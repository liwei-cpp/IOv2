// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * istream<char>::ignore(n, delim): discarding input without looking at it.
 *
 * ignore has three ways to stop and the caller usually cares which: it has
 * discarded n characters, it has found and discarded the delimiter, or the
 * input ran out. The last one sets eofbit but is not a failure, because
 * discarding fewer characters than offered is only a problem if the caller
 * needed them -- and a caller that needed them would not be discarding them.
 * That is the whole difference between ignore and read.
 *
 * The delimiter is a character, not a widened integer, so no character value is
 * reserved to mean "no delimiter" and a byte like 0xFF cannot be mistaken for
 * the end of the input. The unbounded form is spelled by passing the maximum
 * count, which is the one count that does not stop the search.
 *
 * The fixture is "0123456789abcdef", whose character at index n is n in base
 * 16, so how far an ignore got is readable from the character it stopped on.
 */
#include <device/file_device.h>
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <support/file_guard.h>

#include <limits>
#include <string>

using namespace IOv2;

namespace
{
    const std::string kDigits = "0123456789abcdef";

    constexpr auto kUnbounded = std::numeric_limits<std::streamsize>::max();
}

TEST(IstreamIgnoreChar, IgnoreDiscardsOneCharacterByDefaultAndNCharactersOnRequest)
{
    auto expect_counted = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        const ios_defs::iostate before = is.rdstate();
        is.ignore();
        EXPECT_EQ(is.rdstate(), before);   // discarding is not an event
        EXPECT_EQ(is.peek(), '1');

        is.ignore(4);
        EXPECT_EQ(is.peek(), '5');

        is.ignore(10);
        EXPECT_EQ(is.peek(), 'f');
    };

    expect_counted.operator()<istream>();
    expect_counted.operator()<iostream>();
}

// A count of zero asks for nothing, with or without a delimiter, and so cannot
// move the read position even when the delimiter is right there.
TEST(IstreamIgnoreChar, ACountOfZeroDiscardsNothing)
{
    auto expect_nothing = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        is.ignore(0);
        EXPECT_EQ(is.peek(), '0');

        is.ignore(0, '0');             // the delimiter is the next character
        EXPECT_EQ(is.peek(), '0');
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
    };

    expect_nothing.operator()<istream>();
    expect_nothing.operator()<iostream>();
}

// The delimiter is discarded along with everything before it: ignore stops
// after it, not on it, which is what makes repeated calls walk record by
// record without the caller having to step over the separator.
TEST(IstreamIgnoreChar, TheDelimiterIsDiscardedTooAndIgnoreStopsAfterIt)
{
    auto expect_consumed = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        is.ignore(kUnbounded, '4');
        EXPECT_EQ(is.peek(), '5');
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);

        // Finding it immediately still discards it and nothing else.
        is.ignore(kUnbounded, '5');
        EXPECT_EQ(is.peek(), '6');
    };

    expect_consumed.operator()<istream>();
    expect_consumed.operator()<iostream>();
}

// With both a count and a delimiter, whichever comes first wins. The count
// includes the delimiter, so a delimiter at position n is out of reach of a
// call bounded by n.
TEST(IstreamIgnoreChar, TheCountAndTheDelimiterAreBothLimitsAndTheNearerOneStops)
{
    auto expect_first = []<template <typename, typename> class T>()
    {
        {
            // The delimiter is within the count: it stops the call.
            T is(mem_device{kDigits});
            is.ignore(6, '3');
            EXPECT_EQ(is.peek(), '4');
        }
        {
            // The delimiter is past the count: the count stops the call and the
            // delimiter is still in the stream.
            T is(mem_device{kDigits});
            is.ignore(3, '8');
            EXPECT_EQ(is.peek(), '3');
            EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
        }
        {
            // Exactly at the boundary: the count covers the delimiter itself.
            T is(mem_device{kDigits});
            is.ignore(4, '3');
            EXPECT_EQ(is.peek(), '4');
        }
    };

    expect_first.operator()<istream>();
    expect_first.operator()<iostream>();
}

// Running out of input while discarding is the end, not a failure: eofbit is
// set, strfailbit is not, and the stream still converts to true.
TEST(IstreamIgnoreChar, RunningOutOfInputSetsEndOfFileWithoutFailing)
{
    auto expect_end = []<template <typename, typename> class T>()
    {
        {
            // Bounded by a count larger than what is there.
            T is(mem_device{std::string("ab")});
            is.ignore(10);
            EXPECT_TRUE(is.eof());
            EXPECT_FALSE(is.rdstate() & ios_defs::strfailbit);
            EXPECT_TRUE(static_cast<bool>(is));
        }
        {
            // Bounded by a delimiter that is not there.
            T is(mem_device{std::string("ab")});
            is.ignore(kUnbounded, '|');
            EXPECT_TRUE(is.eof());
            EXPECT_FALSE(is.rdstate() & ios_defs::strfailbit);
        }
        {
            // Nothing there to begin with.
            T is{mem_device{std::string("")}};
            is.ignore();
            EXPECT_TRUE(is.eof());
            EXPECT_FALSE(is.rdstate() & ios_defs::strfailbit);
        }
    };

    expect_end.operator()<istream>();
    expect_end.operator()<iostream>();
}

// A character value is a character value: the delimiter parameter has the
// stream's character type, so nothing is set aside to mean "no more input" and
// a byte that would be the traditional end-of-file sentinel is discarded like
// any other.
TEST(IstreamIgnoreChar, NoCharacterValueIsReservedToMeanEndOfInput)
{
    auto expect_transparent = []<template <typename, typename> class T>()
    {
        {
            // 0xFF sits in the middle and is passed over without stopping.
            T is(mem_device{std::string("ab\xFF" "cd")});
            is.ignore(4);
            EXPECT_EQ(is.peek(), 'd');
            EXPECT_FALSE(is.eof());
        }
        {
            // And it works as a delimiter in its own right.
            T is(mem_device{std::string("ab\xFF" "cd")});
            is.ignore(kUnbounded, '\xFF');
            EXPECT_EQ(is.peek(), 'c');
            EXPECT_FALSE(is.eof());
        }
    };

    expect_transparent.operator()<istream>();
    expect_transparent.operator()<iostream>();
}

// Repeated unbounded calls are how a caller skips records, so they have to
// terminate: each one either lands after a delimiter or at the end. Over a file
// large enough to span several device reads, the delimiters found must come to
// exactly the number written, and the loop must stop at the end rather than
// spinning on a stream that is already exhausted.
TEST(IstreamIgnoreChar, RepeatedUnboundedIgnoresWalkTheRecordsAndThenStop)
{
    constexpr int  kRecords = 400;
    constexpr char kDelim   = '|';

    std::string data;
    for (int i = 0; i < kRecords; ++i)
        data += std::string(20 + i % 40, 'x') + kDelim;

    const std::string path = "test_istream_ignore_records.txt";
    file_guard        guard(path, data);

    auto expect_walked = [&]<template <typename, typename> class T, typename TDevice>()
    {
        T is(TDevice{path});
        ASSERT_TRUE(static_cast<bool>(is));

        int found = 0;
        while (is.good())
        {
            is.ignore(kUnbounded, kDelim);
            if (is.good()) ++found;
        }

        EXPECT_EQ(found, kRecords);
        EXPECT_TRUE(is.eof());
        EXPECT_FALSE(is.str_fail());
    };

    expect_walked.operator()<istream, ifile_device<char>>();
    expect_walked.operator()<iostream, file_device<char>>();
}

// Counts far larger than one device read still stop where they were told to,
// which is the case a buffered implementation gets wrong by refilling once too
// often or once too few.
TEST(IstreamIgnoreChar, ACountLargerThanTheBufferStopsWhereItWasTold)
{
    const std::string line = "0123456789\n";
    std::string       data;
    for (int i = 0; i < 1500; ++i)
        data += line;

    const std::string path = "test_istream_ignore_bulk.txt";
    file_guard        guard(path, data);

    auto expect_stopped = [&]<template <typename, typename> class T, typename TDevice>()
    {
        T is(TDevice{path});
        ASSERT_TRUE(static_cast<bool>(is));

        // Each count is a whole number of lines, so what is left under the
        // cursor names how far the call went.
        for (const std::streamsize n : {std::streamsize{11}, std::streamsize{110},
                                        std::streamsize{1100}, std::streamsize{11000}})
        {
            SCOPED_TRACE(n);
            is.ignore(n);
            EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
            EXPECT_EQ(is.peek(), '0');
        }

        // And the unbounded form runs to the end of a file of this size.
        is.ignore(kUnbounded);
        EXPECT_EQ(is.rdstate(), ios_defs::eofbit);
    };

    expect_stopped.operator()<istream, ifile_device<char>>();
    expect_stopped.operator()<iostream, file_device<char>>();
}

TEST(IstreamIgnoreChar, IgnoreAtTheEndThrowsWhenEndOfFileIsMasked)
{
    auto expect_thrown = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::string("")}, locale<char>("C")};
            is.exceptions(ios_defs::eofbit);
            EXPECT_THROW(is.ignore(), eof_error);
            EXPECT_TRUE(is.eof());
        }
        {
            T is{mem_device{std::string("")}, locale<char>("C")};
            EXPECT_NO_THROW(is.ignore());
            EXPECT_TRUE(is.eof());
        }
    };

    expect_thrown.operator()<istream>();
    expect_thrown.operator()<iostream>();
}

// ignore on a stream already in a failed state: the input sentry rejects the invalid stream
// (throws stream_error), which the function's own try/catch routes through handle_exception
// (-> strfailbit). With no exception mask set nothing escapes.
TEST(IstreamIgnoreChar, IgnoreOnAFailedStreamIsReportedRatherThanThrown)
{
    auto expect_reported = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::string("abc")}, locale<char>("C")};

        int v = 0;
        is >> v;                      // non-numeric input -> strfailbit
        EXPECT_FALSE(static_cast<bool>(is));

        // Both the delimited and the plain overload have to take that branch.
        EXPECT_NO_THROW(is.ignore(5, 'x'));
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);

        EXPECT_NO_THROW(is.ignore(5));
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
    };

    expect_reported.operator()<istream>();
    expect_reported.operator()<iostream>();
}
