#pragma once
#include <forward_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <common/streambuf_defs.h>

namespace IOv2
{
/**
 * @brief A prefix tree (Trie) used for internal IOv2 components.
 * 
 * Note:
 * 1. This class is intended for internal use and does not provide a comprehensive
 *    set of public interfaces (e.g., remove, clear, size).
 * 2. It is designed to use a sentinel (default) value to represent "no value" at a node.
 *    As a result, the default value itself cannot be stored as a legitimate value in the tree.
 */
template <typename CharT, typename TValue>
class prefix_tree
{
    struct node
    {
        std::unordered_map<CharT, std::unique_ptr<node>> children;
        TValue val;
        size_t depth;

        node(TValue v, int d)
            : val(v)
            , depth(d) {}
    };

public:
    explicit prefix_tree(TValue dflt = static_cast<TValue>(-1))
        : m_def(dflt)
        , m_root(dflt, 0)
    { }

    prefix_tree(const std::vector<const CharT*>& strs, TValue dflt = (TValue)-1)
        : prefix_tree(dflt)
    {
        for (size_t i = 0; i < strs.size(); ++i)
        {
            add(strs[i], (TValue)i);
        }
    }
    
    void add(std::basic_string_view<CharT> str, TValue v)
    {
        return add(str.begin(), str.end(), v);
    }
    
    template <typename TIter>
    void add(TIter b, TIter e, TValue v)
    {
        if (v == m_def)
            throw std::runtime_error("cannot add with default value");
            
        node* node_ptr = &m_root;
        while (b != e)
        {
            CharT ch = *b++;
            
            auto child = node_ptr->children.find(ch);
            if (child == node_ptr->children.end())
            {
                bool insert_suc = false;
                auto c = std::make_unique<node>(m_def, node_ptr->depth + 1);
                std::tie(child, insert_suc) = node_ptr->children.insert(std::pair(ch, std::move(c)));
                if (!insert_suc)
                    throw std::runtime_error("prefix_tree insert fails");
            }
            node_ptr = child->second.get();
        }
        
        if ((node_ptr->val != m_def) && (node_ptr->val != v))
            throw std::runtime_error("duplicate items in prefix_tree");
        node_ptr->val = v;
    }
    
    template <std::bidirectional_iterator TIter, std::sentinel_for<TIter> TSent>
    TIter max_match(TIter b, TSent e, TValue& out) const
    {
        out = m_root.val;
        size_t found_depth = 0;
        
        const node* node_ptr = &m_root;
        for (; b != e; ++b)
        {
            CharT ch = *b;
            auto it = node_ptr->children.find(ch);
            if (it == node_ptr->children.end()) break;
            node_ptr = it->second.get();
            if (node_ptr->val != m_def)
            {
                out = node_ptr->val;
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
    TIter max_match(TIter b, TSent e, TValue& out) const
    {
        std::forward_list<CharT> checking_chars;

        out = m_root.val;
        int found_depth = 0;
        
        const node* node_ptr = &m_root;
        for (; b != e;)
        {
            CharT ch = *b;
            auto it = node_ptr->children.find(ch);
            if (it == node_ptr->children.end()) break;
            node_ptr = it->second.get();
            if (node_ptr->val != m_def)
            {
                out = node_ptr->val;
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
    TValue m_def;
    node   m_root;
};
}