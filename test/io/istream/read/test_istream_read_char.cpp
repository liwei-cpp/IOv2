// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * istream<char>::read(s, n): taking exactly n characters, or saying why not.
 *
 * read is the unformatted extraction with no delimiter and no terminator: it
 * asks for a count and either gets it or reaches the end of the input trying.
 * Those two outcomes are what the caller has to be able to tell apart, and
 * IOv2 makes the distinction by return value rather than by a separate query --
 * read returns where writing stopped, so the caller measures what it got by
 * subtraction instead of asking gcount(). A short read is also a failure:
 * eofbit says the input ran out, strfailbit says the request was not met.
 *
 * The remaining cases are about what read refuses. A null destination and a
 * count that is not positive are rejected before anything is extracted and
 * before anything is written, which matters because the count is signed
 * precisely so that a negative one cannot arrive as a huge unsigned length.
 *
 * The fixture is "0123456789abcdef", whose character at index n is n in base
 * 16, so how far a read got and what it wrote check each other.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/device/std_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <support/stdio_guard.h>

#include <chrono>
#include <cstddef>
#include <limits>
#include <string>
#include <thread>
#include <unistd.h>

using namespace IOv2;

namespace
{
    const std::string kDigits = "0123456789abcdef";
}

TEST(IstreamReadChar, ReadTakesExactlyTheCountAsked)
{
    auto expect_exact = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        char        buf[8] = {};
        char* end    = is.read(buf, 4);

        EXPECT_EQ(end - buf, 4);
        EXPECT_EQ(std::string(buf, end), "0123");

        // Getting what was asked for is not an event: the state is untouched
        // and the stream is left on the character after the last one taken.
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
        EXPECT_EQ(is.peek(), '4');
    };

    expect_exact.operator()<istream>();
    expect_exact.operator()<iostream>();
}

// Successive reads continue where the previous one stopped, so a caller can
// walk the input in pieces of whatever size suits it.
TEST(IstreamReadChar, SuccessiveReadsWalkTheInput)
{
    auto expect_walked = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        char buf[16] = {};
        EXPECT_EQ(std::string(buf, is.read(buf, 4)), "0123");
        EXPECT_EQ(std::string(buf, is.read(buf, 6)), "456789");
        EXPECT_EQ(std::string(buf, is.read(buf, 6)), "abcdef");
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);

        // The last read ended on the last character without going past it, so
        // the end of the input has not been seen yet.
        EXPECT_FALSE(is.eof());
        EXPECT_FALSE(is.peek().has_value());
        EXPECT_TRUE(is.eof());
    };

    expect_walked.operator()<istream>();
    expect_walked.operator()<iostream>();
}

// A count of zero asks for nothing, which is always satisfiable: no state
// changes and the read position does not move.
TEST(IstreamReadChar, ACountOfZeroIsSatisfiedWithoutReading)
{
    auto expect_nothing = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        char        buf[8];
        for (char& c : buf) c = '#';
        char* end = is.read(buf, 0);

        EXPECT_EQ(end, buf);
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
        EXPECT_EQ(is.peek(), '0');
        for (const char c : buf)
            EXPECT_EQ(c, '#');

        // Also at the end of the input, where there is nothing to read but
        // still nothing being asked for.
        is.ignore(kDigits.size());
        EXPECT_EQ(is.read(buf, 0), buf);
        EXPECT_FALSE(is.eof());
    };

    expect_nothing.operator()<istream>();
    expect_nothing.operator()<iostream>();
}

// Running out with the count unmet writes what there was and reports both
// halves of the story: eofbit for why it stopped, strfailbit for the request
// not having been met.
TEST(IstreamReadChar, AShortReadKeepsWhatItGotAndReportsTheShortfall)
{
    auto expect_short = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        char        buf[32] = {};
        char* end     = is.read(buf, 32);

        EXPECT_EQ(static_cast<std::size_t>(end - buf), kDigits.size());
        EXPECT_EQ(std::string(buf, end), kDigits);
        EXPECT_TRUE(is.rdstate() & ios_defs::eofbit);
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);

        // Reading again from a stream that already failed extracts nothing and
        // leaves the state where it was.
        const ios_defs::iostate after = is.rdstate();
        EXPECT_EQ(is.read(buf, 4), buf);
        EXPECT_EQ(is.rdstate(), after);
    };

    expect_short.operator()<istream>();
    expect_short.operator()<iostream>();
}

// A stream with nothing in it is the shortfall taken to its limit: zero
// characters written, both bits set.
TEST(IstreamReadChar, ReadingFromAnEmptyStreamFails)
{
    auto expect_failed = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::string("")}};

        char        buf[8];
        for (char& c : buf) c = '#';
        char* end = is.read(buf, 4);

        EXPECT_EQ(end, buf);
        EXPECT_TRUE(is.rdstate() & ios_defs::eofbit);
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
        for (const char c : buf)
            EXPECT_EQ(c, '#');
    };

    expect_failed.operator()<istream>();
    expect_failed.operator()<iostream>();
}

// With eofbit masked the shortfall is thrown rather than returned. The throw
// leaves read()'s sentry by unwinding, so the sentry's destructor must not
// throw on the way out -- reaching the assertions at all is what proves it
// did not, since a throwing destructor during unwinding calls std::terminate.
TEST(IstreamReadChar, AShortReadThrowsWhenEndOfFileIsMasked)
{
    auto expect_thrown = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::string("ab")}, locale<char>("C")};
            is.exceptions(ios_defs::eofbit);

            char buf[8] = {};
            EXPECT_ANY_THROW(is.read(buf, 5));
            EXPECT_TRUE(is.rdstate() & ios_defs::eofbit);
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
        }
        {
            T is{mem_device{std::string("ab")}, locale<char>("C")};

            char buf[8] = {};
            EXPECT_NO_THROW(is.read(buf, 5));
            EXPECT_TRUE(is.rdstate() & ios_defs::eofbit);
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
        }
    };

    expect_thrown.operator()<istream>();
    expect_thrown.operator()<iostream>();
}

TEST(IstreamReadChar, ANullDestinationIsRejected)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        // The sentry succeeds -- there is input to be had -- and read then
        // refuses the destination rather than writing through it.
        T is{mem_device{std::string("abc")}, locale<char>("C")};

        char* ret = nullptr;
        EXPECT_NO_THROW(ret = is.read(nullptr, 5));
        EXPECT_EQ(ret, nullptr);
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);

        // Nothing was taken on the way to the refusal.
        is.clear();
        EXPECT_EQ(is.peek(), 'a');
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

// The count is a signed ptrdiff_t so that a negative value is rejected here
// rather than arriving as a length near SIZE_MAX and overrunning the caller's
// buffer. The buffer being untouched afterwards is the point of the test.
TEST(IstreamReadChar, ACountThatIsNegativeIsRejected)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        for (const std::ptrdiff_t n : {std::ptrdiff_t{-1},
                                       std::numeric_limits<std::ptrdiff_t>::min()})
        {
            SCOPED_TRACE(n);
            T is{mem_device{std::string(4096, 'x')}, locale<char>("C")};

            char buf[8];
            for (char& c : buf) c = '#';

            char* ret = nullptr;
            EXPECT_NO_THROW(ret = is.read(buf, n));
            EXPECT_EQ(ret, buf);
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
            for (const char c : buf)
                EXPECT_EQ(c, '#');

            // The rejection happens before anything is extracted.
            is.clear();
            EXPECT_EQ(is.peek(), 'x');
        }

        // Masking strfailbit turns the same refusal into an exception, and the
        // buffer is still left alone.
        {
            T is{mem_device{std::string("abc")}, locale<char>("C")};
            is.exceptions(ios_defs::strfailbit);

            char buf[8];
            for (char& c : buf) c = '#';

            EXPECT_ANY_THROW(is.read(buf, -1));
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
            for (const char c : buf)
                EXPECT_EQ(c, '#');
        }
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

// read asks for a count, so once it has that count it is done. On a device
// that can block -- a pipe whose writer has not finished -- asking for one
// character more than was requested means waiting for a writer nobody asked
// about, so a read that is satisfied must not go back to the device at all.
//
// The write end is closed after a delay rather than left open, so a regression
// waits for that instead of hanging and the elapsed-time check fails cleanly.
TEST(IstreamReadChar, ASatisfiedReadDoesNotGoBackToTheDevice)
{
    pipe_iguard guard("42\n");
    std::thread closer([&guard] {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        guard.close_write();
    });

    istream is{std_device<STDIN_FILENO>{}};

    char       buf[3] = {};
    const auto start   = std::chrono::steady_clock::now();
    is.read(buf, 3);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    closer.join();

    EXPECT_EQ(std::string(buf, 3), "42\n");
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 1000);
}
