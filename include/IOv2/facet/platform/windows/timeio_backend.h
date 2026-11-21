// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file timeio_backend.h
 * @lang{ZH}
 * `timeio` facet 的 Windows 后端：以 `LC_TIME` 的 BCP-47 名字经 NLS（`GetLocaleInfoEx`）读日期时间
 * 数据，与 UCRT 自己的数据同源。
 *
 * - 名字：星期（NLS 从周一起，这里转成从周日起）、月份（主格，不带 `LOCALE_RETURN_GENITIVE_NAMES`，
 *   与 UCRT 的 `%B` 相同；不要第 13 个月）、AM/PM（87 个 locale 为空，配置类照常处理）。
 * - 格式串：`%x` 取 `LOCALE_SSHORTDATE`，`%X` 取 `LOCALE_STIMEFORMAT`，`%c` 是两者以空格相连，与 UCRT
 *   的 `strftime` 相同。`%r` 只在时间格式本身是 12 小时制时取它，否则留空，由配置类补
 *   `"%I:%M:%S %p"`（C 规定 `%r` 为 12 小时制；UCRT 的 `%r` 照搬时间格式，de-DE 是 24 小时制）。纪元
 *   格式留空，即等于普通格式。NLS 的图片格式翻成 strftime 说明符，见 `timeio_from_picture`。
 * - 宽字符形式放进 `timeio_raw_data::wide` 原样交给宽字符配置；窄字符串按 `LC_TIME` 的代码页转换，
 *   与 UCRT 的窄字符数据相同。
 * - 纪元与替代数字：这一轮不做（Windows 只有可选日历才有纪元），两个函数返回空。日历只用公历：缺省
 *   日历非公历的 locale（th-TH 等）也按公历输出，与 UCRT 的 `%x` 不同。
 * @endif
 * @lang{EN}
 * Windows backend of the `timeio` facet: the date and time data, read through NLS
 * (`GetLocaleInfoEx`) by `LC_TIME`'s BCP-47 name, from the same source as the UCRT's own.
 *
 * - Names: the weekdays (NLS counts from Monday; here from Sunday), the months (nominative,
 *   without `LOCALE_RETURN_GENITIVE_NAMES`, as the UCRT's `%B`; no thirteenth month) and AM/PM
 *   (empty in 87 locales, which the configuration handles as it always has).
 * - Formats: `%x` from `LOCALE_SSHORTDATE`, `%X` from `LOCALE_STIMEFORMAT`, and `%c` the two
 *   joined by a space, as the UCRT's `strftime` has them. `%r` takes the time format only where
 *   it is a 12-hour one, and is left empty otherwise for the configuration to supply
 *   `"%I:%M:%S %p"` (C makes `%r` a 12-hour time; the UCRT's `%r` copies the time format, which
 *   for de-DE is 24-hour). The era formats are left empty, that is, the plain ones. NLS's picture
 *   formats are translated into strftime specifiers; see `timeio_from_picture`.
 * - The wide form goes into `timeio_raw_data::wide` for the wide configurations to take as it
 *   is; the narrow strings are converted by `LC_TIME`'s code page, which is what the UCRT's
 *   narrow data is.
 * - Eras and alternative digits: not this time (Windows has eras only in optional calendars),
 *   so both functions return nothing. Gregorian only: a locale whose default calendar is not
 *   (th-TH and others) is still written in the Gregorian calendar, unlike the UCRT's `%x`.
 * @endif
 */

#pragma once

#include <IOv2/common/platform/clocale_backend.h>
#include <IOv2/facet/platform/windows/nls_api.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace IOv2::detail
{
// One era as the C library gives it; Windows gives none (see timeio_read_eras).
struct timeio_raw_era
{
    std::string name;
    std::string format;
    char        direction = '+';
    int32_t     offset = 0;
    int32_t     from_year = 0;
    uint8_t     from_month = 0;
    uint8_t     from_day = 0;
    int32_t     to_year = 0;
    uint8_t     to_month = 0;
    uint8_t     to_day = 0;
};

// The names and formats as NLS gives them, in UTF-16; the same members as timeio_raw_data.
struct timeio_raw_wide
{
    std::array<std::wstring, 7>  day;
    std::array<std::wstring, 7>  abbr_day;
    std::array<std::wstring, 12> month;
    std::array<std::wstring, 12> abbr_month;
    std::wstring                 am;
    std::wstring                 pm;
    std::wstring                 date_format;
    std::wstring                 era_date_format;
    std::wstring                 time_format;
    std::wstring                 era_time_format;
    std::wstring                 date_time_format;
    std::wstring                 era_date_time_format;
    std::wstring                 am_pm_format;
};

// A locale's date and time data: the narrow strings in LC_TIME's code page, and the wide form.
struct timeio_raw_data
{
    std::array<std::string, 7>     day;
    std::array<std::string, 7>     abbr_day;
    std::array<std::string, 12>    month;
    std::array<std::string, 12>    abbr_month;
    std::vector<std::string>       alt_digits;   // always empty
    std::string                    am;
    std::string                    pm;
    std::string                    date_format;
    std::string                    era_date_format;
    std::string                    time_format;
    std::string                    era_time_format;
    std::string                    date_time_format;
    std::string                    era_date_time_format;
    std::string                    am_pm_format;
    std::vector<timeio_raw_era>    eras;         // always empty
    std::optional<timeio_raw_wide> wide;         // always set
};

// The "C" locale's %x and %X, which a picture timeio_from_picture cannot translate falls back to.
inline constexpr std::wstring_view timeio_c_date_picture = L"%m/%d/%y";
inline constexpr std::wstring_view timeio_c_time_picture = L"%H:%M:%S";

/**
 * @lang{ZH}
 * @brief 把 NLS 的图片格式（如 `"M/d/yyyy"`、`"h:mm:ss tt"`）翻成 strftime 格式串。
 *
 * `d`/`dd` → `%d`，`ddd` → `%a`，`dddd` → `%A`，`M`/`MM` → `%m`，`MMM` → `%b`，`MMMM` → `%B`，
 * `yy` → `%y`，`yyyy` → `%Y`，`h`/`hh` → `%I`，`H`/`HH` → `%H`，`m`/`mm` → `%M`，`s`/`ss` → `%S`，
 * `tt` → `%p`。不补零的记号一律映射成补零的说明符（en-US 写作 `03/05/2026`，系统是 `3/5/2026`），
 * 读入时两种写法都认。单引号括起的是字面量，`''` 表示一个单引号；字面量里的 `%` 写成 `%%`。
 * 912 个 locale 的短日期与时间格式只用到这些记号。
 * @return 翻好的格式串；遇到 `gg`（纪元）或其它 ASCII 字母时为 `std::nullopt`，由调用方整串退回
 *         C locale 的格式：宁可丢失字段顺序，也不输出读不回来的文本。
 * @endif
 * @lang{EN}
 * @brief Translates an NLS picture format (`"M/d/yyyy"`, `"h:mm:ss tt"`) into a strftime format.
 *
 * `d`/`dd` → `%d`, `ddd` → `%a`, `dddd` → `%A`, `M`/`MM` → `%m`, `MMM` → `%b`, `MMMM` → `%B`,
 * `yy` → `%y`, `yyyy` → `%Y`, `h`/`hh` → `%I`, `H`/`HH` → `%H`, `m`/`mm` → `%M`, `s`/`ss` → `%S`,
 * `tt` → `%p`. The unpadded tokens map to the padded specifiers too (en-US writes `03/05/2026`
 * where the system writes `3/5/2026`), and reading accepts either. Text in single quotes is
 * literal, `''` standing for one quote; a `%` in literal text becomes `%%`. The short date and
 * time formats of all 912 locales use these tokens and nothing else.
 * @return The translated format; `std::nullopt` on `gg` (an era) or any other ASCII letter, for
 *         the caller to fall back to the "C" locale's format for the whole string: losing the
 *         field order is preferable to writing text that cannot be read back.
 * @endif
 */
inline std::optional<std::wstring> timeio_from_picture(std::wstring_view pic)
{
    std::wstring res;
    const auto literal = [&res](wchar_t c) {
        if (c == L'%') res += L'%';
        res += c;
    };

    for (std::size_t i = 0; i < pic.size();)
    {
        const wchar_t c = pic[i];
        if (c == L'\'')
        {
            // A quoted run up to the next lone quote; '' is a quote, inside a run or out of one.
            ++i;
            if (i < pic.size() && pic[i] == L'\'') { res += L'\''; ++i; continue; }
            for (; i < pic.size(); ++i)
            {
                if (pic[i] != L'\'') { literal(pic[i]); continue; }
                if (i + 1 < pic.size() && pic[i + 1] == L'\'') { res += L'\''; ++i; continue; }
                break;
            }
            ++i;
            continue;
        }

        const bool letter = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
        if (!letter) { literal(c); ++i; continue; }

        std::size_t n = 1;
        while (i + n < pic.size() && pic[i + n] == c) ++n;

        std::wstring_view spec;
        switch (c)
        {
        case L'd': spec = n <= 2 ? L"%d" : n == 3 ? L"%a" : n == 4 ? L"%A" : L""; break;
        case L'M': spec = n <= 2 ? L"%m" : n == 3 ? L"%b" : n == 4 ? L"%B" : L""; break;
        case L'y': spec = n == 2 ? L"%y" : n == 4 ? L"%Y" : L""; break;
        case L'h': spec = n <= 2 ? L"%I" : L""; break;
        case L'H': spec = n <= 2 ? L"%H" : L""; break;
        case L'm': spec = n <= 2 ? L"%M" : L""; break;
        case L's': spec = n <= 2 ? L"%S" : L""; break;
        case L't': spec = n == 2 ? L"%p" : L""; break;
        default: break;
        }
        if (spec.empty())
            return std::nullopt;
        res += spec;
        i += n;
    }
    return res;
}

// Whether a strftime format holds the specifier spec (a %% is a literal, not a specifier).
inline bool timeio_has_specifier(std::wstring_view fmt, wchar_t spec)
{
    for (std::size_t i = 0; i + 1 < fmt.size(); ++i)
    {
        if (fmt[i] != L'%') continue;
        if (fmt[i + 1] == spec) return true;
        ++i;
    }
    return false;
}

// One GetLocaleInfoEx item of the locale named bcp47 ("" being the invariant locale); empty if
// NLS has none.
inline std::wstring timeio_nls(const std::wstring& bcp47, unsigned long item)
{
    const int len = GetLocaleInfoEx(bcp47.c_str(), item, nullptr, 0);
    if (len <= 1)
        return {};
    std::wstring res(static_cast<std::size_t>(len), L'\0');
    if (GetLocaleInfoEx(bcp47.c_str(), item, res.data(), len) != len)
        return {};
    res.resize(static_cast<std::size_t>(len) - 1);   // the terminating NUL
    return res;
}

// src in the code page cp, as the UCRT converts its own date and time strings (no flags: a
// character the code page lacks becomes its best fit or the default character).
inline std::string timeio_narrow(const std::wstring& src, unsigned int cp)
{
    if (src.empty())
        return {};
    const int src_len = static_cast<int>(src.size());
    const int len = WideCharToMultiByte(cp, 0, src.data(), src_len, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return {};
    std::string res(static_cast<std::size_t>(len), '\0');
    if (WideCharToMultiByte(cp, 0, src.data(), src_len, res.data(), len, nullptr, nullptr) != len)
        return {};
    return res;
}

// Eras: none here (only optional calendars have them on Windows).
inline std::vector<timeio_raw_era> timeio_read_eras()
{
    return {};
}

// Alternative digits: none here (the UCRT does not use NLS's native digits either).
inline std::vector<std::string> timeio_read_alt_digits()
{
    return {};
}

// The date and time data of loc's LC_TIME, through NLS by its BCP-47 name.
inline timeio_raw_data timeio_read_data(clocale_handle loc)
{
    const std::wstring& bcp47 = loc->time_bcp47;
    timeio_raw_wide w;
    for (std::size_t i = 0; i < w.day.size(); ++i)
    {
        // NLS counts weekdays from Monday, timeio from Sunday.
        const auto nls_index = static_cast<unsigned long>((i + 6) % 7);
        w.day[i] = timeio_nls(bcp47, win_locale_sdayname1 + nls_index);
        w.abbr_day[i] = timeio_nls(bcp47, win_locale_sabbrevdayname1 + nls_index);
    }
    for (std::size_t i = 0; i < w.month.size(); ++i)
    {
        w.month[i] = timeio_nls(bcp47, win_locale_smonthname1 + static_cast<unsigned long>(i));
        w.abbr_month[i] = timeio_nls(bcp47, win_locale_sabbrevmonthname1 + static_cast<unsigned long>(i));
    }
    w.am = timeio_nls(bcp47, win_locale_sam);
    w.pm = timeio_nls(bcp47, win_locale_spm);

    w.date_format = timeio_from_picture(timeio_nls(bcp47, win_locale_sshortdate))
                        .value_or(std::wstring(timeio_c_date_picture));
    w.time_format = timeio_from_picture(timeio_nls(bcp47, win_locale_stimeformat))
                        .value_or(std::wstring(timeio_c_time_picture));
    w.date_time_format = w.date_format + L' ' + w.time_format;
    if (timeio_has_specifier(w.time_format, L'I') && timeio_has_specifier(w.time_format, L'p'))
        w.am_pm_format = w.time_format;

    timeio_raw_data res;
    const auto narrow = [cp = loc->time_cp](const std::wstring& src) { return timeio_narrow(src, cp); };
    for (std::size_t i = 0; i < w.day.size(); ++i)
    {
        res.day[i] = narrow(w.day[i]);
        res.abbr_day[i] = narrow(w.abbr_day[i]);
    }
    for (std::size_t i = 0; i < w.month.size(); ++i)
    {
        res.month[i] = narrow(w.month[i]);
        res.abbr_month[i] = narrow(w.abbr_month[i]);
    }
    res.am = narrow(w.am);
    res.pm = narrow(w.pm);
    res.date_format = narrow(w.date_format);
    res.time_format = narrow(w.time_format);
    res.date_time_format = narrow(w.date_time_format);
    res.am_pm_format = narrow(w.am_pm_format);
    res.wide = std::move(w);
    return res;
}
}
