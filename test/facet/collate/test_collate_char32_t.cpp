// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same collation contract as test_collate_char.cpp for char32_t, which
 * reaches wcscoll and wcsxfrm by reinterpreting its input as wchar_t.  That
 * only works where wchar_t is UTF-32, so what these cases pin down is that the
 * reinterpretation preserves the contract exactly: the same segment rules, the
 * same key algebra, and the same answers as every other character type.
 *
 * "C" keeps wcsxfrm an identity, so a key is its own input and the
 * expectations can be read off the literals; de_DE.UTF-8 is where collation
 * order and code-point order disagree.
 */
#include <IOv2/facet/collate.h>
#include <IOv2/facet/collate_details.h>

#include <gtest/gtest.h>

#include <compare>
#include <cstddef>
#include <deque>
#include <iterator>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

using namespace IOv2;

namespace
{
    // The two locales the cases above describe.  Every other string in this file
    // is test data in the character type under test.
    constexpr const char* kPlain  = "C";
    constexpr const char* kGerman = "de_DE.UTF-8";

    collate<char32_t> facet_for(const char* loc)
    {
        return collate<char32_t>(std::make_shared<collate_conf<char32_t>>(loc));
    }

    // -1/0/1 rather than the ordering itself: GoogleTest has no printer for
    // std::strong_ordering, so a failed EXPECT_EQ on one prints a byte dump.
    int order(std::strong_ordering res)
    {
        return res < 0 ? -1 : (res > 0 ? 1 : 0);
    }

    // Templated so the three sibling character types share it verbatim.
    template <typename S>
    std::string trace(const S& lhs, const S& rhs)
    {
        return ::testing::PrintToString(lhs) + " vs " + ::testing::PrintToString(rhs);
    }

    // Every fixture is an explicit range over a std::u32string, because what these
    // tests are about is the '\0' inside a range -- a C string cannot carry one.
    int compare_ptr(const collate<char32_t>& obj, const std::u32string& lhs, const std::u32string& rhs)
    {
        return order(obj.compare(lhs.data(), lhs.data() + lhs.size(),
                                 rhs.data(), rhs.data() + rhs.size()));
    }

    // U+00E4 and U+00C4 as single char32_t values.  They reach wcscoll through a
    // reinterpret_cast, so a platform where wchar_t is not UTF-32 would compare
    // something else entirely -- which is why char32_t is only supported there.
    const std::u32string a_umlaut = U"\u00E4";   // ä
    const std::u32string A_umlaut = U"\u00C4";   // Ä

    const std::u32string kCases[] = {
        U"",  U"a",  U"b",  U"ab",  U"abc",
        std::u32string(U"a\0", 2),
        std::u32string(U"a\0a", 3),
        std::u32string(U"a\0b", 3),
        std::u32string(U"b\0a", 3),
        std::u32string(U"ab\0cd", 5),
        std::u32string(U"\0", 1),
        std::u32string(U"\0\0", 2),
        a_umlaut, A_umlaut, U"B",
    };
}

TEST(CollateChar32, ANullConfigurationIsRejected)
{
    std::shared_ptr<collate_conf<char32_t>> empty;
    EXPECT_THROW(collate<char32_t>{empty}, std::runtime_error);
}

TEST(CollateChar32, EqualRangesCompareEqual)
{
    const collate<char32_t> obj = facet_for(kPlain);
    for (const std::u32string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        EXPECT_EQ(compare_ptr(obj, s, s), 0);
    }
}

TEST(CollateChar32, AProperPrefixSortsFirst)
{
    const collate<char32_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, U"abc", U"abcd"), -1);
    EXPECT_EQ(compare_ptr(obj, U"abcd", U"abc"), 1);
}

TEST(CollateChar32, AnEmptyRangeSortsBeforeANonEmptyOne)
{
    const collate<char32_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, U"", U"a"), -1);
    EXPECT_EQ(compare_ptr(obj, U"a", U""), 1);
    EXPECT_EQ(compare_ptr(obj, U"", U""), 0);
}

// 'a' is 0x61 and 'B' is 0x42, so code-point order and dictionary order disagree here.
// The "C" locale has to take the code-point order; the German case below takes the
// other one on the same pair.
TEST(CollateChar32, TheCLocaleComparesByCodePointValue)
{
    const collate<char32_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, U"a", U"B"), 1);
    EXPECT_EQ(compare_ptr(obj, a_umlaut, U"b"), 1);
    EXPECT_EQ(compare_ptr(obj, A_umlaut, U"B"), 1);
}

TEST(CollateChar32, TheFirstUnequalSegmentDecides)
{
    const collate<char32_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, std::u32string(U"a\0b", 3), std::u32string(U"a\0c", 3)), -1);
    EXPECT_EQ(compare_ptr(obj, std::u32string(U"a\0b", 3), std::u32string(U"a\0a", 3)), 1);

    // The second segment loses its say once the first one has spoken.
    EXPECT_EQ(compare_ptr(obj, std::u32string(U"b\0a", 3), std::u32string(U"a\0z", 3)), 1);
}

TEST(CollateChar32, SegmentsAfterAnEqualPrefixMakeTheLongerRangeGreater)
{
    const collate<char32_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, std::u32string(U"a\0b", 3), std::u32string(U"a\0", 2)), 1);
    EXPECT_EQ(compare_ptr(obj, std::u32string(U"a\0", 2), std::u32string(U"a\0b", 3)), -1);
}

// An embedded '\0' is a separator, so a range that contains one ends in an empty
// segment the other range does not have.  That is the only thing separating
// these two ranges, and the terminated one is the greater.
TEST(CollateChar32, AnExplicitTerminatorSortsAfterAMissingOne)
{
    const collate<char32_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, std::u32string(U"a\0", 2), U"a"), 1);
    EXPECT_EQ(compare_ptr(obj, U"a", std::u32string(U"a\0", 2)), -1);
    EXPECT_EQ(compare_ptr(obj, std::u32string(U"a\0", 2), std::u32string(U"a\0", 2)), 0);
}

TEST(CollateChar32, GermanCollationIgnoresCaseAtThePrimaryLevel)
{
    const collate<char32_t> obj = facet_for(kGerman);
    EXPECT_EQ(compare_ptr(obj, U"a", U"B"), -1);
    EXPECT_EQ(compare_ptr(obj, U"B", U"a"), 1);
}

TEST(CollateChar32, GermanCollationPlacesAnUmlautWithItsBaseLetter)
{
    const collate<char32_t> obj = facet_for(kGerman);
    EXPECT_EQ(compare_ptr(obj, a_umlaut, U"b"), -1);
    EXPECT_EQ(compare_ptr(obj, A_umlaut, U"B"), -1);
}

// Same primary weight as 'a', so the tie is broken one level down and the
// umlaut is the greater of the two.  Without this the case above would also be
// satisfied by a locale that simply dropped the diacritic.
TEST(CollateChar32, GermanCollationSeparatesAnUmlautFromItsBaseLetterAtTheSecondaryLevel)
{
    const collate<char32_t> obj = facet_for(kGerman);
    EXPECT_EQ(compare_ptr(obj, a_umlaut, U"a"), 1);
    EXPECT_EQ(compare_ptr(obj, U"a", a_umlaut), -1);
}

// The four compare() overloads reach the segmenting loop by three different
// routes -- std::find over pointers, data_to_vec over iterators, and one of each
// -- so they are only interchangeable if they agree on every pair.
TEST(CollateChar32, ListIteratorsCompareLikePointers)
{
    for (const char* loc : {kPlain, kGerman})
    {
        const collate<char32_t> obj = facet_for(loc);
        for (const std::u32string& lhs : kCases)
            for (const std::u32string& rhs : kCases)
            {
                SCOPED_TRACE(std::string(loc) + " " + trace(lhs, rhs));
                std::list<char32_t> l(lhs.begin(), lhs.end());
                std::list<char32_t> r(rhs.begin(), rhs.end());
                EXPECT_EQ(order(obj.compare(l.begin(), l.end(), r.begin(), r.end())),
                          compare_ptr(obj, lhs, rhs));
            }
    }
}

TEST(CollateChar32, DequeIteratorsCompareLikePointers)
{
    const collate<char32_t> obj = facet_for(kGerman);
    for (const std::u32string& lhs : kCases)
        for (const std::u32string& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            std::deque<char32_t> l(lhs.begin(), lhs.end());
            std::deque<char32_t> r(rhs.begin(), rhs.end());
            EXPECT_EQ(order(obj.compare(l.begin(), l.end(), r.begin(), r.end())),
                      compare_ptr(obj, lhs, rhs));
        }
}

TEST(CollateChar32, APointerAndAnIteratorCompareLikeTwoPointers)
{
    const collate<char32_t> obj = facet_for(kGerman);
    for (const std::u32string& lhs : kCases)
        for (const std::u32string& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            std::list<char32_t>  l(lhs.begin(), lhs.end());
            std::list<char32_t>  r(rhs.begin(), rhs.end());
            std::deque<char32_t> dl(lhs.begin(), lhs.end());
            std::deque<char32_t> dr(rhs.begin(), rhs.end());
            EXPECT_EQ(order(obj.compare(lhs.data(), lhs.data() + lhs.size(), r.begin(), r.end())),
                      compare_ptr(obj, lhs, rhs));
            EXPECT_EQ(order(obj.compare(l.begin(), l.end(), rhs.data(), rhs.data() + rhs.size())),
                      compare_ptr(obj, lhs, rhs));

            // A random-access iterator reaches the same overload by a different
            // deduction, so both container shapes are put through the mix.
            EXPECT_EQ(order(obj.compare(lhs.data(), lhs.data() + lhs.size(), dr.begin(), dr.end())),
                      compare_ptr(obj, lhs, rhs));
            EXPECT_EQ(order(obj.compare(dl.begin(), dl.end(), rhs.data(), rhs.data() + rhs.size())),
                      compare_ptr(obj, lhs, rhs));
        }
}

// The iterator/pointer overload is the pointer/iterator one with the arguments
// swapped and the answer negated, so a sign it forgot to flip would only show up
// here.
TEST(CollateChar32, SwappingTheArgumentsReversesTheResult)
{
    const collate<char32_t> obj = facet_for(kGerman);
    for (const std::u32string& lhs : kCases)
        for (const std::u32string& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            EXPECT_EQ(compare_ptr(obj, lhs, rhs), -compare_ptr(obj, rhs, lhs));
        }
}

// wcsxfrm is the identity in "C", and transform_length adds one per separator,
// so the key of an n-character range is n characters long however the '\0's fall.
TEST(CollateChar32, TheCLocaleKeyIsAsLongAsTheInput)
{
    const collate<char32_t> obj = facet_for(kPlain);
    for (const std::u32string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        EXPECT_EQ(obj.transform_length(s.data(), s.data() + s.size()), s.size());
    }
}

TEST(CollateChar32, TransformLengthCountsEachSegmentSeparator)
{
    const collate<char32_t> obj = facet_for(kGerman);
    const std::u32string    head(U"a\0", 2);
    const std::u32string    tail(U"b");
    const std::u32string    both(U"a\0b", 3);

    EXPECT_EQ(obj.transform_length(both.data(), both.data() + both.size()),
              obj.transform_length(head.data(), head.data() + head.size()) +
              obj.transform_length(tail.data(), tail.data() + tail.size()));
}

TEST(CollateChar32, TransformLengthIsTheSameThroughIterators)
{
    for (const char* loc : {kPlain, kGerman})
    {
        const collate<char32_t> obj = facet_for(loc);
        for (const std::u32string& s : kCases)
        {
            SCOPED_TRACE(std::string(loc) + " " + ::testing::PrintToString(s));
            const std::size_t    expected = obj.transform_length(s.data(), s.data() + s.size());
            std::list<char32_t>  l(s.begin(), s.end());
            std::deque<char32_t> d(s.begin(), s.end());
            EXPECT_EQ(obj.transform_length(l.begin(), l.end()), expected);
            EXPECT_EQ(obj.transform_length(d.begin(), d.end()), expected);
        }
    }
}

TEST(CollateChar32, TheCLocaleKeyIsTheInputItself)
{
    const collate<char32_t> obj = facet_for(kPlain);
    for (const std::u32string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        std::u32string key(s.size(), U'\xFF');
        auto [it, n] = obj.transform(s.data(), s.data() + s.size(), key.data());
        EXPECT_EQ(n, s.size());
        EXPECT_EQ(it, key.data() + s.size());
        EXPECT_EQ(key, s);
    }
}

TEST(CollateChar32, TransformWritesExactlyTransformLengthCharacters)
{
    const collate<char32_t> obj = facet_for(kGerman);
    for (const std::u32string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::u32string    key(len, U'\xFF');
        auto [it, n] = obj.transform(s.data(), s.data() + s.size(), key.data());
        EXPECT_EQ(n, len);
        EXPECT_EQ(it, key.data() + len);
    }
}

// What transform() is for: comparing two keys byte by byte has to give the same
// answer as calling compare() on the originals.  Byte order alone does not, or
// the facet would have nothing to do.
TEST(CollateChar32, KeysOrderLikeCompare)
{
    const collate<char32_t> obj = facet_for(kGerman);
    for (const std::u32string& lhs : kCases)
        for (const std::u32string& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            std::u32string kl, kr;
            obj.transform(lhs.data(), lhs.data() + lhs.size(), std::back_inserter(kl));
            obj.transform(rhs.data(), rhs.data() + rhs.size(), std::back_inserter(kr));
            EXPECT_EQ(order(kl <=> kr), compare_ptr(obj, lhs, rhs));
        }
}

// A terminated segment contributes its weights and a separator; an unterminated
// tail contributes only its weights.  So splitting a range at a '\0' splits its
// key at the same place.
TEST(CollateChar32, AKeyIsTheConcatenationOfItsSegmentKeys)
{
    const collate<char32_t> obj = facet_for(kGerman);
    const std::u32string    head(U"Zange\0", 6);
    const std::u32string    tail(A_umlaut + U"pfel");
    const std::u32string    both = head + tail;

    std::u32string k_head, k_tail, k_both;
    obj.transform(head.data(), head.data() + head.size(), std::back_inserter(k_head));
    obj.transform(tail.data(), tail.data() + tail.size(), std::back_inserter(k_tail));
    obj.transform(both.data(), both.data() + both.size(), std::back_inserter(k_both));

    EXPECT_EQ(k_head.size() + k_tail.size(), k_both.size());
    EXPECT_EQ(k_head + k_tail, k_both);
    EXPECT_EQ(obj.transform_length(both.data(), both.data() + both.size()), k_both.size());
}

TEST(CollateChar32, AnOutputIteratorReceivesTheSameKey)
{
    const collate<char32_t> obj = facet_for(kGerman);
    for (const std::u32string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::u32string    direct(len, U'\xFF');
        obj.transform(s.data(), s.data() + s.size(), direct.data());

        std::u32string through;
        auto [it, n] = obj.transform(s.data(), s.data() + s.size(), std::back_inserter(through));
        EXPECT_EQ(n, len);
        EXPECT_EQ(through, direct);
    }
}

TEST(CollateChar32, AnInputIteratorProducesTheSameKey)
{
    const collate<char32_t> obj = facet_for(kGerman);
    for (const std::u32string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::u32string    direct(len, U'\xFF');
        obj.transform(s.data(), s.data() + s.size(), direct.data());

        std::list<char32_t> l(s.begin(), s.end());
        std::u32string      through(len, U'\xFF');
        auto [it, n] = obj.transform(l.begin(), l.end(), through.data());
        EXPECT_EQ(n, len);
        EXPECT_EQ(it, through.data() + len);
        EXPECT_EQ(through, direct);

        std::deque<char32_t> d(s.begin(), s.end());
        std::u32string       random_access(len, U'\xFF');
        EXPECT_EQ(obj.transform(d.begin(), d.end(), random_access.data()).second, len);
        EXPECT_EQ(random_access, direct);
    }
}

TEST(CollateChar32, IteratorsOnBothSidesProduceTheSameKey)
{
    const collate<char32_t> obj = facet_for(kGerman);
    for (const std::u32string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::u32string    direct(len, U'\xFF');
        obj.transform(s.data(), s.data() + s.size(), direct.data());

        std::deque<char32_t> d(s.begin(), s.end());
        std::u32string       through;
        auto [it, n] = obj.transform(d.begin(), d.end(), std::back_inserter(through));
        EXPECT_EQ(n, len);
        EXPECT_EQ(through, direct);
    }
}

// mx_len is a hard cap on characters written, checked both before a segment and
// before the separator that follows it.  In "C" the key is the input, so the
// truncation point is visible: 3 stops right after the separator, 4 keeps one
// character of the second segment.
TEST(CollateChar32, AMaximumLengthTruncatesTheKey)
{
    const collate<char32_t> obj   = facet_for(kPlain);
    const std::u32string    input(U"ab\0cd", 5);
    const char32_t*         lo = input.data();
    const char32_t*         hi = input.data() + input.size();

    for (std::size_t mx = 1; mx <= input.size(); ++mx)
    {
        SCOPED_TRACE(mx);
        const std::u32string expected = input.substr(0, mx);

        std::u32string a(mx, U'\xFF');
        EXPECT_EQ(obj.transform(lo, hi, a.data(), mx).second, mx);
        EXPECT_EQ(a, expected);

        std::list<char32_t> l(input.begin(), input.end());
        std::u32string      b(mx, U'\xFF');
        EXPECT_EQ(obj.transform(l.begin(), l.end(), b.data(), mx).second, mx);
        EXPECT_EQ(b, expected);

        std::u32string c;
        EXPECT_EQ(obj.transform(lo, hi, std::back_inserter(c), mx).second, mx);
        EXPECT_EQ(c, expected);

        std::u32string d;
        EXPECT_EQ(obj.transform(l.begin(), l.end(), std::back_inserter(d), mx).second, mx);
        EXPECT_EQ(d, expected);
    }
}

// The staging buffers start at reserve(64), so anything past that grows them.
// 128 KiB in one segment forces many growth rounds and a key far too large for
// any small-buffer optimisation; in "C" the result is still exactly the input.
TEST(CollateChar32, ALargeInputIsTransformedInOnePiece)
{
    const collate<char32_t> obj = facet_for(kPlain);
    const std::u32string    input(std::size_t{1} << 17, U'a');

    EXPECT_EQ(obj.transform_length(input.data(), input.data() + input.size()), input.size());

    std::u32string key(input.size(), U'\xFF');
    auto [it, n] = obj.transform(input.data(), input.data() + input.size(), key.data());
    EXPECT_EQ(n, input.size());
    EXPECT_EQ(key, input);
}

// The same amount of work spread over many segments instead of one: every
// iteration reuses the buffers the previous one grew, and the separators have to
// come out in the right places.
TEST(CollateChar32, ManySegmentsAreTransformedInOrder)
{
    const collate<char32_t> obj      = facet_for(kPlain);
    const std::size_t       segments = 5000;
    std::u32string          input;
    for (std::size_t i = 0; i < segments; ++i)
        input += std::u32string(U"ab\0", 3);

    EXPECT_EQ(obj.transform_length(input.data(), input.data() + input.size()), input.size());

    std::u32string key;
    auto [it, n] = obj.transform(input.data(), input.data() + input.size(), std::back_inserter(key));
    EXPECT_EQ(n, input.size());
    EXPECT_EQ(key, input);
}
