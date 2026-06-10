/**
 * @file monetary.h
 * @lang{ZH}
 * 定义了 `monetary<CharT>` 类，这是货币格式化与解析的用户端 facet。
 * 它在构造时从 `monetary_conf<CharT>` 快照所有区域设置数据，并提供
 * `put` 和 `get` 重载，分别用于输出和解析货币字符序列，
 * 同时支持本地（national）和国际（international）两种格式模式。
 * @endif
 *
 * @lang{EN}
 * Defines the `monetary<CharT>` class, the user-facing facet for monetary
 * formatting and parsing. It snapshots all locale data from
 * `monetary_conf<CharT>` at construction and provides `put` and `get`
 * overloads for outputting and parsing monetary character sequences,
 * supporting both national and international format modes.
 * @endif
 */
#pragma once
#include <common/defs.h>
#include <common/metafunctions.h>
#include <facet/facet_common.h>
#include <facet/facet_helper.h>
#include <facet/monetary_details.h>
#include <io/io_base.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 货币格式化与解析的用户端 facet。
 *
 * `monetary<CharT>` 在构造时从 `monetary_conf<CharT>` 复制所有区域设置字段
 * （分组规则、货币符号、符号字符串、格式 pattern、小数点和千位分隔符），
 * 并以快照方式存储于内部，之后不再依赖 `monetary_conf` 对象。
 *
 * - **`put`**：将整数值或预格式化数字字符串格式化为货币字符序列，
 *   写入输出迭代器。`intl=true` 使用国际格式（如 `"USD 1,234.56"`），
 *   `intl=false` 使用本地格式（如 `"$1,234.56"`）。
 * - **`get`**：从字符序列中解析货币字符串，结果存入整数值或数字字符串。
 *   解析失败时抛出 `stream_error`。
 *
 * @tparam CharT 字符类型，由所用的 `monetary_conf` 特化决定。
 * @endif
 *
 * @lang{EN}
 * @brief User-facing facet for monetary formatting and parsing.
 *
 * `monetary<CharT>` copies all locale fields from `monetary_conf<CharT>`
 * (grouping, currency symbols, sign strings, format patterns, decimal point,
 * and thousands separator) at construction and stores them as a snapshot,
 * no longer depending on the `monetary_conf` object afterward.
 *
 * - **`put`**: Formats an integral value or a pre-formatted digit string as
 *   a monetary character sequence written to an output iterator.
 *   `intl=true` uses the international format (e.g. `"USD 1,234.56"`);
 *   `intl=false` uses the national format (e.g. `"$1,234.56"`).
 * - **`get`**: Parses a monetary string from a character sequence into an
 *   integral value or a digit string. Throws `stream_error` on parse failure.
 *
 * @tparam CharT The character type, determined by the `monetary_conf` specialization used.
 * @endif
 */
template <typename CharT>
class monetary
{
    /// @cond
    struct split_info
    {
        std::basic_string<CharT>        m_curr_symbol;
        std::basic_string<CharT>        m_positive_sign;
        std::basic_string<CharT>        m_negative_sign;
        base_ft<monetary>::pattern      m_pos_format;
        base_ft<monetary>::pattern      m_neg_format;
        int                             m_frac_digits;
    };
    /// @endcond

public:
    /// @cond
    using create_rules = facet_create_rule<monetary_conf<CharT>>;
    /// @endcond

    using char_type = CharT; ///< @lang{ZH} 此 facet 使用的字符类型。 @endif @lang{EN} The character type used by this facet. @endif

    /**
     * @lang{ZH}
     * @brief 构造函数，从指向 `monetary_conf<CharT>` 的共享指针创建 facet。
     *
     * 将 `monetary_conf` 中的所有字段一次性复制到内部存储，之后不再访问
     * 该配置对象。`grouping()` 约定返回**内部约定**格式的分组规则
     * （1–255 为组大小，0 表示停止，最后一个元素隐式重复）；
     * POSIX 风格的规范化已在 `monetary_conf` 的 POSIX 边界处完成，此处不再进行。
     *
     * @tparam TConfPtr 满足 `shared_ptr_to<monetary_conf<CharT>>` 约束的指针类型。
     * @param p_obj 指向已初始化的 `monetary_conf<CharT>` 的非空共享指针。
     * @throw stream_error 如果 `p_obj` 为空。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that creates the facet from a shared pointer to `monetary_conf<CharT>`.
     *
     * Copies all fields from the `monetary_conf` into internal storage in one
     * shot and does not access the config object afterward. `grouping()` is
     * contracted to return grouping data in the **internal convention**
     * (1–255 = group size, 0 = stop, last element implicitly repeats);
     * POSIX-style normalisation is performed at the POSIX boundary in
     * `monetary_conf`, not here.
     *
     * @tparam TConfPtr A pointer type satisfying `shared_ptr_to<monetary_conf<CharT>>`.
     * @param p_obj A non-null shared pointer to an initialized `monetary_conf<CharT>`.
     * @throw stream_error If `p_obj` is empty.
     * @endif
     */
    template <shared_ptr_to<monetary_conf<CharT>> TConfPtr>
    monetary(TConfPtr p_obj)
    {
        // Validate before any dereference below: a null pointer would be UB.
        // Mirrors the guard in the messages facet.
        if (!p_obj)
            throw stream_error("shared_ptr is empty");

        m_grouping = p_obj->grouping();
        m_nat = {.m_curr_symbol = p_obj->curr_symbol_nat(),
                 .m_positive_sign = p_obj->positive_sign_nat(),
                 .m_negative_sign = p_obj->negative_sign_nat(),
                 .m_pos_format = p_obj->pos_format_nat(),
                 .m_neg_format = p_obj->neg_format_nat(),
                 .m_frac_digits = p_obj->frac_digits_nat()};
        m_int = {.m_curr_symbol = p_obj->curr_symbol_int(),
                 .m_positive_sign = p_obj->positive_sign_int(),
                 .m_negative_sign = p_obj->negative_sign_int(),
                 .m_pos_format = p_obj->pos_format_int(),
                 .m_neg_format = p_obj->neg_format_int(),
                 .m_frac_digits = p_obj->frac_digits_int()};
        m_decimal_point = p_obj->decimal_point();
        m_thousands_sep = p_obj->thousands_sep();

        // grouping() is contracted to return the INTERNAL convention
        // (1–255 = group size, 0 = stop, last element implicitly repeats).
        // monetary_conf and any user-derived override are both expected to
        // satisfy this contract — POSIX-style normalisation is done at the
        // POSIX boundary in monetary_conf, not here.
    }

    /**
     * @lang{ZH}
     * @brief 返回数字分组规则（内部约定）。
     * @return 描述每组位数的字节向量（1–255 为组大小，0 表示停止，最后一个元素隐式重复）。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the digit-grouping specification (internal convention).
     * @return A byte vector where 1–255 is a group size, 0 means stop, and the
     *         last element repeats implicitly.
     * @endif
     */
    [[nodiscard]] const std::vector<uint8_t>& grouping() const { return m_grouping; }

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
    [[nodiscard]] const std::basic_string<CharT>& curr_symbol_int() const { return m_int.m_curr_symbol; }

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
    [[nodiscard]] const std::basic_string<CharT>& curr_symbol_nat() const { return m_nat.m_curr_symbol; }

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
    [[nodiscard]] const std::basic_string<CharT>& positive_sign_int() const { return m_int.m_positive_sign; }

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
    [[nodiscard]] const std::basic_string<CharT>& positive_sign_nat() const { return m_nat.m_positive_sign; }

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
    [[nodiscard]] const std::basic_string<CharT>& negative_sign_int() const { return m_int.m_negative_sign; }

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
    [[nodiscard]] const std::basic_string<CharT>& negative_sign_nat() const { return m_nat.m_negative_sign; }

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
    [[nodiscard]] const base_ft<monetary>::pattern& pos_format_int() const { return m_int.m_pos_format; }

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
    [[nodiscard]] const base_ft<monetary>::pattern& pos_format_nat() const { return m_nat.m_pos_format; }

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
    [[nodiscard]] const base_ft<monetary>::pattern& neg_format_int() const { return m_int.m_neg_format; }

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
    [[nodiscard]] const base_ft<monetary>::pattern& neg_format_nat() const { return m_nat.m_neg_format; }

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
    [[nodiscard]] int frac_digits_int() const { return m_int.m_frac_digits; }

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
    [[nodiscard]] int frac_digits_nat() const { return m_nat.m_frac_digits; }

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
    [[nodiscard]] CharT decimal_point() const { return m_decimal_point; }

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
    [[nodiscard]] CharT thousands_sep() const { return m_thousands_sep; }

    /**
     * @lang{ZH}
     * @brief 将整数货币值格式化为字符序列，写入输出迭代器。
     *
     * 先将整数值转换为以 `char_type` 表示的十进制数字字符串
     * （有符号负数以 `-` 前缀标记），再委托给 `insert` 完成
     * 货币符号、符号字符串、分组和小数点的组装。
     *
     * @tparam TIter 输出迭代器类型。
     * @tparam TVal 整数类型（不含 `bool`）。
     * @param s 输出迭代器。
     * @param intl 若为 `true`，使用国际格式；否则使用本地格式。
     * @param io 提供格式标志（`showbase`、`adjustfield`）和字段宽度的流对象。
     * @param v 要格式化的整数货币值。
     * @return 指向写入结束位置的迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Formats an integral monetary value as a character sequence written
     *        to an output iterator.
     *
     * Converts the integral value to a decimal digit string represented in
     * `char_type` (with a `-` prefix for signed negatives), then delegates to
     * `insert` for assembling the currency symbol, sign string, grouping, and
     * decimal point.
     *
     * @tparam TIter The output iterator type.
     * @tparam TVal The integral type (not `bool`).
     * @param s The output iterator.
     * @param intl If `true`, use the international format; otherwise use national.
     * @param io The stream object providing format flags (`showbase`, `adjustfield`)
     *           and field width.
     * @param v The integral monetary value to format.
     * @return An iterator pointing past the last written character.
     * @endif
     */
    template <typename TIter, std::integral TVal>
        requires (!std::same_as<TVal, bool>)
    TIter put(TIter s, bool intl, ios_base<char_type>& io, TVal v) const
    {
        constexpr size_t buf_size = std::numeric_limits<TVal>::digits10 + 3;
        std::array<char_type, buf_size> vec;

        char_type* p = vec.data() + buf_size;
        *--p = '\0';

        using TU = std::make_unsigned_t<TVal>;
        bool negative = false;
        TU uv;

        if constexpr(std::is_unsigned_v<TVal>)
            uv = v;
        else
        {
            negative = (v < 0);
            uv = negative ? static_cast<TU>(-(v + 1)) + 1
                          : static_cast<TU>(v);
        }

        do { // NOLINT(cppcoreguidelines-avoid-do-while)
            *--p = static_cast<char_type>('0' + (uv % 10));
            uv /= 10;
        } while (uv != 0);

        if (negative)
            *--p = '-';

        return intl ? insert<true>(s, io, p)
                    : insert<false>(s, io, p);
    }

    /**
     * @lang{ZH}
     * @brief 将预格式化的数字字符串格式化为货币字符序列，写入输出迭代器。
     *
     * 数字字符串中的每个字符应为 `s_atoms` 中的元素（`'-'`、`'0'`–`'9'` 的
     * `char_type` 表示）。直接委托给 `insert` 完成货币格式组装。
     *
     * @tparam TIter 输出迭代器类型。
     * @param s 输出迭代器。
     * @param intl 若为 `true`，使用国际格式；否则使用本地格式。
     * @param io 提供格式标志和字段宽度的流对象。
     * @param digits 以 `char_type` 表示的数字字符串。
     * @return 指向写入结束位置的迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a pre-formatted digit string as a monetary character sequence
     *        written to an output iterator.
     *
     * Each character in the digit string should be an element of `s_atoms`
     * (`char_type` representations of `'-'`, `'0'`–`'9'`). Delegates directly
     * to `insert` for monetary format assembly.
     *
     * @tparam TIter The output iterator type.
     * @param s The output iterator.
     * @param intl If `true`, use the international format; otherwise use national.
     * @param io The stream object providing format flags and field width.
     * @param digits The digit string represented in `char_type`.
     * @return An iterator pointing past the last written character.
     * @endif
     */
    template <typename TIter>
    TIter put(TIter s, bool intl, ios_base<char_type>& io, const std::basic_string<char_type>& digits) const
    {
        return intl ? insert<true>(s, io, digits)
                    : insert<false>(s, io, digits);
    }

    /**
     * @lang{ZH}
     * @brief 从字符序列中解析货币字符串并将结果存入整数值。
     *
     * 先通过 `extract` 解析出 ASCII 数字字符串，再通过 `str_to_v` 转换为整数。
     * 任一步骤失败则抛出异常。
     *
     * @tparam TIter 输入迭代器类型。
     * @tparam TSent 哨兵类型。
     * @tparam TVal 整数类型（不含 `bool`）。
     * @param beg 指向待解析字符序列起始位置的迭代器。
     * @param end 序列末尾的哨兵。
     * @param intl 若为 `true`，按国际格式解析；否则按本地格式解析。
     * @param io 提供格式标志（`showbase` 等）的流对象。
     * @param units 解析成功后存储结果的整数引用。
     * @return 指向已消耗字符之后位置的迭代器。
     * @throw stream_error 如果解析失败或数值超出 `TVal` 范围。
     * @endif
     *
     * @lang{EN}
     * @brief Parses a monetary string from a character sequence and stores the
     *        result in an integral value.
     *
     * Calls `extract` to parse an ASCII digit string, then converts it to an
     * integer via `str_to_v`. Throws on failure at either step.
     *
     * @tparam TIter The input iterator type.
     * @tparam TSent The sentinel type.
     * @tparam TVal The integral type (not `bool`).
     * @param beg An iterator to the start of the character sequence to parse.
     * @param end The sentinel marking the end of the sequence.
     * @param intl If `true`, parse as international format; otherwise as national.
     * @param io The stream object providing format flags (e.g. `showbase`).
     * @param units A reference to the integral variable that receives the result.
     * @return An iterator pointing past the last consumed character.
     * @throw stream_error If parsing fails or the value is out of range for `TVal`.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent, std::integral TVal>
        requires (!std::same_as<TVal, bool>)
    TIter get(TIter beg, TSent end, bool intl, ios_base<char_type>& io, TVal& units) const
    {
        std::string str;
        bool succ = true;

        std::tie(succ, beg) = intl ? extract<true>(beg, end, io, str)
                                   : extract<false>(beg, end, io, str);

        TVal tmp{};
        succ &= str_to_v(str, tmp);
        if (!succ)
            throw stream_error("monetary parse fail");
        units = tmp;
        return beg;
    }

    /**
     * @lang{ZH}
     * @brief 从字符序列中解析货币字符串并将结果存入数字字符串。
     *
     * 解析完成后，结果字符串中的每个字符为 `s_atoms` 中对应的 `char_type`
     * 元素（`'-'` 对应索引 0，`'0'`–`'9'` 对应索引 1–10）。
     * 解析失败时抛出异常；解析成功但未提取到数字时不修改 `digits`。
     *
     * @tparam TIter 输入迭代器类型。
     * @tparam TSent 哨兵类型。
     * @param beg 指向待解析字符序列起始位置的迭代器。
     * @param end 序列末尾的哨兵。
     * @param intl 若为 `true`，按国际格式解析；否则按本地格式解析。
     * @param io 提供格式标志的流对象。
     * @param digits 解析成功后存储结果数字字符串的引用；若无数字则保持不变。
     * @return 指向已消耗字符之后位置的迭代器。
     * @throw stream_error 如果解析失败。
     * @endif
     *
     * @lang{EN}
     * @brief Parses a monetary string from a character sequence and stores the
     *        result in a digit string.
     *
     * In the result string each character is a `char_type` element from `s_atoms`
     * (`'-'` at index 0, `'0'`–`'9'` at indices 1–10). Throws on parse failure;
     * leaves `digits` unmodified if parsing succeeds but no digits were extracted.
     *
     * @tparam TIter The input iterator type.
     * @tparam TSent The sentinel type.
     * @param beg An iterator to the start of the character sequence to parse.
     * @param end The sentinel marking the end of the sequence.
     * @param intl If `true`, parse as international format; otherwise as national.
     * @param io The stream object providing format flags.
     * @param digits A reference to the digit string that receives the result;
     *               left unchanged if no digits are extracted.
     * @return An iterator pointing past the last consumed character.
     * @throw stream_error If parsing fails.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
    TIter get(TIter beg, TSent end, bool intl, ios_base<char_type>& io, std::basic_string<char_type>& digits) const
    {
        bool succ = true;

        std::string str;
        std::tie(succ, beg) = intl ? extract<true>(beg, end, io, str)
                                   : extract<false>(beg, end, io, str);
        const auto len = str.size();
        std::basic_string<char_type> tmp;
        if (len)
        {
            tmp.reserve(len);
            for (auto ch : str)
            {
                if (ch == '-')
                    tmp.push_back(s_atoms[0]);
                else if ((ch >= '0') && (ch <= '9'))
                    tmp.push_back(s_atoms[ch - '0' + 1]);
            }
        }
        if (!succ)
            throw stream_error("monetary parse fail");
        if (len)
            digits.swap(tmp);
        return beg;
    }

private:
    /**
     * @lang{ZH}
     * @brief 将数字字符串按货币格式组装为结果字符串并写入输出迭代器。
     *
     * 模板参数 `isIntl` 静态选择国际（`m_int`）或本地（`m_nat`）格式数据。
     * 函数先捕获并清零字段宽度（宽度是一次性的，必须在任何可能抛出的操作之前清零，
     * 以防宽度泄漏到下一次输出），再按以下步骤组装：
     * - 检测首字符是否为负数符号，选择正/负 pattern 及符号字符串。
     * - 扫描有效的数字字符（基于 `s_atoms`），得到数字部分长度。
     * - 对整数部分按分组规则插入千位分隔符。
     * - 按 `m_frac_digits` 添加小数点和小数部分，不足时补零。
     * - 遍历 pattern，将 `symbol`、`sign`、`value`、`space`/`none` 按序拼接，
     *   在 `ios_defs::internal` 模式下于 `space`/`none` 位置插入填充字符。
     * - 追加多字符符号字符串的剩余部分。
     * - 对整体结果应用左对齐或右对齐填充。
     *
     * @tparam isIntl 若为 `true`，使用国际格式数据；否则使用本地格式数据。
     * @tparam TIter 输出迭代器类型。
     * @param s 输出迭代器。
     * @param io 提供格式标志和字段宽度的流对象。
     * @param digits 以 `char_type` 表示的数字字符串（可含前导 `-`）。
     * @return 指向写入结束位置的迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Assembles a digit string into a formatted monetary string and writes
     *        it to an output iterator.
     *
     * The template parameter `isIntl` statically selects international (`m_int`)
     * or national (`m_nat`) format data. The function first captures and clears
     * the field width (width is one-shot and must be cleared before any
     * potentially-throwing operation to prevent it leaking into the next output),
     * then proceeds as follows:
     * - Detects whether the first character is a negative sign to select
     *   the positive/negative pattern and sign string.
     * - Scans valid digit characters (based on `s_atoms`) to determine the
     *   digit-part length.
     * - Inserts thousands separators into the integer part per grouping rules.
     * - Appends the decimal point and fractional part (zero-padded if needed)
     *   according to `m_frac_digits`.
     * - Traverses the pattern, concatenating `symbol`, `sign`, `value`, and
     *   `space`/`none` in order, inserting fill characters at `space`/`none`
     *   positions in `ios_defs::internal` mode.
     * - Appends remaining characters of a multi-character sign string.
     * - Applies left or right alignment padding to the overall result.
     *
     * @tparam isIntl If `true`, use international format data; otherwise national.
     * @tparam TIter The output iterator type.
     * @param s The output iterator.
     * @param io The stream object providing format flags and field width.
     * @param digits The digit string in `char_type` (may have a leading `-`).
     * @return An iterator pointing past the last written character.
     * @endif
     */
    template <bool isIntl, typename TIter>
    TIter insert(TIter s, ios_base<char_type>& io, const std::basic_string<char_type>& digits) const
    {
        const split_info& info = isIntl ? m_int : m_nat;
        using part = base_ft<monetary>::part;

        // Capture and consume the field width up front. width() is one-shot, so
        // it must be cleared exactly once per put; resetting it here — before any
        // allocation or copy that could throw — guarantees no leftover width
        // leaks into the next output operation on whichever path we leave by,
        // including an exception thrown while formatting. The captured value is
        // used for padding below.
        const int width = static_cast<int>(io.width());
        io.width(0);

        // Determine if negative or positive formats are to be used, and
        // discard leading negative_sign if it is present.
        const char_type* beg = digits.data();

        base_ft<monetary>::pattern p;
        const std::basic_string<char_type>* sign_ptr = nullptr;
        if (!(*beg == s_atoms[s_minus]))
        {
            p = info.m_pos_format;
            sign_ptr = &(info.m_positive_sign);
        }
        else
        {
            p = info.m_neg_format;
            sign_ptr = &(info.m_negative_sign);
            if (digits.size())
                ++beg;
        }

        // Look for valid numbers in input digits.
        int len = 0;
        for (auto i = beg; i != digits.data() + digits.size(); ++i)
        {
            char_type ch = *i;
            int j = 1;
            for (; j < 11; ++j)
                if (ch == s_atoms[j]) break;

            if (j == 11) break;
            ++len;
        }

        if (len)
        {
            // Assume valid input, and attempt to format.
            // Break down input numbers into base components, as follows:
            //   final_value = grouped units + (decimal point) + (digits)
            std::basic_string<char_type> value;
            value.reserve(2 * len);

            // Add thousands separators to non-decimal digits, per
            // grouping rules.
            long paddec = len - info.m_frac_digits;
            if (paddec > 0)
            {
                if (info.m_frac_digits < 0)
                    paddec = len;
                if (!m_grouping.empty())
                {
                    value.assign(2 * paddec, char_type());
                    char_type* vend = FacetHelper::add_grouping(&value[0], m_thousands_sep, m_grouping, beg, beg + paddec);
                    value.erase(vend - &value[0]);
                }
                else
                    value.assign(beg, paddec);
            }

            // Deal with decimal point, decimal digits.
            if (info.m_frac_digits > 0)
            {
                value += m_decimal_point;
                if (paddec >= 0)
                    value.append(beg + paddec, info.m_frac_digits);
                else
                {
                    // Have to pad zeros in the decimal position.
                    value.append(-paddec, s_atoms[s_zero]);
                    value.append(beg, len);
                }
            }

            // Calculate length of resulting string.
            const ios_defs::fmtflags f = io.flags() & ios_defs::adjustfield;
            len = value.size() + sign_ptr->size();
            len += ((io.flags() & ios_defs::showbase) ? info.m_curr_symbol.size() : 0);

            std::basic_string<char_type> res;
            res.reserve(2 * len);

            const bool testipad = (f == ios_defs::internal && len < width);
            // Fit formatted digits into the required pattern.
            for (int i = 0; i < 4; ++i)
            {
                const part which = static_cast<part>(p[i]);
                switch (which)
                {
                case part::symbol:
                    if (io.flags() & ios_defs::showbase)
                        res += info.m_curr_symbol;
                    break;
                case part::sign:
                    // Sign might not exist, or be more than one
                    // character long. In that case, add in the rest
                    // below.
                    if (!sign_ptr->empty())
                        res += (*sign_ptr)[0];
                    break;
                case part::value:
                    res += value;
                    break;
                case part::space:
                    // At least one space is required, but if internal
                    // formatting is required, an arbitrary number of
                    // fill spaces will be necessary.
                    if (testipad)
                        res.append(width - len, io.fill());
                    else
                        res += io.fill();
                    break;
                case part::none:
                    if (testipad)
                        res.append(width - len, io.fill());
                    break;
                }
            }

            // Special case of multi-part sign parts.
            if (sign_ptr->size() > 1)
                res.append(sign_ptr->c_str() + 1, sign_ptr->size() - 1);

            // Pad, if still necessary.
            len = res.size();
            if (width > len)
            {
                if (f == ios_defs::left) // After.
                    res.append(width - len, io.fill());
                else // Before.
                    res.insert(0, width - len, io.fill());
                len = width;
            }

            // Write resulting, fully-formatted string to output iterator.
            s = std::copy(res.data(), res.data() + len, s);
        }
        return s;
    }

    /**
     * @lang{ZH}
     * @brief 从字符序列中按货币格式解析数字字符串。
     *
     * 模板参数 `isIntl` 静态选择国际（`m_int`）或本地（`m_nat`）格式数据。
     * 始终按负数 pattern 遍历，逐段处理：
     * - **`symbol`**：当 `showbase` 置位、或其他因素使符号为必须时进行匹配；
     *   否则可选，仅在不影响其他 part 解析的情况下消耗。
     * - **`sign`**：消耗正/负符号的第一个字符，记录符号极性和长度；
     *   若正符号存在而负符号为空，则按 C++ 标准将缺失的符号解读为负号。
     * - **`value`**：提取数字字符（基于 `s_atoms`），处理千位分隔符（计入
     *   分组向量供后续验证）和小数点。首个未知字符终止提取。
     * - **`space`/`none`**：消耗填充字符，`space` 至少需要一个。
     *
     * 全部 pattern 处理完毕后：
     * - 消耗多字符符号字符串的剩余部分。
     * - 去除前导零（保留至少一位）。
     * - 对负值在首位插入 `'-'`。
     * - 验证千位分组是否与 `m_grouping` 一致。
     * - 检查小数部分的位数是否与 `m_frac_digits` 相符。
     *
     * @tparam isIntl 若为 `true`，使用国际格式数据；否则使用本地格式数据。
     * @tparam TIter 输入迭代器类型。
     * @tparam TSent 哨兵类型。
     * @param beg 指向待解析字符序列起始位置的迭代器。
     * @param end 序列末尾的哨兵。
     * @param io 提供格式标志（`showbase` 等）和填充字符的流对象。
     * @param units 解析出的 ASCII 数字字符串（含可选前导 `'-'`）的输出引用。
     * @return 包含成功标志和已消耗字符末尾迭代器的 `std::pair`。
     * @endif
     *
     * @lang{EN}
     * @brief Parses a digit string from a character sequence according to
     *        a monetary format.
     *
     * The template parameter `isIntl` statically selects international (`m_int`)
     * or national (`m_nat`) format data. Always traverses the negative pattern,
     * processing each part:
     * - **`symbol`**: Matched when `showbase` is set or other conditions require
     *   it; otherwise optional, consumed only if it does not prevent parsing
     *   other parts.
     * - **`sign`**: Consumes the first character of the positive/negative sign,
     *   recording polarity and length; if the positive sign exists but the
     *   negative sign is empty, a missing sign is interpreted as negative per
     *   the C++ standard.
     * - **`value`**: Extracts digit characters (based on `s_atoms`), handling
     *   thousands separators (recorded in a grouping vector for later
     *   verification) and the decimal point. The first unknown character stops
     *   extraction.
     * - **`space`/`none`**: Consumes fill characters; `space` requires at least one.
     *
     * After processing all pattern parts:
     * - Consumes remaining characters of a multi-character sign string.
     * - Strips leading zeros (keeping at least one digit).
     * - Prepends `'-'` for negative values.
     * - Verifies that thousands grouping matches `m_grouping`.
     * - Checks that the fractional digit count matches `m_frac_digits`.
     *
     * @tparam isIntl If `true`, use international format data; otherwise national.
     * @tparam TIter The input iterator type.
     * @tparam TSent The sentinel type.
     * @param beg An iterator to the start of the character sequence to parse.
     * @param end The sentinel marking the end of the sequence.
     * @param io The stream object providing format flags (e.g. `showbase`) and fill char.
     * @param units Output reference for the parsed ASCII digit string (with optional leading `'-'`).
     * @return A `std::pair` of a success flag and an iterator past the last consumed character.
     * @endif
     */
    template <bool isIntl, typename TIter, std::sentinel_for<TIter> TSent>
    std::pair<bool, TIter> extract(TIter beg, TSent end, ios_base<char_type>& io, std::string& units) const
    {
        const split_info& info = isIntl ? m_int : m_nat;
        using part = base_ft<monetary>::part;

        // Deduced sign.
        bool negative = false;
        // Sign size.
        int sign_size = 0;
        // True if sign is mandatory.
        const bool mandatory_sign = (!info.m_positive_sign.empty() && !info.m_negative_sign.empty());
        // Vector of grouping info from thousands_sep plucked from units.
        std::vector<uint8_t> grouping_tmp;
        if (!m_grouping.empty())
            grouping_tmp.reserve(32);
        // Last position before the decimal point.
        int last_pos = 0;
        // Separator positions, then, possibly, fractional digits.
        int n = 0;
        // If input iterator is in a valid state.
        bool testvalid = true;
        // Flag marking when a decimal point is found.
        bool testdecfound = false;

        // The tentative returned string is stored here.
        std::string res; res.reserve(32);

        const char_type* lit_zero = s_atoms.data() + s_zero;
        const base_ft<monetary>::pattern p = info.m_neg_format;

        for (int i = 0; i < 4 && testvalid; ++i)
        {
            const part which = static_cast<part>(p[i]);
            switch (which)
            {
            case part::symbol:
                // According to 22.2.6.1.2, p2, symbol is required
                // if (io.flags() & ios_base::showbase), otherwise
                // is optional and consumed only if other characters
                // are needed to complete the format.
                if (io.flags() & ios_defs::showbase || sign_size > 1
                    || i == 0
                    || (i == 1 && (mandatory_sign
                        || (static_cast<part>(p[0])
                        == part::sign)
                        || (static_cast<part>(p[2])
                        == part::space)))
                    || (i == 2 && ((static_cast<part>(p[3])
                        == part::value)
                        || (mandatory_sign
                        && (static_cast<part>(p[3])
                            == part::sign)))))
                {
                    const int len = info.m_curr_symbol.size();
                    int j = 0;
                    for (; beg != end && j < len && *beg == info.m_curr_symbol[j]; ++beg, (void)++j);
                    if (j != len && (j || io.flags() & ios_defs::showbase))
                        testvalid = false;
                }
                break;
            case part::sign:
                // Sign might not exist, or be more than one character long.
                if (!info.m_positive_sign.empty() && beg != end && *beg == info.m_positive_sign[0])
                {
                    sign_size = info.m_positive_sign.size();
                    ++beg;
                }
                else if (!info.m_negative_sign.empty() && beg != end && *beg == info.m_negative_sign[0])
                {
                    negative = true;
                    sign_size = info.m_negative_sign.size();
                    ++beg;
                }
                else if (!info.m_positive_sign.empty() && info.m_negative_sign.empty())
                // "... if no sign is detected, the result is given the sign
                // that corresponds to the source of the empty string"
                    negative = true;
                else if (mandatory_sign)
                    testvalid = false;
                break;
            case part::value:
                // Extract digits, remove and stash away the
                // grouping of found thousands separators.
                for (; beg != end; ++beg)
                {
                    const char_type c = *beg;
                    const char_type* q = std::find(lit_zero, lit_zero + 10, c);
                    if (q != lit_zero + 10)
                    {
                        res += '0' + (q - lit_zero);
                        ++n;
                    }
                    else if (c == m_decimal_point && !testdecfound)
                    {
                        if (info.m_frac_digits <= 0)
                            break;

                        last_pos = n;
                        n = 0;
                        testdecfound = true;
                    }
                    else if (!m_grouping.empty() && c == m_thousands_sep && !testdecfound)
                    {
                        // A separator with no preceding digits, or a group
                        // longer than the largest representable group size,
                        // can never satisfy any grouping spec: reject outright
                        // rather than truncating the count.
                        if (n == 0 || std::cmp_greater(n, std::numeric_limits<uint8_t>::max()))
                        {
                            testvalid = false;
                            break;
                        }
                        // Mark position for later analysis.
                        grouping_tmp.push_back(static_cast<uint8_t>(n));
                        n = 0;
                    }
                    else
                        break;
                }
                if (res.empty())
                    testvalid = false;
                break;
            case part::space:
                // At least one space is required.
                if (beg != end && (*beg == io.fill()))
                    ++beg;
                else
                    testvalid = false;
                [[fallthrough]];
            case part::none:
                // Only if not at the end of the pattern.
                if (i != 3)
                for (; beg != end && (*beg == io.fill()); ++beg);
                break;
            }
        }

        // Need to get the rest of the sign characters, if they exist.
        if (sign_size > 1 && testvalid)
        {
            const auto& sign_str = negative ? info.m_negative_sign
                                            : info.m_positive_sign;
            int i = 1;
            for (; beg != end && i < sign_size && *beg == sign_str[i]; ++beg, (void)++i);
            if (i != sign_size)
                testvalid = false;
        }

        bool succ = true;
        if (testvalid)
        {
            // Strip leading zeros.
            if (res.size() > 1)
            {
                const auto first = res.find_first_not_of('0');
                const bool only_zeros = first == std::string::npos;
                if (first)
                    res.erase(0, only_zeros ? res.size() - 1 : first);
            }

            // 22.2.6.1.2, p4
            if (negative && !res.empty() && res[0] != '0')
                res.insert(res.begin(), '-');

            // Test for grouping fidelity.
            if (!grouping_tmp.empty())
            {
                // Add the ending grouping. A final group longer than the
                // largest representable group size cannot satisfy any spec:
                // fail rather than truncating.
                const int last_group = testdecfound ? last_pos : n;
                if (std::cmp_greater(last_group, std::numeric_limits<uint8_t>::max()))
                    succ = false;
                else
                {
                    grouping_tmp.push_back(static_cast<uint8_t>(last_group));
                    succ = FacetHelper::verify_grouping(m_grouping, grouping_tmp);
                }
            }

            // Iff not enough digits were supplied after the decimal-point.
            if (testdecfound && n != info.m_frac_digits)
                testvalid = false;
        }

        // Iff valid sequence is not recognized.
        if (!testvalid)
            succ = false;
        else
            units.swap(res);

        return std::pair(succ, beg);
    }

    /**
     * @lang{ZH}
     * @brief 将 ASCII 十进制数字字符串转换为整数值，含溢出检测。
     *
     * 支持有符号和无符号整数类型（通过 `if constexpr` 分支）。
     * 对有符号类型，溢出检测基于 `min_value`/`max_value`；
     * 对无符号类型，不接受前导 `'-'`。字符串为空或含非数字字符时返回 `false`。
     *
     * @tparam TVal 整数类型。
     * @param s 以 ASCII 表示的十进制数字字符串（可含前导 `'-'` 或 `'+'`）。
     * @param value 转换结果的输出引用。
     * @return 转换成功返回 `true`，字符串为空/含非法字符/溢出时返回 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Converts an ASCII decimal digit string to an integral value with
     *        overflow detection.
     *
     * Handles both signed and unsigned integral types via `if constexpr` branches.
     * Overflow detection for signed types uses `min_value`/`max_value`; unsigned
     * types do not accept a leading `'-'`. Returns `false` if the string is empty
     * or contains non-digit characters.
     *
     * @tparam TVal The integral type.
     * @param s An ASCII decimal digit string (may have a leading `'-'` or `'+'`).
     * @param value Output reference for the conversion result.
     * @return `true` on success; `false` if the string is empty, contains illegal
     *         characters, or would overflow.
     * @endif
     */
    template <std::integral TVal>
    bool str_to_v(const std::string& s, TVal& value) const
    {
        if (s.empty()) return false;

        size_t i = 0;
        value = 0;
        constexpr TVal max_value = std::numeric_limits<TVal>::max();

        if constexpr (std::is_signed_v<TVal>)
        {
            bool negative = false;
            constexpr TVal min_value = std::numeric_limits<TVal>::min();

            if (s[i] == '+' || s[i] == '-') {
                negative = (s[i] == '-');
                ++i;
            }

            if (i == s.size()) return false;

            for (; i < s.size(); ++i)
            {
                char c = s[i];
                if (c < '0' || c > '9') return false;

                int digit = c - '0';

                if (!negative)
                {
                    if (value > (max_value - digit) / 10)
                        return false;
                }
                else if (value < (min_value + digit) / 10)
                    return false;

                value = value * 10 + (negative ? -digit : digit);
            }
        }
        else
        {
            if (s[i] == '+')
                ++i;
            else if (s[i] == '-')
                return false;

            if (i == s.size()) return false;

            for (; i < s.size(); ++i)
            {
                char c = s[i];
                if (c < '0' || c > '9') return false;

                TVal digit = static_cast<TVal>(c - '0');
                if (value > (max_value - digit) / 10)
                    return false;

                value = value * 10 + digit;
            }
        }
        return true;
    }

private:
    static constexpr size_t s_minus = 0; // index into s_atoms for the '-' character
    static constexpr size_t s_zero  = 1; // index into s_atoms for the '0' character

private:
    std::vector<uint8_t>      m_grouping;
    split_info                m_nat;
    split_info                m_int;
    CharT                     m_decimal_point;
    CharT                     m_thousands_sep;

    // char_type representations of '-' and '0'-'9', indexed as:
    //   s_atoms[0]    = '-'
    //   s_atoms[1..10] = '0'..'9'
    static constexpr std::array<char_type, 11> s_atoms = {
            (char_type)'-', (char_type)'0', (char_type)'1', (char_type)'2',
            (char_type)'3', (char_type)'4', (char_type)'5', (char_type)'6',
            (char_type)'7', (char_type)'8', (char_type)'9'
        };
};

/// @cond
template<typename TConfPtr>
monetary(TConfPtr) -> monetary<typename TConfPtr::element_type::char_type>;
/// @endcond
}
