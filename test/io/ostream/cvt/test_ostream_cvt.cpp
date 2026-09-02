// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * An output stream with a converter pipeline under it.
 *
 * A creator built with operator| stacks converters, and what reaches the device
 * is the result of running them in order: here the text is first encoded as
 * UTF-8 and then enciphered, so the bytes at the device are the UTF-8 bytes
 * shifted by the repeating key. Checking them one at a time is the point --
 * a stack that ran its stages in the wrong order, or dropped one, would still
 * produce a plausible-looking length.
 *
 * The last test is the other half of that: appmode needs to reposition to the
 * end before every insertion, which a variable-length encoding cannot do, so
 * the combination has to fail rather than write somewhere wrong.
 */
#include <cvt/code_cvt.h>
#include <cvt/crypt/vigenere_cvt.h>
#include <cvt/cvt_pipe_creator.h>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/iostream.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <type_traits>

using namespace IOv2;

namespace
{
    // "李伟" as UTF-8, which is what the cipher stage is handed.
    constexpr char kPlainBytes[] = {'1', '0', '2', '4', ' ',
                                    '\xE6', '\x9D', '\x8E', '\xE4', '\xBC', '\x9F'};
    constexpr char kKey[]        = "abcdefg";

    // The vigenere stage shifts byte i by key[i % 7]; the key is 7 long and the
    // text 11, so the wrap-around is exercised as well.
    std::string ciphered()
    {
        std::string out;
        for (std::size_t i = 0; i < sizeof(kPlainBytes); ++i)
            out += static_cast<char>(static_cast<unsigned char>(kPlainBytes[i])
                                     + static_cast<unsigned char>(kKey[i % 7]));
        return out;
    }
}

TEST(OstreamCvt, APipelineRunsItsStagesInOrder)
{
    auto creator = Crypt::Classic::vigenere_cvt_creator(kKey) |
                   code_cvt_creator<char, char32_t>("zh_CN.UTF-8");

    auto helper = [&creator]<template <typename, typename> class T>()
    {
        T os(mem_device{""}, creator, locale<char32_t>("C"));
        static_assert(std::is_same_v<typename decltype(os)::char_type, char32_t>);

        os << 1024 << U' ' << U"李伟";

        auto [dev, err] = os.detach();
        EXPECT_TRUE(os.good());
        EXPECT_EQ(dev.str(), ciphered());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamCvt, TheSameHoldsThroughTheSyncView)
{
    auto creator = Crypt::Classic::vigenere_cvt_creator(kKey) |
                   code_cvt_creator<char, char32_t>("zh_CN.UTF-8");

    auto helper = [&creator]<template <typename, typename> class T>()
    {
        T os(mem_device{""}, creator, locale<char32_t>("C"));
        static_assert(std::is_same_v<typename decltype(os)::char_type, char32_t>);

        IOv2::sync(os).stream << 1024 << U' ' << U"李伟";

        auto [dev, err] = IOv2::sync(os).stream.detach();
        EXPECT_TRUE(os.good());
        EXPECT_EQ(dev.str(), ciphered());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// Appmode requires a fixed-length, state-independent encoding so the output sentry can
// reposition to the end (rseek(0)) before every insertion. A UTF-8 code_cvt is
// variable-length, so once appmode is set, the sentry's rseek(0) throws cvt_error; the
// sentry rewraps it and operator<< classifies it into cvtfailbit. With no exception mask
// set nothing escapes to the caller. This drives the appmode/cvt_error branch of out_sentry.
TEST(OstreamCvt, AppmodeOverAVariableLengthEncodingIsRefused)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto creator = code_cvt_creator<char, char32_t>("zh_CN.UTF-8");
        T    os(mem_device{""}, creator, locale<char32_t>("C"));

        os << U"李伟";               // ordinary (non-appmode) insertion succeeds
        os.flush();
        ASSERT_TRUE(os.good());

        os << appmode;               // request append semantics
        ASSERT_TRUE(os.good());

        os << U"X";                  // sentry rseek(0) over UTF-8 -> cvt_error -> cvtfailbit
        EXPECT_FALSE(os.good());
        EXPECT_TRUE(os.rdstate() & ios_defs::cvtfailbit);
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}
