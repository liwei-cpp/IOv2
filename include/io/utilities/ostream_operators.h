#pragma once

#include <common/defs.h>
#include <common/metafunctions.h>
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
    /**
     * @lang{ZH}
     * @brief 构造输出哨兵：校验流、（在加锁前）刷新关联流、加锁，并按需切换读写方向 / 定位到末尾。
     *
     * 锁不由哨兵自身持有，而是从调用方借入（`lock`，须处于 `defer_lock` 状态）：哨兵在构造中
     * 对其加锁，但其生命周期由调用方的局部变量掌握。因此当哨兵在 `try` 块末尾析构后，锁仍被
     * 调用方持有，`catch` 中的 `handle_exception` 得以在持锁状态下更新流状态，使成功路径与失败
     * 路径对同一把 `io_mutex()` 的可见性保持一致。析构中的 unitbuf/stdio 刷新同样在这把仍被持有
     * 的锁下进行（同一递归锁、同一线程），故哨兵析构先于调用方的锁析构时刷新依旧安全。
     *
     * 关联流的刷新在加锁之前完成，保证任一时刻本线程至多持有一把流锁，维持不死锁保证。
     * @endif
     * @lang{EN}
     * @brief Constructs the output sentry: validates the stream, flushes the tied stream
     * (before locking), acquires the lock, and switches direction / repositions to end as
     * needed.
     *
     * The lock is not owned by the sentry but borrowed from the caller (`lock`, which must be
     * in `defer_lock` state): the sentry locks it during construction, yet its lifetime is
     * owned by the caller's local variable. Thus, after the sentry is destroyed at the end of
     * the enclosing `try`, the lock is still held by the caller, so `handle_exception` in the
     * `catch` can update the stream state while holding the lock, keeping the success and
     * failure paths consistent with respect to the same `io_mutex()`. The unitbuf/stdio flush
     * in the destructor also runs under that still-held lock (same recursive mutex, same
     * thread), so flushing remains safe even though the sentry is destroyed before the caller's
     * lock.
     *
     * The tied stream is flushed before locking, so at most one stream lock is held by this
     * thread at any time, preserving the no-deadlock guarantee.
     * @endif
     */
    out_sentry(TStream& os, bool is_unit_buf, bool is_app_mode, lock_type& lock)
        : m_os(os)
        , m_lock(lock)
        , m_is_unit_buf(is_unit_buf)
    {
        if (!static_cast<bool>(m_os))
            throw stream_error("ostream_sentry create fail: Invalid ostream");

        if (auto* tied = m_os.tie())
        {
            try { tied->flush(); }
            catch (...) {}
        }

        m_lock.lock();

        if constexpr (is_std)
            m_sync_with_stdio = os.m_sync_with_stdio.load();

        if constexpr (involve_input)
            os.m_streambuf.switch_to_put();

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
                if (m_os)
                {
                    if (m_is_unit_buf || m_sync_with_stdio)
                        m_os.m_streambuf.flush();

                    if (m_is_unit_buf)
                        m_os.m_streambuf.device().dflush();
                }
            }
            catch (...)
            {
                try { m_os.template handle_exception<true>(std::current_exception()); }
                catch (...) {} // NOLINT(bugprone-empty-catch)
            }
            return;
        }

        try
        {
            if (m_os)
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
    lock_type&  m_lock;
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
     * 刷新不经 sentry，而是直接以 `std::lock_guard` 持有本流的 `io_mutex()`（该锁为递归锁）
     * 直至刷新结束。由此：
     *   - **并发刷新被串行化**：多个线程同时 `flush()` 同一流时逐个进入，每个都真正完成一次
     *     刷新（不是“先到者刷、其余跳过”的弱语义）；底层缓冲区不会被并发操作。
     *   - **写与刷互斥**：`put`/`write`/`operator<<` 在其 sentry 生命周期内持有同一把
     *     `io_mutex()`，故写与刷不会并发。
     *
     * 本函数**不刷新 tie 流**：与标准 `std::basic_ostream::flush` 一致，刷新只作用于本流，
     * 关联流的刷新仅由输出操作的 sentry 在其入口触发。因刷新不再沿 tie 链传播，也就不存在
     * 递归回到本流的可能，无需任何“正在刷新”自旋/跳过标志。
     *
     * 输出前的读写模式切换由 `streambuf::flush()` 自行完成（其内部会 `switch_to_put()`），
     * 故此处无需 sentry 代劳。
     * @endif
     *
     * @lang{EN}
     * @brief Flushes this stream: writes the buffer out to the underlying device.
     *
     * Flushing does not go through the sentry; instead a `std::lock_guard` holds this stream's
     * `io_mutex()` (a recursive mutex) for the whole flush. Therefore:
     *   - **Concurrent flushes are serialized**: when several threads `flush()` the same
     *     stream, they enter one at a time and each actually completes a flush (not the weak
     *     "first caller flushes, the rest skip" semantics); the underlying buffer is never
     *     operated on concurrently.
     *   - **Write and flush are mutually exclusive**: `put`/`write`/`operator<<` hold the same
     *     `io_mutex()` for their sentry's lifetime, so a write never races a flush.
     *
     * This function **does not flush tied streams**: like the standard
     * `std::basic_ostream::flush`, it acts on this stream alone; tied streams are flushed only
     * by the sentry at the entry of an output operation. Since a flush no longer propagates
     * down the tie chain, it can never recurse back into this stream, so no "already flushing"
     * spin/skip flag is needed.
     *
     * Switching the buffer from get to put mode is done by `streambuf::flush()` itself (it
     * calls `switch_to_put()` internally), so no sentry is needed for that either.
     * @endif
     */
    virtual void flush() override
    {
        T& obj = static_cast<T&>(*this);
        std::lock_guard guard(obj.io_mutex());
        if (!static_cast<bool>(obj)) return;
        try
        {
            if constexpr (dev_cpt::support_put<typename T::device_type>)
            {
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
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::out_sentry_type;
            sentry_type cerb(self, bool(self.flags() & ios_defs::unitbuf), bool(self.flags() & ios_defs::appmode), lk);
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
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::out_sentry_type;
            sentry_type cerb(self, bool(self.flags() & ios_defs::unitbuf), bool(self.flags() & ios_defs::appmode), lk);
            if (s == nullptr && n != 0)
                throw stream_error("ostream write fail: null character sequence");
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
        requires (is_writer_def<typename U::char_type, TValue>
               || is_writer_def<typename U::char_type, std::decay_t<TValue>>)
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
    requires (is_writer_def<typename T::char_type, TValue>
           || is_writer_def<typename T::char_type, std::decay_t<TValue>>)
T& operator<<(T& obj, const TValue& value)
{
    using TDecay = std::decay_t<TValue>;
    using TChar = typename T::char_type;
    std::unique_lock lk(obj.io_mutex(), std::defer_lock);
    try
    {
        using sentry_type = typename T::out_sentry_type;
        sentry_type cerb(obj, bool(obj.flags() & ios_defs::unitbuf), bool(obj.flags() & ios_defs::appmode), lk);

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
