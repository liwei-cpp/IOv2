#pragma once

#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace IOv2
{
struct _Resetiosflags { ios_defs::fmtflags m_mask; };
inline _Resetiosflags resetiosflags(ios_defs::fmtflags mask) { return { mask }; }

template <ostream_type T>
inline T& operator << (T& os, _Resetiosflags f)
{
    os.setf(ios_defs::fmtflags(0), f.m_mask);
    return os;
}

template <istream_type T>
inline T& operator >> (T& is, _Resetiosflags f)
{
    is.setf(ios_defs::fmtflags(0), f.m_mask);
    return is;
}

struct _Setiosflags { ios_defs::fmtflags m_mask; };
inline _Setiosflags setiosflags(ios_defs::fmtflags mask) { return { mask }; }

template <ostream_type T>
inline T& operator << (T& os, _Setiosflags f)
{
    os.setf(f.m_mask);
    return os;
}

template <istream_type T>
inline T& operator >> (T& is, _Setiosflags f)
{
    is.setf(f.m_mask);
    return is;
}

struct _Setbase { int m_base; };
inline _Setbase setbase(int base) { return { base }; }

template <ostream_type T>
inline T& operator << (T& os, _Setbase f)
{
    os.setf(f.m_base ==  8 ? ios_defs::oct :
        f.m_base == 10 ? ios_defs::dec :
        f.m_base == 16 ? ios_defs::hex :
        ios_defs::fmtflags(0), ios_defs::basefield);
    return os;
}

template <istream_type T>
inline T& operator >> (T& is, _Setbase f)
{
    is.setf(f.m_base ==  8 ? ios_defs::oct :
        f.m_base == 10 ? ios_defs::dec :
        f.m_base == 16 ? ios_defs::hex :
        ios_defs::fmtflags(0), ios_defs::basefield);
    return is;
}

template<typename _CharT> struct _Setfill { _CharT m_c; };
template<typename _CharT>
inline _Setfill<_CharT> setfill(_CharT c) { return { c }; }

template <ostream_type T>
inline T& operator << (T& os, _Setfill<typename T::char_type> f)
{
    os.fill(f.m_c);
    return os;
}

template <istream_type T>
inline T& operator >> (T& is, _Setfill<typename T::char_type> f)
{
    is.fill(f.m_c);
    return is;
}

struct _Setprecision { std::uint8_t m_n; };

/**
 * @lang{ZH}
 * @brief 构造设置浮点精度的操纵符。
 *
 * 精度以 `std::uint8_t` 存储（见 `ios_base::precision`），有效范围 0..255。参数取
 * `size_t` 而非 `std::uint8_t`，是为了让越界值在此处**显式报错**而不是被静默回绕：
 * 若形参本身就是 `std::uint8_t`，`setprecision(300)` 会经隐式转换悄悄变成 44，
 * 而运行期变量连编译警告都不会有。
 * @param n 目标精度，必须落在 0..255。
 * @return 可用于 `os << setprecision(n)` / `is >> setprecision(n)` 的操纵符。
 * @throw stream_error 若 `n > 255`。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that sets the floating-point precision.
 *
 * The precision is stored as a `std::uint8_t` (see `ios_base::precision`), so the valid
 * range is 0..255. The parameter is a `size_t` rather than a `std::uint8_t` so that an
 * out-of-range value is **reported explicitly** here instead of silently wrapping: with a
 * `std::uint8_t` parameter, `setprecision(300)` would quietly become 44 via the implicit
 * conversion, and a run-time argument would not even produce a compiler warning.
 * @param n The target precision; must lie in 0..255.
 * @return A manipulator usable as `os << setprecision(n)` / `is >> setprecision(n)`.
 * @throw stream_error If `n > 255`.
 * @endif
 */
inline _Setprecision setprecision(size_t n)
{
    if (n > std::numeric_limits<std::uint8_t>::max())
        throw stream_error("setprecision fail: precision out of range (0..255)");
    return { static_cast<std::uint8_t>(n) };
}

template <ostream_type T>
inline T& operator << (T& os, _Setprecision f)
{
    os.precision(f.m_n);
    return os;
}

template <istream_type T>
inline T& operator >> (T& is, _Setprecision f)
{
    is.precision(f.m_n);
    return is;
}

struct _Setw { std::uint8_t m_n; };

/**
 * @lang{ZH}
 * @brief 构造设置字段宽度的操纵符。
 *
 * 宽度以 `std::uint8_t` 存储（见 `ios_base::width`），有效范围 0..255。参数取 `size_t`
 * 而非 `std::uint8_t`，是为了让越界值在此处**显式报错**而不是被静默回绕。
 *
 * @warning 这不只是格式化精度问题，而是内存安全问题：`width == 0` 在提取端的语义是
 *          **不限长度**（见 `istream_extract`），因此若形参是 `std::uint8_t`，
 *          `setw(256)`、`setw(512)` 等会静默折叠为 0，把调用方本想加上的长度上限
 *          **反向解除**——`is >> setw(sizeof(buf)) >> ptr` 会一路写到遇见空白为止，
 *          造成缓冲区溢出，且运行期实参连编译警告都没有。改为在此抛异常后，
 *          `width == 0` 就只可能表示"调用方没有要求上限"这一种含义。
 * @param n 目标宽度，必须落在 0..255。
 * @return 可用于 `os << setw(n)` / `is >> setw(n)` 的操纵符。
 * @throw stream_error 若 `n > 255`。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that sets the field width.
 *
 * The width is stored as a `std::uint8_t` (see `ios_base::width`), so the valid range is
 * 0..255. The parameter is a `size_t` rather than a `std::uint8_t` so that an out-of-range
 * value is **reported explicitly** here instead of silently wrapping.
 *
 * @warning This is not merely a formatting concern but a memory-safety one: on the
 *          extraction side `width == 0` means **no length limit** (see `istream_extract`).
 *          With a `std::uint8_t` parameter, `setw(256)`, `setw(512)` and friends would
 *          silently fold to 0, **inverting** the very bound the caller meant to impose --
 *          `is >> setw(sizeof(buf)) >> ptr` would then write on until whitespace,
 *          overflowing the buffer, with no compiler warning for a run-time argument.
 *          Throwing here leaves `width == 0` with exactly one meaning: "the caller asked
 *          for no bound".
 * @param n The target width; must lie in 0..255.
 * @return A manipulator usable as `os << setw(n)` / `is >> setw(n)`.
 * @throw stream_error If `n > 255`.
 * @endif
 */
inline _Setw setw(size_t n)
{
    if (n > std::numeric_limits<std::uint8_t>::max())
        throw stream_error("setw fail: width out of range (0..255)");
    return { static_cast<std::uint8_t>(n) };
}

template <ostream_type T>
inline T& operator << (T& os, _Setw f)
{
    os.width(f.m_n);
    return os;
}

template <istream_type T>
inline T& operator >> (T& is, _Setw f)
{
    is.width(f.m_n);
    return is;
}

template<typename _MoneyT> struct _Put_money { const _MoneyT& m_mon; bool m_intl; };
template<typename _MoneyT>
inline _Put_money<_MoneyT> put_money(const _MoneyT& mon, bool intl = false) { return { mon, intl }; }

template <typename TChar, typename TMoney>
struct writer<TChar, _Put_money<TMoney>>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, _Put_money<TMoney> f)
    {
        auto mp = loc.template get<monetary<TChar>>();
        if (!mp)
            throw stream_error("cannot get monetary facet");
        
        return mp->put(s, f.m_intl, io, f.m_mon);
    }
};

template<typename _MoneyT> struct _Get_money { _MoneyT& m_mon; bool m_intl; };
template<typename _MoneyT>
inline _Get_money<_MoneyT> get_money(_MoneyT& mon, bool intl = false) { return { mon, intl }; }

template <typename TChar, typename TMoney>
struct reader<TChar, _Get_money<TMoney>>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter s, TIter s_end, ios_base<TChar>& io, const locale<TChar>& loc, _Get_money<TMoney>& f)
    {
        auto mp = loc.template get<monetary<TChar>>();
        if (!mp)
            throw stream_error("cannot get monetary facet");

        return mp->get(s, s_end, f.m_intl, io, f.m_mon);
    }
};

template<typename _CharT> struct _Put_time { const std::tm* tmb; const _CharT* fmt; };
template<typename _CharT>
inline _Put_time<_CharT> put_time(const std::tm* tmb, const _CharT* fmt) { return { tmb, fmt }; }

template <typename TChar>
struct writer<TChar, _Put_time<TChar>>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, _Put_time<TChar> f)
    {
        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");
        
        return mp->put(s, *(f.tmb), f.fmt);
    }
};

template<typename _CharT> struct _Get_time { std::tm* tmb; const _CharT* fmt; };
template<typename _CharT>
inline _Get_time<_CharT> get_time(std::tm* tmb, const _CharT* fmt) { return { tmb, fmt }; }

template <typename TChar>
struct reader<TChar, _Get_time<TChar>>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter s, TIter s_end, ios_base<TChar>& io, const locale<TChar>& loc, _Get_time<TChar>& f)
    {
        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        typename timeio<TChar>::get_context tmp;
        auto res = mp->get(s, s_end, tmp, f.fmt);
        *(f.tmb) = tmp.to_tm();

        return res;
    }
};
}