#pragma once
#include <iterator>
#include <type_traits>
#include <io/io_base.h>
#include <io/fp_defs/base_fp.h>
#include <facet/numeric.h>
#include <locale/locale.h>

namespace IOv2
{
template <typename TChar, typename TValue>
    requires std::is_arithmetic_v<TValue>
struct writer<TChar, TValue>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, TValue value)
    {
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        return mp->put(s, io, value);
    }
};

template <typename TChar, typename TValue>
    requires std::is_arithmetic_v<TValue>
struct reader<TChar, TValue>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter s, TSent s_end, ios_base<TChar>& io, const locale<TChar>& loc, TValue& value)
    {
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        return mp->get(s, s_end, io, value);
    }
};

template <typename TChar, typename TValue>
    requires (std::is_pointer_v<TValue>)
struct writer<TChar, TValue>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, TValue value)
    {
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        return mp->put(s, io, value);
    }
};

template <typename TChar, typename TValue>
    requires (std::is_pointer_v<TValue>)
struct reader<TChar, TValue>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter s, TSent s_end, ios_base<TChar>& io, const locale<TChar>& loc, TValue& value)
    {
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        void* tmp = nullptr;
        auto res = mp->get(s, s_end, io, tmp);
        value = reinterpret_cast<TValue>(tmp);
        return res;
    }
};
}
