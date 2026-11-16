// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file monetary_backend.h
 * @lang{ZH}
 * `monetary` facet 的 POSIX 后端：在 `clocale_user` 切换后的线程 locale 下读 `localeconv()`。
 * glibc 没有 `localeconv_l`，故不用 `_l` 形式。
 *
 * POSIX 的 `lconv` 带 C99 的国际格式标志（`int_p_cs_precedes` 等），原样读出。macOS 的这些
 * 标志与国内格式的相同（探针看过的 locale 全是如此），于是那里国际格式的布局与国内格式一样，
 * 例如 en_US 的国际格式没有空格：`USD1,234.56`，而 glibc 是 `USD 1,234.56`。
 * @endif
 * @lang{EN}
 * POSIX backend of the `monetary` facet: `localeconv()`, read under the thread locale a
 * `clocale_user` switched to. glibc has no `localeconv_l`, so the `_l` form is not used.
 *
 * POSIX's `lconv` carries C99's international format flags (`int_p_cs_precedes` and the
 * rest), which are read as they are. macOS sets them to the national ones (in every
 * locale the probe looked at), so the international format there is laid out like the
 * national one: en_US's has no space, `USD1,234.56`, where glibc's is `USD 1,234.56`.
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
    std::string int_curr_symbol;     // in the locale's narrow encoding
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
    monetary_raw_flags int_pos;
    monetary_raw_flags int_neg;
    // The C library's own wide forms of the strings; POSIX has none.
    std::optional<std::wstring> wint_curr_symbol, wcurrency_symbol, wmon_decimal_point,
                                wmon_thousands_sep, wpositive_sign, wnegative_sign;
};

// The monetary data of the calling thread's current locale. Call it under a clocale_user.
inline monetary_raw_punct monetary_read_punct()
{
    const auto str = [](const char* s) { return s ? std::string(s) : std::string(); };
    const lconv* lc = std::localeconv();
    monetary_raw_punct res;
    res.int_curr_symbol   = str(lc->int_curr_symbol);
    res.currency_symbol   = str(lc->currency_symbol);
    res.mon_decimal_point = str(lc->mon_decimal_point);
    res.mon_thousands_sep = str(lc->mon_thousands_sep);
    res.mon_grouping      = str(lc->mon_grouping);
    res.positive_sign     = str(lc->positive_sign);
    res.negative_sign     = str(lc->negative_sign);
    res.int_frac_digits   = lc->int_frac_digits;
    res.frac_digits       = lc->frac_digits;
    res.nat_pos = {.cs_precedes = lc->p_cs_precedes, .sep_by_space = lc->p_sep_by_space,
                   .sign_posn = lc->p_sign_posn};
    res.nat_neg = {.cs_precedes = lc->n_cs_precedes, .sep_by_space = lc->n_sep_by_space,
                   .sign_posn = lc->n_sign_posn};
    res.int_pos = {.cs_precedes = lc->int_p_cs_precedes, .sep_by_space = lc->int_p_sep_by_space,
                   .sign_posn = lc->int_p_sign_posn};
    res.int_neg = {.cs_precedes = lc->int_n_cs_precedes, .sep_by_space = lc->int_n_sep_by_space,
                   .sign_posn = lc->int_n_sign_posn};
    return res;
}
}
