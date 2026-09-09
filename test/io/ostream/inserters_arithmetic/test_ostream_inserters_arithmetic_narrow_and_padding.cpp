// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Two properties of arithmetic insertion that review kept re-deriving because
 * nothing pinned them.
 *
 * The first is a deliberate divergence from the standard, recorded here so it
 * cannot be "fixed" by accident. `signed char` and `unsigned char` are
 * two-dimensional: on a char stream they are characters, on a wide stream the
 * standard has no overload for them and integral promotion sends them to
 * `operator<<(int)`, where LWG 23 reinterprets the value at the width of the
 * *selected overload's parameter* -- 32 bits. This library's operator is a
 * template, so deduction never promotes and the value is formatted at the
 * width of `TValue` itself. A negative `signed char` therefore prints half as
 * wide as the standard's under hex and oct. Keeping the 8-bit form is what
 * makes the text recoverable: read back through `int` it round-trips, whereas
 * the 32-bit form overflows `int` and sets strfailbit. Types that have an
 * overload of their own -- `short` among them -- are unaffected, because there
 * the standard reinterprets at exactly `make_unsigned_t<TValue>`.
 *
 * The second is the fill-count ceiling. `ios_defs::max_pad_count` is checked
 * before a single character is written, so an over-large `setw()` throws
 * rather than emitting gigabytes; the check is on the pad count, not the total
 * length, and it is `>` rather than `>=`. Arithmetic, pointer and nullptr all
 * reach padding through `ostream_insert`, and none of them was covered.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/io/traits/nullptr.h>

#include <climits>
#include <string>

#include <gtest/gtest.h>

namespace
{
using os_c = IOv2::ostream<IOv2::mem_device<char>, char>;
using os_w = IOv2::ostream<IOv2::mem_device<wchar_t>, wchar_t>;

std::wstring wide_hex(auto value)
{
    // The default locale groups digits; these cases are about width, not
    // punctuation, so they run in "C".
    os_w os{IOv2::mem_device<wchar_t>{}, IOv2::locale<wchar_t>("C")};
    os.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
    os << value;
    return os.device().str();
}

std::wstring wide_oct(auto value)
{
    os_w os{IOv2::mem_device<wchar_t>{}, IOv2::locale<wchar_t>("C")};
    os.setf(IOv2::ios_defs::oct, IOv2::ios_defs::basefield);
    os << value;
    return os.device().str();
}
}

TEST(OstreamInsertArithmeticNarrow, ANegativeSignedCharPrintsAtItsOwnWidthOnAWideStream)
{
    // The divergence itself. std::wostringstream gives ffffffff and 37777777777.
    EXPECT_EQ(wide_hex(static_cast<signed char>(-1)), L"ff");
    EXPECT_EQ(wide_hex(static_cast<signed char>(-128)), L"80");
    EXPECT_EQ(wide_oct(static_cast<signed char>(-1)), L"377");
    EXPECT_EQ(wide_oct(static_cast<signed char>(-128)), L"200");
}

TEST(OstreamInsertArithmeticNarrow, TypesWithAnOverloadOfTheirOwnAreUnaffected)
{
    // short has operator<<(short), where the standard reinterprets at
    // unsigned short -- the same width make_unsigned_t<short> gives. These
    // agree with the standard and must keep agreeing.
    EXPECT_EQ(wide_hex(static_cast<short>(-1)), L"ffff");
    EXPECT_EQ(wide_hex(-1), L"ffffffff");

    // unsigned char stays non-negative under promotion, so both widths print
    // the same digits.
    EXPECT_EQ(wide_hex(static_cast<unsigned char>(200)), L"c8");
}

TEST(OstreamInsertArithmeticNarrow, TheEightBitFormIsWhatMakesTheTextRecoverable)
{
    // Reading the written text back through the natural type recovers the
    // value. The 32-bit form would overflow int here and set strfailbit.
    for (signed char original : {static_cast<signed char>(-1), static_cast<signed char>(-128)})
    {
        const std::wstring text = wide_hex(original);

        IOv2::istream<IOv2::mem_device<wchar_t>, wchar_t> is{
            IOv2::mem_device<wchar_t>{text}, IOv2::locale<wchar_t>("C")};
        is.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);

        int read_back = 0;
        is >> read_back;

        EXPECT_FALSE(is.str_fail()) << "original " << static_cast<int>(original);
        EXPECT_EQ(static_cast<signed char>(read_back), original);
    }
}

TEST(OstreamInsertArithmeticNarrow, ACharStreamKeepsCharacterSemantics)
{
    // The char-stream side belongs to char_and_str.h and is unchanged: the
    // byte goes out as a character, not as a number.
    os_c os{IOv2::mem_device<char>{}, IOv2::locale<char>("C")};
    os.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
    os << static_cast<signed char>('A') << static_cast<unsigned char>('B');

    EXPECT_EQ(os.device().str(), "AB");
}

TEST(OstreamInsertPadding, TheFillCeilingIsCheckedBeforeAnythingIsWritten)
{
    // "42" is two characters, so setw(max_pad_count + 2) asks for exactly
    // max_pad_count of fill: the last width that is allowed through.
    constexpr std::size_t cap = IOv2::ios_defs::max_pad_count;

    {
        os_c os{IOv2::mem_device<char>{}, IOv2::locale<char>("C")};
        os << IOv2::setw(static_cast<std::ptrdiff_t>(cap + 2)) << 42;
        EXPECT_FALSE(os.str_fail());
        EXPECT_EQ(os.device().str().size(), cap + 2);
        EXPECT_EQ(os.width(), 0u) << "the width is spent either way";
    }
    {
        // One more character of fill than the ceiling allows: nothing is
        // written at all, and the width is still spent.
        os_c os{IOv2::mem_device<char>{}, IOv2::locale<char>("C")};
        os << IOv2::setw(static_cast<std::ptrdiff_t>(cap + 3)) << 42;
        EXPECT_TRUE(os.str_fail());
        EXPECT_TRUE(os.device().str().empty()) << "the check runs before the first character";
        EXPECT_EQ(os.width(), 0u);
    }
}

TEST(OstreamInsertPadding, ThePointerAndNullptrPathsShareTheSameCeiling)
{
    constexpr std::size_t cap = IOv2::ios_defs::max_pad_count;

    {
        // nullptr writes the seven characters of "nullptr".
        os_c os{IOv2::mem_device<char>{}, IOv2::locale<char>("C")};
        os << IOv2::setw(static_cast<std::ptrdiff_t>(cap + 7)) << nullptr;
        EXPECT_FALSE(os.str_fail());
        EXPECT_EQ(os.device().str().size(), cap + 7);
    }
    {
        os_c os{IOv2::mem_device<char>{}, IOv2::locale<char>("C")};
        os << IOv2::setw(static_cast<std::ptrdiff_t>(cap + 8)) << nullptr;
        EXPECT_TRUE(os.str_fail());
        EXPECT_TRUE(os.device().str().empty());
    }
    {
        // A pointer's text length is not fixed, so this only pins that an
        // obviously over-large width is refused rather than attempted.
        int  value = 0;
        os_c os{IOv2::mem_device<char>{}, IOv2::locale<char>("C")};
        os << IOv2::setw(PTRDIFF_MAX) << static_cast<void*>(&value);
        EXPECT_TRUE(os.str_fail());
        EXPECT_TRUE(os.device().str().empty());
    }
}
