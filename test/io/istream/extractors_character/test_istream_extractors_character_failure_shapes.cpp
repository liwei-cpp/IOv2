// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * What the character extractors leave behind when they do not succeed.
 *
 * Three separate contracts meet here, and each one used to be documented more
 * broadly than it held:
 *
 * The array form promises a terminator on every exit. That promise is about
 * the inside of the extraction helper, not about `is >> buf`: when the sentry
 * reports failure the formatted function returns without attempting any input
 * at all, so the array is never touched. The boundary therefore runs between
 * "the sentry failed" and "the sentry succeeded but no character was
 * extracted" -- only the second writes a terminator. libstdc++ draws it in the
 * same place, so these cases pin conformance rather than a local choice.
 *
 * The basic_string form clears its target up front and fills it through a
 * 128-character staging buffer. A throw part way through therefore used to
 * drop up to 127 characters that had already been consumed off the input,
 * leaving neither an empty string nor the previous value. The extraction loop
 * now appends the staging buffer on the exception path too, so what survives
 * is the complete prefix of what was consumed.
 *
 * That rescuing append can itself throw, and when it did the new exception
 * replaced the one already in flight -- a device failure reached the stream as
 * otherfailbit instead of devfailbit. It is now caught and dropped, so the
 * original exception is what propagates and the staged characters are what is
 * given up. Losing them is the documented outcome; losing the exception was
 * not.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>

#include <support/throwing_get_device.h>

#include <gtest/gtest.h>

namespace
{
using is_c = IOv2::istream<IOv2::mem_device<char>, char>;
using is_throwing = IOv2::istream<throwing_get_device<char>, char>;

// std::allocator's max_size() is what makes a failing append cost gigabytes to
// reach. Capping it moves length_error within reach of an ordinary test while
// leaving allocation itself untouched.
template <class T, std::size_t Max>
struct capped_alloc
{
    using value_type = T;

    capped_alloc() = default;
    template <class U>
    explicit capped_alloc(const capped_alloc<U, Max>&) noexcept
    {}

    T* allocate(std::size_t n) { return std::allocator<T>{}.allocate(n); }
    void deallocate(T* p, std::size_t n) { std::allocator<T>{}.deallocate(p, n); }
    std::size_t max_size() const noexcept { return Max; }
    bool operator==(const capped_alloc&) const noexcept { return true; }
};

template <std::size_t Max>
using capped_string = std::basic_string<char, std::char_traits<char>, capped_alloc<char, Max>>;

// A byte the extractors never write, so "untouched" is distinguishable from
// "written with something that happens to look empty".
constexpr char k_unwritten = '\x5a';

// Position-bearing input: the character at index i is 'a' + i % 26, so a
// truncated result is checked for content and not merely for length.
std::string patterned(std::size_t n)
{
    std::string s;
    s.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        s.push_back(static_cast<char>('a' + i % 26));
    return s;
}

// Consumes exactly `consumed` characters and then takes a device_error, checks
// that the device_error is what reached the stream, and reports how much of the
// token survived. setw only keeps the loop from ending before the device does.
template <std::size_t Max>
std::size_t extract_into_capped(std::size_t consumed)
{
    SCOPED_TRACE(testing::Message() << "cap " << capped_string<Max>{}.max_size()
                                    << ", consumed " << consumed);

    // The sentry spends the first dget() looking for whitespace to skip, so a
    // throw on call n leaves n - 1 characters staged.
    const std::size_t room = consumed + 4;
    is_throwing is{throwing_get_device<char>{patterned(room), consumed + 1},
                   IOv2::locale<char>("C")};
    capped_string<Max> value;

    is >> IOv2::setw(static_cast<std::ptrdiff_t>(room)) >> value;

    EXPECT_TRUE(is.dev_fail());
    EXPECT_FALSE(is.other_fail());
    return value.size();
}
}

TEST(IstreamExtractCharacterFailureShapes, AFailedSentryLeavesTheArrayUntouched)
{
    // Empty input, all-whitespace input and an already-failed stream are the
    // three ways the sentry reports failure. None of them reaches the
    // extraction helper, so none of them writes the terminator.
    const char* const inputs[] = {"", "   ", "ignored"};

    for (int which = 0; which < 3; ++which)
    {
        char buf[8];
        std::memset(buf, k_unwritten, sizeof buf);

        is_c is{IOv2::mem_device<char>{std::string(inputs[which])}, IOv2::locale<char>("C")};
        if (which == 2)
            is.setstate(IOv2::ios_defs::strfailbit);

        is >> buf;

        EXPECT_TRUE(is.str_fail()) << "case " << which;
        for (std::size_t i = 0; i < sizeof buf; ++i)
            EXPECT_EQ(buf[i], k_unwritten) << "case " << which << " byte " << i;
    }
}

TEST(IstreamExtractCharacterFailureShapes, ASucceedingSentryThatExtractsNothingStillTerminates)
{
    // noskipws puts the leading space in front of the extraction rather than
    // in front of the sentry, so the sentry succeeds and the helper runs. It
    // extracts nothing, sets the failure bit -- and writes the terminator,
    // which is the half of the contract that does hold.
    char buf[8];
    std::memset(buf, k_unwritten, sizeof buf);

    is_c is{IOv2::mem_device<char>{std::string(" ab")}, IOv2::locale<char>("C")};
    is.unsetf(IOv2::ios_defs::skipws);

    is >> buf;

    EXPECT_TRUE(is.str_fail());
    EXPECT_EQ(buf[0], '\0');
}

TEST(IstreamExtractCharacterFailureShapes, AThrowMidTokenKeepsTheWholeConsumedPrefixInTheString)
{
    // The staging buffer holds 128 characters, so the interesting boundary is
    // where a flush has and has not happened: before the fix, boom = 130 kept
    // 128 characters -- truncated to the flush granularity -- instead of 129.
    //
    // The sentry spends the first dget() looking for whitespace to skip, so a
    // throw on call n leaves n - 1 characters in the string. boom = 1 is not
    // this case at all: the sentry itself throws, which the sentry test below
    // covers.
    for (std::size_t boom : {std::size_t{2}, std::size_t{127}, std::size_t{128},
                             std::size_t{129}, std::size_t{130}, std::size_t{255},
                             std::size_t{256}, std::size_t{300}})
    {
        const std::string source = patterned(512);

        // dget() hands out one character per call and throws on the boom-th,
        // so exactly boom - 1 characters reach the string.
        is_throwing is{throwing_get_device<char>{source, boom},
                       IOv2::locale<char>("C")};
        std::string value = "PREVIOUS";

        is >> value;

        const std::size_t consumed = boom - 1;
        ASSERT_EQ(value.size(), consumed) << "boom " << boom;
        EXPECT_EQ(value, source.substr(0, consumed)) << "boom " << boom;
    }
}

TEST(IstreamExtractCharacterFailureShapes, AFailingRescueAppendKeepsTheOriginalException)
{
    // Same shape as the case above, but with the string's max_size() cut down
    // so the rescuing append has to fail too.
    //
    // max_size() is read rather than computed: it is not the allocator's
    // max_size() but an implementation-defined function of it, and two
    // libstdc++ builds both reporting _GLIBCXX_RELEASE 15 disagree on which
    // one -- capped_alloc<char, 200> gives 199 on one and 99 on the other.
    // Deriving the boundary from the cap pins the case to the behaviour
    // rather than to the formula.
    constexpr std::size_t k_staging = 128;
    const std::size_t cap = capped_string<100>{}.max_size();

    // Keeping the whole token inside one staging buffer makes the rescuing
    // append the only append there is, so nothing else can hit the cap first.
    // Lower the template argument if some implementation reports more.
    ASSERT_GE(cap, std::size_t{2});
    ASSERT_LE(cap, k_staging - 2);

    // Rescue throws one past the cap and succeeds at it. Both sides are needed:
    // the first alone would also pass on a stream that had simply stopped
    // rescuing anything.
    EXPECT_EQ(extract_into_capped<100>(cap + 1), std::size_t{0});
    EXPECT_EQ(extract_into_capped<100>(cap), cap);
}

TEST(IstreamExtractCharacterFailureShapes, EveryExitOfTheStringFormSpendsTheWidth)
{
    // sread takes the width with one exchange before anything that can throw, so every exit it
    // reaches owes width() == 0. The device-throw exit is pinned beside the array form in
    // test_istream_extractors_character_char.cpp; these are the two that were left.
    {
        // Sentry succeeds on the leading space thanks to noskipws, then nothing is extracted.
        is_c is{IOv2::mem_device<char>{std::string(" ab")}, IOv2::locale<char>("C")};
        is.unsetf(IOv2::ios_defs::skipws);
        std::string value = "PREVIOUS";

        is >> IOv2::setw(5) >> value;

        EXPECT_TRUE(is.str_fail());
        EXPECT_EQ(is.width(), 0);
        EXPECT_EQ(value, "");
    }
    {
        // The facet lookup inside sread throws after the width has been taken. noskipws is what
        // makes it reachable at all -- see the next case.
        const auto loc = IOv2::locale<char>("C").remove<IOv2::ctype_conf<char>>();
        is_c       is{IOv2::mem_device<char>{std::string("abc")}, loc};
        is.unsetf(IOv2::ios_defs::skipws);
        std::string value = "PREVIOUS";

        is >> IOv2::setw(5) >> value;

        EXPECT_FALSE(is.good());
        EXPECT_EQ(is.width(), 0);
        EXPECT_EQ(value, "");
    }
}

TEST(IstreamExtractCharacterFailureShapes, AMissingCtypeStopsAtTheSentryWhileSkippingWhitespace)
{
    // The same missing facet lands on opposite sides of the boundary depending on skipws, because
    // skipping leading whitespace needs ctype too. With skipws the sentry asks first and fails, so
    // sread is never entered: the width is not spent and the target keeps its value. That is the
    // "the guarantee covers the inside of this function only" warning in istream_extract, seen
    // from the basic_string side.
    const auto loc = IOv2::locale<char>("C").remove<IOv2::ctype_conf<char>>();
    is_c       is{IOv2::mem_device<char>{std::string("abc")}, loc};
    std::string value = "PREVIOUS";

    is >> IOv2::setw(5) >> value;

    EXPECT_FALSE(is.good());
    EXPECT_EQ(is.width(), 5);
    EXPECT_EQ(value, "PREVIOUS");
}

TEST(IstreamExtractCharacterFailureShapes, AFailedSentryDoesNotSpendTheWidth)
{
    // The mirror image, and the one asymmetry in the contract: the width is taken inside sread,
    // which a failed sentry never reaches. Nothing on the extraction side holds a width_guard,
    // so the width survives for the next extraction -- as it does in libstdc++.
    is_c is{IOv2::mem_device<char>{std::string("")}, IOv2::locale<char>("C")};
    std::string value = "PREVIOUS";

    is >> IOv2::setw(5) >> value;

    EXPECT_TRUE(is.str_fail());
    EXPECT_EQ(is.width(), 5);
    EXPECT_EQ(value, "PREVIOUS");
}

TEST(IstreamExtractCharacterFailureShapes, AFailedSentryLeavesTheStringAtItsPreviousValue)
{
    // The clear happens inside the extraction helper, which a failed sentry
    // never reaches -- so here, unlike every other failure path, the previous
    // contents really do survive.
    std::string value = "PREVIOUS";

    is_c is{IOv2::mem_device<char>{std::string("")}, IOv2::locale<char>("C")};
    is >> value;

    EXPECT_TRUE(is.str_fail());
    EXPECT_EQ(value, "PREVIOUS");
}
