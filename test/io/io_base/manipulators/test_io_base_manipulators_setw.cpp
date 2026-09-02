// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * setw, whose argument has the same underflow hazard as ios_base::width.
 *
 * A width is computed by subtraction more often than it is written literally,
 * so a value that has already wrapped past zero is the case that matters. The
 * manipulator only stores what it was given; the rejection happens when it is
 * applied, and reaches the caller through the stream's error model rather than
 * as a throw out of the manipulator.
 *
 * The width is also a one-shot: consumed by the next formatted operation and
 * reset. On the extraction side it is an upper bound instead, so an enormous
 * one is legal and simply reads no more than the input holds.
 */
#include <common/defs.h>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <string>

using namespace IOv2;

namespace
{
    // setw() only stores its argument; the rejection happens when the manipulator is
    // applied to a stream, so it goes through the stream's error model like any other
    // failure. Returns true when applying the width was rejected.
    template <typename TStream, typename TWidth>
    bool rejects_width(TStream& s, TWidth n)
    {
        const size_t before = s.width();
        s << setw(n);
        const bool rejected = s.str_fail() && s.width() == before;
        s.clear();
        return rejected;
    }
}

// Negative widths are rejected, including the ones that reach setw() already
// wrapped as unsigned.
TEST(IoBaseManipSetw, ANegativeWidthIsRejectedHoweverItIsSpelled)
{
    ostream oss{mem_device{""}};
    oss.locale(locale<char>("C"));

    EXPECT_TRUE(rejects_width(oss, -1));

    {
        int n = -3;
        EXPECT_TRUE(rejects_width(oss, n));
    }
    {
        long long n = -1;
        EXPECT_TRUE(rejects_width(oss, n));
    }

    // The classic form: the subtraction wraps as unsigned before setw() is
    // called, so the value it receives is near 2^64 rather than negative.
    {
        int total = 10;
        std::string label(20, 'x');
        EXPECT_TRUE(rejects_width(oss, total - label.size()));
    }
    // Same value, computed and stored first.
    {
        int total = 10;
        std::string label(20, 'x');
        size_t w = total - label.size();
        EXPECT_TRUE(rejects_width(oss, w));
    }
    {
        size_t a = 10, b = 20;
        EXPECT_TRUE(rejects_width(oss, a - b));
    }
    // A size_t at or above 2^63 maps to a negative ptrdiff_t and is rejected too.
    {
        size_t w = size_t(1) << 63;
        EXPECT_TRUE(rejects_width(oss, w));
    }
    {
        size_t w = std::numeric_limits<size_t>::max();
        EXPECT_TRUE(rejects_width(oss, w));
    }

    // Constructing the manipulator on its own never throws: the value is only stored.
    (void)setw(-1);
    (void)setw(std::numeric_limits<size_t>::max());
}

// A rejected width is reported through the stream, not thrown at the caller: the
// failure bit is set, the width is left alone, and the rest of the expression is
// skipped because the stream is now failed.
TEST(IoBaseManipSetw, TheRejectionIsReportedThroughTheStream)
{
    ostream oss{mem_device{""}};
    oss.locale(locale<char>("C"));

    oss << "ab";
    EXPECT_EQ(oss.width(), 0);

    // No throw with the default (goodbit) exception mask.
    bool threw = false;
    try { oss << setw(-1) << "cd"; }
    catch (const stream_error&) { threw = true; }

    EXPECT_FALSE(threw);
    EXPECT_FALSE(oss.good());
    EXPECT_TRUE(oss.str_fail());
    EXPECT_EQ(oss.width(), 0);

    // "cd" never made it out: the stream was already failed by then. Had the rejection
    // regressed, the insertion would have emitted ~2^64 fill characters instead.
    oss.clear();
    oss.flush();
    EXPECT_EQ(oss.device().str(), "ab");

    // With strfailbit in the exception mask the same failure propagates, as everywhere
    // else in the library.
    {
        ostream throwing{mem_device{""}};
        throwing.locale(locale<char>("C"));
        throwing.exceptions(ios_defs::strfailbit);

        bool caught = false;
        try { throwing << setw(-1); }
        catch (const stream_error&) { caught = true; }
        EXPECT_TRUE(caught);
        EXPECT_TRUE(throwing.str_fail());
    }
}

// Valid widths are stored exactly, including ones far beyond what a 32-bit
// field could hold; padding and the one-shot consumption still work.
TEST(IoBaseManipSetw, AValidWidthIsStoredExactlyAndConsumedOnce)
{
    ostream oss{mem_device{""}};
    oss.locale(locale<char>("C"));

    oss << setw(0);
    EXPECT_EQ(oss.width(), 0);

    // Applied but never inserted with: these only have to survive unchanged.
    oss << setw(50000000);
    EXPECT_EQ(oss.width(), 50000000u);

    oss << setw(size_t(1) << 40);
    EXPECT_EQ(oss.width(), (size_t(1) << 40));

    oss << setw(std::numeric_limits<std::ptrdiff_t>::max());
    EXPECT_EQ(oss.width(), static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max()));

    oss.width(0);

    oss << setfill('*') << setw(6) << "ab";
    oss.flush();
    EXPECT_EQ(oss.device().str(), "****ab");
    EXPECT_EQ(oss.width(), 0);
}

// The extraction side takes width as an upper bound only, so an arbitrarily
// large one is legal and reads no more than the input holds.
TEST(IoBaseManipSetw, OnExtractionTheWidthIsOnlyAnUpperBound)
{
    {
        istream iss{mem_device{"short text"}};
        iss.locale(locale<char>("C"));
        std::string s;
        iss >> setw(1000000) >> s;
        EXPECT_EQ(s, "short");
        EXPECT_EQ(iss.width(), 0);
    }
    {
        istream iss{mem_device{"short text"}};
        iss.locale(locale<char>("C"));
        std::string s;
        iss >> setw(size_t(1) << 40) >> s;
        EXPECT_EQ(s, "short");
    }
    // width still tightens a character array's own bound.
    {
        istream iss{mem_device{"abcdefghij"}};
        iss.locale(locale<char>("C"));
        char buf[8] = {};
        iss >> setw(3) >> buf;
        EXPECT_EQ(std::string(buf), "ab");
    }
    {
        istream iss{mem_device{"abcdefghij"}};
        iss.locale(locale<char>("C"));
        char buf[8] = {};
        iss >> setw(1000) >> buf;
        EXPECT_EQ(std::string(buf), "abcdefg");
    }
}

TEST(IoBaseManipSetw, TheSameRulesHoldOnAWideStream)
{
    ostream woss{mem_device{L""}};
    woss.locale(locale<wchar_t>("C"));

    EXPECT_TRUE(rejects_width(woss, -1));
    {
        int total = 10;
        std::wstring label(20, L'x');
        EXPECT_TRUE(rejects_width(woss, total - label.size()));
    }

    woss << setw(size_t(1) << 40);
    EXPECT_EQ(woss.width(), (size_t(1) << 40));
    woss.width(0);

    woss << setfill(L'*') << setw(6) << L"ab";
    woss.flush();
    EXPECT_EQ(woss.device().str(), L"****ab");
    EXPECT_EQ(woss.width(), 0);

    bool threw = false;
    try { woss << setw(-1) << L"cd"; }
    catch (const stream_error&) { threw = true; }
    EXPECT_FALSE(threw);
    EXPECT_TRUE(woss.str_fail());
    woss.clear();
    woss.flush();
    EXPECT_EQ(woss.device().str(), L"****ab");
}
