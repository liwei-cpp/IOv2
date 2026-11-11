// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

// A device that stops short of a chosen byte, fails once when asked for it, then carries
// on from that byte; and the check that a converter over it hands over what it had,
// throws on the next get, then delivers the rest. For the deferred-error tests of the
// converters' get_main().

#include <IOv2/common/defs.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace
{
    using namespace IOv2;

    class fail_once_device
    {
    public:
        using char_type = char;

        fail_once_device(std::string str, std::size_t fail_at)
            : m_str(std::move(str))
            , m_fail_at(fail_at)
        {}

        bool deof() const { return m_pos >= m_str.size(); }

        std::size_t dget(char* s, std::size_t n)
        {
            if (n == 0 || m_pos >= m_str.size())
                return 0;
            if (m_pos == m_fail_at && !m_failed)
            {
                m_failed = true;
                throw device_error("fail_once_device::dget: forced failure");
            }
            const std::size_t end = (!m_failed && m_pos < m_fail_at) ? m_fail_at : m_str.size();
            const std::size_t count = std::min(n, end - m_pos);
            std::copy_n(m_str.data() + m_pos, count, s);
            m_pos += count;
            return count;
        }

        // zlib_cvt names the write side even when it only reads; nothing here writes.
        // Not an unconditional throw: once inlined, cl would call the code after the
        // call in root_cvt's put_buf unreachable (C4702).
        void dput(const char*, std::size_t n)
        {
            if (n != 0)
                throw device_error("fail_once_device: read only");
        }
        void dflush() {}

    private:
        std::string m_str;
        std::size_t m_pos     = 0;
        std::size_t m_fail_at = 0;
        bool        m_failed  = false;
    };

    // The first get stops at the failure with what it had, which must be a non-empty
    // prefix; the error comes on the next get; then the rest follows.
    template <typename T, typename C>
    void expect_the_prefix_then_the_error_then_the_rest(T& obj, const std::basic_string<C>& plain)
    {
        obj.main_cont_beg();

        std::basic_string<C> buf(plain.size(), C{});
        const std::size_t first = obj.get(buf.data(), buf.size());
        ASSERT_GT(first, 0u);
        ASSERT_LT(first, plain.size());
        EXPECT_EQ(buf.substr(0, first), plain.substr(0, first));
        EXPECT_FALSE(obj.is_eof());

        EXPECT_THROW(obj.get(buf.data(), buf.size()), device_error);

        std::basic_string<C> got = plain.substr(0, first);
        for (;;)
        {
            const std::size_t n = obj.get(buf.data(), buf.size());
            if (n == 0)
                break;
            got.append(buf.data(), n);
        }
        EXPECT_EQ(got, plain);
        EXPECT_TRUE(obj.is_eof());
    }
}
