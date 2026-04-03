#pragma once
#include <langinfo.h>
#include <nl_types.h>
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
    inline CharT* add_grouping(CharT* __s, CharT __sep, const std::vector<uint8_t>& grouping,
                               const CharT* __first, const CharT* __last)
    {
        size_t __idx = 0;
        size_t __ctr = 0;
        size_t __gsize = grouping.size();

        while (__last - __first > grouping[__idx]
                && (grouping[__idx] > 0))
        {
            __last -= grouping[__idx];
            __idx < __gsize - 1 ? ++__idx : ++__ctr;
        }

        while (__first != __last)
            *__s++ = *__first++;

        while (__ctr--)
        {
            *__s++ = __sep;
            for (char __i = grouping[__idx]; __i > 0; --__i)
                *__s++ = *__first++;
        }

        while (__idx--)
        {
            *__s++ = __sep;
            for (char __i = grouping[__idx]; __i > 0; --__i)
                *__s++ = *__first++;
        }
        return __s;
    }
    
    inline bool verify_grouping(const std::vector<uint8_t>& __grouping, 
                                const std::string& __grouping_tmp)
    {
        const size_t __n = __grouping_tmp.size() - 1;
        const size_t __min = std::min(__n, size_t(__grouping.size() - 1));
        size_t __i = __n;
        bool __test = true;
    
        // Parsed number groupings have to match the
        // numpunct::grouping string exactly, starting at the
        // right-most point of the parsed sequence of elements ...
        for (size_t __j = 0; __j < __min && __test; --__i, ++__j)
            __test = __grouping_tmp[__i] == __grouping[__j];
        for (; __i && __test; --__i)
            __test = __grouping_tmp[__i] == __grouping[__min];
        // ... but the first parsed grouping can be <= numpunct
        // grouping (only do the check if the numpunct char is > 0
        // because <= 0 means any size is ok).
        if (static_cast<signed char>(__grouping[__min]) > 0
            && __grouping[__min] != __gnu_cxx::__numeric_traits<char>::__max)
            __test &= __grouping_tmp[0] <= __grouping[__min];
        return __test;
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
        union { char *__s; wchar_t *__w; } __u;
        __u.__s = nl_langinfo(item);
        return (CharT*)(__u.__w);
    }
}