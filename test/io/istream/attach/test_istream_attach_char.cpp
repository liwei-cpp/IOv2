// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * istream<char>::attach(): replacing the device a stream reads from.
 *
 * The state bits describe what happened on the device the stream is holding --
 * input read to the end, a parse that failed, a device that could not be
 * initialized.  Replacing that device makes all of it history, so attach()
 * clears the state before installing the new one.  The order matters and is not
 * observable from the outside except on the failure path: streambuf::attach()
 * installs the device first and initializes the converter second, and it is the
 * second step that can throw, so a clear placed after the replacement would not
 * run at all on the path that most needs it.
 */
#include <device/file_device.h>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>

#include <gtest/gtest.h>

#include <support/file_guard.h>

#include <string>

using namespace IOv2;

TEST(IstreamAttachChar, AttachClearsEndOfFile)
{
    // Both stream shapes share one streambuf, so both are asked.
    auto expect_cleared = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::string("ab")}};

        std::string tok;
        is >> tok;                                   // reads to the end -> eofbit
        EXPECT_EQ(tok, "ab");
        EXPECT_TRUE(is.eof());

        is.attach(mem_device{std::string("cd")});
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);  // not just eofbit: everything

        std::string tok2;
        is >> tok2;
        EXPECT_EQ(tok2, "cd");
    };

    expect_cleared.operator()<istream>();
    expect_cleared.operator()<iostream>();
}

// eofbit is the easy one to clear because the next read sets it again anyway.
// A parse failure is the bit that would otherwise stick.
TEST(IstreamAttachChar, AttachClearsAParseFailure)
{
    istream is{mem_device{std::string("zz")}};

    int v = 0;
    is >> v;
    EXPECT_TRUE(is.str_fail());

    is.attach(mem_device{std::string("42")});
    EXPECT_EQ(is.rdstate(), ios_defs::goodbit);

    is >> v;
    EXPECT_FALSE(is.str_fail());
    EXPECT_EQ(v, 42);
}

// A device that cannot be initialized is reported through the stream's error
// model rather than by throwing: attach() is an ordinary member function, unlike
// a constructor, whose member-initializer exception C++ requires to propagate.
TEST(IstreamAttachChar, ADeviceThatCannotBeInitializedIsReportedAsState)
{
    const std::string path = "test_istream_attach_failure_char_1.txt";
    file_guard        guard(path, std::string("zz"));

    istream<file_device<char>, char> is{file_device<char>(path)};

    int v = 0;
    is >> v;                                         // "zz" is not a number
    EXPECT_TRUE(is.str_fail());

    // A default-constructed file_device refers to no open file, so initializing
    // the converter over it throws inside attach().
    EXPECT_NO_THROW(is.attach(file_device<char>()));
    EXPECT_TRUE(is.dev_fail());

    // And the state describes this attach only: the earlier strfailbit came from
    // a device that no longer exists.
    EXPECT_FALSE(is.str_fail());
    EXPECT_EQ(is.rdstate(), ios_defs::devfailbit);

    // Installing a working device revives the stream.
    is.attach(file_device<char>(path));
    EXPECT_EQ(is.rdstate(), ios_defs::goodbit);

    std::string tok;
    is >> tok;
    EXPECT_EQ(tok, "zz");
    is.detach();
}
