/**
 * @file numeric.h
 * @lang{ZH}
 * 定义了 `numeric<CharT>` facet 类，提供基于 locale 的数值格式化（输出）与解析（输入）功能。
 * 涵盖整数、浮点数、布尔值和指针类型，支持分组、基数、符号、对齐和精度等格式化控制。
 * @endif
 *
 * @lang{EN}
 * Defines the `numeric<CharT>` facet class, providing locale-aware numeric formatting
 * (output) and parsing (input). Covers integer, floating-point, boolean, and pointer
 * types, with support for grouping, base, sign, alignment, and precision formatting controls.
 * @endif
 */
#pragma once
#include <common/clocale_wrapper.h>
#include <common/defs.h>
#include <common/metafunctions.h>
#include <facet/ctype.h>
#include <facet/facet_common.h>
#include <facet/facet_helper.h>
#include <facet/numeric_details.h>
#include <io/io_base.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 基于 locale 的数值格式化与解析 facet。
 *
 * 封装了 `numeric_conf<CharT>` 提供的 locale 参数（小数点、千位分隔符、分组规则、布尔名称）
 * 和 `ctype<CharT>` 提供的字符扩宽能力，通过 `put()` 系列重载将数值格式化为字符序列，
 * 通过 `get()` 系列重载从字符流中解析数值。
 *
 * @tparam CharT 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief A locale-aware numeric formatting and parsing facet.
 *
 * Encapsulates locale parameters from `numeric_conf<CharT>` (decimal point, thousands
 * separator, grouping rules, boolean names) and character-widening from `ctype<CharT>`.
 * Numeric values are formatted into character sequences via `put()` overloads, and
 * parsed from character streams via `get()` overloads.
 *
 * @tparam CharT The character type.
 * @endif
 */
template <typename CharT>
class numeric
{
public:
    /**
     * @lang{ZH}
     * @brief 关联的配置对象创建规则类型。
     * @endif
     *
     * @lang{EN}
     * @brief The creation-rule type for the associated configuration objects.
     * @endif
     */
    using create_rules = facet_create_rule<facet_create_pack<numeric_conf<CharT>, ctype<CharT>>>;

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

    /**
     * @lang{ZH}
     * @brief 构造函数，初始化所有 locale 相关参数并预先扩宽输入/输出原子字符集。
     *
     * 从 `numeric_conf` 中复制小数点、千位分隔符、分组规则和布尔名称，
     * 然后通过 `ctype::widen_seq` 将两组 ASCII 原子字符集扩宽为目标字符类型，
     * 分别填充 `m_in_atoms`（用于解析）和 `m_out_atoms`（用于格式化）。
     * 在 `wchar_t` 和 `char32_t` 类型下，还会在 debug/sanitizer 构建中断言
     * 扩宽结果的单射性。
     *
     * @tparam TConfPtr 满足 `shared_ptr_to<numeric_conf<CharT>>` 约束的共享指针类型。
     * @tparam TCtypePtr 满足 `shared_ptr_to<ctype<CharT>>` 约束的共享指针类型。
     * @param p_obj 指向 `numeric_conf` 配置对象的共享指针，不得为空。
     * @param p_ctype 指向 `ctype` 配置对象的共享指针，不得为空。
     * @throw stream_error 若任一指针为空。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that initializes all locale-related parameters and pre-widens the input/output atom character sets.
     *
     * Copies the decimal point, thousands separator, grouping rules, and boolean names
     * from `numeric_conf`, then widens two ASCII atom character sets to the target
     * character type via `ctype::widen_seq`, populating `m_in_atoms` (for parsing)
     * and `m_out_atoms` (for formatting). On `wchar_t` and `char32_t`, also asserts
     * the injectivity of the widened results in debug/sanitizer builds.
     *
     * @tparam TConfPtr A shared pointer type satisfying `shared_ptr_to<numeric_conf<CharT>>`.
     * @tparam TCtypePtr A shared pointer type satisfying `shared_ptr_to<ctype<CharT>>`.
     * @param p_obj Shared pointer to the `numeric_conf` configuration object; must not be null.
     * @param p_ctype Shared pointer to the `ctype` configuration object; must not be null.
     * @throw stream_error If either pointer is null.
     * @endif
     */
    template <shared_ptr_to<numeric_conf<CharT>> TConfPtr,
              shared_ptr_to<ctype<CharT>> TCtypePtr>
    numeric(TConfPtr p_obj, TCtypePtr p_ctype) : m_ctype(p_ctype)
    {
        if (!p_obj || !p_ctype) throw stream_error("shared_ptr is empty");
        m_decimal_point = p_obj->decimal_point();
        m_thousands_sep = p_obj->thousands_sep();
        m_true_name = p_obj->truename();
        m_false_name = p_obj->falsename();
        // grouping() is contracted to return the INTERNAL convention
        // (1–255 = group size, 0 = stop, last element implicitly repeats).
        // numeric_conf and any user-derived override are both expected to
        // satisfy this contract — POSIX-style normalisation is done at the
        // POSIX boundary in numeric_conf, not here.
        m_grouping = p_obj->grouping();

        // string_view carries the length (no trailing '\0' counted), so the
        // widened range matches m_in_atoms/m_out_atoms exactly. static_assert
        // keeps the source literal and the destination array in lock-step.
        constexpr std::string_view in_atoms = "-+xX0123456789abcdefABCDEF";
        static_assert(in_atoms.size() == std::tuple_size_v<decltype(m_in_atoms)>);
        m_ctype->widen_seq(in_atoms.data(), in_atoms.data() + in_atoms.size(), m_in_atoms.data());

        constexpr std::string_view out_atoms = "-+xX0123456789abcdef0123456789ABCDEF";
        static_assert(out_atoms.size() == std::tuple_size_v<decltype(m_out_atoms)>);
        m_ctype->widen_seq(out_atoms.data(), out_atoms.data() + out_atoms.size(), m_out_atoms.data());

        if constexpr (std::is_same_v<CharT, wchar_t> ||
                      std::is_same_v<CharT, char32_t>)
        {
            assert(atoms_pairwise_distinct(m_in_atoms.data(), m_in_atoms.size()));
            // m_out_atoms intentionally aliases "0..9" at [4..13] and
            // [20..29] (lowercase / uppercase hex digit slots). Build a
            // 26-position view that skips that overlap before checking
            // distinctness on the semantically distinct output positions.
            std::array<CharT, 26> out_view{};
            std::copy(m_out_atoms.begin(),        m_out_atoms.begin() + 20, out_view.begin());
            std::copy(m_out_atoms.begin() + 30,   m_out_atoms.end(),        out_view.begin() + 20);
            assert(atoms_pairwise_distinct(out_view.data(), out_view.size()));
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
    CharT decimal_point() const noexcept { return m_decimal_point; }

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
    CharT thousands_sep() const noexcept { return m_thousands_sep; }

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
    const std::basic_string<CharT>& truename() const noexcept { return m_true_name; }

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
    const std::basic_string<CharT>& falsename() const noexcept { return m_false_name; }

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
    [[nodiscard]] const std::vector<uint8_t>& grouping() const noexcept { return m_grouping; }

public:
    /**
     * @lang{ZH}
     * @brief 将布尔值格式化并写入输出迭代器。
     *
     * 若 `boolalpha` 标志未设置，则以整数（0/1）形式格式化，委托给 `insert_int`。
     * 若设置了 `boolalpha`，则使用 locale 的 `truename()`/`falsename()` 文本，
     * 并按 `width()` 和对齐方式插入填充字符。
     *
     * @tparam TIter 输出迭代器类型。
     * @param s 输出迭代器，写入格式化结果。
     * @param io 提供格式标志、宽度和填充字符的流对象。
     * @param v 要格式化的布尔值。
     * @return 写入结束后的输出迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a boolean value and writes it to the output iterator.
     *
     * Without `boolalpha`, formats as integer 0 or 1 by delegating to `insert_int`.
     * With `boolalpha`, uses the locale's `truename()`/`falsename()` text with width
     * and alignment padding applied.
     *
     * @tparam TIter The output iterator type.
     * @param s Output iterator to receive the formatted result.
     * @param io Stream object providing format flags, width, and fill character.
     * @param v The boolean value to format.
     * @return The output iterator after writing.
     * @endif
     */
    template <typename TIter>
    TIter put(TIter s, ios_base<char_type>& io, bool v) const
    {
        const auto flags = io.flags();
        if ((flags & ios_defs::boolalpha) == 0)
        {
            return insert_int(s, io, static_cast<long>(v));
        }

        const auto& name = v ? m_true_name : m_false_name;
        size_t len = name.size();

        const auto w = io.width();
        io.width(0);
        if (w > static_cast<decltype(w)>(len))
        {
            const auto plen = w - len;

            if ((flags & ios_defs::adjustfield) == ios_defs::left)
            {
                s = std::copy(name.begin(), name.end(), s);
                s = std::fill_n(s, plen, io.fill());
            }
            else
            {
                s = std::fill_n(s, plen, io.fill());
                s = std::copy(name.begin(), name.end(), s);
            }
            return s;
        }

        return std::copy(name.begin(), name.end(), s);
    }

    /**
     * @lang{ZH}
     * @brief 将整数值格式化并写入输出迭代器，委托给 `insert_int`。
     *
     * @tparam TIter 输出迭代器类型。
     * @tparam TValue 整数类型（非 `bool`）。
     * @param s 输出迭代器，写入格式化结果。
     * @param io 提供格式标志、宽度和填充字符的流对象。
     * @param v 要格式化的整数值。
     * @return 写入结束后的输出迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Formats an integer value and writes it to the output iterator; delegates to `insert_int`.
     *
     * @tparam TIter The output iterator type.
     * @tparam TValue The integer type (not `bool`).
     * @param s Output iterator to receive the formatted result.
     * @param io Stream object providing format flags, width, and fill character.
     * @param v The integer value to format.
     * @return The output iterator after writing.
     * @endif
     */
    template <typename TIter, typename TValue>
        requires (std::is_integral_v<TValue> && (!std::is_same_v<TValue, bool>))
    TIter put(TIter s, ios_base<char_type>& io, TValue v) const { return insert_int(s, io, v); }

    /**
     * @lang{ZH}
     * @brief 将浮点值格式化并写入输出迭代器，委托给 `insert_float`。
     *
     * `long double` 使用 `'L'` 修饰符；其他浮点类型使用空修饰符。
     *
     * @tparam TIter 输出迭代器类型。
     * @tparam TValue 浮点类型。
     * @param s 输出迭代器，写入格式化结果。
     * @param io 提供格式标志、宽度、精度和填充字符的流对象。
     * @param v 要格式化的浮点值。
     * @return 写入结束后的输出迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a floating-point value and writes it to the output iterator; delegates to `insert_float`.
     *
     * Uses the `'L'` modifier for `long double`; uses an empty modifier for other floating-point types.
     *
     * @tparam TIter The output iterator type.
     * @tparam TValue The floating-point type.
     * @param s Output iterator to receive the formatted result.
     * @param io Stream object providing format flags, width, precision, and fill character.
     * @param v The floating-point value to format.
     * @return The output iterator after writing.
     * @endif
     */
    template <typename TIter, typename TValue>
        requires (std::is_floating_point_v<TValue>)
    TIter put(TIter s, ios_base<char_type>& io, TValue v) const
    {
        if constexpr (std::is_same_v<TValue, long double>)
            return insert_float(s, io, v, 'L');
        else
            return insert_float(s, io, v, char());
    }

    /**
     * @lang{ZH}
     * @brief 将指针值以十六进制格式化并写入输出迭代器。
     *
     * 临时将流标志设为 `hex | showbase`（通过 `fmtflags_guard` 保证恢复），
     * 然后将指针值转型为与平台指针大小匹配的无符号整数类型，委托给 `insert_int` 格式化。
     *
     * @tparam TIter 输出迭代器类型。
     * @param s 输出迭代器，写入格式化结果。
     * @param io 提供格式标志、宽度和填充字符的流对象。
     * @param v 要格式化的指针值。
     * @return 写入结束后的输出迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a pointer value in hexadecimal and writes it to the output iterator.
     *
     * Temporarily sets the stream flags to `hex | showbase` (restored by `fmtflags_guard`),
     * then casts the pointer to a platform-sized unsigned integer type and delegates
     * to `insert_int`.
     *
     * @tparam TIter The output iterator type.
     * @param s Output iterator to receive the formatted result.
     * @param io Stream object providing format flags, width, and fill character.
     * @param v The pointer value to format.
     * @return The output iterator after writing.
     * @endif
     */
    template <typename TIter>
    TIter put(TIter s, ios_base<char_type>& io, const void* v) const
    {
        fmtflags_guard guard(io);
        io.flags((io.flags() & ~(ios_defs::basefield | ios_defs::uppercase))
                 | (ios_defs::hex | ios_defs::showbase));

        using uintptr_carrier_t = std::conditional_t<(sizeof(const void*) <= sizeof(unsigned long)),
                                                     unsigned long,
                                                     unsigned long long>;

        return insert_int(s, io, reinterpret_cast<uintptr_carrier_t>(v)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }

    /**
     * @lang{ZH}
     * @brief 从字符流中解析布尔值。
     *
     * 若 `boolalpha` 标志未设置，则将输入解析为 `long`（有效值为 0 或 1），
     * 超出范围则按 LWG 23 将结果设为 `true` 并失败。
     * 若设置了 `boolalpha`，则将输入字符与 `truename()`/`falsename()` 逐字符前缀匹配，
     * 以最长匹配确定结果；若无法确定，则按 LWG 23 将结果设为 `false` 并失败。
     * 解析失败时抛出异常。
     *
     * @tparam TIter 输入迭代器类型。
     * @tparam TSent 哨兵类型，满足 `std::sentinel_for<TIter>`。
     * @param beg 输入范围的起始迭代器。
     * @param end 输入范围的结束哨兵。
     * @param io 提供格式标志的流对象。
     * @param v 解析成功后写入结果的布尔值引用。
     * @return 消费输入后的迭代器。
     * @throw stream_error 若解析失败。
     * @endif
     *
     * @lang{EN}
     * @brief Parses a boolean value from the character stream.
     *
     * Without `boolalpha`, the input is parsed as a `long` (valid values are 0 or 1);
     * out-of-range values set the result to `true` and fail per LWG 23.
     * With `boolalpha`, the input is matched character by character against
     * `truename()`/`falsename()` using the longest-match rule; if no match is
     * determined, the result is set to `false` and fails per LWG 23.
     * Throws on parse failure.
     *
     * @tparam TIter The input iterator type.
     * @tparam TSent The sentinel type satisfying `std::sentinel_for<TIter>`.
     * @param beg Start iterator of the input range.
     * @param end End sentinel of the input range.
     * @param io Stream object providing format flags.
     * @param v Reference to the boolean variable to receive the parsed result.
     * @return The iterator after consuming the parsed input.
     * @throw stream_error If parsing fails.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
    TIter get(TIter beg, TSent end, ios_base<char_type>& io, bool& v) const
    {
        bool success = true;

        if (!(io.flags() & ios_defs::boolalpha))
        {
            // Parse bool values as long.
            long l = -1;
            std::tie(success, beg) = extract_int(beg, end, io, l);

            if (l == 0 || l == 1) v = bool(l);
            else
            {
                // _GLIBCXX_RESOLVE_LIB_DEFECTS
                // 23. Num_get overflow result.
                v = true;
                success = false;
            }
        }
        else
        {
            // Parse bool values as alphanumeric.
            bool testf = true;
            bool testt = true;
            bool donef = (m_false_name.empty());
            bool donet = (m_true_name.empty());
            size_t n = 0;
            while (!donef || !donet)
            {
                if (beg == end)
                    break;

                const CharT c = *beg;

                if (!donef)
                    testf = c == m_false_name[n];

                if (!testf && donet) break;

                if (!donet)
                    testt = c == m_true_name[n];

                if (!testt && donef) break;

                if (!testt && !testf) break;

                ++n;
                ++beg;

                donef = !testf || n >= m_false_name.size();
                donet = !testt || n >= m_true_name.size();
            }
            if (testf && n == m_false_name.size() && n)
            {
                v = false;
                if (testt && n == m_true_name.size())
                    success = false;
            }
            else if (testt && n == m_true_name.size() && n)
            {
                v = true;
            }
            else
            {
                // _GLIBCXX_RESOLVE_LIB_DEFECTS
                // 23. Num_get overflow result.
                v = false;
                success = false;
            }
        }

        if (!success) throw stream_error("numeric::get fail: parse boolean fail");
        return beg;
    }

    /**
     * @lang{ZH}
     * @brief 从字符流中解析整数值，委托给 `extract_int`。
     *
     * 解析失败时抛出异常。
     *
     * @tparam TIter 输入迭代器类型。
     * @tparam TSent 哨兵类型，满足 `std::sentinel_for<TIter>`。
     * @tparam TValue 目标整数类型（非 `bool`）。
     * @param beg 输入范围的起始迭代器。
     * @param end 输入范围的结束哨兵。
     * @param io 提供格式标志的流对象。
     * @param v 解析成功后写入结果的整数引用。
     * @return 消费输入后的迭代器。
     * @throw stream_error 若解析失败。
     * @endif
     *
     * @lang{EN}
     * @brief Parses an integer value from the character stream; delegates to `extract_int`.
     *
     * Throws on parse failure.
     *
     * @tparam TIter The input iterator type.
     * @tparam TSent The sentinel type satisfying `std::sentinel_for<TIter>`.
     * @tparam TValue The target integer type (not `bool`).
     * @param beg Start iterator of the input range.
     * @param end End sentinel of the input range.
     * @param io Stream object providing format flags.
     * @param v Reference to the integer variable to receive the parsed result.
     * @return The iterator after consuming the parsed input.
     * @throw stream_error If parsing fails.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent, typename TValue>
        requires (std::is_integral_v<TValue> && (!std::is_same_v<TValue, bool>))
    TIter get(TIter beg, TSent end, ios_base<char_type>& io, TValue& v) const
    {
        auto [succ, res] = extract_int(beg, end, io, v);
        if (!succ) throw stream_error("numeric::get fail: parse integral fail");
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 从字符流中解析指针地址（十六进制整数）。
     *
     * 临时将流标志设为 `hex`，使用与平台指针大小匹配的无符号整数类型调用 `extract_int`，
     * 再将结果转型为 `void*`。解析失败时抛出异常。
     *
     * @tparam TIter 输入迭代器类型。
     * @tparam TSent 哨兵类型，满足 `std::sentinel_for<TIter>`。
     * @param beg 输入范围的起始迭代器。
     * @param end 输入范围的结束哨兵。
     * @param io 提供格式标志的流对象。
     * @param v 解析成功后写入结果的指针引用。
     * @return 消费输入后的迭代器。
     * @throw stream_error 若解析失败。
     * @endif
     *
     * @lang{EN}
     * @brief Parses a pointer address (hexadecimal integer) from the character stream.
     *
     * Temporarily sets the stream flags to `hex`, calls `extract_int` with a
     * platform-sized unsigned integer type, then casts the result to `void*`.
     * Throws on parse failure.
     *
     * @tparam TIter The input iterator type.
     * @tparam TSent The sentinel type satisfying `std::sentinel_for<TIter>`.
     * @param beg Start iterator of the input range.
     * @param end End sentinel of the input range.
     * @param io Stream object providing format flags.
     * @param v Reference to the pointer variable to receive the parsed result.
     * @return The iterator after consuming the parsed input.
     * @throw stream_error If parsing fails.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
    TIter get(TIter beg, TSent end, ios_base<char_type>& io, void*& v) const
    {
        fmtflags_guard guard(io);
        io.flags((io.flags() & ~ios_defs::basefield) | ios_defs::hex);

        using uintptr_carrier_t = std::conditional_t<(sizeof(const void*) <= sizeof(unsigned long)),
                                                     unsigned long,
                                                     unsigned long long>;
        uintptr_carrier_t ul = 0;
        auto [succ, res] = extract_int(beg, end, io, ul);

        v = reinterpret_cast<void*>(ul); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)

        if (!succ) throw stream_error("numeric::get fail: parse address fail");
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 从字符流中解析浮点值。
     *
     * 先由 `extract_float` 将有效字符提取并规范化为 "C" locale 的 ASCII 字符串，
     * 再由 `convert_to_v` 将其转换为目标浮点类型。解析失败时抛出异常。
     *
     * @tparam TIter 输入迭代器类型。
     * @tparam TSent 哨兵类型，满足 `std::sentinel_for<TIter>`。
     * @tparam TValue 目标浮点类型。
     * @param beg 输入范围的起始迭代器。
     * @param end 输入范围的结束哨兵。
     * @param io 提供格式标志的流对象。
     * @param v 解析成功后写入结果的浮点数引用。
     * @return 消费输入后的迭代器。
     * @throw stream_error 若解析失败。
     * @endif
     *
     * @lang{EN}
     * @brief Parses a floating-point value from the character stream.
     *
     * First, `extract_float` extracts and normalizes the numeric characters into a
     * "C"-locale ASCII string; then `convert_to_v` converts it to the target
     * floating-point type. Throws on parse failure.
     *
     * @tparam TIter The input iterator type.
     * @tparam TSent The sentinel type satisfying `std::sentinel_for<TIter>`.
     * @tparam TValue The target floating-point type.
     * @param beg Start iterator of the input range.
     * @param end End sentinel of the input range.
     * @param io Stream object providing format flags.
     * @param v Reference to the floating-point variable to receive the parsed result.
     * @return The iterator after consuming the parsed input.
     * @throw stream_error If parsing fails.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent, typename TValue>
        requires (std::is_floating_point_v<TValue>)
    TIter get(TIter beg, TSent end, ios_base<char_type>& io, TValue& v) const
    {
        std::string xtrc;
        xtrc.reserve(32);
        auto [succ, res] = extract_float(beg, end, io, xtrc);
        succ &= convert_to_v(xtrc.c_str(), v);

        if (!succ) throw stream_error("numeric::get fail: parse float fail");
        return res;
    }

private:
    /**
     * @lang{ZH}
     * @brief RAII 守卫，在析构时自动恢复 `ios_base` 的格式标志。
     *
     * 用于在临时修改格式标志的代码块中，确保即使发生异常也能还原原始标志。
     * @endif
     *
     * @lang{EN}
     * @brief RAII guard that automatically restores the format flags of an `ios_base` object on destruction.
     *
     * Used in code blocks that temporarily modify format flags, ensuring the original
     * flags are restored even if an exception is thrown.
     * @endif
     */
    struct fmtflags_guard
    {
        fmtflags_guard(ios_base<char_type>& i) : m_io(i), m_saved(i.flags()) {}
        ~fmtflags_guard() { m_io.flags(m_saved); }
        fmtflags_guard(const fmtflags_guard&) = delete;
        fmtflags_guard& operator=(const fmtflags_guard&) = delete;
        fmtflags_guard(fmtflags_guard&&) = delete;
        fmtflags_guard& operator=(fmtflags_guard&&) = delete;
    private:
        ios_base<char_type>& m_io;
        ios_defs::fmtflags m_saved;
    };

    /**
     * @lang{ZH}
     * @brief 将浮点值格式化为字符类型序列并写入输出迭代器。
     *
     * 在 "C" locale 下通过 `snprintf` 生成 ASCII 表示（阶段 1），
     * 再用 `ctype::widen_seq` 扩宽为目标字符类型，并将 ASCII 小数点替换为
     * locale 专有字符（阶段 2），按需插入千位分隔符，最后应用对齐填充（阶段 3/4）。
     *
     * @tparam TIter 输出迭代器类型。
     * @tparam TValue 浮点类型。
     * @param s 输出迭代器，写入格式化结果。
     * @param io 提供格式标志、宽度和精度的流对象。
     * @param v 要格式化的浮点值。
     * @param mod `snprintf` 长度修饰符（`'L'` 表示 `long double`，`'\0'` 表示其他类型）。
     * @return 写入结束后的输出迭代器。
     * @throw stream_error 若 `snprintf` 转换失败或输出为空。
     * @endif
     *
     * @lang{EN}
     * @brief Formats a floating-point value as a character-type sequence and writes it to the output iterator.
     *
     * Generates an ASCII representation via `snprintf` under the "C" locale (stage 1),
     * widens it to the target character type via `ctype::widen_seq`, replaces the ASCII
     * decimal point with the locale-specific character (stage 2), inserts grouping
     * separators if needed, then applies alignment padding (stages 3/4).
     *
     * @tparam TIter The output iterator type.
     * @tparam TValue The floating-point type.
     * @param s Output iterator to receive the formatted result.
     * @param io Stream object providing format flags, width, and precision.
     * @param v The floating-point value to format.
     * @param mod The `snprintf` length modifier (`'L'` for `long double`, `'\0'` otherwise).
     * @return The output iterator after writing.
     * @throw stream_error If `snprintf` conversion fails or produces an empty result.
     * @endif
     */
    template <typename TIter, typename TValue>
    TIter insert_float(TIter s, ios_base<char_type>& io, TValue v, char mod) const
    {
        // precision() is now a bounded uint8_t (0-255); there is no negative /
        // out-of-range sentinel to normalise here.
        const std::streamsize prec = io.precision();
        const int max_digits = std::numeric_limits<TValue>::digits10;

        // Consume the field width up front, before any allocation or conversion
        // that can throw: width() is one-shot and a stale value must not survive
        // onto the stream if we leave by an exception. Used for padding below.
        const std::streamsize w = io.width();
        io.width(0);

        // [22.2.2.2.2] Stage 1, numeric conversion to character.
        size_t len = 0;
        std::array<char, 16> fbuf{};
        format_float_(io.flags(), fbuf.data(), mod);

        const ios_defs::fmtflags fltfield = io.flags() & ios_defs::floatfield;

        // Initial buffer size estimate (GCC style).
        // For non-fixed fields, max_digits * 3 is a safe heuristic.
        size_t cs_size = static_cast<size_t>(max_digits) * 3 + 32;
        if (fltfield == ios_defs::fixed)
            cs_size = static_cast<size_t>(std::numeric_limits<TValue>::max_exponent10) + static_cast<size_t>(prec) + 32;

        // Cap initial allocation to a reasonable size (e.g., 2048) to avoid huge initial pressure.
        if (cs_size > 2048) cs_size = 2048;

        std::vector<char> vec_cs(cs_size);

        {
            clocale_wrapper inter_locale("C");
            clocale_user guard(inter_locale);

            auto do_snprintf = [&](char* buf, size_t size) {
                if (fltfield == (ios_defs::fixed | ios_defs::scientific))
                    return snprintf(buf, size, fbuf.data(), v);
                else
                    return snprintf(buf, size, fbuf.data(), static_cast<int>(prec), v);
            };

            // snprintf returns int. Trap negative (encoding error) before
            // promoting to size_t — otherwise -1 becomes SIZE_MAX and
            // bypasses every downstream bound check.
            int n = do_snprintf(vec_cs.data(), cs_size);
            if (n < 0)
                throw stream_error("numeric::put fail: floating-point conversion failed");
            len = static_cast<size_t>(n);

            // If buffer was too small, snprintf returns required length.
            if (len >= cs_size)
            {
                cs_size = len + 1;
                vec_cs.resize(cs_size);
                n = do_snprintf(vec_cs.data(), cs_size);
                if (n < 0)
                    throw stream_error("numeric::put fail: floating-point conversion failed");
                len = static_cast<size_t>(n);
            }
        }

        // len == 0 is also treated as failure: every well-formed numeric
        // print (including 0, "inf", "nan") emits at least one character,
        // and the downstream path assumes vec_ws(len) has at least one
        // element before dereferencing vec_ws.data().
        if (len == 0 || len >= cs_size)
            throw stream_error("numeric::put fail: floating-point conversion failed");

        const char* cs = vec_cs.data();

        // [22.2.2.2.2] Stage 2, convert to char_type, using correct
        // numeric_conf.decimal_point() values for '.' and adding grouping.
        std::vector<CharT> vec_ws(len);
        CharT* ws = vec_ws.data();
        m_ctype->widen_seq(cs, cs + len, ws);

        // Replace decimal point.
        const char* p = std::find(cs, cs + len, '.');
        CharT* wp = nullptr;
        if (p != cs + len)
        {
            wp = ws + (p - cs);
            *wp = m_decimal_point;
        }

        // Add grouping, if necessary.
        if ((!m_grouping.empty())
            && (wp || len < 3u || (cs[1] <= '9' && cs[2] <= '9' && cs[1] >= '0' && cs[2] >= '0')))
        {
            // Grouping can add (almost) as many separators as the
            // number of digits, but no more.
            std::vector<CharT> vec_ws2(len * 2);
            CharT* ws2 = vec_ws2.data();

            size_t off = 0;
            if (cs[0] == '-' || cs[0] == '+')
            {
                off = 1;
                ws2[0] = ws[0];
                len -= 1;
            }

            group_float(m_grouping, m_thousands_sep, wp,
                        ws2 + off, ws + off, len);
            len += off;
            std::swap(vec_ws, vec_ws2);
            ws = vec_ws.data();
        }

        // Pad.
        if (std::cmp_greater(w, len))
        {
            std::vector<CharT> vec_ws3(w);
            CharT* ws3 = vec_ws3.data();
            bool startSign = (ws[0] == m_out_atoms[s_ominus]) || (ws[0] == m_out_atoms[s_oplus]);
            bool start0x = (ws[0] == m_out_atoms[s_odigits]) && (len > 1u) &&
                           ((ws[1] == m_out_atoms[s_ox]) || (ws[1] == m_out_atoms[s_oX]));
            const ios_defs::fmtflags adjust = io.flags() & ios_defs::adjustfield;
            pad(io.fill(), w, adjust, ws3, ws, len, startSign, start0x);

            std::swap(vec_ws, vec_ws3);
            ws = vec_ws.data();
        }

        // [22.2.2.2.2] Stage 4.
        // Write resulting, fully-formatted string to output iterator.
        return std::copy(ws, ws + len, s);
    }

    /**
     * @lang{ZH}
     * @brief 将整数值格式化为字符类型序列并写入输出迭代器。
     *
     * 按基数（十进制/八进制/十六进制）将数字写入缓冲区（阶段 1），
     * 按需插入千位分组分隔符，然后前置基数前缀（`0`/`0x`/`0X`）或符号（`+`/`-`），
     * 最后应用对齐填充（阶段 3/4）。
     *
     * @tparam TIter 输出迭代器类型。
     * @tparam TValue 整数类型。
     * @param s 输出迭代器，写入格式化结果。
     * @param io 提供格式标志、宽度和填充字符的流对象。
     * @param v 要格式化的整数值。
     * @return 写入结束后的输出迭代器。
     * @endif
     *
     * @lang{EN}
     * @brief Formats an integer value as a character-type sequence and writes it to the output iterator.
     *
     * Converts digits by radix (decimal/octal/hexadecimal) into a buffer (stage 1),
     * inserts grouping separators as needed, prepends the base prefix (`0`/`0x`/`0X`)
     * or sign (`+`/`-`), then applies alignment padding (stages 3/4).
     *
     * @tparam TIter The output iterator type.
     * @tparam TValue The integer type.
     * @param s Output iterator to receive the formatted result.
     * @param io Stream object providing format flags, width, and fill character.
     * @param v The integer value to format.
     * @return The output iterator after writing.
     * @endif
     */
    template <typename TIter, typename TValue>
    TIter insert_int(TIter s, ios_base<char_type>& io, TValue v) const
    {
        using unsigned_type = std::make_unsigned_t<TValue>;

        const ios_defs::fmtflags flags = io.flags();

        // Consume the field width up front, before any allocation that can throw:
        // width() is one-shot and a stale value must not survive onto the stream
        // if we leave by an exception. Used for padding below.
        const std::streamsize w = io.width();
        io.width(0);

        // Long enough to hold hex, dec, and octal representations.
        const auto ilen = 5 * sizeof(TValue);
        std::vector<char_type> cs_vec(ilen);
        char_type* cs = cs_vec.data();

        // [22.2.2.2.2] Stage 1, numeric conversion to character.
        // Result is returned right-justified in the buffer.
        const ios_defs::fmtflags basefield = flags & ios_defs::basefield;
        const bool dec = (basefield != ios_defs::oct && basefield != ios_defs::hex);
        const unsigned_type u = ((v > 0 || !dec) ? unsigned_type(v)
                                                 : -unsigned_type(v));
        auto len = static_cast<size_t>(int_to_char(cs + ilen, u, flags, dec));
        cs += ilen - len;

        // Add grouping, if necessary.
        if (!m_grouping.empty())
        {
            // Grouping can add (almost) as many separators as the number
            // of digits + space is reserved for numeric base or sign.
            std::vector<char_type> cs_vec2((ilen + 1) * 2);
            char_type* cs2 = cs_vec2.data() + 2;
            len = static_cast<size_t>(FacetHelper::add_grouping(cs2, m_thousands_sep, m_grouping, cs, cs + len) - cs2);
            std::swap(cs_vec, cs_vec2);
            cs = cs_vec.data() + 2;
        }

        // Complete Stage 1, prepend numeric base or sign.
        if (dec)
        { // Decimal.
            if (v >= 0)
            {
                if (bool(flags & ios_defs::showpos) && std::is_signed_v<TValue>)
                    *--cs = m_out_atoms[s_oplus], ++len;
            }
            else
                *--cs = m_out_atoms[s_ominus], ++len;
        }
        else if (bool(flags & ios_defs::showbase) && v)
        {
            if (basefield == ios_defs::oct)
                *--cs = m_out_atoms[s_odigits], ++len;
            else
            {
                // 'x' or 'X'
                const bool uppercase = flags & ios_defs::uppercase;
                *--cs = m_out_atoms[s_ox + uppercase];
                // '0'
                *--cs = m_out_atoms[s_odigits];
                len += 2;
            }
        }

        // Pad.
        if (std::cmp_greater(w, len))
        {
            std::vector<char_type> cs_vec3(w);
            char_type* cs3 = cs_vec3.data();
            bool startSign = (cs[0] == m_out_atoms[s_ominus]) || (cs[0] == m_out_atoms[s_oplus]);
            bool start0x = (cs[0] == m_out_atoms[s_odigits]) && (len > 1u) &&
                           ((cs[1] == m_out_atoms[s_ox]) || (cs[1] == m_out_atoms[s_oX]));
            const ios_defs::fmtflags adjust = io.flags() & ios_defs::adjustfield;
            pad(io.fill(), w, adjust, cs3, cs, len, startSign, start0x);
            std::swap(cs_vec, cs_vec3);
            cs = cs_vec.data();
        }

        // [22.2.2.2.2] Stage 4.
        // Write resulting, fully-formatted string to output iterator.
        return std::copy(cs, cs + len, s);
    }

    /**
     * @lang{ZH}
     * @brief 将无符号整数按指定进制逐位写入缓冲区末端，返回写入的字符数。
     *
     * 结果在缓冲区中右对齐（从 `bufend` 向低地址方向填充）。
     *
     * @tparam TValue 无符号整数类型。
     * @param bufend 写入缓冲区的尾后指针（结果从此向低地址填充）。
     * @param v 要转换的无符号整数值。
     * @param flags 格式标志，用于选择基数和大小写。
     * @param dec 若为 `true`，则使用十进制；否则根据 `flags` 选择八进制或十六进制。
     * @return 写入的字符数。
     * @endif
     *
     * @lang{EN}
     * @brief Converts an unsigned integer to characters in the specified base, writing right-to-left from the buffer end.
     *
     * The result is right-justified in the buffer (filled from `bufend` toward lower addresses).
     *
     * @tparam TValue The unsigned integer type.
     * @param bufend One-past-the-end pointer of the write buffer (filling goes toward lower addresses).
     * @param v The unsigned integer value to convert.
     * @param flags Format flags used to select the radix and case.
     * @param dec If `true`, use decimal; otherwise select octal or hexadecimal from `flags`.
     * @return The number of characters written.
     * @endif
     */
    template <typename TValue>
    int int_to_char(CharT* bufend, TValue v, ios_defs::fmtflags flags, bool dec) const
    {
        auto buf = bufend;
        if (dec)
        {   // Decimal.
            do // NOLINT(cppcoreguidelines-avoid-do-while)
            {
                *--buf = m_out_atoms[(v % 10) + s_odigits];
                v /= 10;
            } while (v != 0);
        }
        else if ((flags & ios_defs::basefield) == ios_defs::oct)
        { // Octal.
            do // NOLINT(cppcoreguidelines-avoid-do-while)
            {
                *--buf = m_out_atoms[(v & 0x7) + s_odigits];
                v >>= 3;
            } while (v != 0);
        }
        else
        {   // Hex.
            const bool uppercase = flags & ios_defs::uppercase;
            const int case_offset = uppercase ? s_oudigits : s_odigits;
            do // NOLINT(cppcoreguidelines-avoid-do-while)
            {
                *--buf = m_out_atoms[(v & 0xf) + case_offset];
                v >>= 4;
            } while (v != 0);
        }
        return bufend - buf;
    }

    /**
     * @lang{ZH}
     * @brief 对格式化缓冲区应用宽度填充，委托给 `pad_impl_`。同时将 `len` 更新为填充后的总宽度。
     *
     * @param fill 填充字符。
     * @param w 目标字段宽度。
     * @param adjust 对齐标志（left/right/internal）。
     * @param new_buf 输出缓冲区，容量不小于 `w`。
     * @param cs 原始格式化内容的起始指针。
     * @param len 原始内容的字符数；函数返回后更新为 `w`。
     * @param startSign 原始内容是否以符号字符（`+`/`-`）开头。
     * @param start0x 原始内容是否以 `0x`/`0X` 开头。
     * @endif
     *
     * @lang{EN}
     * @brief Applies width padding to the formatted buffer; delegates to `pad_impl_`. Updates `len` to the padded total width.
     *
     * @param fill The fill character.
     * @param w The target field width.
     * @param adjust The alignment flag (left/right/internal).
     * @param new_buf Output buffer with capacity of at least `w`.
     * @param cs Pointer to the start of the original formatted content.
     * @param len Number of characters in the original content; updated to `w` on return.
     * @param startSign Whether the original content begins with a sign character (`+`/`-`).
     * @param start0x Whether the original content begins with `0x`/`0X`.
     * @endif
     */
    void pad(char_type fill, std::streamsize w, ios_defs::fmtflags adjust,
             char_type* new_buf, const char_type* cs, size_t& len,
             bool startSign, bool start0x) const
    {
      // [22.2.2.2.2] Stage 3.
      // If necessary, pad.
      pad_impl_(adjust, fill, new_buf, cs, w, static_cast<std::streamsize>(len), startSign, start0x);
      len = static_cast<size_t>(w);
    }

    /**
     * @lang{ZH}
     * @brief 按对齐方式将原始内容与填充字符合并到新缓冲区中。
     *
     * - 左对齐：内容在前，填充在后。
     * - 右对齐：填充在前，内容在后。
     * - 内部对齐：若内容以符号字符开头则将其保留在最前，若以 `0x`/`0X` 开头则保留两字符，
     *   之后插入填充，最后是剩余内容。
     *
     * @param adjust 对齐标志。
     * @param fill 填充字符。
     * @param news 输出缓冲区起始指针。
     * @param olds 原始内容起始指针。
     * @param newlen 填充后的总字符数（即字段宽度）。
     * @param oldlen 原始内容的字符数。
     * @param startSign 原始内容是否以符号字符开头。
     * @param start0x 原始内容是否以 `0x`/`0X` 开头。
     * @endif
     *
     * @lang{EN}
     * @brief Merges the original content and fill characters into a new buffer according to the alignment.
     *
     * - Left-align: content first, fill last.
     * - Right-align: fill first, content last.
     * - Internal: if the content begins with a sign character it is placed first; if it
     *   begins with `0x`/`0X` those two characters are placed first; then fill is inserted,
     *   followed by the remaining content.
     *
     * @param adjust The alignment flag.
     * @param fill The fill character.
     * @param news Pointer to the start of the output buffer.
     * @param olds Pointer to the start of the original content.
     * @param newlen Total character count after padding (i.e., the field width).
     * @param oldlen Character count of the original content.
     * @param startSign Whether the original content begins with a sign character.
     * @param start0x Whether the original content begins with `0x`/`0X`.
     * @endif
     */
    void pad_impl_(ios_defs::fmtflags adjust, char_type fill,
                   char_type* news, const char_type* olds,
                   std::streamsize newlen, std::streamsize oldlen,
                   bool startSign, bool start0x) const
    {

        const auto plen = static_cast<size_t>(newlen - oldlen);

        // Padding last.
        if (adjust == ios_defs::left)
        {
            std::copy(olds, olds + oldlen, news);
            std::fill_n(news + oldlen, plen, fill);
            return;
        }

        size_t mod = 0;
        if (adjust == ios_defs::internal)
        {
            // Pad after the sign, if there is one.
            // Pad after 0[xX], if there is one.
            // Who came up with these rules, anyway? Jeeze.
            if (startSign)
            {
                news[0] = olds[0];
                mod = 1;
                ++news;
            }
            else if (start0x)
            {
                news[0] = olds[0];
                news[1] = olds[1];
                mod = 2;
                news += 2;
            }
            // else Padding first.
        }
        std::fill_n(news, plen, fill);
        std::copy(olds + mod, olds + oldlen, news + plen);
    }

    /**
     * @lang{ZH}
     * @brief 根据 `ios_base` 格式标志构造 `snprintf` 用的格式字符串，写入 `fptr`。
     *
     * 按照 C++ 标准 [22.2.2.2.2] 表 58/60 的规则，将 `showpos`、`showpoint`、
     * `floatfield` 和 `uppercase` 等标志翻译为对应的 `printf` 格式说明符。
     *
     * @param flags 当前流的格式标志。
     * @param fptr 写入格式字符串的字符缓冲区，调用者保证容量足够（至少 16 字节）。
     * @param mod 长度修饰符（`'L'` 或 `'\0'`）。
     * @endif
     *
     * @lang{EN}
     * @brief Builds the `snprintf` format string from the `ios_base` format flags, writing to `fptr`.
     *
     * Translates `showpos`, `showpoint`, `floatfield`, and `uppercase` flags into the
     * corresponding `printf` format specifiers following C++ standard [22.2.2.2.2] tables 58/60.
     *
     * @param flags The current stream format flags.
     * @param fptr Character buffer to receive the format string; the caller guarantees sufficient capacity (at least 16 bytes).
     * @param mod The length modifier (`'L'` or `'\0'`).
     * @endif
     */
    void format_float_(ios_defs::fmtflags flags, char* fptr, char mod) const noexcept
    {
        *fptr++ = '%';
        // [22.2.2.2.2] Table 60
        if (flags & ios_defs::showpos)
            *fptr++ = '+';
        if (flags & ios_defs::showpoint)
            *fptr++ = '#';

        ios_defs::fmtflags fltfield = flags & ios_defs::floatfield;
        if (fltfield != (ios_defs::fixed | ios_defs::scientific))
        {
            // As per DR 231: not only when flags & ios_base::fixed || prec > 0
            *fptr++ = '.';
            *fptr++ = '*';
        }

        if (mod)
            *fptr++ = mod;
        // [22.2.2.2.2] Table 58
        if (fltfield == ios_defs::fixed)
            *fptr++ = 'f';
        else if (fltfield == ios_defs::scientific)
            *fptr++ = (flags & ios_defs::uppercase) ? 'E' : 'e';
        else if (fltfield == (ios_defs::fixed | ios_defs::scientific))
            *fptr++ = (flags & ios_defs::uppercase) ? 'A' : 'a';
        else
            *fptr++ = (flags & ios_defs::uppercase) ? 'G' : 'g';
        *fptr = '\0';
    }

    /**
     * @lang{ZH}
     * @brief 向浮点数的整数部分插入千位分隔符，保留小数部分不变，并更新 `len`。
     *
     * 通过 `FacetHelper::add_grouping` 在整数部分插入 `sep`，然后将
     * 原始内容中小数点及其后的部分原样追加到结果末尾。
     *
     * @param grouping 数字分组规则（内部规范化格式）。
     * @param sep 千位分隔符字符。
     * @param p 指向宽字符缓冲区中小数点位置的指针；若无小数点则为 `nullptr`。
     * @param new_buf 输出缓冲区。
     * @param cs 输入宽字符缓冲区的起始指针。
     * @param len 输入字符数；函数返回后更新为输出字符数。
     * @endif
     *
     * @lang{EN}
     * @brief Inserts grouping separators into the integer part of a floating-point number, leaving the fractional part unchanged, and updates `len`.
     *
     * Uses `FacetHelper::add_grouping` to insert `sep` into the integer part, then
     * appends the decimal point and everything after it from the original content.
     *
     * @param grouping Digit grouping rules (internal normalized form).
     * @param sep The thousands separator character.
     * @param p Pointer to the decimal point position in the wide-character buffer; `nullptr` if no decimal point.
     * @param new_buf Output buffer.
     * @param cs Pointer to the start of the input wide-character buffer.
     * @param len Input character count; updated to the output character count on return.
     * @endif
     */
    void group_float(const std::vector<uint8_t>& grouping, char_type sep, const char_type* p, char_type* new_buf, char_type* cs, size_t& len) const
    {
        // _GLIBCXX_RESOLVE_LIB_DEFECTS
        // 282. What types does numpunct grouping refer to?
        // Add grouping, if necessary.
        const size_t declen = p ? static_cast<size_t>(p - cs) : len;
        char_type* p2 = FacetHelper::add_grouping(new_buf, sep, grouping, cs, cs + declen);

        // Tack on decimal part.
        auto newlen = static_cast<size_t>(p2 - new_buf);
        if (p)
        {
            std::copy(p, p + len - declen, p2);
            newlen += len - declen;
        }
        len = newlen;
    }

    /**
     * @lang{ZH}
     * @brief 从字符流中提取整数字段并解析为 `TValue`。
     *
     * 自动检测基数前缀（`0x`/`0X` 表示十六进制，`0` 表示八进制），
     * 在 `basefield == 0` 时依据输入内容动态确定基数。
     * 在提取过程中同步累计分组信息，提取完成后调用 `FacetHelper::verify_grouping`
     * 校验是否符合 locale 的分组规则。
     * 溢出时按 LWG 23 将结果设为 `numeric_limits` 的极值并返回失败；
     * 溢出信号优先于分组校验失败，以避免将结构合法但数值越界的输入误归类为格式错误。
     *
     * @tparam TIter 输入迭代器类型。
     * @tparam TSent 哨兵类型。
     * @tparam TValue 目标整数类型。
     * @param beg 输入范围的起始迭代器。
     * @param end 输入范围的结束哨兵。
     * @param io 提供格式标志的流对象。
     * @param v 解析成功后写入结果的整数引用。
     * @return `{成功标志, 消费后迭代器}` 的 `pair`。
     * @endif
     *
     * @lang{EN}
     * @brief Extracts an integer field from the character stream and parses it into `TValue`.
     *
     * Automatically detects base prefixes (`0x`/`0X` for hexadecimal, `0` for octal),
     * and determines the base dynamically from the input when `basefield == 0`.
     * Grouping information is accumulated during extraction and validated by
     * `FacetHelper::verify_grouping` after extraction. On overflow, the result is clamped
     * to the `numeric_limits` extreme value and failure is returned per LWG 23; the overflow
     * signal takes priority over grouping-check failure to avoid misclassifying
     * structurally valid but out-of-range input as a format error.
     *
     * @tparam TIter The input iterator type.
     * @tparam TSent The sentinel type.
     * @tparam TValue The target integer type.
     * @param beg Start iterator of the input range.
     * @param end End sentinel of the input range.
     * @param io Stream object providing format flags.
     * @param v Reference to the integer variable to receive the parsed result.
     * @return A `pair` of `{success flag, iterator after consumed input}`.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent, typename TValue>
    std::pair<bool, TIter> extract_int(TIter beg, TSent end, ios_base<char_type>& io, TValue& v) const
    {
        using unsigned_type = std::make_unsigned_t<TValue>;

        CharT c{};

        // NB: Iff basefield == 0, base can change based on contents.
        const ios_defs::fmtflags basefield = io.flags() & ios_defs::basefield;
        const bool oct = basefield == ios_defs::oct;
        int base = oct ? 8 : (basefield == ios_defs::hex ? 16 : 10);

        // True if beg becomes equal to end.
        bool testeof = beg == end;

        // First check for sign.
        bool negative = false;
        if (!testeof)
        {
            c = *beg;
            negative = c == m_in_atoms[s_iminus];
            if ((negative || c == m_in_atoms[s_iplus])
                && (m_grouping.empty() || c != m_thousands_sep)
                && (c != m_decimal_point))
            {
                if (++beg != end)
                    c = *beg;
                else
                    testeof = true;
            }
        }

        // Next, look for leading zeros and check required digits
        // for base formats.
        bool found_zero = false;
        int sep_pos = 0;
        while (!testeof)
        {
            if ((!m_grouping.empty() && c == m_thousands_sep)
                || c == m_decimal_point)
                break; // NOLINT(bugprone-branch-clone)
            else if (c == m_in_atoms[s_izero] && (!found_zero || base == 10))
            {
                found_zero = true;
                ++sep_pos;
                if (basefield == 0) base = 8;
                if (base == 8) sep_pos = 0;
            }
            else if (found_zero && (c == m_in_atoms[s_ix] || c == m_in_atoms[s_iX]))
            {
                if (basefield == 0) base = 16;
                if (base == 16)
                {
                    found_zero = false;
                    sep_pos = 0;
                }
                else
                    break;
            }
            else
                break;

            if (++beg != end)
            {
                c = *beg;
                if (!found_zero) break;
            }
            else
                testeof = true;
        }

        // At this point, base is determined. If not hex, only allow
        // base digits as valid input.
        const size_t len = (base == 16 ? s_iend - s_izero : base);

        // Extract.
        std::vector<uint8_t> found_grouping;
        if (!m_grouping.empty()) found_grouping.reserve(32);
        bool testfail = false;
        bool testoverflow = false;
        const unsigned_type max_val = (negative && std::is_signed_v<TValue>)
                                        ? -static_cast<unsigned_type>(std::numeric_limits<TValue>::min()) : std::numeric_limits<TValue>::max();
        const unsigned_type smax = max_val / base;
        unsigned_type result = 0;
        int digit = 0;
        const char_type* lit_zero = m_in_atoms.data() + s_izero;

        while (!testeof)
        {
            // According to 22.2.2.1.2, p8-9, first look for thousands_sep
            // and decimal_point.
            if (!m_grouping.empty() && c == m_thousands_sep)
            {
                // NB: Thousands separator at the beginning of a string
                // is a no-no, as is two consecutive thousands separators.
                if (sep_pos)
                {
                    // found_grouping stores each group's digit count in a
                    // uint8_t. A single group whose size cannot fit in that
                    // byte is rejected rather than silently truncated.
                    if (std::cmp_greater(sep_pos, std::numeric_limits<uint8_t>::max()))
                    {
                        testfail = true;
                        break;
                    }
                    found_grouping.push_back(static_cast<uint8_t>(sep_pos));
                    sep_pos = 0;
                }
                else
                {
                    testfail = true;
                    break;
                }
            }
            else if (c == m_decimal_point) break;
            else
            {
                const char_type* q = std::find(lit_zero, lit_zero + len, c);
                if (q == lit_zero + len) break;

                digit = q - lit_zero;
                if (digit > 15) digit -= 6;
                if (result > smax) testoverflow = true;
                else
                {
                    result *= base;
                    testoverflow |= result > max_val - digit;
                    result += digit;
                    ++sep_pos;
                }
            }

            if (++beg != end) c = *beg;
            else testeof = true;
        }

        bool success = true;
        // Digit grouping is checked. If grouping and found_grouping don't
        // match, then get very very upset, and set failbit.
        if (!found_grouping.empty())
        {
            // Add the ending grouping. Reject inputs whose final group
            // exceeds the uint8_t storage in found_grouping.
            if (std::cmp_greater(sep_pos, std::numeric_limits<uint8_t>::max()))
            {
                testfail = true;
            }
            else
            {
                found_grouping.push_back(static_cast<uint8_t>(sep_pos));
                success = FacetHelper::verify_grouping(m_grouping, found_grouping);
            }
        }

        // LWG 23 (Num_get overflow result): when the field overflows,
        // v must be set to numeric_limits::max() / min() with failbit.
        //
        // testoverflow is checked BEFORE testfail by design. Once the
        // digit-accumulation loop sets testoverflow, ++sep_pos is skipped
        // for every subsequent digit, so sep_pos freezes. A later, otherwise
        // well-formed thousands_sep then fails the grouping check and sets
        // testfail — but that testfail is a side effect of the overflow
        // short-circuit, not an independent structural error in the input.
        //
        // Letting testfail win in that overlap would map a structurally
        // valid, numerically out-of-range input (e.g. "12,345,678,901,234,567"
        // into uint32_t under grouping "\3") to v = 0 instead of v = max,
        // contradicting LWG 23. We diverge from libstdc++'s ordering here
        // (which has the same latent issue) to keep the overflow signal
        // dominant whenever it fires.
        if (testoverflow)
        {
            if (negative && std::is_signed_v<TValue>)
                v = std::numeric_limits<TValue>::min();
            else
                v = std::numeric_limits<TValue>::max();
            success = false;
        }
        else if ((!sep_pos && !found_zero && !found_grouping.size()) || testfail)
        {
            v = 0;
            success = false;
        }
        else
            v = negative ? -result : result;

        return std::pair(success, beg);
    }

    /**
     * @lang{ZH}
     * @brief 从字符流中提取浮点数字段，将其规范化为 "C" locale 的 ASCII 字符串。
     *
     * 依次识别符号、前导零、整数部分（含千位分隔符和分组校验）、小数点、
     * 小数部分以及科学计数法指数（含指数符号）。
     * 所有识别到的字符都被转换为对应的 ASCII 字符并追加到 `xtrc`，
     * 以便随后由 `convert_to_v` 在 "C" locale 下调用 `strtof`/`strtod`/`strtold` 解析。
     *
     * @tparam TIter 输入迭代器类型。
     * @tparam TSent 哨兵类型。
     * @param beg 输入范围的起始迭代器。
     * @param end 输入范围的结束哨兵。
     * @param io 提供格式标志的流对象。
     * @param xtrc 接收规范化 ASCII 字符串的输出字符串；调用前应已预留容量。
     * @return `{成功标志, 消费后迭代器}` 的 `pair`。
     * @endif
     *
     * @lang{EN}
     * @brief Extracts a floating-point field from the character stream and normalizes it into a "C"-locale ASCII string.
     *
     * Recognizes, in order: sign, leading zeros, integer part (including grouping
     * separators and grouping validation), decimal point, fractional digits, and
     * scientific-notation exponent (including exponent sign). All recognized characters
     * are translated to their ASCII equivalents and appended to `xtrc`, ready for
     * `strtof`/`strtod`/`strtold` under the "C" locale inside `convert_to_v`.
     *
     * @tparam TIter The input iterator type.
     * @tparam TSent The sentinel type.
     * @param beg Start iterator of the input range.
     * @param end End sentinel of the input range.
     * @param io Stream object providing format flags.
     * @param xtrc Output string that receives the normalized ASCII representation; should have reserved capacity before the call.
     * @return A `pair` of `{success flag, iterator after consumed input}`.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
    std::pair<bool, TIter> extract_float(TIter beg, TSent end, ios_base<char_type>& io, std::string& xtrc) const
    {
        char_type c = char_type();

        // True if beg becomes equal to end.
        bool testeof = beg == end;

        // First check for sign.
        if (!testeof)
        {
            c = *beg;
            const bool plus = c == m_in_atoms[s_iplus];
            if ((plus || c == m_in_atoms[s_iminus])
                && (m_grouping.empty() || c != m_thousands_sep)
                && (c != m_decimal_point))
            {
                xtrc += plus ? '+' : '-';
                if (++beg != end) c = *beg;
                else
                    testeof = true;
            }
        }

        // Next, look for leading zeros.
        bool found_mantissa = false;
        int sep_pos = 0;
        while (!testeof)
        {
            if ((!m_grouping.empty() && c == m_thousands_sep) || c == m_decimal_point)
                break; // NOLINT(bugprone-branch-clone)
            else if (c == m_in_atoms[s_izero])
            {
                if (!found_mantissa)
                {
                    xtrc += '0';
                    found_mantissa = true;
                }
                ++sep_pos;

                if (++beg != end)
                    c = *beg;
                else
                    testeof = true;
            }
            else
                break;
        }

        // Only need acceptable digits for floating point numbers.
        bool found_dec = false;
        bool found_sci = false;
        std::vector<uint8_t> found_grouping;
        if (!m_grouping.empty())
            found_grouping.reserve(32);
        const char_type* lit_zero = m_in_atoms.data() + s_izero;

        while (!testeof)
        {
            // According to 22.2.2.1.2, p8-9, first look for thousands_sep
            // and decimal_point.
            if (!m_grouping.empty() && c == m_thousands_sep)
            {
                if (!found_dec && !found_sci)
                {
                    // NB: Thousands separator at the beginning of a string
                    // is a no-no, as is two consecutive thousands separators.
                    if (sep_pos)
                    {
                        // found_grouping stores each group's digit count in a
                        // uint8_t. Reject inputs whose group size cannot fit.
                        if (std::cmp_greater(sep_pos, std::numeric_limits<uint8_t>::max()))
                        {
                            xtrc.clear();
                            break;
                        }
                        found_grouping.push_back(static_cast<uint8_t>(sep_pos));
                        sep_pos = 0;
                    }
                    else
                    {
                        // NB: convert_to_v will not assign v and will
                        // set the failbit.
                        xtrc.clear();
                        break;
                    }
                }
                else
                    break;
            }
            else if (c == m_decimal_point)
            {
                if (!found_dec && !found_sci)
                {
                    // If no grouping chars are seen, no grouping check
                    // is applied. Therefore found_grouping is adjusted
                    // only if decimal_point comes after some thousands_sep.
                    if (found_grouping.size())
                    {
                        if (std::cmp_greater(sep_pos, std::numeric_limits<uint8_t>::max()))
                        {
                            xtrc.clear();
                            break;
                        }
                        found_grouping.push_back(static_cast<uint8_t>(sep_pos));
                    }
                    xtrc += '.';
                    found_dec = true;
                }
                else
                    break;
            }
            else
            {
                const char_type* q = std::find(lit_zero, lit_zero + 10, c);
                if (q != lit_zero + 10)
                {
                    xtrc += '0' + (q - lit_zero);
                    found_mantissa = true;
                    ++sep_pos;
                }
                else if ((c == m_in_atoms[s_ie] || c == m_in_atoms[s_iE])
                        && !found_sci && found_mantissa)
                {
                    // Scientific notation.
                    if (found_grouping.size() && !found_dec)
                    {
                        if (std::cmp_greater(sep_pos, std::numeric_limits<uint8_t>::max()))
                        {
                            xtrc.clear();
                            break;
                        }
                        found_grouping.push_back(static_cast<uint8_t>(sep_pos));
                    }
                    xtrc += 'e';
                    found_sci = true;

                    // Remove optional plus or minus sign, if they exist.
                    if (++beg != end)
                    {
                        c = *beg;
                        const bool plus = c == m_in_atoms[s_iplus];
                        if ((plus || c == m_in_atoms[s_iminus])
                            && (m_grouping.empty() || c != m_thousands_sep)
                            && (c != m_decimal_point))
                            xtrc += plus ? '+' : '-';
                        else
                            continue;
                    }
                    else
                    {
                        testeof = true;
                        break;
                    }
                }
                else
                    break;
            }

            if (++beg != end)
                c = *beg;
            else
                testeof = true;
        }

        bool success = true;
        // Digit grouping is checked. If grouping and found_grouping don't
        // match, then get very very upset, and set failbit.
        if (!found_grouping.empty())
        {
            // Add the ending grouping if a decimal or 'e'/'E' wasn't found.
            if (!found_dec && !found_sci)
            {
                if (std::cmp_greater(sep_pos, std::numeric_limits<uint8_t>::max()))
                {
                    // Final group exceeds the uint8_t storage in
                    // found_grouping. Fail cleanly rather than truncate.
                    success = false;
                }
                else
                {
                    found_grouping.push_back(static_cast<uint8_t>(sep_pos));
                    success = FacetHelper::verify_grouping(m_grouping, found_grouping);
                }
            }
            else
            {
                success = FacetHelper::verify_grouping(m_grouping, found_grouping);
            }
        }

        return std::pair(success, beg);
    }

    /**
     * @lang{ZH}
     * @brief 将 "C" locale 格式的 ASCII 浮点字符串转换为浮点值。
     *
     * 在 "C" locale 守卫下调用 `strtof`/`strtod`/`strtold`。
     * 按 LWG 23 处理特殊情况：无穷大映射为 `numeric_limits::max()` 的有限极值并返回失败；
     * 转换失败（`sanity == s` 或字符串未完全消耗）时将 `v` 设为 0 并返回失败。
     *
     * @tparam TValue 浮点类型（`float`、`double` 或 `long double`）。
     * @param s 以 `'\0'` 结尾的 "C" locale ASCII 浮点字符串。
     * @param v 转换成功后写入结果的浮点数引用。
     * @return 若转换成功且结果为有限值则返回 `true`，否则返回 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Converts a "C"-locale ASCII floating-point string to a floating-point value.
     *
     * Calls `strtof`/`strtod`/`strtold` under a "C" locale guard.
     * Handles special cases per LWG 23: infinity is mapped to the finite extreme value
     * `numeric_limits::max()` and failure is returned; conversion failure (when
     * `sanity == s` or the string is not fully consumed) sets `v` to 0 and returns failure.
     *
     * @tparam TValue The floating-point type (`float`, `double`, or `long double`).
     * @param s Null-terminated "C"-locale ASCII floating-point string.
     * @param v Reference to the floating-point variable to receive the converted result.
     * @return `true` if conversion succeeded and the result is finite; `false` otherwise.
     * @endif
     */
    template <typename TValue>
    bool convert_to_v(const char* s, TValue& v) const
    {
        bool res = true;
        char* sanity = nullptr;

        clocale_wrapper inter_locale("C");
        clocale_user guard(inter_locale);

        if constexpr (std::is_same_v<TValue, float>)
            v = strtof(s, &sanity);
        else if constexpr (std::is_same_v<TValue, double>)
            v = strtod(s, &sanity);
        else
            v = strtold(s, &sanity);

        // _GLIBCXX_RESOLVE_LIB_DEFECTS
        // 23. Num_get overflow result.
        if (sanity == s || *sanity != '\0')
        {
            v = TValue(0);
            res = false;
        }
        else if (v == std::numeric_limits<TValue>::infinity())
        {
            v = std::numeric_limits<TValue>::max();
            res = false;
        }
        else if (v == -std::numeric_limits<TValue>::infinity())
        {
            v = -std::numeric_limits<TValue>::max();
            res = false;
        }
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 断言辅助函数：验证宽字符原子数组中所有元素两两不同。
     *
     * `extract_int`/`extract_float` 将输入宽字符与 `m_in_atoms` 中的条目直接比较，
     * `insert_int`/`insert_float` 依据位置索引从 `m_out_atoms` 发出字形；
     * 两者均假设 `ctype::widen` 在这 26 个字符集合上是单射的。
     * 若某个非 ASCII locale 的 `widen` 使 `widen('a') == widen('A')` 之类的碰撞发生，
     * 则某些数字在输入侧将不可达，在输出侧会产生语义上不同位置的相同字形。
     * C++ 标准库对此集合的单射性隐式依赖，但不作显式保证；
     * IOv2 在构造时通过此函数在 debug/覆盖率/sanitizer 构建中捕获违规。
     *
     * @param p 原子字符数组的起始指针。
     * @param n 数组元素个数。
     * @return 若所有元素两两不同则返回 `true`，否则返回 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Assertion helper that verifies all elements in a wide-character atom array are pairwise distinct.
     *
     * `extract_int`/`extract_float` compare incoming wide characters directly against
     * entries in `m_in_atoms`, and `insert_int`/`insert_float` emit glyphs from
     * `m_out_atoms` by position index; both assume `ctype::widen` is injective on
     * this 26-character set. If an exotic locale's `widen` produces a collision such
     * as `widen('a') == widen('A')`, certain digits become unreachable on the input
     * side and semantically distinct output positions emit identical glyphs. The C++
     * standard library implicitly relies on injectivity over this set without an
     * explicit guarantee; IOv2 catches violations at construction time in
     * debug/coverage/sanitizer builds via this function.
     *
     * @param p Pointer to the start of the atom character array.
     * @param n Number of elements in the array.
     * @return `true` if all elements are pairwise distinct; `false` otherwise.
     * @endif
     */
    static bool atoms_pairwise_distinct(const CharT* p, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 1; j < n; ++j)
                if (p[i] == p[j]) return false;
        return true;
    }

private:
    std::shared_ptr<const ctype<CharT>> m_ctype;        ///< @lang{ZH} 用于字符扩宽的 `ctype` facet 共享指针。 @endif @lang{EN} Shared pointer to the `ctype` facet used for character widening. @endif
    CharT m_decimal_point;                              ///< @lang{ZH} 小数点字符，从 `numeric_conf` 缓存。 @endif @lang{EN} Decimal point character, cached from `numeric_conf`. @endif
    CharT m_thousands_sep;                              ///< @lang{ZH} 千位分隔符，从 `numeric_conf` 缓存；`CharT('\0')` 表示不分组。 @endif @lang{EN} Thousands separator, cached from `numeric_conf`; `CharT('\0')` means no grouping. @endif
    std::basic_string<CharT>  m_true_name;              ///< @lang{ZH} `true` 的文本表示，从 `numeric_conf` 缓存。 @endif @lang{EN} Textual representation of `true`, cached from `numeric_conf`. @endif
    std::basic_string<CharT>  m_false_name;             ///< @lang{ZH} `false` 的文本表示，从 `numeric_conf` 缓存。 @endif @lang{EN} Textual representation of `false`, cached from `numeric_conf`. @endif
    std::vector<uint8_t>      m_grouping;               ///< @lang{ZH} 数字分组规则（内部规范化格式），从 `numeric_conf` 缓存；为空时不分组。 @endif @lang{EN} Digit grouping rules (internal normalized form), cached from `numeric_conf`; empty means no grouping. @endif

private:
    ///< @lang{ZH}
    ///< 26 个输入原子字符的宽字符形式，对应 ASCII 串 `"-+xX0123456789abcdefABCDEF"`。
    ///< 用于解析时将输入字符与符号、基数前缀和十六进制数字进行比较。
    ///< 通过 `s_iminus`、`s_iplus`、`s_ix`、`s_iX`、`s_izero` 等索引访问各位置。
    ///< @endif
    ///< @lang{EN}
    ///< Widened form of the 26 input atom characters corresponding to `"-+xX0123456789abcdefABCDEF"`.
    ///< Used during parsing to match incoming characters against signs, base prefixes, and hex digits.
    ///< Positions are accessed via `s_iminus`, `s_iplus`, `s_ix`, `s_iX`, `s_izero`, and related indices.
    ///< @endif
    std::array<char_type, 26> m_in_atoms{};

    ///< @lang{ZH}
    ///< 36 个输出原子字符的宽字符形式，对应 ASCII 串 `"-+xX0123456789abcdef0123456789ABCDEF"`。
    ///< 其中 `'0'..'9'` 在 `[4..13]`（小写十六进制字母表 `s_odigits`）和
    ///< `[20..29]`（大写十六进制字母表 `s_oudigits`）各出现一次，
    ///< 使两段均构成完整的 16 元素字母表。
    ///< 通过 `s_ominus`、`s_oplus`、`s_ox`、`s_oX`、`s_odigits`、`s_oudigits` 等索引访问各位置。
    ///< @endif
    ///< @lang{EN}
    ///< Widened form of the 36 output atom characters corresponding to `"-+xX0123456789abcdef0123456789ABCDEF"`.
    ///< The `'0'..'9'` range appears at both `[4..13]` (lowercase hex alphabet, `s_odigits`) and
    ///< `[20..29]` (uppercase hex alphabet, `s_oudigits`), so each 16-entry section is a
    ///< self-contained digit alphabet for its case.
    ///< Positions are accessed via `s_ominus`, `s_oplus`, `s_ox`, `s_oX`, `s_odigits`, `s_oudigits`, and related indices.
    ///< @endif
    std::array<char_type, 36> m_out_atoms{};

    // Output atom indices into m_out_atoms ("-+xX0123456789abcdef0123456789ABCDEF").
    static constexpr int s_ominus       = 0;            ///< @lang{ZH} `'-'` 在 `m_out_atoms` 中的索引。 @endif @lang{EN} Index of `'-'` in `m_out_atoms`. @endif
    static constexpr int s_oplus        = 1;            ///< @lang{ZH} `'+'` 在 `m_out_atoms` 中的索引。 @endif @lang{EN} Index of `'+'` in `m_out_atoms`. @endif
    static constexpr int s_ox           = 2;            ///< @lang{ZH} `'x'` 在 `m_out_atoms` 中的索引。 @endif @lang{EN} Index of `'x'` in `m_out_atoms`. @endif
    static constexpr int s_oX           = 3;            ///< @lang{ZH} `'X'` 在 `m_out_atoms` 中的索引。 @endif @lang{EN} Index of `'X'` in `m_out_atoms`. @endif
    static constexpr int s_odigits      = 4;            ///< @lang{ZH} 小写十六进制字母表（`'0'..'9','a'..'f'`）在 `m_out_atoms` 中的起始索引。 @endif @lang{EN} Start index of the lowercase hex alphabet (`'0'..'9','a'..'f'`) in `m_out_atoms`. @endif
    static constexpr int s_oudigits     = s_odigits + 16;  ///< @lang{ZH} 大写十六进制字母表（`'0'..'9','A'..'F'`）在 `m_out_atoms` 中的起始索引。 @endif @lang{EN} Start index of the uppercase hex alphabet (`'0'..'9','A'..'F'`) in `m_out_atoms`. @endif
    static constexpr int s_oa           = s_odigits + 10;  ///< @lang{ZH} `'a'` 在 `m_out_atoms` 中的索引。 @endif @lang{EN} Index of `'a'` in `m_out_atoms`. @endif
    static constexpr int s_oe           = s_odigits + 14;  ///< @lang{ZH} `'e'` 在 `m_out_atoms` 中的索引。 @endif @lang{EN} Index of `'e'` in `m_out_atoms`. @endif
    static constexpr int s_oA           = s_oudigits + 10; ///< @lang{ZH} `'A'` 在 `m_out_atoms` 中的索引。 @endif @lang{EN} Index of `'A'` in `m_out_atoms`. @endif
    static constexpr int s_oE           = s_oudigits + 14; ///< @lang{ZH} `'E'` 在 `m_out_atoms` 中的索引。 @endif @lang{EN} Index of `'E'` in `m_out_atoms`. @endif

    // Input atom indices into m_in_atoms ("-+xX0123456789abcdefABCDEF").
    static constexpr int s_iminus       = 0;            ///< @lang{ZH} `'-'` 在 `m_in_atoms` 中的索引。 @endif @lang{EN} Index of `'-'` in `m_in_atoms`. @endif
    static constexpr int s_iplus        = 1;            ///< @lang{ZH} `'+'` 在 `m_in_atoms` 中的索引。 @endif @lang{EN} Index of `'+'` in `m_in_atoms`. @endif
    static constexpr int s_ix           = 2;            ///< @lang{ZH} `'x'` 在 `m_in_atoms` 中的索引。 @endif @lang{EN} Index of `'x'` in `m_in_atoms`. @endif
    static constexpr int s_iX           = 3;            ///< @lang{ZH} `'X'` 在 `m_in_atoms` 中的索引。 @endif @lang{EN} Index of `'X'` in `m_in_atoms`. @endif
    static constexpr int s_izero        = 4;            ///< @lang{ZH} `'0'`（数字段起始）在 `m_in_atoms` 中的索引。 @endif @lang{EN} Index of `'0'` (start of digit section) in `m_in_atoms`. @endif
    static constexpr int s_ie           = s_izero + 14; ///< @lang{ZH} `'e'` 在 `m_in_atoms` 中的索引。 @endif @lang{EN} Index of `'e'` in `m_in_atoms`. @endif
    static constexpr int s_iE           = s_izero + 20; ///< @lang{ZH} `'E'` 在 `m_in_atoms` 中的索引。 @endif @lang{EN} Index of `'E'` in `m_in_atoms`. @endif
    static constexpr int s_iend         = 26;           ///< @lang{ZH} `m_in_atoms` 有效索引的尾后值（同时是数组大小）。 @endif @lang{EN} One-past-the-last valid index into `m_in_atoms` (also the array size). @endif
};

template<typename TConfPtr, typename TCtypePtr>
    requires (std::is_same_v<typename TConfPtr::element_type::char_type,
                             typename TCtypePtr::element_type::char_type>)
numeric(TConfPtr, TCtypePtr) -> numeric<typename TConfPtr::element_type::char_type>;
}
