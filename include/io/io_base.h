#pragma once
#include <common/defs.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <forward_list>
#include <functional>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>

namespace IOv2
{
namespace ios_defs
{
    using fmtflags = std::uint16_t;
    using iostate  = std::uint8_t;

    constexpr static fmtflags boolalpha   = 1L << 0;
    constexpr static fmtflags dec         = 1L << 1;
    constexpr static fmtflags fixed       = 1L << 2;
    constexpr static fmtflags hex         = 1L << 3;
    constexpr static fmtflags internal    = 1L << 4;
    constexpr static fmtflags left        = 1L << 5;
    constexpr static fmtflags oct         = 1L << 6;
    constexpr static fmtflags right       = 1L << 7;
    constexpr static fmtflags scientific  = 1L << 8;
    constexpr static fmtflags showbase    = 1L << 9;
    constexpr static fmtflags showpoint   = 1L << 10;
    constexpr static fmtflags showpos     = 1L << 11;
    constexpr static fmtflags skipws      = 1L << 12;
    constexpr static fmtflags unitbuf     = 1L << 13;
    constexpr static fmtflags uppercase   = 1L << 14;
    constexpr static fmtflags appmode     = 1L << 15;
    constexpr static fmtflags adjustfield = left | right | internal;
    constexpr static fmtflags basefield   = dec | oct | hex;
    constexpr static fmtflags floatfield  = scientific | fixed;

    constexpr static iostate goodbit        = 0;
    constexpr static iostate eofbit         = 1L << 0;
    constexpr static iostate devfailbit     = 1L << 1;
    constexpr static iostate cvtfailbit     = 1L << 2;
    constexpr static iostate strfailbit     = 1L << 3;
    constexpr static iostate otherfailbit   = 1L << 4;
};

struct io_state_and_exp
{
    ios_defs::iostate rdstate() const { return m_stream_state; }

    void clear(ios_defs::iostate s = ios_defs::goodbit)
    {
        m_stream_state = s;
        if ((m_stream_state & ios_defs::devfailbit) == ios_defs::goodbit) m_exp_dev_fail = std::exception_ptr{};
        if ((m_stream_state & ios_defs::cvtfailbit) == ios_defs::goodbit) m_exp_cvt_fail = std::exception_ptr{};
        if ((m_stream_state & ios_defs::strfailbit) == ios_defs::goodbit) m_exp_str_fail = std::exception_ptr{};
        if ((m_stream_state & ios_defs::otherfailbit) == ios_defs::goodbit) m_exp_other_fail = std::exception_ptr{};

        ios_defs::iostate state_in_exp = m_exception & m_stream_state;
        if (state_in_exp & ios_defs::devfailbit)
        {
            if (m_exp_dev_fail)
                std::rethrow_exception(std::exchange(m_exp_dev_fail, nullptr));
            else
                throw device_error("device failure bit has been set");
        }
        else if (state_in_exp & ios_defs::cvtfailbit)
        {
            if (m_exp_cvt_fail)
                std::rethrow_exception(std::exchange(m_exp_cvt_fail, nullptr));
            else
                throw cvt_error("converter failure bit has been set");
        }
        else if (state_in_exp & ios_defs::strfailbit)
        {
            if (m_exp_str_fail)
                std::rethrow_exception(std::exchange(m_exp_str_fail, nullptr));
            else
                throw stream_error("stream failure bit has been set");
        }
        else if (state_in_exp & ios_defs::otherfailbit)
        {
            if (m_exp_other_fail)
                std::rethrow_exception(std::exchange(m_exp_other_fail, nullptr));
            else
                throw stream_error("other failure bit has been set");
        }
        else if (state_in_exp & ios_defs::eofbit)
        {
            throw eof_error{};
        }
    }

    void setstate(ios_defs::iostate s) { clear(rdstate() | s); }
    bool good() const { return rdstate() == 0; }
    bool dev_fail() const { return rdstate() & ios_defs::devfailbit; }
    bool cvt_fail() const { return rdstate() & ios_defs::cvtfailbit; }
    bool str_fail() const { return rdstate() & ios_defs::strfailbit; }
    bool other_fail() const { return rdstate() & ios_defs::otherfailbit; }
    bool eof() const { return rdstate() & ios_defs::eofbit; }

    explicit operator bool() const
    {
        return (m_stream_state == 0) || (m_stream_state == ios_defs::eofbit);
    }

    ios_defs::iostate exceptions() const { return m_exception; }
    void exceptions(ios_defs::iostate e)
    {
        m_exception = e;
        clear(m_stream_state);
    }

    void handle_exception(std::exception_ptr ex)
    {
        if (!ex) return;
        try
        {
            std::rethrow_exception(ex);
        }
        catch (device_error&)
        {
            if (!m_exp_dev_fail)
                m_exp_dev_fail = std::current_exception();
            setstate(ios_defs::devfailbit);
        }
        catch (cvt_error&)
        {
            if (!m_exp_cvt_fail)
                m_exp_cvt_fail = std::current_exception();
            setstate(ios_defs::cvtfailbit);
        }
        catch (stream_error&)
        {
            if (!m_exp_str_fail)
                m_exp_str_fail = std::current_exception();
            setstate(ios_defs::strfailbit);
        }
        catch (eof_error&)
        {
            setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            if (!m_exp_other_fail)
                m_exp_other_fail = std::current_exception();
            setstate(ios_defs::otherfailbit);
        }
    }

private:
    ios_defs::iostate  m_exception = ios_defs::goodbit;
    ios_defs::iostate  m_stream_state = ios_defs::goodbit;
    std::exception_ptr m_exp_dev_fail = std::exception_ptr{};
    std::exception_ptr m_exp_cvt_fail = std::exception_ptr{};
    std::exception_ptr m_exp_str_fail = std::exception_ptr{};
    std::exception_ptr m_exp_other_fail = std::exception_ptr{};
};

template <typename TChar> class locale;

template <typename TChar> class ios_base;
template <>
class ios_base<void>
{
    template <typename>
    friend class ios_base;

    inline static std::atomic<size_t> s_top = 0;
};

template <typename TChar>
class ios_base
{
public:
    using event_callback = std::function<std::shared_ptr<void>(const locale<TChar>&, std::shared_ptr<void>)>;

public:
    ios_base() = default;

public:
    ios_defs::fmtflags flags() const { return m_flags; }
    ios_defs::fmtflags flags(ios_defs::fmtflags fmtfl)
    {
      ios_defs::fmtflags old = m_flags;
      m_flags = fmtfl;
      return old;
    }

    ios_defs::fmtflags setf(ios_defs::fmtflags fmtfl)
    {
      ios_defs::fmtflags old = m_flags;
      m_flags |= fmtfl;
      return old;
    }

    ios_defs::fmtflags setf(ios_defs::fmtflags fmtfl, ios_defs::fmtflags msk)
    {
      ios_defs::fmtflags old = m_flags;
      m_flags &= ~msk;
      m_flags |= (fmtfl & msk);
      return old;
    }

    void unsetf(ios_defs::fmtflags msk) { m_flags &= ~msk; }

    /**
     * @lang{ZH}
     * 获取/设置浮点精度。
     * @note 精度以 std::uint8_t 存储，取值范围被有意限制在 0..255。这与标准
     * std::ios_base 使用 std::streamsize 不同：本库不支持大于 255 的精度，
     * 更大的值无法通过本接口表达（参数类型即为 std::uint8_t）。此为有意设计。
     * @endif
     *
     * @lang{EN}
     * Get/set the floating-point precision.
     * @note The precision is stored as a std::uint8_t and is intentionally limited
     * to the range 0..255. Unlike the standard std::ios_base, which uses
     * std::streamsize, this library does not support precision values greater than
     * 255; larger values cannot be expressed through this interface (the parameter
     * type is std::uint8_t itself). This is by design.
     * @endif
     */
    std::uint8_t precision() const { return m_precision; }
    std::uint8_t precision(std::uint8_t prec)
    {
        std::uint8_t old = m_precision;
        m_precision = prec;
        return old;
    }

    /**
     * @lang{ZH}
     * 获取/设置字段宽度。
     * @note 宽度以 std::uint8_t 存储，取值范围被有意限制在 0..255。这与标准
     * std::ios_base 使用 std::streamsize 不同：本库不支持大于 255 的字段宽度，
     * 更大的值无法通过本接口表达（参数类型即为 std::uint8_t）。此为有意设计。
     * @endif
     *
     * @lang{EN}
     * Get/set the field width.
     * @note The width is stored as a std::uint8_t and is intentionally limited to
     * the range 0..255. Unlike the standard std::ios_base, which uses
     * std::streamsize, this library does not support field widths greater than 255;
     * larger values cannot be expressed through this interface (the parameter type
     * is std::uint8_t itself). This is by design.
     * @endif
     */
    std::uint8_t width() const { return m_width; }
    std::uint8_t width(std::uint8_t wide)
    {
        std::uint8_t old = m_width;
        m_width = wide;
        return old;
    }

    TChar fill() const noexcept { return m_fill; }
    TChar fill(TChar ch)
    {
        TChar old = m_fill;
        m_fill = ch;
        return old;
    }

public:
    /**
     * @lang{ZH}
     * 分配一个进程内唯一的存储索引。
     * @note numeric_limits<size_t>::max() 被保留作为“已耗尽”哨兵：索引空间在发放到
     * max()-1 后即视为耗尽，再次调用抛出 stream_error，而不是让计数器回绕并复用已发放
     * 的索引。采用 CAS 循环实现，保证并发调用下索引唯一且不越过耗尽点。
     * @endif
     *
     * @lang{EN}
     * Allocates a process-wide unique storage index.
     * @note numeric_limits<size_t>::max() is reserved as an "exhausted" sentinel: once
     * indices up to max()-1 have been handed out the space is considered exhausted and
     * a further call throws stream_error, instead of letting the counter wrap around
     * and reuse an already-issued index. Implemented with a CAS loop so that concurrent
     * calls stay unique and never step past the exhaustion point.
     * @endif
     *
     * @throws stream_error
     * @lang{ZH} 索引空间耗尽时抛出。 @endif
     * @lang{EN} Thrown when the index space is exhausted. @endif
     */
    static size_t xalloc()
    {
        size_t cur = ios_base<void>::s_top.load(std::memory_order_relaxed);
        do
        {
            if (cur == std::numeric_limits<size_t>::max())
                throw stream_error("ios_base::xalloc fails: storage index space exhausted");
        } while (!ios_base<void>::s_top.compare_exchange_weak(
                     cur, cur + 1, std::memory_order_relaxed));
        return cur;
    }

    std::shared_ptr<void> set_pword(size_t id, std::shared_ptr<void> pword)
    {
        if (auto it = m_pwords.find(id); it == m_pwords.end())
        {
            if (pword) m_pwords.emplace(id, std::move(pword));
            return nullptr;
        }
        else
        {
            auto res = std::move(it->second);
            if (pword) it->second = std::move(pword);
            else m_pwords.erase(it);
            return res;
        }
    }

    std::shared_ptr<void> get_pword(size_t id) const
    {
        auto it = m_pwords.find(id);
        if (it != m_pwords.end()) return it->second;
        return nullptr;
    }

    void register_callback(event_callback fn, size_t id)
    {
        m_callbacks.push_front({std::move(fn), id});
    }

protected:
    void access_callbacks(const locale<TChar>& new_loc)
    {
        std::exception_ptr throw_exception = nullptr;

        for (const auto& [cb, id] : m_callbacks)
        {
            try
            {
                auto pre = m_pwords.find(id);
                std::shared_ptr<void> old_data =
                    (pre != m_pwords.end()) ? pre->second : nullptr;

                std::shared_ptr<void> new_data = cb(new_loc, old_data);

                // Re-locate after the callback: cb may reentrantly mutate
                // m_pwords (via set_pword), which can rehash/erase and would
                // invalidate an iterator held across the call.
                auto it = m_pwords.find(id);
                if (new_data)
                {
                    if (it != m_pwords.end()) it->second = std::move(new_data);
                    else m_pwords.insert({id, std::move(new_data)});
                }
                else if (it != m_pwords.end())
                {
                    m_pwords.erase(it);
                }
            }
            catch(...)
            {
                if (!throw_exception)
                    throw_exception = std::current_exception();
            }
        }

        if (throw_exception)
            std::rethrow_exception(throw_exception);
    }

protected:
    ios_defs::fmtflags m_flags     = ios_defs::skipws | ios_defs::dec;
    std::uint8_t       m_precision = 6;
    std::uint8_t       m_width     = 0;
    TChar              m_fill      = (TChar)' ';

    std::unordered_map<size_t, std::shared_ptr<void>> m_pwords;
    std::forward_list<std::pair<event_callback, size_t>> m_callbacks;
};

template <typename TChar>
inline void boolalpha(ios_base<TChar>& base)
{
    base.setf(ios_defs::boolalpha);
}

template <typename TChar>
inline void noboolalpha(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::boolalpha);
}

template <typename TChar>
inline void showbase(ios_base<TChar>& base)
{
    base.setf(ios_defs::showbase);
}

template <typename TChar>
inline void noshowbase(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::showbase);
}

template <typename TChar>
inline void showpoint(ios_base<TChar>& base)
{
    base.setf(ios_defs::showpoint);
}

template <typename TChar>
inline void noshowpoint(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::showpoint);
}

template <typename TChar>
inline void showpos(ios_base<TChar>& base)
{
    base.setf(ios_defs::showpos);
}

template <typename TChar>
inline void noshowpos(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::showpos);
}

template <typename TChar>
inline void skipws(ios_base<TChar>& base)
{
    base.setf(ios_defs::skipws);
}

template <typename TChar>
inline void noskipws(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::skipws);
}

template <typename TChar>
inline void uppercase(ios_base<TChar>& base)
{
    base.setf(ios_defs::uppercase);
}

template <typename TChar>
inline void nouppercase(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::uppercase);
}

template <typename TChar>
inline void appmode(ios_base<TChar>& base)
{
    base.setf(ios_defs::appmode);
}

template <typename TChar>
inline void noappmode(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::appmode);
}

template <typename TChar>
inline void unitbuf(ios_base<TChar>& base)
{
    base.setf(ios_defs::unitbuf);
}

template <typename TChar>
inline void nounitbuf(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::unitbuf);
}

template <typename TChar>
inline void internal(ios_base<TChar>& base)
{
    base.setf(ios_defs::internal, ios_defs::adjustfield);
}

template <typename TChar>
inline void left(ios_base<TChar>& base)
{
    base.setf(ios_defs::left, ios_defs::adjustfield);
}

template <typename TChar>
inline void right(ios_base<TChar>& base)
{
    base.setf(ios_defs::right, ios_defs::adjustfield);
}

template <typename TChar>
inline void dec(ios_base<TChar>& base)
{
    base.setf(ios_defs::dec, ios_defs::basefield);
}

template <typename TChar>
inline void hex(ios_base<TChar>& base)
{
    base.setf(ios_defs::hex, ios_defs::basefield);
}

template <typename TChar>
inline void oct(ios_base<TChar>& base)
{
    base.setf(ios_defs::oct, ios_defs::basefield);
}

template <typename TChar>
inline void fixed(ios_base<TChar>& base)
{
    base.setf(ios_defs::fixed, ios_defs::floatfield);
}

template <typename TChar>
inline void scientific(ios_base<TChar>& base)
{
    base.setf(ios_defs::scientific, ios_defs::floatfield);
}

template <typename TChar>
inline void hexfloat(ios_base<TChar>& base)
{
    base.setf(ios_defs::fixed | ios_defs::scientific, ios_defs::floatfield);
}

template <typename TChar>
inline void defaultfloat(ios_base<TChar>& base)
{
    base.unsetf(ios_defs::floatfield);
}

template <typename TStream>
struct sync
{
    sync(TStream& str)
        : stream(str)
    {
        stream.io_mutex().lock();
    }

    ~sync()
    {
        stream.io_mutex().unlock();
    }

    sync(const sync&) = delete;
    sync& operator=(const sync&) = delete;

    TStream& stream;
};
}