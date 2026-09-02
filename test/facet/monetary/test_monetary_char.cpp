// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * IOv2::monetary<char>: assembling and reading back a currency field.
 *
 * The algorithm is the one documented on monetary.h's insert() and extract().
 * A field is four slots in the order the locale's pattern gives -- symbol, sign,
 * value, and a space or nothing -- filled from that locale's punctuation; the
 * amount is a run of digits cut frac_digits places from its right-hand end, with
 * the integral part grouped by thousands_sep.  Reading is the same walk in
 * reverse, and the two have to agree.
 *
 * Almost every case below drives that algorithm through a configuration it sets
 * itself rather than through an installed locale.  A pattern is a pattern
 * whoever asked for it, and stating the format in the test is what makes the
 * expected string readable -- and what keeps the case from failing the next time
 * glibc revises what de_DE's currency looks like.
 *
 * get() is written against a sentinel so it can read a stream it cannot back up
 * in.  Every parse here is therefore run twice, once over a string's iterators
 * and once over an istreambuf_iterator, and the two are required to agree on
 * both the digits and where they stopped.
 */
#include <facet/monetary.h>
#include <facet/monetary_details.h>

#include <common/defs.h>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using namespace IOv2;

namespace
{
    using part    = base_ft<monetary>::part;
    using pattern = base_ft<monetary>::pattern;

    // A configuration whose every answer can be set.  What a format case is
    // about is the format, so it states one here instead of borrowing whichever
    // one a locale happens to carry this year.
    class tunable_conf : public monetary_conf<char>,
                         public std::enable_shared_from_this<tunable_conf>
    {
    public:
        explicit tunable_conf(const std::string& name = "C")
            : monetary_conf<char>(name)
            , m_decimal_point(monetary_conf<char>::decimal_point())
            , m_thousands_sep(monetary_conf<char>::thousands_sep())
            , m_grouping(monetary_conf<char>::grouping())
            , m_curr_symbol(monetary_conf<char>::curr_symbol_nat())
            , m_positive_sign(monetary_conf<char>::positive_sign_nat())
            , m_negative_sign(monetary_conf<char>::negative_sign_nat())
            , m_frac_digits(monetary_conf<char>::frac_digits_nat())
            , m_pos_format(monetary_conf<char>::pos_format_nat())
            , m_neg_format(monetary_conf<char>::neg_format_nat())
        {}

        char decimal_point() const override { return m_decimal_point; }
        char thousands_sep() const override { return m_thousands_sep; }
        const std::vector<uint8_t>& grouping() const override { return m_grouping; }
        const std::string& curr_symbol_nat() const override { return m_curr_symbol; }
        const std::string& positive_sign_nat() const override { return m_positive_sign; }
        const std::string& negative_sign_nat() const override { return m_negative_sign; }
        int frac_digits_nat() const override { return m_frac_digits; }
        const pattern& pos_format_nat() const override { return m_pos_format; }
        const pattern& neg_format_nat() const override { return m_neg_format; }

        tunable_conf& point(char c)                       { m_decimal_point = c; return *this; }
        tunable_conf& separator(char c)                   { m_thousands_sep = c; return *this; }
        tunable_conf& groups(std::vector<uint8_t> g)      { m_grouping = std::move(g); return *this; }
        tunable_conf& symbol(std::string s)               { m_curr_symbol = std::move(s); return *this; }
        tunable_conf& plus(std::string s)                 { m_positive_sign = std::move(s); return *this; }
        tunable_conf& minus(std::string s)                { m_negative_sign = std::move(s); return *this; }
        tunable_conf& fraction(int n)                     { m_frac_digits = n; return *this; }
        tunable_conf& positive(pattern p)                 { m_pos_format = p; return *this; }
        tunable_conf& negative(pattern p)                 { m_neg_format = p; return *this; }
        tunable_conf& both(pattern p)                     { return positive(p).negative(p); }

        // Ends a chain: the facet's constructor takes the configuration by
        // shared pointer, and the chain has been handing back references.
        std::shared_ptr<tunable_conf> ptr()               { return shared_from_this(); }

    private:
        char                 m_decimal_point;
        char                 m_thousands_sep;
        std::vector<uint8_t> m_grouping;
        std::string          m_curr_symbol;
        std::string          m_positive_sign;
        std::string          m_negative_sign;
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

    monetary<char> facet_for(const char* loc)
    {
        return monetary<char>(std::make_shared<monetary_conf<char>>(loc));
    }

    // std::to_string is narrow; an amount reaches the facet in the character
    // type under test.
    std::string to_digits(int64_t v)
    {
        const std::string ascii = std::to_string(v);
        return std::string(ascii.begin(), ascii.end());
    }

    // Takes either spelling of an amount -- the digit string or the integer --
    // because put() has an overload for each and they must agree.
    template <typename TVal>
    std::string put_str(const monetary<char>& obj, bool intl, ios_base<char>& io,
                        const TVal& amount)
    {
        std::string out;
        obj.put(std::back_inserter(out), intl, io, amount);
        return out;
    }

    // The round-trip case names its combination on one line, so the sibling
    // files' literal retyping cannot reach these labels.
    std::string trace_case(int frac, std::size_t groups, bool showbase, const std::string& amount)
    {
        return "frac=" + std::to_string(frac) + " groups=" + std::to_string(groups) + " showbase=" + std::to_string(showbase) + " amount=" + ::testing::PrintToString(amount);
    }

    // What a parse produced: whether it succeeded, the digits it yielded, and
    // the input it left behind.
    struct parse_result
    {
        bool        ok;
        std::string digits;
        std::string rest;
    };

    parse_result parse_over_pointers(const monetary<char>& obj, bool intl, ios_base<char>& io,
                                     const std::string& input, const std::string& seed)
    {
        parse_result res{true, seed, {}};
        try
        {
            auto it  = obj.get(input.begin(), input.end(), intl, io, res.digits);
            res.rest = std::string(it, input.end());
        }
        catch (const stream_error&)
        {
            res.ok = false;
        }
        return res;
    }

    // The same parse over an iterator that cannot be compared to anything but a
    // sentinel and cannot be rewound -- the shape get() is actually written for.
    parse_result parse_over_a_stream(const monetary<char>& obj, bool intl, ios_base<char>& io,
                                     const std::string& input, const std::string& seed)
    {
        parse_result res{true, seed, {}};
        streambuf    sb(mem_device{input});
        auto         beg = istreambuf_iterator(sb);
        try
        {
            auto it = obj.get(beg, std::default_sentinel, intl, io, res.digits);
            res.rest = std::string(it, decltype(it)());
        }
        catch (const stream_error&)
        {
            res.ok = false;
        }
        return res;
    }

    // Every parse assertion goes through here, so no case can check one iterator
    // shape and leave the other unexamined.
    void expect_parses(const monetary<char>& obj, bool intl, ios_base<char>& io,
                       const std::string& input, const std::string& digits,
                       const std::string& rest = "")
    {
        SCOPED_TRACE(::testing::PrintToString(input));
        for (bool streamed : {false, true})
        {
            SCOPED_TRACE(streamed ? "streambuf iterator" : "string iterator");
            const parse_result r = streamed ? parse_over_a_stream(obj, intl, io, input, "")
                                            : parse_over_pointers(obj, intl, io, input, "");
            EXPECT_TRUE(r.ok);
            EXPECT_EQ(r.digits, digits);
            EXPECT_EQ(r.rest, rest);
        }
    }

    // A failed parse throws, and the digit string it was handed must come back
    // exactly as it was: the caller's variable is not a scratch buffer.
    void expect_rejects(const monetary<char>& obj, bool intl, ios_base<char>& io,
                        const std::string& input)
    {
        SCOPED_TRACE(::testing::PrintToString(input));
        const std::string seed = "untouched";
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

TEST(MonetaryChar, TheCharacterTypeIsChar)
{
    static_assert(std::is_same_v<monetary<char>::char_type, char>);
}

// [locale.moneypunct] fixes the "C" locale completely: no currency, no
// grouping, no fractional digits, and the same format both ways round.
TEST(MonetaryChar, TheCLocaleCarriesNoCurrency)
{
    const monetary<char> obj = facet_for("C");

    EXPECT_EQ(obj.decimal_point(), '.');
    EXPECT_EQ(obj.thousands_sep(), ',');
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
TEST(MonetaryChar, TheCLocaleStillHasANegativeSign)
{
    const monetary<char> obj = facet_for("C");
    EXPECT_FALSE(obj.negative_sign_nat().empty());
    EXPECT_FALSE(obj.negative_sign_int().empty());
}

// Every spelling whose language segment is C or POSIX names the same locale.
// glibc normalises the codeset before lookup, so a name matched exactly used to
// fall through to localeconv() and take the all-unavailable monetary data
// verbatim -- losing the negative sign above with it.
TEST(MonetaryChar, EverySpellingOfTheCLocaleCarriesTheSameData)
{
    const monetary<char> plain = facet_for("C");

    for (const char* name : {"POSIX", "C.UTF-8", "C.utf8", "C.UTF8", "C.UTF-8@euro"})
    {
        SCOPED_TRACE(name);
        const monetary<char> obj = facet_for(name);
        EXPECT_EQ(obj.decimal_point(),     plain.decimal_point());
        EXPECT_EQ(obj.thousands_sep(),     plain.thousands_sep());
        EXPECT_EQ(obj.grouping(),          plain.grouping());
        EXPECT_EQ(obj.curr_symbol_nat(),   plain.curr_symbol_nat());
        EXPECT_EQ(obj.curr_symbol_int(),   plain.curr_symbol_int());
        EXPECT_EQ(obj.positive_sign_nat(), plain.positive_sign_nat());
        EXPECT_EQ(obj.positive_sign_int(), plain.positive_sign_int());
        EXPECT_EQ(obj.negative_sign_nat(), plain.negative_sign_nat());
        EXPECT_EQ(obj.negative_sign_int(), plain.negative_sign_int());
        EXPECT_EQ(obj.frac_digits_nat(),   plain.frac_digits_nat());
        EXPECT_EQ(obj.frac_digits_int(),   plain.frac_digits_int());
        EXPECT_EQ(obj.pos_format_nat(),    plain.pos_format_nat());
        EXPECT_EQ(obj.neg_format_nat(),    plain.neg_format_nat());
        EXPECT_EQ(obj.pos_format_int(),    plain.pos_format_int());
        EXPECT_EQ(obj.neg_format_int(),    plain.neg_format_int());
    }
}

// A C or POSIX language segment does not make the whole name legal.  newlocale
// rejects each of these, so the constructor has to as well rather than quietly
// handing back the defaults above.
TEST(MonetaryChar, ANameTheSystemRejectsIsNotTheCLocale)
{
    for (const char* name : {"C.BOGUS", "C@euro", "C.", "C@", "POSIX.utf8", "POSIX@x"})
    {
        SCOPED_TRACE(name);
        EXPECT_THROW(facet_for(name), cvt_error);
    }
}

TEST(MonetaryChar, ALocaleWithCurrencyDataDiffersFromTheCLocale)
{
    const monetary<char> plain = facet_for("C");
    const monetary<char> us    = facet_for("en_US.UTF-8");

    EXPECT_NE(us.curr_symbol_nat(), plain.curr_symbol_nat());
    EXPECT_NE(us.frac_digits_nat(), plain.frac_digits_nat());
    EXPECT_NE(us.grouping(), plain.grouping());
}

// The pattern is not stored by the locale: it is built from the three POSIX
// lconv flags sign_posn, cs_precedes and sep_by_space.  These four locales are
// the ones that reach the sign_posn cases 2, 3 and 4 -- with and without the
// separating space -- and the fifth is the hard-coded default the C locale
// takes without consulting lconv at all.
TEST(MonetaryChar, ThePatternIsBuiltFromThePosixSignPosition)
{
    const std::pair<const char*, pattern> cases[] = {
        {"ar_AE.UTF-8", {part::symbol, part::space, part::value, part::sign}},
        {"nn_NO.UTF-8", {part::sign, part::symbol, part::value, part::none}},
        {"da_DK.UTF-8", {part::symbol, part::sign, part::space, part::value}},
        {"bo_CN.UTF-8", {part::symbol, part::sign, part::value, part::none}},
        {"C.utf8",      {part::symbol, part::sign, part::none, part::value}},
    };

    for (const auto& [name, expected] : cases)
    {
        SCOPED_TRACE(name);
        EXPECT_EQ(facet_for(name).neg_format_nat(), expected);
    }
}

// The digits are the smallest units of the currency, so the amount they spell is
// read off their right-hand end: frac_digits places go behind the decimal point
// and the rest in front of it.
TEST(MonetaryChar, TheAmountIsCutFracDigitsPlacesFromTheRight)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(2).both(kSymbolSignValue).ptr());

    EXPECT_EQ(put_str(obj, false, ios, "827364"), "8273.64");
    EXPECT_EQ(put_str(obj, false, ios, "1"), ".01");
    EXPECT_EQ(put_str(obj, false, ios, "12"), ".12");
    EXPECT_EQ(put_str(obj, false, ios, "123"), "1.23");
}

// A run too short to reach the cut has no integral part at all, and the fraction
// picks up the shortfall as leading zeros: two places turn 7 into .07, not 7.0.
TEST(MonetaryChar, AShortAmountIsPaddedInTheFractionNotTheInteger)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(3).both(kSymbolSignValue).ptr());

    EXPECT_EQ(put_str(obj, false, ios, "7"), ".007");
    EXPECT_EQ(put_str(obj, false, ios, "70"), ".070");
    EXPECT_EQ(put_str(obj, false, ios, "700"), ".700");
    EXPECT_EQ(put_str(obj, false, ios, "7000"), "7.000");
}

TEST(MonetaryChar, NoFractionalDigitsMeansNoDecimalPoint)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(obj, false, ios, "123456"), "123456");
}

// A negative frac_digits asks for no fraction at all and keeps the whole run,
// which is not the same statement as zero places: it is the locale saying the
// question does not apply.
TEST(MonetaryChar, ANegativeFractionWidthKeepsEveryDigitIntegral)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(-1).both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(obj, false, ios, "123456"), "123456");
}

TEST(MonetaryChar, GroupingInsertsTheThousandsSeparator)
{
    ios_base<char> ios;

    const monetary<char> threes(tuned()->fraction(0).groups({3}).separator(',')
                                       .both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(threes, false, ios, "1234567"), "1,234,567");

    const monetary<char> ones(tuned()->fraction(0).groups({1}).separator('#')
                                     .both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(ones, false, ios, "1234"), "1#2#3#4");

    // A grouping vector is read right to left and its last entry repeats, so
    // {3,2} groups three digits then twos all the way up.
    const monetary<char> indian(tuned()->fraction(0).groups({3, 2}).separator(',')
                                       .both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(indian, false, ios, "12345678"), "1,23,45,678");
}

TEST(MonetaryChar, AnEmptyGroupingInsertsNothing)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).groups({}).both(kSymbolSignValue).ptr());
    const std::string    digits(300, '1');
    EXPECT_EQ(put_str(obj, false, ios, digits), digits);
}

// The symbol is the one part of the field the caller decides about: it is
// written when showbase is set and left out otherwise, and nothing else about
// the field changes with it.
TEST(MonetaryChar, TheSymbolIsWrittenOnlyWithShowbase)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(3).symbol("@").both(kSymbolSignValue).ptr());

    EXPECT_EQ(put_str(obj, false, ios, "482715"), "482.715");
    ios.setf(ios_defs::showbase);
    EXPECT_EQ(put_str(obj, false, ios, "482715"), "@482.715");
    ios.unsetf(ios_defs::showbase);
    EXPECT_EQ(put_str(obj, false, ios, "482715"), "482.715");
}

TEST(MonetaryChar, ThePatternDecidesTheOrderOfTheParts)
{
    ios_base<char> ios;
    ios.setf(ios_defs::showbase);

    const std::pair<pattern, const char*> cases[] = {
        {{part::symbol, part::sign, part::value, part::none}, "$-12"},
        {{part::sign, part::symbol, part::value, part::none}, "-$12"},
        {{part::value, part::space, part::symbol, part::sign}, "12 $-"},
        {{part::sign, part::value, part::space, part::symbol}, "-12 $"},
        {{part::symbol, part::space, part::value, part::sign}, "$ 12-"},
    };

    for (const auto& [order, expected] : cases)
    {
        SCOPED_TRACE(::testing::PrintToString(expected));
        const monetary<char> obj(tuned()->fraction(0).symbol("$").minus("-").negative(order).ptr());
        EXPECT_EQ(put_str(obj, false, ios, "-12"), expected);
    }
}

// A sign spelled with more than one character wraps the field: its first
// character sits in the sign slot and the rest trails everything, which is how
// a locale writes a negative amount in parentheses.
TEST(MonetaryChar, AMultiCharacterSignWrapsTheField)
{
    ios_base<char> ios;
    ios.setf(ios_defs::showbase);
    const monetary<char> obj(tuned()->fraction(2).groups({3}).separator(',').symbol("$")
                                    .minus("()")
                                    .negative({part::symbol, part::space, part::sign, part::value}).ptr());

    EXPECT_EQ(put_str(obj, false, ios, "-827364"), "$ (8,273.64)");
}

TEST(MonetaryChar, TheSignOfTheAmountChoosesThePattern)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).plus("+").minus("-")
                                    .positive({part::sign, part::value, part::none, part::none})
                                    .negative({part::value, part::sign, part::none, part::none}).ptr());

    EXPECT_EQ(put_str(obj, false, ios, "12"), "+12");
    EXPECT_EQ(put_str(obj, false, ios, "-12"), "12-");
}

// The international and national sets are independent, and intl is what picks
// between them: the same amount through one facet has two spellings.
TEST(MonetaryChar, TheInternationalFlagSelectsTheOtherPunctuation)
{
    ios_base<char> ios;
    ios.setf(ios_defs::showbase);
    const monetary<char> obj = facet_for("en_US.UTF-8");

    EXPECT_NE(obj.curr_symbol_int(), obj.curr_symbol_nat());
    const std::string national      = put_str(obj, false, ios, "123456");
    const std::string international = put_str(obj, true, ios, "123456");
    EXPECT_NE(national, international);
    EXPECT_NE(national.find(obj.curr_symbol_nat()), std::string::npos);
    EXPECT_NE(international.find(obj.curr_symbol_int()), std::string::npos);
}

TEST(MonetaryChar, AShortFieldIsPaddedToTheWidth)
{
    const monetary<char> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());

    ios_base<char> ios;
    ios.fill('*');
    ios.width(8);
    EXPECT_EQ(put_str(obj, false, ios, "123"), "*****123");

    ios.width(8);
    ios.setf(ios_defs::left, ios_defs::adjustfield);
    EXPECT_EQ(put_str(obj, false, ios, "123"), "123*****");
}

// Under internal the shortfall is not tacked onto an end: it goes into whichever
// pattern slot writes nothing of its own, which is what puts the fill between
// the symbol and the amount rather than outside them.
TEST(MonetaryChar, InternalPaddingGoesIntoTheEmptySlot)
{
    const monetary<char> obj(tuned()->fraction(0).symbol("$").minus("-")
                                    .negative({part::symbol, part::none, part::sign, part::value}).ptr());

    ios_base<char> ios;
    ios.setf(ios_defs::showbase);
    ios.setf(ios_defs::internal, ios_defs::adjustfield);
    ios.fill('*');
    ios.width(9);
    EXPECT_EQ(put_str(obj, false, ios, "-123"), "$****-123");
}

// width() is one-shot: the field it sized is the only one it sizes.
TEST(MonetaryChar, TheWidthIsConsumedByOnePut)
{
    const monetary<char> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    ios_base<char>       ios;
    ios.fill('*');
    ios.width(8);

    EXPECT_EQ(put_str(obj, false, ios, "123"), "*****123");
    EXPECT_EQ(ios.width(), 0u);
    EXPECT_EQ(put_str(obj, false, ios, "123"), "123");
}

// A run of fill can end up where a reader would take it for part of the amount.
// The facet refuses rather than write a field that reads as a different number.
// The C pattern is {symbol, sign, none, value}, so internal padding lands
// directly in front of the digits.
TEST(MonetaryChar, AFillThatWouldChangeTheAmountIsRejected)
{
    const monetary<char> obj = facet_for("C");

    auto put = [&obj](char fill, ios_defs::fmtflags adjust, const std::string& digits)
    {
        ios_base<char> ios;
        ios.fill(fill);
        ios.width(14);
        ios.setf(adjust, ios_defs::adjustfield);
        std::string out;
        try
        {
            obj.put(std::back_inserter(out), false, ios, digits);
        }
        catch (const stream_error&)
        {
            return std::string();
        }
        return out;
    };

    // A '0' in front of the digits is a leading zero and reads as the same
    // amount; behind them it would read as 12345000000000.
    EXPECT_EQ(put('0', ios_defs::internal, "12345"), "00000000012345");
    EXPECT_EQ(put('0', ios_defs::internal, "-12345"), "-0000000012345");
    EXPECT_EQ(put('0', ios_defs::left, "12345"), "");

    // Any other digit is dangerous wherever it lands.
    EXPECT_EQ(put('1', ios_defs::internal, "12345"), "");
    EXPECT_EQ(put('9', ios_defs::right, "12345"), "");

    // The C locale's negative sign is "-" and its positive sign is empty, so a
    // '-' in front of a positive amount turns it negative to reader and parser
    // alike; padding an amount that is already negative changes nothing.
    EXPECT_EQ(put('-', ios_defs::internal, "12345"), "");
    EXPECT_EQ(put('-', ios_defs::internal, "-12345"), "---------12345");
    EXPECT_EQ(put('+', ios_defs::internal, "12345"), "+++++++++12345");

    // The decimal point binds to the digits after it.
    EXPECT_EQ(put('.', ios_defs::internal, "12345"), "");
    EXPECT_EQ(put('.', ios_defs::left, "12345"), "12345.........");

    // Characters that cannot be read into an amount stay allowed, the thousands
    // separator among them.
    EXPECT_EQ(put('*', ios_defs::internal, "12345"), "*********12345");
    EXPECT_EQ(put(',', ios_defs::internal, "12345"), ",,,,,,,,,12345");
    EXPECT_EQ(put(' ', ios_defs::right, "12345"), "         12345");
}

// fill is sticky stream state, so a stream carrying a dangerous one has to keep
// working for every field whose width leaves nothing to pad.
TEST(MonetaryChar, ADangerousFillIsHarmlessWhenNothingIsPadded)
{
    const monetary<char> obj = facet_for("C");
    ios_base<char>       ios;
    ios.fill('1');
    EXPECT_EQ(put_str(obj, false, ios, "12345"), "12345");
}

// The amount runs up to the first character that is not a digit; what the caller
// put after that is not the facet's to format.  With nothing to format at all,
// nothing is written.
TEST(MonetaryChar, WhatIsNotADigitIsNotAnAmount)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());

    EXPECT_EQ(put_str(obj, false, ios, "42 apples"), "42");
    EXPECT_EQ(put_str(obj, false, ios, "-A"), "");
    EXPECT_EQ(put_str(obj, false, ios, ""), "");
    EXPECT_EQ(put_str(obj, false, ios, "-"), "");
}

TEST(MonetaryChar, AnIntegralValueFormatsLikeItsDigitString)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(2).groups({3}).separator(',')
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
TEST(MonetaryChar, PutReturnsThePositionAfterTheField)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());

    std::string buffer(13, '^');
    auto        it = obj.put(buffer.begin() + 2, false, ios, std::string("2607"));

    EXPECT_EQ(it, buffer.begin() + 6);
    EXPECT_EQ(buffer, "^^2607^^^^^^^");
}

// Everything above reads the field back through the same facet that wrote it.
// The two directions are separate code, so this is the case that ties them:
// whatever put() produced, get() has to return the amount put() was given.
TEST(MonetaryChar, WhatPutWritesGetReadsBack)
{
    const std::vector<uint8_t> groupings[] = {{}, {3}, {1}, {3, 2}};
    const std::string          amounts[]   = {"0", "1", "12", "827364", "-1", "-827364",
                                              "98765432109", "-98765432109"};


    for (int frac : {0, 2, 3})
        for (const std::vector<uint8_t>& g : groupings)
            for (bool showbase : {false, true})
            {
                const monetary<char> obj(tuned()->fraction(frac).groups(g).separator(',')
                                                .symbol("$").plus("").minus("-")
                                                .both(kSymbolSignValue).ptr());
                ios_base<char> ios;
                if (showbase) ios.setf(ios_defs::showbase);

                for (const std::string& amount : amounts)
                {
                    SCOPED_TRACE(trace_case(frac, g.size(), showbase, amount));
                    ios_base<char> writer;
                    if (showbase) writer.setf(ios_defs::showbase);
                    const std::string field = put_str(obj, false, writer, amount);
                    ASSERT_FALSE(field.empty());
                    expect_parses(obj, false, ios, field, amount);
                }
            }
}

// Parsing ends at the first character the format has no place for, and what is
// left is the caller's to read next.
TEST(MonetaryChar, ParsingStopsAtTheFirstForeignCharacter)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, "1 apple", "1", " apple");
    expect_parses(obj, false, ios, "123abc", "123", "abc");
}

// With grouping switched off the separator is not part of an amount, so it ends
// one rather than continuing it.
TEST(MonetaryChar, ASeparatorEndsTheAmountWhenThereIsNoGrouping)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).groups({}).separator(',')
                                    .both(kSymbolSignValue).ptr());
    expect_parses(obj, false, ios, "742,908", "742", ",908");
}

// Likewise the decimal point, when the locale has no fractional digits to put
// behind it.
TEST(MonetaryChar, ADecimalPointEndsTheAmountWhenThereIsNoFraction)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).point('.').both(kSymbolSignValue).ptr());
    expect_parses(obj, false, ios, "742.908", "742", ".908");
}

TEST(MonetaryChar, AnEmptySequenceIsNotAnAmount)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    expect_rejects(obj, false, ios, "");
}

TEST(MonetaryChar, TextThatIsNotAnAmountIsRejected)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    expect_rejects(obj, false, ios, "nothing numeric");
    expect_rejects(obj, false, ios, "a sentence with no amount anywhere in it");
}

// A fraction is all or nothing: exactly frac_digits places, or the field is not
// an amount in this locale.
TEST(MonetaryChar, TheFractionMustHaveExactlyFracDigitsPlaces)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(4).point('.').both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, "73.5926", "735926");
    expect_rejects(obj, false, ios, "73.59261");
    expect_rejects(obj, false, ios, "73.592");
    expect_rejects(obj, false, ios, "73.");

    // No decimal point at all is not a short fraction: it is an amount with none.
    expect_parses(obj, false, ios, "73", "73");
}

TEST(MonetaryChar, ASecondDecimalPointIsNotPartOfTheAmount)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(2).point(':').both(kSymbolSignValue).ptr());
    expect_rejects(obj, false, ios, "47::2");
}

// The separators have to fall where this locale's grouping puts them.  A field
// grouped some other way is a field from some other locale.
TEST(MonetaryChar, TheSeparatorsMustFollowTheGrouping)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).groups({2}).separator('#')
                                    .both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, "7#06#45", "70645");
    expect_rejects(obj, false, ios, "007#06#45");
    expect_rejects(obj, false, ios, "7#06##45");
}

// A locale that spells a positive sign but no negative one leaves the absence of
// a sign to mean negative, which is what [locale.money.get] asks for.
TEST(MonetaryChar, NoSignMeansNegativeWhenOnlyThePositiveSignIsSpelled)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).plus("+").minus("")
                                    .both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, "69", "-69");
    expect_parses(obj, false, ios, "+69", "69");
}

TEST(MonetaryChar, ASignInTheLastSlotIsStillFound)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).plus("+").minus("-")
                                    .both({part::value, part::space, part::symbol, part::sign}).ptr());

    expect_parses(obj, false, ios, "123 +", "123");
    expect_parses(obj, false, ios, "123 -", "-123");
}

// With showbase the symbol is part of the field and has to be there.  Without
// it the symbol is optional -- but a symbol that is present is still consumed,
// or the parse would stop in the middle of a field it could read.
TEST(MonetaryChar, ShowbaseDecidesWhetherTheSymbolIsRequired)
{
    const monetary<char> obj(tuned()->fraction(0).symbol("$").minus("-")
                                    .both(kSymbolSignValue).ptr());

    ios_base<char> required;
    required.setf(ios_defs::showbase);
    expect_parses(obj, false, required, "$123", "123");
    expect_rejects(obj, false, required, "123");

    ios_base<char> optional;
    expect_parses(obj, false, optional, "$123", "123");
    expect_parses(obj, false, optional, "123", "123");
}

// A field with a symbol and no digits is not an amount, whichever way round the
// symbol is required.
TEST(MonetaryChar, ASymbolWithoutDigitsIsNotAnAmount)
{
    const monetary<char> obj(tuned()->fraction(0).symbol("$").minus("-")
                                    .both(kSymbolSignValue).ptr());

    ios_base<char> ios;
    ios.setf(ios_defs::showbase);
    expect_rejects(obj, false, ios, "$");
    expect_rejects(obj, false, ios, "$-");
}

// The fraction alone is an amount: the integral part may be empty as long as the
// places behind the point are all there.
TEST(MonetaryChar, AnAmountMayBeAllFraction)
{
    const monetary<char> obj(tuned()->fraction(3).point('.').symbol("@").minus("-")
                                    .both(kSymbolSignValue).ptr());

    ios_base<char> ios;
    expect_parses(obj, false, ios, "@.000 ", "0", " ");
    expect_parses(obj, false, ios, "@-.042 ", "-42", " ");
}

TEST(MonetaryChar, AnAmountTooLargeForTheTargetTypeIsRejected)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).groups({}).both(kSymbolSignValue).ptr());
    const std::string    huge(40, '9');

    int64_t     units = 0;
    std::string digits;
    EXPECT_THROW((void)obj.get(huge.begin(), huge.end(), false, ios, units), stream_error);

    // The same field is a perfectly good digit string, though: it is only the
    // conversion to a fixed-width integer that cannot hold it.
    EXPECT_NO_THROW((void)obj.get(huge.begin(), huge.end(), false, ios, digits));
    EXPECT_EQ(digits, huge);
}

TEST(MonetaryChar, GettingAnIntegralValueAgreesWithGettingTheDigits)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(2).groups({3}).separator(',')
                                    .both(kSymbolSignValue).ptr());

    for (const char* field : {"1,234.56", "-1,234.56", ".01", "-.01", "0.00"})
    {
        SCOPED_TRACE(::testing::PrintToString(field));
        const std::string input(field);

        std::string digits;
        obj.get(input.begin(), input.end(), false, ios, digits);

        int64_t units = 0;
        obj.get(input.begin(), input.end(), false, ios, units);

        EXPECT_EQ(to_digits(units), digits);
    }
}

// put() writes through an output iterator, so an iterator that reaches a stream
// rather than a container has to work as the destination too.
TEST(MonetaryChar, PutWritesThroughAnOutputIteratorOntoAStream)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(2).groups({3}).separator(',')
                                    .both(kSymbolSignValue).ptr());

    streambuf sb{mem_device<char>{""}};
    obj.put(ostreambuf_iterator(sb), false, ios, std::string("123456"));
    sb.flush();
    EXPECT_EQ(sb.device().str(), "1,234.56");
}

// The same fill vetting as on the writing side, but from the reader's end: a run
// of fill in front of the digits is consumed as padding, and the facet refuses
// the ones a reader would have counted as part of the amount instead.
TEST(MonetaryChar, AFillThatWouldChangeTheAmountIsRejectedOnTheWayBackIn)
{
    const monetary<char> obj = facet_for("C");

    auto get = [&obj](char fill, const std::string& input, std::string& digits)
    {
        ios_base<char> ios;
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

    std::string digits;

    // "112345" reads as 112345, never as 12345 with a '1' of padding in front.
    EXPECT_FALSE(get('1', "112345", digits));
    EXPECT_FALSE(get('9', "912345", digits));

    // A leading zero is the one digit that reads the same either way.
    EXPECT_TRUE(get('0', "0000012345", digits));
    EXPECT_EQ(digits, "12345");

    // With the sign consumed first, a '-' run behind it cannot be read as a
    // second sign.
    EXPECT_TRUE(get('-', "-------12345", digits));
    EXPECT_EQ(digits, "-12345");

    // Fill that cannot be read into an amount is consumed as it always was.
    EXPECT_TRUE(get('*', "*****12345", digits));
    EXPECT_EQ(digits, "12345");
    EXPECT_TRUE(get(' ', "     12345", digits));
    EXPECT_EQ(digits, "12345");

    // Nothing consumed means nothing to vet, whatever the stream's fill is: this
    // input does not start with a '9', so the run stops immediately.
    EXPECT_TRUE(get('9', "12345", digits));
    EXPECT_EQ(digits, "12345");
}

// A `space` slot owes at least one character, so a field that put() wrote with
// one has to be read with one.  A `none` slot owes nothing, and a field written
// from a pattern that ends in one has no space to find.
TEST(MonetaryChar, ASpaceSlotIsRequiredAndANoneSlotIsNot)
{
    const pattern with_space = {part::sign, part::value, part::space, part::symbol};
    const pattern with_none  = {part::sign, part::value, part::symbol, part::none};

    ios_base<char> ios;

    const monetary<char> spaced(tuned()->fraction(2).point('.').groups({4}).separator(',')
                                       .symbol("$").plus("()").both(with_space).ptr());
    expect_parses(spaced, false, ios, "(9876.05 $)", "987605");
    expect_parses(spaced, false, ios, "(9876.05 )", "987605");

    const monetary<char> unspaced(tuned()->fraction(2).point('.').groups({4}).separator(',')
                                         .symbol("$").plus("()").both(with_none).ptr());
    expect_parses(unspaced, false, ios, "(9876.05$)", "987605");
    expect_parses(unspaced, false, ios, "(9876.05)", "987605");

    // The character a `space` slot owes is the stream's fill, so a field written
    // with the default fill and read back under another one is missing it.
    ios_base<char> other_fill;
    other_fill.fill('*');
    expect_rejects(spaced, false, other_fill, "(9876.05 $)");
}

// Without showbase the symbol is optional, and a symbol the parse cannot place
// is simply not part of the field: it is left for whoever reads next.
TEST(MonetaryChar, AnUnplaceableSymbolEndsTheField)
{
    ios_base<char> ios;
    const pattern  trailing = {part::value, part::symbol, part::none, part::sign};

    for (const char* symbol : {"$", "%", "&"})
    {
        SCOPED_TRACE(::testing::PrintToString(symbol));
        const monetary<char> obj(tuned()->fraction(0).symbol(symbol).plus("").minus("")
                                        .both(trailing).ptr());
        expect_parses(obj, false, ios, std::string("10") + symbol, "10", symbol);
    }
}

// A locale whose sign position is 0 wraps a negative amount in parentheses
// rather than spelling a sign, so the facet has to supply "()" where lconv has
// only the sign string it would otherwise use.
TEST(MonetaryChar, ASignPositionOfZeroMeansParentheses)
{
    const monetary<char> obj = facet_for("en_HK.UTF-8");
    EXPECT_EQ(obj.negative_sign_nat(), "()");
    EXPECT_EQ(obj.negative_sign_int(), "()");

    ios_base<char> ios;
    const std::string field = put_str(obj, false, ios, "-827364");
    EXPECT_EQ(field.front(), '(');
    EXPECT_EQ(field.back(), ')');
    expect_parses(obj, false, ios, field, "-827364");
}

// A `space` slot writes the stream's fill character, not a literal space, and
// leading padding then shifts everything already written -- that run included.
// A forgotten shift would leave the run recorded at the wrong offset, which is
// what the fill check downstream reads.
TEST(MonetaryChar, PaddingInFrontShiftsTheFillAlreadyWritten)
{
    const monetary<char> obj(tuned()->fraction(0).symbol("$").minus("-")
                                    .negative({part::symbol, part::space, part::sign, part::value})
                                    .ptr());
    ios_base<char> ios;
    ios.setf(ios_defs::showbase);
    ios.fill('*');
    ios.width(10);
    EXPECT_EQ(put_str(obj, false, ios, "-12"), "*****$*-12");

    // With a fill that reads as a space the same field is legible, and the
    // single character the slot owes is still there when nothing is padded.
    ios_base<char> plain;
    plain.setf(ios_defs::showbase);
    EXPECT_EQ(put_str(obj, false, plain, "-12"), "$ -12");
}

// The sign is required when the pattern makes its absence unreadable -- it opens
// the field, or a space follows where the sign would have been.  A field that
// then arrives without one is not an amount.
TEST(MonetaryChar, APatternCanMakeTheSignMandatory)
{
    ios_base<char> ios;

    const monetary<char> leading(tuned()->fraction(0).symbol("$").plus("+").minus("-")
                                        .both({part::sign, part::symbol, part::value, part::none})
                                        .ptr());
    expect_parses(leading, false, ios, "+$12", "12");
    expect_parses(leading, false, ios, "-$12", "-12");
    expect_rejects(leading, false, ios, "$12");

    const monetary<char> spaced(tuned()->fraction(0).symbol("$").plus("+").minus("-")
                                       .both({part::symbol, part::sign, part::space, part::value})
                                       .ptr());
    expect_parses(spaced, false, ios, "$+ 12", "12");
    expect_rejects(spaced, false, ios, "$ 12");
}

// Only the sign's first character sits in the sign slot; the rest trails the
// field.  A field that starts one and does not finish it is not an amount.
TEST(MonetaryChar, AnUnfinishedMultiCharacterSignIsRejected)
{
    ios_base<char>       ios;
    const monetary<char> obj(tuned()->fraction(0).minus("-->").plus("")
                                    .both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, "-12->", "-12");
    expect_rejects(obj, false, ios, "-12-");
    expect_rejects(obj, false, ios, "-12");
}

// Everything above works in the national form.  The international one is a
// separate set of punctuation reached by a separate branch at every entry
// point, so the round trip is run through it too.
TEST(MonetaryChar, TheInternationalFormRoundTripsAsWell)
{
    const monetary<char> obj(tuned()->fraction(2).groups({3}).separator(',')
                                    .symbol("$").plus("").minus("-")
                                    .both(kSymbolSignValue).ptr());

    const std::string amounts[] = {"0", "827364", "-827364", "-1"};

    for (bool intl : {false, true})
        for (const std::string& amount : amounts)
        {
            SCOPED_TRACE(trace_case(0, 0, intl, amount));
            ios_base<char>    writer;
            const std::string field = put_str(obj, intl, writer, amount);
            ASSERT_FALSE(field.empty());

            ios_base<char> reader;
            expect_parses(obj, intl, reader, field, amount);

            // And the same field read straight into an integer.
            int64_t units = 0;
            obj.get(field.begin(), field.end(), intl, reader, units);
            EXPECT_EQ(to_digits(units), amount);
        }
}

// A `space` slot takes the whole internal spread when there is one, rather than
// the single character it owes when there is not.
TEST(MonetaryChar, InternalPaddingFillsTheSpaceSlot)
{
    const monetary<char> obj(tuned()->fraction(0).symbol("$").minus("-")
                                    .negative({part::symbol, part::space, part::sign, part::value})
                                    .ptr());
    ios_base<char> ios;
    ios.setf(ios_defs::showbase);
    ios.setf(ios_defs::internal, ios_defs::adjustfield);
    ios.fill('*');
    ios.width(9);
    EXPECT_EQ(put_str(obj, false, ios, "-12"), "$*****-12");
}

// A field that starts the symbol and does not finish it has not written the
// symbol, so with showbase set there is nothing for the required slot to match.
TEST(MonetaryChar, APartiallyMatchedSymbolIsNotTheSymbol)
{
    const monetary<char> obj(tuned()->fraction(0).symbol("USD").plus("").minus("-")
                                    .both(kSymbolSignValue).ptr());

    ios_base<char> required;
    required.setf(ios_defs::showbase);
    expect_parses(obj, false, required, "USD12", "12");
    expect_rejects(obj, false, required, "US12");

    // Without showbase the half-written symbol is simply not part of the field.
    ios_base<char> optional;
    expect_rejects(obj, false, optional, "US12");
}

// Leading zeros are stripped from the digits, and the sign has to be put back in
// front of what is left rather than in front of what was parsed.
TEST(MonetaryChar, ANegativeAmountKeepsItsSignAfterLeadingZerosAreStripped)
{
    const monetary<char> obj(tuned()->fraction(2).point('.').plus("").minus("-")
                                    .both(kSymbolSignValue).ptr());
    ios_base<char> ios;

    expect_parses(obj, false, ios, "-0.01", "-1");
    expect_parses(obj, false, ios, "-000.10", "-10");
    expect_parses(obj, false, ios, "-0.00", "0");
    expect_parses(obj, false, ios, "0.00", "0");
}

// A field with no digits at all cannot become an integer either, and the target
// is left as the caller had it.
TEST(MonetaryChar, AFieldWithNoDigitsIsNotAnInteger)
{
    const monetary<char> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    ios_base<char>       ios;

    const std::string input = "no digits here";
    int64_t            units = 4242;
    EXPECT_THROW((void)obj.get(input.begin(), input.end(), false, ios, units), stream_error);
    EXPECT_EQ(units, 4242);
}
