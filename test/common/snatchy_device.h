#pragma once
#include <memory>
#include <string>
#include <type_traits>

#include <common/defs.h>
#include <device/device_concepts.h>
template <class CharT, size_t MaxGetLen>
class snatchy_device
{
    static_assert(MaxGetLen != 0);

public:
    using char_type = CharT;
    
public:
    snatchy_device(std::basic_string<CharT> info = std::basic_string<CharT>{})
        : m_str(std::move(info))
        , m_next_pos(0)
    {}

    snatchy_device(const snatchy_device&) = default;
    snatchy_device(snatchy_device&&) = default;
    snatchy_device& operator= (const snatchy_device&) = default;
    snatchy_device& operator= (snatchy_device&&) = default;

    const std::basic_string<CharT>& str() const { return m_str; }

public:
    bool deos() const
    {
        return (m_next_pos >= m_str.size());
    }

    size_t dget(char_type* s, size_t n)
    {
        std::size_t res = std::min(m_str.size() - m_next_pos, n);
        res = std::min(res, MaxGetLen);
        std::copy(m_str.data() + m_next_pos, m_str.data() + m_next_pos + res, s);
        m_next_pos += res;
        return res;
    }
    
    size_t dtell() const
    {
        return m_next_pos;
    }
    
    void dseek(size_t v)
    {
        if (v > m_str.size())
            throw IOv2::device_error("snatchy_device::seek fail: out of boundary");
        m_next_pos = v;
    }

    void drseek(size_t offset)
    {
        if (offset > m_str.size())
            throw IOv2::device_error("snatchy_device::rseek fail: out of boundary");
        m_next_pos = m_str.size() - offset;
    }
private:
    std::basic_string<CharT> m_str;
    size_t m_next_pos = 0;
};
