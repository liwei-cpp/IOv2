#pragma once
#include <type_traits>
#include <io/io_base.h>
#include <io/fp_defs/base_fp.h>
#include <facet/timeio.h>
#include <locale/locale.h>

namespace IOv2
{
template <typename TChar>
struct parse_context_type<TChar, std::tm>
{
    using type = time_parse_context<TChar, true, true, false>;
};


template <typename TChar>
struct writer<TChar, std::tm>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, const std::tm& value)
    {
        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        return mp->put(s, value, 'c');
    }
};

template <typename TChar>
struct reader<TChar, time_parse_context<TChar, true, true, false>>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, time_parse_context<TChar, true, true, false>& value)
    {
        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        return mp->get(iter, iter_end, value, 'c');
    }
};
}
