// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Positions under newline_cvt count characters of the layer above: a `\r\n`
 * is one character, so having written "a\nb", tell() is 3.
 *
 * How many characters a stretch holds is only known by reading it, so the
 * converter supports exactly two repositionings: seek(0) while reading, and
 * rseek(0), which reads to the end counting. Everything else is refused, and
 * tell() is exact after every repositioning that is allowed -- the checks
 * below compare it with what reading actually delivers.
 *
 * The converter under real streams is tested in io/newline_cvt/.
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/newline_cvt.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

using namespace IOv2;

namespace
{
    // Read-write and positionable; its second dget throws once. The first hands
    // out two characters, so a get can deliver them and then meet the error.
    class flaky_device
    {
    public:
        using char_type = char;

        explicit flaky_device(std::string data) : m_data(std::move(data)) {}

        const std::string& str() const { return m_data; }

        std::size_t dget(char* p, std::size_t n)
        {
            ++m_calls;
            if (m_calls == 2)
                throw device_error("flaky_device: forced failure");
            if (m_calls == 1)
                n = std::min<std::size_t>(n, 2);
            n = std::min(n, m_data.size() - m_pos);
            m_data.copy(p, n, m_pos);
            m_pos += n;
            return n;
        }
        bool deof() const { return m_pos == m_data.size(); }
        void dput(const char* p, std::size_t n)
        {
            m_data.replace(m_pos, std::min(n, m_data.size() - m_pos), p, n);
            m_pos += n;
        }
        void dflush() {}
        std::size_t dtell() const { return m_pos; }
        std::size_t dsize() const { return m_data.size(); }
        void dseek(std::size_t p) { m_pos = p; }
        void drseek(std::size_t p) { m_pos = m_data.size() - p; }

    private:
        std::string m_data;
        std::size_t m_pos = 0;
        int         m_calls = 0;
    };

    auto open(std::string src)
    {
        auto c = newline_cvt_creator{}.create(rb_root_cvt{mem_device<char>{std::move(src)}});
        c.bos();
        c.main_cont_beg();
        return c;
    }

    template <typename Cvt>
    std::string read_rest(Cvt& c)
    {
        std::string out;
        char buf[5];
        while (const std::size_t n = c.get(buf, sizeof buf))
            out.append(buf, n);
        return out;
    }
}

TEST(NewlineCvtPositioning, TellCountsCharactersOfThisLayer)
{
    auto w = open("");
    w.put("a\nb", 3);
    EXPECT_EQ(w.tell(), 3u);

    auto r = open("x\r\ny\r");
    char ch = 0;
    ASSERT_EQ(r.get(&ch, 1), 1u);
    EXPECT_EQ(r.tell(), 1u);
    ASSERT_EQ(r.get(&ch, 1), 1u);
    EXPECT_EQ(ch, '\n');
    EXPECT_EQ(r.tell(), 2u);
    EXPECT_EQ(read_rest(r), "y\r");
    EXPECT_EQ(r.tell(), 4u);
}

// A \r held back for the next character has not been delivered yet.
TEST(NewlineCvtPositioning, AHeldCrIsNotCounted)
{
    auto c = open("a\r");
    char ch = 0;
    ASSERT_EQ(c.get(&ch, 1), 1u);
    EXPECT_EQ(c.tell(), 1u);
}

TEST(NewlineCvtPositioning, SeekZeroWhileReading)
{
    auto c = open("a\r\nb");
    EXPECT_EQ(read_rest(c), "a\nb");
    c.seek(0);
    EXPECT_EQ(c.tell(), 0u);
    EXPECT_EQ(read_rest(c), "a\nb");
}

TEST(NewlineCvtPositioning, OtherSeeksAreRefused)
{
    auto r = open("a\r\nb");
    EXPECT_THROW(r.seek(1), cvt_error);
    EXPECT_THROW(r.rseek(1), cvt_error);
    r.seek(r.tell());                // the current position: nothing to do
    EXPECT_FALSE(r.is_tainted());

    auto w = open("");
    w.seek(0);                       // empty while writing: nothing to do
    w.put("ab", 2);
    EXPECT_THROW(w.seek(0), cvt_error);
    EXPECT_THROW(w.rseek(1), cvt_error);
    EXPECT_FALSE(w.is_tainted());
}

TEST(NewlineCvtPositioning, RseekZeroWhileWritingStaysAtTheEnd)
{
    auto c = open("");
    c.put("ab\n", 3);
    c.rseek(0);
    EXPECT_EQ(c.tell(), 3u);
    c.put("c", 1);
    auto [dev, err] = c.detach();
    EXPECT_EQ(dev.str(), "ab\r\nc");
}

// From the middle, rseek(0) reads the rest counting; writing then appends, and
// tell() goes on from the exact count.
TEST(NewlineCvtPositioning, RseekZeroWhileReadingCountsToTheEnd)
{
    auto c = open("ab\r\ncd\r");
    char ch = 0;
    ASSERT_EQ(c.get(&ch, 1), 1u);
    c.rseek(0);
    EXPECT_EQ(c.tell(), 6u);
    EXPECT_TRUE(c.is_eof());
    c.put("Z\n", 2);
    EXPECT_EQ(c.tell(), 8u);
    auto [dev, err] = c.detach();
    EXPECT_FALSE(err);
    EXPECT_EQ(dev.str(), "ab\r\ncd\rZ\r\n");
}

// Counting takes 1024 characters at a time; a \r\n split by that boundary is
// still one character, and the count agrees with what reading delivers.
TEST(NewlineCvtPositioning, RseekZeroAgreesWithReading)
{
    for (const std::size_t at : {1022u, 1023u, 1024u, 2047u})
    {
        std::string in(at, 'x');
        in += "\r\n";
        in += std::string(1500, 'y');
        in += "\r";
        auto c = open(in);
        c.rseek(0);
        const std::size_t counted = c.tell();
        c.seek(0);
        EXPECT_EQ(counted, read_rest(c).size()) << "at " << at;
        EXPECT_EQ(counted, at + 1 + 1500 + 1) << "at " << at;
    }
}

// A get that delivered characters and then met an error from below holds the
// error back for the next get. A repositioning drops it, rseek(0) included.
TEST(NewlineCvtPositioning, RseekZeroDropsAHeldBackError)
{
    newline_cvt<no_rb_root_cvt<flaky_device>> c{no_rb_root_cvt{flaky_device{"abc\r\nd"}}};
    c.bos();
    c.main_cont_beg();
    char buf[10];
    ASSERT_EQ(c.get(buf, sizeof buf), 2u);
    c.rseek(0);
    EXPECT_EQ(c.tell(), 5u);
    std::size_t n = 99;
    EXPECT_NO_THROW(n = c.get(buf, sizeof buf));
    EXPECT_EQ(n, 0u);
    EXPECT_TRUE(c.is_eof());
}

// Above a UTF-8 code_cvt, which refuses rseek itself, rseek(0) still works: it
// reads to the end rather than repositioning below.
TEST(NewlineCvtPositioning, RseekZeroAboveAVariableLengthEncoding)
{
    auto c = newline_cvt_creator{}.create(
        code_cvt_creator<char8_t, char32_t>{}.create(rb_root_cvt{mem_device<char8_t>{u8"李\r\n伟"}}));
    c.bos();
    c.main_cont_beg();
    c.rseek(0);
    EXPECT_EQ(c.tell(), 3u);
    c.put(U"\n", 1);
    auto [dev, err] = c.detach();
    EXPECT_FALSE(err);
    EXPECT_EQ(dev.str(), u8"李\r\n伟\r\n");
}
