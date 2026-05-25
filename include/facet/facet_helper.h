#pragma once
#include <common/clocale_wrapper.h>
#include <cvt/cvt_facilities.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace IOv2::FacetHelper
{
    // Convert the first character of a (possibly multibyte) narrow string
    // `narrow` to a `char` in the given locale, returning '\0' if `narrow`
    // is empty or conversion is not representable.
    //
    // Suitable for fields read out of `lconv` / `nl_langinfo()` that
    // semantically represent a single user-visible character (e.g.
    // decimal_point, thousands_sep, mon_decimal_point, mon_thousands_sep).
    //
    // Why `const std::string&` and not `const char*`: `lconv*` and
    // `nl_langinfo()` pointers may be invalidated by any setlocale() /
    // uselocale() call. The conversion below transitively performs such
    // calls (via detail::to_wstring), so the input must be a caller-owned
    // snapshot rather than a borrowed pointer into the libc-owned locale
    // data. Taking std::string makes this contract unmissable at every
    // call site.
    inline char string_to_char_convert(const std::string& narrow,
                                       const std::string& locale_name)
    {
        if (narrow.empty()) return '\0';
        if (narrow.size() == 1) return narrow[0];

        // Convert (multi-byte) narrow to wchar_t then narrow down
        std::wstring wide_str = detail::to_wstring(narrow.c_str(), locale_name);
        if (wide_str.empty()) return '\0';

        // Narrow the first wide character back to a single byte in the target
        // locale. This reproduces ctype_conf<wchar_t>::narrow() (wctob() under
        // a thread-local locale switch) directly, instead of building a whole
        // ctype_conf<wchar_t> facet whose 256-entry widen table and wctype
        // masks this path never reads. The out_of_wchar_range guard used there
        // is unnecessary here: the value is a wchar_t and is therefore always
        // representable as wchar_t.
        clocale_wrapper loc(locale_name.c_str());
        clocale_user guard(loc);
        const int c = wctob(static_cast<wint_t>(wide_str[0]));
        return c == EOF ? '\0' : static_cast<char>(c);
    }

    // Convert the first character of a (possibly multibyte) narrow string
    // `narrow` to a wide character `CharT` in the given locale, returning
    // `default_char` when `narrow` is empty or conversion produces no
    // characters.
    //
    // Suitable for fields read out of `lconv` / `nl_langinfo()` that
    // semantically represent a single user-visible character (e.g.
    // decimal_point, thousands_sep, mon_decimal_point, mon_thousands_sep).
    //
    // `default_char` contract: returned verbatim on the empty-input and
    // empty-conversion paths; this function does NOT validate that
    // `default_char` is itself representable, well-formed, or otherwise
    // meaningful under `locale_name`. Callers are responsible for choosing a
    // sensible fallback (typically a plain ASCII-range value such as L' ',
    // L'.', U' ', or U'.'). Note the asymmetry with `string_to_char_convert`,
    // which uses a hard-coded `'\0'` fallback and accepts no caller default:
    // here the fallback is a parameter precisely so callers can pick a
    // locale-appropriate substitute, but the price is that validation is
    // their responsibility, not ours.
    //
    // Why `const std::string&` and not `const char*`: `lconv*` and
    // `nl_langinfo()` pointers may be invalidated by any setlocale() /
    // uselocale() call. The conversion below transitively performs such
    // calls (via detail::to_wstring/to_u32string), so the input must be a
    // caller-owned snapshot rather than a borrowed pointer into the
    // libc-owned locale data. Taking std::string makes this contract
    // unmissable at every call site.
    template <typename CharT>
        requires std::is_same_v<CharT, wchar_t> ||
            (std::is_same_v<CharT, char32_t> &&
                (sizeof(char32_t) == sizeof(wchar_t)) &&
                (static_cast<wchar_t>(U'李') == L'李') &&
                (static_cast<char32_t>(L'伟') == U'伟'))
    inline CharT string_to_widechar_convert(const std::string& narrow,
                                            const std::string& locale_name,
                                            CharT default_char)
    {
        if (narrow.empty()) return default_char;
        if constexpr (std::is_same_v<CharT, wchar_t>)
        {
            auto wide = detail::to_wstring(narrow.c_str(), locale_name);
            return wide.empty() ? default_char : wide[0];
        }
        else
        {
            auto wide = detail::to_u32string(narrow.c_str(), locale_name);
            return wide.empty() ? default_char : wide[0];
        }
    }

    // Internal helper function.
    //
    // Preconditions:
    //   - `grouping` is non-empty. [asserted in debug builds]
    //   - `[first, last)` is a valid range, i.e. `first <= last`. Passing
    //     `first > last` causes the inner copy loop to never terminate and
    //     write past the output buffer. [asserted in debug builds]
    //   - `s` points to an output buffer with sufficient capacity to hold
    //     `(last - first)` characters plus one separator per non-leading
    //     group; callers are responsible for sizing the buffer.
    //     [not validated]
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
    // grouping and grouping_tmp are non-empty; non-emptiness is validated only
    // by assert() in debug builds, release builds rely on the caller. Violating
    // the precondition in release causes `size() - 1` to underflow to SIZE_MAX
    // and subsequent indexing to read out of bounds. Callers guarantee grouping
    // is non-empty because grouping_tmp is only populated inside
    // !grouping.empty() branches, so the non-emptiness of grouping_tmp implies
    // the non-emptiness of grouping.
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
}
