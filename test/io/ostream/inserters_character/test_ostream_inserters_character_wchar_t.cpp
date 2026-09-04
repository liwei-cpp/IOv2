// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Inserting characters and strings into an ostream<wchar_t>.
 *
 * The padding contract is the one in the narrow file, with the wide-specific
 * addition that a field is measured in characters: a value made of multi-byte
 * characters occupies as many places as it has characters, not as many as it
 * will take in the device.
 *
 * The type tests below are where the two character types genuinely differ. A
 * narrow string widens into a wide stream, but signed char and unsigned char do
 * not: on a wide stream they are numbers, exactly as they are for std::wostream
 * where they reach operator<<(int) by promotion.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/io/traits/nullptr.h>

#include <support/io_traits_probe.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

using namespace IOv2;

TEST(OstreamInsertCharacterWchar, AValueShorterThanTheWidthIsPaddedOnTheAdjustSide)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os{mem_device{L""}};
            os.width(5);
            os.fill(L'.');
            os.flags(ios_defs::right);
            os << std::wstring(L"ab");
            EXPECT_EQ(os.device().str(), L"...ab");
        }
        {
            T os{mem_device{L""}};
            os.width(5);
            os.fill(L'.');
            os.flags(ios_defs::left);
            os << std::wstring(L"ab");
            EXPECT_EQ(os.device().str(), L"ab...");
        }
        // An empty string is the limiting case: the field is all fill.
        {
            T os{mem_device{L""}};
            os.width(5);
            os.fill(L'.');
            os.flags(ios_defs::right);
            os << std::wstring(L"");
            EXPECT_EQ(os.device().str(), L".....");
        }
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// The field counts characters, so multi-byte content is padded by how many
// characters it has and not by how much room it will take.
TEST(OstreamInsertCharacterWchar, TheFieldIsMeasuredInCharacters)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os{mem_device{L""}};
        os.fill(L'.');

        os << setw(5) << std::wstring(L"中é") << L'|';
        EXPECT_EQ(os.device().str(), std::wstring(L"...中é|"));
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterWchar, AValueAtLeastAsLongAsTheWidthIsWrittenWhole)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os{mem_device{L""}};
            os.width(5);
            os.fill(L'.');
            os << std::wstring(L"abcde");           // exactly the width
            EXPECT_EQ(os.device().str(), L"abcde");
        }
        {
            T os{mem_device{L""}};
            os.width(5);
            os.fill(L'.');
            os << std::wstring(L"abcdefgh");        // longer than the width
            EXPECT_EQ(os.device().str(), L"abcdefgh");
        }
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// The trailing L'|' is the point: it would be padded too if width() had survived.
TEST(OstreamInsertCharacterWchar, TheWidthIsClearedAfterEachInsertion)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os{mem_device{L""}};
        os.fill(L'.');

        os << setw(5) << L'a' << L'|';
        os << setw(5) << L"bc" << L'|';
        os << setw(5) << std::wstring(L"def") << L'|';

        EXPECT_EQ(os.device().str(), L"....a|...bc|..def|");
        EXPECT_EQ(os.width(), 0u);
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterWchar, AWideFieldIsFilledInFull)
{
    auto helper = []<template <typename, typename> class T>()
    {
        constexpr std::size_t field = 200;

        {
            T os{mem_device{L""}};
            os.width(field);
            os << L'a';
            EXPECT_TRUE(os.good());
            EXPECT_EQ(os.device().str().size(), field);
        }
        {
            const std::wstring value(50, L'a');
            T                  os{mem_device{L""}};
            os.width(field);
            os << value.c_str();
            EXPECT_TRUE(os.good());
            EXPECT_EQ(os.device().str().size(), field);
        }
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterWchar, ANullCStringIsRefusedWithoutWritingAnything)
{
    auto helper = []<template <typename, typename> class T>()
    {
        wchar_t* nothing = nullptr;

        T os{mem_device{L""}};
        os.width(10);
        os << nothing;
        EXPECT_FALSE(static_cast<bool>(os));

        // Refusing early is no excuse for keeping the width: the leftover would pad whatever
        // the caller inserts after clearing.
        EXPECT_EQ(os.width(), 0u);

        os.flush();
        EXPECT_TRUE(os.device().str().empty());

        os.clear();
        os << L"";
        EXPECT_TRUE(os.good());
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterWchar, RepeatedInsertionsAccumulateInOrder)
{
    auto helper = []<template <typename, typename> class T>()
    {
        constexpr int rounds = 250;

        T os(mem_device{L""});
        for (int i = 0; i < rounds; ++i)
            os << L"line " << i << endl;
        EXPECT_TRUE(os.good());

        std::wstring expected;
        for (int i = 0; i < rounds; ++i)
            expected += L"line " + std::to_wstring(i) + L"\n";

        auto [dev, err] = os.detach();
        EXPECT_EQ(dev.str(), expected);
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterWchar, WriteWithANullSourceIsRejectedUnlessTheCountIsZero)
{
    auto helper = []<template <typename, typename> class T>()
    {
        // write() with a null source and a non-zero count is rejected with stream_error
        // -> strfailbit; no mask means no throw.
        T os{mem_device{std::wstring(L"")}};
        EXPECT_NO_THROW(os.write(nullptr, 5));
        EXPECT_TRUE(os.rdstate() & ios_defs::strfailbit);

        // null source with a zero count is a well-defined no-op.
        T empty{mem_device{std::wstring(L"")}};
        empty.write(nullptr, 0);
        EXPECT_FALSE(empty.rdstate() & ios_defs::strfailbit);
        auto [dev, err] = empty.detach();
        EXPECT_TRUE(dev.str().empty());
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// Availability is probed through `insertable` (support/io_traits_probe.h) rather than
// `requires { oss << v; }`, so a failure points at io_traits itself and not at the
// value-category and decay handling operator<< layers on top of it.
static_assert( insertable<wchar_t, char> );
static_assert( insertable<wchar_t, wchar_t> );
static_assert( insertable<wchar_t, std::nullptr_t> );
static_assert( !insertable<wchar_t, char8_t> );
static_assert( !insertable<wchar_t, char16_t> );
static_assert( !insertable<wchar_t, char32_t> );

// The string-pointer overloads mirror the single-character ones exactly.
static_assert( insertable<wchar_t, const char*> );
static_assert( insertable<wchar_t, const wchar_t*> );
static_assert( !insertable<wchar_t, const char8_t*> );
static_assert( !insertable<wchar_t, const char16_t*> );
static_assert( !insertable<wchar_t, const char32_t*> );
// A wide stream has no signed char / unsigned char string overload; as for
// std::wostream these reach the address path instead.
static_assert( insertable<wchar_t, const unsigned char*> );
static_assert( insertable<wchar_t, int*> );

// On a wide stream signed char / unsigned char are numeric, as they are for
// std::wostream, where they reach operator<<(int) through integral promotion.
TEST(OstreamInsertCharacterWchar, SignedAndUnsignedCharAreNumbersOnAWideStream)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os{mem_device{std::wstring(L"")}};
            os << static_cast<signed char>(65) << L'-' << static_cast<unsigned char>(66);
            EXPECT_TRUE(os.good());
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), L"65-66");
        }
        {
            T os{mem_device{std::wstring(L"")}};
            os << 'A' << L'B';
            EXPECT_TRUE(os.good());
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), L"AB");
        }
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterWchar, NullptrIsWrittenAsAWordAndPaddedLikeOne)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os{mem_device{std::wstring(L"")}};
            os << nullptr;
            EXPECT_TRUE(os.good());
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), L"nullptr");
        }
        {
            // << nullptr is a formatted output function: it pads to width() and then
            // clears it. Skipping the clear would leak the width into the next
            // insertion, so L'|' is what actually pins that half down.
            T os{mem_device{std::wstring(L"")}};
            os << setw(10) << nullptr << L'|';
            EXPECT_TRUE(os.good());
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), L"   nullptr|");
        }
        {
            T os{mem_device{std::wstring(L"")}};
            os << setw(10) << left << setfill(L'*') << nullptr << L'|';
            EXPECT_TRUE(os.good());
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), L"nullptr***|");
        }
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterWchar, ANarrowStringIsWidenedIntoAWideStream)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            // A narrow string is widened into a wide stream, matching the charT-templated
            // operator<<(basic_ostream<charT>&, const char*). It used to print an address.
            T           os{mem_device{std::wstring(L"")}};
            const char* p = "yo";
            os << "hi" << L'/' << p;
            EXPECT_TRUE(os.good());
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), L"hi/yo");
        }
        {
            T os{mem_device{std::wstring(L"")}};
            os << setw(5) << "hi" << L'|';
            EXPECT_TRUE(os.good());
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), L"   hi|");
        }
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterWchar, ANarrowCharacterIsWidenedBeforeFieldPadding)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os{mem_device{L""}, locale<wchar_t>("C")};
        os << setw(3) << 'x' << L'|';

        EXPECT_TRUE(os.good());
        EXPECT_EQ(os.device().str(), L"  x|");
        EXPECT_EQ(os.width(), 0u);
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterWchar, AMissingCtypeFacetRejectsValuesThatNeedWidening)
{
    const auto loc = locale<wchar_t>("C").remove<ctype_conf<wchar_t>>();

    ostream character{mem_device{L""}, loc};
    character << 'x';
    EXPECT_TRUE(character.str_fail());
    EXPECT_TRUE(character.device().str().empty());

    // Widening needs the facet, so this throws before reaching the code that spends the width.
    // The leftover would otherwise pad the next, unrelated insertion.
    ostream string{mem_device{L""}, loc};
    string.width(10);
    string << "text";
    EXPECT_TRUE(string.str_fail());
    EXPECT_TRUE(string.device().str().empty());
    EXPECT_EQ(string.width(), 0u);

    // Same function, its null-pointer guard rather than its facet lookup.
    ostream null_narrow{mem_device{L""}, loc};
    null_narrow.width(10);
    null_narrow << static_cast<const char*>(nullptr);
    EXPECT_TRUE(null_narrow.str_fail());
    EXPECT_TRUE(null_narrow.device().str().empty());
    EXPECT_EQ(null_narrow.width(), 0u);
}

// A volatile key must land in the same specialization as the unqualified one, which
// on a wide stream means the character types widen and signed / unsigned char stay
// numeric -- exactly the split pinned by the two tests above.
TEST(OstreamInsertCharacterWchar, VolatileValuesLandInTheSameSpecializationAsPlainOnes)
{
    auto helper = []<template <typename, typename> class T>()
    {
        volatile char          vc  = 'A';
        volatile wchar_t       vwc = L'B';
        volatile signed char   vsc = 65;
        volatile unsigned char vuc = 66;

        T os{mem_device{std::wstring(L"")}};
        os << vc << vwc << vsc << L'-' << vuc;
        EXPECT_TRUE(os.good());
        auto [dev, err] = os.detach();
        EXPECT_EQ(dev.str(), L"AB65-66");
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}
