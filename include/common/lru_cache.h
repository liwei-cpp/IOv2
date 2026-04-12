#pragma once

#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

namespace IOv2
{
template <typename TK, typename TV, size_t Capacity>
class lru_cache
{
public:
    lru_cache() = default;
    lru_cache(const lru_cache&) = delete;
    lru_cache& operator= (const lru_cache&) = delete;

public:
    std::optional<TV> get(TK key)
    {
        auto m_it = m_cache_map.find(key);
        if (m_it == m_cache_map.end())
        {
            return std::optional<TV>{};
        }

        auto l_it = m_it->second;
        m_cache_list.splice(m_cache_list.begin(), m_cache_list, l_it);
        m_it->second = m_cache_list.begin();
        return m_it->second->second;
    }
    
    template <typename K, typename V>
    bool try_put(K&& key, V&& value)
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
        m_cache_list.emplace_front(std::forward<K>(key), std::forward<V>(value));
        m_cache_map.insert({m_cache_list.front().first, m_cache_list.begin()});
        return true;
    }

    template <typename K, typename V>
    void put(K&& key, V&& value)
    {
        auto m_it = m_cache_map.find(key);

        if (m_it != m_cache_map.end())
        {
            auto l_it = m_it->second;
            l_it->second = std::forward<V>(value);
            m_cache_list.splice(m_cache_list.begin(), m_cache_list, l_it);
            m_it->second = m_cache_list.begin();
            return;
        }

        if (m_cache_list.size() >= Capacity)
        {
            m_cache_map.erase(m_cache_list.back().first);
            m_cache_list.pop_back();
        }
        m_cache_list.emplace_front(std::forward<K>(key), std::forward<V>(value));
        m_cache_map.insert({m_cache_list.front().first, m_cache_list.begin()});
    }
private:
    std::list<std::pair<TK, TV>> m_cache_list;
    std::unordered_map<TK, typename std::list<std::pair<TK, TV>>::iterator> m_cache_map;
};
}