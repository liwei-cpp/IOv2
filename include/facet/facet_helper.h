#pragma once
#include <langinfo.h>
#include <nl_types.h>
#include <limits>
#include <string>
#include <cvt/cvt_facilities.h>
#include <facet/ctype_details.h>
#include <io/io_base.h>

namespace IOv2::FacetHelper
{
    inline char string_to_char_convert(char* ptr, const std::string& locale_name)
    {
        if (*ptr == '\0')
            return '\0';
        if (ptr[1] == '\0')
            return *ptr;

        // Convert char to wchar_t then narrow down
        std::wstring wide_str = to_wstring(ptr, locale_name);
        if (wide_str.empty()) return '\0';
        
        ctype_conf<wchar_t> tmp_ctype(locale_name);
        auto res = tmp_ctype.narrow(wide_str[0]);
        return res.has_value() ? res.value() : '\0';
    }
    
    template <typename CharT>
    inline CharT* add_grouping(CharT* s, CharT sep, const std::vector<uint8_t>& grouping,
                               const CharT* first, const CharT* last)
    {
        size_t idx = 0;
        size_t ctr = 0;
        size_t gsize = grouping.size();

        while (last - first > grouping[idx]
                && (grouping[idx] > 0))
        {
            last -= grouping[idx];
            idx < gsize - 1 ? ++idx : ++ctr;
        }

        while (first != last)
            *s++ = *first++;

        while (ctr--)
        {
            *s++ = sep;
            for (char i = grouping[idx]; i > 0; --i)
                *s++ = *first++;
        }

        while (idx--)
        {
            *s++ = sep;
            for (char i = grouping[idx]; i > 0; --i)
                *s++ = *first++;
        }
        return s;
    }
    
    inline bool verify_grouping(const std::vector<uint8_t>& grouping,
                                const std::string& grouping_tmp)
    {
        const size_t n = grouping_tmp.size() - 1;
        const size_t min_val = std::min(n, size_t(grouping.size() - 1));
        size_t i = n;
        bool test = true;

        // Parsed number groupings have to match the
        // numpunct::grouping string exactly, starting at the
        // right-most point of the parsed sequence of elements ...
        for (size_t j = 0; j < min_val && test; --i, ++j)
            test = grouping_tmp[i] == grouping[j];
        for (; i && test; --i)
            test = grouping_tmp[i] == grouping[min_val];
        // ... but the first parsed grouping can be <= numpunct
        // grouping (only do the check if the numpunct char is > 0
        // because <= 0 means any size is ok).
        if (static_cast<signed char>(grouping[min_val]) > 0
            && grouping[min_val] != std::numeric_limits<char>::max())
            test &= grouping_tmp[0] <= grouping[min_val];
        return test;
    }

    template <typename T>
    inline auto string_convert(std::string_view str)
    {
        std::basic_string<T> res;
        res.reserve(str.size());
        for (char c : str)
            res.push_back(static_cast<T>(c));
        return res;
    }
    
    template <typename CharT>
    requires std::is_same_v<CharT, wchar_t> ||
        (std::is_same_v<CharT, char32_t> &&
            (sizeof(char32_t) == sizeof(wchar_t)) &&
            (static_cast<wchar_t>(U'李') == L'李') &&
            (static_cast<char32_t>(L'伟') == U'伟'))
    inline std::basic_string<CharT> nl_langinfo_w(nl_item item)
    {
        union { char *s; wchar_t *w; } u;
        u.s = nl_langinfo(item);
        return (CharT*)(u.w);
    }
}