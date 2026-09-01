/**
 * The same arithmetic extraction contract as
 * test_istream_extractors_arithmetic_char.cpp for wchar_t.
 *
 * The parse rules do not depend on the character type, so the base flags, the
 * grouping check, the floating-point syntax and the out-of-range answer are
 * expected to come out the same here. What is genuinely wide is which
 * characters count: the decimal point and the thousands separator are whatever
 * the facet says they are, and a wide facet can say something that has no
 * narrow spelling at all. The last two cases are about that -- a separator and
 * a point outside ASCII, and digits that look like digits to a reader but are
 * not the facet's digits.
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

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using namespace IOv2;

namespace
{
    // "C" in every respect but the three things a number's shape depends on,
    // so that a test changes exactly what it is about.
    class tuned_conf : public numeric_conf<wchar_t>
    {
    public:
        tuned_conf(std::vector<std::uint8_t> g, wchar_t sep = L',', wchar_t point = L'.')
            : numeric_conf<wchar_t>("C")
            , m_group(std::move(g))
            , m_sep(sep)
            , m_point(point)
        { }

        const std::vector<std::uint8_t>& grouping() const override { return m_group; }
        wchar_t                          thousands_sep() const override { return m_sep; }
        wchar_t                          decimal_point() const override { return m_point; }

    private:
        std::vector<std::uint8_t> m_group;
        wchar_t                   m_sep;
        wchar_t                   m_point;
    };

    locale<wchar_t> tuned(std::vector<std::uint8_t> g, wchar_t sep = L',', wchar_t point = L'.')
    {
        return locale<wchar_t>("C").involve(std::make_shared<tuned_conf>(std::move(g), sep, point));
    }
}

TEST(IstreamExtractArithmeticWchar, EveryArithmeticTypeRoundTripsThroughText)
{
    auto expect_round_trip = []<typename TV>(TV value)
    {
        SCOPED_TRACE(::testing::PrintToString(value));

        ostream os{mem_device{std::wstring(L"")}, locale<wchar_t>("C")};
        // The default precision is a formatting choice, not a parsing one, so
        // it is widened here to keep the round trip about the extraction.
        if constexpr (std::is_floating_point_v<TV>)
            os << setprecision(std::numeric_limits<TV>::max_digits10);
        os << value;
        auto [dev, err] = os.detach();
        dev.dseek(0);

        istream is{std::move(dev), locale<wchar_t>("C")};
        TV      back{};
        is >> back;
        EXPECT_EQ(back, value);
        EXPECT_FALSE(is.str_fail());
    };

    expect_round_trip(static_cast<short>(-234));
    expect_round_trip(std::numeric_limits<short>::min());
    expect_round_trip(std::numeric_limits<unsigned short>::max());
    expect_round_trip(-234234);
    expect_round_trip(std::numeric_limits<int>::max());
    expect_round_trip(std::numeric_limits<long>::min());
    expect_round_trip(777777UL);
    expect_round_trip(std::numeric_limits<long long>::min());
    expect_round_trip(std::numeric_limits<unsigned long long>::max());
    expect_round_trip(1.5f);
    expect_round_trip(0.315);
    expect_round_trip(66300.25L);
}

TEST(IstreamExtractArithmeticWchar, BoolReadsAsAWordOnlyUnderBoolalpha)
{
    auto expect_bool = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::wstring(L"1 0")}, locale<wchar_t>("C")};
            bool b = false;
            is >> b;
            EXPECT_TRUE(b);
            is >> b;
            EXPECT_FALSE(b);
        }
        {
            T is{mem_device{std::wstring(L"true false")}, locale<wchar_t>("C")};
            is.setf(ios_defs::boolalpha);
            bool b = false;
            is >> b;
            EXPECT_TRUE(b);
            is >> b;
            EXPECT_FALSE(b);
        }
    };

    expect_bool.operator()<istream>();
    expect_bool.operator()<iostream>();
}

TEST(IstreamExtractArithmeticWchar, ExtractionStopsAtTheFirstCharacterThatCannotBelong)
{
    auto expect_stopped = []<template <typename, typename> class T>()
    {
        struct parse_case { const wchar_t* text; int value; wchar_t next; };
        for (const parse_case tc : {
                 parse_case{L"73109tail", 73109, L't'},
                 parse_case{L"908172635", 908172635, L'\0'},
             })
        {
            T is{mem_device{std::wstring(tc.text)}, locale<wchar_t>("C")};
            int value = -1;
            is >> value;
            EXPECT_EQ(value, tc.value);

            if (tc.next == L'\0')
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

TEST(IstreamExtractArithmeticWchar, TheBaseFlagsSelectWhichDigitsBelong)
{
    auto expect_bases = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::wstring(L"0123")}, locale<wchar_t>("C")};
            int n = 0;
            is >> hex >> n;
            EXPECT_EQ(n, 0x123);
        }
        {
            T is{mem_device{std::wstring(L"0x7d 0X2a 0751 2049")}, locale<wchar_t>("C")};
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
            T is{mem_device{std::wstring(L"0x94")}, locale<wchar_t>("C")};

            int     n = 0, m = 0;
            wchar_t c = 0;
            is >> dec >> n >> c >> m;
            EXPECT_EQ(n, 0);
            EXPECT_EQ(c, L'x');
            EXPECT_EQ(m, 94);
        }
    };

    expect_bases.operator()<istream>();
    expect_bases.operator()<iostream>();
}

TEST(IstreamExtractArithmeticWchar, AValueOutOfRangeIsClampedAndReported)
{
    auto expect_clamped = []<typename TV>()
    {
        auto text = [](long long v) {
            ostream os{mem_device{std::wstring(L"")}, locale<wchar_t>("C")};
            os << v;
            auto [dev, err] = os.detach();
            return dev;
        };

        {
            auto dev = text(static_cast<long long>(std::numeric_limits<TV>::max()) + 1);
            dev.dseek(0);
            istream is{std::move(dev), locale<wchar_t>("C")};
            TV      v{};
            is >> v;
            EXPECT_EQ(v, std::numeric_limits<TV>::max());
            EXPECT_TRUE(is.str_fail());
        }
        {
            auto dev = text(static_cast<long long>(std::numeric_limits<TV>::min()) - 1);
            dev.dseek(0);
            istream is{std::move(dev), locale<wchar_t>("C")};
            TV      v{};
            is >> v;
            EXPECT_EQ(v, std::numeric_limits<TV>::min());
            EXPECT_TRUE(is.str_fail());
        }
    };

    expect_clamped.operator()<short>();
    expect_clamped.operator()<int>();
}

TEST(IstreamExtractArithmeticWchar, GroupingIsCheckedWhenTheLocaleAsksForIt)
{
    auto expect_grouped = []<template <typename, typename> class T>()
    {
        {
            // Without grouping the separator merely stops the number.
            T is{mem_device{std::wstring(L"73,6201")}, locale<wchar_t>("C")};
            unsigned n = 0;
            is >> n;
            EXPECT_EQ(n, 73u);
            EXPECT_FALSE(is.str_fail());
            EXPECT_EQ(is.peek(), L',');
        }
        {
            T is{mem_device{std::wstring(L"73,6201 8,0246,1357 41,286,5091")}, tuned({4})};

            unsigned n = 0;
            is >> n;
            EXPECT_EQ(n, 736201u);
            EXPECT_TRUE(is.good());

            is >> n;
            EXPECT_EQ(n, 802461357u);
            EXPECT_TRUE(is.good());

            is >> n;
            EXPECT_EQ(n, 412865091u);
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
        }
        {
            T is{mem_device{std::wstring(L"7,6 3124,5")}, tuned({1, 4})};

            unsigned n = 0;
            is >> n;
            EXPECT_EQ(n, 76u);
            EXPECT_TRUE(is.good());

            is >> n;
            EXPECT_EQ(n, 31245u);
        }
    };

    expect_grouped.operator()<istream>();
    expect_grouped.operator()<iostream>();
}

TEST(IstreamExtractArithmeticWchar, FloatingPointSyntaxIsAcceptedWhereItIsWellFormed)
{
    auto expect_syntax = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::wstring(L"12. 7.25E+1 .125e2 6.375e-1")}, locale<wchar_t>("C")};
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
            T is{mem_device{std::wstring(L"9.75e1+-2.5e-1")}, locale<wchar_t>("C")};

            double  f1 = 0, f2 = 0;
            wchar_t c  = 0;
            is >> f1 >> c >> f2;
            EXPECT_DOUBLE_EQ(f1, 97.5);
            EXPECT_EQ(c, L'+');
            EXPECT_DOUBLE_EQ(f2, -0.25);
        }
        {
            T is{mem_device{std::wstring(L"8Ez")}, locale<wchar_t>("C")};

            double f = 1;
            is >> f;
            EXPECT_DOUBLE_EQ(f, 0.0);
            EXPECT_EQ(is.rdstate(), ios_defs::strfailbit);
        }
    };

    expect_syntax.operator()<istream>();
    expect_syntax.operator()<iostream>();
}

// The decimal point and the thousands separator are whatever the facet returns,
// and a wide facet can return characters with no narrow spelling. The ASCII
// ones then have no special meaning at all -- which is the half a parse that
// hard-codes '.' and ',' would still pass without.
TEST(IstreamExtractArithmeticWchar, ThePointAndSeparatorAreWhateverTheFacetSays)
{
    // U+2019 RIGHT SINGLE QUOTATION MARK groups, U+066B ARABIC DECIMAL
    // SEPARATOR points -- neither is representable as a narrow character.
    const auto loc = tuned({3}, L'’', L'٫');

    auto expect_facet_characters = [&]<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::wstring(L"1’024’365")}, loc};
            unsigned n = 0;
            is >> n;
            EXPECT_EQ(n, 1024365u);
            EXPECT_FALSE(is.str_fail());
        }
        {
            T is{mem_device{std::wstring(L"23’445٫25")}, loc};
            double d = 0;
            is >> d;
            EXPECT_DOUBLE_EQ(d, 23445.25);
        }
        {
            // The wrong grouping is still caught when the separator is this one.
            T is{mem_device{std::wstring(L"123’22’24")}, loc};
            unsigned n = 0;
            is >> n;
            EXPECT_EQ(n, 1232224u);
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
        }
        {
            // And ASCII '.' and ',' are now ordinary characters that end the
            // number rather than joining it.
            T is{mem_device{std::wstring(L"1,024.5")}, loc};
            double d = 0;
            is >> d;
            EXPECT_DOUBLE_EQ(d, 1.0);
            EXPECT_FALSE(is.str_fail());
            EXPECT_EQ(is.peek(), L',');
        }
    };

    expect_facet_characters.operator()<istream>();
    expect_facet_characters.operator()<iostream>();
}

// Digits are the facet's digits, matched as characters. Full-width forms read
// as digits to a person but are not the ones the facet named, so a number
// written in them is not a number.
TEST(IstreamExtractArithmeticWchar, CharactersThatMerelyLookLikeDigitsAreNotDigits)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::wstring(L"４２")}, locale<wchar_t>("C")};   // full-width "42"
            int n = 7;
            is >> n;
            EXPECT_TRUE(is.str_fail());
            EXPECT_EQ(n, 0);
        }
        {
            // And they end a number that started in real digits.
            T is{mem_device{std::wstring(L"42４")}, locale<wchar_t>("C")};
            int n = 0;
            is >> n;
            EXPECT_EQ(n, 42);
            EXPECT_TRUE(is.good());
            EXPECT_EQ(is.peek(), L'４');
        }
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

TEST(IstreamExtractArithmeticWchar, WithSkipwsOffLeadingWhitespaceStopsTheExtraction)
{
    auto expect_stopped = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::wstring(L" 43")}, locale<wchar_t>("C")};

        int i = 0;
        is >> noskipws >> i;
        EXPECT_FALSE(static_cast<bool>(is));

        is.clear();
        is.ignore();
        is >> i;
        EXPECT_EQ(i, 43);
    };

    expect_stopped.operator()<istream>();
    expect_stopped.operator()<iostream>();
}

TEST(IstreamExtractArithmeticWchar, ATrailingEndOfInputThrowsAfterTheValueIsStored)
{
    auto expect_thrown = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::wstring(L"42")}, locale<wchar_t>("C")};
            is.exceptions(ios_defs::eofbit);

            int x = 0;
            EXPECT_THROW(is >> x, eof_error);
            EXPECT_EQ(x, 42);
            EXPECT_TRUE(is.eof());
        }
        {
            T is{mem_device{std::wstring(L"42")}, locale<wchar_t>("C")};

            int x = 0;
            EXPECT_NO_THROW(is >> x);
            EXPECT_EQ(x, 42);
            EXPECT_TRUE(is.eof());
        }
    };

    expect_thrown.operator()<istream>();
    expect_thrown.operator()<iostream>();
}
