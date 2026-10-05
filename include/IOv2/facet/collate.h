// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file collate.h
 * @lang{ZH}
 * 定义了 `collate<CharT>` facet 类，提供基于 locale 的字符串比较与排序键变换功能。
 * 该类封装了 `collate_conf<CharT>` 的共享实例，接口以迭代器范围为输入，
 * 负责按空字符分段，底层 `collate_conf` 只处理以空字符结尾的单个字符串。
 * @endif
 *
 * @lang{EN}
 * Defines the `collate<CharT>` facet class, providing locale-aware string comparison
 * and collation-key transformation. The class wraps a shared instance of
 * `collate_conf<CharT>` and takes iterator ranges as input. It splits the input
 * at null characters; the underlying `collate_conf` only handles single
 * null-terminated strings.
 * @endif
 */
#pragma once
#include <IOv2/common/defs.h>
#include <IOv2/common/metafunctions.h>
#include <IOv2/facet/collate_details.h>
#include <IOv2/facet/facet_common.h>

#include <algorithm>
#include <compare>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 基于 locale 的字符串排序 facet。
 *
 * 封装 `collate_conf<CharT>` 的共享实例，提供以迭代器范围为输入的
 * `compare()`、`transform_length()` 和 `transform()` 接口。
 * 字符序列以空字符（`\0`）为段分隔符，各段依次交给 `collate_conf` 处理。
 * 指针或连续迭代器输入中以空字符结束的段不做拷贝。
 *
 * @tparam CharT 字符类型，支持 `char`、`wchar_t`、`char8_t` 和 `char32_t`（仅限 UTF-32 平台）。
 * @endif
 *
 * @lang{EN}
 * @brief A locale-aware string collation facet.
 *
 * Wraps a shared instance of `collate_conf<CharT>` and exposes `compare()`,
 * `transform_length()`, and `transform()` interfaces that take iterator ranges.
 * Character sequences are split at null characters (`\0`), and each segment is
 * passed to `collate_conf` in turn. Segments of a pointer or contiguous-iterator
 * input that end at a null character are not copied.
 *
 * @tparam CharT The character type. Supports `char`, `wchar_t`, `char8_t`, and
 *               `char32_t` (only on platforms where `wchar_t` is UTF-32).
 * @endif
 */
template <typename CharT>
class collate
{
public:
    /**
     * @lang{ZH}
     * @brief 关联的配置对象创建规则类型。
     * @endif
     *
     * @lang{EN}
     * @brief The creation-rule type for the associated configuration object.
     * @endif
     */
    using create_rules = facet_create_rule<collate_conf<CharT>>;

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
     * @brief 构造函数，绑定到指定的 `collate_conf` 配置对象。
     *
     * @tparam TConfPtr 满足 `shared_ptr_to<collate_conf<CharT>>` 约束的共享指针类型。
     * @param p_obj 指向配置对象的共享指针，不得为空。
     * @throw stream_error 若 `p_obj` 为空。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that binds to the specified `collate_conf` configuration object.
     *
     * @tparam TConfPtr A shared pointer type satisfying the `shared_ptr_to<collate_conf<CharT>>` constraint.
     * @param p_obj A shared pointer to the configuration object; must not be null.
     * @throw stream_error If `p_obj` is null.
     * @endif
     */
    template <shared_ptr_to<collate_conf<CharT>> TConfPtr>
    collate(TConfPtr p_obj)
        : m_obj(p_obj)
    { if (!m_obj) throw stream_error("shared_ptr is empty"); }

public:
    /**
     * @lang{ZH}
     * @brief 比较两个字符序列的排列顺序。
     *
     * 两个序列各自按空字符分段，逐段交给底层 `collate_conf::compare()` 比较，
     * 遇到不等价的段即返回。两段等价时：若一方在输入中以空字符结束而另一方没有，
     * 没有的一方（即其输入的最后一段）较小，例如 `"abc"` < `"abc\0"`；
     * 若两段都以空字符结束，则继续比较下一段。
     * 全部比较完后，尚有剩余输入的一方较大。
     *
     * 指针或连续迭代器输入中以空字符结束的段不做拷贝，其余段会拷贝到暂存区。
     *
     * @tparam TIter1 第一个序列的输入迭代器类型。
     * @tparam TIter2 第二个序列的输入迭代器类型。
     * @param low1 第一个序列的起始迭代器。
     * @param high1 第一个序列的结束迭代器。
     * @param low2 第二个序列的起始迭代器。
     * @param high2 第二个序列的结束迭代器。
     * @return 表示两序列排列关系的 `std::weak_ordering` 值。
     * @throw stream_error 若底层比较失败。
     * @endif
     *
     * @lang{EN}
     * @brief Compares the collation order of two character sequences.
     *
     * Each sequence is split at null characters, and the segments are compared
     * pairwise by the underlying `collate_conf::compare()`, returning at the
     * first pair that is not equivalent. When two segments are equivalent and
     * only one of them ended at a null character in its input, the other one
     * (the last segment of its input) is less, e.g. `"abc"` < `"abc\0"`; when
     * both ended at a null character, comparison moves on to the next segment.
     * Once one input is exhausted, the side with input remaining is greater.
     *
     * Segments of a pointer or contiguous-iterator input that end at a null
     * character are not copied; all other segments are copied to a staging buffer.
     *
     * @tparam TIter1 The input iterator type of the first sequence.
     * @tparam TIter2 The input iterator type of the second sequence.
     * @param low1 Start iterator of the first sequence.
     * @param high1 End iterator of the first sequence.
     * @param low2 Start iterator of the second sequence.
     * @param high2 End iterator of the second sequence.
     * @return A `std::weak_ordering` value indicating the collation relationship.
     * @throw stream_error If the underlying comparison fails.
     * @endif
     */
    template <std::input_iterator TIter1, std::input_iterator TIter2>
    [[nodiscard]] std::weak_ordering compare(TIter1 low1, TIter1 high1, TIter2 low2, TIter2 high2) const
    {
        std::vector<CharT> buf1;
        std::vector<CharT> buf2;

        while ((low1 != high1) && (low2 != high2))
        {
            auto [seg1, null1] = next_segment(low1, high1, buf1);
            auto [seg2, null2] = next_segment(low2, high2, buf2);

            if (auto res = m_obj->compare(seg1, seg2); res != 0)
                return res;
            // The side whose segment did not end at a null is out of input, so it is less.
            if (null1 != null2)
                return null1 <=> null2;
        }

        if (low1 != high1) return std::weak_ordering::greater;
        if (low2 != high2) return std::weak_ordering::less;
        return std::weak_ordering::equivalent;
    }

    /**
     * @lang{ZH}
     * @brief 计算字符序列的排序键长度。
     *
     * 序列按空字符分段，对每段调用底层 `collate_conf::transform_length()` 并累加；
     * 在输入中以空字符结束的段，其排序键后跟一个空字符分隔符，额外计 1。
     * 返回值即 `transform()` 写出完整排序键所需的字符数（不含任何额外的结尾空字符）。
     *
     * @tparam TIter 输入迭代器类型。
     * @param low 字符序列的起始迭代器。
     * @param high 字符序列的结束迭代器。
     * @return 完整排序键的字符数。
     * @throw stream_error 若底层变换失败。
     * @endif
     *
     * @lang{EN}
     * @brief Computes the collation-key length of a character sequence.
     *
     * The sequence is split at null characters; the underlying
     * `collate_conf::transform_length()` is called on each segment and the
     * results are summed. A segment that ended at a null character in the input
     * is followed in the key by a null separator, which counts 1 more.
     * The result is the number of characters `transform()` writes for the
     * complete key (with no extra terminating null).
     *
     * @tparam TIter The input iterator type.
     * @param low Start iterator of the character sequence.
     * @param high End iterator of the character sequence.
     * @return The number of characters in the complete collation key.
     * @throw stream_error If the underlying transformation fails.
     * @endif
     */
    template <std::input_iterator TIter>
    [[nodiscard]] std::size_t transform_length(TIter low, TIter high) const
    {
        std::size_t res = 0;
        std::vector<CharT> buf;

        while (low != high)
        {
            auto [seg, ended_by_null] = next_segment(low, high, buf);
            res += m_obj->transform_length(seg);
            if (ended_by_null)
                ++res;
        }
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 将字符序列变换为排序键，写入 `dest`，不限长度。
     *
     * 等价于以无限容量调用 `transform(low, high, dest, n)`，写出完整排序键。
     *
     * @tparam TIter 输入迭代器类型。
     * @tparam TOut 接受 `CharT` 的输出迭代器类型。
     * @param low 字符序列的起始迭代器。
     * @param high 字符序列的结束迭代器。
     * @param dest 写入排序键的目标。
     * @return 写入后的目标位置，以及写入的字符数。
     * @throw stream_error 若底层变换失败。
     * @endif
     *
     * @lang{EN}
     * @brief Transforms a character sequence into a collation key written to `dest`, with no length limit.
     *
     * Equivalent to calling `transform(low, high, dest, n)` with unlimited
     * capacity, writing the complete collation key.
     *
     * @tparam TIter The input iterator type.
     * @tparam TOut An output iterator type accepting `CharT`.
     * @param low Start iterator of the character sequence.
     * @param high End iterator of the character sequence.
     * @param dest Destination for the collation key.
     * @return The destination position after writing, and the number of characters written.
     * @throw stream_error If the underlying transformation fails.
     * @endif
     */
    template <std::input_iterator TIter, std::output_iterator<CharT> TOut>
    std::pair<TOut, std::size_t> transform(TIter low, TIter high, TOut dest) const
    {
        return transform(low, high, dest, std::numeric_limits<std::size_t>::max());
    }

    /**
     * @lang{ZH}
     * @brief 将字符序列变换为排序键，最多向 `dest` 写入 `n` 个字符。
     *
     * 序列按空字符分段，每段经底层 `collate_conf::transform()` 变换后写出；
     * 在输入中以空字符结束的段，其排序键后跟一个空字符分隔符。完整排序键
     * 不含额外的结尾空字符，其长度等于 `transform_length(low, high)`。
     * 对完整排序键按字典序比较，结果与 `compare()` 一致。
     *
     * 写满 `n` 个字符即停止，其余输入不再处理。写出的是完整排序键的前缀；
     * 所需的完整长度由 `transform_length()` 给出。
     *
     * @tparam TIter 输入迭代器类型。
     * @tparam TOut 接受 `CharT` 的输出迭代器类型。
     * @param low 字符序列的起始迭代器。
     * @param high 字符序列的结束迭代器。
     * @param dest 写入排序键的目标。
     * @param n 最多写入的字符数，须大于 0。
     * @return 写入后的目标位置，以及实际写入的字符数。
     * @throw stream_error 若 `n` 为 0，或底层变换失败。
     * @endif
     *
     * @lang{EN}
     * @brief Transforms a character sequence into a collation key, writing at most `n` characters to `dest`.
     *
     * The sequence is split at null characters and each segment is transformed by
     * the underlying `collate_conf::transform()`. A segment that ended at a null
     * character in the input is followed in the key by a null separator. The
     * complete key has no extra terminating null, and its length equals
     * `transform_length(low, high)`. Comparing complete keys lexicographically
     * agrees with `compare()`.
     *
     * Writing stops once `n` characters are written, and the rest of the input
     * is not processed. What is written is a prefix of the complete key; the
     * length of the complete key is given by `transform_length()`.
     *
     * @tparam TIter The input iterator type.
     * @tparam TOut An output iterator type accepting `CharT`.
     * @param low Start iterator of the character sequence.
     * @param high End iterator of the character sequence.
     * @param dest Destination for the collation key.
     * @param n Maximum number of characters to write; must be greater than 0.
     * @return The destination position after writing, and the number of characters written.
     * @throw stream_error If `n` is 0, or the underlying transformation fails.
     * @endif
     */
    template <std::input_iterator TIter, std::output_iterator<CharT> TOut>
    std::pair<TOut, std::size_t> transform(TIter low, TIter high, TOut dest, std::size_t n) const
    {
        // 0 used to mean "unlimited"; reject it so old-style calls fail loudly.
        if (n == 0)
            throw stream_error("collate::transform: n must be greater than 0");

        std::size_t written = 0;
        std::vector<CharT> buf;     // input staging for next_segment
        std::vector<CharT> key;     // one segment's key, plus the '\0' strxfrm appends

        while ((low != high) && (written < n))
        {
            auto [seg, ended_by_null] = next_segment(low, high, buf);
            std::size_t len = m_obj->transform_length(seg);
            key.resize(len + 1);
            m_obj->transform(seg, key.data(), key.size());

            std::size_t cnt = std::min(len, n - written);
            dest = std::copy_n(key.data(), cnt, dest);
            written += cnt;

            if (ended_by_null && (written < n))
            {
                *dest++ = static_cast<CharT>(0);
                ++written;
            }
        }
        return {dest, written};
    }


private:
    /**
     * @lang{ZH}
     * @brief 从输入范围取出下一段，返回以空字符结尾的段首指针。
     *
     * 段以输入中的空字符（被消耗）结束，或以 `high` 结束。
     *
     * 若 `TIter` 是元素类型为 `CharT` 的连续迭代器（如指针、`std::basic_string`
     * 的迭代器），且段以空字符结束，返回的指针直接指向输入本身（该空字符即为结尾），
     * 不做拷贝。其余情况将段拷贝到 `buf`，补上结尾空字符及 `SIMD_PADDING_BYTES`
     * 的填充后返回 `buf.data()`。
     *
     * @tparam TIter 输入迭代器类型。
     * @param low 当前段的起始迭代器，返回时前移到下一段的起始位置。
     * @param high 输入范围的结束迭代器，须满足 `low != high`。
     * @param buf 暂存区；返回的指针可能指向其中，在下次调用前有效。
     * @return 段首指针，以及该段在输入中是否以空字符结束。
     * @endif
     *
     * @lang{EN}
     * @brief Takes the next segment from an input range and returns it as a null-terminated string.
     *
     * A segment ends at a null character in the input (which is consumed) or at `high`.
     *
     * If `TIter` is a contiguous iterator over `CharT` (e.g. a pointer or a
     * `std::basic_string` iterator) and the segment ends at a null character, the
     * returned pointer points into the input itself (that null is the terminator)
     * and nothing is copied. Otherwise the segment is copied into `buf`, followed by
     * a terminating null and `SIMD_PADDING_BYTES` of padding, and `buf.data()` is returned.
     *
     * @tparam TIter The input iterator type.
     * @param low Start of the current segment; advanced to the start of the next segment on return.
     * @param high End iterator of the input range; requires `low != high`.
     * @param buf Staging buffer; the returned pointer may point into it and stays
     *            valid until the next call.
     * @return The segment pointer, and whether the segment ended at a null character in the input.
     * @endif
     */
    template <typename TIter>
    static std::pair<const CharT*, bool> next_segment(TIter& low, TIter high, std::vector<CharT>& buf)
    {
        // Terminating null, then zero padding so glibc's 32-byte SIMD reads stay inside buf.
        auto terminate_and_pad = [&buf]
        {
            buf.resize(buf.size() + 1 + SIMD_PADDING_BYTES / sizeof(CharT), static_cast<CharT>(0));
        };

        if constexpr (std::contiguous_iterator<TIter> &&
                      std::is_same_v<std::iter_value_t<TIter>, CharT>)
        {
            const CharT* first = std::to_address(low);
            const CharT* last = first + (high - low);
            if (const CharT* eos = std::find(first, last, static_cast<CharT>(0)); eos != last)
            {
                low += (eos - first) + 1;
                return {first, true};
            }
            buf.assign(first, last);
            low = high;
            terminate_and_pad();
            return {buf.data(), false};
        }
        else
        {
            buf.clear();
            bool ended_by_null = false;
            while (low != high)
            {
                CharT ch = *low++;
                if (ch == static_cast<CharT>(0))
                {
                    ended_by_null = true;
                    break;
                }
                buf.push_back(ch);
            }
            terminate_and_pad();
            return {buf.data(), ended_by_null};
        }
    }

private:
    /**
     * @lang{ZH}
     * @brief 指向配置对象的共享只读指针，驱动所有比较与变换操作。
     * @endif
     *
     * @lang{EN}
     * @brief Shared read-only pointer to the configuration object driving all comparison and transformation operations.
     * @endif
     */
    std::shared_ptr<const collate_conf<CharT>> m_obj;
};

template<typename TConfPtr>
collate(TConfPtr) -> collate<typename TConfPtr::element_type::char_type>;
}
