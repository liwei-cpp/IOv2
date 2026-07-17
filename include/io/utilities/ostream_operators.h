#pragma once

#include <common/copyable_atomic.h>
#include <common/defs.h>
#include <common/metafunctions.h>
#include <common/streambuf_defs.h>
#include <device/device_concepts.h>
#include <io/fp_defs/base_fp.h>
#include <io/io_base.h>
#include <io/streambuf_iterator.h>
#include <locale/locale.h>

#include <concepts>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <type_traits>
#include <utility>

namespace IOv2
{
template <typename TStream, bool involve_input, bool is_std = false>
struct out_sentry
{
    using lock_type =
        std::unique_lock<std::remove_reference_t<decltype(std::declval<TStream&>().io_mutex())>>;

public:
    out_sentry(TStream& os, bool is_unit_buf, bool is_app_mode)
        : m_os(os)
        , m_lock(m_os.io_mutex())
        , m_is_unit_buf(is_unit_buf)
    {
        if (!static_cast<bool>(m_os))
            throw stream_error("ostream_sentry create fail: Invalid ostream");

        if constexpr (is_std)
            m_sync_with_stdio = os.m_sync_with_stdio;

        if constexpr (involve_input)
            os.m_streambuf.switch_to_put();

        if (auto* tied = m_os.tie()) tied->flush();

        if constexpr (!is_std)
        {
            if (is_app_mode)
                m_os.m_streambuf.rseek(0);
        }

        if (!static_cast<bool>(m_os))
            throw stream_error("ostream_sentry create fail: Invalid ostream");
    }

    /**
     * @lang{ZH}
     * @brief 析构时对 unitbuf / 与 stdio 同步的流执行自动刷新，并按异常掩码决定是否上报失败。
     *
     * 对设置了 unitbuf 或与 stdio 同步的流，析构会把缓冲区 `flush()` 出去（unitbuf 时再对设备
     * `dflush()`）。若析构时当前线程没有任何异常正在展开（以 `std::uncaught_exceptions() == 0`
     * 判定）——则刷新失败会经 `handle_exception` 上报：按类别置 `devfailbit`/`cvtfailbit`/
     * `strfailbit`，并在该位处于异常掩码时抛出；该异常会被发起本次输出的操作自身的 try/catch
     * 接住，并按掩码传播给调用者。无失败位入掩码时（默认）只置位不抛，因此正常输出路径不产生
     * 异常开销。
     *
     * 反之，只要析构时已有任何异常正在展开（无论其是否早于本哨兵构造即已在飞），则仍尝试刷新但
     * **吞掉任何异常、绝不抛出**，以免在栈展开期间抛异常导致 `std::terminate`；此分支保持与旧
     * 实现一致的“尽力刷新并吞掉”行为。
     *
     * 为使正常退出分支的通知得以传播，本析构声明为 `noexcept(false)`。
     * @endif
     *
     * @lang{EN}
     * @brief On destruction, auto-flushes a unitbuf / stdio-synced stream and decides whether
     * to report a flush failure according to the exception mask.
     *
     * For a stream with unitbuf set or synced with stdio, destruction flushes the buffer via
     * `flush()` (and, for unitbuf, `dflush()`es the device). If no exception is currently
     * propagating on this thread when the sentry is destroyed — determined by
     * `std::uncaught_exceptions() == 0` — a flush failure is reported through
     * `handle_exception`: it sets `devfailbit`/`cvtfailbit`/`strfailbit` by category and
     * throws when that bit is in the exception mask; the thrown exception is caught by the
     * originating output operation's own try/catch and propagated to the caller per the mask.
     * When no such fail bit is in the mask (the default) the bit is only set and nothing is
     * thrown, so the normal output path incurs no exception overhead.
     *
     * If instead any exception is already unwinding when the sentry is destroyed — including
     * one that was already in flight before this sentry was constructed — it still attempts
     * the flush but **swallows any exception and never throws**, so as not to throw during
     * stack unwinding and trigger `std::terminate`; this branch preserves the prior
     * "best-effort flush and swallow" behavior.
     *
     * To let the normal-exit notification propagate, this destructor is declared
     * `noexcept(false)`.
     * @endif
     */
    ~out_sentry() noexcept(false)
    {
        if (std::uncaught_exceptions() != 0)
        {
            try
            {
                if (m_os.good())
                {
                    if (m_is_unit_buf || m_sync_with_stdio)
                        m_os.m_streambuf.flush();

                    if (m_is_unit_buf)
                        m_os.m_streambuf.device().dflush();
                }
            }
            catch (...) {} // NOLINT(bugprone-empty-catch)
            return;
        }

        try
        {
            if (m_os.good())
            {
                if (m_is_unit_buf || m_sync_with_stdio)
                    m_os.m_streambuf.flush();

                if (m_is_unit_buf)
                    m_os.m_streambuf.device().dflush();
            }
        }
        catch (...)
        {
            m_os.handle_exception(std::current_exception());
        }
    }

    out_sentry(const out_sentry&) = delete;
    out_sentry& operator=(const out_sentry&) = delete;

private:
    TStream&    m_os;
    lock_type   m_lock;
    bool        m_is_unit_buf;
    bool        m_sync_with_stdio = false;
};

template <typename>
struct is_out_sentry_impl
{
    constexpr static bool value = false;
};

template <typename TStream, bool involve_input, bool is_std>
struct is_out_sentry_impl<out_sentry<TStream, involve_input, is_std>>
{
    constexpr static bool value = true;
};

template <typename T>
concept is_out_sentry = is_out_sentry_impl<T>::value;

class abs_ostream
{
public:
    virtual ~abs_ostream() = default;

public:
    virtual void flush() = 0;
};

template <typename T, typename TChar>
struct ostream_operators : public abs_ostream
{
    template<typename TSelf>
    TSelf& put(this TSelf& self, TChar c)
    {
        try
        {
            using sentry_type = typename TSelf::out_sentry_type;
            sentry_type cerb(self, bool(self.flags() & ios_defs::unitbuf), bool(self.flags() & ios_defs::appmode));
            self.m_streambuf.sputc(c);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }
        return self;
    }

    template<typename TSelf>
    TSelf& write(this TSelf& self, const TChar* s, size_t n)
    {
        try
        {
            using sentry_type = typename TSelf::out_sentry_type;
            sentry_type cerb(self, bool(self.flags() & ios_defs::unitbuf), bool(self.flags() & ios_defs::appmode));
            self.m_streambuf.sputn(s, n);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }
        return self;
    }

    /**
     * @lang{ZH}
     * @brief 刷新本流：把缓冲区写出到底层设备。
     *
     * 刷新会经 sentry 触发 `tie()` 的刷新，因而可能沿 tie 链递归回到本流。为打断 tie 环，
     * 并避免多个并发刷新同时操作底层缓冲区，这里使用一个原子的“正在刷新”标志 `m_flushing`：
     * 以一次原子的 `exchange(true)` 完成“测试并置位”，若其旧值已为 true（本流已在刷新中）
     * 则直接返回。
     *
     * 注意：该标志只序列化对本流的**并发刷新**（先到者刷新、其余并发刷新直接返回），
     * 从而使底层缓冲区不会被多个刷新同时操作；它**并不**保护本流与并发 `put`/`write`/
     * `operator<<` 之间的竞争——写与刷的并发仍需调用方经 `io_mutex()` 自行串行化。
     *
     * @warning 并发语义为“先到者刷新，其余并发调用直接返回”。**跳过的线程返回时并不保证
     *          本流已刷新完成，只保证有某个线程正在刷新它。** 若某线程依赖“它本身没有写入、
     *          但要读取的本流内容一定已经落盘”，此跳过语义偏弱，需由调用方自行同步。
     * @endif
     *
     * @lang{EN}
     * @brief Flushes this stream: writes the buffer out to the underlying device.
     *
     * Flushing triggers `tie()`'s flush via the sentry and may therefore recurse back to
     * this stream through the tie chain. To break tie cycles, and to keep concurrent
     * flushes from operating on the underlying buffer at the same time, an atomic "already
     * flushing" flag `m_flushing` is used; its test-and-set is performed as a single atomic
     * `exchange(true)`. If the previous value was already true (this stream is already being
     * flushed), the call returns immediately.
     *
     * Note this flag only serializes **concurrent flushes** of this stream (the first
     * caller flushes, other concurrent flushes return immediately), so the underlying buffer
     * is not operated on by more than one flush at once; it does **not** guard this stream
     * against a concurrent `put`/`write`/`operator<<` — write-vs-flush concurrency must still
     * be serialized by the caller via `io_mutex()`.
     *
     * @warning Concurrency semantics are "the first caller flushes, other concurrent
     *          callers return immediately". **A skipping thread does not, on return,
     *          guarantee that this stream has finished flushing — only that some thread is
     *          flushing it.** If a thread relies on content it did not write itself being
     *          durably flushed before it reads, this skip semantics is too weak and the
     *          caller must synchronize.
     * @endif
     */
    virtual void flush() override
    {
        T& obj = static_cast<T&>(*this);
        if (!static_cast<bool>(obj)) return;

        if (m_flushing.exchange(true)) return;
        struct reset { copyable_atomic<bool>& f; ~reset() noexcept { f.store(false); } } scope{m_flushing};

        try
        {
            if constexpr (dev_cpt::support_put<typename T::device_type>)
            {
                using sentry_type = typename T::out_sentry_type;
                sentry_type cerb(obj, bool(obj.flags() & ios_defs::unitbuf), false);
                obj.m_streambuf.flush();
                obj.m_streambuf.device().dflush();
            }
            else
                throw stream_error("ostream_operators::flush fail: device does not support output");
        }
        catch(...)
        {
            obj.handle_exception(std::current_exception());
        }
    }

    template <typename TSelf>
    auto o_iter(this TSelf& self)
    {
        return ostreambuf_iterator(self.m_streambuf);
    }

private:
    copyable_atomic<bool> m_flushing;
};

template <typename T>
concept ostream_type =
    requires (T a)
    {
        typename T::out_sentry_type;
        typename T::char_type;
        { a.o_iter() } -> is_ostreambuf_iterator;
        { a.locale() } -> std::same_as<const locale<typename T::char_type>&>;
    } &&
    is_out_sentry<typename T::out_sentry_type> &&
    std::derived_from<T, ios_base<typename T::char_type>>;

template <ostream_type T>
T& operator << (T& obj, void(*pf)(ios_base<typename T::char_type>&))
{
    pf(obj);
    return obj;
}

template <ostream_type T>
T& operator << (T& obj, std::function<void(ios_base<typename T::char_type>&)> pf)
{
    pf(obj);
    return obj;
}

template <ostream_type T>
T& operator << (T& obj, void(*pf)(T&))
{
    pf(obj);
    return obj;
}

template <ostream_type T>
T& operator << (T& obj, std::function<void(T&)> pf)
{
    pf(obj);
    return obj;
}

template <ostream_type T, typename U>
    requires std::derived_from<T, U>
T& operator << (T& obj, void(*pf)(U&))
{
    pf(obj);
    return obj;
}

template <ostream_type T, typename U>
    requires std::derived_from<T, U>
T& operator << (T& obj, std::function<void(U&)> pf)
{
    pf(obj);
    return obj;
}

template <ostream_type T, typename TValue>
T& operator<<(T& obj, const TValue& value)
{
    using TDecay = std::decay_t<TValue>;
    using TChar = typename T::char_type;
    try
    {
        using sentry_type = typename T::out_sentry_type;
        sentry_type cerb(obj, bool(obj.flags() & ios_defs::unitbuf), bool(obj.flags() & ios_defs::appmode));

        if constexpr (is_writer_def<TChar, TValue>)
            writer<TChar, TValue>::swrite(obj.o_iter(), obj, obj.locale(), value);
        else if constexpr (is_writer_def<TChar, TDecay>)
            writer<TChar, TDecay>::swrite(obj.o_iter(), obj, obj.locale(), value);
        else
            static_assert(dependent_false_v<TValue>, "No format method provided");
    }
    catch(...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}
}
