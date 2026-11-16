// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file monetary_backend.h
 * @lang{ZH}
 * `monetary` facet 的 Windows（UCRT）后端：在 `clocale_user` 切换后的线程 locale 下读
 * `localeconv()`。
 *
 * - UCRT 的 `lconv` 没有 C99 的国际格式标志（`int_p_cs_precedes` 等），这里以国内格式的标志
 *   顶替，与 libc++ 在 Windows 上的做法相同；国际格式的小数位数仍取 `int_frac_digits`。
 *   于是 en-US 的国际格式是 `USD1,234.56`。
 * - 标志用 UCRT 由 NLS 的 `ICURRENCY`/`INEGCURR` 换算出的值，与 MSVC STL、libc++ 相同。
 *   这一换算有损：`INEGCURR` 12（`$ -1.1`）与 13（`1.1- $`）按 C 的定义是 `sep_by_space`
 *   为 2，UCRT 给的是 1，故 nl-NL 的负数写作 `€- 1.234,56`；Windows 上 `sep_by_space` 从不为 2。
 * - `int_curr_symbol` 取自 NLS 的 `LOCALE_SINTLSYMBOL`，只有三个字母（`"USD"`），没有 C 规定的
 *   第四个字符。
 * - 字符串另有 `_W_` 宽字符形式，宽字符配置直接用它们；窄字段按 locale 的代码页编码，代码页
 *   装不下的字符已被近似（`French_France.1252` 的窄千位分隔符是 `0xA0`）。
 * @endif
 * @lang{EN}
 * Windows (UCRT) backend of the `monetary` facet: `localeconv()`, read under the thread
 * locale a `clocale_user` switched to.
 *
 * - The UCRT's `lconv` has none of C99's international format flags (`int_p_cs_precedes`
 *   and the rest); the national flags stand in for them, as in libc++ on Windows, while
 *   the international fractional digits still come from `int_frac_digits`. en-US's
 *   international format is therefore `USD1,234.56`.
 * - The flags are the ones the UCRT works out from NLS's `ICURRENCY`/`INEGCURR`, as the
 *   MSVC STL and libc++ take them. That mapping loses something: `INEGCURR` 12 (`$ -1.1`)
 *   and 13 (`1.1- $`) are a `sep_by_space` of 2 in C's terms, and the UCRT gives 1, so
 *   nl-NL's negative amounts come out as `€- 1.234,56`; `sep_by_space` is never 2 here.
 * - `int_curr_symbol` comes from NLS's `LOCALE_SINTLSYMBOL` and is the three letters alone
 *   (`"USD"`), without the fourth character C describes.
 * - The strings also come in `_W_` wide forms, which the wide configurations use as they
 *   are; the narrow fields are in the locale's code page, which approximates what it
 *   cannot hold (`French_France.1252`'s narrow thousands separator is `0xA0`).
 * @endif
 */

#pragma once

#include <climits>
#include <clocale>
#include <optional>
#include <string>

namespace IOv2::detail
{
/**
 * @lang{ZH} 一组格式标志：`*_cs_precedes`、`*_sep_by_space`、`*_sign_posn`，按 `lconv` 原样保留（含 `CHAR_MAX`）。 @endif
 * @lang{EN} One set of format flags, `*_cs_precedes`, `*_sep_by_space` and `*_sign_posn`, kept as `lconv` has them (`CHAR_MAX` included). @endif
 */
struct monetary_raw_flags
{
    char cs_precedes  = CHAR_MAX;
    char sep_by_space = CHAR_MAX;
    char sign_posn    = CHAR_MAX;
};

/**
 * @lang{ZH}
 * 一个 locale 的货币格式数据，按 C 库给出的原样复制（读完即与 C 库的缓冲区无关）。
 * @endif
 * @lang{EN}
 * A locale's monetary formatting data, copied as the C library gives it (so nothing refers
 * to the C library's buffers once it is read).
 * @endif
 */
struct monetary_raw_punct
{
    std::string int_curr_symbol;     // in the locale's code page
    std::string currency_symbol;
    std::string mon_decimal_point;
    std::string mon_thousands_sep;
    std::string mon_grouping;        // lconv's bytes, not yet normalised
    std::string positive_sign;
    std::string negative_sign;
    char int_frac_digits = CHAR_MAX;
    char frac_digits     = CHAR_MAX;
    monetary_raw_flags nat_pos;
    monetary_raw_flags nat_neg;
    monetary_raw_flags int_pos;      // the national ones
    monetary_raw_flags int_neg;
    // The UCRT's own wide forms of the strings.
    std::optional<std::wstring> wint_curr_symbol, wcurrency_symbol, wmon_decimal_point,
                                wmon_thousands_sep, wpositive_sign, wnegative_sign;
};

// The monetary data of the calling thread's current locale. Call it under a clocale_user.
inline monetary_raw_punct monetary_read_punct()
{
    const auto str  = [](const char* s) { return s ? std::string(s) : std::string(); };
    const auto wstr = [](const wchar_t* s) { return s ? std::wstring(s) : std::wstring(); };
    const lconv* lc = std::localeconv();
    monetary_raw_punct res;
    res.int_curr_symbol    = str(lc->int_curr_symbol);
    res.currency_symbol    = str(lc->currency_symbol);
    res.mon_decimal_point  = str(lc->mon_decimal_point);
    res.mon_thousands_sep  = str(lc->mon_thousands_sep);
    res.mon_grouping       = str(lc->mon_grouping);
    res.positive_sign      = str(lc->positive_sign);
    res.negative_sign      = str(lc->negative_sign);
    res.int_frac_digits    = lc->int_frac_digits;
    res.frac_digits        = lc->frac_digits;
    res.nat_pos = {.cs_precedes = lc->p_cs_precedes, .sep_by_space = lc->p_sep_by_space,
                   .sign_posn = lc->p_sign_posn};
    res.nat_neg = {.cs_precedes = lc->n_cs_precedes, .sep_by_space = lc->n_sep_by_space,
                   .sign_posn = lc->n_sign_posn};
    res.int_pos = res.nat_pos;
    res.int_neg = res.nat_neg;
    res.wint_curr_symbol   = wstr(lc->_W_int_curr_symbol);
    res.wcurrency_symbol   = wstr(lc->_W_currency_symbol);
    res.wmon_decimal_point = wstr(lc->_W_mon_decimal_point);
    res.wmon_thousands_sep = wstr(lc->_W_mon_thousands_sep);
    res.wpositive_sign     = wstr(lc->_W_positive_sign);
    res.wnegative_sign     = wstr(lc->_W_negative_sign);
    return res;
}
}
