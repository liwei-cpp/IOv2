/**
 * @file lru_cache.h
 * @lang{ZH}
 * 简单的 LRU（最近最少使用）缓存实现。
 * @endif
 * @lang{EN}
 * Simple LRU (Least Recently Used) cache implementation.
 * @endif
 */

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
 * @lang{ZH}
 * 简单的最近最少使用（LRU）缓存。
 *
 * 当缓存达到容量上限时，最久未被访问的项将被驱逐。
 * get() 和 put() 操作的平均时间复杂度为 O(1)。
 *
 * @tparam TK 键类型，必须可复制、可比较且可哈希
 * @tparam TV 值类型，必须可复制
 * @tparam Capacity 缓存的最大容量
 *
 * @note 此类不是线程安全的。
 *
 * @par 示例
 * @code
 * lru_cache<std::string, int, 100> cache;
 * cache.put("key1", 42);
 * if (auto val = cache.get("key1")) {
 *     // 对于小类型，val 是 std::optional<int>
 *     std::cout << *val << std::endl;
 * }
 * @endcode
 * @endif
 *
 * @lang{EN}
 * A simple Least Recently Used (LRU) cache.
 *
 * When the cache reaches capacity, the least recently accessed item is evicted.
 * Both get() and put() operations have O(1) average time complexity.
 *
 * @tparam TK Key type, must be copyable, equality comparable, and hashable
 * @tparam TV Value type, must be copyable
 * @tparam Capacity Maximum number of items to store
 *
 * @note This class is NOT thread-safe.
 *
 * @par Example
 * @code
 * lru_cache<std::string, int, 100> cache;
 * cache.put("key1", 42);
 * if (auto val = cache.get("key1")) {
 *     // For small types, val is std::optional<int>
 *     std::cout << *val << std::endl;
 * }
 * @endcode
 * @endif
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
    /**
     * @lang{ZH}
     * 获取缓存中的值。
     *
     * 如果键存在，该项将被标记为最近使用（移动到缓存前端）。
     *
     * @param key 要查找的键
     * @return 对于小类型返回 std::optional<TV>，对于大类型返回 const TV*；
     *         如果键不存在，返回空 optional 或 nullptr
     *
     * @warning 对于大类型（is_small_type_v<TV> == false），返回的指针指向内部存储。
     *          后续调用 put() 或 try_put() 可能导致该指针失效（当缓存驱逐该项时）。
     *          如需长期持有值，请立即拷贝：
     *          @code
     *          if (auto* p = cache.get(key)) {
     *              TV copy = *p;  // 立即拷贝
     *              // 现在可以安全地调用其他 cache 操作
     *          }
     *          @endcode
     * @endif
     *
     * @lang{EN}
     * Get a value from the cache.
     *
     * If the key exists, the item is marked as recently used (moved to front).
     *
     * @param key The key to look up
     * @return std::optional<TV> for small types, const TV* for large types;
     *         returns empty optional or nullptr if key not found
     *
     * @warning For large types (is_small_type_v<TV> == false), the returned pointer
     *          points to internal storage. Subsequent calls to put() or try_put()
     *          may invalidate this pointer (when the cache evicts this item).
     *          If you need to hold the value long-term, copy it immediately:
     *          @code
     *          if (auto* p = cache.get(key)) {
     *              TV copy = *p;  // Copy immediately
     *              // Now safe to call other cache operations
     *          }
     *          @endcode
     * @endif
     */
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

    /**
     * @lang{ZH}
     * 尝试将键值对放入缓存（不更新已存在的值）。
     *
     * 如果键已存在，仅将其标记为最近使用，不更新值。
     * 如果键不存在，插入新项；如果缓存已满，驱逐最久未使用的项。
     *
     * @param key 键
     * @param value 值
     * @return 如果插入了新项返回 true，如果键已存在返回 false
     * @endif
     *
     * @lang{EN}
     * Try to put a key-value pair into the cache (without updating existing values).
     *
     * If the key already exists, only marks it as recently used without updating the value.
     * If the key doesn't exist, inserts a new item; evicts the LRU item if cache is full.
     *
     * @param key The key
     * @param value The value
     * @return true if a new item was inserted, false if the key already existed
     * @endif
     */
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

    /**
     * @lang{ZH}
     * 将键值对放入缓存（更新已存在的值）。
     *
     * 如果键已存在，更新其值并标记为最近使用。
     * 如果键不存在，插入新项；如果缓存已满，驱逐最久未使用的项。
     *
     * @param key 键
     * @param value 值
     * @endif
     *
     * @lang{EN}
     * Put a key-value pair into the cache (updates existing values).
     *
     * If the key already exists, updates its value and marks it as recently used.
     * If the key doesn't exist, inserts a new item; evicts the LRU item if cache is full.
     *
     * @param key The key
     * @param value The value
     * @endif
     */
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
