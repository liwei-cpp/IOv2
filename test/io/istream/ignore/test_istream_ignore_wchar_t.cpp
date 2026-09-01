/**
 * The same ignore() contract as test_istream_ignore_char.cpp for wchar_t: a
 * count and a delimiter are both limits and the nearer one stops the call, the
 * delimiter is discarded along with what preceded it, and running out of input
 * is an end rather than a failure.
 *
 * The file cases stay in the narrow file; what this instantiation adds is that
 * both the count and the delimiter are in characters, which only shows on
 * characters that are more than one byte wide.
 */
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <limits>
#include <string>

using namespace IOv2;

namespace
{
    const std::wstring kDigits = L"0123456789abcdef";

    constexpr auto kUnbounded = std::numeric_limits<std::streamsize>::max();
}

TEST(IstreamIgnoreWchar, IgnoreDiscardsOneCharacterByDefaultAndNCharactersOnRequest)
{
    auto expect_counted = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        const ios_defs::iostate before = is.rdstate();
        is.ignore();
        EXPECT_EQ(is.rdstate(), before);
        EXPECT_EQ(is.peek(), L'1');

        is.ignore(4);
        EXPECT_EQ(is.peek(), L'5');

        is.ignore(10);
        EXPECT_EQ(is.peek(), L'f');
    };

    expect_counted.operator()<istream>();
    expect_counted.operator()<iostream>();
}

TEST(IstreamIgnoreWchar, ACountOfZeroDiscardsNothing)
{
    auto expect_nothing = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        is.ignore(0);
        EXPECT_EQ(is.peek(), L'0');

        is.ignore(0, L'0');
        EXPECT_EQ(is.peek(), L'0');
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
    };

    expect_nothing.operator()<istream>();
    expect_nothing.operator()<iostream>();
}

TEST(IstreamIgnoreWchar, TheDelimiterIsDiscardedTooAndIgnoreStopsAfterIt)
{
    auto expect_consumed = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        is.ignore(kUnbounded, L'4');
        EXPECT_EQ(is.peek(), L'5');
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
    };

    expect_consumed.operator()<istream>();
    expect_consumed.operator()<iostream>();
}

TEST(IstreamIgnoreWchar, TheCountAndTheDelimiterAreBothLimitsAndTheNearerOneStops)
{
    auto expect_first = []<template <typename, typename> class T>()
    {
        {
            T is(mem_device{kDigits});
            is.ignore(6, L'3');
            EXPECT_EQ(is.peek(), L'4');
        }
        {
            T is(mem_device{kDigits});
            is.ignore(3, L'8');
            EXPECT_EQ(is.peek(), L'3');
            EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
        }
    };

    expect_first.operator()<istream>();
    expect_first.operator()<iostream>();
}

TEST(IstreamIgnoreWchar, RunningOutOfInputSetsEndOfFileWithoutFailing)
{
    auto expect_end = []<template <typename, typename> class T>()
    {
        {
            T is(mem_device{std::wstring(L"ab")});
            is.ignore(10);
            EXPECT_TRUE(is.eof());
            EXPECT_FALSE(is.rdstate() & ios_defs::strfailbit);
            EXPECT_TRUE(static_cast<bool>(is));
        }
        {
            T is(mem_device{std::wstring(L"ab")});
            is.ignore(kUnbounded, L'|');
            EXPECT_TRUE(is.eof());
            EXPECT_FALSE(is.rdstate() & ios_defs::strfailbit);
        }
    };

    expect_end.operator()<istream>();
    expect_end.operator()<iostream>();
}

// Both limits count characters rather than the bytes they encode to, so a
// fixture of multi-byte characters lands on the same positions as an ASCII one.
TEST(IstreamIgnoreWchar, TheCountAndTheDelimiterAreBothInCharacters)
{
    auto expect_characters = []<template <typename, typename> class T>()
    {
        const std::wstring wide = L"é中é中é|x";
        {
            T is(mem_device{wide});
            is.ignore(3);
            EXPECT_EQ(is.peek(), wide[3]);
        }
        {
            T is(mem_device{wide});
            is.ignore(kUnbounded, L'中');
            EXPECT_EQ(is.peek(), wide[2]);
        }
        {
            T is(mem_device{wide});
            is.ignore(kUnbounded, L'|');
            EXPECT_EQ(is.peek(), L'x');
        }
    };

    expect_characters.operator()<istream>();
    expect_characters.operator()<iostream>();
}

TEST(IstreamIgnoreWchar, IgnoreAtTheEndThrowsWhenEndOfFileIsMasked)
{
    auto expect_thrown = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::wstring(L"")}, locale<wchar_t>("C")};
            is.exceptions(ios_defs::eofbit);
            EXPECT_THROW(is.ignore(), eof_error);
            EXPECT_TRUE(is.eof());
        }
        {
            T is{mem_device{std::wstring(L"")}, locale<wchar_t>("C")};
            EXPECT_NO_THROW(is.ignore());
            EXPECT_TRUE(is.eof());
        }
    };

    expect_thrown.operator()<istream>();
    expect_thrown.operator()<iostream>();
}

// wchar_t counterpart: ignore on a failed stream is rejected by the sentry and routed
// through handle_exception (-> strfailbit), no throw.
TEST(IstreamIgnoreWchar, IgnoreOnAFailedStreamIsReportedRatherThanThrown)
{
    auto expect_reported = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::wstring(L"abc")}, locale<wchar_t>("C")};

        int v = 0;
        is >> v;
        EXPECT_FALSE(static_cast<bool>(is));

        EXPECT_NO_THROW(is.ignore(5, L'x'));
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);

        EXPECT_NO_THROW(is.ignore(5));
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
    };

    expect_reported.operator()<istream>();
    expect_reported.operator()<iostream>();
}
