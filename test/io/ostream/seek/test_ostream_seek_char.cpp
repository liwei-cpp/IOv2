// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Positioning an ostream<char>: seek, rseek and tell.
 *
 * The three are one contract. tell says where the next character will be
 * written, seek names that place counting from the beginning, and rseek names
 * it counting from the end -- rseek is not "relative to here", which is the one
 * thing about the pair a caller is likely to get backwards. Both clear eofbit
 * before they move, so a stream that has been read to the end is usable again
 * afterwards without an explicit clear().
 *
 * A refused seek is reported through the state bits rather than by throwing,
 * and the write that follows it is dropped, so the tests check the device as
 * well as the state.
 */
#include <IOv2/device/file_device.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/file_guard.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

using namespace IOv2;

TEST(OstreamSeekChar, SeekPutsTheNextWriteWhereItWasTold)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{""});
        os << "abcdef";

        os.seek(2);
        os << "XY";

        EXPECT_EQ(os.device().str(), "abXYef");
        EXPECT_TRUE(os.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// rseek counts from the end, so rseek(2) on six characters lands on the fifth.
TEST(OstreamSeekChar, RseekIsMeasuredFromTheEndNotFromHere)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{""});
        os << "abcdef";

        os.rseek(2);
        os << "XY";

        EXPECT_EQ(os.device().str(), "abcdXY");
        EXPECT_TRUE(os.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamSeekChar, TellReportsWhereTheNextWriteWillLand)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{""});

        ASSERT_TRUE(os.tell().has_value());
        EXPECT_EQ(os.tell().value(), 0u);

        os << "abcdef";
        EXPECT_EQ(os.tell().value(), 6u);

        os.seek(2);
        EXPECT_EQ(os.tell().value(), 2u);

        os << "XY";
        EXPECT_EQ(os.tell().value(), 4u);

        os.rseek(1);
        EXPECT_EQ(os.tell().value(), 5u);
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// Seeking outside the stream is refused rather than silently growing it: the
// device is reported to have failed, the stream is reported to have failed with
// it, and the write that follows never reaches the device.
TEST(OstreamSeekChar, SeekPastTheEndIsRefusedAndTakesTheWriteWithIt)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{""});
        os << "abcdef";

        // The seek itself is refused by the device.
        os.seek(10);
        EXPECT_EQ(os.rdstate(), ios_defs::devfailbit);

        // The write that follows is then dropped, and says so in its own bit.
        os << "Z";
        EXPECT_TRUE(os.str_fail());
        EXPECT_EQ(os.device().str(), "abcdef");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamSeekChar, SeekAndRseekReturnTheStream)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{""});
        os << "abcdef";

        EXPECT_EQ(&os.seek(0), &os);
        EXPECT_EQ(&os.rseek(0), &os);

        os.seek(0) << "AB";
        EXPECT_EQ(os.device().str(), "ABcdef");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// Both seek and rseek clear eofbit before moving, so a stream read to the end
// can be written again without an explicit clear().
TEST(OstreamSeekChar, SeekingClearsEofbit)
{
    iostream ios(mem_device{std::string("ab")});

    char c = 0;
    while (ios.get(c))
        ;
    ASSERT_TRUE(ios.eof());

    // Reading past the end left strfailbit set as well; seek clears eofbit and
    // nothing else, so the stream is still failed and still refuses writes.
    ios.seek(0);
    EXPECT_FALSE(ios.eof());
    EXPECT_TRUE(ios.rdstate() & ios_defs::strfailbit);

    ios << "XY";
    EXPECT_EQ(ios.device().str(), "ab");

    ios.clear();
    ios << "XY";
    EXPECT_EQ(ios.device().str(), "XY");
}

TEST(OstreamSeekChar, TellOnAFailedStreamHasNoValue)
{
    ostream os(mem_device{""});
    os << "abcdef";
    ASSERT_TRUE(os.tell().has_value());

    os.seek(10);                       // refused: the stream is now failed
    EXPECT_FALSE(os.tell().has_value());

    os.clear();
    EXPECT_TRUE(os.tell().has_value());
}

// The same over a file, where the position has to survive going through the
// device: one fixed-width record is rewritten in place and the rest of the file
// must come back untouched.
TEST(OstreamSeekChar, ARecordRewrittenInAFileLeavesTheRestIntact)
{
    // trunc is spelled out so that both devices create the file: ofile_device opens "w" either
    // way, but file_device without it opens "r+" and would fail on a file that does not exist.
    auto helper = []<template <typename, typename> class T, typename TDevice>()
    {
        const std::string path = "test_ostream_seek_records.txt";
        file_guard        guard(path);

        constexpr int record_count = 8;
        constexpr int record_width = 6;        // "nn:xx\n"

        {
            T os{TDevice{path, file_open_flag::trunc}};
            ASSERT_TRUE(static_cast<bool>(os));

            for (int i = 0; i < record_count; ++i)
                os << setw(2) << setfill('0') << i << ":ab\n";

            os.seek(3 * record_width);
            os << "99:ZZ\n";
            EXPECT_TRUE(os.good());

            auto [dev, err] = os.detach();
            dev.close();
        }

        istream is{ifile_device<char>{path}};
        ASSERT_TRUE(static_cast<bool>(is));

        std::string expected;
        for (int i = 0; i < record_count; ++i)
        {
            if (i == 3)
                expected += "99:ZZ\n";
            else
                expected += std::string(1, '0') + static_cast<char>('0' + i) + ":ab\n";
        }

        std::string got(expected.size() + 1, '\0');
        char*       end = is.read(got.data(), static_cast<std::ptrdiff_t>(got.size()));
        got.resize(static_cast<std::size_t>(end - got.data()));
        EXPECT_EQ(got, expected);

        auto [dev, err] = is.detach();
        dev.close();
    };

    helper.template operator()<ostream, ofile_device<char>>();
    helper.template operator()<iostream, file_device<char>>();
}
