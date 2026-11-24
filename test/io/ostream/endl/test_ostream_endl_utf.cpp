// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * endl on the two character types the char and wchar_t files do not reach.
 *
 * Only what is character-type-specific is repeated here, on the same reasoning the wchar_t file
 * gives: the ctype-facet failure path is checked once in the char file, and what differs per type
 * is dispatch -- the function-pointer overload has to name ios_base<char8_t> / ios_base<char32_t>
 * to be found at all -- plus the claim that endl contributes exactly one code unit whatever text
 * sits beside it. On char8_t that claim has teeth: a code unit there is a byte, so neighbouring
 * multi-byte text is measured in the same units endl writes in.
 *
 * A char8_t locale must be a UTF-8 one. locale<char8_t>("C") throws from collate_conf, and a
 * default-constructed one follows the environment -- which means it throws under LC_ALL=C. Every
 * char8_t case below therefore names a UTF-8 locale explicitly rather than relying on the default.
 * char32_t has no such constraint and takes "C"; its streams exist on glibc alone
 * (wchar_t_is_utf32), and its cases are skipped or left out elsewhere.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/ostream.h>
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

TEST(OstreamEndlUtf, EndlWritesOneCodeUnitOnAUtf8Stream)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{std::u8string()}, locale<char8_t>(kUtf8));

        os << endl;
        EXPECT_EQ(os.device().str(), std::u8string(u8"\n"));
        EXPECT_TRUE(os.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// On char8_t a code unit is a byte, so the multi-byte text around endl is counted in the same
// units endl writes in: "中" is three of them, "é" two, and each endl exactly one.
TEST(OstreamEndlUtf, MultiByteTextDoesNotChangeWhatEndlContributes)
{
    auto os = ostream(mem_device{std::u8string()}, locale<char8_t>(kUtf8));

    os << u8"中é" << endl << u8"漢字ξ" << endl << u8"z";

    EXPECT_EQ(os.device().str(), std::u8string(u8"中é\n漢字ξ\nz"));
    EXPECT_EQ(os.device().str().size(), 3u + 2u + 1u + 3u + 3u + 2u + 1u + 1u);
}

TEST(OstreamEndlUtf, EndlWritesOneCodeUnitOnAUtf32Stream)
{
#if IOV2_TEST_LINUX_GLIBC
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{std::u32string()}, locale<char32_t>("C"));

        os << endl;
        EXPECT_EQ(os.device().str(), std::u32string(U"\n"));
        EXPECT_TRUE(os.good());

        os << ends;
        EXPECT_EQ(os.device().str().size(), 2u);

        os << flush;
        EXPECT_TRUE(os.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
#else
    GTEST_SKIP() << "char32_t streams need wchar_t_is_utf32 (glibc)";
#endif
}

// On char32_t one character is one code unit, so the same text that took 15 bytes above is 7.
TEST(OstreamEndlUtf, AUtf32StreamCountsCharactersNotBytes)
{
#if IOV2_TEST_LINUX_GLIBC
    auto os = ostream(mem_device{std::u32string()}, locale<char32_t>("C"));

    os << U"中é" << endl << U"漢字ξ" << endl << U"z";

    EXPECT_EQ(os.device().str(), std::u32string(U"中é\n漢字ξ\nz"));
    EXPECT_EQ(os.device().str().size(), 8u);
#else
    GTEST_SKIP() << "char32_t streams need wchar_t_is_utf32 (glibc)";
#endif
}

// The function-pointer overload is found only if its parameter names this stream's ios_base.
TEST(OstreamEndlUtf, AFunctionPointerManipulatorDispatchesOnEitherType)
{
    {
        auto       os = ostream(mem_device{std::u8string()}, locale<char8_t>(kUtf8));
        static int calls;
        calls = 0;

        os << +[](ios_base<char8_t>&) { ++calls; };
        EXPECT_EQ(calls, 1);

        os << static_cast<void (*)(ios_base<char8_t>&)>(nullptr);
        EXPECT_TRUE(os.rdstate() & ios_defs::strfailbit);
    }
#if IOV2_TEST_LINUX_GLIBC
    {
        auto       os = ostream(mem_device{std::u32string()}, locale<char32_t>("C"));
        static int calls;
        calls = 0;

        os << +[](ios_base<char32_t>&) { ++calls; };
        EXPECT_EQ(calls, 1);

        os << static_cast<void (*)(ios_base<char32_t>&)>(nullptr);
        EXPECT_TRUE(os.rdstate() & ios_defs::strfailbit);
    }
#endif
}
