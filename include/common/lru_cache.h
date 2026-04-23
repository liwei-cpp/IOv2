#pragma once
#include <common/metafunctions.h>

#include <cstddef>
#include <list>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace IOv2
{
/**
 * @brief A simple Least Recently Used (LRU) cache.
 * 
 * @tparam TK Key type.
 * @tparam TV Value type.
 * @tparam Capacity Maximum number of items to store.
 * 
 * @note This class is NOT thread-safe.
 */
template <typename TK, typename TV, size_t Capacity>
    requires std::is_copy_constructible_v<TK>  // Key must be copyable for storage
          && std::is_copy_constructible_v<TV>  // Value must be copyable for storage
          && std::equality_comparable<TK>      // Key needs operator== for hash map
          && requires { std::hash<TK>{}; }     // Key must be hashable
class lru_cache
{
    static_assert(Capacity > 0, "Capacity must be greater than 0");

public:
    using key_param_type = std::conditional_t<is_small_type_v<TK>, TK, const TK&>;
    using val_param_type = std::conditional_t<is_small_type_v<TV>, TV, const TV&>;
    using return_type = std::conditional_t<is_small_type_v<TV>, std::optional<TV>, const TV*>;

    lru_cache() = default;
    lru_cache(const lru_cache&) = delete;
    lru_cache& operator=(const lru_cache&) = delete;

public:
    [[nodiscard]] return_type get(key_param_type key)
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

        if constexpr (is_small_type_v<TV>)
        {
            return m_it->second->second;
        }
        else
        {
            return &m_it->second->second;
        }
    }

    [[nodiscard]] bool try_put(key_param_type key, val_param_type value)
    {
        auto m_it = m_cache_map.find(key);

        if (m_it != m_cache_map.end())
        {
            auto l_it = m_it->second;
            m_cache_list.splice(m_cache_list.begin(), m_cache_list, l_it);
            return false;
        }

        m_cache_list.emplace_front(key, value);
        try
        {
            m_cache_map.insert({m_cache_list.front().first, m_cache_list.begin()});
        }
        catch (...)
        {
            m_cache_list.pop_front();
            throw;
        }

        if (m_cache_list.size() > Capacity)
        {
            m_cache_map.erase(m_cache_list.back().first);
            m_cache_list.pop_back();
        }

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
            return;
        }

        m_cache_list.emplace_front(key, value);
        try
        {
            m_cache_map.insert({m_cache_list.front().first, m_cache_list.begin()});
        }
        catch (...)
        {
            m_cache_list.pop_front();
            throw;
        }

        if (m_cache_list.size() > Capacity)
        {
            m_cache_map.erase(m_cache_list.back().first);
            m_cache_list.pop_back();
        }
    }
private:
    std::list<std::pair<TK, TV>> m_cache_list;
    std::unordered_map<TK, typename std::list<std::pair<TK, TV>>::iterator> m_cache_map;
};
}
