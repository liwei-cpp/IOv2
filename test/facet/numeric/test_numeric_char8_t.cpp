// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same numeric contract as test_numeric_char.cpp for char8_t.  The digits, the
 * base, the grouping, the sign and the field width are one algorithm over
 * whatever character type the facet was instantiated with, and these cases are
 * here to say that the instantiation changes none of it.
 *
 * The cases about the locale snapshot rather than the field -- what an unnamed
 * boolean falls back to, what a locale without a separator groups by -- and the
 * one about the size of the staging buffer a float is formatted into read the
 * same data whatever the character type is, so they stay in the narrow file.
 */
#include <facet/numeric.h>
#include <facet/numeric_details.h>

#include <common/defs.h>
#include <device/mem_device.h>
#include <facet/ctype.h>
#include <io/io_base.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>

#include <gtest/gtest.h>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

using namespace IOv2;

namespace
{
    // A configuration whose punctuation can be set, so a grouping case describes
    // a grouping instead of borrowing whichever one a locale happens to carry.
    class tunable_conf : public numeric_conf<char8_t>,
                         public std::enable_shared_from_this<tunable_conf>
    {
    public:
        explicit tunable_conf(const std::string& name = "C")
            : numeric_conf<char8_t>(name)
            , m_grouping(numeric_conf<char8_t>::grouping())
            , m_truename(numeric_conf<char8_t>::truename())
            , m_falsename(numeric_conf<char8_t>::falsename())
            , m_thousands_sep(numeric_conf<char8_t>::thousands_sep())
            , m_decimal_point(numeric_conf<char8_t>::decimal_point())
        {}

        const std::vector<uint8_t>& grouping() const override { return m_grouping; }
        const std::u8string&        truename() const override { return m_truename; }
        const std::u8string&        falsename() const override { return m_falsename; }
        char8_t                     thousands_sep() const override { return m_thousands_sep; }
        char8_t                     decimal_point() const override { return m_decimal_point; }

        tunable_conf& groups(std::vector<uint8_t> g) { m_grouping = std::move(g); return *this; }
        tunable_conf& yes(std::u8string n)             { m_truename = std::move(n); return *this; }
        tunable_conf& no(std::u8string n)              { m_falsename = std::move(n); return *this; }
        tunable_conf& separator(char8_t c)              { m_thousands_sep = c; return *this; }
        tunable_conf& point(char8_t c)                  { m_decimal_point = c; return *this; }

        // Ends a chain: the facet takes its configuration by shared pointer.
        std::shared_ptr<tunable_conf> ptr() { return shared_from_this(); }

    private:
        std::vector<uint8_t> m_grouping;
        std::u8string        m_truename;
        std::u8string        m_falsename;
        char8_t              m_thousands_sep;
        char8_t              m_decimal_point;
    };

    std::shared_ptr<ctype<char8_t>> ctype_for(const char* loc)
    {
        return std::make_shared<ctype<char8_t>>(std::make_shared<ctype_conf<char8_t>>(loc));
    }

    numeric<char8_t> facet_for(const char* loc)
    {
        return numeric<char8_t>(std::make_shared<numeric_conf<char8_t>>(loc), ctype_for(loc));
    }

    std::shared_ptr<tunable_conf> tuned(const char* name = "C")
    {
        return std::make_shared<tunable_conf>(name);
    }

    numeric<char8_t> facet_of(std::shared_ptr<tunable_conf> conf)
    {
        return numeric<char8_t>(std::move(conf), ctype_for("C"));
    }

    template <typename TVal>
    std::u8string put_str(const numeric<char8_t>& obj, ios_base<char8_t>& io, TVal v)
    {
        std::u8string out;
        obj.put(std::back_inserter(out), io, v);
        return out;
    }

    // What a parse produced: whether it succeeded, the value it stored -- which
    // the standard requires even on failure -- and the input it left behind.
    template <typename TVal>
    struct parse_result
    {
        bool        ok;
        TVal        value;
        std::u8string rest;
    };

    template <typename TVal>
    parse_result<TVal> parse_over_pointers(const numeric<char8_t>& obj, ios_base<char8_t>& io,
                                           const std::u8string& input, TVal seed)
    {
        parse_result<TVal> res{true, seed, {}};
        try
        {
            auto it  = obj.get(input.begin(), input.end(), io, res.value);
            res.rest = std::u8string(it, input.end());
        }
        catch (const stream_error&)
        {
            res.ok = false;
        }
        return res;
    }

    template <typename TVal>
    parse_result<TVal> parse_over_a_stream(const numeric<char8_t>& obj, ios_base<char8_t>& io,
                                           const std::u8string& input, TVal seed)
    {
        parse_result<TVal> res{true, seed, {}};
        streambuf          sb(mem_device{input});
        auto               beg = istreambuf_iterator(sb);
        try
        {
            auto it  = obj.get(beg, std::default_sentinel, io, res.value);
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
    template <typename TVal>
    void expect_parses(const numeric<char8_t>& obj, ios_base<char8_t>& io, const std::u8string& input,
                       TVal expected, const std::u8string& rest = u8"")
    {
        SCOPED_TRACE(::testing::PrintToString(input));
        for (bool streamed : {false, true})
        {
            SCOPED_TRACE(streamed ? "streambuf iterator" : "string iterator");
            const auto r = streamed ? parse_over_a_stream(obj, io, input, TVal{})
                                    : parse_over_pointers(obj, io, input, TVal{});
            EXPECT_TRUE(r.ok);
            EXPECT_EQ(r.value, expected);
            EXPECT_EQ(r.rest, rest);
        }
    }

    // A failed parse throws.  The standard still requires a value to have been
    // stored, so `stored` is what the target must be left holding.
    template <typename TVal>
    void expect_rejects(const numeric<char8_t>& obj, ios_base<char8_t>& io, const std::u8string& input,
                        TVal stored)
    {
        SCOPED_TRACE(::testing::PrintToString(input));
        for (bool streamed : {false, true})
        {
            SCOPED_TRACE(streamed ? "streambuf iterator" : "string iterator");
            const auto r = streamed ? parse_over_a_stream(obj, io, input, TVal{})
                                    : parse_over_pointers(obj, io, input, TVal{});
            EXPECT_FALSE(r.ok);
            EXPECT_EQ(r.value, stored);
        }
    }

    template <typename TVal>
    void expect_rejects(const numeric<char8_t>& obj, ios_base<char8_t>& io, const std::u8string& input)
    {
        SCOPED_TRACE(::testing::PrintToString(input));
        for (bool streamed : {false, true})
        {
            SCOPED_TRACE(streamed ? "streambuf iterator" : "string iterator");
            const auto r = streamed ? parse_over_a_stream(obj, io, input, TVal{})
                                    : parse_over_pointers(obj, io, input, TVal{});
            EXPECT_FALSE(r.ok);
        }
    }

    // ASCII the standard library produced, in the character type under test.
    std::u8string as_chars(const std::string& ascii) { return std::u8string(ascii.begin(), ascii.end()); }

    // What std::to_chars makes of the same value in the same base: the digits
    // are not the facet's to invent, only to place.
    template <typename TVal>
    std::u8string digits_of(TVal v, int base = 10)
    {
        char       buf[80] = {};
        const auto res     = std::to_chars(buf, buf + sizeof buf, v, base);
        EXPECT_EQ(res.ec, std::errc{});
        return as_chars(std::string(buf, res.ptr));
    }
}

TEST(NumericChar8, TheCharacterTypeIsChar)
{
    static_assert(std::is_same_v<numeric<char8_t>::char_type, char8_t>);
}

TEST(NumericChar8, ThePunctuationComesFromTheLocale)
{
    const numeric<char8_t> plain  = facet_for("C");
    const numeric<char8_t> german = facet_for("de_DE.UTF-8");

    EXPECT_NE(plain.decimal_point(), german.decimal_point());
    EXPECT_NE(plain.thousands_sep(), german.thousands_sep());
    EXPECT_NE(plain.grouping(), german.grouping());

    EXPECT_FALSE(plain.truename().empty());
    EXPECT_FALSE(plain.falsename().empty());
    EXPECT_FALSE(german.truename().empty());
    EXPECT_FALSE(german.falsename().empty());
    EXPECT_NE(plain.truename(), german.truename());
    EXPECT_NE(plain.falsename(), german.falsename());
}

// A locale that answers the boolean-name query with an empty string has not
// named anything, so the ASCII words stand in rather than a stream printing
// nothing at all for a bool.
TEST(NumericChar8, AnUnnamedBooleanFallsBackToAscii)
{
    const numeric_conf<char8_t> conf("gv_GB.utf8");
    EXPECT_EQ(conf.truename(), u8"true");
    EXPECT_EQ(conf.falsename(), u8"false");
    EXPECT_EQ(conf.decimal_point(), u8'.');
    EXPECT_EQ(conf.thousands_sep(), u8',');
}

// A locale with no thousands separator has no grouping either: there is nothing
// to group with.  What the separator itself is left holding differs by character
// type -- the narrow and wide configurations keep the '\0' that says "none",
// while the char8_t one, which has only a single byte to store it in, falls back
// to a comma it then never uses, because the grouping beside it is empty.
TEST(NumericChar8, ALocaleWithNoSeparatorHasNoGrouping)
{
    const numeric_conf<char8_t> conf("gl_ES.utf8");
    EXPECT_EQ(conf.decimal_point(), u8',');
    EXPECT_TRUE(conf.grouping().empty());
    EXPECT_EQ(conf.thousands_sep(), u8',');
}

TEST(NumericChar8, ABooleanIsOneOrZeroWithoutBoolalpha)
{
    ios_base<char8_t>      ios;
    const numeric<char8_t> obj = facet_for("C");
    EXPECT_EQ(put_str(obj, ios, true), u8"1");
    EXPECT_EQ(put_str(obj, ios, false), u8"0");
}

TEST(NumericChar8, BoolalphaWritesTheLocaleNames)
{
    const numeric<char8_t> obj = facet_of(tuned()->yes(u8"ja").no(u8"nein").ptr());
    ios_base<char8_t>      ios;
    ios.setf(ios_defs::boolalpha);

    EXPECT_EQ(put_str(obj, ios, true), u8"ja");
    EXPECT_EQ(put_str(obj, ios, false), u8"nein");
}

TEST(NumericChar8, ABooleanNameIsPaddedToTheWidth)
{
    const numeric<char8_t> obj = facet_of(tuned()->yes(u8"ja").ptr());

    ios_base<char8_t> ios;
    ios.setf(ios_defs::boolalpha);
    ios.fill(u8'*');

    ios.width(6);
    EXPECT_EQ(put_str(obj, ios, true), u8"****ja");
    ios.width(6);
    ios.setf(ios_defs::left, ios_defs::adjustfield);
    EXPECT_EQ(put_str(obj, ios, true), u8"ja****");
}

// The digits themselves are std::to_chars's answer, in whichever base the
// stream asked for; the facet's job starts after them.  Every width and
// signedness the facet accepts goes through a separate insert_int
// instantiation, so each is checked rather than one standing in for all.
TEST(NumericChar8, AnIntegerIsWrittenAsToCharsWouldWriteIt)
{
    const numeric<char8_t> obj = facet_for("C");

    const std::pair<ios_defs::fmtflags, int> bases[] = {
        {ios_defs::dec, 10}, {ios_defs::oct, 8}, {ios_defs::hex, 16},
    };

    auto check = [&](auto v)
    {
        using TVal = decltype(v);
        for (const auto& [flag, base] : bases)
        {
            SCOPED_TRACE(::testing::Message() << "base=" << base << " value=" << +v);
            ios_base<char8_t> ios;
            ios.setf(flag, ios_defs::basefield);
            // A non-decimal base is written from the unsigned bit pattern, which
            // is what to_chars is handed here too.
            const std::u8string expected =
                (base == 10) ? digits_of(v)
                             : digits_of(static_cast<std::make_unsigned_t<TVal>>(v), base);
            EXPECT_EQ(put_str(obj, ios, v), expected);
        }
    };

    for (int v : {0, 1, 7, 42, -1, -42})
    {
        check(static_cast<short>(v));
        check(static_cast<int>(v));
        check(static_cast<long>(v));
        check(static_cast<long long>(v));
        check(static_cast<unsigned short>(v));
        check(static_cast<unsigned>(v));
        check(static_cast<unsigned long>(v));
        check(static_cast<unsigned long long>(v));
    }

    check(std::numeric_limits<short>::max());
    check(std::numeric_limits<int>::min());
    check(std::numeric_limits<long>::max());
    check(std::numeric_limits<long long>::min());
    check(std::numeric_limits<unsigned>::max());
    check(std::numeric_limits<unsigned long long>::max());
    check(1294967294UL);
}

// A negative value in a non-decimal base has no sign: what is written is the
// bit pattern.  Forming the magnitude in the signed type would overflow for the
// minimum, so this is also where that path is checked.
TEST(NumericChar8, ANegativeValueOutsideBaseTenIsItsBitPattern)
{
    const numeric<char8_t> obj = facet_for("C");

    ios_base<char8_t> ios;
    ios.setf(ios_defs::hex, ios_defs::basefield);
    EXPECT_EQ(put_str(obj, ios, -1LL),
              digits_of(static_cast<unsigned long long>(-1LL), 16));

    ios_base<char8_t>  decimal;
    const long long least = std::numeric_limits<long long>::min();
    EXPECT_EQ(put_str(obj, decimal, least), digits_of(least, 10));
}

TEST(NumericChar8, ShowbaseWritesTheBasePrefix)
{
    const numeric<char8_t> obj = facet_for("C");

    ios_base<char8_t> hex;
    hex.setf(ios_defs::hex, ios_defs::basefield);
    hex.setf(ios_defs::showbase);
    EXPECT_EQ(put_str(obj, hex, 255L), u8"0xff");

    // Octal's prefix is a single leading zero, and zero already has one.
    ios_base<char8_t> octal;
    octal.setf(ios_defs::oct, ios_defs::basefield);
    octal.setf(ios_defs::showbase);
    EXPECT_EQ(put_str(obj, octal, 64L), u8"0100");
    EXPECT_EQ(put_str(obj, octal, 0L), u8"0");

    // Decimal has no prefix to write.
    ios_base<char8_t> decimal;
    decimal.setf(ios_defs::showbase);
    EXPECT_EQ(put_str(obj, decimal, 255L), u8"255");
}

TEST(NumericChar8, UppercaseAffectsTheHexAlphabetAndItsPrefix)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;
    ios.setf(ios_defs::hex, ios_defs::basefield);
    ios.setf(ios_defs::showbase | ios_defs::uppercase);
    EXPECT_EQ(put_str(obj, ios, 255L), u8"0XFF");
}

TEST(NumericChar8, ShowposWritesAPlusOnANonNegativeDecimal)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;
    ios.setf(ios_defs::showpos);

    EXPECT_EQ(put_str(obj, ios, 42L), u8"+42");
    EXPECT_EQ(put_str(obj, ios, 0L), u8"+0");
    EXPECT_EQ(put_str(obj, ios, -42L), u8"-42");
}

TEST(NumericChar8, GroupingInsertsTheThousandsSeparator)
{
    ios_base<char8_t> ios;

    EXPECT_EQ(put_str(facet_of(tuned()->groups({3}).separator(u8',').ptr()), ios, 1234567L),
              u8"1,234,567");
    EXPECT_EQ(put_str(facet_of(tuned()->groups({1}).separator(u8'#').ptr()), ios, 1234L),
              u8"1#2#3#4");
    // The last entry of a grouping vector repeats, so {3,2} groups three then
    // twos all the way up.
    EXPECT_EQ(put_str(facet_of(tuned()->groups({3, 2}).separator(u8',').ptr()), ios, 12345678L),
              u8"1,23,45,678");
    EXPECT_EQ(put_str(facet_of(tuned()->groups({}).separator(u8',').ptr()), ios, 1234567L),
              u8"1234567");

    // A leading sign is not part of the number being grouped: it is copied
    // across on its own and the grouper is handed only what follows.
    const numeric<char8_t> grouped = facet_of(tuned()->groups({3}).separator(u8',').point(u8'.').ptr());
    EXPECT_EQ(put_str(grouped, ios, -1234567L), u8"-1,234,567");

    ios_base<char8_t> shown;
    shown.setf(ios_defs::showpos);
    EXPECT_EQ(put_str(grouped, shown, 1234567L), u8"+1,234,567");

    // The same on the floating-point side, where the grouping stops at the
    // decimal point and the fraction is copied through ungrouped.
    ios_base<char8_t> fixed;
    fixed.setf(ios_defs::fixed, ios_defs::floatfield);
    fixed.precision(3);
    EXPECT_EQ(put_str(grouped, fixed, -1234567.5), u8"-1,234,567.500");
}

TEST(NumericChar8, AShortFieldIsPaddedToTheWidth)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;
    ios.fill(u8'*');

    ios.width(8);
    EXPECT_EQ(put_str(obj, ios, 42L), u8"******42");

    ios.width(8);
    ios.setf(ios_defs::left, ios_defs::adjustfield);
    EXPECT_EQ(put_str(obj, ios, 42L), u8"42******");
}

// Under internal the sign stays anchored to the left and the fill goes between
// it and the digits, which is what lines a column of numbers up by their signs.
TEST(NumericChar8, InternalPaddingSeparatesTheSignFromTheDigits)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;
    ios.fill(u8'*');
    ios.width(8);
    ios.setf(ios_defs::internal, ios_defs::adjustfield);

    const std::u8string out = put_str(obj, ios, -42L);
    EXPECT_EQ(out, u8"-*****42");
}

// width() is one-shot: the field it sized is the only one it sizes.
TEST(NumericChar8, TheWidthIsConsumedByOnePut)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;
    ios.fill(u8'*');
    ios.width(8);

    EXPECT_EQ(put_str(obj, ios, 42L), u8"******42");
    EXPECT_EQ(ios.width(), 0u);
    EXPECT_EQ(put_str(obj, ios, 42L), u8"42");
}

// The floating-point digits come from the C library, so the check is that the
// stream's precision and float format reach it unchanged.
TEST(NumericChar8, AFloatIsWrittenAsPrintfWouldWriteIt)
{
    const numeric<char8_t> obj = facet_for("C");

    // long double takes the 'L' length modifier and float is promoted, so the
    // three widths reach printf by three different routes.
    auto check = [&](auto v, const char* fixed_spec, const char* sci_spec)
    {
        const std::tuple<ios_defs::fmtflags, const char*> formats[] = {
            {ios_defs::fixed, fixed_spec},
            {ios_defs::scientific, sci_spec},
        };

        for (const auto& [flag, spec] : formats)
            for (int precision : {0, 1, 6, 12})
            {
                SCOPED_TRACE(::testing::Message() << spec << " precision=" << precision << " value=" << (double)v);
                ios_base<char8_t> ios;
                ios.setf(flag, ios_defs::floatfield);
                ios.precision(precision);

                char expected[512];
                std::snprintf(expected, sizeof expected, spec, precision, v);
                EXPECT_EQ(put_str(obj, ios, v), as_chars(expected));
            }
    };

    for (double v : {0.0, 1.0, 0.5, -3.25, 1234.5678, 1.7976931348623157e+308})
    {
        check(v, "%.*f", "%.*e");
        check(static_cast<long double>(v), "%.*Lf", "%.*Le");
    }
    for (float v : {0.0f, 1.0f, 0.5f, -3.25f, 1234.5678f})
        check(static_cast<double>(v), "%.*f", "%.*e");
}

TEST(NumericChar8, ShowpointKeepsTheDecimalPoint)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;

    EXPECT_EQ(put_str(obj, ios, 1.0), u8"1");
    ios.setf(ios_defs::showpoint);
    const std::u8string out = put_str(obj, ios, 1.0);
    EXPECT_NE(out.find(u8'.'), std::u8string::npos);
    EXPECT_GT(out.size(), 1u);
}

// The decimal point is the locale's, not the C library's, so a locale that
// spells it differently has to reach the output.
TEST(NumericChar8, TheDecimalPointComesFromTheLocale)
{
    const numeric<char8_t> obj = facet_of(tuned()->point(u8':').ptr());
    ios_base<char8_t>      ios;
    ios.setf(ios_defs::fixed, ios_defs::floatfield);
    ios.precision(2);
    EXPECT_EQ(put_str(obj, ios, 1.5), u8"1:50");
}

// Grouping is a property of the integer part; the digits after the point are
// not grouped whatever the locale says.
TEST(NumericChar8, GroupingAppliesToTheIntegerPartOnly)
{
    const numeric<char8_t> obj = facet_of(tuned()->groups({3}).separator(u8',').point(u8'.').ptr());
    ios_base<char8_t>      ios;
    ios.setf(ios_defs::fixed, ios_defs::floatfield);
    ios.precision(4);
    EXPECT_EQ(put_str(obj, ios, 1234567.8125), u8"1,234,567.8125");
}

TEST(NumericChar8, APointerIsHexadecimalWithItsPrefix)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;

    int         anchor = 0;
    const void* p      = &anchor;
    const std::u8string out = put_str(obj, ios, p);

    EXPECT_EQ(out.substr(0, 2), u8"0x");
    EXPECT_EQ(out, u8"0x" + digits_of(reinterpret_cast<std::uintptr_t>(p), 16));
}

// Formatting a pointer needs hex and showbase, but it borrows them: the stream
// is handed back exactly as it was.
TEST(NumericChar8, FormattingAPointerLeavesTheStreamFlagsAlone)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;
    ios.setf(ios_defs::oct, ios_defs::basefield);
    ios.setf(ios_defs::uppercase);
    const ios_defs::fmtflags before = ios.flags();

    int         anchor = 0;
    const void* p      = &anchor;
    (void)put_str(obj, ios, p);

    EXPECT_EQ(ios.flags(), before);
    EXPECT_EQ(put_str(obj, ios, 64L), digits_of(64L, 8));
}

// Without boolalpha the field is an integer, and only 0 and 1 are booleans.
// Anything else is out of range, which by LWG 23 stores true and then fails.
TEST(NumericChar8, WithoutBoolalphaOnlyZeroAndOneAreBooleans)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;

    expect_parses(obj, ios, u8"0", false);
    expect_parses(obj, ios, u8"1", true);
    expect_rejects(obj, ios, u8"2", true);
}

TEST(NumericChar8, BoolalphaReadsTheLocaleNames)
{
    const numeric<char8_t> obj = facet_of(tuned()->yes(u8"ja").no(u8"nein").ptr());
    ios_base<char8_t>      ios;
    ios.setf(ios_defs::boolalpha);

    expect_parses(obj, ios, u8"ja", true);
    expect_parses(obj, ios, u8"nein", false);
    expect_rejects<bool>(obj, ios, u8"vielleicht");
}

// Two names that are the same word cannot be told apart, so a field matching
// both is not a boolean.  LWG 23 asks for false to be stored before the failure.
TEST(NumericChar8, IdenticalBooleanNamesAreAmbiguous)
{
    const numeric<char8_t> obj = facet_of(tuned()->yes(u8"same").no(u8"same").ptr());
    ios_base<char8_t>      ios;
    ios.setf(ios_defs::boolalpha);

    expect_rejects(obj, ios, u8"same", false);
}

// Everything above reads a field the facet wrote through a different code path.
// This is the case that ties the two together across the bases and the sign.
TEST(NumericChar8, WhatPutWritesGetReadsBack)
{
    const numeric<char8_t> plain   = facet_for("C");
    const numeric<char8_t> grouped = facet_of(tuned()->groups({3}).separator(u8',').ptr());

    const ios_defs::fmtflags bases[] = {ios_defs::dec, ios_defs::oct, ios_defs::hex};
    const long               values[] = {0, 1, 7, 42, 255, 1294967, -1, -42,
                                         std::numeric_limits<long>::max(),
                                         std::numeric_limits<long>::min()};

    for (const numeric<char8_t>* obj : {&plain, &grouped})
        for (ios_defs::fmtflags base : bases)
            for (long v : values)
            {
                // A negative value outside base ten is written as a bit pattern
                // that reads back as an unsigned quantity, not as this value.
                if (base != ios_defs::dec && v < 0) continue;

                SCOPED_TRACE(::testing::Message() << "value=" << v);
                ios_base<char8_t> writer;
                writer.setf(base, ios_defs::basefield);
                const std::u8string field = put_str(*obj, writer, v);

                ios_base<char8_t> reader;
                reader.setf(base, ios_defs::basefield);
                expect_parses(*obj, reader, field, v);
            }

    // Each width and signedness parses through its own extract_int
    // instantiation, so the round trip is repeated for each of them.
    auto round_trip = [&](auto v)
    {
        SCOPED_TRACE(::testing::Message() << "value=" << +v);
        ios_base<char8_t>   writer;
        const std::u8string field = put_str(plain, writer, v);
        ios_base<char8_t>   reader;
        expect_parses(plain, reader, field, v);
    };

    round_trip(static_cast<short>(-7));
    round_trip(static_cast<int>(-70000));
    round_trip(static_cast<long long>(-7000000000LL));
    round_trip(static_cast<unsigned short>(7));
    round_trip(static_cast<unsigned>(4000000000U));
    round_trip(static_cast<unsigned long>(4000000000UL));
    round_trip(std::numeric_limits<unsigned long long>::max());
}

TEST(NumericChar8, ALoneSignIsNotANumber)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;

    expect_rejects<long>(obj, ios, u8"-");
    expect_rejects<long>(obj, ios, u8"+");
    expect_rejects<double>(obj, ios, u8"+");
    expect_rejects<long>(obj, ios, u8"");
}

// A "0x" prefix is part of the number only where the base could be sixteen: an
// explicitly octal stream reads the zero and stops at the 'x'.
TEST(NumericChar8, TheHexPrefixIsReadOnlyWhereHexIsPossible)
{
    const numeric<char8_t> obj = facet_for("C");

    ios_base<char8_t> octal;
    octal.setf(ios_defs::oct, ios_defs::basefield);
    expect_parses(obj, octal, u8"0x5", 0L, u8"x5");

    ios_base<char8_t> hex;
    hex.setf(ios_defs::hex, ios_defs::basefield);
    expect_parses(obj, hex, u8"0x5", 5L);

    // With no base selected the prefix is what selects one.
    ios_base<char8_t> automatic;
    automatic.setf(static_cast<ios_defs::fmtflags>(0), ios_defs::basefield);
    expect_parses(obj, automatic, u8"0x5", 5L);
    expect_parses(obj, automatic, u8"010", 8L);
    expect_parses(obj, automatic, u8"10", 10L);
}

TEST(NumericChar8, ParsingStopsAtTheFirstForeignCharacter)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;

    expect_parses(obj, ios, u8"42abc", 42L, u8"abc");
    expect_parses(obj, ios, u8"42 ", 42L, u8" ");

    // A decimal point ends an integer rather than continuing it: the fraction
    // is not part of the number being read, whatever follows.
    expect_parses(obj, ios, u8"42.5", 42L, u8".5");

    const numeric<char8_t> grouped = facet_of(tuned()->groups({3}).separator(u8',').point(u8'.').ptr());
    expect_parses(grouped, ios, u8"1,234.5", 1234L, u8".5");
}

// A field larger than the target can hold stores the nearest extreme and then
// fails, which is what lets a caller tell overflow from a malformed field.
TEST(NumericChar8, AnIntegerOutOfRangeStoresTheExtremeAndFails)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;

    expect_rejects(obj, ios, std::u8string(40, u8'9'), std::numeric_limits<long>::max());
    expect_rejects(obj, ios, u8"-" + std::u8string(40, u8'9'), std::numeric_limits<long>::min());
}

// A group longer than the counter that measures it cannot be checked against
// the grouping, so it is not a number this locale could have written.
TEST(NumericChar8, AnOversizedDigitGroupIsRejected)
{
    const numeric<char8_t> obj = facet_of(tuned()->groups({3}).separator(u8',').ptr());
    ios_base<char8_t>      ios;
    ios.setf(ios_defs::dec, ios_defs::basefield);

    // Leading zeros keep the running value at zero, so what the parse trips over
    // is the group length rather than an overflow.
    expect_rejects<long>(obj, ios, std::u8string(256, u8'0') + u8",5");
    expect_rejects<long>(obj, ios, u8"0," + std::u8string(256, u8'0'));
}

TEST(NumericChar8, AFloatingPointFieldRoundTrips)
{
    const numeric<char8_t> obj = facet_for("C");

    auto round_trip = [&](auto v)
    {
        SCOPED_TRACE((double)v);
        ios_base<char8_t> writer;
        writer.precision(std::numeric_limits<decltype(v)>::max_digits10);
        writer.setf(ios_defs::scientific, ios_defs::floatfield);
        const std::u8string field = put_str(obj, writer, v);

        ios_base<char8_t> reader;
        expect_parses(obj, reader, field, v);
    };

    for (double v : {0.0, 1.0, 0.5, -3.25, 1234.5678, 2.2250738585072014e-308})
    {
        round_trip(v);
        round_trip(static_cast<long double>(v));
    }
    for (float v : {0.0f, 1.0f, 0.5f, -3.25f, 1234.5678f})
        round_trip(v);
}

TEST(NumericChar8, ABareZeroIsAFloatingPointNumber)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;
    expect_parses(obj, ios, u8"0", 0.0);
}

TEST(NumericChar8, AnExponentMarkerNeedsItsDigits)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;
    expect_rejects<double>(obj, ios, u8"1e");
}

// A magnitude that rounds to infinity is out of range for the target, so LWG 23
// applies here too: the finite extreme is stored before the failure.
TEST(NumericChar8, AFloatOutOfRangeStoresTheFiniteExtremeAndFails)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;

    expect_rejects(obj, ios, u8"1e400", std::numeric_limits<double>::max());
    expect_rejects(obj, ios, u8"-1e400", -std::numeric_limits<double>::max());
}

// The same group-length limit as for integers, checked at each place the parse
// can decide a group has ended: a separator, the decimal point, the exponent
// marker, and the end of the input.
TEST(NumericChar8, AnOversizedGroupIsRejectedAtEveryBoundary)
{
    const numeric<char8_t> obj = facet_of(tuned()->groups({3}).separator(u8',').point(u8'.').ptr());
    ios_base<char8_t>      ios;
    const std::u8string    big(256, u8'1');

    expect_rejects<double>(obj, ios, big + u8",5");
    expect_rejects<double>(obj, ios, u8"1," + big + u8".5");
    expect_rejects<double>(obj, ios, u8"1," + big + u8"e5");
    expect_rejects<double>(obj, ios, u8"1," + big);
}

TEST(NumericChar8, APointerRoundTrips)
{
    const numeric<char8_t> obj = facet_for("C");

    int         anchor = 0;
    const void* p      = &anchor;

    ios_base<char8_t>   writer;
    const std::u8string field = put_str(obj, writer, p);

    ios_base<char8_t> reader;
    void*          back = nullptr;
    auto           it   = obj.get(field.begin(), field.end(), reader, back);
    EXPECT_EQ(back, p);
    EXPECT_EQ(it, field.end());
}

// Padding a field whose text begins with a '0' makes the pad path look at what
// follows it, because "0x" and "0X" are a base prefix that internal adjustment
// must not be inserted into.  A float formatted as "0.5" is the shortest way to
// reach that probe with something that is not a prefix.
TEST(NumericChar8, PaddingAValueThatStartsWithAZeroIsNotAPrefix)
{
    const numeric<char8_t> obj = facet_for("C");

    ios_base<char8_t> ios;
    ios.width(10);
    const std::u8string out = put_str(obj, ios, 0.5);
    EXPECT_EQ(out.size(), 10u);
    EXPECT_EQ(out.substr(out.size() - 3), u8"0.5");

    ios_base<char8_t> internal;
    internal.setf(ios_defs::hex, ios_defs::basefield);
    internal.setf(ios_defs::showbase);
    internal.setf(ios_defs::internal, ios_defs::adjustfield);
    internal.fill(u8'*');
    internal.width(8);
    EXPECT_EQ(put_str(obj, internal, 255L), u8"0x****ff");
}

// A pointer is formatted as hex with a base prefix, so every adjustment has to
// treat "0x" the way it treats a sign: as something the fill goes around, never
// into.  A literal address keeps the field width predictable.
TEST(NumericChar8, EachAdjustmentPadsAPointerAroundItsPrefix)
{
    const numeric<char8_t> obj = facet_for("C");
    void* const            p   = reinterpret_cast<void*>(0x1);

    auto put = [&obj, p](ios_defs::fmtflags adjust, bool set_adjust)
    {
        ios_base<char8_t> ios;
        ios.fill(u8'*');
        ios.width(5);
        if (set_adjust)
            ios.setf(adjust, ios_defs::adjustfield);
        return put_str(obj, ios, p);
    };

    // Right is the default here, so the unset case has to agree with it.
    EXPECT_EQ(put(ios_defs::right, false), u8"**0x1");
    EXPECT_EQ(put(ios_defs::right, true), u8"**0x1");
    EXPECT_EQ(put(ios_defs::internal, true), u8"0x**1");
    EXPECT_EQ(put(ios_defs::left, true), u8"0x1**");
}

// The same rule as for a currency field: a run of fill that a reader would take
// for part of the number is refused rather than written.
TEST(NumericChar8, AFillThatWouldChangeTheNumberIsRejected)
{
    const numeric<char8_t> obj = facet_for("C");

    auto put = [&obj](char fill, ios_defs::fmtflags adjust, long v)
    {
        ios_base<char8_t> ios;
        ios.fill(fill);
        ios.width(8);
        ios.setf(adjust, ios_defs::adjustfield);
        std::u8string out;
        try
        {
            obj.put(std::back_inserter(out), ios, v);
        }
        catch (const stream_error&)
        {
            return std::u8string();
        }
        return out;
    };

    // A digit in front of the digits reads as part of the number.
    EXPECT_EQ(put(u8'1', ios_defs::internal, 42L), u8"");
    EXPECT_EQ(put(u8'9', ios_defs::right, 42L), u8"");
    // A leading zero reads as the same number.
    EXPECT_EQ(put(u8'0', ios_defs::internal, 42L), u8"00000042");
    // A '-' in front of a positive number would make it negative.
    EXPECT_EQ(put(u8'-', ios_defs::internal, 42L), u8"");
    EXPECT_EQ(put(u8'-', ios_defs::internal, -42L), u8"------42");

    // A '+' in front of a negative number would cancel its sign; in front of a
    // positive one it says what was already true.
    EXPECT_EQ(put(u8'+', ios_defs::internal, -42L), u8"");
    EXPECT_EQ(put(u8'+', ios_defs::internal, 42L), u8"++++++42");

    // The decimal point binds to whatever digits follow it.
    EXPECT_EQ(put(u8'.', ios_defs::internal, 42L), u8"");
    EXPECT_EQ(put(u8'.', ios_defs::left, 42L), u8"42......");
    // Anything that cannot be read into a number is fine.
    EXPECT_EQ(put(u8'*', ios_defs::internal, 42L), u8"******42");
    EXPECT_EQ(put(u8' ', ios_defs::right, 42L), u8"      42");
}

// A grouped float stops at the first character that is not part of a number,
// which is the same rule as for an integer but reached through the float scan.
TEST(NumericChar8, AGroupedFloatAlsoStopsAtTheFirstForeignCharacter)
{
    const numeric<char8_t> obj = facet_of(tuned()->groups({3}).separator(u8',').point(u8'.').ptr());
    ios_base<char8_t>      ios;

    expect_parses(obj, ios, u8"1,234.5abc", 1234.5, u8"abc");
    expect_parses(obj, ios, u8"1,234.5,6", 1234.5, u8",6");
}

// A sign is read in two places -- in front of the mantissa and in front of the
// exponent -- by the same test, so both spellings have to be tried in both
// positions.
TEST(NumericChar8, AFloatingPointSignIsReadInBothPositions)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;

    expect_parses(obj, ios, u8"+1.5", 1.5);
    expect_parses(obj, ios, u8"-1.5", -1.5);
    expect_parses(obj, ios, u8"1.5e+3", 1500.0);
    expect_parses(obj, ios, u8"1.5e-3", 0.0015);
    expect_parses(obj, ios, u8"+1.5E+3", 1500.0);
    expect_parses(obj, ios, u8"-1.5E-3", -0.0015);
    expect_parses(obj, ios, u8"1.5e3", 1500.0);
}

// A locale is free to nominate a character that is also a sign, and punctuation
// wins: the decimal point is read as a decimal point, not as the start of a
// number that never arrives.
TEST(NumericChar8, PunctuationIsNotReadAsASign)
{
    const numeric<char8_t> obj = facet_of(tuned()->point(u8'-').groups({}).ptr());
    ios_base<char8_t>      ios;
    expect_parses(obj, ios, u8"1-5", 1.5);
}

// The two names are matched together and the longer win decides, so a name that
// is a prefix of the other must not end the match early.  An empty name matches
// nothing at all rather than matching immediately.
TEST(NumericChar8, OneBooleanNameMayBeAPrefixOfTheOther)
{
    ios_base<char8_t> ios;
    ios.setf(ios_defs::boolalpha);

    const numeric<char8_t> prefixed = facet_of(tuned()->yes(u8"yes").no(u8"y").ptr());
    expect_parses(prefixed, ios, u8"yes", true);
    expect_parses(prefixed, ios, u8"y", false);

    const numeric<char8_t> other_way = facet_of(tuned()->yes(u8"y").no(u8"yes").ptr());
    expect_parses(other_way, ios, u8"yes", false);
    expect_parses(other_way, ios, u8"y", true);

    const numeric<char8_t> unnamed = facet_of(tuned()->yes(u8"true").no(u8"").ptr());
    expect_parses(unnamed, ios, u8"true", true);
    expect_rejects<bool>(unnamed, ios, u8"");
}

// Grouping splits the text at the decimal point, so a float printed without one
// is the case where there is no split to make and the whole run is the integer
// part.  The sign in front of it is copied across separately, whichever it is.
TEST(NumericChar8, AGroupedFloatIsGroupedWithOrWithoutAPoint)
{
    const numeric<char8_t> obj = facet_of(tuned()->groups({3}).separator(u8',').point(u8'.').ptr());

    ios_base<char8_t> no_point;
    no_point.setf(ios_defs::fixed, ios_defs::floatfield);
    no_point.precision(0);
    EXPECT_EQ(put_str(obj, no_point, 1234567.0), u8"1,234,567");
    EXPECT_EQ(put_str(obj, no_point, -1234567.0), u8"-1,234,567");

    ios_base<char8_t> shown;
    shown.setf(ios_defs::fixed, ios_defs::floatfield);
    shown.setf(ios_defs::showpos);
    shown.precision(1);
    EXPECT_EQ(put_str(obj, shown, 1234567.5), u8"+1,234,567.5");
}

// Infinity and NaN are words, not numbers, so there is nothing in them to group
// however the locale would like its thousands separated.
TEST(NumericChar8, InfinityAndNotANumberAreNotGrouped)
{
    const numeric<char8_t> obj = facet_of(tuned()->groups({3}).separator(u8',').point(u8'.').ptr());
    ios_base<char8_t>      ios;

    const std::u8string inf = put_str(obj, ios, std::numeric_limits<double>::infinity());
    const std::u8string nan = put_str(obj, ios, std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(inf.find(u8','), std::u8string::npos);
    EXPECT_EQ(nan.find(u8','), std::u8string::npos);
    EXPECT_EQ(put_str(obj, ios, -std::numeric_limits<double>::infinity()), u8"-" + inf);
}

// A leading zero is a base prefix only where the base could still be decided.
// Under an explicit base it is just a digit, and the digits after it belong to
// the same number.
TEST(NumericChar8, ALeadingZeroUnderAnExplicitBaseIsJustADigit)
{
    const numeric<char8_t> obj = facet_for("C");

    ios_base<char8_t> hex;
    hex.setf(ios_defs::hex, ios_defs::basefield);
    expect_parses(obj, hex, u8"0ff", 255L);
    expect_parses(obj, hex, u8"00", 0L);

    ios_base<char8_t> octal;
    octal.setf(ios_defs::oct, ios_defs::basefield);
    expect_parses(obj, octal, u8"017", 15L);
}

// A field has to hold at least one digit to be a number.  A point or an exponent
// on its own is not one, and neither is a second point after the first.
TEST(NumericChar8, AFloatingPointFieldNeedsAtLeastOneDigit)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;

    expect_rejects<double>(obj, ios, u8".");
    expect_rejects<double>(obj, ios, u8"e5");
    expect_rejects<double>(obj, ios, u8".e5");

    // Past the first point or exponent, a second one ends the field rather than
    // extending it.
    expect_parses(obj, ios, u8"1.5.5", 1.5, u8".5");
    expect_parses(obj, ios, u8"1e5e5", 100000.0, u8"e5");
    expect_parses(obj, ios, u8"1.5e5.5", 150000.0, u8".5");
}

// A locale may nominate a character that is also a sign.  Punctuation wins, so
// the integer scanner reads it as the separator it was declared to be rather
// than as the sign it looks like.
TEST(NumericChar8, AnIntegerSignIsNotReadWhenItIsPunctuation)
{
    const numeric<char8_t> obj = facet_of(tuned()->groups({3}).separator(u8'-').point(u8'.').ptr());
    ios_base<char8_t>      ios;
    expect_parses(obj, ios, u8"1-234", 1234L);
}

// A group is closed and measured at every place the scan can decide one has
// ended: a separator, the decimal point, the exponent marker, and the end of the
// field.  A grouping that does not match at any of them is not this locale's.
TEST(NumericChar8, AFloatIsGroupCheckedAtEveryBoundary)
{
    const numeric<char8_t> obj = facet_of(tuned()->groups({3}).separator(u8',').point(u8'.').ptr());
    ios_base<char8_t>      ios;

    expect_parses(obj, ios, u8"1,234", 1234.0);
    expect_parses(obj, ios, u8"1,234.5", 1234.5);
    expect_parses(obj, ios, u8"1,234e2", 123400.0);
    expect_rejects<double>(obj, ios, u8"12,34");
    expect_rejects<double>(obj, ios, u8"12,34.5");
    expect_rejects<double>(obj, ios, u8"12,34e2");
}

// A name nobody spelled matches nothing: a zero-length name would otherwise
// match at once, before the other one had a chance to.
TEST(NumericChar8, AnEmptyBooleanNameMatchesNothing)
{
    ios_base<char8_t> ios;
    ios.setf(ios_defs::boolalpha);

    const numeric<char8_t> no_true = facet_of(tuned()->yes(u8"").no(u8"false").ptr());
    expect_parses(no_true, ios, u8"false", false);
    expect_rejects<bool>(no_true, ios, u8"true");
}

// Every floating-point width parses through its own instantiation of the
// scanner, so the fields that are not numbers have to be refused by each of
// them rather than by whichever one happened to be tried.
TEST(NumericChar8, EveryFloatingPointWidthRefusesTheSameNonNumbers)
{
    const numeric<char8_t> obj = facet_for("C");
    ios_base<char8_t>      ios;

    auto refuses = [&](auto tag)
    {
        using TVal = decltype(tag);
        const std::u8string not_numbers[] = {u8".", u8"e5", u8".e5", u8"+", u8"-", u8"", u8"1e"};
        for (const std::u8string& input : not_numbers)
        {
            SCOPED_TRACE(::testing::PrintToString(input));
            expect_rejects<TVal>(obj, ios, input);
        }
        // And accepts the same shapes it should, so the other side of each of
        // those tests is taken too.
        expect_parses(obj, ios, u8"1", TVal{1});
        expect_parses(obj, ios, u8"1.5", TVal{1.5});
        expect_parses(obj, ios, u8"1e2", TVal{100});
        expect_parses(obj, ios, u8"1.5e2", TVal{150});
        expect_parses(obj, ios, u8"1.5.5", TVal{1.5}, u8".5");
        expect_parses(obj, ios, u8"1e5e5", TVal{100000}, u8"e5");
    };

    refuses(0.0f);
    refuses(0.0);
    refuses(0.0L);
}

// The sign test is guarded by the punctuation test in both scanners, so a
// locale that spells its separator '-' cannot have a field read as negative:
// the '-' is a separator that has arrived before any digits, which is not a
// number at all.
TEST(NumericChar8, ALeadingPunctuationSignIsNotASign)
{
    const numeric<char8_t> obj = facet_of(tuned()->groups({3}).separator(u8'-').point(u8'+').ptr());
    ios_base<char8_t>      ios;

    expect_rejects<long>(obj, ios, u8"-234");
    expect_rejects<double>(obj, ios, u8"-234");

    // The same characters in the places the locale put them are punctuation and
    // parse as such -- including a leading '+', which here opens the fraction of
    // a number with no integer part rather than announcing a positive one.
    expect_parses(obj, ios, u8"1-234", 1234L);
    expect_parses(obj, ios, u8"1-234+5", 1234.5);
    expect_parses(obj, ios, u8"+5", 0.5);
}

// A grouping vector ending in zero says "and no further grouping beyond this",
// so the widths after it are unbounded rather than repeating the last entry.
TEST(NumericChar8, AZeroEndsTheGroupingRule)
{
    const numeric<char8_t> obj = facet_of(tuned()->groups({3, 0}).separator(u8',').ptr());
    ios_base<char8_t>      ios;

    EXPECT_EQ(put_str(obj, ios, 1234567L), u8"1234,567");
    expect_parses(obj, ios, u8"1234,567", 1234567L);
    expect_parses(obj, ios, u8"1,567", 1567L);
    expect_rejects<long>(obj, ios, u8"1234,56");
}
