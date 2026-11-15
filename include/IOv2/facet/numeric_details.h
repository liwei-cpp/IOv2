// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

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
#include <IOv2/common/clocale_wrapper.h>
#include <IOv2/common/metafunctions.h>
#include <IOv2/cvt/cvt_facilities.h>
#include <IOv2/facet/facet_common.h>
#include <IOv2/facet/facet_helper.h>
#include <IOv2/facet/platform/numeric_backend.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace IOv2
{
template <typename CharT> class numeric_conf;
template <typename CharT> class numeric;

namespace detail
{
// The punctuation of the locale called name, copied out with the thread switched to it. The
// conversions the configurations make afterwards switch the thread's locale themselves.
inline numeric_raw_punct numeric_snapshot(const std::string& name)
{
    clocale_wrapper loc(name);
    clocale_user    guard(loc);
    return numeric_read_punct();
}
}

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
     * 先经平台后端（`facet/platform/`）把 locale 的标点与布尔名称原样复制出来，
     * 再做字符转换。
     *
     * 布尔名称在 POSIX 上取自 `nl_langinfo(YESSTR/NOSTR)`（macOS 取其冒号列表的首项），
     * 按 locale 的窄字符编码给出；Windows 没有这项数据。缺数据或为空时用 `"true"`/`"false"`。
     *
     * @param name locale 名称字符串（例如 `"zh_CN.UTF-8"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that reads and initializes all numeric formatting parameters from the specified locale.
     *
     * For "C" and "POSIX" locales, standard ASCII values are assigned directly.
     * For other locales, the locale's punctuation and boolean names are first copied
     * out as they are through the platform backend (`facet/platform/`), and the
     * character conversions come after.
     *
     * The boolean names come from `nl_langinfo(YESSTR/NOSTR)` on POSIX (the first item
     * of its colon-separated list on macOS), in the locale's narrow encoding; Windows has
     * no such data. Where they are missing or empty, `"true"`/`"false"` stand in.
     *
     * @param name The locale name string (e.g., `"zh_CN.UTF-8"`).
     * @endif
     */
    numeric_conf(const std::string& name)
        : ft_basic<numeric<char>>()
    {
        if (FacetHelper::is_c_locale_name(name))
        { // "C" locale
            m_decimal_point = '.';
            m_thousands_sep = ',';
            m_true_name = "true";
            m_false_name = "false";
            return;
        }

        const detail::numeric_raw_punct raw = detail::numeric_snapshot(name);
        const std::string& grp_raw = raw.grouping;

        m_decimal_point = FacetHelper::string_to_char_convert(raw.decimal_point, name);
        m_thousands_sep = FacetHelper::string_to_char_convert(raw.thousands_sep, name);

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
            for (std::size_t i = 0; i < grp_raw.size(); ++i)
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

        // An empty name would make numeric::get(bool&) fail unconditionally (a
        // zero-length prefix never matches) and numeric::put(bool) write nothing
        // but padding, so empty counts as missing.
        m_true_name  = raw.truename.empty()  ? "true"  : raw.truename;
        m_false_name = raw.falsename.empty() ? "false" : raw.falsename;
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
     * 先经平台后端（`facet/platform/`）把 locale 的标点与布尔名称原样复制出来，
     * 再转换为宽字符。C 库自己给出宽字符分隔符时（Windows UCRT 的 `_W_decimal_point`、
     * `_W_thousands_sep`）直接用它们，不经窄字符转换——代码页装不下的分隔符在窄字段里
     * 已被近似。布尔名称的来源与回退同 `numeric_conf<char>`。
     *
     * @param name locale 名称字符串（例如 `"zh_CN.UTF-8"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that reads and initializes all numeric formatting parameters from the specified locale.
     *
     * For "C" and "POSIX" locales, standard ASCII wide-character values are assigned
     * directly. For other locales, the locale's punctuation and boolean names are first
     * copied out as they are through the platform backend (`facet/platform/`), then
     * converted to wide characters. Where the C library gives the separators in wide form
     * itself (the Windows UCRT's `_W_decimal_point` and `_W_thousands_sep`), those are used
     * as they are rather than converted from the narrow ones, in which a separator the code
     * page cannot hold has already been approximated. The boolean names come and fall back
     * as in `numeric_conf<char>`.
     *
     * @param name The locale name string (e.g., `"zh_CN.UTF-8"`).
     * @endif
     */
    numeric_conf(const std::string& name)
        : ft_basic<numeric<CharT>>()
    {
        if (FacetHelper::is_c_locale_name(name))
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

        const detail::numeric_raw_punct raw = detail::numeric_snapshot(name);
        const std::string& grp_raw = raw.grouping;

        // The first character of a separator the C library gave in wide form, or def if empty.
        const auto first_or = [](const std::wstring& s, CharT def) {
            return s.empty() ? def : static_cast<CharT>(s[0]);
        };
        m_decimal_point = raw.wdecimal_point
            ? first_or(*raw.wdecimal_point, static_cast<CharT>('.'))
            : FacetHelper::string_to_widechar_convert<CharT>(
                  raw.decimal_point, name, static_cast<CharT>('.'));
        m_thousands_sep = raw.wthousands_sep
            ? first_or(*raw.wthousands_sep, static_cast<CharT>('\0'))
            : FacetHelper::string_to_widechar_convert<CharT>(
                  raw.thousands_sep, name, static_cast<CharT>('\0'));

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
            for (std::size_t i = 0; i < grp_raw.size(); ++i)
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

        // Empty counts as missing, as in numeric_conf<char>.
        if (raw.truename.empty())
        {
            if constexpr (std::is_same_v<CharT, wchar_t>) m_true_name = L"true";
            else                                          m_true_name = U"true";
        }
        else
        {
            if constexpr(std::is_same_v<CharT, wchar_t>)
                m_true_name = detail::to_wstring(raw.truename, name);
            else
                m_true_name = detail::to_u32string<CharT>(raw.truename, name);   // <CharT>: see facet_helper.h
        }

        if (raw.falsename.empty())
        {
            if constexpr (std::is_same_v<CharT, wchar_t>) m_false_name = L"false";
            else                                          m_false_name = U"false";
        }
        else
        {
            if constexpr(std::is_same_v<CharT, wchar_t>)
                m_false_name = detail::to_wstring(raw.falsename, name);
            else
                m_false_name = detail::to_u32string<CharT>(raw.falsename, name);
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
 * 按 locale 的编码选一个配置类委托：编码为 UTF-8 时委托给 `numeric_conf<char>`，其字节本身
 * 就是 UTF-8；否则委托给 `numeric_conf<wchar_t>`，再把宽字符编码为 UTF-8——这要求 `wchar_t`
 * 为 Unicode（`wchar_t_is_utf32` 或 `wchar_t_is_utf16`，即 Linux 与 Windows）。macOS 的
 * `wchar_t` 在非 UTF-8 locale 下不是 Unicode（探针实测 eucCN 下全角句点是 0xA3AE），没有
 * 可靠的转换途径，故在 macOS 上以非 UTF-8 locale 构造会抛出异常。
 *
 * 分隔符必须是单字节 UTF-8 字符（即 ASCII），否则回退到 ASCII 默认值：小数点回退为
 * `u8'.'`，千位分隔符回退为 `u8','` 并清空分组规则。当两个分隔符回退后重合时，分组规则
 * 也会被清空，以避免解析歧义。
 * @endif
 *
 * @lang{EN}
 * @brief Configuration class for the `numeric<char8_t>` facet.
 *
 * Delegates to one configuration class chosen by the locale's encoding: to
 * `numeric_conf<char>` when it is UTF-8, whose bytes are then UTF-8 already; otherwise to
 * `numeric_conf<wchar_t>`, encoding its wide characters as UTF-8 -- which needs a Unicode
 * `wchar_t` (`wchar_t_is_utf32` or `wchar_t_is_utf16`, i.e. Linux and Windows). macOS's
 * `wchar_t` is not Unicode under a non-UTF-8 locale (the probe found the full-width period
 * to be 0xA3AE under eucCN), leaving no reliable conversion, so constructing for a
 * non-UTF-8 locale on macOS throws.
 *
 * A separator must be a single-byte UTF-8 character (that is, ASCII), or it falls back to
 * an ASCII default: the decimal point to `u8'.'`, the thousands separator to `u8','` with
 * the grouping cleared. When the two separators coincide after fallback, the grouping is
 * also cleared to avoid parsing ambiguity.
 * @endif
 */
template <typename CharT>
    requires std::is_same_v<CharT, char8_t>
class numeric_conf<CharT> : public ft_basic<numeric<char8_t>>
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
    using char_type = char8_t;

public:
    /**
     * @lang{ZH}
     * @brief 构造函数，从指定的 locale 中读取并初始化所有数值格式化参数。
     *
     * 对于 "C" 和 "POSIX" locale，直接使用标准 ASCII UTF-8 值。对于其他 locale，
     * 按其编码委托给 `numeric_conf<char>`（UTF-8）或 `numeric_conf<wchar_t>`（其他），
     * 再把各参数转换为 UTF-8；分隔符不是单字节时使用默认值。
     *
     * @param name locale 名称字符串（例如 `"zh_CN.UTF-8"`）。
     * @throws io_error 在 `wchar_t` 不是 Unicode 的平台（macOS）上，`name` 的编码不是 UTF-8。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that reads and initializes all numeric formatting parameters from the specified locale.
     *
     * For "C" and "POSIX" locales, standard ASCII UTF-8 values are assigned directly.
     * For other locales, delegates by the encoding to `numeric_conf<char>` (UTF-8) or
     * `numeric_conf<wchar_t>` (anything else), then converts each parameter to UTF-8; a
     * separator that is not a single byte gets the default.
     *
     * @param name The locale name string (e.g., `"zh_CN.UTF-8"`).
     * @throws io_error Where `wchar_t` is not Unicode (macOS), if `name`'s encoding is not
     *         UTF-8.
     * @endif
     */
    numeric_conf(const std::string& name)
        : ft_basic<numeric<char8_t>>()
    {
        if (FacetHelper::is_c_locale_name(name))
        { // "C" locale
            m_decimal_point = u8'.';
            m_thousands_sep = u8',';
            m_true_name = u8"true";
            m_false_name = u8"false";
            return;
        }

        if (detail::numeric_snapshot(name).utf8)
        {
            // The narrow configuration's bytes are UTF-8 already.
            const numeric_conf<char> narrow(name);
            assign(static_cast<unsigned char>(narrow.decimal_point()),
                   static_cast<unsigned char>(narrow.thousands_sep()), narrow.grouping(),
                   std::u8string(narrow.truename().begin(), narrow.truename().end()),
                   std::u8string(narrow.falsename().begin(), narrow.falsename().end()));
        }
        else
            init_from_wide<>(name);
    }

private:
    // W only makes the body dependent, so where wchar_t is not Unicode the
    // discarded branch, and the to_u8string it names, is never instantiated.
    template <typename W = wchar_t>
    void init_from_wide(const std::string& name)
    {
        if constexpr (wchar_t_is_utf32 || wchar_t_is_utf16)
        {
            const numeric_conf<W> wide(name);
            assign(static_cast<char32_t>(wide.decimal_point()),
                   static_cast<char32_t>(wide.thousands_sep()), wide.grouping(),
                   detail::to_u8string(std::basic_string_view<W>(wide.truename())),
                   detail::to_u8string(std::basic_string_view<W>(wide.falsename())));
        }
        else
            throw io_error("numeric_conf<char8_t>: " + name +
                           " is not a UTF-8 locale, and wchar_t is not Unicode on this platform");
    }

    // Takes the delegate's values. A separator stays only if it is one UTF-8 byte,
    // i.e. ASCII; u8'\0' is no separator either, as it would pass for a NUL in the input.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): two callers, just above
    void assign(char32_t dp, char32_t ts, std::vector<uint8_t> grouping,
                std::u8string truename, std::u8string falsename)
    {
        const auto single_byte = [](char32_t c) { return c != 0 && c < 0x80; };
        m_decimal_point = single_byte(dp) ? static_cast<char8_t>(dp) : u8'.';

        const bool ts_representable = single_byte(ts);
        m_thousands_sep = ts_representable ? static_cast<char8_t>(ts) : u8',';

        m_true_name  = std::move(truename);
        m_false_name = std::move(falsename);

        m_grouping = std::move(grouping);
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
