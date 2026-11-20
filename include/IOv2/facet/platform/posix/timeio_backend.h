// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file timeio_backend.h
 * @lang{ZH}
 * `timeio` facet 的 POSIX 后端：在 `clocale_user` 切换后的线程 locale 下读 `nl_langinfo`。
 *
 * 星期与月份名、AM/PM 以及 `D_T_FMT` 等格式串各 POSIX 系统都有。纪元与替代数字只在 glibc 上
 * 有数据：glibc 把纪元表存成自己的二进制布局（经 `_NL_TIME_ERA_*` 读取），替代数字存成 NUL
 * 分隔的一串字符串；macOS 的 `nl_langinfo` 对 `ERA`、`ALT_DIGITS` 以及 `ERA_D_FMT` 等纪元格式
 * 一律返回空串（Apple Libc 源码注明尚未实现），这里的两个函数在那里返回空。POSIX 规定的 `;`
 * 分隔形式没有能测到的平台，不解析。
 *
 * 返回的是 C 库给出的原样数据：纪元格式为空时怎样回退、`T_FMT_AMPM` 为空时用什么、格式串怎样
 * 规范化，以及纪元方向怎样规范化，都由配置类决定。
 * @endif
 * @lang{EN}
 * POSIX backend of the `timeio` facet: `nl_langinfo`, read under the thread locale a
 * `clocale_user` switched to.
 *
 * Weekday and month names, AM/PM and the formats such as `D_T_FMT` exist on every POSIX
 * system. Eras and alternative digits have data on glibc alone: glibc keeps its era table in a
 * binary layout of its own (read through `_NL_TIME_ERA_*`) and its alternative digits as a run
 * of NUL-separated strings, while macOS's `nl_langinfo` returns an empty string for `ERA`,
 * `ALT_DIGITS` and the era formats such as `ERA_D_FMT` (Apple's Libc source marks them as not
 * implemented), so the two functions here return nothing there. The `;`-separated form POSIX
 * specifies has no platform to test it on, and is not parsed.
 *
 * What comes back is the C library's data as it is: how an empty era format falls back, what
 * stands in for an empty `T_FMT_AMPM`, how the formats are normalized and how an era's
 * direction is normalized are the configuration class's decisions.
 * @endif
 */

#pragma once

// First: on glibc it brings in <features.h>, which defines __GLIBC__.
#include <langinfo.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <limits>
#include <string>
#include <vector>

namespace IOv2::detail
{
/**
 * @lang{ZH}
 * 一个纪元，按 C 库给出的样子：名称与格式串为该 locale 的窄字符编码；日期已解成公历的年月日，
 * 开放的终点为 `int32` 最大值年的 12 月 31 日；方向标记原样保留（`'+'` 或 `'-'`）。
 * @endif
 * @lang{EN}
 * One era as the C library gives it: the name and the format in the locale's narrow encoding;
 * the dates decoded into Gregorian years, months and days, an open end being December 31 of the
 * `int32` maximum; the direction marker kept as it is (`'+'` or `'-'`).
 * @endif
 */
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

/**
 * @lang{ZH}
 * 一个 locale 的日期时间数据，按 C 库给出的原样复制（读完即与 C 库的缓冲区无关）。字符串都是
 * 该 locale 的窄字符编码；星期从周日起，月份从一月起；C 库没有给出的项为空。
 * @endif
 * @lang{EN}
 * A locale's date and time data, copied as the C library gives it (so nothing refers to the C
 * library's buffers once it is read). Every string is in the locale's narrow encoding; weekdays
 * start on Sunday and months in January; an item the C library does not give is empty.
 * @endif
 */
struct timeio_raw_data
{
    std::array<std::string, 7>  day;
    std::array<std::string, 7>  abbr_day;
    std::array<std::string, 12> month;
    std::array<std::string, 12> abbr_month;
    std::vector<std::string>    alt_digits;   // from 0, at most 100
    std::string                 am;
    std::string                 pm;
    std::string                 date_format;
    std::string                 era_date_format;
    std::string                 time_format;
    std::string                 era_time_format;
    std::string                 date_time_format;
    std::string                 era_date_time_format;
    std::string                 am_pm_format;
    std::vector<timeio_raw_era> eras;
};

// One langinfo item of the calling thread's locale; POSIX promises a string, this does not rely on it.
inline std::string timeio_item(nl_item item)
{
    const char* const p = nl_langinfo(item);
    return p ? std::string{p} : std::string{};
}

/**
 * @lang{ZH}
 * @brief 当前线程 locale 的纪元。glibc 上解析它的纪元二进制表，其它 POSIX 系统（macOS）返回空。
 *
 * @pre 调用方已经以 `clocale_user` 等方式把目标 locale 设为本线程的当前 locale。
 *      `nl_langinfo()` 读的就是它；返回的指针在下一次 `nl_langinfo()` 之前有效，本函数在此之前
 *      用完。
 *
 * @note **glibc 的输入布局（受信任，不做验证）**
 *   这是唯一信任 glibc 纪元二进制布局的地方。`nl_langinfo(_NL_TIME_ERA_ENTRIES)` 被假定指向
 *   恰好 `_NL_TIME_ERA_NUM_ENTRIES` 条连续、完整的记录；指针遍历**不做越界检查**，locale
 *   数据库被篡改或截断时可能越界读。系统 locale 数据视为受信任的；若纪元数据可能来自不可信
 *   的来源，应在这里加上边界检查。
 *
 *   每条记录相对其起点的布局：
 *   - 8 个 int32 的头部（以 memcpy 读出）：
 *       [0] 方向标记（'+' / '-'）  [1] 偏移量
 *       [2] from_year - 1900       [3] from_month - 1   [4] from_day
 *       [5] to_year   - 1900       [6] to_month   - 1   [7] to_day
 *   - NUL 结尾的窄字符名称，随后是 NUL 结尾的窄字符格式串
 *   - 填充到相对记录起点的 4 字节边界
 *   - NUL 结尾的宽字符名称，随后是 NUL 结尾的宽字符格式串
 *
 * @note **输出**：年份加上 1900 时防止 int32 溢出，开放的终点规范化为 int32 最大值年的
 *   12 月 31 日；方向标记原样返回，由配置类规范化。
 *
 * @return 解码后的纪元；没有纪元数据时为空。
 * @endif
 *
 * @lang{EN}
 * @brief The eras of the calling thread's locale: glibc's era binary table, parsed; nothing on
 *        the other POSIX systems (macOS).
 *
 * @pre The caller has made the target locale current on the calling thread (through a
 *      `clocale_user` or the like). `nl_langinfo()` reads that locale; the pointers it returns
 *      stay valid until the next `nl_langinfo()` call, and this function is done with them
 *      before then.
 *
 * @note **glibc's input layout (trusted, not validated)**
 *   This is the single place where the glibc era binary layout is trusted.
 *   `nl_langinfo(_NL_TIME_ERA_ENTRIES)` is assumed to point at exactly
 *   `_NL_TIME_ERA_NUM_ENTRIES` consecutive, well-formed records; the pointer walk is
 *   **unchecked**, so malformed or truncated locale data (a tampered locale database, say) can
 *   cause out-of-bounds reads. System locale data is treated as trusted; if era data could ever
 *   come from an untrusted source, this is the boundary to harden.
 *
 *   Each record, relative to its start, is laid out as:
 *   - an 8 × int32 header (read via memcpy):
 *       [0] direction marker ('+' / '-')   [1] offset
 *       [2] from_year - 1900   [3] from_month - 1   [4] from_day
 *       [5] to_year   - 1900   [6] to_month   - 1   [7] to_day
 *   - a NUL-terminated narrow name, then a NUL-terminated narrow format string
 *   - padding to the next 4-byte boundary (relative to the record's start)
 *   - a NUL-terminated wide name, then a NUL-terminated wide format string
 *
 * @note **Output**: the years are kept from overflowing int32 on the +1900, an open end is
 *   normalized to December 31 of the int32 maximum, and the direction marker is returned as
 *   it is, for the configuration class to normalize.
 *
 * @return The decoded eras; empty if there is no era data.
 * @endif
 */
inline std::vector<timeio_raw_era> timeio_read_eras()
{
    std::vector<timeio_raw_era> items;
#if defined(__GLIBC__)
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    struct era_wire_header
    {
        int32_t direction_marker;
        int32_t offset;
        int32_t from_year;
        int32_t from_month;
        int32_t from_day;
        int32_t to_year;
        int32_t to_month;
        int32_t to_day;
    };
    static_assert(sizeof(era_wire_header) == 8 * sizeof(int32_t));

    const auto record_count = static_cast<int32_t>(
        reinterpret_cast<uintptr_t>(nl_langinfo(_NL_TIME_ERA_NUM_ENTRIES)));
    if (record_count <= 0)
        return items;

    items.reserve(static_cast<std::size_t>(record_count));
    const char* cursor = reinterpret_cast<const char*>(nl_langinfo(_NL_TIME_ERA_ENTRIES));
    for (int32_t index = 0; index < record_count; ++index)
    {
        const char* const record = cursor;
        era_wire_header header{};
        std::memcpy(&header, cursor, sizeof header);
        cursor += sizeof header;

        timeio_raw_era value{};
        constexpr int32_t epoch = 1900;
        constexpr int32_t last_relative_year = std::numeric_limits<int32_t>::max() - epoch;
        value.from_year = header.from_year > last_relative_year
            ? std::numeric_limits<int32_t>::max()
            : header.from_year + epoch;
        value.from_month = static_cast<uint8_t>(header.from_month + 1);
        value.from_day = static_cast<uint8_t>(header.from_day);

        const bool open_ended = header.to_year > last_relative_year;
        value.to_year = open_ended
            ? std::numeric_limits<int32_t>::max()
            : header.to_year + epoch;
        value.to_month = open_ended ? uint8_t{12}
                                    : static_cast<uint8_t>(header.to_month + 1);
        value.to_day = open_ended ? uint8_t{31}
                                  : static_cast<uint8_t>(header.to_day);
        value.offset = header.offset;
        value.direction = header.direction_marker == static_cast<int32_t>('+') ? '+' : '-';

        value.name = cursor;
        cursor += std::strlen(cursor) + 1;
        value.format = cursor;
        cursor += std::strlen(cursor) + 1;

        constexpr std::size_t wire_alignment = sizeof(int32_t);
        const auto narrow_size = static_cast<std::size_t>(cursor - record);
        cursor += (wire_alignment - (narrow_size % wire_alignment)) % wire_alignment;

        const auto* wide_name = reinterpret_cast<const wchar_t*>(cursor);
        cursor += (std::wcslen(wide_name) + 1) * sizeof(wchar_t);
        const auto* wide_format = reinterpret_cast<const wchar_t*>(cursor);
        cursor += (std::wcslen(wide_format) + 1) * sizeof(wchar_t);

        items.push_back(std::move(value));
    }
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
#endif
    return items;
}

// The alternative digits of the calling thread's locale, from 0, at most 100. glibc stores them
// as consecutive NUL-terminated strings ended by an empty one, trusted like the era table (the
// walk is bounded only by that and the cap); elsewhere (macOS) there are none. Call it under a
// clocale_user.
inline std::vector<std::string> timeio_read_alt_digits()
{
    std::vector<std::string> res;
#if defined(__GLIBC__)
    const char* ptr = nl_langinfo(ALT_DIGITS);
    for (std::size_t i = 0; ptr != nullptr && i < 100 && *ptr != '\0'; ++i)
    {
        res.emplace_back(ptr);
        ptr += res.back().size() + 1;
    }
#endif
    return res;
}

// The date and time data of the calling thread's current locale. Call it under a clocale_user.
inline timeio_raw_data timeio_read_data()
{
    static constexpr std::array<nl_item, 7> day_items =
        {DAY_1, DAY_2, DAY_3, DAY_4, DAY_5, DAY_6, DAY_7};
    static constexpr std::array<nl_item, 7> abbr_day_items =
        {ABDAY_1, ABDAY_2, ABDAY_3, ABDAY_4, ABDAY_5, ABDAY_6, ABDAY_7};
    static constexpr std::array<nl_item, 12> month_items =
        {MON_1, MON_2, MON_3, MON_4, MON_5, MON_6, MON_7, MON_8, MON_9, MON_10, MON_11, MON_12};
    static constexpr std::array<nl_item, 12> abbr_month_items =
        {ABMON_1, ABMON_2, ABMON_3, ABMON_4, ABMON_5, ABMON_6,
         ABMON_7, ABMON_8, ABMON_9, ABMON_10, ABMON_11, ABMON_12};

    timeio_raw_data res;
    for (std::size_t i = 0; i < 7; ++i)
    {
        res.day[i] = timeio_item(day_items[i]);
        res.abbr_day[i] = timeio_item(abbr_day_items[i]);
    }
    for (std::size_t i = 0; i < 12; ++i)
    {
        res.month[i] = timeio_item(month_items[i]);
        res.abbr_month[i] = timeio_item(abbr_month_items[i]);
    }
    res.am = timeio_item(AM_STR);
    res.pm = timeio_item(PM_STR);
    res.date_format = timeio_item(D_FMT);
    res.era_date_format = timeio_item(ERA_D_FMT);
    res.time_format = timeio_item(T_FMT);
    res.era_time_format = timeio_item(ERA_T_FMT);
    res.date_time_format = timeio_item(D_T_FMT);
    res.era_date_time_format = timeio_item(ERA_D_T_FMT);
    res.am_pm_format = timeio_item(T_FMT_AMPM);
    res.alt_digits = timeio_read_alt_digits();
    res.eras = timeio_read_eras();
    return res;
}
}
