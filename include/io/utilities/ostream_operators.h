#pragma once

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
#include <string>
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
            {
                try
                {
                    m_os.m_streambuf.rseek(0);
                }
                catch (const cvt_error& e)
                {
                    throw cvt_error(std::string("ostream_sentry create fail: appmode cannot "
                                                "reposition to the end; appmode requires a "
                                                "fixed-length, state-independent encoding: ")
                                    + e.what());
                }
            }
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

class abs_flusher
{
public:
    virtual ~abs_flusher() = default;

public:
    virtual void flush() = 0;
};

/**
 * @lang{ZH}
 * @brief 承载多态 `flush()` 的 CRTP 基类：对具体流类型 `T` 的向下转型集中于此。
 *
 * `flush()` 覆盖 `abs_flusher::flush`，而虚函数无法使用 deducing-this，只能
 * `static_cast<T&>(*this)` 取回具体流类型。单独引入本模板承载该 `T`，从而让
 * `ostream_operators` 不必再携带 CRTP 自身参数（与 `istream_operators<TChar>` 对称）。
 * 每个输出流同时派生 `out_flusher<自身>` 与 `ostream_operators<TChar>`。
 * @endif
 * @lang{EN}
 * @brief CRTP base carrying the polymorphic `flush()`: the down-cast to the concrete stream
 * type `T` is localized here.
 *
 * `flush()` overrides `abs_flusher::flush`; a virtual cannot use deducing-this, so it must
 * `static_cast<T&>(*this)` to recover the concrete stream type. This template exists solely
 * to carry that `T`, letting `ostream_operators` drop its CRTP self-parameter (making it
 * symmetric with `istream_operators<TChar>`). Every output stream derives from both
 * `out_flusher<Self>` and `ostream_operators<TChar>`.
 * @endif
 */
template <typename T>
struct out_flusher : public abs_flusher
{
    /**
     * @lang{ZH}
     * @brief 刷新本流：把缓冲区写出到底层设备。
     *
     * 刷新经 sentry 进行，故整个刷新期间持有本流的 `io_mutex()`（该锁为递归锁）。由此：
     *   - **并发刷新被串行化**：多个线程同时 `flush()` 同一流时逐个进入，每个都真正完成一次
     *     刷新（不是“先到者刷、其余跳过”的弱语义）；底层缓冲区不会被并发操作。
     *   - **写与刷互斥**：`put`/`write`/`operator<<` 同样在其 sentry 生命周期内持有同一把
     *     `io_mutex()`，故写与刷不会并发。
     *
     * sentry 还会触发 `tie()->flush()`，从而可能沿 tie 链继续刷下去。由于 tie 图恒为无环
     * （成环在 `tie()` 设置时即被拒绝，并由 `tie_graph_mutex()` 保证并发下依然无环），该链
     * 有限且必然终止，不会递归回到本流，因此无需任何“正在刷新”自旋/跳过标志。
     * @endif
     *
     * @lang{EN}
     * @brief Flushes this stream: writes the buffer out to the underlying device.
     *
     * Flushing runs through the sentry, so this stream's `io_mutex()` (a recursive mutex) is
     * held for the whole flush. Therefore:
     *   - **Concurrent flushes are serialized**: when several threads `flush()` the same
     *     stream, they enter one at a time and each actually completes a flush (not the weak
     *     "first caller flushes, the rest skip" semantics); the underlying buffer is never
     *     operated on concurrently.
     *   - **Write and flush are mutually excluded**: `put`/`write`/`operator<<` hold the same
     *     `io_mutex()` for their sentry's lifetime, so a write never races a flush.
     *
     * The sentry also triggers `tie()->flush()`, which may keep flushing down the tie chain.
     * Because the tie graph is always acyclic (a cycle is rejected at `tie()` set time and
     * `tie_graph_mutex()` keeps it acyclic under concurrency), that chain is finite and
     * terminates -- it never recurses back to this stream -- so no "already flushing"
     * spin/skip flag is needed.
     * @endif
     */
    virtual void flush() override
    {
        T& obj = static_cast<T&>(*this);
        if (!static_cast<bool>(obj)) return;

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
                throw stream_error("out_flusher::flush fail: device does not support output");
        }
        catch(...)
        {
            obj.handle_exception(std::current_exception());
        }
    }
};

template <typename TChar>
struct ostream_operators;

template <typename T>
concept ostream_type =
    requires (T a)
    {
        typename T::out_sentry_type;
        typename T::char_type;
        { a.locale() } -> std::same_as<const locale<typename T::char_type>&>;
    } &&
    is_out_sentry<typename T::out_sentry_type> &&
    std::derived_from<T, ios_base<typename T::char_type>> &&
    std::derived_from<T, ostream_operators<typename T::char_type>>;

template <typename TChar>
struct ostream_operators
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

private:
    template <typename TSelf>
    auto o_iter(this TSelf& self)
    {
        return ostreambuf_iterator(self.m_streambuf);
    }

    template <ostream_type U, typename TValue>
    friend U& operator<<(U& obj, const TValue& value);
};

template <ostream_type T>
T& operator << (T& obj, void(*pf)(ios_base<typename T::char_type>&))
{
    try
    {
        if (!pf)
            throw stream_error("ostream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

template <ostream_type T>
T& operator << (T& obj, const std::function<void(ios_base<typename T::char_type>&)>& pf)
{
    try
    {
        if (!pf)
            throw stream_error("ostream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

template <ostream_type T>
T& operator << (T& obj, void(*pf)(T&))
{
    try
    {
        if (!pf)
            throw stream_error("ostream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

template <ostream_type T>
T& operator << (T& obj, const std::function<void(T&)>& pf)
{
    try
    {
        if (!pf)
            throw stream_error("ostream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

template <ostream_type T, typename U>
    requires std::derived_from<T, U>
T& operator << (T& obj, void(*pf)(U&))
{
    try
    {
        if (!pf)
            throw stream_error("ostream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

template <ostream_type T, typename U>
    requires std::derived_from<T, U>
T& operator << (T& obj, const std::function<void(U&)>& pf)
{
    try
    {
        if (!pf)
            throw stream_error("ostream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
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
