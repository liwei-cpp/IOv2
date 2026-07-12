#pragma once

#include <io/fp_defs/base_fp.h>

#include <exception>
#include <optional>

namespace IOv2
{
template <typename TStream, bool involve_output>
struct in_sentry
{
    in_sentry(TStream& is, bool noskip)
        : m_is(is)
    {
        if constexpr (involve_output)
            is.m_streambuf.switch_to_get();

        if (is.tie())
            is.tie()->flush();

        if (!noskip)
            is.ignore_ws();

        if (!m_is)
            throw stream_error("istream_sentry create fail: Invalid istream");
    }

    ~in_sentry()
    {
        try
        {
            if (m_is.m_streambuf.is_eof())
                m_is.handle_exception(std::make_exception_ptr(eof_error{}));
        }
        catch (...) {} // NOLINT(bugprone-empty-catch)
    }

    in_sentry(const in_sentry&) = delete;
    in_sentry& operator=(const in_sentry&) = delete;

private:
    TStream& m_is;
};

template <typename>
struct is_in_sentry_impl
{
    constexpr static bool value = false;
};

template <typename TStream, bool involve_output>
struct is_in_sentry_impl<in_sentry<TStream, involve_output>>
{
    constexpr static bool value = true;
};

template <typename T>
concept is_in_sentry = is_in_sentry_impl<T>::value;

struct cons_sep;
struct keep_sep;
struct app_zt;
struct no_zt;

template <typename TChar>
struct istream_operators
{
    template <typename TSelf>
    std::optional<TChar> get(this TSelf& self)
    {
        std::optional<TChar> c;
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            c = self.m_streambuf.sbumpc();
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }
        return c;
    }

    template <typename TSelf>
    TSelf& get(this TSelf& self, TChar& c)
    {
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            auto tmp = self.m_streambuf.sbumpc();
            if (tmp.has_value()) c = tmp.value();
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    template <typename DelimPolicy, typename CStrPolicy, typename TOut, typename TSelf>
        requires ((std::is_same_v<DelimPolicy, cons_sep> || std::is_same_v<DelimPolicy, keep_sep>) &&
                  (std::is_same_v<CStrPolicy, app_zt> || std::is_same_v<CStrPolicy, no_zt>))
    TOut get(this TSelf& self, TOut s, size_t n, TChar delim)
    {
        if (n == 0) return s;
        constexpr bool is_cstr = std::is_same_v<CStrPolicy, app_zt>;

        size_t gcount = 0;
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            auto c = self.m_streambuf.sgetc();
            while ((gcount + is_cstr < n) &&
                   (c.has_value()) &&
                   (c.value() != delim))
            {
                *s++ = c.value();
                ++gcount;
                c = self.m_streambuf.snextc();
            }

            if constexpr(is_cstr)
                *s++ = TChar{};

            if constexpr (std::is_same_v<DelimPolicy, cons_sep>)
            {
                if (c.has_value())
                {
                    if (c.value() == delim)
                    {
                        self.m_streambuf.sbumpc();
                        ++gcount;
                    }
                    else
                        throw stream_error("istream get line fail");
                }
            }

            if (gcount == 0)
                throw stream_error{"No character being extracted"};
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }
        return s;
    }

    template <typename DelimPolicy, typename CStrPolicy, typename TOut, typename TSelf>
        requires ((std::is_same_v<DelimPolicy, cons_sep> || std::is_same_v<DelimPolicy, keep_sep>) &&
                  (std::is_same_v<CStrPolicy, app_zt> || std::is_same_v<CStrPolicy, no_zt>))
    TOut get(this TSelf& self, TOut s, size_t n)
    {
        try
        {
            auto ct = self.m_locale.template get<IOv2::ctype<TChar>>();
            if (!ct)
                throw stream_error{"istream get fail: no ctype facet"};
            TChar delim = ct->widen('\n');
            return self.template get<DelimPolicy, CStrPolicy, TOut>(s, n, delim);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }
        return s;
    }

    template <typename TSelf>
    std::optional<TChar> peek(this TSelf& self)
    {
        std::optional<TChar> c;
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            c = self.m_streambuf.sgetc();
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }
        return c;
    }

    template <typename TSelf>
    TChar* read(this TSelf& self, TChar* s, size_t n)
    {
        size_t gcount = 0;
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            gcount = self.m_streambuf.sgetn(s, n);
            if (gcount != n)
                throw stream_error{"cannot read enough characters"};
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }
        return s + gcount;
    }

    template <typename TSelf>
    TSelf& ignore_ws(this TSelf& self)
    {
        try
        {
            auto ct = self.m_locale.template get<IOv2::ctype<TChar>>();
            if (!ct)
                throw stream_error{"istream ignore_ws fail: no ctype facet"};
            auto c = self.m_streambuf.sgetc();
            while (c.has_value() &&
                    ct->is_any(base_ft<ctype>::space, c.value()))
            {
                c = self.m_streambuf.snextc();
            }

            if (!c.has_value())
                throw eof_error{};
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    template <typename TSelf>
    TSelf& ignore(this TSelf& self, size_t n = 1)
    {
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);

            for (size_t gcount = 0; gcount < n && !self.m_streambuf.is_eof(); ++gcount)
                self.m_streambuf.sbumpc();
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    template <typename TSelf>
    TSelf& ignore(this TSelf& self, size_t n, TChar delim)
    {
        size_t gcount = 0;
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            if (n == 0) return self;

            auto c = self.m_streambuf.sgetc();
            while (gcount < n
                    && c.has_value()
                    && (c.value() != delim))
            {
                ++gcount;
                c = self.m_streambuf.snextc();
            }

            if (gcount < n)
            {
                if (c.has_value())
                {
                    ++gcount;
                    self.m_streambuf.sbumpc();
                }
            }
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    template <typename TSelf>
    TSelf& putback(this TSelf& self, TChar c)
    {
        try
        {
            self.clear(self.rdstate() & ~IOv2::ios_defs::eofbit);

            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            self.m_streambuf.sputbackc(c);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    template <typename TSelf>
    auto i_iter(this TSelf& self)
    {
        return istreambuf_iterator(self.m_streambuf);
    }
};

template <typename T>
concept istream_type =
    requires (T a)
    {
        typename T::in_sentry_type;
        typename T::char_type;
        { a.i_iter() } -> is_istreambuf_iterator;
        { a.locale() } -> std::same_as<const locale<typename T::char_type>&>;
    } &&
    is_in_sentry<typename T::in_sentry_type> &&
    std::derived_from<T, ios_base<typename T::char_type>>;

template <istream_type T>
T& operator >> (T& obj, void(*pf)(ios_base<typename T::char_type>&))
{
    pf(obj);
    return obj;
}

template <istream_type T>
T& operator >> (T& obj, std::function<void(ios_base<typename T::char_type>&)> pf)
{
    pf(obj);
    return obj;
}

template <istream_type T>
T& operator >> (T& obj, void(*pf)(T&))
{
    pf(obj);
    return obj;
}

template <istream_type T>
T& operator >> (T& obj, std::function<void(T&)> pf)
{
    pf(obj);
    return obj;
}

template <istream_type T, typename U>
    requires std::derived_from<T, U>
T& operator >> (T& obj, void(*pf)(U&))
{
    pf(obj);
    return obj;
}

template <istream_type T, typename U>
    requires std::derived_from<T, U>
T& operator >> (T& obj, std::function<void(U&)> pf)
{
    pf(obj);
    return obj;
}

template <istream_type T, typename TValue>
T& operator>>(T& obj, TValue& value)
{
    auto iter = obj.i_iter();

    using TChar = typename T::char_type;
    using TCtx = typename parse_context_type<TChar, TValue>::type;
    using sentry_type = typename T::in_sentry_type;

    try
    {
        bool skip = bool(obj.flags() & ios_defs::skipws);
        sentry_type cerb(obj, !skip);

        if constexpr (is_reader_def<TChar, TCtx>)
        {
            if constexpr (std::is_same_v<TCtx, TValue>)
                reader<TChar, TValue>::sread(iter, std::default_sentinel, obj, obj.locale(), value);
            else
            {
                TCtx tmp;
                reader<TChar, TCtx>::sread(iter, std::default_sentinel, obj, obj.locale(), tmp);
                value = static_cast<TValue>(tmp);
            }
        }
        else
            static_assert(dependent_false_v<TValue>, "No parse method provided");
    }
    catch(...)
    {
        obj.handle_exception(std::current_exception());
    }

    return obj;
}
}
