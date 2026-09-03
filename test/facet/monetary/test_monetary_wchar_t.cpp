// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same currency contract as test_monetary_char.cpp for wchar_t.  What differs is
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
#include <IOv2/facet/monetary.h>
#include <IOv2/facet/monetary_details.h>

#include <IOv2/common/defs.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/streambuf.h>
#include <IOv2/io/streambuf_iterator.h>

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
    class tunable_conf : public monetary_conf<wchar_t>,
                         public std::enable_shared_from_this<tunable_conf>
    {
    public:
        explicit tunable_conf(const std::string& name = "C")
            : monetary_conf<wchar_t>(name)
            , m_decimal_point(monetary_conf<wchar_t>::decimal_point())
            , m_thousands_sep(monetary_conf<wchar_t>::thousands_sep())
            , m_grouping(monetary_conf<wchar_t>::grouping())
            , m_curr_symbol(monetary_conf<wchar_t>::curr_symbol_nat())
            , m_positive_sign(monetary_conf<wchar_t>::positive_sign_nat())
            , m_negative_sign(monetary_conf<wchar_t>::negative_sign_nat())
            , m_frac_digits(monetary_conf<wchar_t>::frac_digits_nat())
            , m_pos_format(monetary_conf<wchar_t>::pos_format_nat())
            , m_neg_format(monetary_conf<wchar_t>::neg_format_nat())
        {}

        wchar_t                     decimal_point() const override { return m_decimal_point; }
        wchar_t                     thousands_sep() const override { return m_thousands_sep; }
        const std::vector<uint8_t>& grouping() const override { return m_grouping; }
        const std::wstring&         curr_symbol_nat() const override { return m_curr_symbol; }
        const std::wstring&         positive_sign_nat() const override { return m_positive_sign; }
        const std::wstring&         negative_sign_nat() const override { return m_negative_sign; }
        int                         frac_digits_nat() const override { return m_frac_digits; }
        const pattern&              pos_format_nat() const override { return m_pos_format; }
        const pattern&              neg_format_nat() const override { return m_neg_format; }

        tunable_conf& point(wchar_t c)                       { m_decimal_point = c; return *this; }
        tunable_conf& separator(wchar_t c)                   { m_thousands_sep = c; return *this; }
        tunable_conf& groups(std::vector<uint8_t> g)      { m_grouping = std::move(g); return *this; }
        tunable_conf& symbol(std::wstring s)               { m_curr_symbol = std::move(s); return *this; }
        tunable_conf& plus(std::wstring s)                 { m_positive_sign = std::move(s); return *this; }
        tunable_conf& minus(std::wstring s)                { m_negative_sign = std::move(s); return *this; }
        tunable_conf& fraction(int n)                     { m_frac_digits = n; return *this; }
        tunable_conf& positive(pattern p)                 { m_pos_format = p; return *this; }
        tunable_conf& negative(pattern p)                 { m_neg_format = p; return *this; }
        tunable_conf& both(pattern p)                     { return positive(p).negative(p); }

        // Ends a chain: the facet's constructor takes the configuration by
        // shared pointer, and the chain has been handing back references.
        std::shared_ptr<tunable_conf> ptr()               { return shared_from_this(); }

    private:
        wchar_t              m_decimal_point;
        wchar_t              m_thousands_sep;
        std::vector<uint8_t> m_grouping;
        std::wstring         m_curr_symbol;
        std::wstring         m_positive_sign;
        std::wstring         m_negative_sign;
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

    monetary<wchar_t> facet_for(const char* loc)
    {
        return monetary<wchar_t>(std::make_shared<monetary_conf<wchar_t>>(loc));
    }

    // std::to_string is narrow; an amount reaches the facet in the character
    // type under test.
    std::wstring to_digits(int64_t v)
    {
        const std::string ascii = std::to_string(v);
        return std::wstring(ascii.begin(), ascii.end());
    }

    // Takes either spelling of an amount -- the digit string or the integer --
    // because put() has an overload for each and they must agree.
    template <typename TVal>
    std::wstring put_str(const monetary<wchar_t>& obj, bool intl, ios_base<wchar_t>& io,
                        const TVal& amount)
    {
        std::wstring out;
        obj.put(std::back_inserter(out), intl, io, amount);
        return out;
    }

    // The round-trip case names its combination on one line, so the sibling
    // files' literal retyping cannot reach these labels.
    std::string trace_case(int frac, std::size_t groups, bool showbase, const std::wstring& amount)
    {
        return "frac=" + std::to_string(frac) + " groups=" + std::to_string(groups) + " showbase=" + std::to_string(showbase) + " amount=" + ::testing::PrintToString(amount);
    }

    // What a parse produced: whether it succeeded, the digits it yielded, and
    // the input it left behind.
    struct parse_result
    {
        bool         ok;
        std::wstring digits;
        std::wstring rest;
    };

    parse_result parse_over_pointers(const monetary<wchar_t>& obj, bool intl, ios_base<wchar_t>& io,
                                     const std::wstring& input, const std::wstring& seed)
    {
        parse_result res{true, seed, {}};
        try
        {
            auto it  = obj.get(input.begin(), input.end(), intl, io, res.digits);
            res.rest = std::wstring(it, input.end());
        }
        catch (const stream_error&)
        {
            res.ok = false;
        }
        return res;
    }

    // The same parse over an iterator that cannot be compared to anything but a
    // sentinel and cannot be rewound -- the shape get() is actually written for.
    parse_result parse_over_a_stream(const monetary<wchar_t>& obj, bool intl, ios_base<wchar_t>& io,
                                     const std::wstring& input, const std::wstring& seed)
    {
        parse_result res{true, seed, {}};
        streambuf    sb(mem_device{input});
        auto         beg = istreambuf_iterator(sb);
        try
        {
            auto it = obj.get(beg, std::default_sentinel, intl, io, res.digits);
            res.rest = std::wstring(it, decltype(it)());
        }
        catch (const stream_error&)
        {
            res.ok = false;
        }
        return res;
    }

    // Every parse assertion goes through here, so no case can check one iterator
    // shape and leave the other unexamined.
    void expect_parses(const monetary<wchar_t>& obj, bool intl, ios_base<wchar_t>& io,
                       const std::wstring& input, const std::wstring& digits,
                       const std::wstring& rest = L"")
    {
        SCOPED_TRACE(::testing::PrintToString(input));
        for (bool streamed : {false, true})
        {
            SCOPED_TRACE(streamed ? "streambuf iterator" : "string iterator");
            const parse_result r = streamed ? parse_over_a_stream(obj, intl, io, input, L"")
                                            : parse_over_pointers(obj, intl, io, input, L"");
            EXPECT_TRUE(r.ok);
            EXPECT_EQ(r.digits, digits);
            EXPECT_EQ(r.rest, rest);
        }
    }

    // A failed parse throws, and the digit string it was handed must come back
    // exactly as it was: the caller's variable is not a scratch buffer.
    void expect_rejects(const monetary<wchar_t>& obj, bool intl, ios_base<wchar_t>& io,
                        const std::wstring& input)
    {
        SCOPED_TRACE(::testing::PrintToString(input));
        const std::wstring seed = L"untouched";
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

TEST(MonetaryWchar, TheCharacterTypeIsChar)
{
    static_assert(std::is_same_v<monetary<wchar_t>::char_type, wchar_t>);
}

// [locale.moneypunct] fixes the "C" locale completely: no currency, no
// grouping, no fractional digits, and the same format both ways round.
TEST(MonetaryWchar, TheCLocaleCarriesNoCurrency)
{
    const monetary<wchar_t> obj = facet_for("C");

    EXPECT_EQ(obj.decimal_point(), L'.');
    EXPECT_EQ(obj.thousands_sep(), L',');
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
TEST(MonetaryWchar, TheCLocaleStillHasANegativeSign)
{
    const monetary<wchar_t> obj = facet_for("C");
    EXPECT_FALSE(obj.negative_sign_nat().empty());
    EXPECT_FALSE(obj.negative_sign_int().empty());
}

TEST(MonetaryWchar, ALocaleWithCurrencyDataDiffersFromTheCLocale)
{
    const monetary<wchar_t> plain = facet_for("C");
    const monetary<wchar_t> us    = facet_for("en_US.UTF-8");

    EXPECT_NE(us.curr_symbol_nat(), plain.curr_symbol_nat());
    EXPECT_NE(us.frac_digits_nat(), plain.frac_digits_nat());
    EXPECT_NE(us.grouping(), plain.grouping());
}

// The digits are the smallest units of the currency, so the amount they spell is
// read off their right-hand end: frac_digits places go behind the decimal point
// and the rest in front of it.
TEST(MonetaryWchar, TheAmountIsCutFracDigitsPlacesFromTheRight)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(2).both(kSymbolSignValue).ptr());

    EXPECT_EQ(put_str(obj, false, ios, L"827364"), L"8273.64");
    EXPECT_EQ(put_str(obj, false, ios, L"1"), L".01");
    EXPECT_EQ(put_str(obj, false, ios, L"12"), L".12");
    EXPECT_EQ(put_str(obj, false, ios, L"123"), L"1.23");
}

// A run too short to reach the cut has no integral part at all, and the fraction
// picks up the shortfall as leading zeros: two places turn 7 into .07, not 7.0.
TEST(MonetaryWchar, AShortAmountIsPaddedInTheFractionNotTheInteger)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(3).both(kSymbolSignValue).ptr());

    EXPECT_EQ(put_str(obj, false, ios, L"7"), L".007");
    EXPECT_EQ(put_str(obj, false, ios, L"70"), L".070");
    EXPECT_EQ(put_str(obj, false, ios, L"700"), L".700");
    EXPECT_EQ(put_str(obj, false, ios, L"7000"), L"7.000");
}

TEST(MonetaryWchar, NoFractionalDigitsMeansNoDecimalPoint)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(obj, false, ios, L"123456"), L"123456");
}

// A negative frac_digits asks for no fraction at all and keeps the whole run,
// which is not the same statement as zero places: it is the locale saying the
// question does not apply.
TEST(MonetaryWchar, ANegativeFractionWidthKeepsEveryDigitIntegral)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(-1).both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(obj, false, ios, L"123456"), L"123456");
}

TEST(MonetaryWchar, GroupingInsertsTheThousandsSeparator)
{
    ios_base<wchar_t> ios;

    const monetary<wchar_t> threes(tuned()->fraction(0).groups({3}).separator(L',')
                                       .both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(threes, false, ios, L"1234567"), L"1,234,567");

    const monetary<wchar_t> ones(tuned()->fraction(0).groups({1}).separator(L'#')
                                     .both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(ones, false, ios, L"1234"), L"1#2#3#4");

    // A grouping vector is read right to left and its last entry repeats, so
    // {3,2} groups three digits then twos all the way up.
    const monetary<wchar_t> indian(tuned()->fraction(0).groups({3, 2}).separator(L',')
                                       .both(kSymbolSignValue).ptr());
    EXPECT_EQ(put_str(indian, false, ios, L"12345678"), L"1,23,45,678");
}

TEST(MonetaryWchar, AnEmptyGroupingInsertsNothing)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).groups({}).both(kSymbolSignValue).ptr());
    const std::wstring      digits(300, L'1');
    EXPECT_EQ(put_str(obj, false, ios, digits), digits);
}

// The symbol is the one part of the field the caller decides about: it is
// written when showbase is set and left out otherwise, and nothing else about
// the field changes with it.
TEST(MonetaryWchar, TheSymbolIsWrittenOnlyWithShowbase)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(3).symbol(L"@").both(kSymbolSignValue).ptr());

    EXPECT_EQ(put_str(obj, false, ios, L"482715"), L"482.715");
    ios.setf(ios_defs::showbase);
    EXPECT_EQ(put_str(obj, false, ios, L"482715"), L"@482.715");
    ios.unsetf(ios_defs::showbase);
    EXPECT_EQ(put_str(obj, false, ios, L"482715"), L"482.715");
}

TEST(MonetaryWchar, ThePatternDecidesTheOrderOfTheParts)
{
    ios_base<wchar_t> ios;
    ios.setf(ios_defs::showbase);

    const std::pair<pattern, const wchar_t*> cases[] = {
        {{part::symbol, part::sign, part::value, part::none}, L"$-12"},
        {{part::sign, part::symbol, part::value, part::none}, L"-$12"},
        {{part::value, part::space, part::symbol, part::sign}, L"12 $-"},
        {{part::sign, part::value, part::space, part::symbol}, L"-12 $"},
        {{part::symbol, part::space, part::value, part::sign}, L"$ 12-"},
    };

    for (const auto& [order, expected] : cases)
    {
        SCOPED_TRACE(::testing::PrintToString(expected));
        const monetary<wchar_t> obj(tuned()->fraction(0).symbol(L"$").minus(L"-").negative(order).ptr());
        EXPECT_EQ(put_str(obj, false, ios, L"-12"), expected);
    }
}

// A sign spelled with more than one character wraps the field: its first
// character sits in the sign slot and the rest trails everything, which is how
// a locale writes a negative amount in parentheses.
TEST(MonetaryWchar, AMultiCharacterSignWrapsTheField)
{
    ios_base<wchar_t> ios;
    ios.setf(ios_defs::showbase);
    const monetary<wchar_t> obj(tuned()->fraction(2).groups({3}).separator(L',').symbol(L"$")
                                    .minus(L"()")
                                    .negative({part::symbol, part::space, part::sign, part::value}).ptr());

    EXPECT_EQ(put_str(obj, false, ios, L"-827364"), L"$ (8,273.64)");
}

TEST(MonetaryWchar, TheSignOfTheAmountChoosesThePattern)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).plus(L"+").minus(L"-")
                                    .positive({part::sign, part::value, part::none, part::none})
                                    .negative({part::value, part::sign, part::none, part::none}).ptr());

    EXPECT_EQ(put_str(obj, false, ios, L"12"), L"+12");
    EXPECT_EQ(put_str(obj, false, ios, L"-12"), L"12-");
}

// The international and national sets are independent, and intl is what picks
// between them: the same amount through one facet has two spellings.
TEST(MonetaryWchar, TheInternationalFlagSelectsTheOtherPunctuation)
{
    ios_base<wchar_t> ios;
    ios.setf(ios_defs::showbase);
    const monetary<wchar_t> obj = facet_for("en_US.UTF-8");

    EXPECT_NE(obj.curr_symbol_int(), obj.curr_symbol_nat());
    const std::wstring national      = put_str(obj, false, ios, L"123456");
    const std::wstring international = put_str(obj, true, ios, L"123456");
    EXPECT_NE(national, international);
    EXPECT_NE(national.find(obj.curr_symbol_nat()), std::wstring::npos);
    EXPECT_NE(international.find(obj.curr_symbol_int()), std::wstring::npos);
}

TEST(MonetaryWchar, AShortFieldIsPaddedToTheWidth)
{
    const monetary<wchar_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());

    ios_base<wchar_t> ios;
    ios.fill(L'*');
    ios.width(8);
    EXPECT_EQ(put_str(obj, false, ios, L"123"), L"*****123");

    ios.width(8);
    ios.setf(ios_defs::left, ios_defs::adjustfield);
    EXPECT_EQ(put_str(obj, false, ios, L"123"), L"123*****");
}

// Under internal the shortfall is not tacked onto an end: it goes into whichever
// pattern slot writes nothing of its own, which is what puts the fill between
// the symbol and the amount rather than outside them.
TEST(MonetaryWchar, InternalPaddingGoesIntoTheEmptySlot)
{
    const monetary<wchar_t> obj(tuned()->fraction(0).symbol(L"$").minus(L"-")
                                    .negative({part::symbol, part::none, part::sign, part::value}).ptr());

    ios_base<wchar_t> ios;
    ios.setf(ios_defs::showbase);
    ios.setf(ios_defs::internal, ios_defs::adjustfield);
    ios.fill(L'*');
    ios.width(9);
    EXPECT_EQ(put_str(obj, false, ios, L"-123"), L"$****-123");
}

// width() is one-shot: the field it sized is the only one it sizes.
TEST(MonetaryWchar, TheWidthIsConsumedByOnePut)
{
    const monetary<wchar_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    ios_base<wchar_t>       ios;
    ios.fill(L'*');
    ios.width(8);

    EXPECT_EQ(put_str(obj, false, ios, L"123"), L"*****123");
    EXPECT_EQ(ios.width(), 0u);
    EXPECT_EQ(put_str(obj, false, ios, L"123"), L"123");
}

// The amount runs up to the first character that is not a digit; what the caller
// put after that is not the facet's to format.  With nothing to format at all,
// nothing is written.
TEST(MonetaryWchar, WhatIsNotADigitIsNotAnAmount)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());

    EXPECT_EQ(put_str(obj, false, ios, L"42 apples"), L"42");
    EXPECT_EQ(put_str(obj, false, ios, L"-A"), L"");
    EXPECT_EQ(put_str(obj, false, ios, L""), L"");
    EXPECT_EQ(put_str(obj, false, ios, L"-"), L"");
}

TEST(MonetaryWchar, AnIntegralValueFormatsLikeItsDigitString)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(2).groups({3}).separator(L',')
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
TEST(MonetaryWchar, PutReturnsThePositionAfterTheField)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());

    std::wstring buffer(13, L'^');
    auto         it = obj.put(buffer.begin() + 2, false, ios, std::wstring(L"2607"));

    EXPECT_EQ(it, buffer.begin() + 6);
    EXPECT_EQ(buffer, L"^^2607^^^^^^^");

    std::wstring international(13, L'^');
    it = obj.put(international.begin() + 2, true, ios, std::wstring(L"2607"));

    EXPECT_EQ(it, international.begin() + 6);
    EXPECT_EQ(international, L"^^2607^^^^^^^");
}

// Everything above reads the field back through the same facet that wrote it.
// The two directions are separate code, so this is the case that ties them:
// whatever put() produced, get() has to return the amount put() was given.
TEST(MonetaryWchar, WhatPutWritesGetReadsBack)
{
    const std::vector<uint8_t> groupings[] = {{}, {3}, {1}, {3, 2}};
    const std::wstring         amounts[]   = {L"0", L"1", L"12", L"827364", L"-1", L"-827364",
                                              L"98765432109", L"-98765432109"};


    for (int frac : {0, 2, 3})
        for (const std::vector<uint8_t>& g : groupings)
            for (bool showbase : {false, true})
            {
                const monetary<wchar_t> obj(tuned()->fraction(frac).groups(g).separator(L',')
                                                .symbol(L"$").plus(L"").minus(L"-")
                                                .both(kSymbolSignValue).ptr());
                ios_base<wchar_t> ios;
                if (showbase) ios.setf(ios_defs::showbase);

                for (const std::wstring& amount : amounts)
                {
                    SCOPED_TRACE(trace_case(frac, g.size(), showbase, amount));
                    ios_base<wchar_t> writer;
                    if (showbase) writer.setf(ios_defs::showbase);
                    const std::wstring field = put_str(obj, false, writer, amount);
                    ASSERT_FALSE(field.empty());
                    expect_parses(obj, false, ios, field, amount);
                }
            }
}

// Parsing ends at the first character the format has no place for, and what is
// left is the caller's to read next.
TEST(MonetaryWchar, ParsingStopsAtTheFirstForeignCharacter)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, L"1 apple", L"1", L" apple");
    expect_parses(obj, false, ios, L"123abc", L"123", L"abc");
}

// With grouping switched off the separator is not part of an amount, so it ends
// one rather than continuing it.
TEST(MonetaryWchar, ASeparatorEndsTheAmountWhenThereIsNoGrouping)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).groups({}).separator(L',')
                                    .both(kSymbolSignValue).ptr());
    expect_parses(obj, false, ios, L"742,908", L"742", L",908");
}

// Likewise the decimal point, when the locale has no fractional digits to put
// behind it.
TEST(MonetaryWchar, ADecimalPointEndsTheAmountWhenThereIsNoFraction)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).point(L'.').both(kSymbolSignValue).ptr());
    expect_parses(obj, false, ios, L"742.908", L"742", L".908");
}

TEST(MonetaryWchar, AnEmptySequenceIsNotAnAmount)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    expect_rejects(obj, false, ios, L"");
}

TEST(MonetaryWchar, TextThatIsNotAnAmountIsRejected)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    expect_rejects(obj, false, ios, L"nothing numeric");
    expect_rejects(obj, false, ios, L"a sentence with no amount anywhere in it");
}

// A fraction is all or nothing: exactly frac_digits places, or the field is not
// an amount in this locale.
TEST(MonetaryWchar, TheFractionMustHaveExactlyFracDigitsPlaces)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(4).point(L'.').both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, L"73.5926", L"735926");
    expect_rejects(obj, false, ios, L"73.59261");
    expect_rejects(obj, false, ios, L"73.592");
    expect_rejects(obj, false, ios, L"73.");

    // No decimal point at all is not a short fraction: it is an amount with none.
    expect_parses(obj, false, ios, L"73", L"73");
}

TEST(MonetaryWchar, ASecondDecimalPointIsNotPartOfTheAmount)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(2).point(L':').both(kSymbolSignValue).ptr());
    expect_rejects(obj, false, ios, L"47::2");
}

// The separators have to fall where this locale's grouping puts them.  A field
// grouped some other way is a field from some other locale.
TEST(MonetaryWchar, TheSeparatorsMustFollowTheGrouping)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).groups({2}).separator(L'#')
                                    .both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, L"7#06#45", L"70645");
    expect_rejects(obj, false, ios, L"007#06#45");
    expect_rejects(obj, false, ios, L"7#06##45");
}

// A locale that spells a positive sign but no negative one leaves the absence of
// a sign to mean negative, which is what [locale.money.get] asks for.
TEST(MonetaryWchar, NoSignMeansNegativeWhenOnlyThePositiveSignIsSpelled)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).plus(L"+").minus(L"")
                                    .both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, L"69", L"-69");
    expect_parses(obj, false, ios, L"+69", L"69");
}

TEST(MonetaryWchar, ASignInTheLastSlotIsStillFound)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).plus(L"+").minus(L"-")
                                    .both({part::value, part::space, part::symbol, part::sign}).ptr());

    expect_parses(obj, false, ios, L"123 +", L"123");
    expect_parses(obj, false, ios, L"123 -", L"-123");
}

// With showbase the symbol is part of the field and has to be there.  Without
// it the symbol is optional -- but a symbol that is present is still consumed,
// or the parse would stop in the middle of a field it could read.
TEST(MonetaryWchar, ShowbaseDecidesWhetherTheSymbolIsRequired)
{
    const monetary<wchar_t> obj(tuned()->fraction(0).symbol(L"$").minus(L"-")
                                    .both(kSymbolSignValue).ptr());

    ios_base<wchar_t> required;
    required.setf(ios_defs::showbase);
    expect_parses(obj, false, required, L"$123", L"123");
    expect_rejects(obj, false, required, L"123");

    ios_base<wchar_t> optional;
    expect_parses(obj, false, optional, L"$123", L"123");
    expect_parses(obj, false, optional, L"123", L"123");
}

// A field with a symbol and no digits is not an amount, whichever way round the
// symbol is required.
TEST(MonetaryWchar, ASymbolWithoutDigitsIsNotAnAmount)
{
    const monetary<wchar_t> obj(tuned()->fraction(0).symbol(L"$").minus(L"-")
                                    .both(kSymbolSignValue).ptr());

    ios_base<wchar_t> ios;
    ios.setf(ios_defs::showbase);
    expect_rejects(obj, false, ios, L"$");
    expect_rejects(obj, false, ios, L"$-");
}

// The fraction alone is an amount: the integral part may be empty as long as the
// places behind the point are all there.
TEST(MonetaryWchar, AnAmountMayBeAllFraction)
{
    const monetary<wchar_t> obj(tuned()->fraction(3).point(L'.').symbol(L"@").minus(L"-")
                                    .both(kSymbolSignValue).ptr());

    ios_base<wchar_t> ios;
    expect_parses(obj, false, ios, L"@.000 ", L"0", L" ");
    expect_parses(obj, false, ios, L"@-.042 ", L"-42", L" ");
}

TEST(MonetaryWchar, AnAmountTooLargeForTheTargetTypeIsRejected)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).groups({}).both(kSymbolSignValue).ptr());
    const std::wstring      huge(40, L'9');

    int64_t      units = 0;
    std::wstring digits;
    EXPECT_THROW((void)obj.get(huge.begin(), huge.end(), false, ios, units), stream_error);

    // The same field is a perfectly good digit string, though: it is only the
    // conversion to a fixed-width integer that cannot hold it.
    EXPECT_NO_THROW((void)obj.get(huge.begin(), huge.end(), false, ios, digits));
    EXPECT_EQ(digits, huge);
}

TEST(MonetaryWchar, GettingAnIntegralValueAgreesWithGettingTheDigits)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(2).groups({3}).separator(L',')
                                    .both(kSymbolSignValue).ptr());

    for (const wchar_t* field : {L"1,234.56", L"-1,234.56", L".01", L"-.01", L"0.00"})
    {
        SCOPED_TRACE(::testing::PrintToString(field));
        const std::wstring input(field);

        std::wstring digits;
        obj.get(input.begin(), input.end(), false, ios, digits);

        int64_t units = 0;
        obj.get(input.begin(), input.end(), false, ios, units);

        EXPECT_EQ(to_digits(units), digits);
    }
}

// put() writes through an output iterator, so an iterator that reaches a stream
// rather than a container has to work as the destination too.
TEST(MonetaryWchar, PutWritesThroughAnOutputIteratorOntoAStream)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(2).groups({3}).separator(L',')
                                    .both(kSymbolSignValue).ptr());

    streambuf sb{mem_device<wchar_t>{L""}};
    obj.put(ostreambuf_iterator(sb), false, ios, std::wstring(L"123456"));
    obj.put(ostreambuf_iterator(sb), true, ios, std::wstring(L"123456"));
    sb.flush();
    EXPECT_EQ(sb.device().str(), L"1,234.56123,456");
}

// The same fill vetting as on the writing side, but from the reader's end: a run
// of fill in front of the digits is consumed as padding, and the facet refuses
// the ones a reader would have counted as part of the amount instead.
TEST(MonetaryWchar, AFillThatWouldChangeTheAmountIsRejectedOnTheWayBackIn)
{
    const monetary<wchar_t> obj = facet_for("C");

    auto get = [&obj](char fill, const std::wstring& input, std::wstring& digits)
    {
        ios_base<wchar_t> ios;
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

    std::wstring digits;

    // "112345" reads as 112345, never as 12345 with a '1' of padding in front.
    EXPECT_FALSE(get(L'1', L"112345", digits));
    EXPECT_FALSE(get(L'9', L"912345", digits));

    // A leading zero is the one digit that reads the same either way.
    EXPECT_TRUE(get(L'0', L"0000012345", digits));
    EXPECT_EQ(digits, L"12345");

    // With the sign consumed first, a '-' run behind it cannot be read as a
    // second sign.
    EXPECT_TRUE(get(L'-', L"-------12345", digits));
    EXPECT_EQ(digits, L"-12345");

    // Fill that cannot be read into an amount is consumed as it always was.
    EXPECT_TRUE(get(L'*', L"*****12345", digits));
    EXPECT_EQ(digits, L"12345");
    EXPECT_TRUE(get(L' ', L"     12345", digits));
    EXPECT_EQ(digits, L"12345");

    // Nothing consumed means nothing to vet, whatever the stream's fill is: this
    // input does not start with a '9', so the run stops immediately.
    EXPECT_TRUE(get(L'9', L"12345", digits));
    EXPECT_EQ(digits, L"12345");
}

// A `space` slot owes at least one character, so a field that put() wrote with
// one has to be read with one.  A `none` slot owes nothing, and a field written
// from a pattern that ends in one has no space to find.
TEST(MonetaryWchar, ASpaceSlotIsRequiredAndANoneSlotIsNot)
{
    const pattern with_space = {part::sign, part::value, part::space, part::symbol};
    const pattern with_none  = {part::sign, part::value, part::symbol, part::none};

    ios_base<wchar_t> ios;

    const monetary<wchar_t> spaced(tuned()->fraction(2).point(L'.').groups({4}).separator(L',')
                                       .symbol(L"$").plus(L"()").both(with_space).ptr());
    expect_parses(spaced, false, ios, L"(9876.05 $)", L"987605");
    expect_parses(spaced, false, ios, L"(9876.05 )", L"987605");

    const monetary<wchar_t> unspaced(tuned()->fraction(2).point(L'.').groups({4}).separator(L',')
                                         .symbol(L"$").plus(L"()").both(with_none).ptr());
    expect_parses(unspaced, false, ios, L"(9876.05$)", L"987605");
    expect_parses(unspaced, false, ios, L"(9876.05)", L"987605");

    // The character a `space` slot owes is the stream's fill, so a field written
    // with the default fill and read back under another one is missing it.
    ios_base<wchar_t> other_fill;
    other_fill.fill(L'*');
    expect_rejects(spaced, false, other_fill, L"(9876.05 $)");
}

// Without showbase the symbol is optional, and a symbol the parse cannot place
// is simply not part of the field: it is left for whoever reads next.
TEST(MonetaryWchar, AnUnplaceableSymbolEndsTheField)
{
    ios_base<wchar_t> ios;
    const pattern     trailing = {part::value, part::symbol, part::none, part::sign};

    for (const wchar_t* symbol : {L"$", L"%", L"&"})
    {
        SCOPED_TRACE(::testing::PrintToString(symbol));
        const monetary<wchar_t> obj(tuned()->fraction(0).symbol(symbol).plus(L"").minus(L"")
                                        .both(trailing).ptr());
        expect_parses(obj, false, ios, std::wstring(L"10") + symbol, L"10", symbol);
    }
}

// A locale whose sign position is 0 wraps a negative amount in parentheses
// rather than spelling a sign, so the facet has to supply "()" where lconv has
// only the sign string it would otherwise use.
TEST(MonetaryWchar, ASignPositionOfZeroMeansParentheses)
{
    const monetary<wchar_t> obj = facet_for("en_HK.UTF-8");
    EXPECT_EQ(obj.negative_sign_nat(), L"()");
    EXPECT_EQ(obj.negative_sign_int(), L"()");

    ios_base<wchar_t>  ios;
    const std::wstring field = put_str(obj, false, ios, L"-827364");
    EXPECT_EQ(field.front(), L'(');
    EXPECT_EQ(field.back(), L')');
    expect_parses(obj, false, ios, field, L"-827364");
}

// A `space` slot writes the stream's fill character, not a literal space, and
// leading padding then shifts everything already written -- that run included.
// A forgotten shift would leave the run recorded at the wrong offset, which is
// what the fill check downstream reads.
TEST(MonetaryWchar, PaddingInFrontShiftsTheFillAlreadyWritten)
{
    const monetary<wchar_t> obj(tuned()->fraction(0).symbol(L"$").minus(L"-")
                                    .negative({part::symbol, part::space, part::sign, part::value})
                                    .ptr());
    ios_base<wchar_t> ios;
    ios.setf(ios_defs::showbase);
    ios.fill(L'*');
    ios.width(10);
    EXPECT_EQ(put_str(obj, false, ios, L"-12"), L"*****$*-12");

    // With a fill that reads as a space the same field is legible, and the
    // single character the slot owes is still there when nothing is padded.
    ios_base<wchar_t> plain;
    plain.setf(ios_defs::showbase);
    EXPECT_EQ(put_str(obj, false, plain, L"-12"), L"$ -12");
}

// The sign is required when the pattern makes its absence unreadable -- it opens
// the field, or a space follows where the sign would have been.  A field that
// then arrives without one is not an amount.
TEST(MonetaryWchar, APatternCanMakeTheSignMandatory)
{
    ios_base<wchar_t> ios;

    const monetary<wchar_t> leading(tuned()->fraction(0).symbol(L"$").plus(L"+").minus(L"-")
                                        .both({part::sign, part::symbol, part::value, part::none})
                                        .ptr());
    expect_parses(leading, false, ios, L"+$12", L"12");
    expect_parses(leading, false, ios, L"-$12", L"-12");
    expect_rejects(leading, false, ios, L"$12");

    const monetary<wchar_t> spaced(tuned()->fraction(0).symbol(L"$").plus(L"+").minus(L"-")
                                       .both({part::symbol, part::sign, part::space, part::value})
                                       .ptr());
    expect_parses(spaced, false, ios, L"$+ 12", L"12");
    expect_rejects(spaced, false, ios, L"$ 12");
}

// Only the sign's first character sits in the sign slot; the rest trails the
// field.  A field that starts one and does not finish it is not an amount.
TEST(MonetaryWchar, AnUnfinishedMultiCharacterSignIsRejected)
{
    ios_base<wchar_t>       ios;
    const monetary<wchar_t> obj(tuned()->fraction(0).minus(L"-->").plus(L"")
                                    .both(kSymbolSignValue).ptr());

    expect_parses(obj, false, ios, L"-12->", L"-12");
    expect_rejects(obj, false, ios, L"-12-");
    expect_rejects(obj, false, ios, L"-12");
}

// Everything above works in the national form.  The international one is a
// separate set of punctuation reached by a separate branch at every entry
// point, so the round trip is run through it too.
TEST(MonetaryWchar, TheInternationalFormRoundTripsAsWell)
{
    const monetary<wchar_t> obj(tuned()->fraction(2).groups({3}).separator(L',')
                                    .symbol(L"$").plus(L"").minus(L"-")
                                    .both(kSymbolSignValue).ptr());

    const std::wstring amounts[] = {L"0", L"827364", L"-827364", L"-1"};

    for (bool intl : {false, true})
        for (const std::wstring& amount : amounts)
        {
            SCOPED_TRACE(trace_case(0, 0, intl, amount));
            ios_base<wchar_t>    writer;
            const std::wstring field = put_str(obj, intl, writer, amount);
            ASSERT_FALSE(field.empty());

            ios_base<wchar_t> reader;
            expect_parses(obj, intl, reader, field, amount);

            // And the same field read straight into an integer.
            int64_t units = 0;
            obj.get(field.begin(), field.end(), intl, reader, units);
            EXPECT_EQ(to_digits(units), amount);
        }
}

// A `space` slot takes the whole internal spread when there is one, rather than
// the single character it owes when there is not.
TEST(MonetaryWchar, InternalPaddingFillsTheSpaceSlot)
{
    const monetary<wchar_t> obj(tuned()->fraction(0).symbol(L"$").minus(L"-")
                                    .negative({part::symbol, part::space, part::sign, part::value})
                                    .ptr());
    ios_base<wchar_t> ios;
    ios.setf(ios_defs::showbase);
    ios.setf(ios_defs::internal, ios_defs::adjustfield);
    ios.fill(L'*');
    ios.width(9);
    EXPECT_EQ(put_str(obj, false, ios, L"-12"), L"$*****-12");
}

// A field that starts the symbol and does not finish it has not written the
// symbol, so with showbase set there is nothing for the required slot to match.
TEST(MonetaryWchar, APartiallyMatchedSymbolIsNotTheSymbol)
{
    const monetary<wchar_t> obj(tuned()->fraction(0).symbol(L"USD").plus(L"").minus(L"-")
                                    .both(kSymbolSignValue).ptr());

    ios_base<wchar_t> required;
    required.setf(ios_defs::showbase);
    expect_parses(obj, false, required, L"USD12", L"12");
    expect_rejects(obj, false, required, L"US12");

    // Without showbase the half-written symbol is simply not part of the field.
    ios_base<wchar_t> optional;
    expect_rejects(obj, false, optional, L"US12");
}

// Leading zeros are stripped from the digits, and the sign has to be put back in
// front of what is left rather than in front of what was parsed.
TEST(MonetaryWchar, ANegativeAmountKeepsItsSignAfterLeadingZerosAreStripped)
{
    const monetary<wchar_t> obj(tuned()->fraction(2).point(L'.').plus(L"").minus(L"-")
                                    .both(kSymbolSignValue).ptr());
    ios_base<wchar_t> ios;

    expect_parses(obj, false, ios, L"-0.01", L"-1");
    expect_parses(obj, false, ios, L"-000.10", L"-10");
    expect_parses(obj, false, ios, L"-0.00", L"0");
    expect_parses(obj, false, ios, L"0.00", L"0");
}

// A field with no digits at all cannot become an integer either, and the target
// is left as the caller had it.
TEST(MonetaryWchar, AFieldWithNoDigitsIsNotAnInteger)
{
    const monetary<wchar_t> obj(tuned()->fraction(0).both(kSymbolSignValue).ptr());
    ios_base<wchar_t>       ios;

    const std::wstring input = L"no digits here";
    int64_t            units = 4242;
    EXPECT_THROW((void)obj.get(input.begin(), input.end(), false, ios, units), stream_error);
    EXPECT_EQ(units, 4242);
}
