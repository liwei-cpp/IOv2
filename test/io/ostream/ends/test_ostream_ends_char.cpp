// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The ends manipulator on an ostream<char>.
 *
 * [ostream.manip] gives ends one sentence: it inserts a null character into the
 * output sequence and returns the stream. Two things follow that a caller can
 * get wrong. It writes charT(), not the digit '0' and not a space, so what
 * lands in the device is one byte of value zero. And it does not flush -- endl
 * is the manipulator that flushes, ends is not -- so nothing about the device
 * is guaranteed to have changed until the stream is flushed by other means.
 *
 * Its reason to exist is terminating a C string inside a buffer, so the last
 * test reads the buffer back the way such a caller would.
 *
 * The other half of the contract -- that ends, unlike endl, does not flush --
 * is not asserted anywhere here, because it is not observable through the
 * public interface: an IOv2 stream over mem_device hands every write to the
 * device as it happens, with or without a converter in the stack, so a flush
 * afterwards changes nothing either way.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

using namespace IOv2;

TEST(OstreamEndsChar, EndsWritesOneNullCharacter)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{""});

        os << ends;
        os.flush();

        const std::string out = os.device().str();
        ASSERT_EQ(out.size(), 1u);
        EXPECT_EQ(out[0], char());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamEndsChar, EachEndsAddsExactlyOneCharacterWhereItWasWritten)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{""});

        os << "first" << ends << "second" << ends << "third";
        os.flush();

        const std::string expected("first\0second\0third", 18);
        EXPECT_EQ(os.device().str(), expected);
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// ends returns the stream, which is what let the expression above chain; here
// the same thing is checked one insertion at a time.
TEST(OstreamEndsChar, EndsReturnsTheStream)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{""});

        auto& same = (os << ends);
        EXPECT_EQ(&same, &os);
        same << "tail";
        os.flush();

        EXPECT_EQ(os.device().str(), std::string("\0tail", 5));
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// What ends is for: the buffer can afterwards be walked as a sequence of C
// strings, which only works because the terminator is a real zero byte.
TEST(OstreamEndsChar, TerminatedRecordsCanBeReadBackAsCStrings)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{""});

        os << "alpha" << ends << "" << ends << "gamma" << ends;
        os.flush();

        const std::string   out = os.device().str();
        std::vector<std::string> records;
        for (const char* p = out.data(); p < out.data() + out.size(); p += std::strlen(p) + 1)
            records.emplace_back(p);

        ASSERT_EQ(records.size(), 3u);
        EXPECT_EQ(records[0], "alpha");
        EXPECT_EQ(records[1], "");
        EXPECT_EQ(records[2], "gamma");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}
