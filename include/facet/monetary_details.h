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
#include <common/clocale_wrapper.h>
#include <common/metafunctions.h>
#include <cvt/cvt_facilities.h>
#include <facet/facet_common.h>
#include <facet/facet_helper.h>

#include <array>
#include <climits>
#include <clocale>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace IOv2
{
template <typename CharT> class monetary;

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
        int8_t sep_by_space; ///< @lang{ZH} `*_sep_by_space`：货币符号与数值之间是否有空格。 @endif @lang{EN} `*_sep_by_space`: a space separates symbol and value. @endif
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
     * - 若 `precedes`，则输出顺序中 `symbol` 在 `value` 之前；否则相反。
     * - 若 `sep_by_space`，则在 `symbol` 与 `value` 之间插入 `space`；否则插入 `none`。
     * - `none` 不能作为第一个元素；`space` 不能作为第一个或最后一个元素。
     * - `sign_posn` 超出范围（默认分支）时回退到 `s_default_pattern`。
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
     * - If `precedes`, `symbol` comes before `value` in the output; otherwise reversed.
     * - If `sep_by_space`, a `space` is inserted between `symbol` and `value`;
     *   otherwise `none` is used.
     * - `none` is never the first element; `space` is never first or last.
     * - An out-of-range `sign_posn` (default branch) falls back to `s_default_pattern`.
     *
     * @param spec A spec struct containing `precedes`, `sep_by_space`, and `sign_posn`.
     * @return The constructed four-element `pattern`.
     * @endif
     */
    static pattern s_construct_pattern(pattern_spec spec)
    {
        using enum part;
        const auto [precedes, sp, posn] = spec;
        pattern ret;

        // This insanely complicated routine attempts to construct a valid
        // pattern for use with moneypunct. A couple of invariants:

        // if (precedes) symbol -> value
        // else value -> symbol

        // if (sp) space
        // else none

        // none == never first
        // space never first or last
        switch (posn)
        {
        case 0:
        case 1:
            // 1 The sign precedes the value and symbol.
            ret[0] = sign;
            if (sp)
            {
                // Pattern starts with sign.
                if (precedes)
                {
                    ret[1] = symbol;
                    ret[3] = value;
                }
                else
                {
                    ret[1] = value;
                    ret[3] = symbol;
                }
                ret[2] = space;
            }
            else
            {
                // Pattern starts with sign and ends with none.
                if (precedes)
                {
                    ret[1] = symbol;
                    ret[2] = value;
                }
                else
                {
                    ret[1] = value;
                    ret[2] = symbol;
                }
                ret[3] = none;
            }
            break;
        case 2:
            // 2 The sign follows the value and symbol.
            if (sp)
            {
                // Pattern either ends with sign.
                if (precedes)
                {
                    ret[0] = symbol;
                    ret[2] = value;
                }
                else
                {
                    ret[0] = value;
                    ret[2] = symbol;
                }
                ret[1] = space;
                ret[3] = sign;
            }
            else
            {
                // Pattern ends with sign then none.
                if (precedes)
                {
                    ret[0] = symbol;
                    ret[1] = value;
                }
                else
                {
                    ret[0] = value;
                    ret[1] = symbol;
                }
                ret[2] = sign;
                ret[3] = none;
            }
            break;
        case 3:
            // 3 The sign immediately precedes the symbol.
            if (precedes)
            {
                ret[0] = sign;
                ret[1] = symbol;
                if (sp)
                {
                    ret[2] = space;
                    ret[3] = value;
                }
                else
                {
                    ret[2] = value;
                    ret[3] = none;
                }
            }
            else
            {
                ret[0] = value;
                if (sp)
                {
                    ret[1] = space;
                    ret[2] = sign;
                    ret[3] = symbol;
                }
                else
                {
                    ret[1] = sign;
                    ret[2] = symbol;
                    ret[3] = none;
                }
            }
            break;
        case 4:
            // 4 The sign immediately follows the symbol.
            if (precedes)
            {
                ret[0] = symbol;
                ret[1] = sign;
                if (sp)
                {
                    ret[2] = space;
                    ret[3] = value;
                }
                else
                {
                    ret[2] = value;
                    ret[3] = none;
                }
            }
            else
            {
                ret[0] = value;
                if (sp)
                {
                    ret[1] = space;
                    ret[2] = symbol;
                    ret[3] = sign;
                }
                else
                {
                    ret[1] = symbol;
                    ret[2] = sign;
                    ret[3] = none;
                }
            }
            break;
        default:
            ret = s_default_pattern;
        }
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
     * 字段，而非已窄化后的参数。（`*_sign_posn` 不需要此辅助函数：其 `CHAR_MAX`
     * 已由 switch 的默认分支导向 `s_default_pattern`。）
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
     * argument. (`*_sign_posn` needs no such helper: its `CHAR_MAX` is already
     * funnelled to `s_default_pattern` by the switch's default branch.)
     *
     * @param v The raw `char` `lconv` flag field.
     * @return The normalized value: `0` if `v == CHAR_MAX`, otherwise `static_cast<int8_t>(v)`.
     * @endif
     */
    static int8_t s_norm_flag(char v) { return (v == CHAR_MAX) ? int8_t{0} : static_cast<int8_t>(v); }
};

template <typename CharT> class monetary_conf;

/**
 * @lang{ZH}
 * @brief `monetary_conf` 的 `char` 特化，从 POSIX `lconv` 直接读取货币格式化数据。
 *
 * 对于 `"C"` 和 `"POSIX"` 区域设置，使用硬编码的默认值；
 * 对于其他区域设置，通过 `clocale_wrapper` 切换至目标区域设置并调用
 * `localeconv()` 读取数据。所有 `lconv` 字段在任何转换操作之前整体快照，
 * 以避免转换器内部的 `setlocale()`/`uselocale()` 调用使 `lconv` 指针失效。
 * @endif
 *
 * @lang{EN}
 * @brief Specialization of `monetary_conf` for `char`, reading monetary formatting
 *        data directly from POSIX `lconv`.
 *
 * For the `"C"` and `"POSIX"` locales, hard-coded defaults are used.
 * For other locales, the target locale is switched via `clocale_wrapper`
 * and `localeconv()` is called to read the data. All `lconv` fields are
 * snapshotted in bulk before any conversion operation, to prevent the
 * converter's internal `setlocale()`/`uselocale()` calls from invalidating
 * the `lconv` pointers.
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
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            m_decimal_point = '.';
            m_thousands_sep = ',';
            m_frac_digits_int = 0;
            m_frac_digits_nat = 0;
            m_negative_sign_int = "-";
            m_negative_sign_nat = "-";
            m_pos_format_nat = s_default_pattern;
            m_neg_format_nat = s_default_pattern;
            m_pos_format_int = s_default_pattern;
            m_neg_format_int = s_default_pattern;
        }
        else
        {
            clocale_wrapper inter_locale(name.c_str());
            clocale_user guard(inter_locale);
            const lconv* lc = localeconv();

            // Read every lconv-derived field up front, BEFORE the
            // string_to_char_convert calls further down. That converter
            // transitively calls setlocale()/uselocale() (via
            // detail::to_wstring), which may invalidate the libc-owned
            // lconv pointers, so no lc-> access may follow it. The two
            // fields whose use is gated on the converted decimal point /
            // thousands separator (frac digits and grouping) are stashed
            // in locals here and applied after the conversion; the
            // conversion result only selects whether to adopt them, it is
            // never an input to reading lconv.
            std::vector<uint8_t> grouping_raw;
            if (lc->mon_grouping)
            {
                const size_t len = strlen(lc->mon_grouping);
                grouping_raw.resize(len);
                for (size_t i = 0; i < len; ++i)
                    grouping_raw[i] = static_cast<uint8_t>(lc->mon_grouping[i]);
            }

            m_positive_sign_nat = lc->positive_sign;
            m_positive_sign_int = m_positive_sign_nat;

            if (!lc->n_sign_posn)
                m_negative_sign_nat = "()";
            else
                m_negative_sign_nat = lc->negative_sign;

            if (!lc->int_n_sign_posn)
                m_negative_sign_int = "()";
            else
                m_negative_sign_int = lc->negative_sign;

            m_curr_symbol_nat = lc->currency_symbol;
            m_curr_symbol_int = lc->int_curr_symbol;

            m_pos_format_nat = s_construct_pattern({.precedes = s_norm_flag(lc->p_cs_precedes), .sep_by_space = s_norm_flag(lc->p_sep_by_space), .sign_posn = lc->p_sign_posn});
            m_neg_format_nat = s_construct_pattern({.precedes = s_norm_flag(lc->n_cs_precedes), .sep_by_space = s_norm_flag(lc->n_sep_by_space), .sign_posn = lc->n_sign_posn});
            m_pos_format_int = s_construct_pattern({.precedes = s_norm_flag(lc->int_p_cs_precedes), .sep_by_space = s_norm_flag(lc->int_p_sep_by_space), .sign_posn = lc->int_p_sign_posn});
            m_neg_format_int = s_construct_pattern({.precedes = s_norm_flag(lc->int_n_cs_precedes), .sep_by_space = s_norm_flag(lc->int_n_sep_by_space), .sign_posn = lc->int_n_sign_posn});

            std::string mdp_raw, mts_raw;
            if (lc->mon_decimal_point) mdp_raw = lc->mon_decimal_point;
            if (lc->mon_thousands_sep) mts_raw = lc->mon_thousands_sep;

            // CHAR_MAX is POSIX's "value not specified by the locale"; map it
            // to 0 fractional digits rather than taking it literally.
            const int lc_frac_nat = (lc->frac_digits == CHAR_MAX) ? 0 : lc->frac_digits;
            const int lc_frac_int = (lc->int_frac_digits == CHAR_MAX) ? 0 : lc->int_frac_digits;

            // No lc-> access beyond this point: the conversions may
            // invalidate the lconv pointers.
            // The decimal point and thousands separator are each stored as a
            // single char. A locale separator that does not fit in one char
            // (multibyte) cannot be represented: the convert yields '\0' and
            // the fallbacks below regress to '.' / ','.
            m_decimal_point = FacetHelper::string_to_char_convert(mdp_raw, name);
            m_thousands_sep = FacetHelper::string_to_char_convert(mts_raw, name);

            if (m_decimal_point == '\0')
            {
                m_decimal_point = '.';
                if (mdp_raw.empty())
                {
                    m_frac_digits_int = 0;
                    m_frac_digits_nat = 0;
                }
                else
                {
                    m_frac_digits_nat = lc_frac_nat;
                    m_frac_digits_int = lc_frac_int;
                }
            }
            else
            {
                m_frac_digits_nat = lc_frac_nat;
                m_frac_digits_int = lc_frac_int;
            }

            if (m_thousands_sep == '\0') m_thousands_sep = ',';
            else if (!grouping_raw.empty())
            {
                // Normalise raw POSIX grouping bytes into the internal
                // convention here at the POSIX boundary.
                m_grouping = std::move(grouping_raw);
                FacetHelper::adjust_grouping(m_grouping);
            }

            if (m_thousands_sep == m_decimal_point)
                m_grouping.clear();
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
     * @brief 返回国际货币符号字符串（如 `"USD "`）。
     * @return 国际货币符号。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the international currency symbol string (e.g. `"USD "`).
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
     * @return A `pattern` describing the ordering of symbol, sign string, and value.
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
     * @return A `pattern` describing the ordering of symbol, sign string, and value.
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
     * @return A `pattern` describing the ordering of symbol, sign string, and value.
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
     * @return A `pattern` describing the ordering of symbol, sign string, and value.
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
     * @brief 返回货币小数点字符。
     * @return 小数点字符（如 `'.'`）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the monetary decimal point character.
     * @return The decimal point character (e.g. `'.'`).
     * @endif
     */
    [[nodiscard]] virtual char decimal_point() const { return m_decimal_point; }

    /**
     * @lang{ZH}
     * @brief 返回千位分隔符字符。
     * @return 千位分隔符字符（如 `','`）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the thousands separator character.
     * @return The thousands separator character (e.g. `','`).
     * @endif
     */
    [[nodiscard]] virtual char thousands_sep() const { return m_thousands_sep; }

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
    char                    m_decimal_point;
    char                    m_thousands_sep;
};

/**
 * @lang{ZH}
 * @brief `monetary_conf` 的 `wchar_t`/`char32_t` 特化，以宽字符存储货币格式化数据。
 *
 * 对于 `"C"` 和 `"POSIX"` 区域设置，使用硬编码的宽字符默认值；
 * 对于其他区域设置，通过 `clocale_wrapper` 切换至目标区域设置，
 * 从 `localeconv()` 读取所有窄字符字段并将其批量快照，再通过
 * `detail::to_wstring` / `detail::to_u32string` 转换为宽字符。
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
 * For other locales, the target locale is switched via `clocale_wrapper`,
 * all narrow fields are read from `localeconv()` and snapshotted in bulk,
 * then converted to wide characters via `detail::to_wstring` / `detail::to_u32string`.
 * This specialization is enabled only when `CharT` is `wchar_t`, or `char32_t`
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
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            if constexpr (std::is_same_v<CharT, wchar_t>)
            {
                m_decimal_point = L'.';
                m_thousands_sep = L',';
                m_negative_sign_int = L"-";
                m_negative_sign_nat = L"-";
            }
            else
            {
                m_decimal_point = U'.';
                m_thousands_sep = U',';
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
        else
        {
            clocale_wrapper inter_locale(name.c_str());
            clocale_user guard(inter_locale);
            const lconv* lc = localeconv();

            // Snapshot every lconv-derived field up front, BEFORE any
            // converter runs. string_to_widechar_convert and
            // detail::to_wstring/to_u32string all transitively invoke
            // setlocale()/uselocale(), which may invalidate every string
            // pointer inside `lc`. So all narrow source strings are copied
            // into std::string locals and the value fields into locals
            // here; every conversion below reads only those snapshots, and
            // no lc-> access may follow the first conversion.
            const std::string mon_dp_raw =
                lc->mon_decimal_point ? lc->mon_decimal_point : "";
            const std::string mon_ts_raw =
                lc->mon_thousands_sep ? lc->mon_thousands_sep : "";
            const std::string positive_sign_raw =
                lc->positive_sign ? lc->positive_sign : "";
            const std::string negative_sign_raw =
                lc->negative_sign ? lc->negative_sign : "";
            const std::string curr_symbol_raw =
                lc->currency_symbol ? lc->currency_symbol : "";
            const std::string int_curr_symbol_raw =
                lc->int_curr_symbol ? lc->int_curr_symbol : "";

            // CHAR_MAX is POSIX's "value not specified by the locale"; map it
            // to 0 fractional digits rather than taking it literally.
            const int lc_frac_nat = (lc->frac_digits == CHAR_MAX) ? 0 : lc->frac_digits;
            const int lc_frac_int = (lc->int_frac_digits == CHAR_MAX) ? 0 : lc->int_frac_digits;
            const bool has_n_sign_posn     = lc->n_sign_posn;
            const bool has_int_n_sign_posn = lc->int_n_sign_posn;

            std::vector<uint8_t> grouping_raw;
            if (lc->mon_grouping)
            {
                const size_t len = strlen(lc->mon_grouping);
                grouping_raw.resize(len);
                for (size_t i = 0; i < len; ++i)
                    grouping_raw[i] = static_cast<uint8_t>(lc->mon_grouping[i]);
            }

            m_pos_format_nat = base_ft<monetary>::s_construct_pattern({.precedes = base_ft<monetary>::s_norm_flag(lc->p_cs_precedes), .sep_by_space = base_ft<monetary>::s_norm_flag(lc->p_sep_by_space), .sign_posn = lc->p_sign_posn});
            m_neg_format_nat = base_ft<monetary>::s_construct_pattern({.precedes = base_ft<monetary>::s_norm_flag(lc->n_cs_precedes), .sep_by_space = base_ft<monetary>::s_norm_flag(lc->n_sep_by_space), .sign_posn = lc->n_sign_posn});
            m_pos_format_int = base_ft<monetary>::s_construct_pattern({.precedes = base_ft<monetary>::s_norm_flag(lc->int_p_cs_precedes), .sep_by_space = base_ft<monetary>::s_norm_flag(lc->int_p_sep_by_space), .sign_posn = lc->int_p_sign_posn});
            m_neg_format_int = base_ft<monetary>::s_construct_pattern({.precedes = base_ft<monetary>::s_norm_flag(lc->int_n_cs_precedes), .sep_by_space = base_ft<monetary>::s_norm_flag(lc->int_n_sep_by_space), .sign_posn = lc->int_n_sign_posn});

            // No lc-> access beyond this point: the conversions may
            // invalidate the lconv pointers.
            // The decimal point and thousands separator are each a single
            // CharT. A locale separator that does not fit in one code unit
            // (e.g. multiple code points) cannot be represented: the convert
            // yields '\0' and the fallbacks below regress to '.' / ','.
            m_decimal_point = FacetHelper::string_to_widechar_convert<CharT>(
                mon_dp_raw, name, static_cast<CharT>('\0'));
            m_thousands_sep = FacetHelper::string_to_widechar_convert<CharT>(
                mon_ts_raw, name, static_cast<CharT>('\0'));

            if (static_cast<int>(m_decimal_point) == 0)
            {
                if constexpr (std::is_same_v<CharT, wchar_t>) m_decimal_point = L'.';
                else m_decimal_point = U'.';
                if (mon_dp_raw.empty())
                {
                    m_frac_digits_int = 0;
                    m_frac_digits_nat = 0;
                }
                else
                {
                    m_frac_digits_nat = lc_frac_nat;
                    m_frac_digits_int = lc_frac_int;
                }
            }
            else
            {
                m_frac_digits_nat = lc_frac_nat;
                m_frac_digits_int = lc_frac_int;
            }

            if (static_cast<int>(m_thousands_sep) == 0)
            {
                if constexpr (std::is_same_v<CharT, wchar_t>) m_thousands_sep = L',';
                else m_thousands_sep = U',';
            }
            else if (!grouping_raw.empty())
            {
                // Normalise raw POSIX grouping bytes into the internal
                // convention here at the POSIX boundary.
                m_grouping = std::move(grouping_raw);
                FacetHelper::adjust_grouping(m_grouping);
            }

            if (m_thousands_sep == m_decimal_point)
                m_grouping.clear();

            if constexpr (std::is_same_v<CharT, wchar_t>)
                m_positive_sign_nat = detail::to_wstring(positive_sign_raw, name);
            else
                m_positive_sign_nat = detail::to_u32string(positive_sign_raw, name);
            m_positive_sign_int = m_positive_sign_nat;

            if (!has_n_sign_posn)
            {
                if constexpr (std::is_same_v<CharT, wchar_t>) m_negative_sign_nat = L"()";
                else m_negative_sign_nat = U"()";
            }
            else
            {
                if constexpr (std::is_same_v<CharT, wchar_t>)
                    m_negative_sign_nat = detail::to_wstring(negative_sign_raw, name);
                else
                    m_negative_sign_nat = detail::to_u32string(negative_sign_raw, name);
            }

            if (!has_int_n_sign_posn)
            {
                if constexpr (std::is_same_v<CharT, wchar_t>) m_negative_sign_int = L"()";
                else m_negative_sign_int = U"()";
            }
            else
            {
                if constexpr (std::is_same_v<CharT, wchar_t>)
                    m_negative_sign_int = detail::to_wstring(negative_sign_raw, name);
                else
                    m_negative_sign_int = detail::to_u32string(negative_sign_raw, name);
            }

            if constexpr (std::is_same_v<CharT, wchar_t>)
                m_curr_symbol_nat = detail::to_wstring(curr_symbol_raw, name);
            else
                m_curr_symbol_nat = detail::to_u32string(curr_symbol_raw, name);

            if constexpr (std::is_same_v<CharT, wchar_t>)
                m_curr_symbol_int = detail::to_wstring(int_curr_symbol_raw, name);
            else
                m_curr_symbol_int = detail::to_u32string(int_curr_symbol_raw, name);
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
     * @return A `pattern` describing the ordering of symbol, sign string, and value.
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
     * @return A `pattern` describing the ordering of symbol, sign string, and value.
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
     * @return A `pattern` describing the ordering of symbol, sign string, and value.
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
     * @return A `pattern` describing the ordering of symbol, sign string, and value.
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
     * @brief 返回货币小数点字符。
     * @return 小数点字符。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the monetary decimal point character.
     * @return The decimal point character.
     * @endif
     */
    [[nodiscard]] virtual CharT decimal_point() const { return m_decimal_point; }

    /**
     * @lang{ZH}
     * @brief 返回千位分隔符字符。
     * @return 千位分隔符字符。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the thousands separator character.
     * @return The thousands separator character.
     * @endif
     */
    [[nodiscard]] virtual CharT thousands_sep() const { return m_thousands_sep; }

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
    CharT                       m_decimal_point;
    CharT                       m_thousands_sep;
};

/**
 * @lang{ZH}
 * @brief `monetary_conf` 的 `char8_t` 特化，以 UTF-8 字符串存储货币格式化数据。
 *
 * 对于非 `"C"`/`"POSIX"` 区域设置，此特化通过 `init_from_u32` 委托给
 * `monetary_conf<char32_t>` 来填充数据：先构造一个临时的 `char32_t` conf，
 * 再将各字段转换为 `char8_t`。
 *
 * @note **完整性要求：** 非 `"C"`/`"POSIX"` 路径要求 `monetary_conf<char32_t>`
 *       在 `monetary_conf<char8_t>` 构造时已是完整类型。该委托位于成员函数模板
 *       `init_from_u32` 中，其对 `monetary_conf<T>` 的引用依赖于模板参数，
 *       因此整个函数体仅在该辅助函数被实例化时才检查（即仅在非 `"C"`/`"POSIX"`
 *       构造路径上），而非提前检查（IFNDR）。仅命名或默认构造 `monetary_conf<char8_t>`
 *       即使在 `monetary_conf<char32_t>` 不可用的平台（`wchar_t` 非 UTF-32）上
 *       也是格式良好的；在这些平台上，只有非 `"C"`/`"POSIX"` 构造路径无法编译，
 *       这是有意为之。该完整性要求有意**不**编码为此类上的约束（constraint）：
 *       在 `requires` 子句中对 `sizeof(monetary_conf<char32_t>)` 的非依赖求值
 *       是硬错误（而非软约束失败），会破坏无关 `monetary_conf<T>` 的偏特化解析。
 *
 * @endif
 *
 * @lang{EN}
 * @brief Specialization of `monetary_conf` for `char8_t`, storing monetary
 *        formatting data as UTF-8 strings.
 *
 * For non-`"C"`/`"POSIX"` locales, this specialization delegates to
 * `monetary_conf<char32_t>` via `init_from_u32`: a temporary `char32_t` conf
 * is constructed and its fields are then converted to `char8_t`.
 *
 * @note **Completeness requirement:** The non-`"C"`/`"POSIX"` path requires
 *       `monetary_conf<char32_t>` to be a complete type when a `char8_t` conf
 *       is constructed. The delegation lives in the member function template
 *       `init_from_u32`, whose reference to `monetary_conf<T>` is dependent on
 *       a template parameter, so the whole body is checked only upon instantiation
 *       of that helper (i.e. only on the non-`"C"`/`"POSIX"` construction path),
 *       never eagerly (hence not IFNDR). Merely naming or default-constructing
 *       `monetary_conf<char8_t>` stays well-formed even on platforms where
 *       `monetary_conf<char32_t>` is unavailable (`wchar_t` is not UTF-32);
 *       there, only the non-`"C"`/`"POSIX"` construction path fails to compile,
 *       which is intended. The completeness requirement is deliberately NOT
 *       encoded as a constraint on this class: a non-dependent
 *       `sizeof(monetary_conf<char32_t>)` in the `requires`-clause is a hard
 *       error (not a soft constraint failure) and would break partial-specialization
 *       resolution for unrelated `monetary_conf<T>`.
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
     * 对于其他区域设置，委托给 `init_from_u32` 通过 `monetary_conf<char32_t>`
     * 读取并转换数据。
     *
     * @param name 区域设置名称，或 `"C"`/`"POSIX"` 使用默认值。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that reads monetary formatting data from the named locale.
     *
     * For `"C"`/`"POSIX"` locales, hard-coded `char8_t` defaults are used directly.
     * For other locales, delegates to `init_from_u32` which reads and converts
     * data via `monetary_conf<char32_t>`.
     *
     * @param name The locale name, or `"C"`/`"POSIX"` for defaults.
     * @endif
     */
    monetary_conf(const std::string& name)
        : ft_basic<monetary<char8_t>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            m_decimal_point = u8'.';
            m_thousands_sep = u8',';
            m_negative_sign_int = u8"-";
            m_negative_sign_nat = u8"-";
            m_frac_digits_int = 0;
            m_frac_digits_nat = 0;
            m_pos_format_nat = base_ft<monetary>::s_default_pattern;
            m_neg_format_nat = base_ft<monetary>::s_default_pattern;
            m_pos_format_int = base_ft<monetary>::s_default_pattern;
            m_neg_format_int = base_ft<monetary>::s_default_pattern;
        }
        else
            init_from_u32<>(name);
    }

private:
    /**
     * @lang{ZH}
     * @brief 委托给 `monetary_conf<char32_t>` 填充数据的私有辅助函数。
     *
     * `T` 默认为 `char32_t` 并受约束；将其模板化使函数体中对 `monetary_conf<T>`
     * 的引用成为依赖名，从而整个函数体仅在此辅助函数被实例化时（即非 `"C"`/`"POSIX"`
     * 构造路径）才检查，而非提前检查。
     *
     * 小数点和千位分隔符各为单个 `char8_t`（一个 UTF-8 码元），因此只有编码为
     * 单字节（ASCII）的分隔符才能表示；更宽的分隔符将回退为 `'.'`/`','`。
     *
     * @tparam T 必须为 `char32_t`（默认值）。
     * @param name 区域设置名称。
     * @endif
     *
     * @lang{EN}
     * @brief Private helper that delegates to `monetary_conf<char32_t>` to populate data.
     *
     * `T` is defaulted and constrained to `char32_t`; templating it makes the
     * `monetary_conf<T>` reference in the body dependent, so the whole body is
     * checked only upon instantiation of this helper (i.e. only on the
     * non-`"C"`/`"POSIX"` construction path), never eagerly.
     *
     * The decimal point and thousands separator are each a single `char8_t`
     * (one UTF-8 code unit), so only a separator that encodes to a single byte
     * (ASCII) fits; anything wider regresses to `'.'` / `','`.
     *
     * @tparam T Must be `char32_t` (the default).
     * @param name The locale name.
     * @endif
     */
    template <typename T = char32_t>
        requires std::is_same_v<T, char32_t>
    void init_from_u32(const std::string& name)
    {
        monetary_conf<T> monetary_tmp(name);

        // The decimal point and thousands separator are each a single char8_t
        // (one UTF-8 code unit), so only a separator that encodes to a single
        // byte (ASCII) fits; anything wider regresses to '.' / ','.
        {
            const auto& input = monetary_tmp.decimal_point();
            auto byte_str = detail::to_u8string(input);
            if (byte_str.size() == 1) m_decimal_point = byte_str[0];
            else m_decimal_point = u8'.';
        }

        {
            const auto& input = monetary_tmp.thousands_sep();
            auto byte_str = detail::to_u8string(input);
            if (byte_str.size() == 1 && byte_str[0] != u8'\0')
            {
                m_thousands_sep = byte_str[0];
                m_grouping = monetary_tmp.grouping();
            }
            else
                m_thousands_sep = u8',';
        }

        if (m_thousands_sep == m_decimal_point)
            m_grouping.clear();

        m_frac_digits_int = monetary_tmp.frac_digits_int();
        m_frac_digits_nat = monetary_tmp.frac_digits_nat();

        {
            const auto& input = monetary_tmp.positive_sign_int();
            m_positive_sign_int = detail::to_u8string(input);
        }
        {
            const auto& input = monetary_tmp.positive_sign_nat();
            m_positive_sign_nat = detail::to_u8string(input);
        }

        {
            const auto& input = monetary_tmp.negative_sign_int();
            m_negative_sign_int = detail::to_u8string(input);
        }
        {
            const auto& input = monetary_tmp.negative_sign_nat();
            m_negative_sign_nat = detail::to_u8string(input);
        }

        {
            const auto& input = monetary_tmp.curr_symbol_nat();
            m_curr_symbol_nat = detail::to_u8string(input);
        }

        {
            const auto& input = monetary_tmp.curr_symbol_int();
            m_curr_symbol_int = detail::to_u8string(input);
        }

        m_pos_format_nat = monetary_tmp.pos_format_nat();
        m_pos_format_int = monetary_tmp.pos_format_int();
        m_neg_format_nat = monetary_tmp.neg_format_nat();
        m_neg_format_int = monetary_tmp.neg_format_int();
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
     * @return A `pattern` describing the ordering of symbol, sign string, and value.
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
     * @return A `pattern` describing the ordering of symbol, sign string, and value.
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
     * @return A `pattern` describing the ordering of symbol, sign string, and value.
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
     * @return A `pattern` describing the ordering of symbol, sign string, and value.
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
     * @brief 返回货币小数点字符（UTF-8 码元）。
     * @return 小数点字符。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the monetary decimal point character (UTF-8 code unit).
     * @return The decimal point character.
     * @endif
     */
    [[nodiscard]] virtual char8_t decimal_point() const { return m_decimal_point; }

    /**
     * @lang{ZH}
     * @brief 返回千位分隔符字符（UTF-8 码元）。
     * @return 千位分隔符字符。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the thousands separator character (UTF-8 code unit).
     * @return The thousands separator character.
     * @endif
     */
    [[nodiscard]] virtual char8_t thousands_sep() const { return m_thousands_sep; }

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
    char8_t                     m_decimal_point;
    char8_t                     m_thousands_sep;
};
}
