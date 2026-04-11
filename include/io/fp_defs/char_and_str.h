#pragma once
#include <algorithm>
#include <limits>
#include <string>
#include <type_traits>
#include <io/io_base.h>
#include <io/fp_defs/base_fp.h>
#include <facet/ctype.h>
#include <locale/locale.h>

namespace IOv2
{
template <typename TIter, typename TChar>
    requires (std::is_same_v<TChar, typename TIter::value_type>)
TIter ostream_insert(TIter iter, ios_base<TChar>& io, const TChar* s, std::streamsize n)
{
    const std::streamsize w = io.width();
    if (w > n)
    {
        const bool left = ((io.flags() & ios_defs::adjustfield) == ios_defs::left);
        const TChar f = io.fill();
        if (!left)
            iter = std::fill_n(iter, w - n, f);
        iter = std::copy(s, s + n, iter);
        if (left)
            iter = std::fill_n(iter, w - n, f);
    }
    else
        iter = std::copy(s, s + n, iter);
    io.width(0);
    return iter;
}

template <typename TIter, std::sentinel_for<TIter> TSent, typename TChar>
    requires (std::is_same_v<TChar, typename TIter::value_type>)
TIter istream_extract(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, TChar* s, std::streamsize num)
{
    std::streamsize width = io.width();
    if (0 < width && width < num)
        num = width;

    if (num <= 1)
    {
        if (num == 1) *s = TChar{};
        io.width(0);
        throw stream_error("DO not have enough buffer to save character");
    }

    auto ct = loc.template get<ctype<TChar>>();
    if (!ct)
        throw stream_error("cannot get ctype facet");

    if (iter == iter_end) throw stream_error("cannot extract character from empty stream");

    std::streamsize extracted = 0;

    while (extracted < num - 1
           && (iter != iter_end)
           && !(ct->is_any(base_ft<ctype>::space, *iter)))
    {
        *s++ = *iter;
        ++extracted;
        ++iter;
    }

    *s = TChar{};
    
    io.width(0);

    return iter;
}

template <typename TChar>
struct writer<TChar, TChar>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>&, TChar c)
    {
        if (io.width() != 0)
            return ostream_insert(iter, io, &c, 1);
        *iter++ = c;
        return iter;
    }
};

template <typename TChar>
struct reader<TChar, TChar>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>&, TChar& c)
    {
        if (iter == iter_end)
            throw stream_error("Cannot parse character");

        c = *iter;
        return ++iter;
    }
};

template <typename TChar>
struct writer<TChar, char>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>& loc, char c)
    {
        auto mp = loc.template get<ctype<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        TChar wc = mp->widen(c);
        if (io.width() != 0)
            return ostream_insert(iter, io, &wc, 1);
        *iter++ = wc;
        return iter;
    }
};

template <>
struct writer<char, char>
{
    template <typename TIter>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, char c)
    {
        auto mp = loc.template get<ctype<char>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        char wc = mp->widen(c);
        if (io.width() != 0)
            return ostream_insert(iter, io, &wc, 1);
        *iter++ = wc;
        return iter;
    }
};

template <typename TChar>
struct writer<TChar, TChar*>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>&, const TChar* c)
    {
        if (c == nullptr) throw IOv2::stream_error("Cannot write NULL character sequence");

        size_t n = 0;
        for (const TChar* ptr = c; *ptr != 0; ++ptr, ++n);

        return ostream_insert(iter, io, c, n);
    }
};

template <typename TChar>
struct writer<TChar, const TChar*>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>& loc, const TChar* c)
    {
        return writer<TChar, TChar*>::swrite(iter, io, loc, c);
    }
};

template <typename TChar>
struct reader<TChar, TChar*>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, TChar* c)
    {
        if (c == nullptr) throw IOv2::stream_error("Cannot read NULL character sequence");
        
        std::streamsize n = std::numeric_limits<std::streamsize>::max();
        return istream_extract(iter, iter_end, io, loc, c, n);
    }
};

template <typename TChar, size_t N>
struct reader<TChar, TChar[N]>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, TChar* c)
    {
        if constexpr (N <= 1)
            throw IOv2::stream_error("Character buffer not enough");
        
        std::streamsize n = N;
        return istream_extract(iter, iter_end, io, loc, c, n);
    }
};

template <>
struct reader<char, unsigned char*>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, unsigned char* c)
    {
        return reader<char, char*>::sread(iter, iter_end, io, loc, reinterpret_cast<char*>(c));
    }
};

template <size_t N>
struct reader<char, unsigned char[N]>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, unsigned char* c)
    {
        if constexpr (N <= 1)
            throw IOv2::stream_error("Character buffer not enough");
        std::streamsize n = N;
        return istream_extract(iter, iter_end, io, loc, reinterpret_cast<char*>(c), n);
    }
};

template <>
struct reader<char, signed char*>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, signed char* c)
    {
        return reader<char, char*>::sread(iter, iter_end, io, loc, reinterpret_cast<char*>(c));
    }
};

template <size_t N>
struct reader<char, signed char[N]>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, signed char* c)
    {
        if constexpr (N <= 1)
            throw IOv2::stream_error("Character buffer not enough");
        std::streamsize n = N;
        return istream_extract(iter, iter_end, io, loc, reinterpret_cast<char*>(c), n);
    }
};

template <>
struct reader<char, unsigned char>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, unsigned char& c)
    {
        char tmp;
        auto res = reader<char, char>::sread(iter, iter_end, io, loc, tmp);
        c = tmp;
        return res;
    }
};

template <>
struct reader<char, signed char>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, signed char& c)
    {
        char tmp;
        auto res = reader<char, char>::sread(iter, iter_end, io, loc, tmp);
        c = tmp;
        return res;
    }
};

template <typename TChar, typename TTraits, typename TAlloc>
struct writer<TChar, std::basic_string<TChar, TTraits, TAlloc>>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>&, const std::basic_string<TChar, TTraits, TAlloc>& str)
    {
        return ostream_insert(iter, io, str.data(), str.size());
    }
};

template <typename TChar, typename TTraits, typename TAlloc>
struct reader<TChar, std::basic_string<TChar, TTraits, TAlloc>>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, std::basic_string<TChar, TTraits, TAlloc>& str)
    {
        str.erase();
        TChar buf[128];
        size_t len = 0;
        auto w = io.width();
        size_t n = w > 0  ? static_cast<size_t>(w) : str.max_size();
        size_t extracted = 0;

        auto ct = loc.template get<ctype<TChar>>();
        if (!ct)
            throw stream_error("cannot get ctype facet");
        while (extracted < n
               && (iter != iter_end)
               && !(ct->is_any(base_ft<ctype>::space, *iter)))
        {
            if (len == 128)
            {
                str.append(buf, 128);
                len = 0;
            }
            buf[len++] = *iter;
            ++extracted;
            ++iter;
        }
        str.append(buf, len);

        io.width(0);
        
        return iter;
    }
};
}