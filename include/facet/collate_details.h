#pragma once
#include <common/clocale_wrapper.h>
#include <common/defs.h>
#include <common/metafunctions.h>
#include <cvt/cvt_facilities.h>
#include <facet/facet_common.h>

#include <algorithm>
#include <compare>
#include <cstring>
#include <cuchar>
#include <cwchar>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace IOv2
{
template <typename CharT> class collate;
template <typename CharT> class collate_conf;

template <typename CharT>
class collate_conf : public ft_basic<collate<CharT>>
{
    // strxfrm / wcsxfrm signal failure by returning (size_t)-1. Using that
    // value as a buffer size would wrap on `+ 1` and silently produce a
    // zero-sized buffer that subsequent writes overflow.
    static constexpr size_t xfrm_failed = static_cast<size_t>(-1);

public:
    collate_conf(const std::string& name)
        : ft_basic<collate<CharT>>()
        , m_inter_locale(name.c_str())
    {
        if constexpr (std::is_same_v<CharT, char8_t>)
        {
            // collate<char8_t> routes through the narrow strcoll/strxfrm path,
            // which interprets its input as UTF-8 only when the inter locale's
            // LC_CTYPE codeset is UTF-8. The codeset name from
            // nl_langinfo(CODESET) is implementation-defined, so rather than
            // string-matching it we probe the locale's own multibyte decoder on
            // known code points: mbrtoc32 exercises the same LC_CTYPE codeset
            // that strcoll/strxfrm use to parse their byte input, and because
            // m_inter_locale is built from a single name with LC_ALL_MASK its
            // CTYPE and COLLATE codesets are the same. Agreement here therefore
            // means strcoll/strxfrm agree.
            static constexpr struct { const char* mb; size_t len; char32_t cp; }
            probes[] = {
                {"\xC3\xA9",         2, U'é'},      // U+00E9  e-acute
                {"\xE2\x82\xAC",     3, U'€'},      // U+20AC  euro sign
                {"\xF0\x9F\x98\x80", 4, U'\U0001F600'},  // U+1F600 grinning face
            };
            clocale_user guard(m_inter_locale);
            for (const auto& p : probes)
            {
                char32_t c32 = 0;
                std::mbstate_t st{};
                size_t n = std::mbrtoc32(&c32, p.mb, p.len, &st);
                if ((n != p.len) || (c32 != p.cp))
                    throw cvt_error("collate_conf<char8_t>: inter locale is not UTF-8");
            }
        }
    }

public:
    virtual std::strong_ordering compare(const CharT* low1, const CharT* high1,
                                         const CharT* low2, const CharT* high2) const
    {
        std::vector<CharT> buf1; bool extra_eos1 = false;
        std::vector<CharT> buf2; bool extra_eos2 = false;

        clocale_user guard(m_inter_locale);

        while ((low1 != high1) && (low2 != high2))
        {
            const CharT* cl1 = low1;
            if (auto ch1 = std::find(low1, high1, static_cast<CharT>(0)); ch1 == high1)
            {
                auto data_len = high1 - low1;
                buf1.resize(data_len + 1 + SIMD_PADDING_BYTES / sizeof(CharT));
                std::copy(low1, high1, buf1.data());
                buf1[data_len] = static_cast<CharT>(0);
                extra_eos1 = true;
                cl1 = buf1.data();
                low1 = high1;
            }
            else low1 = ch1 + 1;

            const CharT* cl2 = low2;
            if (auto ch2 = std::find(low2, high2, static_cast<CharT>(0)); ch2 == high2)
            {
                auto data_len = high2 - low2;
                buf2.resize(data_len + 1 + SIMD_PADDING_BYTES / sizeof(CharT));
                std::copy(low2, high2, buf2.data());
                buf2[data_len] = static_cast<CharT>(0);
                extra_eos2 = true;
                cl2 = buf2.data();
                low2 = high2;
            }
            else low2 = ch2 + 1;

            int c_res = 0;
            if constexpr (std::is_same_v<CharT, char>)
                c_res = std::strcoll(cl1, cl2);
            else if constexpr (std::is_same_v<CharT, wchar_t>)
                c_res = std::wcscoll(cl1, cl2);
            else if constexpr (std::is_same_v<CharT, char8_t>)
                c_res = std::strcoll(reinterpret_cast<const char*>(cl1),
                                     reinterpret_cast<const char*>(cl2));
            else if constexpr (wchar_t_is_utf32)
            {
                if constexpr (std::is_same_v<CharT, char32_t>)
                    c_res = std::wcscoll(reinterpret_cast<const wchar_t*>(cl1),
                                         reinterpret_cast<const wchar_t*>(cl2));
                else
                    static_assert(dependent_false_v<CharT>, "collate_conf::compare is not implemented.");
            }
            else
                static_assert(dependent_false_v<CharT>, "collate_conf::compare is not implemented.");

            if (c_res < 0) return std::strong_ordering::less;
            if (c_res > 0) return std::strong_ordering::greater;
        }

        if (low1 != high1) return std::strong_ordering::greater;
        if (low2 != high2) return std::strong_ordering::less;
        if (extra_eos1 && !extra_eos2) return std::strong_ordering::less;
        if (!extra_eos1 && extra_eos2) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }

    virtual size_t transform_length(const CharT* low, const CharT* high) const
    {
        size_t res = 0;
        std::vector<CharT> buf;

        clocale_user guard(m_inter_locale);
        while (low != high)
        {
            const CharT* cur = low;
            if (auto next = std::find(low, high, static_cast<CharT>(0)); next == high)
            {
                auto data_len = high - low;
                buf.resize(data_len + 1 + SIMD_PADDING_BYTES / sizeof(CharT));
                std::copy(low, high, buf.data());
                buf[data_len] = static_cast<CharT>(0);
                cur = buf.data();
                low = high;
            }
            else
            {
                low = next + 1;
                ++res;  // for the terminal character
            }

            size_t seg_len = 0;
            if constexpr (std::is_same_v<CharT, char>)
                seg_len = strxfrm(nullptr, cur, 0);
            else if constexpr (std::is_same_v<CharT, wchar_t>)
                seg_len = wcsxfrm(nullptr, cur, 0);
            else if constexpr (std::is_same_v<CharT, char8_t>)
                seg_len = strxfrm(nullptr, reinterpret_cast<const char*>(cur), 0);
            else if constexpr (wchar_t_is_utf32)
            {
                if constexpr (std::is_same_v<CharT, char32_t>)
                    seg_len = wcsxfrm(nullptr, reinterpret_cast<const wchar_t*>(cur), 0);
                else
                    static_assert(dependent_false_v<CharT>, "collate_conf::transform_length is not implemented.");
            }
            else
                static_assert(dependent_false_v<CharT>, "collate_conf::transform_length is not implemented.");

            if (seg_len == xfrm_failed)
                throw cvt_error("collate_conf::transform_length: strxfrm/wcsxfrm failed");
            res += seg_len;
        }

        return res;
    }

    virtual size_t transform(const CharT* low, const CharT* high, CharT* dest, size_t mx_len = 0) const
    {
        // transform() emits an opaque collation key, not text: the output bytes
        // are order-preserving sort weights — strcmp/wcscmp on the result
        // reproduces strcoll/wcscoll on the input — and are NOT a readable or
        // valid char/UTF sequence. The key is only ever compared bytewise,
        // never decoded back to characters.
        size_t trans_count = 0;
        // All buffers are hoisted out of the loop so resize() reuses their
        // capacity instead of reallocating per segment.
        // buf  : input staging — null-terminated copy of a segment that has
        //        no embedded '\0' (so cur can point at a terminated string).
        // buf2 : output staging for strxfrm/wcsxfrm.
        std::vector<CharT> buf;
        std::vector<CharT> buf2;
        bool extra_eos = false;

        clocale_user guard(m_inter_locale);
        while ((low != high) && ((mx_len == 0) || (trans_count < mx_len)))
        {
            const CharT* cur = low;
            if (auto next = std::find(low, high, static_cast<CharT>(0)); next == high)
            {
                auto data_len = high - low;
                buf.resize(data_len + 1 + SIMD_PADDING_BYTES / sizeof(CharT));
                std::copy(low, high, buf.data());
                buf[data_len] = static_cast<CharT>(0);
                cur = buf.data();
                low = high;
                extra_eos = true;
            }
            else
                low = next + 1;

            {
                // strxfrm/wcsxfrm always append a terminating '\0' (writing
                // cur_trans + 1 elements). Transform into the reused buffer
                // first, then copy exactly cur_trans elements to dest, so the
                // appended '\0' is discarded. Writing it straight into dest
                // would overflow the caller's buffer by one on the trailing
                // segment, for which transform_length only accounts for
                // cur_trans (no terminator).
                size_t trans_len = 0;
                if constexpr (std::is_same_v<CharT, char>)
                    trans_len = strxfrm(nullptr, cur, 0);
                else if constexpr (std::is_same_v<CharT, wchar_t>)
                    trans_len = wcsxfrm(nullptr, cur, 0);
                else if constexpr (std::is_same_v<CharT, char8_t>)
                    trans_len = strxfrm(nullptr, reinterpret_cast<const char*>(cur), 0);
                else if constexpr ((std::is_same_v<CharT, char32_t> &&
                                   wchar_t_is_utf32))
                    trans_len = wcsxfrm(nullptr, reinterpret_cast<const wchar_t*>(cur), 0);
                else
                    static_assert(dependent_false_v<CharT>, "collate_conf::transform is not implemented.");

                if (trans_len == xfrm_failed)
                    throw cvt_error("collate_conf::transform: strxfrm/wcsxfrm failed");
                buf2.resize(trans_len + 1);

                size_t cur_trans = 0;
                if constexpr (std::is_same_v<CharT, char>)
                    cur_trans = strxfrm(buf2.data(), cur, buf2.size());
                else if constexpr (std::is_same_v<CharT, wchar_t>)
                    cur_trans = wcsxfrm(buf2.data(), cur, buf2.size());
                else if constexpr (std::is_same_v<CharT, char8_t>)
                    cur_trans = strxfrm(reinterpret_cast<char*>(buf2.data()), reinterpret_cast<const char*>(cur), buf2.size());
                else if constexpr ((std::is_same_v<CharT, char32_t> &&
                                   wchar_t_is_utf32))
                    cur_trans = wcsxfrm(reinterpret_cast<wchar_t*>(buf2.data()), reinterpret_cast<const wchar_t*>(cur), buf2.size());
                else
                    static_assert(dependent_false_v<CharT>, "collate_conf::transform is not implemented.");

                if (cur_trans == xfrm_failed)
                    throw cvt_error("collate_conf::transform: strxfrm/wcsxfrm failed");
                if (mx_len != 0)
                    cur_trans = std::min(cur_trans, mx_len - trans_count);
                dest = std::copy(buf2.data(), buf2.data() + cur_trans, dest);
                trans_count += cur_trans;
            }

            if ((!extra_eos) && ((mx_len == 0) || (trans_count < mx_len)))
            {
                *dest++ = '\0';
                ++trans_count;
            }
        }
        return trans_count;
    }
private:
    // m_inter_locale is shared across compare / transform_length / transform,
    // all of which are const. Each call constructs a clocale_user that calls
    // uselocale(m_inter_locale.c_locale) to swap the calling thread's locale.
    // POSIX does not explicitly guarantee that the same locale_t may be
    // passed to uselocale() concurrently from multiple threads; IOv2 relies
    // on the de-facto thread-safety provided by glibc and macOS libc on the
    // target platforms (Linux / macOS). Porting to platforms with stricter
    // semantics requires reevaluating this assumption.
    clocale_wrapper   m_inter_locale;
};
}
