// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Formatted extraction of arithmetic values from an istream<char>.
 *
 * The parse is one pass with no backtracking: characters are taken while they
 * could still belong to a number in the current base, and the first one that
 * could not is left where it is. So "73109tail" yields 73109 and leaves 't'
 * behind, and every case below that checks a value also checks where the
 * stream stopped -- a parse that consumed one character too many looks correct
 * until the next extraction.
 *
 * Three things decide what "could belong" means, and each gets its own section:
 * the base flags, which also have an auto-detecting state with no flag set; the
 * locale's grouping, which is off in "C" and turns the separator into a
 * stopping character; and, for floating point, the exponent syntax.
 *
 * The one outcome worth stating on its own is out of range. The value does not
 * come back wrong and it does not come back untouched: the nearest
 * representable limit is stored *and* the failure is reported, so a caller who
 * checks learns of it and a caller who does not gets a bounded value rather
 * than a wrapped one.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/facet/numeric_details.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace IOv2;

namespace
{
    // A numeric_conf that is "C" in every respect but its grouping, so that a
    // test changes exactly the one thing it is about.
    class grouped_conf : public numeric_conf<char>
    {
    public:
        explicit grouped_conf(std::vector<std::uint8_t> g)
            : numeric_conf<char>("C")
            , m_group(std::move(g))
        { }

        const std::vector<std::uint8_t>& grouping() const override { return m_group; }

    private:
        std::vector<std::uint8_t> m_group;
    };

    locale<char> grouped_by(std::vector<std::uint8_t> g)
    {
        return locale<char>("C").involve(std::make_shared<grouped_conf>(std::move(g)));
    }
}

// Everything an ostream writes, an istream reads back, for every arithmetic
// type and at the ends of each one's range. This is the property the rest of
// the file then picks apart.
TEST(IstreamExtractArithmeticChar, EveryArithmeticTypeRoundTripsThroughText)
{
    auto expect_round_trip = []<typename TV>(TV value)
    {
        SCOPED_TRACE(::testing::PrintToString(value));

        ostream os{mem_device{std::string("")}, locale<char>("C")};
        // The default precision is a formatting choice, not a parsing one, so
        // it is widened here to keep the round trip about the extraction.
        if constexpr (std::is_floating_point_v<TV>)
            os << setprecision(std::numeric_limits<TV>::max_digits10);
        os << value;
        auto [dev, err] = os.detach();
        dev.dseek(0);

        istream is{std::move(dev), locale<char>("C")};
        TV      back{};
        is >> back;
        EXPECT_EQ(back, value);
        EXPECT_FALSE(is.str_fail());
    };

    expect_round_trip(static_cast<short>(-234));
    expect_round_trip(std::numeric_limits<short>::max());
    expect_round_trip(std::numeric_limits<short>::min());
    expect_round_trip(static_cast<unsigned short>(33));
    expect_round_trip(std::numeric_limits<unsigned short>::max());
    expect_round_trip(-234234);
    expect_round_trip(std::numeric_limits<int>::max());
    expect_round_trip(std::numeric_limits<int>::min());
    expect_round_trip(233u);
    expect_round_trip(std::numeric_limits<unsigned>::max());
    expect_round_trip(-19999999L);
    expect_round_trip(std::numeric_limits<long>::max());
    expect_round_trip(std::numeric_limits<long>::min());
    expect_round_trip(777777UL);
    expect_round_trip(std::numeric_limits<long long>::min());
    expect_round_trip(std::numeric_limits<unsigned long long>::max());
    expect_round_trip(1.5f);
    expect_round_trip(0.315);
    expect_round_trip(66300.25L);
}

// A bool is a number by default and a word under boolalpha, and the flag
// switches the parse rather than adding to it: the word form does not accept
// digits and the numeric form does not accept the word.
TEST(IstreamExtractArithmeticChar, BoolReadsAsAWordOnlyUnderBoolalpha)
{
    auto expect_bool = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::string("1 0 2")}, locale<char>("C")};

            bool b = false;
            is >> b;
            EXPECT_TRUE(b);
            is >> b;
            EXPECT_FALSE(b);

            // Only 0 and 1 are numbers a bool can hold.
            is >> b;
            EXPECT_TRUE(is.str_fail());
        }
        {
            T is{mem_device{std::string("true false")}, locale<char>("C")};
            is.setf(ios_defs::boolalpha);

            bool b = false;
            is >> b;
            EXPECT_TRUE(b);
            is >> b;
            EXPECT_FALSE(b);
        }
        {
            // Each form rejects the other's spelling.
            T words{mem_device{std::string("1")}, locale<char>("C")};
            words.setf(ios_defs::boolalpha);
            bool b = false;
            words >> b;
            EXPECT_TRUE(words.str_fail());

            T digits{mem_device{std::string("true")}, locale<char>("C")};
            digits >> b;
            EXPECT_TRUE(digits.str_fail());
        }
    };

    expect_bool.operator()<istream>();
    expect_bool.operator()<iostream>();
}

// The extraction takes what belongs to a number and stops; it does not skip
// past what does not, and it does not fail because of it.
TEST(IstreamExtractArithmeticChar, ExtractionStopsAtTheFirstCharacterThatCannotBelong)
{
    auto expect_stopped = []<template <typename, typename> class T>()
    {
        struct parse_case { const char* text; long value; char next; };
        for (const parse_case tc : {
                 parse_case{"73109tail", 73109, 't'},
                 parse_case{"908172635", 908172635, '\0'},
             })
        {
            SCOPED_TRACE(tc.text);
            T is{mem_device{std::string(tc.text)}, locale<char>("C")};
            long value = -1;
            is >> value;
            EXPECT_EQ(value, tc.value);

            if (tc.next == '\0')
                EXPECT_EQ(is.rdstate(), ios_defs::eofbit);
            else
            {
                EXPECT_TRUE(is.good());
                EXPECT_EQ(is.peek(), tc.next);
            }
        }
    };

    expect_stopped.operator()<istream>();
    expect_stopped.operator()<iostream>();
}

// The base flags say which digits belong. With no flag set the base is read
// from the text itself, the way strtol does with a base of zero.
TEST(IstreamExtractArithmeticChar, TheBaseFlagsSelectWhichDigitsBelong)
{
    auto expect_bases = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::string("0123")}, locale<char>("C")};
            int n = 0;
            is >> hex >> n;
            EXPECT_EQ(n, 0x123);
        }
        {
            // No basefield flag: 0x is hexadecimal, a leading 0 is octal, and
            // anything else is decimal.
            T is{mem_device{std::string("0x7d 0X2a 0751 2049")}, locale<char>("C")};
            is.unsetf(ios_defs::basefield);

            int n = 0;
            is >> n;
            EXPECT_EQ(n, 125);
            is >> n;
            EXPECT_EQ(n, 42);
            is >> n;
            EXPECT_EQ(n, 489);
            is >> n;
            EXPECT_EQ(n, 2049);
            EXPECT_EQ(is.rdstate(), ios_defs::eofbit);
        }
        {
            // With a base fixed, the prefix is no longer a prefix: under dec
            // the 0 of "0x94" is a whole number and 'x' is the next character.
            T is{mem_device{std::string("0x94 0X57 071 86")}, locale<char>("C")};

            int  n = 0, m = 0;
            char c = 0;

            is >> dec >> n >> c >> m;
            EXPECT_EQ(n, 0);
            EXPECT_EQ(c, 'x');
            EXPECT_EQ(m, 94);

            is >> oct >> m >> c >> n;
            EXPECT_EQ(m, 0);
            EXPECT_EQ(c, 'X');
            EXPECT_EQ(n, 47);          // "57" in octal

            is >> dec >> m >> n;
            EXPECT_EQ(m, 71);
            EXPECT_EQ(n, 86);
            EXPECT_EQ(is.rdstate(), ios_defs::eofbit);
        }
    };

    expect_bases.operator()<istream>();
    expect_bases.operator()<iostream>();
}

// Signs and redundant leading zeros are part of the number, and a value of zero
// spelled any of the ways that mean zero comes back as zero.
TEST(IstreamExtractArithmeticChar, SignsAndLeadingZerosAreAcceptedWithoutChangingTheValue)
{
    auto expect_zero = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::string("0 00 000 +0 -0 +7 -7")}, locale<char>("C")};

        int n = 365;
        for (int i = 0; i < 5; ++i)
        {
            SCOPED_TRACE(i);
            n = 365;
            is >> n;
            EXPECT_EQ(n, 0);
        }

        is >> n;
        EXPECT_EQ(n, 7);
        is >> n;
        EXPECT_EQ(n, -7);
        EXPECT_EQ(is.rdstate(), ios_defs::eofbit);
    };

    expect_zero.operator()<istream>();
    expect_zero.operator()<iostream>();
}

// Out of range is reported *and* answered: the nearest limit is stored, so a
// caller who does not check gets a bounded value rather than a wrapped one.
TEST(IstreamExtractArithmeticChar, AValueOutOfRangeIsClampedAndReported)
{
    auto expect_clamped = []<typename TV>()
    {
        SCOPED_TRACE(::testing::PrintToString(std::numeric_limits<TV>::max()));

        // One past each end, written by a type wide enough to hold it.
        auto text = [](long long v) {
            ostream os{mem_device{std::string("")}, locale<char>("C")};
            os << v;
            auto [dev, err] = os.detach();
            return dev;
        };

        {
            auto dev = text(static_cast<long long>(std::numeric_limits<TV>::max()) + 1);
            dev.dseek(0);
            istream is{std::move(dev), locale<char>("C")};
            TV      v{};
            is >> v;
            EXPECT_EQ(v, std::numeric_limits<TV>::max());
            EXPECT_TRUE(is.str_fail());
        }
        {
            auto dev = text(static_cast<long long>(std::numeric_limits<TV>::min()) - 1);
            dev.dseek(0);
            istream is{std::move(dev), locale<char>("C")};
            TV      v{};
            is >> v;
            EXPECT_EQ(v, std::numeric_limits<TV>::min());
            EXPECT_TRUE(is.str_fail());
        }
    };

    expect_clamped.operator()<short>();
    expect_clamped.operator()<int>();

    // Far past the end rather than just past it, for every width, including the
    // ones no wider type could have written.
    auto expect_far = []<typename TV>(bool integral)
    {
        const int overflow_digits = integral
                                        ? std::numeric_limits<TV>::digits10 + 2
                                        : std::numeric_limits<TV>::max_exponent10 + 1;

        std::string digits;
        while (digits.size() < static_cast<std::size_t>(overflow_digits) + 1)
            digits += "9753108642";

        istream is{mem_device{digits}, locale<char>("C")};
        TV      v{};
        is >> v;
        EXPECT_TRUE(is.str_fail());
    };

    expect_far.operator()<short>(true);
    expect_far.operator()<int>(true);
    expect_far.operator()<long>(true);
    expect_far.operator()<long long>(true);
    expect_far.operator()<unsigned>(true);
    expect_far.operator()<unsigned long long>(true);
    expect_far.operator()<float>(false);
    expect_far.operator()<double>(false);
}

// A number of exactly the greatest number of digits the type can always hold
// fits; one more than that does not, and it is the length rather than the value
// that has to be got right at the boundary.
TEST(IstreamExtractArithmeticChar, TheDigitCountBoundaryIsExact)
{
    auto expect_boundary = []<template <typename, typename> class T>()
    {
        const int   max_digits = std::numeric_limits<int>::digits10 + 1;
        std::string digits(static_cast<std::size_t>(max_digits), '1');

        {
            T is{mem_device{digits}, locale<char>("C")};
            int n = 0;
            is >> n;
            EXPECT_FALSE(is.str_fail());
        }
        {
            T is{mem_device{digits + '1'}, locale<char>("C")};
            int n = 0;
            is >> n;
            EXPECT_EQ(n, std::numeric_limits<int>::max());
            EXPECT_TRUE(is.str_fail());
        }
    };

    expect_boundary.operator()<istream>();
    expect_boundary.operator()<iostream>();
}

// In "C" there is no grouping, so a separator is simply not part of a number
// and the parse stops at it. Nothing fails: the caller can read the separator
// itself and carry on.
TEST(IstreamExtractArithmeticChar, WithoutGroupingASeparatorIsJustAStoppingCharacter)
{
    auto expect_stopped = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::string("73,4,9021")}, locale<char>("C")};
        const unsigned expected[] = {73u, 4u, 9021u};

        for (unsigned index = 0; index < 3; ++index)
        {
            unsigned value = 0;
            is >> value;
            EXPECT_EQ(value, expected[index]);
            if (index != 2)
            {
                char separator = 0;
                is >> separator;
                EXPECT_EQ(separator, ',');
            }
        }
        EXPECT_EQ(is.rdstate(), ios_defs::eofbit);
        EXPECT_FALSE(is.str_fail());
    };

    expect_stopped.operator()<istream>();
    expect_stopped.operator()<iostream>();
}

// With grouping the separator becomes part of the number, and the group sizes
// are checked. A wrong grouping is reported *after* the digits have been read
// and the value stored -- the digits were all valid, it is only their spacing
// that was not.
TEST(IstreamExtractArithmeticChar, GroupingIsCheckedWhenTheLocaleAsksForIt)
{
    auto expect_grouped = []<template <typename, typename> class T>()
    {
        {
            // Groups of four.
            T is{mem_device{std::string("73,6201 8,0246,1357 41,286,5091")}, grouped_by({4})};

            unsigned n = 0;
            is >> n;
            EXPECT_EQ(n, 736201u);
            EXPECT_TRUE(is.good());

            is >> n;
            EXPECT_EQ(n, 802461357u);
            EXPECT_TRUE(is.good());

            // The middle group is too short: its digits are kept, its layout is not.
            is >> n;
            EXPECT_EQ(n, 412865091u);
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
        }
        {
            // A grouping of {1, 4} is one on the right and four thereafter,
            // which is what makes the sizes position-dependent rather than one
            // repeated number.
            T is{mem_device{std::string("7,6 3124,5 84,3021,7")}, grouped_by({1, 4})};

            unsigned n = 0;
            is >> n;
            EXPECT_EQ(n, 76u);
            EXPECT_TRUE(is.good());

            is >> n;
            EXPECT_EQ(n, 31245u);
            EXPECT_TRUE(is.good());

            is >> n;
            EXPECT_EQ(n, 8430217u);
            EXPECT_FALSE(is.str_fail());
        }
    };

    expect_grouped.operator()<istream>();
    expect_grouped.operator()<iostream>();
}

// The ways a grouped number can be malformed, and where each leaves the stream.
TEST(IstreamExtractArithmeticChar, MalformedGroupingIsRejectedWithoutConsumingTheSeparator)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        {
            // A leading separator: there are no digits before it, so nothing
            // was extracted at all and the separator is still there.
            T is{mem_device{std::string(",111")}, grouped_by({3})};

            unsigned n = 7;
            is >> n;
            EXPECT_TRUE(is.str_fail());

            is.clear();
            char c = 0;
            is >> c;
            EXPECT_EQ(c, ',');
            EXPECT_TRUE(is.good());
        }
        {
            // A separator is part of a grouped number, so the first one is
            // taken and the number then ends on the second, which cannot
            // follow it. Exactly one separator is left behind.
            T is{mem_device{std::string("4,,4")}, grouped_by({3})};

            unsigned n = 7;
            is >> n;
            EXPECT_TRUE(is.str_fail());

            is.clear();
            char c = 0;
            is >> c;
            EXPECT_EQ(c, ',');
            is >> c;
            EXPECT_EQ(c, '4');
        }
        {
            // A group too wide, and -- the case that has to stay distinct -- the
            // same digits with no separators at all, which is always allowed.
            T is{mem_device{std::string("1,000000 1000000")}, grouped_by({3})};

            unsigned n = 0;
            is >> n;
            EXPECT_EQ(n, 1000000u);
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);

            is.clear();
            n = 0;
            is >> n;
            EXPECT_EQ(n, 1000000u);
            EXPECT_FALSE(is.str_fail());
        }
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

// Grouping applies to the digits before the decimal point and to nothing after
// it, so a separator in the fraction ends the number.
TEST(IstreamExtractArithmeticChar, GroupingAppliesOnlyToTheIntegerPart)
{
    auto expect_integer_part = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::string("23,445.25 0.25,345")}, grouped_by({3})};

        float f = 0;
        is >> f;
        EXPECT_FLOAT_EQ(f, 23445.25f);
        EXPECT_TRUE(is.good());

        is >> f;
        EXPECT_FLOAT_EQ(f, 0.25f);
        EXPECT_TRUE(is.good());

        char c = 0;
        is >> c;
        EXPECT_EQ(c, ',');

        unsigned n = 0;
        is >> n;
        EXPECT_EQ(n, 345u);
    };

    expect_integer_part.operator()<istream>();
    expect_integer_part.operator()<iostream>();
}

// A floating-point number may leave out the integer part or the fraction, but
// not both, and an exponent that has been started must be finished.
TEST(IstreamExtractArithmeticChar, FloatingPointSyntaxIsAcceptedWhereItIsWellFormed)
{
    auto expect_syntax = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::string("12. 7.25E+1 .125e2 6.375e-1")}, locale<char>("C")};
            const double expected[] = {12.0, 72.5, 12.5, 0.6375};

            for (double answer : expected)
            {
                double value = 0;
                is >> value;
                EXPECT_DOUBLE_EQ(value, answer);
            }
            EXPECT_EQ(is.rdstate(), ios_defs::eofbit);
        }
        {
            // A sign belongs to the exponent only after the 'e'; elsewhere it
            // ends the number, which is what separates these two values.
            T is{mem_device{std::string("9.75e1+-2.5e-1")}, locale<char>("C")};

            double f1 = 0, f2 = 0;
            char   c  = 0;
            is >> f1 >> c >> f2;
            EXPECT_DOUBLE_EQ(f1, 97.5);
            EXPECT_EQ(c, '+');
            EXPECT_DOUBLE_EQ(f2, -0.25);
        }
        {
            // An 'E' with nothing usable after it is a number that was started
            // and not finished: the failure is reported and the 'E' is consumed.
            T is{mem_device{std::string("6E2 8Ez E9")}, locale<char>("C")};

            double f = 0;
            is >> f;
            EXPECT_DOUBLE_EQ(f, 600.0);
            EXPECT_TRUE(is.good());

            f = 1;
            is >> f;
            EXPECT_DOUBLE_EQ(f, 0.0);
            EXPECT_EQ(is.rdstate(), ios_defs::strfailbit);

            is.clear();
            char c = 0;
            is >> c;
            EXPECT_EQ(c, 'z');

            // And an exponent with no mantissa at all is not a number either.
            f = 1;
            is >> f;
            EXPECT_DOUBLE_EQ(f, 0.0);
            EXPECT_EQ(is.rdstate(), ios_defs::strfailbit);
        }
        {
            // A second decimal point ends the number rather than spoiling it.
            T is{mem_device{std::string("5..25")}, locale<char>("C")};

            double f = 0;
            is >> f;
            EXPECT_DOUBLE_EQ(f, 5.0);
            EXPECT_TRUE(is.good());
            is >> f;
            EXPECT_DOUBLE_EQ(f, 0.25);
        }
    };

    expect_syntax.operator()<istream>();
    expect_syntax.operator()<iostream>();
}

// A mantissa longer than the type can represent is rounded, not rejected: it is
// still a number, unlike a value whose magnitude is out of range.
TEST(IstreamExtractArithmeticChar, AnOverlongMantissaIsRoundedRatherThanRejected)
{
    auto expect_rounded = []<template <typename, typename> class T>()
    {
        std::string input = "0.";
        input.append(80, '7');
        input += "  861.25";
        T is{mem_device{std::move(input)}, locale<char>("C")};

        double d = 0;
        is >> d;
        EXPECT_FALSE(is.str_fail());
        EXPECT_NEAR(d, 7.0 / 9.0, 1e-15);

        is >> d;
        EXPECT_DOUBLE_EQ(d, 861.25);
    };

    expect_rounded.operator()<istream>();
    expect_rounded.operator()<iostream>();
}

// The leading whitespace an extraction skips is skipped by the sentry, so
// turning skipws off leaves the space in the way and the number is never found.
TEST(IstreamExtractArithmeticChar, WithSkipwsOffLeadingWhitespaceStopsTheExtraction)
{
    auto expect_stopped = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::string(" 43")}, locale<char>("C")};

        int i = 0;
        is >> noskipws >> i;
        EXPECT_FALSE(static_cast<bool>(is));
        EXPECT_EQ(i, 0);

        // The space is still there, so a caller who steps over it gets the value.
        is.clear();
        is.ignore();
        is >> i;
        EXPECT_EQ(i, 43);
    };

    expect_stopped.operator()<istream>();
    expect_stopped.operator()<iostream>();
}

// A number that ends exactly at the end of the input has been parsed before
// eofbit is set, so with eofbit masked the value is stored and the throw comes
// afterwards, out of the sentry's ordinary exit path.
TEST(IstreamExtractArithmeticChar, ATrailingEndOfInputThrowsAfterTheValueIsStored)
{
    auto expect_thrown = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::string("42")}, locale<char>("C")};
            is.exceptions(ios_defs::eofbit);

            int x = 0;
            EXPECT_THROW(is >> x, eof_error);
            EXPECT_EQ(x, 42);
            EXPECT_TRUE(is.eof());
        }
        {
            T is{mem_device{std::string("42")}, locale<char>("C")};

            int x = 0;
            EXPECT_NO_THROW(is >> x);
            EXPECT_EQ(x, 42);
            EXPECT_TRUE(is.eof());
        }
    };

    expect_thrown.operator()<istream>();
    expect_thrown.operator()<iostream>();
}

namespace
{
    struct throwing_target {};
}

namespace IOv2
{
template <typename TChar>
struct io_traits<TChar, throwing_target>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter, TSent, ios_base<TChar>&, const locale<TChar>&, throwing_target&)
    {
        throw 0;
    }
};
}

// An exception from a user's own io_traits is not a stream error and is not
// turned into one: it reaches the caller unchanged. What the stream does is
// record that the operation failed, so the state is right for whoever catches.
TEST(IstreamExtractArithmeticChar, AnExtractorThatThrowsSomethingElseIsNotSwallowed)
{
    auto expect_propagated = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::string("hello")}, locale<char>("C")};
        is.exceptions(ios_defs::otherfailbit);

        throwing_target arg;
        EXPECT_THROW(is >> arg, int);
        EXPECT_FALSE(static_cast<bool>(is));
    };

    expect_propagated.operator()<istream>();
    expect_propagated.operator()<iostream>();
}
