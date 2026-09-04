// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Inserting characters and strings into an ostream<char>.
 *
 * These are formatted output functions, so [ostream.formatted.reqmts] applies
 * to all of them alike: the value is padded to width() with fill() on the side
 * that adjustfield names, a value at least as long as the width is written
 * whole and never truncated, and width() is reset to zero afterwards. That last
 * step is the one an implementation forgets, and forgetting it is invisible
 * until the next insertion, so every padding case below is followed by a second
 * insertion that would show the leak.
 *
 * The remaining tests are about which types reach the character path at all --
 * signed char, unsigned char and their string forms belong to it, the wide
 * character types must not, and cv-qualification must not move a value into the
 * arithmetic path.
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

TEST(OstreamInsertCharacterChar, AValueShorterThanTheWidthIsPaddedOnTheAdjustSide)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os{mem_device{""}};
            os.width(5);
            os.fill('.');
            os.flags(ios_defs::right);
            os << std::string("ab");
            EXPECT_EQ(os.device().str(), "...ab");
        }
        {
            T os{mem_device{""}};
            os.width(5);
            os.fill('.');
            os.flags(ios_defs::left);
            os << std::string("ab");
            EXPECT_EQ(os.device().str(), "ab...");
        }
        // An empty string is the limiting case: the field is all fill.
        {
            T os{mem_device{""}};
            os.width(5);
            os.fill('.');
            os.flags(ios_defs::right);
            os << std::string("");
            EXPECT_EQ(os.device().str(), ".....");
        }
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterChar, AValueAtLeastAsLongAsTheWidthIsWrittenWhole)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os{mem_device{""}};
            os.width(5);
            os.fill('.');
            os << std::string("abcde");            // exactly the width
            EXPECT_EQ(os.device().str(), "abcde");
        }
        {
            T os{mem_device{""}};
            os.width(5);
            os.fill('.');
            os << std::string("abcdefgh");         // longer than the width
            EXPECT_EQ(os.device().str(), "abcdefgh");
        }
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// The trailing '|' is the point: it would be padded too if width() had survived.
TEST(OstreamInsertCharacterChar, TheWidthIsClearedAfterEachInsertion)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os{mem_device{""}};
        os.fill('.');

        os << setw(5) << 'a' << '|';
        os << setw(5) << "bc" << '|';
        os << setw(5) << std::string("def") << '|';

        EXPECT_EQ(os.device().str(), "....a|...bc|..def|");
        EXPECT_EQ(os.width(), 0u);
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// A width far wider than the value still produces exactly that many characters,
// whether the value is one character or a whole string.
TEST(OstreamInsertCharacterChar, AWideFieldIsFilledInFull)
{
    auto helper = []<template <typename, typename> class T>()
    {
        constexpr std::size_t field = 200;

        {
            T os{mem_device{""}};
            os.width(field);
            os << 'a';
            EXPECT_TRUE(os.good());
            EXPECT_EQ(os.device().str().size(), field);
        }
        {
            const std::string value(50, 'a');
            T                 os{mem_device{""}};
            os.width(field);
            os << value.c_str();
            EXPECT_TRUE(os.good());
            EXPECT_EQ(os.device().str().size(), field);
        }
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// A null C string has no characters to write and no length to ask for, so it is
// refused rather than dereferenced. The stream is usable again once cleared.
TEST(OstreamInsertCharacterChar, ANullCStringIsRefusedWithoutWritingAnything)
{
    auto helper = []<template <typename, typename> class T>()
    {
        char* nothing = nullptr;

        T os{mem_device{""}};
        os.width(10);
        os << nothing;
        EXPECT_FALSE(static_cast<bool>(os));

        // Refusing early is no excuse for keeping the width: the leftover would pad whatever
        // the caller inserts after clearing.
        EXPECT_EQ(os.width(), 0u);

        os.flush();
        EXPECT_TRUE(os.device().str().empty());

        os.clear();
        os << "";
        EXPECT_TRUE(os.good());
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// Insertion after insertion, with nothing lost or doubled in between.
TEST(OstreamInsertCharacterChar, RepeatedInsertionsAccumulateInOrder)
{
    auto helper = []<template <typename, typename> class T>()
    {
        constexpr int rounds = 250;

        T os(mem_device{""});
        for (int i = 0; i < rounds; ++i)
            os << "line " << i << endl;
        EXPECT_TRUE(os.good());

        std::string expected;
        for (int i = 0; i < rounds; ++i)
            expected += "line " + std::to_string(i) + "\n";

        auto [dev, err] = os.detach();
        EXPECT_EQ(dev.str(), expected);
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterChar, WriteWithANullSourceIsRejectedUnlessTheCountIsZero)
{
    auto helper = []<template <typename, typename> class T>()
    {
        // write() with a null source and a non-zero count is rejected with stream_error
        // -> strfailbit. With no exception mask set it does not throw.
        T os{mem_device{std::string("")}};
        EXPECT_NO_THROW(os.write(nullptr, 5));
        EXPECT_TRUE(os.rdstate() & ios_defs::strfailbit);

        // write() of a null source with a zero count is the well-defined no-op: nothing is
        // emitted and no failure bit is set.
        T empty{mem_device{std::string("")}};
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
static_assert( insertable<char, signed char> );
static_assert( insertable<char, unsigned char> );
static_assert( insertable<char, std::nullptr_t> );
static_assert( !insertable<char, wchar_t> );
static_assert( !insertable<char, char8_t> );
static_assert( !insertable<char, char16_t> );
static_assert( !insertable<char, char32_t> );

// The string-pointer overloads mirror the single-character ones exactly.
static_assert( insertable<char, const char*> );
static_assert( insertable<char, const signed char*> );
static_assert( insertable<char, const unsigned char*> );
static_assert( !insertable<char, const wchar_t*> );
static_assert( !insertable<char, const char8_t*> );
static_assert( !insertable<char, const char16_t*> );
static_assert( !insertable<char, const char32_t*> );
// A non-character pointer keeps the address path, as it does for std::ostream.
static_assert( insertable<char, int*> );
static_assert( insertable<char, void*> );

TEST(OstreamInsertCharacterChar, SignedAndUnsignedCharAreWrittenAsCharacters)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os{mem_device{""}};
        os << static_cast<signed char>('A') << static_cast<unsigned char>('B');
        EXPECT_TRUE(os.good());
        auto [dev, err] = os.detach();
        EXPECT_EQ(dev.str(), "AB");
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterChar, NullptrIsWrittenAsAWordAndPaddedLikeOne)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            T os{mem_device{""}};
            os << nullptr;
            EXPECT_TRUE(os.good());
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), "nullptr");
        }
        {
            // << nullptr is a formatted output function: it pads to width() and then
            // clears it. Skipping the clear would leak the width into the next
            // insertion, so '|' is what actually pins that half down.
            T os{mem_device{""}};
            os << setw(10) << nullptr << '|';
            EXPECT_TRUE(os.good());
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), "   nullptr|");
        }
        {
            T os{mem_device{""}};
            os << setw(10) << left << setfill('*') << nullptr << '|';
            EXPECT_TRUE(os.good());
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), "nullptr***|");
        }
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

TEST(OstreamInsertCharacterChar, AMissingCtypeFacetRejectsCharacterAndNullptrFormatting)
{
    const auto loc = locale<char>("C").remove<ctype_conf<char>>();

    ostream character{mem_device{""}, loc};
    character << 'x';
    EXPECT_TRUE(character.str_fail());
    EXPECT_TRUE(character.device().str().empty());

    // The width is owed even here: these throw before reaching the code that spends it, and a
    // leftover would pad the next, unrelated insertion.
    ostream null_pointer{mem_device{""}, loc};
    null_pointer.width(10);
    null_pointer << nullptr;
    EXPECT_TRUE(null_pointer.str_fail());
    EXPECT_TRUE(null_pointer.device().str().empty());
    EXPECT_EQ(null_pointer.width(), 0u);
}

TEST(OstreamInsertCharacterChar, SignedAndUnsignedCharStringsAreWrittenAsText)
{
    auto helper = []<template <typename, typename> class T>()
    {
        {
            // A char stream writes signed char / unsigned char strings as text, byte for
            // byte. Without their own writers these are swallowed by the generic pointer
            // io_traits and come out as an address, with the stream still good().
            T os{mem_device{""}};
            os << reinterpret_cast<const unsigned char*>("hi")
               << '/'
               << reinterpret_cast<const signed char*>("yo");
            EXPECT_TRUE(os.good());
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), "hi/yo");
        }
        {
            // Being a formatted output function, it pads and then clears width; '|' pins
            // the clearing half down.
            T os{mem_device{""}};
            os << setw(6) << reinterpret_cast<const unsigned char*>("hi") << '|';
            EXPECT_TRUE(os.good());
            auto [dev, err] = os.detach();
            EXPECT_EQ(dev.str(), "    hi|");
        }
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// A volatile key must land in the same specialization as the unqualified one. It used
// to reach the arithmetic io_traits instead -- is_arithmetic_v ignores cv while the
// exclusions on char / wchar_t / charN_t do not -- and every one of these came out as
// a number, with the stream still good().
TEST(OstreamInsertCharacterChar, VolatileCharactersStayOnTheCharacterPath)
{
    auto helper = []<template <typename, typename> class T>()
    {
        volatile char          vc  = 'x';
        volatile signed char   vsc = 'A';
        volatile unsigned char vuc = 'B';

        T os{mem_device{""}};
        os << vc << vsc << vuc;
        EXPECT_TRUE(os.good());
        auto [dev, err] = os.detach();
        EXPECT_EQ(dev.str(), "xAB");
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}
