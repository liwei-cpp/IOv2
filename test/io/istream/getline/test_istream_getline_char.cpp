/**
 * Reading a line at a time out of an istream<char>, which IOv2 spells as
 * get<cons_sep, app_zt>: consume the delimiter, terminate the result.
 *
 * The policies themselves are covered in the get suite. What is left here, and
 * what a line-reading loop actually depends on, is telling the four ways such a
 * call can end apart *afterwards*, from the state alone:
 *
 *   the delimiter was found        goodbit
 *   the capacity ran out first     strfailbit
 *   the input ran out, got some    eofbit
 *   the input ran out, got none    eofbit | strfailbit
 *
 * A loop that treats the second as the third truncates a long line into two,
 * and one that treats the fourth as the third loops forever on an exhausted
 * stream -- so the four are pinned down as a sequence over one fixture, in the
 * order a real reader meets them.
 */
#include <device/mem_device.h>
#include <facet/ctype.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <string>

using namespace IOv2;

// One fixture arranged so that consecutive calls with a capacity of five --
// four characters and the terminator -- meet each ending in turn.
TEST(IstreamGetlineChar, TheStoppingConditionIsReadableFromTheState)
{
    auto expect_matrix = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::string("ab\ncdefgh\ni")});

        char buf[5];

        // 1. The delimiter arrived with room to spare: nothing is reported.
        char* end = is.template get<cons_sep, app_zt>(buf, 5, '\n');
        EXPECT_EQ(std::string(buf), "ab");
        EXPECT_EQ(end - buf, 3);                       // two characters and the terminator
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);

        // 2. The capacity ran out before the delimiter did. This is a failure
        //    even though four perfectly good characters came back, because the
        //    line asked for is not the line delivered.
        end = is.template get<cons_sep, app_zt>(buf, 5, '\n');
        EXPECT_EQ(std::string(buf), "cdef");
        EXPECT_EQ(end - buf, 5);
        EXPECT_EQ(is.rdstate(), ios_defs::strfailbit);
        EXPECT_FALSE(is.eof());                        // and not because of the input

        // The rest of that line is still there, which is how a caller recovers.
        is.clear();
        end = is.template get<cons_sep, app_zt>(buf, 5, '\n');
        EXPECT_EQ(std::string(buf), "gh");
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);

        // 3. The input ran out with something extracted: an end, not a failure.
        //    A last line without a trailing delimiter arrives this way.
        end = is.template get<cons_sep, app_zt>(buf, 5, '\n');
        EXPECT_EQ(std::string(buf), "i");
        EXPECT_EQ(end - buf, 2);
        EXPECT_EQ(is.rdstate(), ios_defs::eofbit);

        // 4. The input ran out with nothing extracted: both, which is the
        //    answer a loop must stop on.
        is.clear();
        end = is.template get<cons_sep, app_zt>(buf, 5, '\n');
        EXPECT_EQ(std::string(buf), "");
        EXPECT_EQ(end - buf, 1);                       // the terminator alone
        EXPECT_EQ(is.rdstate(), ios_defs::eofbit | ios_defs::strfailbit);
    };

    expect_matrix.operator()<istream>();
    expect_matrix.operator()<iostream>();
}

// A capacity of one is entirely spent on the terminator, so there is no room
// for a character and the call fails without consuming anything -- as distinct
// from a capacity of zero, which has no room for the terminator either.
TEST(IstreamGetlineChar, ACapacityOfOneLeavesRoomOnlyForTheTerminator)
{
    auto expect_terminator_only = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::string("abc")});

        char buf[4];
        for (char& c : buf) c = '*';

        char* end = is.template get<cons_sep, app_zt>(buf, 1, '\n');
        EXPECT_EQ(end - buf, 1);
        EXPECT_EQ(buf[0], '\0');
        EXPECT_EQ(buf[1], '*');                        // and no further
        EXPECT_TRUE(is.str_fail());

        // Nothing was taken, so the input is intact for the next reader.
        is.clear();
        EXPECT_EQ(is.peek(), 'a');
    };

    expect_terminator_only.operator()<istream>();
    expect_terminator_only.operator()<iostream>();
}

// The loop invariant a line reader relies on, checked over lines that grow past
// the buffer and back: on every call exactly one of the endings holds, and when
// the delimiter was found the result is shorter than the capacity allows --
// which is what makes "did it fit" answerable without a second query.
TEST(IstreamGetlineChar, EveryCallEndsInExactlyOneOfTheFourWays)
{
    constexpr std::ptrdiff_t kCapacity = 8;

    // Lines shorter than, exactly at, and well past the capacity, so that all
    // four endings occur and the capacity boundary is crossed in both directions.
    std::string data;
    for (const int n : {0, 1, 6, 7, 8, 20, 3})
        data += std::string(static_cast<std::size_t>(n), 'x') + "\n";

    auto expect_invariant = [&]<template <typename, typename> class T>()
    {
        T is(mem_device{data});

        std::string reassembled;
        int         guard = 0;
        while (is.good() && guard++ < 100)
        {
            char        buf[kCapacity];
            char*       end = is.template get<cons_sep, app_zt>(buf, kCapacity, '\n');
            const auto  len = static_cast<std::size_t>(end - buf) - 1;   // less the terminator

            EXPECT_EQ(std::strlen(buf), len);

            if (is.good())
            {
                // The delimiter was found, so what came back is a whole line
                // and the delimiter itself is gone.
                EXPECT_LE(static_cast<std::ptrdiff_t>(len), kCapacity - 1);
                reassembled += std::string(buf, len) + "\n";
            }
            else if (is.str_fail() && !is.eof())
            {
                // The capacity stopped it, so the buffer is exactly full.
                EXPECT_EQ(static_cast<std::ptrdiff_t>(len), kCapacity - 1);
                reassembled += std::string(buf, len);
                is.clear();
            }
        }

        // Ending on both bits is the only way out of the loop for input that
        // ends with a delimiter.
        EXPECT_EQ(is.rdstate(), ios_defs::eofbit | ios_defs::strfailbit);

        // And nothing was lost or invented along the way.
        EXPECT_EQ(reassembled, data);
    };

    expect_invariant.operator()<istream>();
    expect_invariant.operator()<iostream>();
}

// A null output pointer with a non-zero size is rejected up front with stream_error
// -> strfailbit. no_zt means no trailing terminator is written, so nothing touches
// the null pointer; the returned pointer is the (unmodified) null input.
TEST(IstreamGetlineChar, ANullDestinationIsRejected)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::string("hello")});

        char* ret = nullptr;
        EXPECT_NO_THROW((ret = is.template get<cons_sep, no_zt>(
                             static_cast<char*>(nullptr), 5, '\n')));
        EXPECT_EQ(ret, nullptr);
        EXPECT_TRUE(is.str_fail());
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

// The delimiter-less get(s, n) derives the '\n' delimiter via ctype::widen. With the
// ctype facet removed, that lookup throws stream_error ("no ctype facet") -> strfailbit.
// Because the app_zt (C-string) policy is in effect, the error path still writes the
// trailing terminator, so buf[0] == '\0' and the returned pointer is buf + 1.
TEST(IstreamGetlineChar, TheDefaultDelimiterNeedsTheCtypeFacet)
{
    auto expect_failed = []<template <typename, typename> class T>()
    {
        const auto loc = locale<char>("C").remove<ctype_conf<char>>();
        T is{mem_device{std::string("hello")}, loc};

        char buf[8];
        buf[0] = '*';

        char* ret = nullptr;
        EXPECT_NO_THROW((ret = is.template get<cons_sep, app_zt>(buf, 8)));
        EXPECT_EQ(ret, buf + 1);
        EXPECT_EQ(buf[0], '\0');
        EXPECT_TRUE(is.str_fail());
    };

    expect_failed.operator()<istream>();
    expect_failed.operator()<iostream>();
}
