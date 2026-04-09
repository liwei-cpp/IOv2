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
    
    StampInputIterator(TIter& internal)
        : m_internal(internal)
        , m_pos(0) {}

    StampInputIterator(std::default_sentinel_t)
        : m_internal() {}
    
    using value_type        = typename TIter::value_type;
    using difference_type   = typename TIter::difference_type;
    using pointer           = typename TIter::pointer;
    using reference         = typename TIter::reference;
    using iterator_category = typename TIter::iterator_category;

    reference operator*() const { return *m_internal; }
    pointer operator->() const { return m_internal; }

    reference operator[] (difference_type n) const
        requires (std::random_access_iterator<TIter>)
    { return m_internal[n]; }
    
    StampInputIterator& operator++() { ++m_internal; ++m_pos; return *this; }
    StampInputIterator operator++(int) { StampInputIterator tmp = *this; ++(*this); return tmp; }
    StampInputIterator& operator--() { --m_internal; --m_pos; return *this; }
    StampInputIterator operator--(int) { StampInputIterator tmp = *this; --(*this); return tmp; }
    
    StampInputIterator& operator += (difference_type n)
        requires (std::random_access_iterator<TIter>)
    {
        m_internal += n;
        m_pos += n;
        return *this;
    }
    
    StampInputIterator& operator -= (difference_type n)
        requires (std::random_access_iterator<TIter>)
    {
        m_internal -= n;
        m_pos -= n;
        return *this;
    }

    friend bool operator== (const StampInputIterator& a, const StampInputIterator& b) { return a.m_internal == b.m_internal; };
    friend bool operator!= (const StampInputIterator& a, const StampInputIterator& b) { return a.m_internal != b.m_internal; }; 
    friend bool operator< (const StampInputIterator& a, const StampInputIterator& b)
        requires (std::random_access_iterator<TIter>) { return a.m_internal < b.m_internal; };
    friend bool operator> (const StampInputIterator& a, const StampInputIterator& b)
        requires (std::random_access_iterator<TIter>) { return a.m_internal > b.m_internal; };
    friend bool operator<= (const StampInputIterator& a, const StampInputIterator& b)
        requires (std::random_access_iterator<TIter>) { return a.m_internal <= b.m_internal; };
    friend bool operator>= (const StampInputIterator& a, const StampInputIterator& b)
        requires (std::random_access_iterator<TIter>) { return a.m_internal >= b.m_internal; };
    
    friend StampInputIterator operator+(const StampInputIterator& it, difference_type n)
        requires (std::random_access_iterator<TIter>)
    {
        StampInputIterator tmp = it;
        tmp += n;
        return tmp;
    }
    
    friend StampInputIterator operator+(difference_type n, const StampInputIterator& it)
        requires (std::random_access_iterator<TIter>)
    {
        return it + n;
    }

    friend StampInputIterator operator-(const StampInputIterator& it, difference_type n)
        requires (std::random_access_iterator<TIter>)
    {
        StampInputIterator tmp = it;
        tmp -= n;
        return tmp;
    }
    
    friend difference_type operator-(const StampInputIterator& lhs, const StampInputIterator& rhs)
        requires (std::random_access_iterator<TIter>)
    {
        return lhs.m_internal - rhs.m_internal;
    }
    
    void rollback()
    {
        if (m_pos) std::advance(m_internal, -m_pos);
    }
    
    TIter internal() const { return m_internal; }
private:
    TIter m_internal;
    int   m_pos;
};

template <is_istreambuf_iterator_v TIter>
struct StampInputIterator<TIter>
{
    StampInputIterator()
        : m_internal() {}
    
    StampInputIterator(TIter& internal)
        : m_internal(internal) {}

    StampInputIterator(std::default_sentinel_t)
        : m_internal() {}
    
    using value_type        = typename TIter::value_type;
    using difference_type   = typename TIter::difference_type;

    auto operator*() const { return *m_internal; }
    auto operator->() const { return m_internal; }

    StampInputIterator& operator++()
    {
        m_rec.push_front(*m_internal);
        ++m_internal;
        return *this;
    }
    StampInputIterator operator++(int) { StampInputIterator tmp = *this; ++(*this); return tmp; }

    StampInputIterator& operator--()
    {
        if (m_rec.empty())
            throw std::runtime_error("StampInputIterator fail, cannot move backward");

        m_internal.sputbackc(m_rec.front());
        m_rec.pop_front();
        return *this;
    }
    StampInputIterator operator--(int) { StampInputIterator tmp = *this; --(*this); return tmp; }

    friend bool operator== (const StampInputIterator& a, const StampInputIterator& b) { return a.m_internal == b.m_internal; };
    friend bool operator!= (const StampInputIterator& a, const StampInputIterator& b) { return a.m_internal != b.m_internal; };  
    void rollback()
    {
        while (!m_rec.empty())
        {
            m_internal.sputbackc(m_rec.front());
            m_rec.pop_front();
        }
    }
    
    TIter internal() const { return m_internal; }
private:
    TIter m_internal;
    std::forward_list<value_type> m_rec;
};

template <typename TIter>
StampInputIterator(TIter&) -> StampInputIterator<TIter>;

template <typename TIter>
StampInputIterator(StampInputIterator<TIter>&) -> StampInputIterator<TIter>;

template <typename T>
constexpr static bool IsStampInputIterator = false;

template <typename T>
constexpr static bool IsStampInputIterator<StampInputIterator<T>> = true;
}