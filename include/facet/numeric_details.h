/**
 * @file numeric_details.h
 * @lang{ZH}
 * 定义了 `numeric_conf` 类模板的各特化版本，这些类是 `numeric<CharT>` facet 的底层实现。
 * 每个特化版本从指定的 locale 中提取数值格式化所需的参数，包括小数点字符、千位分隔符、
 * 数字分组规则以及布尔值的文本表示。
 * @endif
 *
 * @lang{EN}
 * Defines the specializations of the `numeric_conf` class template, which are the
 * underlying implementations for the `numeric<CharT>` facet. Each specialization
 * extracts locale-dependent numeric formatting parameters, including the decimal point
 * character, thousands separator, digit grouping rules, and textual representations
 * of boolean values.
 * @endif
 */
#pragma once
#include <common/clocale_wrapper.h>
#include <common/metafunctions.h>
#include <cvt/cvt_facilities.h>
#include <facet/facet_common.h>
#include <facet/facet_helper.h>

#include <clocale>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include <langinfo.h>

namespace IOv2
{
template <typename CharT> class numeric_conf;
template <typename CharT> class numeric;

/**
 * @lang{ZH}
 * @brief `numeric<char>` facet 的配置类。
 *
 * 从指定的 locale 中提取窄字符（`char`）类型的数值格式化参数，包括小数点字符、
 * 千位分隔符、数字分组规则以及布尔值的文本名称。
 * @endif
 *
 * @lang{EN}
 * @brief Configuration class for the `numeric<char>` facet.
 *
 * Extracts narrow-character (`char`) numeric formatting parameters from the
 * specified locale, including the decimal point character, thousands separator,
 * digit grouping rules, and boolean text names.
 * @endif
 */
template <>
class numeric_conf<char> : public ft_basic<numeric<char>>
{
public:
    /**
     * @lang{ZH}
     * @brief 构造函数，从指定的 locale 中读取并初始化所有数值格式化参数。
     *
     * 对于 "C" 和 "POSIX" locale，直接使用标准 ASCII 值。对于其他 locale，
     * 在 locale 守卫有效期间将所有依赖 locale 的指针（`lconv*`、`nl_langinfo()`）
     * 快照为原始字符串，以防后续的 `setlocale()`/`uselocale()` 调用使指针失效，
     * 再于守卫结束后执行字符转换。
     *
     * @param name locale 名称字符串（例如 `"zh_CN.UTF-8"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that reads and initializes all numeric formatting parameters from the specified locale.
     *
     * For "C" and "POSIX" locales, standard ASCII values are assigned directly.
     * For other locales, all locale-dependent pointers (`lconv*`, `nl_langinfo()`)
     * are snapshotted into raw strings while the locale guard is active, preventing
     * invalidation by subsequent `setlocale()`/`uselocale()` calls; character
     * conversion is then performed after the guard ends.
     *
     * @param name The locale name string (e.g., `"zh_CN.UTF-8"`).
     * @endif
     */
    numeric_conf(const std::string& name)
        : ft_basic<numeric<char>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            m_decimal_point = '.';
            m_thousands_sep = ',';
            m_true_name = "true";
            m_false_name = "false";
            return;
        }

        // Snapshot all locale-dependent strings while the locale guard is
        // active. The lconv* and nl_langinfo() pointers may be invalidated
        // by any subsequent setlocale()/uselocale() call (e.g. inside
        // FacetHelper::string_to_char_convert), so copy first
        // and convert after.
        std::string dp_raw, ts_raw, grp_raw, yes_raw, no_raw;
        bool yes_set = false, no_set = false;
        {
            clocale_wrapper inter_locale(name.c_str());
            clocale_user guard(inter_locale);
            const lconv* lc = localeconv();
            if (lc->decimal_point) dp_raw  = lc->decimal_point;
            if (lc->thousands_sep) ts_raw  = lc->thousands_sep;
            if (lc->grouping)      grp_raw = lc->grouping;

            // Treat empty YESSTR/NOSTR the same as a missing key: an empty
            // m_true_name / m_false_name would make numeric::get(bool&) fail
            // unconditionally (zero-length prefix never matches) and let
            // numeric::put(bool) write nothing but padding.
            if (auto* p = nl_langinfo(YESSTR); p && *p) { yes_raw = p; yes_set = true; }
            if (auto* p = nl_langinfo(NOSTR);  p && *p) { no_raw  = p; no_set  = true; }
        }

        m_decimal_point = FacetHelper::string_to_char_convert(dp_raw, name);
        m_thousands_sep = FacetHelper::string_to_char_convert(ts_raw, name);

        // string_to_char_convert hard-codes '\0' as its failure sentinel. A
        // decimal point of '\0' would later be mistaken for an in-stream
        // null byte during parsing; fall back to '.' so the contract that
        // m_decimal_point is a real, distinguishable character is preserved.
        if (m_decimal_point == '\0') m_decimal_point = '.';

        if (m_thousands_sep != '\0' && !grp_raw.empty())
        {
            // Copy raw POSIX grouping bytes, then normalise into the
            // internal convention here at the POSIX boundary.
            // Downstream (numeric, user-derived numeric_conf) sees
            // only the internal form.
            m_grouping.resize(grp_raw.size());
            for (size_t i = 0; i < grp_raw.size(); ++i)
                m_grouping[i] = static_cast<uint8_t>(grp_raw[i]);
            FacetHelper::adjust_grouping(m_grouping);
        }

        // If a locale (or a user-derived numeric_conf that overrides
        // thousands_sep() / decimal_point()) ends up with the same
        // character for both separators, downstream extract_int /
        // extract_float in facet/numeric.h check thousands_sep BEFORE
        // decimal_point against each input character. An equal pair
        // makes the decimal-point branch unreachable and groups would
        // be parsed where a decimal mark was intended. Drop grouping
        // so parsing falls back to the no-grouping path. Mirrors the
        // same guard in numeric_conf<char8_t>, which can hit this case
        // after multi-byte UTF-8 separators collapse onto single-byte
        // fallbacks.
        if (m_thousands_sep == m_decimal_point)
            m_grouping.clear();

        if (yes_set) m_true_name  = yes_raw;
        else         m_true_name  = "true";
        if (no_set)  m_false_name = no_raw;
        else         m_false_name = "false";
    }

public:
    /**
     * @lang{ZH}
     * @brief 返回此 locale 的小数点字符。
     * @return 小数点字符。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the decimal point character for this locale.
     * @return The decimal point character.
     * @endif
     */
    [[nodiscard]] virtual char decimal_point() const { return m_decimal_point; }

    /**
     * @lang{ZH}
     * @brief 返回此 locale 的千位分隔符字符。
     *
     * 若返回值为 `'\0'`，表示不使用分隔符，此时 `grouping()` 必为空。
     * @return 千位分隔符字符，或 `'\0'`（不使用分隔符时）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the thousands separator character for this locale.
     *
     * A return value of `'\0'` indicates that no separator is used,
     * in which case `grouping()` is always empty.
     * @return The thousands separator character, or `'\0'` if none is used.
     * @endif
     */
    [[nodiscard]] virtual char thousands_sep() const { return m_thousands_sep; }

    /**
     * @lang{ZH}
     * @brief 返回此 locale 中 `true` 的文本表示。
     * @return `true` 的文本字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the textual representation of `true` for this locale.
     * @return The text string for `true`.
     * @endif
     */
    [[nodiscard]] virtual const std::string& truename() const { return m_true_name; }

    /**
     * @lang{ZH}
     * @brief 返回此 locale 中 `false` 的文本表示。
     * @return `false` 的文本字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the textual representation of `false` for this locale.
     * @return The text string for `false`.
     * @endif
     */
    [[nodiscard]] virtual const std::string& falsename() const { return m_false_name; }

    /**
     * @lang{ZH}
     * @brief 返回数字分组规则（内部规范化格式）。
     *
     * 向量中每个元素表示一个数字组的位数。若向量为空，则表示不进行分组。
     * @return 数字分组规则向量；为空时表示不分组。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the digit grouping specification in the internal normalized form.
     *
     * Each element in the vector specifies the number of digits in one group.
     * An empty vector indicates that no grouping is applied.
     * @return The digit grouping vector; empty means no grouping.
     * @endif
     */
    [[nodiscard]] virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }

private:
    char m_decimal_point;   ///< @lang{ZH} 小数点字符。 @endif @lang{EN} The decimal point character. @endif
    char m_thousands_sep;   ///< @lang{ZH} 千位分隔符；`'\0'` 表示不使用分隔符。 @endif @lang{EN} Thousands separator; `'\0'` means no separator. @endif
    std::string m_true_name;              ///< @lang{ZH} `true` 的文本表示。 @endif @lang{EN} Textual representation of `true`. @endif
    std::string m_false_name;             ///< @lang{ZH} `false` 的文本表示。 @endif @lang{EN} Textual representation of `false`. @endif
    std::vector<uint8_t> m_grouping;      ///< @lang{ZH} 数字分组规则（内部规范化格式）；为空时表示不分组。 @endif @lang{EN} Digit grouping rules (internal normalized form); empty means no grouping. @endif
};

/**
 * @lang{ZH}
 * @brief `numeric<CharT>` facet 的配置类，适用于宽字符类型。
 *
 * 从指定的 locale 中提取宽字符（`wchar_t` 或 `char32_t`）类型的数值格式化参数，
 * 包括小数点字符、千位分隔符、数字分组规则以及布尔值的文本名称。
 * 此特化仅适用于 `wchar_t`，以及在 `wchar_t` 为 UTF-32 的平台上的 `char32_t`。
 *
 * @tparam CharT 宽字符类型，必须为 `wchar_t` 或（在 UTF-32 平台上的）`char32_t`。
 * @endif
 *
 * @lang{EN}
 * @brief Configuration class for the `numeric<CharT>` facet for wide character types.
 *
 * Extracts wide-character (`wchar_t` or `char32_t`) numeric formatting parameters
 * from the specified locale, including the decimal point character, thousands separator,
 * digit grouping rules, and boolean text names. This specialization applies to `wchar_t`
 * and, on platforms where `wchar_t` is UTF-32, to `char32_t`.
 *
 * @tparam CharT The wide character type; must be `wchar_t` or (on UTF-32 platforms) `char32_t`.
 * @endif
 */
template <typename CharT>
    requires std::is_same_v<CharT, wchar_t> ||
                (std::is_same_v<CharT, char32_t> &&
                 wchar_t_is_utf32)
class numeric_conf<CharT> : public ft_basic<numeric<CharT>>
{
public:
    /**
     * @lang{ZH}
     * @brief 字符类型。
     * @endif
     *
     * @lang{EN}
     * @brief The character type.
     * @endif
     */
    using char_type = CharT;

public:
    /**
     * @lang{ZH}
     * @brief 构造函数，从指定的 locale 中读取并初始化所有数值格式化参数。
     *
     * 对于 "C" 和 "POSIX" locale，直接使用标准 ASCII 宽字符值。对于其他 locale，
     * 在 locale 守卫有效期间将所有依赖 locale 的指针（`lconv*`、`nl_langinfo()`）
     * 快照为原始窄字符串，以防后续的 `setlocale()`/`uselocale()` 调用使指针失效，
     * 再于守卫结束后执行宽字符转换。
     *
     * @param name locale 名称字符串（例如 `"zh_CN.UTF-8"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that reads and initializes all numeric formatting parameters from the specified locale.
     *
     * For "C" and "POSIX" locales, standard ASCII wide-character values are assigned
     * directly. For other locales, all locale-dependent pointers (`lconv*`, `nl_langinfo()`)
     * are snapshotted into raw narrow strings while the locale guard is active, preventing
     * invalidation by subsequent `setlocale()`/`uselocale()` calls; wide-character
     * conversion is then performed after the guard ends.
     *
     * @param name The locale name string (e.g., `"zh_CN.UTF-8"`).
     * @endif
     */
    numeric_conf(const std::string& name)
        : ft_basic<numeric<CharT>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            if constexpr (std::is_same_v<CharT, wchar_t>)
            {
                m_decimal_point = L'.';
                m_thousands_sep = L',';
                m_true_name = L"true";
                m_false_name = L"false";
            }
            else
            {
                m_decimal_point = U'.';
                m_thousands_sep = U',';
                m_true_name = U"true";
                m_false_name = U"false";
            }
            return;
        }

        // Snapshot all locale-dependent strings while the locale guard is
        // active. The lconv* and nl_langinfo() pointers may be invalidated
        // by any subsequent setlocale()/uselocale() call (e.g. inside
        // detail::to_wstring / detail::to_u32string), so copy first and
        // convert after.
        std::string dp_raw, ts_raw, grp_raw, yes_raw, no_raw;
        bool yes_set = false, no_set = false;
        {
            clocale_wrapper inter_locale(name.c_str());
            clocale_user guard(inter_locale);
            const lconv* lc = localeconv();
            if (lc->decimal_point) dp_raw  = lc->decimal_point;
            if (lc->thousands_sep) ts_raw  = lc->thousands_sep;
            if (lc->grouping)      grp_raw = lc->grouping;

            // Treat empty YESSTR/NOSTR the same as a missing key: an empty
            // m_true_name / m_false_name would make numeric::get(bool&) fail
            // unconditionally (zero-length prefix never matches) and let
            // numeric::put(bool) write nothing but padding.
            if (auto* p = nl_langinfo(YESSTR); p && *p) { yes_raw = p; yes_set = true; }
            if (auto* p = nl_langinfo(NOSTR);  p && *p) { no_raw  = p; no_set  = true; }
        }

        m_decimal_point = FacetHelper::string_to_widechar_convert<CharT>(
            dp_raw, name, static_cast<CharT>('.'));
        m_thousands_sep = FacetHelper::string_to_widechar_convert<CharT>(
            ts_raw, name, static_cast<CharT>('\0'));

        // string_to_widechar_convert only substitutes default_char on the
        // empty-input / empty-conversion paths; a successful conversion
        // that yields CharT('\0') is passed through. Treat that case the
        // same way and fall back to '.', so m_decimal_point is always a
        // real, distinguishable character.
        if (m_decimal_point == static_cast<CharT>('\0'))
            m_decimal_point = static_cast<CharT>('.');

        if (m_thousands_sep != static_cast<CharT>('\0') && !grp_raw.empty())
        {
            // Copy raw POSIX grouping bytes, then normalise into the
            // internal convention here at the POSIX boundary.
            m_grouping.resize(grp_raw.size());
            for (size_t i = 0; i < grp_raw.size(); ++i)
                m_grouping[i] = static_cast<uint8_t>(grp_raw[i]);
            FacetHelper::adjust_grouping(m_grouping);
        }

        // If a locale (or a user-derived numeric_conf that overrides
        // thousands_sep() / decimal_point()) ends up with the same
        // character for both separators, downstream extract_int /
        // extract_float in facet/numeric.h check thousands_sep BEFORE
        // decimal_point against each input character. An equal pair
        // makes the decimal-point branch unreachable and groups would
        // be parsed where a decimal mark was intended. Drop grouping
        // so parsing falls back to the no-grouping path. Mirrors the
        // same guard in numeric_conf<char8_t>, which can hit this case
        // after multi-byte UTF-8 separators collapse onto single-byte
        // fallbacks.
        if (m_thousands_sep == m_decimal_point)
            m_grouping.clear();

        if (!yes_set)
        {
            if constexpr (std::is_same_v<CharT, wchar_t>) m_true_name = L"true";
            else                                          m_true_name = U"true";
        }
        else
        {
            if constexpr(std::is_same_v<CharT, wchar_t>)
                m_true_name = detail::to_wstring(yes_raw.c_str(), name);
            else
                m_true_name = detail::to_u32string(yes_raw.c_str(), name);
        }

        if (!no_set)
        {
            if constexpr (std::is_same_v<CharT, wchar_t>) m_false_name = L"false";
            else                                          m_false_name = U"false";
        }
        else
        {
            if constexpr(std::is_same_v<CharT, wchar_t>)
                m_false_name = detail::to_wstring(no_raw.c_str(), name);
            else
                m_false_name = detail::to_u32string(no_raw.c_str(), name);
        }
    }

public:
    /**
     * @lang{ZH}
     * @brief 返回此 locale 的小数点字符。
     * @return 小数点字符。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the decimal point character for this locale.
     * @return The decimal point character.
     * @endif
     */
    [[nodiscard]] virtual CharT decimal_point() const { return m_decimal_point; }

    /**
     * @lang{ZH}
     * @brief 返回此 locale 的千位分隔符字符。
     *
     * 若返回值为 `CharT('\0')`，表示不使用分隔符，此时 `grouping()` 必为空。
     * @return 千位分隔符字符，或 `CharT('\0')`（不使用分隔符时）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the thousands separator character for this locale.
     *
     * A return value of `CharT('\0')` indicates that no separator is used,
     * in which case `grouping()` is always empty.
     * @return The thousands separator character, or `CharT('\0')` if none is used.
     * @endif
     */
    [[nodiscard]] virtual CharT thousands_sep() const { return m_thousands_sep; }

    /**
     * @lang{ZH}
     * @brief 返回此 locale 中 `true` 的文本表示。
     * @return `true` 的文本字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the textual representation of `true` for this locale.
     * @return The text string for `true`.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& truename() const { return m_true_name; }

    /**
     * @lang{ZH}
     * @brief 返回此 locale 中 `false` 的文本表示。
     * @return `false` 的文本字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the textual representation of `false` for this locale.
     * @return The text string for `false`.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& falsename() const { return m_false_name; }

    /**
     * @lang{ZH}
     * @brief 返回数字分组规则（内部规范化格式）。
     *
     * 向量中每个元素表示一个数字组的位数。若向量为空，则表示不进行分组。
     * @return 数字分组规则向量；为空时表示不分组。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the digit grouping specification in the internal normalized form.
     *
     * Each element in the vector specifies the number of digits in one group.
     * An empty vector indicates that no grouping is applied.
     * @return The digit grouping vector; empty means no grouping.
     * @endif
     */
    [[nodiscard]] virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }

private:
    CharT m_decimal_point;                  ///< @lang{ZH} 小数点字符。 @endif @lang{EN} The decimal point character. @endif
    CharT m_thousands_sep;                  ///< @lang{ZH} 千位分隔符；`CharT('\0')` 表示不使用分隔符。 @endif @lang{EN} Thousands separator; `CharT('\0')` means no separator. @endif
    std::basic_string<CharT> m_true_name;   ///< @lang{ZH} `true` 的文本表示。 @endif @lang{EN} Textual representation of `true`. @endif
    std::basic_string<CharT> m_false_name;  ///< @lang{ZH} `false` 的文本表示。 @endif @lang{EN} Textual representation of `false`. @endif
    std::vector<uint8_t>     m_grouping;    ///< @lang{ZH} 数字分组规则（内部规范化格式）；为空时表示不分组。 @endif @lang{EN} Digit grouping rules (internal normalized form); empty means no grouping. @endif
};

/**
 * @lang{ZH}
 * @brief `numeric<char8_t>` facet 的配置类。
 *
 * 通过委托给 `numeric_conf<char32_t>` 来获取 locale 参数，再将各参数收窄为单字节
 * UTF-8 字符。若某参数的 UTF-8 编码超过一个字节，或编码结果为 `u8'\0'`，则回退到
 * ASCII 默认值（小数点回退为 `u8'.'`，千位分隔符回退并清空分组规则）。
 * 当两个分隔符回退后重合时，分组规则也会被清空，以避免解析歧义。
 *
 * @note 此特化要求 `wchar_t` 为 32 位 UTF-32 编码单元（即 `wchar_t_is_utf32` 为真），
 *       因为其实现依赖 `numeric_conf<char32_t>`。
 * @endif
 *
 * @lang{EN}
 * @brief Configuration class for the `numeric<char8_t>` facet.
 *
 * Obtains locale parameters by delegating to `numeric_conf<char32_t>`, then narrows
 * each parameter to a single-byte UTF-8 character. If a parameter's UTF-8 encoding
 * spans more than one byte, or the encoded result is `u8'\0'`, it falls back to an
 * ASCII default (decimal point falls back to `u8'.'`; thousands separator falls back
 * and the grouping is cleared). When the two separators coincide after fallback,
 * the grouping is also cleared to avoid parsing ambiguity.
 *
 * @note This specialization requires `wchar_t` to be a 32-bit UTF-32 code unit
 *       (i.e., `wchar_t_is_utf32` is true), as the implementation delegates to
 *       `numeric_conf<char32_t>`.
 * @endif
 */
template <>
class numeric_conf<char8_t> : public ft_basic<numeric<char8_t>>
{
    static_assert(wchar_t_is_utf32,
        "numeric_conf<char8_t> delegates to numeric_conf<char32_t>, which "
        "requires wchar_t to be a 32-bit UTF-32 code unit. This platform "
        "(e.g. wchar_t is 16-bit UTF-16) does not satisfy that assumption.");

public:
    /**
     * @lang{ZH}
     * @brief 字符类型。
     * @endif
     *
     * @lang{EN}
     * @brief The character type.
     * @endif
     */
    using char_type = char8_t;

public:
    /**
     * @lang{ZH}
     * @brief 构造函数，从指定的 locale 中读取并初始化所有数值格式化参数。
     *
     * 对于 "C" 和 "POSIX" locale，直接使用标准 ASCII UTF-8 值。对于其他 locale，
     * 先构造 `numeric_conf<char32_t>` 获取 Unicode 参数，再逐一将其收窄为单字节
     * UTF-8 字符；若无法收窄则使用默认值。
     *
     * @param name locale 名称字符串（例如 `"zh_CN.UTF-8"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that reads and initializes all numeric formatting parameters from the specified locale.
     *
     * For "C" and "POSIX" locales, standard ASCII UTF-8 values are assigned directly.
     * For other locales, a `numeric_conf<char32_t>` is constructed first to obtain
     * Unicode parameters, which are then narrowed one by one to single-byte UTF-8
     * characters; a default value is used whenever narrowing is not possible.
     *
     * @param name The locale name string (e.g., `"zh_CN.UTF-8"`).
     * @endif
     */
    numeric_conf(const std::string& name)
        : ft_basic<numeric<char8_t>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            m_decimal_point = u8'.';
            m_thousands_sep = u8',';
            m_true_name = u8"true";
            m_false_name = u8"false";
            return;
        }

        numeric_conf<char32_t> numeric_temp(name);
        {
            const auto input = numeric_temp.decimal_point();
            auto output = detail::to_u8string(input);
            // Fall back to u8'.' when the UTF-8 encoding is not a single
            // byte OR when it is a single u8'\0' byte. The latter would be
            // indistinguishable from an in-stream null byte during parsing.
            m_decimal_point = (output.size() != 1 || output[0] == u8'\0')
                                  ? u8'.' : output[0];
        }

        bool ts_representable = false;
        {
            const auto input = numeric_temp.thousands_sep();
            auto output = detail::to_u8string(input);
            ts_representable = (output.size() == 1 && output[0] != u8'\0');
            m_thousands_sep = ts_representable ? output[0] : u8',';
        }

        m_true_name = detail::to_u8string(numeric_temp.truename());
        m_false_name = detail::to_u8string(numeric_temp.falsename());

        m_grouping = numeric_temp.grouping();

        if (!ts_representable)
            m_grouping.clear();

        // After the single-byte fallbacks above, m_thousands_sep may have
        // collapsed onto u8',' while m_decimal_point also resolved to u8','
        // (e.g. a locale where decimal_point is ',' and thousands_sep is a
        // multi-byte UTF-8 codepoint such as U+202F NARROW NO-BREAK SPACE).
        // When the two separators coincide, downstream extract_int /
        // extract_float would compare each input byte against both fields
        // with the same value, making grouping detection ambiguous. Drop
        // grouping in that case so parsing falls back to the no-grouping
        // path instead of misclassifying digits.
        if (m_thousands_sep == m_decimal_point)
            m_grouping.clear();
    }

public:
    /**
     * @lang{ZH}
     * @brief 返回此 locale 的小数点字符（单字节 UTF-8）。
     * @return 小数点字符。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the decimal point character for this locale (single-byte UTF-8).
     * @return The decimal point character.
     * @endif
     */
    [[nodiscard]] virtual char8_t decimal_point() const { return m_decimal_point; }

    /**
     * @lang{ZH}
     * @brief 返回此 locale 的千位分隔符字符（单字节 UTF-8）。
     *
     * 若返回值为 `u8'\0'`，表示不使用分隔符，此时 `grouping()` 必为空。
     * @return 千位分隔符字符，或 `u8'\0'`（不使用分隔符时）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the thousands separator character for this locale (single-byte UTF-8).
     *
     * A return value of `u8'\0'` indicates that no separator is used,
     * in which case `grouping()` is always empty.
     * @return The thousands separator character, or `u8'\0'` if none is used.
     * @endif
     */
    [[nodiscard]] virtual char8_t thousands_sep() const { return m_thousands_sep; }

    /**
     * @lang{ZH}
     * @brief 返回此 locale 中 `true` 的文本表示（UTF-8 编码）。
     * @return `true` 的 UTF-8 文本字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the textual representation of `true` for this locale (UTF-8 encoded).
     * @return The UTF-8 text string for `true`.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& truename() const { return m_true_name; }

    /**
     * @lang{ZH}
     * @brief 返回此 locale 中 `false` 的文本表示（UTF-8 编码）。
     * @return `false` 的 UTF-8 文本字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the textual representation of `false` for this locale (UTF-8 encoded).
     * @return The UTF-8 text string for `false`.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& falsename() const { return m_false_name; }

    /**
     * @lang{ZH}
     * @brief 返回数字分组规则（内部规范化格式）。
     *
     * 向量中每个元素表示一个数字组的位数。若向量为空，则表示不进行分组。
     * @return 数字分组规则向量；为空时表示不分组。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the digit grouping specification in the internal normalized form.
     *
     * Each element in the vector specifies the number of digits in one group.
     * An empty vector indicates that no grouping is applied.
     * @return The digit grouping vector; empty means no grouping.
     * @endif
     */
    [[nodiscard]] virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }

private:
    char8_t m_decimal_point;                    ///< @lang{ZH} 小数点字符（单字节 UTF-8）。 @endif @lang{EN} The decimal point character (single-byte UTF-8). @endif
    char8_t m_thousands_sep;                    ///< @lang{ZH} 千位分隔符（单字节 UTF-8）；`u8'\0'` 表示不使用分隔符。 @endif @lang{EN} Thousands separator (single-byte UTF-8); `u8'\0'` means no separator. @endif
    std::basic_string<char8_t> m_true_name;     ///< @lang{ZH} `true` 的 UTF-8 文本表示。 @endif @lang{EN} UTF-8 textual representation of `true`. @endif
    std::basic_string<char8_t> m_false_name;    ///< @lang{ZH} `false` 的 UTF-8 文本表示。 @endif @lang{EN} UTF-8 textual representation of `false`. @endif
    std::vector<uint8_t>       m_grouping;      ///< @lang{ZH} 数字分组规则（内部规范化格式）；为空时表示不分组。 @endif @lang{EN} Digit grouping rules (internal normalized form); empty means no grouping. @endif
};
}
