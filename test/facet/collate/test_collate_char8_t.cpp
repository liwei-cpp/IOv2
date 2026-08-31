/**
 * The same collation contract as test_collate_char.cpp for char8_t, which
 * routes through the narrow strcoll and strxfrm on its UTF-8 bytes.  That is
 * only correct when the locale's codeset is UTF-8, so collate_conf<char8_t>
 * refuses anything else -- the plain locale here is "C.UTF-8" rather than "C.UTF-8"
 * for that reason, and the last case in the file is what checks the refusal.
 *
 * "C.UTF-8" collates by code point and keeps strxfrm an identity, so a key is
 * still its own input; de_DE.UTF-8 is where collation order and byte order
 * disagree.
 */
#include <common/defs.h>
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
    constexpr const char* kPlain  = "C.UTF-8";
    constexpr const char* kGerman = "de_DE.UTF-8";

    collate<char8_t> facet_for(const char* loc)
    {
        return collate<char8_t>(std::make_shared<collate_conf<char8_t>>(loc));
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

    // Every fixture is an explicit range over a std::u8string, because what these
    // tests are about is the '\0' inside a range -- a C string cannot carry one.
    int compare_ptr(const collate<char8_t>& obj, const std::u8string& lhs, const std::u8string& rhs)
    {
        return order(obj.compare(lhs.data(), lhs.data() + lhs.size(),
                                 rhs.data(), rhs.data() + rhs.size()));
    }

    // U+00E4 and U+00C4 in UTF-8, two bytes each.  char8_t goes to the narrow
    // strcoll, so these are the bytes it actually sees -- the same ones the char
    // file uses, under a type that says they are UTF-8.
    const std::u8string a_umlaut = u8"\u00E4";   // ä
    const std::u8string A_umlaut = u8"\u00C4";   // Ä

    const std::u8string kCases[] = {
        u8"",  u8"a",  u8"b",  u8"ab",  u8"abc",
        std::u8string(u8"a\0", 2),
        std::u8string(u8"a\0a", 3),
        std::u8string(u8"a\0b", 3),
        std::u8string(u8"b\0a", 3),
        std::u8string(u8"ab\0cd", 5),
        std::u8string(u8"\0", 1),
        std::u8string(u8"\0\0", 2),
        a_umlaut, A_umlaut, u8"B",
    };
}

TEST(CollateChar8, ANullConfigurationIsRejected)
{
    std::shared_ptr<collate_conf<char8_t>> empty;
    EXPECT_THROW(collate<char8_t>{empty}, std::runtime_error);
}

TEST(CollateChar8, EqualRangesCompareEqual)
{
    const collate<char8_t> obj = facet_for(kPlain);
    for (const std::u8string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        EXPECT_EQ(compare_ptr(obj, s, s), 0);
    }
}

TEST(CollateChar8, AProperPrefixSortsFirst)
{
    const collate<char8_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, u8"abc", u8"abcd"), -1);
    EXPECT_EQ(compare_ptr(obj, u8"abcd", u8"abc"), 1);
}

TEST(CollateChar8, AnEmptyRangeSortsBeforeANonEmptyOne)
{
    const collate<char8_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, u8"", u8"a"), -1);
    EXPECT_EQ(compare_ptr(obj, u8"a", u8""), 1);
    EXPECT_EQ(compare_ptr(obj, u8"", u8""), 0);
}

// 'a' is 0x61 and 'B' is 0x42, so byte order and dictionary order disagree here.
// The "C.UTF-8" locale has to take the byte order; the German case below takes the
// other one on the same pair.
TEST(CollateChar8, TheCLocaleComparesByByteValue)
{
    const collate<char8_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, u8"a", u8"B"), 1);
    EXPECT_EQ(compare_ptr(obj, a_umlaut, u8"b"), 1);
    EXPECT_EQ(compare_ptr(obj, A_umlaut, u8"B"), 1);
}

TEST(CollateChar8, TheFirstUnequalSegmentDecides)
{
    const collate<char8_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, std::u8string(u8"a\0b", 3), std::u8string(u8"a\0c", 3)), -1);
    EXPECT_EQ(compare_ptr(obj, std::u8string(u8"a\0b", 3), std::u8string(u8"a\0a", 3)), 1);

    // The second segment loses its say once the first one has spoken.
    EXPECT_EQ(compare_ptr(obj, std::u8string(u8"b\0a", 3), std::u8string(u8"a\0z", 3)), 1);
}

TEST(CollateChar8, SegmentsAfterAnEqualPrefixMakeTheLongerRangeGreater)
{
    const collate<char8_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, std::u8string(u8"a\0b", 3), std::u8string(u8"a\0", 2)), 1);
    EXPECT_EQ(compare_ptr(obj, std::u8string(u8"a\0", 2), std::u8string(u8"a\0b", 3)), -1);
}

// An embedded '\0' is a separator, so a range that contains one ends in an empty
// segment the other range does not have.  That is the only thing separating
// these two ranges, and the terminated one is the greater.
TEST(CollateChar8, AnExplicitTerminatorSortsAfterAMissingOne)
{
    const collate<char8_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, std::u8string(u8"a\0", 2), u8"a"), 1);
    EXPECT_EQ(compare_ptr(obj, u8"a", std::u8string(u8"a\0", 2)), -1);
    EXPECT_EQ(compare_ptr(obj, std::u8string(u8"a\0", 2), std::u8string(u8"a\0", 2)), 0);
}

TEST(CollateChar8, GermanCollationIgnoresCaseAtThePrimaryLevel)
{
    const collate<char8_t> obj = facet_for(kGerman);
    EXPECT_EQ(compare_ptr(obj, u8"a", u8"B"), -1);
    EXPECT_EQ(compare_ptr(obj, u8"B", u8"a"), 1);
}

TEST(CollateChar8, GermanCollationPlacesAnUmlautWithItsBaseLetter)
{
    const collate<char8_t> obj = facet_for(kGerman);
    EXPECT_EQ(compare_ptr(obj, a_umlaut, u8"b"), -1);
    EXPECT_EQ(compare_ptr(obj, A_umlaut, u8"B"), -1);
}

// Same primary weight as 'a', so the tie is broken one level down and the
// umlaut is the greater of the two.  Without this the case above would also be
// satisfied by a locale that simply dropped the diacritic.
TEST(CollateChar8, GermanCollationSeparatesAnUmlautFromItsBaseLetterAtTheSecondaryLevel)
{
    const collate<char8_t> obj = facet_for(kGerman);
    EXPECT_EQ(compare_ptr(obj, a_umlaut, u8"a"), 1);
    EXPECT_EQ(compare_ptr(obj, u8"a", a_umlaut), -1);
}

// The four compare() overloads reach the segmenting loop by three different
// routes -- std::find over pointers, data_to_vec over iterators, and one of each
// -- so they are only interchangeable if they agree on every pair.
TEST(CollateChar8, ListIteratorsCompareLikePointers)
{
    for (const char* loc : {kPlain, kGerman})
    {
        const collate<char8_t> obj = facet_for(loc);
        for (const std::u8string& lhs : kCases)
            for (const std::u8string& rhs : kCases)
            {
                SCOPED_TRACE(std::string(loc) + " " + trace(lhs, rhs));
                std::list<char8_t> l(lhs.begin(), lhs.end());
                std::list<char8_t> r(rhs.begin(), rhs.end());
                EXPECT_EQ(order(obj.compare(l.begin(), l.end(), r.begin(), r.end())),
                          compare_ptr(obj, lhs, rhs));
            }
    }
}

TEST(CollateChar8, DequeIteratorsCompareLikePointers)
{
    const collate<char8_t> obj = facet_for(kGerman);
    for (const std::u8string& lhs : kCases)
        for (const std::u8string& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            std::deque<char8_t> l(lhs.begin(), lhs.end());
            std::deque<char8_t> r(rhs.begin(), rhs.end());
            EXPECT_EQ(order(obj.compare(l.begin(), l.end(), r.begin(), r.end())),
                      compare_ptr(obj, lhs, rhs));
        }
}

TEST(CollateChar8, APointerAndAnIteratorCompareLikeTwoPointers)
{
    const collate<char8_t> obj = facet_for(kGerman);
    for (const std::u8string& lhs : kCases)
        for (const std::u8string& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            std::list<char8_t>  l(lhs.begin(), lhs.end());
            std::list<char8_t>  r(rhs.begin(), rhs.end());
            std::deque<char8_t> dl(lhs.begin(), lhs.end());
            std::deque<char8_t> dr(rhs.begin(), rhs.end());
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
TEST(CollateChar8, SwappingTheArgumentsReversesTheResult)
{
    const collate<char8_t> obj = facet_for(kGerman);
    for (const std::u8string& lhs : kCases)
        for (const std::u8string& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            EXPECT_EQ(compare_ptr(obj, lhs, rhs), -compare_ptr(obj, rhs, lhs));
        }
}

// strxfrm is the identity in "C.UTF-8", and transform_length adds one per separator,
// so the key of an n-character range is n characters long however the '\0's fall.
TEST(CollateChar8, TheCLocaleKeyIsAsLongAsTheInput)
{
    const collate<char8_t> obj = facet_for(kPlain);
    for (const std::u8string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        EXPECT_EQ(obj.transform_length(s.data(), s.data() + s.size()), s.size());
    }
}

TEST(CollateChar8, TransformLengthCountsEachSegmentSeparator)
{
    const collate<char8_t> obj = facet_for(kGerman);
    const std::u8string    head(u8"a\0", 2);
    const std::u8string    tail(u8"b");
    const std::u8string    both(u8"a\0b", 3);

    EXPECT_EQ(obj.transform_length(both.data(), both.data() + both.size()),
              obj.transform_length(head.data(), head.data() + head.size()) +
              obj.transform_length(tail.data(), tail.data() + tail.size()));
}

TEST(CollateChar8, TransformLengthIsTheSameThroughIterators)
{
    for (const char* loc : {kPlain, kGerman})
    {
        const collate<char8_t> obj = facet_for(loc);
        for (const std::u8string& s : kCases)
        {
            SCOPED_TRACE(std::string(loc) + " " + ::testing::PrintToString(s));
            const std::size_t   expected = obj.transform_length(s.data(), s.data() + s.size());
            std::list<char8_t>  l(s.begin(), s.end());
            std::deque<char8_t> d(s.begin(), s.end());
            EXPECT_EQ(obj.transform_length(l.begin(), l.end()), expected);
            EXPECT_EQ(obj.transform_length(d.begin(), d.end()), expected);
        }
    }
}

TEST(CollateChar8, TheCLocaleKeyIsTheInputItself)
{
    const collate<char8_t> obj = facet_for(kPlain);
    for (const std::u8string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        std::u8string key(s.size(), u8'\xFF');
        auto [it, n] = obj.transform(s.data(), s.data() + s.size(), key.data());
        EXPECT_EQ(n, s.size());
        EXPECT_EQ(it, key.data() + s.size());
        EXPECT_EQ(key, s);
    }
}

TEST(CollateChar8, TransformWritesExactlyTransformLengthCharacters)
{
    const collate<char8_t> obj = facet_for(kGerman);
    for (const std::u8string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::u8string     key(len, u8'\xFF');
        auto [it, n] = obj.transform(s.data(), s.data() + s.size(), key.data());
        EXPECT_EQ(n, len);
        EXPECT_EQ(it, key.data() + len);
    }
}

// What transform() is for: comparing two keys byte by byte has to give the same
// answer as calling compare() on the originals.  Byte order alone does not, or
// the facet would have nothing to do.
TEST(CollateChar8, KeysOrderLikeCompare)
{
    const collate<char8_t> obj = facet_for(kGerman);
    for (const std::u8string& lhs : kCases)
        for (const std::u8string& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            std::u8string kl, kr;
            obj.transform(lhs.data(), lhs.data() + lhs.size(), std::back_inserter(kl));
            obj.transform(rhs.data(), rhs.data() + rhs.size(), std::back_inserter(kr));
            EXPECT_EQ(order(kl <=> kr), compare_ptr(obj, lhs, rhs));
        }
}

// A terminated segment contributes its weights and a separator; an unterminated
// tail contributes only its weights.  So splitting a range at a '\0' splits its
// key at the same place.
TEST(CollateChar8, AKeyIsTheConcatenationOfItsSegmentKeys)
{
    const collate<char8_t> obj = facet_for(kGerman);
    const std::u8string    head(u8"Zange\0", 6);
    const std::u8string    tail(A_umlaut + u8"pfel");
    const std::u8string    both = head + tail;

    std::u8string k_head, k_tail, k_both;
    obj.transform(head.data(), head.data() + head.size(), std::back_inserter(k_head));
    obj.transform(tail.data(), tail.data() + tail.size(), std::back_inserter(k_tail));
    obj.transform(both.data(), both.data() + both.size(), std::back_inserter(k_both));

    EXPECT_EQ(k_head.size() + k_tail.size(), k_both.size());
    EXPECT_EQ(k_head + k_tail, k_both);
    EXPECT_EQ(obj.transform_length(both.data(), both.data() + both.size()), k_both.size());
}

TEST(CollateChar8, AnOutputIteratorReceivesTheSameKey)
{
    const collate<char8_t> obj = facet_for(kGerman);
    for (const std::u8string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::u8string     direct(len, u8'\xFF');
        obj.transform(s.data(), s.data() + s.size(), direct.data());

        std::u8string through;
        auto [it, n] = obj.transform(s.data(), s.data() + s.size(), std::back_inserter(through));
        EXPECT_EQ(n, len);
        EXPECT_EQ(through, direct);
    }
}

TEST(CollateChar8, AnInputIteratorProducesTheSameKey)
{
    const collate<char8_t> obj = facet_for(kGerman);
    for (const std::u8string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::u8string     direct(len, u8'\xFF');
        obj.transform(s.data(), s.data() + s.size(), direct.data());

        std::list<char8_t> l(s.begin(), s.end());
        std::u8string      through(len, u8'\xFF');
        auto [it, n] = obj.transform(l.begin(), l.end(), through.data());
        EXPECT_EQ(n, len);
        EXPECT_EQ(it, through.data() + len);
        EXPECT_EQ(through, direct);

        std::deque<char8_t> d(s.begin(), s.end());
        std::u8string       random_access(len, u8'\xFF');
        EXPECT_EQ(obj.transform(d.begin(), d.end(), random_access.data()).second, len);
        EXPECT_EQ(random_access, direct);
    }
}

TEST(CollateChar8, IteratorsOnBothSidesProduceTheSameKey)
{
    const collate<char8_t> obj = facet_for(kGerman);
    for (const std::u8string& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::u8string     direct(len, u8'\xFF');
        obj.transform(s.data(), s.data() + s.size(), direct.data());

        std::deque<char8_t> d(s.begin(), s.end());
        std::u8string       through;
        auto [it, n] = obj.transform(d.begin(), d.end(), std::back_inserter(through));
        EXPECT_EQ(n, len);
        EXPECT_EQ(through, direct);
    }
}

// mx_len is a hard cap on characters written, checked both before a segment and
// before the separator that follows it.  In "C.UTF-8" the key is the input, so the
// truncation point is visible: 3 stops right after the separator, 4 keeps one
// character of the second segment.
TEST(CollateChar8, AMaximumLengthTruncatesTheKey)
{
    const collate<char8_t> obj   = facet_for(kPlain);
    const std::u8string    input(u8"ab\0cd", 5);
    const char8_t*         lo = input.data();
    const char8_t*         hi = input.data() + input.size();

    for (std::size_t mx = 1; mx <= input.size(); ++mx)
    {
        SCOPED_TRACE(mx);
        const std::u8string expected = input.substr(0, mx);

        std::u8string a(mx, u8'\xFF');
        EXPECT_EQ(obj.transform(lo, hi, a.data(), mx).second, mx);
        EXPECT_EQ(a, expected);

        std::list<char8_t> l(input.begin(), input.end());
        std::u8string      b(mx, u8'\xFF');
        EXPECT_EQ(obj.transform(l.begin(), l.end(), b.data(), mx).second, mx);
        EXPECT_EQ(b, expected);

        std::u8string c;
        EXPECT_EQ(obj.transform(lo, hi, std::back_inserter(c), mx).second, mx);
        EXPECT_EQ(c, expected);

        std::u8string d;
        EXPECT_EQ(obj.transform(l.begin(), l.end(), std::back_inserter(d), mx).second, mx);
        EXPECT_EQ(d, expected);
    }
}

// The staging buffers start at reserve(64), so anything past that grows them.
// 128 KiB in one segment forces many growth rounds and a key far too large for
// any small-buffer optimisation; in "C.UTF-8" the result is still exactly the input.
TEST(CollateChar8, ALargeInputIsTransformedInOnePiece)
{
    const collate<char8_t> obj = facet_for(kPlain);
    const std::u8string    input(std::size_t{1} << 17, u8'a');

    EXPECT_EQ(obj.transform_length(input.data(), input.data() + input.size()), input.size());

    std::u8string key(input.size(), u8'\xFF');
    auto [it, n] = obj.transform(input.data(), input.data() + input.size(), key.data());
    EXPECT_EQ(n, input.size());
    EXPECT_EQ(key, input);
}

// The same amount of work spread over many segments instead of one: every
// iteration reuses the buffers the previous one grew, and the separators have to
// come out in the right places.
TEST(CollateChar8, ManySegmentsAreTransformedInOrder)
{
    const collate<char8_t> obj      = facet_for(kPlain);
    const std::size_t      segments = 5000;
    std::u8string          input;
    for (std::size_t i = 0; i < segments; ++i)
        input += std::u8string(u8"ab\0", 3);

    EXPECT_EQ(obj.transform_length(input.data(), input.data() + input.size()), input.size());

    std::u8string key;
    auto [it, n] = obj.transform(input.data(), input.data() + input.size(), std::back_inserter(key));
    EXPECT_EQ(n, input.size());
    EXPECT_EQ(key, input);
}

// collate_conf<char8_t> probes the locale's CTYPE codeset with mbrtoc32 before
// accepting it: the narrow strcoll it delegates to reads its argument as bytes,
// and those bytes only carry the intended characters when the codeset is UTF-8.
TEST(CollateChar8, ANonUtf8LocaleIsRejected)
{
    for (const char* name : {"C", "POSIX"})
    {
        SCOPED_TRACE(name);
        EXPECT_THROW(collate_conf<char8_t>{name}, cvt_error);
    }
    EXPECT_NO_THROW(collate_conf<char8_t>{kPlain});
}
