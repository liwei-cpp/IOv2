#pragma once
#include <common/clocale_wrapper.h>
#include <cvt/cvt_facilities.h>
#include <facet/ctype_details.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <langinfo.h>
#include <nl_types.h>

namespace IOv2::FacetHelper
{
    // Convert the first character of a (possibly multibyte) C string `ptr`
    // to a narrow char in the given locale, returning '\0' if conversion
    // is not representable.
    //
    // Preconditions (not validated):
    //   - `ptr` is non-null.
    //   - `ptr` points to a NUL-terminated C string.
    inline char string_to_char_convert(const char* ptr, const std::string& locale_name)
    {
        assert(ptr != nullptr);
        if (*ptr == '\0')
            return '\0';
        if (ptr[1] == '\0')
            return *ptr;

        // Convert char to wchar_t then narrow down
        std::wstring wide_str = detail::to_wstring(ptr, locale_name);
        if (wide_str.empty()) return '\0';

        ctype_conf<wchar_t> tmp_ctype(locale_name);
        auto res = tmp_ctype.narrow(wide_str[0]);
        return res.has_value() ? res.value() : '\0';
    }

    // Internal helper function.
    //
    // Preconditions (not validated):
    //   - `grouping` is non-empty (only the first precondition is asserted).
    //   - `[first, last)` is a valid range, i.e. `first <= last`. Passing
    //     `first > last` causes the inner copy loop to never terminate and
    //     write past the output buffer.
    //   - `s` points to an output buffer with sufficient capacity to hold
    //     `(last - first)` characters plus one separator per non-leading
    //     group; callers are responsible for sizing the buffer.
    template <typename CharT>
    inline CharT* add_grouping(CharT* s, CharT sep, const std::vector<uint8_t>& grouping,
                               const CharT* first, const CharT* last)
    {
        assert(!grouping.empty());
        assert(first <= last);

        size_t idx = 0;
        size_t ctr = 0;
        const size_t max_idx = grouping.size() - 1;

        while (last - first > grouping[idx]
                && (grouping[idx] > 0))
        {
            last -= grouping[idx];
            idx < max_idx ? ++idx : ++ctr;
        }

        s = std::copy(first, last, s);
        first = last;

        const uint8_t n = grouping[idx];
        while (ctr--)
        {
            *s++ = sep;
            s = std::copy_n(first, n, s);
            first += n;
        }

        while (idx--)
        {
            *s++ = sep;
            const uint8_t m = grouping[idx];
            s = std::copy_n(first, m, s);
            first += m;
        }
        return s;
    }

    // Normalise a raw grouping vector and discard it if it carries no usable
    // constraint. Sentinel convention (internal, platform-independent):
    //   0   — repeat the previous group size indefinitely
    //   255 — no further grouping
    //   1–254 — explicit group size
    //
    // POSIX CHAR_MAX (127 on signed-char platforms) is normalised to 255 so
    // that callers never need to know the platform's char signedness.
    inline void adjust_grouping(std::vector<uint8_t>& grouping)
    {
        constexpr uint8_t posix_sentinel = static_cast<uint8_t>(std::numeric_limits<char>::max());
        if constexpr (posix_sentinel != std::numeric_limits<uint8_t>::max())
            for (auto& g : grouping)
                if (g == posix_sentinel) g = std::numeric_limits<uint8_t>::max();

        if ((!grouping.empty()) &&
            (grouping[0] > 0) &&
            (grouping[0] != std::numeric_limits<uint8_t>::max()))
            return;
        else
            grouping.clear();
    }

    // Internal helper function. Callers are responsible for ensuring that both
    // grouping and grouping_tmp are non-empty; this function does not validate
    // those preconditions. Callers guarantee grouping is non-empty because
    // grouping_tmp is only populated inside !grouping.empty() branches, so
    // the non-emptiness of grouping_tmp implies the non-emptiness of grouping.
    //
    // grouping uses an internal uint8_t sentinel convention:
    //   0   — repeat the previous group size for all remaining groups
    //   255 — no further grouping (normalised from POSIX CHAR_MAX at load time)
    //   1–254 — explicit group size
    inline bool verify_grouping(const std::vector<uint8_t>& grouping,
                                const std::vector<uint8_t>& grouping_tmp)
    {
        assert(!grouping.empty());
        assert(!grouping_tmp.empty());

        size_t i = grouping_tmp.size() - 1;
        const size_t min_val = std::min(i, grouping.size() - 1);
        bool test = true;

        // Parsed number groupings have to match the
        // numpunct::grouping string exactly, starting at the
        // right-most point of the parsed sequence of elements ...
        for (size_t j = 0; j < min_val && test; --i, ++j)
            test = grouping_tmp[i] == grouping[j];
        for (; i && test; --i)
            test = grouping_tmp[i] == grouping[min_val];
        // ... but the first parsed grouping can be <= numpunct grouping.
        // Skip this check when grouping[min_val] is 0 (repeat-last, no bound)
        // or 255 (no-further-grouping sentinel).
        if (grouping[min_val] > 0
            && grouping[min_val] != std::numeric_limits<uint8_t>::max())
            test &= grouping_tmp[0] <= grouping[min_val];
        return test;
    }

    // Portable nl_langinfo function (use narrow POSIX item and convert to wide)
    template <typename CharT>
    requires std::is_same_v<CharT, wchar_t> ||
        (std::is_same_v<CharT, char32_t> &&
            (sizeof(char32_t) == sizeof(wchar_t)) &&
            (static_cast<wchar_t>(U'李') == L'李') &&
            (static_cast<char32_t>(L'伟') == U'伟'))
    inline CharT nl_langinfo_char(nl_item item, const std::string& locale_name, CharT default_char)
    {
        clocale_wrapper inter_locale(locale_name.c_str());
        clocale_user guard(inter_locale);

        const char* narrow = nl_langinfo(item);
        if (!narrow || narrow[0] == '\0')
            return default_char;
        if constexpr (std::is_same_v<CharT, wchar_t>)
        {
            auto wide = detail::to_wstring(narrow, locale_name);
            return wide.empty() ? default_char : wide[0];
        }
        else
        {
            auto wide = detail::to_u32string(narrow, locale_name);
            return wide.empty() ? default_char : wide[0];
        }
    }
}
