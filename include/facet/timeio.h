/**
 * @file timeio.h
 * @lang{ZH}
 * 定义 `timeio` facet，提供对日期/时间值的格式化输出（`put`）与解析输入（`get`）功能，
 * 支持 `strftime`/`strptime` 风格的格式说明符（包括 `E`/`O` 修饰符与纪元扩展）。
 *
 * 还定义了以下辅助类型：
 * - `date_parse_helper`、`time_parse_helper`、`time_zone_parse_helper`：
 *   按需激活的解析状态容器，作为 `time_parse_context` 的基类；
 * - `time_parse_context`：聚合解析上下文，作为 `get()` 的输出参数，
 *   并提供向 `std::chrono::year_month_day`、`std::chrono::hh_mm_ss` 及
 *   `std::chrono::zoned_time` 的转换运算符。
 * @endif
 *
 * @lang{EN}
 * Defines the `timeio` facet, which provides locale-aware formatting (`put`) and
 * parsing (`get`) of date/time values using `strftime`/`strptime`-style format
 * specifiers, including `E`/`O` modifiers and era extensions.
 *
 * Also defines the following helper types:
 * - `date_parse_helper`, `time_parse_helper`, `time_zone_parse_helper`:
 *   conditionally activated parse-state containers that serve as base classes
 *   of `time_parse_context`;
 * - `time_parse_context`: an aggregate parse context used as the output argument
 *   of `get()`, providing conversion operators to
 *   `std::chrono::year_month_day`, `std::chrono::hh_mm_ss`, and
 *   `std::chrono::zoned_time`.
 * @endif
 */
#pragma once
#include <common/defs.h>
#include <common/metafunctions.h>
#include <common/prefix_tree.h>
#include <common/stamp_input_iterator.h>
#include <common/streambuf_defs.h>
#include <facet/ctype.h>
#include <facet/facet_common.h>
#include <facet/timeio_details.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace IOv2
{
/// @cond
template <typename, bool> struct date_parse_helper
{
    bool operator==(const date_parse_helper&) const = default;  // for test
};
/// @endcond

/**
 * @lang{ZH}
 * @brief 解析日期字段的辅助结构（`HaveDate = true` 特化）。
 *
 * 作为 `time_parse_context` 的基类，负责累积从格式说明符解析到的日期字段
 * （年、月、日、星期、一年中的第几天等），并在转换时通过 `compute_ymd()` 将
 * 这些字段还原为 `std::chrono::year_month_day`。
 *
 * @note 此结构为内部实现细节；请通过 `time_parse_context` 访问其功能。
 * @tparam CharT 字符类型，用于持有纪元条目的字符串。
 * @endif
 *
 * @lang{EN}
 * @brief Date-field accumulator helper struct (`HaveDate = true` specialization).
 *
 * Serves as a base class of `time_parse_context`, accumulating date fields
 * parsed from format specifiers (year, month, day, weekday, day-of-year, etc.)
 * and reconstructing a `std::chrono::year_month_day` via `compute_ymd()` on
 * conversion.
 *
 * @note This struct is an internal implementation detail; access its functionality
 *       through `time_parse_context`.
 * @tparam CharT The character type, used for era entry strings.
 * @endif
 */
template <typename CharT>
struct date_parse_helper<CharT, true>
{
    /// @cond
    bool operator==(const date_parse_helper&) const = default;  // for test
    /// @endcond
    date_parse_helper()
    {
        using namespace std::chrono;
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};

        m_year = int(ymd.year());
        m_month = unsigned(ymd.month());
        m_mday = unsigned(ymd.day());
    }

    /**
     * @lang{ZH}
     * @brief 将已累积的日期字段转换为 `std::chrono::year_month_day`。
     * @return 还原出的日历日期。
     * @throw stream_error 若还原结果不是有效的日历日期。
     * @endif
     *
     * @lang{EN}
     * @brief Converts the accumulated date fields to `std::chrono::year_month_day`.
     * @return The reconstructed calendar date.
     * @throw stream_error If the reconstructed date is not a valid calendar date.
     * @endif
     */
    explicit operator std::chrono::year_month_day() const
    {
        auto ymd = compute_ymd();
        if (!ymd.ok())
            throw stream_error("timeio get error: year_month_day is not a valid calendar date");
        return ymd;
    }

    /**
     * @lang{ZH}
     * @brief 根据已累积的字段推算 `std::chrono::year_month_day`（不做有效性检查）。
     *
     * 按以下优先级尝试各种推算路径：
     * 1. 年 + 月 + 日；
     * 2. 年 + 年内第几天（`%j`）；
     * 3. ISO-8601 周日期（`%G`/`%Y` + `%V` + 星期）；
     * 4. 根据周序号（`%U`/`%W`）和星期推算；
     * 5. 仅根据年份或世纪等做尽力推算。
     * @return 推算出的日历日期（可能无效，调用方需检查 `ok()`）。
     * @endif
     *
     * @lang{EN}
     * @brief Deduces a `std::chrono::year_month_day` from the accumulated fields
     *        without validity checking.
     *
     * The following deduction paths are tried in priority order:
     * 1. year + month + day;
     * 2. year + day-of-year (`%j`);
     * 3. ISO-8601 week date (`%G`/`%Y` + `%V` + weekday);
     * 4. deduction from week-of-year (`%U`/`%W`) and weekday;
     * 5. best-effort deduction from year or century alone.
     * @return The deduced calendar date (may be invalid; caller must check `ok()`).
     * @endif
     */
    std::chrono::year_month_day compute_ymd() const
    {
        using namespace std::chrono;
        if (m_have_year && m_have_mon && m_have_mday)
            return year_month_day{ year{m_year}, month{static_cast<uint8_t>(m_month)}, day{m_mday} };
        if (m_have_year && m_have_yday)
        {
            sys_days sd = sys_days{ year{m_year} / 1 / 1 } + days{ m_yday };
            return year_month_day{sd};
        }

        // ISO-8601 week date. Prefer the ISO year (%G); if it is absent but a
        // Gregorian year (%Y) was supplied alongside %V and a weekday, fall back
        // to that year instead of dropping the week number -- otherwise the
        // deduction path below would silently default %V to week 1. The two
        // fully-specified branches above (year+mon+mday, year+yday) already
        // returned, so reaching here means the date is not pinned down by an
        // explicit month/day or day-of-year.
        if (m_have_iso_8601_week && m_have_wday && (m_have_iso_8601_year || m_have_year))
        {
            int iso_year = m_have_iso_8601_year ? m_iso_8601_year : m_year;
            int iso_wd = (m_wday == 0 ? 7 : m_wday);
            year_month_day jan4 = year{iso_year}/January/4;
            weekday wd_jan4{sys_days{jan4}};
            sys_days week1_monday = sys_days{jan4} - (wd_jan4 - Monday);
            sys_days final = week1_monday + days{7 * (m_iso_8601_week - 1)} + days{iso_wd - 1};
            return year_month_day{final};
        }

        auto deduced_year = m_year;
        // Deduce year
        if (!m_have_year)
        {
            if (m_have_century && m_have_year_in_century)
                deduced_year = deduced_year % 100 + m_century * 100; // NOLINT(bugprone-branch-clone)
            else if ((m_have_year_of_era) && (!m_era_items.empty()))
            {
                using namespace TimeioHelper;

                // use month and day to decide
                if (m_have_mon && m_have_mday)
                {
                    auto it = m_era_items.begin();
                    for (; it != m_era_items.end(); ++it)
                    {
                        int64_t est_year_64 = static_cast<int64_t>(it->from_year)
                            + (static_cast<int64_t>(m_year_of_era) - static_cast<int64_t>(it->offset)) * it->direction;
                        int est_year = static_cast<int>(std::clamp<int64_t>(est_year_64,
                            std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
                        bool cmp1 = era_small_or_equal(it->from_year, it->from_month, it->from_day,
                                                        est_year, m_month, m_mday) &&
                                    era_small_or_equal(est_year, m_month, m_mday,
                                                        it->to_year, it->to_month, it->to_day);
                        bool cmp2 = era_small_or_equal(est_year, m_month, m_mday,
                                                        it->from_year, it->from_month, it->from_day) &&
                                    era_small_or_equal(it->to_year, it->to_month, it->to_day,
                                                        est_year, m_month, m_mday);
                        if (!cmp1 && !cmp2) continue;
                        deduced_year = est_year;
                        break;
                    }

                    // nothing matches, choose the first item.
                    if (it == m_era_items.end())
                        deduced_year = m_era_items.begin()->from_year;
                }
                else if (m_have_mon)
                {
                    auto it = m_era_items.begin();
                    for (; it != m_era_items.end(); ++it)
                    {
                        int64_t est_year_64 = static_cast<int64_t>(it->from_year)
                            + (static_cast<int64_t>(m_year_of_era) - static_cast<int64_t>(it->offset)) * it->direction;
                        int est_year = static_cast<int>(std::clamp<int64_t>(est_year_64,
                            std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
                        bool cmp1 = era_small_or_equal(it->from_year, it->from_month, it->from_day,
                                                        est_year, m_month, m_mday) &&
                                    era_small_or_equal(est_year, m_month, m_mday,
                                                        it->to_year, it->to_month, it->to_day);
                        bool cmp2 = era_small_or_equal(est_year, m_month, m_mday,
                                                        it->from_year, it->from_month, it->from_day) &&
                                    era_small_or_equal(it->to_year, it->to_month, it->to_day,
                                                        est_year, m_month, m_mday);
                        if (!cmp1 && !cmp2) continue;
                        deduced_year = est_year;
                        break;
                    }

                    // nothing matches, choose the first item.
                    if (it == m_era_items.end())
                        deduced_year = m_era_items.begin()->from_year;
                }
                else
                {
                    auto it = m_era_items.begin();
                    for (; it != m_era_items.end(); ++it)
                    {
                        int64_t est_year_64 = static_cast<int64_t>(it->from_year)
                            + (static_cast<int64_t>(m_year_of_era) - static_cast<int64_t>(it->offset)) * it->direction;
                        int est_year = static_cast<int>(std::clamp<int64_t>(est_year_64,
                            std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
                        if ((it->from_year > est_year) || (est_year > it->to_year)) continue;
                        deduced_year = est_year;
                        break;
                    }

                    // nothing matches, choose the first item.
                    if (it == m_era_items.end())
                        deduced_year = m_era_items.begin()->from_year;
                }
            }

            // Fuzzy matching, we do not have enough information, but need to set year.
            else if (m_have_year_in_century) { /* do nothing */ }
            else if (m_have_century)
                deduced_year = deduced_year % 100 + m_century * 100;
            else if (!m_era_items.empty())
                deduced_year = m_era_items.begin()->from_year;
        }

        auto deduced_month = m_month;
        auto deduced_mday = m_mday;
        int deduced_yday = static_cast<int>(m_yday);
        bool have_yday = m_have_yday;
        auto deduced_wday = m_wday;
        // Deduce month / mday. When neither is given, both are derived from the
        // day-of-year. When the month IS given but the day is not, the day-of-month
        // is computed relative to the *reported* month (deduced_month) rather than
        // the yday-derived month, so the returned month and day stay mutually
        // consistent (contradictory month-vs-week input is GIGO and may still yield
        // an out-of-range day, since this conversion has no error channel).
        if ((m_have_uweek || m_have_wweek) && m_have_wday && (!have_yday))
        {
            int w_offset = m_have_uweek ? 0 : 1;

            // calculate the week of day for Jan 1
            int wday = day_of_the_week(deduced_year, 1, 1);

            deduced_yday = ((7 - (wday - w_offset)) % 7 + (m_week_no - 1) * 7 + (m_wday - w_offset + 7) % 7);
            have_yday = true;
        }

        if (!m_have_mon || !m_have_mday)
        {
            if (have_yday)
            {
                if (deduced_yday < 0)
                {
                    deduced_year -= 1;
                    deduced_yday += isleap(deduced_year) ? 366 : 365;
                }
                while (deduced_yday >= (isleap(deduced_year) ? 366 : 365))
                {
                    deduced_yday -= isleap(deduced_year) ? 366 : 365;
                    deduced_year += 1;
                }
                int t_mon = 0;
                while (t_mon < 12 && s_mon_yday[isleap(deduced_year)][t_mon] <= deduced_yday)
                    t_mon++;
                if (!m_have_mon) deduced_month = t_mon;
                if (!m_have_mday) deduced_mday = (deduced_yday - s_mon_yday[isleap(deduced_year)][deduced_month - 1] + 1);
            }
            else if (m_have_wday)
            {
                // assume week number is 1;
                auto j1_wday = day_of_the_week(deduced_year, 1, 1);

                if (!have_yday)
                {
                    deduced_yday = ((7 - (j1_wday)) % 7 + (deduced_wday + 7) % 7);
                    have_yday = true;
                }

                if (!m_have_mday || !m_have_mon)
                {
                    int t_mon = 0;
                    while (t_mon < 12 && s_mon_yday[isleap(deduced_year)][t_mon] <= deduced_yday)
                        t_mon++;
                    if (!m_have_mon)
                        deduced_month = t_mon;
                    if (!m_have_mday)
                        deduced_mday = (deduced_yday - s_mon_yday[isleap(deduced_year)][deduced_month - 1] + 1);
                }
            }
            else if (m_have_uweek || m_have_wweek)
            {
                // assume wday is 1
                int w_offset = m_have_uweek ? 0 : 1;
                auto j1_wday = day_of_the_week(deduced_year, 1, 1);

                if (!have_yday)
                {
                    deduced_yday = ((7 - (j1_wday - w_offset)) % 7 + (m_week_no - 1) * 7 + (1 - w_offset + 7) % 7);
                    have_yday = true;
                }

                if (!m_have_mday || !m_have_mon)
                {
                    if (deduced_yday < 0)
                    {
                        deduced_year -= 1;
                        deduced_yday += isleap(deduced_year) ? 366 : 365;
                    }
                    while (deduced_yday >= (isleap(deduced_year) ? 366 : 365))
                    {
                        deduced_yday -= isleap(deduced_year) ? 366 : 365;
                        deduced_year += 1;
                    }
                    int t_mon = 0;
                    while (t_mon < 12 && s_mon_yday[isleap(deduced_year)][t_mon] <= deduced_yday)
                        t_mon++;
                    if (!m_have_mon)
                        deduced_month = t_mon;
                    if (!m_have_mday)
                        deduced_mday = (deduced_yday - s_mon_yday[isleap(deduced_year)][deduced_month - 1] + 1);
                }
            }
        }

        return year_month_day{ year{deduced_year}, month{static_cast<uint8_t>(deduced_month)}, day{static_cast<uint8_t>(deduced_mday)} };
    }

    using era_entry = typename ft_basic<timeio<CharT>>::era_entry;

    std::list<era_entry> m_era_items;

    int m_century = 0;
    int m_iso_8601_year = 0;
    int m_year_of_era = 0;

    int     m_year = 0;
    uint8_t m_month = 1;        // months since January – [1, 12]
    uint8_t m_iso_8601_week = 0;
    uint8_t m_week_no = 0;
    uint8_t m_mday = 1;         // day of the month – [1, 31]
    uint8_t m_wday = 0;         // days since Sunday – [0, 6]
    unsigned short m_yday = 0;  // days since January 1 – [0, 365]

    bool is_init : 1 = false;
    bool m_have_century : 1 = false;
    bool m_have_year : 1 = false;
    bool m_have_year_in_century : 1 = false;
    bool m_have_iso_8601_year : 1 = false;
    bool m_have_iso_8601_week : 1 = false;
    bool m_have_year_of_era : 1 = false;

    bool m_have_mon : 1 = false;
    bool m_have_uweek : 1 = false;
    bool m_have_wweek : 1 = false;

    bool m_have_yday : 1 = false;
    bool m_have_mday : 1 = false;
    bool m_have_wday : 1 = false;

private:
    constexpr static const unsigned short int s_mon_yday[2][13] =
    {
        /* Normal years.  */
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
        /* Leap years.  */
        { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
    };

    static bool isleap(int year)
    {
        return ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0));
    }

    static int day_of_the_week(int year, int month, int mday)
    {
        /* We know that January 1st 1970 was a Thursday (= 4).  Compute the
            difference between this date and the one on TM and so determine
            the weekday.  */
        month -= 1;
        int64_t corr_year = static_cast<int64_t>(year) - (month < 2);
        int64_t wday = (-473 + (365 * (static_cast<int64_t>(year) - 1970)) + (corr_year / 4) - ((corr_year / 4) / 25) + ((corr_year / 4) % 25 < 0) + (((corr_year / 4) / 25) / 4)
            + s_mon_yday[0][month] + mday - 1);
        return static_cast<int>(((wday % 7) + 7) % 7);
    }
};

/// @cond
template <bool> struct time_parse_helper
{
    bool operator==(const time_parse_helper&) const = default;  // for test
};
/// @endcond

/**
 * @lang{ZH}
 * @brief 解析时间字段的辅助结构（`HaveTime = true` 特化）。
 *
 * 作为 `time_parse_context` 的基类，累积从格式说明符解析到的时、分、秒及 AM/PM 标志，
 * 并在转换时还原为 `std::chrono::hh_mm_ss<std::chrono::seconds>`。
 *
 * @note 此结构为内部实现细节；请通过 `time_parse_context` 访问其功能。
 * @endif
 *
 * @lang{EN}
 * @brief Time-field accumulator helper struct (`HaveTime = true` specialization).
 *
 * Serves as a base class of `time_parse_context`, accumulating hour, minute,
 * second, and AM/PM fields parsed from format specifiers, then reconstructing
 * a `std::chrono::hh_mm_ss<std::chrono::seconds>` on conversion.
 *
 * @note This struct is an internal implementation detail; access its functionality
 *       through `time_parse_context`.
 * @endif
 */
template <>
struct time_parse_helper<true>
{
    /// @cond
    bool operator==(const time_parse_helper&) const = default;  // for test
    /// @endcond
    /**
     * @lang{ZH}
     * @brief 将已累积的时间字段转换为 `std::chrono::hh_mm_ss<std::chrono::seconds>`。
     *
     * 若通过 `%I`（12 小时制）解析并设置了 PM 标志，则自动加上 12 小时。
     * @return 还原出的 24 小时制时间。
     * @endif
     *
     * @lang{EN}
     * @brief Converts the accumulated time fields to
     *        `std::chrono::hh_mm_ss<std::chrono::seconds>`.
     *
     * If the hour was parsed via `%I` (12-hour clock) and the PM flag is set,
     * 12 hours are added automatically.
     * @return The reconstructed 24-hour time-of-day.
     * @endif
     */
    explicit operator std::chrono::hh_mm_ss<std::chrono::seconds>() const
    {
        uint8_t hour_in_24 = m_hour;
        // When %I sets m_have_I, m_hour was stored as (mem % 12), so 12 AM is
        // already normalised to 0 at parse time and m_hour is always in [0,11]
        // here; only the PM (+12) adjustment remains.
        if (m_have_I && m_is_pm && hour_in_24 < 12)
            hour_in_24 += 12;

        std::chrono::seconds time_sec =
            std::chrono::hours{hour_in_24} +
            std::chrono::minutes{m_minute} +
            std::chrono::seconds{m_second};

        return std::chrono::hh_mm_ss{time_sec};
    }

    uint8_t m_hour = 0;         // hours since midnight – [0, 23]
    uint8_t m_minute = 0;       // minutes after the hour – [0, 59]
    uint8_t m_second = 0;       // seconds after the minute – [0, 59]
    bool m_have_I : 1 = false;
    bool m_is_pm : 1 = false;
};

/// @cond
template <bool> struct time_zone_parse_helper
{
    bool operator==(const time_zone_parse_helper&) const = default;  // for test
};
/// @endcond

/**
 * @lang{ZH}
 * @brief 解析时区字段的辅助结构（`HaveTimeZone = true` 特化）。
 *
 * 作为 `time_parse_context` 的基类，累积从 `%Z` 格式说明符解析到的时区名称或
 * 时区缩写，并在转换时还原为 `const std::chrono::time_zone*`。
 *
 * @note 此结构为内部实现细节；请通过 `time_parse_context` 访问其功能。
 * @endif
 *
 * @lang{EN}
 * @brief Time-zone-field accumulator helper struct (`HaveTimeZone = true` specialization).
 *
 * Serves as a base class of `time_parse_context`, accumulating the timezone name
 * or abbreviation parsed from the `%Z` format specifier, then resolving it to a
 * `const std::chrono::time_zone*` on conversion.
 *
 * @note This struct is an internal implementation detail; access its functionality
 *       through `time_parse_context`.
 * @endif
 */
template <>
struct time_zone_parse_helper<true>
{
    /// @cond
    bool operator==(const time_zone_parse_helper&) const = default;  // for test
    /// @endcond
    /**
     * @lang{ZH}
     * @brief 将已解析的时区信息转换为 `const std::chrono::time_zone*`。
     *
     * 若已解析到完整 IANA 时区名称，则通过 `std::chrono::locate_zone` 定位；
     * 若仅有时区缩写（有歧义），则抛出 `stream_error`；
     * 若两者均无，则默认返回 UTC。
     * @return 指向时区对象的指针（从不为 null）。
     * @throw stream_error 若时区缩写有歧义或时区数据库不可用。
     * @endif
     *
     * @lang{EN}
     * @brief Converts the parsed timezone information to `const std::chrono::time_zone*`.
     *
     * If a full IANA timezone name was parsed, it is located via
     * `std::chrono::locate_zone`. If only an abbreviation was parsed (ambiguous),
     * a `stream_error` is thrown. If neither is present, UTC is returned as default.
     * @return A non-null pointer to the timezone object.
     * @throw stream_error If the timezone abbreviation is ambiguous or the tz
     *        database is unavailable.
     * @endif
     */
    explicit operator const std::chrono::time_zone*() const
    {
        if (!m_zone_name.empty())
        {
            try { return std::chrono::locate_zone(m_zone_name); }
            catch (...) {} // NOLINT(bugprone-empty-catch)
        }
        if (!m_zone_abbrev.empty())
            throw stream_error(
                "timeio get error: timezone abbreviation '" + m_zone_abbrev + "' is ambiguous");
        try { return std::chrono::locate_zone("UTC"); }
        catch (...)
        {
            throw stream_error(
                "timeio parse error: no usable time zone (tz database unavailable)");
        }
    }
    std::string m_zone_name;
    std::string m_zone_abbrev;
};

/**
 * @lang{ZH}
 * @brief `timeio::get()` 的聚合解析上下文。
 *
 * 将日期、时间、时区三个辅助结构组合为统一的解析状态容器，作为 `get()` 的输出参数。
 * 模板参数控制哪些字段被激活：
 * - `HaveDate`：启用日期字段（年、月、日、星期等）；
 * - `HaveTime`：启用时间字段（时、分、秒、AM/PM 等）；
 * - `HaveTimeZone`：启用时区字段（`%Z`）。
 *
 * 典型用法：
 * 1. 默认构造一个 `time_parse_context`；
 * 2. 将其传入一次或多次 `get()` 调用（跨多次调用累积同一值的字段）；
 * 3. 调用转换运算符提取结果；
 * 4. 若要解析下一个不同的时间值，先调用 `reset()`。
 *
 * @tparam CharT       字符类型。
 * @tparam HaveDate    为 `true` 时激活日期解析，默认 `true`。
 * @tparam HaveTime    为 `true` 时激活时间解析，默认 `true`。
 * @tparam HaveTimeZone 为 `true` 时激活时区解析，默认 `true`。
 * @endif
 *
 * @lang{EN}
 * @brief Aggregate parse context for `timeio::get()`.
 *
 * Combines the date, time, and time-zone helper structs into a single
 * parse-state container used as the output argument of `get()`.
 * Template parameters control which fields are activated:
 * - `HaveDate`: enables date fields (year, month, day, weekday, etc.);
 * - `HaveTime`: enables time fields (hour, minute, second, AM/PM, etc.);
 * - `HaveTimeZone`: enables timezone fields (`%Z`).
 *
 * Typical usage:
 * 1. Default-construct a `time_parse_context`;
 * 2. Pass it to one or more `get()` calls (fields of the *same* value
 *    accumulate across multiple calls);
 * 3. Call a conversion operator to extract the result;
 * 4. Call `reset()` before parsing a *different* time value.
 *
 * @tparam CharT        The character type.
 * @tparam HaveDate     Activates date parsing when `true` (default `true`).
 * @tparam HaveTime     Activates time parsing when `true` (default `true`).
 * @tparam HaveTimeZone Activates timezone parsing when `true` (default `true`).
 * @endif
 */
template <typename CharT, bool HaveDate = true, bool HaveTime = true, bool HaveTimeZone = true>
struct time_parse_context
    : date_parse_helper<CharT, HaveDate>
    , time_parse_helper<HaveTime>
    , time_zone_parse_helper<HaveTimeZone>
{
    /// @cond
    bool operator==(const time_parse_context&) const = default;
    /// @endcond

    /**
     * @lang{ZH}
     * @brief 默认构造函数，所有字段初始化为默认值。
     * @endif
     *
     * @lang{EN}
     * @brief Default constructor; all fields are initialized to their defaults.
     * @endif
     */
    time_parse_context() = default;

    /**
     * @lang{ZH}
     * @brief 清除所有已累积的解析状态，恢复到默认构造时的状态。
     *
     * 在复用同一上下文解析**不同**时间值之前调用此函数；
     * 若要在多次 `get()` 调用中累积**同一**时间值的字段，则无需调用。
     * @endif
     *
     * @lang{EN}
     * @brief Clears all accumulated parse state, restoring the context to its
     *        default-constructed state.
     *
     * Call this before reusing one context to parse a *different* time value;
     * skip it to keep accumulating fields of the *same* value across multiple
     * `get()` calls.
     * @endif
     */
    void reset() { *this = time_parse_context{}; }

    /**
     * @lang{ZH}
     * @brief 将已累积的日期、时间和时区字段转换为 `std::chrono::zoned_time<seconds>`。
     *
     * 仅当 `HaveDate`、`HaveTime` 和 `HaveTimeZone` 均为 `true` 时可用。
     * @return 还原出的带时区时间点。
     * @throw stream_error 若日期无效或时区解析失败。
     * @endif
     *
     * @lang{EN}
     * @brief Converts the accumulated date, time, and timezone fields to
     *        `std::chrono::zoned_time<seconds>`.
     *
     * Available only when `HaveDate`, `HaveTime`, and `HaveTimeZone` are all `true`.
     * @return The reconstructed zoned time point.
     * @throw stream_error If the date is invalid or the timezone lookup fails.
     * @endif
     */
    explicit operator const std::chrono::zoned_time<std::chrono::seconds>() const
        requires(HaveDate && HaveTime && HaveTimeZone)
    {
        using namespace std::chrono;
        auto ymd = static_cast<year_month_day>(*this);
        auto hms = static_cast<std::chrono::hh_mm_ss<std::chrono::seconds>>(*this);
        auto tz = static_cast<const std::chrono::time_zone*>(*this);

        local_time<seconds> lt{ local_days{ymd} + hms.to_duration() };
        return zoned_time<seconds>{tz, lt};
    }

    /**
     * @lang{ZH}
     * @brief 将已累积的日期和时间字段转换为 `std::tm`。
     *
     * 仅当 `HaveDate` 和 `HaveTime` 均为 `true` 时可用。
     * @return 还原出的 `std::tm` 结构体。
     * @throw stream_error 若日期无效。
     * @endif
     *
     * @lang{EN}
     * @brief Converts the accumulated date and time fields to `std::tm`.
     *
     * Available only when both `HaveDate` and `HaveTime` are `true`.
     * @return The reconstructed `std::tm` struct.
     * @throw stream_error If the date is invalid.
     * @endif
     */
    explicit operator std::tm() const
        requires(HaveDate && HaveTime)
    {
        using namespace std::chrono;
        auto ymd = static_cast<year_month_day>(*this);
        auto hms = static_cast<std::chrono::hh_mm_ss<std::chrono::seconds>>(*this);

        int d = static_cast<int>(static_cast<unsigned>(ymd.day()));
        int m = static_cast<int>(static_cast<unsigned>(ymd.month()));
        int y = int(ymd.year());

        std::tm res{};
        res.tm_year = y - 1900;
        res.tm_mon  = m - 1;
        res.tm_mday = d;

        // Time fields
        res.tm_hour = int(hms.hours().count());
        res.tm_min  = int(hms.minutes().count());
        res.tm_sec  = int(hms.seconds().count());

        res.tm_isdst = -1;   // let the C library figure out DST

        res.tm_wday = static_cast<int>(weekday{sys_days{ymd}}.c_encoding());
        bool isLeap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);

        const int days[12] = {-1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333};
        res.tm_yday = days[res.tm_mon] + res.tm_mday + (isLeap && res.tm_mon >= 2 ? 1 : 0);

        return res;
    }
};

/**
 * @lang{ZH}
 * @brief 时间 I/O facet，提供日期/时间的格式化输出与 locale 感知解析。
 *
 * `timeio` 实现了 `strftime`/`strptime` 风格的日期时间格式化与解析，
 * 支持完整的格式说明符集合（包括 `E`/`O` 修饰符与纪元扩展）。
 * 从 `timeio_conf<CharT>` 加载 locale 数据（星期/月份名称、格式串、
 * 替代数字、纪元条目），可选与 `ctype<CharT>` 配合使用以支持 locale
 * 感知的空白字符识别。
 *
 * **格式化**（`put`）：接受 `std::chrono::year_month_day`、
 * `std::chrono::hh_mm_ss`、`std::chrono::zoned_time` 或 `std::tm`，
 * 将其按指定格式串写入输出迭代器。
 *
 * **解析**（`get`）：从输入迭代器按指定格式串解析时间字段，
 * 将结果累积到 `time_parse_context` 中；解析完成后通过转换运算符
 * 提取 `year_month_day`、`hh_mm_ss` 或 `zoned_time`。
 *
 * @note 格式串来自 locale 数据库（`nl_langinfo`），视为受信任输入；
 *   含自引用格式串（如 `D_T_FMT == "%c"`）将导致无限递归。参见
 *   `do_get` 中关于受信任 locale 假设的说明。
 *
 * @tparam CharT 字符类型（`char`、`wchar_t`、`char8_t`、`char32_t`）。
 * @endif
 *
 * @lang{EN}
 * @brief Time I/O facet providing locale-aware date/time formatting and parsing.
 *
 * `timeio` implements `strftime`/`strptime`-style date-time formatting and
 * parsing, supporting the full set of format specifiers including `E`/`O`
 * modifiers and era extensions. It loads locale data from `timeio_conf<CharT>`
 * (weekday/month names, format strings, alternative digits, era entries) and
 * optionally cooperates with `ctype<CharT>` for locale-aware whitespace
 * recognition.
 *
 * **Formatting** (`put`): accepts `std::chrono::year_month_day`,
 * `std::chrono::hh_mm_ss`, `std::chrono::zoned_time`, or `std::tm` and
 * writes the result to an output iterator according to the given format string.
 *
 * **Parsing** (`get`): parses time fields from an input iterator according to
 * a format string, accumulating results into a `time_parse_context`; after
 * parsing, a conversion operator on the context extracts a `year_month_day`,
 * `hh_mm_ss`, or `zoned_time`.
 *
 * @note Format strings sourced from the locale database (`nl_langinfo`) are
 *   treated as trusted input; a self-referential format string (e.g.
 *   `D_T_FMT == "%c"`) would cause unbounded recursion. See the note in
 *   `do_get` regarding the trusted-locale assumption.
 *
 * @tparam CharT The character type (`char`, `wchar_t`, `char8_t`, `char32_t`).
 * @endif
 */
template <typename CharT>
class timeio
{
    using era_entry = typename ft_basic<timeio<CharT>>::era_entry;

public:
    using create_rules =
        facet_create_rule<facet_create_pack<timeio_conf<CharT>, ctype<CharT>>,
                          timeio_conf<CharT>>;

    using char_type = CharT;

    /**
     * @lang{ZH}
     * @brief 构造函数，从 locale 配置和 ctype facet 初始化。
     *
     * 先委托给单参数构造函数完成 locale 数据加载，再保存 `ctype` 指针用于
     * locale 感知的空白字符识别（`is_space`）。
     * @tparam TConfPtr  指向 `timeio_conf<CharT>` 的 `shared_ptr` 类型。
     * @tparam TCtypePtr 指向 `ctype<CharT>` 的 `shared_ptr` 类型。
     * @param p_obj   locale 配置对象（不得为空）。
     * @param p_ctype ctype facet（不得为空）。
     * @throw std::runtime_error 若任一指针为空。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that initializes from a locale configuration and a ctype facet.
     *
     * Delegates to the single-argument constructor to load locale data, then
     * stores the `ctype` pointer for locale-aware whitespace recognition in
     * `is_space`.
     * @tparam TConfPtr  A `shared_ptr` type pointing to `timeio_conf<CharT>`.
     * @tparam TCtypePtr A `shared_ptr` type pointing to `ctype<CharT>`.
     * @param p_obj   The locale configuration object (must not be null).
     * @param p_ctype The ctype facet (must not be null).
     * @throw std::runtime_error If either pointer is null.
     * @endif
     */
    template <shared_ptr_to<timeio_conf<CharT>> TConfPtr,
              shared_ptr_to<ctype<CharT>> TCtypePtr>
    timeio(TConfPtr p_obj, TCtypePtr p_ctype)
        : timeio(p_obj)
    {
        if (!p_ctype) throw std::runtime_error("shared_ptr is empty");
        m_ctype = p_ctype;
    }

    /**
     * @lang{ZH}
     * @brief 构造函数，从 locale 配置初始化。
     *
     * 从 `timeio_conf<CharT>` 复制所有 locale 数据（名称、格式串、纪元条目等），
     * 验证名称表的唯一性，并构建用于高效解析的前缀树（星期、月份、AM/PM、
     * 纪元名称、替代数字）。若未提供 ctype，空白字符识别使用基础 ASCII 判断。
     * @tparam TConfPtr 指向 `timeio_conf<CharT>` 的 `shared_ptr` 类型。
     * @param p_obj locale 配置对象（不得为空）。
     * @throw std::runtime_error 若指针为空或 locale 名称表存在重复/空条目。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that initializes from a locale configuration.
     *
     * Copies all locale data from `timeio_conf<CharT>` (names, format strings,
     * era entries, etc.), validates the name tables for uniqueness, and builds
     * prefix tries for efficient parsing (weekday, month, AM/PM, era names,
     * alternative digits). If no ctype is provided, whitespace recognition uses
     * basic ASCII comparison.
     * @tparam TConfPtr A `shared_ptr` type pointing to `timeio_conf<CharT>`.
     * @param p_obj The locale configuration object (must not be null).
     * @throw std::runtime_error If the pointer is null or the locale name tables
     *        contain duplicates or empty entries.
     * @endif
     */
    template <shared_ptr_to<timeio_conf<CharT>> TConfPtr>
    timeio(TConfPtr p_obj)
    {
        if (!p_obj) throw std::runtime_error("shared_ptr is empty");
        m_day = p_obj->day_names();
        m_abbr_day = p_obj->abbr_day_names();
        m_month = p_obj->month_names();
        m_abbr_month = p_obj->abbr_month_names();
        m_alt_digits = p_obj->alt_digit_names();
        m_am = p_obj->am_name();
        m_pm = p_obj->pm_name();
        m_date_format = p_obj->date_format();
        m_era_date_format = p_obj->era_date_format();
        m_time_format = p_obj->time_format();
        m_era_time_format = p_obj->era_time_format();
        m_time_zone_format = p_obj->time_zone_format();
        m_era_time_zone_format = p_obj->era_time_zone_format();
        m_date_time_format = p_obj->date_time_format();
        m_era_date_time_format = p_obj->era_date_time_format();
        m_date_time_zone_format = p_obj->date_time_zone_format();
        m_era_date_time_zone_format = p_obj->era_date_time_zone_format();
        m_am_pm_format = p_obj->am_pm_format();
        m_era_master = p_obj->era_items();

        // Validate the locale name tables up front, so malformed locale data
        // fails here with one clear error instead of a cryptic prefix_tree
        // "duplicate items" throw deep inside add(), or a silent zero-length
        // mis-match at parse time. Day/month names must be non-empty and must
        // not map one spelling to two different indices; AM/PM may legitimately
        // be empty (e.g. de_DE / fr_FR / ru_RU) and is tolerated.
        validate_locale_names();

        for (int i = 0; i < 7; ++i)
        {
            m_day_tree.add(p_obj->day_names()[i], i);
            m_day_tree.add(p_obj->abbr_day_names()[i], i);
        }

        for (int i = 0; i < 12; ++i)
        {
            m_month_tree.add(p_obj->month_names()[i], i);
            m_month_tree.add(p_obj->abbr_month_names()[i], i);
        }

        if (!m_am.empty()) m_am_pm_tree.add(m_am, 0);
        if (!m_pm.empty()) m_am_pm_tree.add(m_pm, 1);

        create_era_name_tree();
        create_alt_digits_tree();
    }

public:
    /**
     * @lang{ZH}
     * @brief 返回星期全称数组（索引 0 为星期日，索引 6 为星期六）。
     * @endif
     * @lang{EN}
     * @brief Returns the full weekday name array (index 0 = Sunday, index 6 = Saturday).
     * @endif
     */
    const std::array<std::basic_string<CharT>, 7>& day_names() const noexcept { return m_day; }
    /**
     * @lang{ZH}
     * @brief 返回星期缩写数组（索引 0 为星期日，索引 6 为星期六）。
     * @endif
     * @lang{EN}
     * @brief Returns the abbreviated weekday name array (index 0 = Sunday, index 6 = Saturday).
     * @endif
     */
    const std::array<std::basic_string<CharT>, 7>& abbr_day_names() const noexcept { return m_abbr_day; }
    /**
     * @lang{ZH}
     * @brief 返回月份全称数组（索引 0 为一月，索引 11 为十二月）。
     * @endif
     * @lang{EN}
     * @brief Returns the full month name array (index 0 = January, index 11 = December).
     * @endif
     */
    const std::array<std::basic_string<CharT>, 12>& month_names() const noexcept { return m_month; }
    /**
     * @lang{ZH}
     * @brief 返回月份缩写数组（索引 0 为一月，索引 11 为十二月）。
     * @endif
     * @lang{EN}
     * @brief Returns the abbreviated month name array (index 0 = January, index 11 = December).
     * @endif
     */
    const std::array<std::basic_string<CharT>, 12>& abbr_month_names() const noexcept { return m_abbr_month; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的替代数字字符串数组（最多 100 项）。
     * @endif
     * @lang{EN}
     * @brief Returns the locale-defined alternative digit strings (up to 100 entries).
     * @endif
     */
    const std::array<std::basic_string<CharT>, 100>& alt_digit_names() const noexcept { return m_alt_digits; }
    /**
     * @lang{ZH}
     * @brief 返回 AM 时段字符串。
     * @endif
     * @lang{EN}
     * @brief Returns the AM period string.
     * @endif
     */
    const std::basic_string<CharT>& am_name() const noexcept { return m_am; }
    /**
     * @lang{ZH}
     * @brief 返回 PM 时段字符串。
     * @endif
     * @lang{EN}
     * @brief Returns the PM period string.
     * @endif
     */
    const std::basic_string<CharT>& pm_name() const noexcept { return m_pm; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期格式串（对应 `%x`）。
     * @endif
     * @lang{EN}
     * @brief Returns the locale date format string (corresponding to `%x`).
     * @endif
     */
    const std::basic_string<CharT>& date_format() const noexcept { return m_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期格式串（对应 `%Ex`）。
     * @endif
     * @lang{EN}
     * @brief Returns the locale era-modified date format string (corresponding to `%Ex`).
     * @endif
     */
    const std::basic_string<CharT>& era_date_format() const noexcept { return m_era_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 时间格式串（对应 `%X`）。
     * @endif
     * @lang{EN}
     * @brief Returns the locale time format string (corresponding to `%X`).
     * @endif
     */
    const std::basic_string<CharT>& time_format() const noexcept { return m_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰时间格式串（对应 `%EX`）。
     * @endif
     * @lang{EN}
     * @brief Returns the locale era-modified time format string (corresponding to `%EX`).
     * @endif
     */
    const std::basic_string<CharT>& era_time_format() const noexcept { return m_era_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期时间格式串（对应 `%c`）。
     * @endif
     * @lang{EN}
     * @brief Returns the locale date-time format string (corresponding to `%c`).
     * @endif
     */
    const std::basic_string<CharT>& date_time_format() const noexcept { return m_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期时间格式串（对应 `%Ec`）。
     * @endif
     * @lang{EN}
     * @brief Returns the locale era-modified date-time format string (corresponding to `%Ec`).
     * @endif
     */
    const std::basic_string<CharT>& era_date_time_format() const noexcept { return m_era_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 AM/PM 时间格式串（对应 `%r`）。
     * @endif
     * @lang{EN}
     * @brief Returns the AM/PM time format string (corresponding to `%r`).
     * @endif
     */
    const std::basic_string<CharT>& am_pm_format() const noexcept { return m_am_pm_format; }

    /**
     * @lang{ZH}
     * @brief 按单个格式字符（可带修饰符）格式化时间值。
     *
     * 将 `format`（以及可选的 `modifier`）组合为 `%[modifier]format` 格式串后
     * 委托给 `put(out, t, fmt)`。
     * @tparam OutIt 输出迭代器类型。
     * @tparam TVal  时间值类型（`year_month_day`、`hh_mm_ss`、`zoned_time` 或 `std::tm`）。
     * @param out      输出迭代器。
     * @param t        要格式化的时间值。
     * @param format   格式字符（如 `'Y'`、`'m'`、`'d'`）。
     * @param modifier 可选修饰符（`'E'`、`'O'` 或 `0` 表示无修饰符）。
     * @return 写入后的输出迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a time value using a single format character (with optional modifier).
     *
     * Combines `format` and the optional `modifier` into a `%[modifier]format`
     * string, then delegates to `put(out, t, fmt)`.
     * @tparam OutIt Output iterator type.
     * @tparam TVal  Time value type (`year_month_day`, `hh_mm_ss`, `zoned_time`, or `std::tm`).
     * @param out      The output iterator.
     * @param t        The time value to format.
     * @param format   The format character (e.g. `'Y'`, `'m'`, `'d'`).
     * @param modifier Optional modifier (`'E'`, `'O'`, or `0` for none).
     * @return The output iterator after writing.
     * @endif
     */
    template <typename OutIt, typename TVal>
    OutIt put(OutIt out, const TVal& t, char format, char modifier = 0) const // NOLINT(bugprone-easily-swappable-parameters)
    {
        CharT fmt[4]; fmt[0] = static_cast<CharT>('%');
        if (modifier)
        {
            fmt[1] = modifier;
            fmt[2] = format;
            fmt[3] = static_cast<CharT>('\0');
        }
        else
        {
            fmt[1] = format;
            fmt[2] = static_cast<CharT>('\0');
        }

        return put(out, t, fmt);
    }

    /**
     * @lang{ZH}
     * @brief 将 `std::chrono::zoned_time` 按格式串格式化到输出迭代器。
     * @tparam OutIt       输出迭代器类型。
     * @tparam Duration    `zoned_time` 的时间精度类型。
     * @tparam TimeZonePtr `zoned_time` 的时区指针类型。
     * @param out 输出迭代器。
     * @param t   要格式化的带时区时间点。
     * @param fmt 格式串（`strftime` 风格）。
     * @return 写入后的输出迭代器。
     * @throw stream_error 若 `zoned_time` 的日期超出范围。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a `std::chrono::zoned_time` to an output iterator using a format string.
     * @tparam OutIt       Output iterator type.
     * @tparam Duration    Duration type of the `zoned_time`.
     * @tparam TimeZonePtr Time-zone pointer type of the `zoned_time`.
     * @param out The output iterator.
     * @param t   The zoned time point to format.
     * @param fmt The format string (`strftime`-style).
     * @return The output iterator after writing.
     * @throw stream_error If the date of the `zoned_time` is out of range.
     * @endif
     */
    template <typename OutIt, typename Duration, typename TimeZonePtr>
    OutIt put(OutIt out, const std::chrono::zoned_time<Duration, TimeZonePtr>& t, std::basic_string_view<CharT> fmt) const
    {
        auto local = t.get_local_time();
        auto local_day = std::chrono::floor<std::chrono::days>(local);

        std::chrono::year_month_day ymd{local_day};
        if (!ymd.ok())
            throw stream_error("timeio put error: zoned_time date is out of range");

        std::chrono::weekday wd(local_day);

        auto time_since_midnight = std::chrono::duration_cast<std::chrono::seconds>(local - local_day);
        std::chrono::hh_mm_ss<std::chrono::seconds> time_of_day{time_since_midnight};
        return do_put(out, fmt, &ymd, &wd, &time_of_day, t.get_time_zone());
    }

    /**
     * @lang{ZH}
     * @brief 将 `std::chrono::year_month_day` 按格式串格式化到输出迭代器。
     * @tparam OutIt 输出迭代器类型。
     * @param out 输出迭代器。
     * @param t   要格式化的日历日期（必须是有效日期）。
     * @param fmt 格式串（`strftime` 风格）。
     * @return 写入后的输出迭代器。
     * @throw stream_error 若 `t` 不是有效的日历日期。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a `std::chrono::year_month_day` to an output iterator using a format string.
     * @tparam OutIt Output iterator type.
     * @param out The output iterator.
     * @param t   The calendar date to format (must be a valid date).
     * @param fmt The format string (`strftime`-style).
     * @return The output iterator after writing.
     * @throw stream_error If `t` is not a valid calendar date.
     * @endif
     */
    template <typename OutIt>
    OutIt put(OutIt out, const std::chrono::year_month_day& t, std::basic_string_view<CharT> fmt) const
    {
        if (!t.ok())
            throw stream_error("timeio put error: year_month_day is not a valid calendar date");

        std::chrono::weekday wd(t);
        return do_put(out, fmt, &t, &wd, nullptr, nullptr);
    }

    /**
     * @lang{ZH}
     * @brief 将 `std::chrono::hh_mm_ss` 按格式串格式化到输出迭代器。
     *
     * 输入时间必须在 `[00:00:00, 23:59:59]` 范围内（不支持闰秒）。
     * @tparam OutIt     输出迭代器类型。
     * @tparam TDuration `hh_mm_ss` 的时间精度类型。
     * @param out 输出迭代器。
     * @param t   要格式化的一天中的时间。
     * @param fmt 格式串（`strftime` 风格）。
     * @return 写入后的输出迭代器。
     * @throw stream_error 若 `t` 超出有效时间范围（负数或 ≥ 24 小时）。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a `std::chrono::hh_mm_ss` to an output iterator using a format string.
     *
     * The input time must be in the range `[00:00:00, 23:59:59]`; leap seconds
     * are not supported.
     * @tparam OutIt     Output iterator type.
     * @tparam TDuration Duration type of the `hh_mm_ss`.
     * @param out The output iterator.
     * @param t   The time-of-day to format.
     * @param fmt The format string (`strftime`-style).
     * @return The output iterator after writing.
     * @throw stream_error If `t` is outside the valid range (negative or ≥ 24 hours).
     * @endif
     */
    template <typename OutIt, typename TDuration>
    OutIt put(OutIt out, const std::chrono::hh_mm_ss<TDuration>& t, std::basic_string_view<CharT> fmt) const
    {
        using namespace std::chrono;

        const seconds total = duration_cast<seconds>(t.to_duration());
        if (total < seconds{0} || total >= hours{24})
            throw stream_error("timeio put error: hh_mm_ss is not a valid time of day");

        const hh_mm_ss<seconds> t_sec{total};
        return do_put(out, fmt, nullptr, nullptr, &t_sec, nullptr);
    }

    /**
     * @lang{ZH}
     * @brief 将 `std::tm` 按格式串格式化到输出迭代器。
     *
     * 在格式化前对 `std::tm` 的各字段进行范围验证，并拒绝闰秒（`tm_sec == 60`）。
     * 具体检查项：月份 [0,11]、日期 [1,31]、时 [0,23]、分/秒 [0,59]，
     * 以及年份需在 `std::chrono::year` 的有效范围内，且日期组合需构成有效日历日期。
     * @tparam OutIt 输出迭代器类型。
     * @param out 输出迭代器。
     * @param t   要格式化的 `std::tm` 结构体。
     * @param fmt 格式串（`strftime` 风格）。
     * @return 写入后的输出迭代器。
     * @throw stream_error 若任何字段超出范围或日期组合无效。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a `std::tm` to an output iterator using a format string.
     *
     * Validates all `std::tm` fields before formatting and rejects leap seconds
     * (`tm_sec == 60`). Checks include: month [0,11], day [1,31], hour [0,23],
     * minute/second [0,59], year within the valid range of `std::chrono::year`,
     * and that the date combination forms a valid calendar date.
     * @tparam OutIt Output iterator type.
     * @param out The output iterator.
     * @param t   The `std::tm` struct to format.
     * @param fmt The format string (`strftime`-style).
     * @return The output iterator after writing.
     * @throw stream_error If any field is out of range or the date combination is invalid.
     * @endif
     */
    template <typename OutIt>
    OutIt put(OutIt out, const std::tm& t, std::basic_string_view<CharT> fmt) const
    {
        using namespace std::chrono;

        // Validate raw tm fields before constructing chrono types: year/month
        // truncate out-of-range values (short / unsigned char) and would silently
        // wrap, so range-check the integers first, then reject field combinations
        // that are individually in range but not a real calendar date (e.g. Feb 30).
        // tm_sec is intentionally limited to [0,59], NOT C's [0,60]: this facet
        // routes time-of-day through std::chrono::hh_mm_ss<seconds>, which cannot
        // represent a leap second (23:59:60 would normalise to 24:00:00 and be
        // mis-formatted), so a leap-second tm is rejected up front here rather
        // than silently corrupted on output.
        const int y = t.tm_year + 1900;
        if (t.tm_mon  < 0 || t.tm_mon  > 11 ||
            t.tm_mday < 1 || t.tm_mday > 31 ||
            t.tm_hour < 0 || t.tm_hour > 23 ||
            t.tm_min  < 0 || t.tm_min  > 59 ||
            t.tm_sec  < 0 || t.tm_sec  > 59 ||
            y < static_cast<int>(year::min()) || y > static_cast<int>(year::max()))
            throw stream_error("timeio put error: std::tm field out of range");

        year_month_day ymd{ year{y}, month{static_cast<unsigned>(t.tm_mon) + 1}, day{static_cast<unsigned>(t.tm_mday)} };
        if (!ymd.ok())
            throw stream_error("timeio put error: std::tm is not a valid calendar date");

        weekday wd{ymd};

        seconds sec = hours{t.tm_hour} + minutes{t.tm_min} + seconds{t.tm_sec};
        std::chrono::hh_mm_ss hms{sec};
        return do_put(out, fmt, &ymd, &wd, &hms, nullptr);
    }

    /**
     * @lang{ZH}
     * @brief 按单个格式字符（可带修饰符）从输入范围中解析时间字段。
     *
     * 将 `format` 与可选的 `modifier` 组合为 `%[modifier]format` 格式串后
     * 委托给 `get(beg, end, ctx, fmt)`。
     * @tparam TIter      双向迭代器或 `istreambuf_iterator` 类型。
     * @tparam TSent      哨兵类型。
     * @tparam HaveDate   是否解析日期字段。
     * @tparam HaveTime   是否解析时间字段。
     * @tparam HaveTimeZone 是否解析时区字段。
     * @param beg      输入范围起始迭代器。
     * @param end      输入范围结束哨兵。
     * @param ctx      累积解析结果的上下文（in/out）。
     * @param format   格式字符（如 `'Y'`、`'m'`、`'d'`）。
     * @param modifier 可选修饰符（`'E'`、`'O'` 或 `0` 表示无修饰符）。
     * @return 指向未被消费的第一个字符的迭代器。
     * @throw stream_error 若解析失败。
     * @endif
     *
     * @lang{EN}
     * @brief Parses time fields from an input range using a single format character
     *        (with optional modifier).
     *
     * Combines `format` and the optional `modifier` into a `%[modifier]format`
     * string, then delegates to `get(beg, end, ctx, fmt)`.
     * @tparam TIter      Bidirectional iterator or `istreambuf_iterator` type.
     * @tparam TSent      Sentinel type.
     * @tparam HaveDate   Whether date fields are parsed.
     * @tparam HaveTime   Whether time fields are parsed.
     * @tparam HaveTimeZone Whether timezone fields are parsed.
     * @param beg      Beginning of the input range.
     * @param end      End sentinel of the input range.
     * @param ctx      Parse context accumulating results (in/out).
     * @param format   The format character (e.g. `'Y'`, `'m'`, `'d'`).
     * @param modifier Optional modifier (`'E'`, `'O'`, or `0` for none).
     * @return Iterator pointing to the first unconsumed character.
     * @throw stream_error If parsing fails.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent, bool HaveDate, bool HaveTime, bool HaveTimeZone>
        requires (std::bidirectional_iterator<TIter> || is_istreambuf_iterator<TIter>)
    TIter get(TIter beg, TSent end, time_parse_context<char_type, HaveDate, HaveTime, HaveTimeZone>& ctx,
              char format, char modifier = 0) const // NOLINT(bugprone-easily-swappable-parameters)
    {
        CharT fmt[4]; fmt[0] = static_cast<CharT>('%');
        if (modifier)
        {
            fmt[1] = modifier;
            fmt[2] = format;
            fmt[3] = static_cast<CharT>('\0');
            return get(beg, end, ctx, fmt);
        }
        else
        {
            fmt[1] = format;
            fmt[2] = static_cast<CharT>('\0');
            return get(beg, end, ctx, fmt);
        }
    }

    /**
     * @lang{ZH}
     * @brief 按格式串从输入范围中解析时间字段，将结果累积到 `ctx`。
     *
     * 各格式说明符与 POSIX `strptime` / `std::chrono::from_stream` 的语义一致。
     * 复合说明符（`%c`、`%x`、`%X`、`%r`、`%EY`）会将 locale 提供的格式串
     * 展开后递归处理，详见 `do_get` 中关于受信任 locale 假设的说明。
     * @tparam TIter      双向迭代器或 `istreambuf_iterator` 类型。
     * @tparam TSent      哨兵类型。
     * @tparam HaveDate   是否解析日期字段。
     * @tparam HaveTime   是否解析时间字段。
     * @tparam HaveTimeZone 是否解析时区字段。
     * @param rp     输入范围起始迭代器。
     * @param rp_end 输入范围结束哨兵。
     * @param ctx    累积解析结果的上下文（in/out）。
     * @param _fmt   格式串（`strptime` 风格）。
     * @return 指向未被消费的第一个字符的迭代器。
     * @throw stream_error 若解析失败（格式不匹配或字段值超出范围）。
     * @endif
     *
     * @lang{EN}
     * @brief Parses time fields from an input range according to a format string,
     *        accumulating results into `ctx`.
     *
     * Format specifiers follow the semantics of POSIX `strptime` /
     * `std::chrono::from_stream`. Compound specifiers (`%c`, `%x`, `%X`, `%r`,
     * `%EY`) expand locale-provided format strings and re-enter recursively;
     * see the trusted-locale note in `do_get`.
     * @tparam TIter      Bidirectional iterator or `istreambuf_iterator` type.
     * @tparam TSent      Sentinel type.
     * @tparam HaveDate   Whether date fields are parsed.
     * @tparam HaveTime   Whether time fields are parsed.
     * @tparam HaveTimeZone Whether timezone fields are parsed.
     * @param rp     Beginning of the input range.
     * @param rp_end End sentinel of the input range.
     * @param ctx    Parse context accumulating results (in/out).
     * @param _fmt   The format string (`strptime`-style).
     * @return Iterator pointing to the first unconsumed character.
     * @throw stream_error If parsing fails (format mismatch or field value out of range).
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent, bool HaveDate, bool HaveTime, bool HaveTimeZone>
        requires (std::bidirectional_iterator<TIter> || is_istreambuf_iterator<TIter>)
    TIter get(TIter rp, TSent rp_end, time_parse_context<char_type, HaveDate, HaveTime, HaveTimeZone>& ctx,
              std::basic_string_view<CharT> _fmt) const
    {
        bool succ = true;
        auto res = do_get(rp, rp_end, ctx, succ, _fmt);
        if (!succ)
            throw stream_error("timeio parse error");
        return res;
    }

private:
    /**
     * @lang{ZH}
     * @brief 解析核心函数，按格式串逐字符处理输入，将结果写入 `ctx`。
     *
     * @note **递归与受信任 locale 假设**
     *   以下复合说明符会将 locale 提供的格式串展开后递归调用此函数：
     *   - `%c` → `m_[era_]date_time[_zone]_format`
     *   - `%x` → `m_[era_]date_format`
     *   - `%X` → `m_[era_]time[_zone]_format`
     *   - `%r` → `m_am_pm_format`
     *   - `%EY` → 纪元的 `format` 字符串
     *
     *   这些字符串来自 locale 数据库，视为受信任输入，假定不包含自引用
     *   （如 `D_T_FMT` 不含 `%c`，纪元格式不含 `%EY`）。现实中不存在违反此
     *   约定的 locale。**此处刻意不设置递归深度上限**：递归仅由受信任假设约束，
     *   而非由输入约束——若 locale 存在自引用格式串，任意非空输入均会导致栈溢出。
     *   手工构造或被篡改的 locale 是触发无限递归的唯一途径，其安全性与
     *   `timeio_details.h` 中 `parse_glibc_era_entries` 的输入边界共用相同的
     *   信任模型。若需加固，则需在所有递归调用点传递深度预算。
     *
     * @tparam TIter      双向迭代器或 `istreambuf_iterator` 类型。
     * @tparam TSent      哨兵类型。
     * @param rp     当前输入位置。
     * @param rp_end 输入范围结束哨兵。
     * @param ctx    累积解析结果的上下文（in/out）。
     * @param succ   输出参数：解析成功时为 `true`，失败时置为 `false`。
     * @param _fmt   格式串。
     * @return 指向未被消费的第一个字符的迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Core parsing function that processes the input character by character
     *        according to the format string, writing results to `ctx`.
     *
     * @note **Recursion and trusted-locale assumption**
     *   The following compound specifiers expand a locale-provided format string
     *   and re-enter this function recursively:
     *   - `%c` → `m_[era_]date_time[_zone]_format`
     *   - `%x` → `m_[era_]date_format`
     *   - `%X` → `m_[era_]time[_zone]_format`
     *   - `%r` → `m_am_pm_format`
     *   - `%EY` → era `format` string
     *
     *   These strings come verbatim from the locale database and are TRUSTED to
     *   be non-self-referential (e.g. `D_T_FMT` must not contain `%c`, an era
     *   format must not contain `%EY`). No real locale violates this.
     *   There is deliberately **NO recursion-depth guard**: the recursion is bounded
     *   ONLY by that trust, not by the input — a self-referential format string
     *   would recurse until the stack overflows on any non-empty input. A hand-crafted
     *   or corrupted locale is the only way to trigger unbounded recursion, and this
     *   is consciously left to the same trust boundary as `parse_glibc_era_entries`
     *   in `timeio_details.h`. Hardening it would require threading a depth budget
     *   through every recursive call site.
     *
     * @tparam TIter      Bidirectional iterator or `istreambuf_iterator` type.
     * @tparam TSent      Sentinel type.
     * @param rp     Current input position.
     * @param rp_end End sentinel of the input range.
     * @param ctx    Parse context accumulating results (in/out).
     * @param succ   Output flag: set to `false` on parse failure.
     * @param _fmt   The format string.
     * @return Iterator pointing to the first unconsumed character.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent, bool HaveDate, bool HaveTime, bool HaveTimeZone>
        requires (std::bidirectional_iterator<TIter> || is_istreambuf_iterator<TIter>)
    TIter do_get(TIter rp, TSent rp_end, time_parse_context<char_type, HaveDate, HaveTime, HaveTimeZone>& ctx,
                 bool& succ, std::basic_string_view<CharT> _fmt) const
    {
        if constexpr (HaveDate)
        {
            if (ctx.is_init == false)
            {
                std::copy(m_era_master.begin(), m_era_master.end(), std::back_inserter(ctx.m_era_items));
                ctx.is_init = true;
            }
        }

        auto fmt = _fmt.cbegin();
        while ((fmt != _fmt.cend()) && (rp != rp_end))
        {
            if (*fmt != static_cast<CharT>('%'))
            {
                if (is_space(*fmt))
                {
                    ++fmt;
                    while (rp != rp_end && is_space(*rp))
                        ++rp;
                    continue;
                }
                if (*fmt != *rp)
                {
                    succ = false;
                    return rp;
                }
                ++fmt; ++rp;
                continue;
            }

            if (++fmt == _fmt.cend())
            {
                succ = false;
                return rp;
            }

            CharT modifier = 0;
            if (*fmt == static_cast<CharT>('E') || *fmt == static_cast<CharT>('O'))
            {
                modifier = *fmt;
                if (++fmt == _fmt.cend())
                {
                    succ = false;
                    return rp;
                }
            }

            switch(*fmt)
            {
            case static_cast<CharT>('%'):
                if (modifier) goto bad_parse_format;
                if ('%' != *rp)
                {
                    succ = false;
                    return rp;
                }
                ++rp;
                break;

            case static_cast<CharT>('a'):
            case static_cast<CharT>('A'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    typename decltype(m_day_tree)::match_out_type tmp;
                    rp = m_day_tree.max_match(rp, rp_end, tmp);
                    if (tmp)
                    {
                        ctx.m_wday = *tmp;
                        ctx.m_have_wday = true;
                    }
                    else
                    {
                        succ = false;
                        return rp;
                    }
                }
                break;

            case static_cast<CharT>('b'):
            case static_cast<CharT>('B'):
            case static_cast<CharT>('h'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    typename decltype(m_month_tree)::match_out_type tmp;
                    rp = m_month_tree.max_match(rp, rp_end, tmp);
                    if (tmp && (*tmp >= 0) && (*tmp < 12))
                    {
                        ctx.m_month = (uint8_t)*tmp + 1;
                        ctx.m_have_mon = true;
                    }
                    else
                    {
                        succ = false;
                        return rp;
                    }
                }
                break;

            case static_cast<CharT>('c'):
                if constexpr (!HaveDate || !HaveTime) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('O')) goto bad_parse_format;
                    if constexpr (HaveTimeZone)
                    {
                        if (modifier == static_cast<CharT>('E'))
                            rp = do_get(rp, rp_end, ctx, succ, m_era_date_time_zone_format);
                        else
                            rp = do_get(rp, rp_end, ctx, succ, m_date_time_zone_format);
                        if (!succ) return rp;
                    }
                    else
                    {
                        if (modifier == static_cast<CharT>('E'))
                            rp = do_get(rp, rp_end, ctx, succ, m_era_date_time_format);
                        else
                            rp = do_get(rp, rp_end, ctx, succ, m_date_time_format);
                        if (!succ) return rp;
                    }
                }
                break;

            case static_cast<CharT>('C'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('O')) goto bad_parse_format;
                    if ((modifier == static_cast<CharT>('\0')) || (ctx.m_era_items.empty()))
                    {
                        // 0..99 only, no sign (see the %Y parse case for why this
                        // format/parse asymmetry is intentional and standard-aligned).
                        int mem = 0;
                        rp = extract_num(rp, rp_end, mem, 0, 99, 2, succ);
                        if (!succ) return rp;
                        ctx.m_century = mem;
                        ctx.m_have_century = true;
                    }
                    else
                    {
                        typename decltype(m_era_tree)::match_out_type tmp;
                        rp = m_era_tree.max_match(rp, rp_end, tmp);
                        if (!tmp)
                            ctx.m_era_items.clear();
                        else
                        {
                            for (auto it = ctx.m_era_items.begin(); it != ctx.m_era_items.end();)
                            {
                                if (it->name == *tmp) ++it;
                                else it = ctx.m_era_items.erase(it);
                            }
                        }
                        if (ctx.m_era_items.empty())
                        {
                            succ = false;
                            return rp;
                        }
                    }
                }
                break;

            case static_cast<CharT>('d'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                    int mem = -1;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 1, 31, 2, succ);
                    else
                        rp = extract_num(rp, rp_end, mem, 1, 31, 2, succ);
                    if (!succ) return rp;
                    ctx.m_mday = mem;
                    ctx.m_have_mday = true;
                }
                break;

            case static_cast<CharT>('e'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                    int mem = -1;
                    if (rp != rp_end && *rp == static_cast<CharT>(' '))
                        ++rp;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 1, 31, 2, succ);
                    else
                        rp = extract_num(rp, rp_end, mem, 1, 31, 2, succ);
                    if (!succ) return rp;
                    ctx.m_mday = mem;
                    ctx.m_have_mday = true;
                }
                break;

            case static_cast<CharT>('D'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    CharT subfmt[] = {static_cast<CharT>('%'), static_cast<CharT>('m'), static_cast<CharT>('/'),
                                      static_cast<CharT>('%'), static_cast<CharT>('d'), static_cast<CharT>('/'),
                                      static_cast<CharT>('%'), static_cast<CharT>('y'), CharT()};
                    rp = do_get(rp, rp_end, ctx, succ, subfmt);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('F'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    CharT subfmt[] = {static_cast<CharT>('%'), static_cast<CharT>('Y'), static_cast<CharT>('-'),
                                      static_cast<CharT>('%'), static_cast<CharT>('m'), static_cast<CharT>('-'),
                                      static_cast<CharT>('%'), static_cast<CharT>('d'), CharT()};
                    rp = do_get(rp, rp_end, ctx, succ, subfmt);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('g'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    int val = 0;
                    rp = extract_num(rp, rp_end, val, 0, 99, 2, succ);
                    if (!succ) return rp;
                    ctx.m_iso_8601_year = val >= 69 ? val + 1900 : val + 2000;
                    ctx.m_have_iso_8601_year = true;
                }
                break;
            case static_cast<CharT>('G'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    // 0..9999 only, no sign (see the %Y parse case for why this
                    // format/parse asymmetry is intentional and standard-aligned).
                    int val = 0;
                    rp = extract_num(rp, rp_end, val, 0, 9999, 4, succ);
                    if (!succ) return rp;
                    ctx.m_iso_8601_year = val;
                    ctx.m_have_iso_8601_year = true;
                }
                break;

            case static_cast<CharT>('H'):
                if constexpr (!HaveTime) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                    int mem = -1;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 0, 23, 2, succ);
                    else
                        rp= extract_num(rp, rp_end, mem, 0, 23, 2, succ);
                    if (!succ) return rp;
                    ctx.m_hour = mem;
                    ctx.m_have_I = false;
                }
                break;

            case static_cast<CharT>('I'):
                if constexpr (!HaveTime) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                    int mem = -1;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 1, 12, 2, succ);
                    else
                        rp = extract_num(rp, rp_end, mem, 1, 12, 2, succ);
                    if (!succ) return rp;
                    ctx.m_hour = mem % 12;
                    ctx.m_have_I = true;
                }
                break;

            case static_cast<CharT>('j'):
                /* Match day number of year.  */
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    int mem = 0;
                    rp = extract_num(rp, rp_end, mem, 1, 366, 3, succ);
                    if (!succ) return rp;
                    ctx.m_yday = mem - 1;
                    ctx.m_have_yday = true;
                }
                break;

            case static_cast<CharT>('m'):
                /* Match number of month.  */
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                    int mem = -1;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 1, 12, 2, succ);
                    else
                        rp = extract_num(rp, rp_end, mem, 1, 12, 2, succ);
                    if (!succ) return rp;
                    ctx.m_month = mem;
                    ctx.m_have_mon = true;
                }
                break;
            case static_cast<CharT>('M'):
                /* Match minute.  */
                if constexpr (!HaveTime) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                    int mem = -1;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 0, 59, 2, succ);
                    else
                        rp = extract_num(rp, rp_end, mem, 0, 59, 2, succ);
                    if (!succ) return rp;
                    ctx.m_minute = mem;
                }
                break;

            case static_cast<CharT>('n'):
            case static_cast<CharT>('t'):
                if (modifier) goto bad_parse_format;
                while ((rp != rp_end) && is_space(*rp))
                    ++rp;
                break;

            case static_cast<CharT>('p'):
                if constexpr (!HaveTime) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    typename decltype(m_am_pm_tree)::match_out_type tmp;
                    rp = m_am_pm_tree.max_match(rp, rp_end, tmp);
                    if (tmp)
                    {
                        if (*tmp == 0) ctx.m_is_pm = false;
                        else if (*tmp == 1) ctx.m_is_pm = true;
                        else
                        {
                            succ = false;
                            return rp;
                        }
                    }
                    else
                    {
                        succ = false;
                        return rp;
                    }
                }
                break;

            case static_cast<CharT>('r'):
                if constexpr (!HaveTime) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    rp = do_get(rp, rp_end, ctx, succ, m_am_pm_format);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('R'):
                if constexpr (!HaveTime) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    CharT subfmt[] = {static_cast<CharT>('%'), static_cast<CharT>('H'), static_cast<CharT>(':'),
                                      static_cast<CharT>('%'), static_cast<CharT>('M'), CharT()};
                    rp = do_get(rp, rp_end, ctx, succ, subfmt);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('S'):
                if constexpr (!HaveTime) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                    // Upper bound is 59, not C's 60: leap seconds are not supported
                    // (hh_mm_ss cannot represent them); see put() for rationale.
                    int mem = -1;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 0, 59, 2, succ);
                    else
                        rp = extract_num(rp, rp_end, mem, 0, 59, 2, succ);
                    if (!succ) return rp;
                    ctx.m_second = mem;
                }
                break;

            case static_cast<CharT>('T'):
                if constexpr (!HaveTime) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    CharT subfmt[] = {static_cast<CharT>('%'), static_cast<CharT>('H'), static_cast<CharT>(':'),
                                      static_cast<CharT>('%'), static_cast<CharT>('M'), static_cast<CharT>(':'),
                                      static_cast<CharT>('%'), static_cast<CharT>('S'), CharT()};
                    rp = do_get(rp, rp_end, ctx, succ, subfmt);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('u'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                    int mem = -1;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 1, 7, 1, succ);
                    else
                        rp = extract_num(rp, rp_end, mem, 1, 7, 1, succ);
                    if (!succ) return rp;
                    ctx.m_wday = mem % 7;
                    ctx.m_have_wday = true;
                }
                break;

            case static_cast<CharT>('U'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                    int mem = -1;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 0, 53, 2, succ);
                    else
                        rp = extract_num(rp, rp_end, mem, 0, 53, 2, succ);
                    if (!succ) return rp;
                    ctx.m_week_no = mem;
                    ctx.m_have_uweek = true;
                    ctx.m_have_wweek = false;
                }
                break;

            case static_cast<CharT>('V'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                    int mem = -1;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 1, 53, 2, succ);
                    else
                        rp = extract_num(rp, rp_end, mem, 1, 53, 2, succ);
                    if (!succ) return rp;
                    ctx.m_iso_8601_week = mem;
                    ctx.m_have_iso_8601_week = true;
                }
                break;

            case static_cast<CharT>('w'):
                /* Match number of weekday.  */
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                    int mem = -1;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 0, 6, 1, succ);
                    else
                        rp = extract_num(rp, rp_end, mem, 0, 6, 1, succ);
                    if (!succ) return rp;
                    ctx.m_wday = mem;
                    ctx.m_have_wday = 1;
                }
                break;

            case static_cast<CharT>('W'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                    int mem = -1;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 0, 53, 2, succ);
                    else
                        rp = extract_num(rp, rp_end, mem, 0, 53, 2, succ);
                    if (!succ) return rp;
                    ctx.m_week_no = mem;
                    ctx.m_have_wweek = true;
                    ctx.m_have_uweek = false;
                }
                break;

            case static_cast<CharT>('x'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('O')) goto bad_parse_format;
                    if (modifier == static_cast<CharT>('E'))
                        rp = do_get(rp, rp_end, ctx, succ, m_era_date_format);
                    else
                        rp = do_get(rp, rp_end, ctx, succ, m_date_format);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('X'):
                if constexpr (!HaveTime) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('O')) goto bad_parse_format;
                    if constexpr (HaveTimeZone)
                    {
                        if (modifier == static_cast<CharT>('E'))
                            rp = do_get(rp, rp_end, ctx, succ, m_era_time_zone_format);
                        else
                            rp = do_get(rp, rp_end, ctx, succ, m_time_zone_format);
                        if (!succ) return rp;
                    }
                    else
                    {
                        if (modifier == static_cast<CharT>('E'))
                            rp = do_get(rp, rp_end, ctx, succ, m_era_time_format);
                        else
                            rp = do_get(rp, rp_end, ctx, succ, m_time_format);
                        if (!succ) return rp;
                    }
                }
                break;

            case static_cast<CharT>('y'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else if ((modifier == static_cast<CharT>('E')) && (!ctx.m_era_items.empty()))
                {
                    int val = 0;
                    rp = extract_num(rp, rp_end, val, 0, 9999, 4, succ);
                    if (!succ) return rp;
                    ctx.m_year_of_era = val;
                    ctx.m_have_year_of_era = 1;

                    // Mirrors the glibc strptime_l.c validation formula:
                    //   delta = (era_year − offset) × absolute_direction
                    //   valid  iff  0 ≤ delta ≤ (to_year − from_year) × direction
                    // direction here is glibc's absolute_direction (see timeio_details.h
                    // OUTPUT INVARIANTS for how it is normalised).  For all real-world
                    // locales every era has from < to and direction = +1, so range is
                    // always positive and the check is straightforward.  The inclusive
                    // upper bound (≤ vs glibc's strict <) is intentional: it retains
                    // eras whose epoch year coincides with the last calendar year of the
                    // era, which matters for locales where two eras share a boundary year
                    // (e.g. Showa/Heisei 1989); the full-date check in get_era_entry()
                    // then selects the correct one.
                    for (auto it = ctx.m_era_items.begin(); it != ctx.m_era_items.end();)
                    {
                        const auto& cur_era = *it;
                        int64_t delta = (static_cast<int64_t>(ctx.m_year_of_era) - cur_era.offset) * cur_era.direction;
                        int64_t range = (static_cast<int64_t>(cur_era.to_year) - cur_era.from_year) * cur_era.direction;
                        bool match = (delta >= 0 && delta <= range);
                        if (match) ++it;
                        else it = ctx.m_era_items.erase(it);
                    }

                    if (ctx.m_era_items.empty())
                    {
                        succ = false;
                        return rp;
                    }
                }
                else
                {/* Match year within century.  */
                    int mem = -1;
                    if (modifier == static_cast<CharT>('O'))
                        rp = extract_num_with_alt_digits(rp, rp_end, mem, 0, 99, 2, succ);
                    else
                        rp = extract_num(rp, rp_end, mem, 0, 99, 2, succ);
                    if (!succ) return rp;
                    /* The "Year 2000: The Millennium Rollover" paper suggests that
                    values in the range 69-99 refer to the twentieth century.  */
                    ctx.m_year = mem >= 69 ? mem + 1900 : mem + 2000;
                    ctx.m_have_year_in_century = 1;
                }
                break;

            case static_cast<CharT>('Y'):
                /* Match year including century number.
                 *
                 * Intentional format/parse asymmetry (NOT a bug): put() can
                 * emit a leading '-' for negative years and more than four
                 * digits for years > 9999, to stay consistent with
                 * std::format. This parser, however, deliberately accepts only
                 * 0..9999 with no sign -- which is exactly what
                 * std::chrono::from_stream("%Y") and POSIX strptime() do; both
                 * likewise refuse to read back "-0044" or "12345". Keeping the
                 * same restriction makes get() match the standard parse
                 * facilities; widening it (sign / 5+ digits) would diverge from
                 * them and make separator-less formats such as "%Y%m%d"
                 * ambiguous due to greedy digit capture. The same reasoning
                 * applies to the %G and %C parse cases.  */
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('O')) goto bad_parse_format;
                    if ((modifier == static_cast<CharT>('E')) && (!ctx.m_era_items.empty()))
                    {
                        if constexpr (is_stamp_input_iterator_v<TIter>)
                        {
                            succ = false;
                            return rp;
                        }
                        else
                        {
                            stamp_input_iterator rp_wrapper(rp);
                            decltype(rp_wrapper) rp_end_wrapper(rp_end);

                            auto format_it = m_era_formats.begin();
                            for (; format_it != m_era_formats.end(); ++format_it)
                            {
                                auto tmp_ctx = ctx;
                                bool tmp_succ = true;
                                rp_wrapper = do_get(rp_wrapper, rp_end_wrapper, tmp_ctx, tmp_succ, *format_it);
                                if (!tmp_succ)
                                    rp_wrapper.rollback();
                                else
                                {
                                    rp = rp_wrapper.internal();
                                    ctx = tmp_ctx;
                                    break;
                                }
                            }

                            if (format_it == m_era_formats.end())
                            {
                                rp = rp_wrapper.internal();
                                int val = 0;
                                rp = extract_num(rp, rp_end, val, 0, 9999, 4, succ);
                                if (!succ) return rp;
                                ctx.m_year = val;
                                ctx.m_have_year = 1;
                            }
                        }
                    }
                    else
                    {
                        int val = 0;
                        rp = extract_num(rp, rp_end, val, 0, 9999, 4, succ);
                        if (!succ) return rp;
                        ctx.m_year = val;
                        ctx.m_have_year = 1;
                    }
                }
                break;

            case static_cast<CharT>('z'):
                if constexpr (!HaveDate || !HaveTime || !HaveTimeZone) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    /* We recognize four formats:
                        1. Two digits specify hours.
                        2. Four digits specify hours and minutes.
                        3. Two digits, ':', and two digits specify hours and minutes.
                        4. 'Z' is equivalent to +0000.
                    */
                    // In C++, there is no way to store timezone offset in a standard structure.
                    // so we just omit the parse result
                    int val = 0;
                    if (*rp == static_cast<CharT>('Z'))
                    {
                        ++rp; break;
                    }

                    if ((*rp != static_cast<CharT>('+')) && (*rp != static_cast<CharT>('-')))
                    {
                        succ = false; return rp;
                    }
                    ++rp;

                    if (rp == rp_end)
                    {
                        succ = false; return rp;
                    }

                    int n = 0;
                    while (n < 4 && *rp >= static_cast<CharT>('0') && *rp <= static_cast<CharT>('9'))
                    {
                        val = val * 10 + *rp++ - static_cast<CharT>('0');
                        ++n;
                        if (rp == rp_end) break;
                        if (*rp == static_cast<CharT>(':') && n == 2) ++rp;
                        if (rp == rp_end) break;
                    }

                    if (n == 2) val *= 100;
                    else if (n != 4)
                    {
                        /* Only two or four digits recognized.  */
                        succ = false; return rp;
                    }

                    if (val / 100 >= 24 || val % 100 >= 60)
                    {
                        succ = false; return rp;
                    }
                }
                break;

            case static_cast<CharT>('Z'):
                if constexpr (!HaveTimeZone) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    typename decltype(ft_basic<timeio<CharT>>::s_timezone_tree)::match_out_type zone_res;
                    rp = ft_basic<timeio<CharT>>::s_timezone_tree.max_match(rp, rp_end, zone_res);
                    if (!zone_res)
                    {
                        succ = false;
                        return rp;
                    }
                    else if ((*zone_res)[0] != '*')
                        ctx.m_zone_name = *zone_res;
                    else
                        ctx.m_zone_abbrev = zone_res->substr(1);
                }
                break;

            default:
            bad_parse_format:
                {
                    if ((rp == rp_end) || (*rp != static_cast<CharT>('%')))
                    {
                        succ = false;
                        return rp;
                    }

                    ++rp;
                    if (modifier)
                    {
                        if ((rp == rp_end) || (*rp != modifier))
                        {
                            succ = false;
                            return rp;
                        }
                        ++rp;
                    }

                    if ((rp == rp_end) || (*rp != *fmt))
                    {
                        succ = false;
                        return rp;
                    }
                    ++rp;
                }
            }
            ++fmt;
        }

        // A format tail consisting solely of whitespace / %n / %t matches
        // zero-or-more whitespace and is satisfied even at end of input,
        // matching POSIX strptime and std::get_time. Skip such a tail before
        // judging failure.
        while (fmt != _fmt.cend())
        {
            if (is_space(*fmt)) { ++fmt; continue; }
            auto next = fmt + 1;
            if ((*fmt != static_cast<CharT>('%')) || (next == _fmt.cend()))
                break;
            if ((*next != static_cast<CharT>('n')) && (*next != static_cast<CharT>('t')))
                break;
            fmt += 2;
        }

        if ((fmt != _fmt.cend()) && (rp == rp_end))
            succ = false;
        return rp;
    }

    /**
     * @lang{ZH}
     * @brief 判断字符是否为空白字符。
     *
     * 若已设置 `ctype` facet，则使用其 `space` 分类；否则回退为基础 ASCII 空白判断。
     * @param c 要检查的字符。
     * @return 若 `c` 为空白字符则返回 `true`。
     * @endif
     *
     * @lang{EN}
     * @brief Checks whether a character is a whitespace character.
     *
     * Uses the `ctype` facet's `space` classification if one is set; otherwise
     * falls back to basic ASCII whitespace comparison.
     * @param c The character to check.
     * @return `true` if `c` is a whitespace character.
     * @endif
     */
    bool is_space(CharT c) const
    {
        if (m_ctype)
            return m_ctype->is_any(base_ft<ctype>::space, c);
        return c == static_cast<CharT>(' ')  || c == static_cast<CharT>('\t') ||
               c == static_cast<CharT>('\n') || c == static_cast<CharT>('\v') ||
               c == static_cast<CharT>('\f') || c == static_cast<CharT>('\r');
    }

    /**
     * @lang{ZH}
     * @brief 验证名称数组中所有条目均非空，且不同索引的名称互不重复。
     *
     * 允许 `full[i] == abbr[i]`（同一索引的全称与缩写相同，如 en_US 的 "May"）。
     * @tparam N 名称数组大小（7 或 12）。
     * @param full 全称数组。
     * @param abbr 缩写数组。
     * @param what 名称类别描述（用于错误消息，如 `"day"` 或 `"month"`）。
     * @throw std::runtime_error 若存在空名称或不同索引的名称重复。
     * @endif
     *
     * @lang{EN}
     * @brief Verifies that all entries in the name arrays are non-empty and that
     *        no spelling is shared by two different indices.
     *
     * `full[i] == abbr[i]` (same index, e.g. "May" in en_US) is allowed.
     * @tparam N Size of the name arrays (7 or 12).
     * @param full The full-name array.
     * @param abbr The abbreviated-name array.
     * @param what A category label for error messages (e.g. `"day"` or `"month"`).
     * @throw std::runtime_error If any name is empty or two different indices share a spelling.
     * @endif
     */
    template <size_t N>
    static void check_unique_nonempty(const std::array<std::basic_string<CharT>, N>& full,
                                      const std::array<std::basic_string<CharT>, N>& abbr,
                                      const char* what)
    {
        std::array<const std::basic_string<CharT>*, 2 * N> names{};
        for (size_t i = 0; i < N; ++i)
        {
            if (full[i].empty() || abbr[i].empty())
                throw std::runtime_error(std::string("timeio: empty ") + what + " name in locale data");
            names[2 * i]     = &full[i];
            names[2 * i + 1] = &abbr[i];
        }
        // names[p] belongs to index p / 2.
        for (size_t a = 0; a < names.size(); ++a)
            for (size_t b = a + 1; b < names.size(); ++b)
                if ((a / 2) != (b / 2) && *names[a] == *names[b])
                    throw std::runtime_error(std::string("timeio: duplicate ") + what + " name in locale data");
    }

    /**
     * @lang{ZH}
     * @brief 验证 locale 名称表的完整性。
     *
     * 检查星期和月份名称的非空性与唯一性；允许 AM/PM 为空（如 de_DE、fr_FR），
     * 但若两者均非空且相同，则抛出异常。
     * @throw std::runtime_error 若名称表存在无效条目。
     * @endif
     *
     * @lang{EN}
     * @brief Validates the integrity of the locale name tables.
     *
     * Checks weekday and month names for non-emptiness and uniqueness. AM/PM may
     * legitimately be empty (e.g. de_DE, fr_FR); only a non-empty AM == PM
     * collision is considered invalid.
     * @throw std::runtime_error If the name tables contain invalid entries.
     * @endif
     */
    void validate_locale_names() const
    {
        check_unique_nonempty<7>(m_day, m_abbr_day, "day");
        check_unique_nonempty<12>(m_month, m_abbr_month, "month");

        // AM/PM is empty in locales that do not use 12-hour designators
        // (de_DE / fr_FR / ru_RU); that is tolerated and simply leaves %p
        // unmatched. Only a non-empty AM == PM collision is genuinely invalid.
        if (!m_am.empty() && m_am == m_pm)
            throw std::runtime_error("timeio: AM and PM designators are identical in locale data");
    }

    /**
     * @lang{ZH}
     * @brief 从 `m_era_master` 构建纪元名称前缀树和纪元格式集合。
     * @endif
     *
     * @lang{EN}
     * @brief Builds the era name prefix trie and era format string set from `m_era_master`.
     * @endif
     */
    void create_era_name_tree()
    {
        for (size_t i = 0; i < m_era_master.size(); ++i)
        {
            const std::basic_string<CharT>& name = m_era_master[i].name;
            if (!name.empty())
                m_era_tree.add(name, name);
            if (!m_era_master[i].format.empty())
                m_era_formats.insert(m_era_master[i].format);
        }
    }

    /**
     * @lang{ZH}
     * @brief 从 `m_alt_digits` 构建替代数字前缀树，用于 `%Od` 等说明符的解析。
     * @endif
     *
     * @lang{EN}
     * @brief Builds the alternative digits prefix trie from `m_alt_digits`,
     *        used for parsing specifiers such as `%Od`.
     * @endif
     */
    void create_alt_digits_tree()
    {
        for (size_t i = 0; i < 100; ++i)
        {
            if (!m_alt_digits[i].empty())
                m_alt_digits_tree.add(m_alt_digits[i], i);
        }
    }

    /**
     * @lang{ZH}
     * @brief 格式化核心函数，按格式串将日期/时间各分量写入输出迭代器。
     *
     * @note 复合说明符 `%c`、`%x`、`%X`、`%r`、`%EY` 会将 locale 提供的格式串
     *   展开后递归调用此函数，与 `do_get` 共用相同的受信任 locale 假设：
     *   自引用格式串（如 `D_T_FMT == "%c"`）将导致无限递归。
     *
     * @tparam OutIt 输出迭代器类型。
     * @param out    输出迭代器。
     * @param format 格式串（`strftime` 风格）。
     * @param ymd    日期指针（若不含日期分量则为 `nullptr`）。
     * @param wd     星期指针（若不含星期分量则为 `nullptr`）。
     * @param hms    时间指针（若不含时间分量则为 `nullptr`）。
     * @param tz     时区指针（若不含时区分量则为 `nullptr`）。
     * @return 写入后的输出迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Core formatting function that writes date/time components to an output
     *        iterator according to the format string.
     *
     * @note The compound specifiers `%c`, `%x`, `%X`, `%r`, `%EY` expand a
     *   locale-provided format string and re-enter this function recursively,
     *   sharing the same trusted-locale assumption as `do_get`: a self-referential
     *   format string (e.g. `D_T_FMT == "%c"`) would recurse without bound.
     *
     * @tparam OutIt Output iterator type.
     * @param out    The output iterator.
     * @param format The format string (`strftime`-style).
     * @param ymd    Date pointer (`nullptr` if no date components are needed).
     * @param wd     Weekday pointer (`nullptr` if no weekday component is needed).
     * @param hms    Time pointer (`nullptr` if no time components are needed).
     * @param tz     Timezone pointer (`nullptr` if no timezone component is needed).
     * @return The output iterator after writing.
     * @endif
     */
    template <typename OutIt>
    OutIt do_put(OutIt out, std::basic_string_view<CharT> format,
                 const std::chrono::year_month_day* ymd,
                 std::chrono::weekday* wd,
                 const std::chrono::hh_mm_ss<std::chrono::seconds>* hms,
                 const std::chrono::time_zone* tz) const
    {
        auto f = format.cbegin();
        while (f != format.cend())
        {
            if (*f != static_cast<CharT>('%'))
            {
                *out++ = *f++;
                continue;
            }

            if (++f == format.cend()) break;

            CharT modifier = 0;
            if (*f == static_cast<CharT>('E') || *f == static_cast<CharT>('O'))
            {
                modifier = *f++;
                if (f == format.cend()) break;
            }

            CharT format_char = *f;
            switch (format_char)
            {
            case static_cast<CharT>('%'):
                if (modifier) goto bad_format;
                *out++ = *f;
                break;

            case static_cast<CharT>('a'):
                if (!wd || modifier) goto bad_format;
                {
                    const auto index = wd->c_encoding();
                    if (index > 6) *out++ = static_cast<CharT>('?');
                    else
                    {
                        const auto& abbr_wkday = m_abbr_day[index];
                        out = std::copy(abbr_wkday.begin(), abbr_wkday.end(), out);
                    }
                }
                break;

            case static_cast<CharT>('A'):
                if (!wd || modifier) goto bad_format;
                {
                    const auto index = wd->c_encoding();
                    if (index > 6) *out++ = static_cast<CharT>('?');
                    else
                    {
                        const auto& wkday = m_day[index];
                        out = std::copy(wkday.begin(), wkday.end(), out);
                    }
                }
                break;

            case static_cast<CharT>('b'):
            case static_cast<CharT>('h'):
                if (!ymd || modifier) goto bad_format;
                else
                {
                    unsigned m = static_cast<unsigned>(ymd->month()) - 1;
                    if (m > 11) *out++ = static_cast<CharT>('?');
                    else
                    {
                        const auto& mon = m_abbr_month[m];
                        out = std::copy(mon.begin(), mon.end(), out);
                    }
                }
                break;

            case static_cast<CharT>('B'):
                if (!ymd || modifier) goto bad_format;
                else
                {
                    unsigned m = static_cast<unsigned>(ymd->month()) - 1;
                    if (m > 11) *out++ = static_cast<CharT>('?');
                    else
                    {
                        const auto& mon = m_month[m];
                        out = std::copy(mon.begin(), mon.end(), out);
                    }
                }
                break;

            case static_cast<CharT>('c'):
                if (!ymd || !hms || modifier == static_cast<CharT>('O')) goto bad_format;
                {
                    const std::basic_string<CharT>* ptr = nullptr;
                    if (tz)
                    {
                        ptr = &m_date_time_zone_format;
                        if (modifier == static_cast<CharT>('E')) ptr = &m_era_date_time_zone_format;
                    }
                    else
                    {
                        ptr = &m_date_time_format;
                        if (modifier == static_cast<CharT>('E')) ptr = &m_era_date_time_format;
                    }
                    out = do_put(out, *ptr, ymd, wd, hms, tz);
                }
                break;

            case static_cast<CharT>('C'):
                if (!ymd || modifier == static_cast<CharT>('O')) goto bad_format;
                {
                    const era_entry* era = nullptr;
                    if (modifier == static_cast<CharT>('E'))
                        era = get_era_entry(*ymd);
                    if (era) out = std::copy(era->name.begin(), era->name.end(), out);
                    else
                    {
                        int year = static_cast<int>(ymd->year());
                        int century = year / 100 - (year % 100 < 0);
                        if (century < 0) {
                            *out++ = static_cast<CharT>('-');
                            century = -century;
                        }
                        // Min width 2, but never truncate (matches std::format).
                        if (century > 99) out = put_dec<0>(out, century);
                        else              out = put_dec<2>(out, century);
                    }
                }
                break;

            case static_cast<CharT>('d'):
                if (!ymd || modifier == static_cast<CharT>('E')) goto bad_format;
                else
                {
                    unsigned val = static_cast<unsigned>(ymd->day());
                    if (val < 1) val = 1;
                    if (val > 31) val = 31;
                    out = put_dec<2>(out, static_cast<int>(val), (modifier == static_cast<CharT>('O')));
                }
                break;

            case static_cast<CharT>('D'):
                if (!ymd || modifier) goto bad_format;
                {
                    CharT subfmt[] = {static_cast<CharT>('%'), static_cast<CharT>('m'), static_cast<CharT>('/'),
                                      static_cast<CharT>('%'), static_cast<CharT>('d'), static_cast<CharT>('/'),
                                      static_cast<CharT>('%'), static_cast<CharT>('y'), CharT()};
                    out = do_put(out, subfmt, ymd, wd, hms, tz);
                }
                break;

            case static_cast<CharT>('e'):
                if (!ymd || modifier == static_cast<CharT>('E')) goto bad_format;
                else
                {
                    unsigned val = static_cast<unsigned>(ymd->day());
                    if (val < 1) val = 1;
                    if (val > 31) val = 31;
                    out = put_dec<2, static_cast<CharT>(' ')>(out, static_cast<int>(val), (modifier == static_cast<CharT>('O')));
                }
                break;

            case static_cast<CharT>('F'):
                if (!ymd || modifier) goto bad_format;
                {
                    CharT subfmt[] = {static_cast<CharT>('%'), static_cast<CharT>('Y'), static_cast<CharT>('-'),
                                      static_cast<CharT>('%'), static_cast<CharT>('m'), static_cast<CharT>('-'),
                                      static_cast<CharT>('%'), static_cast<CharT>('d'), CharT()};
                    out = do_put(out, subfmt, ymd, wd, hms, tz);
                }
                break;

            case static_cast<CharT>('g'):
            case static_cast<CharT>('G'):
                if (!ymd || !wd || modifier) goto bad_format;
                {
                    std::chrono::sys_days sd{*ymd};
                    std::chrono::sys_days thursday = sd + std::chrono::days{4 - static_cast<int>(wd->iso_encoding())};
                    std::chrono::year_month_day thursday_ymd{thursday};
                    int val = int(thursday_ymd.year());
                    if (format_char == static_cast<CharT>('G'))
                    {
                        int yr = val;
                        if (yr < 0)
                        {
                            *out++ = static_cast<CharT>('-');
                            yr = -yr;
                        }
                        // Min width 4, but never truncate (matches std::format).
                        if (yr > 9999) out = put_dec<0>(out, yr);
                        else           out = put_dec<4>(out, yr);
                    }
                    if (format_char == static_cast<CharT>('g'))
                        out = put_dec<2>(out, (val % 100 + 100) % 100);
                }
                break;

            case static_cast<CharT>('H'):
                if (!hms || modifier == static_cast<CharT>('E')) goto bad_format;
                else
                {
                    auto val = hms->hours().count();
                    if (val < 0) val = 0;
                    if (val > 23) val = 23;
                    out = put_dec<2>(out, static_cast<int>(val), (modifier == static_cast<CharT>('O')));
                }
                break;

            case static_cast<CharT>('I'):
                if (!hms || modifier == static_cast<CharT>('E')) goto bad_format;
                {
                    auto val = hms->hours().count();
                    if (val < 0) val = 0;
                    if (val > 23) val = 23;

                    if (val > 12) val -= 12;
                    else if (val == 0) val = 12;
                    out = put_dec<2>(out, static_cast<int>(val), (modifier == static_cast<CharT>('O')));
                }
                break;

            case static_cast<CharT>('j'):
                if (!ymd || modifier) goto bad_format;
                {
                    std::chrono::year_month_day first_day{ymd->year(), std::chrono::January, std::chrono::day{1}};
                    auto doy = (std::chrono::sys_days(*ymd) - std::chrono::sys_days(first_day)).count() + 1;
                    out = put_dec<3>(out, doy);
                }
                break;

            case static_cast<CharT>('M'):
                if (!hms || modifier == static_cast<CharT>('E')) goto bad_format;
                {
                    auto val = hms->minutes().count();
                    if (val < 0) val = 0;
                    if (val > 59) val = 59;
                    out = put_dec<2>(out, static_cast<int>(val), (modifier == static_cast<CharT>('O')));
                }
                break;

            case static_cast<CharT>('m'):
                if (!ymd || modifier == static_cast<CharT>('E')) goto bad_format;
                {
                    auto val = static_cast<unsigned>(ymd->month());
                    if (val < 1) val = 1;
                    if (val > 12) val = 12;
                    out = put_dec<2>(out, static_cast<int>(val), (modifier == static_cast<CharT>('O')));
                }
                break;

            case static_cast<CharT>('n'):
                if (modifier) goto bad_format;
                *out++ = static_cast<CharT>('\n');
                break;

            case static_cast<CharT>('p'):
                if (!hms || modifier) goto bad_format;
                {
                    auto val = hms->hours().count();
                    const auto& obj = (val > 11) ? m_pm : m_am;
                    out = std::copy(obj.begin(), obj.end(), out);
                }
                break;

            case static_cast<CharT>('r'):
                if (!hms || modifier) goto bad_format;
                out = do_put(out, m_am_pm_format, ymd, wd, hms, tz);
                break;

            case static_cast<CharT>('R'):
                if (!hms || modifier) goto bad_format;
                {
                    CharT subfmt[] = {static_cast<CharT>('%'), static_cast<CharT>('H'), static_cast<CharT>(':'),
                                      static_cast<CharT>('%'), static_cast<CharT>('M'), CharT()};
                    out = do_put(out, subfmt, ymd, wd, hms, tz);
                }
                break;

            case static_cast<CharT>('S'):
                if (!hms || modifier == static_cast<CharT>('E')) goto bad_format;
                {
                    auto val = hms->seconds().count();
                    if (val < 0) val = 0;
                    if (val > 59) val = 59;
                    out = put_dec<2>(out, static_cast<int>(val), (modifier == static_cast<CharT>('O')));
                }
                break;

            case static_cast<CharT>('t'):
                if (modifier) goto bad_format;
                *out++ = static_cast<CharT>('\t');
                break;

            case static_cast<CharT>('T'):
                if (!hms || modifier) goto bad_format;
                {
                    CharT subfmt[] = {static_cast<CharT>('%'), static_cast<CharT>('H'), static_cast<CharT>(':'),
                                      static_cast<CharT>('%'), static_cast<CharT>('M'), static_cast<CharT>(':'),
                                      static_cast<CharT>('%'), static_cast<CharT>('S'), CharT()};
                    out = do_put(out, subfmt, ymd, wd, hms, tz);
                }
                break;

            case static_cast<CharT>('u'):
                if (!wd || modifier == static_cast<CharT>('E')) goto bad_format;
                {
                    const auto index = wd->iso_encoding();
                    if ((index < 1) || (index > 7)) *out++ = static_cast<CharT>('?');
                    else out = put_dec<1>(out, static_cast<int>(index), (modifier == static_cast<CharT>('O')));
                }
                break;

            case static_cast<CharT>('U'):
                if (!ymd || !wd || modifier == static_cast<CharT>('E')) goto bad_format;
                {
                    std::chrono::sys_days sd{*ymd};
                    std::chrono::sys_days jan1 = {ymd->year()/std::chrono::January/1};
                    int doy = static_cast<int>((sd - jan1).count());
                    int wday = static_cast<int>(wd->c_encoding());
                    int val = (doy - wday + 7) / 7;
                    if (val < 0) val = 0;
                    if (val > 53) val = 53;
                    out = put_dec<2>(out, val, (modifier == static_cast<CharT>('O')));
                }
                break;

            case static_cast<CharT>('V'):
                if (!ymd || !wd || modifier == static_cast<CharT>('E')) goto bad_format;
                {
                    std::chrono::sys_days sd{*ymd};
                    std::chrono::sys_days this_thursday = sd + std::chrono::days{4 - static_cast<int>(wd->iso_encoding())};
                    auto iso_year = std::chrono::year_month_day{this_thursday}.year();
                    std::chrono::sys_days jan4{ iso_year / std::chrono::January / 4 };
                    std::chrono::weekday wd_jan4{jan4};
                    std::chrono::sys_days first_thursday = jan4 + std::chrono::days{4 - static_cast<int>(wd_jan4.iso_encoding())};
                    int week = int((this_thursday - first_thursday) / std::chrono::days{7}) + 1;
                    if (week < 1) week = 1;
                    if (week > 53) week = 53;

                    out = put_dec<2>(out, week, (modifier == static_cast<CharT>('O')));
                }
                break;

            case static_cast<CharT>('w'):
                if (!wd || modifier == static_cast<CharT>('E')) goto bad_format;
                {
                    int val = static_cast<int>(wd->c_encoding());
                    if (val < 0) val = 0;
                    if (val > 6) val = 6;
                    out = put_dec<1>(out, val, (modifier == static_cast<CharT>('O')));
                }
                break;

            case static_cast<CharT>('W'):
                if (!ymd || !wd || modifier == static_cast<CharT>('E')) goto bad_format;
                {
                    std::chrono::sys_days sd{*ymd};
                    std::chrono::sys_days jan1{ymd->year()/std::chrono::January/1};
                    int doy = static_cast<int>((sd - jan1).count());
                    int wday_monday = static_cast<int>((wd->c_encoding() + 6) % 7);
                    int val = (doy - wday_monday + 7) / 7;
                    if (val < 0) val = 0;
                    if (val > 53) val = 53;
                    out = put_dec<2>(out, val, (modifier == static_cast<CharT>('O')));
                }
                break;

            case static_cast<CharT>('x'):
                if (!ymd || modifier == static_cast<CharT>('O')) goto bad_format;
                {
                    const std::basic_string<CharT>& subFmt = (modifier == static_cast<CharT>('E')) ?
                                                             m_era_date_format : m_date_format;
                    out = do_put(out, subFmt, ymd, wd, hms, tz);
                }
                break;

            case static_cast<CharT>('X'):
                if (!hms || modifier == static_cast<CharT>('O')) goto bad_format;
                {
                    const std::basic_string<CharT>* ptr = nullptr;
                    if (tz)
                        ptr = (modifier == static_cast<CharT>('E')) ? &m_era_time_zone_format : &m_time_zone_format;
                    else
                        ptr = (modifier == static_cast<CharT>('E')) ? &m_era_time_format : &m_time_format;
                    out = do_put(out, *ptr, ymd, wd, hms, tz);
                }
                break;

            case static_cast<CharT>('y'):
                if (!ymd) goto bad_format;
                {
                    int val = (static_cast<int>(ymd->year()) % 100 + 100) % 100;
                    if (val < 0) val = 0;
                    if (val > 99) val = 99;
                    if (modifier == static_cast<CharT>('O'))
                    {
                        const auto& str = m_alt_digits[val];
                        if (!str.empty())
                            out = std::copy(str.begin(), str.end(), out);
                        else out = put_dec<2>(out, val);
                    }
                    else
                    {
                        const era_entry* era = nullptr;
                        if (modifier == static_cast<CharT>('E'))
                            era = get_era_entry(*ymd);
                        if (era)
                        {
                            int64_t v = static_cast<int64_t>(era->offset)
                                + (static_cast<int64_t>(static_cast<int>(ymd->year())) - era->from_year) * era->direction;
                            int iv = static_cast<int>(std::clamp<int64_t>(v,
                                -static_cast<int64_t>(std::numeric_limits<int>::max()), std::numeric_limits<int>::max()));
                            if (iv < 0) { *out++ = static_cast<CharT>('-'); iv = -iv; }
                            out = put_dec<0>(out, iv);
                        }
                        else
                            out = put_dec<2>(out, val);
                    }
                }
                break;

            case static_cast<CharT>('Y'):
                if (!ymd || modifier == static_cast<CharT>('O')) goto bad_format;
                {
                    const era_entry* era = nullptr;
                    if (modifier == static_cast<CharT>('E'))
                        era = get_era_entry(*ymd);
                    if (era)
                    {
                        const auto& subfmt = era->format;
                        out = do_put(out, subfmt, ymd, wd, hms, tz);
                    }
                    else
                    {
                        int yr = static_cast<int>(ymd->year());
                        if (yr < 0)
                        {
                            *out++ = static_cast<CharT>('-');
                            yr = -yr;
                        }
                        // Min width 4, but never truncate (matches std::format).
                        if (yr > 9999) out = put_dec<0>(out, yr);
                        else           out = put_dec<4>(out, yr);
                    }
                }
                break;

            case static_cast<CharT>('z'):
                if (!tz || !ymd || !hms || modifier) goto bad_format;
                {
                    std::chrono::local_time<std::chrono::seconds> lt{
                        std::chrono::local_days{*ymd} + hms->to_duration()
                    };
                    int val = static_cast<int>(tz->get_info(lt).first.offset.count());
                    if (val < 0)
                    {
                        *out++ = static_cast<CharT>('-');
                        val = -val;
                    }
                    else
                        *out++ = static_cast<CharT>('+');
                    val /= 60;
                    out = put_dec<4>(out, (val / 60) * 100 + val % 60);
                }
                break;

            case static_cast<CharT>('Z'):
                if (!tz || modifier) goto bad_format;
                {
                    auto iana = tz->name();
                    out = std::copy(iana.begin(), iana.end(), out);
                }
                break;

            default:
            bad_format:
                /* Unknown format; output the format, including the '%',
                since this is most likely the right thing to do if a
                multibyte string has been misparsed.  */
                *out++ = static_cast<CharT>('%');
                if (modifier) *out++ = modifier;
                while (f != format.cend() && *f != static_cast<CharT>('%'))
                    *out++ = *f++;
                continue;
            }
            ++f;
        }
        return out;
    }

private:
    /**
     * @lang{ZH}
     * @brief 在 `m_era_master` 中查找包含指定日期的纪元条目。
     * @param ymd 要查询的日历日期。
     * @return 指向匹配的 `era_entry` 的指针；若无匹配则返回 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * @brief Finds the era entry in `m_era_master` that contains the given date.
     * @param ymd The calendar date to query.
     * @return Pointer to the matching `era_entry`, or `nullptr` if none matches.
     * @endif
     */
    const era_entry* get_era_entry(const std::chrono::year_month_day& ymd) const
    {
        int year  = static_cast<int>(ymd.year());
        uint8_t month = static_cast<unsigned>(ymd.month());
        uint8_t day   = static_cast<unsigned>(ymd.day());

        using namespace TimeioHelper;
        for (size_t i = 0; i < m_era_master.size(); ++i)
        {
            const auto& _cmp = m_era_master[i];
            if (era_small_or_equal(_cmp.from_year, _cmp.from_month, _cmp.from_day,
                                   year, month, day) &&
                era_small_or_equal(year, month, day,
                                   _cmp.to_year, _cmp.to_month, _cmp.to_day))
                return &_cmp;
            if (era_small_or_equal(year, month, day,
                                   _cmp.from_year, _cmp.from_month, _cmp.from_day) &&
                era_small_or_equal(_cmp.to_year, _cmp.to_month, _cmp.to_day,
                                   year, month, day))
                return &_cmp;
        }
        return nullptr;
    }

    /**
     * @lang{ZH}
     * @brief 将整数以十进制格式写入输出迭代器，支持替代数字。
     *
     * 若 `alt` 为 `true` 且 locale 为 `val` 定义了非空替代数字字符串，则输出该字符串；
     * 否则委托给固定宽度的 `put_dec<n>(out, val)` 重载。
     * @tparam n   最小宽度（不足时用 `def` 填充，`0` 表示不限宽度）。
     * @tparam def 填充字符，默认为 `'0'`。
     * @param out 输出迭代器。
     * @param val 要输出的非负整数（断言 `[0, 99]`）。
     * @param alt 若为 `true`，优先使用替代数字。
     * @return 写入后的输出迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Writes an integer in decimal to the output iterator with alternative-digit support.
     *
     * If `alt` is `true` and the locale defines a non-empty alternative digit string for
     * `val`, that string is emitted; otherwise delegates to the fixed-width
     * `put_dec<n>(out, val)` overload.
     * @tparam n   Minimum width (padded with `def`; `0` means no minimum).
     * @tparam def Padding character, default `'0'`.
     * @param out The output iterator.
     * @param val The non-negative integer to output (asserted in `[0, 99]`).
     * @param alt If `true`, prefer alternative digit strings.
     * @return The output iterator after writing.
     * @endif
     */
    template <size_t n, CharT def = static_cast<CharT>('0'), typename OutIt>
    OutIt put_dec(OutIt out, int val, bool alt) const
    {
        assert((val >= 0) && (val < 100));
        if (alt)
        {
            const auto& str = m_alt_digits[val];
            if (!str.empty())
                out = std::copy(str.begin(), str.end(), out);
            else out = put_dec<n, def>(out, val);
        }
        else
            out = put_dec<n, def>(out, val);
        return out;
    }

    /**
     * @lang{ZH}
     * @brief 将整数以最小宽度 `n` 的十进制格式写入输出迭代器。
     *
     * 若 `n == 0`，按实际位数输出（不限宽度）；否则最小输出 `n` 位，
     * 前导位以 `def` 填充。永不截断（位数超过 `n` 时输出全部位数）。
     * @tparam n   最小宽度（`0` 表示不限宽度）。
     * @tparam def 前导填充字符，默认为 `'0'`。
     * @param out 输出迭代器。
     * @param val 要输出的整数。
     * @return 写入后的输出迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Writes an integer in decimal with a minimum width of `n` to the output iterator.
     *
     * When `n == 0`, outputs the exact number of digits (no minimum). Otherwise,
     * outputs at least `n` digits, padding leading positions with `def`. Never
     * truncates (outputs all digits if the value exceeds `n` digits).
     * @tparam n   Minimum width (`0` means no minimum).
     * @tparam def Leading padding character, default `'0'`.
     * @param out The output iterator.
     * @param val The integer to output.
     * @return The output iterator after writing.
     * @endif
     */
    template <size_t n, CharT def = static_cast<CharT>('0'), typename OutIt>
    OutIt put_dec(OutIt out, int val) const
    {
        if (val < 0) val = 0;

        if constexpr (n == 0)
        {
            char digits[std::numeric_limits<int>::digits10 + 1];
            int i = 0;
            do {
                digits[i++] = static_cast<char>('0' + val % 10);
                val /= 10;
            } while (val != 0);
            for (int j = i - 1; j >= 0; --j)
                *out++ = static_cast<CharT>(digits[j]);
        }
        else
        {
            int buf[n];
            for (size_t i = 0; i < n; ++i)
            {
                buf[n - i -1] = val % 10;
                val /= 10;
            }
            bool leading = true;
            for (size_t i = 0; i < n; ++i)
            {
                if (buf[i] != 0) leading = false;
                if (leading && (i < n - 1)) *out++ = def;
                else *out++ = static_cast<CharT>(buf[i] + '0');
            }
        }
        return out;
    }

    /**
     * @lang{ZH}
     * @brief 从输入范围中解析最多 `len` 位的十进制整数，并写入 `member`。
     *
     * 解析成功（至少读取 1 位，且结果在 `[min_val, max_val]` 内）时更新 `member`；
     * 否则将 `succ` 置为 `false`。
     * @tparam TIter 输入迭代器类型。
     * @tparam TSent 哨兵类型。
     * @param beg     当前输入位置。
     * @param end     输入范围结束哨兵。
     * @param member  输出：解析结果。
     * @param min_val 可接受的最小值（含）。
     * @param max_val 可接受的最大值（含）。
     * @param len     最多读取的位数。
     * @param succ    输出标志：失败时置为 `false`。
     * @return 指向未被消费的第一个字符的迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Parses up to `len` decimal digits from the input range and writes the
     *        result into `member`.
     *
     * Updates `member` on success (at least one digit read and result within
     * `[min_val, max_val]`); otherwise sets `succ` to `false`.
     * @tparam TIter Input iterator type.
     * @tparam TSent Sentinel type.
     * @param beg     Current input position.
     * @param end     End sentinel of the input range.
     * @param member  Output: the parsed integer value.
     * @param min_val Minimum acceptable value (inclusive).
     * @param max_val Maximum acceptable value (inclusive).
     * @param len     Maximum number of digits to read.
     * @param succ    Output flag: set to `false` on failure.
     * @return Iterator pointing to the first unconsumed character.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
    static TIter extract_num(TIter beg, TSent end, int& member, int min_val, int max_val, size_t len, bool& succ) // NOLINT(bugprone-easily-swappable-parameters)
    {
        size_t i = 0;
        int value = 0;
        for (; beg != end && i < len; ++beg, (void)++i)
        {
            const CharT c = *beg;
            if (c >= static_cast<CharT>('0') && c <= static_cast<CharT>('9'))
            {
                value = value * 10 + (c - static_cast<CharT>('0'));
                if (value > max_val) break;
            }
            else
                break;
        }
        if (i && value >= min_val && value <= max_val) member = value;
        else succ = false;
        return beg;
    }

    /**
     * @lang{ZH}
     * @brief 从输入范围中解析十进制整数，优先匹配替代数字前缀树。
     *
     * 先用 `m_alt_digits_tree` 尝试最长匹配；若失败则回退到 `extract_num`。
     * @tparam TIter 输入迭代器类型。
     * @tparam TSent 哨兵类型。
     * @param beg     当前输入位置。
     * @param end     输入范围结束哨兵。
     * @param member  输出：解析结果。
     * @param min_val 可接受的最小值（含）。
     * @param max_val 可接受的最大值（含）。
     * @param len     回退到 ASCII 数字时最多读取的位数。
     * @param succ    输出标志：失败时置为 `false`。
     * @return 指向未被消费的第一个字符的迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Parses a decimal integer from the input range, preferring alternative
     *        digits via the alt-digits prefix trie.
     *
     * Attempts a longest match against `m_alt_digits_tree` first; falls back to
     * `extract_num` if no match is found.
     * @tparam TIter Input iterator type.
     * @tparam TSent Sentinel type.
     * @param beg     Current input position.
     * @param end     End sentinel of the input range.
     * @param member  Output: the parsed integer value.
     * @param min_val Minimum acceptable value (inclusive).
     * @param max_val Maximum acceptable value (inclusive).
     * @param len     Maximum digits to read when falling back to ASCII parsing.
     * @param succ    Output flag: set to `false` on failure.
     * @return Iterator pointing to the first unconsumed character.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
    TIter extract_num_with_alt_digits(TIter beg, TSent end, int& member, int min_val, int max_val, size_t len, bool& succ) const
    {
        typename decltype(m_alt_digits_tree)::match_out_type match_res;
        beg = m_alt_digits_tree.max_match(beg, end, match_res);
        if (match_res)
        {
            member = *match_res;
            if ((member < min_val) || (member > max_val))
                succ = false;
        }
        else
            beg = extract_num(beg, end, member, min_val, max_val, len, succ);
        return beg;
    }

private:
    prefix_tree<CharT, int>                       m_day_tree;
    prefix_tree<CharT, int>                       m_month_tree;
    prefix_tree<CharT, int>                       m_am_pm_tree;
    prefix_tree<CharT, int>                       m_alt_digits_tree;
    prefix_tree<CharT, std::basic_string<CharT>>  m_era_tree;

    std::array<std::basic_string<CharT>, 7>   m_day;
    std::array<std::basic_string<CharT>, 7>   m_abbr_day;
    std::array<std::basic_string<CharT>, 12>  m_month;
    std::array<std::basic_string<CharT>, 12>  m_abbr_month;
    std::array<std::basic_string<CharT>, 100> m_alt_digits;
    std::basic_string<CharT>                  m_am;
    std::basic_string<CharT>                  m_pm;
    std::basic_string<CharT>                  m_date_format;
    std::basic_string<CharT>                  m_era_date_format;
    std::basic_string<CharT>                  m_time_format;
    std::basic_string<CharT>                  m_era_time_format;
    std::basic_string<CharT>                  m_time_zone_format;
    std::basic_string<CharT>                  m_era_time_zone_format;
    std::basic_string<CharT>                  m_date_time_format;
    std::basic_string<CharT>                  m_era_date_time_format;
    std::basic_string<CharT>                  m_date_time_zone_format;
    std::basic_string<CharT>                  m_era_date_time_zone_format;
    std::basic_string<CharT>                  m_am_pm_format;
    std::vector<era_entry>                    m_era_master;

    std::set<std::basic_string<CharT>>        m_era_formats;

    std::shared_ptr<const ctype<CharT>>       m_ctype;
};

/// @cond
template<typename TConfPtr, typename TCtypePtr>
    requires (std::is_same_v<typename TConfPtr::element_type::char_type,
                             typename TCtypePtr::element_type::char_type>)
timeio(TConfPtr, TCtypePtr) -> timeio<typename TConfPtr::element_type::char_type>;

template<typename TConfPtr>
timeio(TConfPtr) -> timeio<typename TConfPtr::element_type::char_type>;
/// @endcond
}
