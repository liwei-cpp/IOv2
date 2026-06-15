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
template <typename, bool> struct date_parse_helper
{
    bool operator==(const date_parse_helper&) const = default;  // for test
};

template <typename CharT>
struct date_parse_helper<CharT, true>
{
    bool operator==(const date_parse_helper&) const = default;  // for test
    date_parse_helper()
    {
        using namespace std::chrono;
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};

        m_year = int(ymd.year());
        m_month = unsigned(ymd.month());
        m_mday = unsigned(ymd.day());
    }

    explicit operator std::chrono::year_month_day() const
    {
        using namespace std::chrono;
        if (m_have_year && m_have_mon && m_have_mday)
            return year_month_day{ year{m_year}, month{static_cast<uint8_t>(m_month)}, day{m_mday} };
        if (m_have_year && m_have_yday)
        {
            sys_days sd = sys_days{ year{m_year} / 1 / 1 } + days{ m_yday };
            return year_month_day{sd};
        }

        if (m_have_iso_8601_year && m_have_iso_8601_week && m_have_wday)
        {
            int iso_wd = (m_wday == 0 ? 7 : m_wday);
            year_month_day jan4 = year{m_iso_8601_year}/January/4;
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
                deduced_year = deduced_year % 100 + m_century * 100;
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
    uint8_t m_month = 1;        // months since January – [​1, 12]
    uint8_t m_iso_8601_week = 0;
    uint8_t m_week_no = 0;
    uint8_t m_mday = 1;         // day of the month – [​1​, 31]
    uint8_t m_wday = 0;         // days since Sunday – [​0​, 6]
    unsigned short m_yday = 0;  // days since January 1 – [​0​, 365]

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

template <bool> struct time_parse_helper
{
    bool operator==(const time_parse_helper&) const = default;  // for test
};

template <>
struct time_parse_helper<true>
{
    bool operator==(const time_parse_helper&) const = default;  // for test
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

    uint8_t m_hour = 0;         // hours since midnight – [​0​, 23]
    uint8_t m_minute = 0;       // minutes after the hour – [​0​, 59]
    uint8_t m_second = 0;       // seconds after the minute – [​0​, 59]
    bool m_have_I : 1 = false;
    bool m_is_pm : 1 = false;
};

template <bool> struct time_zone_parse_helper
{
    bool operator==(const time_zone_parse_helper&) const = default;  // for test
};

template <>
struct time_zone_parse_helper<true>
{
    bool operator==(const time_zone_parse_helper&) const = default;  // for test
    explicit operator const std::chrono::time_zone*() const
    {
        try
        {
            return std::chrono::locate_zone(m_zone_name);
        }
        catch(...)
        {
            try
            {
                return std::chrono::locate_zone("UTC");
            }
            catch(...)
            {
                throw stream_error(
                    "timeio parse error: no usable time zone (tz database unavailable)");
            }
        }
    }
    std::string m_zone_name;
};

template <typename CharT, bool HaveDate = true, bool HaveTime = true, bool HaveTimeZone = true>
struct time_parse_context
    : date_parse_helper<CharT, HaveDate>
    , time_parse_helper<HaveTime>
    , time_zone_parse_helper<HaveTimeZone>
{
    bool operator==(const time_parse_context&) const = default;

    time_parse_context() = default;

    // Clears all accumulated parse state (date / time / time-zone fields, the
    // have_* flags, the era working set and the is_init guard), restoring the
    // context to its default-constructed state. Call this before reusing one
    // context to parse a *different* time value; skip it to keep accumulating
    // fields of the *same* value across multiple get() calls.
    void reset() { *this = time_parse_context{}; }

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

    explicit operator std::tm() const
        requires(HaveDate && HaveTime)
    {
        using namespace std::chrono;
        auto ymd = static_cast<year_month_day>(*this);
        auto hms = static_cast<std::chrono::hh_mm_ss<std::chrono::seconds>>(*this);

        int d = unsigned(ymd.day());
        int m = unsigned(ymd.month());
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

template <typename CharT>
class timeio
{
    using era_entry = typename ft_basic<timeio<CharT>>::era_entry;

public:
    using create_rules =
        facet_create_rule<facet_create_pack<timeio_conf<CharT>, ctype<CharT>>,
                          timeio_conf<CharT>>;

    using char_type = CharT;

    template <shared_ptr_to<timeio_conf<CharT>> TConfPtr,
              shared_ptr_to<ctype<CharT>> TCtypePtr>
    timeio(TConfPtr p_obj, TCtypePtr p_ctype)
        : timeio(p_obj)
    {
        if (!p_ctype) throw std::runtime_error("shared_ptr is empty");
        m_ctype = p_ctype;
    }

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
    const std::array<std::basic_string<CharT>, 7>& day_names() const noexcept { return m_day; }
    const std::array<std::basic_string<CharT>, 7>& abbr_day_names() const noexcept { return m_abbr_day; }
    const std::array<std::basic_string<CharT>, 12>& month_names() const noexcept { return m_month; }
    const std::array<std::basic_string<CharT>, 12>& abbr_month_names() const noexcept { return m_abbr_month; }
    const std::array<std::basic_string<CharT>, 100>& alt_digit_names() const noexcept { return m_alt_digits; }
    const std::basic_string<CharT>& am_name() const noexcept { return m_am; }
    const std::basic_string<CharT>& pm_name() const noexcept { return m_pm; }
    const std::basic_string<CharT>& date_format() const noexcept { return m_date_format; }
    const std::basic_string<CharT>& era_date_format() const noexcept { return m_era_date_format; }
    const std::basic_string<CharT>& time_format() const noexcept { return m_time_format; }
    const std::basic_string<CharT>& era_time_format() const noexcept { return m_era_time_format; }
    const std::basic_string<CharT>& date_time_format() const noexcept { return m_date_time_format; }
    const std::basic_string<CharT>& era_date_time_format() const noexcept { return m_era_date_time_format; }
    const std::basic_string<CharT>& am_pm_format() const noexcept { return m_am_pm_format; }

    template <typename OutIt, typename TVal>
    OutIt put(OutIt out, const TVal& t, char format, char modifier = 0) const
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

    template <typename OutIt, typename Duration, typename TimeZonePtr>
    OutIt put(OutIt out, const std::chrono::zoned_time<Duration, TimeZonePtr>& t, std::basic_string_view<CharT> fmt) const
    {
        auto local = t.get_local_time();
        auto local_day = std::chrono::floor<std::chrono::days>(local);

        std::chrono::year_month_day ymd{local_day};
        if (!ymd.ok())
            throw stream_error("timeio put error: zoned_time date is out of range");

        std::chrono::weekday wd(local_day);

        auto time_since_midnight = local - local_day;
        std::chrono::hh_mm_ss time_of_day{time_since_midnight};
        return do_put(out, fmt, &ymd, &wd, &time_of_day, t.get_time_zone());
    }

    template <typename OutIt>
    OutIt put(OutIt out, const std::chrono::year_month_day& t, std::basic_string_view<CharT> fmt) const
    {
        if (!t.ok())
            throw stream_error("timeio put error: year_month_day is not a valid calendar date");

        std::chrono::weekday wd(t);
        return do_put(out, fmt, &t, &wd, nullptr, nullptr);
    }

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

    template <typename TIter, std::sentinel_for<TIter> TSent, bool HaveDate, bool HaveTime, bool HaveTimeZone>
        requires (std::bidirectional_iterator<TIter> || is_istreambuf_iterator<TIter>)
    TIter get(TIter beg, TSent end, time_parse_context<char_type, HaveDate, HaveTime, HaveTimeZone>& ctx,
              char format, char modifier = 0) const
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
    // RECURSION / TRUSTED-LOCALE ASSUMPTION (applies to do_get and do_put):
    //   Several compound specifiers expand a locale-provided format string and
    //   re-enter this function recursively:
    //     %c -> m_[era_]date_time[_zone]_format   %x -> m_[era_]date_format
    //     %X -> m_[era_]time[_zone]_format        %r -> m_am_pm_format
    //     %EY -> era->format (via m_era_formats)
    //   Those strings come verbatim from the locale database (nl_langinfo / the
    //   glibc era table) and are TRUSTED to be non self-referential: e.g.
    //   D_T_FMT must not itself contain %c, an era format must not contain %EY,
    //   and so on. No real locale violates this.
    //
    //   There is deliberately NO recursion-depth guard. The recursion is bounded
    //   ONLY by that trust, NOT by the input: on these specifiers the recursive
    //   call is made with the *same* rp (no input is consumed between dispatch
    //   and recursion), so the `rp != rp_end` loop guard does not terminate a
    //   self-referential format -- any non-empty input would recurse until the
    //   stack overflows. A hand-crafted/corrupted locale is therefore the only
    //   way to trigger unbounded recursion, and it is consciously left to the
    //   same trust boundary as the rest of the locale data (see
    //   timeio_details.h parse_glibc_era_entries INPUT CONTRACT). Hardening it
    //   would require threading a depth budget through every recursive call site.
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
                else if (modifier) goto bad_parse_format;
                else
                {
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
                else if (modifier) goto bad_parse_format;
                else
                {
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
                else if (modifier == static_cast<CharT>('O')) goto bad_parse_format;
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
                break;

            case static_cast<CharT>('C'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else if (modifier == static_cast<CharT>('O')) goto bad_parse_format;
                else if ((modifier == static_cast<CharT>('\0')) || (ctx.m_era_items.empty()))
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
                break;

            case static_cast<CharT>('d'):
            case static_cast<CharT>('e'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                else
                {
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

            case static_cast<CharT>('D'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else if (modifier) goto bad_parse_format;
                else {
                    CharT subfmt[] = {static_cast<CharT>('%'), static_cast<CharT>('m'), static_cast<CharT>('/'),
                                      static_cast<CharT>('%'), static_cast<CharT>('d'), static_cast<CharT>('/'),
                                      static_cast<CharT>('%'), static_cast<CharT>('y'), CharT()};
                    rp = do_get(rp, rp_end, ctx, succ, subfmt);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('F'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else if (modifier) goto bad_parse_format;
                else
                {
                    CharT subfmt[] = {static_cast<CharT>('%'), static_cast<CharT>('Y'), static_cast<CharT>('-'),
                                      static_cast<CharT>('%'), static_cast<CharT>('m'), static_cast<CharT>('-'),
                                      static_cast<CharT>('%'), static_cast<CharT>('d'), CharT()};
                    rp = do_get(rp, rp_end, ctx, succ, subfmt);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('g'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else if (modifier) goto bad_parse_format;
                else
                {
                    int val = 0;
                    rp = extract_num(rp, rp_end, val, 0, 99, 2, succ);
                    if (!succ) return rp;
                    ctx.m_iso_8601_year = val >= 69 ? val + 1900 : val + 2000;
                    ctx.m_have_iso_8601_year = true;
                }
                break;
            case static_cast<CharT>('G'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else if (modifier) goto bad_parse_format;
                else
                {
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
                else if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                else
                {
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
                else if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                else
                {
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
                else if (modifier) goto bad_parse_format;
                else
                {
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
                else if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                else
                {
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
                else if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                else
                {
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
                else if (modifier) goto bad_parse_format;
                else
                {
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
                else if (modifier) goto bad_parse_format;
                else
                {
                    rp = do_get(rp, rp_end, ctx, succ, m_am_pm_format);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('R'):
                if constexpr (!HaveTime) goto bad_parse_format;
                else if (modifier) goto bad_parse_format;
                else
                {
                    CharT subfmt[] = {static_cast<CharT>('%'), static_cast<CharT>('H'), static_cast<CharT>(':'),
                                      static_cast<CharT>('%'), static_cast<CharT>('M'), CharT()};
                    rp = do_get(rp, rp_end, ctx, succ, subfmt);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('S'):
                if constexpr (!HaveTime) goto bad_parse_format;
                else if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                else
                {
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
                else if (modifier) goto bad_parse_format;
                else
                {
                    CharT subfmt[] = {static_cast<CharT>('%'), static_cast<CharT>('H'), static_cast<CharT>(':'),
                                      static_cast<CharT>('%'), static_cast<CharT>('M'), static_cast<CharT>(':'),
                                      static_cast<CharT>('%'), static_cast<CharT>('S'), CharT()};
                    rp = do_get(rp, rp_end, ctx, succ, subfmt);
                    if (!succ) return rp;
                }
                break;

            case static_cast<CharT>('u'):
                if constexpr (!HaveDate) goto bad_parse_format;
                else if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                else
                {
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
                else if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                else
                {
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
                else if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                else
                {
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
                else if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                else
                {
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
                else if (modifier == static_cast<CharT>('E')) goto bad_parse_format;
                else
                {
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
                else if (modifier == static_cast<CharT>('O')) goto bad_parse_format;
                else if (modifier == static_cast<CharT>('E'))
                    rp = do_get(rp, rp_end, ctx, succ, m_era_date_format);
                else
                    rp = do_get(rp, rp_end, ctx, succ, m_date_format);
                if (!succ) return rp;
                break;

            case static_cast<CharT>('X'):
                if constexpr (!HaveTime) goto bad_parse_format;
                else if (modifier == static_cast<CharT>('O')) goto bad_parse_format;
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
                else if (modifier == static_cast<CharT>('O')) goto bad_parse_format;
                else if ((modifier == static_cast<CharT>('E')) && (!ctx.m_era_items.empty()))
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
                break;

            case static_cast<CharT>('z'):
                if constexpr (!HaveDate || !HaveTime || !HaveTimeZone) goto bad_parse_format;
                else if (modifier) goto bad_parse_format;
                /* We recognize four formats:
                    1. Two digits specify hours.
                    2. Four digits specify hours and minutes.
                    3. Two digits, ':', and two digits specify hours and minutes.
                    4. 'Z' is equivalent to +0000.
                */
                // In C++, there is no way to store timezone offset in a standard structure.
                // so we just omit the parse result
                else
                {
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
                    else if (val % 100 >= 60)
                    {
                        succ = false; return rp;
                    }
                }
                break;

            case static_cast<CharT>('Z'):
                if constexpr (!HaveTimeZone) goto bad_parse_format;
                else if (modifier) goto bad_parse_format;
                /* Read timezone but perform no conversion.  */
                else
                {
                    typename decltype(ft_basic<timeio<CharT>>::s_timezone_tree)::match_out_type zone_res;
                    rp = ft_basic<timeio<CharT>>::s_timezone_tree.max_match(rp, rp_end, zone_res);
                    if (!zone_res)
                    {
                        succ = false;
                        return rp;
                    }
                    else if ((*zone_res)[0] != (CharT)'*')
                        ctx.m_zone_name = *zone_res;
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

        // A format tail consisting solely of %n / %t matches zero-or-more
        // whitespace and is satisfied even at end of input, matching POSIX
        // strptime and std::get_time. Skip such a tail before judging failure.
        while (fmt != _fmt.cend())
        {
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

    bool is_space(CharT c) const
    {
        if (m_ctype)
            return m_ctype->is_any(base_ft<ctype>::space, c);
        return c == static_cast<CharT>(' ')  || c == static_cast<CharT>('\t') ||
               c == static_cast<CharT>('\n') || c == static_cast<CharT>('\v') ||
               c == static_cast<CharT>('\f') || c == static_cast<CharT>('\r');
    }

    // Throws std::runtime_error if any of the 2*N names is empty, or if one
    // spelling is shared by two *different* indices. full[i] == abbr[i] (same
    // index, e.g. "May" in en_US) is allowed.
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

    void create_alt_digits_tree()
    {
        for (size_t i = 0; i < 100; ++i)
        {
            if (!m_alt_digits[i].empty())
                m_alt_digits_tree.add(m_alt_digits[i], i);
        }
    }

    // The compound specifiers %c / %x / %X / %r / %EY recurse into a
    // locale-provided format string here, just as in do_get. The same
    // trusted-locale, no-depth-guard assumption applies: a self-referential
    // locale format string (e.g. D_T_FMT == "%c") would recurse without bound.
    // See the RECURSION / TRUSTED-LOCALE ASSUMPTION note above do_get.
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
                    std::chrono::sys_days thursday = sd + std::chrono::days{4 - wd->iso_encoding()};
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
                    int doy = (sd - jan1).count();
                    int wday = wd->c_encoding();
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
                    int val = wd->c_encoding();
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
                    int doy = (sd - jan1).count();
                    int wday_monday = (wd->c_encoding() + 6) % 7;
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
                            out = put_dec<0>(out, static_cast<int>(std::clamp<int64_t>(v,
                                std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
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
                    int val = tz->get_info(lt).first.offset.count();
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

    template <typename TIter, std::sentinel_for<TIter> TSent>
    static TIter extract_num(TIter beg, TSent end, int& member, int min_val, int max_val, size_t len, bool& succ)
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

template<typename TConfPtr, typename TCtypePtr>
    requires (std::is_same_v<typename TConfPtr::element_type::char_type,
                             typename TCtypePtr::element_type::char_type>)
timeio(TConfPtr, TCtypePtr) -> timeio<typename TConfPtr::element_type::char_type>;

template<typename TConfPtr>
timeio(TConfPtr) -> timeio<typename TConfPtr::element_type::char_type>;
}
