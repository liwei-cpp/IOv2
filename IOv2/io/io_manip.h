#pragma once

#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>

namespace IOv2
{
struct _Resetiosflags { ios_defs::fmtflags _M_mask; };
inline _Resetiosflags resetiosflags(ios_defs::fmtflags __mask) { return { __mask }; }

template <ostream_type T>
inline T& operator << (T& os, _Resetiosflags f)
{
    os.setf(ios_defs::fmtflags(0), f._M_mask);
    return os;
}

template <istream_type T>
inline T& operator >> (T& is, _Resetiosflags f)
{
    is.setf(ios_defs::fmtflags(0), f._M_mask);
    return is;
}

struct _Setiosflags { ios_defs::fmtflags _M_mask; };
inline _Setiosflags setiosflags(ios_defs::fmtflags __mask) { return { __mask }; }

template <ostream_type T>
inline T& operator << (T& os, _Setiosflags f)
{
    os.setf(f._M_mask);
    return os;
}

template <istream_type T>
inline T& operator >> (T& is, _Setiosflags f)
{
    is.setf(f._M_mask);
    return is;
}

struct _Setbase { int _M_base; };
inline _Setbase setbase(int __base) { return { __base }; }

template <ostream_type T>
inline T& operator << (T& os, _Setbase f)
{
    os.setf(f._M_base ==  8 ? ios_defs::oct :
        f._M_base == 10 ? ios_defs::dec :
        f._M_base == 16 ? ios_defs::hex :
        ios_defs::fmtflags(0), ios_defs::basefield);
    return os;
}

template <istream_type T>
inline T& operator >> (T& is, _Setbase f)
{
    is.setf(f._M_base ==  8 ? ios_defs::oct :
        f._M_base == 10 ? ios_defs::dec :
        f._M_base == 16 ? ios_defs::hex :
        ios_defs::fmtflags(0), ios_defs::basefield);
    return is;
}

template<typename _CharT> struct _Setfill { _CharT _M_c; };
template<typename _CharT>
inline _Setfill<_CharT> setfill(_CharT __c) { return { __c }; }

template <ostream_type T>
inline T& operator << (T& os, _Setfill<typename T::char_type> f)
{
    os.fill(f._M_c);
    return os;
}

template <istream_type T>
inline T& operator >> (T& is, _Setfill<typename T::char_type> f)
{
    is.fill(f._M_c);
    return is;
}

struct _Setprecision { int _M_n; };
inline _Setprecision setprecision(int __n) { return { __n }; }

template <ostream_type T>
inline T& operator << (T& os, _Setprecision f)
{
    os.precision(f._M_n);
    return os;
}

template <istream_type T>
inline T& operator >> (T& is, _Setprecision f)
{
    is.precision(f._M_n);
    return is;
}

struct _Setw { int _M_n; };
inline _Setw setw(int __n) { return { __n }; }

template <ostream_type T>
inline T& operator << (T& os, _Setw f)
{
    os.width(f._M_n);
    return os;
}

template <istream_type T>
inline T& operator >> (T& is, _Setw f)
{
    is.width(f._M_n);
    return is;
}

template<typename _MoneyT> struct _Put_money { const _MoneyT& _M_mon; bool _M_intl; };
template<typename _MoneyT>
inline _Put_money<_MoneyT> put_money(const _MoneyT& __mon, bool __intl = false) { return { __mon, __intl }; }

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
        
        return mp->put(s, f._M_intl, io, f._M_mon);
    }
};

template<typename _MoneyT> struct _Get_money { _MoneyT& _M_mon; bool _M_intl; };
template<typename _MoneyT>
inline _Get_money<_MoneyT> get_money(_MoneyT& __mon, bool __intl = false) { return { __mon, __intl }; }

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

        return mp->get(s, s_end, f._M_intl, io, f._M_mon);
    }
};

template<typename _CharT> struct _Put_time { const std::tm* tmb; const _CharT* fmt; };
template<typename _CharT>
inline _Put_time<_CharT> put_time(const std::tm* __tmb, const _CharT* __fmt) { return { __tmb, __fmt }; }

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
inline _Get_time<_CharT> get_time(std::tm* __tmb, const _CharT* __fmt) { return { __tmb, __fmt }; }

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