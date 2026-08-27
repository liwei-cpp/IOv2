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
#include <cmath>
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
        std::size_t len = name.size();

        const auto w = io.width();
        io.width(0);
        if (w > static_cast<decltype(w)>(len))
        {
            const auto plen = w - len;
            if (plen > ios_defs::max_pad_count)
                throw stream_error("numeric put fail: fill count exceeds max_pad_count");

            // No fill_alters_reading check here: the padded content is truename() /
            // falsename(), not a number, so no fill character can make the field read
            // as a different value. Rejecting digits here would only break the ordinary
            // setfill('0') << setw(n) << boolalpha spelling.
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

        return insert_int(s, io, reinterpret_cast<std::uintptr_t>(v)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
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
            // Numeric form: read the field as a `long` and accept only 0/1.
            long l = -1;
            std::tie(success, beg) = extract_int(beg, end, io, l);

            if (l == 0 || l == 1) v = bool(l);
            else
            {
                // LWG 23 resolved that a field which does not fit the target
                // still stores a value rather than leaving it untouched: the
                // most positive representable one, which for bool is `true`.
                v = true;
                success = false;
            }
        }
        else
        {
            // `boolalpha` form: match the input against the two locale names.
            //
            // Both names race for the input. A name stays in the running only
            // while every character read so far is a prefix of it, and only
            // while it still has a character left to offer; extraction stops
            // the moment no name can take the next character, which is what
            // keeps the facet from consuming more than it needs.
            const std::basic_string<CharT>& fname = m_false_name;
            const std::basic_string<CharT>& tname = m_true_name;

            bool f_live = true;
            bool t_live = true;
            std::size_t n = 0;

            while (beg != end)
            {
                const bool f_open = f_live && n < fname.size();
                const bool t_open = t_live && n < tname.size();
                if (!f_open && !t_open)
                    break;

                const CharT c = *beg;
                const bool f_takes = f_open && fname[n] == c;
                const bool t_takes = t_open && tname[n] == c;
                if (!f_takes && !t_takes)
                    break;

                f_live = f_takes;
                t_live = t_takes;
                ++n;
                ++beg;
            }

            // A name won only if it is still live and came out exactly
            // exhausted; `n != 0` keeps an empty name from matching nothing.
            const bool got_false = f_live && n == fname.size() && n != 0;
            const bool got_true  = t_live && n == tname.size() && n != 0;

            // Exactly one name may win. Both winning means the two are
            // indistinguishable on this input; neither winning means nothing
            // matched. The standard asks for `false` plus a failure either way.
            v = got_true && !got_false;
            if (got_false == got_true)
                success = false;
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

        std::uintptr_t ul = 0;
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
        const std::size_t w = io.width();
        io.width(0);

        // Build the printf conversion spec, then let the C library render the
        // value into a narrow buffer; everything after this is localisation.
        std::size_t len = 0;
        std::array<char, 16> fbuf{};
        format_float(io.flags(), fbuf.data(), mod);

        const ios_defs::fmtflags fltfield = io.flags() & ios_defs::floatfield;

        // First guess at the buffer. Outside `fixed`, three times the decimal
        // digit count leaves room for sign, point, exponent and slack; `fixed`
        // is the notation that can run long, so it gets the full integer part.
        std::size_t cs_size = static_cast<std::size_t>(max_digits) * 3 + 32;
        if (fltfield == ios_defs::fixed)
            cs_size = static_cast<std::size_t>(std::numeric_limits<TValue>::max_exponent10) + static_cast<std::size_t>(prec) + 32;

        // Cap initial allocation to a reasonable size (e.g., 2048) to avoid huge initial pressure.
        if (cs_size > 2048) cs_size = 2048;

        std::vector<char> vec_cs(cs_size);

        {
            clocale_wrapper inter_locale("C");
            clocale_user guard(inter_locale);

            auto do_snprintf = [&](char* buf, std::size_t size) {
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
            len = static_cast<std::size_t>(n);

            // If buffer was too small, snprintf returns required length.
            if (len >= cs_size)
            {
                cs_size = len + 1;
                vec_cs.resize(cs_size);
                n = do_snprintf(vec_cs.data(), cs_size);
                if (n < 0)
                    throw stream_error("numeric::put fail: floating-point conversion failed");
                len = static_cast<std::size_t>(n);
            }
        }

        // len == 0 is also treated as failure: every well-formed numeric
        // print (including 0, "inf", "nan") emits at least one character,
        // and the downstream path assumes vec_ws(len) has at least one
        // element before dereferencing vec_ws.data().
        if (len == 0 || len >= cs_size)
            throw stream_error("numeric::put fail: floating-point conversion failed");

        const char* cs = vec_cs.data();

        // Widen into char_type, then apply the two things the C locale could
        // not know about: this facet's decimal point, and its grouping.
        std::vector<CharT> vec_ws(len);
        CharT* ws = vec_ws.data();
        m_ctype->widen_seq(cs, cs + len, ws);

        const char* p = std::find(cs, cs + len, '.');
        CharT* wp = nullptr;
        if (p != cs + len)
        {
            wp = ws + (p - cs);
            *wp = m_decimal_point;
        }

        // Only a real number gets grouped -- "inf", "nan" and exponent-only forms
        // like 2e20 must come through untouched. A decimal point settles it
        // outright; failing that, text too short to be one of those words is a
        // number, and so is text whose second and third characters are digits.
        const auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
        const bool numeric_text =
            wp || len < 3u || (is_digit(cs[1]) && is_digit(cs[2]));

        if (!m_grouping.empty() && numeric_text)
        {
            // Worst case one separator per digit, so twice the length is ample.
            std::vector<CharT> vec_ws2(len * 2);
            CharT* ws2 = vec_ws2.data();

            // A leading sign is not part of the number being grouped. Copy it
            // across on its own and hand the grouper only what follows, so the
            // digit count it works from never has to be unwound afterwards.
            const std::size_t sign_len = (cs[0] == '-' || cs[0] == '+') ? 1u : 0u;
            if (sign_len != 0)
                ws2[0] = ws[0];

            // LWG 282: grouping applies to the integer part only, so the split
            // happens here and the fraction is handed over to be copied through.
            const std::size_t digit_len = len - sign_len;
            const std::size_t int_len   = wp ? static_cast<std::size_t>(wp - (ws + sign_len))
                                             : digit_len;
            len = sign_len + group_float(m_grouping, m_thousands_sep, ws2 + sign_len,
                                         ws + sign_len, int_len, digit_len - int_len);
            std::swap(vec_ws, vec_ws2);
            ws = vec_ws.data();
        }

        // Pad.
        if (std::cmp_greater(w, len))
        {
            if (w - len > ios_defs::max_pad_count)
                throw stream_error("numeric put fail: fill count exceeds max_pad_count");

            std::vector<CharT> vec_ws3(w);
            CharT* ws3 = vec_ws3.data();
            bool startSign = (ws[0] == m_out_atoms[s_ominus]) || (ws[0] == m_out_atoms[s_oplus]);
            bool start0x = (ws[0] == m_out_atoms[s_odigits]) && (len > 1u) &&
                           ((ws[1] == m_out_atoms[s_ox]) || (ws[1] == m_out_atoms[s_oX]));
            const ios_defs::fmtflags adjust = io.flags() & ios_defs::adjustfield;
            pad(io.fill(), w, adjust, io.flags() & ios_defs::basefield,
                ws3, ws, len, startSign, start0x);

            std::swap(vec_ws, vec_ws3);
            ws = vec_ws.data();
        }

        // Nothing is left to decide; hand the finished field to the caller.
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
        const std::size_t w = io.width();
        io.width(0);

        const ios_defs::fmtflags basefield = flags & ios_defs::basefield;
        const bool dec = (basefield != ios_defs::oct && basefield != ios_defs::hex);

        auto u = static_cast<unsigned_type>(v);
        if constexpr (std::is_signed_v<TValue>)
        {
            if (dec && v < 0)
                u = -u;
        }

        // Even base two would need no more than one character per value bit. Two
        // fixed buffers therefore cover both raw and maximally grouped output while
        // keeping every ordinary integer conversion off the heap.
        constexpr std::size_t digit_capacity = std::numeric_limits<unsigned_type>::digits;
        std::array<char_type, digit_capacity> raw_digits{};
        char_type* const raw_end = raw_digits.data() + raw_digits.size();
        char_type* raw_begin = raw_end;

        if (dec)
        {
            do // NOLINT(cppcoreguidelines-avoid-do-while)
            {
                *--raw_begin = m_out_atoms[s_odigits + (u % 10)];
                u /= 10;
            } while (u != 0);
        }
        else
        {
            const unsigned shift = (basefield == ios_defs::oct) ? 3u : 4u;
            const unsigned_type mask = (unsigned_type{1} << shift) - 1;
            const int alphabet = (basefield == ios_defs::hex && (flags & ios_defs::uppercase))
                ? s_oudigits : s_odigits;
            do // NOLINT(cppcoreguidelines-avoid-do-while)
            {
                *--raw_begin = m_out_atoms[alphabet + static_cast<std::size_t>(u & mask)];
                u >>= shift;
            } while (u != 0);
        }

        const char_type* digits = raw_begin;
        auto digit_len = static_cast<std::size_t>(raw_end - raw_begin);
        std::array<char_type, digit_capacity * 2> grouped_digits{};

        if (!m_grouping.empty())
        {
            char_type* const grouped_end = FacetHelper::add_grouping(
                grouped_digits.data(), m_thousands_sep, m_grouping, raw_begin, raw_end);
            digits = grouped_digits.data();
            digit_len = static_cast<std::size_t>(grouped_end - digits);
        }

        std::array<char_type, 2> prefix{};
        std::size_t prefix_len = 0;
        if (dec)
        {
            if constexpr (std::is_signed_v<TValue>)
            {
                if (v < 0)
                    prefix[prefix_len++] = m_out_atoms[s_ominus];
                else if (flags & ios_defs::showpos)
                    prefix[prefix_len++] = m_out_atoms[s_oplus];
            }
        }
        else if (bool(flags & ios_defs::showbase) && v)
        {
            prefix[prefix_len++] = m_out_atoms[s_odigits];
            if (basefield == ios_defs::hex)
                prefix[prefix_len++] = m_out_atoms[s_ox + bool(flags & ios_defs::uppercase)];
        }

        const std::size_t len = prefix_len + digit_len;
        if (w <= len)
        {
            s = std::copy_n(prefix.data(), prefix_len, s);
            return std::copy_n(digits, digit_len, s);
        }

        const std::size_t pad_len = w - len;
        if (pad_len > ios_defs::max_pad_count)
            throw stream_error("numeric put fail: fill count exceeds max_pad_count");

        const bool start_sign = prefix_len == 1
            && (prefix[0] == m_out_atoms[s_ominus] || prefix[0] == m_out_atoms[s_oplus]);
        const bool start_0x = prefix_len == 2
            && prefix[0] == m_out_atoms[s_odigits]
            && (prefix[1] == m_out_atoms[s_ox] || prefix[1] == m_out_atoms[s_oX]);
        const ios_defs::fmtflags adjust = flags & ios_defs::adjustfield;
        const bool trails_value = adjust == ios_defs::left;
        const bool leads_digits = !trails_value
            && (adjust == ios_defs::internal || !(start_sign || start_0x));
        if (fill_alters_reading(io.fill(), basefield, leads_digits, trails_value,
                                prefix_len != 0 && prefix[0] == m_out_atoms[s_ominus]))
            throw stream_error("numeric put fail: fill would change the value the field reads as");

        if (trails_value)
        {
            s = std::copy_n(prefix.data(), prefix_len, s);
            s = std::copy_n(digits, digit_len, s);
            return std::fill_n(s, pad_len, io.fill());
        }

        if (adjust == ios_defs::internal && (start_sign || start_0x))
        {
            s = std::copy_n(prefix.data(), prefix_len, s);
            s = std::fill_n(s, pad_len, io.fill());
            return std::copy_n(digits, digit_len, s);
        }

        s = std::fill_n(s, pad_len, io.fill());
        s = std::copy_n(prefix.data(), prefix_len, s);
        return std::copy_n(digits, digit_len, s);
    }

    /**
     * @lang{ZH}
     * @brief 判断一段填充字符是否会改变人从该字段读到的数值。
     *
     * @par 为什么需要这个判断
     * 格式化的产物是给人看的。`fill` 只应起补齐字段宽度的作用，不应改变这段文本读起来
     * 是几。`setfill('9') << setw(6) << 42` 写出 `"999942"`，读者读到的是 999942 而不是
     * 42——这段文本本身就已经错了，与它能否被原样解析回来无关。因此判据是"人读到的数
     * 是否还是原来那个数"，命中即由调用方抛出 `stream_error`。
     *
     * @par 判据
     * 危险与否既取决于是哪个字符，也取决于它落在哪里：
     * - **digit**：仅当它是 `'0'` 且紧贴数字之前时安全（前导零按约定不改变读数）。
     *   其余数字在任何位置都危险；`'0'` 落在数值之后（`"42000000"` 读作 42000000）
     *   或落在符号 / 进制前缀之前（`"00000-42"` 读不成任何数）同样危险。
     *   **"是不是 digit"随 `basefield` 变**：十六进制下 `'a'`–`'f'` 与 `'A'`–`'F'` 也是数字，
     *   `setfill('f') << hex << setw(8) << 0xab` 写出的 `"ffffffab"` 读作 4294967211。
     *   两种大小写都要查，因为提取侧接受混合大小写，`uppercase` 是哪个值都挡不住另一半。
     *   其余进制仍是 `'0'`–`'9'`：八进制下 `'8'`/`'9'` 虽然不是合法八进制数字，人读起来
     *   仍是数字（`"88888842"` 读作八千多万），故照样算。
     * - **小数点**：向右与数字结合，故仅当位于数值之后时安全（`"42......"` 仍读作 42）；
     *   落在前面会被读进数值里（`"......42"` 读作 0.42）。
     * - **`'+'` / `'-'`**：落在数值之前时会被读成该数值的符号，故仅当它与数值本身的
     *   符号一致时才安全：`"++++42"` 读作 +42（正确），`"----42"` 读作 −42（而值是
     *   +42，错误）；对负值则正好相反。位于数值之后一律安全（`"42----"` 仍读作 42）。
     * - **其余字符**一律安全：
     *   - 千位分隔符必须左右都有数字才成立，而填充段靠数字的那一侧之外只可能是符号、
     *     进制前缀或字段边界，故它永远无法与数值结合（`"    12345"` 在以空格为千位
     *     分隔符的 locale 下仍读作 12345——把它算作危险会让这些 locale 的默认填充全部失败）。
     *   - `'x'`/`'X'`/`'e'`/`'E'` 需要其后跟数字才有意义，而"填充段后面跟数字"这一位置
     *     上任何digit填充都已被拒绝。
     * @endif
     *
     * @lang{EN}
     * @brief Decides whether a run of fill characters changes the number a human reads
     * out of the field.
     *
     * @par Why this test exists
     * Formatted output is meant to be read by people. `fill` is only supposed to pad a
     * field to its width, never to change what number the text says.
     * `setfill('9') << setw(6) << 42` writes `"999942"`, which a reader reads as 999942
     * rather than 42 — that text is already wrong, independently of whether it can be
     * parsed back. The criterion is therefore "does a human still read the same number",
     * and the caller throws `stream_error` on a hit.
     *
     * @par The criterion
     * Danger depends both on which character it is and on where it lands:
     * - **A digit**: safe only when it is `'0'` sitting immediately before the digits
     *   (leading zeros conventionally do not change the reading). Every other digit is
     *   dangerous everywhere; `'0'` is equally dangerous after the value
     *   (`"42000000"` reads as 42000000) or before a sign or base prefix
     *   (`"00000-42"` reads as no number at all).
     *   **What counts as a digit follows `basefield`**: in hex `'a'`–`'f'` and `'A'`–`'F'`
     *   are digits too, so `setfill('f') << hex << setw(8) << 0xab` would write
     *   `"ffffffab"`, which reads as 4294967211. Both cases are checked, because the
     *   extractor accepts mixed-case hex and so neither setting of `uppercase` rules the
     *   other half out. Every other base keeps `'0'`–`'9'`: `'8'` and `'9'` are not valid
     *   octal digits but still read as digits to a person (`"88888842"` reads as a number in
     *   the tens of millions), so they count under `oct` too.
     * - **The decimal point**: it binds rightwards to digits, so it is safe only after
     *   the value (`"42......"` still reads as 42); in front it is read into the value
     *   (`"......42"` reads as 0.42).
     * - **`'+'` / `'-'`**: in front of the value they are read as that value's sign, so
     *   they are safe only when they agree with the sign the value actually has:
     *   `"++++42"` reads as +42 (right), while `"----42"` reads as −42 although the
     *   value is +42 (wrong); for a negative value it is the other way round. After the
     *   value they are always safe (`"42----"` still reads as 42).
     * - **Everything else** is safe:
     *   - The thousands separator needs digits on both sides, and on the far side of a
     *     fill run there can only be a sign, a base prefix or the field edge, so it can
     *     never bind to the value (`"    12345"` still reads as 12345 in a locale whose
     *     separator is a space — treating it as dangerous would fail every default-fill
     *     output in such locales).
     *   - `'x'`/`'X'`/`'e'`/`'E'` are only meaningful with digits after them, and in the
     *     one position where digits do follow the run, any digit fill is already rejected.
     * @endif
     *
     * @param fill
     * @lang{ZH} 待判断的填充字符。 @endif
     * @lang{EN} The fill character to test. @endif
     *
     * @param basefield
     * @lang{ZH} 该数值写出时所用的进制，即 `flags() & basefield`。决定哪些字形算数字：
     * `hex` 为 16 个，其余一律 10 个。判据问的是"人读起来像不像数字"，不是"该进制收不收
     * 这个字符"，所以八进制下 `'8'`/`'9'` 照样算——`"88888842"` 会被读成八千多万。 @endif
     * @lang{EN} The base the value was written in, i.e. `flags() & basefield`. It decides
     * which glyphs count as digits: 16 under `hex`, 10 otherwise. The test asks whether a
     * character *looks* like a digit to a reader, not whether the base would accept it, so
     * `'8'` and `'9'` still count under `oct` -- `"88888842"` reads as a number in the
     * tens of millions. @endif
     *
     * @param leads_digits
     * @lang{ZH} 该段填充是否紧贴数字之前，中间不隔符号或进制前缀。 @endif
     * @lang{EN} Whether the run sits immediately before the digits, with no sign or
     * base prefix in between. @endif
     *
     * @param trails_value
     * @lang{ZH} 该段填充是否位于数值之后。与 `leads_digits` 互斥；两者皆为 `false`
     * 表示填充位于整个字段之前、与数字之间还隔着符号或进制前缀。 @endif
     * @lang{EN} Whether the run sits after the value. Mutually exclusive with
     * `leads_digits`; both `false` means the run is before the whole field, with a sign
     * or base prefix between it and the digits. @endif
     *
     * @param negative
     * @lang{ZH} 被格式化的数值本身是否为负。仅用于判断符号填充是否与它一致。 @endif
     * @lang{EN} Whether the value being formatted is itself negative. Used only to tell
     * whether a sign-shaped fill agrees with it. @endif
     *
     * @return
     * @lang{ZH} 若这段填充会改变人读到的数值则返回 `true`。 @endif
     * @lang{EN} `true` if the run changes the number a human reads. @endif
     */
    [[nodiscard]] bool fill_alters_reading(char_type fill, ios_defs::fmtflags basefield,
                                           bool leads_digits, bool trails_value,
                                           bool negative) const
    {
        const int radix = basefield == ios_defs::hex ? 16 : 10;
        const char_type* const digits  = m_out_atoms.data() + s_odigits;
        const char_type* const udigits = m_out_atoms.data() + s_oudigits;
        if (std::find(digits,  digits  + radix, fill) != digits  + radix
         || std::find(udigits, udigits + radix, fill) != udigits + radix)
            return !(fill == digits[0] && leads_digits);

        // Past the value nothing but a digit can still be read into it.
        if (trails_value)
            return false;

        if (fill == m_decimal_point)
            return true;
        if (fill == m_out_atoms[s_ominus])
            return !negative;
        if (fill == m_out_atoms[s_oplus])
            return negative;
        return false;
    }

    /**
     * @lang{ZH}
     * @brief 对格式化缓冲区应用宽度填充，委托给 `pad_impl`。同时将 `len` 更新为填充后的总宽度。
     *
     * @param fill 填充字符。
     * @param w 目标字段宽度。
     * @param adjust 对齐标志（left/right/internal）。
     * @param basefield 数值写出时所用的进制（`flags() & basefield`），转交
     *        `fill_alters_reading` 用于判断哪些字形算数字。
     * @param new_buf 输出缓冲区，容量不小于 `w`。
     * @param cs 原始格式化内容的起始指针。
     * @param len 原始内容的字符数；函数返回后更新为 `w`。
     * @param startSign 原始内容是否以符号字符（`+`/`-`）开头。
     * @param start0x 原始内容是否以 `0x`/`0X` 开头。
     * @endif
     *
     * @lang{EN}
     * @brief Applies width padding to the formatted buffer; delegates to `pad_impl`. Updates `len` to the padded total width.
     *
     * @param fill The fill character.
     * @param w The target field width.
     * @param adjust The alignment flag (left/right/internal).
     * @param basefield The base the value was written in (`flags() & basefield`), handed to
     *        `fill_alters_reading` to decide which glyphs count as digits.
     * @param new_buf Output buffer with capacity of at least `w`.
     * @param cs Pointer to the start of the original formatted content.
     * @param len Number of characters in the original content; updated to `w` on return.
     * @param startSign Whether the original content begins with a sign character (`+`/`-`).
     * @param start0x Whether the original content begins with `0x`/`0X`.
     *
     * @throw stream_error If `fill` would change the number the padded field reads as;
     * see `fill_alters_reading`.
     * @endif
     */
    void pad(char_type fill, std::size_t w, ios_defs::fmtflags adjust, // NOLINT(bugprone-easily-swappable-parameters)
             ios_defs::fmtflags basefield,
             char_type* new_buf, const char_type* cs, std::size_t& len,
             bool startSign, bool start0x) const
    {
      // Fill is about to be written, so this is where it gets vetted: a fill character
      // that would change the number this field reads as is rejected outright. Where
      // the run lands decides what is safe, and `pad_impl` below fixes that: `left`
      // appends it after the value; `internal`, and `right` on content that starts with
      // a digit, put it directly in front of the digits; `right` on content that starts
      // with a sign or `0x` leaves that prefix between the fill and the digits.
      // The check belongs here rather than at the top of the insert functions because
      // `fill` is sticky stream state: a stream carrying setfill('1') must keep working
      // for every output whose width leaves nothing to pad.
      const bool trails_value = (adjust == ios_defs::left);
      const bool leads_digits = !trails_value
          && (adjust == ios_defs::internal || !(startSign || start0x));
      if (fill_alters_reading(fill, basefield, leads_digits, trails_value,
                              cs[0] == m_out_atoms[s_ominus]))
          throw stream_error("numeric put fail: fill would change the value the field reads as");

      // The field is short of `w`, so the fill run gets placed.
      pad_impl(adjust, fill, new_buf, cs, w, len, startSign, start0x);
      len = w;
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
    void pad_impl(ios_defs::fmtflags adjust, char_type fill,
                   char_type* news, const char_type* olds,
                   std::size_t newlen, std::size_t oldlen,
                   bool startSign, bool start0x) const
    {

        const std::size_t plen = newlen - oldlen;

        if (adjust == ios_defs::left)
        {
            // Fill trails the value.
            std::copy_n(olds, oldlen, news);
            std::fill_n(news + oldlen, plen, fill);
            return;
        }

        // Every other adjustment puts the fill ahead of the digits, and the two
        // that remain differ only in what gets held back at the left edge:
        // `internal` pins whatever announces the number -- a sign, or a `0x` /
        // `0X` base marker -- and lets the fill slot in behind it, while `right`
        // pins nothing at all. A pinned run of length zero makes that one case,
        // so both are served by a single three-step write.
        std::size_t pinned = 0;
        if (adjust == ios_defs::internal)
        {
            // At most two characters, so they go across by hand: handing a
            // length of 0-2 to a range algorithm costs a memmove call that
            // dwarfs the copy itself, and `right` would pay it for nothing.
            if (startSign)
            {
                news[0] = olds[0];
                pinned = 1;
            }
            else if (start0x)
            {
                news[0] = olds[0];
                news[1] = olds[1];
                pinned = 2;
            }
        }

        std::fill_n(news + pinned, plen, fill);
        std::copy_n(olds + pinned, oldlen - pinned, news + pinned + plen);
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
    void format_float(ios_defs::fmtflags flags, char* fptr, char mod) const noexcept
    {
        // The notation picks the conversion letter, and with it the one thing
        // that is not a free choice: hexfloat carries no explicit precision,
        // because `%a` already prints exactly as many hex digits as the value
        // needs and pinning a precision on it would round the result away.
        // Every other notation takes its precision as a `*` argument, which is
        // what LWG 231 asked for -- unconditionally, rather than only under
        // `fixed` or a non-zero precision as an earlier reading had it.
        const ios_defs::fmtflags notation = flags & ios_defs::floatfield;
        const bool hexfloat = (notation == (ios_defs::fixed | ios_defs::scientific));

        char conv = 'g';
        if (notation == ios_defs::fixed)           conv = 'f';
        else if (notation == ios_defs::scientific) conv = 'e';
        else if (hexfloat)                         conv = 'a';

        // `f` has no upper-case spelling; the other three do, and they sit a
        // fixed case-distance apart in ASCII.
        if (conv != 'f' && (flags & ios_defs::uppercase))
            conv = static_cast<char>(conv - ('a' - 'A'));

        // Assemble in the order printf's own grammar fixes: introducer, flags,
        // precision, length modifier, conversion.
        *fptr++ = '%';
        if (flags & ios_defs::showpos)   *fptr++ = '+';
        if (flags & ios_defs::showpoint) *fptr++ = '#';
        if (!hexfloat)                 { *fptr++ = '.'; *fptr++ = '*'; }
        if (mod)                         *fptr++ = mod;
        *fptr++ = conv;
        *fptr   = '\0';
    }

    /**
     * @lang{ZH}
     * @brief 将已切分好的整数部分加上千位分隔符写入 `new_buf`，小数部分原样跟随。
     *
     * @param grouping 数字分组规则（内部规范化格式）。
     * @param sep 千位分隔符字符。
     * @param new_buf 输出缓冲区。
     * @param src 输入宽字符缓冲区的起始指针，整数部分在前、小数部分紧随其后。
     * @param int_len 整数部分的字符数。
     * @param frac_len 小数部分的字符数（含小数点），无小数部分时为 0。
     * @return 写入 `new_buf` 的字符总数。
     * @endif
     *
     * @lang{EN}
     * @brief Writes the already-split integer part into `new_buf` with grouping separators, followed by the fractional part verbatim.
     *
     * @param grouping Digit grouping rules (internal normalized form).
     * @param sep The thousands separator character.
     * @param new_buf Output buffer.
     * @param src Start of the input wide-character buffer: integer part first, fractional part immediately after.
     * @param int_len Character count of the integer part.
     * @param frac_len Character count of the fractional part (including the decimal point); 0 when there is none.
     * @return The total number of characters written to `new_buf`.
     * @endif
     */
    std::size_t group_float(const std::vector<uint8_t>& grouping, char_type sep,
                            char_type* new_buf, const char_type* src,
                            std::size_t int_len, std::size_t frac_len) const
    {
        char_type* const grouped_end =
            FacetHelper::add_grouping(new_buf, sep, grouping, src, src + int_len);
        std::copy(src + int_len, src + int_len + frac_len, grouped_end);
        return static_cast<std::size_t>(grouped_end - new_buf) + frac_len;
    }

    /**
     * @lang{ZH}
     * @brief 判断某个字符在本 locale 中是否担任标点角色（千位分隔符或小数点）。
     *
     * 两个提取器都要在多处问同一个问题：符号位、前导零扫描、数字累加循环、指数符号。
     * locale 完全可以指派一个同时也是符号或数字字形的字符来当分隔符或小数点；一旦如此，
     * **标点身份优先**。把判断收在一处，四个调用点就不会各自跑偏。
     *
     * `m_grouping` 为空时不启用分组，此时 `m_thousands_sep` 无意义，故只检查小数点。
     * @endif
     *
     * @lang{EN}
     * @brief Tests whether a character serves a punctuation role in this locale --
     * thousands separator or decimal point.
     *
     * Both extractors ask this in several places: at the sign, during the
     * leading-zero scan, inside the digit loop, and at the exponent sign. A locale
     * is free to nominate a character that also spells a sign or a digit, and where
     * it does, **the punctuation role is the one that counts**. Keeping the test in
     * one place stops the four call sites from drifting apart.
     *
     * An empty `m_grouping` means grouping is off, which makes `m_thousands_sep`
     * meaningless, so only the decimal point is considered.
     * @endif
     *
     * @param ch
     * @lang{ZH} 待判断的字符。 @endif
     * @lang{EN} The character to test. @endif
     *
     * @return
     * @lang{ZH} 若 `ch` 是生效的千位分隔符或小数点则为 `true`。 @endif
     * @lang{EN} `true` if `ch` is the active thousands separator or the decimal point. @endif
     */
    [[nodiscard]] bool is_punct(char_type ch) const noexcept
    {
        return (!m_grouping.empty() && ch == m_thousands_sep) || ch == m_decimal_point;
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

        // With no base set in the flags the text itself decides, so `base` is
        // provisional until the prefix scan below has run.
        const ios_defs::fmtflags basefield = io.flags() & ios_defs::basefield;
        int base = basefield == ios_defs::oct ? 8
                 : basefield == ios_defs::hex ? 16
                 : 10;

        bool at_end = beg == end;

        // -- sign ----------------------------------------------------------
        bool negative = false;
        if (!at_end)
        {
            c = *beg;
            negative = c == m_in_atoms[s_iminus];
            if ((negative || c == m_in_atoms[s_iplus]) && !is_punct(c))
            {
                if (++beg != end)
                    c = *beg;
                else
                    at_end = true;
            }
        }

        // -- leading zeros and base prefix ---------------------------------
        // Base ten is the only base that lets a run of zeros stand in front of
        // the number. Everywhere else a second zero is an ordinary digit and
        // belongs to the accumulation loop, so at most one is taken here.
        bool saw_zero  = false;
        int  group_len = 0;
        while (!at_end && !is_punct(c) && c == m_in_atoms[s_izero]
               && (!saw_zero || base == 10))
        {
            saw_zero = true;
            ++group_len;
            if (basefield == 0) base = 8;   // a bare leading `0` means octal
            if (base == 8) group_len = 0;   // that prefix zero starts no group
            if (++beg != end) c = *beg;
            else at_end = true;
        }

        // `0x` / `0X` behind that zero makes it a hexadecimal prefix -- either
        // confirming a base the flags already fixed, or choosing one when they
        // left it open. Under any other base the `x` is simply not ours, and is
        // left for the caller to trip over.
        if (saw_zero && !at_end && !is_punct(c)
            && (c == m_in_atoms[s_ix] || c == m_in_atoms[s_iX]))
        {
            if (basefield == 0) base = 16;
            if (base == 16)
            {
                saw_zero  = false;          // the `0` was prefix, not a digit
                group_len = 0;
                if (++beg != end) c = *beg;
                else at_end = true;
            }
        }

        // The base is settled now, and with it how much of the atom table is
        // in play: hexadecimal reaches across both letter alphabets, every
        // other base stops at its own digit count.
        const std::size_t span = (base == 16 ? s_iend - s_izero : base);

        // -- digits --------------------------------------------------------
        std::vector<uint8_t> groups_seen;
        if (!m_grouping.empty()) groups_seen.reserve(32);
        bool malformed = false;
        bool overflowed = false;
        const unsigned_type value_limit = (negative && std::is_signed_v<TValue>)
                                        ? -static_cast<unsigned_type>(std::numeric_limits<TValue>::min()) : std::numeric_limits<TValue>::max();
        const unsigned_type accum_limit = value_limit / static_cast<unsigned_type>(base);
        unsigned_type result = 0;
        const char_type* const digit_table = m_in_atoms.data() + s_izero;

        while (!at_end)
        {
            // Punctuation is tested before the digit table, because a locale
            // may well have nominated a character that appears in both.
            if (!m_grouping.empty() && c == m_thousands_sep)
            {
                // A separator has to close a group that actually holds digits,
                // which rules out a leading separator and two in a row alike.
                // Group widths are kept as bytes, so one too wide to record is
                // rejected rather than silently wrapped.
                if (group_len == 0
                    || std::cmp_greater(group_len, std::numeric_limits<uint8_t>::max()))
                {
                    malformed = true;
                    break;
                }
                groups_seen.push_back(static_cast<uint8_t>(group_len));
                group_len = 0;
            }
            else if (c == m_decimal_point)
                break;
            else
            {
                const char_type* q = std::find(digit_table, digit_table + span, c);
                if (q == digit_table + span)
                    break;

                // The table runs '0'-'9', then 'a'-'f', then 'A'-'F', so an
                // upper-case letter sits six slots past the value it denotes.
                int digit = static_cast<int>(q - digit_table);
                if (digit > 15) digit -= 6;

                // Once the running value has passed the point where another
                // digit could fit, accumulation stops but consumption does not:
                // the caller still has to be left past the whole field.
                if (result > accum_limit)
                    overflowed = true;
                else
                {
                    result *= static_cast<unsigned_type>(base);
                    overflowed |= result > value_limit - static_cast<unsigned_type>(digit);
                    result += static_cast<unsigned_type>(digit);
                    ++group_len;
                }
            }

            if (++beg != end) c = *beg;
            else at_end = true;
        }

        bool success = true;
        // Grouping was only ever recorded if the input actually used it; when
        // it did, the trailing group is closed off and the whole shape held up
        // against what the locale demands.
        if (!groups_seen.empty())
        {
            if (std::cmp_greater(group_len, std::numeric_limits<uint8_t>::max()))
            {
                malformed = true;
            }
            else
            {
                groups_seen.push_back(static_cast<uint8_t>(group_len));
                success = FacetHelper::verify_grouping(m_grouping, groups_seen);
            }
        }

        // LWG 23 (Num_get overflow result): when the field overflows,
        // v must be set to numeric_limits::max() / min() with failbit.
        //
        // `overflowed` is checked BEFORE `malformed` by design. Once the
        // digit-accumulation loop sets `overflowed`, ++group_len is skipped
        // for every subsequent digit, so group_len freezes. A later, otherwise
        // well-formed thousands_sep then fails the grouping check and sets
        // `malformed` — but that is a side effect of the overflow
        // short-circuit, not an independent structural error in the input.
        //
        // Letting `malformed` win in that overlap would map a structurally
        // valid, numerically out-of-range input (e.g. "12,345,678,901,234,567"
        // into uint32_t under grouping "\3") to v = 0 instead of v = max,
        // contradicting LWG 23. We diverge from libstdc++'s ordering here
        // (which has the same latent issue) to keep the overflow signal
        // dominant whenever it fires.
        if (overflowed)
        {
            if (negative && std::is_signed_v<TValue>)
                v = std::numeric_limits<TValue>::min();
            else
                v = std::numeric_limits<TValue>::max();
            success = false;
        }
        else if ((group_len == 0 && !saw_zero && groups_seen.empty()) || malformed)
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
        bool at_end = beg == end;

        std::vector<uint8_t> groups_seen;
        if (!m_grouping.empty())
            groups_seen.reserve(32);

        // A sign is recognised in two places -- in front of the mantissa and in
        // front of the exponent -- and behaves identically in both, so the test
        // and the transcription live here rather than being spelled twice.
        const auto take_sign = [&](char_type ch) -> bool
        {
            const bool plus = ch == m_in_atoms[s_iplus];
            if ((plus || ch == m_in_atoms[s_iminus]) && !is_punct(ch))
            {
                xtrc += plus ? '+' : '-';
                return true;
            }
            return false;
        };

        // Closing a group records how many digits it held. Widths are stored as
        // bytes, so one too wide to record abandons the field rather than
        // wrapping silently; the caller breaks out on a false return.
        int group_len = 0;
        const auto close_group = [&]() -> bool
        {
            if (std::cmp_greater(group_len, std::numeric_limits<uint8_t>::max()))
                return false;
            groups_seen.push_back(static_cast<uint8_t>(group_len));
            return true;
        };

        const auto advance = [&]()
        {
            if (++beg != end) c = *beg;
            else at_end = true;
        };

        // -- mantissa sign -------------------------------------------------
        if (!at_end)
        {
            c = *beg;
            if (take_sign(c))
                advance();
        }

        // -- leading zeros -------------------------------------------------
        // They all count towards the first group, but only one reaches `xtrc`:
        // strtod does not care how many were written, and keeping them would
        // just make the buffer longer.
        bool saw_digit = false;
        while (!at_end && !is_punct(c) && c == m_in_atoms[s_izero])
        {
            if (!saw_digit)
            {
                xtrc += '0';
                saw_digit = true;
            }
            ++group_len;
            advance();
        }

        // -- mantissa, decimal point and exponent --------------------------
        bool saw_point = false;
        bool saw_exp   = false;
        const char_type* const digit_table = m_in_atoms.data() + s_izero;

        while (!at_end)
        {
            // Punctuation is tested before the digit table, because a locale
            // may well have nominated a character that appears in both.
            if (!m_grouping.empty() && c == m_thousands_sep)
            {
                // Separators belong to the integer part alone; past a point or
                // an exponent one simply ends the field. A separator also has
                // to close a group that holds digits, which rules out a leading
                // separator and two in a row alike.
                if (saw_point || saw_exp)
                    break;
                if (group_len == 0 || !close_group())
                {
                    // Leaving `xtrc` empty is what tells convert_to_v to fail.
                    xtrc.clear();
                    break;
                }
                group_len = 0;
            }
            else if (c == m_decimal_point)
            {
                if (saw_point || saw_exp)
                    break;
                // A grouping check only happens if the input used grouping at
                // all, so the run of digits before the point is worth recording
                // only when some separator already opened the tally.
                if (!groups_seen.empty() && !close_group())
                {
                    xtrc.clear();
                    break;
                }
                xtrc += '.';
                saw_point = true;
            }
            else
            {
                const char_type* q = std::find(digit_table, digit_table + 10, c);
                if (q != digit_table + 10)
                {
                    xtrc += static_cast<char>('0' + (q - digit_table));
                    saw_digit = true;
                    ++group_len;
                }
                else if ((c == m_in_atoms[s_ie] || c == m_in_atoms[s_iE])
                        && !saw_exp && saw_digit)
                {
                    // The exponent ends the integer part too, so an open tally
                    // is closed here for the same reason as at a decimal point
                    // -- unless a point already did it.
                    if (!groups_seen.empty() && !saw_point && !close_group())
                    {
                        xtrc.clear();
                        break;
                    }
                    xtrc += 'e';
                    saw_exp = true;

                    if (++beg == end)
                    {
                        at_end = true;
                        break;
                    }
                    c = *beg;
                    if (!take_sign(c))
                        // No exponent sign after all, so `c` is already the next
                        // character to judge: go round again without advancing.
                        continue;
                }
                else
                    break;
            }

            advance();
        }

        bool success = true;
        // A grouping check only applies to input that actually used grouping.
        // When it did, the final group still has to be closed -- unless a point
        // or an exponent closed it already.
        if (!groups_seen.empty())
        {
            if (!saw_point && !saw_exp && !close_group())
                success = false;
            else
                success = FacetHelper::verify_grouping(m_grouping, groups_seen);
        }

        return std::pair(success, beg);
    }

    /**
     * @lang{ZH}
     * @brief 将 "C" locale 格式的 ASCII 浮点字符串转换为浮点值。
     *
     * 在 "C" locale 守卫下调用 `strtof`/`strtod`/`strtold`。
     * 按 LWG 23 处理特殊情况：无穷大映射为 `numeric_limits::max()` 的有限极值并返回失败；
     * 转换失败（`parse_end == s` 或字符串未完全消耗）时将 `v` 设为 0 并返回失败；
     * NaN 作为完整、非无穷的转换结果原样保留。
     *
     * @tparam TValue 浮点类型（`float`、`double` 或 `long double`）。
     * @param s 以 `'\0'` 结尾的 "C" locale ASCII 浮点字符串。
     * @param v 转换成功后写入结果的浮点数引用。
     * @return 若字符串被完整转换且结果不是无穷大则返回 `true`（包括 NaN），否则返回 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Converts a "C"-locale ASCII floating-point string to a floating-point value.
     *
     * Calls `strtof`/`strtod`/`strtold` under a "C" locale guard.
     * Handles special cases per LWG 23: infinity is mapped to the finite extreme value
     * `numeric_limits::max()` and failure is returned; conversion failure (when
     * `parse_end == s` or the string is not fully consumed) sets `v` to 0 and returns
     * failure; a fully converted NaN is retained as a successful result.
     *
     * @tparam TValue The floating-point type (`float`, `double`, or `long double`).
     * @param s Null-terminated "C"-locale ASCII floating-point string.
     * @param v Reference to the floating-point variable to receive the converted result.
     * @return `true` if the whole string converted and the result is not infinity
     * (including NaN); `false` otherwise.
     * @endif
     */
    template <typename TValue>
    bool convert_to_v(const char* s, TValue& v) const
    {
        TValue parsed{};
        char* parse_end = nullptr;

        {
            clocale_wrapper inter_locale("C");
            clocale_user guard(inter_locale);

            if constexpr (std::is_same_v<TValue, float>)
                parsed = strtof(s, &parse_end);
            else if constexpr (std::is_same_v<TValue, double>)
                parsed = strtod(s, &parse_end);
            else
                parsed = strtold(s, &parse_end);
        }

        // Decide the semantic result before committing it to the caller. A malformed
        // field becomes zero, overflow becomes the matching finite extreme, and NaN
        // remains a successful conversion just as it was before this refactoring.
        if (parse_end == s || *parse_end != '\0')
        {
            v = TValue(0);
            return false;
        }
        if (std::isinf(parsed))
        {
            const TValue limit = std::numeric_limits<TValue>::max();
            v = std::signbit(parsed) ? -limit : limit;
            return false;
        }

        v = parsed;
        return true;
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
    static bool atoms_pairwise_distinct(const CharT* p, std::size_t n)
    {
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j)
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
