// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The appmode / noappmode manipulators on an ostream<char>.
 *
 * appmode is IOv2's own: while it is on, every write goes to the end of the
 * stream no matter where the position was left, and it does not move the
 * position for anything else. So a seek followed by a write under appmode
 * appends, while the same seek followed by a write under noappmode overwrites
 * exactly where the seek landed -- the two are checked in one interleaved
 * sequence, because a flag that is only ever turned on tells you nothing about
 * whether it can be turned off again.
 *
 * Each case is checked twice, once directly and once through the synchronized
 * view, since that view reaches the stream by a different path and has to end
 * up in the same place.
 */
#include <device/file_device.h>
#include <device/mem_device.h>
#include <io/io_manip.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/traits/char_and_str.h>

#include <support/file_guard.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

TEST(OstreamAppmodeChar, AppmodeAppendsWhileNoappmodeWritesWhereTheSeekLanded)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os{mem_device{"abcde"}};

        os.put('L');                   // no appmode: overwrites at the start
        os.flush();
        EXPECT_EQ(os.device().str(), "Lbcde");

        os << appmode;
        os.put('W');                   // appended, wherever the position was
        os.flush();
        EXPECT_EQ(os.device().str(), "LbcdeW");

        os.seek(0);
        ASSERT_TRUE(static_cast<bool>(os));

        os.put('X');                   // still appmode: the seek does not matter
        os.flush();

        os.seek(1);
        ASSERT_TRUE(static_cast<bool>(os));
        os << noappmode;
        os.put('Y');                   // now the seek does matter
        os << appmode;
        os.flush();

        EXPECT_EQ(os.device().str(), "LYcdeWX");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamAppmodeChar, TheSameHoldsThroughTheSyncView)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os{mem_device{"abcde"}};

        IOv2::sync(os).stream.put('L');
        IOv2::sync(os).stream.flush();
        EXPECT_EQ(IOv2::sync(os).stream.device().str(), "Lbcde");

        IOv2::sync(os).stream << appmode;
        IOv2::sync(os).stream.put('W');
        IOv2::sync(os).stream.flush();
        EXPECT_EQ(IOv2::sync(os).stream.device().str(), "LbcdeW");

        IOv2::sync(os).stream.seek(0);
        ASSERT_TRUE(static_cast<bool>(IOv2::sync(os).stream));

        IOv2::sync(os).stream.put('X');
        IOv2::sync(os).stream.flush();

        IOv2::sync(os).stream.seek(1);
        ASSERT_TRUE(static_cast<bool>(IOv2::sync(os).stream));
        IOv2::sync(os).stream << noappmode;
        IOv2::sync(os).stream.put('Y');
        IOv2::sync(os).stream << appmode;
        IOv2::sync(os).stream.flush();

        EXPECT_EQ(IOv2::sync(os).stream.device().str(), "LYcdeWX");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// The same sequence against a real file, where "the end" is the file's end and
// the answer has to survive being written out and read back.
TEST(OstreamAppmodeChar, AppmodeMeansTheEndOfTheFile)
{
    auto helper = []<template <typename, typename> class T>()
    {
        file_guard g("appmode_test", "abcde");
        T          os{file_device<char>{"appmode_test"}};

        os.put('L');
        os << appmode;
        os.put('W');
        os.seek(0);
        os.put('X');
        os.seek(1);
        os << noappmode;
        os.put('Y');
        os << appmode;
        os.flush();
        EXPECT_TRUE(static_cast<bool>(os));

        auto [dev, err] = os.detach();
        dev.close();

        EXPECT_EQ(g.contents(), "LYcdeWX");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamAppmodeChar, TheSameHoldsOnAFileThroughTheSyncView)
{
    auto helper = []<template <typename, typename> class T>()
    {
        file_guard g("appmode_test", "abcde");
        T          os{file_device<char>{"appmode_test"}};

        IOv2::sync(os).stream.put('L');
        IOv2::sync(os).stream << appmode;
        IOv2::sync(os).stream.put('W');
        IOv2::sync(os).stream.seek(0);
        IOv2::sync(os).stream.put('X');
        IOv2::sync(os).stream.seek(1);
        IOv2::sync(os).stream << noappmode;
        IOv2::sync(os).stream.put('Y');
        IOv2::sync(os).stream << appmode;
        IOv2::sync(os).stream.flush();
        EXPECT_TRUE(static_cast<bool>(IOv2::sync(os).stream));

        auto [dev, err] = IOv2::sync(os).stream.detach();
        dev.close();

        EXPECT_EQ(g.contents(), "LYcdeWX");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// appmode is about writing. Reading is left where it was -- at the end here,
// so the read fails on eof -- and writing afterwards still appends.
TEST(OstreamAppmodeChar, AppmodeDoesNotMoveTheReadPosition)
{
    iostream ios{mem_device{"abcde"}};

    ios << appmode << 'W';
    ios.flush();
    EXPECT_EQ(ios.device().str(), "abcdeW");

    char ch = 0;
    ios.get(ch);
    EXPECT_FALSE(static_cast<bool>(ios));
    EXPECT_EQ(ch, 0);
    EXPECT_TRUE(ios.eof());
    ios.clear();

    ios << " hello";

    auto [dev, err] = ios.detach();
    EXPECT_EQ(dev.str(), "abcdeW hello");
}

TEST(OstreamAppmodeChar, TheSameWhileReadingThroughTheSyncView)
{
    iostream ios{mem_device{"abcde"}};

    IOv2::sync(ios).stream << appmode << 'W';
    IOv2::sync(ios).stream.flush();
    EXPECT_EQ(IOv2::sync(ios).stream.device().str(), "abcdeW");

    char ch = 0;
    IOv2::sync(ios).stream.get(ch);
    EXPECT_FALSE(static_cast<bool>(IOv2::sync(ios).stream));
    EXPECT_EQ(ch, 0);
    EXPECT_TRUE(IOv2::sync(ios).stream.eof());
    IOv2::sync(ios).stream.clear();

    IOv2::sync(ios).stream << " hello";

    auto [dev, err] = IOv2::sync(ios).stream.detach();
    EXPECT_EQ(dev.str(), "abcdeW hello");
}
