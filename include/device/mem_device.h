#pragma once
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>

#include <common/defs.h>
#include <device/device_concepts.h>
namespace IOv2
{
template <class CharT,
          class Traits = std::char_traits<CharT>,
          class Allocator = std::allocator<CharT>>
class mem_device
{
public:
    using char_type = CharT;
    
public:
    mem_device(std::basic_string<CharT, Traits, Allocator> info = std::basic_string<CharT, Traits, Allocator>{})
        : m_str(std::move(info))
        , m_next_pos(0)
    {}

    mem_device(const mem_device&) = default;
    mem_device(mem_device&&) = default;
    mem_device& operator= (const mem_device&) = default;
    mem_device& operator= (mem_device&&) = default;

    const std::basic_string<CharT, Traits, Allocator>& str() const { return m_str; }

public:
    bool deos() const
    {
        return (m_next_pos >= m_str.size());
    }

    size_t dget(char_type* s, size_t n)
    {
        std::size_t res = std::min(m_str.size() - m_next_pos, n);
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
            throw device_error("mem_device::seek fail: out of boundary");
        m_next_pos = v;
    }

    void drseek(size_t offset)
    {
        if (offset > m_str.size())
            throw device_error("mem_device::rseek fail: out of boundary");
        m_next_pos = m_str.size() - offset;
    }
    
    void dput(const char_type* ch, size_t n)
    {
        if (m_next_pos + n >= m_str.size())
        {
            m_str.erase(m_str.begin() + m_next_pos, m_str.end());
            m_str.reserve(m_next_pos + n);
            std::copy(ch, ch + n, std::back_inserter(m_str));
        }
        else
            std::copy(ch, ch + n, m_str.data() + m_next_pos);
        m_next_pos += n;
    }
    
    void dflush() {}

private:
    std::basic_string<CharT, Traits, Allocator> m_str;
    size_t m_next_pos = 0;
};

template <typename TChar>
mem_device(const TChar []) -> mem_device<TChar>;
}