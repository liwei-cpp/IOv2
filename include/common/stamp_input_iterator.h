#pragma once
#include <common/streambuf_defs.h>

#include <algorithm>
#include <forward_list>
#include <iterator>
#include <stdexcept>

namespace IOv2
{
template <typename TIter>
struct stamp_input_iterator;

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
    
    void rollback()
    {
        if (m_pos) std::advance(m_internal, -m_pos);
        m_pos = 0;
    }
    
    [[nodiscard]] TIter internal() const { return m_internal; }
private:
    TIter           m_internal;
    difference_type m_pos;
};

/**
 * @brief stamp_input_iterator specialization for IOv2 istreambuf_iterator.
 *
 * @note This specialization is designed specifically for IOv2::istreambuf_iterator
 *       and is NOT compatible with std::istreambuf_iterator. Key differences:
 *
 *       1. IOv2's sputbackc() returns void and never fails (uses unlimited deque)
 *       2. std::streambuf's sputbackc() returns int_type and may fail (limited putback area)
 *
 *       The istreambuf_iterator concept constraint ensures this specialization
 *       only matches IOv2::istreambuf_iterator types.
 *
 * @tparam TIter Must satisfy istreambuf_iterator (IOv2::istreambuf_iterator only)
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
    void rollback()
    {
        while (!m_rec.empty())
        {
            m_internal.sputbackc(m_rec.front());
            m_rec.pop_front();
        }
    }
    
    [[nodiscard]] TIter internal() const { return m_internal; }
private:
    TIter m_internal;
    std::forward_list<value_type> m_rec;
};

template <typename TIter>
stamp_input_iterator(TIter) -> stamp_input_iterator<std::remove_reference_t<TIter>>;

template <typename TIter>
stamp_input_iterator(stamp_input_iterator<TIter>&) -> stamp_input_iterator<TIter>;

template <typename T>
constexpr static bool is_stamp_input_iterator_v = false;

template <typename T>
constexpr static bool is_stamp_input_iterator_v<stamp_input_iterator<T>> = true;
}