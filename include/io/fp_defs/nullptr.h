#pragma once
#include <cstddef>
#include <string>

#include <type_traits>
#include <facets/ctype.h>
#include <io/io_base.h>
#include <io/fp_defs/base_fp.h>
#include <locale/locale.h>

namespace IOv2
{
template <typename TChar>
struct writer<TChar, std::nullptr_t>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter s, ios_base<TChar>&, const locale<TChar>& loc, std::nullptr_t)
    {
        const char* c_buf = "nullptr";
        auto mp = loc.template get<ctype<TChar>>();
        if (!mp)
            throw stream_error("cannot get ctype facet");

        return mp->widen_seq(c_buf, c_buf + 7, s);
    }
};

template <typename TChar, typename TValue>
    requires ((std::is_integral_v<TValue> || std::is_floating_point_v<TValue> || std::is_convertible_v<TValue, void*>)
              && (!std::is_same_v<TChar, TValue>)
              && (!std::is_same_v<char, TValue>))
struct reader<TChar, TValue>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter s, TSent s_end, ios_base<TChar>& io, const locale<TChar>& loc, TValue& value)
    {
        const char* c_buf = "nullptr";
        auto mp = loc.template get<ctype<TChar>>();
        if (!mp)
            throw stream_error("cannot get ctype facet");
        
        size_t i = 0;
        for (; i < 7; ++i)
        {
            if (s == s_end) break;
            if (*s != mp->widen(c_buf[i]))
                throw stream_error("get invalid character");
            ++s;
        }
        
        if (i != 7) throw eof_error{};
        
        return s;
    }
};
}
