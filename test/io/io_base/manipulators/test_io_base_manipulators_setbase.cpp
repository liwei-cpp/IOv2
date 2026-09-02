// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * setbase, whose whole job is to map a number to a basefield flag.
 *
 * Only 8, 10 and 16 name a base; every other value clears basefield, which is
 * not an error. The parameter is a std::ptrdiff_t rather than an int so that a
 * wider argument cannot be truncated onto one of those three and silently look
 * like it selected that base -- the tests below pass values whose low 32 bits
 * are 8, 10 and 16 to pin that down.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <string>

using namespace IOv2;

namespace
{
    // Applies setbase(base) and returns what the stream then makes of 255. The stream is
    // required to stay good throughout: no value of `base` is an error.
    template <typename TBase>
    std::string based(TBase base)
    {
        ostream oss{mem_device{""}};
        oss.locale(locale<char>("C"));

        oss << setbase(base) << 255;
        EXPECT_TRUE(oss.good());

        auto [dev, err] = oss.detach();
        return dev.str();
    }

    ios_defs::fmtflags base_of(std::ptrdiff_t base)
    {
        ostream oss{mem_device{""}};
        oss.locale(locale<char>("C"));

        oss << setbase(base);
        EXPECT_TRUE(oss.good());

        return oss.flags() & ios_defs::basefield;
    }
}

// 8, 10 and 16 select oct / dec / hex; every other value clears basefield, which is not an
// error and leaves the stream good.
TEST(IoBaseManipSetbase, OnlyEightTenAndSixteenNameABase)
{
    EXPECT_EQ(base_of(8), ios_defs::oct);
    EXPECT_EQ(base_of(10), ios_defs::dec);
    EXPECT_EQ(base_of(16), ios_defs::hex);

    EXPECT_EQ(based(8), "377");
    EXPECT_EQ(based(10), "255");
    EXPECT_EQ(based(16), "ff");

    // Anything else clears basefield outright. Output is then decimal, same as dec, but the
    // flag state is distinct from what setbase(10) leaves behind.
    for (std::ptrdiff_t base : {std::ptrdiff_t{0}, std::ptrdiff_t{1}, std::ptrdiff_t{2},
                                std::ptrdiff_t{7}, std::ptrdiff_t{9}, std::ptrdiff_t{15},
                                std::ptrdiff_t{17}, std::ptrdiff_t{-8}})
    {
        EXPECT_EQ(base_of(base), ios_defs::fmtflags(0));
        EXPECT_EQ(based(base), "255");
    }

}

// The parameter is a std::ptrdiff_t so that a wider argument cannot be truncated onto 8, 10
// or 16 and thereby look like it selected that base.
TEST(IoBaseManipSetbase, AWiderArgumentIsNotTruncatedOntoAValidBase)
{
    // Each of these has the low 32 bits of a valid base. Narrowed to int they would select
    // oct / dec / hex; kept whole they are just "some other value".
    EXPECT_EQ(base_of(8LL  + (1LL << 32)), ios_defs::fmtflags(0));
    EXPECT_EQ(base_of(10LL + (1LL << 32)), ios_defs::fmtflags(0));
    EXPECT_EQ(base_of(16LL + (1LL << 32)), ios_defs::fmtflags(0));

    EXPECT_EQ(based(8LL  + (1LL << 32)), "255");
    EXPECT_EQ(based(10LL + (1LL << 32)), "255");
    EXPECT_EQ(based(16LL + (1LL << 32)), "255");

    // The stored value is the one that was passed in, not a truncation of it.
    EXPECT_EQ(setbase(8LL + (1LL << 32)).m_base, 8LL + (1LL << 32));
    EXPECT_EQ(setbase(-1).m_base, -1);

    // Extreme values are stored and applied like any other non-base, without failing.
    EXPECT_EQ(base_of(std::numeric_limits<std::ptrdiff_t>::max()), ios_defs::fmtflags(0));
    EXPECT_EQ(base_of(std::numeric_limits<std::ptrdiff_t>::min()), ios_defs::fmtflags(0));

}

// setbase never fails, so a stream with strfailbit in its exception mask sees nothing thrown
// and stays good, and a later insertion still goes out.
TEST(IoBaseManipSetbase, SetbaseNeverFailsSoAMaskedStreamStaysGood)
{
    ostream oss{mem_device{""}};
    oss.locale(locale<char>("C"));
    oss.exceptions(ios_defs::strfailbit);

    EXPECT_NO_THROW(oss << setbase(7) << 255 << setbase(16) << 255);
    EXPECT_TRUE(oss.good());

    oss.flush();
    EXPECT_EQ(oss.device().str(), "255ff");

}

// The extraction direction goes through the same apply(): 16 reads hex, and a cleared
// basefield makes the base come from the text itself.
TEST(IoBaseManipSetbase, ExtractionGoesThroughTheSameMapping)
{
    // The last token in each device runs to the end of the input, so eofbit is set by the
    // time the value is out; what matters here is that nothing *failed*.
    {
        istream iss{mem_device{std::string("ff")}};
        iss.locale(locale<char>("C"));

        long v = 0;
        iss >> setbase(16) >> v;
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(v, 255);
    }

    // setbase(0) clears basefield, so each token is read by its own prefix.
    {
        istream iss{mem_device{std::string("0x1f 017 42")}};
        iss.locale(locale<char>("C"));

        long hex = 0, oct = 0, dec = 0;
        iss >> setbase(0) >> hex >> oct >> dec;
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(hex, 31);
        EXPECT_EQ(oct, 15);
        EXPECT_EQ(dec, 42);
    }

    // A truncating argument must not turn this into a hex read.
    {
        istream iss{mem_device{std::string("017")}};
        iss.locale(locale<char>("C"));

        long v = 0;
        iss >> setbase(16LL + (1LL << 32)) >> v;
        EXPECT_FALSE(iss.str_fail());
        EXPECT_EQ(v, 15);        // read as octal by its leading 0, not as 0x17
    }

}

TEST(IoBaseManipSetbase, TheSameMappingHoldsOnAWideStream)
{
    ostream woss{mem_device{L""}};
    woss.locale(locale<wchar_t>("C"));

    woss << setbase(16) << 255;
    EXPECT_EQ((woss.flags() & ios_defs::basefield), ios_defs::hex);

    woss << L' ' << setbase(16LL + (1LL << 32)) << 255;
    EXPECT_EQ((woss.flags() & ios_defs::basefield), ios_defs::fmtflags(0));

    EXPECT_TRUE(woss.good());
    auto [dev, err] = woss.detach();
    EXPECT_EQ(dev.str(), L"ff 255");

}
