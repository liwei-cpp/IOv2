// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * A get_main() that fails after it has delivered characters hands them over, and the
 * next get throws the error. Each converter below calls the layer below more than once
 * per get, so a device error on a later call used to throw away what the earlier calls
 * had already decoded -- and iochannel::getn reported 0 for characters it had written.
 *
 * The device stops short of a chosen byte, fails once when asked for it, then carries on
 * from that byte, so the whole text must still come out in order.
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/comp/zlib_cvt.h>
#include <IOv2/cvt/crypt/chacha20_cvt.h>
#include <IOv2/cvt/crypt/vigenere_cvt.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/iochannel.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

using namespace IOv2;

namespace
{
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
        void dput(const char*, std::size_t) { throw device_error("fail_once_device: read only"); }
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

    std::string sample()
    {
        std::string out;
        for (int i = 0; i < 60; ++i)
            out += "the quick brown fox " + std::to_string(i) + "\n";
        return out;
    }

    template <typename Writer>
    std::string encode(Writer&& w, const std::string& plain)
    {
        EXPECT_EQ(w.bos(), io_status::output);
        w.main_cont_beg();
        w.put(plain.data(), plain.size());
        auto [dev, err] = w.detach();
        EXPECT_FALSE(err);
        return dev.str();
    }
}

// Half a character is held when the device fails: it is completed by the bytes after.
TEST(GetMainDeferredError, CodeCvt)
{
    const std::string ext = "ab\xE6\x9D\x8E" "cd";
    code_cvt<no_rb_root_cvt<fail_once_device>, char32_t> obj{no_rb_root_cvt{fail_once_device(ext, 3)}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::input);
    expect_the_prefix_then_the_error_then_the_rest(obj, std::u32string(U"ab李cd"));
}

TEST(GetMainDeferredError, Vigenere)
{
    const std::string plain = sample();
    const std::string ext = encode(Crypt::Classic::vigenere_cvt{no_rb_root_cvt{mem_device("")}, "liwei"}, plain);
    Crypt::Classic::vigenere_cvt obj{no_rb_root_cvt{fail_once_device(ext, ext.size() / 2)}, "liwei"};
    EXPECT_EQ(obj.bos(), io_status::input);
    expect_the_prefix_then_the_error_then_the_rest(obj, plain);
}

TEST(GetMainDeferredError, Chacha20)
{
    const std::string plain = sample();
    const std::string ext = encode(Crypt::chacha20_cvt{no_rb_root_cvt{mem_device("")}, "liwei"}, plain);
    Crypt::chacha20_cvt obj{no_rb_root_cvt{fail_once_device(ext, ext.size() / 2)}, "liwei"};
    EXPECT_EQ(obj.bos(), io_status::input);
    expect_the_prefix_then_the_error_then_the_rest(obj, plain);
}

TEST(GetMainDeferredError, Zlib)
{
    const std::string plain = sample();
    const std::string ext = encode(Comp::zlib_cvt{no_rb_root_cvt{mem_device("")}}, plain);
    Comp::zlib_cvt obj{no_rb_root_cvt{fail_once_device(ext, ext.size() / 2)}};
    EXPECT_EQ(obj.bos(), io_status::input);
    expect_the_prefix_then_the_error_then_the_rest(obj, plain);
}

// What wcin.read() saw: getn's count, on the exception path, is what it wrote.
TEST(GetMainDeferredError, GetnCountsWhatItWroteBeforeTheError)
{
    ichannel ch(fail_once_device("abcdef", 3), code_cvt_creator<char, char32_t>("zh_CN.UTF-8"));

    std::u32string buf(10, U'#');
    std::size_t got = 99;
    EXPECT_THROW(ch.getn(buf.data(), buf.size(), &got), device_error);
    EXPECT_EQ(got, 3u);
    EXPECT_EQ(buf.substr(0, 3), U"abc");

    EXPECT_EQ(ch.getn(buf.data(), buf.size(), &got), 3u);
    EXPECT_EQ(buf.substr(0, 3), U"def");
}

