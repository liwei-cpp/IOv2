#pragma once

#include <common/streambuf_defs.h>
#include <io/fp_defs/base_fp.h>
#include <locale/locale.h>

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

    virtual void flush() override
    {
        T& obj = static_cast<T&>(*this);
        if (!obj.good()) return;

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
