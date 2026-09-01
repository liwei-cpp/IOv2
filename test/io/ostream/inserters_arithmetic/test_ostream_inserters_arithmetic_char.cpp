/**
 * Inserting arithmetic values into an ostream<char>.
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
 */
#include <device/mem_device.h>
#include <io/io_manip.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <limits>
#include <memory>
#include <utility>
#include <string>
#include <type_traits>
#include <vector>

using namespace IOv2;

namespace
{
    // The base-N spelling of a value, so the expectations below are derived
    // rather than copied out of a table.
    std::string in_base(unsigned long long v, unsigned base)
    {
        if (v == 0)
            return "0";

        static const char digits[] = "0123456789abcdef";
        std::string       out;
        for (; v != 0; v /= base)
            out += digits[v % base];
        std::reverse(out.begin(), out.end());
        return out;
    }

    // A numeric facet whose punctuation is deliberately nothing like the usual,
    // so a hard-coded '.' or ',' anywhere in the formatting path shows up.
    class odd_punct : public numeric_conf<char>
    {
    public:
        explicit odd_punct(std::vector<std::uint8_t> group = {})
            : numeric_conf<char>("C")
            , m_group(std::move(group))
        {}

        char decimal_point() const override { return '#'; }
        char thousands_sep() const override { return '_'; }
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

    locale<char> with_punct(std::vector<std::uint8_t> group = {})
    {
        return locale<char>("C").involve(std::make_shared<odd_punct>(std::move(group)));
    }
}

TEST(OstreamInsertArithmeticChar, TheBasefieldFlagsNameTheBase)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os(mem_device{""}, locale<char>("C"));
            os << 42 << ' ' << oct << 42 << ' ' << hex << 42 << ' ' << dec << 42;
            EXPECT_EQ(os.device().str(), "42 52 2a 42");
        }
        {
            T os(mem_device{""}, locale<char>("C"));
            os << showbase << oct << 42 << ' ' << hex << 42;
            EXPECT_EQ(os.device().str(), "052 0x2a");
        }
        {
            T os(mem_device{""}, locale<char>("C"));
            os << showbase << uppercase << hex << 42;
            EXPECT_EQ(os.device().str(), "0X2A");
        }
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// oct and hex are unsigned conversions, so a negative value is written as the
// bit pattern of its own type -- which is why the answer depends on the width
// of the type and not on the value.
TEST(OstreamInsertArithmeticChar, ANegativeValueInOctOrHexIsItsUnsignedPattern)
{
    auto helper = []<template <typename, typename> class TO, typename T>(T n)
    {
        const auto bits = static_cast<unsigned long long>(std::make_unsigned_t<T>(n));

        TO os(mem_device{""}, locale<char>("C"));
        os << oct << n << ' ' << hex << n;

        auto [dev, err] = os.detach();
        EXPECT_EQ(dev.str(), in_base(bits, 8) + ' ' + in_base(bits, 16));
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
TEST(OstreamInsertArithmeticChar, TwoBasefieldFlagsAtOnceNameNoBaseAndFallBackToDecimal)
{
    auto helper = []<template <typename, typename> class TO, typename T>(T n)
    {
        TO os(mem_device{""}, locale<char>("C"));
        os.setf(ios_defs::oct);
        os.setf(ios_defs::hex);
        os << n;

        auto [dev, err] = os.detach();
        EXPECT_EQ(dev.str(), "-1");
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
TEST(OstreamInsertArithmeticChar, InternalAdjustmentPadsAfterTheBasePrefix)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os(mem_device{""}, locale<char>("C"));
            os << hex << showbase << setw(8) << internal << 0x2a;
            EXPECT_EQ(os.device().str(), "0x    2a");
        }
        {
            T os(mem_device{""}, locale<char>("C"));
            os << hex << showbase << setw(8) << internal << "0x2a";
            EXPECT_EQ(os.device().str(), "    0x2a");
        }
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamInsertArithmeticChar, FixedWritesExactlyPrecisionDigitsAfterThePoint)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto formatted = [](double v, int prec, bool showpoint_on = false) {
            T os(mem_device{""}, locale<char>("C"));
            os << fixed << setprecision(prec);
            if (showpoint_on)
                os << showpoint;
            os << v;
            auto [dev, err] = os.detach();
            return dev.str();
        };

        EXPECT_EQ(formatted(1.5, 3), "1.500");
        EXPECT_EQ(formatted(7.625, 3), "7.625");
        EXPECT_EQ(formatted(-3.25, 2), "-3.25");
        EXPECT_EQ(formatted(2.0, 0), "2");
        EXPECT_EQ(formatted(2.0, 0, true), "2.");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamInsertArithmeticChar, ScientificWritesOneDigitBeforeThePoint)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os(mem_device{""}, locale<char>("C"));
            os << scientific << setprecision(2) << 1.5e10;
            EXPECT_EQ(os.device().str(), "1.50e+10");
        }
        {
            T os(mem_device{""}, locale<char>("C"));
            os << scientific << setprecision(1) << -2.5e-8;
            EXPECT_EQ(os.device().str(), "-2.5e-08");
        }
        {
            T os(mem_device{""}, locale<char>("C"));
            os << scientific << uppercase << setprecision(2) << 1.5e10;
            EXPECT_EQ(os.device().str(), "1.50E+10");
        }
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// A short value is padded to the field; where the fill goes is adjustfield's
// business, and under internal the sign stays at the front. The fill is '*'
// rather than '.' on purpose: a fill equal to the decimal point is refused
// outright, which is its own contract and has its own test below.
TEST(OstreamInsertArithmeticChar, AdjustfieldDecidesWhereTheFillGoes)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto formatted = [](double v, ios_defs::fmtflags adjust, bool plus = false) {
            T os(mem_device{""}, locale<char>("C"));
            os << fixed << setprecision(3) << setw(10) << setfill('*');
            os.setf(adjust, ios_defs::adjustfield);
            if (plus)
                os << showpos;
            os << v;
            auto [dev, err] = os.detach();
            return dev.str();
        };

        EXPECT_EQ(formatted(7.625, ios_defs::right), "*****7.625");
        EXPECT_EQ(formatted(7.625, ios_defs::left), "7.625*****");
        EXPECT_EQ(formatted(-7.625, ios_defs::internal), "-****7.625");
        EXPECT_EQ(formatted(7.625, ios_defs::internal, true), "+****7.625");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// Under internal the fill goes behind whatever announces the number. A hexfloat
// announces itself with "0x" and has no sign, so the marker is what gets
// pinned; give it a sign and the sign is pinned instead and the marker moves
// along with the digits.
TEST(OstreamInsertArithmeticChar, InternalPinsTheSignOrElseTheBaseMarker)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto render = [](double value, ios_defs::fmtflags adjust) {
            T os(mem_device{""}, locale<char>("C"));
            os << hexfloat << setw(13) << setfill('~');
            os.setf(adjust, ios_defs::adjustfield);
            os << value;
            return os.device().str();
        };

        EXPECT_EQ(render(40.0, ios_defs::internal), "0x~~~~~1.4p+5");
        EXPECT_EQ(render(-40.0, ios_defs::internal), "-~~~~0x1.4p+5");
        // right pins nothing, so the whole value moves to the end.
        EXPECT_EQ(render(40.0, ios_defs::right), "~~~~~0x1.4p+5");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// A locale that is not "C" gets its punctuation from the C library rather than
// from a facet written here, which is a different path through locale itself.
TEST(OstreamInsertArithmeticChar, ARealSystemLocaleSuppliesItsOwnPunctuation)
{
    locale<char> de("C");
    try
    {
        de = locale<char>("de_DE.ISO-8859-1");
    }
    catch (const cvt_error&)
    {
        GTEST_SKIP() << "de_DE.ISO-8859-1 is not installed here";
    }

    ostream os(mem_device{""}, de);
    os << fixed << setprecision(1) << 1234567.5;
    EXPECT_EQ(os.device().str(), "1.234.567,5");
}

TEST(OstreamInsertArithmeticChar, AValueAtLeastAsWideAsTheFieldIsNotPadded)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{""}, locale<char>("C"));
        os << fixed << setprecision(3) << setw(5) << setfill('*') << 7.625;
        EXPECT_EQ(os.device().str(), "7.625");
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
TEST(OstreamInsertArithmeticChar, AFillThatWouldChangeTheValueIsRefused)
{
    auto padded = [](char fill, ios_defs::fmtflags adjust, auto value, bool as_hex = false) {
        ostream os(mem_device{""}, locale<char>("C"));
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
    EXPECT_TRUE(refused('.', ios_defs::right, 7.625));
    EXPECT_TRUE(refused('.', ios_defs::internal, 7.625));
    EXPECT_EQ(accepted('.', ios_defs::left, 7.625), "7.625.....");
    EXPECT_EQ(accepted('.', ios_defs::left, -7.625), "-7.625....");

    // A digit changes the value outright; '0' does not, where it leads.
    EXPECT_TRUE(refused('9', ios_defs::right, 7.625));
    EXPECT_EQ(accepted('0', ios_defs::right, 7.625), "000007.625");
    EXPECT_EQ(accepted('0', ios_defs::internal, 7.625), "000007.625");

    // With a sign in the way, right puts the zeros before it and internal after.
    EXPECT_TRUE(refused('0', ios_defs::right, -42));
    EXPECT_EQ(accepted('0', ios_defs::internal, -42), "-000000042");

    // In hex the letters are digits too.
    EXPECT_TRUE(refused('a', ios_defs::right, 0x2a, true));

    // A sign the value does not already carry would be read as its own.
    EXPECT_TRUE(refused('-', ios_defs::right, 7.625));
    EXPECT_TRUE(refused('+', ios_defs::right, -7.625));
    EXPECT_EQ(accepted('-', ios_defs::right, -7.625), "-----7.625");
    EXPECT_EQ(accepted('+', ios_defs::right, 7.625), "+++++7.625");
}

TEST(OstreamInsertArithmeticChar, TheRefusedFillThrowsWhenStrfailbitIsMasked)
{
    ostream os(mem_device{""}, locale<char>("C"));
    os.exceptions(ios_defs::strfailbit);
    os << fixed << setprecision(3) << setw(10) << setfill('.') << right;

    EXPECT_THROW(os << 7.625, stream_error);
    EXPECT_TRUE(os.device().str().empty());
}

TEST(OstreamInsertArithmeticChar, ThePunctuationComesFromTheFacet)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os(mem_device{""}, with_punct());
            os << fixed << setprecision(3) << 7.625;
            EXPECT_EQ(os.device().str(), "7#625");
        }
        {
            // Grouping applies to the integer part only, and counts from the point.
            T os(mem_device{""}, with_punct({3}));
            os << fixed << setprecision(3) << 1234567.5;
            EXPECT_EQ(os.device().str(), "1_234_567#500");
        }
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamInsertArithmeticChar, ShowposWritesAPlusOnlyOnNonNegativeValues)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{""}, locale<char>("C"));
        os << showpos << 42 << ' ' << -42 << ' ' << 0;
        EXPECT_EQ(os.device().str(), "+42 -42 +0");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// Printing with max_digits10 and reading back must give the identical value:
// anything less than that and the stream cannot be used to persist a number.
TEST(OstreamInsertArithmeticChar, AValueRoundTripsExactlyThroughItsOwnText)
{
    auto helper = []<template <typename, typename> class TO, typename T>()
    {
        for (const T v : {static_cast<T>(3.14159265358979323846L),
                          std::numeric_limits<T>::min(),
                          std::numeric_limits<T>::max(),
                          static_cast<T>(-0.0),
                          static_cast<T>(1)})
        {
            TO os(mem_device{""}, locale<char>("C"));
            os << setprecision(std::numeric_limits<T>::max_digits10) << v;
            ASSERT_TRUE(static_cast<bool>(os));

            auto [dev, err] = os.detach();

            istream is(mem_device{dev.str()}, locale<char>("C"));
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
TEST(OstreamInsertArithmeticChar, AVeryLongValueIsWrittenInFull)
{
    auto helper = []<template <typename, typename> class T>()
    {
        char buf[512];

        {
            const long double val  = std::numeric_limits<long double>::max();
            const int         prec = std::numeric_limits<long double>::digits10;

            T os(mem_device{""}, locale<char>("C"));
            os << scientific << setprecision(prec) << val;
            EXPECT_TRUE(static_cast<bool>(os));

            std::snprintf(buf, sizeof(buf), "%.*Le", prec, val);
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), buf);
        }
        {
            // Fixed format on the largest double is the case that used to run off
            // the end of a fixed-size buffer: the integer part alone is 309 digits.
            const double val = std::numeric_limits<double>::max();

            T os(mem_device{""}, locale<char>("C"));
            os << fixed << setprecision(3) << val;
            EXPECT_TRUE(static_cast<bool>(os));

            std::snprintf(buf, sizeof(buf), "%.*f", 3, val);
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), buf);
        }
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamInsertArithmeticChar, HexfloatWritesAHexSignificandAndABinaryExponent)
{
    auto helper = []<template <typename, typename> class TO, typename T>()
    {
        struct format_case { T value; const char* decimal; };
        TO os{mem_device{""}, locale<char>("C")};
        for (const format_case tc : {
                 format_case{static_cast<T>(40), "40"},
                 format_case{static_cast<T>(-13.5), "-13.5"},
                 format_case{static_cast<T>(0.125), "0.125"},
             })
        {
            os << hexfloat << tc.value << '|' << uppercase << tc.value
               << '|' << nouppercase << defaultfloat << tc.value;
            EXPECT_TRUE(static_cast<bool>(os));

            auto [dev, err] = os.detach();
            const std::string text = dev.str();
            const std::size_t first = text.find('|');
            ASSERT_NE(first, std::string::npos);
            const std::size_t second = text.find('|', first + 1);
            ASSERT_NE(second, std::string::npos);

            const std::string lower = text.substr(0, first);
            const std::string upper = text.substr(first + 1, second - first - 1);
            const std::string decimal = text.substr(second + 1);
            const std::size_t prefix = tc.value < 0 ? 1u : 0u;

            EXPECT_EQ(std::stold(lower), static_cast<long double>(tc.value));
            EXPECT_EQ(lower.substr(prefix, 2), "0x");
            EXPECT_NE(lower.find('p'), std::string::npos);
            EXPECT_EQ(std::stold(upper), static_cast<long double>(tc.value));
            EXPECT_EQ(upper.substr(prefix, 2), "0X");
            EXPECT_NE(upper.find('P'), std::string::npos);
            EXPECT_EQ(decimal, tc.decimal);

            os.attach(mem_device{""});
        }
    };

    helper.template operator()<ostream, double>();
    helper.template operator()<ostream, long double>();
    helper.template operator()<iostream, double>();
    helper.template operator()<iostream, long double>();
}

// A NaN has no digits, but it does have a sign, and the sign must survive.
TEST(OstreamInsertArithmeticChar, TheSignOfANaNIsWritten)
{
    auto helper = []<template <typename, typename> class T>()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();

        T os(mem_device{""}, locale<char>("C"));
        os << -nan;
        auto [dev, err] = os.detach();
        ASSERT_FALSE(dev.str().empty());
        EXPECT_EQ(dev.str()[0], '-');
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// A user-defined operator that yields bool feeds the bool inserter, not the
// pointer or the arithmetic one.
TEST(OstreamInsertArithmeticChar, AUserDefinedOperatorYieldingBoolReachesTheBoolInserter)
{
    auto helper = [](auto& os)
    {
        os << (3 && measurement{3.1}) << ' ' << (4 && measurement{3.1});
    };

    {
        ostream os(mem_device{""}, locale<char>("C"));
        helper(os);
        EXPECT_EQ(os.device().str(), "1 0");
    }
    {
        iostream os(mem_device{""}, locale<char>("C"));
        helper(os);
        EXPECT_EQ(os.device().str(), "1 0");
    }
}

TEST(OstreamInsertArithmeticChar, ArraysAndPointersReachTheAddressPath)
{
    auto helper = []<template <typename, typename> class T>()
    {
        int    ia[3] = {1, 2, 3};
        double da[2] = {1.0, 2.0};
        char   ca[4] = "abc";

        T os(mem_device{""}, locale<char>("C"));
        os << ia;
        auto [dev19, err19] = os.detach();

        T os2(mem_device{""}, locale<char>("C"));
        os2 << static_cast<const void*>(ia);
        auto [dev20, err20] = os2.detach();
        EXPECT_EQ(dev19.str(), dev20.str());

        T os3(mem_device{""}, locale<char>("C"));
        os3 << da;
        auto [dev21, err21] = os3.detach();

        T os4(mem_device{""}, locale<char>("C"));
        os4 << static_cast<const void*>(da);
        auto [dev22, err22] = os4.detach();
        EXPECT_EQ(dev21.str(), dev22.str());

        T os5(mem_device{""}, locale<char>("C"));
        os5 << ca;
        auto [dev23, err23] = os5.detach();
        EXPECT_EQ(dev23.str(), "abc");

        // A volatile pointee reaches the same address path. A qualification conversion can only
        // add cv, so without the cast in swrite these would find only put(bool) and print "1"
        // while the stream stayed good(). C++23 P1147R1 made std::ostream print the address here.
        volatile int*  via = ia;
        volatile char* vca = ca;

        T os6(mem_device{""}, locale<char>("C"));
        os6 << via;
        auto [dev24, err24] = os6.detach();
        EXPECT_EQ(dev24.str(), dev20.str());

        T os7(mem_device{""}, locale<char>("C"));
        os7 << vca;
        auto [dev25, err25] = os7.detach();

        T os8(mem_device{""}, locale<char>("C"));
        os8 << static_cast<const void*>(ca);
        auto [dev26, err26] = os8.detach();
        EXPECT_EQ(dev25.str(), dev26.str());

        // Top-level volatile is the other axis and must change nothing: it is dropped when the
        // pointer is passed by value, so the standard still writes the characters. The pointer
        // io_traits used to answer instead -- is_pointer_v ignores top-level cv while the string
        // specializations do not -- and printed an address with the stream good().
        char* volatile       cpv  = ca;
        const char* volatile ccpv = ca;
        int* volatile        ipv  = ia;

        T os9(mem_device{""}, locale<char>("C"));
        os9 << cpv << '/' << ccpv;
        EXPECT_TRUE(os9.good());
        auto [dev27, err27] = os9.detach();
        EXPECT_EQ(dev27.str(), "abc/abc");

        T os10(mem_device{""}, locale<char>("C"));
        os10 << ipv;
        auto [dev28, err28] = os10.detach();
        EXPECT_EQ(dev28.str(), dev20.str());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}
