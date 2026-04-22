#pragma once
#include <algorithm>
#include <forward_list>
#include <iterator>
#include <common/streambuf_defs.h>

namespace IOv2
{
template <typename TIter>
struct StampInputIterator;

template <std::bidirectional_iterator TIter>
struct StampInputIterator<TIter>
{
    StampInputIterator()
        : m_internal()
        , m_pos(0) {}
    
    StampInputIterator(TIter internal)
        : m_internal(std::move(internal))
        , m_pos(0) {}

    StampInputIterator(std::default_sentinel_t)
        : m_internal()
        , m_pos(0) {}

    StampInputIterator(const StampInputIterator&) = default;
    StampInputIterator& operator=(const StampInputIterator&) = default;

    StampInputIterator(StampInputIterator&& val) noexcept
        : m_internal(std::move(val.m_internal))
        , m_pos(val.m_pos)
    {
        val.m_pos = 0;
    }

    StampInputIterator& operator=(StampInputIterator&& val) noexcept
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
    
    StampInputIterator& operator++() { ++m_internal; ++m_pos; return *this; }
    [[nodiscard]] StampInputIterator operator++(int) { StampInputIterator tmp = *this; ++(*this); return tmp; }
    StampInputIterator& operator--() { --m_internal; --m_pos; return *this; }
    [[nodiscard]] StampInputIterator operator--(int) { StampInputIterator tmp = *this; --(*this); return tmp; }
    
    StampInputIterator& operator+=(difference_type n)
        requires (std::random_access_iterator<TIter>)
    {
        m_internal += n;
        m_pos += n;
        return *this;
    }
    
    StampInputIterator& operator-=(difference_type n)
        requires (std::random_access_iterator<TIter>)
    {
        m_internal -= n;
        m_pos -= n;
        return *this;
    }

    friend bool operator==(const StampInputIterator& a, const StampInputIterator& b) { return a.m_internal == b.m_internal; };
    friend bool operator!=(const StampInputIterator& a, const StampInputIterator& b) { return a.m_internal != b.m_internal; }; 
    friend bool operator<(const StampInputIterator& a, const StampInputIterator& b)
        requires (std::random_access_iterator<TIter>) { return a.m_internal < b.m_internal; };
    friend bool operator>(const StampInputIterator& a, const StampInputIterator& b)
        requires (std::random_access_iterator<TIter>) { return a.m_internal > b.m_internal; };
    friend bool operator<=(const StampInputIterator& a, const StampInputIterator& b)
        requires (std::random_access_iterator<TIter>) { return a.m_internal <= b.m_internal; };
    friend bool operator>=(const StampInputIterator& a, const StampInputIterator& b)
        requires (std::random_access_iterator<TIter>) { return a.m_internal >= b.m_internal; };
    
    [[nodiscard]] friend StampInputIterator operator+(const StampInputIterator& it, difference_type n)
        requires (std::random_access_iterator<TIter>)
    {
        StampInputIterator tmp = it;
        tmp += n;
        return tmp;
    }
    
    [[nodiscard]] friend StampInputIterator operator+(difference_type n, const StampInputIterator& it)
        requires (std::random_access_iterator<TIter>)
    {
        return it + n;
    }

    [[nodiscard]] friend StampInputIterator operator-(const StampInputIterator& it, difference_type n)
        requires (std::random_access_iterator<TIter>)
    {
        StampInputIterator tmp = it;
        tmp -= n;
        return tmp;
    }
    
    [[nodiscard]] friend difference_type operator-(const StampInputIterator& lhs, const StampInputIterator& rhs)
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
 * @brief StampInputIterator specialization for IOv2 istreambuf_iterator.
 *
 * @note This specialization is designed specifically for IOv2::istreambuf_iterator
 *       and is NOT compatible with std::istreambuf_iterator. Key differences:
 *
 *       1. IOv2's sputbackc() returns void and never fails (uses unlimited deque)
 *       2. std::streambuf's sputbackc() returns int_type and may fail (limited putback area)
 *
 *       The is_istreambuf_iterator_v concept constraint ensures this specialization
 *       only matches IOv2::istreambuf_iterator types.
 *
 * @tparam TIter Must satisfy is_istreambuf_iterator_v (IOv2::istreambuf_iterator only)
 */
template <is_istreambuf_iterator_v TIter>
struct StampInputIterator<TIter>
{
    StampInputIterator()
        : m_internal() {}
    
    StampInputIterator(TIter internal)
        : m_internal(std::move(internal)) {}

    StampInputIterator(std::default_sentinel_t)
        : m_internal() {}
    
    using value_type        = typename TIter::value_type;
    using difference_type   = typename TIter::difference_type;

    [[nodiscard]] auto operator*() const { return *m_internal; }
    [[nodiscard]] auto operator->() const { return m_internal; }

    StampInputIterator& operator++()
    {
        m_rec.push_front(*m_internal);
        ++m_internal;
        return *this;
    }
    [[nodiscard]] StampInputIterator operator++(int) { StampInputIterator tmp = *this; ++(*this); return tmp; }

    StampInputIterator& operator--()
    {
        if (m_rec.empty())
            throw std::runtime_error("StampInputIterator fail, cannot move backward");

        m_internal.sputbackc(m_rec.front());
        m_rec.pop_front();
        return *this;
    }
    [[nodiscard]] StampInputIterator operator--(int) { StampInputIterator tmp = *this; --(*this); return tmp; }

    friend bool operator==(const StampInputIterator& a, const StampInputIterator& b) { return a.m_internal == b.m_internal; };
    friend bool operator!=(const StampInputIterator& a, const StampInputIterator& b) { return a.m_internal != b.m_internal; };  
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
StampInputIterator(TIter) -> StampInputIterator<std::remove_reference_t<TIter>>;

template <typename TIter>
StampInputIterator(StampInputIterator<TIter>&) -> StampInputIterator<TIter>;

template <typename T>
constexpr static bool IsStampInputIterator = false;

template <typename T>
constexpr static bool IsStampInputIterator<StampInputIterator<T>> = true;
}