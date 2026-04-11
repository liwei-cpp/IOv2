#pragma once

#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>

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

struct _Setprecision { int m_n; };
inline _Setprecision setprecision(int n) { return { n }; }

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

struct _Setw { int m_n; };
inline _Setw setw(int n) { return { n }; }

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