#pragma once

#include <common/defs.h>
#include <common/metafunctions.h>
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

    /**
     * @lang{ZH}
     * @brief 构造输入哨兵：校验流、（在加锁前）刷新关联流、加锁，并按需跳过前导空白。
     *
     * 锁不由哨兵自身持有，而是从调用方借入（`lock`，须处于 `defer_lock` 状态）：哨兵在构造
     * 中对其加锁，但其生命周期由调用方的局部变量掌握。因此当哨兵在 `try` 块末尾析构后，锁
     * 仍被调用方持有，`catch` 中的 `handle_exception` 得以在持锁状态下更新流状态，使成功路径
     * 与失败路径对同一把 `io_mutex()` 的可见性保持一致。
     *
     * 关联流的刷新在加锁之前完成，保证任一时刻本线程至多持有一把流锁，维持不死锁保证。
     * @endif
     * @lang{EN}
     * @brief Constructs the input sentry: validates the stream, flushes the tied stream
     * (before locking), acquires the lock, and skips leading whitespace if requested.
     *
     * The lock is not owned by the sentry but borrowed from the caller (`lock`, which must be
     * in `defer_lock` state): the sentry locks it during construction, yet its lifetime is
     * owned by the caller's local variable. Thus, after the sentry is destroyed at the end of
     * the enclosing `try`, the lock is still held by the caller, so `handle_exception` in the
     * `catch` can update the stream state while holding the lock, keeping the success and
     * failure paths consistent with respect to the same `io_mutex()`.
     *
     * The tied stream is flushed before locking, so at most one stream lock is held by this
     * thread at any time, preserving the no-deadlock guarantee.
     * @endif
     */
    in_sentry(TStream& is, bool noskip, lock_type& lock)
        : m_is(is)
        , m_lock(lock)
    {
        if (!m_is)
            throw stream_error("istream_sentry create fail: Invalid istream");

        if (auto* tied = is.tie())
        {
            try { tied->flush(); }
            catch (...) {}
        }

        m_lock.lock();

        if constexpr (involve_output)
            is.m_streambuf.switch_to_get();

        if (!noskip)
        {
            try
            {
                auto ct = is.m_locale.template get<IOv2::ctype<typename TStream::char_type>>();
                if (!ct)
                    throw stream_error{"istream ignore_ws fail: no ctype facet"};
                auto c = is.m_streambuf.sgetc();
                while (c.has_value() &&
                        ct->is_any(base_ft<ctype>::space, c.value()))
                {
                    c = is.m_streambuf.snextc();
                }

                if (!c.has_value())
                    throw eof_error{};
            }
            catch(...)
            {
                is.handle_exception(std::current_exception());
            }
        }

        if (!m_is)
            throw stream_error("istream_sentry create fail: Invalid istream");
    }

    ~in_sentry() = default;

    in_sentry(const in_sentry&) = delete;
    in_sentry& operator=(const in_sentry&) = delete;

private:
    TStream&   m_is;
    lock_type& m_lock;
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
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
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
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
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
        constexpr bool is_cstr = std::is_same_v<CStrPolicy, app_zt>;

        size_t gcount = 0;
        bool at_eof = false;
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
            if constexpr (std::is_pointer_v<TOut>)
            {
                if (s == nullptr)
                    throw stream_error("istream get fail: null character sequence");
            }
            if (n == 0)
                throw stream_error("istream get fail: zero buffer size");
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
                        throw stream_error("istream getline fail: delimiter not found within buffer capacity");
                }
            }

            if (gcount == 0)
                throw stream_error{"istream get fail: no character extracted"};

            if (at_eof)
                self.setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            if constexpr (is_cstr)
            {
                if constexpr (std::is_pointer_v<TOut>)
                {
                    if (s != nullptr && n != 0)
                        *s++ = TChar{};
                }
                else if (n != 0)
                    *s++ = TChar{};
            }
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
        TChar delim;
        {
            std::lock_guard guard(self.io_mutex());
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
                {
                    if constexpr (std::is_pointer_v<TOut>)
                    {
                        if (s != nullptr && n != 0)
                            *s++ = TChar{};
                    }
                    else if (n != 0)
                        *s++ = TChar{};
                }
                self.handle_exception(std::current_exception());
                return s;
            }
        }

        return self.template get<DelimPolicy, CStrPolicy, TOut>(s, n, delim);
    }

    template <typename TSelf>
    std::optional<TChar> peek(this TSelf& self)
    {
        std::optional<TChar> c;
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
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
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
            if (s == nullptr && n != 0)
                throw stream_error{"istream read fail: null character sequence"};
            self.m_streambuf.sgetn(s, n, &gcount);
            if (gcount != n)
            {
                at_eof = true;
                throw stream_error{"istream read fail: cannot read enough characters"};
            }
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }
        return s + gcount;
    }

    template <typename TSelf>
    TSelf& ignore(this TSelf& self, size_t n = 1)
    {
        bool at_eof = false;
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);

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
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
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
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
            self.unset_state(IOv2::ios_defs::eofbit);
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
        requires is_reader_def<typename U::char_type,
                               typename parse_context_type<typename U::char_type, TValue>::type>
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
    requires is_reader_def<typename T::char_type,
                           typename parse_context_type<typename T::char_type, TValue>::type>
T& operator>>(T& obj, TValue& value)
{
    using TChar = typename T::char_type;
    using TCtx = typename parse_context_type<TChar, TValue>::type;
    using sentry_type = typename T::in_sentry_type;

    bool saw_eof = false;
    std::unique_lock lk(obj.io_mutex(), std::defer_lock);
    try
    {
        auto iter = obj.i_iter(&saw_eof);
        bool skip = bool(obj.flags() & ios_defs::skipws);
        sentry_type cerb(obj, !skip, lk);

        if constexpr (is_reader_def<TChar, TCtx>)
        {
            if (obj.eof())
                throw stream_error("istream extraction fail: reached EOF with no value extracted");

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
