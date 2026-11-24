// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * ws on the two character types the char and wchar_t files do not reach.
 *
 * ws asks ctype whether each code unit is whitespace, so the per-type claim is that the answer is
 * about code units and stops at the first one that is not whitespace -- including when that unit
 * is the lead byte of a multi-byte sequence, which is the char8_t-specific risk: a lead byte must
 * not be mistaken for whitespace, and ws must leave the whole sequence unconsumed.
 *
 * The facet-failure and tie paths are character-type-independent and stay in the char file.
 *
 * locale<char8_t>("C") throws from collate_conf and a default-constructed one follows the
 * environment, so the char8_t cases name a UTF-8 locale explicitly. char32_t streams exist on
 * glibc alone (wchar_t_is_utf32); elsewhere their cases are left out or skipped.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/platform_macros.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

namespace
{
    // A UTF-8 locale every platform has. Windows has no "C.UTF-8".
#if !IOV2_TEST_WINDOWS
    constexpr const char* kUtf8 = "C.UTF-8";
#else
    constexpr const char* kUtf8 = "en_US.UTF-8";
#endif
}

TEST(IstreamWsUtf, WsDiscardsWhitespaceAndStopsOnTheFirstOther)
{
    {
        istream is{mem_device{std::u8string(u8" \t\n\v\f\r x")}, locale<char8_t>(kUtf8)};
        is >> ws;
        EXPECT_EQ(is.peek(), u8'x');
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
    }
#if IOV2_TEST_LINUX_GLIBC
    {
        istream is{mem_device{std::u32string(U" \t\n\v\f\r x")}, locale<char32_t>("C")};
        is >> ws;
        EXPECT_EQ(is.peek(), U'x');
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
    }
#endif
}

// The lead byte of a multi-byte sequence is not whitespace, so ws stops in front of it and the
// sequence is still there to be read whole.
TEST(IstreamWsUtf, WsStopsInFrontOfAMultiByteSequenceWithoutSplittingIt)
{
    istream is{mem_device{std::u8string(u8"   中é")}, locale<char8_t>(kUtf8)};

    is >> ws;
    EXPECT_EQ(is.rdstate(), ios_defs::goodbit);

    std::u8string rest;
    is >> rest;
    EXPECT_EQ(rest, std::u8string(u8"中é"));
    EXPECT_EQ(rest.size(), 5u);
}

TEST(IstreamWsUtf, WsOnANonWhitespaceCharacterChangesNothing)
{
#if IOV2_TEST_LINUX_GLIBC
    istream is{mem_device{std::u32string(U"abc")}, locale<char32_t>("C")};

    const ios_defs::iostate before = is.rdstate();
    is >> ws;

    EXPECT_EQ(is.rdstate(), before);
    EXPECT_EQ(is.peek(), U'a');
#else
    GTEST_SKIP() << "char32_t streams need wchar_t_is_utf32 (glibc)";
#endif
}

TEST(IstreamWsUtf, WsAtTheEndSetsEndOfFileWithoutFailing)
{
    {
        istream is{mem_device{std::u8string(u8"   \t\n")}, locale<char8_t>(kUtf8)};
        is >> ws;
        EXPECT_TRUE(is.eof());
        EXPECT_FALSE(is.rdstate() & ios_defs::strfailbit);
        EXPECT_TRUE(static_cast<bool>(is));
    }
#if IOV2_TEST_LINUX_GLIBC
    {
        istream is{mem_device{std::u32string(U"   \t\n")}, locale<char32_t>("C")};
        is >> ws;
        EXPECT_TRUE(is.eof());
        EXPECT_FALSE(is.rdstate() & ios_defs::strfailbit);
        EXPECT_TRUE(static_cast<bool>(is));
    }
#endif
}

// The function-pointer overload is found only if its parameter names this stream's ios_base.
TEST(IstreamWsUtf, AFunctionPointerManipulatorDispatchesOnEitherType)
{
    {
        istream    is{mem_device{std::u8string(u8"a")}, locale<char8_t>(kUtf8)};
        static int calls;
        calls = 0;

        is >> +[](ios_base<char8_t>&) { ++calls; };
        EXPECT_EQ(calls, 1);

        is >> static_cast<void (*)(ios_base<char8_t>&)>(nullptr);
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
    }
#if IOV2_TEST_LINUX_GLIBC
    {
        istream    is{mem_device{std::u32string(U"a")}, locale<char32_t>("C")};
        static int calls;
        calls = 0;

        is >> +[](ios_base<char32_t>&) { ++calls; };
        EXPECT_EQ(calls, 1);

        is >> static_cast<void (*)(ios_base<char32_t>&)>(nullptr);
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
    }
#endif
}
