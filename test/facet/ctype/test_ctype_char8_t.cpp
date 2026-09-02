// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * IOv2::ctype<char8_t>.
 *
 * char8_t shares the single-byte specialization with char, but a char8_t is a
 * UTF-8 code unit rather than a character: only the ASCII half of the byte range
 * stands for a character on its own, and the continuation bytes 0x80-0xFF stand
 * for nothing until they are assembled.  So the facet answers for the ASCII half
 * exactly as ctype<char> does, and for the rest it classifies nothing and maps
 * nothing -- which is what most of the cases here pin down.
 *
 * The remaining cases are the properties [locale.ctype.virtuals] states outright:
 * is_any as a masked is(), the two scans as searches over is_any, and the bulk
 * operations as repetitions of the single-character ones.
 */
#include <facet/ctype.h>
#include <facet/ctype_details.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using namespace IOv2;

namespace
{
    using CT   = base_ft<ctype>;
    using mask = CT::mask;

    // Every value a char8_t can take, and the point where UTF-8 stops being
    // one-byte-per-character.
    constexpr int kByteValues = 256;
    constexpr int kAsciiEnd   = 128;

    ctype<char8_t> facet_for(const char* loc)
    {
        return ctype<char8_t>(std::make_shared<ctype_conf<char8_t>>(loc));
    }

    ctype<char> narrow_facet_for(const char* loc)
    {
        return ctype<char>(std::make_shared<ctype_conf<char>>(loc));
    }

    struct named_mask { const char* name; mask value; };
    const named_mask kCategories[] = {
        {"upper", CT::upper}, {"lower", CT::lower}, {"alpha", CT::alpha},
        {"digit", CT::digit}, {"xdigit", CT::xdigit}, {"space", CT::space},
        {"print", CT::print}, {"cntrl", CT::cntrl}, {"punct", CT::punct},
        {"alnum", CT::alnum}, {"graph", CT::graph},
    };

    std::u8string every_byte()
    {
        std::u8string out(kByteValues, u8'\0');
        for (int i = 0; i < kByteValues; ++i) out[i] = static_cast<char8_t>(i);
        return out;
    }
}

TEST(CtypeChar8, TheCharacterTypeIsChar8)
{
    static_assert(std::is_same_v<ctype<char8_t>::char_type, char8_t>);
    static_assert(std::is_same_v<ctype<char8_t>::mask, mask>);
}

// On the ASCII half a code unit is a character, so the two single-byte
// instantiations have to give the same answers for the same locale.
TEST(CtypeChar8, AsciiIsClassifiedLikeTheNarrowFacet)
{
    const ctype<char8_t> obj = facet_for("en_US.UTF-8");
    const ctype<char>    ref = narrow_facet_for("en_US.UTF-8");

    for (int i = 0; i < kAsciiEnd; ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(obj.is(static_cast<char8_t>(i)), ref.is(static_cast<char>(i)));
    }
}

TEST(CtypeChar8, AsciiIsMappedLikeTheNarrowFacet)
{
    const ctype<char8_t> obj = facet_for("en_US.UTF-8");
    const ctype<char>    ref = narrow_facet_for("en_US.UTF-8");

    for (int i = 0; i < kAsciiEnd; ++i)
    {
        SCOPED_TRACE(i);
        const auto u = static_cast<char8_t>(i);
        const auto c = static_cast<char>(i);
        EXPECT_EQ(obj.toupper(u), static_cast<char8_t>(ref.toupper(c)));
        EXPECT_EQ(obj.tolower(u), static_cast<char8_t>(ref.tolower(c)));
        EXPECT_EQ(obj.widen(c), static_cast<char8_t>(ref.widen(c)));
        EXPECT_EQ(obj.narrow(u, 0), ref.narrow(c, 0));
    }
}

// Above 0x7F a char8_t is a continuation byte, not a character.  Classifying one
// would claim it means something on its own, so the facet declines: no
// categories, no case mapping, and the caller's default from narrow().
TEST(CtypeChar8, ContinuationBytesBelongToNoCategory)
{
    const ctype<char8_t> obj = facet_for("en_US.UTF-8");
    for (int i = kAsciiEnd; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(obj.is(static_cast<char8_t>(i)), static_cast<mask>(0));
    }
}

TEST(CtypeChar8, ContinuationBytesAreLeftAlone)
{
    const ctype<char8_t> obj = facet_for("en_US.UTF-8");
    for (int i = kAsciiEnd; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        const auto u = static_cast<char8_t>(i);
        EXPECT_EQ(obj.toupper(u), u);
        EXPECT_EQ(obj.tolower(u), u);
        EXPECT_EQ(obj.widen(static_cast<char>(i)), u);
        EXPECT_EQ(obj.narrow(u, 0), 0);
        EXPECT_EQ(obj.narrow(u, '?'), '?');
    }
}

// The five bulk operations exist only to repeat the single-character one over a
// range, so each is checked against its own scalar counterpart.
TEST(CtypeChar8, IsSeqClassifiesEveryCharacterOfTheRange)
{
    const ctype<char8_t> obj = facet_for("en_US.UTF-8");
    const std::u8string  all = every_byte();

    std::vector<mask> out(kByteValues);
    EXPECT_EQ(obj.is_seq(all.data(), all.data() + kByteValues, out.data()),
              out.data() + kByteValues);
    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(out[i], obj.is(all[i]));
    }
}

TEST(CtypeChar8, CaseMappingSeqMapsEveryCharacterOfTheRange)
{
    const ctype<char8_t> obj = facet_for("en_US.UTF-8");
    const std::u8string  all = every_byte();

    std::u8string upper(kByteValues, u8'\0');
    std::u8string lower(kByteValues, u8'\0');
    EXPECT_EQ(obj.toupper_seq(all.data(), all.data() + kByteValues, upper.data()),
              upper.data() + kByteValues);
    EXPECT_EQ(obj.tolower_seq(all.data(), all.data() + kByteValues, lower.data()),
              lower.data() + kByteValues);

    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(upper[i], obj.toupper(all[i]));
        EXPECT_EQ(lower[i], obj.tolower(all[i]));
    }
}

TEST(CtypeChar8, WidenAndNarrowSeqMapEveryCharacterOfTheRange)
{
    const ctype<char8_t> obj = facet_for("en_US.UTF-8");
    const std::u8string  all = every_byte();

    std::string   source(kByteValues, '\0');
    for (int i = 0; i < kByteValues; ++i) source[i] = static_cast<char>(i);

    std::u8string widened(kByteValues, u8'\0');
    std::string   narrowed(kByteValues, '\0');
    EXPECT_EQ(obj.widen_seq(source.data(), source.data() + kByteValues, widened.data()),
              widened.data() + kByteValues);
    EXPECT_EQ(obj.narrow_seq(all.data(), all.data() + kByteValues, 0, narrowed.data()),
              narrowed.data() + kByteValues);

    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(widened[i], obj.widen(source[i]));
        EXPECT_EQ(narrowed[i], obj.narrow(all[i], 0));
    }
}

TEST(CtypeChar8, AnEmptyRangeWritesNothing)
{
    const ctype<char8_t> obj = facet_for("en_US.UTF-8");
    const char8_t        one = u8'a';
    const char           src = 'a';

    mask    m = static_cast<mask>(0);
    char8_t u = u8'\0';
    char    c = '\0';
    EXPECT_EQ(obj.is_seq(&one, &one, &m), &m);
    EXPECT_EQ(obj.toupper_seq(&one, &one, &u), &u);
    EXPECT_EQ(obj.tolower_seq(&one, &one, &u), &u);
    EXPECT_EQ(obj.widen_seq(&src, &src, &u), &u);
    EXPECT_EQ(obj.narrow_seq(&one, &one, 0, &c), &c);
}

TEST(CtypeChar8, IsAnyIsTheClassificationMaskedByTheCategory)
{
    const ctype<char8_t> obj = facet_for("en_US.UTF-8");
    for (int i = 0; i < kByteValues; ++i)
        for (const named_mask& cat : kCategories)
        {
            SCOPED_TRACE(std::string(cat.name) + " " + std::to_string(i));
            const auto u = static_cast<char8_t>(i);
            EXPECT_EQ(obj.is_any(cat.value, u), (obj.is(u) & cat.value) != 0);
        }
}

TEST(CtypeChar8, AskingAboutAUnionOfCategoriesUnionsTheAnswers)
{
    const ctype<char8_t> obj = facet_for("en_US.UTF-8");
    for (int i = 0; i < kByteValues; ++i)
        for (const named_mask& lhs : kCategories)
            for (const named_mask& rhs : kCategories)
            {
                SCOPED_TRACE(std::string(lhs.name) + "|" + rhs.name + " " + std::to_string(i));
                const auto u = static_cast<char8_t>(i);
                EXPECT_EQ(obj.is_any(lhs.value | rhs.value, u),
                          obj.is_any(lhs.value, u) || obj.is_any(rhs.value, u));
            }
}

TEST(CtypeChar8, ScanFindsTheFirstMatchAndTheFirstNonMatch)
{
    const ctype<char8_t> obj  = facet_for("C");
    const std::u8string  data = u8"  9x";
    const char8_t*       beg  = data.data();
    const char8_t*       end  = data.data() + data.size();

    EXPECT_EQ(obj.scan_is_any(CT::digit, beg, end), beg + 2);
    EXPECT_EQ(obj.scan_is_any(CT::alpha, beg, end), beg + 3);
    EXPECT_EQ(obj.scan_is_any(CT::cntrl, beg, end), end);
    EXPECT_EQ(obj.scan_not_any(CT::space, beg, end), beg + 2);
    EXPECT_EQ(obj.scan_not_any(CT::digit, beg, end), beg);
    EXPECT_EQ(obj.scan_not_any(CT::print, beg, end), end);
}

TEST(CtypeChar8, ScanningAnEmptyRangeReturnsItsEnd)
{
    const ctype<char8_t> obj = facet_for("C");
    const char8_t        one = u8'a';
    EXPECT_EQ(obj.scan_is_any(CT::alpha, &one, &one), &one);
    EXPECT_EQ(obj.scan_not_any(CT::alpha, &one, &one), &one);
}

// On a run of one repeated code unit each scan can only answer the front of the
// range or its end, and which one has to be exactly what is_any says.  Running
// it over all 256 values covers the continuation bytes too, where the answer is
// "not a member" for every category.
TEST(CtypeChar8, ScanAgreesWithIsAnyForEveryCharacterAndCategory)
{
    const ctype<char8_t> obj = facet_for("C");
    for (int i = 0; i < kByteValues; ++i)
    {
        const std::u8string run(5, static_cast<char8_t>(i));
        const char8_t*      beg = run.data();
        const char8_t*      end = run.data() + run.size();

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
// Case mapping is idempotent: a character that has already been mapped is a
// fixed point of the same mapping.  The stronger claim that the two undo each
// other is false in Unicode -- U+00B5 MICRO SIGN uppercases to GREEK CAPITAL MU,
// which lowercases to GREEK SMALL MU rather than back to the micro sign.
TEST(CtypeChar8, CaseMappingIsIdempotent)
{
    const ctype<char8_t> obj = facet_for("en_US.UTF-8");
    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        const auto u = static_cast<char8_t>(i);
        EXPECT_EQ(obj.toupper(obj.toupper(u)), obj.toupper(u));
        EXPECT_EQ(obj.tolower(obj.tolower(u)), obj.tolower(u));
    }
}

// widen and narrow are inverses over the ASCII half.  Above it widen produces a
// continuation byte that narrow refuses, so the round trip is only claimed where
// both halves of it are defined.
TEST(CtypeChar8, NarrowUndoesWidenOverAscii)
{
    const ctype<char8_t> obj = facet_for("en_US.UTF-8");
    for (int i = 0; i < kAsciiEnd; ++i)
    {
        SCOPED_TRACE(i);
        const auto c = static_cast<char>(i);
        EXPECT_EQ(obj.narrow(obj.widen(c), '?'), c);
    }
}
