// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * IOv2::ctype<char>, checked two ways.
 *
 * The classification and case-mapping answers themselves come from the C
 * library, so restating them here would only restate the locale database.  What
 * the cases below check instead is that the facet delivers them faithfully:
 * every one of the 256 byte values is compared against std::ctype_byname for the
 * same locale, and every bulk operation is compared against the single-character
 * one it is supposed to repeat.
 *
 * The rest are the properties [locale.ctype.virtuals] states outright -- is_any
 * as a masked is(), scan_is_any and scan_not_any as searches over is_any, and
 * the "C" locale's fixed answers for the categories the standard pins down.
 */
#include <IOv2/facet/ctype.h>
#include <IOv2/facet/ctype_details.h>

#include <support/platform_macros.h>

#include <gtest/gtest.h>

#include <locale>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using namespace IOv2;

namespace
{
    using CT   = base_ft<ctype>;
    using mask = CT::mask;

    // Every value an unsigned char can take, the full domain of the facet's table.
    constexpr int kByteValues = 256;

    ctype<char> facet_for(const char* loc)
    {
        return ctype<char>(std::make_shared<ctype_conf<char>>(loc));
    }

    // std::ctype_byname's constructor is protected against direct use, so a
    // trivial derived class is the only way to get one outside a std::locale.
    struct ref_ctype : std::ctype_byname<char>
    {
        explicit ref_ctype(const char* name) : std::ctype_byname<char>(name) {}
    };

    // The reference's classification of c, asked one category at a time.  is(m, c)
    // means "has any of m" on every implementation, whereas what a std mask's bits
    // are is each library's own: the MSVC STL makes alpha, print and space
    // composites of several, so comparing bits would misread it.
    mask expected_for(const ref_ctype& ref, char c)
    {
        mask res = static_cast<mask>(0);
        auto copy = [&](std::ctype_base::mask from, mask to)
        { if (ref.is(from, c)) res |= to; };

        copy(std::ctype_base::upper,  CT::upper);
        copy(std::ctype_base::lower,  CT::lower);
        copy(std::ctype_base::alpha,  CT::alpha);
        copy(std::ctype_base::digit,  CT::digit);
        copy(std::ctype_base::xdigit, CT::xdigit);
        copy(std::ctype_base::space,  CT::space);
        copy(std::ctype_base::print,  CT::print);
        copy(std::ctype_base::cntrl,  CT::cntrl);
        copy(std::ctype_base::punct,  CT::punct);

#if IOV2_TEST_WINDOWS
        // The MSVC STL takes the UCRT's table as it is, where tab prints under a named
        // locale; IOv2 does not (ctype_isprint in facet/platform/windows/ctype_backend.h).
        if (c == '\t') res = static_cast<mask>(res & ~CT::print);
#endif
        return res;
    }

    // alnum and graph are unions of the bits above rather than bits of their
    // own, so they are listed apart: they belong in the mask algebra cases but
    // not in a table that has to enumerate distinct bits.
    struct named_mask { const char* name; mask value; };
    const named_mask kCategories[] = {
        {"upper", CT::upper}, {"lower", CT::lower}, {"alpha", CT::alpha},
        {"digit", CT::digit}, {"xdigit", CT::xdigit}, {"space", CT::space},
        {"print", CT::print}, {"cntrl", CT::cntrl}, {"punct", CT::punct},
        {"alnum", CT::alnum}, {"graph", CT::graph},
    };

    std::string every_byte()
    {
        std::string out(kByteValues, '\0');
        for (int i = 0; i < kByteValues; ++i) out[i] = static_cast<char>(i);
        return out;
    }
}

TEST(CtypeChar, TheCharacterTypeIsChar)
{
    static_assert(std::is_same_v<ctype<char>::char_type, char>);
    static_assert(std::is_same_v<ctype<char>::mask, mask>);
}

TEST(CtypeChar, ClassificationMatchesTheStandardFacet)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    const ref_ctype   ref("en_US.UTF-8");

    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        const char c = static_cast<char>(i);
        EXPECT_EQ(obj.is(c), expected_for(ref, c));
    }
}

TEST(CtypeChar, CaseMappingMatchesTheStandardFacet)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    const ref_ctype   ref("en_US.UTF-8");

    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        const char c = static_cast<char>(i);
#if IOV2_TEST_MACOS
        // libc++ hands the byte to macOS's toupper_l, which maps it as the code point
        // of the same value (0xE0 to 0xC0) although under UTF-8 it is no character.
        // IOv2 keeps such bytes as they are (ctype_byte_is_char in
        // facet/platform/posix/ctype_backend.h), as glibc does.
        if (i >= 0x80)
        {
            EXPECT_EQ(obj.toupper(c), c);
            EXPECT_EQ(obj.tolower(c), c);
            continue;
        }
#endif
        EXPECT_EQ(obj.toupper(c), ref.toupper(c));
        EXPECT_EQ(obj.tolower(c), ref.tolower(c));
    }
}

TEST(CtypeChar, WidenAndNarrowMatchTheStandardFacet)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    const ref_ctype   ref("en_US.UTF-8");

    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        const char c = static_cast<char>(i);
        EXPECT_EQ(obj.widen(c), ref.widen(c));
#if IOV2_TEST_MACOS
        // How ctype<char> narrows is up to the implementation: libc++ gives the default
        // for every non-ASCII byte, IOv2 gives the byte back, as libstdc++ and the MSVC
        // STL do.
        if (i >= 0x80)
        {
            EXPECT_EQ(obj.narrow(c, 0), c);
            continue;
        }
#endif
        EXPECT_EQ(obj.narrow(c, 0), ref.narrow(c, 0));
    }
}

// The five bulk operations exist only to repeat the single-character one over a
// range, so each is checked against its own scalar counterpart rather than
// against a second copy of the locale data.
TEST(CtypeChar, IsSeqClassifiesEveryCharacterOfTheRange)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    const std::string all = every_byte();

    std::vector<mask> out(kByteValues);
    EXPECT_EQ(obj.is_seq(all.data(), all.data() + kByteValues, out.data()),
              out.data() + kByteValues);
    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(out[i], obj.is(all[i]));
    }
}

TEST(CtypeChar, CaseMappingSeqMapsEveryCharacterOfTheRange)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    const std::string all = every_byte();

    std::string upper(kByteValues, '\0');
    std::string lower(kByteValues, '\0');
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

TEST(CtypeChar, WidenAndNarrowSeqMapEveryCharacterOfTheRange)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    const std::string all = every_byte();

    std::string widened(kByteValues, '\0');
    std::string narrowed(kByteValues, '\0');
    EXPECT_EQ(obj.widen_seq(all.data(), all.data() + kByteValues, widened.data()),
              widened.data() + kByteValues);
    EXPECT_EQ(obj.narrow_seq(all.data(), all.data() + kByteValues, 0, narrowed.data()),
              narrowed.data() + kByteValues);

    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(widened[i], obj.widen(all[i]));
        EXPECT_EQ(narrowed[i], obj.narrow(all[i], 0));
    }
}

TEST(CtypeChar, AnEmptyRangeWritesNothing)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    const char        one = 'a';

    mask m = static_cast<mask>(0);
    char c = '\0';
    EXPECT_EQ(obj.is_seq(&one, &one, &m), &m);
    EXPECT_EQ(obj.toupper_seq(&one, &one, &c), &c);
    EXPECT_EQ(obj.tolower_seq(&one, &one, &c), &c);
    EXPECT_EQ(obj.widen_seq(&one, &one, &c), &c);
    EXPECT_EQ(obj.narrow_seq(&one, &one, 0, &c), &c);
}

// is_any is is() masked by the category, which is what makes every other query
// in the facet a question about the same table.
TEST(CtypeChar, IsAnyIsTheClassificationMaskedByTheCategory)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    for (int i = 0; i < kByteValues; ++i)
        for (const named_mask& cat : kCategories)
        {
            SCOPED_TRACE(std::string(cat.name) + " " + std::to_string(i));
            const char c = static_cast<char>(i);
            EXPECT_EQ(obj.is_any(cat.value, c), (obj.is(c) & cat.value) != 0);
        }
}

// A mask names a set of categories, so asking about a union has to answer the
// union of the answers -- for every pair, not just the disjoint ones.
TEST(CtypeChar, AskingAboutAUnionOfCategoriesUnionsTheAnswers)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    for (int i = 0; i < kByteValues; ++i)
        for (const named_mask& lhs : kCategories)
            for (const named_mask& rhs : kCategories)
            {
                SCOPED_TRACE(std::string(lhs.name) + "|" + rhs.name + " " + std::to_string(i));
                const char c = static_cast<char>(i);
                EXPECT_EQ(obj.is_any(lhs.value | rhs.value, c),
                          obj.is_any(lhs.value, c) || obj.is_any(rhs.value, c));
            }
}

TEST(CtypeChar, UpperAndLowerCharactersAreAlphabetic)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        const char c = static_cast<char>(i);
        if (obj.is_any(CT::upper, c) || obj.is_any(CT::lower, c))
        {
            EXPECT_TRUE(obj.is_any(CT::alpha, c));
        }
    }
}

TEST(CtypeChar, ControlCharactersAreNotPrintable)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        const char c = static_cast<char>(i);
        EXPECT_FALSE(obj.is_any(CT::cntrl, c) && obj.is_any(CT::print, c));
    }
}

// The "C" locale is the one the standard fixes completely, so it is the only one
// whose membership can be written down here rather than looked up.
TEST(CtypeChar, TheCLocalePinsDownTheDigitCategories)
{
    const ctype<char> obj = facet_for("C");
    const std::string digits  = "0123456789";
    const std::string hex     = "0123456789abcdefABCDEF";

    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        const char c = static_cast<char>(i);
        EXPECT_EQ(obj.is_any(CT::digit, c), digits.find(c) != std::string::npos);
        EXPECT_EQ(obj.is_any(CT::xdigit, c), hex.find(c) != std::string::npos);
    }
}

// print is graph plus the space character, and nothing else: a printable
// character that is neither alphanumeric nor punctuation can only be ' '.
TEST(CtypeChar, TheCLocalePrintsGraphAndTheSpaceCharacter)
{
    const ctype<char> obj = facet_for("C");
    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        const char c = static_cast<char>(i);
        EXPECT_EQ(obj.is_any(CT::print, c), obj.is_any(CT::graph, c) || c == ' ');
    }
}

TEST(CtypeChar, ScanIsAnyReturnsTheFirstCharacterInTheCategory)
{
    const ctype<char> obj  = facet_for("C");
    const std::string data = "  9x";

    EXPECT_EQ(obj.scan_is_any(CT::digit, data.data(), data.data() + data.size()),
              data.data() + 2);
    EXPECT_EQ(obj.scan_is_any(CT::space, data.data(), data.data() + data.size()),
              data.data());
    EXPECT_EQ(obj.scan_is_any(CT::alpha, data.data(), data.data() + data.size()),
              data.data() + 3);
}

TEST(CtypeChar, ScanIsAnyReturnsTheEndWhenNothingMatches)
{
    const ctype<char> obj  = facet_for("C");
    const std::string data = "  9x";
    EXPECT_EQ(obj.scan_is_any(CT::cntrl, data.data(), data.data() + data.size()),
              data.data() + data.size());
}

TEST(CtypeChar, ScanNotAnyReturnsTheFirstCharacterOutsideTheCategory)
{
    const ctype<char> obj  = facet_for("C");
    const std::string data = "  9x";

    EXPECT_EQ(obj.scan_not_any(CT::space, data.data(), data.data() + data.size()),
              data.data() + 2);
    EXPECT_EQ(obj.scan_not_any(CT::digit, data.data(), data.data() + data.size()),
              data.data());
    EXPECT_EQ(obj.scan_not_any(CT::print, data.data(), data.data() + data.size()),
              data.data() + data.size());
}

TEST(CtypeChar, ScanningAnEmptyRangeReturnsItsEnd)
{
    const ctype<char> obj = facet_for("C");
    const char        one = 'a';
    EXPECT_EQ(obj.scan_is_any(CT::alpha, &one, &one), &one);
    EXPECT_EQ(obj.scan_not_any(CT::alpha, &one, &one), &one);
}

// The two scans are searches over is_any, so on a run of one repeated character
// each can only answer the front of the range or its end -- and which one it is
// has to be exactly what is_any says about that character.
TEST(CtypeChar, ScanAgreesWithIsAnyForEveryCharacterAndCategory)
{
    const ctype<char> obj = facet_for("C");
    for (int i = 0; i < kByteValues; ++i)
    {
        const std::string run(5, static_cast<char>(i));
        const char*       beg = run.data();
        const char*       end = run.data() + run.size();

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
TEST(CtypeChar, CaseMappingIsIdempotent)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        const char c = static_cast<char>(i);
        EXPECT_EQ(obj.toupper(obj.toupper(c)), obj.toupper(c));
        EXPECT_EQ(obj.tolower(obj.tolower(c)), obj.tolower(c));
    }
}

// widen and narrow are inverses on the character type they are both defined
// over, which for char is every value it can hold.
TEST(CtypeChar, NarrowUndoesWiden)
{
    const ctype<char> obj = facet_for("en_US.UTF-8");
    for (int i = 0; i < kByteValues; ++i)
    {
        SCOPED_TRACE(i);
        const char c = static_cast<char>(i);
        EXPECT_EQ(obj.narrow(obj.widen(c), '?'), c);
    }
}

// Under a single-byte code page the high half holds letters of its own, and the
// byte table has to know them: é and É are 0xE9 and 0xC9 in 1252, а and А are
// 0xE0 and 0xC0 in 1251.  Under UTF-8, the locale the other tests use, those
// bytes are not characters, so only a code-page locale reaches this.
TEST(CtypeChar, ACodePageLocaleClassifiesAndMapsItsHighBytes)
{
#if IOV2_TEST_WINDOWS
    const ctype<char> western = facet_for("English_United States.1252");
    EXPECT_TRUE(western.is_any(CT::alpha, '\xE9'));
    EXPECT_TRUE(western.is_any(CT::lower, '\xE9'));
    EXPECT_TRUE(western.is_any(CT::upper, '\xC9'));
    EXPECT_EQ(western.toupper('\xE9'), '\xC9');
    EXPECT_EQ(western.tolower('\xC9'), '\xE9');
    // A named locale's table makes tab blank, which the UCRT's isprint counts as printing.
    EXPECT_TRUE(western.is_any(CT::cntrl, '\t'));
    EXPECT_FALSE(western.is_any(CT::print, '\t'));
    EXPECT_TRUE(western.is_any(CT::print, ' '));

    const ctype<char> cyrillic = facet_for("ru-RU");
    EXPECT_TRUE(cyrillic.is_any(CT::lower, '\xE0'));
    EXPECT_EQ(cyrillic.toupper('\xE0'), '\xC0');
    EXPECT_EQ(cyrillic.tolower('\xC0'), '\xE0');
#else
    GTEST_SKIP() << "code-page locale names such as English_United States.1252 are the UCRT's";
#endif
}
