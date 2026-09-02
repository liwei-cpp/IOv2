// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * IOv2::ctype<wchar_t>.
 *
 * The wide facet is the one with two answers to give for every question.  It
 * snapshots the configuration into tables covering [0, 256) at construction and
 * serves those directly; anything above that goes back to the ctype_conf, and so
 * to iswctype_l, on every call.  Most of what is worth checking here is that the
 * seam is invisible: the same question has to get the same answer on either side
 * of it, and the table has to agree with the configuration it was taken from.
 *
 * The rest are the properties [locale.ctype.virtuals] states outright -- is_any
 * as a masked is(), the two scans as searches over is_any, the bulk operations
 * as repetitions of the single-character ones, and narrow() reporting through an
 * optional what the two-argument overload reports through the caller's default.
 */
#include <IOv2/facet/ctype.h>
#include <IOv2/facet/ctype_details.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <iterator>
#include <locale>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

using namespace IOv2;

namespace
{
    using CT   = base_ft<ctype>;
    using mask = CT::mask;

    // The width of the snapshot the wide facet takes at construction.  Below it
    // every query is a table lookup; at or above it every query goes to the
    // configuration.
    constexpr int kCachedValues = 256;

    std::shared_ptr<ctype_conf<wchar_t>> conf_for(const char* loc)
    {
        return std::make_shared<ctype_conf<wchar_t>>(loc);
    }

    // std::ctype_byname's constructor is protected against direct use, so a
    // trivial derived class is the only way to get one outside a std::locale.
    struct ref_ctype : std::ctype_byname<wchar_t>
    {
        explicit ref_ctype(const char* name) : std::ctype_byname<wchar_t>(name) {}
    };

    mask to_iov2(std::ctype_base::mask in)
    {
        mask res = static_cast<mask>(0);
        auto copy_bit = [&](std::ctype_base::mask from, mask to)
        { if ((in & from) == from) res |= to; };

        copy_bit(std::ctype_base::upper,  CT::upper);
        copy_bit(std::ctype_base::lower,  CT::lower);
        copy_bit(std::ctype_base::alpha,  CT::alpha);
        copy_bit(std::ctype_base::digit,  CT::digit);
        copy_bit(std::ctype_base::xdigit, CT::xdigit);
        copy_bit(std::ctype_base::space,  CT::space);
        copy_bit(std::ctype_base::print,  CT::print);
        copy_bit(std::ctype_base::cntrl,  CT::cntrl);
        copy_bit(std::ctype_base::punct,  CT::punct);
        return res;
    }

    struct named_mask { const char* name; mask value; };
    const named_mask kCategories[] = {
        {"upper", CT::upper}, {"lower", CT::lower}, {"alpha", CT::alpha},
        {"digit", CT::digit}, {"xdigit", CT::xdigit}, {"space", CT::space},
        {"print", CT::print}, {"cntrl", CT::cntrl}, {"punct", CT::punct},
        {"alnum", CT::alnum}, {"graph", CT::graph},
    };

    // Two straddling the boundary, then a letter, an ideograph and a full-width
    // pair -- enough shapes that a table indexed past its end would be caught.
    const wchar_t kAboveTheCache[] = {
        L'ÿ', L'Ā', L'ā', L'Ә', L'你', L'Ａ', L'ａ',
    };

    std::wstring every_cached_value()
    {
        std::wstring out(kCachedValues, L'\0');
        for (int i = 0; i < kCachedValues; ++i) out[i] = static_cast<wchar_t>(i);
        return out;
    }
}

TEST(CtypeWchar, TheCharacterTypeIsWchar)
{
    static_assert(std::is_same_v<ctype<wchar_t>::char_type, wchar_t>);
    static_assert(std::is_same_v<ctype<wchar_t>::mask, mask>);
}

// A locale holds one facet per id, so the wide configuration has to carry an id
// of its own or it would displace the narrow one.
TEST(CtypeWchar, TheWideConfigurationHasItsOwnFacetId)
{
    EXPECT_NE(ctype_conf<wchar_t>::id(), ctype_conf<char>::id());
}

// The snapshot is only sound if it says what the configuration says.  Reading
// both for the same character is the only way to see the table itself rather
// than the answer it happens to agree with.
TEST(CtypeWchar, TheCachedTableAgreesWithTheConfiguration)
{
    auto              conf = conf_for("en_US.UTF-8");
    const ctype<wchar_t> obj(conf);

    for (int i = 0; i < kCachedValues; ++i)
    {
        SCOPED_TRACE(i);
        const auto c = static_cast<wchar_t>(i);
        EXPECT_EQ(obj.is(c), conf->is(c));
        EXPECT_EQ(obj.toupper(c), conf->toupper(c));
        EXPECT_EQ(obj.tolower(c), conf->tolower(c));
        EXPECT_EQ(obj.narrow(c), conf->narrow(c));
    }
}

TEST(CtypeWchar, AboveTheCacheTheConfigurationAnswersDirectly)
{
    auto                 conf = conf_for("en_US.UTF-8");
    const ctype<wchar_t> obj(conf);

    for (wchar_t c : kAboveTheCache)
    {
        SCOPED_TRACE(static_cast<int>(c));
        EXPECT_EQ(obj.is(c), conf->is(c));
        EXPECT_EQ(obj.toupper(c), conf->toupper(c));
        EXPECT_EQ(obj.tolower(c), conf->tolower(c));
        EXPECT_EQ(obj.narrow(c), conf->narrow(c));
    }
}

TEST(CtypeWchar, ClassificationMatchesTheStandardFacet)
{
    const ctype<wchar_t> obj(conf_for("en_US.UTF-8"));
    const ref_ctype      ref("en_US.UTF-8");
    const std::wstring   all = every_cached_value();

    std::vector<std::ctype_base::mask> expected(kCachedValues);
    ref.is(all.data(), all.data() + kCachedValues, expected.data());

    for (int i = 0; i < kCachedValues; ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(obj.is(all[i]), to_iov2(expected[i]));
    }
}

TEST(CtypeWchar, CaseMappingMatchesTheStandardFacet)
{
    const ctype<wchar_t> obj(conf_for("en_US.UTF-8"));
    const ref_ctype      ref("en_US.UTF-8");

    for (int i = 0; i < kCachedValues; ++i)
    {
        SCOPED_TRACE(i);
        const auto c = static_cast<wchar_t>(i);
        EXPECT_EQ(obj.toupper(c), ref.toupper(c));
        EXPECT_EQ(obj.tolower(c), ref.tolower(c));
    }
    for (wchar_t c : kAboveTheCache)
    {
        SCOPED_TRACE(static_cast<int>(c));
        EXPECT_EQ(obj.toupper(c), ref.toupper(c));
        EXPECT_EQ(obj.tolower(c), ref.tolower(c));
    }
}

TEST(CtypeWchar, WidenMatchesTheStandardFacet)
{
    const ctype<wchar_t> obj(conf_for("en_US.UTF-8"));
    const ref_ctype      ref("en_US.UTF-8");

    for (int i = 0; i < kCachedValues; ++i)
    {
        SCOPED_TRACE(i);
        const auto c = static_cast<char>(i);
        EXPECT_EQ(obj.widen(c), ref.widen(c));
    }
}

// The five bulk operations exist only to repeat the single-character one over a
// range, so each is checked against its own scalar counterpart.
TEST(CtypeWchar, IsSeqClassifiesEveryCharacterOfTheRange)
{
    const ctype<wchar_t> obj(conf_for("en_US.UTF-8"));
    const std::wstring   all = every_cached_value();

    std::vector<mask> out(kCachedValues);
    EXPECT_EQ(obj.is_seq(all.data(), all.data() + kCachedValues, out.data()),
              out.data() + kCachedValues);
    for (int i = 0; i < kCachedValues; ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(out[i], obj.is(all[i]));
    }
}

TEST(CtypeWchar, CaseMappingSeqMapsEveryCharacterOfTheRange)
{
    const ctype<wchar_t> obj(conf_for("en_US.UTF-8"));
    const std::wstring   all = every_cached_value();

    std::wstring upper(kCachedValues, L'\0');
    std::wstring lower(kCachedValues, L'\0');
    EXPECT_EQ(obj.toupper_seq(all.data(), all.data() + kCachedValues, upper.data()),
              upper.data() + kCachedValues);
    EXPECT_EQ(obj.tolower_seq(all.data(), all.data() + kCachedValues, lower.data()),
              lower.data() + kCachedValues);

    for (int i = 0; i < kCachedValues; ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(upper[i], obj.toupper(all[i]));
        EXPECT_EQ(lower[i], obj.tolower(all[i]));
    }
}

TEST(CtypeWchar, WidenAndNarrowSeqMapEveryCharacterOfTheRange)
{
    const ctype<wchar_t> obj(conf_for("en_US.UTF-8"));
    const std::wstring   all = every_cached_value();

    std::string source(kCachedValues, '\0');
    for (int i = 0; i < kCachedValues; ++i) source[i] = static_cast<char>(i);

    std::wstring widened(kCachedValues, L'\0');
    std::string  narrowed(kCachedValues, '\0');
    EXPECT_EQ(obj.widen_seq(source.data(), source.data() + kCachedValues, widened.data()),
              widened.data() + kCachedValues);
    EXPECT_EQ(obj.narrow_seq(all.data(), all.data() + kCachedValues, '?', narrowed.data()),
              narrowed.data() + kCachedValues);

    for (int i = 0; i < kCachedValues; ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(widened[i], obj.widen(source[i]));
        EXPECT_EQ(narrowed[i], obj.narrow(all[i], '?'));
    }
}

// narrow_seq takes an output iterator, not just a pointer, so a container that
// grows as it is written has to work as the destination.
TEST(CtypeWchar, NarrowSeqWritesThroughAnOutputIterator)
{
    const ctype<wchar_t> obj(conf_for("en_US.UTF-8"));
    const std::wstring   wide = L"wibble";

    std::vector<char> out;
    obj.narrow_seq(wide.begin(), wide.end(), '?', std::back_inserter(out));
    ASSERT_EQ(out.size(), wide.size());
    for (std::size_t i = 0; i < wide.size(); ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(out[i], obj.narrow(wide[i], '?'));
    }
}

TEST(CtypeWchar, AnEmptyRangeWritesNothing)
{
    const ctype<wchar_t> obj(conf_for("en_US.UTF-8"));
    const wchar_t        one = L'a';
    const char           src = 'a';

    mask    m = static_cast<mask>(0);
    wchar_t w = L'\0';
    char    c = '\0';
    EXPECT_EQ(obj.is_seq(&one, &one, &m), &m);
    EXPECT_EQ(obj.toupper_seq(&one, &one, &w), &w);
    EXPECT_EQ(obj.tolower_seq(&one, &one, &w), &w);
    EXPECT_EQ(obj.widen_seq(&src, &src, &w), &w);
    EXPECT_EQ(obj.narrow_seq(&one, &one, '?', &c), &c);
}

// A character with no single-byte form cannot be narrowed at all.  The one
// overload says so by returning nothing; the other substitutes what the caller
// asked for.  They have to disagree about nothing else.
TEST(CtypeWchar, AnUnrepresentableCharacterNarrowsToTheCallersDefault)
{
    const ctype<wchar_t> obj(conf_for("C"));

    EXPECT_EQ(obj.narrow(L'你'), std::nullopt);
    EXPECT_EQ(obj.narrow(L'你', '?'), '?');
    EXPECT_EQ(obj.narrow(L'你', '!'), '!');

    ASSERT_TRUE(obj.narrow(L'w').has_value());
    EXPECT_EQ(*obj.narrow(L'w'), 'w');
    EXPECT_EQ(obj.narrow(L'w', '?'), 'w');
}

TEST(CtypeWchar, TheDefaultOnlyReplacesWhatCannotBeNarrowed)
{
    const ctype<wchar_t> obj(conf_for("C"));
    const std::wstring   wide = L"wibbleӘkibble";

    std::string out;
    obj.narrow_seq(wide.begin(), wide.end(), '?', std::back_inserter(out));
    EXPECT_EQ(out, "wibble?kibble");
}

// The two locales differ in what they call a letter, and U+00E4 is where: it is
// alphabetic in German and nothing at all in "C".  Naming the character says
// more than observing that two tables are unequal somewhere.
TEST(CtypeWchar, TheLocaleDecidesWhatCountsAsALetter)
{
    const ctype<wchar_t> plain(conf_for("C"));
    const ctype<wchar_t> german(conf_for("de_DE.UTF-8"));

    EXPECT_FALSE(plain.is_any(CT::alpha, L'ä'));
    EXPECT_TRUE(german.is_any(CT::alpha, L'ä'));
    EXPECT_TRUE(german.is_any(CT::lower, L'ä'));
    EXPECT_EQ(german.toupper(L'ä'), L'Ä');

    // Both agree about ASCII, so a difference there would be the surprise.
    for (wchar_t c = L'a'; c <= L'z'; ++c)
    {
        SCOPED_TRACE(static_cast<int>(c));
        EXPECT_EQ(plain.is(c), german.is(c));
    }
}

TEST(CtypeWchar, IsAnyIsTheClassificationMaskedByTheCategory)
{
    const ctype<wchar_t> obj(conf_for("en_US.UTF-8"));
    for (int i = 0; i < kCachedValues; ++i)
        for (const named_mask& cat : kCategories)
        {
            SCOPED_TRACE(std::string(cat.name) + " " + std::to_string(i));
            const auto c = static_cast<wchar_t>(i);
            EXPECT_EQ(obj.is_any(cat.value, c), (obj.is(c) & cat.value) != 0);
        }
}

TEST(CtypeWchar, AskingAboutAUnionOfCategoriesUnionsTheAnswers)
{
    const ctype<wchar_t> obj(conf_for("en_US.UTF-8"));
    for (int i = 0; i < kCachedValues; ++i)
        for (const named_mask& lhs : kCategories)
            for (const named_mask& rhs : kCategories)
            {
                SCOPED_TRACE(std::string(lhs.name) + "|" + rhs.name + " " + std::to_string(i));
                const auto c = static_cast<wchar_t>(i);
                EXPECT_EQ(obj.is_any(lhs.value | rhs.value, c),
                          obj.is_any(lhs.value, c) || obj.is_any(rhs.value, c));
            }
}

TEST(CtypeWchar, ScanFindsTheFirstMatchAndTheFirstNonMatch)
{
    const ctype<wchar_t> obj(conf_for("C"));
    const std::wstring   data = L"  9x";
    const wchar_t*       beg  = data.data();
    const wchar_t*       end  = data.data() + data.size();

    EXPECT_EQ(obj.scan_is_any(CT::digit, beg, end), beg + 2);
    EXPECT_EQ(obj.scan_is_any(CT::alpha, beg, end), beg + 3);
    EXPECT_EQ(obj.scan_is_any(CT::cntrl, beg, end), end);
    EXPECT_EQ(obj.scan_not_any(CT::space, beg, end), beg + 2);
    EXPECT_EQ(obj.scan_not_any(CT::digit, beg, end), beg);
    EXPECT_EQ(obj.scan_not_any(CT::print, beg, end), end);
}

TEST(CtypeWchar, ScanningAnEmptyRangeReturnsItsEnd)
{
    const ctype<wchar_t> obj(conf_for("C"));
    const wchar_t        one = L'a';
    EXPECT_EQ(obj.scan_is_any(CT::alpha, &one, &one), &one);
    EXPECT_EQ(obj.scan_not_any(CT::alpha, &one, &one), &one);
}

// On a run of one repeated character each scan can only answer the front of the
// range or its end, and which one has to be exactly what is_any says.
TEST(CtypeWchar, ScanAgreesWithIsAnyForEveryCharacterAndCategory)
{
    const ctype<wchar_t> obj(conf_for("C"));
    for (int i = 0; i < kCachedValues; ++i)
    {
        const std::wstring run(5, static_cast<wchar_t>(i));
        const wchar_t*     beg = run.data();
        const wchar_t*     end = run.data() + run.size();

        for (const named_mask& cat : kCategories)
        {
            SCOPED_TRACE(std::string(cat.name) + " " + std::to_string(i));
            const bool member = obj.is_any(cat.value, run[0]);
            EXPECT_EQ(obj.scan_is_any(cat.value, beg, end), member ? beg : end);
            EXPECT_EQ(obj.scan_not_any(cat.value, beg, end), member ? end : beg);
        }
    }
}

// Case mapping is idempotent: a character that has already been mapped is a
// fixed point of the same mapping.  The stronger claim that the two undo each
// other is false in Unicode -- U+00B5 MICRO SIGN uppercases to GREEK CAPITAL MU,
// which lowercases to GREEK SMALL MU rather than back to the micro sign.
TEST(CtypeWchar, CaseMappingIsIdempotent)
{
    const ctype<wchar_t> obj(conf_for("en_US.UTF-8"));
    for (int i = 0; i < kCachedValues; ++i)
    {
        SCOPED_TRACE(i);
        const auto c = static_cast<wchar_t>(i);
        EXPECT_EQ(obj.toupper(obj.toupper(c)), obj.toupper(c));
        EXPECT_EQ(obj.tolower(obj.tolower(c)), obj.tolower(c));
    }
}

// widen and narrow are inverses over the bytes the locale actually has
// characters for.  In a UTF-8 locale that is ASCII and nothing else: a byte
// above 0x7F is a fragment of a character rather than one, so widening it
// produces something narrow() then declines to map back.
TEST(CtypeWchar, NarrowUndoesWidenOverAscii)
{
    const ctype<wchar_t> obj(conf_for("en_US.UTF-8"));
    for (int i = 0; i < 128; ++i)
    {
        SCOPED_TRACE(i);
        const auto c = static_cast<char>(i);
        EXPECT_EQ(obj.narrow(obj.widen(c), '?'), c);
    }
    for (int i = 128; i < kCachedValues; ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(obj.narrow(obj.widen(static_cast<char>(i))), std::nullopt);
    }
}

// Full-width Latin letters sit at U+FF21 and U+FF41, far above the snapshot, so
// this is the case mapping reaching the configuration rather than a table.
TEST(CtypeWchar, FullWidthLettersMapCaseThroughTheConfiguration)
{
    const ctype<wchar_t> obj(conf_for("zh_CN.UTF-8"));
    EXPECT_EQ(obj.tolower(L'Ａ'), L'ａ');
    EXPECT_EQ(obj.toupper(L'ａ'), L'Ａ');
    EXPECT_TRUE(obj.is_any(CT::alpha, L'Ａ'));
    EXPECT_TRUE(obj.is_any(CT::upper, L'Ａ'));
    EXPECT_TRUE(obj.is_any(CT::lower, L'ａ'));
}
