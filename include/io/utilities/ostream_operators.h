#pragma once

#include <common/copyable_mutex.h>
#include <common/streambuf_defs.h>
#include <io/fp_defs/base_fp.h>
#include <locale/locale.h>

#include <mutex>

namespace IOv2
{
template <typename TStream, bool involve_input, bool is_std = false>
struct out_sentry
{
public:
    out_sentry(TStream& os, bool is_unit_buf, bool is_app_mode)
        : m_os(os)
        , m_is_unit_buf(is_unit_buf)
    {
        if constexpr (is_std)
            m_sync_with_stdio = os.m_sync_with_stdio;

        if constexpr (involve_input)
            os.m_streambuf.switch_to_put();

        if (m_os.tie()) m_os.tie()->flush();

        if constexpr (!is_std)
        {
            if (is_app_mode)
                m_os.m_streambuf.rseek(0);
        }

        if (!m_os.good())
            throw stream_error("ostream_sentry create fail: Invalid ostream");
    }

    ~out_sentry()
    {
        try
        {
            if (m_os.good())
            {
                if (m_is_unit_buf || m_sync_with_stdio)
                {
                    m_os.m_streambuf.flush();
                }

                if (m_is_unit_buf)
                {
                    m_os.m_streambuf.device().dflush();
                }
            }
        }
        catch (...) {} // NOLINT(bugprone-empty-catch)
    }

    out_sentry(const out_sentry&) = delete;
    out_sentry& operator=(const out_sentry&) = delete;

private:
    TStream&    m_os;
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
     * 并在并发下保护底层缓冲区，这里使用一个“正在刷新”标志，其判断与设置在 `m_flush_mutex`
     * 保护下完成：若本流已在刷新中，则直接返回。
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
     * this stream through the tie chain. To break tie cycles, and to protect the
     * underlying buffer under concurrency, an "already flushing" flag is used; its test
     * and set are performed under `m_flush_mutex`. If this stream is already being
     * flushed, the call returns immediately.
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
        if (!obj.good()) return;

        {
            std::lock_guard g(m_flush_mutex);
            if (m_flushing) return;
            m_flushing = true;
        }
        struct reset { copyable_mutex& m; bool& f; ~reset() { std::lock_guard g(m); f = false; } } scope{m_flush_mutex, m_flushing};

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
    bool           m_flushing = false;
    copyable_mutex m_flush_mutex;
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
