// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include <IOv2/common/defs.h>

// An input device that hands out one character per dget() and throws device_error on the Nth
// call. The short reads are what give per-character granularity, so the failure can be placed
// in the middle of a token rather than before it -- which is where a reader that abandons its
// work part way through has to be caught.
//
// injectable_device cannot stand in for this: its switch is a boolean, so it fails from the
// first call onwards, and the mem_device behind it answers a whole token in one go.
template <class CharT>
class throwing_get_device
{
public:
    using char_type = CharT;

public:
    throwing_get_device(std::basic_string<CharT> info, std::size_t boom_call)
        : m_str(std::move(info))
        , m_boom(boom_call)
    {}

    throwing_get_device(const throwing_get_device&) = default;
    throwing_get_device(throwing_get_device&&) noexcept = default;
    throwing_get_device& operator=(const throwing_get_device&) = default;
    throwing_get_device& operator=(throwing_get_device&&) noexcept = default;

    const std::basic_string<CharT>& str() const { return m_str; }

public:
    bool deof() const { return m_pos >= m_str.size(); }

    std::size_t dget(char_type* s, std::size_t n)
    {
        ++m_calls;
        if (m_calls == m_boom)
            throw IOv2::device_error("throwing_get_device::dget: forced failure");

        if (n == 0 || m_pos >= m_str.size())
            return 0;

        s[0] = m_str[m_pos++];
        return 1;
    }

    std::size_t dtell() const { return m_pos; }

    void dseek(std::size_t v)
    {
        if (v > m_str.size())
            throw IOv2::device_error("throwing_get_device::dseek fail: out of boundary");
        m_pos = v;
    }

    void drseek(std::size_t offset)
    {
        if (offset > m_str.size())
            throw IOv2::device_error("throwing_get_device::drseek fail: out of boundary");
        m_pos = m_str.size() - offset;
    }

private:
    std::basic_string<CharT> m_str;
    std::size_t              m_pos   = 0;
    std::size_t              m_calls = 0;
    std::size_t              m_boom  = 0;
};
