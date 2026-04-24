/**
 * @file prefix_tree.h
 * @lang{ZH}
 * 前缀树（Trie）实现，用于字符串匹配。
 * @endif
 * @lang{EN}
 * Prefix tree (Trie) implementation for string matching.
 * @endif
 */

#pragma once
#include <common/metafunctions.h>
#include <common/streambuf_defs.h>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <forward_list>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace IOv2
{
/**
 * @lang{ZH}
 * 前缀树（Trie）实现，用于 IOv2 内部组件。
 *
 * 此类实现了一个前缀树数据结构，用于高效的字符串前缀匹配。
 * 支持最长前缀匹配，可用于解析器等场景。
 *
 * @tparam CharT 字符类型，必须可比较且可哈希
 * @tparam TValue 与字符串关联的值类型，必须可比较
 *
 * @note 此类供内部使用，不提供完整的公共接口（如 remove、clear、size）。
 * @note 此类不是线程安全的。
 *
 * @par 示例
 * @code
 * prefix_tree<char, int> tree;
 * tree.add("hello", 1);
 * tree.add("help", 2);
 *
 * std::string input = "helper";
 * prefix_tree<char, int>::match_out_type result;
 * auto end_it = tree.max_match(input.begin(), input.end(), result);
 * // result == 2 ("help" 是最长匹配)
 * // end_it 指向 "er"
 * @endcode
 * @endif
 *
 * @lang{EN}
 * Prefix tree (Trie) implementation for internal IOv2 components.
 *
 * This class implements a prefix tree data structure for efficient string
 * prefix matching. Supports longest prefix matching, useful for parsers
 * and similar scenarios.
 *
 * @tparam CharT Character type, must be equality comparable and hashable
 * @tparam TValue Value type associated with strings, must be equality comparable
 *
 * @note This class is intended for internal use and does not provide a comprehensive
 *       set of public interfaces (e.g., remove, clear, size).
 * @note This class is NOT thread-safe.
 *
 * @par Example
 * @code
 * prefix_tree<char, int> tree;
 * tree.add("hello", 1);
 * tree.add("help", 2);
 *
 * std::string input = "helper";
 * prefix_tree<char, int>::match_out_type result;
 * auto end_it = tree.max_match(input.begin(), input.end(), result);
 * // result == 2 ("help" is the longest match)
 * // end_it points to "er"
 * @endcode
 * @endif
 */
template <typename CharT, typename TValue>
class prefix_tree
{
    // [NOTE] Prevents confusing compile errors
    static_assert(std::equality_comparable<CharT>, "CharT must be equality comparable");
    static_assert(std::equality_comparable<TValue>, "TValue must be equality comparable");
    static_assert(requires { std::hash<CharT>{}; }, "CharT must be hashable");

    /// @cond INTERNAL
    struct node
    {
        std::unordered_map<CharT, std::unique_ptr<node>> children;
        std::optional<TValue> val;
        size_t depth;

        node(std::optional<TValue> v, size_t d)
            : val(std::move(v))
            , depth(d) {}
    };
    /// @endcond

public:
    /**
     * @lang{ZH}
     * 匹配结果的输出类型。
     *
     * 对于小类型返回 std::optional<TValue>，对于大类型返回 const TValue*。
     * @endif
     *
     * @lang{EN}
     * Output type for match results.
     *
     * Returns std::optional<TValue> for small types, const TValue* for large types.
     * @endif
     */
    using match_out_type = std::conditional_t<is_small_type_v<TValue>, std::optional<TValue>, const TValue*>;

    /**
     * @lang{ZH}
     * 构造一个空的前缀树。
     * @endif
     *
     * @lang{EN}
     * Constructs an empty prefix tree.
     * @endif
     */
    prefix_tree()
        : m_root(std::nullopt, 0)
    { }

    /**
     * @lang{ZH}
     * 从 C 字符串数组构造前缀树。
     *
     * 每个字符串的值设置为其在数组中的索引。
     *
     * @param strs C 字符串指针数组
     * @throw std::runtime_error 如果数组大小超过 TValue 能表示的最大值
     * @throw std::runtime_error 如果数组中包含空指针
     * @endif
     *
     * @lang{EN}
     * Constructs a prefix tree from an array of C strings.
     *
     * Each string's value is set to its index in the array.
     *
     * @param strs Array of C string pointers
     * @throw std::runtime_error If array size exceeds TValue's maximum representable value
     * @throw std::runtime_error If the array contains null pointers
     * @endif
     */
    explicit prefix_tree(const std::vector<const CharT*>& strs)
        requires std::is_integral_v<TValue>
        : prefix_tree()
    {
        if (strs.size() > 0 &&
            strs.size() - 1 > static_cast<size_t>(std::numeric_limits<TValue>::max()))
            throw std::runtime_error("prefix_tree: too many strings for TValue type");

        for (size_t i = 0; i < strs.size(); ++i)
        {
            if (strs[i] == nullptr)
                throw std::runtime_error("prefix_tree: null pointer in input vector");
            add(strs[i], static_cast<TValue>(i));
        }
    }

    /**
     * @lang{ZH}
     * 向前缀树添加字符串及其关联值。
     *
     * @param str 要添加的字符串
     * @param v 与字符串关联的值
     * @throw std::runtime_error 如果字符串已存在且关联不同的值
     * @endif
     *
     * @lang{EN}
     * Adds a string and its associated value to the prefix tree.
     *
     * @param str The string to add
     * @param v The value associated with the string
     * @throw std::runtime_error If the string already exists with a different value
     * @endif
     */
    void add(std::basic_string_view<CharT> str, TValue v)
    {
        add(str.begin(), str.end(), v);
    }

    /**
     * @lang{ZH}
     * 向前缀树添加由迭代器范围指定的字符串及其关联值。
     *
     * @tparam TIter 迭代器类型
     * @param b 范围起始迭代器
     * @param e 范围结束迭代器
     * @param v 与字符串关联的值
     * @throw std::runtime_error 如果字符串已存在且关联不同的值
     * @throw std::runtime_error 如果插入节点失败
     * @endif
     *
     * @lang{EN}
     * Adds a string specified by an iterator range and its associated value.
     *
     * @tparam TIter Iterator type
     * @param b Begin iterator of the range
     * @param e End iterator of the range
     * @param v The value associated with the string
     * @throw std::runtime_error If the string already exists with a different value
     * @throw std::runtime_error If node insertion fails
     * @endif
     */
    template <typename TIter>
    void add(TIter b, TIter e, TValue v)
    {
        node* node_ptr = &m_root;
        while (b != e)
        {
            CharT ch = *b++;

            auto child = node_ptr->children.find(ch);
            if (child == node_ptr->children.end())
            {
                bool insert_suc = false;
                auto c = std::make_unique<node>(std::nullopt, node_ptr->depth + 1);
                std::tie(child, insert_suc) = node_ptr->children.insert(std::pair(ch, std::move(c)));
                if (!insert_suc)
                    throw std::runtime_error("prefix_tree insert fails");
            }
            node_ptr = child->second.get();
        }

        if (node_ptr->val.has_value() && *node_ptr->val != v)
            throw std::runtime_error("duplicate items in prefix_tree");
        node_ptr->val = v;
    }

    /**
     * @lang{ZH}
     * 在输入范围中查找最长匹配的前缀。
     *
     * 此重载适用于双向迭代器（如 std::string::iterator）。
     *
     * @tparam TIter 双向迭代器类型
     * @tparam TSent 哨兵类型
     * @param b 范围起始迭代器
     * @param e 范围结束迭代器/哨兵
     * @param[out] out 匹配结果，如果没有匹配则为 nullopt/nullptr
     * @return 指向最长匹配之后位置的迭代器
     * @endif
     *
     * @lang{EN}
     * Finds the longest matching prefix in the input range.
     *
     * This overload is for bidirectional iterators (e.g., std::string::iterator).
     *
     * @tparam TIter Bidirectional iterator type
     * @tparam TSent Sentinel type
     * @param b Begin iterator of the range
     * @param e End iterator/sentinel of the range
     * @param[out] out Match result, nullopt/nullptr if no match
     * @return Iterator pointing to the position after the longest match
     * @endif
     */
    template <std::bidirectional_iterator TIter, std::sentinel_for<TIter> TSent>
    [[nodiscard]] TIter max_match(TIter b, TSent e, match_out_type& out) const
    {
        // Debug assertion: check for valid range (only for random access iterators)
        if constexpr (std::random_access_iterator<TIter> && std::same_as<TIter, TSent>)
            assert(b <= e && "max_match: invalid iterator range (b > e)");

        if constexpr (is_small_type_v<TValue>)
            out = std::nullopt;
        else
            out = nullptr;

        if (m_root.val.has_value())
        {
            if constexpr (is_small_type_v<TValue>)
                out = m_root.val;
            else
                out = &(*m_root.val);
        }

        size_t found_depth = 0;

        const node* node_ptr = &m_root;
        for (; b != e; ++b)
        {
            CharT ch = *b;
            auto it = node_ptr->children.find(ch);
            if (it == node_ptr->children.end()) break;
            node_ptr = it->second.get();
            if (node_ptr->val.has_value())
            {
                if constexpr (is_small_type_v<TValue>)
                    out = node_ptr->val;
                else
                    out = &(*node_ptr->val);
                found_depth = node_ptr->depth;
            }
        }

        if (node_ptr->depth != found_depth)
        {
            const size_t steps_back = node_ptr->depth - found_depth;
            std::advance(b, -static_cast<std::ptrdiff_t>(steps_back));
        }
        return b;
    }

    /**
     * @lang{ZH}
     * 在输入范围中查找最长匹配的前缀。
     *
     * 此重载适用于 IOv2::istreambuf_iterator，使用 sputbackc 进行回退。
     *
     * @tparam TIter istreambuf_iterator 类型
     * @tparam TSent 哨兵类型
     * @param b 范围起始迭代器
     * @param e 范围结束迭代器/哨兵
     * @param[out] out 匹配结果，如果没有匹配则为 nullopt/nullptr
     * @return 指向最长匹配之后位置的迭代器
     * @endif
     *
     * @lang{EN}
     * Finds the longest matching prefix in the input range.
     *
     * This overload is for IOv2::istreambuf_iterator, using sputbackc for backtracking.
     *
     * @tparam TIter istreambuf_iterator type
     * @tparam TSent Sentinel type
     * @param b Begin iterator of the range
     * @param e End iterator/sentinel of the range
     * @param[out] out Match result, nullopt/nullptr if no match
     * @return Iterator pointing to the position after the longest match
     * @endif
     */
    template <is_istreambuf_iterator TIter, std::sentinel_for<TIter> TSent>
    [[nodiscard]] TIter max_match(TIter b, TSent e, match_out_type& out) const
    {
        if constexpr (is_small_type_v<TValue>)
            out = std::nullopt;
        else
            out = nullptr;

        std::forward_list<CharT> checking_chars;

        if (m_root.val.has_value())
        {
            if constexpr (is_small_type_v<TValue>)
                out = m_root.val;
            else
                out = &(*m_root.val);
        }

        size_t found_depth = 0;

        const node* node_ptr = &m_root;
        for (; b != e;)
        {
            CharT ch = *b;
            auto it = node_ptr->children.find(ch);
            if (it == node_ptr->children.end()) break;
            node_ptr = it->second.get();
            if (node_ptr->val.has_value())
            {
                if constexpr (is_small_type_v<TValue>)
                    out = node_ptr->val;
                else
                    out = &(*node_ptr->val);
                found_depth = node_ptr->depth;
            }
            checking_chars.push_front(ch);
            ++b;
        }

        if (node_ptr->depth != found_depth)
        {
            size_t count = node_ptr->depth - found_depth;
            for (size_t i = 0; i < count; ++i)
            {
                b.sputbackc(checking_chars.front());
                checking_chars.pop_front();
            }
        }
        return b;
    }

private:
    node m_root;
};
}
