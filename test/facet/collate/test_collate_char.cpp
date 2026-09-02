// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The collation contract for IOv2::collate<char>, as stated in
 * include/facet/collate.h and include/facet/collate_details.h: a range is cut
 * into segments at every '\0', each segment goes through strcoll/strxfrm under
 * the configured locale, and the first unequal segment decides the order.
 *
 * Two locales make those rules checkable without depending on collation data.
 * In "C", strcoll is byte order and strxfrm is the identity, so a key is its own
 * input and every expectation below can be read off the literal.  In
 * de_DE.UTF-8, a letter's primary weight ignores case and diacritics, which is
 * precisely where collation order and byte order disagree -- the reason
 * transform() exists at all.
 */
#include <facet/collate.h>
#include <facet/collate_details.h>

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

    collate<char> facet_for(const char* loc)
    {
        return collate<char>(std::make_shared<collate_conf<char>>(loc));
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

    // Every fixture is an explicit range over a std::string, because what these
    // tests are about is the '\0' inside a range -- a C string cannot carry one.
    int compare_ptr(const collate<char>& obj, const std::string& lhs, const std::string& rhs)
    {
        return order(obj.compare(lhs.data(), lhs.data() + lhs.size(),
                                 rhs.data(), rhs.data() + rhs.size()));
    }

    // U+00E4 and U+00C4 spelled out: the file is read as UTF-8, but a reader
    // still has to know which bytes reach strcoll for the German cases to mean
    // anything.
    const std::string a_umlaut = "\xC3\xA4";  // ä
    const std::string A_umlaut = "\xC3\x84";  // Ä

    const std::string kCases[] = {
        "",  "a",  "b",  "ab",  "abc",
        std::string("a\0", 2),
        std::string("a\0a", 3),
        std::string("a\0b", 3),
        std::string("b\0a", 3),
        std::string("ab\0cd", 5),
        std::string("\0", 1),
        std::string("\0\0", 2),
        a_umlaut, A_umlaut, "B",
    };
}

TEST(CollateChar, ANullConfigurationIsRejected)
{
    std::shared_ptr<collate_conf<char>> empty;
    EXPECT_THROW(collate<char>{empty}, std::runtime_error);
}

TEST(CollateChar, EqualRangesCompareEqual)
{
    const collate<char> obj = facet_for(kPlain);
    for (const std::string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        EXPECT_EQ(compare_ptr(obj, s, s), 0);
    }
}

TEST(CollateChar, AProperPrefixSortsFirst)
{
    const collate<char> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, "abc", "abcd"), -1);
    EXPECT_EQ(compare_ptr(obj, "abcd", "abc"), 1);
}

TEST(CollateChar, AnEmptyRangeSortsBeforeANonEmptyOne)
{
    const collate<char> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, "", "a"), -1);
    EXPECT_EQ(compare_ptr(obj, "a", ""), 1);
    EXPECT_EQ(compare_ptr(obj, "", ""), 0);
}

// 'a' is 0x61 and 'B' is 0x42, so byte order and dictionary order disagree here.
// The "C" locale has to take the byte order; the German case below takes the
// other one on the same pair.
TEST(CollateChar, TheCLocaleComparesByByteValue)
{
    const collate<char> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, "a", "B"), 1);
    EXPECT_EQ(compare_ptr(obj, a_umlaut, "b"), 1);
    EXPECT_EQ(compare_ptr(obj, A_umlaut, "B"), 1);
}

TEST(CollateChar, TheFirstUnequalSegmentDecides)
{
    const collate<char> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, std::string("a\0b", 3), std::string("a\0c", 3)), -1);
    EXPECT_EQ(compare_ptr(obj, std::string("a\0b", 3), std::string("a\0a", 3)), 1);

    // The second segment loses its say once the first one has spoken.
    EXPECT_EQ(compare_ptr(obj, std::string("b\0a", 3), std::string("a\0z", 3)), 1);
}

TEST(CollateChar, SegmentsAfterAnEqualPrefixMakeTheLongerRangeGreater)
{
    const collate<char> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, std::string("a\0b", 3), std::string("a\0", 2)), 1);
    EXPECT_EQ(compare_ptr(obj, std::string("a\0", 2), std::string("a\0b", 3)), -1);
}

// An embedded '\0' is a separator, so a range that contains one ends in an empty
// segment the other range does not have.  That is the only thing separating
// these two ranges, and the terminated one is the greater.
TEST(CollateChar, AnExplicitTerminatorSortsAfterAMissingOne)
{
    const collate<char> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, std::string("a\0", 2), "a"), 1);
    EXPECT_EQ(compare_ptr(obj, "a", std::string("a\0", 2)), -1);
    EXPECT_EQ(compare_ptr(obj, std::string("a\0", 2), std::string("a\0", 2)), 0);
}

TEST(CollateChar, GermanCollationIgnoresCaseAtThePrimaryLevel)
{
    const collate<char> obj = facet_for(kGerman);
    EXPECT_EQ(compare_ptr(obj, "a", "B"), -1);
    EXPECT_EQ(compare_ptr(obj, "B", "a"), 1);
}

TEST(CollateChar, GermanCollationPlacesAnUmlautWithItsBaseLetter)
{
    const collate<char> obj = facet_for(kGerman);
    EXPECT_EQ(compare_ptr(obj, a_umlaut, "b"), -1);
    EXPECT_EQ(compare_ptr(obj, A_umlaut, "B"), -1);
}

// Same primary weight as 'a', so the tie is broken one level down and the
// umlaut is the greater of the two.  Without this the case above would also be
// satisfied by a locale that simply dropped the diacritic.
TEST(CollateChar, GermanCollationSeparatesAnUmlautFromItsBaseLetterAtTheSecondaryLevel)
{
    const collate<char> obj = facet_for(kGerman);
    EXPECT_EQ(compare_ptr(obj, a_umlaut, "a"), 1);
    EXPECT_EQ(compare_ptr(obj, "a", a_umlaut), -1);
}

// The four compare() overloads reach the segmenting loop by three different
// routes -- std::find over pointers, data_to_vec over iterators, and one of each
// -- so they are only interchangeable if they agree on every pair.
TEST(CollateChar, ListIteratorsCompareLikePointers)
{
    for (const char* loc : {kPlain, kGerman})
    {
        const collate<char> obj = facet_for(loc);
        for (const std::string& lhs : kCases)
            for (const std::string& rhs : kCases)
            {
                SCOPED_TRACE(std::string(loc) + " " + trace(lhs, rhs));
                std::list<char> l(lhs.begin(), lhs.end());
                std::list<char> r(rhs.begin(), rhs.end());
                EXPECT_EQ(order(obj.compare(l.begin(), l.end(), r.begin(), r.end())),
                          compare_ptr(obj, lhs, rhs));
            }
    }
}

TEST(CollateChar, DequeIteratorsCompareLikePointers)
{
    const collate<char> obj = facet_for(kGerman);
    for (const std::string& lhs : kCases)
        for (const std::string& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            std::deque<char> l(lhs.begin(), lhs.end());
            std::deque<char> r(rhs.begin(), rhs.end());
            EXPECT_EQ(order(obj.compare(l.begin(), l.end(), r.begin(), r.end())),
                      compare_ptr(obj, lhs, rhs));
        }
}

TEST(CollateChar, APointerAndAnIteratorCompareLikeTwoPointers)
{
    const collate<char> obj = facet_for(kGerman);
    for (const std::string& lhs : kCases)
        for (const std::string& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            std::list<char>  l(lhs.begin(), lhs.end());
            std::list<char>  r(rhs.begin(), rhs.end());
            std::deque<char> dl(lhs.begin(), lhs.end());
            std::deque<char> dr(rhs.begin(), rhs.end());
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
TEST(CollateChar, SwappingTheArgumentsReversesTheResult)
{
    const collate<char> obj = facet_for(kGerman);
    for (const std::string& lhs : kCases)
        for (const std::string& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            EXPECT_EQ(compare_ptr(obj, lhs, rhs), -compare_ptr(obj, rhs, lhs));
        }
}

// strxfrm is the identity in "C", and transform_length adds one per separator,
// so the key of an n-character range is n characters long however the '\0's fall.
TEST(CollateChar, TheCLocaleKeyIsAsLongAsTheInput)
{
    const collate<char> obj = facet_for(kPlain);
    for (const std::string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        EXPECT_EQ(obj.transform_length(s.data(), s.data() + s.size()), s.size());
    }
}

TEST(CollateChar, TransformLengthCountsEachSegmentSeparator)
{
    const collate<char> obj = facet_for(kGerman);
    const std::string   head("a\0", 2);
    const std::string   tail("b");
    const std::string   both("a\0b", 3);

    EXPECT_EQ(obj.transform_length(both.data(), both.data() + both.size()),
              obj.transform_length(head.data(), head.data() + head.size()) +
              obj.transform_length(tail.data(), tail.data() + tail.size()));
}

TEST(CollateChar, TransformLengthIsTheSameThroughIterators)
{
    for (const char* loc : {kPlain, kGerman})
    {
        const collate<char> obj = facet_for(loc);
        for (const std::string& s : kCases)
        {
            SCOPED_TRACE(std::string(loc) + " " + ::testing::PrintToString(s));
            const std::size_t expected = obj.transform_length(s.data(), s.data() + s.size());
            std::list<char>   l(s.begin(), s.end());
            std::deque<char>  d(s.begin(), s.end());
            EXPECT_EQ(obj.transform_length(l.begin(), l.end()), expected);
            EXPECT_EQ(obj.transform_length(d.begin(), d.end()), expected);
        }
    }
}

TEST(CollateChar, TheCLocaleKeyIsTheInputItself)
{
    const collate<char> obj = facet_for(kPlain);
    for (const std::string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        std::string key(s.size(), '\xFF');
        auto [it, n] = obj.transform(s.data(), s.data() + s.size(), key.data());
        EXPECT_EQ(n, s.size());
        EXPECT_EQ(it, key.data() + s.size());
        EXPECT_EQ(key, s);
    }
}

TEST(CollateChar, TransformWritesExactlyTransformLengthCharacters)
{
    const collate<char> obj = facet_for(kGerman);
    for (const std::string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::string       key(len, '\xFF');
        auto [it, n] = obj.transform(s.data(), s.data() + s.size(), key.data());
        EXPECT_EQ(n, len);
        EXPECT_EQ(it, key.data() + len);
    }
}

// What transform() is for: comparing two keys byte by byte has to give the same
// answer as calling compare() on the originals.  Byte order alone does not, or
// the facet would have nothing to do.
TEST(CollateChar, KeysOrderLikeCompare)
{
    const collate<char> obj = facet_for(kGerman);
    for (const std::string& lhs : kCases)
        for (const std::string& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            std::string kl, kr;
            obj.transform(lhs.data(), lhs.data() + lhs.size(), std::back_inserter(kl));
            obj.transform(rhs.data(), rhs.data() + rhs.size(), std::back_inserter(kr));
            EXPECT_EQ(order(kl <=> kr), compare_ptr(obj, lhs, rhs));
        }
}

// A terminated segment contributes its weights and a separator; an unterminated
// tail contributes only its weights.  So splitting a range at a '\0' splits its
// key at the same place.
TEST(CollateChar, AKeyIsTheConcatenationOfItsSegmentKeys)
{
    const collate<char> obj = facet_for(kGerman);
    const std::string   head("Zange\0", 6);
    const std::string   tail(A_umlaut + "pfel");
    const std::string   both = head + tail;

    std::string k_head, k_tail, k_both;
    obj.transform(head.data(), head.data() + head.size(), std::back_inserter(k_head));
    obj.transform(tail.data(), tail.data() + tail.size(), std::back_inserter(k_tail));
    obj.transform(both.data(), both.data() + both.size(), std::back_inserter(k_both));

    EXPECT_EQ(k_head.size() + k_tail.size(), k_both.size());
    EXPECT_EQ(k_head + k_tail, k_both);
    EXPECT_EQ(obj.transform_length(both.data(), both.data() + both.size()), k_both.size());
}

TEST(CollateChar, AnOutputIteratorReceivesTheSameKey)
{
    const collate<char> obj = facet_for(kGerman);
    for (const std::string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::string       direct(len, '\xFF');
        obj.transform(s.data(), s.data() + s.size(), direct.data());

        std::string through;
        auto [it, n] = obj.transform(s.data(), s.data() + s.size(), std::back_inserter(through));
        EXPECT_EQ(n, len);
        EXPECT_EQ(through, direct);
    }
}

TEST(CollateChar, AnInputIteratorProducesTheSameKey)
{
    const collate<char> obj = facet_for(kGerman);
    for (const std::string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::string       direct(len, '\xFF');
        obj.transform(s.data(), s.data() + s.size(), direct.data());

        std::list<char> l(s.begin(), s.end());
        std::string     through(len, '\xFF');
        auto [it, n] = obj.transform(l.begin(), l.end(), through.data());
        EXPECT_EQ(n, len);
        EXPECT_EQ(it, through.data() + len);
        EXPECT_EQ(through, direct);

        std::deque<char> d(s.begin(), s.end());
        std::string      random_access(len, '\xFF');
        EXPECT_EQ(obj.transform(d.begin(), d.end(), random_access.data()).second, len);
        EXPECT_EQ(random_access, direct);
    }
}

TEST(CollateChar, IteratorsOnBothSidesProduceTheSameKey)
{
    const collate<char> obj = facet_for(kGerman);
    for (const std::string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::string       direct(len, '\xFF');
        obj.transform(s.data(), s.data() + s.size(), direct.data());

        std::deque<char> d(s.begin(), s.end());
        std::string      through;
        auto [it, n] = obj.transform(d.begin(), d.end(), std::back_inserter(through));
        EXPECT_EQ(n, len);
        EXPECT_EQ(through, direct);
    }
}

// mx_len is a hard cap on characters written, checked both before a segment and
// before the separator that follows it.  In "C" the key is the input, so the
// truncation point is visible: 3 stops right after the separator, 4 keeps one
// character of the second segment.
TEST(CollateChar, AMaximumLengthTruncatesTheKey)
{
    const collate<char> obj   = facet_for(kPlain);
    const std::string   input("ab\0cd", 5);
    const char*         lo = input.data();
    const char*         hi = input.data() + input.size();

    for (std::size_t mx = 1; mx <= input.size(); ++mx)
    {
        SCOPED_TRACE(mx);
        const std::string expected = input.substr(0, mx);

        std::string a(mx, '\xFF');
        EXPECT_EQ(obj.transform(lo, hi, a.data(), mx).second, mx);
        EXPECT_EQ(a, expected);

        std::list<char> l(input.begin(), input.end());
        std::string     b(mx, '\xFF');
        EXPECT_EQ(obj.transform(l.begin(), l.end(), b.data(), mx).second, mx);
        EXPECT_EQ(b, expected);

        std::string c;
        EXPECT_EQ(obj.transform(lo, hi, std::back_inserter(c), mx).second, mx);
        EXPECT_EQ(c, expected);

        std::string d;
        EXPECT_EQ(obj.transform(l.begin(), l.end(), std::back_inserter(d), mx).second, mx);
        EXPECT_EQ(d, expected);
    }
}

// The staging buffers start at reserve(64), so anything past that grows them.
// 128 KiB in one segment forces many growth rounds and a key far too large for
// any small-buffer optimisation; in "C" the result is still exactly the input.
TEST(CollateChar, ALargeInputIsTransformedInOnePiece)
{
    const collate<char> obj = facet_for(kPlain);
    const std::string   input(std::size_t{1} << 17, 'a');

    EXPECT_EQ(obj.transform_length(input.data(), input.data() + input.size()), input.size());

    std::string key(input.size(), '\xFF');
    auto [it, n] = obj.transform(input.data(), input.data() + input.size(), key.data());
    EXPECT_EQ(n, input.size());
    EXPECT_EQ(key, input);
}

// The same amount of work spread over many segments instead of one: every
// iteration reuses the buffers the previous one grew, and the separators have to
// come out in the right places.
TEST(CollateChar, ManySegmentsAreTransformedInOrder)
{
    const collate<char> obj      = facet_for(kPlain);
    const std::size_t   segments = 5000;
    std::string         input;
    for (std::size_t i = 0; i < segments; ++i)
        input += std::string("ab\0", 3);

    EXPECT_EQ(obj.transform_length(input.data(), input.data() + input.size()), input.size());

    std::string key;
    auto [it, n] = obj.transform(input.data(), input.data() + input.size(), std::back_inserter(key));
    EXPECT_EQ(n, input.size());
    EXPECT_EQ(key, input);
}
