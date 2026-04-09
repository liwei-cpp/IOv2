#pragma once
#include <iterator>
#include <optional>
#include <common/streambuf_defs.h>
#include <io/streambuf.h>

namespace IOv2
{
// https://github.com/gcc-mirror/gcc/blob/ce28eb9f91e39a95a499eea36865e394c2bf614b/libstdc%2B%2B-v3/include/bits/streambuf_iterator.h
template <typename TStreamBuf>
    requires (is_streambuf<TStreamBuf> || is_istreambuf<TStreamBuf>)
class istreambuf_iterator
{
public:
    using value_type = typename TStreamBuf::char_type;
    using difference_type = std::ptrdiff_t;

public:
    constexpr istreambuf_iterator()
        : m_streambuf(nullptr) {}

    constexpr istreambuf_iterator(std::default_sentinel_t) noexcept
      : istreambuf_iterator() {}
      
    istreambuf_iterator(TStreamBuf& p_streambuf)
        : m_streambuf(&p_streambuf) {}

public:
    value_type operator*() const
    {
        auto res = get();
        if (!res.has_value()) throw eof_error{};
        return res.value();
    }

    istreambuf_iterator& operator++ ()
    {
        if (m_streambuf)
            m_streambuf->sbumpc();
        m_c = std::optional<value_type>{};
        return *this;
    }

    istreambuf_iterator operator++(int)
    {
        istreambuf_iterator __old = *this;
        __old.m_c = m_streambuf->sbumpc();
        m_c = std::optional<value_type>{};
        return __old;
    }

    void sputbackc(value_type ch)
    {
        if (!m_streambuf)
            throw cvt_error("put back fails");

        if (m_c.has_value())
        {
            m_streambuf->sputbackc(m_c.value());
            m_c = std::optional<value_type>{};
        }
        m_streambuf->sputbackc(ch);
    }

    friend bool operator==(const istreambuf_iterator& val1, const istreambuf_iterator& val2)
    {
        const bool v1 = val1.get().has_value();
        const bool v2 = val2.get().has_value();
        return v1 == v2;
    }

private:
    std::optional<value_type> get() const
    {
        auto ret = m_c;
        if (m_streambuf && (!ret.has_value()))
        {
            ret = m_streambuf->sgetc();
            if (!ret.has_value()) m_streambuf = nullptr;
        }
        return ret;
    }

private:
    mutable TStreamBuf* m_streambuf;
    std::optional<value_type> m_c;
};

template <typename TStreamBuf>
    requires (is_streambuf<TStreamBuf> || is_ostreambuf<TStreamBuf>)
class ostreambuf_iterator
{
public:
    using value_type = typename TStreamBuf::char_type;
    using difference_type = ptrdiff_t;

public:
    ostreambuf_iterator(TStreamBuf& p_streambuf)
        : m_streambuf(&p_streambuf) {}

public:
    ostreambuf_iterator& operator*()
    { return *this; }

    ostreambuf_iterator& operator++(int)
    { return *this; }

    ostreambuf_iterator& operator++()
    { return *this; }

    ostreambuf_iterator& operator=(value_type c)
    {
        m_streambuf->sputc(c);
        return *this;
    }

private:
    TStreamBuf* m_streambuf;
};
}