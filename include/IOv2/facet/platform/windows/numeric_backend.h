// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file numeric_backend.h
 * @lang{ZH}
 * `numeric` facet 的 Windows（UCRT）后端：在 `clocale_user` 切换后的线程 locale 下读
 * `localeconv()`。
 *
 * UCRT 的 `lconv` 除窄字段外还有 `_W_decimal_point`、`_W_thousands_sep`，取自 NLS 的
 * `LOCALE_SDECIMAL`/`LOCALE_STHOUSAND`，宽字符配置直接用它们。窄字段按该 locale 的代码页
 * 编码，代码页装不下的字符会被近似：真机上 `French_France.1252` 的窄千位分隔符是 `"\xA0"`，
 * 宽的则是 U+202F。
 *
 * Windows 没有布尔名称的来源：NLS 没有这一项，系统自带的 `icu.dll` 数据里也没有（探针在
 * Windows Server 2022 的 ICU 64 与 2025 的 ICU 72 上都确认过），故布尔名称总为空，由配置类
 * 回退到 `"true"`/`"false"`，与 MSVC STL 相同。
 * @endif
 * @lang{EN}
 * Windows (UCRT) backend of the `numeric` facet: `localeconv()`, read under the thread locale
 * a `clocale_user` switched to.
 *
 * The UCRT's `lconv` has, besides the narrow fields, `_W_decimal_point` and `_W_thousands_sep`,
 * taken from NLS's `LOCALE_SDECIMAL`/`LOCALE_STHOUSAND`, and the wide configurations use them
 * as they are. The narrow fields are in the locale's code page, which approximates what it
 * cannot hold: on real Windows the narrow thousands separator of `French_France.1252` is
 * `"\xA0"`, the wide one U+202F.
 *
 * Windows has no source of boolean names: NLS has no such item, nor does the data of the
 * system's `icu.dll` (the probe checked ICU 64 on Windows Server 2022 and ICU 72 on 2025), so
 * they are always empty and the configuration classes fall back to `"true"`/`"false"`, as the
 * MSVC STL does.
 * @endif
 */

#pragma once

#include <IOv2/facet/platform/windows/nls_api.h>

#include <clocale>
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
    std::string decimal_point;   // lconv, in the locale's code page
    std::string thousands_sep;
    std::string grouping;        // lconv's bytes, not yet normalised
    std::string truename;        // always empty: Windows names none
    std::string falsename;
    // The UCRT's own wide forms of the two separators.
    std::optional<std::wstring> wdecimal_point;
    std::optional<std::wstring> wthousands_sep;
    bool utf8 = false;           // whether the code page is UTF-8
};

// The punctuation of the calling thread's current locale. Call it under a clocale_user.
inline numeric_raw_punct numeric_read_punct()
{
    numeric_raw_punct res;
    const lconv* lc = std::localeconv();
    if (lc->decimal_point) res.decimal_point = lc->decimal_point;
    if (lc->thousands_sep) res.thousands_sep = lc->thousands_sep;
    if (lc->grouping)      res.grouping      = lc->grouping;
    res.wdecimal_point = lc->_W_decimal_point ? lc->_W_decimal_point : L"";
    res.wthousands_sep = lc->_W_thousands_sep ? lc->_W_thousands_sep : L"";
    res.utf8 = ___lc_codepage_func() == win_cp_utf8;
    return res;
}
}
