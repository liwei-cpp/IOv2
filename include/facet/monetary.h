// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

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
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
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
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
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
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
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
     * @return The four-slot `pattern` giving the order the currency symbol, the sign
     *         string, the amount and the separating space are laid out in.
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
        constexpr std::size_t buf_size = std::numeric_limits<TVal>::digits10 + 3;
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
     * @brief 判断一段填充字符是否会改变人从该字段读到的金额。
     *
     * @par 为什么需要这个判断
     * 格式化的产物是给人看的。`fill` 只应起补齐字段宽度的作用，不应改变这段文本读起来
     * 是多少钱。`setfill('1')` 把 123.45 补成 `"USD 1123.45"`，读者读到的是 1123.45，
     * 解析器读到的却是 23.45（那个 `'1'` 被当成填充吃掉了）——两边读数不一致，而且写出
     * 的文本本身已经错了。因此插入与提取两侧用同一判据把关：人读到的金额是否还是原来
     * 那个金额；命中即由调用方抛出 `stream_error`。解析逻辑本身不变。
     *
     * @par 判据
     * 危险与否既取决于是哪个字符，也取决于它落在哪里：
     * - **digit**：仅当它是 `'0'` 且紧贴金额之前时安全（前导零按约定不改变读数）。
     *   `'1'`–`'9'` 在任何位置都危险；`'0'` 落在金额之后（`"$123.450000000"`）或落在
     *   符号 / 货币符号之前（`"00000-42"` 一类）同样危险。
     * - **小数点**：向右与数字结合，故仅当位于金额之后时安全；落在前面会被读进金额里。
     * - **正/负号首字符**：落在金额之前时会被读成该金额的符号，故仅当它与金额本身的
     *   符号一致时才安全。C locale 下 `setfill('-')` 把 +123.45 补成 `"-------12345"`，
     *   读者与解析器一致地读成负数——这是要拒的；而同一个 `'-'` 用来补齐一个本来就是
     *   负数的金额则无害。
     * - **其余字符**一律安全。千位分隔符必须左右都有数字才成立，而填充段靠数字的那一
     *   侧之外只可能是符号、货币符号或字段边界，故它永远无法与金额结合——把它算作危险
     *   会让以空格为千位分隔符的 locale 连默认填充都用不了。货币符号同理：它不是金额
     *   的一部分，多写几个不改变读数。
     * @endif
     *
     * @lang{EN}
     * @brief Decides whether a run of fill characters changes the amount a human reads
     * out of the field.
     *
     * @par Why this test exists
     * Formatted output is meant to be read by people. `fill` is only supposed to pad a
     * field to its width, never to change what amount the text says. `setfill('1')` pads
     * 123.45 to `"USD 1123.45"`, which a reader reads as 1123.45 while the parser reads
     * 23.45 (that `'1'` is eaten as fill) — the two disagree, and the text that was
     * written is already wrong. Insertion and extraction therefore apply the same test:
     * does a human still read the same amount; the caller throws `stream_error` on a
     * hit. The parsing logic itself is unchanged.
     *
     * @par The criterion
     * Danger depends both on which character it is and on where it lands:
     * - **A digit**: safe only when it is `'0'` sitting immediately before the amount
     *   (leading zeros conventionally do not change the reading). `'1'`–`'9'` are
     *   dangerous everywhere; `'0'` is equally dangerous after the amount
     *   (`"$123.450000000"`) or before a sign or currency symbol.
     * - **The decimal point**: it binds rightwards to digits, so it is safe only after
     *   the amount; in front it is read into it.
     * - **The first character of the positive/negative sign**: in front of the amount it
     *   is read as that amount's sign, so it is safe only when it agrees with the sign
     *   the amount actually has. In the C locale `setfill('-')` pads +123.45 to
     *   `"-------12345"`, which reader and parser alike take for a negative amount —
     *   that one is rejected; the same `'-'` padding an amount that is already negative
     *   is harmless.
     * - **Everything else** is safe. The thousands separator needs digits on both sides,
     *   and on the far side of a fill run there can only be a sign, a currency symbol or
     *   the field edge, so it can never bind to the amount — treating it as dangerous
     *   would rule out even the default fill in locales whose separator is a space. The
     *   same goes for the currency symbol: it is not part of the amount, so repeating it
     *   does not change the reading.
     * @endif
     *
     * @param fill
     * @lang{ZH} 待判断的填充字符。 @endif
     * @lang{EN} The fill character to test. @endif
     *
     * @param info
     * @lang{ZH} 本次使用的格式数据（国际或本地），提供正/负号字符串。 @endif
     * @lang{EN} The format data in use (international or national), supplying the
     * positive/negative sign strings. @endif
     *
     * @param leads_digits
     * @lang{ZH} 该段填充是否紧贴金额之前，中间不隔符号或货币符号。 @endif
     * @lang{EN} Whether the run sits immediately before the amount, with no sign or
     * currency symbol in between. @endif
     *
     * @param trails_value
     * @lang{ZH} 该段填充是否位于金额之后。与 `leads_digits` 互斥；两者皆为 `false`
     * 表示填充与金额之间还隔着别的字符。 @endif
     * @lang{EN} Whether the run sits after the amount. Mutually exclusive with
     * `leads_digits`; both `false` means other characters stand between the run and the
     * amount. @endif
     *
     * @param negative
     * @lang{ZH} 该金额本身是否为负。仅用于判断符号填充是否与它一致。 @endif
     * @lang{EN} Whether the amount is itself negative. Used only to tell whether a
     * sign-shaped fill agrees with it. @endif
     *
     * @return
     * @lang{ZH} 若这段填充会改变人读到的金额则返回 `true`。 @endif
     * @lang{EN} `true` if the run changes the amount a human reads. @endif
     */
    [[nodiscard]] bool fill_alters_reading(char_type fill, const split_info& info,
                                            bool leads_digits, bool trails_value,
                                            bool negative) const
    {
        const char_type* const digits = s_atoms.data() + s_zero;
        if (std::find(digits, digits + 10, fill) != digits + 10)
            return !(fill == digits[0] && leads_digits);

        // Past the amount nothing but a digit can still be read into it.
        if (trails_value)
            return false;

        if (fill == m_decimal_point)
            return true;
        if (!info.m_negative_sign.empty() && fill == info.m_negative_sign[0])
            return !negative;
        if (!info.m_positive_sign.empty() && fill == info.m_positive_sign[0])
            return negative;
        return false;
    }

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
        const std::size_t width = io.width();
        io.width(0);

        // A leading minus is how the caller says the amount is negative. It selects
        // this locale's negative pattern and negative sign string and is then dropped:
        // the sign the field ends up carrying comes from that string, which may spell
        // it quite differently (or not at all).
        const char_type*       first = digits.data();
        const char_type* const stop  = first + digits.size();

        const bool negative = (*first == s_atoms[s_minus]);
        // The pattern is four bytes and is read once per slot while the strings below
        // are being built; taken by value it stays in a register, where a reference
        // into the facet would have to be reloaded around every write.
        const base_ft<monetary>::pattern    order = negative ? info.m_neg_format : info.m_pos_format;
        const std::basic_string<char_type>& sign  = negative ? info.m_negative_sign : info.m_positive_sign;
        if (negative)
            ++first;

        // The amount runs up to the first character that is not one of this facet's
        // digits; whatever the caller put after that is not ours to format.
        const char_type* const digit_table = s_atoms.data() + s_zero;
        std::size_t            len         = 0;
        for (const char_type* q = first; q != stop; ++q)
        {
            if (std::find(digit_table, digit_table + 10, *q) == digit_table + 10)
                break;
            ++len;
        }
        if (!len)
            return s;

        // The digits are the smallest units of the currency, so the amount they spell
        // is read off their right-hand end: the run splits into an integer head and a
        // fractional tail with the cut m_frac_digits places from that end. A run too
        // short to reach the cut has a head of nothing, and the fraction picks up the
        // shortfall as leading zeros — two fractional places turn 7 into .07, not 7.0.
        // A negative frac_digits asks for no fraction at all and keeps the whole run.
        const int         frac = info.m_frac_digits;
        // Widened once, and only from a value the `frac < 0` arm has already ruled
        // out as negative, so every length comparison below stays unsigned-to-unsigned.
        const std::size_t fracw = (frac > 0) ? static_cast<std::size_t>(frac) : 0;
        const std::size_t head  = (frac < 0)      ? len
                                : (len > fracw)   ? len - fracw
                                                  : 0;

        std::basic_string<char_type> value;
        value.reserve(2 * len);

        if (head)
        {
            if (m_grouping.empty())
                value.assign(first, head);
            else
            {
                // add_grouping writes at most one separator per digit, so twice the
                // head is always room enough; the slack is trimmed off afterwards.
                value.assign(2 * head, char_type());
                const char_type* const wrote =
                    FacetHelper::add_grouping(value.data(), m_thousands_sep, m_grouping, first, first + head);
                value.erase(static_cast<std::size_t>(wrote - value.data()));
            }
        }

        if (frac > 0)
        {
            value += m_decimal_point;
            if (fracw > len)
                value.append(fracw - len, s_atoms[s_zero]);
            value.append(first + head, len - head);
        }

        const ios_defs::fmtflags adjust      = io.flags() & ios_defs::adjustfield;
        const bool               show_symbol = (io.flags() & ios_defs::showbase) != 0;

        // What the field is worth before any padding: the amount, the sign string in
        // full (even the part that trails), and the currency symbol if it is shown.
        len = value.size() + sign.size() + (show_symbol ? info.m_curr_symbol.size() : 0);

        if (width > len && width - len > ios_defs::max_pad_count)
            throw stream_error("monetary put fail: fill count exceeds max_pad_count");

        std::basic_string<char_type> res;
        res.reserve(2 * len);

        // Non-zero only under `internal`, where the shortfall is not tacked onto an
        // end but poured into whichever pattern slot is spare.
        const std::size_t spread = (adjust == ios_defs::internal && len < width) ? width - len : 0;

        // Where each run of fill lands is recorded as it is written, so that the
        // readability check below can be made against the finished field instead of
        // against the pattern: whether a run abuts the amount depends on which
        // parts between them turn out to be empty, which the pattern alone does not
        // say. A pattern writes at most one run per part, plus one final pad.
        struct fill_run { std::size_t pos; std::size_t len; };
        std::array<fill_run, 5> runs{};
        std::size_t run_count = 0;
        std::size_t value_pos = std::basic_string<char_type>::npos;

        // Lay the parts down in the order this locale asks for.
        for (const part which : order)
        {
            switch (which)
            {
            case part::symbol:
                if (show_symbol)
                    res += info.m_curr_symbol;
                break;
            case part::sign:
                // Only the sign's first character belongs to this slot; a longer sign
                // string has its remainder appended once the pattern is fully down.
                if (!sign.empty())
                    res += sign[0];
                break;
            case part::value:
                value_pos = res.size();
                res += value;
                break;
            case part::space:
                // This slot owes at least one fill character, and takes the whole
                // internal spread when there is one to take.
                runs[run_count++] = {res.size(), spread ? spread : 1};
                res.append(spread ? spread : 1, io.fill());
                break;
            case part::none:
                // A slot that writes nothing of its own is still somewhere the
                // internal spread can go.
                if (spread)
                {
                    runs[run_count++] = {res.size(), spread};
                    res.append(spread, io.fill());
                }
                break;
            }
        }

        // A sign spelled with more than one character wraps the field: its first
        // character sat in the sign slot, the rest trails everything.
        if (sign.size() > 1)
            res.append(sign, 1);

        // Whatever width is still unaccounted for is fill.
        len = res.size();
        if (width > len)
        {
            const std::size_t pad = width - len;
            if (adjust == ios_defs::left)
            {
                runs[run_count++] = {len, pad};
                res.append(pad, io.fill());
            }
            else
            {
                // Padding in front shifts everything already recorded — the amount
                // included — right by that much.
                res.insert(0, pad, io.fill());
                for (std::size_t r = 0; r < run_count; ++r)
                    runs[r].pos += pad;
                if (value_pos != std::basic_string<char_type>::npos)
                    value_pos += pad;
                runs[run_count++] = {0, pad};
            }
            len = width;
        }

        // Fill has now been written, so this is where it gets vetted: reject a fill
        // character that would change the amount this field reads as. The check sits
        // here rather than at the top of the function because `fill` is sticky stream
        // state — a stream carrying setfill('1') must keep working for every output
        // whose width leaves nothing to pad, and those take no run at all.
        for (std::size_t r = 0; r < run_count; ++r)
        {
            const bool leads_digits = (value_pos != std::basic_string<char_type>::npos)
                                   && (runs[r].pos + runs[r].len == value_pos);
            const bool trails_value = (value_pos != std::basic_string<char_type>::npos)
                                   && (runs[r].pos >= value_pos + value.size());
            if (fill_alters_reading(io.fill(), info, leads_digits, trails_value, negative))
                throw stream_error("monetary put fail: fill would change the value the field reads as");
        }

        return std::copy(res.data(), res.data() + len, s);
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

        // The negative pattern is the one walked, whichever sign the input turns out
        // to carry: it is the wider of the two, since a locale may spell the positive
        // sign as nothing at all, and the polarity is settled by the sign slot itself.
        const base_ft<monetary>::pattern& order = info.m_neg_format;

        const char_type* const digit_table = s_atoms.data() + s_zero;
        const bool             show_symbol = (io.flags() & ios_defs::showbase) != 0;

        // A locale that spells both signs cannot have the field leave one out.
        const bool sign_required = !info.m_positive_sign.empty() && !info.m_negative_sign.empty();

        bool negative  = false;   // polarity the sign slot deduced
        bool valid     = true;    // still looking at a well-formed field
        bool saw_point = false;   // the decimal point has gone by
        int  sign_len  = 0;       // length of the sign string the input turned out to carry

        // Digits seen since the last structural character. Before the decimal point
        // that is the current group; when the point arrives the count is handed to
        // `int_run` and restarts, so afterwards it counts fractional places.
        int run     = 0;
        int int_run = 0;

        // Group widths in the order met, checked against m_grouping once the whole
        // amount is in.
        std::vector<uint8_t> grouping_tmp;
        if (!m_grouping.empty())
            grouping_tmp.reserve(32);

        // Digits accumulate here as ASCII, and are handed to the caller only if the
        // field turns out to be well-formed all the way to its end.
        std::string out;
        out.reserve(32);

        // Which part carries the amount: a run of fill consumed before that index sits
        // in front of the amount, one consumed after it sits behind.
        int value_idx = 3;
        for (int i = 0; i < 4; ++i)
            if (order[i] == part::value)
            {
                value_idx = i;
                break;
            }

        for (int i = 0; i < 4 && valid; ++i)
        {
            // Set when this part consumes at least one fill character, which is the
            // only case in which fill decides how much of the input counts as the
            // amount — and so the only case worth vetting.
            bool ate_fill = false;
            switch (order[i])
            {
            case part::symbol:
            {
                // [locale.money.get.virtuals]: showbase makes the currency symbol
                // mandatory. Otherwise it is optional, and is read only where letting
                // it go would strand a part that still has to be reached — which
                // depends on what this slot is followed by, hence the case analysis.
                bool required = show_symbol || sign_len > 1;
                if (!required)
                    switch (i)
                    {
                    case 0:
                        // Nothing has been read yet; parsing has to start somewhere.
                        required = true;
                        break;
                    case 1:
                        required = sign_required || order[0] == part::sign
                                                 || order[2] == part::space;
                        break;
                    case 2:
                        required = order[3] == part::value
                                || (sign_required && order[3] == part::sign);
                        break;
                    default:
                        break;
                    }
                if (!required)
                    break;

                const std::basic_string<char_type>& symbol = info.m_curr_symbol;
                std::size_t                         taken  = 0;
                while (taken != symbol.size() && beg != end && *beg == symbol[taken])
                {
                    ++beg;
                    ++taken;
                }
                // Stopping part-way through the symbol always makes the field
                // malformed; stopping before its first character only does so when
                // showbase promised the symbol would be there.
                if (taken != symbol.size() && (taken != 0 || show_symbol))
                    valid = false;
                break;
            }
            case part::sign:
            {
                // Whichever sign string the input opens with is the one it wears.
                const std::basic_string<char_type>& pos  = info.m_positive_sign;
                const std::basic_string<char_type>& neg  = info.m_negative_sign;
                const std::basic_string<char_type>* worn = nullptr;
                if (beg != end)
                {
                    if (!pos.empty() && *beg == pos[0])
                        worn = &pos;
                    else if (!neg.empty() && *beg == neg[0])
                        worn = &neg;
                }

                if (worn)
                {
                    negative = (worn == &neg);
                    sign_len = static_cast<int>(worn->size());
                    ++beg;
                }
                else if (pos.empty() != neg.empty())
                    // [locale.money.get.virtuals]: where one of the two sign strings
                    // is empty, a field with no sign in it takes the polarity of that
                    // empty string. Only an empty negative string makes that negative.
                    negative = neg.empty();
                else if (sign_required)
                    valid = false;
                break;
            }
            case part::value:
                // Digits go straight into the result; the separators between them are
                // not kept, only the widths of the groups they mark off.
                for (; beg != end; ++beg)
                {
                    const char_type  c = *beg;
                    const char_type* d = std::find(digit_table, digit_table + 10, c);
                    if (d != digit_table + 10)
                    {
                        out += static_cast<char>('0' + (d - digit_table));
                        ++run;
                        continue;
                    }

                    // Punctuation structures the integer part only. Past the decimal
                    // point the fraction is a plain run of digits, so a separator or a
                    // second point there ends the amount instead of shaping it.
                    if (saw_point)
                        break;

                    if (c == m_decimal_point)
                    {
                        // A locale with no fractional places has nothing for a decimal
                        // point to introduce; the character is not ours to consume.
                        if (info.m_frac_digits <= 0)
                            break;

                        int_run   = run;
                        run       = 0;
                        saw_point = true;
                    }
                    else if (!m_grouping.empty() && c == m_thousands_sep)
                    {
                        // A separator with no preceding digits, or a group
                        // longer than the largest representable group size,
                        // can never satisfy any grouping spec: reject outright
                        // rather than truncating the count.
                        if (run == 0 || std::cmp_greater(run, std::numeric_limits<uint8_t>::max()))
                        {
                            valid = false;
                            break;
                        }
                        grouping_tmp.push_back(static_cast<uint8_t>(run));
                        run = 0;
                    }
                    else
                        break;
                }
                // An amount with no digits at all is not an amount.
                if (out.empty())
                    valid = false;
                break;
            case part::space:
                // This slot owes at least one fill character.
                if (beg != end && (*beg == io.fill()))
                {
                    ++beg;
                    ate_fill = true;
                }
                else
                    valid = false;
                [[fallthrough]];
            case part::none:
                // Either slot then soaks up whatever further fill follows — except in
                // the last one, where trailing fill belongs to the stream, not to us.
                if (i != 3)
                    for (; beg != end && (*beg == io.fill()); ++beg)
                        ate_fill = true;
                // Reject fill that a reader would have taken for part of the amount:
                // `is >> setfill('1') >> get_money(v)` on "112" must fail loudly rather
                // than silently hand back 2. Whether the run leads the digits is read
                // off the input itself — the character it stopped on — because the
                // pattern does not say which parts in between came out empty.
                if (ate_fill
                    && fill_alters_reading(io.fill(), info,
                                            i < value_idx && beg != end
                                                && (*beg == m_decimal_point
                                                    || std::find(digit_table, digit_table + 10, *beg)
                                                           != digit_table + 10),
                                            i > value_idx, negative))
                    throw stream_error("monetary get fail: fill would change the value the field reads as");
                break;
            }
        }

        // A sign spelled with more than one character wraps the field: its first
        // character came out of the sign slot, and the rest has to trail everything.
        if (sign_len > 1 && valid)
        {
            const std::basic_string<char_type>& sign_str = negative ? info.m_negative_sign
                                                                    : info.m_positive_sign;
            std::size_t                         taken    = 1;
            while (taken != sign_str.size() && beg != end && *beg == sign_str[taken])
            {
                ++beg;
                ++taken;
            }
            if (taken != sign_str.size())
                valid = false;
        }

        bool succ = true;
        if (valid)
        {
            // Leading zeros carry no value, but the amount must not be erased along
            // with them: a field of nothing but zeros still comes back as one digit.
            if (out.size() > 1)
            {
                const std::size_t keep = out.find_first_not_of('0');
                if (keep == std::string::npos)
                    out.erase(0, out.size() - 1);
                else if (keep != 0)
                    out.erase(0, keep);
            }

            // [locale.money.get.virtuals]: the deduced sign goes on the result — but
            // zero has no sign, so a field that read as zero stays unmarked.
            if (negative && !out.empty() && out[0] != '0')
                out.insert(out.begin(), '-');

            // Whether the separators fell where m_grouping says they should.
            if (!grouping_tmp.empty())
            {
                // The leading group is the one no separator introduced, so it is
                // counted here rather than in the loop. A group longer than the
                // largest representable group size cannot satisfy any spec: fail
                // rather than truncating.
                const int last_group = saw_point ? int_run : run;
                if (std::cmp_greater(last_group, std::numeric_limits<uint8_t>::max()))
                    succ = false;
                else
                {
                    grouping_tmp.push_back(static_cast<uint8_t>(last_group));
                    succ = FacetHelper::verify_grouping(m_grouping, grouping_tmp);
                }
            }

            // A decimal point commits the field to exactly as many fractional places
            // as the locale declares — no more, and no fewer.
            if (saw_point && run != info.m_frac_digits)
                valid = false;
        }

        // The result is handed over only for a field that held up all the way through.
        if (!valid)
            succ = false;
        else
            units.swap(out);

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

        std::size_t i = 0;
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
    static constexpr std::size_t s_minus = 0; // index into s_atoms for the '-' character
    static constexpr std::size_t s_zero  = 1; // index into s_atoms for the '0' character

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
