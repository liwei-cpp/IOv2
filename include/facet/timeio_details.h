/**
 * @file timeio_details.h
 * @lang{ZH}
 * `timeio` facet 的实现细节，包含：
 * - `ft_basic<timeio<CharT>>`：`timeio` 的基类模板特化，持有时区前缀树与 facet 类型标识；
 * - `TimeioHelper`：日历纪元日期比较的辅助工具函数；
 * - `timeio_conf<CharT>`：各字符类型（`char`、`wchar_t`/`char32_t`、`char8_t`）的
 *   locale 配置类，负责从系统 locale（通过 `nl_langinfo`）或 C/POSIX 硬编码默认值
 *   中加载日期/时间格式串、星期/月份名称、AM/PM 字符串、替代数字及纪元数据。
 * @endif
 *
 * @lang{EN}
 * Implementation details for the `timeio` facet, comprising:
 * - `ft_basic<timeio<CharT>>`: base class template specialization for `timeio`,
 *   holding the timezone prefix trie and the facet type-identity token;
 * - `TimeioHelper`: helper utilities for calendar era date comparison;
 * - `timeio_conf<CharT>`: per-character-type locale configuration classes
 *   (`char`, `wchar_t`/`char32_t`, `char8_t`) that load date/time format strings,
 *   weekday/month names, AM/PM strings, alternative digits, and era data from the
 *   system locale (via `nl_langinfo`) or from C/POSIX hard-coded defaults.
 * @endif
 */
#pragma once
#include <common/clocale_wrapper.h>
#include <common/metafunctions.h>
#include <common/prefix_tree.h>
#include <cvt/cvt_facilities.h>
#include <facet/facet_common.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <limits>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <langinfo.h>

namespace IOv2
{
    template <typename CharT> class timeio;

    /**
     * @lang{ZH}
     * @brief `timeio` facet 的基类模板特化。
     *
     * 为所有字符类型的 `timeio` facet 提供公共基础，包括：
     * - `era_entry`：描述单个日历纪元的结构体；
     * - 静态 `id()` 方法：返回 `timeio` facet 在 facet_store 中的唯一类型标识；
     * - `s_timezone_tree`：静态前缀树，将时区缩写（如 `"EST"`）及 IANA 完整时区名称
     *   映射到规范的时区名称，供 `%Z` 格式说明符解析时使用。
     *
     * @tparam CharT 字符类型。
     * @endif
     *
     * @lang{EN}
     * @brief Base class template specialization for the `timeio` facet.
     *
     * Provides the common foundation shared across all character types of the `timeio`
     * facet, including:
     * - `era_entry`: a struct describing a single calendar era;
     * - static `id()`: returns the unique type-identity token of the `timeio` facet
     *   within a facet_store;
     * - `s_timezone_tree`: a static prefix trie mapping timezone abbreviations
     *   (e.g. `"EST"`) and full IANA zone names to canonical timezone names,
     *   used when parsing the `%Z` format specifier.
     *
     * @tparam CharT The character type.
     * @endif
     */
    template <typename CharT>
    class ft_basic<timeio<CharT>> : public base_ft<timeio>
    {
    public:
        /**
         * @lang{ZH}
         * @brief 表示一个日历纪元（calendar era）的条目。
         *
         * 每个纪元条目对应 locale 数据中定义的一段历史时期（如公元 A.D.、某国皇纪）。
         * 起止日期（`from_*` / `to_*`）定义了该纪元的日历范围，`offset` 与 `direction`
         * 共同决定了如何将公历年份映射到纪元年份：
         * @code
         *   era_year = offset + (calendar_year - from_year) * direction
         * @endcode
         * @endif
         *
         * @lang{EN}
         * @brief An entry representing a single calendar era.
         *
         * Each era entry corresponds to a historical period defined in locale data
         * (e.g. A.D., a country-specific imperial era). The start and end dates
         * (`from_*` / `to_*`) define the calendar range of the era; `offset` together
         * with `direction` determine how a Gregorian year maps to the era year:
         * @code
         *   era_year = offset + (calendar_year - from_year) * direction
         * @endcode
         * @endif
         */
        struct era_entry
        {
            /**
             * @lang{ZH}
             * @brief 纪元名称（用于 `%EC` 格式输出）。
             * @endif
             *
             * @lang{EN}
             * @brief Era name (used for `%EC` format output).
             * @endif
             */
            std::basic_string<CharT> name;
            /**
             * @lang{ZH}
             * @brief 纪元年份格式串（用于 `%EY` 格式输出）。
             * @endif
             *
             * @lang{EN}
             * @brief Era year format string (used for `%EY` format output).
             * @endif
             */
            std::basic_string<CharT> format;
            /**
             * @lang{ZH}
             * @brief 纪元起始年份（公历）。
             * @endif
             *
             * @lang{EN}
             * @brief Era start year (Gregorian calendar).
             * @endif
             */
            int32_t from_year;
            /**
             * @lang{ZH}
             * @brief 纪元起始月份（1–12）。
             * @endif
             *
             * @lang{EN}
             * @brief Era start month (1–12).
             * @endif
             */
            uint8_t from_month;
            /**
             * @lang{ZH}
             * @brief 纪元起始日（1–31）。
             * @endif
             *
             * @lang{EN}
             * @brief Era start day (1–31).
             * @endif
             */
            uint8_t from_day;
            /**
             * @lang{ZH}
             * @brief 纪元结束年份（公历）。开放结尾的纪元规范化为 `INT32_MAX`。
             * @endif
             *
             * @lang{EN}
             * @brief Era end year (Gregorian calendar). Open-ended eras are normalised to `INT32_MAX`.
             * @endif
             */
            int32_t to_year;
            /**
             * @lang{ZH}
             * @brief 纪元结束月份（1–12）。
             * @endif
             *
             * @lang{EN}
             * @brief Era end month (1–12).
             * @endif
             */
            uint8_t to_month;
            /**
             * @lang{ZH}
             * @brief 纪元结束日（1–31）。
             * @endif
             *
             * @lang{EN}
             * @brief Era end day (1–31).
             * @endif
             */
            uint8_t to_day;
            /**
             * @lang{ZH}
             * @brief 纪元纪年偏移量，即纪元起始年（`from_year`）所对应的纪元年号。
             * @endif
             *
             * @lang{EN}
             * @brief Year offset for the era: the era year number assigned to `from_year`.
             * @endif
             */
            int32_t offset;
            /**
             * @lang{ZH}
             * @brief 纪元年份增长方向。
             *
             * `+1` 表示年份数值随时间向未来方向增大（如公元 A.D.）；
             * `-1` 表示年份数值随时间向过去方向增大（如公元前 B.C.）。
             * @endif
             *
             * @lang{EN}
             * @brief Direction in which era year numbers increase.
             *
             * `+1` indicates that the year number increases toward the future (e.g. A.D.);
             * `-1` indicates that the year number increases toward the past (e.g. B.C.).
             * @endif
             */
            int8_t direction;

            /// @cond
            bool operator==(const era_entry&) const = default; // for test.
            /// @endcond
        };
    public:
        /**
         * @lang{ZH}
         * @brief 默认构造函数，使用 `id()` 初始化基类。
         * @endif
         *
         * @lang{EN}
         * @brief Default constructor, initializes the base class with `id()`.
         * @endif
         */
        ft_basic()
            : base_ft<timeio>(id()) {}

        using char_type = CharT;

        /**
         * @lang{ZH}
         * @brief 返回 `timeio` facet 的唯一类型标识。
         *
         * 经由统一入口 `type_id_v<ft_basic>()`（见 facet_common.h 顶部说明）：
         * header-only 模式下是每类型静态量的地址，共享库模式下是 `std::type_index(typeid)`，
         * 与 `base_ft` 的类型分发机制配合使用。
         * @return `timeio` facet 的唯一类型 ID。
         * @endif
         *
         * @lang{EN}
         * @brief Returns the unique type-identity token for the `timeio` facet.
         *
         * Via the single entry point `type_id_v<ft_basic>()` (see the note at the
         * top of facet_common.h): a per-type static's address in header-only mode,
         * `std::type_index(typeid)` in shared mode; integrates with the
         * type-dispatch mechanism of `base_ft`.
         * @return The unique type ID for the `timeio` facet.
         * @endif
         */
        static facet_id_t id() { return type_id_v<ft_basic>(); }

    public:
        /**
         * @lang{ZH}
         * @brief 时区缩写与 IANA 全名到规范时区名的静态前缀树。
         *
         * 在程序启动时通过 `std::chrono::get_tzdb()` 构建，将：
         * - 时区缩写（如 `"EST"`）映射到其规范 IANA 名称（若 `locate_zone` 成功），
         *   或映射到带 `"*"` 前缀的原始缩写（若无法定位对应的 IANA 时区）；
         * - 与缩写不同的完整 IANA 时区名称映射到自身。
         *
         * 供 `%Z` 格式说明符的解析使用。若时区数据库在静态初始化期间不可用或格式
         * 有误，树将为空或不完整，`%Z` 解析会在运行时产生可捕获的 `stream_error`
         * 而非调用 `std::terminate`。
         * @endif
         *
         * @lang{EN}
         * @brief Static prefix trie mapping timezone abbreviations and IANA full names
         *        to canonical timezone names.
         *
         * Built at program startup via `std::chrono::get_tzdb()`, mapping:
         * - timezone abbreviations (e.g. `"EST"`) to their canonical IANA name (if
         *   `locate_zone` succeeds), or to the abbreviation prefixed with `"*"` (if
         *   no corresponding IANA zone is found);
         * - full IANA timezone names (when different from their abbreviation) to themselves.
         *
         * Used by the `%Z` format specifier during parsing. If the timezone database is
         * unavailable or malformed at static-initialization time, the trie is left empty
         * or partial, and `%Z` parsing produces a catchable `stream_error` at runtime
         * rather than calling `std::terminate`.
         * @endif
         */
        inline static const prefix_tree<CharT, std::string> s_timezone_tree =
        []()
        {
            prefix_tree<CharT, std::string> res;

            try
            {
                const auto& tzdb = std::chrono::get_tzdb();

                // Collect all abbreviations (deduplicated across zones).
                std::set<std::string> abbrevs;
                for (const auto& zone : tzdb.zones)
                {
                    std::string abbr = zone.get_info(std::chrono::sys_time<std::chrono::seconds>{}).abbrev;
                    if (!abbr.empty())
                        abbrevs.insert(std::move(abbr));
                }

                // Resolve each abbreviation: if locate_zone succeeds (e.g. "EST"
                // is an IANA link), store the canonical name; otherwise mark "*".
                for (const auto& abbr : abbrevs)
                {
                    try
                    {
                        std::string canonical{std::chrono::locate_zone(abbr)->name()};
                        res.add(abbr.begin(), abbr.end(), canonical);
                    }
                    catch (...)
                    {
                        res.add(abbr.begin(), abbr.end(), "*" + abbr);
                    }
                }

                // Add full zone names that differ from their own abbreviation.
                for (const auto& zone : tzdb.zones)
                {
                    std::string full_name{zone.name()};
                    std::string abbr_name = zone.get_info(std::chrono::sys_time<std::chrono::seconds>{}).abbrev;
                    if (full_name != abbr_name)
                        res.add(full_name.begin(), full_name.end(), full_name);
                }
            }
            catch (...) // NOLINT(bugprone-empty-catch)
            {
                // tz database unavailable or malformed at static-init time:
                // degrade to an empty/partial tree instead of letting the
                // exception escape this static initializer (which would call
                // std::terminate). %Z then simply fails to match and reports a
                // catchable stream_error at parse time, matching the defensive
                // behaviour of time_zone_parse_helper.
            }
            return res;
        }();
    };

/**
 * @lang{ZH}
 * @brief 供 `timeio` 内部使用的辅助工具命名空间。
 *
 * 包含用于日历纪元日期比较的内联函数，不属于 `timeio` 的公共接口。
 * @endif
 *
 * @lang{EN}
 * @brief Internal helper namespace for `timeio`.
 *
 * Contains inline functions for calendar era date comparison; these are
 * not part of the public interface of `timeio`.
 * @endif
 */
namespace TimeioHelper
{
    /**
     * @lang{ZH}
     * @brief 比较两个完整日期（年、月、日）的前后关系。
     *
     * 用于判断纪元起始日期是否早于或等于结束日期，从而确定纪元的方向。
     * @param from_year  起始年份。
     * @param from_month 起始月份（1–12）。
     * @param from_day   起始日（1–31）。
     * @param to_year    结束年份。
     * @param to_month   结束月份（1–12）。
     * @param to_day     结束日（1–31）。
     * @return 若起始日期早于或等于结束日期，返回 `true`；否则返回 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Compares two full dates (year, month, day) for ordering.
     *
     * Used to determine whether an era's start date is no later than its end date,
     * which in turn determines the era's direction.
     * @param from_year  The starting year.
     * @param from_month The starting month (1–12).
     * @param from_day   The starting day (1–31).
     * @param to_year    The ending year.
     * @param to_month   The ending month (1–12).
     * @param to_day     The ending day (1–31).
     * @return `true` if the start date is earlier than or equal to the end date; `false` otherwise.
     * @endif
     */
    inline bool era_small_or_equal(int from_year, uint8_t from_month, uint8_t from_day, // NOLINT(bugprone-easily-swappable-parameters)
                                   int to_year, uint8_t to_month, uint8_t to_day)        // NOLINT(bugprone-easily-swappable-parameters)
    {
        if (from_year < to_year) return true;
        if (from_year > to_year) return false;
        if (from_month < to_month) return true;
        if (from_month > to_month) return false;
        return from_day <= to_day;
    }

    /**
     * @lang{ZH}
     * @brief 比较两个年月组合（不含日）的前后关系。
     *
     * 为 `era_small_or_equal` 的简化重载，仅比较年份和月份。
     * @param from_year  起始年份。
     * @param from_month 起始月份（1–12）。
     * @param to_year    结束年份。
     * @param to_month   结束月份（1–12）。
     * @return 若起始年月早于或等于结束年月，返回 `true`；否则返回 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Compares two year-month pairs (without day) for ordering.
     *
     * A simplified overload of `era_small_or_equal` that compares only year and month.
     * @param from_year  The starting year.
     * @param from_month The starting month (1–12).
     * @param to_year    The ending year.
     * @param to_month   The ending month (1–12).
     * @return `true` if the start year-month is earlier than or equal to the end year-month; `false` otherwise.
     * @endif
     */
    inline bool era_small_or_equal(int from_year, uint8_t from_month,                   // NOLINT(bugprone-easily-swappable-parameters)
                                   int to_year, uint8_t to_month)                        // NOLINT(bugprone-easily-swappable-parameters)
    {
        if (from_year < to_year) return true;
        if (from_year > to_year) return false;
        return from_month <= to_month;
    }
}

template <typename CharT> class timeio_conf;

/**
 * @lang{ZH}
 * @brief `timeio` facet 的 `char` 字符类型 locale 配置类。
 *
 * 这是 `timeio_conf` 的主特化，负责从系统 locale 或 C/POSIX 硬编码默认值中加载
 * `timeio` facet 所需的全部 locale 数据，包括日期/时间格式串、星期与月份全称及
 * 缩写、AM/PM 字符串及格式、替代数字，以及纪元（era）条目。
 *
 * 当 locale 名称为 `"C"` 或 `"POSIX"` 时，使用硬编码的英语默认值；
 * 其他 locale 名称通过 `clocale_wrapper` 切换线程 locale 后，调用 `nl_langinfo`
 * 获取系统提供的字符串，并调用 `parse_glibc_era_entries` 解码 glibc 纪元数据。
 * @endif
 *
 * @lang{EN}
 * @brief Locale configuration class for the `timeio` facet specialised for `char`.
 *
 * This is the primary specialization of `timeio_conf`, responsible for loading all
 * locale data required by the `timeio` facet from the system locale or from C/POSIX
 * hard-coded defaults. The loaded data includes: date/time format strings, full and
 * abbreviated weekday and month names, AM/PM strings and format, alternative digits,
 * and era entries.
 *
 * When the locale name is `"C"` or `"POSIX"`, hard-coded English defaults are used.
 * For other locales, `clocale_wrapper` switches the thread locale and `nl_langinfo`
 * is called to retrieve system-provided strings; `parse_glibc_era_entries` is then
 * called to decode the glibc era binary data.
 * @endif
 */
template <>
class timeio_conf<char> : public ft_basic<timeio<char>>
{
public:
    /**
     * @lang{ZH}
     * @brief 构造函数，根据 locale 名称加载日期时间 locale 数据。
     *
     * 若 `name` 为 `"C"` 或 `"POSIX"`，则使用硬编码的英语默认值（ISO C 标准格式）；
     * 否则通过 `clocale_wrapper` 切换至指定 locale 后，调用 `nl_langinfo` 及
     * `parse_glibc_era_entries` 从系统 locale 数据库中加载相应数据。
     * @param name locale 名称（如 `"C"`、`"zh_CN.UTF-8"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that loads date-time locale data for the given locale name.
     *
     * If `name` is `"C"` or `"POSIX"`, hard-coded English defaults (ISO C standard
     * formats) are used. Otherwise, `clocale_wrapper` switches to the specified locale
     * and `nl_langinfo` together with `parse_glibc_era_entries` are called to load
     * the corresponding data from the system locale database.
     * @param name The locale name (e.g. `"C"`, `"zh_CN.UTF-8"`).
     * @endif
     */
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
                // TRUSTED-LOCALE BOUNDARY (same assumption as
                // parse_glibc_era_entries below): ALT_DIGITS is trusted to be a
                // sequence of at most 100 consecutive, NUL-terminated narrow
                // strings, the last followed by a final empty string. The walk
                // below advances a raw pointer by each string's length and is
                // UNCHECKED, so malformed or unterminated locale data could read
                // out of bounds. It is bounded in practice only by the empty-string
                // sentinel and the 100-entry cap. System locale data is treated as
                // trusted; if ALT_DIGITS could ever come from an untrusted source,
                // harden this here. nl_langinfo() is POSIX-guaranteed non-null, but
                // the null guard below protects against non-conforming platforms.
                if (char* ptr = nl_langinfo(ALT_DIGITS); ptr)
                {
                    for (size_t i = 0; i < 100; ++i)
                    {
                        if (*ptr == '\0') break;
                        m_alt_digits[i] = ptr;
                        ptr += m_alt_digits[i].size() + 1;
                    }
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

    /**
     * @lang{ZH}
     * @brief 返回星期全称数组（索引 0 为星期日，索引 6 为星期六）。
     * @return 包含 7 个星期全称字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of full weekday names (index 0 = Sunday, index 6 = Saturday).
     * @return A constant reference to the array of 7 full weekday name strings.
     * @endif
     */
    virtual const std::array<std::string, 7>& day_names() const { return m_day; }
    /**
     * @lang{ZH}
     * @brief 返回星期缩写数组（索引 0 为星期日，索引 6 为星期六）。
     * @return 包含 7 个星期缩写字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of abbreviated weekday names (index 0 = Sunday, index 6 = Saturday).
     * @return A constant reference to the array of 7 abbreviated weekday name strings.
     * @endif
     */
    virtual const std::array<std::string, 7>& abbr_day_names() const { return m_abbr_day; }
    /**
     * @lang{ZH}
     * @brief 返回月份全称数组（索引 0 为一月，索引 11 为十二月）。
     * @return 包含 12 个月份全称字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of full month names (index 0 = January, index 11 = December).
     * @return A constant reference to the array of 12 full month name strings.
     * @endif
     */
    virtual const std::array<std::string, 12>& month_names() const { return m_month; }
    /**
     * @lang{ZH}
     * @brief 返回月份缩写数组（索引 0 为一月，索引 11 为十二月）。
     * @return 包含 12 个月份缩写字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of abbreviated month names (index 0 = January, index 11 = December).
     * @return A constant reference to the array of 12 abbreviated month name strings.
     * @endif
     */
    virtual const std::array<std::string, 12>& abbr_month_names() const { return m_abbr_month; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的替代数字字符串数组（最多 100 项）。
     *
     * 替代数字用于 `%Od`、`%Oe` 等替代格式说明符的输出与解析。
     * 未被 locale 定义的条目为空字符串。
     * @return 包含 100 个替代数字字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale-defined alternative digit strings (up to 100 entries).
     *
     * Alternative digits are used for output and parsing of alternative format specifiers
     * such as `%Od` and `%Oe`. Entries not defined by the locale are empty strings.
     * @return A constant reference to the array of 100 alternative digit strings.
     * @endif
     */
    virtual const std::array<std::string, 100>& alt_digit_names() const { return m_alt_digits; }
    /**
     * @lang{ZH}
     * @brief 返回 AM 时段字符串（如 `"AM"`）。
     * @return AM 字符串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the AM period string (e.g. `"AM"`).
     * @return A constant reference to the AM string.
     * @endif
     */
    virtual const std::string& am_name() const { return m_am; }
    /**
     * @lang{ZH}
     * @brief 返回 PM 时段字符串（如 `"PM"`）。
     * @return PM 字符串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the PM period string (e.g. `"PM"`).
     * @return A constant reference to the PM string.
     * @endif
     */
    virtual const std::string& pm_name() const { return m_pm; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期格式串（对应 `%x`）。
     * @return 日期格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale date format string (corresponding to `%x`).
     * @return A constant reference to the date format string.
     * @endif
     */
    virtual const std::string& date_format() const { return m_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期格式串（对应 `%Ex`）。
     *
     * 若 locale 未定义纪元修饰格式，则回退为普通日期格式串。
     * @return 纪元修饰日期格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified date format string (corresponding to `%Ex`).
     *
     * Falls back to the plain date format string if the locale does not define an
     * era-modified variant.
     * @return A constant reference to the era-modified date format string.
     * @endif
     */
    virtual const std::string& era_date_format() const { return m_era_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 时间格式串（对应 `%X`）。
     * @return 时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale time format string (corresponding to `%X`).
     * @return A constant reference to the time format string.
     * @endif
     */
    virtual const std::string& time_format() const { return m_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰时间格式串（对应 `%EX`）。
     *
     * 若 locale 未定义纪元修饰格式，则回退为普通时间格式串。
     * @return 纪元修饰时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified time format string (corresponding to `%EX`).
     *
     * Falls back to the plain time format string if the locale does not define an
     * era-modified variant.
     * @return A constant reference to the era-modified time format string.
     * @endif
     */
    virtual const std::string& era_time_format() const { return m_era_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回时间加时区格式串（时间格式串后附加 `%Z`）。
     * @return 时间加时区格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the time-plus-timezone format string (time format string with `%Z` appended).
     * @return A constant reference to the time-plus-timezone format string.
     * @endif
     */
    virtual const std::string& time_zone_format() const { return m_time_zone_format; }
    /**
     * @lang{ZH}
     * @brief 返回纪元修饰时间加时区格式串。
     * @return 纪元修饰时间加时区格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the era-modified time-plus-timezone format string.
     * @return A constant reference to the era-modified time-plus-timezone format string.
     * @endif
     */
    virtual const std::string& era_time_zone_format() const { return m_era_time_zone_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期时间格式串（对应 `%c`）。
     * @return 日期时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale date-time format string (corresponding to `%c`).
     * @return A constant reference to the date-time format string.
     * @endif
     */
    virtual const std::string& date_time_format() const { return m_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期时间格式串（对应 `%Ec`）。
     *
     * 若 locale 未定义纪元修饰格式，则回退为普通日期时间格式串。
     * @return 纪元修饰日期时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified date-time format string (corresponding to `%Ec`).
     *
     * Falls back to the plain date-time format string if the locale does not define an
     * era-modified variant.
     * @return A constant reference to the era-modified date-time format string.
     * @endif
     */
    virtual const std::string& era_date_time_format() const { return m_era_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回日期时间加时区格式串（日期时间格式串后附加 `%Z`）。
     * @return 日期时间加时区格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the date-time-plus-timezone format string (date-time format with `%Z` appended).
     * @return A constant reference to the date-time-plus-timezone format string.
     * @endif
     */
    virtual const std::string& date_time_zone_format() const { return m_date_time_zone_format; }
    /**
     * @lang{ZH}
     * @brief 返回纪元修饰日期时间加时区格式串。
     * @return 纪元修饰日期时间加时区格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the era-modified date-time-plus-timezone format string.
     * @return A constant reference to the era-modified date-time-plus-timezone format string.
     * @endif
     */
    virtual const std::string& era_date_time_zone_format() const { return m_era_date_time_zone_format; }
    /**
     * @lang{ZH}
     * @brief 返回 AM/PM 时间格式串（对应 `%r`）。
     * @return AM/PM 时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the AM/PM time format string (corresponding to `%r`).
     * @return A constant reference to the AM/PM time format string.
     * @endif
     */
    virtual const std::string& am_pm_format() const { return m_am_pm_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的纪元条目列表。
     * @return 纪元条目列表的常量引用；若 locale 无纪元定义则为空 `vector`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the list of locale-defined era entries.
     * @return A constant reference to the era entry list; an empty `vector` if the locale
     *         defines no eras.
     * @endif
     */
    virtual const std::vector<era_entry>& era_items() const { return m_era_items; }

private:
    /**
     * @lang{ZH}
     * @brief 解析当前活动线程 locale 的 glibc 纪元二进制表，返回解码后的纪元条目列表。
     *
     * @pre 调用前，调用方必须通过 `clocale_user` / `uselocale` 等方式将目标 locale
     *      激活于当前线程。`nl_langinfo()` 读取该活动 locale；返回的指针在下一次
     *      `nl_langinfo()` 调用前保持有效，足以在本函数内完成消费。
     *
     * @note **输入布局（受信任，不做验证）**
     *   此函数是唯一一处信任 glibc 纪元二进制布局的地方。
     *   `nl_langinfo(_NL_TIME_ERA_ENTRIES)` 被假定为指向恰好
     *   `_NL_TIME_ERA_NUM_ENTRIES` 条连续、格式完整的记录；指针遍历**未作越界检查**，
     *   因此若 locale 数据库被篡改或截断，可能引发越界读取。
     *   系统 locale 数据视为受信任数据；若纪元数据可能来自不可信来源，
     *   应在此函数边界处增加防御性边界检查。
     *
     *   每条记录相对于其起始地址（base_ptr）的内存布局如下：
     *   - 8 × int32 头部（通过 memcpy 读取）：
     *       [0] 方向标记（'+' / '-'）  [1] 偏移量
     *       [2] from_year - 1900       [3] from_month - 1   [4] from_day
     *       [5] to_year   - 1900       [6] to_month   - 1   [7] to_day
     *   - NUL 结尾的窄字符名称，随后是 NUL 结尾的窄字符格式串
     *   - 填充至相对于 base_ptr 的 4 字节对齐边界
     *   - NUL 结尾的宽字符名称，随后是 NUL 结尾的宽字符格式串
     *
     * @note **输出不变量**
     *   - `from_year` / `to_year` 在加上 1900 时防止 int32 溢出（开放结尾的 "to"
     *     被规范化为 int32 最大值的 12 月 31 日）。
     *   - `direction` 被规范化为恰好 `+1` / `-1`，使得以下线性公式对所有纪元类型均一致有效：
     *     @code
     *       era_year = offset + (calendar_year - from_year) * direction
     *     @endcode
     *     规范化逻辑参照 glibc 的 era.c（约第 98 行，
     *     https://github.com/lattera/glibc/blob/master/time/era.c）：
     *     前向纪元（from ≤ to，如公元 AD、日本皇纪、泰国佛历）方向标记直接映射，
     *     '+' → +1，'-' → -1；
     *     后向纪元（from > to）方向标记取反，以保证公式一致性（详见 OUTPUT INVARIANTS 说明）。
     *     此为与 glibc 共有的已知局限：实际上没有 glibc locale 定义后向纪元，
     *     故该路径在实践中从未被执行。
     *
     * @return 解码后的 `era_entry` 列表；若无纪元数据则返回空 `vector`。
     * @endif
     *
     * @lang{EN}
     * @brief Parses the glibc era binary table of the currently active thread locale
     *        and returns the decoded era entries.
     *
     * @pre The caller must have made the target locale current on the calling thread
     *      (e.g. via `clocale_user` / `uselocale`) before invoking this function.
     *      `nl_langinfo()` reads that active locale; the returned pointers are consumed
     *      before any further `nl_langinfo()` call, so they remain valid for the
     *      duration of this function.
     *
     * @note **Input layout (trusted, not validated)**
     *   This is the single place where the glibc era binary layout is trusted.
     *   `nl_langinfo(_NL_TIME_ERA_ENTRIES)` is assumed to point at exactly
     *   `_NL_TIME_ERA_NUM_ENTRIES` consecutive, well-formed records; the pointer
     *   walk is **unchecked**, so malformed or truncated locale data (e.g. a tampered
     *   locale database) can cause out-of-bounds reads. System locale data is treated
     *   as trusted; if era data could ever originate from an untrusted source, this
     *   function is the boundary to harden.
     *
     *   Each record, relative to its start (base_ptr), is laid out as:
     *   - 8 × int32 header (read via memcpy):
     *       [0] direction marker ('+' / '-')   [1] offset
     *       [2] from_year - 1900   [3] from_month - 1   [4] from_day
     *       [5] to_year   - 1900   [6] to_month   - 1   [7] to_day
     *   - NUL-terminated narrow name, then NUL-terminated narrow format string
     *   - padding to the next 4-byte boundary (relative to base_ptr)
     *   - NUL-terminated wide name, then NUL-terminated wide format string
     *
     * @note **Output invariants**
     *   - `from_year` / `to_year` are clamped against int32 overflow on the +1900
     *     addition (an open-ended "to" is normalised to Dec 31 of the int32 maximum).
     *   - `direction` is normalised to exactly `+1` / `-1` so that the linear formula
     *     @code
     *       era_year = offset + (calendar_year - from_year) * direction
     *     @endcode
     *     is uniformly valid for all era types.
     *     The normalisation mirrors glibc's era.c (≈ line 98,
     *     https://github.com/lattera/glibc/blob/master/time/era.c):
     *     for forward eras (from ≤ to, e.g. AD / Japanese / Thai), the marker maps
     *     directly: '+' → +1, '-' → -1;
     *     for backward eras (from > to), the marker is intentionally inverted.
     *     The reason: in a backward era, from_year is the epoch and to_year is a
     *     sentinel for "negative infinity". As the calendar year moves away from the
     *     epoch toward the past, (calendar - from_year) grows increasingly negative.
     *     Flipping the stored direction to +1 preserves the sign convention so that
     *     the same formula produces consistent era_year values regardless of which
     *     direction the era flows. This is a known limitation shared with glibc: no
     *     real glibc locale defines a backward era, so the path is never exercised
     *     in practice.
     *
     * @return A list of decoded `era_entry` objects; an empty vector if no era data exists.
     * @endif
     */
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

/**
 * @lang{ZH}
 * @brief `timeio` facet 的宽字符（`wchar_t` / `char32_t` UTF-32）locale 配置类。
 *
 * 此特化适用于 `wchar_t`，以及在 `wchar_t` 为 UTF-32 的平台上的 `char32_t`。
 * 通过内部构造 `timeio_conf<char>` 临时对象来加载 locale 数据，
 * 再将所有窄字符串转换为对应的宽字符或 UTF-32 字符串。
 *
 * @tparam CharT 字符类型，限定为 `wchar_t` 或（在 `wchar_t` 为 UTF-32 的平台上）`char32_t`。
 * @endif
 *
 * @lang{EN}
 * @brief Locale configuration class for the `timeio` facet specialised for wide characters
 *        (`wchar_t` / `char32_t` UTF-32).
 *
 * This specialization applies to `wchar_t`, and to `char32_t` on platforms where
 * `wchar_t` is UTF-32. It loads locale data by internally constructing a
 * `timeio_conf<char>` temporary, then converts all narrow strings to the
 * corresponding wide or UTF-32 strings.
 *
 * @tparam CharT The character type, constrained to `wchar_t` or (on UTF-32 platforms) `char32_t`.
 * @endif
 */
template <typename CharT>
    requires std::is_same_v<CharT, wchar_t> ||
            (std::is_same_v<CharT, char32_t> &&
            wchar_t_is_utf32)
class timeio_conf<CharT> : public ft_basic<timeio<CharT>>
{
    using era_entry = typename ft_basic<timeio<CharT>>::era_entry;

public:
    /**
     * @lang{ZH}
     * @brief 构造函数，通过 `timeio_conf<char>` 加载 locale 数据并转换为宽字符串。
     * @param name locale 名称（如 `"C"`、`"ja_JP.UTF-8"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that loads locale data via `timeio_conf<char>` and converts
     *        all strings to wide character strings.
     * @param name The locale name (e.g. `"C"`, `"ja_JP.UTF-8"`).
     * @endif
     */
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

    /**
     * @lang{ZH}
     * @brief 返回星期全称数组（索引 0 为星期日，索引 6 为星期六）。
     * @return 包含 7 个星期全称字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of full weekday names (index 0 = Sunday, index 6 = Saturday).
     * @return A constant reference to the array of 7 full weekday name strings.
     * @endif
     */
    virtual const std::array<std::basic_string<CharT>, 7>& day_names() const { return m_day; }
    /**
     * @lang{ZH}
     * @brief 返回星期缩写数组（索引 0 为星期日，索引 6 为星期六）。
     * @return 包含 7 个星期缩写字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of abbreviated weekday names (index 0 = Sunday, index 6 = Saturday).
     * @return A constant reference to the array of 7 abbreviated weekday name strings.
     * @endif
     */
    virtual const std::array<std::basic_string<CharT>, 7>& abbr_day_names() const { return m_abbr_day; }
    /**
     * @lang{ZH}
     * @brief 返回月份全称数组（索引 0 为一月，索引 11 为十二月）。
     * @return 包含 12 个月份全称字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of full month names (index 0 = January, index 11 = December).
     * @return A constant reference to the array of 12 full month name strings.
     * @endif
     */
    virtual const std::array<std::basic_string<CharT>, 12>& month_names() const { return m_month; }
    /**
     * @lang{ZH}
     * @brief 返回月份缩写数组（索引 0 为一月，索引 11 为十二月）。
     * @return 包含 12 个月份缩写字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of abbreviated month names (index 0 = January, index 11 = December).
     * @return A constant reference to the array of 12 abbreviated month name strings.
     * @endif
     */
    virtual const std::array<std::basic_string<CharT>, 12>& abbr_month_names() const { return m_abbr_month; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的替代数字字符串数组（最多 100 项）。
     * @return 包含 100 个替代数字字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale-defined alternative digit strings (up to 100 entries).
     * @return A constant reference to the array of 100 alternative digit strings.
     * @endif
     */
    virtual const std::array<std::basic_string<CharT>, 100>& alt_digit_names() const { return m_alt_digits; }
    /**
     * @lang{ZH}
     * @brief 返回 AM 时段字符串。
     * @return AM 字符串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the AM period string.
     * @return A constant reference to the AM string.
     * @endif
     */
    virtual const std::basic_string<CharT>& am_name() const { return m_am; }
    /**
     * @lang{ZH}
     * @brief 返回 PM 时段字符串。
     * @return PM 字符串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the PM period string.
     * @return A constant reference to the PM string.
     * @endif
     */
    virtual const std::basic_string<CharT>& pm_name() const { return m_pm; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期格式串（对应 `%x`）。
     * @return 日期格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale date format string (corresponding to `%x`).
     * @return A constant reference to the date format string.
     * @endif
     */
    virtual const std::basic_string<CharT>& date_format() const { return m_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期格式串（对应 `%Ex`）。
     * @return 纪元修饰日期格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified date format string (corresponding to `%Ex`).
     * @return A constant reference to the era-modified date format string.
     * @endif
     */
    virtual const std::basic_string<CharT>& era_date_format() const { return m_era_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 时间格式串（对应 `%X`）。
     * @return 时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale time format string (corresponding to `%X`).
     * @return A constant reference to the time format string.
     * @endif
     */
    virtual const std::basic_string<CharT>& time_format() const { return m_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰时间格式串（对应 `%EX`）。
     * @return 纪元修饰时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified time format string (corresponding to `%EX`).
     * @return A constant reference to the era-modified time format string.
     * @endif
     */
    virtual const std::basic_string<CharT>& era_time_format() const { return m_era_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回时间加时区格式串。
     * @return 时间加时区格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the time-plus-timezone format string.
     * @return A constant reference to the time-plus-timezone format string.
     * @endif
     */
    virtual const std::basic_string<CharT>& time_zone_format() const { return m_time_zone_format; }
    /**
     * @lang{ZH}
     * @brief 返回纪元修饰时间加时区格式串。
     * @return 纪元修饰时间加时区格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the era-modified time-plus-timezone format string.
     * @return A constant reference to the era-modified time-plus-timezone format string.
     * @endif
     */
    virtual const std::basic_string<CharT>& era_time_zone_format() const { return m_era_time_zone_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期时间格式串（对应 `%c`）。
     * @return 日期时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale date-time format string (corresponding to `%c`).
     * @return A constant reference to the date-time format string.
     * @endif
     */
    virtual const std::basic_string<CharT>& date_time_format() const { return m_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期时间格式串（对应 `%Ec`）。
     * @return 纪元修饰日期时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified date-time format string (corresponding to `%Ec`).
     * @return A constant reference to the era-modified date-time format string.
     * @endif
     */
    virtual const std::basic_string<CharT>& era_date_time_format() const { return m_era_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回日期时间加时区格式串。
     * @return 日期时间加时区格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the date-time-plus-timezone format string.
     * @return A constant reference to the date-time-plus-timezone format string.
     * @endif
     */
    virtual const std::basic_string<CharT>& date_time_zone_format() const { return m_date_time_zone_format; }
    /**
     * @lang{ZH}
     * @brief 返回纪元修饰日期时间加时区格式串。
     * @return 纪元修饰日期时间加时区格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the era-modified date-time-plus-timezone format string.
     * @return A constant reference to the era-modified date-time-plus-timezone format string.
     * @endif
     */
    virtual const std::basic_string<CharT>& era_date_time_zone_format() const { return m_era_date_time_zone_format; }
    /**
     * @lang{ZH}
     * @brief 返回 AM/PM 时间格式串（对应 `%r`）。
     * @return AM/PM 时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the AM/PM time format string (corresponding to `%r`).
     * @return A constant reference to the AM/PM time format string.
     * @endif
     */
    virtual const std::basic_string<CharT>& am_pm_format() const { return m_am_pm_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的纪元条目列表。
     * @return 纪元条目列表的常量引用；若 locale 无纪元定义则为空 `vector`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the list of locale-defined era entries.
     * @return A constant reference to the era entry list; an empty `vector` if the locale
     *         defines no eras.
     * @endif
     */
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

/**
 * @lang{ZH}
 * @brief `timeio` facet 的 `char8_t`（UTF-8）locale 配置类。
 *
 * 此特化通过 `init_from_u32` 内部模板辅助函数委托给 `timeio_conf<char32_t>` 来加载数据，
 * 再将所有 UTF-32 字符串转换为 UTF-8（`char8_t`）字符串。
 *
 * @note 对 `timeio_conf<char32_t>` 的引用位于成员函数模板 `init_from_u32` 内部（其类型
 *   为依赖类型），因此仅在该函数被实例化时才进行检查，而不会在类定义解析时进行。
 *   这意味着即使在 `timeio_conf<char32_t>` 不可用的平台上（`wchar_t` 非 UTF-32），
 *   仅命名或默认构造 `timeio_conf<char8_t>` 也不会触发错误；实际的构造路径在这类
 *   平台上会编译失败，此为预期行为。完整性要求未被刻意编码为此类的约束条件：
 *   在 requires 子句中使用非依赖的 `sizeof(timeio_conf<char32_t>)` 是硬错误（而非软
 *   约束失败），会破坏无关 `timeio_conf<T>` 的偏特化解析。
 * @endif
 *
 * @lang{EN}
 * @brief Locale configuration class for the `timeio` facet specialised for `char8_t` (UTF-8).
 *
 * This specialization delegates to `timeio_conf<char32_t>` via the `init_from_u32`
 * internal template helper, then converts all UTF-32 strings to UTF-8 (`char8_t`) strings.
 *
 * @note The reference to `timeio_conf<char32_t>` resides inside the member function template
 *   `init_from_u32` (where it is a dependent type), so it is only checked upon instantiation
 *   of that helper — never eagerly at class-definition parse time. This means that merely
 *   naming or default-constructing `timeio_conf<char8_t>` remains well-formed even on
 *   platforms where `timeio_conf<char32_t>` is unavailable (`wchar_t` is not UTF-32); only
 *   the construction path fails to compile there, which is intentional. The completeness
 *   requirement is deliberately NOT encoded as a constraint on this class: a non-dependent
 *   `sizeof(timeio_conf<char32_t>)` in the requires-clause is a hard error (not a soft
 *   constraint failure) and would break partial-specialization resolution for unrelated
 *   `timeio_conf<T>`.
 * @endif
 */
template <typename CharT>
    requires std::is_same_v<CharT, char8_t>
class timeio_conf<CharT> : public ft_basic<timeio<char8_t>>
{
public:
    /**
     * @lang{ZH}
     * @brief 构造函数，通过 `timeio_conf<char32_t>` 加载 locale 数据并转换为 UTF-8 字符串。
     * @param name locale 名称（如 `"C"`、`"zh_CN.UTF-8"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that loads locale data via `timeio_conf<char32_t>` and converts
     *        all strings to UTF-8 (`char8_t`) strings.
     * @param name The locale name (e.g. `"C"`, `"zh_CN.UTF-8"`).
     * @endif
     */
    timeio_conf(const std::string& name)
        : ft_basic<timeio<char8_t>>()
    {
        init_from_u32<>(name);
    }

private:
    /**
     * @lang{ZH}
     * @brief 委托给 `timeio_conf<char32_t>` 加载数据，并将所有字符串转换为 UTF-8（内部辅助函数）。
     *
     * 将 `T`（默认为 `char32_t`）模板化，是为了使函数体内对 `timeio_conf<T>` 的引用成为
     * 依赖类型，从而仅在该辅助函数被实例化时（即实际构造路径上）才进行检查，而不会在类
     * 定义解析时进行，避免在 `timeio_conf<char32_t>` 不可用的平台上产生硬错误。
     * @tparam T 固定为 `char32_t`；通过约束防止其他类型实例化。
     * @param name locale 名称。
     * @endif
     *
     * @lang{EN}
     * @brief Delegates to `timeio_conf<char32_t>` to load data, then converts all strings
     *        to UTF-8 (internal helper).
     *
     * Templating on `T` (defaulting to `char32_t`) makes the reference to `timeio_conf<T>`
     * inside the function body a dependent type, so it is only checked when the helper is
     * actually instantiated (i.e. on the construction path), not at class-definition parse
     * time — preventing a hard error on platforms where `timeio_conf<char32_t>` is
     * unavailable.
     * @tparam T Fixed to `char32_t`; other types are prevented by the constraint.
     * @param name The locale name.
     * @endif
     */
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
    /**
     * @lang{ZH}
     * @brief 返回星期全称数组（索引 0 为星期日，索引 6 为星期六）。
     * @return 包含 7 个星期全称字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of full weekday names (index 0 = Sunday, index 6 = Saturday).
     * @return A constant reference to the array of 7 full weekday name strings.
     * @endif
     */
    virtual const std::array<std::basic_string<char8_t>, 7>& day_names() const { return m_day; }
    /**
     * @lang{ZH}
     * @brief 返回星期缩写数组（索引 0 为星期日，索引 6 为星期六）。
     * @return 包含 7 个星期缩写字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of abbreviated weekday names (index 0 = Sunday, index 6 = Saturday).
     * @return A constant reference to the array of 7 abbreviated weekday name strings.
     * @endif
     */
    virtual const std::array<std::basic_string<char8_t>, 7>& abbr_day_names() const { return m_abbr_day; }
    /**
     * @lang{ZH}
     * @brief 返回月份全称数组（索引 0 为一月，索引 11 为十二月）。
     * @return 包含 12 个月份全称字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of full month names (index 0 = January, index 11 = December).
     * @return A constant reference to the array of 12 full month name strings.
     * @endif
     */
    virtual const std::array<std::basic_string<char8_t>, 12>& month_names() const { return m_month; }
    /**
     * @lang{ZH}
     * @brief 返回月份缩写数组（索引 0 为一月，索引 11 为十二月）。
     * @return 包含 12 个月份缩写字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the array of abbreviated month names (index 0 = January, index 11 = December).
     * @return A constant reference to the array of 12 abbreviated month name strings.
     * @endif
     */
    virtual const std::array<std::basic_string<char8_t>, 12>& abbr_month_names() const { return m_abbr_month; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的替代数字字符串数组（最多 100 项）。
     * @return 包含 100 个替代数字字符串的数组的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale-defined alternative digit strings (up to 100 entries).
     * @return A constant reference to the array of 100 alternative digit strings.
     * @endif
     */
    virtual const std::array<std::basic_string<char8_t>, 100>& alt_digit_names() const { return m_alt_digits; }
    /**
     * @lang{ZH}
     * @brief 返回 AM 时段字符串。
     * @return AM 字符串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the AM period string.
     * @return A constant reference to the AM string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& am_name() const { return m_am; }
    /**
     * @lang{ZH}
     * @brief 返回 PM 时段字符串。
     * @return PM 字符串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the PM period string.
     * @return A constant reference to the PM string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& pm_name() const { return m_pm; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期格式串（对应 `%x`）。
     * @return 日期格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale date format string (corresponding to `%x`).
     * @return A constant reference to the date format string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& date_format() const { return m_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期格式串（对应 `%Ex`）。
     * @return 纪元修饰日期格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified date format string (corresponding to `%Ex`).
     * @return A constant reference to the era-modified date format string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& era_date_format() const { return m_era_date_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 时间格式串（对应 `%X`）。
     * @return 时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale time format string (corresponding to `%X`).
     * @return A constant reference to the time format string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& time_format() const { return m_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰时间格式串（对应 `%EX`）。
     * @return 纪元修饰时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified time format string (corresponding to `%EX`).
     * @return A constant reference to the era-modified time format string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& era_time_format() const { return m_era_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回时间加时区格式串。
     * @return 时间加时区格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the time-plus-timezone format string.
     * @return A constant reference to the time-plus-timezone format string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& time_zone_format() const { return m_time_zone_format; }
    /**
     * @lang{ZH}
     * @brief 返回纪元修饰时间加时区格式串。
     * @return 纪元修饰时间加时区格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the era-modified time-plus-timezone format string.
     * @return A constant reference to the era-modified time-plus-timezone format string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& era_time_zone_format() const { return m_era_time_zone_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 日期时间格式串（对应 `%c`）。
     * @return 日期时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale date-time format string (corresponding to `%c`).
     * @return A constant reference to the date-time format string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& date_time_format() const { return m_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 纪元修饰日期时间格式串（对应 `%Ec`）。
     * @return 纪元修饰日期时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale era-modified date-time format string (corresponding to `%Ec`).
     * @return A constant reference to the era-modified date-time format string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& era_date_time_format() const { return m_era_date_time_format; }
    /**
     * @lang{ZH}
     * @brief 返回日期时间加时区格式串。
     * @return 日期时间加时区格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the date-time-plus-timezone format string.
     * @return A constant reference to the date-time-plus-timezone format string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& date_time_zone_format() const { return m_date_time_zone_format; }
    /**
     * @lang{ZH}
     * @brief 返回纪元修饰日期时间加时区格式串。
     * @return 纪元修饰日期时间加时区格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the era-modified date-time-plus-timezone format string.
     * @return A constant reference to the era-modified date-time-plus-timezone format string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& era_date_time_zone_format() const { return m_era_date_time_zone_format; }
    /**
     * @lang{ZH}
     * @brief 返回 AM/PM 时间格式串（对应 `%r`）。
     * @return AM/PM 时间格式串的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the AM/PM time format string (corresponding to `%r`).
     * @return A constant reference to the AM/PM time format string.
     * @endif
     */
    virtual const std::basic_string<char8_t>& am_pm_format() const { return m_am_pm_format; }
    /**
     * @lang{ZH}
     * @brief 返回 locale 定义的纪元条目列表。
     * @return 纪元条目列表的常量引用；若 locale 无纪元定义则为空 `vector`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the list of locale-defined era entries.
     * @return A constant reference to the era entry list; an empty `vector` if the locale
     *         defines no eras.
     * @endif
     */
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
