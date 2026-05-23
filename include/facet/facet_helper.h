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

    // Normalise a raw POSIX-style grouping vector to the internal convention,
    // or discard it if it carries no usable constraint.
    //
    // Internal convention (uint8_t, platform-independent):
    //   1–255 — explicit group size.
    //   0     — stop: no further grouping after this point (the ONLY sentinel).
    //   The LAST element (if in 1–255) is implicitly repeated for all
    //   remaining groups. If the last element is 0, the explicit stop wins.
    //
    // Conversions applied to raw POSIX input:
    //   - POSIX "0 = repeat previous": the 0 and everything after it is
    //     dropped. POSIX [N1, ..., Nk, 0, ...] becomes [N1, ..., Nk] in our
    //     convention, because last-element implicit repeat already covers
    //     that semantic exactly.
    //   - POSIX CHAR_MAX (= stop): rewritten to internal 0. CHAR_MAX is 127
    //     on signed-char platforms and 255 on unsigned-char platforms;
    //     callers do not need to know the platform's char signedness after
    //     this step.
    //
    // Order matters: stripping POSIX "0 = repeat" MUST happen before
    // CHAR_MAX → 0 rewrite, otherwise our newly-introduced internal stop
    // would be mistaken for a POSIX repeat-previous marker and dropped.
    inline void adjust_grouping(std::vector<uint8_t>& grouping)
    {
        // Step 1: POSIX "0 = repeat previous" → drop the 0 and the tail.
        auto zero = std::find(grouping.begin(), grouping.end(), uint8_t{0});
        grouping.erase(zero, grouping.end());

        // Step 2: POSIX CHAR_MAX (= stop) → internal 0 sentinel, plus
        // canonical-form cleanup. After step 1 there are no 0s in the vector,
        // so the FIRST CHAR_MAX is the only point at which we introduce one,
        // and three cases fully cover the outcome:
        //   - at begin():   grouping would start with the stop sentinel,
        //                   i.e. no usable leading group ⇒ discard.
        //                   (This also subsumes the empty-vector case left
        //                   by step 1, since find on an empty range returns
        //                   end() == begin().)
        //   - in the tail:  rewrite to 0 and drop everything after it; those
        //                   bytes are dead code anyway because add_grouping
        //                   short-circuits on 0.
        //   - not found:    nothing to do.
        constexpr uint8_t posix_sentinel =
            static_cast<uint8_t>(std::numeric_limits<char>::max());
        auto stop = std::find(grouping.begin(), grouping.end(), posix_sentinel);
        if (stop == grouping.begin())
            grouping.clear();
        else if (stop != grouping.end())
        {
            *stop = 0;
            grouping.erase(std::next(stop), grouping.end());
        }
    }

    // Internal helper function. Callers are responsible for ensuring that both
    // grouping and grouping_tmp are non-empty; this function does not validate
    // those preconditions. Callers guarantee grouping is non-empty because
    // grouping_tmp is only populated inside !grouping.empty() branches, so
    // the non-emptiness of grouping_tmp implies the non-emptiness of grouping.
    //
    // grouping uses the internal uint8_t convention established by
    // adjust_grouping(): 1–255 are explicit group sizes, 0 is the sole
    // "stop" sentinel, and the last 1–255 element is implicitly repeated
    // for all remaining groups.
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
        // Skip this check when grouping[min_val] is 0 (stop sentinel,
        // no upper bound on the first parsed group).
        if (grouping[min_val] > 0)
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
