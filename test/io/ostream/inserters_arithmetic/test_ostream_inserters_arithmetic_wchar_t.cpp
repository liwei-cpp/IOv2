// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Inserting arithmetic values into an ostream<wchar_t>.
 *
 * All of this is [ostream.inserters.arithmetic] handing the value to the
 * numeric facet, so the tests are organised the way the facet's contract is:
 * which base the integer flags name, how many digits the float flags ask for,
 * where the padding goes, and which characters the facet -- not the library --
 * decides. The values are chosen so every expectation can be worked out by
 * hand: the floats are dyadic, so no expectation depends on how a tie rounds.
 *
 * The two round-trip tests are the ones that matter most in practice. Printing
 * with max_digits10 and reading back has to give the identical value, not a
 * close one, or the stream cannot be used to persist anything.
 *
 * What a wide stream adds is that none of the punctuation has to be ASCII: the
 * facet here answers with characters that have no narrow equivalent at all, so
 * a formatting path that assumed a one-byte point or separator cannot pass.
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

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace IOv2;

namespace
{
    // The base-N spelling of a value, so the expectations below are derived
    // rather than copied out of a table.
    std::wstring in_base(unsigned long long v, unsigned base)
    {
        if (v == 0)
            return L"0";

        static const wchar_t digits[] = L"0123456789abcdef";
        std::wstring         out;
        for (; v != 0; v /= base)
            out += digits[v % base];
        std::reverse(out.begin(), out.end());
        return out;
    }

    // A numeric facet whose punctuation is deliberately nothing like the usual,
    // so a hard-coded '.' or ',' anywhere in the formatting path shows up.
    class odd_punct : public numeric_conf<wchar_t>
    {
    public:
        explicit odd_punct(std::vector<std::uint8_t> group = {})
            : numeric_conf<wchar_t>("C")
            , m_group(std::move(group))
        {}

        // U+066B ARABIC DECIMAL SEPARATOR and U+2019 RIGHT SINGLE QUOTATION MARK:
        // neither exists in any narrow encoding this test can reach.
        wchar_t decimal_point() const override { return L'\u066b'; }
        wchar_t thousands_sep() const override { return L'\u2019'; }
        const std::vector<std::uint8_t>& grouping() const override { return m_group; }

    private:
        std::vector<std::uint8_t> m_group;
    };

    // A user-defined operator&& yielding bool, at namespace scope because a
    // local class cannot define a friend.
    struct measurement
    {
        double x;
        friend bool operator&&(int i, const measurement& m) { return int(m.x) == i; }
    };

    locale<wchar_t> with_punct(std::vector<std::uint8_t> group = {})
    {
        return locale<wchar_t>("C").involve(std::make_shared<odd_punct>(std::move(group)));
    }
}

TEST(OstreamInsertArithmeticWchar, TheBasefieldFlagsNameTheBase)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << 42 << L' ' << oct << 42 << L' ' << hex << 42 << L' ' << dec << 42;
            EXPECT_EQ(os.device().str(), L"42 52 2a 42");
        }
        {
            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << showbase << oct << 42 << L' ' << hex << 42;
            EXPECT_EQ(os.device().str(), L"052 0x2a");
        }
        {
            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << showbase << uppercase << hex << 42;
            EXPECT_EQ(os.device().str(), L"0X2A");
        }
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// oct and hex are unsigned conversions, so a negative value is written as the
// bit pattern of its own type -- which is why the answer depends on the width
// of the type and not on the value.
TEST(OstreamInsertArithmeticWchar, ANegativeValueInOctOrHexIsItsUnsignedPattern)
{
    auto helper = []<template <typename, typename> class TO, typename T>(T n)
    {
        const auto bits = static_cast<unsigned long long>(std::make_unsigned_t<T>(n));

        TO os(mem_device{L""}, locale<wchar_t>("C"));
        os << oct << n << L' ' << hex << n;

        auto [dev, err] = os.detach();
        EXPECT_EQ(dev.str(), in_base(bits, 8) + L' ' + in_base(bits, 16));
    };

    helper.operator()<ostream>(static_cast<short>(-1));
    helper.operator()<ostream>(static_cast<int>(-1));
    helper.operator()<ostream>(static_cast<long>(-1));

    helper.operator()<iostream>(static_cast<short>(-1));
    helper.operator()<iostream>(static_cast<int>(-1));
    helper.operator()<iostream>(static_cast<long>(-1));
}

// setf(oct) then setf(hex) leaves both bits set, which names no base at all;
// the facet then falls back to decimal, and a decimal conversion is signed, so
// the minus sign comes back.
TEST(OstreamInsertArithmeticWchar, TwoBasefieldFlagsAtOnceNameNoBaseAndFallBackToDecimal)
{
    auto helper = []<template <typename, typename> class TO, typename T>(T n)
    {
        TO os(mem_device{L""}, locale<wchar_t>("C"));
        os.setf(ios_defs::oct);
        os.setf(ios_defs::hex);
        os << n;

        auto [dev, err] = os.detach();
        EXPECT_EQ(dev.str(), L"-1");
    };

    helper.operator()<ostream>(static_cast<short>(-1));
    helper.operator()<ostream>(static_cast<int>(-1));
    helper.operator()<ostream>(static_cast<long>(-1));
    helper.operator()<ostream>(-1LL);
    helper.operator()<iostream>(-1);
}

// internal adjustment puts the fill between the prefix and the digits. The same
// text inserted as a string has no prefix to speak of, so it is simply
// right-adjusted -- the pair is what shows the padding is the number's, not the
// field's.
TEST(OstreamInsertArithmeticWchar, InternalAdjustmentPadsAfterTheBasePrefix)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << hex << showbase << setw(8) << internal << 0x2a;
            EXPECT_EQ(os.device().str(), L"0x    2a");
        }
        {
            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << hex << showbase << setw(8) << internal << L"0x2a";
            EXPECT_EQ(os.device().str(), L"    0x2a");
        }
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamInsertArithmeticWchar, FixedWritesExactlyPrecisionDigitsAfterThePoint)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto formatted = [](double v, int prec, bool showpoint_on = false) {
            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << fixed << setprecision(prec);
            if (showpoint_on)
                os << showpoint;
            os << v;
            auto [dev, err] = os.detach();
            return dev.str();
        };

        EXPECT_EQ(formatted(1.5, 3), L"1.500");
        EXPECT_EQ(formatted(7.625, 3), L"7.625");
        EXPECT_EQ(formatted(-3.25, 2), L"-3.25");
        EXPECT_EQ(formatted(2.0, 0), L"2");
        EXPECT_EQ(formatted(2.0, 0, true), L"2.");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamInsertArithmeticWchar, ScientificWritesOneDigitBeforeThePoint)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << scientific << setprecision(2) << 1.5e10;
            EXPECT_EQ(os.device().str(), L"1.50e+10");
        }
        {
            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << scientific << setprecision(1) << -2.5e-8;
            EXPECT_EQ(os.device().str(), L"-2.5e-08");
        }
        {
            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << scientific << uppercase << setprecision(2) << 1.5e10;
            EXPECT_EQ(os.device().str(), L"1.50E+10");
        }
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// A short value is padded to the field; where the fill goes is adjustfield's
// business, and under internal the sign stays at the front. The fill is '*'
// rather than '.' on purpose: a fill equal to the decimal point is refused
// outright, which is its own contract and has its own test below.
TEST(OstreamInsertArithmeticWchar, AdjustfieldDecidesWhereTheFillGoes)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto formatted = [](double v, ios_defs::fmtflags adjust, bool plus = false) {
            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << fixed << setprecision(3) << setw(10) << setfill(L'*');
            os.setf(adjust, ios_defs::adjustfield);
            if (plus)
                os << showpos;
            os << v;
            auto [dev, err] = os.detach();
            return dev.str();
        };

        EXPECT_EQ(formatted(7.625, ios_defs::right), L"*****7.625");
        EXPECT_EQ(formatted(7.625, ios_defs::left), L"7.625*****");
        EXPECT_EQ(formatted(-7.625, ios_defs::internal), L"-****7.625");
        EXPECT_EQ(formatted(7.625, ios_defs::internal, true), L"+****7.625");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// Under internal the fill goes behind whatever announces the number. A hexfloat
// announces itself with "0x" and has no sign, so the marker is what gets
// pinned; give it a sign and the sign is pinned instead and the marker moves
// along with the digits.
TEST(OstreamInsertArithmeticWchar, InternalPinsTheSignOrElseTheBaseMarker)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto render = [](double value, ios_defs::fmtflags adjust) {
            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << hexfloat << setw(13) << setfill(L'~');
            os.setf(adjust, ios_defs::adjustfield);
            os << value;
            return os.device().str();
        };

        EXPECT_EQ(render(40.0, ios_defs::internal), L"0x~~~~~1.4p+5");
        EXPECT_EQ(render(-40.0, ios_defs::internal), L"-~~~~0x1.4p+5");
        // right pins nothing, so the whole value moves to the end.
        EXPECT_EQ(render(40.0, ios_defs::right), L"~~~~~0x1.4p+5");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// A locale that is not "C" gets its punctuation from the C library rather than
// from a facet written here, which is a different path through locale itself.
TEST(OstreamInsertArithmeticWchar, ARealSystemLocaleSuppliesItsOwnPunctuation)
{
    locale<wchar_t> de("C");
    try
    {
        de = locale<wchar_t>("de_DE.ISO-8859-1");
    }
    catch (const cvt_error&)
    {
        GTEST_SKIP() << "de_DE.ISO-8859-1 is not installed here";
    }

    ostream os(mem_device{L""}, de);
    os << fixed << setprecision(1) << 1234567.5;
    EXPECT_EQ(os.device().str(), L"1.234.567,5");
}

TEST(OstreamInsertArithmeticWchar, AValueAtLeastAsWideAsTheFieldIsNotPadded)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{L""}, locale<wchar_t>("C"));
        os << fixed << setprecision(3) << setw(5) << setfill(L'*') << 7.625;
        EXPECT_EQ(os.device().str(), L"7.625");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// The point and the separator are the facet's, not the library's.
/**
 * A fill character that would change the number the field reads as is refused.
 *
 * This is a deliberate departure from std::ostream, which pads with whatever it
 * is given. Which characters are unsafe depends on where the fill lands: past
 * the value nothing can be read back into it, so left adjustment accepts
 * anything, while a run placed in front of the digits must not be a digit, a
 * decimal point, or a sign that the number does not already carry. A leading
 * '0' is the exception, since leading zeros do not change a value -- but only
 * where it really does lead the digits, which for a signed value means internal
 * rather than right.
 *
 * The refusal is reported like every other stream failure: strfailbit, and a
 * throw only when that bit is masked in.
 */
TEST(OstreamInsertArithmeticWchar, AFillThatWouldChangeTheValueIsRefused)
{
    auto padded = [](wchar_t fill, ios_defs::fmtflags adjust, auto value, bool as_hex = false) {
        ostream os(mem_device{L""}, locale<wchar_t>("C"));
        os << fixed << setprecision(3) << setw(10) << setfill(fill);
        if (as_hex)
            os << hex << showbase;
        os.setf(adjust, ios_defs::adjustfield);
        os << value;
        return std::make_pair(os.device().str(), os.str_fail());
    };

    auto accepted = [&](auto... args) { return padded(args...).first; };
    auto refused  = [&](auto... args) {
        const auto [text, failed] = padded(args...);
        return failed && text.empty();
    };

    // The decimal point ahead of the digits would add a second point.
    EXPECT_TRUE(refused(L'.', ios_defs::right, 7.625));
    EXPECT_TRUE(refused(L'.', ios_defs::internal, 7.625));
    EXPECT_EQ(accepted(L'.', ios_defs::left, 7.625), L"7.625.....");
    EXPECT_EQ(accepted(L'.', ios_defs::left, -7.625), L"-7.625....");

    // A digit changes the value outright; '0' does not, where it leads.
    EXPECT_TRUE(refused(L'9', ios_defs::right, 7.625));
    EXPECT_EQ(accepted(L'0', ios_defs::right, 7.625), L"000007.625");
    EXPECT_EQ(accepted(L'0', ios_defs::internal, 7.625), L"000007.625");

    // With a sign in the way, right puts the zeros before it and internal after.
    EXPECT_TRUE(refused(L'0', ios_defs::right, -42));
    EXPECT_EQ(accepted(L'0', ios_defs::internal, -42), L"-000000042");

    // In hex the letters are digits too.
    EXPECT_TRUE(refused(L'a', ios_defs::right, 0x2a, true));

    // A sign the value does not already carry would be read as its own.
    EXPECT_TRUE(refused(L'-', ios_defs::right, 7.625));
    EXPECT_TRUE(refused(L'+', ios_defs::right, -7.625));
    EXPECT_EQ(accepted(L'-', ios_defs::right, -7.625), L"-----7.625");
    EXPECT_EQ(accepted(L'+', ios_defs::right, 7.625), L"+++++7.625");
}

TEST(OstreamInsertArithmeticWchar, TheRefusedFillThrowsWhenStrfailbitIsMasked)
{
    ostream os(mem_device{L""}, locale<wchar_t>("C"));
    os.exceptions(ios_defs::strfailbit);
    os << fixed << setprecision(3) << setw(10) << setfill(L'.') << right;

    EXPECT_THROW(os << 7.625, stream_error);
    EXPECT_TRUE(os.device().str().empty());
}

TEST(OstreamInsertArithmeticWchar, ThePunctuationComesFromTheFacet)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os(mem_device{L""}, with_punct());
            os << fixed << setprecision(3) << 7.625;
            EXPECT_EQ(os.device().str(), std::wstring(L"7\u066b625"));
        }
        {
            // Grouping applies to the integer part only, and counts from the point.
            T os(mem_device{L""}, with_punct({3}));
            os << fixed << setprecision(3) << 1234567.5;
            EXPECT_EQ(os.device().str(), std::wstring(L"1\u2019234\u2019567\u066b500"));
        }
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamInsertArithmeticWchar, ShowposWritesAPlusOnlyOnNonNegativeValues)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{L""}, locale<wchar_t>("C"));
        os << showpos << 42 << L' ' << -42 << L' ' << 0;
        EXPECT_EQ(os.device().str(), L"+42 -42 +0");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// Printing with max_digits10 and reading back must give the identical value:
// anything less than that and the stream cannot be used to persist a number.
TEST(OstreamInsertArithmeticWchar, AValueRoundTripsExactlyThroughItsOwnText)
{
    auto helper = []<template <typename, typename> class TO, typename T>()
    {
        for (const T v : {static_cast<T>(3.14159265358979323846L),
                          std::numeric_limits<T>::min(),
                          std::numeric_limits<T>::max(),
                          static_cast<T>(-0.0),
                          static_cast<T>(1)})
        {
            TO os(mem_device{L""}, locale<wchar_t>("C"));
            os << setprecision(std::numeric_limits<T>::max_digits10) << v;
            ASSERT_TRUE(static_cast<bool>(os));

            auto [dev, err] = os.detach();

            istream is(mem_device{dev.str()}, locale<wchar_t>("C"));
            T       back{};
            is >> back;
            EXPECT_EQ(back, v) << "text was " << dev.str();
        }
    };

    helper.template operator()<ostream, float>();
    helper.template operator()<ostream, double>();
    helper.template operator()<ostream, long double>();
    helper.template operator()<iostream, double>();
}

// The longest output any float format can produce; the C library is the oracle
// for what it should look like.
TEST(OstreamInsertArithmeticWchar, AVeryLongValueIsWrittenInFull)
{
    auto helper = []<template <typename, typename> class T>()
    {
        char buf[512];

        {
            const long double val  = std::numeric_limits<long double>::max();
            const int         prec = std::numeric_limits<long double>::digits10;

            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << scientific << setprecision(prec) << val;
            EXPECT_TRUE(static_cast<bool>(os));

            std::snprintf(buf, sizeof(buf), "%.*Le", prec, val);
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), std::wstring(buf, buf + std::strlen(buf)));
        }
        {
            // Fixed format on the largest double is the case that used to run off
            // the end of a fixed-size buffer: the integer part alone is 309 digits.
            const double val = std::numeric_limits<double>::max();

            T os(mem_device{L""}, locale<wchar_t>("C"));
            os << fixed << setprecision(3) << val;
            EXPECT_TRUE(static_cast<bool>(os));

            std::snprintf(buf, sizeof(buf), "%.*f", 3, val);
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), std::wstring(buf, buf + std::strlen(buf)));
        }
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamInsertArithmeticWchar, HexfloatWritesAHexSignificandAndABinaryExponent)
{
    auto helper = []<template <typename, typename> class TO, typename T>()
    {
        struct format_case { T value; const wchar_t* decimal; };
        TO os{mem_device{L""}, locale<wchar_t>("C")};
        for (const format_case tc : {
                 format_case{static_cast<T>(40), L"40"},
                 format_case{static_cast<T>(-13.5), L"-13.5"},
                 format_case{static_cast<T>(0.125), L"0.125"},
             })
        {
            os << hexfloat << tc.value << L'|' << uppercase << tc.value
               << L'|' << nouppercase << defaultfloat << tc.value;
            EXPECT_TRUE(static_cast<bool>(os));

            auto [dev, err] = os.detach();
            const std::wstring text = dev.str();
            const std::size_t first = text.find(L'|');
            ASSERT_NE(first, std::wstring::npos);
            const std::size_t second = text.find(L'|', first + 1);
            ASSERT_NE(second, std::wstring::npos);

            const std::wstring lower = text.substr(0, first);
            const std::wstring upper = text.substr(first + 1, second - first - 1);
            const std::wstring decimal = text.substr(second + 1);
            const std::size_t prefix = tc.value < 0 ? 1u : 0u;

            EXPECT_EQ(std::stold(lower), static_cast<long double>(tc.value));
            EXPECT_EQ(lower.substr(prefix, 2), L"0x");
            EXPECT_NE(lower.find(L'p'), std::wstring::npos);
            EXPECT_EQ(std::stold(upper), static_cast<long double>(tc.value));
            EXPECT_EQ(upper.substr(prefix, 2), L"0X");
            EXPECT_NE(upper.find(L'P'), std::wstring::npos);
            EXPECT_EQ(decimal, tc.decimal);

            os.attach(mem_device{L""});
        }
    };

    helper.template operator()<ostream, double>();
    helper.template operator()<ostream, long double>();
    helper.template operator()<iostream, double>();
    helper.template operator()<iostream, long double>();
}

// A NaN has no digits, but it does have a sign, and the sign must survive.
TEST(OstreamInsertArithmeticWchar, TheSignOfANaNIsWritten)
{
    auto helper = []<template <typename, typename> class T>()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();

        T os(mem_device{L""}, locale<wchar_t>("C"));
        os << -nan;
        auto [dev, err] = os.detach();
        ASSERT_FALSE(dev.str().empty());
        EXPECT_EQ(dev.str()[0], L'-');
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// A user-defined operator that yields bool feeds the bool inserter, not the
// pointer or the arithmetic one.
TEST(OstreamInsertArithmeticWchar, AUserDefinedOperatorYieldingBoolReachesTheBoolInserter)
{
    auto helper = [](auto& os)
    {
        os << (3 && measurement{3.1}) << L' ' << (4 && measurement{3.1});
    };

    {
        ostream os(mem_device{L""}, locale<wchar_t>("C"));
        helper(os);
        EXPECT_EQ(os.device().str(), L"1 0");
    }
    {
        iostream os(mem_device{L""}, locale<wchar_t>("C"));
        helper(os);
        EXPECT_EQ(os.device().str(), L"1 0");
    }
}

TEST(OstreamInsertArithmeticWchar, ArraysAndPointersReachTheAddressPath)
{
    auto helper = []<template <typename, typename> class T>()
    {
        int    ia[3] = {1, 2, 3};
        double da[2] = {1.0, 2.0};
        wchar_t ca[4] = L"abc";

        T os(mem_device{L""}, locale<wchar_t>("C"));
        os << ia;
        auto [dev19, err19] = os.detach();

        T os2(mem_device{L""}, locale<wchar_t>("C"));
        os2 << static_cast<const void*>(ia);
        auto [dev20, err20] = os2.detach();
        EXPECT_EQ(dev19.str(), dev20.str());

        T os3(mem_device{L""}, locale<wchar_t>("C"));
        os3 << da;
        auto [dev21, err21] = os3.detach();

        T os4(mem_device{L""}, locale<wchar_t>("C"));
        os4 << static_cast<const void*>(da);
        auto [dev22, err22] = os4.detach();
        EXPECT_EQ(dev21.str(), dev22.str());

        T os5(mem_device{L""}, locale<wchar_t>("C"));
        os5 << ca;
        auto [dev23, err23] = os5.detach();
        EXPECT_EQ(dev23.str(), L"abc");

        // A volatile pointee reaches the same address path. A qualification conversion can only
        // add cv, so without the cast in swrite these would find only put(bool) and print "1"
        // while the stream stayed good(). C++23 P1147R1 made std::ostream print the address here.
        volatile int*  via = ia;
        volatile wchar_t* vca = ca;

        T os6(mem_device{L""}, locale<wchar_t>("C"));
        os6 << via;
        auto [dev24, err24] = os6.detach();
        EXPECT_EQ(dev24.str(), dev20.str());

        T os7(mem_device{L""}, locale<wchar_t>("C"));
        os7 << vca;
        auto [dev25, err25] = os7.detach();

        T os8(mem_device{L""}, locale<wchar_t>("C"));
        os8 << static_cast<const void*>(ca);
        auto [dev26, err26] = os8.detach();
        EXPECT_EQ(dev25.str(), dev26.str());

        // Top-level volatile is the other axis and must change nothing: it is dropped when the
        // pointer is passed by value, so the standard still writes the characters. The pointer
        // io_traits used to answer instead -- is_pointer_v ignores top-level cv while the string
        // specializations do not -- and printed an address with the stream good().
        wchar_t* volatile       cpv  = ca;
        const wchar_t* volatile ccpv = ca;
        int* volatile        ipv  = ia;

        T os9(mem_device{L""}, locale<wchar_t>("C"));
        os9 << cpv << L'/' << ccpv;
        EXPECT_TRUE(os9.good());
        auto [dev27, err27] = os9.detach();
        EXPECT_EQ(dev27.str(), L"abc/abc");

        T os10(mem_device{L""}, locale<wchar_t>("C"));
        os10 << ipv;
        auto [dev28, err28] = os10.detach();
        EXPECT_EQ(dev28.str(), dev20.str());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}
