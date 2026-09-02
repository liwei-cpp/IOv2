// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * istream<char>::get(): unformatted reads, one character or a bufferful.
 *
 * The buffered overload is spelled with two policies rather than with the
 * standard's two separate functions. DelimPolicy says what happens to the
 * delimiter -- keep_sep leaves it in the stream, cons_sep takes it out -- and
 * CStrPolicy says whether the result is a C string, which costs one of the
 * capacity for the terminator. The return value is where writing stopped, so
 * the caller measures what it got by subtraction instead of asking gcount().
 *
 * Three ways to stop: the capacity ran out, the delimiter arrived, or the input
 * did. They are distinguishable, and the cases below separate them. A get that
 * extracted nothing at all is a failure whichever of the three it was, which is
 * the one rule that catches a caller who never checks.
 *
 * The fixture is "0123456789abcdef", whose character at index n is n in base
 * 16, so how far a read got and what it wrote check each other.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/file_device.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/io/utilities/istream_operators.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <support/file_guard.h>

#include <cstddef>
#include <limits>
#include <string>

using namespace IOv2;

namespace
{
    const std::string kDigits = "0123456789abcdef";
}

TEST(IstreamGetChar, ASingleGetYieldsTheNextCharacterAndAdvances)
{
    auto expect_one_at_a_time = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        EXPECT_EQ(is.get(), '0');
        EXPECT_EQ(is.get(), '1');

        char c = '#';
        is.get(c);
        EXPECT_EQ(c, '2');
        EXPECT_EQ(is.peek(), '3');
    };

    expect_one_at_a_time.operator()<istream>();
    expect_one_at_a_time.operator()<iostream>();
}

TEST(IstreamGetChar, AGetIntoABufferStopsAtTheCapacity)
{
    auto expect_capped = []<template <typename, typename> class T>()
    {
        T    is(mem_device{kDigits});
        char buf[8] = {};

        char* end = is.template get<keep_sep, no_zt>(buf, 3);
        EXPECT_EQ(end - buf, 3);
        EXPECT_EQ(std::string(buf, end), "012");

        // The stream is left exactly where writing stopped.
        EXPECT_EQ(is.peek(), '3');
    };

    expect_capped.operator()<istream>();
    expect_capped.operator()<iostream>();
}

// app_zt reserves one of the capacity for the terminator, so the same n yields
// one character fewer than no_zt does.
TEST(IstreamGetChar, AppendingATerminatorCostsOneOfTheCapacity)
{
    auto expect_reserved = []<template <typename, typename> class T>()
    {
        T    is(mem_device{kDigits});
        char buf[8];
        for (char& c : buf) c = '#';

        char* end = is.template get<keep_sep, app_zt>(buf, 3);
        EXPECT_EQ(std::string(buf), "01");   // reads as a C string
        EXPECT_EQ(buf[2], '\0');
        EXPECT_EQ(buf[3], '#');              // and no further

        // The returned pointer is past everything written, the terminator
        // included, so it is one more than the character count.
        EXPECT_EQ(end - buf, 3);
        EXPECT_EQ(is.peek(), '2');
    };

    expect_reserved.operator()<istream>();
    expect_reserved.operator()<iostream>();
}

TEST(IstreamGetChar, KeepingTheDelimiterLeavesItInTheStream)
{
    auto expect_kept = []<template <typename, typename> class T>()
    {
        T    is(mem_device{std::string("012\n345")});
        char buf[8] = {};

        char* end = is.template get<keep_sep, no_zt>(buf, 7, '\n');
        EXPECT_EQ(std::string(buf, end), "012");
        EXPECT_EQ(is.peek(), '\n');          // still there for the next reader
    };

    expect_kept.operator()<istream>();
    expect_kept.operator()<iostream>();
}

TEST(IstreamGetChar, ConsumingTheDelimiterTakesItOutOfTheStream)
{
    auto expect_consumed = []<template <typename, typename> class T>()
    {
        T    is(mem_device{std::string("012\n345")});
        char buf[8] = {};

        char* end = is.template get<cons_sep, no_zt>(buf, 7, '\n');
        EXPECT_EQ(std::string(buf, end), "012");   // the delimiter is not written
        EXPECT_EQ(is.peek(), '3');                 // but it is gone
    };

    expect_consumed.operator()<istream>();
    expect_consumed.operator()<iostream>();
}

// cons_sep promises to consume a delimiter, so a read that filled the buffer
// without reaching one has not done what was asked and says so.
TEST(IstreamGetChar, ConsumingRequiresTheDelimiterWithinTheCapacity)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        T    is(mem_device{kDigits});
        char buf[8] = {};

        is.template get<cons_sep, no_zt>(buf, 3, '\n');
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

// Nothing extracted is a failure however it came about: no input at all, or a
// delimiter sitting where the first character would have been.
TEST(IstreamGetChar, ExtractingNothingIsAFailure)
{
    auto expect_failed = []<template <typename, typename> class T>()
    {
        {
            T    empty(mem_device{std::string("")});
            char buf[8] = {};
            char* end = empty.template get<keep_sep, no_zt>(buf, 4);
            EXPECT_EQ(end, buf);
            EXPECT_TRUE(empty.rdstate() & ios_defs::strfailbit);
        }
        {
            T    at_delim(mem_device{std::string("\n012")});
            char buf[8] = {};
            char* end = at_delim.template get<keep_sep, no_zt>(buf, 4, '\n');
            EXPECT_EQ(end, buf);
            EXPECT_TRUE(at_delim.rdstate() & ios_defs::strfailbit);
        }
    };

    expect_failed.operator()<istream>();
    expect_failed.operator()<iostream>();
}

// Stopping because the input ran out, with room still left, is the end of file --
// as distinct from stopping because the capacity was reached, which is not.
TEST(IstreamGetChar, RunningOutOfInputSetsEndOfFileAndFillingTheBufferDoesNot)
{
    auto expect_distinguished = []<template <typename, typename> class T>()
    {
        {
            T    is(mem_device{std::string("012")});
            char buf[8] = {};
            char* end = is.template get<keep_sep, no_zt>(buf, 7);
            EXPECT_EQ(std::string(buf, end), "012");
            EXPECT_TRUE(is.eof());
        }
        {
            T    is(mem_device{kDigits});
            char buf[8] = {};
            is.template get<keep_sep, no_zt>(buf, 4);
            EXPECT_FALSE(is.eof());
        }
    };

    expect_distinguished.operator()<istream>();
    expect_distinguished.operator()<iostream>();
}

// The capacity is a signed ptrdiff_t so that a negative value is rejected here
// rather than arriving as SIZE_MAX and filling the caller's buffer until the
// delimiter or the end of the input.
TEST(IstreamGetChar, ACapacityThatIsNotPositiveIsRejected)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        for (const std::ptrdiff_t n : {std::ptrdiff_t{-1},
                                       std::numeric_limits<std::ptrdiff_t>::min(),
                                       std::ptrdiff_t{0}})
        {
            SCOPED_TRACE(n);
            T    is{mem_device{std::string(4096, 'x')}, locale<char>("C")};
            char buf[8];
            for (char& c : buf) c = '#';

            char* ret = nullptr;
            EXPECT_NO_THROW((ret = is.template get<keep_sep, app_zt>(buf, n, '\n')));
            EXPECT_EQ(ret, buf);
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);

            // app_zt must not read a non-positive capacity as room for the
            // terminator either.
            for (const char c : buf)
                EXPECT_EQ(c, '#');

            // And nothing was taken from the stream.
            is.clear();
            EXPECT_EQ(is.peek(), 'x');
        }

        // The two-argument overload widens '\n' and forwards, so it has to reject
        // the same capacities on the way through.
        {
            T    is{mem_device{std::string(4096, 'x')}, locale<char>("C")};
            char buf[8];
            for (char& c : buf) c = '#';

            char* ret = nullptr;
            EXPECT_NO_THROW((ret = is.template get<cons_sep, app_zt>(buf, -1)));
            EXPECT_EQ(ret, buf);
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
            for (const char c : buf)
                EXPECT_EQ(c, '#');
        }
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

TEST(IstreamGetChar, TheEndOfInputThrowsWhenEndOfFileIsMasked)
{
    auto expect_thrown = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::string("")}, locale<char>("C")};
            is.exceptions(ios_defs::eofbit);
            EXPECT_THROW((void)is.get(), eof_error);
            EXPECT_TRUE(is.eof());
        }
        {
            T is{mem_device{std::string("")}, locale<char>("C")};
            is.exceptions(ios_defs::eofbit);
            char c = 'Z';
            EXPECT_THROW(is.get(c), eof_error);
            EXPECT_TRUE(is.eof());
        }
        // Unmasked, the same end is reported rather than thrown, and the
        // caller's variable is left as it was.
        {
            T is{mem_device{std::string("")}, locale<char>("C")};
            EXPECT_FALSE(is.get().has_value());
            EXPECT_TRUE(is.eof());
        }
        {
            T is{mem_device{std::string("")}, locale<char>("C")};
            char c = 'Z';
            is.get(c);
            EXPECT_EQ(c, 'Z');
            EXPECT_TRUE(is.eof());
        }
    };

    expect_thrown.operator()<istream>();
    expect_thrown.operator()<iostream>();
}

// The delimiter-terminated form over a real file, repeated until the input runs
// out: each line comes back whole and in order, which is what a caller reading a
// file a line at a time depends on.
TEST(IstreamGetChar, RepeatedGetsWalkAFileLineByLine)
{
    constexpr int kLines = 200;

    std::string data;
    for (int i = 0; i < kLines; ++i)
        data += "line-" + std::to_string(i) + "\n";

    const std::string path = "test_istream_get_lines.txt";
    file_guard        guard(path, data);

    auto expect_walked = [&]<template <typename, typename> class T, typename TDevice>()
    {
        T is(TDevice{path});
        ASSERT_TRUE(static_cast<bool>(is));

        int read = 0;
        while (is)
        {
            char  line[64] = {};
            char* end = is.template get<cons_sep, app_zt>(line, sizeof line, '\n');
            if (!is) break;
            EXPECT_EQ(std::string(line), "line-" + std::to_string(read));
            EXPECT_GT(end, line);
            ++read;
        }
        EXPECT_EQ(read, kLines);
    };

    expect_walked.operator()<istream, ifile_device<char>>();
    expect_walked.operator()<iostream, file_device<char>>();
}
