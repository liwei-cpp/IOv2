// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The endl manipulator on an ostream<char>, and around it the manipulator
 * dispatch it shares with ends and flush.
 *
 * [ostream.manip] gives endl two steps: insert widen('\n'), then flush. The
 * widening is the part that can fail, because it goes through the ctype facet,
 * and the tests below pin down what the stream looks like when it does.
 *
 * The rest of the file is about how a manipulator reaches the stream at all.
 * endl, ends and flush are tag objects whose whole behaviour lives in
 * io_traits, and os << m is the only entry -- the standard's direct-call form,
 * endl(os), does not exist here. A function pointer is the one manipulator
 * shape that bypasses io_traits, so it has to be checked separately.
 */
#include <device/mem_device.h>
#include <facet/ctype.h>
#include <io/iostream.h>
#include <io/ostream.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

TEST(OstreamEndlChar, EndlWritesANewline)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{std::string("")});

        os << endl;
        EXPECT_EQ(os.device().str(), "\n");
        EXPECT_TRUE(os.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// One newline per call, at the position writing has reached, so the text comes
// back as lines rather than as one run.
TEST(OstreamEndlChar, EachEndlEndsTheLineItWasWrittenOn)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{std::string("")});

        os << "first" << endl << "second" << endl << "third";
        EXPECT_EQ(os.device().str(), "first\nsecond\nthird");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// ends and flush reach the stream the same way endl does; that they do is the
// claim here, not what they write, which their own suites cover.
TEST(OstreamEndlChar, EndsAndFlushDispatchThroughIoTraitsToo)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{std::string("")});

        os << endl;
        EXPECT_EQ(os.device().str().size(), 1u);
        EXPECT_TRUE(os.good());

        os << ends;
        EXPECT_EQ(os.device().str().size(), 2u);
        EXPECT_TRUE(os.good());

        os << flush;
        EXPECT_TRUE(os.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// Insertion-side companion to the istream function-manipulator case. A function pointer is the
// only manipulator shape that bypasses io_traits, and its parameter is a non-deduced context,
// so it has to beat the generic operator<< -- whose parameter is const TValue& -- on partial
// ordering. The invocation counter proves the manipulator overload ran.
TEST(OstreamEndlChar, AFunctionPointerManipulatorBeatsTheGenericInserter)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto       os = T(mem_device{std::string("")});
        static int calls;
        calls = 0;
        void (*manip)(ios_base<char>&) = [](ios_base<char>&) { ++calls; };

        os << manip;
        EXPECT_EQ(calls, 1);

        os << manip << manip;        // operator<< returns the stream, so manipulators chain
        EXPECT_EQ(calls, 3);

        // a capture-less lambda reaches the same overload once decayed with unary +
        os << +[](ios_base<char>&) { ++calls; };
        EXPECT_EQ(calls, 4);
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// A null function-pointer manipulator passed to operator<< must be rejected: the operator
// throws stream_error, its own handler categorizes it into strfailbit, and -- with no
// exception mask set -- returns the stream without throwing. The tag-object manipulators
// need no such branch: an object cannot be null.
TEST(OstreamEndlChar, ANullFunctionPointerManipulatorIsRejected)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{std::string("")});

        // overload: operator<<(T&, void(*)(ios_base<char>&))
        os << static_cast<void (*)(ios_base<char>&)>(nullptr);
        EXPECT_TRUE(os.rdstate() & ios_defs::strfailbit);
        os.clear();

        // same overload, non-null: the callable runs against the stream's ios_base
        static int base_calls;
        base_calls = 0;
        os << +[](ios_base<char>&) { ++base_calls; };
        EXPECT_EQ(base_calls, 1);
        EXPECT_FALSE(os.rdstate() & ios_defs::strfailbit);

        // stream is still usable after the rejected manipulator
        os << "ok";
        auto [dev, err] = os.detach();
        EXPECT_EQ(dev.str(), "ok");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// endl needs a ctype facet to widen '\n'; without one it fails. It used to set
// unitbuf before that point and restore it only on the normal path, so the failure
// left unitbuf set on the stream for good -- every later write then flushed to the
// device, silently and permanently. endl no longer touches the flags at all.
TEST(OstreamEndlChar, EndlWithoutACtypeFacetFailsWithoutDisturbingTheFlags)
{
    auto helper = []<template <typename, typename> class T>()
    {
        locale<char> loc("C");
        T            os(mem_device{""}, loc.remove<ctype_conf<char>>());

        const ios_defs::fmtflags before = os.flags();
        ASSERT_EQ(before & ios_defs::unitbuf, 0);

        os << endl;

        EXPECT_TRUE(os.str_fail());        // reported as a stream failure
        EXPECT_EQ(os.flags(), before);     // and nothing else was disturbed
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// The same failure reported by throwing takes the same care with the flags.
TEST(OstreamEndlChar, TheThrownFormOfThatFailureAlsoLeavesTheFlagsAlone)
{
    auto helper = []<template <typename, typename> class T>()
    {
        locale<char> loc("C");
        T            os(mem_device{""}, loc.remove<ctype_conf<char>>());
        os.exceptions(ios_defs::strfailbit);

        const ios_defs::fmtflags before = os.flags();

        EXPECT_THROW(os << endl, stream_error);
        EXPECT_EQ(os.flags(), before);
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}
