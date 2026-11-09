// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Writing through newline_cvt, and switching between reading and writing.
 *
 * Every `\n` written goes out as `\r\n` and nothing else changes, so a `\r\n`
 * already in the data becomes `\r\r\n` and reads back as `\r\n`. Writes happen
 * only at the end of the stream: switching from reading needs eof, and a held
 * `\r` is not eof. A write-only layer below cannot be read to tell, so the
 * converter asks it by position instead -- a write-only device over a
 * non-empty stream, opened at the start, has its writes refused.
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
    // Write-only and positionable: the converter can only learn whether it stands
    // at the end by comparing positions.
    class write_only_device
    {
    public:
        using char_type = char;

        explicit write_only_device(std::string data = {}, std::size_t pos = 0)
            : m_data(std::move(data)), m_pos(pos) {}

        const std::string& str() const { return m_data; }

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
        std::size_t m_pos;
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

    template <typename Cvt>
    auto contents(Cvt& c)
    {
        auto [dev, err] = c.detach();
        EXPECT_FALSE(err);
        return dev.str();
    }
}

TEST(NewlineCvtWrite, LfBecomesCrLf)
{
    auto c = open("");
    c.put("a\nb\r\n\n", 6);
    EXPECT_EQ(contents(c), "a\r\nb\r\r\n\r\n");
}

TEST(NewlineCvtWrite, WhatIsWrittenReadsBack)
{
    const std::string text = "x\r\ny\rz\n\n" + std::string(200, 'w') + "\n";
    auto c = open("");
    c.put(text.data(), text.size());
    c.seek(c.tell());                // nothing to do: already there
    c.switch_to_get();
    c.seek(0);
    EXPECT_EQ(read_rest(c), text);
}

TEST(NewlineCvtWrite, AboveAVariableLengthEncoding)
{
    auto c = newline_cvt_creator{}.create(
        code_cvt_creator<char8_t, char32_t>{}.create(rb_root_cvt{mem_device<char8_t>{}}));
    c.bos();
    c.main_cont_beg();
    c.put(U"李\n伟", 3);
    EXPECT_EQ(contents(c), u8"李\r\n伟");
}

TEST(NewlineCvtWrite, ReadingToTheEndThenWritingAppends)
{
    auto c = open("x\r\ny");
    EXPECT_EQ(read_rest(c), "x\ny");
    c.put("\nz", 2);
    EXPECT_EQ(contents(c), "x\r\ny\r\nz");
}

TEST(NewlineCvtWrite, WritingReadingAndWritingAgainStaysAtTheEnd)
{
    auto c = open("");
    c.put("a\n", 2);
    EXPECT_EQ(read_rest(c), "");     // the switch happens at the end: eof at once
    c.put("b", 1);
    EXPECT_EQ(contents(c), "a\r\nb");
}

// Switching mid-stream is refused before anything changes: no taint, and reading
// goes on from where it was.
TEST(NewlineCvtWrite, SwitchingBeforeTheEndIsRefused)
{
    auto c = open("ab\r\nc");
    char ch = 0;
    ASSERT_EQ(c.get(&ch, 1), 1u);
    EXPECT_THROW(c.put("X", 1), cvt_error);
    EXPECT_FALSE(c.is_tainted());
    EXPECT_EQ(read_rest(c), "b\nc");
}

// A \r held back at the end of the data is not eof until it has been read.
TEST(NewlineCvtWrite, AHeldCrIsNotTheEnd)
{
    auto c = open("a\r");
    char ch = 0;
    ASSERT_EQ(c.get(&ch, 1), 1u);
    EXPECT_THROW(c.put("X", 1), cvt_error);
    EXPECT_EQ(read_rest(c), "\r");
    c.put("X", 1);
    EXPECT_EQ(contents(c), "a\rX");
}

TEST(NewlineCvtWrite, WriteOnlyOverAnEmptyStream)
{
    newline_cvt<rb_root_cvt<write_only_device>> c{rb_root_cvt{write_only_device{}}};
    c.bos();
    c.main_cont_beg();
    c.put("a\n", 2);
    c.rseek(0);                      // already at the end
    c.put("b", 1);
    EXPECT_EQ(contents(c), "a\r\nb");
}

// Opened at the start of existing content, a write-only stream may not write:
// that would overwrite \r\n pairs. The probe leaves the device where it was.
TEST(NewlineCvtWrite, WriteOnlyOverExistingContentIsRefused)
{
    newline_cvt<rb_root_cvt<write_only_device>> c{rb_root_cvt{write_only_device{"old\r\n"}}};
    c.bos();
    c.main_cont_beg();
    EXPECT_THROW(c.put("a", 1), cvt_error);
    EXPECT_TRUE(c.is_tainted());
    auto [dev, err] = c.detach();
    EXPECT_EQ(dev.str(), "old\r\n");
    EXPECT_EQ(dev.dtell(), 0u);
}

// The same stream standing at the end of the existing content writes normally.
TEST(NewlineCvtWrite, WriteOnlyAtTheEndOfExistingContent)
{
    newline_cvt<rb_root_cvt<write_only_device>> c{rb_root_cvt{write_only_device{"old\r\n", 5}}};
    c.bos();
    c.main_cont_beg();
    c.put("new\n", 4);
    EXPECT_EQ(contents(c), "old\r\nnew\r\n");
}
