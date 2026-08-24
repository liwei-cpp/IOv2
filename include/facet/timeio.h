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
#include <utility>
#include <vector>

namespace IOv2
{
/// @cond
template <typename, bool> struct date_parse_helper
{
    bool operator==(const date_parse_helper&) const = default;  // for test
    void convert_to() const = delete;
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
        // Per-field fallbacks for date components the parse leaves unset. These are
        // independent defaults, not a coherent "date": only the year is taken from the
        // clock (the useful, tested "this year" default), while month and day are fixed
        // to January 1. Day 1 is the only day-of-month valid in every month, so a parse
        // that supplies the month but not the day can never form an invalid calendar
        // date -- e.g. parsing "Feb" must not inherit today's 29/30/31.
        m_year = int(year_month_day{floor<days>(system_clock::now())}.year());
    }

    /**
     * @lang{ZH}
     * @brief 用 @p hint 替换年 / 月 / 日的默认回退值。
     *
     * 覆盖默认构造时装入的回退值（当前年份 + 1 月 1 日），使格式串未解析到的日期字段取
     * @p hint 的对应值，而不是取当前时间。已解析到的字段不受影响：解析会连同 `m_have_*`
     * 标志一起覆盖这里写入的值。
     * @note @p hint 的日在解析结果里不存在时会被夹到当月最后一天，而不是让整次转换因日期
     *       无效而失败；详见 `compute_ymd()`。@p hint 的年与月不做这种让步，它们在任何
     *       年 / 月组合下都是合法的。
     * @warning 必须在 `get()` **之前**调用。本函数直接写入解析字段，在 `get()` 之后调用会
     *          抹掉解析结果。
     * @param hint 各日期字段的回退值。
     * @endif
     *
     * @lang{EN}
     * @brief Replaces the year / month / day fallbacks with @p hint.
     *
     * Overrides the fallbacks installed by the default constructor (the current year plus
     * January 1) so that date fields the format string does not parse come from @p hint
     * rather than from the current time. Parsed fields are unaffected: parsing overwrites
     * what is stored here along with the matching `m_have_*` flag.
     * @note The day of @p hint is clamped to the last day of the month when it does not
     *       exist in the parsed result, rather than failing the whole conversion on an
     *       invalid date; see `compute_ymd()`. The year and month of @p hint need no such
     *       concession, as they are valid in any year/month combination.
     * @warning Must be called **before** `get()`. This writes the parse fields directly, so
     *          calling it afterwards discards the parsed result.
     * @param hint The fallback value for each date field.
     * @endif
     */
    void set_date_hint(const std::chrono::year_month_day& hint)
    {
        m_year = int(hint.year());
        m_month = static_cast<uint8_t>(static_cast<unsigned>(hint.month()));
        m_mday = static_cast<uint8_t>(static_cast<unsigned>(hint.day()));
    }

    /**
     * @lang{ZH}
     * @brief 将已累积的日期字段转换为 `std::chrono::year_month_day`，写入 @p out。
     * @param out 接收还原出的日历日期；抛出时不被写入。
     * @throw stream_error 若还原结果不是有效的日历日期。
     * @endif
     *
     * @lang{EN}
     * @brief Converts the accumulated date fields to a `std::chrono::year_month_day` in @p out.
     * @param out Receives the reconstructed calendar date; left untouched if this throws.
     * @throw stream_error If the reconstructed date is not a valid calendar date.
     * @endif
     */
    void convert_to(std::chrono::year_month_day& out) const
    {
        auto ymd = compute_ymd();
        if (!ymd.ok())
            throw stream_error("timeio get error: year_month_day is not a valid calendar date");
        out = ymd;
    }

    /**
     * @lang{ZH}
     * @brief 根据已累积的字段推算 `std::chrono::year_month_day`（不做有效性检查）。
     *
     * 按以下优先级尝试各种推算路径：
     * 1. 年 + 月 + 日；
     * 2. 年 + 年内第几天（`%j`）；
     * 3. ISO-8601 周日期（`%V` + 星期，年份取 `%G`，无 `%G` 时取上一步推算出的年份）；
     * 4. 根据周序号（`%U`/`%W`）和星期推算；
     * 5. 仅有星期时，按回退日期就近推算；
     * 6. 仅根据年份或世纪等做尽力推算。
     *
     * 只有星期、没有任何周号、也没有月与日时（路径 5），星期解析为**回退日期当天或之后**第一个
     * 匹配的日子。一个星期几只说明它在某一周里的位置，定位不到具体日期，所以必须补一个基准；
     * 取回退日期作基准，结果与回退值至多相差 6 天，而不会跳到当年碰巧匹配的某一周去。月或日
     * 只要有一个来自输入，星期就不再参与推算，缺的那个仍按回退值补齐。
     *
     * 若日不是从输入解析或推算出来的，而是 `set_date_hint()` / 默认构造留下的回退值，则它在
     * 超出推算月份的天数时会被夹到该月最后一天。回退值的作用只是补齐格式串没说的字段，不应
     * 让一次本可成功的解析失败——例如上下文回退到 1 月 31 日、格式串只有 `%m` 且输入为 `02`
     * 时，结果是 2 月的最后一天而不是不存在的 2 月 31 日。日若确实来自输入则原样保留，此时
     * 返回的日期可能无效。
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
     * 3. ISO-8601 week date (`%V` + weekday, with the year taken from `%G`, or from the
     *    year deduced by the previous step when `%G` is absent);
     * 4. deduction from week-of-year (`%U`/`%W`) and weekday;
     * 5. deduction from a lone weekday, relative to the fallback date;
     * 6. best-effort deduction from year or century alone.
     *
     * A weekday with no week number and no month or day of its own (path 5) resolves to the
     * first matching day **on or after the fallback date**. A weekday only states a position
     * within some week, so locating a date from it needs a reference point; taking the
     * fallback as that reference keeps the result within six days of it, rather than jumping
     * to whichever week of the year happens to match. Once either the month or the day comes
     * from the input, the weekday takes no part in the deduction and the missing one of the
     * two is filled from the fallback as usual.
     *
     * When the day was neither parsed nor deduced from the input but is the fallback left
     * by `set_date_hint()` or by the default constructor, it is clamped to the last day of
     * the deduced month if it would overrun it. A fallback exists only to fill in fields
     * the format string is silent about and must never turn a parse that succeeded into a
     * failure -- a context falling back to January 31 with a format of just `%m` and an
     * input of `02` yields the last day of February, not a nonexistent February 31. A day
     * that really does come from the input is left alone, so the returned date may be
     * invalid.
     * @return The deduced calendar date (may be invalid; caller must check `ok()`).
     * @endif
     */
    [[nodiscard]] std::chrono::year_month_day compute_ymd() const
    {
        using namespace std::chrono;
        if (m_have_year && m_have_mon && m_have_mday)
            return year_month_day{ year{m_year}, month{static_cast<uint8_t>(m_month)}, day{m_mday} };
        if (m_have_year && m_have_yday)
        {
            sys_days sd = sys_days{ year{m_year} / 1 / 1 } + days{ m_yday };
            return year_month_day{sd};
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
                // %C with no year within the century: the year within the century is 0,
                // as in POSIX strptime. Deriving it from m_year would make the result
                // depend on whatever the fallback happens to be -- the wall-clock year
                // for a default-constructed context, the caller's old value for a hinted
                // one -- for a format string that says nothing about it.
                deduced_year = m_century * 100;
            else if (m_have_era && !m_era_items.empty())
                deduced_year = m_era_items.begin()->from_year;
        }

        // ISO-8601 week date. Prefer the ISO year (%G); otherwise use the year deduced
        // above -- an explicit %Y, a century plus %y, an era, or the fallback -- instead
        // of dropping the week number. The two fully-specified branches above
        // (year+mon+mday, year+yday) already returned, so reaching here means the date is
        // not pinned down by an explicit month/day or day-of-year.
        if (m_have_iso_8601_week && m_have_wday)
        {
            int iso_year = m_have_iso_8601_year ? m_iso_8601_year : deduced_year;
            int iso_wd = (m_wday == 0 ? 7 : m_wday);
            year_month_day jan4 = year{iso_year}/January/4;
            weekday wd_jan4{sys_days{jan4}};
            sys_days week1_monday = sys_days{jan4} - (wd_jan4 - Monday);
            sys_days final = week1_monday + days{7 * (m_iso_8601_week - 1)} + days{iso_wd - 1};
            return year_month_day{final};
        }

        auto deduced_month = m_month;
        auto deduced_mday = m_mday;
        // True when no deduction path below can touch the day, i.e. the day is whatever
        // set_date_hint() or the default constructor left behind. Every assignment to
        // deduced_mday sits inside one of those paths and is guarded by !m_have_mday.
        // A lone weekday, with no week number and no month or day of its own, is resolved
        // against the fallback date rather than against January 1: see the branch below.
        const bool wday_snaps = m_have_wday && !m_have_mon && !m_have_mday
            && !(m_have_yday || m_have_uweek || m_have_wweek);
        const bool mday_is_fallback = !m_have_mday
            && !(m_have_yday || m_have_uweek || m_have_wweek || wday_snaps);
        int deduced_yday = static_cast<int>(m_yday);
        bool have_yday = m_have_yday;
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
            else if (wday_snaps)
            {
                // A weekday on its own cannot locate a date; only the offset within some
                // week is known. Resolve it as the first matching weekday on or after the
                // fallback date, so the result stays within six days of the fallback
                // instead of jumping to whichever week of the year happens to match.
                auto last = static_cast<unsigned>(
                    year_month_day_last{year{deduced_year},
                                        month_day_last{month{static_cast<unsigned>(deduced_month)}}}
                        .day());
                sys_days base{year{deduced_year}
                              / month{static_cast<unsigned>(deduced_month)}
                              / day{std::min<unsigned>(deduced_mday, last)}};
                auto cur = static_cast<int>(weekday{base}.c_encoding());
                year_month_day snapped{base + days{(static_cast<int>(m_wday) - cur + 7) % 7}};
                deduced_year  = int(snapped.year());
                deduced_month = static_cast<uint8_t>(static_cast<unsigned>(snapped.month()));
                deduced_mday  = static_cast<uint8_t>(static_cast<unsigned>(snapped.day()));
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

        // A day that came from the fallback rather than from the input must give way when
        // it cannot exist in the deduced month: the fallback only supplies fields the
        // format string did not parse, so it must never turn a well-formed parse into an
        // invalid date. A day the input really did specify is left alone and reported as
        // invalid.
        if (mday_is_fallback && deduced_month >= 1 && deduced_month <= 12)
        {
            auto last = static_cast<unsigned>(
                year_month_day_last{year{deduced_year},
                                    month_day_last{month{static_cast<unsigned>(deduced_month)}}}
                    .day());
            if (static_cast<unsigned>(deduced_mday) > last)
                deduced_mday = static_cast<uint8_t>(last);
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
    bool m_have_era : 1 = false;

    bool m_have_mon : 1 = false;
    bool m_have_uweek : 1 = false;
    bool m_have_wweek : 1 = false;

    bool m_have_yday : 1 = false;
    bool m_have_mday : 1 = false;
    bool m_have_wday : 1 = false;

private:
    constexpr static const std::array<std::array<unsigned short int, 13>, 2> s_mon_yday =
    {{
        /* Normal years.  */
        {{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 }},
        /* Leap years.  */
        {{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }}
    }};

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
    void convert_to() const = delete;
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
     * @brief 将已累积的时间字段转换为 `std::chrono::hh_mm_ss<TDuration>`，写入 @p out。
     *
     * 若通过 `%I`（12 小时制）解析并设置了 PM 标志，则自动加上 12 小时。
     *
     * @tparam TDuration 目标精度。约束为"`std::chrono::seconds` 可**隐式**转换成它"，即周期
     *         不粗于秒：`milliseconds` 可以（精确加宽），`minutes` 不可以（会丢掉秒）。本上下文
     *         只累积到整秒，这条约束保证转换永不静默截断，与本库"有损即报错"的一贯立场一致。
     * @param out 接收还原出的 24 小时制时间。
     * @endif
     *
     * @lang{EN}
     * @brief Converts the accumulated time fields to a `std::chrono::hh_mm_ss<TDuration>` in
     *        @p out.
     *
     * If the hour was parsed via `%I` (12-hour clock) and the PM flag is set,
     * 12 hours are added automatically.
     *
     * @tparam TDuration The target precision, constrained so that `std::chrono::seconds` is
     *         **implicitly** convertible to it -- that is, a period no coarser than a second:
     *         `milliseconds` qualifies (an exact widening), `minutes` does not (it would drop the
     *         seconds). This context only ever accumulates whole seconds, so the constraint makes
     *         a silent truncation impossible, matching this library's standing rule that a lossy
     *         operation is an error rather than a quiet one.
     * @param out Receives the reconstructed 24-hour time-of-day.
     * @endif
     */
    template <typename TDuration>
        requires std::convertible_to<std::chrono::seconds, TDuration>
    void convert_to(std::chrono::hh_mm_ss<TDuration>& out) const
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

        out = std::chrono::hh_mm_ss<TDuration>{time_sec};
    }

    /**
     * @lang{ZH}
     * @brief 用 @p hint 替换时 / 分 / 秒的默认回退值。
     *
     * 覆盖默认的 00:00:00，使格式串未解析到的时间字段取 @p hint 的对应值。已解析到的字段
     * 不受影响。`m_have_I` / `m_is_pm` 不被触碰，因此后续的 `%I` / `%p` 解析照常生效。
     * @note @p hint 按 24 小时取模后再拆分：`hours() >= 24` 或为负的 @p hint 都会被折回
     *       一天之内（日期不受影响），因此本函数总是产出合法的时分秒。取模用
     *       `to_duration()` 而非 `hours()` / `minutes()` / `seconds()`，后者对负的
     *       `hh_mm_ss` 返回的是绝对值分量。
     * @warning 必须在 `get()` **之前**调用；理由同 `date_parse_helper::set_date_hint`。
     * @tparam TDur @p hint 的时长精度，可为任意精度；本结构只保存到秒，更细的精度按向零
     *         取整丢弃。
     * @param hint 各时间字段的回退值。
     * @endif
     *
     * @lang{EN}
     * @brief Replaces the hour / minute / second fallbacks with @p hint.
     *
     * Overrides the default 00:00:00 so that time fields the format string does not parse
     * come from @p hint. Parsed fields are unaffected. `m_have_I` / `m_is_pm` are left
     * untouched, so a later `%I` / `%p` parse still applies as usual.
     * @note @p hint is reduced modulo 24 hours before being split, so a @p hint whose
     *       `hours() >= 24`, or a negative one, folds back into a single day (the date is
     *       not affected) and this function always yields a valid time of day. The
     *       reduction uses `to_duration()` rather than `hours()` / `minutes()` /
     *       `seconds()`, which report absolute-value components for a negative
     *       `hh_mm_ss`.
     * @warning Must be called **before** `get()`, for the reason given on
     *          `date_parse_helper::set_date_hint`.
     * @tparam TDur The duration precision of @p hint; any precision is accepted. This struct
     *         only stores whole seconds, so anything finer is truncated toward zero.
     * @param hint The fallback value for each time field.
     * @endif
     */
    template <typename TDur>
    void set_time_hint(const std::chrono::hh_mm_ss<TDur>& hint)
    {
        using namespace std::chrono;

        auto tod = duration_cast<seconds>(hint.to_duration()) % days{1};
        if (tod < seconds{0}) tod += days{1};

        m_hour = static_cast<uint8_t>(duration_cast<hours>(tod).count());
        m_minute = static_cast<uint8_t>(duration_cast<minutes>(tod % hours{1}).count());
        m_second = static_cast<uint8_t>((tod % minutes{1}).count());
    }

    uint8_t m_hour = 0;         // hours since midnight – [0, 23]
    uint8_t m_minute = 0;       // minutes after the hour – [0, 59]
    uint8_t m_second = 0;       // seconds after the minute – [0, 59]
    bool m_have_I : 1 = false;
    bool m_is_pm : 1 = false;
};

/**
 * @lang{ZH}
 * @brief 解析上下文所携带的时区信息档位。
 *
 * 每一档回答两个问题——`%z` 解不解析、`%Z` 解不解析——两个说明符在不解析的那一档按
 * 字面量处理，与 put 对同一类值的退化正好对上：
 *
 * | 档 | `%z` | `%Z` | `time_value_fields` | 对应的值类型 |
 * |---|---|---|---|---|
 * | `none`   | 字面量 | 字面量 | `has_offset` 假、`has_zone` 假 | `year_month_day`、`hh_mm_ss` |
 * | `offset` | 解析   | 字面量 | `has_offset` 真、`has_zone` 假 | `local_time` |
 * | `zone`   | 解析   | 解析   | 两者皆真                       | `std::tm`、`sys_time`、`zoned_time` |
 *
 * 第四种组合（`%z` 字面量、`%Z` 解析）不存在，也不需要：能说出区名的值必然说得出偏移。
 * 于是三档仍然是累积的，`zone` 的存储包含 `offset`，`offset` 的包含 `none`。
 *
 * 偏移与区名之所以叠在一档而不是像 `HaveDate` / `HaveTime` 那样各占一个独立开关：它们是
 * 同一件事的两种说法，转换的工作恰恰是把两者对上——`convert_to(sys_time&)` 的取值阶梯
 * 一条链上同时读两边，`convert_to(zoned_time&)` 还要拿解析到的偏移去校验解析到的区。日期
 * 与时间没有这种纠缠，所以那两个才是独立的。
 * @endif
 *
 * @lang{EN}
 * @brief The tier of time-zone information a parse context carries.
 *
 * Each tier answers two questions -- is `%z` parsed, is `%Z` parsed -- and a specifier the
 * tier does not parse is matched literally, which is exactly what put degrades it to for the
 * corresponding values:
 *
 * | Tier | `%z` | `%Z` | `time_value_fields` | Value types |
 * |---|---|---|---|---|
 * | `none`   | literal | literal | `has_offset` and `has_zone` false | `year_month_day`, `hh_mm_ss` |
 * | `offset` | parsed  | literal | `has_offset` true, `has_zone` false | `local_time` |
 * | `zone`   | parsed  | parsed  | both true | `std::tm`, `sys_time`, `zoned_time` |
 *
 * The fourth combination -- `%z` literal, `%Z` parsed -- does not arise and is not needed: a
 * value able to name a zone can always produce an offset. The three tiers therefore remain
 * cumulative, `zone` storage containing `offset`, which contains `none`.
 *
 * Offset and zone share one tier rather than getting independent switches the way `HaveDate`
 * and `HaveTime` do because they are two descriptions of one thing, and reconciling them is
 * precisely what conversion does: the ladder in `convert_to(sys_time&)` reads both along one
 * chain, and `convert_to(zoned_time&)` checks the parsed offset against the parsed zone. Date
 * and time have no such entanglement, which is why those two are separate.
 * @endif
 */
enum class tz_level : unsigned char
{
    none   = 0, ///< @lang{ZH} 不携带时区字段，`%z` 与 `%Z` 都按字面量处理。 @endif @lang{EN} Carries no time-zone field; `%z` and `%Z` are both matched literally. @endif
    offset = 1, ///< @lang{ZH} 解析 `%z` 的 UTC 偏移；`%Z` 仍按字面量处理。 @endif @lang{EN} Parses the UTC offset from `%z`; `%Z` is still matched literally. @endif
    zone   = 2, ///< @lang{ZH} 在 `offset` 之上再解析 `%Z`，得到区域身份与缩写。 @endif @lang{EN} Everything in `offset`, plus `%Z` parsed into a zone identity and abbreviation. @endif
};

/// @cond
template <tz_level> struct time_zone_parse_helper
{
    bool operator==(const time_zone_parse_helper&) const = default;  // for test
    void convert_to() const = delete;
};
/// @endcond

/**
 * @lang{ZH}
 * @brief 解析时区字段的辅助结构（`tz_level::offset` 特化）。
 *
 * 作为 `time_parse_context` 的基类，累积从 `%z` 解析到的 UTC 偏移，以及 `%Z` 读到的原文
 * ——按 tzdb 的身份分存 @ref m_zone_name 与 @ref m_zone_abbrev 两处，但本档只记不解析，
 * 定位是 `tz_level::zone` 档的事。
 *
 * @note 此结构为内部实现细节；请通过 `time_parse_context` 访问其功能。
 * @endif
 *
 * @lang{EN}
 * @brief Time-zone-field accumulator helper struct (`tz_level::offset` specialization).
 *
 * Serves as a base class of `time_parse_context`, accumulating the UTC offset parsed from `%z`
 * together with the raw text read by `%Z`, filed into @ref m_zone_name and @ref m_zone_abbrev
 * according to what the tzdb says it is. This tier only records that text; resolving it is
 * `tz_level::zone`'s business.
 *
 * @note This struct is an internal implementation detail; access its functionality
 *       through `time_parse_context`.
 * @endif
 */
template <>
struct time_zone_parse_helper<tz_level::offset>
{
    /// @cond
    bool operator==(const time_zone_parse_helper&) const = default;  // for test
    /// @endcond

    /**
     * @lang{ZH}
     * @brief 将已解析的 UTC 偏移写入 @p out。
     *
     * 取值顺序：解析到的偏移 → `set_offset_hint()` 设的回退值；两者都没有则抛出。
     * @param out 接收 UTC 偏移；抛出时不被写入。
     * @throw stream_error 若既未解析到偏移，也未设置回退值。
     * @endif
     *
     * @lang{EN}
     * @brief Writes the parsed UTC offset into @p out.
     *
     * The order is the parsed offset, then the fallback installed by `set_offset_hint()`;
     * with neither present this throws.
     * @param out Receives the UTC offset; left untouched if this throws.
     * @throw stream_error If no offset was parsed and no fallback was installed.
     * @endif
     */
    void convert_to(std::chrono::minutes& out) const
    {
        if (m_have_offset) { out = m_offset; return; }
        if (m_have_offset_hint) { out = m_offset_hint; return; }
        throw stream_error("timeio get error: no UTC offset was parsed");
    }

    /**
     * @lang{ZH}
     * @brief 设置解析未得到 UTC 偏移时使用的回退偏移。
     *
     * 与 `set_time_zone_hint()` 一样，本 hint **不写入解析字段**，而是单独保存、仅在转换时
     * 兜底，因此在 `get()` 之前或之后调用都可以。默认不设：缺省行为是报错，不是猜。
     * @param hint 回退偏移。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the fallback UTC offset used when parsing yields none.
     *
     * Like `set_time_zone_hint()`, this hint is **not written into the parse fields**: it is
     * stored separately and consulted only on conversion, so it may be called either before
     * or after `get()`. None is installed by default; the default behaviour is to report an
     * error rather than to guess.
     * @param hint The fallback offset.
     * @endif
     */
    void set_offset_hint(std::chrono::minutes hint)
    {
        m_offset_hint = hint;
        m_have_offset_hint = true;
    }

    std::chrono::minutes m_offset{0};        // %z
    std::chrono::minutes m_offset_hint{0};
    bool m_have_offset = false;
    bool m_have_offset_hint = false;
};

/**
 * @lang{ZH}
 * @brief 解析时区字段的辅助结构（`tz_level::zone` 特化）。
 *
 * 在 `tz_level::offset` 的存储之上，把那一档记下来的 @ref
 * time_zone_parse_helper<tz_level::offset>::m_zone_name 真正拿去用：在转换时以
 * `std::chrono::locate_zone()` 还原为 `const std::chrono::time_zone*`，并提供解析不出时区
 * 时的回退时区。
 *
 * @note 此结构为内部实现细节；请通过 `time_parse_context` 访问其功能。
 * @endif
 *
 * @lang{EN}
 * @brief Time-zone-field accumulator helper struct (`tz_level::zone` specialization).
 *
 * On top of the `tz_level::offset` storage, this is the tier that actually puts that tier's
 * @ref time_zone_parse_helper<tz_level::offset>::m_zone_name to use: it resolves it to a
 * `const std::chrono::time_zone*` on conversion via `std::chrono::locate_zone()`, and offers a
 * fallback zone for when the parse yields none.
 *
 * @note This struct is an internal implementation detail; access its functionality
 *       through `time_parse_context`.
 * @endif
 */
template <>
struct time_zone_parse_helper<tz_level::zone>
    : time_zone_parse_helper<tz_level::offset>
{
    /// @cond
    using time_zone_parse_helper<tz_level::offset>::convert_to;
    bool operator==(const time_zone_parse_helper&) const = default;  // for test
    /// @endcond
    /**
     * @lang{ZH}
     * @brief 将已解析的时区信息转换为 `const std::chrono::time_zone*`，写入 @p out。
     *
     * 若已解析到 tzdb 认得的标识符，则通过 `std::chrono::locate_zone` 定位；
     * 若只解析到定位不到的缩写（有歧义），则抛出 `stream_error`；
     * 若两者均无——包括 `%Z` 解析到 @ref base_ft<timeio>::s_unknown_zone 这种「明说没有
     * 时区」的情形——则依次退到 `set_time_zone_hint()` 设的时区与 UTC。
     * @param out 接收指向时区对象的指针（从不为 null）；抛出时不被写入。
     * @throw stream_error 若时区缩写有歧义或时区数据库不可用。
     * @endif
     *
     * @lang{EN}
     * @brief Converts the parsed timezone information to a `const std::chrono::time_zone*` in
     *        @p out.
     *
     * If a tzdb-known identifier was parsed, it is located via `std::chrono::locate_zone`. If
     * only an abbreviation that locates nothing was parsed (ambiguous), a `stream_error` is
     * thrown. If neither is present -- including when `%Z` parsed
     * @ref base_ft<timeio>::s_unknown_zone, which says outright that there is no zone -- the
     * zone installed by `set_time_zone_hint()` is used, and failing that UTC.
     * @param out Receives a non-null pointer to the timezone object; left untouched if this
     *            throws.
     * @throw stream_error If the timezone abbreviation is ambiguous or the tz
     *        database is unavailable.
     * @endif
     */
    void convert_to(const std::chrono::time_zone*& out) const
    {
        if (m_zone_name)
        {
            // Set only for keys the tzdb knew when the trie was built, so this all but always
            // succeeds; it can still fail if a reload_tzdb() has since dropped the name, and
            // then the text counts as naming nothing.
            try { out = std::chrono::locate_zone(m_zone_name); return; }
            catch (...) {} // NOLINT(bugprone-empty-catch)
        }
        // An abbreviation that names no zone is an error rather than a reason to fall back:
        // the text did say something about the zone, we just cannot act on it. The
        // `*m_zone_abbrev` keeps the unknown-zone token out of here -- it leaves a pointer to
        // an empty string, which says there is no zone, and that the hint below may fill in.
        if (m_zone_abbrev && *m_zone_abbrev)
            throw stream_error(std::string("timeio get error: timezone abbreviation '")
                               + m_zone_abbrev + "' is ambiguous");
        if (m_zone_hint) { out = m_zone_hint; return; }
        try { out = std::chrono::locate_zone("UTC"); }
        catch (...)
        {
            throw stream_error(
                "timeio parse error: no usable time zone (tz database unavailable)");
        }
    }

    /**
     * @lang{ZH}
     * @brief 设置解析未得到时区时使用的回退时区。
     *
     * 与日期 / 时间的 hint 不同，本 hint **不写入解析字段**，而是单独保存、仅在转换时兜底：
     * 解析到的完整时区名优先，其次是"缩写有歧义"这一错误，再次才是本 hint，最后才是 UTC。
     * 因此本函数在 `get()` 之前或之后调用都可以。
     * @note 日期 / 时间侧无法采用同样的写法：它们的回退值必须参与 `compute_ymd()` 的字段推导，
     *       只能预置进解析字段。
     * @param hint 回退时区；传 `nullptr` 恢复为默认的 UTC 兜底。指向的对象须在本上下文
     *        转换期间保持有效（`locate_zone` 返回的指针指向 tz 数据库，其生命周期为整个程序）。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the fallback time zone used when parsing yields none.
     *
     * Unlike the date / time hints, this one is **not written into the parse fields**: it is
     * stored separately and consulted only on conversion, after a fully parsed zone name and
     * after the ambiguous-abbreviation error, and before the UTC default. It may therefore be
     * called either before or after `get()`.
     * @note The date / time sides cannot work this way: their fallbacks have to take part in
     *       the field deduction in `compute_ymd()`, so they must be pre-seeded into the parse
     *       fields.
     * @param hint The fallback zone; `nullptr` restores the default UTC fallback. The pointee
     *        must stay valid for as long as this context is converted (a pointer from
     *        `locate_zone` refers to the tz database and lives for the whole program).
     * @endif
     */
    void set_time_zone_hint(const std::chrono::time_zone* hint)
    {
        m_zone_hint = hint;
    }

    /**
     * @lang{ZH}
     * @brief `%Z` 读到的、tzdb 认得的那个标识符，指向时区前缀树内的存储。
     *
     * 只在 `%Z` 匹配到的文本是 `tzdb.zones` 或 `tzdb.links` 里的名字时才非空，因此
     * 「非空」与「`std::chrono::locate_zone()` 定得到」是同一件事，判它就够了，不必先去
     * 定位一次。反过来，纯缩写（本机 179 条里的 171 条）在这里永远是 `nullptr`——那是
     * @ref m_zone_abbrev 的事。两者同时非空的条目有 8 个（`CET`、`EET`、`EST`、`GMT`、
     * `HST`、`MST`、`UTC`、`WET`），它们既是 link 名又是缩写。
     *
     * 存的是**原文，不做规范化**：解析 `US/Pacific` 得到的就是 `US/Pacific`，解析 `EST`
     * 得到的就是 `EST`。要规范名请用 `tz_level::zone` 档的
     * `convert_to(const std::chrono::time_zone*&)` 拿到时区再取 `name()`——`locate_zone`
     * 自己会归一化，没有理由在这里再存一份；而原文是 `std::tm::tm_zone` 往返所必需的。
     *
     * 本字段两档都有，因为 `tz_level::offset` 档的 `std::tm` 同样要靠它还原 `tm_zone`；
     * 档位管的是解析之后**能不能拿去定位**，不是能不能记下来。
     *
     * 指向的存储在时区前缀树里，随程序始终有效。
     * @endif
     *
     * @lang{EN}
     * @brief The tzdb-known identifier read by `%Z`, pointing into the time-zone trie's storage.
     *
     * Non-null only when the text `%Z` matched is a name in `tzdb.zones` or `tzdb.links`, so
     * "non-null" and "`std::chrono::locate_zone()` resolves it" are one and the same: testing
     * the pointer is enough, with no need to locate first. Conversely a bare abbreviation (171
     * of the 179 here) always leaves this `nullptr` -- that is @ref m_zone_abbrev's business.
     * Eight entries set both (`CET`, `EET`, `EST`, `GMT`, `HST`, `MST`, `UTC`, `WET`), each
     * being a link name and an abbreviation at once.
     *
     * What is stored is the text **verbatim, not canonicalized**: parsing `US/Pacific` yields
     * `US/Pacific` and parsing `EST` yields `EST`. For the canonical name, take the zone from
     * the `tz_level::zone` tier's `convert_to(const std::chrono::time_zone*&)` and read its
     * `name()` -- `locate_zone` normalizes on its own and there is no reason to keep a second
     * copy here, whereas the verbatim text is what a `std::tm::tm_zone` round trip needs.
     *
     * Both tiers carry this field, because a `std::tm` at `tz_level::offset` needs it just as
     * much to restore `tm_zone`: the tier governs whether the parsed text can be **resolved**,
     * not whether it can be recorded.
     *
     * The pointee lives in the time-zone trie and stays valid for the whole program.
     * @endif
     */
    const char* m_zone_name = nullptr;

    /**
     * @lang{ZH}
     * @brief `%Z` 读到的原文，指向时区前缀树内的存储。
     *
     * 三个状态，别把后两个混为一谈：
     * - `nullptr`：`%Z` 压根没解析到（格式串里没有，或者有但没匹配上）；
     * - 指向空串：解析到了 @ref base_ft<timeio>::s_unknown_zone，即文本明说「没有时区」；
     * - 指向非空串：解析到了真实的缩写或区名，原文逐字节保留，不做规范化。
     *
     * 于是有两个不同的判据，别互相顶替：问「有没有时区」用 `p && *p`（与 put 侧
     * `t.tm_zone && *t.tm_zone` 对称）；问「`%Z` 有没有解析到东西」只看 `p`——回写
     * `std::tm::tm_zone` 用的是后者，空串正是要写进去的值。
     *
     * 指向的存储在时区前缀树里，随程序始终有效，因此可以直接交给 `tm_zone` 这类只收
     * `const char*` 又没有释放接口的字段。本档不解析它：能不能定位到时区是
     * `tz_level::zone` 才关心的事。
     * @endif
     *
     * @lang{EN}
     * @brief The raw text read by `%Z`, pointing into the time-zone trie's storage.
     *
     * Three states, and the latter two are not the same thing:
     * - `nullptr`: no `%Z` was parsed at all (none in the format, or one that did not match);
     * - pointing at an empty string: @ref base_ft<timeio>::s_unknown_zone was parsed, i.e. the
     *   text says outright that there is no zone;
     * - pointing at a non-empty string: a real abbreviation or zone name was parsed, kept byte
     *   for byte and not canonicalized.
     *
     * Two distinct tests follow, and neither substitutes for the other: ask "is there a zone"
     * with `p && *p` (symmetric with the put side's `t.tm_zone && *t.tm_zone`); ask "did `%Z`
     * parse anything" with just `p` -- writing back to `std::tm::tm_zone` uses the latter,
     * because the empty string is precisely the value to write there.
     *
     * The pointee lives in the time-zone trie and stays valid for the whole program, so it may
     * be handed straight to a field such as `tm_zone`, which takes a `const char*` and offers
     * no matching release call. This tier does not resolve it: whether it locates a zone is
     * `tz_level::zone`'s concern.
     * @endif
     */
    const char*          m_zone_abbrev = nullptr;

    const std::chrono::time_zone* m_zone_hint = nullptr;
};

/**
 * @lang{ZH}
 * @brief `timeio::get()` 的聚合解析上下文。
 *
 * 将日期、时间、时区三个辅助结构组合为统一的解析状态容器，作为 `get()` 的输出参数。
 * 模板参数控制哪些字段被激活：
 * - `HaveDate`：启用日期字段（年、月、日、星期等）；
 * - `HaveTime`：启用时间字段（时、分、秒、AM/PM 等）；
 * - `TzLevel`：启用哪一档时区字段（`%z` / `%Z`），见 `tz_level`。
 *
 * 典型用法：
 * 1. 默认构造一个 `time_parse_context`；
 * 2. 可选：调用 `set_hint()` 为格式串不会解析到的字段指定回退值，否则这些字段取默认回退值
 *    （日期取"今年 1 月 1 日"，时间取 00:00:00，时区取 UTC）；
 * 3. 将其传入一次或多次 `get()` 调用（跨多次调用累积同一值的字段）；
 * 4. 调用转换运算符提取结果；
 * 5. 若要解析下一个不同的时间值，先调用 `reset()`。
 *
 * @tparam CharT    字符类型。
 * @tparam HaveDate 为 `true` 时激活日期解析，默认 `true`。
 * @tparam HaveTime 为 `true` 时激活时间解析，默认 `true`。
 * @tparam TzLevel  激活的时区档位，默认 `tz_level::zone`。
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
 * - `TzLevel`: selects which tier of timezone fields (`%z` / `%Z`) is enabled; see `tz_level`.
 *
 * Typical usage:
 * 1. Default-construct a `time_parse_context`;
 * 2. Optionally call `set_hint()` to choose the fallbacks for fields the format string will
 *    not parse; without it those fields take the default fallbacks (January 1 of the current
 *    year for the date, 00:00:00 for the time, UTC for the zone);
 * 3. Pass it to one or more `get()` calls (fields of the *same* value
 *    accumulate across multiple calls);
 * 4. Call a conversion operator to extract the result;
 * 5. Call `reset()` before parsing a *different* time value.
 *
 * @tparam CharT    The character type.
 * @tparam HaveDate Activates date parsing when `true` (default `true`).
 * @tparam HaveTime Activates time parsing when `true` (default `true`).
 * @tparam TzLevel  The activated timezone tier (default `tz_level::zone`).
 * @endif
 */
template <typename CharT, bool HaveDate = true, bool HaveTime = true,
          tz_level TzLevel = tz_level::zone>
struct time_parse_context
    : date_parse_helper<CharT, HaveDate>
    , time_parse_helper<HaveTime>
    , time_zone_parse_helper<TzLevel>
{
    /**
     * @lang{ZH}
     * @brief 把三个字段组各自的 `convert_to` 并入本类的同一个重载集。
     *
     * 没有这三条，`ctx.convert_to(x)` 是编译错误而非重载决议问题：同名成员分散在多个基类里
     * 会在**名字查找**阶段就歧义，而本类自己声明的那几个 `convert_to`（`zoned_time`、
     * `sys_time`、`local_time`、`std::tm`）又会把基类版本整体隐藏掉。两者都发生在比较
     * 参数类型之前。
     *
     * 字段组被关掉时，用到的是各 helper 的主模板，其中只有一个零参且已删除的 `convert_to`
     * 占位——它使这里的 `using` 始终合法，而自身永远不会被选中。
     * @endif
     *
     * @lang{EN}
     * @brief Merges each field group's `convert_to` into this class's single overload set.
     *
     * Without these three, `ctx.convert_to(x)` is a compile error rather than an overload
     * resolution question: a member name spread across several base classes is ambiguous at
     * **name lookup** time, and this class's own `convert_to` declarations (the `zoned_time`,
     * `sys_time`, `local_time` and `std::tm` ones) would otherwise hide every base version
     * outright. Both happen before parameter types are ever compared.
     *
     * When a field group is switched off, the helper's primary template is used instead, which
     * carries nothing but a zero-argument deleted `convert_to` placeholder -- enough to keep
     * these `using` declarations well-formed, and never selectable itself.
     * @endif
     */
    using date_parse_helper<CharT, HaveDate>::convert_to;
    using time_parse_helper<HaveTime>::convert_to;
    using time_zone_parse_helper<TzLevel>::convert_to;

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
     * @note "恢复到默认构造时的状态"包括**清除经 `set_hint()` 设置的回退值**；如需保留，
     *       请在 `reset()` 之后重新设置。
     * @endif
     *
     * @lang{EN}
     * @brief Clears all accumulated parse state, restoring the context to its
     *        default-constructed state.
     *
     * Call this before reusing one context to parse a *different* time value;
     * skip it to keep accumulating fields of the *same* value across multiple
     * `get()` calls.
     * @note "Restoring the default-constructed state" includes **discarding the fallbacks
     *       installed by `set_hint()`**; install them again after `reset()` if they are still
     *       wanted.
     * @endif
     */
    void reset() { *this = time_parse_context{}; }

    /**
     * @lang{ZH}
     * @brief 设置日期字段的回退值。仅当 `HaveDate` 为 `true` 时可用。
     *
     * 转发到 `date_parse_helper::set_date_hint()`，语义与该函数完全一致，包括"必须在
     * `get()` 之前调用"这一约束。日期未激活时本重载不在候选集中，传日期 hint 是编译错误
     * 而非静默丢弃。
     * @param hint 各日期字段的回退值。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the fallback for the date fields. Available only when `HaveDate` is `true`.
     *
     * Forwards to `date_parse_helper::set_date_hint()` and shares its semantics exactly,
     * including the "must be called before `get()`" constraint. When the date is not
     * activated this overload is not in the candidate set, so passing a date hint is a
     * compile error rather than being silently discarded.
     * @param hint The fallback value for each date field.
     * @endif
     */
    void set_hint(const std::chrono::year_month_day& hint)
        requires(HaveDate)
    {
        this->set_date_hint(hint);
    }

    /**
     * @lang{ZH}
     * @brief 设置时间字段的回退值。仅当 `HaveTime` 为 `true` 时可用。
     *
     * 转发到 `time_parse_helper::set_time_hint()`，语义与该函数完全一致，包括 24 小时取模、
     * 亚秒精度按向零取整丢弃，以及"必须在 `get()` 之前调用"。
     * @tparam TDur @p hint 的时长精度，可为任意精度。
     * @param hint 各时间字段的回退值。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the fallback for the time fields. Available only when `HaveTime` is `true`.
     *
     * Forwards to `time_parse_helper::set_time_hint()` and shares its semantics exactly,
     * including the modulo-24-hour reduction, the truncation of sub-second precision toward
     * zero, and the "must be called before `get()`" constraint.
     * @tparam TDur The duration precision of @p hint; any precision is accepted.
     * @param hint The fallback value for each time field.
     * @endif
     */
    template <typename TDur>
    void set_hint(const std::chrono::hh_mm_ss<TDur>& hint)
        requires(HaveTime)
    {
        this->set_time_hint(hint);
    }

    /**
     * @lang{ZH}
     * @brief 设置 UTC 偏移的回退值。`TzLevel` 至少为 `tz_level::offset` 时可用。
     *
     * 转发到 `time_zone_parse_helper::set_offset_hint()`。该 hint 是转换时兜底而非预置解析
     * 字段，因此在 `get()` 之前或之后调用都可以；详见被转发的函数。
     * @param hint 回退偏移。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the fallback for the UTC offset. Available when `TzLevel` is at least
     *        `tz_level::offset`.
     *
     * Forwards to `time_zone_parse_helper::set_offset_hint()`. The hint is a conversion-time
     * fallback rather than a pre-seeded parse field, so it may be called either before or
     * after `get()`; see the forwarded-to function.
     * @param hint The fallback offset.
     * @endif
     */
    void set_hint(std::chrono::minutes hint)
        requires(TzLevel >= tz_level::offset)
    {
        this->set_offset_hint(hint);
    }

    /**
     * @lang{ZH}
     * @brief 设置时区的回退值。仅当 `TzLevel` 为 `tz_level::zone` 时可用。
     *
     * 转发到 `time_zone_parse_helper::set_time_zone_hint()`。注意该 hint 是转换时兜底而非
     * 预置解析字段，因此在 `get()` 之前或之后调用都可以；详见被转发的函数。
     * @param hint 回退时区；传 `nullptr` 恢复为默认的 UTC 兜底。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the fallback for the time zone. Available only when `TzLevel` is
     *        `tz_level::zone`.
     *
     * Forwards to `time_zone_parse_helper::set_time_zone_hint()`. Note that this hint is a
     * conversion-time fallback rather than a pre-seeded parse field, so it may be called
     * either before or after `get()`; see the forwarded-to function.
     * @param hint The fallback zone; `nullptr` restores the default UTC fallback.
     * @endif
     */
    void set_hint(const std::chrono::time_zone* hint)
        requires(TzLevel == tz_level::zone)
    {
        this->set_time_zone_hint(hint);
    }

    /**
     * @lang{ZH}
     * @brief 将已累积的日期、时间和时区字段转换为 `std::chrono::zoned_time<seconds>`，
     *        写入 @p out。
     *
     * 仅当 `HaveDate`、`HaveTime` 均为 `true` 且 `TzLevel` 为 `tz_level::zone` 时可用。
     *
     * 解析到 `%z` 偏移时，**偏移定瞬间、区域定展示**：瞬间取 `本地时间 - 偏移`，得到的
     * `sys_time` 与区域一起构造 `zoned_time`——这个构造函数是全函数，夏令时折返小时不再
     * 是歧义。此时若区域身份也来自输入（即解析到了完整 IANA 名），两个说法会被对质，
     * 矛盾即拒绝；来自 hint 或 UTC 兜底的区域是猜测而非输入的断言，不参与对质。
     *
     * 未解析到偏移时，退回原有行为：用本地时间构造，折返 / 空洞由 `<chrono>` 抛出。
     * @param out 接收还原出的带时区时间点；抛出时不被写入。
     * @throw stream_error 若日期无效、时区解析失败，或偏移与已解析的区域矛盾。
     * @endif
     *
     * @lang{EN}
     * @brief Converts the accumulated date, time, and timezone fields to a
     *        `std::chrono::zoned_time<seconds>` in @p out.
     *
     * Available only when `HaveDate` and `HaveTime` are `true` and `TzLevel` is
     * `tz_level::zone`.
     *
     * Once a `%z` offset has been parsed, **the offset pins the instant and the zone decides
     * the rendering**: the instant is `local time - offset`, and that `sys_time` is paired
     * with the zone to build the `zoned_time` -- that constructor is total, so a DST fold hour
     * is no longer ambiguous. If the zone identity also came from the input (a full IANA name
     * was parsed), the two claims are cross-examined and a contradiction is rejected; a zone
     * that came from the hint or the UTC fallback is a guess rather than an assertion made by
     * the input, and takes no part in that.
     *
     * Without a parsed offset the previous behaviour stands: the local time is used to
     * construct, and `<chrono>` throws on a fold or a gap.
     * @param out Receives the reconstructed zoned time point; left untouched if this throws.
     * @throw stream_error If the date is invalid, the timezone lookup fails, or the offset
     *        contradicts the parsed zone.
     * @endif
     */
    void convert_to(std::chrono::zoned_time<std::chrono::seconds>& out) const
        requires(HaveDate && HaveTime && TzLevel == tz_level::zone)
    {
        using namespace std::chrono;
        year_month_day ymd{};
        hh_mm_ss<seconds> hms{};
        const time_zone* tz = nullptr;
        convert_to(ymd);
        convert_to(hms);
        convert_to(tz);

        local_time<seconds> lt{ local_days{ymd} + hms.to_duration() };
        if (!this->m_have_offset)
        {
            out = zoned_time<seconds>{tz, lt};
            return;
        }

        sys_time<seconds> st{ lt.time_since_epoch() - this->m_offset };
        if (this->m_zone_name)
        {
            const minutes actual = duration_cast<minutes>(tz->get_info(st).offset);
            if (actual != this->m_offset)
                throw stream_error(
                    "timeio get error: UTC offset of "
                    + std::to_string(this->m_offset.count())
                    + " minutes contradicts time zone '" + this->m_zone_name + "', which is "
                    + std::to_string(actual.count()) + " minutes at that instant");
        }
        out = zoned_time<seconds>{tz, st};
    }

    /**
     * @lang{ZH}
     * @brief 将已累积的日期、时间和时区字段转换为 `std::chrono::sys_time<seconds>`，
     *        写入 @p out。
     *
     * 仅当 `HaveDate`、`HaveTime` 均为 `true` 且 `TzLevel` 至少为 `tz_level::offset`
     * 时可用。取值顺序按「解析到的数据优先于回退值」排列：
     * 1. 解析到的 `%z` 偏移；
     * 2. 2 档且解析到完整 IANA 名：用该区域把本地时间换算为瞬间；
     * 3. `set_hint(minutes)` 设的回退偏移；
     * 4. 2 档：hint 区域，再不然 UTC。
     *
     * 走第 2 / 4 条时用的是本地时间，因此夏令时折返 / 空洞仍由 `<chrono>` 抛出；
     * 走第 1 / 3 条时偏移已把瞬间钉死，不会有歧义。
     * @param out 接收还原出的时间点；抛出时不被写入。
     * @throw stream_error 若日期无效、时区解析失败，或 1 档下既无偏移也无回退偏移。
     * @endif
     *
     * @lang{EN}
     * @brief Converts the accumulated date, time, and timezone fields to a
     *        `std::chrono::sys_time<seconds>` in @p out.
     *
     * Available only when `HaveDate` and `HaveTime` are `true` and `TzLevel` is at least
     * `tz_level::offset`. The order puts parsed data ahead of fallbacks:
     * 1. the parsed `%z` offset;
     * 2. at `tz_level::zone` with a full IANA name parsed, that zone converts the local time
     *    to an instant;
     * 3. the fallback offset installed by `set_hint(minutes)`;
     * 4. at `tz_level::zone`, the hint zone, and failing that UTC.
     *
     * Steps 2 and 4 go through the local time, so `<chrono>` still throws on a DST fold or
     * gap; steps 1 and 3 have the instant pinned by an offset and cannot be ambiguous.
     * @param out Receives the reconstructed time point; left untouched if this throws.
     * @throw stream_error If the date is invalid, the timezone lookup fails, or
     *        `tz_level::offset` was requested with neither a parsed nor a fallback offset.
     * @endif
     */
    void convert_to(std::chrono::sys_time<std::chrono::seconds>& out) const
        requires(HaveDate && HaveTime && TzLevel >= tz_level::offset)
    {
        using namespace std::chrono;
        year_month_day ymd{};
        hh_mm_ss<seconds> hms{};
        convert_to(ymd);
        convert_to(hms);
        local_time<seconds> lt{ local_days{ymd} + hms.to_duration() };

        if (!this->m_have_offset)
        {
            if constexpr (TzLevel == tz_level::zone)
            {
                if (this->m_zone_name || !this->m_have_offset_hint)
                {
                    const time_zone* tz = nullptr;
                    convert_to(tz);
                    out = tz->to_sys(lt);
                    return;
                }
            }
        }

        minutes offset{};
        convert_to(offset);
        out = sys_time<seconds>{ lt.time_since_epoch() - offset };
    }

    /**
     * @lang{ZH}
     * @brief 将已累积的日期和时间字段转换为 `std::chrono::local_time<seconds>`，写入 @p out。
     *
     * 仅当 `HaveDate` 和 `HaveTime` 均为 `true` 时可用；不依赖任何时区档，因为
     * `local_time` 表达的就是不带时区的墙上时间。**解析到的偏移与时区被直接丢弃**——
     * 输入里的 `%z` / `%Z` 说的是这个墙上时间属于哪个区，而 `local_time` 装不下这个信息。
     * 要保留它，请转换到 `sys_time` 或 `zoned_time`。
     * @param out 接收还原出的本地时间；抛出时不被写入。
     * @throw stream_error 若日期无效。
     * @endif
     *
     * @lang{EN}
     * @brief Converts the accumulated date and time fields to a
     *        `std::chrono::local_time<seconds>` in @p out.
     *
     * Available whenever `HaveDate` and `HaveTime` are `true`, at any timezone tier: a
     * `local_time` is by definition a wall time with no zone attached. **A parsed offset or
     * zone is simply discarded** -- `%z` / `%Z` in the input say which zone that wall time
     * belongs to, and a `local_time` has nowhere to keep that. Convert to `sys_time` or
     * `zoned_time` to retain it.
     * @param out Receives the reconstructed local time; left untouched if this throws.
     * @throw stream_error If the date is invalid.
     * @endif
     */
    void convert_to(std::chrono::local_time<std::chrono::seconds>& out) const
        requires(HaveDate && HaveTime)
    {
        using namespace std::chrono;
        year_month_day ymd{};
        hh_mm_ss<seconds> hms{};
        convert_to(ymd);
        convert_to(hms);
        out = local_time<seconds>{ local_days{ymd} + hms.to_duration() };
    }

    /**
     * @lang{ZH}
     * @brief 将已累积的日期和时间字段写入 @p out。
     *
     * 仅当 `HaveDate` 和 `HaveTime` 均为 `true` 时可用。
     *
     * **只覆盖它真正重建出来的字段**：`tm_year` / `tm_mon` / `tm_mday` / `tm_hour` /
     * `tm_min` / `tm_sec` / `tm_wday` / `tm_yday` / `tm_isdst` 这九项总是写；`tm_gmtoff`
     * 只在三个条件同时成立时写——`TzLevel` 至少为 `tz_level::offset`、确实解析到了 `%z`、
     * 且当前平台的 `std::tm` 带这个成员（用 `requires` 表达式探测，不看平台宏）。@p out 的
     * 其余成员保持原值。整体覆盖会把 `tm_gmtoff`、`tm_zone` 清成 `0` 与 `nullptr`，等于把调用
     * 方结构体里一个有效指针改成空指针。逐字段写入也与 `std::time_get::get` 的行为一致
     * （后者同样只写它解析到的字段）。
     *
     * `tm_zone` 的条件与 `tm_gmtoff` 平行：`TzLevel` 至少为 `tz_level::offset`、平台的
     * `std::tm` 带这个成员、且 `%Z` 确实解析到了东西。写进去的是**解析到的原文**——缩写
     * 优先，没有缩写才退到区名。`tm_zone` 按定义装的就是缩写（`localtime()` 放进去的是
     * `CST` 这种），而区名之所以会走到这个字段，本就是因为 put 侧先从这里写出去过。原文
     * 不做规范化：`%Z` 读到 `EST` 就写回 `EST`，不写 `America/Panama`。
     *
     * 解析到 @ref base_ft<timeio>::s_unknown_zone 时写**空串**，而不是跳过。这是往返闭合
     * 所必需的：put 侧对空的 `tm_zone` 写出该记号，若 get 侧读到它却什么都不做，调用方
     * `tm` 里上一次留下的区名就会残留，下一次 put 写出的是那个陈旧的名字，与文本不符。
     * 「`%Z` 没解析到」与「`%Z` 解析到了、且它明说没有时区」是两回事，只有前者不写。
     *
     * 指针指向时区前缀树内的存储，随程序始终有效——`tm_zone` 是 `const char*` 而 `std::tm`
     * 没有配套的释放接口，能填进去的只能是这种由库长期持有的内存，`localtime()` 指向 libc
     * 静态存储也是同一个道理。
     *
     * @note 这一点上本库比 `strptime` / `std::get_time` 做得多：那两者解析 `%Z` 时不动
     *       `tm_zone`。但它们的 put 对应物 `strftime` 是写 `%Z` 的，本库的 put 也写
     *       （见 `timeio::put`），只读不写会让写得出的东西读不回来。需要确定的偏移仍请用
     *       `%z`——`tm_zone` 里的缩写可以是有歧义的（`CST` 同时属于 America/Chicago、
     *       America/Havana、Asia/Harbin、Australia/Darwin 与 Australia/Adelaide），本类的
     *       `convert_to(const time_zone*&)` 遇到这种缩写照样抛 "ambiguous"。
     *
     * @param out 接收还原出的日期与时间；抛出时不被写入。
     * @throw stream_error 若日期无效。
     * @endif
     *
     * @lang{EN}
     * @brief Writes the accumulated date and time fields into @p out.
     *
     * Available only when both `HaveDate` and `HaveTime` are `true`.
     *
     * **Only the fields it actually reconstructs are overwritten.** These nine are always
     * written: `tm_year`, `tm_mon`, `tm_mday`, `tm_hour`, `tm_min`, `tm_sec`, `tm_wday`,
     * `tm_yday` and `tm_isdst`. `tm_gmtoff` is written only when all three of the following
     * hold -- `TzLevel` is at least `tz_level::offset`, a `%z` was in fact parsed, and this
     * platform's `std::tm` carries that member (detected with a `requires` expression rather
     * than a platform macro). Every other member of @p out keeps its value. Overwriting the
     * struct as a whole would clear `tm_gmtoff` and `tm_zone` to `0` and `nullptr`, turning a
     * valid pointer in the caller's struct into a null one. Assigning field by field also
     * matches `std::time_get::get`, which likewise writes only the fields it parsed.
     *
     * `tm_zone` follows conditions parallel to `tm_gmtoff`'s: `TzLevel` at least
     * `tz_level::offset`, the platform's `std::tm` carrying the member, and `%Z` having parsed
     * something. What goes in is the **text as parsed** -- the abbreviation first, falling back
     * to the zone name only when there is no abbreviation. `tm_zone` holds an abbreviation by
     * definition (`localtime()` puts things like `CST` there), and a zone name reaches this
     * field only because the put side wrote one out of it to begin with. The text is not
     * canonicalized: a `%Z` that read `EST` writes `EST` back, not `America/Panama`.
     *
     * Parsing @ref base_ft<timeio>::s_unknown_zone writes an **empty string** rather than
     * skipping the field. That is what closes the round trip: the put side emits that token for
     * an empty `tm_zone`, so if get read it back and did nothing, whatever zone name the
     * caller's `tm` already held would survive and the next put would emit that stale name
     * instead of what the text said. "No `%Z` was parsed" and "a `%Z` was parsed and it says
     * there is no zone" are different things; only the former leaves the field alone.
     *
     * The pointer refers to storage inside the time-zone trie and stays valid for the whole
     * program -- `tm_zone` is a `const char*` and `std::tm` has no matching release call, so
     * only memory owned long-term by the library can go in there, which is the same reason
     * `localtime()` points into libc's static storage.
     *
     * @note Here this library does more than `strptime` and `std::get_time`, which leave
     *       `tm_zone` alone when parsing `%Z`. But their put counterpart `strftime` does write
     *       `%Z`, and so does this library's put (see `timeio::put`); reading without writing
     *       would leave output this library can produce but cannot consume. For a definite
     *       offset still use `%z`: an abbreviation in `tm_zone` may well be ambiguous (`CST`
     *       belongs to America/Chicago, America/Havana, Asia/Harbin, Australia/Darwin and
     *       Australia/Adelaide alike), and this class's `convert_to(const time_zone*&)` throws
     *       "ambiguous" on such an abbreviation regardless.
     *
     * @param out Receives the reconstructed date and time; left untouched if this throws.
     * @throw stream_error If the date is invalid.
     * @endif
     */
    void convert_to(std::tm& out) const
        requires(HaveDate && HaveTime)
    {
        using namespace std::chrono;
        year_month_day ymd{};
        hh_mm_ss<seconds> hms{};
        convert_to(ymd);
        convert_to(hms);

        int d = static_cast<int>(static_cast<unsigned>(ymd.day()));
        int m = static_cast<int>(static_cast<unsigned>(ymd.month()));
        int y = int(ymd.year());

        out.tm_year = y - 1900;
        out.tm_mon  = m - 1;
        out.tm_mday = d;

        // Time fields
        out.tm_hour = int(hms.hours().count());
        out.tm_min  = int(hms.minutes().count());
        out.tm_sec  = int(hms.seconds().count());

        out.tm_isdst = -1;   // let the C library figure out DST

        out.tm_wday = static_cast<int>(weekday{sys_days{ymd}}.c_encoding());
        bool isLeap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);

        static constexpr std::array<int, 12> days = {-1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333};
        out.tm_yday = days[out.tm_mon] + out.tm_mday + (isLeap && out.tm_mon >= 2 ? 1 : 0);

        if constexpr ((TzLevel >= tz_level::offset) && (requires { out.tm_gmtoff; }))
        {
            if (this->m_have_offset)
                out.tm_gmtoff = static_cast<decltype(out.tm_gmtoff)>(
                    seconds{this->m_offset}.count());
        }

        if constexpr ((TzLevel == tz_level::zone) && (requires { out.tm_zone; }))
        {
            const char* zone = this->m_zone_abbrev ? this->m_zone_abbrev : this->m_zone_name;
            if (zone) out.tm_zone = const_cast<decltype(out.tm_zone)>(zone);
        }
    }
};

/**
 * @lang{ZH}
 * @brief 某个时间值类型能供出哪几类字段——`timeio::expand_format` 据此裁剪格式串。
 *
 * 每种类型一个特化，四个标志位对应 `timeio::put` 传给 `do_put` 的那几个指针：日期
 * （`ymd` 与随之而来的 `wd`）、时分秒（`hms`）、UTC 偏移（`zi`）、区域身份（`tz` 或非空的
 * `zi->abbrev`）。`put` 对供不出的说明符原样退化，本表把同一件事提前到编译期，好让
 * `expand_format` 不必真去格式化一遍就知道该摘掉谁。
 *
 * 主模板**不定义**：不在 `put` 支持范围内的类型在此报错，而不是悄悄按「什么都供不出」处理。
 *
 * @note 星期不单独占一位：`put` 只要给得出 `ymd` 就一并给得出 `wd`，两者从不分家。
 * @note 本表的 `has_offset` / `has_zone` 与 get 侧的 @ref tz_level 是同一个判据的两侧：
 *       两者皆假 → `tz_level::none`，只有 `has_offset` → `tz_level::offset`，两者皆真 →
 *       `tz_level::zone`。put 摘掉的说明符，get 恰好按字面量匹配，往返因此闭合。
 * @tparam TVal 时间值类型。
 * @endif
 *
 * @lang{EN}
 * @brief Which groups of fields a time value type can supply -- what `timeio::expand_format`
 *        trims a format string against.
 *
 * One specialization per type, its four flags matching the pointers `timeio::put` hands to
 * `do_put`: date (`ymd`, and the `wd` that comes with it), time of day (`hms`), UTC offset
 * (`zi`), zone identity (`tz`, or a non-empty `zi->abbrev`). `put` degrades a specifier it
 * cannot supply; this table moves that same decision to compile time, so `expand_format` knows
 * what to remove without formatting anything.
 *
 * The primary template is **left undefined**: a type outside what `put` accepts is an error
 * here rather than being quietly treated as supplying nothing.
 *
 * @note The weekday gets no flag of its own: whenever `put` can supply `ymd` it supplies `wd`
 *       as well, and the two never come apart.
 * @note This table's `has_offset` / `has_zone` and the get side's @ref tz_level are two views
 *       of one test: neither flag means `tz_level::none`, `has_offset` alone means
 *       `tz_level::offset`, and both mean `tz_level::zone`. A specifier put trims is exactly
 *       one get matches literally, which is what closes the round trip.
 * @tparam TVal The time value type.
 * @endif
 */
template <typename TVal>
struct time_value_fields;

template <typename Duration, typename TimeZonePtr>
struct time_value_fields<std::chrono::zoned_time<Duration, TimeZonePtr>>
{
    static constexpr bool has_date = true;
    static constexpr bool has_time = true;
    static constexpr bool has_offset = true;
    static constexpr bool has_zone = true;
};

template <typename Duration>
struct time_value_fields<std::chrono::sys_time<Duration>>
{
    static constexpr bool has_date = true;
    static constexpr bool has_time = true;
    static constexpr bool has_offset = true;
    static constexpr bool has_zone = true;
};

template <typename Duration>
struct time_value_fields<std::chrono::local_time<Duration>>
{
    static constexpr bool has_date = true;
    static constexpr bool has_time = true;
    static constexpr bool has_offset = true;
    static constexpr bool has_zone = false;
};

template <>
struct time_value_fields<std::chrono::year_month_day>
{
    static constexpr bool has_date = true;
    static constexpr bool has_time = false;
    static constexpr bool has_offset = false;
    static constexpr bool has_zone = false;
};

template <typename TDuration>
struct time_value_fields<std::chrono::hh_mm_ss<TDuration>>
{
    static constexpr bool has_date = false;
    static constexpr bool has_time = true;
    static constexpr bool has_offset = false;
    static constexpr bool has_zone = false;
};

template <>
struct time_value_fields<std::tm>
{
    static constexpr bool has_date = true;
    static constexpr bool has_time = true;
    static constexpr bool has_offset = requires(const std::tm& t) { t.tm_gmtoff; };
    static constexpr bool has_zone = has_offset && requires(const std::tm& t) { t.tm_zone; };
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
 * `std::chrono::hh_mm_ss`、`std::chrono::zoned_time`、`std::chrono::sys_time`、
 * `std::chrono::local_time` 加一个偏移，或 `std::tm`，
 * 将其按指定格式串写入输出迭代器。
 *
 * **解析**（`get`）：从输入迭代器按指定格式串解析时间字段，
 * 将结果累积到 `time_parse_context` 中；解析完成后通过转换运算符
 * 提取 `year_month_day`、`hh_mm_ss`、`local_time`、`sys_time` 或 `zoned_time`。
 *
 * **说明符与值不匹配时退化为字面量。** 每种值类型只携带一部分信息：`year_month_day`
 * 只有日期，`hh_mm_ss` 只有时间。时区则分三档（见 `tz_level`）：`year_month_day` 与
 * `hh_mm_ss` 一档都没有；`std::tm`、`sys_time`、`local_time` 只带得动偏移和缩写；
 * 只有 `zoned_time` 带得动区域身份。当格式串里出现该值无法提供的说明符时，它不构成错误，
 * 而是被当作字面量处理——`put` 原样写出 `%` + 修饰符 + 说明符字符，`get` 则要求输入中出现
 * 同样的字面量。退化这一步两侧对称，因此**因退化而写出的字面量**总能被同一格式串的
 * `get` 读回：
 *
 * | 值类型 | 时区档 | 格式串 | `put` 输出 | `get` 接受的输入 |
 * |---|---|---|---|---|
 * | `year_month_day` | 无 | `%Y-%m-%d` | `2020-05-17` | `2020-05-17` |
 * | `year_month_day` | 无 | `%H:%M`    | `%H:%M`      | 字面量 `%H:%M` |
 * | `hh_mm_ss`       | 无 | `%Y-%m-%d` | `%Y-%m-%d`   | 字面量 `%Y-%m-%d` |
 * | `year_month_day`、`hh_mm_ss` | 无 | `%z`、`%Z` | `%z`、`%Z` | 字面量 `%z`、`%Z` |
 * | `std::tm`        | 区域 | `%z` | 由 `tm_gmtoff` 算得，如 `+0800`；钳到 `±23:59:59` | `+0800` |
 * | `std::tm`        | 区域 | `%Z` | `tm_zone`；为空则写 `UNKNOWN` | 时区名或缩写、`UNKNOWN` |
 * | `sys_time`       | 偏移 | `%z`、`%Z` | `+0000`、`UTC` | 同上 |
 * | `local_time` + 偏移 | 偏移 | `%z`、`%Z` | `±hhmm`、退化为 `%Z` | 同上 |
 * | `zoned_time`     | 区域 | `%z`、`%Z` | `+0800`、`Asia/Shanghai` | 时区名，不收字面量 |
 *
 * `get` 侧的档位由 `time_parse_context` 的 `TzLevel` 模板参数给出，对应上表"时区档"一列：
 * 低于该档的说明符退化为字面量，达到该档的说明符真的解析。`std::tm` 那两行填的是本平台的档：
 * 它随两个扩展成员的有无而定，两个都有是"区域"，只有 `tm_gmtoff` 是"偏移"，都没有是"无"。
 *
 * 能否供出 `%Z` 只看值类型，不看运行期取值：带 `tm_zone` 成员的平台上，`std::tm` 的 `%Z`
 * 对**任何**取值都写得出内容——有名字写名字，`tm_zone` 为空指针或空串则写
 * @ref base_ft<timeio>::s_unknown_zone（`UNKNOWN`）。该记号也在时区前缀树里注册，
 * 因此写得出就读得回。真正没有时区概念的类型（`local_time`、`year_month_day`、
 * `hh_mm_ss`）的 `%Z` 才退化为字面量。
 *
 * 为使退化仍然对称，`tz_level::offset` 档的 `%Z` 在时区数据库里匹配不到时会退回"要求输入中
 * 出现字面量 `%Z`"，于是 put 退化写出的内容照样读得回；这条退路只在输入真是那两个字符时
 * 才走得通，而那正是 put 没有时区可写的情形，因此不会让任何本该记录的时区丢掉。
 * `tz_level::zone` 档不这样做：那一档的
 * `%Z` 决定用哪个时区，且 put 在该档从不退化，故匹配不到即失败。
 *
 * @note 解析到的区名、缩写与 `UNKNOWN` 都会写回 `std::tm::tm_zone`（`UNKNOWN` 写空串），
 *   于是 `%Z` 在 `std::tm` 上也是往返闭合的。细节与理由见 `convert_to(std::tm&)`。
 *
 * @warning 上面那句"读得回来"只管**退化**这条规则，不是说两侧取值域处处相同。已知的例外是
 *   **年份**：`put` 为与 `std::format` 一致，负年份带 `-`、大于 9999 的年份写四位以上；而
 *   `%Y` / `%G` / `%C` 的解析侧**只收 0..9999 且不带符号**（与
 *   `std::chrono::from_stream("%Y")` 和 POSIX `strptime` 一致）。于是年 10000、32767、
 *   −32767 都是 `put` 成功、同格式串 `get` 得 `strfailbit`（`*tmb` 一字节不改），年 0..9999
 *   才真的往返。这是有意的取舍，理由见 `do_get` 里 `%Y` 分支上的整段注释。
 *
 * @warning 时区上与 `std::get_time` / `std::put_time` 的分歧在**取值域**，不在退化：本库的
 *   `%Z` 接受时区数据库认识的任何名字或缩写；libstdc++ 比对的是一张硬编码 14 条的表，
 *   只认标准时缩写。于是 `get_time(&tm, "%Y-%m-%d %Z")` 读 `2020-05-17 PST` 两边都成功，
 *   读 `2020-05-17 UTC`、`2020-05-17 PDT` 或 `2020-05-17 America/Los_Angeles` 只有本库成功，
 *   读 `2020-05-17 XYZ` 两边都失败。作为交换，本库的失败是原子的：整次提取失败时目标对象
 *   完全不被改写，而 `std` 会留下已写入一半的 `std::tm`。
 *
 * @note 格式串来自 locale 数据库（`nl_langinfo`），其中的自引用（如 `D_T_FMT == "%c"`）
 *   在构造时即被拒绝：构造函数检查这些复合格式串构成的图是否有环，有环则抛
 *   `std::runtime_error`。因此 `put` / `get` / `expand_format` 的递归展开总是有界的。
 *   参见 `validate_format_recursion`。
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
 * `std::chrono::hh_mm_ss`, `std::chrono::zoned_time`, `std::chrono::sys_time`,
 * a `std::chrono::local_time` plus an offset, or `std::tm`, and writes the
 * result to an output iterator according to the given format string.
 *
 * **Parsing** (`get`): parses time fields from an input iterator according to
 * a format string, accumulating results into a `time_parse_context`; after
 * parsing, a conversion operator on the context extracts a `year_month_day`,
 * `hh_mm_ss`, `local_time`, `sys_time`, or `zoned_time`.
 *
 * **A specifier the value cannot supply degrades to a literal.** Each value type carries only
 * part of the information: `year_month_day` has the date and `hh_mm_ss` the time. The time zone
 * comes in three tiers (see `tz_level`): `year_month_day` and `hh_mm_ss` carry none of it;
 * `std::tm`, `sys_time` and `local_time` carry an offset and an abbreviation; only `zoned_time`
 * carries a zone identity. A specifier the value cannot supply is not an error; it is treated as
 * a literal -- `put` writes out the `%`, the modifier and the specifier character unchanged, and
 * `get` requires that same literal in the input. The degradation itself is symmetric, so a
 * **literal produced by degradation** always reads back through `get` with the same format
 * string:
 *
 * | Value type | Tier | Format | `put` writes | `get` accepts |
 * |---|---|---|---|---|
 * | `year_month_day` | none | `%Y-%m-%d` | `2020-05-17` | `2020-05-17` |
 * | `year_month_day` | none | `%H:%M`    | `%H:%M`      | the literal `%H:%M` |
 * | `hh_mm_ss`       | none | `%Y-%m-%d` | `%Y-%m-%d`   | the literal `%Y-%m-%d` |
 * | `year_month_day`, `hh_mm_ss` | none | `%z`, `%Z` | `%z`, `%Z` | the literals `%z`, `%Z` |
 * | `std::tm`        | zone | `%z` | computed from `tm_gmtoff`, e.g. `+0800`; clamped to `±23:59:59` | `+0800` |
 * | `std::tm`        | zone | `%Z` | `tm_zone`, or `UNKNOWN` when empty | a zone name or abbreviation, `UNKNOWN` |
 * | `sys_time`       | offset | `%z`, `%Z` | `+0000`, `UTC` | as above |
 * | `local_time` + offset | offset | `%z`, `%Z` | `±hhmm`, degrades to `%Z` | as above |
 * | `zoned_time`     | zone | `%z`, `%Z` | `+0800`, `Asia/Shanghai` | a zone name; no literal |
 *
 * On the `get` side the tier is the `TzLevel` template argument of `time_parse_context`, matching
 * the "Tier" column above: a specifier needing more than that tier degrades to a literal, and one
 * the tier can hold really parses. The two `std::tm` rows carry this platform's tier; it follows
 * the two extension members -- zone with both, offset with `tm_gmtoff` alone, none with neither.
 *
 * Whether `%Z` can be supplied depends on the value's type, not on its run-time value: on a
 * platform whose `std::tm` carries `tm_zone`, `%Z` produces content for **every** `std::tm` -- the
 * name when there is one, and @ref base_ft<timeio>::s_unknown_zone (`UNKNOWN`) when
 * `tm_zone` is null or empty. That token is registered in the time-zone trie as well, so whatever
 * can be written can be read back. Only the types with no notion of a
 * zone at all (`local_time`, `year_month_day`, `hh_mm_ss`) degrade `%Z` to a literal.
 *
 * To keep that degradation symmetric, a `%Z` at `tz_level::offset` that matches nothing in the
 * time-zone database falls back to requiring the literal `%Z` in the input, so whatever `put`
 * degraded to still reads back; `%Z` supplies nothing to any `convert_to` at that tier, so widening
 * it changes no conversion result. `tz_level::zone` does not do this: there `%Z` chooses the zone,
 * and `put` never degrades it, so failing to match is an error.
 *
 * @note A parsed zone name, abbreviation or `UNKNOWN` alike is written back into
 *   `std::tm::tm_zone` (`UNKNOWN` as an empty string), so `%Z` round-trips on a `std::tm` too.
 *   See `convert_to(std::tm&)` for the details and the reasoning.
 *
 * @warning That "reads back" covers the **degradation** rule only; it does not claim that the two
 *   sides accept the same values everywhere. The known exception is the **year**: to stay
 *   consistent with `std::format`, `put` emits a leading `-` for negative years and more than
 *   four digits for years past 9999, while the parse side of `%Y` / `%G` / `%C` accepts **only
 *   0..9999, unsigned** (matching `std::chrono::from_stream("%Y")` and POSIX `strptime`). Years
 *   10000, 32767 and -32767 therefore `put` successfully and then fail `get` on the same format
 *   string with `strfailbit`, leaving `*tmb` untouched; only years 0..9999 really round-trip.
 *   The trade-off is deliberate; see the block comment on the `%Y` case in `do_get`.
 *
 * @warning On the time zone the divergence from `std::get_time` / `std::put_time` is one of
 *   **accepted values**, not of degradation: `%Z` here takes any name or abbreviation the
 *   time-zone database knows, where libstdc++ matches against a hard-coded table of 14 entries
 *   and so knows standard-time abbreviations only. `get_time(&tm, "%Y-%m-%d %Z")` reading
 *   `2020-05-17 PST` therefore succeeds on both sides; `2020-05-17 UTC`, `2020-05-17 PDT` and
 *   `2020-05-17 America/Los_Angeles` succeed only here; `2020-05-17 XYZ` fails on both. In
 *   exchange the failure here is atomic: a failed extraction leaves the target completely
 *   unmodified, whereas `std` leaves a half-written `std::tm` behind.
 *
 * @note Format strings sourced from the locale database (`nl_langinfo`) are checked for
 *   self-reference at construction (e.g. `D_T_FMT == "%c"`): the constructor rejects a
 *   cycle among the compound format strings with `std::runtime_error`. The recursive
 *   expansion in `put` / `get` / `expand_format` is therefore always bounded. See
 *   `validate_format_recursion`.
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
     * 验证名称表的唯一性与复合格式串的非自引用性，并构建用于高效解析的前缀树
     * （星期、月份、AM/PM、纪元名称、替代数字）。若未提供 ctype，空白字符识别
     * 使用基础 ASCII 判断。
     * @tparam TConfPtr 指向 `timeio_conf<CharT>` 的 `shared_ptr` 类型。
     * @param p_obj locale 配置对象（不得为空）。
     * @throw std::runtime_error 若指针为空、locale 名称表存在重复/空条目，或复合
     *        格式串自引用（见 `validate_format_recursion`）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that initializes from a locale configuration.
     *
     * Copies all locale data from `timeio_conf<CharT>` (names, format strings,
     * era entries, etc.), validates the name tables for uniqueness and the compound
     * format strings for self-reference, and builds prefix tries for efficient
     * parsing (weekday, month, AM/PM, era names, alternative digits). If no ctype is
     * provided, whitespace recognition uses basic ASCII comparison.
     * @tparam TConfPtr A `shared_ptr` type pointing to `timeio_conf<CharT>`.
     * @param p_obj The locale configuration object (must not be null).
     * @throw std::runtime_error If the pointer is null, the locale name tables
     *        contain duplicates or empty entries, or a compound format string is
     *        self-referential (see `validate_format_recursion`).
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
        m_date_time_format = p_obj->date_time_format();
        m_era_date_time_format = p_obj->era_date_time_format();
        m_am_pm_format = p_obj->am_pm_format();
        m_era_master = p_obj->era_items();

        // Reject malformed locale data here, with one clear error, rather than as a
        // prefix_tree "duplicate items" throw or a silent parse mis-match later.
        // An empty AM/PM is legal (de_DE, fr_FR, ru_RU) and just leaves %p unmatched.
        check_unique_nonempty<7>(m_day, m_abbr_day, "day");
        check_unique_nonempty<12>(m_month, m_abbr_month, "month");
        if (!m_am.empty() && m_am == m_pm)
            throw std::runtime_error("timeio: AM and PM designators are identical in locale data");
        validate_format_recursion();

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
     * @brief 返回某个类型的值在某个格式串下**实际会写出**的那个格式串：复合说明符逐层展开，
     *        该类型供不出的说明符连同一侧分隔符摘掉。
     *
     * 两件事一起做，因为它们回答的是同一个问题——「我写下这个格式串，到底会得到什么」。
     *
     * **展开**：`%c` / `%x` / `%X` / `%r`（含 `%Ec` / `%Ex` / `%EX`）代表的是 locale 数据里的
     * 一串格式，写下它们的人不知道自己实际会得到哪些字段；固定复合 `%D` / `%F` / `%T` / `%R`
     * 同理。本函数把它们替换为各自的内容并继续展开，直到只剩基本说明符。
     *
     * **过滤**：本库对值供不出的说明符采取**退化**策略——原样写出 `%` 和说明符字符，put 与
     * get 两侧对称（见类说明）。退化不置失败位，所以给 `year_month_day` 写 `%c` 会得到一串
     * `%H:%M:%S`，给 `local_time` 写 `%c` 会在 en_US 得到一个字面 `%Z`，而这些都要等看见输出
     * 才发现。本函数只看 `TVal` 这个**类型**：它决定了能供出哪几类字段——日期、星期、时分秒、
     * UTC 偏移、区域身份，见 @ref time_value_fields——正如它决定了 `put` / `get` 走哪一条路。
     * 逐个说明符按 put 的同一张表判断，供不出的摘掉；不在本库支持集内的说明符（如 `%Q`）
     * 以及非法的修饰符组合（如 `%Oc`）落在同一条规则里。既然只用得上类型，本函数就不收值参数，
     * 手上没有现成对象时也能调。
     *
     * 摘的时候连一侧分隔符一起摘，否则留下孤立标点：`"%T %Z"` 摘成 `"%T"` 而不是 `"%T "`，
     * `"%T (%Z)"` 摘成 `"%T"` 而不是 `"%T ()"`。规则是：说明符前面有分隔符就摘前面的，
     * 只有它位于串首时才摘后面的；分隔符指**空白，加至多一个 ASCII 标点**；摘掉的标点是
     * 左括号（`(` `[` `{`）时，紧跟说明符的那个右括号一并摘掉。非 ASCII 字符一律不当分隔符
     * ——`"%S秒 %Z"` 只摘那个空格，不会把「秒」当分隔符吃掉，代价是 `"%r፡%Z"` 这类用多字节
     * 标点分隔的会留下一个 `፡`。
     *
     * ```cpp
     * // en_US 的 %c 是 "%a %d %b %Y %r %Z"，%r 又是 "%I:%M:%S %p"
     * using T = std::chrono::local_time<std::chrono::seconds>;
     * auto fmt = obj.expand_format<T>("%c");  // "%a %d %b %Y %I:%M:%S %p"，%Z 已摘掉
     * obj.put(out, t, offset, fmt);
     * ```
     *
     * @note 判据是类型，不是值，而 put 侧与之严丝合缝：`std::tm` 只要平台上带 `tm_zone` 字段
     *       就算供得出 `%Z`，而 put 对这样的 `tm` 也确实总写得出内容——`tm_zone` 为空时写
     *       @ref base_ft<timeio>::s_unknown_zone。因此保留下来的说明符 put 一定供得出，
     *       没有"表说供得出、运行期却退化"的缝隙。
     * @note `%EY` **不展开**：纪元格式取决于年份落在哪个纪元，是值相关的，静态展不开。它照样
     *       参与过滤，供不出就摘掉。
     * @note 供不出的复合说明符整个摘掉，不展开：给 `hh_mm_ss` 写 `%c`，得到的是空串，而不是
     *       一串摘剩下的标点。
     * @note 落单的 `%`（其后没有说明符）原样保留，与 put 的处理一致。
     * @note 自引用的复合格式串（如 `D_T_FMT` 里含 `%c`）在构造时就被拒绝，所以这里的展开
     *       不会无限递归。
     *
     * @tparam TVal 时间值类型（`year_month_day`、`hh_mm_ss`、`sys_time`、`local_time`、
     *              `zoned_time` 或 `std::tm`），与 `put` 接受的相同。
     * @param fmt 格式串。
     * @return 展开并过滤后的格式串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the format string a value of a given type will **actually** produce for a
     *        given format: compound specifiers expanded, and specifiers the type cannot supply
     *        removed together with one adjacent separator.
     *
     * Both halves answer the same question -- what do I really get if I write this format?
     *
     * **Expansion**: `%c` / `%x` / `%X` / `%r` (and `%Ec` / `%Ex` / `%EX`) stand for a format
     * string held in locale data, and whoever writes one does not know which fields they will
     * get; the fixed compounds `%D` / `%F` / `%T` / `%R` are no different. Each is substituted
     * with its content and expanded again until only basic specifiers are left.
     *
     * **Filtering**: a specifier the value cannot supply **degrades** -- the `%` and the
     * specifier character are written out unchanged, symmetrically in put and get (see the
     * class documentation). Degradation sets no failure bit, so `%c` on a `year_month_day`
     * yields a literal `%H:%M:%S`, and `%c` on a `local_time` yields a literal `%Z` under
     * en_US, and neither shows up until you look at the output. Only the **type** `TVal` is
     * consulted: it fixes which groups of fields can be supplied -- date, weekday, time of day,
     * UTC offset, zone identity, see @ref time_value_fields -- exactly as it fixes which path
     * `put` / `get` take. Every specifier is judged against the same table put uses and the
     * unsuppliable ones removed; specifiers outside this library's supported set (`%Q`, say) and
     * illegal modifier combinations (`%Oc`, say) fall under the same rule. Since only the type
     * matters, this takes no value argument and can be called with no object at hand.
     *
     * A removal takes one adjacent separator with it, or an orphaned piece of punctuation is
     * left behind: `"%T %Z"` becomes `"%T"`, not `"%T "`, and `"%T (%Z)"` becomes `"%T"`, not
     * `"%T ()"`. The rule: take the separator before the specifier, and the one after it only
     * when the specifier starts the string; a separator is **whitespace plus at most one ASCII
     * punctuation character**; and when the punctuation removed is an opening bracket
     * (`(` `[` `{`), the closing one right after the specifier goes too. Non-ASCII characters
     * are never separators -- `"%S秒 %Z"` loses only the space, not the 秒, at the price of
     * leaving one `፡` behind in something like `"%r፡%Z"`.
     *
     * ```cpp
     * // en_US's %c is "%a %d %b %Y %r %Z", and its %r is "%I:%M:%S %p"
     * using T = std::chrono::local_time<std::chrono::seconds>;
     * auto fmt = obj.expand_format<T>("%c");  // "%a %d %b %Y %I:%M:%S %p" -- the %Z is gone
     * obj.put(out, t, offset, fmt);
     * ```
     *
     * @note The test is the type, not the value, and the put side matches it exactly. A
     *       `std::tm` counts as able to supply `%Z` as long as the platform's `tm` carries a
     *       `tm_zone` field, and put really does write content for every such `tm` -- when
     *       `tm_zone` is empty it writes @ref base_ft<timeio>::s_unknown_zone. So a
     *       specifier that survives the filter is one put can supply, with no gap between what
     *       the table claims and what happens at run time.
     * @note `%EY` is **not** expanded: which era format applies depends on the year, so it is a
     *       property of the value, not of the locale alone. It is still filtered like the rest.
     * @note An unsuppliable compound is removed whole rather than expanded: `%c` on an
     *       `hh_mm_ss` gives an empty string, not the punctuation its expansion would leave.
     * @note A lone trailing `%` is kept as-is, matching put.
     * @note A self-referential compound format (a `D_T_FMT` holding a `%c`, say) is rejected at
     *       construction, so the expansion here cannot recurse without bound.
     *
     * @tparam TVal The time value type (`year_month_day`, `hh_mm_ss`, `sys_time`, `local_time`,
     *              `zoned_time` or `std::tm`) -- the same ones `put` accepts.
     * @param fmt The format string.
     * @return The expanded, filtered format string.
     * @endif
     */
    template <typename TVal>
    std::basic_string<CharT> expand_format(std::basic_string_view<CharT> fmt) const
    {
        using fields = time_value_fields<std::remove_cvref_t<TVal>>;
        std::basic_string<CharT> result;
        expand_and_filter<fields::has_date, fields::has_time,
                          fields::has_offset, fields::has_zone>(result, fmt);
        return result;
    }

    /**
     * @lang{ZH}
     * @brief @ref expand_format 的单字符形式：给出一个说明符，返回它展开、过滤后的样子。
     *
     * 把 `format`（以及可选的 `modifier`）组合为 `%[modifier]format` 后委托给
     * `expand_format<TVal>(fmt)`，与单字符形式的 `put` 一一对应。供不出时返回空串。
     * @tparam TVal 时间值类型，见 @ref time_value_fields。
     * @param format   说明符字符。
     * @param modifier `E` / `O` 修饰符，`0` 表示没有。
     * @return 展开并过滤后的格式串。
     * @endif
     *
     * @lang{EN}
     * @brief The single-character form of @ref expand_format: what one specifier expands and
     *        filters down to.
     *
     * Composes `format` (and the optional `modifier`) into `%[modifier]format` and delegates to
     * `expand_format<TVal>(fmt)`, matching the single-character form of `put`. An unsuppliable
     * specifier yields an empty string.
     * @tparam TVal The time value type; see @ref time_value_fields.
     * @param format   The specifier character.
     * @param modifier The `E` / `O` modifier, or `0` for none.
     * @return The expanded, filtered format string.
     * @endif
     */
    template <typename TVal>
    std::basic_string<CharT> expand_format(char format, char modifier = 0) const // NOLINT(bugprone-easily-swappable-parameters)
    {
        std::array<CharT, 4> fmt;
        std::size_t n = 0;
        fmt[n++] = static_cast<CharT>('%');
        if (modifier) fmt[n++] = static_cast<CharT>(modifier);
        fmt[n++] = static_cast<CharT>(format);
        return expand_format<TVal>(std::basic_string_view<CharT>(fmt.data(), n));
    }

    /**
     * @lang{ZH}
     * @brief 判断格式串里有没有某个说明符。
     *
     * @ref expand_format 交回来的串通常还要再问一句「里面到底有没有 `%Z`」，好决定要不要
     * 自己在末尾补一个。这一问不能用 `fmt.find("%Z")` 回答：`"%%Z"` 是一个字面 `%` 加一个
     * 字面 `Z`，`find` 却会报告找到了。判据是那个 `%` 前面连着几个 `%`——连自己算上是奇数才
     * 真的起了一个说明符。本函数按 put 解析格式串的同一套走法扫一遍，把这件事做对。
     *
     * ```cpp
     * auto fmt = obj.expand_format<T>("%c");
     * if (!timeio<char>::contains_specifier(fmt, 'Z'))
     *     fmt += " %Z";
     * ```
     *
     * @param fmt      要检查的格式串。
     * @param format   说明符字符。
     * @param modifier `E` / `O` 修饰符，`0` 表示要找不带修饰符的那个。
     * @return 若 @p fmt 里确实有这个说明符则返回 `true`。
     * @endif
     *
     * @lang{EN}
     * @brief Whether a format string holds a given specifier.
     *
     * What @ref expand_format hands back usually raises one more question -- is there a `%Z` in
     * here? -- to decide whether to append one. `fmt.find("%Z")` does not answer it: `"%%Z"` is
     * a literal `%` followed by a literal `Z`, and `find` reports a hit anyway. The test is how
     * many `%` run together before that one: counting itself, an odd number means it really does
     * start a specifier. This walks the string the way put parses it and gets that right.
     *
     * ```cpp
     * auto fmt = obj.expand_format<T>("%c");
     * if (!timeio<char>::contains_specifier(fmt, 'Z'))
     *     fmt += " %Z";
     * ```
     *
     * @param fmt      The format string to examine.
     * @param format   The specifier character.
     * @param modifier The `E` / `O` modifier, or `0` to look for the unmodified one.
     * @return `true` if @p fmt really holds that specifier.
     * @endif
     */
    static bool contains_specifier(std::basic_string_view<CharT> fmt, char format, char modifier = 0) // NOLINT(bugprone-easily-swappable-parameters)
    {
        auto f = fmt.cbegin();
        while (f != fmt.cend())
        {
            if (*f != static_cast<CharT>('%')) { ++f; continue; }
            if (++f == fmt.cend()) break;

            CharT mod = 0;
            if (*f == static_cast<CharT>('E') || *f == static_cast<CharT>('O'))
            {
                mod = *f++;
                if (f == fmt.cend()) break;
            }
            if (*f == static_cast<CharT>(format) && mod == static_cast<CharT>(modifier))
                return true;
            ++f;
        }
        return false;
    }

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
     * @note 值无法提供的说明符不构成错误，会原样写出 `%` + 修饰符 + 说明符字符；
     *       详见 `timeio` 的类说明。
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
     * @note A specifier this value cannot supply is not an error: the `%`, the modifier
     *       and the specifier character are written out unchanged. See the `timeio`
     *       class documentation.
     * @endif
     */
    template <typename OutIt, typename TVal>
    OutIt put(OutIt out, const TVal& t, char format, char modifier = 0) const // NOLINT(bugprone-easily-swappable-parameters)
    {
        std::array<CharT, 4> fmt; fmt[0] = static_cast<CharT>('%');
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

        // `fmt.data()`, not `fmt`: the target takes a `basic_string_view<CharT>`, and building one
        // from the array as a range would take all 4 elements, NUL included, instead of stopping
        // at the terminator this function just wrote.
        return put(out, t, fmt.data());
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
     * @note 值无法提供的说明符不构成错误，会原样写出 `%` + 修饰符 + 说明符字符；
     *       详见 `timeio` 的类说明。
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
     * @note A specifier this value cannot supply is not an error: the `%`, the modifier
     *       and the specifier character are written out unchanged. See the `timeio`
     *       class documentation.
     * @endif
     */
    template <typename OutIt, typename Duration, typename TimeZonePtr>
    OutIt put(OutIt out, const std::chrono::zoned_time<Duration, TimeZonePtr>& t, std::basic_string_view<CharT> fmt) const
    {
        const auto info = t.get_info();

        auto local = t.get_local_time();
        auto local_day = std::chrono::floor<std::chrono::days>(local);

        std::chrono::year_month_day ymd{local_day};
        if (!ymd.ok())
            throw stream_error("timeio put error: zoned_time date is out of range");

        std::chrono::weekday wd(local_day);

        auto time_since_midnight = std::chrono::duration_cast<std::chrono::seconds>(local - local_day);
        std::chrono::hh_mm_ss<std::chrono::seconds> time_of_day{time_since_midnight};
        const zone_info zi{info.offset, info.abbrev};
        return do_put(out, fmt, &ymd, &wd, &time_of_day, &zi, t.get_time_zone());
    }

    /**
     * @lang{ZH}
     * @brief 将 `std::chrono::sys_time` 按格式串格式化到输出迭代器。
     *
     * 一个 `sys_time` 就是 UTC 下的一个瞬间，没有区域身份，所以按 UTC 展示：`%z` 写
     * `+0000`，`%Z` 写 `UTC`。要按某个区域展示，先构造 `zoned_time`。
     * @tparam OutIt    输出迭代器类型。
     * @tparam Duration 时间点的精度类型。
     * @param out 输出迭代器。
     * @param t   要格式化的时间点。
     * @param fmt 格式串（`strftime` 风格）。
     * @return 写入后的输出迭代器。
     * @throw stream_error 若 @p t 的日期超出范围。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a `std::chrono::sys_time` to an output iterator using a format string.
     *
     * A `sys_time` is an instant in UTC with no zone identity, so it is rendered as UTC:
     * `%z` writes `+0000` and `%Z` writes `UTC`. To render it in some zone, build a
     * `zoned_time` first.
     * @tparam OutIt    Output iterator type.
     * @tparam Duration Duration type of the time point.
     * @param out The output iterator.
     * @param t   The time point to format.
     * @param fmt The format string (`strftime`-style).
     * @return The output iterator after writing.
     * @throw stream_error If the date of @p t is out of range.
     * @endif
     */
    template <typename OutIt, typename Duration>
    OutIt put(OutIt out, const std::chrono::sys_time<Duration>& t, std::basic_string_view<CharT> fmt) const
    {
        auto day = std::chrono::floor<std::chrono::days>(t);

        std::chrono::year_month_day ymd{day};
        if (!ymd.ok())
            throw stream_error("timeio put error: sys_time date is out of range");

        std::chrono::weekday wd(day);

        std::chrono::hh_mm_ss<std::chrono::seconds> time_of_day{
            std::chrono::duration_cast<std::chrono::seconds>(t - day)
        };
        const zone_info zi{std::chrono::seconds{0}, "UTC"};
        return do_put(out, fmt, &ymd, &wd, &time_of_day, &zi, nullptr);
    }

    /**
     * @lang{ZH}
     * @brief 将 `std::chrono::local_time` 连同一个已知的 UTC 偏移格式化到输出迭代器。
     *
     * 这是 tz_level::offset 那一档在 put 侧的对应物：墙上时间加偏移，没有区域身份。
     * `%z` 写 @p offset；`%Z` 无内容可写，按未知说明符原样透传。`%c` / `%X` / `%r` 用
     * locale 自己那一份格式串，本机 890 个 locale 里有 185 个的 `%c` 自带 `%Z`，此时那个
     * `%Z` 同样退化为字面量。要一份不含时区字段的格式串，把它交给 @ref expand_format。
     *
     * 因为多了 @p offset 这个参数，单字符形式的 `put(out, t, 'z')` 到不了这个重载。
     * @tparam OutIt    输出迭代器类型。
     * @tparam Duration 时间点的精度类型。
     * @param out    输出迭代器。
     * @param t      要格式化的墙上时间。
     * @param offset @p t 相对 UTC 的偏移。`%z` 只能写 `±hhmm`，因此超出 `(-24h, +24h)` 的值
     *               会被静默钳到 `±23:59:59`，且秒被截到分钟；两者都不置失败位。
     * @param fmt    格式串（`strftime` 风格）。
     * @return 写入后的输出迭代器。
     * @throw stream_error 若 @p t 的日期超出范围。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a `std::chrono::local_time` together with a known UTC offset.
     *
     * This is the put-side counterpart of the `tz_level::offset` tier: a wall-clock time plus
     * an offset, with no zone identity. `%z` writes @p offset; `%Z` has nothing to write and
     * is passed through like an unknown specifier. `%c` / `%X` / `%r` use the locale's own
     * format string, and 185 of the 890 locales on this machine carry a `%Z` inside `%c`,
     * where it degrades to a literal just the same. For a format string with no zone field
     * in it, run it through @ref expand_format.
     *
     * The extra @p offset parameter puts this overload out of reach of the single-character
     * form `put(out, t, 'z')`.
     * @tparam OutIt    Output iterator type.
     * @tparam Duration Duration type of the time point.
     * @param out    The output iterator.
     * @param t      The wall-clock time to format.
     * @param offset The offset of @p t from UTC. `%z` can only spell `±hhmm`, so a value outside
     *               `(-24h, +24h)` is clamped silently to `±23:59:59` and the seconds are
     *               truncated to a whole minute; neither sets a failure bit.
     * @param fmt    The format string (`strftime`-style).
     * @return The output iterator after writing.
     * @throw stream_error If the date of @p t is out of range.
     * @endif
     */
    template <typename OutIt, typename Duration>
    OutIt put(OutIt out, const std::chrono::local_time<Duration>& t, std::chrono::seconds offset,
              std::basic_string_view<CharT> fmt) const
    {
        auto day = std::chrono::floor<std::chrono::days>(t);

        std::chrono::year_month_day ymd{day};
        if (!ymd.ok())
            throw stream_error("timeio put error: local_time date is out of range");

        std::chrono::weekday wd(day);

        std::chrono::hh_mm_ss<std::chrono::seconds> time_of_day{
            std::chrono::duration_cast<std::chrono::seconds>(t - day)
        };
        const zone_info zi{offset, {}};
        return do_put(out, fmt, &ymd, &wd, &time_of_day, &zi, nullptr);
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
     * @note 值无法提供的说明符不构成错误，会原样写出 `%` + 修饰符 + 说明符字符；
     *       详见 `timeio` 的类说明。
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
     * @note A specifier this value cannot supply is not an error: the `%`, the modifier
     *       and the specifier character are written out unchanged. See the `timeio`
     *       class documentation.
     * @endif
     */
    template <typename OutIt>
    OutIt put(OutIt out, const std::chrono::year_month_day& t, std::basic_string_view<CharT> fmt) const
    {
        if (!t.ok())
            throw stream_error("timeio put error: year_month_day is not a valid calendar date");

        std::chrono::weekday wd(t);
        return do_put(out, fmt, &t, &wd, nullptr, nullptr, nullptr);
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
     * @note 值无法提供的说明符不构成错误，会原样写出 `%` + 修饰符 + 说明符字符；
     *       详见 `timeio` 的类说明。
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
     * @note A specifier this value cannot supply is not an error: the `%`, the modifier
     *       and the specifier character are written out unchanged. See the `timeio`
     *       class documentation.
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
        return do_put(out, fmt, nullptr, nullptr, &t_sec, nullptr, nullptr);
    }

    /**
     * @lang{ZH}
     * @brief 将 `std::tm` 按格式串格式化到输出迭代器。
     *
     * 在格式化前对 `std::tm` 的日期与时刻字段进行范围验证，并拒绝闰秒（`tm_sec == 60`）。
     * 具体检查项：月份 [0,11]、日期 [1,31]、时 [0,23]、分/秒 [0,59]，
     * 以及年份需在 `std::chrono::year` 的有效范围内，且日期组合需构成有效日历日期。
     * `tm_gmtoff` **不在检查之列**：它只在 `%z` 处按该说明符的取值域钳位，不会让本函数抛出。
     * @tparam OutIt 输出迭代器类型。
     * @param out 输出迭代器。
     * @param t   要格式化的 `std::tm` 结构体。
     * @param fmt 格式串（`strftime` 风格）。
     * @return 写入后的输出迭代器。
     * @throw stream_error 若任何字段超出范围或日期组合无效。
     * @note 值无法提供的说明符不构成错误，会原样写出 `%` + 修饰符 + 说明符字符；
     *       详见 `timeio` 的类说明。
     * @note **时区取自扩展成员。** 当前平台的 `std::tm` 带 `tm_gmtoff` 时（POSIX.1-2024 起
     *       是标准成员，glibc / BSD / macOS 一直都有），`%z` 写它的值；带 `tm_zone` 时
     *       `%Z` 写它，`tm_zone` 为空指针或空串则写
     *       @ref base_ft<timeio>::s_unknown_zone。成员的有无用 `requires` 表达式
     *       探测，不看平台宏。平台的 `std::tm` **不带** `tm_gmtoff` 时 `%z` 与 `%Z` 才退化为
     *       字面量，`%c` / `%X` / `%r` 里 locale 自带的那个 `%Z` 也一样。
     * @note 因此在带 `tm_zone` 的平台上，`%Z` 对任何 `std::tm` 都写得出内容——这正是
     *       @ref time_value_fields<std::tm>::has_zone 恒为 `true` 所声明的，两者不会脱节。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a `std::tm` to an output iterator using a format string.
     *
     * Validates the date and time-of-day fields of the `std::tm` before formatting and rejects
     * leap seconds (`tm_sec == 60`). Checks include: month [0,11], day [1,31], hour [0,23],
     * minute/second [0,59], year within the valid range of `std::chrono::year`,
     * and that the date combination forms a valid calendar date. `tm_gmtoff` is **not** among
     * them: it is clamped to the `%z` range at that specifier instead of making this call throw.
     * @tparam OutIt Output iterator type.
     * @param out The output iterator.
     * @param t   The `std::tm` struct to format.
     * @param fmt The format string (`strftime`-style).
     * @return The output iterator after writing.
     * @throw stream_error If any field is out of range or the date combination is invalid.
     * @note A specifier this value cannot supply is not an error: the `%`, the modifier
     *       and the specifier character are written out unchanged. See the `timeio`
     *       class documentation.
     * @note **The time zone comes from the extension members.** When this platform's
     *       `std::tm` carries `tm_gmtoff` (a standard member since POSIX.1-2024, and present
     *       on glibc / BSD / macOS all along), `%z` writes its value; when it carries
     *       `tm_zone`, `%Z` writes that, or
     *       @ref base_ft<timeio>::s_unknown_zone if `tm_zone` is null or empty. Their
     *       presence is detected with a `requires` expression rather than a platform macro.
     *       Only on a platform whose `std::tm` lacks `tm_gmtoff` do `%z` and `%Z` degrade to
     *       literals, and with them a locale's own `%Z` inside `%c` / `%X` / `%r`.
     * @note So on a platform with `tm_zone`, `%Z` produces content for every `std::tm` -- which
     *       is exactly what @ref time_value_fields<std::tm>::has_zone being unconditionally
     *       `true` claims, leaving no gap between the two.
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
        if (t.tm_mon  < 0 || t.tm_mon  > 11 ||
            t.tm_mday < 1 || t.tm_mday > 31 ||
            t.tm_hour < 0 || t.tm_hour > 23 ||
            t.tm_min  < 0 || t.tm_min  > 59 ||
            t.tm_sec  < 0 || t.tm_sec  > 59 ||
            t.tm_year > static_cast<int>(year::max()) - 1900 ||
            t.tm_year + 1900 < static_cast<int>(year::min()))
            throw stream_error("timeio put error: std::tm field out of range");

        const int y = t.tm_year + 1900;

        year_month_day ymd{ year{y}, month{static_cast<unsigned>(t.tm_mon) + 1}, day{static_cast<unsigned>(t.tm_mday)} };
        if (!ymd.ok())
            throw stream_error("timeio put error: std::tm is not a valid calendar date");

        weekday wd{ymd};

        seconds sec = hours{t.tm_hour} + minutes{t.tm_min} + seconds{t.tm_sec};
        std::chrono::hh_mm_ss hms{sec};

        if constexpr (requires { t.tm_gmtoff; })
        {
            std::string_view abbrev;
            if constexpr (requires { t.tm_zone; })
            {
                abbrev = (t.tm_zone && *t.tm_zone)
                       ? std::string_view{t.tm_zone}
                       : base_ft<timeio>::s_unknown_zone;
            }
            const zone_info zi{seconds{t.tm_gmtoff}, abbrev};
            return do_put(out, fmt, &ymd, &wd, &hms, &zi, nullptr);
        }
        else
            return do_put(out, fmt, &ymd, &wd, &hms, nullptr, nullptr);
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
     * @tparam TzLevel 解析哪一档时区字段，见 tz_level。
     * @param beg      输入范围起始迭代器。
     * @param end      输入范围结束哨兵。
     * @param ctx      累积解析结果的上下文（in/out）。
     * @param format   格式字符（如 `'Y'`、`'m'`、`'d'`）。
     * @param modifier 可选修饰符（`'E'`、`'O'` 或 `0` 表示无修饰符）。
     * @return 指向未被消费的第一个字符的迭代器。
     * @throw stream_error 若解析失败。
     * @note 上下文无法接收的说明符不构成错误，会转而要求输入中出现 `%` + 修饰符 +
     *       说明符字符这一字面量；详见 `timeio` 的类说明。
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
     * @tparam TzLevel Which tier of timezone fields is parsed; see tz_level.
     * @param beg      Beginning of the input range.
     * @param end      End sentinel of the input range.
     * @param ctx      Parse context accumulating results (in/out).
     * @param format   The format character (e.g. `'Y'`, `'m'`, `'d'`).
     * @param modifier Optional modifier (`'E'`, `'O'`, or `0` for none).
     * @return Iterator pointing to the first unconsumed character.
     * @throw stream_error If parsing fails.
     * @note A specifier the context cannot receive is not an error: the input is instead
     *       required to carry the literal `%`, modifier and specifier character. See
     *       the `timeio` class documentation.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent, bool HaveDate, bool HaveTime, tz_level TzLevel>
        requires (steppable_back<TIter> || is_istreambuf_iterator<TIter>)
    TIter get(TIter beg, TSent end, time_parse_context<char_type, HaveDate, HaveTime, TzLevel>& ctx,
              char format, char modifier = 0) const // NOLINT(bugprone-easily-swappable-parameters)
    {
        std::array<CharT, 4> fmt; fmt[0] = static_cast<CharT>('%');
        // `fmt.data()`, not `fmt`: see the note in the `put` overload above.
        if (modifier)
        {
            fmt[1] = modifier;
            fmt[2] = format;
            fmt[3] = static_cast<CharT>('\0');
            return get(beg, end, ctx, fmt.data());
        }
        else
        {
            fmt[1] = format;
            fmt[2] = static_cast<CharT>('\0');
            return get(beg, end, ctx, fmt.data());
        }
    }

    /**
     * @lang{ZH}
     * @brief 按格式串从输入范围中解析时间字段，将结果累积到 `ctx`。
     *
     * 各格式说明符与 POSIX `strptime` / `std::chrono::from_stream` 的语义一致。
     * 复合说明符（`%c`、`%x`、`%X`、`%r`、`%EY`）会将 locale 提供的格式串
     * 展开后递归处理，详见 `do_get` 中关于递归的说明。
     * @tparam TIter      双向迭代器或 `istreambuf_iterator` 类型。
     * @tparam TSent      哨兵类型。
     * @tparam HaveDate   是否解析日期字段。
     * @tparam HaveTime   是否解析时间字段。
     * @tparam TzLevel 解析哪一档时区字段，见 tz_level。
     * @param rp     输入范围起始迭代器。
     * @param rp_end 输入范围结束哨兵。
     * @param ctx    累积解析结果的上下文（in/out）。
     * @param _fmt   格式串（`strptime` 风格）。
     * @return 指向未被消费的第一个字符的迭代器。
     * @throw stream_error 若解析失败（格式不匹配或字段值超出范围）。
     * @note 上下文无法接收的说明符不构成错误，会转而要求输入中出现 `%` + 修饰符 +
     *       说明符字符这一字面量；详见 `timeio` 的类说明。
     * @endif
     *
     * @lang{EN}
     * @brief Parses time fields from an input range according to a format string,
     *        accumulating results into `ctx`.
     *
     * Format specifiers follow the semantics of POSIX `strptime` /
     * `std::chrono::from_stream`. Compound specifiers (`%c`, `%x`, `%X`, `%r`,
     * `%EY`) expand locale-provided format strings and re-enter recursively;
     * see the recursion note in `do_get`.
     * @tparam TIter      Bidirectional iterator or `istreambuf_iterator` type.
     * @tparam TSent      Sentinel type.
     * @tparam HaveDate   Whether date fields are parsed.
     * @tparam HaveTime   Whether time fields are parsed.
     * @tparam TzLevel Which tier of timezone fields is parsed; see tz_level.
     * @param rp     Beginning of the input range.
     * @param rp_end End sentinel of the input range.
     * @param ctx    Parse context accumulating results (in/out).
     * @param _fmt   The format string (`strptime`-style).
     * @return Iterator pointing to the first unconsumed character.
     * @throw stream_error If parsing fails (format mismatch or field value out of range).
     * @note A specifier the context cannot receive is not an error: the input is instead
     *       required to carry the literal `%`, modifier and specifier character. See
     *       the `timeio` class documentation.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent, bool HaveDate, bool HaveTime, tz_level TzLevel>
        requires (steppable_back<TIter> || is_istreambuf_iterator<TIter>)
    TIter get(TIter rp, TSent rp_end, time_parse_context<char_type, HaveDate, HaveTime, TzLevel>& ctx,
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
     * @brief 判断当前说明符是否应按纪元解析，并在首次遇到 `E` 修饰符时按需填充纪元条目表。
     *
     * 纪元条目表仅被带 `E` 修饰符的 `%C`、`%y`、`%Y` 三个分支读取，因此改为在首次
     * 遇到这三者时才从 `m_era_master` 拷入 `ctx`，而不是每次解析前无条件填充：不含
     * `%E` 说明符的格式串必须让 `ctx.m_era_items` 保持为空，也不应为此付出拷贝
     * locale 纪元表的代价。
     *
     * 判据是 `mod == 'E'` **且**表非空，两个条件都不能省：
     * - 修饰符属于**单个**转换说明，不会沿格式串向后粘连。POSIX 把 `E`/`O` 定义为
     *   "修饰 field descriptor"，纪元语义只挂在 `%EC`/`%Ey`/`%EY` 上；`%EC %y` 里的
     *   `%y` 是不带修饰符的 `%y`，必须按"年内世纪"解析。若只看表非空，前面出现过任何
     *   `%E…` 就会把后面普通的 `%C`/`%y`/`%Y` 一并拖进纪元分支。`%Oy` 更是范畴错误：
     *   `O` 表示"使用 locale 的替代数字符号"，与纪元无关，必须落到下面的
     *   `extract_num_with_alt_digits` 一支。
     * - 表非空这一半对应 POSIX 的"替代格式在当前 locale 不存在时，使用未修饰的说明符"：
     *   `C` locale 下 `%EC` 退化为普通两位世纪、`%Ey` 退化为 `%y`。
     *
     * @tparam HaveDate     上下文是否包含日期字段。
     * @tparam HaveTime     上下文是否包含时间字段。
     * @tparam TzLevel 上下文携带哪一档时区字段，见 tz_level。
     * @param ctx 累积解析结果的上下文（in/out）。
     * @param mod 当前说明符的修饰符字符，`E` 以外的值不触发填充且一律返回 `false`。
     * @return `mod` 为 `E` 且纪元条目表非空时返回 `true`；`HaveDate` 为 `false` 时恒为 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Reports whether the current specifier is to be parsed as an era, filling the
     *        era item table on demand the first time an `E` modifier is seen.
     *
     * The era table is only ever read by the E-modified `%C`, `%y` and `%Y` branches,
     * so it is copied from `m_era_master` into `ctx` on first sight of one of them
     * rather than unconditionally before every parse: a format string with no `%E`
     * specifier must leave `ctx.m_era_items` empty, and must not pay for copying the
     * locale's era list.
     *
     * The test is `mod == 'E'` **and** a non-empty table; neither half may be dropped:
     * - A modifier belongs to a **single** conversion specification and does not carry
     *   over to the rest of the format string. POSIX defines `E`/`O` as modifying a field
     *   descriptor, and era semantics attach only to `%EC`/`%Ey`/`%EY`; the `%y` in
     *   `%EC %y` is an unmodified `%y` and must be read as a year within century. Testing
     *   only for a non-empty table would let any earlier `%E…` drag every later plain
     *   `%C`/`%y`/`%Y` into the era branch. `%Oy` would be an outright category error:
     *   `O` selects the locale's alternative numeric symbols, nothing to do with eras, and
     *   has to reach the `extract_num_with_alt_digits` arm below.
     * - The non-empty half implements POSIX's "if the alternative format or specification
     *   does not exist in the current locale, the unmodified field descriptor is used":
     *   in the `C` locale `%EC` degrades to a plain two-digit century and `%Ey` to `%y`.
     *
     * @tparam HaveDate     Whether the context carries date fields.
     * @tparam HaveTime     Whether the context carries time fields.
     * @tparam TzLevel Which tier of timezone fields the context carries; see tz_level.
     * @param ctx Parse context accumulating results (in/out).
     * @param mod Modifier character of the current specifier; anything other than `E`
     *            neither triggers the fill nor ever yields `true`.
     * @return `true` when `mod` is `E` and the era item table is non-empty; always
     *         `false` when `HaveDate` is `false`.
     * @endif
     */
    template <bool HaveDate, bool HaveTime, tz_level TzLevel>
    bool era_items_active(time_parse_context<char_type, HaveDate, HaveTime, TzLevel>& ctx,
                          CharT mod) const
    {
        if constexpr (!HaveDate) { (void)ctx; (void)mod; return false; }
        else
        {
            if (mod != static_cast<CharT>('E')) return false;
            if (ctx.is_init == false)
            {
                std::copy(m_era_master.begin(), m_era_master.end(), std::back_inserter(ctx.m_era_items));
                ctx.is_init = true;
            }
            return !ctx.m_era_items.empty();
        }
    }

    /**
     * @lang{ZH}
     * @brief 解析核心函数，按格式串逐字符处理输入，将结果写入 `ctx`。
     *
     * @note **递归**
     *   以下复合说明符会将 locale 提供的格式串展开后递归调用此函数：
     *   - `%c` → `m_[era_]date_time_format`
     *   - `%x` → `m_[era_]date_format`
     *   - `%X` → `m_[era_]time_format`
     *   - `%r` → `m_am_pm_format`
     *   - `%EY` → 纪元的 `format` 字符串
     *
     *   **此处刻意不设置递归深度上限**，因为不需要：这些字符串全部来自 locale 数据，
     *   而构造函数已经用 `validate_format_recursion` 证明了它们构成的图无环
     *   （如 `D_T_FMT` 不含 `%c`，纪元格式不含 `%EY`）。用户传进来的格式串只是入口，
     *   只能通过这些表进入递归，因此深度上界就是节点数，与输入长度无关。手工构造或被
     *   篡改的 locale 在构造 facet 时即被拒绝，而不是在这里溢出栈。
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
     * @note **Recursion**
     *   The following compound specifiers expand a locale-provided format string
     *   and re-enter this function recursively:
     *   - `%c` → `m_[era_]date_time_format`
     *   - `%x` → `m_[era_]date_format`
     *   - `%X` → `m_[era_]time_format`
     *   - `%r` → `m_am_pm_format`
     *   - `%EY` → era `format` string
     *
     *   There is deliberately **NO recursion-depth guard**, because none is needed:
     *   every one of those strings comes from locale data, and the constructor has
     *   already proven via `validate_format_recursion` that the graph they form is
     *   acyclic (e.g. `D_T_FMT` does not contain `%c`, an era format does not contain
     *   `%EY`). A user-supplied format string is only an entry point and can reach the
     *   recursion solely through those tables, so the depth is bounded by the node
     *   count and is independent of the input length. A hand-crafted or corrupted
     *   locale is rejected when the facet is constructed, not by overflowing the
     *   stack here.
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
    // NOLINTBEGIN(cppcoreguidelines-avoid-goto)
    template <typename TIter, std::sentinel_for<TIter> TSent, bool HaveDate, bool HaveTime, tz_level TzLevel>
        requires (steppable_back<TIter> || is_istreambuf_iterator<TIter>)
    TIter do_get(TIter rp, TSent rp_end, time_parse_context<char_type, HaveDate, HaveTime, TzLevel>& ctx,
                 bool& succ, std::basic_string_view<CharT> _fmt) const
    {
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
                if (static_cast<CharT>('%') != *rp)
                {
                    succ = false;
                    return rp;
                }
                ++rp;
                break;
            }

            CharT modifier = 0;
            if (*fmt == static_cast<CharT>('E') || *fmt == static_cast<CharT>('O'))
            {
                modifier = *fmt;
                if (++fmt == _fmt.cend())
                {
                    if ((static_cast<CharT>('%') != *rp))
                    {
                        succ = false;
                        return rp;
                    }
                    ++rp;
                    if ((rp == rp_end) || (modifier != *rp))
                    {
                        succ = false;
                        return rp;
                    }
                    ++rp;
                    break;
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
                    if (modifier == static_cast<CharT>('E'))
                        rp = do_get(rp, rp_end, ctx, succ, m_era_date_time_format);
                    else
                        rp = do_get(rp, rp_end, ctx, succ, m_date_time_format);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('C'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier == static_cast<CharT>('O')) goto bad_parse_format;
                    if (!era_items_active(ctx, modifier))
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
                        ctx.m_have_era = true;
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
                    static constexpr std::array subfmt = {static_cast<CharT>('%'), static_cast<CharT>('m'), static_cast<CharT>('/'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('d'), static_cast<CharT>('/'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('y'), CharT()};
                    rp = do_get(rp, rp_end, ctx, succ, subfmt.data());
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('F'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    static constexpr std::array subfmt = {static_cast<CharT>('%'), static_cast<CharT>('Y'), static_cast<CharT>('-'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('m'), static_cast<CharT>('-'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('d'), CharT()};
                    rp = do_get(rp, rp_end, ctx, succ, subfmt.data());
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
                    static constexpr std::array subfmt = {static_cast<CharT>('%'), static_cast<CharT>('H'), static_cast<CharT>(':'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('M'), CharT()};
                    rp = do_get(rp, rp_end, ctx, succ, subfmt.data());
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
                    static constexpr std::array subfmt = {static_cast<CharT>('%'), static_cast<CharT>('H'), static_cast<CharT>(':'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('M'), static_cast<CharT>(':'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('S'), CharT()};
                    rp = do_get(rp, rp_end, ctx, succ, subfmt.data());
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
                    if (modifier == static_cast<CharT>('E'))
                        rp = do_get(rp, rp_end, ctx, succ, m_era_time_format);
                    else
                        rp = do_get(rp, rp_end, ctx, succ, m_time_format);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('y'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else if (era_items_active(ctx, modifier))
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
                    if (era_items_active(ctx, modifier))
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
                                    bool any_match = false;
                                    for (const auto& cur_era : ctx.m_era_items)
                                    {
                                        if (cur_era.format == *format_it) { any_match = true; break; }
                                    }
                                    if (any_match)
                                    {
                                        for (auto it = ctx.m_era_items.begin(); it != ctx.m_era_items.end();)
                                        {
                                            if (it->format == *format_it) ++it;
                                            else it = ctx.m_era_items.erase(it);
                                        }
                                    }
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
                if constexpr (TzLevel < tz_level::offset) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    /* We recognize four formats:
                        1. Two digits specify hours.
                        2. Four digits specify hours and minutes.
                        3. Two digits, ':', and two digits specify hours and minutes.
                        4. 'Z' is equivalent to +0000.
                    */
                    int val = 0;
                    if (*rp == static_cast<CharT>('Z'))
                    {
                        ctx.m_offset = std::chrono::minutes{0};
                        ctx.m_have_offset = true;
                        ++rp; break;
                    }

                    if ((*rp != static_cast<CharT>('+')) && (*rp != static_cast<CharT>('-')))
                    {
                        succ = false; return rp;
                    }
                    const int sign = (*rp == static_cast<CharT>('-')) ? -1 : 1;
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
                        if (*rp == static_cast<CharT>(':') && n == 2)
                        {
                            ++rp;
                            if (rp == rp_end || *rp < static_cast<CharT>('0')
                                             || *rp > static_cast<CharT>('9'))
                            {
                                if constexpr (steppable_back<TIter>) --rp;
                                else rp.sputbackc(static_cast<CharT>(':'));
                                break;
                            }
                        }
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

                    ctx.m_offset = std::chrono::minutes{sign * ((val / 100) * 60 + val % 100)};
                    ctx.m_have_offset = true;
                }
                break;

            case static_cast<CharT>('Z'):
                // Below tz_level::zone this specifier is matched literally, which is what put
                // degrades it to for a value with no zone to name.
                if constexpr (TzLevel != tz_level::zone) goto bad_parse_format;
                else
                {
                    if (modifier) goto bad_parse_format;
                    typename decltype(base_ft<timeio>::s_timezone_tree)::match_out_type zone_res{};
                    rp = base_ft<timeio>::s_timezone_tree.max_match(rp, rp_end, zone_res);
                    if (!zone_res) { succ = false; return rp; }
                    if (zone_res->is_name)   ctx.m_zone_name   = zone_res->text.c_str();
                    if (zone_res->is_abbrev) ctx.m_zone_abbrev = zone_res->text.c_str();
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
    // NOLINTEND(cppcoreguidelines-avoid-goto)

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
     * @brief 判断一个字符是否为标点。`%` 不算。
     *
     * 与 @ref is_space 同样的做法：设了 `ctype` facet 就用它的 `punct` 分类，否则退回基础的
     * ASCII 标点表。`expand_and_filter` 摘说明符时靠它认分隔符——所以「秒」这类文字不会被当成
     * 分隔符吃掉，而字符类型放得下的真标点（如 `፡`）会。`%` 排除在外，否则紧跟其后的那个
     * 说明符会被误当作分隔符。
     * @param c 要检查的字符。
     * @return 若 `c` 为 `%` 以外的标点则返回 `true`。
     * @endif
     *
     * @lang{EN}
     * @brief Checks whether a character is punctuation. `%` is not.
     *
     * Same approach as @ref is_space: the `ctype` facet's `punct` classification if one is set,
     * otherwise a basic ASCII punctuation table. This is how `expand_and_filter` recognizes a
     * separator when it drops a specifier -- so a letter like 秒 is never eaten as one, while a
     * genuine punctuation mark the character type can hold (`፡`, say) is. `%` is excluded, or
     * the specifier right after it would be mistaken for a separator.
     * @param c The character to check.
     * @return `true` if `c` is punctuation other than `%`.
     * @endif
     */
    bool is_punct(CharT c) const
    {
        if (c == static_cast<CharT>('%')) return false;
        if (m_ctype)
            return m_ctype->is_any(base_ft<ctype>::punct, c);
        for (char p : std::string_view("!\"#$&'()*+,-./:;<=>?@[\\]^_`{|}~"))
            if (c == static_cast<CharT>(p)) return true;
        return false;
    }

    /**
     * @lang{ZH}
     * @brief 在格式串中找出与某个左括号配对的右括号。
     *
     * 供 `expand_and_filter` 划出括号组的范围。计数嵌套，并跳过 `%` 引导的说明符，免得
     * `"%("` 这种把说明符字符当成括号数进去。
     * @param fmt   格式串。
     * @param open  指向左括号的迭代器。
     * @param close 配对的右括号字符。
     * @return 指向配对右括号的迭代器；找不到则返回 `fmt.cend()`。
     * @endif
     *
     * @lang{EN}
     * @brief Finds the closing bracket matching an opening one in a format string.
     *
     * This is how `expand_and_filter` delimits a bracket group. Nesting is counted, and
     * `%`-introduced specifiers are skipped so that a `"%("` does not have its specifier
     * character counted as a bracket.
     * @param fmt   The format string.
     * @param open  Iterator at the opening bracket.
     * @param close The matching closing bracket character.
     * @return An iterator at the matching closing bracket, or `fmt.cend()` if there is none.
     * @endif
     */
    static typename std::basic_string_view<CharT>::const_iterator
    match_bracket(std::basic_string_view<CharT> fmt,
                  typename std::basic_string_view<CharT>::const_iterator open, CharT close)
    {
        int depth = 0;
        for (auto p = open; p != fmt.cend(); )
        {
            if (*p == static_cast<CharT>('%'))
            {
                if (++p == fmt.cend()) break;
                if (*p == static_cast<CharT>('E') || *p == static_cast<CharT>('O'))
                    if (++p == fmt.cend()) break;
                ++p;
                continue;
            }
            if (*p == *open) ++depth;
            else if (*p == close && --depth == 0) return p;
            ++p;
        }
        return fmt.cend();
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
    template <std::size_t N>
    static void check_unique_nonempty(const std::array<std::basic_string<CharT>, N>& full,
                                      const std::array<std::basic_string<CharT>, N>& abbr,
                                      const char* what)
    {
        std::array<const std::basic_string<CharT>*, 2 * N> names{};
        for (std::size_t i = 0; i < N; ++i)
        {
            if (full[i].empty() || abbr[i].empty())
                throw std::runtime_error(std::string("timeio: empty ") + what + " name in locale data");
            names[2 * i]     = &full[i];
            names[2 * i + 1] = &abbr[i];
        }
        // names[p] belongs to index p / 2.
        for (std::size_t a = 0; a < names.size(); ++a)
            for (std::size_t b = a + 1; b < names.size(); ++b)
                if ((a / 2) != (b / 2) && *names[a] == *names[b])
                    throw std::runtime_error(std::string("timeio: duplicate ") + what + " name in locale data");
    }

    /**
     * @lang{ZH}
     * @brief 验证 locale 的复合格式串不自引用——把递归有界这件事在构造时一次证明。
     *
     * `%c` / `%x` / `%X` / `%r` / `%EY` 会把另一个格式串重新送进 `do_put` / `do_get` /
     * `expand_and_filter` 的 switch，所以这些串构成一张有向图：节点是七个 locale 复合格式加上
     * 每条纪元格式，边是「这个串里出现了那个说明符」。图无环，展开链长度就有上界。
     *
     * 用户自己传的格式串不是节点——它只是一次入口，只能通过这些表进入递归。所以这一次检查把三
     * 条递归路径**整体**变成有界的，而不只是挡住 `D_T_FMT == "%c"` 这一种写法。
     *
     * @throw std::runtime_error 若存在环。
     * @endif
     *
     * @lang{EN}
     * @brief Verifies that the locale's compound format strings are not self-referential --
     *        proving once, at construction, that the recursion is bounded.
     *
     * `%c` / `%x` / `%X` / `%r` / `%EY` hand another format string back to the switch in
     * `do_put` / `do_get` / `expand_and_filter`, so those strings form a directed graph: a node
     * per locale compound (seven of them) plus one per era format, an edge wherever one string
     * names another. An acyclic graph bounds the length of every expansion chain.
     *
     * A user-supplied format string is not a node -- it is one entry point, and it can only
     * reach the recursion through these tables. So this single check bounds all three recursive
     * paths **as a whole**, rather than merely rejecting the `D_T_FMT == "%c"` spelling.
     *
     * @throw std::runtime_error If there is a cycle.
     * @endif
     */
    void validate_format_recursion() const
    {
        // The node numbering. era_first is both the count of fixed nodes and the number of the
        // first era format: how many era formats a locale has varies, so they can only be
        // numbered from there on.
        // NOLINTNEXTLINE(performance-enum-size)
        enum class fmt_node : std::size_t
        {
            date_time, era_date_time,
            date,      era_date,
            time,      era_time,
            am_pm,
            era_first
        };

        // The order here IS fmt_node; era formats take the numbers from era_first on.
        std::vector<const std::basic_string<CharT>*> nodes{
            &m_date_time_format, &m_era_date_time_format,
            &m_date_format,      &m_era_date_format,
            &m_time_format,      &m_era_time_format,
            &m_am_pm_format};
        assert(nodes.size() == std::to_underlying(fmt_node::era_first));
        for (const era_entry& entry : m_era_master) nodes.push_back(&entry.format);

        // Modifier gating mirrors the three switches: %Oc / %Ox / %OX / %Er / %Or degrade to
        // literals there, so no edge here. %% needs no case -- the second % is consumed as the
        // specifier character. Brackets need none either: a %c inside a group still recurses.
        //
        // The graph deliberately over-approximates, since a shade too many edges costs nothing
        // while a missing one costs the guarantee: %EY edges to every era node (do_get does try
        // them all, do_put expands only the era the year lands in, and expand_and_filter leaves
        // %EY unexpanded), and an edge stands even where the value type could not supply the
        // specifier and the switch would drop it. A %EY inside an era format re-resolves to that
        // same era, so that self-loop is real rather than an approximation.
        auto scan_edges = [n_total = nodes.size()]
                          (std::basic_string_view<CharT> fmt, auto on_edge)
        {
            for (auto p = fmt.cbegin(); p != fmt.cend(); )
            {
                if (*p != static_cast<CharT>('%')) { ++p; continue; }
                if (++p == fmt.cend()) break;

                CharT modifier = 0;
                if (*p == static_cast<CharT>('E') || *p == static_cast<CharT>('O'))
                {
                    modifier = *p;
                    if (++p == fmt.cend()) break;
                }

                const bool era = (modifier == static_cast<CharT>('E'));
                switch (*p)
                {
                case static_cast<CharT>('c'):
                    if (modifier != static_cast<CharT>('O'))
                        on_edge(era ? fmt_node::era_date_time : fmt_node::date_time);
                    break;

                case static_cast<CharT>('x'):
                    if (modifier != static_cast<CharT>('O'))
                        on_edge(era ? fmt_node::era_date : fmt_node::date);
                    break;

                case static_cast<CharT>('X'):
                    if (modifier != static_cast<CharT>('O'))
                        on_edge(era ? fmt_node::era_time : fmt_node::time);
                    break;

                case static_cast<CharT>('r'):
                    if (!modifier) on_edge(fmt_node::am_pm);
                    break;

                case static_cast<CharT>('Y'):
                    if (era)
                        for (std::size_t i = std::to_underlying(fmt_node::era_first); i < n_total; ++i)
                            on_edge(static_cast<fmt_node>(i));
                    break;

                default:
                    break;
                }
                ++p;
            }
        };

        // Three-colour DFS: 0 unvisited, 1 on the stack, 2 finished. A grey target is a back
        // edge, i.e. a cycle; a directly self-referential %c lands here with u == v.
        std::vector<char> color(nodes.size(), 0);
        auto visit = [&](auto&& self, std::size_t v) -> void
        {
            color[v] = 1;
            scan_edges(*nodes[v], [&](fmt_node node)
            {
                const std::size_t u = std::to_underlying(node);
                if (color[u] == 1)
                    throw std::runtime_error("timeio: self-referential compound format in locale data");
                if (color[u] == 0) self(self, u);
            });
            color[v] = 2;
        };

        for (std::size_t v = 0; v < nodes.size(); ++v)
            if (color[v] == 0) visit(visit, v);
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
        for (std::size_t i = 0; i < m_era_master.size(); ++i)
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
        for (std::size_t i = 0; i < 100; ++i)
        {
            if (!m_alt_digits[i].empty())
                m_alt_digits_tree.add(m_alt_digits[i], i);
        }
    }

    /**
     * @lang{ZH}
     * @brief 传给 do_put 的时区信息，对应 tz_level::offset 那一档。
     *
     * 拆成 @ref zone_info 和 `const std::chrono::time_zone*` 两个参数，是因为这两样东西的
     * 可得性并不同步：`sys_time` 有偏移没身份，`std::tm` 两样都没有，只有 `zoned_time`
     * 两样都有。`%z` 只看 @ref zone_info，`%Z` 优先用身份、退而求其次用缩写。
     *
     * @p abbrev 不持有字符串，指向的存储必须活过整次 do_put 调用。
     * @endif
     *
     * @lang{EN}
     * @brief The timezone information handed to do_put, matching the `tz_level::offset` tier.
     *
     * This is split from the `const std::chrono::time_zone*` parameter because the two are
     * not available together: a `sys_time` has an offset but no identity, a `std::tm` has
     * neither, and only a `zoned_time` has both. `%z` reads only @ref zone_info; `%Z` prefers
     * the identity and falls back to the abbreviation.
     *
     * @p abbrev does not own its characters -- the storage it points at must outlive the
     * whole do_put call.
     * @endif
     */
    struct zone_info
    {
        std::chrono::seconds offset{};   ///< @lang{ZH} UTC 偏移。 @endif @lang{EN} The UTC offset. @endif
        std::string_view     abbrev{};   ///< @lang{ZH} 时区缩写，可为空。 @endif @lang{EN} The zone abbreviation; may be empty. @endif
    };

    /**
     * @lang{ZH}
     * @brief 格式化核心函数，按格式串将日期/时间各分量写入输出迭代器。
     *
     * @note 复合说明符 `%c`、`%x`、`%X`、`%r`、`%EY` 会将 locale 提供的格式串
     *   展开后递归调用此函数。递归有界，理由与 `do_get` 相同：自引用格式串
     *   （如 `D_T_FMT == "%c"`）在构造 facet 时就被拒绝。
     *
     * @tparam OutIt 输出迭代器类型。
     * @param out    输出迭代器。
     * @param format 格式串（`strftime` 风格）。
     * @param ymd    日期指针（若不含日期分量则为 `nullptr`）。
     * @param wd     星期指针（若不含星期分量则为 `nullptr`）。
     * @param hms    时间指针（若不含时间分量则为 `nullptr`）。
     * @param zi     偏移 + 缩写指针，喂 `%z`（若值不带偏移则为 `nullptr`）。
     * @param tz     时区身份指针，喂 `%Z`（若值不带区域身份则为 `nullptr`）。
     * @return 写入后的输出迭代器。
     * @note @p format 以落单的 `%` 结尾（或以落单的 `%E` / `%O` 修饰符结尾）时，按「无法解释的
     *       格式内容原样透传」处理：写出 `%`（及修饰符）本身，不视为错误。这与未知说明符
     *       （如 `%Q`）走的是同一条策略，也与 `strftime` 一致。`do_get` 对称地把它当作字面
     *       `%`（及修饰符）来匹配，故往返不变量成立。
     * @endif
     *
     * @lang{EN}
     * @brief Core formatting function that writes date/time components to an output
     *        iterator according to the format string.
     *
     * @note The compound specifiers `%c`, `%x`, `%X`, `%r`, `%EY` expand a
     *   locale-provided format string and re-enter this function recursively. The
     *   recursion is bounded for the same reason as in `do_get`: a self-referential
     *   format string (e.g. `D_T_FMT == "%c"`) is rejected when the facet is built.
     *
     * @tparam OutIt Output iterator type.
     * @param out    The output iterator.
     * @param format The format string (`strftime`-style).
     * @param ymd    Date pointer (`nullptr` if no date components are needed).
     * @param wd     Weekday pointer (`nullptr` if no weekday component is needed).
     * @param hms    Time pointer (`nullptr` if no time components are needed).
     * @param zi     Offset + abbreviation pointer feeding `%z` (`nullptr` if the value
     *               carries no offset).
     * @param tz     Zone-identity pointer feeding `%Z` (`nullptr` if the value carries no
     *               zone identity).
     * @return The output iterator after writing.
     * @note When @p format ends with a lone `%` (or with a lone `%E` / `%O` modifier), it is
     *       handled by the "pass unrecognized format content through unchanged" policy: the `%`
     *       (and the modifier) is written out and it is not an error. This is the same policy
     *       unknown specifiers such as `%Q` follow, and it matches `strftime`. `do_get`
     *       symmetrically matches it as a literal `%` (and modifier), so the round-trip
     *       invariant holds.
     * @endif
     */
    // NOLINTBEGIN(cppcoreguidelines-avoid-goto)
    template <typename OutIt>
    OutIt do_put(OutIt out, std::basic_string_view<CharT> format,
                 const std::chrono::year_month_day* ymd,
                 const std::chrono::weekday* wd,
                 const std::chrono::hh_mm_ss<std::chrono::seconds>* hms,
                 const zone_info* zi,
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

            if (++f == format.cend())
            {
                *out++ = static_cast<CharT>('%');
                break;
            }

            CharT modifier = 0;
            if (*f == static_cast<CharT>('E') || *f == static_cast<CharT>('O'))
            {
                modifier = *f++;
                if (f == format.cend())
                {
                    *out++ = static_cast<CharT>('%');
                    *out++ = modifier;
                    break;
                }
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
                    const std::basic_string<CharT>& subFmt = (modifier == static_cast<CharT>('E')) ?
                                                              m_era_date_time_format : m_date_time_format;
                    out = do_put(out, subFmt, ymd, wd, hms, zi, tz);
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
                    static constexpr std::array subfmt = {static_cast<CharT>('%'), static_cast<CharT>('m'), static_cast<CharT>('/'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('d'), static_cast<CharT>('/'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('y'), CharT()};
                    out = do_put(out, subfmt.data(), ymd, wd, hms, zi, tz);
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
                    static constexpr std::array subfmt = {static_cast<CharT>('%'), static_cast<CharT>('Y'), static_cast<CharT>('-'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('m'), static_cast<CharT>('-'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('d'), CharT()};
                    out = do_put(out, subfmt.data(), ymd, wd, hms, zi, tz);
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
                out = do_put(out, m_am_pm_format, ymd, wd, hms, zi, tz);
                break;

            case static_cast<CharT>('R'):
                if (!hms || modifier) goto bad_format;
                {
                    static constexpr std::array subfmt = {static_cast<CharT>('%'), static_cast<CharT>('H'), static_cast<CharT>(':'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('M'), CharT()};
                    out = do_put(out, subfmt.data(), ymd, wd, hms, zi, tz);
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
                    static constexpr std::array subfmt = {static_cast<CharT>('%'), static_cast<CharT>('H'), static_cast<CharT>(':'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('M'), static_cast<CharT>(':'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('S'), CharT()};
                    out = do_put(out, subfmt.data(), ymd, wd, hms, zi, tz);
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
                    out = do_put(out, subFmt, ymd, wd, hms, zi, tz);
                }
                break;

            case static_cast<CharT>('X'):
                if (!hms || modifier == static_cast<CharT>('O')) goto bad_format;
                {
                    const std::basic_string<CharT>& subFmt = (modifier == static_cast<CharT>('E')) ?
                                                             m_era_time_format : m_time_format;
                    out = do_put(out, subFmt, ymd, wd, hms, zi, tz);
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
                        out = do_put(out, subfmt, ymd, wd, hms, zi, tz);
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
                if (!zi || modifier) goto bad_format;
                {
                    // Clamped before the narrowing, or a caller-supplied count too big for
                    // an int would wrap and print the wrong sign. The bound is the widest
                    // offset the %z parse branch accepts, so what we write reads back.
                    constexpr auto max_off = 23 * 3600 + 59 * 60 + 59;
                    const auto raw = zi->offset.count();
                    int val = raw >  max_off ?  max_off
                            : raw < -max_off ? -max_off
                            : static_cast<int>(raw);
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
                if (modifier) goto bad_format;
                if (tz)
                {
                    auto iana = tz->name();
                    out = std::copy(iana.begin(), iana.end(), out);
                }
                else if (zi && !zi->abbrev.empty())
                    out = std::copy(zi->abbrev.begin(), zi->abbrev.end(), out);
                else
                    goto bad_format;
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
    // NOLINTEND(cppcoreguidelines-avoid-goto)

    /**
     * @lang{ZH}
     * @brief @ref expand_format 的实现：把复合说明符换成它们的内容，把供不出的说明符
     *        连同一侧分隔符摘掉，结果追加到 @p result。
     *
     * 这个 switch 是 `do_put` 那个 switch 的镜像：那里问 `ymd` / `wd` / `hms` / `zi` / `tz`
     * 五个指针空不空，这里问对应的模板标志，那里 `goto bad_format` 的地方这里
     * `goto filter_format`。两边必须一一对应，摘掉的才正好是 put 会退化的那些。
     *
     * 复合说明符只在**供得出**时才展开，否则整个摘掉：展开一个供不出的 `%c` 只会把它的内容
     * 拆成一堆各自被摘掉的说明符，留下一串孤零零的标点。`%EY` 也不展开——纪元格式取决于年份
     * 落在哪个纪元，是值相关的。
     *
     * @tparam has_date   值是否带日期，见 @ref time_value_fields。
     * @tparam has_time   值是否带时分秒。
     * @tparam has_offset 值是否带 UTC 偏移。
     * @tparam has_zone   值是否带区域身份。
     * @param result 追加目标，尾部可能因摘分隔符被截短。
     * @param fmt    格式串。
     * @note 复合说明符的递归有界，与 `do_put` / `do_get` 同理：自引用的复合格式串在构造 facet
     *       时就被拒绝，见 `validate_format_recursion`。括号组的递归另算：它按嵌套层数递归，
     *       深度来自调用方传进来的格式串，不在上面那条保证的范围内。
     * @endif
     *
     * @lang{EN}
     * @brief The implementation of @ref expand_format: compounds replaced by their content,
     *        unsuppliable specifiers removed along with one adjacent separator, the result
     *        appended to @p result.
     *
     * This switch mirrors the one in `do_put`: there the question is whether the `ymd` / `wd` /
     * `hms` / `zi` / `tz` pointers are null, here it is the matching template flags, and where
     * that one says `goto bad_format` this one says `goto filter_format`. The two have to
     * correspond case for case, or this would not be removing exactly what put degrades.
     *
     * A compound is expanded only when it can be **supplied**, and otherwise removed whole:
     * expanding an unsuppliable `%c` would break it into a crowd of individually removed
     * specifiers and leave a trail of orphaned punctuation. `%EY` is not expanded either --
     * which era format applies depends on the year, so it is a property of the value.
     *
     * @tparam has_date   Whether the value carries a date; see @ref time_value_fields.
     * @tparam has_time   Whether it carries a time of day.
     * @tparam has_offset Whether it carries a UTC offset.
     * @tparam has_zone   Whether it carries a zone identity.
     * @param result Where output is appended; its tail may be truncated to drop a separator.
     * @param fmt    The format string.
     * @note The recursion through compound specifiers is bounded just as in `do_put` / `do_get`:
     *       a self-referential compound format is rejected when the facet is built, see
     *       `validate_format_recursion`. Bracket groups are a separate matter -- they recurse
     *       once per nesting level, so that depth comes from the caller's format string and is
     *       not covered by the guarantee above.
     * @endif
     */
    // NOLINTBEGIN(cppcoreguidelines-avoid-goto)
    template <bool has_date, bool has_time, bool has_offset, bool has_zone>
    void expand_and_filter(std::basic_string<CharT>& result, std::basic_string_view<CharT> fmt) const
    {
        auto closer_for = [](CharT c)
        {
            if (c == static_cast<CharT>('(')) return static_cast<CharT>(')');
            if (c == static_cast<CharT>('[')) return static_cast<CharT>(']');
            if (c == static_cast<CharT>('{')) return static_cast<CharT>('}');
            return CharT();
        };

        auto f = fmt.cbegin();

        /* Drop one separator adjacent to whatever was just removed, or an orphaned piece of
        punctuation is left behind: "%T %Z" has to become "%T" rather than "%T ", and
        "%m/%d/%Y %T" on a time-only value "%T" rather than "// %T". Trim backwards when
        anything precedes, forwards only at the very start. A closing bracket is never a
        separator -- its opener still stands, and eating it would orphan that one -- and
        symmetrically an opening bracket is never one going forwards. */
        auto trim_separator = [&]
        {
            if (!result.empty())
            {
                auto n = result.size();
                while (n > 0 && is_space(result[n - 1])) --n;

                if (n > 0 && is_punct(result[n - 1]) &&
                    result[n - 1] != static_cast<CharT>(')') &&
                    result[n - 1] != static_cast<CharT>(']') &&
                    result[n - 1] != static_cast<CharT>('}'))
                {
                    --n;
                    while (n > 0 && is_space(result[n - 1])) --n;
                }
                result.resize(n);
            }

            if (result.empty())
            {
                while (f != fmt.cend() && is_space(*f)) ++f;
                if (f != fmt.cend() && is_punct(*f) && closer_for(*f) == CharT())
                {
                    ++f;
                    while (f != fmt.cend() && is_space(*f)) ++f;
                }
            }
        };

        while (f != fmt.cend())
        {
            if (*f != static_cast<CharT>('%'))
            {
                /* A bracket group is all-or-nothing: filter its content on its own, and if a
                group that held something comes back empty, the brackets go with it. That is
                what turns "%T (%Z)" into "%T" and "%T (%Z, %z)" into "%T", while leaving both
                halves of a group that keeps something ("%T (%Z, x)" -> "%T (x)") and every
                bracket the format itself left unpaired ("%T :-)"). */
                const CharT closer = closer_for(*f);
                if (closer != CharT())
                {
                    const auto close_it = match_bracket(fmt, f, closer);
                    if (close_it != fmt.cend())
                    {
                        const auto content = fmt.substr(f + 1 - fmt.cbegin(), close_it - f - 1);
                        std::basic_string<CharT> inner;
                        expand_and_filter<has_date, has_time, has_offset, has_zone>(inner, content);

                        auto blank = [this](std::basic_string_view<CharT> s)
                        { return std::ranges::all_of(s, [this](CharT c) { return is_space(c); }); };

                        if (blank(inner) && !blank(content))
                        {
                            f = close_it + 1;
                            trim_separator();
                            continue;
                        }

                        result.push_back(*f);
                        result.append(inner);
                        result.push_back(closer);
                        f = close_it + 1;
                        continue;
                    }
                }
                result.push_back(*f++);
                continue;
            }

            const auto head = f;
            if (++f == fmt.cend())
            {
                result.push_back(static_cast<CharT>('%'));
                break;
            }

            CharT modifier = 0;
            if (*f == static_cast<CharT>('E') || *f == static_cast<CharT>('O'))
            {
                modifier = *f++;
                if (f == fmt.cend())
                {
                    result.append(head, f);
                    break;
                }
            }

            CharT format_char = *f;
            switch (format_char)
            {
            case static_cast<CharT>('%'):
            case static_cast<CharT>('n'):
            case static_cast<CharT>('t'):
                if (modifier) goto filter_format;
                break;

            case static_cast<CharT>('a'):
            case static_cast<CharT>('A'):
            case static_cast<CharT>('b'):
            case static_cast<CharT>('h'):
            case static_cast<CharT>('B'):
            case static_cast<CharT>('g'):
            case static_cast<CharT>('G'):
            case static_cast<CharT>('j'):
                if constexpr (!has_date) goto filter_format;
                if (modifier) goto filter_format;
                break;

            case static_cast<CharT>('C'):
            case static_cast<CharT>('Y'):
                if constexpr (!has_date) goto filter_format;
                if (modifier == static_cast<CharT>('O')) goto filter_format;
                break;

            case static_cast<CharT>('d'):
            case static_cast<CharT>('e'):
            case static_cast<CharT>('m'):
            case static_cast<CharT>('u'):
            case static_cast<CharT>('w'):
            case static_cast<CharT>('U'):
            case static_cast<CharT>('V'):
            case static_cast<CharT>('W'):
                if constexpr (!has_date) goto filter_format;
                if (modifier == static_cast<CharT>('E')) goto filter_format;
                break;

            case static_cast<CharT>('y'):
                if constexpr (!has_date) goto filter_format;
                break;

            case static_cast<CharT>('p'):
                if constexpr (!has_time) goto filter_format;
                if (modifier) goto filter_format;
                break;

            case static_cast<CharT>('H'):
            case static_cast<CharT>('I'):
            case static_cast<CharT>('M'):
            case static_cast<CharT>('S'):
                if constexpr (!has_time) goto filter_format;
                if (modifier == static_cast<CharT>('E')) goto filter_format;
                break;

            case static_cast<CharT>('z'):
                if constexpr (!has_offset) goto filter_format;
                if (modifier) goto filter_format;
                break;

            case static_cast<CharT>('Z'):
                if constexpr (!has_zone) goto filter_format;
                if (modifier) goto filter_format;
                break;

            case static_cast<CharT>('c'):
                if constexpr (!has_date || !has_time) goto filter_format;
                if (modifier == static_cast<CharT>('O')) goto filter_format;
                expand_and_filter<has_date, has_time, has_offset, has_zone>(
                    result, (modifier == static_cast<CharT>('E')) ? m_era_date_time_format
                                                                  : m_date_time_format);
                ++f;
                continue;

            case static_cast<CharT>('x'):
                if constexpr (!has_date) goto filter_format;
                if (modifier == static_cast<CharT>('O')) goto filter_format;
                expand_and_filter<has_date, has_time, has_offset, has_zone>(
                    result, (modifier == static_cast<CharT>('E')) ? m_era_date_format
                                                                  : m_date_format);
                ++f;
                continue;

            case static_cast<CharT>('X'):
                if constexpr (!has_time) goto filter_format;
                if (modifier == static_cast<CharT>('O')) goto filter_format;
                expand_and_filter<has_date, has_time, has_offset, has_zone>(
                    result, (modifier == static_cast<CharT>('E')) ? m_era_time_format
                                                                  : m_time_format);
                ++f;
                continue;

            case static_cast<CharT>('r'):
                if constexpr (!has_time) goto filter_format;
                if (modifier) goto filter_format;
                expand_and_filter<has_date, has_time, has_offset, has_zone>(result, m_am_pm_format);
                ++f;
                continue;

            case static_cast<CharT>('D'):
                if constexpr (!has_date) goto filter_format;
                if (modifier) goto filter_format;
                {
                    static constexpr std::array subfmt = {static_cast<CharT>('%'), static_cast<CharT>('m'), static_cast<CharT>('/'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('d'), static_cast<CharT>('/'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('y'), CharT()};
                    expand_and_filter<has_date, has_time, has_offset, has_zone>(result, subfmt.data());
                }
                ++f;
                continue;

            case static_cast<CharT>('F'):
                if constexpr (!has_date) goto filter_format;
                if (modifier) goto filter_format;
                {
                    static constexpr std::array subfmt = {static_cast<CharT>('%'), static_cast<CharT>('Y'), static_cast<CharT>('-'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('m'), static_cast<CharT>('-'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('d'), CharT()};
                    expand_and_filter<has_date, has_time, has_offset, has_zone>(result, subfmt.data());
                }
                ++f;
                continue;

            case static_cast<CharT>('R'):
                if constexpr (!has_time) goto filter_format;
                if (modifier) goto filter_format;
                {
                    static constexpr std::array subfmt = {static_cast<CharT>('%'), static_cast<CharT>('H'), static_cast<CharT>(':'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('M'), CharT()};
                    expand_and_filter<has_date, has_time, has_offset, has_zone>(result, subfmt.data());
                }
                ++f;
                continue;

            case static_cast<CharT>('T'):
                if constexpr (!has_time) goto filter_format;
                if (modifier) goto filter_format;
                {
                    static constexpr std::array subfmt = {static_cast<CharT>('%'), static_cast<CharT>('H'), static_cast<CharT>(':'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('M'), static_cast<CharT>(':'),
                                                          static_cast<CharT>('%'), static_cast<CharT>('S'), CharT()};
                    expand_and_filter<has_date, has_time, has_offset, has_zone>(result, subfmt.data());
                }
                ++f;
                continue;

            default:
            filter_format:
                ++f;
                trim_separator();
                continue;
            }
            result.append(head, ++f);
        }
    }
    // NOLINTEND(cppcoreguidelines-avoid-goto)

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
        for (std::size_t i = 0; i < m_era_master.size(); ++i)
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
    template <std::size_t n, CharT def = static_cast<CharT>('0'), typename OutIt>
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
     * 若 `n == 0`，按实际位数输出（不限宽度），永不截断；否则输出**恰好** `n` 位，
     * 前导位以 `def` 填充，而 `val` 高于 `n` 位的那部分被**丢弃**。因此 `n > 0` 时
     * 调用方必须自己保证 `val` 装得下 `n` 位——本库的调用点都在上游做了范围检查或钳位。
     * @tparam n   宽度（`0` 表示按实际位数、不限宽度）。
     * @tparam def 前导填充字符，默认为 `'0'`。
     * @param out 输出迭代器。
     * @param val 要输出的整数。
     * @return 写入后的输出迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Writes an integer in decimal with a width of `n` to the output iterator.
     *
     * When `n == 0`, outputs the exact number of digits (no minimum) and never truncates.
     * Otherwise it outputs **exactly** `n` digits, padding leading positions with `def` and
     * **discarding** whatever of `val` sits above the `n`th digit. With `n > 0` the caller must
     * therefore keep `val` within `n` digits; every call site in this library range-checks or
     * clamps upstream.
     * @tparam n   Width (`0` means the exact number of digits, no minimum).
     * @tparam def Leading padding character, default `'0'`.
     * @param out The output iterator.
     * @param val The integer to output.
     * @return The output iterator after writing.
     * @endif
     */
    template <std::size_t n, CharT def = static_cast<CharT>('0'), typename OutIt>
    OutIt put_dec(OutIt out, int val) const
    {
        if (val < 0) val = 0;

        if constexpr (n == 0)
        {
            std::array<char, std::numeric_limits<int>::digits10 + 1> digits{};
            int i = 0;
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
            do {
                digits[i++] = static_cast<char>('0' + val % 10);
                val /= 10;
            } while (val != 0);
            for (int j = i - 1; j >= 0; --j)
                *out++ = static_cast<CharT>(digits[j]);
        }
        else
        {
            std::array<int, n> buf;
            for (std::size_t i = 0; i < n; ++i)
            {
                buf[n - i -1] = val % 10;
                val /= 10;
            }
            bool leading = true;
            for (std::size_t i = 0; i < n; ++i)
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
    static TIter extract_num(TIter beg, TSent end, int& member, int min_val, int max_val, std::size_t len, bool& succ) // NOLINT(bugprone-easily-swappable-parameters)
    {
        std::size_t i = 0;
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
    TIter extract_num_with_alt_digits(TIter beg, TSent end, int& member, int min_val, int max_val, std::size_t len, bool& succ) const
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
    std::basic_string<CharT>                  m_date_time_format;
    std::basic_string<CharT>                  m_era_date_time_format;
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
