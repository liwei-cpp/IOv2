#pragma once

#include <common/defs.h>
#include <common/metafunctions.h>
#include <common/streambuf_defs.h>
#include <facet/ctype.h>
#include <io/fp_defs/base_fp.h>
#include <io/io_base.h>
#include <io/streambuf_iterator.h>
#include <locale/locale.h>

#include <concepts>
#include <cstddef>
#include <exception>
#include <functional>
#include <iterator>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace IOv2
{
template <typename TStream, bool involve_output>
struct in_sentry
{
    using lock_type =
        std::unique_lock<std::remove_reference_t<decltype(std::declval<TStream&>().io_mutex())>>;

    in_sentry(TStream& is, bool noskip)
        : m_is(is)
        , m_lock(m_is.io_mutex())
    {
        if (!m_is)
            throw stream_error("istream_sentry create fail: Invalid istream");

        if constexpr (involve_output)
            is.m_streambuf.switch_to_get();

        if (auto* tied = is.tie())
            tied->flush();

        if (!noskip)
            is.ignore_ws();

        if (!m_is)
            throw stream_error("istream_sentry create fail: Invalid istream");
    }

    ~in_sentry() = default;

    in_sentry(const in_sentry&) = delete;
    in_sentry& operator=(const in_sentry&) = delete;

private:
    TStream&  m_is;
    lock_type m_lock;
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
struct istream_operators;

template <typename T>
concept istream_type =
    requires (T a)
    {
        typename T::in_sentry_type;
        typename T::char_type;
        { a.locale() } -> std::same_as<const locale<typename T::char_type>&>;
    } &&
    is_in_sentry<typename T::in_sentry_type> &&
    std::derived_from<T, ios_base<typename T::char_type>> &&
    std::derived_from<T, istream_operators<typename T::char_type>>;

template <typename TChar>
struct istream_operators
{
    template <typename TSelf>
    std::optional<TChar> get(this TSelf& self)
    {
        std::optional<TChar> c;
        bool at_eof = false;
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            c = self.m_streambuf.sbumpc();
            if (!c.has_value())
            {
                at_eof = true;
                throw stream_error{"istream get fail: no character extracted"};
            }
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }
        return c;
    }

    template <typename TSelf>
    TSelf& get(this TSelf& self, TChar& c)
    {
        bool at_eof = false;
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            auto tmp = self.m_streambuf.sbumpc();
            if (tmp.has_value()) c = tmp.value();
            else
            {
                at_eof = true;
                throw stream_error{"istream get fail: no character extracted"};
            }
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
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
        bool at_eof = false;
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

            at_eof = (gcount + is_cstr < n) && (!c.has_value());

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

            if (at_eof)
                self.setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            if constexpr (is_cstr)
                *s++ = TChar{};
            self.handle_exception(std::current_exception(), at_eof);
            return s;
        }

        if constexpr (is_cstr)
            *s++ = TChar{};
        return s;
    }

    template <typename DelimPolicy, typename CStrPolicy, typename TOut, typename TSelf>
        requires ((std::is_same_v<DelimPolicy, cons_sep> || std::is_same_v<DelimPolicy, keep_sep>) &&
                  (std::is_same_v<CStrPolicy, app_zt> || std::is_same_v<CStrPolicy, no_zt>))
    TOut get(this TSelf& self, TOut s, size_t n)
    {
        std::lock_guard guard(self.io_mutex());
        TChar delim;
        try
        {
            auto ct = self.m_locale.template get<IOv2::ctype<TChar>>();
            if (!ct)
                throw stream_error{"istream get fail: no ctype facet"};
            delim = ct->widen('\n');
        }
        catch(...)
        {
            if constexpr (std::is_same_v<CStrPolicy, app_zt>)
                if (n != 0) *s++ = TChar{};
            self.handle_exception(std::current_exception());
            return s;
        }

        return self.template get<DelimPolicy, CStrPolicy, TOut>(s, n, delim);
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
            if (!c.has_value()) throw eof_error{};
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
        bool at_eof = false;
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
            self.m_streambuf.sgetn(s, n, &gcount);
            if (gcount != n)
            {
                at_eof = true;
                throw stream_error{"cannot read enough characters"};
            }
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }
        return s + gcount;
    }

    template <typename TSelf>
    TSelf& ignore_ws(this TSelf& self)
    {
        std::lock_guard guard(self.io_mutex());
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
        bool at_eof = false;
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);

            for (size_t gcount = 0; gcount < n; ++gcount)
            {
                if (self.m_streambuf.is_eof())
                {
                    at_eof = true;
                    break;
                }
                self.m_streambuf.sbumpc();
            }

            if (at_eof)
                self.setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }

        return self;
    }

    template <typename TSelf>
    TSelf& ignore(this TSelf& self, size_t n, TChar delim)
    {
        size_t gcount = 0;
        bool at_eof = false;
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

            at_eof = (gcount < n) && (!c.has_value());

            if (gcount < n)
            {
                if (c.has_value())
                {
                    ++gcount;
                    self.m_streambuf.sbumpc();
                }
            }

            if (at_eof)
                self.setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }

        return self;
    }

    template <typename TSelf>
    TSelf& putback(this TSelf& self, TChar c)
    {
        std::lock_guard guard(self.io_mutex());
        try
        {
            self.unset_state(IOv2::ios_defs::eofbit);

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

    /**
     * @lang{ZH}
     * @brief 取输入迭代器；可选地附加一个“已观察到输入结束”的报告位。
     * @param saw_eof 可选的报告位；生存期必须覆盖迭代器及其所有副本，`nullptr` 表示不
     *                上报。详见 istreambuf_iterator。
     * @endif
     * @lang{EN}
     * @brief Gets an input iterator; optionally attaches an "observed end of input" flag.
     * @param saw_eof Optional report flag; its lifetime must cover the iterator and all
     *                copies, `nullptr` means do not report. See istreambuf_iterator.
     * @endif
     */
private:
    template <typename TSelf>
    auto i_iter(this TSelf& self, bool* saw_eof = nullptr)
    {
        return istreambuf_iterator(self.m_streambuf, saw_eof);
    }

    template <istream_type U, typename TValue>
    friend U& operator>>(U& obj, TValue& value);
};

template <istream_type T>
T& operator >> (T& obj, void(*pf)(ios_base<typename T::char_type>&))
{
    try
    {
        if (!pf)
            throw stream_error("istream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

template <istream_type T>
T& operator >> (T& obj, const std::function<void(ios_base<typename T::char_type>&)>& pf)
{
    try
    {
        if (!pf)
            throw stream_error("istream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

template <istream_type T>
T& operator >> (T& obj, void(*pf)(T&))
{
    try
    {
        if (!pf)
            throw stream_error("istream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

template <istream_type T>
T& operator >> (T& obj, const std::function<void(T&)>& pf)
{
    try
    {
        if (!pf)
            throw stream_error("istream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

template <istream_type T, typename U>
    requires std::derived_from<T, U>
T& operator >> (T& obj, void(*pf)(U&))
{
    try
    {
        if (!pf)
            throw stream_error("istream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

template <istream_type T, typename U>
    requires std::derived_from<T, U>
T& operator >> (T& obj, const std::function<void(U&)>& pf)
{
    try
    {
        if (!pf)
            throw stream_error("istream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

template <istream_type T, typename TValue>
T& operator>>(T& obj, TValue& value)
{
    bool saw_eof = false;
    auto iter = obj.i_iter(&saw_eof);

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

            if (saw_eof) obj.setstate(ios_defs::eofbit);
        }
        else
            static_assert(dependent_false_v<TValue>, "No parse method provided");
    }
    catch(...)
    {
        obj.handle_exception(std::current_exception(), saw_eof);
    }

    return obj;
}
}
