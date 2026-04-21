#pragma once
#include <forward_list>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <common/streambuf_defs.h>
#include <common/metafunctions.h>

namespace IOv2
{
/**
 * @brief A prefix tree (Trie) used for internal IOv2 components.
 * 
 * Note: This class is intended for internal use and does not provide a comprehensive
 *       set of public interfaces (e.g., remove, clear, size).
 */
template <typename CharT, typename TValue>
class prefix_tree
{
    struct node
    {
        std::unordered_map<CharT, std::unique_ptr<node>> children;
        std::optional<TValue> val;
        size_t depth;

        node(std::optional<TValue> v, size_t d)
            : val(v)
            , depth(d) {}
    };

public:
    using match_out_type = std::conditional_t<is_small_type_v<TValue>, std::optional<TValue>, const TValue*>;

    explicit prefix_tree()
        : m_root(std::nullopt, 0)
    { }

    prefix_tree(const std::vector<const CharT*>& strs)
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
    
    void add(std::basic_string_view<CharT> str, TValue v)
    {
        return add(str.begin(), str.end(), v);
    }
    
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
    
    template <std::bidirectional_iterator TIter, std::sentinel_for<TIter> TSent>
    TIter max_match(TIter b, TSent e, match_out_type& out) const
    {
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

    template <is_istreambuf_iterator_v TIter, std::sentinel_for<TIter> TSent>
    TIter max_match(TIter b, TSent e, match_out_type& out) const
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
    node   m_root;
};
}