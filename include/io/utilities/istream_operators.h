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
        , m_uncaught(std::uncaught_exceptions())
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

    /**
     * @lang{ZH}
     * @brief 析构时检测输入是否到达 EOF，并按异常掩码决定是否上报。
     *
     * 若本哨兵是在其作用域**正常退出**时析构——即没有新的异常正在展开，通过与构造时捕获的
     * `std::uncaught_exceptions()` 基线比较判定——则允许 `handle_exception(eof_error)` 在
     * `eofbit` 位于异常掩码时抛出；该异常会被发起本次 I/O 的操作自身的 try/catch 接住，并按
     * 掩码传播给调用者。`eofbit` 未入掩码时（默认）`handle_exception` 只置位不抛，因此常见的
     * EOF 路径不产生任何异常开销。
     *
     * 若本哨兵是在别处异常展开过程中被析构，则仅更新 `eofbit` 状态而**绝不抛出**，以免在栈
     * 展开期间抛异常导致 `std::terminate`；此分支保持与旧实现一致的“置位并吞掉”行为。
     *
     * 为使正常退出分支的通知得以传播，本析构声明为 `noexcept(false)`。
     * @endif
     *
     * @lang{EN}
     * @brief On destruction, detects whether input reached EOF and decides whether to report
     * it according to the exception mask.
     *
     * If this sentry is destroyed on a **normal scope exit** — i.e. no new exception is
     * propagating, determined by comparing against the `std::uncaught_exceptions()` baseline
     * captured at construction — then `handle_exception(eof_error)` is allowed to throw when
     * `eofbit` is in the exception mask; that exception is caught by the originating I/O
     * operation's own try/catch and propagated to the caller per the mask. When `eofbit` is
     * not in the mask (the default) `handle_exception` only sets the bit and does not throw,
     * so the common EOF path incurs no exception overhead.
     *
     * If this sentry is instead destroyed while another exception is unwinding, it only
     * updates the `eofbit` state and **never throws**, so as not to throw during stack
     * unwinding and trigger `std::terminate`; this branch preserves the prior
     * "set-the-bit-and-swallow" behavior.
     *
     * To let the normal-exit notification propagate, this destructor is declared
     * `noexcept(false)`.
     * @endif
     */
    ~in_sentry() noexcept(false)
    {
        if (std::uncaught_exceptions() != m_uncaught)
        {
            try
            {
                if (m_is.m_streambuf.is_eof())
                    m_is.handle_exception(std::make_exception_ptr(eof_error{}));
            }
            catch (...) {} // NOLINT(bugprone-empty-catch)
            return;
        }

        if (m_is.m_streambuf.is_eof())
            m_is.handle_exception(std::make_exception_ptr(eof_error{}));
    }

    in_sentry(const in_sentry&) = delete;
    in_sentry& operator=(const in_sentry&) = delete;

private:
    TStream& m_is;
    int      m_uncaught;
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
