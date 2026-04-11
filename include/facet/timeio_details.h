#pragma once
#include <langinfo.h>

#include <array>
#include <cstring>
#include <chrono>
#include <ctime>
#include <limits>
#include <string>
#include <stdexcept>
#include <common/prefix_tree.h>
#include <cvt/cvt_facilities.h>
#include <facet/facet_common.h>
#include <facet/facet_helper.h>

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
            int8_t offset;
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
        
        static size_t id() { return (size_t)(&s_id); }

    public:
        inline static const prefix_tree<CharT, std::string> s_timezone_tree = 
        []()
        {
            prefix_tree<CharT, std::string> res("");
            
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
        if (from_month > to_month) return true;
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
            clocale_user guard(inter_locale.c_locale);

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
                size_t i = 0;
                for (; i < 100; ++i)
                {
                    if (*ptr == '\0') break;
                    m_alt_digits[i] = ptr;
                    ptr += m_alt_digits[i].size() + 1;
                }
                if (i != 100)
                {
                    for (size_t j = 0; j< i; ++j)
                        m_alt_digits[i].clear();
                }
            }
            union
            {
                char *ptr;
                int32_t word;
            } u;
            
            u.ptr = nl_langinfo(_NL_TIME_ERA_NUM_ENTRIES);
            int32_t era_item_num = u.word;
            if (era_item_num > 0)
            {
                m_era_items.reserve(era_item_num);
                const char *ptr = (const char*)nl_langinfo(_NL_TIME_ERA_ENTRIES);
                for (int32_t cnt = 0; cnt < era_item_num; ++cnt)
                {
                    const char *base_ptr = ptr;
                    era_entry cur_entry;
                    
                    int32_t buf[8];
                    memcpy ((void *) (buf), (const void *)ptr, sizeof (int32_t) * 8);
                    ptr += sizeof (uint32_t) * 8;
                    
                    cur_entry.from_year = buf[2] + 1900;
                    cur_entry.from_month = buf[3] + 1;
                    cur_entry.from_day = buf[4];
                    cur_entry.to_year = (buf[5] > std::numeric_limits<int32_t>::max() - 1900) ? std::numeric_limits<int32_t>::max() : buf[5] + 1900;
                    cur_entry.to_month = buf[6] + 1;
                    cur_entry.to_day = buf[7];
                    
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
                    ptr += 3 - (((ptr - (const char *) base_ptr) + 3) & 3);
                    ptr = (char *) (wcschr ((wchar_t *) ptr, L'\0') + 1);
                    ptr = (char *) (wcschr ((wchar_t *) ptr, L'\0') + 1);

                    m_era_items.push_back(std::move(cur_entry));
                }
            }
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
            (sizeof(char32_t) == sizeof(wchar_t)) && 
            (static_cast<wchar_t>(U'李') == L'李') &&
            (static_cast<char32_t>(L'伟') == U'伟'))
class timeio_conf<CharT> : public ft_basic<timeio<CharT>>
{
    using era_entry = typename ft_basic<timeio<CharT>>::era_entry;

public:
    timeio_conf(const std::string& name)
        : ft_basic<timeio<CharT>>()
    {
        using namespace IOv2::FacetHelper;
        if ((name == "C") || (name == "POSIX"))
        {
            m_date_format = string_convert<CharT>("%m/%d/%y");     m_era_date_format = m_date_format;
            m_time_format = string_convert<CharT>("%H:%M:%S");     m_era_time_format = m_time_format;

            m_date_time_format = m_date_format + static_cast<CharT>(' ') + m_time_format;
            m_era_date_time_format = m_date_time_format;

            m_am = string_convert<CharT>("AM");
            m_pm = string_convert<CharT>("PM");
            m_am_pm_format = string_convert<CharT>("%I:%M:%S %p");

            m_day[0] = string_convert<CharT>("Sunday");
            m_day[1] = string_convert<CharT>("Monday");
            m_day[2] = string_convert<CharT>("Tuesday");
            m_day[3] = string_convert<CharT>("Wednesday");
            m_day[4] = string_convert<CharT>("Thursday");
            m_day[5] = string_convert<CharT>("Friday");
            m_day[6] = string_convert<CharT>("Saturday");

            m_abbr_day[0] = string_convert<CharT>("Sun");
            m_abbr_day[1] = string_convert<CharT>("Mon");
            m_abbr_day[2] = string_convert<CharT>("Tue");
            m_abbr_day[3] = string_convert<CharT>("Wed");
            m_abbr_day[4] = string_convert<CharT>("Thu");
            m_abbr_day[5] = string_convert<CharT>("Fri");
            m_abbr_day[6] = string_convert<CharT>("Sat");

            // Month names, starting with "C"'s January.
            m_month[0]  = string_convert<CharT>("January");
            m_month[1]  = string_convert<CharT>("February");
            m_month[2]  = string_convert<CharT>("March");
            m_month[3]  = string_convert<CharT>("April");
            m_month[4]  = string_convert<CharT>("May");
            m_month[5]  = string_convert<CharT>("June");
            m_month[6]  = string_convert<CharT>("July");
            m_month[7]  = string_convert<CharT>("August");
            m_month[8]  = string_convert<CharT>("September");
            m_month[9]  = string_convert<CharT>("October");
            m_month[10] = string_convert<CharT>("November");
            m_month[11] = string_convert<CharT>("December");

            // Abbreviated month names, starting with "C"'s Jan.
            m_abbr_month[0]  = string_convert<CharT>("Jan");
            m_abbr_month[1]  = string_convert<CharT>("Feb");
            m_abbr_month[2]  = string_convert<CharT>("Mar");
            m_abbr_month[3]  = string_convert<CharT>("Apr");
            m_abbr_month[4]  = string_convert<CharT>("May");
            m_abbr_month[5]  = string_convert<CharT>("Jun");
            m_abbr_month[6]  = string_convert<CharT>("Jul");
            m_abbr_month[7]  = string_convert<CharT>("Aug");
            m_abbr_month[8]  = string_convert<CharT>("Sep");
            m_abbr_month[9]  = string_convert<CharT>("Oct");
            m_abbr_month[10] = string_convert<CharT>("Nov");
            m_abbr_month[11] = string_convert<CharT>("Dec");
        }
        else
        {
            clocale_wrapper inter_locale(name.c_str());
            clocale_user guard(inter_locale.c_locale);
            using namespace IOv2::FacetHelper;
            m_date_format = nl_langinfo_w<CharT>(_NL_WD_FMT);
            m_era_date_format = nl_langinfo_w<CharT>(_NL_WERA_D_FMT);  if (m_era_date_format.empty()) m_era_date_format = m_date_format;
            m_time_format = nl_langinfo_w<CharT>(_NL_WT_FMT);
            m_era_time_format = nl_langinfo_w<CharT>(_NL_WERA_T_FMT);  if (m_era_time_format.empty()) m_era_time_format = m_time_format;

            m_date_time_format = nl_langinfo_w<CharT>(_NL_WD_T_FMT);
            m_era_date_time_format = nl_langinfo_w<CharT>(_NL_WERA_D_T_FMT);
            if (m_era_date_time_format.empty()) m_era_date_time_format = m_date_time_format;

            m_am = nl_langinfo_w<CharT>(_NL_WAM_STR);
            m_pm = nl_langinfo_w<CharT>(_NL_WPM_STR);
            m_am_pm_format = nl_langinfo_w<CharT>(_NL_WT_FMT_AMPM);

            m_day[0] = nl_langinfo_w<CharT>(_NL_WDAY_1);
            m_day[1] = nl_langinfo_w<CharT>(_NL_WDAY_2);
            m_day[2] = nl_langinfo_w<CharT>(_NL_WDAY_3);
            m_day[3] = nl_langinfo_w<CharT>(_NL_WDAY_4);
            m_day[4] = nl_langinfo_w<CharT>(_NL_WDAY_5);
            m_day[5] = nl_langinfo_w<CharT>(_NL_WDAY_6);
            m_day[6] = nl_langinfo_w<CharT>(_NL_WDAY_7);

            m_abbr_day[0] = nl_langinfo_w<CharT>(_NL_WABDAY_1);
            m_abbr_day[1] = nl_langinfo_w<CharT>(_NL_WABDAY_2);
            m_abbr_day[2] = nl_langinfo_w<CharT>(_NL_WABDAY_3);
            m_abbr_day[3] = nl_langinfo_w<CharT>(_NL_WABDAY_4);
            m_abbr_day[4] = nl_langinfo_w<CharT>(_NL_WABDAY_5);
            m_abbr_day[5] = nl_langinfo_w<CharT>(_NL_WABDAY_6);
            m_abbr_day[6] = nl_langinfo_w<CharT>(_NL_WABDAY_7);

            // Month names, starting with "C"'s January.
            m_month[0]  = nl_langinfo_w<CharT>(_NL_WMON_1);
            m_month[1]  = nl_langinfo_w<CharT>(_NL_WMON_2);
            m_month[2]  = nl_langinfo_w<CharT>(_NL_WMON_3);
            m_month[3]  = nl_langinfo_w<CharT>(_NL_WMON_4);
            m_month[4]  = nl_langinfo_w<CharT>(_NL_WMON_5);
            m_month[5]  = nl_langinfo_w<CharT>(_NL_WMON_6);
            m_month[6]  = nl_langinfo_w<CharT>(_NL_WMON_7);
            m_month[7]  = nl_langinfo_w<CharT>(_NL_WMON_8);
            m_month[8]  = nl_langinfo_w<CharT>(_NL_WMON_9);
            m_month[9]  = nl_langinfo_w<CharT>(_NL_WMON_10);
            m_month[10] = nl_langinfo_w<CharT>(_NL_WMON_11);
            m_month[11] = nl_langinfo_w<CharT>(_NL_WMON_12);

            // Abbreviated month names, starting with "C"'s Jan.
            m_abbr_month[0]  = nl_langinfo_w<CharT>(_NL_WABMON_1);
            m_abbr_month[1]  = nl_langinfo_w<CharT>(_NL_WABMON_2);
            m_abbr_month[2]  = nl_langinfo_w<CharT>(_NL_WABMON_3);
            m_abbr_month[3]  = nl_langinfo_w<CharT>(_NL_WABMON_4);
            m_abbr_month[4]  = nl_langinfo_w<CharT>(_NL_WABMON_5);
            m_abbr_month[5]  = nl_langinfo_w<CharT>(_NL_WABMON_6);
            m_abbr_month[6]  = nl_langinfo_w<CharT>(_NL_WABMON_7);
            m_abbr_month[7]  = nl_langinfo_w<CharT>(_NL_WABMON_8);
            m_abbr_month[8]  = nl_langinfo_w<CharT>(_NL_WABMON_9);
            m_abbr_month[9]  = nl_langinfo_w<CharT>(_NL_WABMON_10);
            m_abbr_month[10] = nl_langinfo_w<CharT>(_NL_WABMON_11);
            m_abbr_month[11] = nl_langinfo_w<CharT>(_NL_WABMON_12);

            {// alternative digits
                union { char* s; CharT* w; } u;
                u.s = nl_langinfo(_NL_WALT_DIGITS);

                CharT* ptr = u.w;
                size_t i = 0;
                for (; i < 100; ++i)
                {
                    if (*ptr == CharT{}) break;
                    m_alt_digits[i] = ptr;
                    ptr += m_alt_digits[i].size() + 1;
                }
                if (i != 100)
                {
                    for (size_t j = 0; j< i; ++j)
                        m_alt_digits[i].clear();
                }
            }

            union
            {
                char *ptr;
                int32_t word;
            } u;

            u.ptr = nl_langinfo(_NL_TIME_ERA_NUM_ENTRIES);
            int32_t era_item_num = u.word;
            if (era_item_num > 0)
            {
                m_era_items.reserve(era_item_num);
                const char *ptr = (const char*)nl_langinfo(_NL_TIME_ERA_ENTRIES);
                for (int32_t cnt = 0; cnt < era_item_num; ++cnt)
                {
                    const char *base_ptr = ptr;
                    era_entry cur_entry;
                    
                    int32_t buf[8];
                    memcpy ((void *) (buf), (const void *)ptr, sizeof (int32_t) * 8);
                    ptr += sizeof (uint32_t) * 8;
                    
                    cur_entry.from_year = buf[2] + 1900;
                    cur_entry.from_month = buf[3] + 1;
                    cur_entry.from_day = buf[4];
                    cur_entry.to_year = (buf[5] > std::numeric_limits<int32_t>::max() - 1900) ? std::numeric_limits<int32_t>::max() : buf[5] + 1900;
                    cur_entry.to_month = buf[6] + 1;
                    cur_entry.to_day = buf[7];
                    
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
                    
                    // skip char name and format
                    ptr = strchr(ptr, '\0') + 1;
                    ptr = strchr(ptr, '\0') + 1;
                    
                    // skip wchar_t name and format
                    ptr += 3 - (((ptr - (const char *) base_ptr) + 3) & 3);
                    cur_entry.name = (CharT *) ptr; ptr = (char *) (wcschr ((wchar_t *) ptr, L'\0') + 1);
                    cur_entry.format = (CharT *) ptr; ptr = (char *) (wcschr ((wchar_t *) ptr, L'\0') + 1);

                    m_era_items.push_back(std::move(cur_entry));
                }
            }
        }
        m_time_zone_format = m_time_format + (CharT*)(L" %Z");
        m_era_time_zone_format = m_era_time_format + (CharT*)(L" %Z");
        m_date_time_zone_format = m_date_time_format + (CharT*)(L" %Z");
        m_era_date_time_zone_format = m_era_date_time_format + (CharT*)(L" %Z");
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

template <>
class timeio_conf<char8_t> : public ft_basic<timeio<char8_t>>
{
public:
    timeio_conf(const std::string& name)
        : ft_basic<timeio<char8_t>>()
    {
        timeio_conf<char32_t> tmp_obj(name);

        using namespace IOv2::FacetHelper;
        m_date_format = to_u8string(tmp_obj.date_format());
        m_era_date_format = to_u8string(tmp_obj.era_date_format());
        m_time_format = to_u8string(tmp_obj.time_format());
        m_era_time_format = to_u8string(tmp_obj.era_time_format());
        m_date_time_format = to_u8string(tmp_obj.date_time_format());
        m_era_date_time_format = to_u8string(tmp_obj.era_date_time_format());

        m_am = to_u8string(tmp_obj.am_name());
        m_pm = to_u8string(tmp_obj.pm_name());
        m_am_pm_format = to_u8string(tmp_obj.am_pm_format());

        for (size_t i = 0; i < 7; ++i)
        {
            m_day[i] = to_u8string(tmp_obj.day_names()[i]);
            m_abbr_day[i] = to_u8string(tmp_obj.abbr_day_names()[i]);
        }

        for (size_t i = 0; i < 12; ++i)
        {
            m_month[i] = to_u8string(tmp_obj.month_names()[i]);
            m_abbr_month[i] = to_u8string(tmp_obj.abbr_month_names()[i]);
        }

        for (size_t i = 0; i < 100; ++i)
        {
            m_alt_digits[i] = to_u8string(tmp_obj.alt_digit_names()[i]);
        }

        const auto& tmp_era = tmp_obj.era_items();
        if (!tmp_era.empty())
        {
            m_era_items.reserve(tmp_era.size());
            for (size_t i = 0; i < tmp_era.size(); ++i)
            {
                const auto& src = tmp_era[i];
                era_entry aim;
                aim.name = to_u8string(src.name);
                aim.format = to_u8string(src.format);
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