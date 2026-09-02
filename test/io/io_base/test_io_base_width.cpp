// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * ios_base::width, whose whole difficulty is the type of its argument.
 *
 * The width is a count, so it is held as an unsigned value, but callers compute
 * it with subtraction -- `total - label.size()` -- and an underflow there
 * arrives as a huge positive number rather than as a negative one. IOv2 refuses
 * those instead of padding to half the address space, which means the setter has
 * to reject both spellings of "negative": the signed one, and the unsigned one
 * that has already wrapped.
 *
 * The refusal has to leave the previous width in place, and has to leave the
 * stream it was called on good, or a caller's next write would silently change.
 */
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/ostream.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <string>

using namespace IOv2;

// The setter rejects negative widths, including the ones that arrive already
// wrapped as unsigned, and leaves the previous width in place when it does.
TEST(IosBaseWidth, ANegativeWidthIsRejectedHoweverItIsSpelled)
{
    ios_base<char> ios;
    ASSERT_EQ(ios.width(), 0u);

    EXPECT_EQ(ios.width(12), 0u);
    ASSERT_EQ(ios.width(), 12u);

    EXPECT_THROW(ios.width(-1), stream_error);
    EXPECT_EQ(ios.width(), 12u);

    const int n = -3;
    EXPECT_THROW(ios.width(n), stream_error);

    // The shape a caller actually writes: a field width computed by subtraction,
    // where the label turned out longer than the field.
    const int         total = 10;
    const std::string label(20, 'x');
    EXPECT_THROW(ios.width(total - label.size()), stream_error);

    // The same value arriving already wrapped, with no sign left to look at.
    EXPECT_THROW(ios.width(size_t(10) - size_t(20)), stream_error);
    EXPECT_THROW(ios.width(size_t(1) << 63), stream_error);
    EXPECT_THROW(ios.width(std::numeric_limits<size_t>::max()), stream_error);

    EXPECT_EQ(ios.width(), 12u);
}

// Valid widths round-trip exactly, including ones a 32-bit field could not hold.
TEST(IosBaseWidth, AValidWidthRoundTripsExactly)
{
    ios_base<char> ios;

    EXPECT_EQ(ios.width(0), 0u);
    EXPECT_EQ(ios.width(), 0u);

    ios.width(50000000);
    EXPECT_EQ(ios.width(), 50000000u);

    ios.width(std::ptrdiff_t(1) << 40);
    EXPECT_EQ(ios.width(), (size_t(1) << 40));

    const auto pmax = std::numeric_limits<std::ptrdiff_t>::max();
    EXPECT_EQ(ios.width(pmax), (size_t(1) << 40));
    EXPECT_EQ(ios.width(), static_cast<size_t>(pmax));

    // A width read back from the getter can be fed to the setter unchanged.
    ios.width(static_cast<std::ptrdiff_t>(ios.width()));
    EXPECT_EQ(ios.width(), static_cast<size_t>(pmax));

    ios.width(0);
    EXPECT_EQ(ios.width(), 0u);
}

// A rejected width does not disturb the stream it was called on.
TEST(IosBaseWidth, ARejectedWidthLeavesTheStreamUsable)
{
    ostream os{mem_device{""}};
    os.locale(locale<char>("C"));

    os << "ab";

    EXPECT_THROW(os.width(-1), stream_error);
    EXPECT_TRUE(os.good());
    EXPECT_EQ(os.width(), 0u);

    os << "cd";
    os.flush();
    EXPECT_EQ(os.device().str(), "abcd");
}

TEST(IosBaseWidth, TheSameHoldsForAWideStream)
{
    ios_base<wchar_t> ios;

    ios.width(7);
    EXPECT_EQ(ios.width(), 7u);

    EXPECT_THROW(ios.width(-1), stream_error);
    EXPECT_EQ(ios.width(), 7u);

    ios.width(std::ptrdiff_t(1) << 40);
    EXPECT_EQ(ios.width(), (size_t(1) << 40));

    ios.width(0);
    EXPECT_EQ(ios.width(), 0u);
}
