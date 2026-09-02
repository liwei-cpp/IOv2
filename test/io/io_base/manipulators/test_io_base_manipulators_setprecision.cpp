// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * setprecision, whose argument has a range and whose range check is deferred.
 *
 * The manipulator object only stores what it was given; the check happens when
 * it is applied to a stream, so an out-of-range precision is reported through
 * the stream's error model rather than by throwing out of the manipulator. The
 * precision itself is held in a single byte, so the value that matters is one
 * just past 255 -- narrowing instead of checking would turn 300 into 44.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

using namespace IOv2;

namespace
{
    // setprecision() only stores its argument; the range check happens when the
    // manipulator is applied to a stream, so it goes through the stream's error model
    // like any other failure. Returns true when applying the precision was rejected.
    template <typename TStream, typename TPrec>
    bool rejects_precision(TStream& s, TPrec n)
    {
        const std::uint8_t before = s.precision();
        s << setprecision(n);
        const bool rejected = s.str_fail() && s.precision() == before;
        s.clear();
        return rejected;
    }
}

// The whole 0..255 range is accepted and stored exactly; anything above it is rejected
// rather than wrapping into the low byte.
TEST(IoBaseManipSetprecision, TheWholeByteRangeIsAcceptedAndAnythingAboveItRefused)
{
    ostream oss{mem_device{""}};
    oss.locale(locale<char>("C"));

    oss << setprecision(0);
    EXPECT_EQ(oss.precision(), 0);

    oss << setprecision(17);
    EXPECT_EQ(oss.precision(), 17);

    oss << setprecision(255);
    EXPECT_EQ(oss.precision(), 255);

    // 300 would become 44 if it were narrowed instead of checked.
    EXPECT_TRUE(rejects_precision(oss, 300));
    EXPECT_EQ(oss.precision(), 255);

    EXPECT_TRUE(rejects_precision(oss, 256));
    EXPECT_TRUE(rejects_precision(oss, std::numeric_limits<size_t>::max()));
    {
        size_t n = 1000;
        EXPECT_TRUE(rejects_precision(oss, n));
    }

    // Constructing the manipulator on its own never throws: the value is only stored.
    (void)setprecision(300);
    (void)setprecision(std::numeric_limits<size_t>::max());
}

// An out-of-range precision is reported through the stream, not thrown at the caller.
TEST(IoBaseManipSetprecision, TheRefusalIsReportedThroughTheStream)
{
    ostream oss{mem_device{""}};
    oss.locale(locale<char>("C"));

    oss << "ab";

    EXPECT_NO_THROW(oss << setprecision(300) << "cd");
    EXPECT_FALSE(oss.good());
    EXPECT_TRUE(oss.str_fail());

    // "cd" never made it out: the stream was already failed by then.
    oss.clear();
    oss.flush();
    EXPECT_EQ(oss.device().str(), "ab");

    // With strfailbit in the exception mask the same failure propagates.
    {
        ostream throwing{mem_device{""}};
        throwing.locale(locale<char>("C"));
        throwing.exceptions(ios_defs::strfailbit);

        EXPECT_THROW(throwing << setprecision(300), stream_error);
        EXPECT_TRUE(throwing.str_fail());
    }
}

// The extraction direction goes through the same check.
TEST(IoBaseManipSetprecision, ExtractionGoesThroughTheSameCheck)
{
    istream iss{mem_device{std::string("1.5")}};
    iss.locale(locale<char>("C"));

    iss >> setprecision(10);
    EXPECT_EQ(iss.precision(), 10);

    iss >> setprecision(300);
    EXPECT_TRUE(iss.str_fail());
    EXPECT_EQ(iss.precision(), 10);
    iss.clear();
}

TEST(IoBaseManipSetprecision, TheSameRangeHoldsOnAWideStream)
{
    ostream woss{mem_device{L""}};
    woss.locale(locale<wchar_t>("C"));

    woss << setprecision(255);
    EXPECT_EQ(woss.precision(), 255);

    EXPECT_TRUE(rejects_precision(woss, 300));
    EXPECT_EQ(woss.precision(), 255);
}
