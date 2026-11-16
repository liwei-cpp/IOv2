// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file monetary_details.h
 * @lang{ZH}
 * 定义了 `monetary` facet 的实现细节，包括 `base_ft<monetary>` 基类和
 * `monetary_conf` 的各字符类型特化。`base_ft<monetary>` 提供基于 POSIX
 * `lconv` 标志的货币格式 pattern 构造工具；`monetary_conf` 的各特化
 * 在此基础上从区域设置名称读取 `lconv`，填充货币格式化所需的全部字段。
 * @endif
 *
 * @lang{EN}
 * Defines the implementation details of the `monetary` facet, including
 * the `base_ft<monetary>` base class and per-character-type specializations
 * of `monetary_conf`. `base_ft<monetary>` provides pattern-construction
 * utilities driven by POSIX `lconv` flags; the `monetary_conf` specializations
 * read `lconv` from a locale name and populate all fields required for
 * monetary formatting.
 * @endif
 */
#pragma once
#include <IOv2/common/clocale_wrapper.h>
#include <IOv2/common/metafunctions.h>
#include <IOv2/cvt/cvt_facilities.h>
#include <IOv2/facet/facet_common.h>
#include <IOv2/facet/facet_helper.h>
#include <IOv2/facet/platform/monetary_backend.h>

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace IOv2
{
template <typename CharT> class monetary;

namespace detail
{
// The monetary data of the locale called name, copied out with the thread switched to it.
// The conversions the configurations make afterwards switch the thread's locale themselves.
inline monetary_raw_punct monetary_snapshot(const std::string& name)
{
    clocale_wrapper loc(name);
    clocale_user    guard(loc);
    return monetary_read_punct();
}
}

/**
 * @lang{ZH}
 * @brief `monetary` facet 的基类，提供 POSIX `lconv` 到货币格式 pattern 的转换工具。
 *
 * 该类定义了表示货币格式组成部分的 `part` 枚举、描述完整格式顺序的 `pattern` 类型，
 * 以及将 POSIX `*_cs_precedes`、`*_sep_by_space` 和 `*_sign_posn` 标志转换为
 * `pattern` 的受保护静态工具函数。`monetary_conf` 的各特化继承此基类。
 *
 * @note 该类不直接实例化，仅作为 `monetary_conf` 的基类使用。
 * @endif
 *
 * @lang{EN}
 * @brief Base class for the `monetary` facet, providing POSIX `lconv`-to-pattern
 *        conversion utilities.
 *
 * This class defines the `part` enum representing the components of a monetary
 * format, the `pattern` type describing a complete format ordering, and
 * protected static helpers that convert POSIX `*_cs_precedes`, `*_sep_by_space`,
 * and `*_sign_posn` flags into a `pattern`. The `monetary_conf` specializations
 * inherit from this class.
 *
 * @note This class is not instantiated directly; it serves only as the
 *       base for `monetary_conf`.
 * @endif
 */
template <>
class base_ft<monetary> : public abs_ft
{
public:
    using abs_ft::abs_ft;

    /**
     * @lang{ZH}
     * @brief 货币格式 pattern 中各组成部分的枚举。
     *
     * 每个 `pattern` 由四个 `part` 值组成，依次描述货币字符串中
     * 货币符号、符号与数值之间的空格、符号字符串和数值的排列顺序。
     * @endif
     *
     * @lang{EN}
     * @brief Enumeration of the components that make up a monetary format pattern.
     *
     * Each `pattern` consists of four `part` values that together describe the
     * ordering of the currency symbol, optional space, sign string, and value
     * within a formatted monetary string.
     * @endif
     */
    enum class part : std::uint8_t
    {
        none,   ///< @lang{ZH} 占位槽，不输出任何内容（通常作为 pattern 末尾的填充）。 @endif @lang{EN} Padding slot that emits nothing (typically used at the end of a pattern). @endif
        space,  ///< @lang{ZH} 货币符号与数值之间的空格。 @endif @lang{EN} Space separator between the currency symbol and the value. @endif
        symbol, ///< @lang{ZH} 货币符号字符串。 @endif @lang{EN} Currency symbol string. @endif
        sign,   ///< @lang{ZH} 正负符号字符串。 @endif @lang{EN} Sign string (positive or negative). @endif
        value   ///< @lang{ZH} 货币数值部分。 @endif @lang{EN} Monetary value digits. @endif
    };

    /**
     * @lang{ZH}
     * @brief 描述货币字符串完整格式顺序的 pattern 类型。
     *
     * 由四个 `part` 值组成的固定长度数组，各元素依次指定输出的组成部分。
     * @endif
     *
     * @lang{EN}
     * @brief Pattern type describing the complete ordering of a monetary string.
     *
     * A fixed-length array of four `part` values, each specifying one output
     * component in order.
     * @endif
     */
    using pattern = std::array<part, 4>;

    /**
     * @lang{ZH}
     * @brief 将驱动 `s_construct_pattern` 的三个 POSIX `lconv` 标志捆绑为一个具名参数。
     *
     * 这三个字段均为小整数，若以位置参数传递极易被意外交换
     * （bugprone-easily-swappable-parameters）；在此将它们命名化，
     * 使每处调用自我说明，且不可能发生参数交换。
     * @endif
     *
     * @lang{EN}
     * @brief Groups the three POSIX `lconv` flags that drive `s_construct_pattern`
     *        into one named argument.
     *
     * These are all small integers and trivially swapped if passed positionally
     * (bugprone-easily-swappable-parameters); naming the fields here makes each
     * call site self-documenting and the swap impossible.
     * @endif
     */
    struct pattern_spec
    {
        int8_t precedes;     ///< @lang{ZH} `*_cs_precedes`：货币符号在数值之前（1）还是之后（0）。 @endif @lang{EN} `*_cs_precedes`: symbol precedes (1) or follows (0) the value. @endif
        int8_t sep_by_space; ///< @lang{ZH} `*_sep_by_space`：0 无空格；1 空格在货币符号（连同贴着它的符号字符串）与数值之间；2 空格在符号字符串旁边（C11 7.11.2.1）。 @endif @lang{EN} `*_sep_by_space`: 0 no space; 1 a space between the symbol (with any sign glued to it) and the value; 2 a space next to the sign string (C11 7.11.2.1). @endif
        int    sign_posn;    ///< @lang{ZH} `*_sign_posn`：符号的位置（0..4）。 @endif @lang{EN} `*_sign_posn`: sign placement (0..4). @endif
    };

protected:
    /**
     * @lang{ZH}
     * @brief 当区域设置的 `sign_posn` 超出范围时使用的默认 pattern。
     *
     * 格式为 `symbol sign none value`，对应货币符号在前、符号字符串紧随其后的保守布局。
     * @endif
     *
     * @lang{EN}
     * @brief Default pattern used when the locale's `sign_posn` is out of range.
     *
     * The format is `symbol sign none value`, a conservative layout with the
     * currency symbol first followed immediately by the sign string.
     * @endif
     */
    inline const static pattern s_default_pattern = {part::symbol, part::sign, part::none, part::value};

    /**
     * @lang{ZH}
     * @brief 根据 POSIX `lconv` 标志构建货币格式 pattern。
     *
     * 依据 `pattern_spec` 中的三个标志（符号位置、空格、符号字符串位置）
     * 组合出满足 `moneypunct` 约束的四元素 pattern。
     * 基本不变量：
     * - 需要排布的是两样东西：金额，以及货币符号；`sign_posn` 为 3 或 4 时符号字符串
     *   紧贴货币符号之前或之后，随之一同移动，其余取值则由符号字符串包住整个字段
     *   （0、1 在最前，2 在最后）。
     * - 若 `precedes`，则货币符号（连同贴着它的符号字符串）在 `value` 之前；否则相反。
     * - `sep_by_space` 为 1 时，`space` 落在货币符号（连同贴着它的符号字符串）与金额
     *   的接缝处。
     * - `sep_by_space` 为 2 时，`space` 落在符号字符串旁边：符号字符串贴着货币符号时
     *   在两者之间，否则在符号字符串与金额之间（C11 7.11.2.1 EXAMPLE 2）。`sign_posn`
     *   为 0 时符号字符串是包住整个字段的括号，不加空格。
     * - 两种情况下 `space` 都既不是第一个也不是最后一个元素。
     * - 没有 `space` 时字段少一格，由 `none` 补在末尾，这也是 `none` 唯一允许的位置。
     * - `sign_posn` 超出 0–4、或 `sep_by_space` 超出 0–2 时回退到 `s_default_pattern`。
     *
     * @param spec 包含 `precedes`、`sep_by_space` 和 `sign_posn` 的规格结构体。
     * @return 构造出的四元素 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Builds a monetary format pattern from POSIX `lconv` flags.
     *
     * Combines the three flags in `pattern_spec` (symbol position, space,
     * sign-string position) into a four-element `pattern` that satisfies
     * `moneypunct` constraints. Key invariants:
     * - Two things are laid out: the amount and the currency symbol. For `sign_posn`
     *   3 and 4 the sign string is glued to the symbol's front or back and travels
     *   with it; for the other values it brackets the whole field instead (in front
     *   for 0 and 1, behind for 2).
     * - If `precedes`, the symbol (with any sign glued to it) comes before `value`;
     *   otherwise reversed.
     * - With `sep_by_space` 1, the `space` sits on the seam between the symbol (with
     *   any sign glued to it) and the amount.
     * - With `sep_by_space` 2, the `space` sits next to the sign string: between it
     *   and the symbol when the two are glued together, otherwise between it and the
     *   amount (C11 7.11.2.1 EXAMPLE 2). With `sign_posn` 0 the sign is the pair of
     *   parentheses around the whole field, and no space is added.
     * - Either way the `space` is neither the opening nor the closing element.
     * - Without a space the field is one slot short; `none` fills the tail, which is
     *   the only position it is allowed to hold.
     * - A `sign_posn` outside 0–4 or a `sep_by_space` outside 0–2 falls back to
     *   `s_default_pattern`.
     *
     * @param spec A spec struct containing `precedes`, `sep_by_space`, and `sign_posn`.
     * @return The constructed four-element `pattern`.
     * @endif
     */
    static pattern s_construct_pattern(pattern_spec spec)
    {
        using enum part;
        const auto [precedes, sp, posn] = spec;

        // POSIX fills sign_posn with CHAR_MAX when the locale has nothing to say; a
        // value outside either range leaves nothing to build a layout from.
        if (posn < 0 || posn > 4 || sp < 0 || sp > 2)
            return s_default_pattern;

        // Two things get laid out: the amount, and the currency symbol -- which for
        // sign_posn 3 and 4 has the sign glued to its front or its back, and so
        // travels with it. Whichever of the two goes first is what `precedes` says.
        std::array<part, 2> group{};
        std::size_t         group_len = 0;
        if (posn == 3)
            group[group_len++] = sign;
        group[group_len++] = symbol;
        if (posn == 4)
            group[group_len++] = sign;

        pattern     ret{};
        std::size_t at = 0;

        // A sign that was not glued to the symbol brackets the whole field instead:
        // in front for sign_posn 0 and 1, behind for 2.
        if (posn <= 1)
            ret[at++] = sign;

        // sep_by_space 1 puts the separator on the seam between the symbol and the
        // amount.
        if (precedes)
        {
            for (std::size_t k = 0; k < group_len; ++k)
                ret[at++] = group[k];
            if (sp == 1)
                ret[at++] = space;
            ret[at++] = value;
        }
        else
        {
            ret[at++] = value;
            if (sp == 1)
                ret[at++] = space;
            for (std::size_t k = 0; k < group_len; ++k)
                ret[at++] = group[k];
        }

        if (posn == 2)
            ret[at++] = sign;

        // sep_by_space 2 puts it next to the sign instead: after a leading sign, before
        // a trailing one, and between a sign and the symbol it is glued to.
        if (sp == 2 && posn != 0)
        {
            std::size_t gap = 2;
            if (ret[0] == sign || (ret[1] == sign && ret[0] == symbol))
                gap = 1;
            for (std::size_t k = at; k > gap; --k)
                ret[k] = ret[k - 1];
            ret[gap] = space;
            ++at;
        }

        // Without a space the field is one slot short. `none` takes up the slack, and
        // being last is the one position it is allowed to hold.
        if (at < ret.size())
            ret[at++] = none;

        return ret;
    }

    /**
     * @lang{ZH}
     * @brief 将 POSIX `lconv` 的布尔标志字段规范化，消除 `CHAR_MAX` 哨兵值。
     *
     * POSIX 规定，当区域设置不提供 `*_cs_precedes` 或 `*_sep_by_space` 字段时，
     * `lconv` 将其填充为 `CHAR_MAX`。`s_construct_pattern` 把这些标志视为布尔值，
     * 原始的 `CHAR_MAX` 将错误地被解读为真值（即"符号在前"或"有空格"）；
     * 此函数将哨兵值映射为 0（保守默认值），与 `frac_digits` 的处理方式一致。
     * 必须在任何窄化转换之前执行此检查，因此调用者应传入原始的 `char` 类型 `lconv`
     * 字段，而非已窄化后的参数。（`*_sign_posn` 不需要此辅助函数：它有自己的取值范围，
     * `CHAR_MAX` 落在 0–4 之外，已被 `s_construct_pattern` 的范围检查导向
     * `s_default_pattern`。）
     *
     * @param v 原始的 `char` 类型 `lconv` 标志字段。
     * @return 规范化后的值：若 `v == CHAR_MAX` 则返回 `0`，否则返回 `static_cast<int8_t>(v)`。
     * @endif
     *
     * @lang{EN}
     * @brief Normalizes a POSIX `lconv` boolean-flag field by eliminating the
     *        `CHAR_MAX` sentinel.
     *
     * POSIX fills `lconv`'s `*_cs_precedes` / `*_sep_by_space` with `CHAR_MAX`
     * when the field is "not available" in the locale. `s_construct_pattern`
     * interprets these flags as booleans, where a raw `CHAR_MAX` would wrongly
     * read as truthy (i.e. "symbol precedes" / "space present"); this function
     * maps the sentinel to 0 — the conservative default — mirroring the
     * `frac_digits` handling. The test must happen before any narrowing, so
     * callers pass the raw `char` `lconv` field here rather than the narrowed
     * argument. (`*_sign_posn` needs no such helper: it has a range of its own, and
     * `CHAR_MAX` falls outside 0–4, so `s_construct_pattern`'s range check already
     * funnels it to `s_default_pattern`.)
     *
     * @param v The raw `char` `lconv` flag field.
     * @return The normalized value: `0` if `v == CHAR_MAX`, otherwise `static_cast<int8_t>(v)`.
     * @endif
     */
    static int8_t s_norm_flag(char v) { return (v == CHAR_MAX) ? int8_t{0} : static_cast<int8_t>(v); }

    /**
     * @lang{ZH}
     * @brief 取 `lconv::int_curr_symbol` 中的货币代码，去掉 C 规定的第 4 个字符。
     *
     * C11 7.11.2.1 规定 `int_curr_symbol` 的前三个字符是 ISO 4217 代码，第四个字符是
     * 国际货币符号与金额之间的分隔符，并且"For int_p_sep_by_space and
     * int_n_sep_by_space, the fourth character of int_curr_symbol is used instead of a
     * space"——分隔与否、分隔在哪由 `int_*_sep_by_space` 决定，而不是随符号一同输出。
     * 因此存下的符号只取代码；分隔一律由 pattern 的 `space` 承担。第四个字符本身不
     * 保存：已知的数据里它都是空格（glibc 与 macOS 的全部 locale），Windows 的 UCRT
     * 只给三个字符。宽字符配置在转换之后调用，按字符计数。
     *
     * @param s `int_curr_symbol`，已转换为配置的字符类型。
     * @return 长度为 4 时去掉最后一个字符，否则原样返回。
     * @endif
     *
     * @lang{EN}
     * @brief Takes the currency code out of `lconv::int_curr_symbol`, dropping the
     *        fourth character C defines.
     *
     * C11 7.11.2.1 makes the first three characters of `int_curr_symbol` the ISO 4217
     * code and the fourth the separator between the international symbol and the
     * amount, and says "For int_p_sep_by_space and int_n_sep_by_space, the fourth
     * character of int_curr_symbol is used instead of a space": whether and where to
     * separate is `int_*_sep_by_space`'s to say, not something the symbol carries. So
     * the symbol kept is the code alone, and the pattern's `space` does any separating.
     * The fourth character itself is not kept: in all known data it is a space (every
     * glibc and macOS locale), and the Windows UCRT gives three characters only. The wide
     * configurations call this after converting, so it counts characters.
     *
     * @param s `int_curr_symbol`, already in the configuration's character type.
     * @return The string less its last character if it is four long, else as it is.
     * @endif
     */
    template <typename CharT>
    static std::basic_string<CharT> s_int_curr_code(std::basic_string<CharT> s)
    {
        if (s.size() == 4)
            s.pop_back();
        return s;
    }

    /**
     * @lang{ZH} 由一组原样的 `lconv` 标志构建 pattern：先经 `s_norm_flag` 去掉 `CHAR_MAX`，再交给 `s_construct_pattern`。 @endif
     * @lang{EN} Builds a pattern from one set of raw `lconv` flags: `s_norm_flag` takes `CHAR_MAX` out, then `s_construct_pattern` lays it out. @endif
     */
    static pattern s_pattern(const detail::monetary_raw_flags& f)
    {
        return s_construct_pattern({.precedes = s_norm_flag(f.cs_precedes),
                                    .sep_by_space = s_norm_flag(f.sep_by_space),
                                    .sign_posn = f.sign_posn});
    }

    /**
     * @lang{ZH}
     * @brief 判断 `lconv` 是否完全没有提供货币数据。
     *
     * ISO C 规定 `char` 类型成员为 `CHAR_MAX`、`char*` 类型成员为 `""` 表示该值
     * "在当前区域设置中不可用"，并规定 `"C"` 区域设置下所有货币成员均取这些哨兵值
     * （§7.11 `struct lconv` 声明处的注释为规范性内容）。POSIX 对 POSIX 区域设置
     * 的 LC_MONETARY 同样规定 "All values shall be unavailable"。
     *
     * 标准只定义逐字段的"不可用"，并未定义区域设置级的"无货币数据"谓词，且允许部分
     * 字段可用而其余不可用。因此本函数对本类实际替换的每个字段逐一取合取，而非仅凭
     * 单个字段推断整体。
     *
     * @param raw 平台后端复制出的 `lconv` 数据。
     * @return 若符号、货币符号与小数位数均不可用则返回 `true`。
     * @endif
     *
     * @lang{EN}
     * @brief Tests whether `lconv` provides no monetary data at all.
     *
     * ISO C specifies `CHAR_MAX` for `char` members and `""` for `char*` members
     * to mean the value is "not available in the current locale", and mandates
     * these sentinels for every monetary member in the `"C"` locale (the comments
     * on the §7.11 `struct lconv` declaration are normative). POSIX likewise
     * states "All values shall be unavailable" for LC_MONETARY in the POSIX locale.
     *
     * The standard defines availability only per member — there is no locale-level
     * "no monetary data" predicate, and partial availability is permitted. So this
     * function conjoins every field this class would substitute rather than
     * inferring the whole from any single one.
     *
     * @param raw The `lconv` data the platform backend copied out.
     * @return `true` if the signs, currency symbols and fractional digits are all unavailable.
     * @endif
     */
    static bool s_monetary_unavailable(const detail::monetary_raw_punct& raw)
    {
        return (raw.frac_digits == CHAR_MAX) && (raw.int_frac_digits == CHAR_MAX)
            && (raw.nat_neg.sign_posn == CHAR_MAX) && (raw.int_neg.sign_posn == CHAR_MAX)
            && raw.negative_sign.empty() && raw.currency_symbol.empty()
            && raw.int_curr_symbol.empty();
    }
};

template <typename CharT> class monetary_conf;

/**
 * @lang{ZH}
 * @brief `monetary_conf` 的 `char` 特化，从 C 库的 `lconv` 直接读取货币格式化数据。
 *
 * 对于 `"C"` 和 `"POSIX"` 区域设置，使用硬编码的默认值；
 * 对于其他区域设置，经平台后端（`facet/platform/`）把 locale 的货币数据原样复制出来。
 * 各字段按 locale 的窄字符编码原样保存，不做转换。
 * @endif
 *
 * @lang{EN}
 * @brief Specialization of `monetary_conf` for `char`, reading monetary formatting
 *        data directly from the C library's `lconv`.
 *
 * For the `"C"` and `"POSIX"` locales, hard-coded defaults are used.
 * For other locales, the locale's monetary data is copied out as it is through the
 * platform backend (`facet/platform/`). Every field is kept as it is, in the locale's
 * narrow encoding, with no conversion.
 * @endif
 */
template <>
class monetary_conf<char> : public ft_basic<monetary<char>>
{
public:
    using char_type = char; ///< @lang{ZH} 此特化使用的字符类型。 @endif @lang{EN} The character type used by this specialization. @endif

public:
    /**
     * @lang{ZH}
     * @brief 构造函数，从指定区域设置名称读取货币格式化数据。
     *
     * @param name 区域设置名称（如 `"en_US.UTF-8"`），或 `"C"`/`"POSIX"` 使用默认值。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that reads monetary formatting data from the named locale.
     *
     * @param name The locale name (e.g. `"en_US.UTF-8"`), or `"C"`/`"POSIX"` for defaults.
     * @endif
     */
    monetary_conf(const std::string& name)
        : ft_basic<monetary<char>>()
    {
        if (FacetHelper::is_c_locale_name(name))
        { // "C" locale
            init_c_defaults();
        }
        else
        {
            const detail::monetary_raw_punct raw = detail::monetary_snapshot(name);
            if (s_monetary_unavailable(raw))
            {
                init_c_defaults();
                return;
            }

            // Every string is kept as the C library spells it, in the locale's narrow
            // encoding, however many bytes that takes. A sign position of 0 means
            // parentheses rather than the negative sign.
            m_positive_sign_nat = raw.positive_sign;
            m_positive_sign_int = raw.positive_sign;
            m_negative_sign_nat = (raw.nat_neg.sign_posn == 0) ? "()" : raw.negative_sign;
            m_negative_sign_int = (raw.int_neg.sign_posn == 0) ? "()" : raw.negative_sign;
            m_curr_symbol_nat = raw.currency_symbol;
            m_curr_symbol_int = s_int_curr_code(raw.int_curr_symbol);

            m_pos_format_nat = s_pattern(raw.nat_pos);
            m_neg_format_nat = s_pattern(raw.nat_neg);
            m_pos_format_int = s_pattern(raw.int_pos);
            m_neg_format_int = s_pattern(raw.int_neg);

            // A locale with no decimal point has no fractional places either. CHAR_MAX
            // is POSIX's "value not specified by the locale", which is 0 places.
            const bool point = !raw.mon_decimal_point.empty();
            m_decimal_point = point ? raw.mon_decimal_point : ".";
            m_thousands_sep = raw.mon_thousands_sep;
            m_frac_digits_nat = (!point || raw.frac_digits == CHAR_MAX) ? 0 : raw.frac_digits;
            m_frac_digits_int = (!point || raw.int_frac_digits == CHAR_MAX) ? 0 : raw.int_frac_digits;
            m_grouping = FacetHelper::lconv_grouping<char>(raw.mon_grouping, m_thousands_sep, m_decimal_point);
        }
    }

public:
    /**
     * @lang{ZH}
     * @brief 返回数字分组规则。
     * @return 描述每组位数的字节向量（POSIX 规范，末尾为 0 表示重复最后一组）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the digit-grouping specification.
     * @return A byte vector describing the number of digits per group (POSIX convention,
     *         trailing 0 means repeat the last group).
     * @endif
     */
    [[nodiscard]] virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }

    /**
     * @lang{ZH}
     * @brief 返回国际货币符号字符串（如 `"USD"`；C 的 `int_curr_symbol` 第 4 个字符不在其中）。
     * @return 国际货币符号。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the international currency symbol string (e.g. `"USD"`; the fourth character of C's `int_curr_symbol` is not part of it).
     * @return The international currency symbol.
     * @endif
     */
    [[nodiscard]] virtual const std::string& curr_symbol_int() const { return m_curr_symbol_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地货币符号字符串（如 `"$"`）。
     * @return 本地货币符号。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the local (national) currency symbol string (e.g. `"$"`).
     * @return The local currency symbol.
     * @endif
     */
    [[nodiscard]] virtual const std::string& curr_symbol_nat() const { return m_curr_symbol_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际格式的正数符号字符串。
     * @return 正数符号（通常为空字符串）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the positive sign string for the international format.
     * @return The positive sign (usually an empty string).
     * @endif
     */
    [[nodiscard]] virtual const std::string& positive_sign_int() const { return m_positive_sign_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地格式的正数符号字符串。
     * @return 正数符号（通常为空字符串）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the positive sign string for the national format.
     * @return The positive sign (usually an empty string).
     * @endif
     */
    [[nodiscard]] virtual const std::string& positive_sign_nat() const { return m_positive_sign_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际格式的负数符号字符串。
     * @return 负数符号（如 `"-"` 或 `"()"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the negative sign string for the international format.
     * @return The negative sign (e.g. `"-"` or `"()"`).
     * @endif
     */
    [[nodiscard]] virtual const std::string& negative_sign_int() const { return m_negative_sign_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地格式的负数符号字符串。
     * @return 负数符号（如 `"-"` 或 `"()"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the negative sign string for the national format.
     * @return The negative sign (e.g. `"-"` or `"()"`).
     * @endif
     */
    [[nodiscard]] virtual const std::string& negative_sign_nat() const { return m_negative_sign_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际格式正数的排列 pattern。
     * @return 描述货币符号、符号字符串和数值顺序的 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the positive format pattern for the international format.
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
     * @endif
     */
    [[nodiscard]] virtual const pattern& pos_format_int() const { return m_pos_format_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地格式正数的排列 pattern。
     * @return 描述货币符号、符号字符串和数值顺序的 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the positive format pattern for the national format.
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
     * @endif
     */
    [[nodiscard]] virtual const pattern& pos_format_nat() const { return m_pos_format_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际格式负数的排列 pattern。
     * @return 描述货币符号、符号字符串和数值顺序的 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the negative format pattern for the international format.
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
     * @endif
     */
    [[nodiscard]] virtual const pattern& neg_format_int() const { return m_neg_format_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地格式负数的排列 pattern。
     * @return 描述货币符号、符号字符串和数值顺序的 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the negative format pattern for the national format.
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
     * @endif
     */
    [[nodiscard]] virtual const pattern& neg_format_nat() const { return m_neg_format_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际货币格式的小数位数。
     * @return 小数点后的位数。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the number of fractional digits for the international format.
     * @return The number of digits after the decimal point.
     * @endif
     */
    [[nodiscard]] virtual int frac_digits_int() const { return m_frac_digits_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地货币格式的小数位数。
     * @return 小数点后的位数。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the number of fractional digits for the national format.
     * @return The number of digits after the decimal point.
     * @endif
     */
    [[nodiscard]] virtual int frac_digits_nat() const { return m_frac_digits_nat; }

    /**
     * @lang{ZH}
     * @brief 返回货币小数点，按 locale 的窄字符编码给出，可以是多个字节。
     * @return 小数点，非空（如 `"."`）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the monetary decimal point, in the locale's narrow encoding; it may be
     *        several bytes long.
     * @return The decimal point, never empty (e.g. `"."`).
     * @endif
     */
    [[nodiscard]] virtual const std::string& decimal_point() const { return m_decimal_point; }

    /**
     * @lang{ZH}
     * @brief 返回货币千位分隔符，按 locale 的窄字符编码给出，可以是多个字节。
     *
     * 为空表示不使用分隔符，此时 `grouping()` 必为空。
     * @return 千位分隔符（如 `","`），或空串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the monetary thousands separator, in the locale's narrow encoding; it
     *        may be several bytes long.
     *
     * An empty string means no separator is used, in which case `grouping()` is always
     * empty.
     * @return The thousands separator (e.g. `","`), or an empty string.
     * @endif
     */
    [[nodiscard]] virtual const std::string& thousands_sep() const { return m_thousands_sep; }

private:
    /**
     * @lang{ZH}
     * @brief 填充 C/POSIX 区域设置下的硬编码货币格式化数据。
     * @endif
     *
     * @lang{EN}
     * @brief Populates the hard-coded monetary formatting data for the C/POSIX locale.
     * @endif
     */
    void init_c_defaults()
    {
        m_decimal_point = ".";
        m_thousands_sep = ",";
        m_frac_digits_int = 0;
        m_frac_digits_nat = 0;
        m_negative_sign_int = "-";
        m_negative_sign_nat = "-";
        m_pos_format_nat = s_default_pattern;
        m_neg_format_nat = s_default_pattern;
        m_pos_format_int = s_default_pattern;
        m_neg_format_int = s_default_pattern;
    }

private:
    std::vector<uint8_t>    m_grouping;
    std::string             m_curr_symbol_int;
    std::string             m_curr_symbol_nat;
    std::string             m_positive_sign_int;
    std::string             m_positive_sign_nat;
    std::string             m_negative_sign_int;
    std::string             m_negative_sign_nat;
    pattern                 m_pos_format_int{};
    pattern                 m_pos_format_nat{};
    pattern                 m_neg_format_int{};
    pattern                 m_neg_format_nat{};
    int                     m_frac_digits_int;
    int                     m_frac_digits_nat;
    std::string             m_decimal_point;
    std::string             m_thousands_sep;
};

/**
 * @lang{ZH}
 * @brief `monetary_conf` 的 `wchar_t`/`char32_t` 特化，以宽字符存储货币格式化数据。
 *
 * 对于 `"C"` 和 `"POSIX"` 区域设置，使用硬编码的宽字符默认值；
 * 对于其他区域设置，先经平台后端（`facet/platform/`）把 locale 的货币数据原样复制出来，
 * 再转换为宽字符：C 库自己给出宽字符形式时（Windows UCRT 的 `_W_` 字段）直接用它们，
 * 否则经 `detail::to_wstring` / `detail::to_u32string` 整串转换窄字段。
 * 仅当 `CharT` 为 `wchar_t`，或 `char32_t` 且 `wchar_t` 为 UTF-32 时启用此特化。
 *
 * @tparam CharT 字符类型，必须为 `wchar_t` 或（在 UTF-32 wchar_t 平台上的）`char32_t`。
 * @endif
 *
 * @lang{EN}
 * @brief Specialization of `monetary_conf` for `wchar_t` / `char32_t`, storing
 *        monetary formatting data as wide characters.
 *
 * For the `"C"` and `"POSIX"` locales, hard-coded wide-character defaults are used.
 * For other locales, the locale's monetary data is first copied out as it is through the
 * platform backend (`facet/platform/`), then converted to wide characters: where the C
 * library gives a string in wide form itself (the Windows UCRT's `_W_` fields) that is
 * used as it is, otherwise the narrow field is converted whole via `detail::to_wstring` /
 * `detail::to_u32string`. This specialization is enabled only when `CharT` is `wchar_t`, or `char32_t`
 * on a platform where `wchar_t` is UTF-32.
 *
 * @tparam CharT The character type; must be `wchar_t` or (on a UTF-32 `wchar_t` platform) `char32_t`.
 * @endif
 */
template <typename CharT>
    requires std::is_same_v<CharT, wchar_t> ||
                (std::is_same_v<CharT, char32_t> &&
                 wchar_t_is_utf32)
class monetary_conf<CharT> : public ft_basic<monetary<CharT>>
{
public:
    using char_type = CharT; ///< @lang{ZH} 此特化使用的字符类型。 @endif @lang{EN} The character type used by this specialization. @endif

public:
    /**
     * @lang{ZH}
     * @brief 构造函数，从指定区域设置名称读取货币格式化数据并转换为宽字符。
     *
     * @param name 区域设置名称（如 `"zh_CN.UTF-8"`），或 `"C"`/`"POSIX"` 使用默认值。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that reads monetary formatting data from the named locale
     *        and converts it to wide characters.
     *
     * @param name The locale name (e.g. `"zh_CN.UTF-8"`), or `"C"`/`"POSIX"` for defaults.
     * @endif
     */
    monetary_conf(const std::string& name)
        : ft_basic<monetary<CharT>>()
    {
        if (FacetHelper::is_c_locale_name(name))
        { // "C" locale
            init_c_defaults();
        }
        else
        {
            using base = base_ft<monetary>;
            const detail::monetary_raw_punct raw = detail::monetary_snapshot(name);
            if (base::s_monetary_unavailable(raw))
            {
                init_c_defaults();
                return;
            }

            // A string the C library gives in wide form (the UCRT's _W_ fields) is taken
            // as it is; otherwise the narrow one is converted whole.
            const auto widen = [&name](const std::string& narrow, const std::optional<std::wstring>& wide) {
                if (wide)
                    return std::basic_string<CharT>(wide->begin(), wide->end());
                if (narrow.empty())
                    return std::basic_string<CharT>();
                if constexpr (std::is_same_v<CharT, wchar_t>)
                    return detail::to_wstring(narrow, name);
                else
                    return detail::to_u32string<CharT>(narrow, name);   // <CharT>: see facet_helper.h
            };
            const std::basic_string<CharT> parens{static_cast<CharT>('('), static_cast<CharT>(')')};

            // A sign position of 0 means parentheses rather than the negative sign.
            m_positive_sign_nat = widen(raw.positive_sign, raw.wpositive_sign);
            m_positive_sign_int = m_positive_sign_nat;
            const std::basic_string<CharT> negative = widen(raw.negative_sign, raw.wnegative_sign);
            m_negative_sign_nat = (raw.nat_neg.sign_posn == 0) ? parens : negative;
            m_negative_sign_int = (raw.int_neg.sign_posn == 0) ? parens : negative;
            m_curr_symbol_nat = widen(raw.currency_symbol, raw.wcurrency_symbol);
            m_curr_symbol_int = base::s_int_curr_code(widen(raw.int_curr_symbol, raw.wint_curr_symbol));

            m_pos_format_nat = base::s_pattern(raw.nat_pos);
            m_neg_format_nat = base::s_pattern(raw.nat_neg);
            m_pos_format_int = base::s_pattern(raw.int_pos);
            m_neg_format_int = base::s_pattern(raw.int_neg);

            // A locale with no decimal point has no fractional places either. CHAR_MAX
            // is POSIX's "value not specified by the locale", which is 0 places.
            m_decimal_point = widen(raw.mon_decimal_point, raw.wmon_decimal_point);
            const bool point = !m_decimal_point.empty();
            if (!point)
                m_decimal_point.assign(1, static_cast<CharT>('.'));
            m_thousands_sep = widen(raw.mon_thousands_sep, raw.wmon_thousands_sep);
            m_frac_digits_nat = (!point || raw.frac_digits == CHAR_MAX) ? 0 : raw.frac_digits;
            m_frac_digits_int = (!point || raw.int_frac_digits == CHAR_MAX) ? 0 : raw.int_frac_digits;
            m_grouping = FacetHelper::lconv_grouping<CharT>(raw.mon_grouping, m_thousands_sep, m_decimal_point);
        }
    }

public:
    /**
     * @lang{ZH}
     * @brief 返回数字分组规则。
     * @return 描述每组位数的字节向量。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the digit-grouping specification.
     * @return A byte vector describing the number of digits per group.
     * @endif
     */
    [[nodiscard]] virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }

    /**
     * @lang{ZH}
     * @brief 返回国际货币符号字符串。
     * @return 国际货币符号。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the international currency symbol string.
     * @return The international currency symbol.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& curr_symbol_int() const { return m_curr_symbol_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地货币符号字符串。
     * @return 本地货币符号。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the local (national) currency symbol string.
     * @return The local currency symbol.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& curr_symbol_nat() const { return m_curr_symbol_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际格式的正数符号字符串。
     * @return 正数符号（通常为空字符串）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the positive sign string for the international format.
     * @return The positive sign (usually an empty string).
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& positive_sign_int() const { return m_positive_sign_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地格式的正数符号字符串。
     * @return 正数符号（通常为空字符串）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the positive sign string for the national format.
     * @return The positive sign (usually an empty string).
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& positive_sign_nat() const { return m_positive_sign_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际格式的负数符号字符串。
     * @return 负数符号（如 `L"-"` 或 `L"()"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the negative sign string for the international format.
     * @return The negative sign (e.g. `L"-"` or `L"()"`).
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& negative_sign_int() const { return m_negative_sign_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地格式的负数符号字符串。
     * @return 负数符号（如 `L"-"` 或 `L"()"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the negative sign string for the national format.
     * @return The negative sign (e.g. `L"-"` or `L"()"`).
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& negative_sign_nat() const { return m_negative_sign_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际格式正数的排列 pattern。
     * @return 描述货币符号、符号字符串和数值顺序的 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the positive format pattern for the international format.
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
     * @endif
     */
    [[nodiscard]] virtual const base_ft<monetary>::pattern& pos_format_int() const { return m_pos_format_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地格式正数的排列 pattern。
     * @return 描述货币符号、符号字符串和数值顺序的 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the positive format pattern for the national format.
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
     * @endif
     */
    [[nodiscard]] virtual const base_ft<monetary>::pattern& pos_format_nat() const { return m_pos_format_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际格式负数的排列 pattern。
     * @return 描述货币符号、符号字符串和数值顺序的 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the negative format pattern for the international format.
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
     * @endif
     */
    [[nodiscard]] virtual const base_ft<monetary>::pattern& neg_format_int() const { return m_neg_format_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地格式负数的排列 pattern。
     * @return 描述货币符号、符号字符串和数值顺序的 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the negative format pattern for the national format.
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
     * @endif
     */
    [[nodiscard]] virtual const base_ft<monetary>::pattern& neg_format_nat() const { return m_neg_format_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际货币格式的小数位数。
     * @return 小数点后的位数。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the number of fractional digits for the international format.
     * @return The number of digits after the decimal point.
     * @endif
     */
    [[nodiscard]] virtual int frac_digits_int() const { return m_frac_digits_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地货币格式的小数位数。
     * @return 小数点后的位数。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the number of fractional digits for the national format.
     * @return The number of digits after the decimal point.
     * @endif
     */
    [[nodiscard]] virtual int frac_digits_nat() const { return m_frac_digits_nat; }

    /**
     * @lang{ZH}
     * @brief 返回货币小数点。
     * @return 小数点，非空。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the monetary decimal point.
     * @return The decimal point, never empty.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& decimal_point() const { return m_decimal_point; }

    /**
     * @lang{ZH}
     * @brief 返回货币千位分隔符。
     *
     * 为空表示不使用分隔符，此时 `grouping()` 必为空。
     * @return 千位分隔符，或空串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the monetary thousands separator.
     *
     * An empty string means no separator is used, in which case `grouping()` is always
     * empty.
     * @return The thousands separator, or an empty string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<CharT>& thousands_sep() const { return m_thousands_sep; }

private:
    /**
     * @lang{ZH}
     * @brief 填充 C/POSIX 区域设置下的硬编码货币格式化数据。
     * @endif
     *
     * @lang{EN}
     * @brief Populates the hard-coded monetary formatting data for the C/POSIX locale.
     * @endif
     */
    void init_c_defaults()
    {
        if constexpr (std::is_same_v<CharT, wchar_t>)
        {
            m_decimal_point = L".";
            m_thousands_sep = L",";
            m_negative_sign_int = L"-";
            m_negative_sign_nat = L"-";
        }
        else
        {
            m_decimal_point = U".";
            m_thousands_sep = U",";
            m_negative_sign_int = U"-";
            m_negative_sign_nat = U"-";
        }
        m_frac_digits_int = 0;
        m_frac_digits_nat = 0;
        m_pos_format_nat = base_ft<monetary>::s_default_pattern;
        m_neg_format_nat = base_ft<monetary>::s_default_pattern;
        m_pos_format_int = base_ft<monetary>::s_default_pattern;
        m_neg_format_int = base_ft<monetary>::s_default_pattern;
    }

private:
    std::vector<uint8_t>        m_grouping;
    std::basic_string<CharT>    m_curr_symbol_int;
    std::basic_string<CharT>    m_curr_symbol_nat;
    std::basic_string<CharT>    m_positive_sign_int;
    std::basic_string<CharT>    m_positive_sign_nat;
    std::basic_string<CharT>    m_negative_sign_int;
    std::basic_string<CharT>    m_negative_sign_nat;
    base_ft<monetary>::pattern      m_pos_format_int{};
    base_ft<monetary>::pattern      m_pos_format_nat{};
    base_ft<monetary>::pattern      m_neg_format_int{};
    base_ft<monetary>::pattern      m_neg_format_nat{};
    int                         m_frac_digits_int;
    int                         m_frac_digits_nat;
    std::basic_string<CharT>    m_decimal_point;
    std::basic_string<CharT>    m_thousands_sep;
};

/**
 * @lang{ZH}
 * @brief `monetary_conf` 的 `char8_t` 特化，以 UTF-8 字符串存储货币格式化数据。
 *
 * 按 locale 的编码选一个配置类委托：编码为 UTF-8 时委托给 `monetary_conf<char>`，其字节
 * 本身就是 UTF-8；否则委托给 `monetary_conf<wchar_t>`，再把宽字符编码为 UTF-8——这要求
 * `wchar_t` 为 Unicode（`wchar_t_is_utf32` 或 `wchar_t_is_utf16`，即 Linux 与 Windows）。
 * macOS 的 `wchar_t` 在非 UTF-8 locale 下不是 Unicode，没有可靠的转换途径，故在 macOS 上
 * 以非 UTF-8 locale 构造会抛出异常。做法与 `numeric_conf<char8_t>` 相同。
 * @endif
 *
 * @lang{EN}
 * @brief Specialization of `monetary_conf` for `char8_t`, storing monetary
 *        formatting data as UTF-8 strings.
 *
 * Delegates to one configuration class chosen by the locale's encoding: to
 * `monetary_conf<char>` when it is UTF-8, whose bytes are then UTF-8 already; otherwise
 * to `monetary_conf<wchar_t>`, encoding its wide characters as UTF-8 -- which needs a
 * Unicode `wchar_t` (`wchar_t_is_utf32` or `wchar_t_is_utf16`, i.e. Linux and Windows).
 * macOS's `wchar_t` is not Unicode under a non-UTF-8 locale, leaving no reliable
 * conversion, so constructing for a non-UTF-8 locale on macOS throws. This is what
 * `numeric_conf<char8_t>` does.
 * @endif
 */
template <typename CharT>
    requires std::is_same_v<CharT, char8_t>
class monetary_conf<CharT> : public ft_basic<monetary<char8_t>>
{
public:
    using char_type = char8_t; ///< @lang{ZH} 此特化使用的字符类型。 @endif @lang{EN} The character type used by this specialization. @endif

public:
    /**
     * @lang{ZH}
     * @brief 构造函数，从指定区域设置名称读取货币格式化数据。
     *
     * 对于 `"C"`/`"POSIX"` 区域设置，直接使用硬编码的 `char8_t` 默认值；
     * 对于其他区域设置，按其编码委托给 `monetary_conf<char>`（UTF-8）或
     * `monetary_conf<wchar_t>`（其他），再把各字段转换为 UTF-8。
     *
     * @param name 区域设置名称，或 `"C"`/`"POSIX"` 使用默认值。
     * @throws io_error 在 `wchar_t` 不是 Unicode 的平台（macOS）上，`name` 的编码不是 UTF-8。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that reads monetary formatting data from the named locale.
     *
     * For `"C"`/`"POSIX"` locales, hard-coded `char8_t` defaults are used directly.
     * For other locales, delegates by the encoding to `monetary_conf<char>` (UTF-8) or
     * `monetary_conf<wchar_t>` (anything else), then converts each field to UTF-8.
     *
     * @param name The locale name, or `"C"`/`"POSIX"` for defaults.
     * @throws io_error Where `wchar_t` is not Unicode (macOS), if `name`'s encoding is not
     *         UTF-8.
     * @endif
     */
    monetary_conf(const std::string& name)
        : ft_basic<monetary<char8_t>>()
    {
        if (FacetHelper::is_c_locale_name(name))
        { // "C" locale
            m_decimal_point = u8".";
            m_thousands_sep = u8",";
            m_negative_sign_int = u8"-";
            m_negative_sign_nat = u8"-";
            m_frac_digits_int = 0;
            m_frac_digits_nat = 0;
            m_pos_format_nat = base_ft<monetary>::s_default_pattern;
            m_neg_format_nat = base_ft<monetary>::s_default_pattern;
            m_pos_format_int = base_ft<monetary>::s_default_pattern;
            m_neg_format_int = base_ft<monetary>::s_default_pattern;
        }
        else if (detail::monetary_snapshot(name).utf8)
        {
            // The narrow configuration's bytes are UTF-8 already.
            take(monetary_conf<char>(name),
                 [](const std::string& s) { return std::u8string(s.begin(), s.end()); });
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
            take(monetary_conf<W>(name), [](const std::basic_string<W>& s) {
                return detail::to_u8string(std::basic_string_view<W>(s));
            });
        else
            throw io_error("monetary_conf<char8_t>: " + name +
                           " is not a UTF-8 locale, and wchar_t is not Unicode on this platform");
    }

    // Takes every field of the delegate, its strings through utf8.
    template <typename TConf, typename TUtf8>
    void take(const TConf& src, TUtf8 utf8)
    {
        m_grouping          = src.grouping();
        m_curr_symbol_int   = utf8(src.curr_symbol_int());
        m_curr_symbol_nat   = utf8(src.curr_symbol_nat());
        m_positive_sign_int = utf8(src.positive_sign_int());
        m_positive_sign_nat = utf8(src.positive_sign_nat());
        m_negative_sign_int = utf8(src.negative_sign_int());
        m_negative_sign_nat = utf8(src.negative_sign_nat());
        m_pos_format_int    = src.pos_format_int();
        m_pos_format_nat    = src.pos_format_nat();
        m_neg_format_int    = src.neg_format_int();
        m_neg_format_nat    = src.neg_format_nat();
        m_frac_digits_int   = src.frac_digits_int();
        m_frac_digits_nat   = src.frac_digits_nat();
        m_decimal_point     = utf8(src.decimal_point());
        m_thousands_sep     = utf8(src.thousands_sep());
    }

public:
    /**
     * @lang{ZH}
     * @brief 返回数字分组规则。
     * @return 描述每组位数的字节向量。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the digit-grouping specification.
     * @return A byte vector describing the number of digits per group.
     * @endif
     */
    [[nodiscard]] virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }

    /**
     * @lang{ZH}
     * @brief 返回国际货币符号字符串（UTF-8）。
     * @return 国际货币符号。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the international currency symbol string (UTF-8).
     * @return The international currency symbol.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& curr_symbol_int() const { return m_curr_symbol_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地货币符号字符串（UTF-8）。
     * @return 本地货币符号。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the local (national) currency symbol string (UTF-8).
     * @return The local currency symbol.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& curr_symbol_nat() const { return m_curr_symbol_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际格式的正数符号字符串（UTF-8）。
     * @return 正数符号（通常为空字符串）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the positive sign string for the international format (UTF-8).
     * @return The positive sign (usually an empty string).
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& positive_sign_int() const { return m_positive_sign_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地格式的正数符号字符串（UTF-8）。
     * @return 正数符号（通常为空字符串）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the positive sign string for the national format (UTF-8).
     * @return The positive sign (usually an empty string).
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& positive_sign_nat() const { return m_positive_sign_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际格式的负数符号字符串（UTF-8）。
     * @return 负数符号（如 `u8"-"` 或 `u8"()"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the negative sign string for the international format (UTF-8).
     * @return The negative sign (e.g. `u8"-"` or `u8"()"`).
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& negative_sign_int() const { return m_negative_sign_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地格式的负数符号字符串（UTF-8）。
     * @return 负数符号（如 `u8"-"` 或 `u8"()"`）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the negative sign string for the national format (UTF-8).
     * @return The negative sign (e.g. `u8"-"` or `u8"()"`).
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& negative_sign_nat() const { return m_negative_sign_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际格式正数的排列 pattern。
     * @return 描述货币符号、符号字符串和数值顺序的 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the positive format pattern for the international format.
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
     * @endif
     */
    [[nodiscard]] virtual const base_ft<monetary>::pattern& pos_format_int() const { return m_pos_format_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地格式正数的排列 pattern。
     * @return 描述货币符号、符号字符串和数值顺序的 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the positive format pattern for the national format.
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
     * @endif
     */
    [[nodiscard]] virtual const base_ft<monetary>::pattern& pos_format_nat() const { return m_pos_format_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际格式负数的排列 pattern。
     * @return 描述货币符号、符号字符串和数值顺序的 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the negative format pattern for the international format.
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
     * @endif
     */
    [[nodiscard]] virtual const base_ft<monetary>::pattern& neg_format_int() const { return m_neg_format_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地格式负数的排列 pattern。
     * @return 描述货币符号、符号字符串和数值顺序的 `pattern`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the negative format pattern for the national format.
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
     * @endif
     */
    [[nodiscard]] virtual const base_ft<monetary>::pattern& neg_format_nat() const { return m_neg_format_nat; }

    /**
     * @lang{ZH}
     * @brief 返回国际货币格式的小数位数。
     * @return 小数点后的位数。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the number of fractional digits for the international format.
     * @return The number of digits after the decimal point.
     * @endif
     */
    [[nodiscard]] virtual int frac_digits_int() const { return m_frac_digits_int; }

    /**
     * @lang{ZH}
     * @brief 返回本地货币格式的小数位数。
     * @return 小数点后的位数。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the number of fractional digits for the national format.
     * @return The number of digits after the decimal point.
     * @endif
     */
    [[nodiscard]] virtual int frac_digits_nat() const { return m_frac_digits_nat; }

    /**
     * @lang{ZH}
     * @brief 返回货币小数点（UTF-8 编码，可以是多个字节）。
     * @return 小数点，非空。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the monetary decimal point (UTF-8; it may be several bytes long).
     * @return The decimal point, never empty.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& decimal_point() const { return m_decimal_point; }

    /**
     * @lang{ZH}
     * @brief 返回货币千位分隔符（UTF-8 编码，可以是多个字节）。
     *
     * 为空表示不使用分隔符，此时 `grouping()` 必为空。
     * @return 千位分隔符，或空串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the monetary thousands separator (UTF-8; it may be several bytes long).
     *
     * An empty string means no separator is used, in which case `grouping()` is always
     * empty.
     * @return The thousands separator, or an empty string.
     * @endif
     */
    [[nodiscard]] virtual const std::basic_string<char8_t>& thousands_sep() const { return m_thousands_sep; }

private:
    std::vector<uint8_t>        m_grouping;
    std::basic_string<char8_t>  m_curr_symbol_int;
    std::basic_string<char8_t>  m_curr_symbol_nat;
    std::basic_string<char8_t>  m_positive_sign_int;
    std::basic_string<char8_t>  m_positive_sign_nat;
    std::basic_string<char8_t>  m_negative_sign_int;
    std::basic_string<char8_t>  m_negative_sign_nat;
    base_ft<monetary>::pattern      m_pos_format_int{};
    base_ft<monetary>::pattern      m_pos_format_nat{};
    base_ft<monetary>::pattern      m_neg_format_int{};
    base_ft<monetary>::pattern      m_neg_format_nat{};
    int                         m_frac_digits_int;
    int                         m_frac_digits_nat;
    std::basic_string<char8_t>  m_decimal_point;
    std::basic_string<char8_t>  m_thousands_sep;
};
}
