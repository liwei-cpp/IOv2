#pragma once

#include <list>
#include <optional>
#include <unordered_map>
#include <utility>
#include <type_traits>
#include <common/metafunctions.h>

namespace IOv2
{
template <typename TK, typename TV, size_t Capacity>
class lru_cache
{
public:
    using key_param_type = std::conditional_t<is_small_type_v<TK>, TK, const TK&>;
    using val_param_type = std::conditional_t<is_small_type_v<TV>, TV, const TV&>;
    using return_type = std::conditional_t<is_small_type_v<TV>, std::optional<TV>, const TV*>;

    lru_cache() = default;
    lru_cache(const lru_cache&) = delete;
    lru_cache& operator= (const lru_cache&) = delete;

public:
    return_type get(key_param_type key)
    {
        auto m_it = m_cache_map.find(key);
        if (m_it == m_cache_map.end())
        {
            if constexpr (is_small_type_v<TV>)
            {
                return std::optional<TV>{};
            }
            else
            {
                return nullptr;
            }
        }

        auto l_it = m_it->second;
        m_cache_list.splice(m_cache_list.begin(), m_cache_list, l_it);
        m_it->second = m_cache_list.begin();

        if constexpr (is_small_type_v<TV>)
        {
            return m_it->second->second;
        }
        else
        {
            return &m_it->second->second;
        }
    }

    bool try_put(key_param_type key, val_param_type value)
    {
        auto m_it = m_cache_map.find(key);

        if (m_it != m_cache_map.end())
        {
            auto l_it = m_it->second;
            m_cache_list.splice(m_cache_list.begin(), m_cache_list, l_it);
            m_it->second = m_cache_list.begin();
            return false;
        }

        if (m_cache_list.size() >= Capacity)
        {
            m_cache_map.erase(m_cache_list.back().first);
            m_cache_list.pop_back();
        }
        m_cache_list.emplace_front(key, value);
        m_cache_map.insert({m_cache_list.front().first, m_cache_list.begin()});
        return true;
    }

    void put(key_param_type key, val_param_type value)
    {
        auto m_it = m_cache_map.find(key);

        if (m_it != m_cache_map.end())
        {
            auto l_it = m_it->second;
            l_it->second = value;
            m_cache_list.splice(m_cache_list.begin(), m_cache_list, l_it);
            m_it->second = m_cache_list.begin();
            return;
        }

        if (m_cache_list.size() >= Capacity)
        {
            m_cache_map.erase(m_cache_list.back().first);
            m_cache_list.pop_back();
        }
        m_cache_list.emplace_front(key, value);
        m_cache_map.insert({m_cache_list.front().first, m_cache_list.begin()});
    }
private:
    std::list<std::pair<TK, TV>> m_cache_list;
    std::unordered_map<TK, typename std::list<std::pair<TK, TV>>::iterator> m_cache_map;
};
}
