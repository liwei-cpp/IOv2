/**
 * The same collation contract as test_collate_char.cpp, one level up: for
 * wchar_t the segments go to wcscoll and wcsxfrm rather than wcscoll and
 * wcsxfrm, and a segment is a run of wchar_t values rather than of bytes.  An
 * umlaut is therefore one code unit here, not the two UTF-8 bytes the narrow
 * file has to spell out, which is the whole reason both instantiations are
 * worth testing.
 *
 * "C" keeps wcsxfrm an identity, so a key is its own input and the
 * expectations can be read off the literals; de_DE.UTF-8 is where collation
 * order and code-unit order disagree.
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

    collate<wchar_t> facet_for(const char* loc)
    {
        return collate<wchar_t>(std::make_shared<collate_conf<wchar_t>>(loc));
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

    // Every fixture is an explicit range over a std::wstring, because what these
    // tests are about is the '\0' inside a range -- a C string cannot carry one.
    int compare_ptr(const collate<wchar_t>& obj, const std::wstring& lhs, const std::wstring& rhs)
    {
        return order(obj.compare(lhs.data(), lhs.data() + lhs.size(),
                                 rhs.data(), rhs.data() + rhs.size()));
    }

    // U+00E4 and U+00C4 as single wchar_t values, which is what separates this
    // instantiation from the narrow one: there the same two characters are four
    // bytes across two segments' worth of code units.
    const std::wstring a_umlaut = L"\u00E4";   // ä
    const std::wstring A_umlaut = L"\u00C4";   // Ä

    const std::wstring kCases[] = {
        L"",  L"a",  L"b",  L"ab",  L"abc",
        std::wstring(L"a\0", 2),
        std::wstring(L"a\0a", 3),
        std::wstring(L"a\0b", 3),
        std::wstring(L"b\0a", 3),
        std::wstring(L"ab\0cd", 5),
        std::wstring(L"\0", 1),
        std::wstring(L"\0\0", 2),
        a_umlaut, A_umlaut, L"B",
    };
}

TEST(CollateWchar, ANullConfigurationIsRejected)
{
    std::shared_ptr<collate_conf<wchar_t>> empty;
    EXPECT_THROW(collate<wchar_t>{empty}, std::runtime_error);
}

TEST(CollateWchar, EqualRangesCompareEqual)
{
    const collate<wchar_t> obj = facet_for(kPlain);
    for (const std::wstring& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        EXPECT_EQ(compare_ptr(obj, s, s), 0);
    }
}

TEST(CollateWchar, AProperPrefixSortsFirst)
{
    const collate<wchar_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, L"abc", L"abcd"), -1);
    EXPECT_EQ(compare_ptr(obj, L"abcd", L"abc"), 1);
}

TEST(CollateWchar, AnEmptyRangeSortsBeforeANonEmptyOne)
{
    const collate<wchar_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, L"", L"a"), -1);
    EXPECT_EQ(compare_ptr(obj, L"a", L""), 1);
    EXPECT_EQ(compare_ptr(obj, L"", L""), 0);
}

// 'a' is 0x61 and 'B' is 0x42, so code-unit order and dictionary order disagree here.
// The "C" locale has to take the code-unit order; the German case below takes the
// other one on the same pair.
TEST(CollateWchar, TheCLocaleComparesByCodeUnitValue)
{
    const collate<wchar_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, L"a", L"B"), 1);
    EXPECT_EQ(compare_ptr(obj, a_umlaut, L"b"), 1);
    EXPECT_EQ(compare_ptr(obj, A_umlaut, L"B"), 1);
}

TEST(CollateWchar, TheFirstUnequalSegmentDecides)
{
    const collate<wchar_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, std::wstring(L"a\0b", 3), std::wstring(L"a\0c", 3)), -1);
    EXPECT_EQ(compare_ptr(obj, std::wstring(L"a\0b", 3), std::wstring(L"a\0a", 3)), 1);

    // The second segment loses its say once the first one has spoken.
    EXPECT_EQ(compare_ptr(obj, std::wstring(L"b\0a", 3), std::wstring(L"a\0z", 3)), 1);
}

TEST(CollateWchar, SegmentsAfterAnEqualPrefixMakeTheLongerRangeGreater)
{
    const collate<wchar_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, std::wstring(L"a\0b", 3), std::wstring(L"a\0", 2)), 1);
    EXPECT_EQ(compare_ptr(obj, std::wstring(L"a\0", 2), std::wstring(L"a\0b", 3)), -1);
}

// An embedded '\0' is a separator, so a range that contains one ends in an empty
// segment the other range does not have.  That is the only thing separating
// these two ranges, and the terminated one is the greater.
TEST(CollateWchar, AnExplicitTerminatorSortsAfterAMissingOne)
{
    const collate<wchar_t> obj = facet_for(kPlain);
    EXPECT_EQ(compare_ptr(obj, std::wstring(L"a\0", 2), L"a"), 1);
    EXPECT_EQ(compare_ptr(obj, L"a", std::wstring(L"a\0", 2)), -1);
    EXPECT_EQ(compare_ptr(obj, std::wstring(L"a\0", 2), std::wstring(L"a\0", 2)), 0);
}

TEST(CollateWchar, GermanCollationIgnoresCaseAtThePrimaryLevel)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    EXPECT_EQ(compare_ptr(obj, L"a", L"B"), -1);
    EXPECT_EQ(compare_ptr(obj, L"B", L"a"), 1);
}

TEST(CollateWchar, GermanCollationPlacesAnUmlautWithItsBaseLetter)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    EXPECT_EQ(compare_ptr(obj, a_umlaut, L"b"), -1);
    EXPECT_EQ(compare_ptr(obj, A_umlaut, L"B"), -1);
}

// Same primary weight as 'a', so the tie is broken one level down and the
// umlaut is the greater of the two.  Without this the case above would also be
// satisfied by a locale that simply dropped the diacritic.
TEST(CollateWchar, GermanCollationSeparatesAnUmlautFromItsBaseLetterAtTheSecondaryLevel)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    EXPECT_EQ(compare_ptr(obj, a_umlaut, L"a"), 1);
    EXPECT_EQ(compare_ptr(obj, L"a", a_umlaut), -1);
}

// The four compare() overloads reach the segmenting loop by three different
// routes -- std::find over pointers, data_to_vec over iterators, and one of each
// -- so they are only interchangeable if they agree on every pair.
TEST(CollateWchar, ListIteratorsCompareLikePointers)
{
    for (const char* loc : {kPlain, kGerman})
    {
        const collate<wchar_t> obj = facet_for(loc);
        for (const std::wstring& lhs : kCases)
            for (const std::wstring& rhs : kCases)
            {
                SCOPED_TRACE(std::string(loc) + " " + trace(lhs, rhs));
                std::list<wchar_t> l(lhs.begin(), lhs.end());
                std::list<wchar_t> r(rhs.begin(), rhs.end());
                EXPECT_EQ(order(obj.compare(l.begin(), l.end(), r.begin(), r.end())),
                          compare_ptr(obj, lhs, rhs));
            }
    }
}

TEST(CollateWchar, DequeIteratorsCompareLikePointers)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    for (const std::wstring& lhs : kCases)
        for (const std::wstring& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            std::deque<wchar_t> l(lhs.begin(), lhs.end());
            std::deque<wchar_t> r(rhs.begin(), rhs.end());
            EXPECT_EQ(order(obj.compare(l.begin(), l.end(), r.begin(), r.end())),
                      compare_ptr(obj, lhs, rhs));
        }
}

TEST(CollateWchar, APointerAndAnIteratorCompareLikeTwoPointers)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    for (const std::wstring& lhs : kCases)
        for (const std::wstring& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            std::list<wchar_t>  l(lhs.begin(), lhs.end());
            std::list<wchar_t>  r(rhs.begin(), rhs.end());
            std::deque<wchar_t> dl(lhs.begin(), lhs.end());
            std::deque<wchar_t> dr(rhs.begin(), rhs.end());
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
TEST(CollateWchar, SwappingTheArgumentsReversesTheResult)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    for (const std::wstring& lhs : kCases)
        for (const std::wstring& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            EXPECT_EQ(compare_ptr(obj, lhs, rhs), -compare_ptr(obj, rhs, lhs));
        }
}

// wcsxfrm is the identity in "C", and transform_length adds one per separator,
// so the key of an n-character range is n characters long however the '\0's fall.
TEST(CollateWchar, TheCLocaleKeyIsAsLongAsTheInput)
{
    const collate<wchar_t> obj = facet_for(kPlain);
    for (const std::wstring& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        EXPECT_EQ(obj.transform_length(s.data(), s.data() + s.size()), s.size());
    }
}

TEST(CollateWchar, TransformLengthCountsEachSegmentSeparator)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    const std::wstring     head(L"a\0", 2);
    const std::wstring     tail(L"b");
    const std::wstring     both(L"a\0b", 3);

    EXPECT_EQ(obj.transform_length(both.data(), both.data() + both.size()),
              obj.transform_length(head.data(), head.data() + head.size()) +
              obj.transform_length(tail.data(), tail.data() + tail.size()));
}

TEST(CollateWchar, TransformLengthIsTheSameThroughIterators)
{
    for (const char* loc : {kPlain, kGerman})
    {
        const collate<wchar_t> obj = facet_for(loc);
        for (const std::wstring& s : kCases)
        {
            SCOPED_TRACE(std::string(loc) + " " + ::testing::PrintToString(s));
            const std::size_t   expected = obj.transform_length(s.data(), s.data() + s.size());
            std::list<wchar_t>  l(s.begin(), s.end());
            std::deque<wchar_t> d(s.begin(), s.end());
            EXPECT_EQ(obj.transform_length(l.begin(), l.end()), expected);
            EXPECT_EQ(obj.transform_length(d.begin(), d.end()), expected);
        }
    }
}

TEST(CollateWchar, TheCLocaleKeyIsTheInputItself)
{
    const collate<wchar_t> obj = facet_for(kPlain);
    for (const std::wstring& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        std::wstring key(s.size(), L'\xFF');
        auto [it, n] = obj.transform(s.data(), s.data() + s.size(), key.data());
        EXPECT_EQ(n, s.size());
        EXPECT_EQ(it, key.data() + s.size());
        EXPECT_EQ(key, s);
    }
}

TEST(CollateWchar, TransformWritesExactlyTransformLengthCharacters)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    for (const std::wstring& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::wstring      key(len, L'\xFF');
        auto [it, n] = obj.transform(s.data(), s.data() + s.size(), key.data());
        EXPECT_EQ(n, len);
        EXPECT_EQ(it, key.data() + len);
    }
}

// What transform() is for: comparing two keys byte by byte has to give the same
// answer as calling compare() on the originals.  Byte order alone does not, or
// the facet would have nothing to do.
TEST(CollateWchar, KeysOrderLikeCompare)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    for (const std::wstring& lhs : kCases)
        for (const std::wstring& rhs : kCases)
        {
            SCOPED_TRACE(trace(lhs, rhs));
            std::wstring kl, kr;
            obj.transform(lhs.data(), lhs.data() + lhs.size(), std::back_inserter(kl));
            obj.transform(rhs.data(), rhs.data() + rhs.size(), std::back_inserter(kr));
            EXPECT_EQ(order(kl <=> kr), compare_ptr(obj, lhs, rhs));
        }
}

// A terminated segment contributes its weights and a separator; an unterminated
// tail contributes only its weights.  So splitting a range at a '\0' splits its
// key at the same place.
TEST(CollateWchar, AKeyIsTheConcatenationOfItsSegmentKeys)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    const std::wstring     head(L"Zange\0", 6);
    const std::wstring     tail(A_umlaut + L"pfel");
    const std::wstring     both = head + tail;

    std::wstring k_head, k_tail, k_both;
    obj.transform(head.data(), head.data() + head.size(), std::back_inserter(k_head));
    obj.transform(tail.data(), tail.data() + tail.size(), std::back_inserter(k_tail));
    obj.transform(both.data(), both.data() + both.size(), std::back_inserter(k_both));

    EXPECT_EQ(k_head.size() + k_tail.size(), k_both.size());
    EXPECT_EQ(k_head + k_tail, k_both);
    EXPECT_EQ(obj.transform_length(both.data(), both.data() + both.size()), k_both.size());
}

TEST(CollateWchar, AnOutputIteratorReceivesTheSameKey)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    for (const std::wstring& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::wstring      direct(len, L'\xFF');
        obj.transform(s.data(), s.data() + s.size(), direct.data());

        std::wstring through;
        auto [it, n] = obj.transform(s.data(), s.data() + s.size(), std::back_inserter(through));
        EXPECT_EQ(n, len);
        EXPECT_EQ(through, direct);
    }
}

TEST(CollateWchar, AnInputIteratorProducesTheSameKey)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    for (const std::wstring& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::wstring      direct(len, L'\xFF');
        obj.transform(s.data(), s.data() + s.size(), direct.data());

        std::list<wchar_t> l(s.begin(), s.end());
        std::wstring       through(len, L'\xFF');
        auto [it, n] = obj.transform(l.begin(), l.end(), through.data());
        EXPECT_EQ(n, len);
        EXPECT_EQ(it, through.data() + len);
        EXPECT_EQ(through, direct);

        std::deque<wchar_t> d(s.begin(), s.end());
        std::wstring        random_access(len, L'\xFF');
        EXPECT_EQ(obj.transform(d.begin(), d.end(), random_access.data()).second, len);
        EXPECT_EQ(random_access, direct);
    }
}

TEST(CollateWchar, IteratorsOnBothSidesProduceTheSameKey)
{
    const collate<wchar_t> obj = facet_for(kGerman);
    for (const std::wstring& s : kCases)
    {
        SCOPED_TRACE(::testing::PrintToString(s));
        const std::size_t len = obj.transform_length(s.data(), s.data() + s.size());
        std::wstring      direct(len, L'\xFF');
        obj.transform(s.data(), s.data() + s.size(), direct.data());

        std::deque<wchar_t> d(s.begin(), s.end());
        std::wstring        through;
        auto [it, n] = obj.transform(d.begin(), d.end(), std::back_inserter(through));
        EXPECT_EQ(n, len);
        EXPECT_EQ(through, direct);
    }
}

// mx_len is a hard cap on characters written, checked both before a segment and
// before the separator that follows it.  In "C" the key is the input, so the
// truncation point is visible: 3 stops right after the separator, 4 keeps one
// character of the second segment.
TEST(CollateWchar, AMaximumLengthTruncatesTheKey)
{
    const collate<wchar_t> obj   = facet_for(kPlain);
    const std::wstring     input(L"ab\0cd", 5);
    const wchar_t*         lo = input.data();
    const wchar_t*         hi = input.data() + input.size();

    for (std::size_t mx = 1; mx <= input.size(); ++mx)
    {
        SCOPED_TRACE(mx);
        const std::wstring expected = input.substr(0, mx);

        std::wstring a(mx, L'\xFF');
        EXPECT_EQ(obj.transform(lo, hi, a.data(), mx).second, mx);
        EXPECT_EQ(a, expected);

        std::list<wchar_t> l(input.begin(), input.end());
        std::wstring       b(mx, L'\xFF');
        EXPECT_EQ(obj.transform(l.begin(), l.end(), b.data(), mx).second, mx);
        EXPECT_EQ(b, expected);

        std::wstring c;
        EXPECT_EQ(obj.transform(lo, hi, std::back_inserter(c), mx).second, mx);
        EXPECT_EQ(c, expected);

        std::wstring d;
        EXPECT_EQ(obj.transform(l.begin(), l.end(), std::back_inserter(d), mx).second, mx);
        EXPECT_EQ(d, expected);
    }
}

// The staging buffers start at reserve(64), so anything past that grows them.
// 128 KiB in one segment forces many growth rounds and a key far too large for
// any small-buffer optimisation; in "C" the result is still exactly the input.
TEST(CollateWchar, ALargeInputIsTransformedInOnePiece)
{
    const collate<wchar_t> obj = facet_for(kPlain);
    const std::wstring     input(std::size_t{1} << 17, L'a');

    EXPECT_EQ(obj.transform_length(input.data(), input.data() + input.size()), input.size());

    std::wstring key(input.size(), L'\xFF');
    auto [it, n] = obj.transform(input.data(), input.data() + input.size(), key.data());
    EXPECT_EQ(n, input.size());
    EXPECT_EQ(key, input);
}

// The same amount of work spread over many segments instead of one: every
// iteration reuses the buffers the previous one grew, and the separators have to
// come out in the right places.
TEST(CollateWchar, ManySegmentsAreTransformedInOrder)
{
    const collate<wchar_t> obj      = facet_for(kPlain);
    const std::size_t      segments = 5000;
    std::wstring           input;
    for (std::size_t i = 0; i < segments; ++i)
        input += std::wstring(L"ab\0", 3);

    EXPECT_EQ(obj.transform_length(input.data(), input.data() + input.size()), input.size());

    std::wstring key;
    auto [it, n] = obj.transform(input.data(), input.data() + input.size(), std::back_inserter(key));
    EXPECT_EQ(n, input.size());
    EXPECT_EQ(key, input);
}
