/**
 * @file stamp_input_iterator.h
 * @lang{ZH}
 * 带回滚功能的输入迭代器包装器。
 * @endif
 * @lang{EN}
 * Input iterator wrapper with rollback capability.
 * @endif
 */

#pragma once
#include <common/streambuf_defs.h>

#include <forward_list>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * stamp_input_iterator 的前向声明。
 *
 * @tparam TIter 底层迭代器类型
 * @endif
 *
 * @lang{EN}
 * Forward declaration of stamp_input_iterator.
 *
 * @tparam TIter The underlying iterator type
 * @endif
 */
template <typename TIter>
struct stamp_input_iterator;

/**
 * @lang{ZH}
 * 双向迭代器的 stamp_input_iterator 特化。
 *
 * 此特化包装双向迭代器，记录位置偏移以支持回滚到初始位置。
 * 适用于随机访问迭代器和普通双向迭代器。
 *
 * @tparam TIter 底层双向迭代器类型
 *
 * @par 示例
 * @code
 * std::vector<int> vec = {1, 2, 3, 4, 5};
 * stamp_input_iterator it(vec.begin());
 * ++it; ++it;  // 前进两步
 * it.rollback();  // 回滚到初始位置
 * // *it == 1
 * @endcode
 * @endif
 *
 * @lang{EN}
 * stamp_input_iterator specialization for bidirectional iterators.
 *
 * This specialization wraps bidirectional iterators and records position offset
 * to support rollback to the initial position. Works with random access iterators
 * and regular bidirectional iterators.
 *
 * @tparam TIter The underlying bidirectional iterator type
 *
 * @par Example
 * @code
 * std::vector<int> vec = {1, 2, 3, 4, 5};
 * stamp_input_iterator it(vec.begin());
 * ++it; ++it;  // Advance two steps
 * it.rollback();  // Rollback to initial position
 * // *it == 1
 * @endcode
 * @endif
 */
template <std::bidirectional_iterator TIter>
struct stamp_input_iterator<TIter>
{
    stamp_input_iterator()
        : m_internal()
        , m_pos(0) {}

    stamp_input_iterator(TIter internal)
        : m_internal(std::move(internal))
        , m_pos(0) {}

    stamp_input_iterator(std::default_sentinel_t)
        : m_internal()
        , m_pos(0) {}

    stamp_input_iterator(const stamp_input_iterator&) = default;
    stamp_input_iterator& operator=(const stamp_input_iterator&) = default;

    stamp_input_iterator(stamp_input_iterator&& val) noexcept
        : m_internal(std::move(val.m_internal))
        , m_pos(val.m_pos)
    {
        val.m_pos = 0;
    }

    stamp_input_iterator& operator=(stamp_input_iterator&& val) noexcept
    {
        if (this != &val)
        {
            m_internal = std::move(val.m_internal);
            m_pos = val.m_pos;
            val.m_pos = 0;
        }
        return *this;
    }

    ~stamp_input_iterator() = default;

    using value_type        = typename std::iterator_traits<TIter>::value_type;
    using difference_type   = typename std::iterator_traits<TIter>::difference_type;
    using pointer           = typename std::iterator_traits<TIter>::pointer;
    using reference         = typename std::iterator_traits<TIter>::reference;
    using iterator_category = typename std::iterator_traits<TIter>::iterator_category;

    [[nodiscard]] reference operator*() const { return *m_internal; }
    [[nodiscard]] pointer operator->() const
    {
        if constexpr (std::is_pointer_v<TIter>)
            return m_internal;
        else
            return m_internal.operator->();
    }

    [[nodiscard]] reference operator[](difference_type n) const
        requires (std::random_access_iterator<TIter>)
    { return m_internal[n]; }

    stamp_input_iterator& operator++() { ++m_internal; ++m_pos; return *this; }
    [[nodiscard]] stamp_input_iterator operator++(int) { stamp_input_iterator tmp = *this; ++(*this); return tmp; }
    stamp_input_iterator& operator--() { --m_internal; --m_pos; return *this; }
    [[nodiscard]] stamp_input_iterator operator--(int) { stamp_input_iterator tmp = *this; --(*this); return tmp; }

    stamp_input_iterator& operator+=(difference_type n)
        requires (std::random_access_iterator<TIter>)
    {
        m_internal += n;
        m_pos += n;
        return *this;
    }

    stamp_input_iterator& operator-=(difference_type n)
        requires (std::random_access_iterator<TIter>)
    {
        m_internal -= n;
        m_pos -= n;
        return *this;
    }

    friend bool operator==(const stamp_input_iterator& a, const stamp_input_iterator& b) { return a.m_internal == b.m_internal; };
    friend bool operator!=(const stamp_input_iterator& a, const stamp_input_iterator& b) { return a.m_internal != b.m_internal; };
    friend bool operator<(const stamp_input_iterator& a, const stamp_input_iterator& b)
        requires (std::random_access_iterator<TIter>) { return a.m_internal < b.m_internal; };
    friend bool operator>(const stamp_input_iterator& a, const stamp_input_iterator& b)
        requires (std::random_access_iterator<TIter>) { return a.m_internal > b.m_internal; };
    friend bool operator<=(const stamp_input_iterator& a, const stamp_input_iterator& b)
        requires (std::random_access_iterator<TIter>) { return a.m_internal <= b.m_internal; };
    friend bool operator>=(const stamp_input_iterator& a, const stamp_input_iterator& b)
        requires (std::random_access_iterator<TIter>) { return a.m_internal >= b.m_internal; };

    [[nodiscard]] friend stamp_input_iterator operator+(const stamp_input_iterator& it, difference_type n)
        requires (std::random_access_iterator<TIter>)
    {
        stamp_input_iterator tmp = it;
        tmp += n;
        return tmp;
    }

    [[nodiscard]] friend stamp_input_iterator operator+(difference_type n, const stamp_input_iterator& it)
        requires (std::random_access_iterator<TIter>)
    {
        return it + n;
    }

    [[nodiscard]] friend stamp_input_iterator operator-(const stamp_input_iterator& it, difference_type n)
        requires (std::random_access_iterator<TIter>)
    {
        stamp_input_iterator tmp = it;
        tmp -= n;
        return tmp;
    }

    [[nodiscard]] friend difference_type operator-(const stamp_input_iterator& lhs, const stamp_input_iterator& rhs)
        requires (std::random_access_iterator<TIter>)
    {
        return lhs.m_internal - rhs.m_internal;
    }

    /**
     * @lang{ZH}
     * 回滚到初始位置。
     *
     * 将迭代器移动回创建时的位置。
     * @endif
     *
     * @lang{EN}
     * Rollback to the initial position.
     *
     * Moves the iterator back to the position when it was created.
     * @endif
     */
    void rollback()
    {
        if (m_pos) std::advance(m_internal, -m_pos);
        m_pos = 0;
    }

    /**
     * @lang{ZH}
     * 获取底层迭代器。
     *
     * @return 底层迭代器的副本
     * @endif
     *
     * @lang{EN}
     * Get the underlying iterator.
     *
     * @return A copy of the underlying iterator
     * @endif
     */
    [[nodiscard]] TIter internal() const { return m_internal; }
private:
    TIter           m_internal;
    difference_type m_pos;
};

/**
 * @lang{ZH}
 * IOv2 istreambuf_iterator 的 stamp_input_iterator 特化。
 *
 * 此特化专为 IOv2::istreambuf_iterator 设计，不兼容 std::istreambuf_iterator。
 * 主要区别：
 *   1. IOv2 的 sputbackc() 返回 void 且永不失败（使用无限 deque）
 *   2. std::streambuf 的 sputbackc() 返回 int_type 且可能失败（有限的回退区域）
 *
 * istreambuf_iterator 概念约束确保此特化仅匹配 IOv2::istreambuf_iterator 类型。
 *
 * @tparam TIter 必须满足 is_istreambuf_iterator（仅限 IOv2::istreambuf_iterator）
 * @endif
 *
 * @lang{EN}
 * stamp_input_iterator specialization for IOv2 istreambuf_iterator.
 *
 * This specialization is designed specifically for IOv2::istreambuf_iterator
 * and is NOT compatible with std::istreambuf_iterator. Key differences:
 *   1. IOv2's sputbackc() returns void and never fails (uses unlimited deque)
 *   2. std::streambuf's sputbackc() returns int_type and may fail (limited putback area)
 *
 * The istreambuf_iterator concept constraint ensures this specialization
 * only matches IOv2::istreambuf_iterator types.
 *
 * @tparam TIter Must satisfy is_istreambuf_iterator (IOv2::istreambuf_iterator only)
 * @endif
 */
template <is_istreambuf_iterator TIter>
struct stamp_input_iterator<TIter>
{
    stamp_input_iterator()
        : m_internal() {}

    stamp_input_iterator(TIter internal)
        : m_internal(std::move(internal)) {}

    stamp_input_iterator(std::default_sentinel_t)
        : m_internal() {}

    stamp_input_iterator(const stamp_input_iterator&) = default;
    stamp_input_iterator& operator=(const stamp_input_iterator&) = default;
    stamp_input_iterator(stamp_input_iterator&&) noexcept = default;
    stamp_input_iterator& operator=(stamp_input_iterator&&) noexcept = default;
    ~stamp_input_iterator() = default;

    using value_type        = typename TIter::value_type;
    using difference_type   = typename TIter::difference_type;

    [[nodiscard]] auto operator*() const { return *m_internal; }
    [[nodiscard]] auto operator->() const
    {
        if constexpr (std::is_pointer_v<TIter>)
            return m_internal;
        else
            return m_internal.operator->();
    }

    stamp_input_iterator& operator++()
    {
        m_rec.push_front(*m_internal);
        ++m_internal;
        return *this;
    }
    [[nodiscard]] stamp_input_iterator operator++(int) { stamp_input_iterator tmp = *this; ++(*this); return tmp; }

    stamp_input_iterator& operator--()
    {
        if (m_rec.empty())
            throw std::runtime_error("stamp_input_iterator fail, cannot move backward");

        m_internal.sputbackc(m_rec.front());
        m_rec.pop_front();
        return *this;
    }
    [[nodiscard]] stamp_input_iterator operator--(int) { stamp_input_iterator tmp = *this; --(*this); return tmp; }

    friend bool operator==(const stamp_input_iterator& a, const stamp_input_iterator& b) { return a.m_internal == b.m_internal; };
    friend bool operator!=(const stamp_input_iterator& a, const stamp_input_iterator& b) { return a.m_internal != b.m_internal; };

    /**
     * @lang{ZH}
     * 回滚到初始位置。
     *
     * 将所有记录的字符放回流缓冲区。
     * @endif
     *
     * @lang{EN}
     * Rollback to the initial position.
     *
     * Puts back all recorded characters to the stream buffer.
     * @endif
     */
    void rollback()
    {
        while (!m_rec.empty())
        {
            m_internal.sputbackc(m_rec.front());
            m_rec.pop_front();
        }
    }

    /**
     * @lang{ZH}
     * 获取底层迭代器。
     *
     * @return 底层迭代器的副本
     * @endif
     *
     * @lang{EN}
     * Get the underlying iterator.
     *
     * @return A copy of the underlying iterator
     * @endif
     */
    [[nodiscard]] TIter internal() const { return m_internal; }
private:
    TIter m_internal;
    std::forward_list<value_type> m_rec;
};

/// @cond INTERNAL
template <typename TIter>
stamp_input_iterator(TIter) -> stamp_input_iterator<std::remove_reference_t<TIter>>;

template <typename TIter>
stamp_input_iterator(stamp_input_iterator<TIter>&) -> stamp_input_iterator<TIter>;
/// @endcond

/**
 * @lang{ZH}
 * 检查类型 T 是否为 stamp_input_iterator 特化。
 *
 * @tparam T 要检查的类型
 * @endif
 *
 * @lang{EN}
 * Checks if type T is a stamp_input_iterator specialization.
 *
 * @tparam T The type to check
 * @endif
 */
template <typename T>
constexpr static bool is_stamp_input_iterator_v = false;

/// @cond INTERNAL
template <typename T>
constexpr static bool is_stamp_input_iterator_v<stamp_input_iterator<T>> = true;
/// @endcond
}
