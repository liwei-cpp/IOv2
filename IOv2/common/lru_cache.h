#pragma once

#include <list>
#include <optional>
#include <unordered_map>

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
    
    void put(TK key, TV value)
    {
        auto m_it = m_cache_map.find(key);

        if (m_it != m_cache_map.end())
        {
            auto l_it = m_it->second;
            m_cache_list.splice(m_cache_list.begin(), m_cache_list, l_it);
            m_it->second = m_cache_list.begin();
            return;
        }
        
        if (m_cache_list.size() >= Capacity)
        {
            m_cache_map.erase(m_cache_list.back().first);
            m_cache_list.pop_back();
        }
        m_cache_list.push_front({key, value});
        m_cache_map.insert({key, m_cache_list.begin()});
    }
private:
    std::list<std::pair<TK, TV>> m_cache_list;
    std::unordered_map<TK, typename std::list<std::pair<TK, TV>>::iterator> m_cache_map;
};
}