// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same currency contract as test_monetary_char.cpp for char8_t.  What differs is
 * only the type the field is written in: the pattern, the grouping, the sign
 * placement and the parse are one algorithm over whatever character type the
 * facet was instantiated with, and these cases are here to say that the
 * instantiation changes none of it.
 *
 * The cases about the configuration rather than the field -- how a locale name
 * is spelled, how the POSIX sign position becomes a pattern, which fill
 * characters would change the amount a field reads as -- read the same data
 * whatever the character type is, so they stay in the narrow file.
 */
#include <facet/monetary.h>
#include <facet/monetary_details.h>

#include <common/defs.h>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace IOv2;

namespace
{
    using part    = base_ft<monetary>::part;
    using pattern = base_ft<monetary>::pattern;

    // A configuration whose every answer can be set.  What a format case is
    // about is the format, so it states one here instead of borrowing whichever
    // one a locale happens to carry this year.
    class tunable_conf : public monetary_conf<char8_t>,
                         public std::enable_shared_from_this<tunable_conf>
    {
    public:
        explicit tunable_conf(const std::string& name = "C")
            : monetary_conf<char8_t>(name)
            , m_decimal_point(monetary_conf<char8_t>::decimal_point())
            , m_thousands_sep(monetary_conf<char8_t>::thousands_sep())
            , m_grouping(monetary_conf<char8_t>::grouping())
            , m_curr_symbol(monetary_conf<char8_t>::curr_symbol_nat())
            , m_positive_sign(monetary_conf<char8_t>::positive_sign_nat())
            , m_negative_sign(monetary_conf<char8_t>::negative_sign_nat())
            , m_frac_digits(monetary_conf<char8_t>::frac_digits_nat())
            , m_pos_format(monetary_conf<char8_t>::pos_format_nat())
            , m_neg_format(monetary_conf<char8_t>::neg_format_nat())
        {}

        char8_t                     decimal_point() const override { return m_decimal_point; }
        char8_t                     thousands_sep() const override { return m_thousands_sep; }
        const std::vector<uint8_t>& grouping() const override { return m_grouping; }
        const std::u8string&        curr_symbol_nat() const override { return m_curr_symbol; }
        const std::u8string&        positive_sign_nat() const override { return m_positive_sign; }
        const std::u8string&        negative_sign_nat() const override { return m_negative_sign; }
        int                         frac_digits_nat() const override { return m_frac_digits; }
        const pattern&              pos_format_nat() const override { return m_pos_format; }
        const pattern&              neg_format_nat() const override { return m_neg_format; }

        tunable_conf& point(char8_t c)                       { m_decimal_point = c; return *this; }
        tunable_conf& separator(char8_t c)                   { m_thousands_sep = c; return *this; }
        tunable_conf& groups(std::vector<uint8_t> g)      { m_grouping = std::move(g); return *this; }
        tunable_conf& symbol(std::u8string s)               { m_curr_symbol = std::move(s); return *this; }
        tunable_conf& plus(std::u8string s)                 { m_positive_sign = std::move(s); return *this; }
        tunable_conf& minus(std::u8string s)                { m_negative_sign = std::move(s); return *this; }
        tunable_conf& fraction(int n)                     { m_frac_digits = n; return *this; }
        tunable_conf& positive(pattern p)                 { m_pos_format = p; return *this; }
        tunable_conf& negative(pattern p)                 { m_neg_format = p; return *this; }
        tunable_conf& both(pattern p)                     { return positive(p).negative(p); }

        // Ends a chain: the facet's constructor takes the configuration by
        // shared pointer, and the chain has been handing back references.
        std::shared_ptr<tunable_conf> ptr()               { return shared_from_this(); }

    private:
        char8_t              m_decimal_point;
        char8_t              m_thousands_sep;
        std::vector<uint8_t> m_grouping;
        std::u8string        m_curr_symbol;
        std::u8string        m_positive_sign;
        std::u8string        m_negative_sign;
        int                  m_frac_digits;
        pattern              m_pos_format;
        pattern              m_neg_format;
    };

    // The pattern the cases below start from unless they say otherwise: the
    // symbol first, then the sign, then the amount, with nothing at the end.
    constexpr pattern kSymbolSignValue = {part::symbol, part::none, part::sign, part::value};

    std::shared_ptr<tunable_conf> tuned(const char* name = "C")
    {
        return std::make_shared<tunable_conf>(name);
    }

    monetary<char8_t> facet_for(const char* loc)
    {
        return monetary<char8_t>(std::make_shared<monetary_conf<char8_t>>(loc));
    }

    // std::to_string is narrow; an amount reaches the facet in the character
    // type under test.
    std::u8string to_digits(int64_t v)
    {
        const std::string ascii = std::to_string(v);
        return std::u8string(ascii.begin(), ascii.end());
    }

    // Takes either spelling of an amount -- the digit string or the integer --
    // because put() has an overload for each and they must agree.
    template <typename TVal>
    std::u8string put_str(const monetary<char8_t>& obj, bool intl, ios_base<char8_t>& io,
                        const TVal& amount)
    {
        std::u8string out;
        obj.put(std::back_inserter(out), intl, io, amount);
        return out;
    }

    // The round-trip case names its combination on one line, so the sibling
    // files' literal retyping cannot reach these labels.
    std::string trace_case(int frac, std::size_t groups, bool showbase, const std::u8string& amount)
    {
        return "frac=" + std::to_string(frac) + " groups=" + std::to_string(groups) + " showbase=" + std::to_string(showbase) + " amount=" + ::testing::PrintToString(amount);
    }

    // What a parse produced: whether it succeeded, the digits it yielded, and
    // the input it left behind.
    struct parse_result
    {
        bool          ok;
        std::u8string digits;
        std::u8string rest;
    };

    parse_result parse_over_pointers(const monetary<char8_t>& obj, bool intl, ios_base<char8_t>& io,
                                     const std::u8string& input, const std::u8string& seed)
    {
        parse_result res{true, seed, {}};
        try
        {
            auto it  = obj.get(input.begin(), input.end(), intl, io, res.digits);
            res.rest = std::u8string(it, input.end());
        }
        catch (const stream_error&)
        {
            res.ok = false;
        }
        return res;
    }

    // The same parse over an iterator that cannot be compared to anything but a
    // sentinel and cannot be rewound -- the shape get() is actually written for.
    parse_result parse_over_a_stream(const monetary<char8_t>& obj, bool intl, ios_base<char8_t>& io,
                                     const std::u8string& input, const std::u8string& seed)
    {
        parse_result res{true, seed, {}};
        streambuf    sb(mem_device{input});
        auto         beg = istreambuf_iterator(sb);
        try
        {
            auto it = obj.get(beg, std::default_sentinel, intl, io, res.digits);
            res.rest = std::u8string(it, decltype(it)());
        }
        catch (const stream_error&)
        {
            res.ok = false;
        }
        return res;
    }

    // Every parse assertion goes through here, so no case can check one iterator
    // shape and leave the other unexamined.
    void expect_parses(const monetary<char8_t>& obj, bool intl, ios_base<char8_t>& io,
                       const std::u8string& input, const std::u8string& digits,
                       const std::u8string& rest = u8"")
    {
        SCOPED_TRACE(::testing::PrintToString(input));
        for (bool streamed : {false, true})
        {
            SCOPED_TRACE(streamed ? "streambuf iterator" : "string iterator");
            const parse_result r = streamed ? parse_over_a_stream(obj, intl, io, input, u8"")
                                            : parse_over_pointers(obj, intl, io, input, u8"");
            EXPECT_TRUE(r.ok);
            EXPECT_EQ(r.digits, digits);
            EXPECT_EQ(r.rest, rest);
        }
    }

    // A failed parse throws, and the digit string it was handed must come back
    // exactly as it was: the caller's variable is not a scratch buffer.
    void expect_rejects(const monetary<char8_t>& obj, bool intl, ios_base<char8_t>& io,
                        const std::u8string& input)
    {
        SCOPED_TRACE(::testing::PrintToString(input));
        const std::u8string seed = u8"untouched";
        for (bool streamed : {false, true})
        {
            SCOPED_TRACE(streamed ? "streambuf iterator" : "string iterator");
            const parse_result r = streamed ? parse_over_a_stream(obj, intl, io, input, seed)
                                            : parse_over_pointers(obj, intl, io, input, seed);
            EXPECT_FALSE(r.ok);
            EXPECT_EQ(r.digits, seed);
        }
    }
}

TEST(MonetaryChar8, TheCharacterTypeIsChar)
{
    static_assert(std::is_same_v<monetary<char8_t>::char_type, char8_t>);
}

// [locale.moneypunct] fixes the "C" locale completely: no currency, no
// grouping, no fractional digits, and the same format both ways round.
TEST(MonetaryChar8, TheCLocaleCarriesNoCurrency)
{
    const monetary<char8_t> obj = facet_for("C");

    EXPECT_EQ(obj.decimal_point(), u8'.');
    EXPECT_EQ(obj.thousands_sep(), u8',');
    EXPECT_TRUE(obj.grouping().empty());
    EXPECT_TRUE(obj.curr_symbol_nat().empty());
    EXPECT_TRUE(obj.curr_symbol_int().empty());
    EXPECT_TRUE(obj.positive_sign_nat().empty());
    EXPECT_TRUE(obj.positive_sign_int().empty());
    EXPECT_EQ(obj.frac_digits_int(), 0);
    EXPECT_EQ(obj.frac_digits_nat(), 0);
    EXPECT_EQ(obj.pos_format_int(), obj.pos_format_nat());
    EXPECT_EQ(obj.neg_format_int(), obj.neg_format_nat());
}

// The negative sign is the one thing the "C" locale must still spell, or a
// negative amount would come out indistinguishable from a positive one.
TEST(MonetaryChar8, TheCLocaleStillHasANegativeSign)
{
    const monetary<char8_t> obj = facet_for("C");
    EXPECT_FALSE(obj.negative_sign_nat().empty());
    EXPECT_FALSE(obj.negative_sign_int().empty());
}

TEST(MonetaryChar8, ALocaleWithCurrencyDataDiffersFromTheCLocale)
{
    const monetary<char8_t> plain = facet_for("C");
    const monetary<char8_t> us    = facet_for("en_US.UTF-8");

    EXPECT_NE(us.curr_symbol_nat(), plain.curr_symbol_nat());
    EXPECT_NE(us.frac_digits_nat(), plain.frac_digits_nat());
    EXPECT_NE(us.grouping(), plain.grouping());
}

// The digits are the smallest units of the currency, so the amount they spell is
// read off their right-hand end: frac_digits places go behind the decimal point
// and the rest in front of it.
TEST(MonetaryChar8, TheAmountIsCutFracDigitsPlacesFromTheRight)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(2).both(kSymbolSignValue).ptr());

    EXPECT_EQ(put_str(obj, false, ios, u8"827364"), u8"8273.64");
    EXPECT_EQ(put_str(obj, false, ios, u8"1"), u8".01");
    EXPECT_EQ(put_str(obj, false, ios, u8"12"), u8".12");
    EXPECT_EQ(put_str(obj, false, ios, u8"123"), u8"1.23");
}

// A run too short to reach the cut has no integral part at all, and the fraction
// picks up the shortfall as leading zeros: two places turn 7 into .07, not 7.0.
TEST(MonetaryChar8, AShortAmountIsPaddedInTheFractionNotTheInteger)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(3).both(kSymbolSignValue).ptr());

    EXPECT_EQ(put_str(obj, false, ios, u8"7"), u8".007");
    EXPECT_EQ(put_str(obj, false, ios, u8"70"), u8".070");
    EXPECT_EQ(put_str(obj, false, ios, u8"700"), u8".700");
    EXPECT_EQ(put_str(obj, false, ios, u8"7000"), u8"7.000");
}

TEST(MonetaryChar8, NoFractionalDigitsMeansNoDecimalPoint)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(obj, false, ios, u8"123456"), u8"123456");
}

// A negative frac_digits asks for no fraction at all and keeps the whole run,
// which is not the same statement as zero places: it is the locale saying the
// question does not apply.
TEST(MonetaryChar8, ANegativeFractionWidthKeepsEveryDigitIntegral)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(-1).both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(obj, false, ios, u8"123456"), u8"123456");
}

TEST(MonetaryChar8, GroupingInsertsTheThousandsSeparator)
{
    ios_base<char8_t> ios;

    const monetary<char8_t> threes(tuned()->fraction(0).groups({3}).separator(u8',')
                                       .both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(threes, false, ios, u8"1234567"), u8"1,234,567");

    const monetary<char8_t> ones(tuned()->fraction(0).groups({1}).separator(u8'#')
                                     .both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(ones, false, ios, u8"1234"), u8"1#2#3#4");

    // A grouping vector is read right to left and its last entry repeats, so
    // {3,2} groups three digits then twos all the way up.
    const monetary<char8_t> indian(tuned()->fraction(0).groups({3, 2}).separator(u8',')
                                       .both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(indian, false, ios, u8"12345678"), u8"1,23,45,678");
}

TEST(MonetaryChar8, AnEmptyGroupingInsertsNothing)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).groups({}).both(kSymbolSignValue).ptr());
    const std::u8string     digits(300, u8'1');
    EXPECT_EQ(put_str(obj, false, ios, digits), digits);
}

// The symbol is the one part of the field the caller decides about: it is
// written when showbase is set and left out otherwise, and nothing else about
// the field changes with it.
TEST(MonetaryChar8, TheSymbolIsWrittenOnlyWithShowbase)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(3).symbol(u8"@").both(kSymbolSignValue).ptr());

    EXPECT_EQ(put_str(obj, false, ios, u8"482715"), u8"482.715");
    ios.setf(ios_defs::showbase);
    EXPECT_EQ(put_str(obj, false, ios, u8"482715"), u8"@482.715");
    ios.unsetf(ios_defs::showbase);
    EXPECT_EQ(put_str(obj, false, ios, u8"482715"), u8"482.715");
}

TEST(MonetaryChar8, ThePatternDecidesTheOrderOfTheParts)
{
    ios_base<char8_t> ios;
    ios.setf(ios_defs::showbase);

    const std::pair<pattern, const char8_t*> cases[] = {
        {{part::symbol, part::sign, part::value, part::none}, u8"$-12"},
        {{part::sign, part::symbol, part::value, part::none}, u8"-$12"},
        {{part::value, part::space, part::symbol, part::sign}, u8"12 $-"},
        {{part::sign, part::value, part::space, part::symbol}, u8"-12 $"},
        {{part::symbol, part::space, part::value, part::sign}, u8"$ 12-"},
    };

    for (const auto& [order, expected] : cases)
    {
        SCOPED_TRACE(::testing::PrintToString(expected));
        const monetary<char8_t> obj(tuned()->fraction(0).symbol(u8"$").minus(u8"-").negative(order).ptr());
        EXPECT_EQ(put_str(obj, false, ios, u8"-12"), expected);
    }
}

// A sign spelled with more than one character wraps the field: its first
// character sits in the sign slot and the rest trails everything, which is how
// a locale writes a negative amount in parentheses.
TEST(MonetaryChar8, AMultiCharacterSignWrapsTheField)
{
    ios_base<char8_t> ios;
    ios.setf(ios_defs::showbase);
    const monetary<char8_t> obj(tuned()->fraction(2).groups({3}).separator(u8',').symbol(u8"$")
                                    .minus(u8"()")
                                    .negative({part::symbol, part::space, part::sign, part::value}).ptr());

    EXPECT_EQ(put_str(obj, false, ios, u8"-827364"), u8"$ (8,273.64)");
}

TEST(MonetaryChar8, TheSignOfTheAmountChoosesThePattern)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).plus(u8"+").minus(u8"-")
                                    .positive({part::sign, part::value, part::none, part::none})
                                    .negative({part::value, part::sign, part::none, part::none}).ptr());

    EXPECT_EQ(put_str(obj, false, ios, u8"12"), u8"+12");
    EXPECT_EQ(put_str(obj, false, ios, u8"-12"), u8"12-");
}

// The international and national sets are independent, and intl is what picks
// between them: the same amount through one facet has two spellings.
TEST(MonetaryChar8, TheInternationalFlagSelectsTheOtherPunctuation)
{
    ios_base<char8_t> ios;
    ios.setf(ios_defs::showbase);
    const monetary<char8_t> obj = facet_for("en_US.UTF-8");

    EXPECT_NE(obj.curr_symbol_int(), obj.curr_symbol_nat());
    const std::u8string national      = put_str(obj, false, ios, u8"123456");
    const std::u8string international = put_str(obj, true, ios, u8"123456");
    EXPECT_NE(national, international);
    EXPECT_NE(national.find(obj.curr_symbol_nat()), std::u8string::npos);
    EXPECT_NE(international.find(obj.curr_symbol_int()), std::u8string::npos);
}

TEST(MonetaryChar8, AShortFieldIsPaddedToTheWidth)
{
    const monetary<char8_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());

    ios_base<char8_t> ios;
    ios.fill(u8'*');
    ios.width(8);
    EXPECT_EQ(put_str(obj, false, ios, u8"123"), u8"*****123");

    ios.width(8);
    ios.setf(ios_defs::left, ios_defs::adjustfield);
    EXPECT_EQ(put_str(obj, false, ios, u8"123"), u8"123*****");
}

// Under internal the shortfall is not tacked onto an end: it goes into whichever
// pattern slot writes nothing of its own, which is what puts the fill between
// the symbol and the amount rather than outside them.
TEST(MonetaryChar8, InternalPaddingGoesIntoTheEmptySlot)
{
    const monetary<char8_t> obj(tuned()->fraction(0).symbol(u8"$").minus(u8"-")
                                    .negative({part::symbol, part::none, part::sign, part::value}).ptr());

    ios_base<char8_t> ios;
    ios.setf(ios_defs::showbase);
    ios.setf(ios_defs::internal, ios_defs::adjustfield);
    ios.fill(u8'*');
    ios.width(9);
    EXPECT_EQ(put_str(obj, false, ios, u8"-123"), u8"$****-123");
}

// width() is one-shot: the field it sized is the only one it sizes.
TEST(MonetaryChar8, TheWidthIsConsumedByOnePut)
{
    const monetary<char8_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    ios_base<char8_t>       ios;
    ios.fill(u8'*');
    ios.width(8);

    EXPECT_EQ(put_str(obj, false, ios, u8"123"), u8"*****123");
    EXPECT_EQ(ios.width(), 0u);
    EXPECT_EQ(put_str(obj, false, ios, u8"123"), u8"123");
}

// The amount runs up to the first character that is not a digit; what the caller
// put after that is not the facet's to format.  With nothing to format at all,
// nothing is written.
TEST(MonetaryChar8, WhatIsNotADigitIsNotAnAmount)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());

    EXPECT_EQ(put_str(obj, false, ios, u8"42 apples"), u8"42");
    EXPECT_EQ(put_str(obj, false, ios, u8"-A"), u8"");
    EXPECT_EQ(put_str(obj, false, ios, u8""), u8"");
    EXPECT_EQ(put_str(obj, false, ios, u8"-"), u8"");
}

TEST(MonetaryChar8, AnIntegralValueFormatsLikeItsDigitString)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(2).groups({3}).separator(u8',')
                                    .both(kSymbolSignValue).ptr());

    // Each width reaches put() through its own instantiation, so each is asked
    // to agree with the digit-string overload rather than one standing in.
    auto agrees = [&](auto v)
    {
        SCOPED_TRACE(::testing::Message() << +v);
        EXPECT_EQ(put_str(obj, false, ios, to_digits(static_cast<int64_t>(v))),
                  put_str(obj, false, ios, v));
    };

    for (int v : {0, 7, 2607, -3, -654321})
    {
        agrees(static_cast<short>(v));
        agrees(static_cast<int>(v));
        agrees(static_cast<long>(v));
        agrees(static_cast<long long>(v));
    }
    agrees(98765432109LL);
    agrees(static_cast<unsigned>(4000000000U));
    agrees(static_cast<unsigned long long>(12345678901234ULL));
}

// put() returns where it stopped, so writing into an existing buffer has to
// leave everything past that point alone.
TEST(MonetaryChar8, PutReturnsThePositionAfterTheField)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());

    std::u8string buffer(13, u8'^');
    auto          it = obj.put(buffer.begin() + 2, false, ios, std::u8string(u8"2607"));

    EXPECT_EQ(it, buffer.begin() + 6);
    EXPECT_EQ(buffer, u8"^^2607^^^^^^^");
}

// Everything above reads the field back through the same facet that wrote it.
// The two directions are separate code, so this is the case that ties them:
// whatever put() produced, get() has to return the amount put() was given.
TEST(MonetaryChar8, WhatPutWritesGetReadsBack)
{
    const std::vector<uint8_t> groupings[] = {{}, {3}, {1}, {3, 2}};
    const std::u8string        amounts[]   = {u8"0", u8"1", u8"12", u8"827364", u8"-1", u8"-827364",
                                              u8"98765432109", u8"-98765432109"};


    for (int frac : {0, 2, 3})
        for (const std::vector<uint8_t>& g : groupings)
            for (bool showbase : {false, true})
            {
                const monetary<char8_t> obj(tuned()->fraction(frac).groups(g).separator(u8',')
                                                .symbol(u8"$").plus(u8"").minus(u8"-")
                                                .both(kSymbolSignValue).ptr());
                ios_base<char8_t> ios;
                if (showbase) ios.setf(ios_defs::showbase);

                for (const std::u8string& amount : amounts)
                {
                    SCOPED_TRACE(trace_case(frac, g.size(), showbase, amount));
                    ios_base<char8_t> writer;
                    if (showbase) writer.setf(ios_defs::showbase);
                    const std::u8string field = put_str(obj, false, writer, amount);
                    ASSERT_FALSE(field.empty());
                    expect_parses(obj, false, ios, field, amount);
                }
            }
}

// Parsing ends at the first character the format has no place for, and what is
// left is the caller's to read next.
TEST(MonetaryChar8, ParsingStopsAtTheFirstForeignCharacter)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, u8"1 apple", u8"1", u8" apple");
    expect_parses(obj, false, ios, u8"123abc", u8"123", u8"abc");
}

// With grouping switched off the separator is not part of an amount, so it ends
// one rather than continuing it.
TEST(MonetaryChar8, ASeparatorEndsTheAmountWhenThereIsNoGrouping)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).groups({}).separator(u8',')
                                    .both(kSymbolSignValue).ptr());
    expect_parses(obj, false, ios, u8"742,908", u8"742", u8",908");
}

// Likewise the decimal point, when the locale has no fractional digits to put
// behind it.
TEST(MonetaryChar8, ADecimalPointEndsTheAmountWhenThereIsNoFraction)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).point(u8'.').both(kSymbolSignValue).ptr());
    expect_parses(obj, false, ios, u8"742.908", u8"742", u8".908");
}

TEST(MonetaryChar8, AnEmptySequenceIsNotAnAmount)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    expect_rejects(obj, false, ios, u8"");
}

TEST(MonetaryChar8, TextThatIsNotAnAmountIsRejected)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    expect_rejects(obj, false, ios, u8"nothing numeric");
    expect_rejects(obj, false, ios, u8"a sentence with no amount anywhere in it");
}

// A fraction is all or nothing: exactly frac_digits places, or the field is not
// an amount in this locale.
TEST(MonetaryChar8, TheFractionMustHaveExactlyFracDigitsPlaces)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(4).point(u8'.').both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, u8"73.5926", u8"735926");
    expect_rejects(obj, false, ios, u8"73.59261");
    expect_rejects(obj, false, ios, u8"73.592");
    expect_rejects(obj, false, ios, u8"73.");

    // No decimal point at all is not a short fraction: it is an amount with none.
    expect_parses(obj, false, ios, u8"73", u8"73");
}

TEST(MonetaryChar8, ASecondDecimalPointIsNotPartOfTheAmount)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(2).point(u8':').both(kSymbolSignValue).ptr());
    expect_rejects(obj, false, ios, u8"47::2");
}

// The separators have to fall where this locale's grouping puts them.  A field
// grouped some other way is a field from some other locale.
TEST(MonetaryChar8, TheSeparatorsMustFollowTheGrouping)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).groups({2}).separator(u8'#')
                                    .both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, u8"7#06#45", u8"70645");
    expect_rejects(obj, false, ios, u8"007#06#45");
    expect_rejects(obj, false, ios, u8"7#06##45");
}

// A locale that spells a positive sign but no negative one leaves the absence of
// a sign to mean negative, which is what [locale.money.get] asks for.
TEST(MonetaryChar8, NoSignMeansNegativeWhenOnlyThePositiveSignIsSpelled)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).plus(u8"+").minus(u8"")
                                    .both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, u8"69", u8"-69");
    expect_parses(obj, false, ios, u8"+69", u8"69");
}

TEST(MonetaryChar8, ASignInTheLastSlotIsStillFound)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).plus(u8"+").minus(u8"-")
                                    .both({part::value, part::space, part::symbol, part::sign}).ptr());

    expect_parses(obj, false, ios, u8"123 +", u8"123");
    expect_parses(obj, false, ios, u8"123 -", u8"-123");
}

// With showbase the symbol is part of the field and has to be there.  Without
// it the symbol is optional -- but a symbol that is present is still consumed,
// or the parse would stop in the middle of a field it could read.
TEST(MonetaryChar8, ShowbaseDecidesWhetherTheSymbolIsRequired)
{
    const monetary<char8_t> obj(tuned()->fraction(0).symbol(u8"$").minus(u8"-")
                                    .both(kSymbolSignValue).ptr());

    ios_base<char8_t> required;
    required.setf(ios_defs::showbase);
    expect_parses(obj, false, required, u8"$123", u8"123");
    expect_rejects(obj, false, required, u8"123");

    ios_base<char8_t> optional;
    expect_parses(obj, false, optional, u8"$123", u8"123");
    expect_parses(obj, false, optional, u8"123", u8"123");
}

// A field with a symbol and no digits is not an amount, whichever way round the
// symbol is required.
TEST(MonetaryChar8, ASymbolWithoutDigitsIsNotAnAmount)
{
    const monetary<char8_t> obj(tuned()->fraction(0).symbol(u8"$").minus(u8"-")
                                    .both(kSymbolSignValue).ptr());

    ios_base<char8_t> ios;
    ios.setf(ios_defs::showbase);
    expect_rejects(obj, false, ios, u8"$");
    expect_rejects(obj, false, ios, u8"$-");
}

// The fraction alone is an amount: the integral part may be empty as long as the
// places behind the point are all there.
TEST(MonetaryChar8, AnAmountMayBeAllFraction)
{
    const monetary<char8_t> obj(tuned()->fraction(3).point(u8'.').symbol(u8"@").minus(u8"-")
                                    .both(kSymbolSignValue).ptr());

    ios_base<char8_t> ios;
    expect_parses(obj, false, ios, u8"@.000 ", u8"0", u8" ");
    expect_parses(obj, false, ios, u8"@-.042 ", u8"-42", u8" ");
}

TEST(MonetaryChar8, AnAmountTooLargeForTheTargetTypeIsRejected)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).groups({}).both(kSymbolSignValue).ptr());
    const std::u8string     huge(40, u8'9');

    int64_t       units = 0;
    std::u8string digits;
    EXPECT_THROW((void)obj.get(huge.begin(), huge.end(), false, ios, units), stream_error);

    // The same field is a perfectly good digit string, though: it is only the
    // conversion to a fixed-width integer that cannot hold it.
    EXPECT_NO_THROW((void)obj.get(huge.begin(), huge.end(), false, ios, digits));
    EXPECT_EQ(digits, huge);
}

TEST(MonetaryChar8, GettingAnIntegralValueAgreesWithGettingTheDigits)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(2).groups({3}).separator(u8',')
                                    .both(kSymbolSignValue).ptr());

    for (const char8_t* field : {u8"1,234.56", u8"-1,234.56", u8".01", u8"-.01", u8"0.00"})
    {
        SCOPED_TRACE(::testing::PrintToString(field));
        const std::u8string input(field);

        std::u8string digits;
        obj.get(input.begin(), input.end(), false, ios, digits);

        int64_t units = 0;
        obj.get(input.begin(), input.end(), false, ios, units);

        EXPECT_EQ(to_digits(units), digits);
    }
}

// put() writes through an output iterator, so an iterator that reaches a stream
// rather than a container has to work as the destination too.
TEST(MonetaryChar8, PutWritesThroughAnOutputIteratorOntoAStream)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(2).groups({3}).separator(u8',')
                                    .both(kSymbolSignValue).ptr());

    streambuf sb{mem_device<char8_t>{u8""}};
    obj.put(ostreambuf_iterator(sb), false, ios, std::u8string(u8"123456"));
    sb.flush();
    EXPECT_EQ(sb.device().str(), u8"1,234.56");
}

// The same fill vetting as on the writing side, but from the reader's end: a run
// of fill in front of the digits is consumed as padding, and the facet refuses
// the ones a reader would have counted as part of the amount instead.
TEST(MonetaryChar8, AFillThatWouldChangeTheAmountIsRejectedOnTheWayBackIn)
{
    const monetary<char8_t> obj = facet_for("C");

    auto get = [&obj](char fill, const std::u8string& input, std::u8string& digits)
    {
        ios_base<char8_t> ios;
        ios.fill(fill);
        digits.clear();
        try
        {
            obj.get(input.begin(), input.end(), false, ios, digits);
        }
        catch (const stream_error&)
        {
            return false;
        }
        return true;
    };

    std::u8string digits;

    // "112345" reads as 112345, never as 12345 with a '1' of padding in front.
    EXPECT_FALSE(get(u8'1', u8"112345", digits));
    EXPECT_FALSE(get(u8'9', u8"912345", digits));

    // A leading zero is the one digit that reads the same either way.
    EXPECT_TRUE(get(u8'0', u8"0000012345", digits));
    EXPECT_EQ(digits, u8"12345");

    // With the sign consumed first, a '-' run behind it cannot be read as a
    // second sign.
    EXPECT_TRUE(get(u8'-', u8"-------12345", digits));
    EXPECT_EQ(digits, u8"-12345");

    // Fill that cannot be read into an amount is consumed as it always was.
    EXPECT_TRUE(get(u8'*', u8"*****12345", digits));
    EXPECT_EQ(digits, u8"12345");
    EXPECT_TRUE(get(u8' ', u8"     12345", digits));
    EXPECT_EQ(digits, u8"12345");

    // Nothing consumed means nothing to vet, whatever the stream's fill is: this
    // input does not start with a '9', so the run stops immediately.
    EXPECT_TRUE(get(u8'9', u8"12345", digits));
    EXPECT_EQ(digits, u8"12345");
}

// A `space` slot owes at least one character, so a field that put() wrote with
// one has to be read with one.  A `none` slot owes nothing, and a field written
// from a pattern that ends in one has no space to find.
TEST(MonetaryChar8, ASpaceSlotIsRequiredAndANoneSlotIsNot)
{
    const pattern with_space = {part::sign, part::value, part::space, part::symbol};
    const pattern with_none  = {part::sign, part::value, part::symbol, part::none};

    ios_base<char8_t> ios;

    const monetary<char8_t> spaced(tuned()->fraction(2).point(u8'.').groups({4}).separator(u8',')
                                       .symbol(u8"$").plus(u8"()").both(with_space).ptr());
    expect_parses(spaced, false, ios, u8"(9876.05 $)", u8"987605");
    expect_parses(spaced, false, ios, u8"(9876.05 )", u8"987605");

    const monetary<char8_t> unspaced(tuned()->fraction(2).point(u8'.').groups({4}).separator(u8',')
                                         .symbol(u8"$").plus(u8"()").both(with_none).ptr());
    expect_parses(unspaced, false, ios, u8"(9876.05$)", u8"987605");
    expect_parses(unspaced, false, ios, u8"(9876.05)", u8"987605");

    // The character a `space` slot owes is the stream's fill, so a field written
    // with the default fill and read back under another one is missing it.
    ios_base<char8_t> other_fill;
    other_fill.fill(u8'*');
    expect_rejects(spaced, false, other_fill, u8"(9876.05 $)");
}

// Without showbase the symbol is optional, and a symbol the parse cannot place
// is simply not part of the field: it is left for whoever reads next.
TEST(MonetaryChar8, AnUnplaceableSymbolEndsTheField)
{
    ios_base<char8_t> ios;
    const pattern     trailing = {part::value, part::symbol, part::none, part::sign};

    for (const char8_t* symbol : {u8"$", u8"%", u8"&"})
    {
        SCOPED_TRACE(::testing::PrintToString(symbol));
        const monetary<char8_t> obj(tuned()->fraction(0).symbol(symbol).plus(u8"").minus(u8"")
                                        .both(trailing).ptr());
        expect_parses(obj, false, ios, std::u8string(u8"10") + symbol, u8"10", symbol);
    }
}

// A locale whose sign position is 0 wraps a negative amount in parentheses
// rather than spelling a sign, so the facet has to supply "()" where lconv has
// only the sign string it would otherwise use.
TEST(MonetaryChar8, ASignPositionOfZeroMeansParentheses)
{
    const monetary<char8_t> obj = facet_for("en_HK.UTF-8");
    EXPECT_EQ(obj.negative_sign_nat(), u8"()");
    EXPECT_EQ(obj.negative_sign_int(), u8"()");

    ios_base<char8_t>   ios;
    const std::u8string field = put_str(obj, false, ios, u8"-827364");
    EXPECT_EQ(field.front(), u8'(');
    EXPECT_EQ(field.back(), u8')');
    expect_parses(obj, false, ios, field, u8"-827364");
}

// A `space` slot writes the stream's fill character, not a literal space, and
// leading padding then shifts everything already written -- that run included.
// A forgotten shift would leave the run recorded at the wrong offset, which is
// what the fill check downstream reads.
TEST(MonetaryChar8, PaddingInFrontShiftsTheFillAlreadyWritten)
{
    const monetary<char8_t> obj(tuned()->fraction(0).symbol(u8"$").minus(u8"-")
                                    .negative({part::symbol, part::space, part::sign, part::value})
                                    .ptr());
    ios_base<char8_t> ios;
    ios.setf(ios_defs::showbase);
    ios.fill(u8'*');
    ios.width(10);
    EXPECT_EQ(put_str(obj, false, ios, u8"-12"), u8"*****$*-12");

    // With a fill that reads as a space the same field is legible, and the
    // single character the slot owes is still there when nothing is padded.
    ios_base<char8_t> plain;
    plain.setf(ios_defs::showbase);
    EXPECT_EQ(put_str(obj, false, plain, u8"-12"), u8"$ -12");
}

// The sign is required when the pattern makes its absence unreadable -- it opens
// the field, or a space follows where the sign would have been.  A field that
// then arrives without one is not an amount.
TEST(MonetaryChar8, APatternCanMakeTheSignMandatory)
{
    ios_base<char8_t> ios;

    const monetary<char8_t> leading(tuned()->fraction(0).symbol(u8"$").plus(u8"+").minus(u8"-")
                                        .both({part::sign, part::symbol, part::value, part::none})
                                        .ptr());
    expect_parses(leading, false, ios, u8"+$12", u8"12");
    expect_parses(leading, false, ios, u8"-$12", u8"-12");
    expect_rejects(leading, false, ios, u8"$12");

    const monetary<char8_t> spaced(tuned()->fraction(0).symbol(u8"$").plus(u8"+").minus(u8"-")
                                       .both({part::symbol, part::sign, part::space, part::value})
                                       .ptr());
    expect_parses(spaced, false, ios, u8"$+ 12", u8"12");
    expect_rejects(spaced, false, ios, u8"$ 12");
}

// Only the sign's first character sits in the sign slot; the rest trails the
// field.  A field that starts one and does not finish it is not an amount.
TEST(MonetaryChar8, AnUnfinishedMultiCharacterSignIsRejected)
{
    ios_base<char8_t>       ios;
    const monetary<char8_t> obj(tuned()->fraction(0).minus(u8"-->").plus(u8"")
                                    .both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, u8"-12->", u8"-12");
    expect_rejects(obj, false, ios, u8"-12-");
    expect_rejects(obj, false, ios, u8"-12");
}

// Everything above works in the national form.  The international one is a
// separate set of punctuation reached by a separate branch at every entry
// point, so the round trip is run through it too.
TEST(MonetaryChar8, TheInternationalFormRoundTripsAsWell)
{
    const monetary<char8_t> obj(tuned()->fraction(2).groups({3}).separator(u8',')
                                    .symbol(u8"$").plus(u8"").minus(u8"-")
                                    .both(kSymbolSignValue).ptr());

    const std::u8string amounts[] = {u8"0", u8"827364", u8"-827364", u8"-1"};

    for (bool intl : {false, true})
        for (const std::u8string& amount : amounts)
        {
            SCOPED_TRACE(trace_case(0, 0, intl, amount));
            ios_base<char8_t>    writer;
            const std::u8string field = put_str(obj, intl, writer, amount);
            ASSERT_FALSE(field.empty());

            ios_base<char8_t> reader;
            expect_parses(obj, intl, reader, field, amount);

            // And the same field read straight into an integer.
            int64_t units = 0;
            obj.get(field.begin(), field.end(), intl, reader, units);
            EXPECT_EQ(to_digits(units), amount);
        }
}

// A `space` slot takes the whole internal spread when there is one, rather than
// the single character it owes when there is not.
TEST(MonetaryChar8, InternalPaddingFillsTheSpaceSlot)
{
    const monetary<char8_t> obj(tuned()->fraction(0).symbol(u8"$").minus(u8"-")
                                    .negative({part::symbol, part::space, part::sign, part::value})
                                    .ptr());
    ios_base<char8_t> ios;
    ios.setf(ios_defs::showbase);
    ios.setf(ios_defs::internal, ios_defs::adjustfield);
    ios.fill(u8'*');
    ios.width(9);
    EXPECT_EQ(put_str(obj, false, ios, u8"-12"), u8"$*****-12");
}

// A field that starts the symbol and does not finish it has not written the
// symbol, so with showbase set there is nothing for the required slot to match.
TEST(MonetaryChar8, APartiallyMatchedSymbolIsNotTheSymbol)
{
    const monetary<char8_t> obj(tuned()->fraction(0).symbol(u8"USD").plus(u8"").minus(u8"-")
                                    .both(kSymbolSignValue).ptr());

    ios_base<char8_t> required;
    required.setf(ios_defs::showbase);
    expect_parses(obj, false, required, u8"USD12", u8"12");
    expect_rejects(obj, false, required, u8"US12");

    // Without showbase the half-written symbol is simply not part of the field.
    ios_base<char8_t> optional;
    expect_rejects(obj, false, optional, u8"US12");
}

// Leading zeros are stripped from the digits, and the sign has to be put back in
// front of what is left rather than in front of what was parsed.
TEST(MonetaryChar8, ANegativeAmountKeepsItsSignAfterLeadingZerosAreStripped)
{
    const monetary<char8_t> obj(tuned()->fraction(2).point(u8'.').plus(u8"").minus(u8"-")
                                    .both(kSymbolSignValue).ptr());
    ios_base<char8_t> ios;

    expect_parses(obj, false, ios, u8"-0.01", u8"-1");
    expect_parses(obj, false, ios, u8"-000.10", u8"-10");
    expect_parses(obj, false, ios, u8"-0.00", u8"0");
    expect_parses(obj, false, ios, u8"0.00", u8"0");
}

// A field with no digits at all cannot become an integer either, and the target
// is left as the caller had it.
TEST(MonetaryChar8, AFieldWithNoDigitsIsNotAnInteger)
{
    const monetary<char8_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    ios_base<char8_t>       ios;

    const std::u8string input = u8"no digits here";
    int64_t            units = 4242;
    EXPECT_THROW((void)obj.get(input.begin(), input.end(), false, ios, units), stream_error);
    EXPECT_EQ(units, 4242);
}
