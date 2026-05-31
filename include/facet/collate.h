/**
 * @file collate.h
 * @lang{ZH}
 * 定义了 `collate<CharT>` facet 类，提供基于 locale 的字符串比较与排序键变换功能。
 * 该类封装了 `collate_conf<CharT>` 的共享实例，并通过多个重载接口同时支持
 * 原始指针范围和泛型迭代器范围两种输入输出形式。
 * @endif
 *
 * @lang{EN}
 * Defines the `collate<CharT>` facet class, providing locale-aware string comparison
 * and collation-key transformation. The class wraps a shared instance of
 * `collate_conf<CharT>` and exposes multiple overloads that accept both raw-pointer
 * ranges and generic-iterator ranges as input and output.
 * @endif
 */
#pragma once
#include <common/metafunctions.h>
#include <facet/collate_details.h>
#include <facet/facet_common.h>

#include <algorithm>
#include <compare>
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
 * 封装 `collate_conf<CharT>` 的共享实例，提供对原始指针范围和泛型迭代器范围
 * 均适用的 `compare()`、`transform_length()` 和 `transform()` 接口。
 * 字符序列以空字符（`\0`）为段分隔符，各段独立处理。
 *
 * @tparam CharT 字符类型，支持 `char`、`wchar_t`、`char8_t` 和 `char32_t`（仅限 UTF-32 平台）。
 * @endif
 *
 * @lang{EN}
 * @brief A locale-aware string collation facet.
 *
 * Wraps a shared instance of `collate_conf<CharT>` and exposes `compare()`,
 * `transform_length()`, and `transform()` interfaces that accept both raw-pointer
 * ranges and generic-iterator ranges. Character sequences are processed segment
 * by segment, with null characters (`\0`) acting as segment delimiters.
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
     * @throw std::runtime_error 若 `p_obj` 为空。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that binds to the specified `collate_conf` configuration object.
     *
     * @tparam TConfPtr A shared pointer type satisfying the `shared_ptr_to<collate_conf<CharT>>` constraint.
     * @param p_obj A shared pointer to the configuration object; must not be null.
     * @throw std::runtime_error If `p_obj` is null.
     * @endif
     */
    template <shared_ptr_to<collate_conf<CharT>> TConfPtr>
    collate(TConfPtr p_obj)
        : m_obj(p_obj)
    { if (!m_obj) throw std::runtime_error("shared_ptr is empty"); }

public:
    /**
     * @lang{ZH}
     * @brief 比较两个原始指针范围所表示的字符序列的排列顺序。
     *
     * 直接委托给底层 `collate_conf` 实现。
     *
     * @param low1 第一个序列的起始指针。
     * @param high1 第一个序列的结束指针（不包含）。
     * @param low2 第二个序列的起始指针。
     * @param high2 第二个序列的结束指针（不包含）。
     * @return 表示两序列排列关系的 `std::strong_ordering` 值。
     * @endif
     *
     * @lang{EN}
     * @brief Compares the collation order of two character sequences represented as raw pointer ranges.
     *
     * Delegates directly to the underlying `collate_conf` implementation.
     *
     * @param low1 Pointer to the start of the first sequence.
     * @param high1 Pointer to one past the end of the first sequence.
     * @param low2 Pointer to the start of the second sequence.
     * @param high2 Pointer to one past the end of the second sequence.
     * @return A `std::strong_ordering` value indicating the collation relationship.
     * @endif
     */
    std::strong_ordering compare(const CharT* low1, const CharT* high1, const CharT* low2, const CharT* high2) const
    {
        return m_obj->compare(low1, high1, low2, high2);
    }

    /**
     * @lang{ZH}
     * @brief 比较一个原始指针范围与一个泛型迭代器范围的排列顺序。
     *
     * 将迭代器一侧逐段物化到缓冲区后，与指针一侧逐段进行比较。
     *
     * @tparam TIter 不可隐式转换为 `const CharT*` 的迭代器类型。
     * @param low1 第一个序列（指针范围）的起始指针。
     * @param high1 第一个序列（指针范围）的结束指针（不包含）。
     * @param low2 第二个序列（迭代器范围）的起始迭代器。
     * @param high2 第二个序列（迭代器范围）的结束迭代器。
     * @return 表示两序列排列关系的 `std::strong_ordering` 值。
     * @endif
     *
     * @lang{EN}
     * @brief Compares the collation order of a raw-pointer range against a generic-iterator range.
     *
     * The iterator side is materialized segment by segment into a buffer before
     * comparison against the pointer side.
     *
     * @tparam TIter An iterator type not implicitly convertible to `const CharT*`.
     * @param low1 Start pointer of the first (pointer) range.
     * @param high1 One-past-the-end pointer of the first (pointer) range.
     * @param low2 Start iterator of the second (iterator) range.
     * @param high2 End iterator of the second (iterator) range.
     * @return A `std::strong_ordering` value indicating the collation relationship.
     * @endif
     */
    template <typename TIter>
        requires (!std::is_convertible_v<TIter, const CharT*>)
    std::strong_ordering compare(const CharT* low1, const CharT* high1, TIter low2, TIter high2) const
    {
        std::vector<CharT> buf2; buf2.reserve(64);

        while ((low1 != high1) && (low2 != high2))
        {
            std::strong_ordering c_res = std::strong_ordering::equal;
            low2 = data_to_vec(low2, high2, buf2);

            const CharT* cl1 = low1;
            auto ch1 = std::find(low1, high1, static_cast<CharT>(0));
            if (ch1 == high1)
            {
                c_res = m_obj->compare(cl1, ch1, buf2.data(), buf2.data() + buf2.size());
                low1 = high1;
            }
            else
            {
                low1 = ch1 + 1;
                c_res = m_obj->compare(cl1, low1, buf2.data(), buf2.data() + buf2.size());
            }

            if (c_res != std::strong_ordering::equal)
                return c_res;
        }

        if (low1 != high1) return std::strong_ordering::greater;
        if (low2 != high2) return std::strong_ordering::less;
        return std::strong_ordering::equal;
    }

    /**
     * @lang{ZH}
     * @brief 比较一个泛型迭代器范围与一个原始指针范围的排列顺序。
     *
     * 通过交换参数顺序委托给指针/迭代器重载，并将结果取反以还原正确的大小关系。
     *
     * @tparam TIter 不可隐式转换为 `const CharT*` 的迭代器类型。
     * @param low1 第一个序列（迭代器范围）的起始迭代器。
     * @param high1 第一个序列（迭代器范围）的结束迭代器。
     * @param low2 第二个序列（指针范围）的起始指针。
     * @param high2 第二个序列（指针范围）的结束指针（不包含）。
     * @return 表示两序列排列关系的 `std::strong_ordering` 值。
     * @endif
     *
     * @lang{EN}
     * @brief Compares the collation order of a generic-iterator range against a raw-pointer range.
     *
     * Delegates to the pointer/iterator overload with swapped arguments and inverts
     * the result to restore the correct ordering relationship.
     *
     * @tparam TIter An iterator type not implicitly convertible to `const CharT*`.
     * @param low1 Start iterator of the first (iterator) range.
     * @param high1 End iterator of the first (iterator) range.
     * @param low2 Start pointer of the second (pointer) range.
     * @param high2 One-past-the-end pointer of the second (pointer) range.
     * @return A `std::strong_ordering` value indicating the collation relationship.
     * @endif
     */
    template <typename TIter>
        requires (!std::is_convertible_v<TIter, const CharT*>)
    std::strong_ordering compare(TIter low1, TIter high1, const CharT* low2, const CharT* high2) const
    {
        auto res = this->compare(low2, high2, low1, high1);
        if (res == std::strong_ordering::greater) return std::strong_ordering::less;
        if (res == std::strong_ordering::less) return std::strong_ordering::greater;
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 比较两个泛型迭代器范围的排列顺序。
     *
     * 两侧均逐段物化到各自的缓冲区后再进行比较。
     *
     * @tparam TIter1 第一个范围的迭代器类型，不可隐式转换为 `const CharT*`。
     * @tparam TIter2 第二个范围的迭代器类型，不可隐式转换为 `const CharT*`。
     * @param low1 第一个序列的起始迭代器。
     * @param high1 第一个序列的结束迭代器。
     * @param low2 第二个序列的起始迭代器。
     * @param high2 第二个序列的结束迭代器。
     * @return 表示两序列排列关系的 `std::strong_ordering` 值。
     * @endif
     *
     * @lang{EN}
     * @brief Compares the collation order of two generic-iterator ranges.
     *
     * Both sides are materialized segment by segment into their own buffers
     * before comparison.
     *
     * @tparam TIter1 The iterator type for the first range; not implicitly convertible to `const CharT*`.
     * @tparam TIter2 The iterator type for the second range; not implicitly convertible to `const CharT*`.
     * @param low1 Start iterator of the first sequence.
     * @param high1 End iterator of the first sequence.
     * @param low2 Start iterator of the second sequence.
     * @param high2 End iterator of the second sequence.
     * @return A `std::strong_ordering` value indicating the collation relationship.
     * @endif
     */
    template <typename TIter1, typename TIter2>
        requires (!(std::is_convertible_v<TIter1, const CharT*> || std::is_convertible_v<TIter2, const CharT*>))
    std::strong_ordering compare(TIter1 low1, TIter1 high1, TIter2 low2, TIter2 high2) const
    {
        std::vector<CharT> buf1; buf1.reserve(64);
        std::vector<CharT> buf2; buf2.reserve(64);

        while ((low1 != high1) && (low2 != high2))
        {
            low1 = data_to_vec(low1, high1, buf1);
            low2 = data_to_vec(low2, high2, buf2);

            auto c_res = m_obj->compare(buf1.data(), buf1.data() + buf1.size(), buf2.data(), buf2.data() + buf2.size());

            if (c_res != std::strong_ordering::equal)
                return c_res;
        }

        if (low1 != high1) return std::strong_ordering::greater;
        if (low2 != high2) return std::strong_ordering::less;
        return std::strong_ordering::equal;
    }

    /**
     * @lang{ZH}
     * @brief 计算原始指针范围的排序键变换所需的存储长度。
     *
     * 直接委托给底层 `collate_conf` 实现。
     *
     * @param low 字符序列的起始指针。
     * @param high 字符序列的结束指针（不包含）。
     * @return 存储完整排序键所需的字符数。
     * @endif
     *
     * @lang{EN}
     * @brief Computes the storage length required to transform the raw-pointer range into a collation key.
     *
     * Delegates directly to the underlying `collate_conf` implementation.
     *
     * @param low Pointer to the start of the character sequence.
     * @param high Pointer to one past the end of the character sequence.
     * @return The number of characters required to store the complete collation key.
     * @endif
     */
    size_t transform_length(const CharT* low, const CharT* high) const
    {
        return m_obj->transform_length(low, high);
    }

    /**
     * @lang{ZH}
     * @brief 计算泛型迭代器范围的排序键变换所需的存储长度。
     *
     * 将输入逐段物化到缓冲区后，累加各段的变换长度并返回总和。
     *
     * @tparam TIter 不可隐式转换为 `const CharT*` 的迭代器类型。
     * @param low 字符序列的起始迭代器。
     * @param high 字符序列的结束迭代器。
     * @return 存储完整排序键所需的字符数。
     * @endif
     *
     * @lang{EN}
     * @brief Computes the storage length required to transform a generic-iterator range into a collation key.
     *
     * The input is materialized segment by segment into a buffer, and the
     * transformed lengths of all segments are accumulated and returned.
     *
     * @tparam TIter An iterator type not implicitly convertible to `const CharT*`.
     * @param low Start iterator of the character sequence.
     * @param high End iterator of the character sequence.
     * @return The number of characters required to store the complete collation key.
     * @endif
     */
    template <typename TIter>
        requires (!std::is_convertible_v<TIter, const CharT*>)
    size_t transform_length(TIter low, TIter high) const
    {
        size_t res = 0;
        std::vector<CharT> buf; buf.reserve(64);

        while (low != high)
        {
            low = data_to_vec(low, high, buf);
            res += m_obj->transform_length(buf.data(), buf.data() + buf.size());
        }
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 将原始指针输入范围变换为排序键，写入原始指针目标缓冲区。
     *
     * 直接委托给底层 `collate_conf` 实现。
     *
     * @param low 字符序列的起始指针。
     * @param high 字符序列的结束指针（不包含）。
     * @param dest 写入排序键的目标缓冲区。
     * @param mx_len 最多写入的字符数；传入 `0` 表示不限制。
     * @return 包含写入终点指针与实际写入字符数的 `pair`。
     * @endif
     *
     * @lang{EN}
     * @brief Transforms a raw-pointer input range into a collation key, writing to a raw-pointer destination buffer.
     *
     * Delegates directly to the underlying `collate_conf` implementation.
     *
     * @param low Pointer to the start of the character sequence.
     * @param high Pointer to one past the end of the character sequence.
     * @param dest Destination buffer where the collation key is written.
     * @param mx_len Maximum number of characters to write; pass `0` for unlimited.
     * @return A pair of the past-the-end destination pointer and the number of characters written.
     * @endif
     */
    std::pair<CharT*, size_t> transform(const CharT* low, const CharT* high, CharT* dest, size_t mx_len = 0) const
    {
        auto s = m_obj->transform(low, high, dest, mx_len);
        return std::pair{dest + s, s};
    }

    /**
     * @lang{ZH}
     * @brief 将泛型迭代器输入范围变换为排序键，写入原始指针目标缓冲区。
     *
     * 将输入逐段物化到缓冲区后，依次调用底层变换并写入目标地址。
     *
     * @tparam TIter 不可隐式转换为 `const CharT*` 的迭代器类型。
     * @param low 字符序列的起始迭代器。
     * @param high 字符序列的结束迭代器。
     * @param dest 写入排序键的目标缓冲区。
     * @param mx_len 最多写入的字符数；传入 `0` 表示不限制。
     * @return 包含写入终点指针与实际写入字符数的 `pair`。
     * @endif
     *
     * @lang{EN}
     * @brief Transforms a generic-iterator input range into a collation key, writing to a raw-pointer destination buffer.
     *
     * The input is materialized segment by segment into a buffer, and the
     * underlying transformation is invoked for each segment in turn.
     *
     * @tparam TIter An iterator type not implicitly convertible to `const CharT*`.
     * @param low Start iterator of the character sequence.
     * @param high End iterator of the character sequence.
     * @param dest Destination buffer where the collation key is written.
     * @param mx_len Maximum number of characters to write; pass `0` for unlimited.
     * @return A pair of the past-the-end destination pointer and the number of characters written.
     * @endif
     */
    template <typename TIter>
        requires (!std::is_convertible_v<TIter, const CharT*>)
    std::pair<CharT*, size_t> transform(TIter low, TIter high, CharT* dest, size_t mx_len = 0) const
    {
        size_t trans_count = 0;
        std::vector<CharT> buf; buf.reserve(64);

        while ((low != high) && ((mx_len == 0) || (trans_count < mx_len)))
        {
            low = data_to_vec(low, high, buf);
            size_t cur_trans = 0;

            if (mx_len == 0)
                cur_trans = m_obj->transform(buf.data(), buf.data() + buf.size(), dest);
            else
                cur_trans = m_obj->transform(buf.data(), buf.data() + buf.size(), dest, mx_len - trans_count);
            trans_count += cur_trans;
            dest += cur_trans;
        }
        return std::pair(dest, trans_count);
    }

    /**
     * @lang{ZH}
     * @brief 将原始指针输入范围变换为排序键，通过泛型输出迭代器写出。
     *
     * 各段先变换到暂存缓冲区，再通过 `std::copy` 写入目标迭代器，
     * 避免在不支持随机访问的迭代器上直接写入。
     *
     * @tparam TIter 不可隐式转换为 `CharT*` 的输出迭代器类型。
     * @param low 字符序列的起始指针。
     * @param high 字符序列的结束指针（不包含）。
     * @param dest 写入排序键的目标输出迭代器。
     * @param mx_len 最多写入的字符数；传入 `0` 表示不限制。
     * @return 包含写入后目标迭代器位置与实际写入字符数的 `pair`。
     * @endif
     *
     * @lang{EN}
     * @brief Transforms a raw-pointer input range into a collation key, writing through a generic output iterator.
     *
     * Each segment is first transformed into a staging buffer, then copied to
     * the destination iterator via `std::copy`, avoiding direct writes on
     * iterators that do not support random access.
     *
     * @tparam TIter An output iterator type not implicitly convertible to `CharT*`.
     * @param low Pointer to the start of the character sequence.
     * @param high Pointer to one past the end of the character sequence.
     * @param dest Output iterator to which the collation key is written.
     * @param mx_len Maximum number of characters to write; pass `0` for unlimited.
     * @return A pair of the updated destination iterator and the number of characters written.
     * @endif
     */
    template <typename TIter>
        requires (!std::is_convertible_v<TIter, CharT*>)
    std::pair<TIter, size_t> transform(const CharT* low, const CharT* high, TIter dest, size_t mx_len = 0) const
    {
        size_t trans_count = 0;
        std::vector<CharT> buf;
        std::vector<CharT> buf2;

        while ((low != high) && ((mx_len == 0) || (trans_count < mx_len)))
        {
            const CharT* cur = low;
            if (const CharT* next = std::find(low, high, static_cast<CharT>(0)); next == high)
            {
                buf.resize(high - low);
                std::copy(low, high, buf.data());
                size_t cur_trans = transform_length(buf.data(), buf.data() + buf.size());
                buf2.resize(cur_trans);
                buf2.resize(m_obj->transform(buf.data(), buf.data() + buf.size(), buf2.data(), buf2.size()));
                low = high;
            }
            else
            {
                low = next + 1;
                size_t cur_trans = transform_length(cur, low);
                buf2.resize(cur_trans);
                buf2.resize(m_obj->transform(cur, low, buf2.data(), buf2.size()));
            }

            if (mx_len == 0)
            {
                dest = std::copy(buf2.begin(), buf2.end(), dest);
                trans_count += buf2.size();
            }
            else
            {
                auto dest_size = std::min(buf2.size(), mx_len - trans_count);
                dest = std::copy(buf2.begin(), buf2.begin() + dest_size, dest);
                trans_count += dest_size;
            }
        }
        return std::pair(dest, trans_count);
    }

    /**
     * @lang{ZH}
     * @brief 将泛型迭代器输入范围变换为排序键，通过泛型输出迭代器写出。
     *
     * 输入逐段物化到缓冲区，各段分别变换到暂存缓冲区后再写入目标迭代器。
     *
     * @tparam TIterIn 不可隐式转换为 `const CharT*` 的输入迭代器类型。
     * @tparam TIterOut 不可隐式转换为 `CharT*` 的输出迭代器类型。
     * @param low 字符序列的起始迭代器。
     * @param high 字符序列的结束迭代器。
     * @param dest 写入排序键的目标输出迭代器。
     * @param mx_len 最多写入的字符数；传入 `0` 表示不限制。
     * @return 包含写入后目标迭代器位置与实际写入字符数的 `pair`。
     * @endif
     *
     * @lang{EN}
     * @brief Transforms a generic-iterator input range into a collation key, writing through a generic output iterator.
     *
     * The input is materialized segment by segment into a buffer. Each segment is
     * transformed into a staging buffer before being copied to the destination iterator.
     *
     * @tparam TIterIn An input iterator type not implicitly convertible to `const CharT*`.
     * @tparam TIterOut An output iterator type not implicitly convertible to `CharT*`.
     * @param low Start iterator of the character sequence.
     * @param high End iterator of the character sequence.
     * @param dest Output iterator to which the collation key is written.
     * @param mx_len Maximum number of characters to write; pass `0` for unlimited.
     * @return A pair of the updated destination iterator and the number of characters written.
     * @endif
     */
    template <typename TIterIn, typename TIterOut>
        requires (!(std::is_convertible_v<TIterIn, const CharT*> || std::is_convertible_v<TIterOut, CharT*>))
    std::pair<TIterOut, size_t> transform(TIterIn low, TIterIn high, TIterOut dest, size_t mx_len = 0) const
    {
        size_t trans_count = 0;
        std::vector<CharT> buf; buf.reserve(64);
        std::vector<CharT> buf2;

        while ((low != high) && ((mx_len == 0) || (trans_count < mx_len)))
        {
            low = data_to_vec(low, high, buf);

            size_t cur_trans = m_obj->transform_length(buf.data(), buf.data() + buf.size());
            buf2.resize(cur_trans);
            buf2.resize(m_obj->transform(buf.data(), buf.data() + buf.size(), buf2.data(), buf2.size()));

            if (mx_len == 0)
            {
                dest = std::copy(buf2.begin(), buf2.end(), dest);
                trans_count += buf2.size();
            }
            else
            {
                auto dest_size = std::min(buf2.size(), mx_len - trans_count);
                dest = std::copy(buf2.begin(), buf2.begin() + dest_size, dest);
                trans_count += dest_size;
            }
        }

        return std::pair(dest, trans_count);
    }

private:
    /**
     * @lang{ZH}
     * @brief 从迭代器范围读取一个段到 `buf`，并返回下一段的起始迭代器。
     *
     * 从 `low` 开始依次读取字符并追加到 `buf`，直到遇到空字符（包含该空字符）或到达 `high` 为止。
     * 若未遇到空字符，则将剩余所有字符读入 `buf`。
     *
     * @tparam TIter 输入迭代器类型。
     * @param low 当前段的起始迭代器。
     * @param high 输入范围的结束迭代器。
     * @param buf 用于接收本段数据的缓冲区；调用前会被清空。
     * @return 下一段起始位置的迭代器，即本段结束后的位置。
     * @endif
     *
     * @lang{EN}
     * @brief Reads one segment from an iterator range into `buf` and returns the iterator to the next segment.
     *
     * Characters are appended to `buf` starting from `low` until a null character is
     * encountered (inclusive) or `high` is reached. If no null character is found,
     * all remaining characters are consumed into `buf`.
     *
     * @tparam TIter The input iterator type.
     * @param low Start iterator of the current segment.
     * @param high End iterator of the input range.
     * @param buf Buffer that receives the segment data; cleared before use.
     * @return Iterator to the start of the next segment, i.e., the position after this segment ends.
     * @endif
     */
    template <typename TIter>
    static TIter data_to_vec(TIter low, TIter high, std::vector<CharT>& buf)
    {
        buf.clear();
        while (low != high)
        {
            buf.push_back(*low++);
            if (buf.back() == static_cast<CharT>(0))
                break;
        }
        return low;
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
