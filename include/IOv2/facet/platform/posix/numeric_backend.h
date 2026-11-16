// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file numeric_backend.h
 * @lang{ZH}
 * `numeric` facet 的 POSIX 后端：在 `clocale_user` 切换后的线程 locale 下读 `localeconv()` 与
 * `nl_langinfo(YESSTR/NOSTR)`。glibc 没有 `localeconv_l`，故不用 `_l` 形式。
 *
 * 布尔名称取自 `YESSTR`/`NOSTR`，这是 IOv2 自己的扩展：libstdc++、libc++ 与 MSVC STL 都固定
 * 用 `"true"`/`"false"`。glibc 每个 locale 存一个词（de_DE 为 `"ja"`/`"nein"`），也可能为空
 * （`C`、gv_GB）。macOS 存的是 CLDR 规定的冒号分隔列表——“通常被识别为肯定/否定回答的
 * 一组字符串”，首项为小写的本地化完整写法，如 de_DE 的 `"ja:j:JA:J:yes:y:YES:Y"`——本后端
 * 只取首项：它是唯一适合作输出的一项，整个列表用来匹配输入又会与前缀匹配冲突（`"n"`、
 * `"nein"`、`"no"` 同在否定列表里）。macOS 的 `nl_langinfo` 遇空串会回退到 C locale 的值，
 * 故那里缺数据的 locale 得到的是 `"yes"`/`"no"`，而不是空串。
 * @endif
 * @lang{EN}
 * POSIX backend of the `numeric` facet: `localeconv()` and `nl_langinfo(YESSTR/NOSTR)`,
 * read under the thread locale a `clocale_user` switched to. glibc has no `localeconv_l`, so
 * the `_l` forms are not used.
 *
 * The boolean names come from `YESSTR`/`NOSTR`, an extension of IOv2's own: libstdc++, libc++
 * and the MSVC STL all keep `"true"`/`"false"`. glibc stores one word per locale (`"ja"`/`"nein"`
 * in de_DE), possibly an empty one (`C`, gv_GB). macOS stores the colon-separated list CLDR
 * specifies -- "strings that would normally be recognized as yes and no responses", led by
 * the lower-case native word, such as de_DE's `"ja:j:JA:J:yes:y:YES:Y"` -- and this backend
 * takes the first item alone: it is the only one fit for output, and matching input against
 * the whole list would clash with prefix matching (`"n"`, `"nein"` and `"no"` sit in the same
 * negative list). macOS's `nl_langinfo` falls back to the C locale's value for an empty
 * string, so a locale there without the data gets `"yes"`/`"no"` rather than nothing.
 * @endif
 */

#pragma once

#include <clocale>
#include <cstring>
#include <langinfo.h>
#include <optional>
#include <string>

namespace IOv2::detail
{
/**
 * @lang{ZH}
 * 一个 locale 的数值标点与布尔名称，按 C 库给出的原样复制（读完即与 C 库的缓冲区无关）。
 * @endif
 * @lang{EN}
 * A locale's numeric punctuation and boolean names, copied as the C library gives them (so
 * nothing refers to the C library's buffers once they are read).
 * @endif
 */
struct numeric_raw_punct
{
    std::string decimal_point;   // lconv, in the locale's narrow encoding
    std::string thousands_sep;
    std::string grouping;        // lconv's bytes, not yet normalised
    std::string truename;        // in the locale's narrow encoding; empty if it names none
    std::string falsename;
    // The C library's own wide forms of the two separators; POSIX has none.
    std::optional<std::wstring> wdecimal_point;
    std::optional<std::wstring> wthousands_sep;
};

// The items of YESSTR and NOSTR. POSIX dropped them in 2001, so the macros hide under a
// strict _POSIX_C_SOURCE, which IOv2's build defines: glibc defines them for __USE_GNU only
// (its __YESSTR is always there), Apple's <_langinfo.h> only below 200112L (they are 54/55
// there). nl_langinfo answers them either way.
#if defined(YESSTR)
inline constexpr nl_item numeric_yesstr = YESSTR;
inline constexpr nl_item numeric_nostr  = NOSTR;
#elif defined(__GLIBC__)
inline constexpr nl_item numeric_yesstr = __YESSTR;
inline constexpr nl_item numeric_nostr  = __NOSTR;
#elif defined(__APPLE__)
inline constexpr nl_item numeric_yesstr = 54;
inline constexpr nl_item numeric_nostr  = 55;
#endif

// YESSTR / NOSTR, cut at macOS's first ':'.
inline std::string numeric_bool_name(const char* s)
{
    if (!s)
        return {};
#if defined(__APPLE__)
    return std::string(s, std::strcspn(s, ":"));
#else
    return s;
#endif
}

// The punctuation of the calling thread's current locale. Call it under a clocale_user.
inline numeric_raw_punct numeric_read_punct()
{
    numeric_raw_punct res;
    const lconv* lc = std::localeconv();
    if (lc->decimal_point) res.decimal_point = lc->decimal_point;
    if (lc->thousands_sep) res.thousands_sep = lc->thousands_sep;
    if (lc->grouping)      res.grouping      = lc->grouping;
    res.truename  = numeric_bool_name(nl_langinfo(numeric_yesstr));
    res.falsename = numeric_bool_name(nl_langinfo(numeric_nostr));
    return res;
}
}
