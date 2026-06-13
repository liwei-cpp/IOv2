#pragma once
#include <common/metafunctions.h>
#include <common/prefix_tree.h>
#include <cvt/cvt_facilities.h>
#include <facet/facet_common.h>
#include <facet/facet_helper.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <langinfo.h>

namespace IOv2
{
    template <typename CharT> class timeio;

    template <typename CharT>
    class ft_basic<timeio<CharT>> : public base_ft<timeio>
    {
    public:
        struct era_entry
        {
            std::basic_string<CharT> name;
            std::basic_string<CharT> format;
            int32_t from_year;
            uint8_t from_month;
            uint8_t from_day;
            int32_t to_year;
            uint8_t to_month;
            uint8_t to_day;
            int32_t offset;
            /* direction:
                +1 indicates that year number is higher in the future. (like A.D.)
                -1 indicates that year number is higher in the past. (like B.C.)  */
            int8_t direction;

            bool operator==(const era_entry&) const = default; // for test.
        };
    public:
        ft_basic()
            : base_ft<timeio>(id()) {}

        using char_type = CharT;

        static size_t id() { return reinterpret_cast<size_t>(&s_id); }

    public:
        inline static const prefix_tree<CharT, std::string> s_timezone_tree =
        []()
        {
            prefix_tree<CharT, std::string> res;

            const auto& tzdb = std::chrono::get_tzdb();

            for (const auto& zone : tzdb.zones)
            {
                std::string full_name{zone.name()};
                std::string abbr_name = zone.get_info(std::chrono::sys_time<std::chrono::seconds>{}).abbrev;
                res.add(abbr_name.begin(), abbr_name.end(), "*");
                if (full_name != abbr_name)
                    res.add(full_name.begin(), full_name.end(), full_name);
            }
            return res;
        }();

    private:
        inline static const void* s_id = nullptr;
    };

namespace TimeioHelper
{
    inline bool era_small_or_equal(int from_year, uint8_t from_month, uint8_t from_day,
                                   int to_year, uint8_t to_month, uint8_t to_day)
    {
        if (from_year < to_year) return true;
        if (from_year > to_year) return false;
        if (from_month < to_month) return true;
        if (from_month > to_month) return false;
        return from_day <= to_day;
    }

    inline bool era_small_or_equal(int from_year, uint8_t from_month,
                                   int to_year, uint8_t to_month)
    {
        if (from_year < to_year) return true;
        if (from_year > to_year) return false;
        return from_month <= to_month;
    }
}

template <typename CharT> class timeio_conf;

template <>
class timeio_conf<char> : public ft_basic<timeio<char>>
{
public:
    timeio_conf(const std::string& name)
        : ft_basic<timeio<char>>()
    {
        if ((name == "C") || (name == "POSIX"))
        {
            m_date_format = "%m/%d/%y";     m_era_date_format = m_date_format;
            m_time_format = "%H:%M:%S";     m_era_time_format = m_time_format;

            m_date_time_format = m_date_format + ' ' + m_time_format;
            m_era_date_time_format = m_date_time_format;

            m_am = "AM";
            m_pm = "PM";
            m_am_pm_format = "%I:%M:%S %p";

            m_day[0] = "Sunday";
            m_day[1] = "Monday";
            m_day[2] = "Tuesday";
            m_day[3] = "Wednesday";
            m_day[4] = "Thursday";
            m_day[5] = "Friday";
            m_day[6] = "Saturday";

            m_abbr_day[0] = "Sun";
            m_abbr_day[1] = "Mon";
            m_abbr_day[2] = "Tue";
            m_abbr_day[3] = "Wed";
            m_abbr_day[4] = "Thu";
            m_abbr_day[5] = "Fri";
            m_abbr_day[6] = "Sat";

            // Month names, starting with "C"'s January.
            m_month[0]  = "January";
            m_month[1]  = "February";
            m_month[2]  = "March";
            m_month[3]  = "April";
            m_month[4]  = "May";
            m_month[5]  = "June";
            m_month[6]  = "July";
            m_month[7]  = "August";
            m_month[8]  = "September";
            m_month[9]  = "October";
            m_month[10] = "November";
            m_month[11] = "December";

            // Abbreviated month names, starting with "C"'s Jan.
            m_abbr_month[0]  = "Jan";
            m_abbr_month[1]  = "Feb";
            m_abbr_month[2]  = "Mar";
            m_abbr_month[3]  = "Apr";
            m_abbr_month[4]  = "May";
            m_abbr_month[5]  = "Jun";
            m_abbr_month[6]  = "Jul";
            m_abbr_month[7]  = "Aug";
            m_abbr_month[8]  = "Sep";
            m_abbr_month[9]  = "Oct";
            m_abbr_month[10] = "Nov";
            m_abbr_month[11] = "Dec";
        }
        else
        {
            clocale_wrapper inter_locale(name.c_str());
            clocale_user guard(inter_locale);

            m_date_format = nl_langinfo(D_FMT);
            m_era_date_format = nl_langinfo(ERA_D_FMT);  if (m_era_date_format.empty()) m_era_date_format = m_date_format;
            m_time_format = nl_langinfo(T_FMT);
            m_era_time_format = nl_langinfo(ERA_T_FMT);  if (m_era_time_format.empty()) m_era_time_format = m_time_format;

            m_date_time_format = nl_langinfo(D_T_FMT);
            m_era_date_time_format = nl_langinfo(ERA_D_T_FMT);
            if (m_era_date_time_format.empty()) m_era_date_time_format = m_date_time_format;

            m_am = nl_langinfo(AM_STR);
            m_pm = nl_langinfo(PM_STR);
            m_am_pm_format = nl_langinfo(T_FMT_AMPM);

            m_day[0] = nl_langinfo(DAY_1);
            m_day[1] = nl_langinfo(DAY_2);
            m_day[2] = nl_langinfo(DAY_3);
            m_day[3] = nl_langinfo(DAY_4);
            m_day[4] = nl_langinfo(DAY_5);
            m_day[5] = nl_langinfo(DAY_6);
            m_day[6] = nl_langinfo(DAY_7);

            m_abbr_day[0] = nl_langinfo(ABDAY_1);
            m_abbr_day[1] = nl_langinfo(ABDAY_2);
            m_abbr_day[2] = nl_langinfo(ABDAY_3);
            m_abbr_day[3] = nl_langinfo(ABDAY_4);
            m_abbr_day[4] = nl_langinfo(ABDAY_5);
            m_abbr_day[5] = nl_langinfo(ABDAY_6);
            m_abbr_day[6] = nl_langinfo(ABDAY_7);

            // Month names, starting with "C"'s January.
            m_month[0]  = nl_langinfo(MON_1);
            m_month[1]  = nl_langinfo(MON_2);
            m_month[2]  = nl_langinfo(MON_3);
            m_month[3]  = nl_langinfo(MON_4);
            m_month[4]  = nl_langinfo(MON_5);
            m_month[5]  = nl_langinfo(MON_6);
            m_month[6]  = nl_langinfo(MON_7);
            m_month[7]  = nl_langinfo(MON_8);
            m_month[8]  = nl_langinfo(MON_9);
            m_month[9]  = nl_langinfo(MON_10);
            m_month[10] = nl_langinfo(MON_11);
            m_month[11] = nl_langinfo(MON_12);

            // Abbreviated month names, starting with "C"'s Jan.
            m_abbr_month[0]  = nl_langinfo(ABMON_1);
            m_abbr_month[1]  = nl_langinfo(ABMON_2);
            m_abbr_month[2]  = nl_langinfo(ABMON_3);
            m_abbr_month[3]  = nl_langinfo(ABMON_4);
            m_abbr_month[4]  = nl_langinfo(ABMON_5);
            m_abbr_month[5]  = nl_langinfo(ABMON_6);
            m_abbr_month[6]  = nl_langinfo(ABMON_7);
            m_abbr_month[7]  = nl_langinfo(ABMON_8);
            m_abbr_month[8]  = nl_langinfo(ABMON_9);
            m_abbr_month[9]  = nl_langinfo(ABMON_10);
            m_abbr_month[10] = nl_langinfo(ABMON_11);
            m_abbr_month[11] = nl_langinfo(ABMON_12);

            {// alternative digits
                char* ptr = nl_langinfo(ALT_DIGITS);
                for (size_t i = 0; i < 100; ++i)
                {
                    if (*ptr == '\0') break;
                    m_alt_digits[i] = ptr;
                    ptr += m_alt_digits[i].size() + 1;
                }
            }
            // Must run with the target locale active on this thread (the
            // clocale_user guard above ensures that); see the function's
            // contract for the trusted glibc layout it assumes.
            m_era_items = parse_glibc_era_entries();
        }

        m_date_time_zone_format = m_date_time_format +" %Z";
        m_era_date_time_zone_format = m_era_date_time_format +" %Z";
        m_time_zone_format = m_time_format + " %Z";
        m_era_time_zone_format = m_era_time_format + " %Z";
    }

    virtual const std::array<std::string, 7>& day_names() const { return m_day; }
    virtual const std::array<std::string, 7>& abbr_day_names() const { return m_abbr_day; }
    virtual const std::array<std::string, 12>& month_names() const { return m_month; }
    virtual const std::array<std::string, 12>& abbr_month_names() const { return m_abbr_month; }
    virtual const std::array<std::string, 100>& alt_digit_names() const { return m_alt_digits; }
    virtual const std::string& am_name() const { return m_am; }
    virtual const std::string& pm_name() const { return m_pm; }
    virtual const std::string& date_format() const { return m_date_format; }
    virtual const std::string& era_date_format() const { return m_era_date_format; }
    virtual const std::string& time_format() const { return m_time_format; }
    virtual const std::string& era_time_format() const { return m_era_time_format; }
    virtual const std::string& time_zone_format() const { return m_time_zone_format; }
    virtual const std::string& era_time_zone_format() const { return m_era_time_zone_format; }
    virtual const std::string& date_time_format() const { return m_date_time_format; }
    virtual const std::string& era_date_time_format() const { return m_era_date_time_format; }
    virtual const std::string& date_time_zone_format() const { return m_date_time_zone_format; }
    virtual const std::string& era_date_time_zone_format() const { return m_era_date_time_zone_format; }
    virtual const std::string& am_pm_format() const { return m_am_pm_format; }
    virtual const std::vector<era_entry>& era_items() const { return m_era_items; }

private:
    // Parse the glibc-specific binary era table for the *currently active thread
    // locale* and return the decoded entries.
    //
    // PRECONDITION
    //   The caller must have made the target locale current on this thread
    //   (e.g. via clocale_user / uselocale) before calling. nl_langinfo() reads
    //   that active locale; the returned pointers are consumed before any further
    //   nl_langinfo() call, so they stay valid for the duration of this function.
    //
    // INPUT CONTRACT (TRUSTED, NOT VALIDATED)
    //   This is the single place where we trust the glibc era binary layout.
    //   nl_langinfo(_NL_TIME_ERA_ENTRIES) is assumed to point at exactly
    //   _NL_TIME_ERA_NUM_ENTRIES consecutive, well-formed records; the pointer
    //   walk below is UNCHECKED, so malformed or truncated data (e.g. a tampered
    //   locale database) can cause out-of-bounds reads. Data coming from the
    //   system locale database is treated as trusted, so no defensive bounds
    //   checking is performed. If era data could ever originate from an
    //   untrusted source, this function is the one boundary to harden.
    //
    //   Each record, relative to its start (base_ptr), is laid out as:
    //     - 8 x int32 header (read via memcpy):
    //         [0] direction marker ('+' / '-')   [1] offset
    //         [2] from_year - 1900  [3] from_month - 1  [4] from_day
    //         [5] to_year   - 1900  [6] to_month   - 1  [7] to_day
    //     - NUL-terminated narrow name, then NUL-terminated narrow format
    //     - padding up to the next 4-byte boundary (relative to base_ptr)
    //     - NUL-terminated wide name, then NUL-terminated wide format
    //
    // OUTPUT INVARIANTS
    //   - from_year / to_year are clamped against int32 overflow on the +1900
    //     addition (an open-ended "to" is normalised to Dec 31 of int32 max).
    //   - direction is normalised to exactly +1 / -1 so that the linear formula
    //
    //       era_year = offset + (calendar_year - from_year) * direction
    //
    //     is uniformly valid for all era types.  The normalisation mirrors glibc
    //     era.c (≈ line 98, https://github.com/lattera/glibc/blob/master/time/era.c):
    //
    //       if ERA_DATE_CMP(start, stop)   /* start < stop — forward era */
    //           absolute_direction = ('+') ? +1 : -1;
    //       else                           /* start ≥ stop — backward era */
    //           absolute_direction = ('+') ? -1 : +1;   /* sign flipped */
    //
    //     For forward eras (from < to, e.g. AD / Japanese / Thai), the raw
    //     direction marker maps directly: '+' → +1, '-' → -1.
    //
    //     For backward eras (from > to), the marker is intentionally inverted.
    //     The reason: in a backward era (e.g. the hypothetical BC entry
    //     "-:1:-0001/01/01:-*:BC:…"), from_year is the epoch (1 BC) and to_year
    //     is a sentinel for "negative infinity".  As the calendar year moves away
    //     from the epoch toward the past, (calendar - from_year) grows increasingly
    //     negative.  Flipping the stored direction to +1 preserves the sign
    //     convention that the same formula produces consistent era_year values
    //     regardless of which direction the era flows.  Concretely, with
    //     direction = +1 after the flip, delta = (calendar - from_year) × 1 is
    //     negative for any BC year other than 1 BC, so %Ey emits 1, 0, −1, …
    //     rather than the traditional 1, 2, 3, … counting.  This is a known
    //     limitation shared with glibc: no real glibc locale defines a backward
    //     era, so the path is never exercised in practice.
    static std::vector<era_entry> parse_glibc_era_entries()
    {
        std::vector<era_entry> items;

        const int32_t era_item_num = static_cast<int32_t>(
            reinterpret_cast<uintptr_t>(nl_langinfo(_NL_TIME_ERA_NUM_ENTRIES)));
        if (era_item_num <= 0)
            return items;

        items.reserve(era_item_num);
        const char *ptr = reinterpret_cast<const char*>(nl_langinfo(_NL_TIME_ERA_ENTRIES));
        for (int32_t cnt = 0; cnt < era_item_num; ++cnt)
        {
            const char *base_ptr = ptr;
            era_entry cur_entry;

            int32_t buf[8];
            std::memcpy(static_cast<void*>(buf), static_cast<const void*>(ptr), sizeof(int32_t) * 8);
            ptr += sizeof(uint32_t) * 8;

            if (buf[2] > std::numeric_limits<int32_t>::max() - 1900)
                cur_entry.from_year = std::numeric_limits<int32_t>::max();
            else
                cur_entry.from_year = buf[2] + 1900;
            cur_entry.from_month = buf[3] + 1;
            cur_entry.from_day = buf[4];
            if (buf[5] > std::numeric_limits<int32_t>::max() - 1900)
            {
                cur_entry.to_year = std::numeric_limits<int32_t>::max();
                cur_entry.to_month = 12;
                cur_entry.to_day = 31;
            }
            else
            {
                cur_entry.to_year = buf[5] + 1900;
                cur_entry.to_month = buf[6] + 1;
                cur_entry.to_day = buf[7];
            }

            // Normalise direction to match glibc's absolute_direction (era.c ~line 98).
            // Forward era (from <= to): marker maps directly.
            // Backward era (from > to): marker is flipped so that the linear formula
            //   era_year = offset + (calendar - from_year) * direction
            // stays consistent; see OUTPUT INVARIANTS above for the full rationale.
            if (TimeioHelper::era_small_or_equal(cur_entry.from_year, cur_entry.from_month, cur_entry.from_day,
                                                cur_entry.to_year, cur_entry.to_month, cur_entry.to_day))
            {
                if (buf[0] == (uint32_t) '+') cur_entry.direction = 1;
                else cur_entry.direction = -1;
            }
            else
            {
                if (buf[0] == (uint32_t) '+') cur_entry.direction = -1;
                else cur_entry.direction = 1;
            }
            cur_entry.offset = buf[1];

            cur_entry.name = ptr; ptr = strchr(ptr, '\0') + 1;
            cur_entry.format = ptr; ptr = strchr(ptr, '\0') + 1;

            // skip wchar_t name and format
            ptr += 3 - (((ptr - base_ptr) + 3) & 3);
            ptr = reinterpret_cast<const char*>(wcschr(reinterpret_cast<const wchar_t*>(ptr), L'\0') + 1);
            ptr = reinterpret_cast<const char*>(wcschr(reinterpret_cast<const wchar_t*>(ptr), L'\0') + 1);

            items.push_back(std::move(cur_entry));
        }
        return items;
    }

    std::array<std::string, 7>   m_day;
    std::array<std::string, 7>   m_abbr_day;
    std::array<std::string, 12>  m_month;
    std::array<std::string, 12>  m_abbr_month;
    std::array<std::string, 100> m_alt_digits;
    std::string                  m_am;
    std::string                  m_pm;
    std::string                  m_date_format;
    std::string                  m_era_date_format;
    std::string                  m_time_format;
    std::string                  m_era_time_format;
    std::string                  m_time_zone_format;
    std::string                  m_era_time_zone_format;
    std::string                  m_date_time_format;
    std::string                  m_era_date_time_format;
    std::string                  m_date_time_zone_format;
    std::string                  m_era_date_time_zone_format;
    std::string                  m_am_pm_format;
    std::vector<era_entry>       m_era_items;
};

template <typename CharT>
    requires std::is_same_v<CharT, wchar_t> ||
            (std::is_same_v<CharT, char32_t> &&
            wchar_t_is_utf32)
class timeio_conf<CharT> : public ft_basic<timeio<CharT>>
{
    using era_entry = typename ft_basic<timeio<CharT>>::era_entry;

public:
    timeio_conf(const std::string& name)
        : ft_basic<timeio<CharT>>()
    {
        timeio_conf<char> tmp_obj(name);

        auto convert = [&name](const std::string& src) -> std::basic_string<CharT>
        {
            if constexpr (std::is_same_v<CharT, wchar_t>)
                return detail::to_wstring(src.c_str(), name);
            else
                return detail::to_u32string(src.c_str(), name);
        };

        m_date_format = convert(tmp_obj.date_format());
        m_era_date_format = convert(tmp_obj.era_date_format());
        m_time_format = convert(tmp_obj.time_format());
        m_era_time_format = convert(tmp_obj.era_time_format());
        m_date_time_format = convert(tmp_obj.date_time_format());
        m_era_date_time_format = convert(tmp_obj.era_date_time_format());

        m_am = convert(tmp_obj.am_name());
        m_pm = convert(tmp_obj.pm_name());
        m_am_pm_format = convert(tmp_obj.am_pm_format());

        for (size_t i = 0; i < 7; ++i)
        {
            m_day[i] = convert(tmp_obj.day_names()[i]);
            m_abbr_day[i] = convert(tmp_obj.abbr_day_names()[i]);
        }

        for (size_t i = 0; i < 12; ++i)
        {
            m_month[i] = convert(tmp_obj.month_names()[i]);
            m_abbr_month[i] = convert(tmp_obj.abbr_month_names()[i]);
        }

        for (size_t i = 0; i < 100; ++i)
        {
            m_alt_digits[i] = convert(tmp_obj.alt_digit_names()[i]);
        }

        const auto& tmp_era = tmp_obj.era_items();
        if (!tmp_era.empty())
        {
            m_era_items.reserve(tmp_era.size());
            for (const auto& src : tmp_era)
            {
                era_entry cur_entry;
                cur_entry.name = convert(src.name);
                cur_entry.format = convert(src.format);
                cur_entry.from_year = src.from_year;
                cur_entry.from_month = src.from_month;
                cur_entry.from_day = src.from_day;
                cur_entry.to_year = src.to_year;
                cur_entry.to_month = src.to_month;
                cur_entry.to_day = src.to_day;
                cur_entry.offset = src.offset;
                cur_entry.direction = src.direction;
                m_era_items.push_back(std::move(cur_entry));
            }
        }

        m_time_zone_format = convert(tmp_obj.time_zone_format());
        m_era_time_zone_format = convert(tmp_obj.era_time_zone_format());
        m_date_time_zone_format = convert(tmp_obj.date_time_zone_format());
        m_era_date_time_zone_format = convert(tmp_obj.era_date_time_zone_format());
    }

    virtual const std::array<std::basic_string<CharT>, 7>& day_names() const { return m_day; }
    virtual const std::array<std::basic_string<CharT>, 7>& abbr_day_names() const { return m_abbr_day; }
    virtual const std::array<std::basic_string<CharT>, 12>& month_names() const { return m_month; }
    virtual const std::array<std::basic_string<CharT>, 12>& abbr_month_names() const { return m_abbr_month; }
    virtual const std::array<std::basic_string<CharT>, 100>& alt_digit_names() const { return m_alt_digits; }
    virtual const std::basic_string<CharT>& am_name() const { return m_am; }
    virtual const std::basic_string<CharT>& pm_name() const { return m_pm; }
    virtual const std::basic_string<CharT>& date_format() const { return m_date_format; }
    virtual const std::basic_string<CharT>& era_date_format() const { return m_era_date_format; }
    virtual const std::basic_string<CharT>& time_format() const { return m_time_format; }
    virtual const std::basic_string<CharT>& era_time_format() const { return m_era_time_format; }
    virtual const std::basic_string<CharT>& time_zone_format() const { return m_time_zone_format; }
    virtual const std::basic_string<CharT>& era_time_zone_format() const { return m_era_time_zone_format; }
    virtual const std::basic_string<CharT>& date_time_format() const { return m_date_time_format; }
    virtual const std::basic_string<CharT>& era_date_time_format() const { return m_era_date_time_format; }
    virtual const std::basic_string<CharT>& date_time_zone_format() const { return m_date_time_zone_format; }
    virtual const std::basic_string<CharT>& era_date_time_zone_format() const { return m_era_date_time_zone_format; }
    virtual const std::basic_string<CharT>& am_pm_format() const { return m_am_pm_format; }
    virtual const std::vector<era_entry>& era_items() const { return m_era_items; }

private:
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
    std::vector<era_entry>                    m_era_items;
};

// This specialization delegates to timeio_conf<char32_t> to populate its data
// (see init_from_u32 below), so it requires timeio_conf<char32_t> to be a
// complete type when a char8_t conf is constructed. That delegation lives in a
// member function template whose reference to timeio_conf<char32_t> is dependent
// on a template parameter, so it is checked only when actually instantiated
// (never eagerly, hence not IFNDR): merely naming or default-constructing
// timeio_conf<char8_t> stays well-formed even on platforms where
// timeio_conf<char32_t> is unavailable (wchar_t is not UTF-32); there, only the
// construction path fails to compile, which is intended.
// The completeness requirement is deliberately NOT encoded as a constraint on
// this class: a non-dependent sizeof(timeio_conf<char32_t>) in the
// requires-clause is a hard error (not a soft constraint failure) and would
// break partial-specialization resolution for unrelated timeio_conf<T>.
template <typename CharT>
    requires std::is_same_v<CharT, char8_t>
class timeio_conf<CharT> : public ft_basic<timeio<char8_t>>
{
public:
    timeio_conf(const std::string& name)
        : ft_basic<timeio<char8_t>>()
    {
        init_from_u32<>(name);
    }

private:
    // T is defaulted/constrained to char32_t; templating it makes the
    // timeio_conf<T> reference below dependent, so the whole body is checked
    // only upon instantiation of this helper (i.e. only on the construction
    // path), never eagerly.
    template <typename T = char32_t>
        requires std::is_same_v<T, char32_t>
    void init_from_u32(const std::string& name)
    {
        timeio_conf<T> tmp_obj(name);

        m_date_format = detail::to_u8string(tmp_obj.date_format());
        m_era_date_format = detail::to_u8string(tmp_obj.era_date_format());
        m_time_format = detail::to_u8string(tmp_obj.time_format());
        m_era_time_format = detail::to_u8string(tmp_obj.era_time_format());
        m_date_time_format = detail::to_u8string(tmp_obj.date_time_format());
        m_era_date_time_format = detail::to_u8string(tmp_obj.era_date_time_format());

        m_am = detail::to_u8string(tmp_obj.am_name());
        m_pm = detail::to_u8string(tmp_obj.pm_name());
        m_am_pm_format = detail::to_u8string(tmp_obj.am_pm_format());

        for (size_t i = 0; i < 7; ++i)
        {
            m_day[i] = detail::to_u8string(tmp_obj.day_names()[i]);
            m_abbr_day[i] = detail::to_u8string(tmp_obj.abbr_day_names()[i]);
        }

        for (size_t i = 0; i < 12; ++i)
        {
            m_month[i] = detail::to_u8string(tmp_obj.month_names()[i]);
            m_abbr_month[i] = detail::to_u8string(tmp_obj.abbr_month_names()[i]);
        }

        for (size_t i = 0; i < 100; ++i)
        {
            m_alt_digits[i] = detail::to_u8string(tmp_obj.alt_digit_names()[i]);
        }

        const auto& tmp_era = tmp_obj.era_items();
        if (!tmp_era.empty())
        {
            m_era_items.reserve(tmp_era.size());
            for (size_t i = 0; i < tmp_era.size(); ++i)
            {
                const auto& src = tmp_era[i];
                era_entry aim;
                aim.name = detail::to_u8string(src.name);
                aim.format = detail::to_u8string(src.format);
                aim.from_year = src.from_year;
                aim.from_month = src.from_month;
                aim.from_day = src.from_day;
                aim.to_year = src.to_year;
                aim.to_month = src.to_month;
                aim.to_day = src.to_day;
                aim.offset = src.offset;
                aim.direction = src.direction;
                m_era_items.push_back(aim);
            }
        }
        m_time_zone_format = m_time_format + u8" %Z";
        m_era_time_zone_format = m_era_time_format + u8" %Z";
        m_date_time_zone_format = m_date_time_format + u8" %Z";
        m_era_date_time_zone_format = m_era_date_time_format + u8" %Z";
    }

public:
    virtual const std::array<std::basic_string<char8_t>, 7>& day_names() const { return m_day; }
    virtual const std::array<std::basic_string<char8_t>, 7>& abbr_day_names() const { return m_abbr_day; }
    virtual const std::array<std::basic_string<char8_t>, 12>& month_names() const { return m_month; }
    virtual const std::array<std::basic_string<char8_t>, 12>& abbr_month_names() const { return m_abbr_month; }
    virtual const std::array<std::basic_string<char8_t>, 100>& alt_digit_names() const { return m_alt_digits; }
    virtual const std::basic_string<char8_t>& am_name() const { return m_am; }
    virtual const std::basic_string<char8_t>& pm_name() const { return m_pm; }
    virtual const std::basic_string<char8_t>& date_format() const { return m_date_format; }
    virtual const std::basic_string<char8_t>& era_date_format() const { return m_era_date_format; }
    virtual const std::basic_string<char8_t>& time_format() const { return m_time_format; }
    virtual const std::basic_string<char8_t>& era_time_format() const { return m_era_time_format; }
    virtual const std::basic_string<char8_t>& time_zone_format() const { return m_time_zone_format; }
    virtual const std::basic_string<char8_t>& era_time_zone_format() const { return m_era_time_zone_format; }
    virtual const std::basic_string<char8_t>& date_time_format() const { return m_date_time_format; }
    virtual const std::basic_string<char8_t>& era_date_time_format() const { return m_era_date_time_format; }
    virtual const std::basic_string<char8_t>& date_time_zone_format() const { return m_date_time_zone_format; }
    virtual const std::basic_string<char8_t>& era_date_time_zone_format() const { return m_era_date_time_zone_format; }
    virtual const std::basic_string<char8_t>& am_pm_format() const { return m_am_pm_format; }
    virtual const std::vector<era_entry>& era_items() const { return m_era_items; }

private:
    std::array<std::basic_string<char8_t>, 7>   m_day;
    std::array<std::basic_string<char8_t>, 7>   m_abbr_day;
    std::array<std::basic_string<char8_t>, 12>  m_month;
    std::array<std::basic_string<char8_t>, 12>  m_abbr_month;
    std::array<std::basic_string<char8_t>, 100> m_alt_digits;
    std::basic_string<char8_t>                  m_am;
    std::basic_string<char8_t>                  m_pm;
    std::basic_string<char8_t>                  m_date_format;
    std::basic_string<char8_t>                  m_era_date_format;
    std::basic_string<char8_t>                  m_time_format;
    std::basic_string<char8_t>                  m_era_time_format;
    std::basic_string<char8_t>                  m_time_zone_format;
    std::basic_string<char8_t>                  m_era_time_zone_format;
    std::basic_string<char8_t>                  m_date_time_format;
    std::basic_string<char8_t>                  m_era_date_time_format;
    std::basic_string<char8_t>                  m_date_time_zone_format;
    std::basic_string<char8_t>                  m_era_date_time_zone_format;
    std::basic_string<char8_t>                  m_am_pm_format;
    std::vector<era_entry>                      m_era_items;
};
}
