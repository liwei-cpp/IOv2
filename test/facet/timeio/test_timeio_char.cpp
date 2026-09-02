// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * IOv2::timeio<char>: writing a time value out through strftime's conversion
 * specifiers and reading one back in through strptime's.
 *
 * Two things decide what a specifier produces.  The locale decides the words and
 * the composite layouts -- %a, %B, %c, %x, %X -- and those answers come from the
 * C library, so the cases that involve them are checked against strftime under
 * the same locale rather than against a date order written down here, which the
 * next glibc release is free to change.  The value decides what can be produced
 * at all: a hh_mm_ss has no year to print and a std::tm carries no zone, and a
 * specifier a value cannot supply is echoed back verbatim rather than guessed
 * at.  That second rule is what the six tables below enumerate, one per
 * (locale, value shape) pair, across every specifier and both modifiers.
 *
 * Reading is the same set of rules backwards, with one addition of its own: get()
 * is written against a sentinel so it can read a stream it cannot back up in.
 * Every parse here therefore runs three times -- over a string's iterators, over
 * a std::list's, and over an istreambuf_iterator -- and the three are required to
 * produce the same parse context.
 */
#include <facet/timeio.h>
#include <facet/timeio_details.h>

#include <common/defs.h>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>

#include <gtest/gtest.h>

#include <chrono>
#include <clocale>
#include <cstddef>
#include <ctime>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace IOv2;

namespace
{
    // m_zone_name / m_zone_abbrev point into the time-zone trie rather than owning a
    // string, so a null pointer -- not an empty one -- is what "the field was not
    // parsed" looks like.
    bool zone_is(const char* p, std::string_view s)
    {
        return p != nullptr && std::string_view{p} == s;
    }

    // Retrieves one conversion target from a parse context as a value.
    // time_parse_context fills into an out-parameter (convert_to) so that a std::tm keeps
    // whatever it held in fields the context does not reconstruct; these tests only ever
    // want the value, so they start from a value-initialised object.
    template <typename T, typename TCtx>
    T ctx_to(const TCtx& ctx)
    {
        if constexpr (std::is_same_v<T, std::remove_cvref_t<TCtx>>)
            return ctx;
        else
        {
            T v{};
            ctx.convert_to(v);
            return v;
        }
    }

    std::tm calendar_time(int year, int month, int day, int hour, int minute, int second,
                          int weekday, int yearday, int daylight)
    {
        std::tm tmp{};
        tmp.tm_year  = year;
        tmp.tm_mon   = month;
        tmp.tm_mday  = day;
        tmp.tm_hour  = hour;
        tmp.tm_min   = minute;
        tmp.tm_sec   = second;
        tmp.tm_wday  = weekday;
        tmp.tm_yday  = yearday;
        tmp.tm_isdst = daylight;
        return tmp;
    }

    template <typename TChar>
    class expanded_composite_conf final : public timeio_conf<TChar>
    {
    public:
        expanded_composite_conf()
            : timeio_conf<TChar>("C")
            , m_format(140, static_cast<TChar>('q'))
        {
            for (char c : std::string_view{"-%Y-%m-%d-%T-"})
                m_format.push_back(static_cast<TChar>(c));
            m_format.append(20, static_cast<TChar>('z'));
        }

        const std::basic_string<TChar>& date_time_format() const override { return m_format; }

    private:
        std::basic_string<TChar> m_format;
    };

    auto create_zoned_time(int y, unsigned m, unsigned d, int h, int min, int s,
                           const std::string& tz)
    {
        using namespace std::chrono;
        local_time<seconds> lt = local_days{year{y} / month{m} / day{d}}
                               + hours{h} + minutes{min} + seconds{s};
        return zoned_time{locate_zone(tz), lt};
    }

    timeio<char> facet_for(const char* loc)
    {
        return timeio<char>(std::make_shared<timeio_conf<char>>(loc));
    }

    template <typename TVal, typename... TSpec>
    std::string put_one(const timeio<char>& obj, const TVal& tp, TSpec... spec)
    {
        std::string res;
        obj.put(std::back_inserter(res), tp, spec...);
        return res;
    }

    // One conversion specifier, the modifier applied to it, and what the facet
    // writes.  A specifier the value cannot supply comes back as the format text
    // that asked for it, which is why so many rows read "%Ea" and the like.
    struct conversion
    {
        char        spec;
        char        mod;
        const char* expected;
    };

    template <typename TVal, std::size_t N>
    void expect_conversions(const timeio<char>& obj, const TVal& tp, const conversion (&table)[N])
    {
        for (const conversion& c : table)
        {
            // The specifiers are ASCII whatever the facet's character type is, so the
            // trace can name them narrowly.
            SCOPED_TRACE(std::string("%") + (c.mod ? std::string(1, static_cast<char>(c.mod))
                                                   : std::string())
                         + static_cast<char>(c.spec));
            EXPECT_EQ(c.mod ? put_one(obj, tp, c.spec, c.mod) : put_one(obj, tp, c.spec),
                      c.expected);
        }
    }

    constexpr ios_defs::iostate febit = ios_defs::eofbit | ios_defs::strfailbit;

    // Runs one parse three ways and requires the three to agree: over a string's
    // iterators, over a std::list's -- bidirectional, and not a pointer -- and over
    // an istreambuf_iterator against a sentinel, which is the shape get() is written
    // for and the only one that cannot be backed up or measured in advance.
    //
    // `err_exp` says which of the three outcomes to expect: goodbit for a parse that
    // stops before the end, eofbit for one that consumes everything, and anything
    // carrying strfailbit for one that fails.
    template <typename T = time_parse_context<char>, bool HaveDate = true, bool HaveTime = true,
              tz_level TzLevel = tz_level::zone, typename... TFmt>
    T run_get(const timeio<char>& obj, const std::string& input,
              ios_defs::iostate err_exp, TFmt... fmt)
    {
        time_parse_context<char, HaveDate, HaveTime, TzLevel> ctx1, ctx2, ctx3;
        std::list<char> lst(input.begin(), input.end());
        streambuf       sb(mem_device{input});
        auto            beg = istreambuf_iterator(sb);

        if ((err_exp & ios_defs::strfailbit) != 0)
        {
            EXPECT_THROW((void)obj.get(input.begin(), input.end(), ctx1, fmt...), stream_error);
            EXPECT_THROW((void)obj.get(lst.begin(), lst.end(), ctx2, fmt...), stream_error);
            EXPECT_THROW((void)obj.get(beg, std::default_sentinel, ctx3, fmt...), stream_error);
            return ctx_to<T>(ctx1);
        }

        const bool to_the_end = (err_exp == ios_defs::eofbit);
        EXPECT_EQ(obj.get(input.begin(), input.end(), ctx1, fmt...) == input.end(), to_the_end);
        EXPECT_EQ(obj.get(lst.begin(), lst.end(), ctx2, fmt...) == lst.end(), to_the_end);
        EXPECT_EQ(obj.get(beg, std::default_sentinel, ctx3, fmt...) == std::default_sentinel,
                  to_the_end);
        EXPECT_EQ(ctx2, ctx1);
        EXPECT_EQ(ctx3, ctx1);
        return ctx_to<T>(ctx1);
    }

    // The single-specifier and the format-string spellings, in the argument order the
    // cases below read most naturally: what to parse, how, and what to expect of it.
    template <typename T = time_parse_context<char>, bool HaveDate = true, bool HaveTime = true,
              tz_level TzLevel = tz_level::zone>
    T CheckGet(const timeio<char>& obj, const std::string& input, char fmt, char modif,
               ios_defs::iostate err_exp)
    {
        SCOPED_TRACE(::testing::PrintToString(input) + " | %"
                     + (modif ? std::string(1, static_cast<char>(modif)) : std::string())
                     + static_cast<char>(fmt));
        return run_get<T, HaveDate, HaveTime, TzLevel>(obj, input, err_exp, fmt, modif);
    }

    template <typename T = time_parse_context<char>, bool HaveDate = true, bool HaveTime = true,
              tz_level TzLevel = tz_level::zone>
    T CheckGet(const timeio<char>& obj, const std::string& input, const std::string& fmt,
               ios_defs::iostate err_exp)
    {
        SCOPED_TRACE(::testing::PrintToString(input) + " | " + ::testing::PrintToString(fmt));
        return run_get<T, HaveDate, HaveTime, TzLevel>(obj, input, err_exp, fmt);
    }
}

// Every conversion specifier and both modifiers, against one instant a reader can
// check by hand: 2024-09-04 13:33:18 in America/Los_Angeles, a Wednesday.  A
// specifier the value can supply is written; a modifier the locale has no
// alternative representation for leaves the format text as it was.
TEST(TimeioChar, CLocaleWritesEveryConversionSpecifier)
{
    const timeio<char> obj = facet_for("C");
    const auto         tp  = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    static const conversion kConversions[] = {
        {'%', 0, "%"},
        {'a', 0, "Wed"},
        {'a', 'E', "%Ea"},
        {'a', 'O', "%Oa"},
        {'A', 0, "Wednesday"},
        {'A', 'E', "%EA"},
        {'A', 'O', "%OA"},
        {'b', 0, "Sep"},
        {'b', 'E', "%Eb"},
        {'b', 'O', "%Ob"},
        {'h', 0, "Sep"},
        {'h', 'E', "%Eh"},
        {'h', 'O', "%Oh"},
        {'B', 0, "September"},
        {'B', 'E', "%EB"},
        {'B', 'O', "%OB"},
        {'c', 0, "Wed Sep  4 13:33:18 2024"},
        {'c', 'E', "Wed Sep  4 13:33:18 2024"},
        {'c', 'O', "%Oc"},
        {'C', 0, "20"},
        {'C', 'E', "20"},
        {'C', 'O', "%OC"},
        {'x', 0, "09/04/24"},
        {'x', 'E', "09/04/24"},
        {'x', 'O', "%Ox"},
        {'D', 0, "09/04/24"},
        {'D', 'E', "%ED"},
        {'D', 'O', "%OD"},
        {'d', 0, "04"},
        {'d', 'E', "%Ed"},
        {'d', 'O', "04"},
        {'e', 0, " 4"},
        {'e', 'E', "%Ee"},
        {'e', 'O', " 4"},
        {'F', 0, "2024-09-04"},
        {'F', 'E', "%EF"},
        {'F', 'O', "%OF"},
        {'H', 0, "13"},
        {'H', 'E', "%EH"},
        {'H', 'O', "13"},
        {'I', 0, "01"},
        {'I', 'E', "%EI"},
        {'I', 'O', "01"},
        {'j', 0, "248"},
        {'j', 'E', "%Ej"},
        {'j', 'O', "%Oj"},
        {'M', 0, "33"},
        {'M', 'E', "%EM"},
        {'M', 'O', "33"},
        {'m', 0, "09"},
        {'m', 'E', "%Em"},
        {'m', 'O', "09"},
        {'n', 0, "\n"},
        {'n', 'E', "%En"},
        {'n', 'O', "%On"},
        {'p', 0, "PM"},
        {'p', 'E', "%Ep"},
        {'p', 'O', "%Op"},
        {'R', 0, "13:33"},
        {'R', 'E', "%ER"},
        {'R', 'O', "%OR"},
        {'r', 0, "01:33:18 PM"},
        {'r', 'E', "%Er"},
        {'r', 'O', "%Or"},
        {'S', 0, "18"},
        {'S', 'E', "%ES"},
        {'S', 'O', "18"},
        {'X', 0, "13:33:18"},
        {'X', 'E', "13:33:18"},
        {'X', 'O', "%OX"},
        {'T', 0, "13:33:18"},
        {'T', 'E', "%ET"},
        {'T', 'O', "%OT"},
        {'t', 0, "\t"},
        {'t', 'E', "%Et"},
        {'t', 'O', "%Ot"},
        {'u', 0, "3"},
        {'u', 'E', "%Eu"},
        {'u', 'O', "3"},
        {'U', 0, "35"},
        {'U', 'E', "%EU"},
        {'U', 'O', "35"},
        {'V', 0, "36"},
        {'V', 'E', "%EV"},
        {'V', 'O', "36"},
        {'g', 0, "24"},
        {'g', 'E', "%Eg"},
        {'g', 'O', "%Og"},
        {'G', 0, "2024"},
        {'G', 'E', "%EG"},
        {'G', 'O', "%OG"},
        {'W', 0, "36"},
        {'W', 'E', "%EW"},
        {'W', 'O', "36"},
        {'w', 0, "3"},
        {'w', 'E', "%Ew"},
        {'w', 'O', "3"},
        {'Y', 0, "2024"},
        {'Y', 'E', "2024"},
        {'Y', 'O', "%OY"},
        {'y', 0, "24"},
        {'y', 'E', "24"},
        {'y', 'O', "24"},
        {'Z', 0, "America/Los_Angeles"},
        {'Z', 'E', "%EZ"},
        {'Z', 'O', "%OZ"},
        {'z', 0, "-0700"},
        {'z', 'E', "%Ez"},
        {'z', 'O', "%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// The same instant and the same specifiers under zh_CN, where the words and the
// composite layouts differ but the rules about what a value can supply do not.
TEST(TimeioChar, ChineseWritesEveryConversionSpecifier)
{
    const timeio<char> obj = facet_for("zh_CN.UTF-8");
    const auto         tp  = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    static const conversion kConversions[] = {
        {'%', 0, "%"},
        {'a', 0, "三"},
        {'a', 'E', "%Ea"},
        {'a', 'O', "%Oa"},
        {'A', 0, "星期三"},
        {'A', 'E', "%EA"},
        {'A', 'O', "%OA"},
        {'b', 0, "9月"},
        {'b', 'E', "%Eb"},
        {'b', 'O', "%Ob"},
        {'h', 0, "9月"},
        {'h', 'E', "%Eh"},
        {'h', 'O', "%Oh"},
        {'B', 0, "九月"},
        {'B', 'E', "%EB"},
        {'B', 'O', "%OB"},
        {'c', 0, "2024年09月04日 星期三 13时33分18秒"},
        {'c', 'E', "2024年09月04日 星期三 13时33分18秒"},
        {'c', 'O', "%Oc"},
        {'C', 0, "20"},
        {'C', 'E', "20"},
        {'C', 'O', "%OC"},
        {'x', 0, "2024年09月04日"},
        {'x', 'E', "2024年09月04日"},
        {'x', 'O', "%Ox"},
        {'D', 0, "09/04/24"},
        {'D', 'E', "%ED"},
        {'D', 'O', "%OD"},
        {'d', 0, "04"},
        {'d', 'E', "%Ed"},
        {'d', 'O', "04"},
        {'e', 0, " 4"},
        {'e', 'E', "%Ee"},
        {'e', 'O', " 4"},
        {'F', 0, "2024-09-04"},
        {'F', 'E', "%EF"},
        {'F', 'O', "%OF"},
        {'H', 0, "13"},
        {'H', 'E', "%EH"},
        {'H', 'O', "13"},
        {'I', 0, "01"},
        {'I', 'E', "%EI"},
        {'I', 'O', "01"},
        {'j', 0, "248"},
        {'j', 'E', "%Ej"},
        {'j', 'O', "%Oj"},
        {'M', 0, "33"},
        {'M', 'E', "%EM"},
        {'M', 'O', "33"},
        {'m', 0, "09"},
        {'m', 'E', "%Em"},
        {'m', 'O', "09"},
        {'n', 0, "\n"},
        {'n', 'E', "%En"},
        {'n', 'O', "%On"},
        {'p', 0, "下午"},
        {'p', 'E', "%Ep"},
        {'p', 'O', "%Op"},
        {'R', 0, "13:33"},
        {'R', 'E', "%ER"},
        {'R', 'O', "%OR"},
        {'r', 0, "下午 01时33分18秒"},
        {'r', 'E', "%Er"},
        {'r', 'O', "%Or"},
        {'S', 0, "18"},
        {'S', 'E', "%ES"},
        {'S', 'O', "18"},
        {'X', 0, "13时33分18秒"},
        {'X', 'E', "13时33分18秒"},
        {'X', 'O', "%OX"},
        {'T', 0, "13:33:18"},
        {'T', 'E', "%ET"},
        {'T', 'O', "%OT"},
        {'t', 0, "\t"},
        {'t', 'E', "%Et"},
        {'t', 'O', "%Ot"},
        {'u', 0, "3"},
        {'u', 'E', "%Eu"},
        {'u', 'O', "3"},
        {'U', 0, "35"},
        {'U', 'E', "%EU"},
        {'U', 'O', "35"},
        {'V', 0, "36"},
        {'V', 'E', "%EV"},
        {'V', 'O', "36"},
        {'g', 0, "24"},
        {'g', 'E', "%Eg"},
        {'g', 'O', "%Og"},
        {'G', 0, "2024"},
        {'G', 'E', "%EG"},
        {'G', 'O', "%OG"},
        {'W', 0, "36"},
        {'W', 'E', "%EW"},
        {'W', 'O', "36"},
        {'w', 0, "3"},
        {'w', 'E', "%Ew"},
        {'w', 'O', "3"},
        {'Y', 0, "2024"},
        {'Y', 'E', "2024"},
        {'Y', 'O', "%OY"},
        {'y', 0, "24"},
        {'y', 'E', "24"},
        {'y', 'O', "24"},
        {'Z', 0, "America/Los_Angeles"},
        {'Z', 'E', "%EZ"},
        {'Z', 'O', "%OZ"},
        {'z', 0, "-0700"},
        {'z', 'E', "%Ez"},
        {'z', 'O', "%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// And under ja_JP, which is the locale with an era representation, so %EC, %Ey
// and %EY are the rows to look at here.
TEST(TimeioChar, JapaneseWritesEveryConversionSpecifier)
{
    const timeio<char> obj = facet_for("ja_JP.UTF-8");
    const auto         tp  = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    static const conversion kConversions[] = {
        {'%', 0, "%"},
        {'a', 0, "水"},
        {'a', 'E', "%Ea"},
        {'a', 'O', "%Oa"},
        {'A', 0, "水曜日"},
        {'A', 'E', "%EA"},
        {'A', 'O', "%OA"},
        {'b', 0, " 9月"},
        {'b', 'E', "%Eb"},
        {'b', 'O', "%Ob"},
        {'h', 0, " 9月"},
        {'h', 'E', "%Eh"},
        {'h', 'O', "%Oh"},
        {'B', 0, "9月"},
        {'B', 'E', "%EB"},
        {'B', 'O', "%OB"},
        {'c', 0, "2024年09月04日 13時33分18秒"},
        {'c', 'E', "令和6年09月04日 13時33分18秒"},
        {'c', 'O', "%Oc"},
        {'C', 0, "20"},
        {'C', 'E', "令和"},
        {'C', 'O', "%OC"},
        {'x', 0, "2024年09月04日"},
        {'x', 'E', "令和6年09月04日"},
        {'x', 'O', "%Ox"},
        {'D', 0, "09/04/24"},
        {'D', 'E', "%ED"},
        {'D', 'O', "%OD"},
        {'d', 0, "04"},
        {'d', 'E', "%Ed"},
        {'d', 'O', "四"},
        {'e', 0, " 4"},
        {'e', 'E', "%Ee"},
        {'e', 'O', "四"},
        {'F', 0, "2024-09-04"},
        {'F', 'E', "%EF"},
        {'F', 'O', "%OF"},
        {'H', 0, "13"},
        {'H', 'E', "%EH"},
        {'H', 'O', "十三"},
        {'I', 0, "01"},
        {'I', 'E', "%EI"},
        {'I', 'O', "一"},
        {'j', 0, "248"},
        {'j', 'E', "%Ej"},
        {'j', 'O', "%Oj"},
        {'M', 0, "33"},
        {'M', 'E', "%EM"},
        {'M', 'O', "三十三"},
        {'m', 0, "09"},
        {'m', 'E', "%Em"},
        {'m', 'O', "九"},
        {'n', 0, "\n"},
        {'n', 'E', "%En"},
        {'n', 'O', "%On"},
        {'p', 0, "午後"},
        {'p', 'E', "%Ep"},
        {'p', 'O', "%Op"},
        {'R', 0, "13:33"},
        {'R', 'E', "%ER"},
        {'R', 'O', "%OR"},
        {'r', 0, "午後01時33分18秒"},
        {'r', 'E', "%Er"},
        {'r', 'O', "%Or"},
        {'S', 0, "18"},
        {'S', 'E', "%ES"},
        {'S', 'O', "十八"},
        {'X', 0, "13時33分18秒"},
        {'X', 'E', "13時33分18秒"},
        {'X', 'O', "%OX"},
        {'T', 0, "13:33:18"},
        {'T', 'E', "%ET"},
        {'T', 'O', "%OT"},
        {'t', 0, "\t"},
        {'t', 'E', "%Et"},
        {'t', 'O', "%Ot"},
        {'u', 0, "3"},
        {'u', 'E', "%Eu"},
        {'u', 'O', "三"},
        {'U', 0, "35"},
        {'U', 'E', "%EU"},
        {'U', 'O', "三十五"},
        {'V', 0, "36"},
        {'V', 'E', "%EV"},
        {'V', 'O', "三十六"},
        {'g', 0, "24"},
        {'g', 'E', "%Eg"},
        {'g', 'O', "%Og"},
        {'G', 0, "2024"},
        {'G', 'E', "%EG"},
        {'G', 'O', "%OG"},
        {'W', 0, "36"},
        {'W', 'E', "%EW"},
        {'W', 'O', "三十六"},
        {'w', 0, "3"},
        {'w', 'E', "%Ew"},
        {'w', 'O', "三"},
        {'Y', 0, "2024"},
        {'Y', 'E', "令和6年"},
        {'Y', 'O', "%OY"},
        {'y', 0, "24"},
        {'y', 'E', "6"},
        {'y', 'O', "二十四"},
        {'Z', 0, "America/Los_Angeles"},
        {'Z', 'E', "%EZ"},
        {'Z', 'O', "%OZ"},
        {'z', 0, "-0700"},
        {'z', 'E', "%Ez"},
        {'z', 'O', "%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// A year_month_day is a date and nothing else, so every specifier that asks for a
// time of day or a zone comes back as the text that asked for it.
TEST(TimeioChar, ADateWritesEveryConversionSpecifierItCanSupply)
{
    using namespace std::chrono;
    const timeio<char>   obj = facet_for("ja_JP.UTF-8");
    const year_month_day tp{year{2024}, month{9}, day{4}};

    static const conversion kConversions[] = {
        {'%', 0, "%"},
        {'a', 0, "水"},
        {'a', 'E', "%Ea"},
        {'a', 'O', "%Oa"},
        {'A', 0, "水曜日"},
        {'A', 'E', "%EA"},
        {'A', 'O', "%OA"},
        {'b', 0, " 9月"},
        {'b', 'E', "%Eb"},
        {'b', 'O', "%Ob"},
        {'h', 0, " 9月"},
        {'h', 'E', "%Eh"},
        {'h', 'O', "%Oh"},
        {'B', 0, "9月"},
        {'B', 'E', "%EB"},
        {'B', 'O', "%OB"},
        {'c', 0, "%c"},
        {'c', 'E', "%Ec"},
        {'c', 'O', "%Oc"},
        {'C', 0, "20"},
        {'C', 'E', "令和"},
        {'C', 'O', "%OC"},
        {'x', 0, "2024年09月04日"},
        {'x', 'E', "令和6年09月04日"},
        {'x', 'O', "%Ox"},
        {'D', 0, "09/04/24"},
        {'D', 'E', "%ED"},
        {'D', 'O', "%OD"},
        {'d', 0, "04"},
        {'d', 'E', "%Ed"},
        {'d', 'O', "四"},
        {'e', 0, " 4"},
        {'e', 'E', "%Ee"},
        {'e', 'O', "四"},
        {'F', 0, "2024-09-04"},
        {'F', 'E', "%EF"},
        {'F', 'O', "%OF"},
        {'H', 0, "%H"},
        {'H', 'E', "%EH"},
        {'H', 'O', "%OH"},
        {'I', 0, "%I"},
        {'I', 'E', "%EI"},
        {'I', 'O', "%OI"},
        {'j', 0, "248"},
        {'j', 'E', "%Ej"},
        {'j', 'O', "%Oj"},
        {'M', 0, "%M"},
        {'M', 'E', "%EM"},
        {'M', 'O', "%OM"},
        {'m', 0, "09"},
        {'m', 'E', "%Em"},
        {'m', 'O', "九"},
        {'n', 0, "\n"},
        {'n', 'E', "%En"},
        {'n', 'O', "%On"},
        {'p', 0, "%p"},
        {'p', 'E', "%Ep"},
        {'p', 'O', "%Op"},
        {'R', 0, "%R"},
        {'R', 'E', "%ER"},
        {'R', 'O', "%OR"},
        {'r', 0, "%r"},
        {'r', 'E', "%Er"},
        {'r', 'O', "%Or"},
        {'S', 0, "%S"},
        {'S', 'E', "%ES"},
        {'S', 'O', "%OS"},
        {'X', 0, "%X"},
        {'X', 'E', "%EX"},
        {'X', 'O', "%OX"},
        {'T', 0, "%T"},
        {'T', 'E', "%ET"},
        {'T', 'O', "%OT"},
        {'t', 0, "\t"},
        {'t', 'E', "%Et"},
        {'t', 'O', "%Ot"},
        {'u', 0, "3"},
        {'u', 'E', "%Eu"},
        {'u', 'O', "三"},
        {'U', 0, "35"},
        {'U', 'E', "%EU"},
        {'U', 'O', "三十五"},
        {'V', 0, "36"},
        {'V', 'E', "%EV"},
        {'V', 'O', "三十六"},
        {'g', 0, "24"},
        {'g', 'E', "%Eg"},
        {'g', 'O', "%Og"},
        {'G', 0, "2024"},
        {'G', 'E', "%EG"},
        {'G', 'O', "%OG"},
        {'W', 0, "36"},
        {'W', 'E', "%EW"},
        {'W', 'O', "三十六"},
        {'w', 0, "3"},
        {'w', 'E', "%Ew"},
        {'w', 'O', "三"},
        {'Y', 0, "2024"},
        {'Y', 'E', "令和6年"},
        {'Y', 'O', "%OY"},
        {'y', 0, "24"},
        {'y', 'E', "6"},
        {'y', 'O', "二十四"},
        {'Z', 0, "%Z"},
        {'Z', 'E', "%EZ"},
        {'Z', 'O', "%OZ"},
        {'z', 0, "%z"},
        {'z', 'E', "%Ez"},
        {'z', 'O', "%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// An hh_mm_ss is the mirror image: it has a time of day and no date at all.
TEST(TimeioChar, ATimeOfDayWritesEveryConversionSpecifierItCanSupply)
{
    using namespace std::chrono;
    const timeio<char>    obj = facet_for("ja_JP.UTF-8");
    const hh_mm_ss<seconds> tp{hours{13} + minutes{33} + seconds{18}};

    static const conversion kConversions[] = {
        {'%', 0, "%"},
        {'a', 0, "%a"},
        {'a', 'E', "%Ea"},
        {'a', 'O', "%Oa"},
        {'A', 0, "%A"},
        {'A', 'E', "%EA"},
        {'A', 'O', "%OA"},
        {'b', 0, "%b"},
        {'b', 'E', "%Eb"},
        {'b', 'O', "%Ob"},
        {'h', 0, "%h"},
        {'h', 'E', "%Eh"},
        {'h', 'O', "%Oh"},
        {'B', 0, "%B"},
        {'B', 'E', "%EB"},
        {'B', 'O', "%OB"},
        {'c', 0, "%c"},
        {'c', 'E', "%Ec"},
        {'c', 'O', "%Oc"},
        {'x', 0, "%x"},
        {'x', 'E', "%Ex"},
        {'x', 'O', "%Ox"},
        {'D', 0, "%D"},
        {'D', 'E', "%ED"},
        {'D', 'O', "%OD"},
        {'d', 0, "%d"},
        {'d', 'E', "%Ed"},
        {'d', 'O', "%Od"},
        {'e', 0, "%e"},
        {'e', 'E', "%Ee"},
        {'e', 'O', "%Oe"},
        {'F', 0, "%F"},
        {'F', 'E', "%EF"},
        {'F', 'O', "%OF"},
        {'H', 0, "13"},
        {'H', 'E', "%EH"},
        {'H', 'O', "十三"},
        {'I', 0, "01"},
        {'I', 'E', "%EI"},
        {'I', 'O', "一"},
        {'j', 0, "%j"},
        {'j', 'E', "%Ej"},
        {'j', 'O', "%Oj"},
        {'M', 0, "33"},
        {'M', 'E', "%EM"},
        {'M', 'O', "三十三"},
        {'m', 0, "%m"},
        {'m', 'E', "%Em"},
        {'m', 'O', "%Om"},
        {'n', 0, "\n"},
        {'n', 'E', "%En"},
        {'n', 'O', "%On"},
        {'p', 0, "午後"},
        {'p', 'E', "%Ep"},
        {'p', 'O', "%Op"},
        {'R', 0, "13:33"},
        {'R', 'E', "%ER"},
        {'R', 'O', "%OR"},
        {'r', 0, "午後01時33分18秒"},
        {'r', 'E', "%Er"},
        {'r', 'O', "%Or"},
        {'S', 0, "18"},
        {'S', 'E', "%ES"},
        {'S', 'O', "十八"},
        {'X', 0, "13時33分18秒"},
        {'X', 'E', "13時33分18秒"},
        {'X', 'O', "%OX"},
        {'T', 0, "13:33:18"},
        {'T', 'E', "%ET"},
        {'T', 'O', "%OT"},
        {'t', 0, "\t"},
        {'t', 'E', "%Et"},
        {'t', 'O', "%Ot"},
        {'u', 0, "%u"},
        {'u', 'E', "%Eu"},
        {'u', 'O', "%Ou"},
        {'U', 0, "%U"},
        {'U', 'E', "%EU"},
        {'U', 'O', "%OU"},
        {'V', 0, "%V"},
        {'V', 'E', "%EV"},
        {'V', 'O', "%OV"},
        {'g', 0, "%g"},
        {'g', 'E', "%Eg"},
        {'g', 'O', "%Og"},
        {'G', 0, "%G"},
        {'G', 'E', "%EG"},
        {'G', 'O', "%OG"},
        {'W', 0, "%W"},
        {'W', 'E', "%EW"},
        {'W', 'O', "%OW"},
        {'w', 0, "%w"},
        {'w', 'E', "%Ew"},
        {'w', 'O', "%Ow"},
        {'Y', 0, "%Y"},
        {'Y', 'E', "%EY"},
        {'Y', 'O', "%OY"},
        {'y', 0, "%y"},
        {'y', 'E', "%Ey"},
        {'y', 'O', "%Oy"},
        {'Z', 0, "%Z"},
        {'Z', 'E', "%EZ"},
        {'Z', 'O', "%OZ"},
        {'z', 0, "%z"},
        {'z', 'E', "%Ez"},
        {'z', 'O', "%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// A std::tm carries both halves, so almost everything is available; what it does
// not carry is a zone, which the cases after this one take up.
TEST(TimeioChar, ABrokenDownTimeWritesEveryConversionSpecifier)
{
    const timeio<char> obj = facet_for("ja_JP.UTF-8");
    const std::tm      tp  = calendar_time(2024 - 1900, 9 - 1, 4, 13, 33, 18, 0, 0, 0);

    static const conversion kConversions[] = {
        {'%', 0, "%"},
        {'a', 0, "水"},
        {'a', 'E', "%Ea"},
        {'a', 'O', "%Oa"},
        {'A', 0, "水曜日"},
        {'A', 'E', "%EA"},
        {'A', 'O', "%OA"},
        {'b', 0, " 9月"},
        {'b', 'E', "%Eb"},
        {'b', 'O', "%Ob"},
        {'h', 0, " 9月"},
        {'h', 'E', "%Eh"},
        {'h', 'O', "%Oh"},
        {'B', 0, "9月"},
        {'B', 'E', "%EB"},
        {'B', 'O', "%OB"},
        {'c', 0, "2024年09月04日 13時33分18秒"},
        {'c', 'E', "令和6年09月04日 13時33分18秒"},
        {'c', 'O', "%Oc"},
        {'C', 0, "20"},
        {'C', 'E', "令和"},
        {'C', 'O', "%OC"},
        {'x', 0, "2024年09月04日"},
        {'x', 'E', "令和6年09月04日"},
        {'x', 'O', "%Ox"},
        {'D', 0, "09/04/24"},
        {'D', 'E', "%ED"},
        {'D', 'O', "%OD"},
        {'d', 0, "04"},
        {'d', 'E', "%Ed"},
        {'d', 'O', "四"},
        {'e', 0, " 4"},
        {'e', 'E', "%Ee"},
        {'e', 'O', "四"},
        {'F', 0, "2024-09-04"},
        {'F', 'E', "%EF"},
        {'F', 'O', "%OF"},
        {'H', 0, "13"},
        {'H', 'E', "%EH"},
        {'H', 'O', "十三"},
        {'I', 0, "01"},
        {'I', 'E', "%EI"},
        {'I', 'O', "一"},
        {'j', 0, "248"},
        {'j', 'E', "%Ej"},
        {'j', 'O', "%Oj"},
        {'M', 0, "33"},
        {'M', 'E', "%EM"},
        {'M', 'O', "三十三"},
        {'m', 0, "09"},
        {'m', 'E', "%Em"},
        {'m', 'O', "九"},
        {'n', 0, "\n"},
        {'n', 'E', "%En"},
        {'n', 'O', "%On"},
        {'p', 0, "午後"},
        {'p', 'E', "%Ep"},
        {'p', 'O', "%Op"},
        {'R', 0, "13:33"},
        {'R', 'E', "%ER"},
        {'R', 'O', "%OR"},
        {'r', 0, "午後01時33分18秒"},
        {'r', 'E', "%Er"},
        {'r', 'O', "%Or"},
        {'S', 0, "18"},
        {'S', 'E', "%ES"},
        {'S', 'O', "十八"},
        {'X', 0, "13時33分18秒"},
        {'X', 'E', "13時33分18秒"},
        {'X', 'O', "%OX"},
        {'T', 0, "13:33:18"},
        {'T', 'E', "%ET"},
        {'T', 'O', "%OT"},
        {'t', 0, "\t"},
        {'t', 'E', "%Et"},
        {'t', 'O', "%Ot"},
        {'u', 0, "3"},
        {'u', 'E', "%Eu"},
        {'u', 'O', "三"},
        {'U', 0, "35"},
        {'U', 'E', "%EU"},
        {'U', 'O', "三十五"},
        {'V', 0, "36"},
        {'V', 'E', "%EV"},
        {'V', 'O', "三十六"},
        {'g', 0, "24"},
        {'g', 'E', "%Eg"},
        {'g', 'O', "%Og"},
        {'G', 0, "2024"},
        {'G', 'E', "%EG"},
        {'G', 'O', "%OG"},
        {'W', 0, "36"},
        {'W', 'E', "%EW"},
        {'W', 'O', "三十六"},
        {'w', 0, "3"},
        {'w', 'E', "%Ew"},
        {'w', 'O', "三"},
        {'Y', 0, "2024"},
        {'Y', 'E', "令和6年"},
        {'Y', 'O', "%OY"},
        {'y', 0, "24"},
        {'y', 'E', "6"},
        {'y', 'O', "二十四"},
        {'Z', 0, "UNKNOWN"},
        {'Z', 'E', "%EZ"},
        {'Z', 'O', "%OZ"},
        {'z', 0, "+0000"},
        {'z', 'E', "%Ez"},
        {'z', 'O', "%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

namespace
{
    template <typename T>
    concept can_hint_date = requires (T& c)
    { c.set_hint(std::chrono::year_month_day{}); };

    template <typename T>
    concept can_hint_time = requires (T& c)
    { c.set_hint(std::chrono::hh_mm_ss<std::chrono::seconds>{}); };

    template <typename T>
    concept can_hint_zone = requires (T& c)
    { c.set_hint(static_cast<const std::chrono::time_zone*>(nullptr)); };
}

namespace
{
    // Locale data no real locale would produce: each compound format string is a plain
    // assignable member, so the constructor's cycle check can be driven directly.
    struct rigged_conf : timeio_conf<char>
    {
        using base = timeio_conf<char>;
        using era_entry = ft_basic<timeio<char>>::era_entry;

        rigged_conf()
            : base("C")
            , m_dt(base::date_time_format())
            , m_era_dt(base::era_date_time_format())
            , m_d(base::date_format())
            , m_era_d(base::era_date_format())
            , m_t(base::time_format())
            , m_era_t(base::era_time_format())
            , m_r(base::am_pm_format())
            , m_eras(base::era_items())
        {}

        const std::string& date_time_format()     const override { return m_dt; }
        const std::string& era_date_time_format() const override { return m_era_dt; }
        const std::string& date_format()          const override { return m_d; }
        const std::string& era_date_format()      const override { return m_era_d; }
        const std::string& time_format()          const override { return m_t; }
        const std::string& era_time_format()      const override { return m_era_t; }
        const std::string& am_pm_format()         const override { return m_r; }
        const std::vector<era_entry>& era_items() const override { return m_eras; }

        std::string m_dt, m_era_dt, m_d, m_era_d, m_t, m_era_t, m_r;
        std::vector<era_entry> m_eras;
    };

    // One era spanning every year, so %EY always resolves to it.
    rigged_conf::era_entry one_era(const std::string& fmt)
    {
        rigged_conf::era_entry e{};
        e.name       = "TE";
        e.format     = fmt;
        e.from_year  = -32767;
        e.from_month = 1;
        e.from_day   = 1;
        e.to_year    = 32767;
        e.to_month   = 12;
        e.to_day     = 31;
        e.offset     = 1;
        e.direction  = 1;
        return e;
    }

    // Builds a rigged conf, lets `rig` change its format strings, and reports whether the
    // timeio constructor rejected it. Only the construction sits inside the try, so a
    // VERIFY failure -- which also throws runtime_error -- cannot pass for a rejection.
    template <typename TRig>
    bool rejects(TRig rig)
    {
        auto conf = std::make_shared<rigged_conf>();
        rig(*conf);
        try
        {
            timeio<char> obj(conf);
            (void)obj;
            return false;
        }
        catch (const std::runtime_error&) { return true; }
    }
}

// %Z is the one specifier a std::tm answers from a field rather than from the
// calendar, and the field may be empty two different ways.  calendar_time leaves
// tm_zone null; a caller can just as well set it to "".  Neither names a zone.
TEST(TimeioChar, ABrokenDownTimeNamesItsZoneOrSaysItCannot)
{
    const timeio<char> obj = facet_for("ja_JP.UTF-8");
    const std::tm      tp  = calendar_time(2024 - 1900, 9 - 1, 4, 13, 33, 18, 0, 0, 0);

    EXPECT_EQ(put_one(obj, tp, 'Z'), "UNKNOWN");
    EXPECT_EQ(put_one(obj, tp, 'z'), "+0000");

#ifdef __USE_MISC
    std::tm named = tp;
    named.tm_zone = "PST";
    EXPECT_EQ(put_one(obj, named, 'Z'), "PST");

    // An empty string is as nameless as a null pointer.
    named.tm_zone = "";
    EXPECT_EQ(put_one(obj, named, 'Z'), "UNKNOWN");
#endif
}

// %a, %A, %b, %B, %c, %x, %X and %r are the locale's own words and layouts.  What
// they should produce is therefore whatever the C library produces for the same
// instant under the same locale -- writing the strings out here instead would
// only record one glibc's idea of, say, the German date order, and break on the
// next release that revises it.
TEST(TimeioChar, TheLocaleDecidesItsWordsAndLayouts)
{
    const std::tm broken = calendar_time(1971 - 1900, 4 - 1, 4, 12, 0, 0, 0, 93, 0);

    for (const char* loc : {"C", "de_DE.UTF-8", "en_HK.UTF-8", "es_ES.UTF-8", "fr_FR.UTF-8"})
    {
        SCOPED_TRACE(loc);
        const timeio<char> obj = facet_for(loc);
        const auto         tp  = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

        // Three groups are left out.  %p and %r, because a locale with no AM/PM
        // designation makes strftime return zero, which it also returns on error,
        // so the two cannot be told apart.  %c and %X, because several locales
        // spell a zone into them, and the zone is what the value carries rather
        // than what the locale says -- a zoned_time names "America/Los_Angeles"
        // where the std::tm handed to strftime can only offer "PST".
        for (char spec : {'a', 'A', 'b', 'B', 'h', 'x'})
        {
            SCOPED_TRACE(spec);
            const std::string fmt = std::string("%") + spec;
            char              want[256] = {};
            std::setlocale(LC_ALL, loc);
            const std::size_t n = std::strftime(want, sizeof want, fmt.c_str(), &broken);
            std::setlocale(LC_ALL, "C");
            ASSERT_GT(n, 0u);
            EXPECT_EQ(put_one(obj, tp, spec), std::string(want, n));
        }

        // %Ex and %EX ask for the era date and time representations.  None of these
        // locales has one, so each falls back to the plain form rather than being
        // echoed as unsupported.
        EXPECT_EQ(put_one(obj, tp, 'x', 'E'), put_one(obj, tp, 'x'));
        EXPECT_EQ(put_one(obj, tp, 'X', 'E'), put_one(obj, tp, 'X'));
    }
}

// A format string is expanded one specifier at a time with the literal text
// between them passed through unchanged, so what it produces is exactly the
// concatenation of its pieces.  Stated that way the case needs no locale's words
// written down, and holds in every locale rather than in the one it was written
// for.
TEST(TimeioChar, AFormatStringIsExpandedSpecifierBySpecifier)
{
    const auto tp = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    for (const char* loc : {"C", "de_DE.UTF-8", "en_HK.UTF-8", "fr_FR.UTF-8", "ja_JP.UTF-8"})
    {
        SCOPED_TRACE(loc);
        const timeio<char> obj = facet_for(loc);

        EXPECT_EQ(put_one(obj, tp, std::string_view("%A, week %W of %B")),
                  put_one(obj, tp, 'A') + ", week " + put_one(obj, tp, 'W')
                                        + " of " + put_one(obj, tp, 'B'));

        // Literal text alone, and a format that is nothing but literal text.
        EXPECT_EQ(put_one(obj, tp, std::string_view("[%Y]")), "[" + put_one(obj, tp, 'Y') + "]");
        EXPECT_EQ(put_one(obj, tp, std::string_view("no specifiers")), "no specifiers");
    }
}

// put() writes through an iterator into whatever the caller supplied and returns
// where it stopped, so everything past that point has to be exactly as it was.
TEST(TimeioChar, PutIntoAnExistingBufferReturnsWhereItStopped)
{
    const timeio<char> obj = facet_for("C");
    const auto         tp  = create_zoned_time(1997, 6, 26, 12, 0, 0, "America/Los_Angeles");

    std::string buffer(50, '.');
    const auto  end = obj.put(buffer.begin(), tp, std::string_view("%F %T"));
    EXPECT_EQ(std::string(buffer.begin(), end), "1997-06-26 12:00:00");
    EXPECT_EQ(buffer.substr(19), std::string(31, '.'));

    // The same for a single specifier, whose length the caller cannot know in
    // advance because it is a word the locale chose.
    std::string one(20, '.');
    const auto  one_end = obj.put(one.begin(), tp, 'A');
    EXPECT_EQ(std::string(one.begin(), one_end), "Thursday");
    EXPECT_EQ(one.substr(8), std::string(12, '.'));
}

// The literal text in a format is part of what has to match: it is how the
// caller says which of several numbers is which.  Input past what the format
// asked for is left for whoever reads next.
TEST(TimeioChar, AFormatStringMustMatchTheInputLiterally)
{
    const timeio<char> obj = facet_for("C");

    const auto t = ctx_to<std::tm>(
        CheckGet(obj, "on 2024-09-04 at 01:09:35", "on %Y-%m-%d at %H:%M:%S", ios_defs::eofbit));
    EXPECT_EQ(t.tm_year, 124);
    EXPECT_EQ(t.tm_mon, 8);
    EXPECT_EQ(t.tm_mday, 4);
    EXPECT_EQ(t.tm_hour, 1);
    EXPECT_EQ(t.tm_min, 9);
    EXPECT_EQ(t.tm_sec, 35);

    // Literal text the input does not carry.
    CheckGet(obj, "at 2024-09-04", "on %Y-%m-%d", ios_defs::strfailbit);
    CheckGet(obj, "2024-09-04", "on %Y-%m-%d", ios_defs::strfailbit);

    // A '%' with nothing after it is not a specifier.
    CheckGet(obj, "2024-09-04", "%", ios_defs::strfailbit);

    // What the format did not ask for stays in the input.
    const auto rest = ctx_to<std::tm>(CheckGet(obj, "2020  ", "%Y", ios_defs::goodbit));
    EXPECT_EQ(rest.tm_year, 120);

    // A single specifier without a format string reads the same field.
    EXPECT_EQ(ctx_to<std::tm>(CheckGet(obj, "2020", 'Y', 0, ios_defs::eofbit)).tm_year, 120);
}

// The words a locale writes are the words it reads.  Round-tripping through the
// facet's own output says that in every locale at once, without this file having
// to know how any of them spells a month.
TEST(TimeioChar, TheNamesTheLocaleWritesAreTheNamesItReads)
{
    using namespace std::chrono;
    const year_month_day date{year{2014}, month{4}, day{14}};

    for (const char* loc : {"C", "de_DE.UTF-8", "es_ES.UTF-8", "fr_FR.UTF-8", "ja_JP.UTF-8"})
    {
        SCOPED_TRACE(loc);
        const timeio<char> obj = facet_for(loc);

        for (const char* fmt : {"%A, %d. %B %Y", "%a %d %b %Y", "%A %j %Y"})
        {
            SCOPED_TRACE(::testing::PrintToString(fmt));
            const std::string written = put_one(obj, date, std::string_view(fmt));
            EXPECT_EQ(CheckGet<year_month_day>(obj, written, fmt, ios_defs::eofbit), date);
        }
    }
}

// The specifiers that carry only part of a date -- a week number and a weekday,
// a day of the year, a century and a two-digit year -- have to reassemble into
// the date they were written from.  Stated as a round trip it holds for every
// date rather than for a handful with hand-computed week numbers.
TEST(TimeioChar, EveryDateReassemblesFromItsPartialSpecifiers)
{
    using namespace std::chrono;
    const timeio<char> obj = facet_for("C");

    const char* const formats[] = {
        "%F", "%Y-%m-%d", "%d-%b-%Y", "%C%y-%m-%d",
        "%Y %U %w", "%Y %W %w", "%Y %W %a", "%Y %U %A", "%j %Y",
    };

    // 29 days apart, so the sweep lands on every weekday and crosses the turn of
    // each year, which is where the week-number rules disagree with each other.
    for (sys_days d = sys_days{2019y / January / 1}; d <= sys_days{2024y / December / 31};
         d += days{29})
    {
        const year_month_day date{d};
        for (const char* fmt : formats)
        {
            SCOPED_TRACE(::testing::PrintToString(fmt) + " | "
                         + ::testing::PrintToString(put_one(obj, date, std::string_view("%F"))));
            const std::string written = put_one(obj, date, std::string_view(fmt));
            EXPECT_EQ(CheckGet<year_month_day>(obj, written, fmt, ios_defs::eofbit), date);
        }
    }
}

// %I is a clock face: it cannot tell noon from midnight on its own, and %p is
// what supplies the half of the day it belongs to.  Either order.
TEST(TimeioChar, TheTwelveHourClockNeedsItsMeridiem)
{
    using namespace std::chrono;
    const timeio<char> obj = facet_for("C");

    for (int hour = 0; hour < 24; ++hour)
    {
        SCOPED_TRACE(hour);
        const seconds          when = hours{hour} + minutes{5} + seconds{9};
        const hh_mm_ss<seconds> tp{when};

        for (const char* fmt : {"%I:%M:%S %p", "%p%I:%M:%S", "%r", "%T"})
        {
            SCOPED_TRACE(::testing::PrintToString(fmt));
            const std::string written = put_one(obj, tp, std::string_view(fmt));
            const auto        back =
                CheckGet<hh_mm_ss<seconds>, false, true, tz_level::none>(obj, written, fmt,
                                                                         ios_defs::eofbit);
            EXPECT_EQ(back.to_duration(), when);
        }
    }

    // Without the meridiem the same field reads as the morning hour, because that
    // is the half of the day a clock face means when nothing says otherwise.
    const auto morning = ctx_to<std::tm>(CheckGet(obj, "07:05:09", "%I:%M:%S", ios_defs::eofbit));
    EXPECT_EQ(morning.tm_hour, 7);
}

// A composite is a format indirection, not a fixed-size C buffer.  Supplying a
// deliberately long format makes that observable without depending on one
// platform locale continuing to have a particular spelling.
TEST(TimeioChar, ACompositeFormatCanExpandPastAConventionalStackBuffer)
{
    std::shared_ptr<timeio_conf<char>> conf = std::make_shared<expanded_composite_conf<char>>();
    timeio                            obj(conf);
    auto zt = create_zoned_time(2022, 11, 17, 21, 47, 26, "America/Los_Angeles");

    std::string actual;
    obj.put(std::back_inserter(actual), zt, 'c');

    std::string expected(140, 'q');
    expected += "-2022-11-17-21:47:26-";
    expected.append(20, 'z');

    EXPECT_GT(actual.size(), 128u);
    EXPECT_EQ(actual, expected);
}

TEST(TimeioChar, TheCLocaleReadsEveryConversionSpecifier)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};

    timeio obj(std::make_shared<timeio_conf<char>>("C"));
    CheckGet(obj, "%",   '%',  0,  ios_defs::eofbit);
    CheckGet(obj, "x",   '%',  0,  ios_defs::strfailbit);
    CheckGet(obj, "%",   '%', 'E', febit);
    CheckGet(obj, "%E%", '%', 'E', ios_defs::eofbit);
    CheckGet(obj, "%",   '%', 'O', febit);
    CheckGet(obj, "%O%", '%', 'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet(obj, "Wed", 'a', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, "%Ea", 'a', 'E', ios_defs::eofbit);
    CheckGet(obj, "a",   'a', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oa", 'a', 'O', ios_defs::eofbit);
    CheckGet(obj, "a",   'a', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "Wednesday", 'A', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, "%EA", 'A', 'E', ios_defs::eofbit);
    CheckGet(obj, "A",   'A', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OA", 'A', 'O', ios_defs::eofbit);
    CheckGet(obj, "A",   'A', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "Sep", 'b', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, "%Eb", 'b', 'E', ios_defs::eofbit);
    CheckGet(obj, "b",   'b', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Ob", 'b', 'O', ios_defs::eofbit);
    CheckGet(obj, "b",   'b', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "September", 'B', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, "%EB", 'B', 'E', ios_defs::eofbit);
    CheckGet(obj, "B",   'B', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OB", 'B', 'O', ios_defs::eofbit);
    CheckGet(obj, "B",   'B', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "Sep", 'h', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, "%Eh", 'h', 'E', ios_defs::eofbit);
    CheckGet(obj, "h",   'h', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oh", 'h', 'O', ios_defs::eofbit);
    CheckGet(obj, "h",   'h', 'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(CheckGet<year_month_day>(obj, "Wed Sep  4 13:33:18 2024", 'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "Wed Sep  4 13:33:18 2024", 'c', 'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, "c",   'c', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oc", 'c', 'O', ios_defs::eofbit);
    CheckGet(obj, "c",   'c', 'O', ios_defs::strfailbit);


    EXPECT_EQ(CheckGet(obj, "20", 'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(CheckGet(obj, "20", 'C', 'E', ios_defs::eofbit).m_century, 20);
    CheckGet(obj, "C",   'C', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OC", 'C', 'O', ios_defs::eofbit);
    CheckGet(obj, "C",   'C', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "04", 'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, "04", 'd', 'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, "%Ed", 'd', 'E', ios_defs::eofbit);
    CheckGet(obj, "d",   'd', 'E', ios_defs::strfailbit);
    CheckGet(obj, "d",   'd', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "4", 'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, "4", 'e', 'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, "%Ee", 'e', 'E', ios_defs::eofbit);
    CheckGet(obj, "e",   'e', 'E', ios_defs::strfailbit);
    CheckGet(obj, "e",   'e', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024-09-04", 'F', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, "%EF", 'F', 'E', ios_defs::eofbit);
    CheckGet(obj, "F",   'F', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OF", 'F', 'O', ios_defs::eofbit);
    CheckGet(obj, "F",   'F', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "09/04/24", 'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "09/04/24", 'x', 'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, "x",   'x', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Ox", 'x', 'O', ios_defs::eofbit);
    CheckGet(obj, "x",   'x', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "09/04/24", 'D', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, "%ED", 'D', 'E', ios_defs::eofbit);
    CheckGet(obj, "D",   'D', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OD", 'D', 'O', ios_defs::eofbit);
    CheckGet(obj, "D",   'D', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "13", 'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, "13", 'H', 'O', ios_defs::eofbit).m_hour, 13);
    CheckGet(obj, "%EH", 'H', 'E', ios_defs::eofbit);
    CheckGet(obj, "H",   'H', 'E', ios_defs::strfailbit);
    CheckGet(obj, "H",   'H', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "01", 'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, "01", 'I', 'O', ios_defs::eofbit).m_hour, 1);
    CheckGet(obj, "%EI", 'I', 'E', ios_defs::eofbit);
    CheckGet(obj, "I",   'I', 'E', ios_defs::strfailbit);
    CheckGet(obj, "I",   'I', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "248", 'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 248", "%Y %j", ios_defs::eofbit), check_date1);
    CheckGet(obj, "%Ej", 'j', 'E', ios_defs::eofbit);
    CheckGet(obj, "j",   'j', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oj", 'j', 'O', ios_defs::eofbit);
    CheckGet(obj, "j",   'j', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "09", 'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, "09", 'm', 'O', ios_defs::eofbit).m_month, 9);
    CheckGet(obj, "%Em", 'm', 'E', ios_defs::eofbit);
    CheckGet(obj, "m",   'm', 'E', ios_defs::strfailbit);
    CheckGet(obj, "m",   'm', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "33", 'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, "33", 'M', 'O', ios_defs::eofbit).m_minute, 33);
    CheckGet(obj, "%EM", 'M', 'E', ios_defs::eofbit);
    CheckGet(obj, "M",   'M', 'E', ios_defs::strfailbit);
    CheckGet(obj, "M",   'M', 'O', ios_defs::strfailbit);

    CheckGet(obj, "\n",   'n',  0,  ios_defs::eofbit);
    CheckGet(obj, "x",    'n',  0,  ios_defs::goodbit);
    CheckGet(obj, "\n",   'n', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%En",  'n', 'E', ios_defs::eofbit);
    CheckGet(obj, "n",    'n', 'O', ios_defs::strfailbit);
    CheckGet(obj, "%On",  'n', 'O', ios_defs::eofbit);

    CheckGet(obj, "\t",   't',  0,  ios_defs::eofbit);
    CheckGet(obj, "x",    't',  0,  ios_defs::goodbit);
    CheckGet(obj, "\t",   't', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Et",  't', 'E', ios_defs::eofbit);
    CheckGet(obj, "n",    't', 'O', ios_defs::strfailbit);
    CheckGet(obj, "%Ot",  't', 'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 PM", "%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 AM", "%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(CheckGet(obj, "PM", 'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(CheckGet(obj, "AM", 'p', 0, ios_defs::eofbit).m_is_pm, false);
    CheckGet(obj, "%Ep", 'p', 'E', ios_defs::eofbit);
    CheckGet(obj, "p",   'p', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Op", 'p', 'O', ios_defs::eofbit);
    CheckGet(obj, "p",   'p', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01:33:18 PM", "%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "%Er", 'r', 'E', ios_defs::eofbit);
    CheckGet(obj, "r",   'r', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Or", 'r', 'O', ios_defs::eofbit);
    CheckGet(obj, "r",   'r', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33", "%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, "%ER", 'R', 'E', ios_defs::eofbit);
    CheckGet(obj, "R",   'R', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OR", 'R', 'O', ios_defs::eofbit);
    CheckGet(obj, "R",   'R', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "18", 'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, "18", 'S', 'O', ios_defs::eofbit).m_second, 18);
    CheckGet(obj, "%ES", 'S', 'E', ios_defs::eofbit);
    CheckGet(obj, "S",   'S', 'E', ios_defs::strfailbit);
    CheckGet(obj, "S",   'S', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18", "%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18", "%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "X",   'X', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OX", 'X', 'O', ios_defs::eofbit);
    CheckGet(obj, "X",   'X', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18", "%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "%ET", 'T', 'E', ios_defs::eofbit);
    CheckGet(obj, "T",   'T', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OT", 'T', 'O', ios_defs::eofbit);
    CheckGet(obj, "T",   'T', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "3", 'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, "3", 'u', 'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, "%Eu", 'u', 'E', ios_defs::eofbit);
    CheckGet(obj, "u",   'u', 'E', ios_defs::strfailbit);
    CheckGet(obj, "u",   'u', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "24", 'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, "%Eg", 'g', 'E', ios_defs::eofbit);
    CheckGet(obj, "g",   'g', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Og", 'g', 'O', ios_defs::eofbit);
    CheckGet(obj, "g",   'g', 'O', ios_defs::strfailbit);


    EXPECT_EQ(CheckGet(obj, "2024", 'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, "%EG", 'G', 'E', ios_defs::eofbit);
    CheckGet(obj, "G",   'G', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OG", 'G', 'O', ios_defs::eofbit);
    CheckGet(obj, "G",   'G', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 35 Wed", "%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 35 Wed", "%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, "35", 'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(CheckGet(obj, "35", 'U', 'O', ios_defs::eofbit).m_week_no, 35);
    CheckGet(obj, "%EU", 'U', 'E', ios_defs::eofbit);
    CheckGet(obj, "U",   'U', 'E', ios_defs::strfailbit);
    CheckGet(obj, "U",   'U', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 36 Wed", "%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 36 Wed", "%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, "36", 'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(CheckGet(obj, "36", 'W', 'O', ios_defs::eofbit).m_week_no, 36);
    CheckGet(obj, "%EW", 'W', 'E', ios_defs::eofbit);
    CheckGet(obj, "W",   'W', 'E', ios_defs::strfailbit);
    CheckGet(obj, "W",   'W', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "36", 'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    CheckGet(obj, "54",  'V', 'O', ios_defs::strfailbit);
    CheckGet(obj, "36",  'V', 'O', ios_defs::eofbit);
    CheckGet(obj, "%EV", 'V', 'E', ios_defs::eofbit);
    CheckGet(obj, "V",   'V', 'E', ios_defs::strfailbit);
    CheckGet(obj, "V",   'V', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "3", 'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, "3", 'w', 'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, "%Ew", 'w', 'E', ios_defs::eofbit);
    CheckGet(obj, "w",   'w', 'E', ios_defs::strfailbit);
    CheckGet(obj, "w",   'w', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "24", 'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, "24", 'y', 'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, "24", 'y', 'O', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, "y",  'y', 'E', ios_defs::strfailbit);
    CheckGet(obj, "y",  'y', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "2024", 'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, "2024", 'Y', 'E', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, "Y",   'Y', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OY", 'Y', 'O', ios_defs::eofbit);
    CheckGet(obj, "Y",   'Y', 'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(CheckGet(obj, "America/Los_Angeles", 'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, "PST", 'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, "America/Los_Angexes", 'Z', 0, ios_defs::strfailbit);
    CheckGet(obj, "%EZ", 'Z', 'E', ios_defs::eofbit);
    CheckGet(obj, "Z",   'Z', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OZ", 'Z', 'O', ios_defs::eofbit);
    CheckGet(obj, "Z",   'Z', 'O', ios_defs::strfailbit);

    CheckGet(obj, "Z", 'z', 0, ios_defs::eofbit);
    CheckGet(obj, "+13", 'z', 0, ios_defs::eofbit);
    CheckGet(obj, "-1110", 'z', 0, ios_defs::eofbit);
    CheckGet(obj, "+11:10", 'z', 0, ios_defs::eofbit);
    CheckGet(obj, "%Ez", 'z', 'E', ios_defs::eofbit);
    CheckGet(obj, "z",  'z', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oz", 'z', 'O', ios_defs::eofbit);
    CheckGet(obj, "z",  'z', 'O', ios_defs::strfailbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(CheckGet<year_month_day>(obj, "1999-W52-6", "%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2019-W01-1", "%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "1999-W52-5", "%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "99-W52-6", "%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "19-W01-1", "%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "99-W52-5", "%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "20 24/09/04", "%C %y/%m/%d", ios_defs::eofbit), check_date1);

    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((CheckGet<year_month_day>(obj, "20 01 01", "%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar, ChineseReadsEveryConversionSpecifier)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<char>>("zh_CN.UTF-8"));

    CheckGet(obj, "%",  '%',  0,  ios_defs::eofbit);
    CheckGet(obj, "x",  '%',  0,  ios_defs::strfailbit);
    CheckGet(obj, "%",  '%', 'E', febit);
    CheckGet(obj, "%E%", '%', 'E', ios_defs::eofbit);
    CheckGet(obj, "%",  '%', 'O', febit);
    CheckGet(obj, "%O%", '%', 'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet(obj, "三", 'a', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, "%Ea", 'a', 'E', ios_defs::eofbit);
    CheckGet(obj, "a",   'a', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oa", 'a', 'O', ios_defs::eofbit);
    CheckGet(obj, "a",   'a', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "星期三", 'A', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, "%EA", 'A', 'E', ios_defs::eofbit);
    CheckGet(obj, "A",   'A', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OA", 'A', 'O', ios_defs::eofbit);
    CheckGet(obj, "A",   'A', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "九月", 'b', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, "%Eb", 'b', 'E', ios_defs::eofbit);
    CheckGet(obj, "b",   'b', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Ob", 'b', 'O', ios_defs::eofbit);
    CheckGet(obj, "b",   'b', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "九月", 'B', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, "%EB", 'B', 'E', ios_defs::eofbit);
    CheckGet(obj, "B",   'B', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OB", 'B', 'O', ios_defs::eofbit);
    CheckGet(obj, "B",   'B', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "九月", 'h', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, "%Eh", 'h', 'E', ios_defs::eofbit);
    CheckGet(obj, "h",   'h', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oh", 'h', 'O', ios_defs::eofbit);
    CheckGet(obj, "h",   'h', 'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024年09月04日 星期三 13时33分18秒", 'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024年09月04日 星期三 13时33分18秒", 'c', 'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, "c",   'c', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oc", 'c', 'O', ios_defs::eofbit);
    CheckGet(obj, "c",   'c', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "20", 'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(CheckGet(obj, "20", 'C', 'E', ios_defs::eofbit).m_century, 20);
    CheckGet(obj, "C",   'C', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OC", 'C', 'O', ios_defs::eofbit);
    CheckGet(obj, "C",   'C', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "04", 'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, "04", 'd', 'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, "%Ed", 'd', 'E', ios_defs::eofbit);
    CheckGet(obj, "d",   'd', 'E', ios_defs::strfailbit);
    CheckGet(obj, "d",   'd', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "4", 'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, "4", 'e', 'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, "%Ee", 'e', 'E', ios_defs::eofbit);
    CheckGet(obj, "e",   'e', 'E', ios_defs::strfailbit);
    CheckGet(obj, "e",   'e', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024-09-04", 'F', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, "%EF", 'F', 'E', ios_defs::eofbit);
    CheckGet(obj, "F",   'F', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OF", 'F', 'O', ios_defs::eofbit);
    CheckGet(obj, "F",   'F', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024年09月04日", 'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024年09月04日", 'x', 'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, "x",   'x', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Ox", 'x', 'O', ios_defs::eofbit);
    CheckGet(obj, "x",   'x', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "09/04/24", 'D', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, "%ED", 'D', 'E', ios_defs::eofbit);
    CheckGet(obj, "D",   'D', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OD", 'D', 'O', ios_defs::eofbit);
    CheckGet(obj, "D",   'D', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "13", 'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, "13", 'H', 'O', ios_defs::eofbit).m_hour, 13);
    CheckGet(obj, "%EH", 'H', 'E', ios_defs::eofbit);
    CheckGet(obj, "H",   'H', 'E', ios_defs::strfailbit);
    CheckGet(obj, "H",   'H', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "01", 'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, "01", 'I', 'O', ios_defs::eofbit).m_hour, 1);
    CheckGet(obj, "%EI", 'I', 'E', ios_defs::eofbit);
    CheckGet(obj, "I",   'I', 'E', ios_defs::strfailbit);
    CheckGet(obj, "I",   'I', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "248", 'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 248", "%Y %j", ios_defs::eofbit), check_date1);
    CheckGet(obj, "%Ej", 'j', 'E', ios_defs::eofbit);
    CheckGet(obj, "j",   'j', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oj", 'j', 'O', ios_defs::eofbit);
    CheckGet(obj, "j",   'j', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "09", 'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, "09", 'm', 'O', ios_defs::eofbit).m_month, 9);
    CheckGet(obj, "%Em", 'm', 'E', ios_defs::eofbit);
    CheckGet(obj, "m",   'm', 'E', ios_defs::strfailbit);
    CheckGet(obj, "m",   'm', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "33", 'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, "33", 'M', 'O', ios_defs::eofbit).m_minute, 33);
    CheckGet(obj, "%EM", 'M', 'E', ios_defs::eofbit);
    CheckGet(obj, "M",   'M', 'E', ios_defs::strfailbit);
    CheckGet(obj, "M",   'M', 'O', ios_defs::strfailbit);

    CheckGet(obj, "\n",   'n',  0,  ios_defs::eofbit);
    CheckGet(obj, "x",    'n',  0,  ios_defs::goodbit);
    CheckGet(obj, "\n",   'n', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%En",  'n', 'E', ios_defs::eofbit);
    CheckGet(obj, "n",    'n', 'O', ios_defs::strfailbit);
    CheckGet(obj, "%On",  'n', 'O', ios_defs::eofbit);

    CheckGet(obj, "\t",   't',  0,  ios_defs::eofbit);
    CheckGet(obj, "x",    't',  0,  ios_defs::goodbit);
    CheckGet(obj, "\t",   't', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Et",  't', 'E', ios_defs::eofbit);
    CheckGet(obj, "n",    't', 'O', ios_defs::strfailbit);
    CheckGet(obj, "%Ot",  't', 'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 下午", "%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 上午", "%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(CheckGet(obj, "下午", 'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(CheckGet(obj, "上午", 'p', 0, ios_defs::eofbit).m_is_pm, false);
    CheckGet(obj, "%Ep", 'p', 'E', ios_defs::eofbit);
    CheckGet(obj, "p",   'p', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Op", 'p', 'O', ios_defs::eofbit);
    CheckGet(obj, "p",   'p', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "下午 01时33分18秒", "%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "%Er", 'r', 'E', ios_defs::eofbit);
    CheckGet(obj, "r",   'r', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Or", 'r', 'O', ios_defs::eofbit);
    CheckGet(obj, "r",   'r', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33", "%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, "%ER", 'R', 'E', ios_defs::eofbit);
    CheckGet(obj, "R",   'R', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OR", 'R', 'O', ios_defs::eofbit);
    CheckGet(obj, "R",   'R', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "18", 'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, "18", 'S', 'O', ios_defs::eofbit).m_second, 18);
    CheckGet(obj, "%ES", 'S', 'E', ios_defs::eofbit);
    CheckGet(obj, "S",   'S', 'E', ios_defs::strfailbit);
    CheckGet(obj, "S",   'S', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13时33分18秒", "%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13时33分18秒", "%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "X",   'X', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OX", 'X', 'O', ios_defs::eofbit);
    CheckGet(obj, "X",   'X', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18", "%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "%ET", 'T', 'E', ios_defs::eofbit);
    CheckGet(obj, "T",   'T', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OT", 'T', 'O', ios_defs::eofbit);
    CheckGet(obj, "T",   'T', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "3", 'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, "3", 'u', 'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, "%Eu", 'u', 'E', ios_defs::eofbit);
    CheckGet(obj, "u",   'u', 'E', ios_defs::strfailbit);
    CheckGet(obj, "u",   'u', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "24", 'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, "%Eg", 'g', 'E', ios_defs::eofbit);
    CheckGet(obj, "g",   'g', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Og", 'g', 'O', ios_defs::eofbit);
    CheckGet(obj, "g",   'g', 'O', ios_defs::strfailbit);


    EXPECT_EQ(CheckGet(obj, "2024", 'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, "%EG", 'G', 'E', ios_defs::eofbit);
    CheckGet(obj, "G",   'G', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OG", 'G', 'O', ios_defs::eofbit);
    CheckGet(obj, "G",   'G', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 35 三", "%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 35 三", "%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, "35", 'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(CheckGet(obj, "35", 'U', 'O', ios_defs::eofbit).m_week_no, 35);
    CheckGet(obj, "%EU", 'U', 'E', ios_defs::eofbit);
    CheckGet(obj, "U",   'U', 'E', ios_defs::strfailbit);
    CheckGet(obj, "U",   'U', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 36 三", "%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 36 三", "%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, "36", 'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(CheckGet(obj, "36", 'W', 'O', ios_defs::eofbit).m_week_no, 36);
    CheckGet(obj, "%EW", 'W', 'E', ios_defs::eofbit);
    CheckGet(obj, "W",   'W', 'E', ios_defs::strfailbit);
    CheckGet(obj, "W",   'W', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "36", 'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    CheckGet(obj, "54",  'V', 'O', ios_defs::strfailbit);
    CheckGet(obj, "36",  'V', 'O', ios_defs::eofbit);
    CheckGet(obj, "%EV", 'V', 'E', ios_defs::eofbit);
    CheckGet(obj, "V",   'V', 'E', ios_defs::strfailbit);
    CheckGet(obj, "V",   'V', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "3", 'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, "3", 'w', 'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, "%Ew", 'w', 'E', ios_defs::eofbit);
    CheckGet(obj, "w",   'w', 'E', ios_defs::strfailbit);
    CheckGet(obj, "w",   'w', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "24", 'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, "24", 'y', 'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, "24", 'y', 'O', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, "y",  'y', 'E', ios_defs::strfailbit);
    CheckGet(obj, "y",  'y', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "2024", 'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, "2024", 'Y', 'E', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, "Y",   'Y', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OY", 'Y', 'O', ios_defs::eofbit);
    CheckGet(obj, "Y",   'Y', 'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(CheckGet(obj, "America/Los_Angeles", 'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, "PST", 'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, "America/Los_Angexes", 'Z', 0, ios_defs::strfailbit);
    CheckGet(obj, "%EZ", 'Z', 'E', ios_defs::eofbit);
    CheckGet(obj, "Z",   'Z', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OZ", 'Z', 'O', ios_defs::eofbit);
    CheckGet(obj, "Z",   'Z', 'O', ios_defs::strfailbit);

    CheckGet(obj, "Z", 'z', 0, ios_defs::eofbit);
    CheckGet(obj, "+13", 'z', 0, ios_defs::eofbit);
    CheckGet(obj, "-1110", 'z', 0, ios_defs::eofbit);
    CheckGet(obj, "+11:10", 'z', 0, ios_defs::eofbit);
    CheckGet(obj, "%Ez", 'z', 'E', ios_defs::eofbit);
    CheckGet(obj, "z",  'z', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oz", 'z', 'O', ios_defs::eofbit);
    CheckGet(obj, "z",  'z', 'O', ios_defs::strfailbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(CheckGet<year_month_day>(obj, "1999-W52-6", "%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2019-W01-1", "%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "1999-W52-5", "%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "99-W52-6", "%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "19-W01-1", "%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "99-W52-5", "%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "20 24/09/04", "%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((CheckGet<year_month_day>(obj, "20 01 01", "%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar, JapaneseReadsEveryConversionSpecifier)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<char>>("ja_JP.UTF-8"));

    CheckGet(obj, "%",  '%',  0,  ios_defs::eofbit);
    CheckGet(obj, "x",  '%',  0,  ios_defs::strfailbit);
    CheckGet(obj, "%",  '%', 'E', febit);
    CheckGet(obj, "%E%", '%', 'E', ios_defs::eofbit);
    CheckGet(obj, "%",  '%', 'O', febit);
    CheckGet(obj, "%O%", '%', 'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet(obj, "水", 'a', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, "%Ea", 'a', 'E', ios_defs::eofbit);
    CheckGet(obj, "a",   'a', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oa", 'a', 'O', ios_defs::eofbit);
    CheckGet(obj, "a",   'a', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "水曜日", 'A', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, "%EA", 'A', 'E', ios_defs::eofbit);
    CheckGet(obj, "A",   'A', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OA", 'A', 'O', ios_defs::eofbit);
    CheckGet(obj, "A",   'A', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "9月", 'b', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, "%Eb", 'b', 'E', ios_defs::eofbit);
    CheckGet(obj, "b",   'b', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Ob", 'b', 'O', ios_defs::eofbit);
    CheckGet(obj, "b",   'b', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "9月", 'B', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, "%EB", 'B', 'E', ios_defs::eofbit);
    CheckGet(obj, "B",   'B', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OB", 'B', 'O', ios_defs::eofbit);
    CheckGet(obj, "B",   'B', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "9月", 'h', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, "%Eh", 'h', 'E', ios_defs::eofbit);
    CheckGet(obj, "h",   'h', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oh", 'h', 'O', ios_defs::eofbit);
    CheckGet(obj, "h",   'h', 'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024年09月04日 13時33分18秒", 'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "令和6年09月04日 13時33分18秒", 'c', 'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "202409月04日 13時33分18秒", 'c', 'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, "c",   'c', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oc", 'c', 'O', ios_defs::eofbit);
    CheckGet(obj, "c",   'c', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "20", 'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "平成", 'C', 'E', ios_defs::eofbit).year(), std::chrono::year(1990));
    CheckGet(obj, "C",   'C', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OC", 'C', 'O', ios_defs::eofbit);
    CheckGet(obj, "C",   'C', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "04", 'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, "04", 'd', 'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, "四", 'd', 'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, "%Ed", 'd', 'E', ios_defs::eofbit);
    CheckGet(obj, "d",   'd', 'E', ios_defs::strfailbit);
    CheckGet(obj, "d",   'd', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "4", 'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, "4", 'e', 'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, "四", 'e', 'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, "%Ee", 'e', 'E', ios_defs::eofbit);
    CheckGet(obj, "e",   'e', 'E', ios_defs::strfailbit);
    CheckGet(obj, "e",   'e', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024-09-04", 'F', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, "%EF", 'F', 'E', ios_defs::eofbit);
    CheckGet(obj, "F",   'F', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OF", 'F', 'O', ios_defs::eofbit);
    CheckGet(obj, "F",   'F', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024年09月04日", 'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "令和6年09月04日", 'x', 'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "202409月04日", 'x', 'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, "x",   'x', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Ox", 'x', 'O', ios_defs::eofbit);
    CheckGet(obj, "x",   'x', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "09/04/24", 'D', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, "%ED", 'D', 'E', ios_defs::eofbit);
    CheckGet(obj, "D",   'D', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OD", 'D', 'O', ios_defs::eofbit);
    CheckGet(obj, "D",   'D', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "13", 'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, "13", 'H', 'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, "十三", 'H', 'O', ios_defs::eofbit).m_hour, 13);
    CheckGet(obj, "%EH", 'H', 'E', ios_defs::eofbit);
    CheckGet(obj, "H",   'H', 'E', ios_defs::strfailbit);
    CheckGet(obj, "H",   'H', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "01", 'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, "01", 'I', 'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, "一", 'I', 'O', ios_defs::eofbit).m_hour, 1);
    CheckGet(obj, "%EI", 'I', 'E', ios_defs::eofbit);
    CheckGet(obj, "I",   'I', 'E', ios_defs::strfailbit);
    CheckGet(obj, "I",   'I', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "248", 'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 248", "%Y %j", ios_defs::eofbit), check_date1);
    CheckGet(obj, "%Ej", 'j', 'E', ios_defs::eofbit);
    CheckGet(obj, "j",   'j', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oj", 'j', 'O', ios_defs::eofbit);
    CheckGet(obj, "j",   'j', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "09", 'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, "09", 'm', 'O', ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, "九", 'm', 'O', ios_defs::eofbit).m_month, 9);
    CheckGet(obj, "%Em", 'm', 'E', ios_defs::eofbit);
    CheckGet(obj, "m",   'm', 'E', ios_defs::strfailbit);
    CheckGet(obj, "m",   'm', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "33", 'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, "33", 'M', 'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, "三十三", 'M', 'O', ios_defs::eofbit).m_minute, 33);
    CheckGet(obj, "%EM", 'M', 'E', ios_defs::eofbit);
    CheckGet(obj, "M",   'M', 'E', ios_defs::strfailbit);
    CheckGet(obj, "M",   'M', 'O', ios_defs::strfailbit);

    CheckGet(obj, "\n",   'n',  0,  ios_defs::eofbit);
    CheckGet(obj, "x",    'n',  0,  ios_defs::goodbit);
    CheckGet(obj, "\n",   'n', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%En",  'n', 'E', ios_defs::eofbit);
    CheckGet(obj, "n",    'n', 'O', ios_defs::strfailbit);
    CheckGet(obj, "%On",  'n', 'O', ios_defs::eofbit);

    CheckGet(obj, "\t",   't',  0,  ios_defs::eofbit);
    CheckGet(obj, "x",    't',  0,  ios_defs::goodbit);
    CheckGet(obj, "\t",   't', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Et",  't', 'E', ios_defs::eofbit);
    CheckGet(obj, "n",    't', 'O', ios_defs::strfailbit);
    CheckGet(obj, "%Ot",  't', 'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 午後", "%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 午前", "%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(CheckGet(obj, "午後", 'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(CheckGet(obj, "午前", 'p', 0, ios_defs::eofbit).m_is_pm, false);
    CheckGet(obj, "%Ep", 'p', 'E', ios_defs::eofbit);
    CheckGet(obj, "p",   'p', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Op", 'p', 'O', ios_defs::eofbit);
    CheckGet(obj, "p",   'p', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "午後01時33分18秒", "%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "%Er", 'r', 'E', ios_defs::eofbit);
    CheckGet(obj, "r",   'r', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Or", 'r', 'O', ios_defs::eofbit);
    CheckGet(obj, "r",   'r', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33", "%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, "%ER", 'R', 'E', ios_defs::eofbit);
    CheckGet(obj, "R",   'R', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OR", 'R', 'O', ios_defs::eofbit);
    CheckGet(obj, "R",   'R', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "18", 'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, "18", 'S', 'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, "十八", 'S', 'O', ios_defs::eofbit).m_second, 18);
    CheckGet(obj, "%ES", 'S', 'E', ios_defs::eofbit);
    CheckGet(obj, "S",   'S', 'E', ios_defs::strfailbit);
    CheckGet(obj, "S",   'S', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13時33分18秒", "%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13時33分18秒", "%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "X",   'X', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OX", 'X', 'O', ios_defs::eofbit);
    CheckGet(obj, "X",   'X', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18", "%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "%ET", 'T', 'E', ios_defs::eofbit);
    CheckGet(obj, "T",   'T', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OT", 'T', 'O', ios_defs::eofbit);
    CheckGet(obj, "T",   'T', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "3", 'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, "3", 'u', 'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, "三", 'u', 'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, "%Eu", 'u', 'E', ios_defs::eofbit);
    CheckGet(obj, "u",   'u', 'E', ios_defs::strfailbit);
    CheckGet(obj, "u",   'u', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "24", 'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, "%Eg", 'g', 'E', ios_defs::eofbit);
    CheckGet(obj, "g",   'g', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Og", 'g', 'O', ios_defs::eofbit);
    CheckGet(obj, "g",   'g', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "2024", 'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, "%EG", 'G', 'E', ios_defs::eofbit);
    CheckGet(obj, "G",   'G', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OG", 'G', 'O', ios_defs::eofbit);
    CheckGet(obj, "G",   'G', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 35 水", "%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 35 水", "%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 三十五 水", "%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, "35", 'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(CheckGet(obj, "35", 'U', 'O', ios_defs::eofbit).m_week_no, 35);
    CheckGet(obj, "%EU", 'U', 'E', ios_defs::eofbit);
    CheckGet(obj, "U",   'U', 'E', ios_defs::strfailbit);
    CheckGet(obj, "U",   'U', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 36 水", "%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 36 水", "%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2024 三十六 水", "%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, "36", 'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(CheckGet(obj, "36", 'W', 'O', ios_defs::eofbit).m_week_no, 36);
    CheckGet(obj, "%EW", 'W', 'E', ios_defs::eofbit);
    CheckGet(obj, "W",   'W', 'E', ios_defs::strfailbit);
    CheckGet(obj, "W",   'W', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "36", 'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(CheckGet(obj, "36", 'V', 'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(CheckGet(obj, "三十六", 'V', 'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    CheckGet(obj, "54",  'V', 'O', ios_defs::strfailbit);
    CheckGet(obj, "%EV", 'V', 'E', ios_defs::eofbit);
    CheckGet(obj, "V",   'V', 'E', ios_defs::strfailbit);
    CheckGet(obj, "V",   'V', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "3", 'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, "3", 'w', 'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, "三", 'w', 'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, "%Ew", 'w', 'E', ios_defs::eofbit);
    CheckGet(obj, "w",   'w', 'E', ios_defs::strfailbit);
    CheckGet(obj, "w",   'w', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "24", 'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "6", 'y', 'E', ios_defs::eofbit).year(), std::chrono::year(2024));
    EXPECT_EQ(CheckGet(obj, "24", 'y', 'O', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, "二十四", 'y', 'O', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, "y",  'y', 'E', ios_defs::strfailbit);
    CheckGet(obj, "y",  'y', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, "2024", 'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, "2024", 'Y', 'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "平成3年", 'Y', 'E', ios_defs::eofbit).year(), std::chrono::year(1991));
    CheckGet(obj, "Y",   'Y', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OY", 'Y', 'O', ios_defs::eofbit);
    CheckGet(obj, "Y",   'Y', 'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(CheckGet(obj, "America/Los_Angeles", 'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, "PST", 'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, "America/Los_Angexes", 'Z', 0, ios_defs::strfailbit);
    CheckGet(obj, "%EZ", 'Z', 'E', ios_defs::eofbit);
    CheckGet(obj, "Z",   'Z', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%OZ", 'Z', 'O', ios_defs::eofbit);
    CheckGet(obj, "Z",   'Z', 'O', ios_defs::strfailbit);

    CheckGet(obj, "Z", 'z', 0, ios_defs::eofbit);
    CheckGet(obj, "+13", 'z', 0, ios_defs::eofbit);
    CheckGet(obj, "-1110", 'z', 0, ios_defs::eofbit);
    CheckGet(obj, "+11:10", 'z', 0, ios_defs::eofbit);
    CheckGet(obj, "%Ez", 'z', 'E', ios_defs::eofbit);
    CheckGet(obj, "z",  'z', 'E', ios_defs::strfailbit);
    CheckGet(obj, "%Oz", 'z', 'O', ios_defs::eofbit);
    CheckGet(obj, "z",  'z', 'O', ios_defs::strfailbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(CheckGet<year_month_day>(obj, "1999-W52-6", "%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "2019-W01-1", "%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "1999-W52-5", "%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "99-W52-6", "%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "19-W01-1", "%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, "99-W52-5", "%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, "20 24/09/04", "%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((CheckGet<year_month_day>(obj, "20 01 01", "%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar, AWeekdayOrMonthNameIsMatchedAgainstBothSpellings)
{
    timeio obj(std::make_shared<timeio_conf<char>>("C"));
    {
        std::string input = "Mon";
        std::string format = "%a";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 1);
    }

    {
        std::string input = "Tue ";
        std::string format = "%a";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != ' '));
        EXPECT_EQ(time.tm_wday, 2);
    }

    {
        std::string input = "Wednesday";
        std::string format = "%a";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 3);
    }

    {
        std::string input = "Thu";
        std::string format = "%A";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 4);
    }

    {
        std::string input = "Fri ";
        std::string format = "%A";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != ' '));
        EXPECT_EQ(time.tm_wday, 5);
    }

    {
        std::string input = "Saturday";
        std::string format = "%A";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 6);
    }

    {
        std::string input = "Feb";
        std::string format = "%b";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 1);
    }

    {
        std::string input = "Mar ";
        std::string format = "%b";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != ' '));
        EXPECT_EQ(time.tm_mon, 2);
    }

    {
        std::string input = "April";
        std::string format = "%b";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 3);
    }

    {
        std::string input = "May";
        std::string format = "%B";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 4);
    }

    {
        std::string input = "Jun ";
        std::string format = "%B";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != ' '));
        EXPECT_EQ(time.tm_mon, 5);
    }

    {
        std::string input = "July";
        std::string format = "%B";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 6);
    }

    {
        std::string input = "Aug";
        std::string format = "%h";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 7);
    }

    {
        std::string input = "May ";
        std::string format = "%h";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != ' '));
        EXPECT_EQ(time.tm_mon, 4);
    }

    {
        std::string input = "October";
        std::string format = "%h";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 9);
    }

    // Other tests.
    {
        std::string input = "2.";
        std::string format = "%d.";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mday, 2);
    }

    {
        std::string input = "0.";
        std::string format = "%d.";

        time_parse_context<char> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::string input = "32.";
        std::string format = "%d.";

        time_parse_context<char> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::string input = "5.";
        std::string format = "%e.";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mday, 5);
    }

    {
        std::string input = "06.";
        std::string format = "%e.";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mday, 6);
    }

    {
        std::string input = "0";
        std::string format = "%e";

        time_parse_context<char> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::string input = "35";
        std::string format = "%e";

        time_parse_context<char> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    struct clock_case { const char* input; int hour; int minute; };
    for (const clock_case tc : {
             clock_case{"12:11AM", 0, 11},
             clock_case{"03:14AM", 3, 14},
             clock_case{"09:27AM", 9, 27},
             clock_case{"12:29PM", 12, 29},
             clock_case{"02:38PM", 14, 38},
             clock_case{"09:52PM", 21, 52},
         })
    {
        SCOPED_TRACE(tc.input);
        std::string input(tc.input);
        time_parse_context<char> ctx;
        const auto ret = obj.get(input.begin(), input.end(), ctx, std::string_view{"%I:%M%p"});
        const auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_hour, tc.hour);
        EXPECT_EQ(time.tm_min, tc.minute);
    }

    {
        std::string input = "08%46";
        std::string format = "%H%%%S";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_hour, 8);
        EXPECT_EQ(time.tm_sec, 46);
    }

    {
        std::string input = "29:14";
        std::string format = "%H:%M";

        time_parse_context<char> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::string input = "Oct+tail";
        std::string format = "%b+tail";

        time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mon, 9);
    }
}

TEST(TimeioChar, AWeekdayOrMonthNameIsMatchedTheSameWayFromAStream)
{
    timeio obj(std::make_shared<timeio_conf<char>>("C"));
    using namespace IOv2;
    {
        streambuf sb(mem_device{"Mon"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%a";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, std::default_sentinel);
        EXPECT_EQ(time.tm_wday, 1);
    }

    {
        streambuf sb(mem_device{"Tue "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%a";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == std::default_sentinel) || (*ret != ' '));
        EXPECT_EQ(time.tm_wday, 2);
    }

    {
        streambuf sb(mem_device{"Wednesday"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%a";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, std::default_sentinel);
        EXPECT_EQ(time.tm_wday, 3);
    }

    {
        streambuf sb(mem_device{"Thu"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, std::default_sentinel);
        EXPECT_EQ(time.tm_wday, 4);
    }

    {
        streambuf sb(mem_device{"Fri "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == std::default_sentinel) || (*ret != ' '));
        EXPECT_EQ(time.tm_wday, 5);
    }

    {
        streambuf sb(mem_device{"Saturday"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, std::default_sentinel);
        EXPECT_EQ(time.tm_wday, 6);
    }

    {
        streambuf sb(mem_device{"Feb"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, std::default_sentinel);
        EXPECT_EQ(time.tm_mon, 1);
    }

    {
        streambuf sb(mem_device{"Mar "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == std::default_sentinel) || (*ret != ' '));
        EXPECT_EQ(time.tm_mon, 2);
    }

    {
        streambuf sb(mem_device{"April"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, std::default_sentinel);
        EXPECT_EQ(time.tm_mon, 3);
    }

    {
        streambuf sb(mem_device{"May"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%B";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, std::default_sentinel);
        EXPECT_EQ(time.tm_mon, 4);
    }

    {
        streambuf sb(mem_device{"Jun "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%B";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == std::default_sentinel) || (*ret != ' '));
        EXPECT_EQ(time.tm_mon, 5);
    }

    {
        streambuf sb(mem_device{"July"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%B";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, std::default_sentinel);
        EXPECT_EQ(time.tm_mon, 6);
    }

    {
        streambuf sb(mem_device{"Aug"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%h";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, std::default_sentinel);
        EXPECT_EQ(time.tm_mon, 7);
    }

    {
        streambuf sb(mem_device{"May "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%h";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == std::default_sentinel) || (*ret != ' '));
        EXPECT_EQ(time.tm_mon, 4);
    }

    {
        streambuf sb(mem_device{"October"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%h";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, std::default_sentinel);
        EXPECT_EQ(time.tm_mon, 9);
    }

    // Other tests.
    {
        streambuf sb(mem_device{"2."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d.";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, std::default_sentinel);
        EXPECT_EQ(time.tm_mday, 2);
    }

    {
        streambuf sb(mem_device{"0."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d.";

        time_parse_context<char> ctx;
        EXPECT_THROW(obj.get(beg, std::default_sentinel, ctx, format), stream_error);
    }

    {
        streambuf sb(mem_device{"32."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d.";

        time_parse_context<char> ctx;
        EXPECT_THROW(obj.get(beg, std::default_sentinel, ctx, format), stream_error);
    }

    {
        streambuf sb(mem_device{"5."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%e.";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        EXPECT_EQ(ret, std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mday, 5);
    }

    {
        streambuf sb(mem_device{"06."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%e.";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        EXPECT_EQ(ret, std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mday, 6);
    }

    {
        streambuf sb(mem_device{"0"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%e";

        time_parse_context<char> ctx;
        EXPECT_THROW(obj.get(beg, std::default_sentinel, ctx, format), stream_error);
    }

    {
        streambuf sb(mem_device{"35"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%e";

        time_parse_context<char> ctx;
        EXPECT_THROW(obj.get(beg, std::default_sentinel, ctx, format), stream_error);
    }

    struct clock_case { const char* input; int hour; int minute; };
    for (const clock_case tc : {
             clock_case{"12:11AM", 0, 11},
             clock_case{"03:14AM", 3, 14},
             clock_case{"09:27AM", 9, 27},
             clock_case{"12:29PM", 12, 29},
             clock_case{"02:38PM", 14, 38},
             clock_case{"09:52PM", 21, 52},
         })
    {
        SCOPED_TRACE(tc.input);
        streambuf sb(mem_device{tc.input});
        auto beg = istreambuf_iterator(sb);
        time_parse_context<char> ctx;
        const auto ret = obj.get(beg, std::default_sentinel, ctx,
                                 std::string_view{"%I:%M%p"});
        const auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, std::default_sentinel);
        EXPECT_EQ(time.tm_hour, tc.hour);
        EXPECT_EQ(time.tm_min, tc.minute);
    }

    {
        streambuf sb(mem_device{"08%46"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%H%%%S";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        EXPECT_EQ(ret, std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_hour, 8);
        EXPECT_EQ(time.tm_sec, 46);
    }

    {
        streambuf sb(mem_device{"29:14"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%H:%M";

        time_parse_context<char> ctx;
        auto it = beg;
        EXPECT_THROW(it = obj.get(beg, std::default_sentinel, ctx, format), stream_error);
        EXPECT_FALSE((it == std::default_sentinel) || (*it != '9'));
    }

    {
        streambuf sb(mem_device{"Oct+tail"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b+tail";

        time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        EXPECT_EQ(ret, std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mon, 9);
    }
}

TEST(TimeioChar, JapaneseReadsEveryConversionSpecifierIntoADate)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<char>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<char, true, true, tz_level::none>, true, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri("%",  '%',  0,  ios_defs::eofbit);
    FOri("x",  '%',  0,  ios_defs::strfailbit);
    FOri("%",  '%', 'E', febit);
    FOri("%E%", '%', 'E', ios_defs::eofbit);
    FOri("%",  '%', 'O', febit);
    FOri("%O%", '%', 'O', ios_defs::eofbit);

    EXPECT_EQ(FOri("水", 'a', 0, ios_defs::eofbit).m_wday, 3);
    FOri("%Ea", 'a', 'E', ios_defs::eofbit);
    FOri("a",   'a', 'E', ios_defs::strfailbit);
    FOri("%Oa", 'a', 'O', ios_defs::eofbit);
    FOri("a",   'a', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("水曜日", 'A', 0, ios_defs::eofbit).m_wday, 3);
    FOri("%EA", 'A', 'E', ios_defs::eofbit);
    FOri("A",   'A', 'E', ios_defs::strfailbit);
    FOri("%OA", 'A', 'O', ios_defs::eofbit);
    FOri("A",   'A', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("9月", 'b', 0, ios_defs::eofbit).m_month, 9);
    FOri("%Eb", 'b', 'E', ios_defs::eofbit);
    FOri("b",   'b', 'E', ios_defs::strfailbit);
    FOri("%Ob", 'b', 'O', ios_defs::eofbit);
    FOri("b",   'b', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("9月", 'B', 0, ios_defs::eofbit).m_month, 9);
    FOri("%EB", 'B', 'E', ios_defs::eofbit);
    FOri("B",   'B', 'E', ios_defs::strfailbit);
    FOri("%OB", 'B', 'O', ios_defs::eofbit);
    FOri("B",   'B', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("9月", 'h', 0, ios_defs::eofbit).m_month, 9);
    FOri("%Eh", 'h', 'E', ios_defs::eofbit);
    FOri("h",   'h', 'E', ios_defs::strfailbit);
    FOri("%Oh", 'h', 'O', ios_defs::eofbit);
    FOri("h",   'h', 'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(FYmd("2024年09月04日 13時33分18秒", 'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("令和6年09月04日 13時33分18秒", 'c', 'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("202409月04日 13時33分18秒", 'c', 'E', ios_defs::eofbit), check_date1);
    FOri("c",   'c', 'E', ios_defs::strfailbit);
    FOri("%Oc", 'c', 'O', ios_defs::eofbit);
    FOri("c",   'c', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("20", 'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(FYmd("平成", 'C', 'E', ios_defs::eofbit).year(), std::chrono::year(1990));
    FOri("C",   'C', 'E', ios_defs::strfailbit);
    FOri("%OC", 'C', 'O', ios_defs::eofbit);
    FOri("C",   'C', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("04", 'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri("04", 'd', 'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri("四", 'd', 'O', ios_defs::eofbit).m_mday, 4);
    FOri("%Ed", 'd', 'E', ios_defs::eofbit);
    FOri("d",   'd', 'E', ios_defs::strfailbit);
    FOri("d",   'd', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("4", 'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri("4", 'e', 'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri("四", 'e', 'O', ios_defs::eofbit).m_mday, 4);
    FOri("%Ee", 'e', 'E', ios_defs::eofbit);
    FOri("e",   'e', 'E', ios_defs::strfailbit);
    FOri("e",   'e', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd("2024-09-04", 'F', 0, ios_defs::eofbit), check_date1);
    FOri("%EF", 'F', 'E', ios_defs::eofbit);
    FOri("F",   'F', 'E', ios_defs::strfailbit);
    FOri("%OF", 'F', 'O', ios_defs::eofbit);
    FOri("F",   'F', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd("2024年09月04日", 'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("令和6年09月04日", 'x', 'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("202409月04日", 'x', 'E', ios_defs::eofbit), check_date1);
    FOri("x",   'x', 'E', ios_defs::strfailbit);
    FOri("%Ox", 'x', 'O', ios_defs::eofbit);
    FOri("x",   'x', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd("09/04/24", 'D', 0, ios_defs::eofbit), check_date1);
    FOri("%ED", 'D', 'E', ios_defs::eofbit);
    FOri("D",   'D', 'E', ios_defs::strfailbit);
    FOri("%OD", 'D', 'O', ios_defs::eofbit);
    FOri("D",   'D', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("13", 'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri("13", 'H', 'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri("十三", 'H', 'O', ios_defs::eofbit).m_hour, 13);
    FOri("%EH", 'H', 'E', ios_defs::eofbit);
    FOri("H",   'H', 'E', ios_defs::strfailbit);
    FOri("H",   'H', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("01", 'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri("01", 'I', 'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri("一", 'I', 'O', ios_defs::eofbit).m_hour, 1);
    FOri("%EI", 'I', 'E', ios_defs::eofbit);
    FOri("I",   'I', 'E', ios_defs::strfailbit);
    FOri("I",   'I', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("248", 'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(FYmd("2024 248", "%Y %j", ios_defs::eofbit), check_date1);
    FOri("%Ej", 'j', 'E', ios_defs::eofbit);
    FOri("j",   'j', 'E', ios_defs::strfailbit);
    FOri("%Oj", 'j', 'O', ios_defs::eofbit);
    FOri("j",   'j', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("09", 'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri("09", 'm', 'O', ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri("九", 'm', 'O', ios_defs::eofbit).m_month, 9);
    FOri("%Em", 'm', 'E', ios_defs::eofbit);
    FOri("m",   'm', 'E', ios_defs::strfailbit);
    FOri("m",   'm', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("33", 'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri("33", 'M', 'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri("三十三", 'M', 'O', ios_defs::eofbit).m_minute, 33);
    FOri("%EM", 'M', 'E', ios_defs::eofbit);
    FOri("M",   'M', 'E', ios_defs::strfailbit);
    FOri("M",   'M', 'O', ios_defs::strfailbit);

    FOri("\n",   'n',  0,  ios_defs::eofbit);
    FOri("x",    'n',  0,  ios_defs::goodbit);
    FOri("\n",   'n', 'E', ios_defs::strfailbit);
    FOri("%En",  'n', 'E', ios_defs::eofbit);
    FOri("n",    'n', 'O', ios_defs::strfailbit);
    FOri("%On",  'n', 'O', ios_defs::eofbit);

    FOri("\t",   't',  0,  ios_defs::eofbit);
    FOri("x",    't',  0,  ios_defs::goodbit);
    FOri("\t",   't', 'E', ios_defs::strfailbit);
    FOri("%Et",  't', 'E', ios_defs::eofbit);
    FOri("n",    't', 'O', ios_defs::strfailbit);
    FOri("%Ot",  't', 'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 午後", "%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 午前", "%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(FOri("午後", 'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(FOri("午前", 'p', 0, ios_defs::eofbit).m_is_pm, false);
    FOri("%Ep", 'p', 'E', ios_defs::eofbit);
    FOri("p",   'p', 'E', ios_defs::strfailbit);
    FOri("%Op", 'p', 'O', ios_defs::eofbit);
    FOri("p",   'p', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "午後01時33分18秒", "%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("%Er", 'r', 'E', ios_defs::eofbit);
    FOri("r",   'r', 'E', ios_defs::strfailbit);
    FOri("%Or", 'r', 'O', ios_defs::eofbit);
    FOri("r",   'r', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33", "%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri("%ER", 'R', 'E', ios_defs::eofbit);
    FOri("R",   'R', 'E', ios_defs::strfailbit);
    FOri("%OR", 'R', 'O', ios_defs::eofbit);
    FOri("R",   'R', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("18", 'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri("18", 'S', 'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri("十八", 'S', 'O', ios_defs::eofbit).m_second, 18);
    FOri("%ES", 'S', 'E', ios_defs::eofbit);
    FOri("S",   'S', 'E', ios_defs::strfailbit);
    FOri("S",   'S', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13時33分18秒", "%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13時33分18秒", "%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("X",   'X', 'E', ios_defs::strfailbit);
    FOri("%OX", 'X', 'O', ios_defs::eofbit);
    FOri("X",   'X', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18", "%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("%ET", 'T', 'E', ios_defs::eofbit);
    FOri("T",   'T', 'E', ios_defs::strfailbit);
    FOri("%OT", 'T', 'O', ios_defs::eofbit);
    FOri("T",   'T', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("3", 'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri("3", 'u', 'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri("三", 'u', 'O', ios_defs::eofbit).m_wday, 3);
    FOri("%Eu", 'u', 'E', ios_defs::eofbit);
    FOri("u",   'u', 'E', ios_defs::strfailbit);
    FOri("u",   'u', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("24", 'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri("%Eg", 'g', 'E', ios_defs::eofbit);
    FOri("g",   'g', 'E', ios_defs::strfailbit);
    FOri("%Og", 'g', 'O', ios_defs::eofbit);
    FOri("g",   'g', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("2024", 'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri("%EG", 'G', 'E', ios_defs::eofbit);
    FOri("G",   'G', 'E', ios_defs::strfailbit);
    FOri("%OG", 'G', 'O', ios_defs::eofbit);
    FOri("G",   'G', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd("2024 35 水", "%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("2024 35 水", "%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("2024 三十五 水", "%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri("35", 'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(FOri("35", 'U', 'O', ios_defs::eofbit).m_week_no, 35);
    FOri("%EU", 'U', 'E', ios_defs::eofbit);
    FOri("U",   'U', 'E', ios_defs::strfailbit);
    FOri("U",   'U', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd("2024 36 水", "%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("2024 36 水", "%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("2024 三十六 水", "%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri("36", 'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(FOri("36", 'W', 'O', ios_defs::eofbit).m_week_no, 36);
    FOri("%EW", 'W', 'E', ios_defs::eofbit);
    FOri("W",   'W', 'E', ios_defs::strfailbit);
    FOri("W",   'W', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("36", 'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri("36", 'V', 'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri("三十六", 'V', 'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    FOri("54",  'V', 'O', ios_defs::strfailbit);
    FOri("%EV", 'V', 'E', ios_defs::eofbit);
    FOri("V",   'V', 'E', ios_defs::strfailbit);
    FOri("V",   'V', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("3", 'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri("3", 'w', 'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri("三", 'w', 'O', ios_defs::eofbit).m_wday, 3);
    FOri("%Ew", 'w', 'E', ios_defs::eofbit);
    FOri("w",   'w', 'E', ios_defs::strfailbit);
    FOri("w",   'w', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("24", 'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd("6", 'y', 'E', ios_defs::eofbit).year(), std::chrono::year(2024));
    EXPECT_EQ(FOri("24", 'y', 'O', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri("二十四", 'y', 'O', ios_defs::eofbit).m_year, 2024);
    FOri("y",  'y', 'E', ios_defs::strfailbit);
    FOri("y",  'y', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("2024", 'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri("2024", 'Y', 'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd("平成3年", 'Y', 'E', ios_defs::eofbit).year(), std::chrono::year(1991));
    FOri("Y",   'Y', 'E', ios_defs::strfailbit);
    FOri("%OY", 'Y', 'O', ios_defs::eofbit);
    FOri("Y",   'Y', 'O', ios_defs::strfailbit);

    FOri("%Z", 'Z', 0, ios_defs::eofbit);
    FOri("%EZ", 'Z', 'E', ios_defs::eofbit);
    FOri("Z",   'Z', 'E', ios_defs::strfailbit);
    FOri("%OZ", 'Z', 'O', ios_defs::eofbit);
    FOri("Z",   'Z', 'O', ios_defs::strfailbit);

    FOri("%z", 'z', 0, ios_defs::eofbit);
    FOri("%Ez", 'z', 'E', ios_defs::eofbit);
    FOri("%Oz", 'z', 'O', ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(FYmd("1999-W52-6", "%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd("2019-W01-1", "%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd("1999-W52-5", "%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd("99-W52-6", "%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd("19-W01-1", "%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd("99-W52-5", "%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd("20 24/09/04", "%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((FYmd("20 01 01", "%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar, JapaneseReadsEveryConversionSpecifierIntoADateWithoutAZone)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<char>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<char, true, false, tz_level::none>, true, false, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, false, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri("%",  '%',  0,  ios_defs::eofbit);
    FOri("x",  '%',  0,  ios_defs::strfailbit);
    FOri("%",  '%', 'E', febit);
    FOri("%E%", '%', 'E', ios_defs::eofbit);
    FOri("%",  '%', 'O', febit);
    FOri("%O%", '%', 'O', ios_defs::eofbit);

    EXPECT_EQ(FOri("水", 'a', 0, ios_defs::eofbit).m_wday, 3);
    FOri("%Ea", 'a', 'E', ios_defs::eofbit);
    FOri("a",   'a', 'E', ios_defs::strfailbit);
    FOri("%Oa", 'a', 'O', ios_defs::eofbit);
    FOri("a",   'a', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("水曜日", 'A', 0, ios_defs::eofbit).m_wday, 3);
    FOri("%EA", 'A', 'E', ios_defs::eofbit);
    FOri("A",   'A', 'E', ios_defs::strfailbit);
    FOri("%OA", 'A', 'O', ios_defs::eofbit);
    FOri("A",   'A', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("9月", 'b', 0, ios_defs::eofbit).m_month, 9);
    FOri("%Eb", 'b', 'E', ios_defs::eofbit);
    FOri("b",   'b', 'E', ios_defs::strfailbit);
    FOri("%Ob", 'b', 'O', ios_defs::eofbit);
    FOri("b",   'b', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("9月", 'B', 0, ios_defs::eofbit).m_month, 9);
    FOri("%EB", 'B', 'E', ios_defs::eofbit);
    FOri("B",   'B', 'E', ios_defs::strfailbit);
    FOri("%OB", 'B', 'O', ios_defs::eofbit);
    FOri("B",   'B', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("9月", 'h', 0, ios_defs::eofbit).m_month, 9);
    FOri("%Eh", 'h', 'E', ios_defs::eofbit);
    FOri("h",   'h', 'E', ios_defs::strfailbit);
    FOri("%Oh", 'h', 'O', ios_defs::eofbit);
    FOri("h",   'h', 'O', ios_defs::strfailbit);

    using namespace std::chrono;
    FYmd("%c", 'c', 0, ios_defs::eofbit);
    FYmd("%Ec", 'c', 'E', ios_defs::eofbit);
    FOri("c",   'c', 'E', ios_defs::strfailbit);
    FOri("%Oc", 'c', 'O', ios_defs::eofbit);
    FOri("c",   'c', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("20", 'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(FYmd("平成", 'C', 'E', ios_defs::eofbit).year(), std::chrono::year(1990));
    FOri("C",   'C', 'E', ios_defs::strfailbit);
    FOri("%OC", 'C', 'O', ios_defs::eofbit);
    FOri("C",   'C', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("04", 'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri("04", 'd', 'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri("四", 'd', 'O', ios_defs::eofbit).m_mday, 4);
    FOri("%Ed", 'd', 'E', ios_defs::eofbit);
    FOri("d",   'd', 'E', ios_defs::strfailbit);
    FOri("d",   'd', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("4", 'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri("4", 'e', 'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri("四", 'e', 'O', ios_defs::eofbit).m_mday, 4);
    FOri("%Ee", 'e', 'E', ios_defs::eofbit);
    FOri("e",   'e', 'E', ios_defs::strfailbit);
    FOri("e",   'e', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd("2024-09-04", 'F', 0, ios_defs::eofbit), check_date1);
    FOri("%EF", 'F', 'E', ios_defs::eofbit);
    FOri("F",   'F', 'E', ios_defs::strfailbit);
    FOri("%OF", 'F', 'O', ios_defs::eofbit);
    FOri("F",   'F', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd("2024年09月04日", 'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("令和6年09月04日", 'x', 'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("202409月04日", 'x', 'E', ios_defs::eofbit), check_date1);
    FOri("x",   'x', 'E', ios_defs::strfailbit);
    FOri("%Ox", 'x', 'O', ios_defs::eofbit);
    FOri("x",   'x', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd("09/04/24", 'D', 0, ios_defs::eofbit), check_date1);
    FOri("%ED", 'D', 'E', ios_defs::eofbit);
    FOri("D",   'D', 'E', ios_defs::strfailbit);
    FOri("%OD", 'D', 'O', ios_defs::eofbit);
    FOri("D",   'D', 'O', ios_defs::strfailbit);

    FOri("%H", 'H', 0,   ios_defs::eofbit);
    FOri("%EH", 'H', 'E', ios_defs::eofbit);
    FOri("H",   'H', 'E', ios_defs::strfailbit);
    FOri("H",   'H', 'O', ios_defs::strfailbit);

    FOri("%I", 'I', 0,   ios_defs::eofbit);
    FOri("%EI", 'I', 'E', ios_defs::eofbit);
    FOri("I",   'I', 'E', ios_defs::strfailbit);
    FOri("I",   'I', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("248", 'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(FYmd("2024 248", "%Y %j", ios_defs::eofbit), check_date1);
    FOri("%Ej", 'j', 'E', ios_defs::eofbit);
    FOri("j",   'j', 'E', ios_defs::strfailbit);
    FOri("%Oj", 'j', 'O', ios_defs::eofbit);
    FOri("j",   'j', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("09", 'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri("09", 'm', 'O', ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri("九", 'm', 'O', ios_defs::eofbit).m_month, 9);
    FOri("%Em", 'm', 'E', ios_defs::eofbit);
    FOri("m",   'm', 'E', ios_defs::strfailbit);
    FOri("m",   'm', 'O', ios_defs::strfailbit);

    FOri("%M", 'M', 0,   ios_defs::eofbit);
    FOri("%OM", 'M', 'O', ios_defs::eofbit);
    FOri("%EM", 'M', 'E', ios_defs::eofbit);
    FOri("M",   'M', 'E', ios_defs::strfailbit);
    FOri("M",   'M', 'O', ios_defs::strfailbit);

    FOri("\n",   'n',  0,  ios_defs::eofbit);
    FOri("x",    'n',  0,  ios_defs::goodbit);
    FOri("\n",   'n', 'E', ios_defs::strfailbit);
    FOri("%En",  'n', 'E', ios_defs::eofbit);
    FOri("n",    'n', 'O', ios_defs::strfailbit);
    FOri("%On",  'n', 'O', ios_defs::eofbit);

    FOri("\t",   't',  0,  ios_defs::eofbit);
    FOri("x",    't',  0,  ios_defs::goodbit);
    FOri("\t",   't', 'E', ios_defs::strfailbit);
    FOri("%Et",  't', 'E', ios_defs::eofbit);
    FOri("n",    't', 'O', ios_defs::strfailbit);
    FOri("%Ot",  't', 'O', ios_defs::eofbit);

    FOri("%p", 'p', 0, ios_defs::eofbit);
    FOri("%Ep", 'p', 'E', ios_defs::eofbit);
    FOri("p",   'p', 'E', ios_defs::strfailbit);
    FOri("%Op", 'p', 'O', ios_defs::eofbit);
    FOri("p",   'p', 'O', ios_defs::strfailbit);

    FOri("%r", "%r",  ios_defs::eofbit);
    FOri("%Er", 'r', 'E', ios_defs::eofbit);
    FOri("r",   'r', 'E', ios_defs::strfailbit);
    FOri("%Or", 'r', 'O', ios_defs::eofbit);
    FOri("r",   'r', 'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33", "%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri("%ER", 'R', 'E', ios_defs::eofbit);
    FOri("R",   'R', 'E', ios_defs::strfailbit);
    FOri("%OR", 'R', 'O', ios_defs::eofbit);
    FOri("R",   'R', 'O', ios_defs::strfailbit);

    FOri("%S", 'S', 0,   ios_defs::eofbit);
    FOri("%OS", 'S', 'O', ios_defs::eofbit);
    FOri("%ES", 'S', 'E', ios_defs::eofbit);
    FOri("S",   'S', 'E', ios_defs::strfailbit);
    FOri("S",   'S', 'O', ios_defs::strfailbit);

    FOri("%X", "%X",  ios_defs::eofbit);
    FOri("%EX", "%EX",  ios_defs::eofbit);
    FOri("X",   'X', 'E', ios_defs::strfailbit);
    FOri("%OX", 'X', 'O', ios_defs::eofbit);
    FOri("X",   'X', 'O', ios_defs::strfailbit);

    FOri("%T", "%T",  ios_defs::eofbit);
    FOri("%ET", 'T', 'E', ios_defs::eofbit);
    FOri("T",   'T', 'E', ios_defs::strfailbit);
    FOri("%OT", 'T', 'O', ios_defs::eofbit);
    FOri("T",   'T', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("3", 'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri("3", 'u', 'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri("三", 'u', 'O', ios_defs::eofbit).m_wday, 3);
    FOri("%Eu", 'u', 'E', ios_defs::eofbit);
    FOri("u",   'u', 'E', ios_defs::strfailbit);
    FOri("u",   'u', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("24", 'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri("%Eg", 'g', 'E', ios_defs::eofbit);
    FOri("g",   'g', 'E', ios_defs::strfailbit);
    FOri("%Og", 'g', 'O', ios_defs::eofbit);
    FOri("g",   'g', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("2024", 'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri("%EG", 'G', 'E', ios_defs::eofbit);
    FOri("G",   'G', 'E', ios_defs::strfailbit);
    FOri("%OG", 'G', 'O', ios_defs::eofbit);
    FOri("G",   'G', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd("2024 35 水", "%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("2024 35 水", "%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("2024 三十五 水", "%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri("35", 'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(FOri("35", 'U', 'O', ios_defs::eofbit).m_week_no, 35);
    FOri("%EU", 'U', 'E', ios_defs::eofbit);
    FOri("U",   'U', 'E', ios_defs::strfailbit);
    FOri("U",   'U', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd("2024 36 水", "%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("2024 36 水", "%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd("2024 三十六 水", "%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri("36", 'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(FOri("36", 'W', 'O', ios_defs::eofbit).m_week_no, 36);
    FOri("%EW", 'W', 'E', ios_defs::eofbit);
    FOri("W",   'W', 'E', ios_defs::strfailbit);
    FOri("W",   'W', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("36", 'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri("36", 'V', 'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri("三十六", 'V', 'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    FOri("54",  'V', 'O', ios_defs::strfailbit);
    FOri("%EV", 'V', 'E', ios_defs::eofbit);
    FOri("V",   'V', 'E', ios_defs::strfailbit);
    FOri("V",   'V', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("3", 'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri("3", 'w', 'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri("三", 'w', 'O', ios_defs::eofbit).m_wday, 3);
    FOri("%Ew", 'w', 'E', ios_defs::eofbit);
    FOri("w",   'w', 'E', ios_defs::strfailbit);
    FOri("w",   'w', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("24", 'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd("6", 'y', 'E', ios_defs::eofbit).year(), std::chrono::year(2024));
    EXPECT_EQ(FOri("24", 'y', 'O', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri("二十四", 'y', 'O', ios_defs::eofbit).m_year, 2024);
    FOri("y",  'y', 'E', ios_defs::strfailbit);
    FOri("y",  'y', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("2024", 'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri("2024", 'Y', 'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd("平成3年", 'Y', 'E', ios_defs::eofbit).year(), std::chrono::year(1991));
    FOri("Y",   'Y', 'E', ios_defs::strfailbit);
    FOri("%OY", 'Y', 'O', ios_defs::eofbit);
    FOri("Y",   'Y', 'O', ios_defs::strfailbit);

    FOri("%Z", 'Z', 0, ios_defs::eofbit);
    FOri("%EZ", 'Z', 'E', ios_defs::eofbit);
    FOri("Z",   'Z', 'E', ios_defs::strfailbit);
    FOri("%OZ", 'Z', 'O', ios_defs::eofbit);
    FOri("Z",   'Z', 'O', ios_defs::strfailbit);

    FOri("%z", 'z', 0, ios_defs::eofbit);
    FOri("%Ez", 'z', 'E', ios_defs::eofbit);
    FOri("%Oz", 'z', 'O', ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(FYmd("1999-W52-6", "%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd("2019-W01-1", "%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd("1999-W52-5", "%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd("99-W52-6", "%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd("19-W01-1", "%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd("99-W52-5", "%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd("20 24/09/04", "%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((FYmd("20 01 01", "%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar, ATimeOfDayReadsEveryConversionSpecifierItCanSupply)
{
    timeio obj(std::make_shared<timeio_conf<char>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<char, false, true, tz_level::zone>, false, true, tz_level::zone>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, tz_level::zone>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri("%",  '%',  0,  ios_defs::eofbit);
    FOri("x",  '%',  0,  ios_defs::strfailbit);
    FOri("%",  '%', 'E', febit);
    FOri("%E%", '%', 'E', ios_defs::eofbit);
    FOri("%",  '%', 'O', febit);
    FOri("%O%", '%', 'O', ios_defs::eofbit);

    FOri("%a", 'a', 0, ios_defs::eofbit);
    FOri("%Ea", 'a', 'E', ios_defs::eofbit);
    FOri("a",   'a', 'E', ios_defs::strfailbit);
    FOri("%Oa", 'a', 'O', ios_defs::eofbit);
    FOri("a",   'a', 'O', ios_defs::strfailbit);

    FOri("%A", 'A', 0, ios_defs::eofbit);
    FOri("%EA", 'A', 'E', ios_defs::eofbit);
    FOri("A",   'A', 'E', ios_defs::strfailbit);
    FOri("%OA", 'A', 'O', ios_defs::eofbit);
    FOri("A",   'A', 'O', ios_defs::strfailbit);

    FOri("%b", 'b', 0, ios_defs::eofbit);
    FOri("%Eb", 'b', 'E', ios_defs::eofbit);
    FOri("b",   'b', 'E', ios_defs::strfailbit);
    FOri("%Ob", 'b', 'O', ios_defs::eofbit);
    FOri("b",   'b', 'O', ios_defs::strfailbit);

    FOri("%B", 'B', 0, ios_defs::eofbit);
    FOri("%EB", 'B', 'E', ios_defs::eofbit);
    FOri("B",   'B', 'E', ios_defs::strfailbit);
    FOri("%OB", 'B', 'O', ios_defs::eofbit);
    FOri("B",   'B', 'O', ios_defs::strfailbit);

    FOri("%h", 'h', 0, ios_defs::eofbit);
    FOri("%Eh", 'h', 'E', ios_defs::eofbit);
    FOri("h",   'h', 'E', ios_defs::strfailbit);
    FOri("%Oh", 'h', 'O', ios_defs::eofbit);
    FOri("h",   'h', 'O', ios_defs::strfailbit);

    using namespace std::chrono;
    FOri("%c", 'c', 0, ios_defs::eofbit);
    FOri("%Ec", 'c', 'E', ios_defs::eofbit);
    FOri("c",   'c', 'E', ios_defs::strfailbit);
    FOri("%Oc", 'c', 'O', ios_defs::eofbit);
    FOri("c",   'c', 'O', ios_defs::strfailbit);

    FOri("%C", 'C', 0,   ios_defs::eofbit);
    FOri("%EC", 'C', 'E', ios_defs::eofbit);
    FOri("C",   'C', 'E', ios_defs::strfailbit);
    FOri("%OC", 'C', 'O', ios_defs::eofbit);
    FOri("C",   'C', 'O', ios_defs::strfailbit);

    FOri("%d", 'd', 0,   ios_defs::eofbit);
    FOri("%Od", 'd', 'O', ios_defs::eofbit);
    FOri("%Ed", 'd', 'E', ios_defs::eofbit);
    FOri("d",   'd', 'E', ios_defs::strfailbit);
    FOri("d",   'd', 'O', ios_defs::strfailbit);

    FOri("%e", 'e', 0,   ios_defs::eofbit);
    FOri("%Oe", 'e', 'O', ios_defs::eofbit);
    FOri("%Ee", 'e', 'E', ios_defs::eofbit);
    FOri("e",   'e', 'E', ios_defs::strfailbit);
    FOri("e",   'e', 'O', ios_defs::strfailbit);

    FOri("%F", 'F', 0, ios_defs::eofbit);
    FOri("%EF", 'F', 'E', ios_defs::eofbit);
    FOri("F",   'F', 'E', ios_defs::strfailbit);
    FOri("%OF", 'F', 'O', ios_defs::eofbit);
    FOri("F",   'F', 'O', ios_defs::strfailbit);

    FOri("%x", 'x', 0, ios_defs::eofbit);
    FOri("%Ex", 'x', 'E', ios_defs::eofbit);
    FOri("x",   'x', 'E', ios_defs::strfailbit);
    FOri("%Ox", 'x', 'O', ios_defs::eofbit);
    FOri("x",   'x', 'O', ios_defs::strfailbit);

    FOri("%D", 'D', 0, ios_defs::eofbit);
    FOri("%ED", 'D', 'E', ios_defs::eofbit);
    FOri("D",   'D', 'E', ios_defs::strfailbit);
    FOri("%OD", 'D', 'O', ios_defs::eofbit);
    FOri("D",   'D', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("13", 'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri("13", 'H', 'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri("十三", 'H', 'O', ios_defs::eofbit).m_hour, 13);
    FOri("%EH", 'H', 'E', ios_defs::eofbit);
    FOri("H",   'H', 'E', ios_defs::strfailbit);
    FOri("H",   'H', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("01", 'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri("01", 'I', 'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri("一", 'I', 'O', ios_defs::eofbit).m_hour, 1);
    FOri("%EI", 'I', 'E', ios_defs::eofbit);
    FOri("I",   'I', 'E', ios_defs::strfailbit);
    FOri("I",   'I', 'O', ios_defs::strfailbit);

    FOri("%j", 'j', 0, ios_defs::eofbit);
    FOri("%Ej", 'j', 'E', ios_defs::eofbit);
    FOri("j",   'j', 'E', ios_defs::strfailbit);
    FOri("%Oj", 'j', 'O', ios_defs::eofbit);
    FOri("j",   'j', 'O', ios_defs::strfailbit);

    FOri("%m", 'm',  0, ios_defs::eofbit);
    FOri("%Om", 'm', 'O', ios_defs::eofbit);
    FOri("%Em", 'm', 'E', ios_defs::eofbit);
    FOri("m",   'm', 'E', ios_defs::strfailbit);
    FOri("m",   'm', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("33", 'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri("33", 'M', 'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri("三十三", 'M', 'O', ios_defs::eofbit).m_minute, 33);
    FOri("%EM", 'M', 'E', ios_defs::eofbit);
    FOri("M",   'M', 'E', ios_defs::strfailbit);
    FOri("M",   'M', 'O', ios_defs::strfailbit);

    FOri("\n",   'n',  0,  ios_defs::eofbit);
    FOri("x",    'n',  0,  ios_defs::goodbit);
    FOri("\n",   'n', 'E', ios_defs::strfailbit);
    FOri("%En",  'n', 'E', ios_defs::eofbit);
    FOri("n",    'n', 'O', ios_defs::strfailbit);
    FOri("%On",  'n', 'O', ios_defs::eofbit);

    FOri("\t",   't',  0,  ios_defs::eofbit);
    FOri("x",    't',  0,  ios_defs::goodbit);
    FOri("\t",   't', 'E', ios_defs::strfailbit);
    FOri("%Et",  't', 'E', ios_defs::eofbit);
    FOri("n",    't', 'O', ios_defs::strfailbit);
    FOri("%Ot",  't', 'O', ios_defs::eofbit);

    EXPECT_EQ(FHms("01 午後", "%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(FHms("01 午前", "%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(FOri("午後", 'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(FOri("午前", 'p', 0, ios_defs::eofbit).m_is_pm, false);
    FOri("%Ep", 'p', 'E', ios_defs::eofbit);
    FOri("p",   'p', 'E', ios_defs::strfailbit);
    FOri("%Op", 'p', 'O', ios_defs::eofbit);
    FOri("p",   'p', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms("午後01時33分18秒", "%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("%Er", 'r', 'E', ios_defs::eofbit);
    FOri("r",   'r', 'E', ios_defs::strfailbit);
    FOri("%Or", 'r', 'O', ios_defs::eofbit);
    FOri("r",   'r', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms("13:33", "%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri("%ER", 'R', 'E', ios_defs::eofbit);
    FOri("R",   'R', 'E', ios_defs::strfailbit);
    FOri("%OR", 'R', 'O', ios_defs::eofbit);
    FOri("R",   'R', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("18", 'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri("18", 'S', 'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri("十八", 'S', 'O', ios_defs::eofbit).m_second, 18);
    FOri("%ES", 'S', 'E', ios_defs::eofbit);
    FOri("S",   'S', 'E', ios_defs::strfailbit);
    FOri("S",   'S', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms("13時33分18秒", "%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(FHms("13時33分18秒", "%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("X",   'X', 'E', ios_defs::strfailbit);
    FOri("%OX", 'X', 'O', ios_defs::eofbit);
    FOri("X",   'X', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms("13:33:18", "%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("%ET", 'T', 'E', ios_defs::eofbit);
    FOri("T",   'T', 'E', ios_defs::strfailbit);
    FOri("%OT", 'T', 'O', ios_defs::eofbit);
    FOri("T",   'T', 'O', ios_defs::strfailbit);

    FOri("%u", 'u', 0,   ios_defs::eofbit);
    FOri("%Ou", 'u', 'O', ios_defs::eofbit);
    FOri("%Eu", 'u', 'E', ios_defs::eofbit);
    FOri("u",   'u', 'E', ios_defs::strfailbit);
    FOri("u",   'u', 'O', ios_defs::strfailbit);

    FOri("%g", 'g', 0, ios_defs::eofbit);
    FOri("%Eg", 'g', 'E', ios_defs::eofbit);
    FOri("g",   'g', 'E', ios_defs::strfailbit);
    FOri("%Og", 'g', 'O', ios_defs::eofbit);
    FOri("g",   'g', 'O', ios_defs::strfailbit);

    FOri("%G", 'G', 0, ios_defs::eofbit);
    FOri("%EG", 'G', 'E', ios_defs::eofbit);
    FOri("G",   'G', 'E', ios_defs::strfailbit);
    FOri("%OG", 'G', 'O', ios_defs::eofbit);
    FOri("G",   'G', 'O', ios_defs::strfailbit);

    FOri("%U", 'U', 0,   ios_defs::eofbit);
    FOri("%OU", 'U', 'O', ios_defs::eofbit);
    FOri("%EU", 'U', 'E', ios_defs::eofbit);
    FOri("U",   'U', 'E', ios_defs::strfailbit);
    FOri("U",   'U', 'O', ios_defs::strfailbit);

    FOri("%W", 'W', 0,   ios_defs::eofbit);
    FOri("%OW", 'W', 'O', ios_defs::eofbit);
    FOri("%EW", 'W', 'E', ios_defs::eofbit);
    FOri("W",   'W', 'E', ios_defs::strfailbit);
    FOri("W",   'W', 'O', ios_defs::strfailbit);

    FOri("%V", 'V', 0,   ios_defs::eofbit);
    FOri("%OV", 'V', 'O',   ios_defs::eofbit);
    FOri("54",  'V', 'O', ios_defs::strfailbit);
    FOri("%EV", 'V', 'E', ios_defs::eofbit);
    FOri("V",   'V', 'E', ios_defs::strfailbit);
    FOri("V",   'V', 'O', ios_defs::strfailbit);

    FOri("%w", 'w', 0,   ios_defs::eofbit);
    FOri("%Ow", 'w', 'O', ios_defs::eofbit);
    FOri("%Ew", 'w', 'E', ios_defs::eofbit);
    FOri("w",   'w', 'E', ios_defs::strfailbit);
    FOri("w",   'w', 'O', ios_defs::strfailbit);

    FOri("%y", 'y', 0,   ios_defs::eofbit);
    FOri("%Ey", 'y', 'E', ios_defs::eofbit);
    FOri("%Oy", 'y', 'O', ios_defs::eofbit);
    FOri("y",  'y', 'E', ios_defs::strfailbit);
    FOri("y",  'y', 'O', ios_defs::strfailbit);

    FOri("%Y", 'Y', 0,   ios_defs::eofbit);
    FOri("%EY", 'Y', 'E', ios_defs::eofbit);
    FOri("Y",   'Y', 'E', ios_defs::strfailbit);
    FOri("%OY", 'Y', 'O', ios_defs::eofbit);
    FOri("Y",   'Y', 'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(FOri("America/Los_Angeles", 'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = FOri("PST", 'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    FOri("America/Los_Angexes", 'Z', 0, ios_defs::strfailbit);
    FOri("%EZ", 'Z', 'E', ios_defs::eofbit);
    FOri("Z",   'Z', 'E', ios_defs::strfailbit);
    FOri("%OZ", 'Z', 'O', ios_defs::eofbit);
    FOri("Z",   'Z', 'O', ios_defs::strfailbit);

    { auto r = FOri("+0800", 'z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_have_offset && r.m_offset == minutes{480}); }
    FOri("%z", 'z', 0, ios_defs::strfailbit);
    FOri("%Ez", 'z', 'E', ios_defs::eofbit);
    FOri("z",  'z', 'E', ios_defs::strfailbit);
    FOri("%Oz", 'z', 'O', ios_defs::eofbit);
    FOri("z",  'z', 'O', ios_defs::strfailbit);
}

TEST(TimeioChar, ATimeOfDayReadsTheSameSpecifiersWithNoZoneTier)
{
    timeio obj(std::make_shared<timeio_conf<char>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<char, false, true, tz_level::none>, false, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri("%",  '%',  0,  ios_defs::eofbit);
    FOri("x",  '%',  0,  ios_defs::strfailbit);
    FOri("%",  '%', 'E', febit);
    FOri("%E%", '%', 'E', ios_defs::eofbit);
    FOri("%",  '%', 'O', febit);
    FOri("%O%", '%', 'O', ios_defs::eofbit);

    FOri("%a", 'a', 0, ios_defs::eofbit);
    FOri("%Ea", 'a', 'E', ios_defs::eofbit);
    FOri("a",   'a', 'E', ios_defs::strfailbit);
    FOri("%Oa", 'a', 'O', ios_defs::eofbit);
    FOri("a",   'a', 'O', ios_defs::strfailbit);

    FOri("%A", 'A', 0, ios_defs::eofbit);
    FOri("%EA", 'A', 'E', ios_defs::eofbit);
    FOri("A",   'A', 'E', ios_defs::strfailbit);
    FOri("%OA", 'A', 'O', ios_defs::eofbit);
    FOri("A",   'A', 'O', ios_defs::strfailbit);

    FOri("%b", 'b', 0, ios_defs::eofbit);
    FOri("%Eb", 'b', 'E', ios_defs::eofbit);
    FOri("b",   'b', 'E', ios_defs::strfailbit);
    FOri("%Ob", 'b', 'O', ios_defs::eofbit);
    FOri("b",   'b', 'O', ios_defs::strfailbit);

    FOri("%B", 'B', 0, ios_defs::eofbit);
    FOri("%EB", 'B', 'E', ios_defs::eofbit);
    FOri("B",   'B', 'E', ios_defs::strfailbit);
    FOri("%OB", 'B', 'O', ios_defs::eofbit);
    FOri("B",   'B', 'O', ios_defs::strfailbit);

    FOri("%h", 'h', 0, ios_defs::eofbit);
    FOri("%Eh", 'h', 'E', ios_defs::eofbit);
    FOri("h",   'h', 'E', ios_defs::strfailbit);
    FOri("%Oh", 'h', 'O', ios_defs::eofbit);
    FOri("h",   'h', 'O', ios_defs::strfailbit);

    using namespace std::chrono;
    FOri("%c", 'c', 0, ios_defs::eofbit);
    FOri("%Ec", 'c', 'E', ios_defs::eofbit);
    FOri("c",   'c', 'E', ios_defs::strfailbit);
    FOri("%Oc", 'c', 'O', ios_defs::eofbit);
    FOri("c",   'c', 'O', ios_defs::strfailbit);

    FOri("%C", 'C', 0,   ios_defs::eofbit);
    FOri("%EC", 'C', 'E', ios_defs::eofbit);
    FOri("C",   'C', 'E', ios_defs::strfailbit);
    FOri("%OC", 'C', 'O', ios_defs::eofbit);
    FOri("C",   'C', 'O', ios_defs::strfailbit);

    FOri("%d", 'd', 0,   ios_defs::eofbit);
    FOri("%Od", 'd', 'O', ios_defs::eofbit);
    FOri("%Ed", 'd', 'E', ios_defs::eofbit);
    FOri("d",   'd', 'E', ios_defs::strfailbit);
    FOri("d",   'd', 'O', ios_defs::strfailbit);

    FOri("%e", 'e', 0,   ios_defs::eofbit);
    FOri("%Oe", 'e', 'O', ios_defs::eofbit);
    FOri("%Ee", 'e', 'E', ios_defs::eofbit);
    FOri("e",   'e', 'E', ios_defs::strfailbit);
    FOri("e",   'e', 'O', ios_defs::strfailbit);

    FOri("%F", 'F', 0, ios_defs::eofbit);
    FOri("%EF", 'F', 'E', ios_defs::eofbit);
    FOri("F",   'F', 'E', ios_defs::strfailbit);
    FOri("%OF", 'F', 'O', ios_defs::eofbit);
    FOri("F",   'F', 'O', ios_defs::strfailbit);

    FOri("%x", 'x', 0, ios_defs::eofbit);
    FOri("%Ex", 'x', 'E', ios_defs::eofbit);
    FOri("x",   'x', 'E', ios_defs::strfailbit);
    FOri("%Ox", 'x', 'O', ios_defs::eofbit);
    FOri("x",   'x', 'O', ios_defs::strfailbit);

    FOri("%D", 'D', 0, ios_defs::eofbit);
    FOri("%ED", 'D', 'E', ios_defs::eofbit);
    FOri("D",   'D', 'E', ios_defs::strfailbit);
    FOri("%OD", 'D', 'O', ios_defs::eofbit);
    FOri("D",   'D', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("13", 'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri("13", 'H', 'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri("十三", 'H', 'O', ios_defs::eofbit).m_hour, 13);
    FOri("%EH", 'H', 'E', ios_defs::eofbit);
    FOri("H",   'H', 'E', ios_defs::strfailbit);
    FOri("H",   'H', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("01", 'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri("01", 'I', 'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri("一", 'I', 'O', ios_defs::eofbit).m_hour, 1);
    FOri("%EI", 'I', 'E', ios_defs::eofbit);
    FOri("I",   'I', 'E', ios_defs::strfailbit);
    FOri("I",   'I', 'O', ios_defs::strfailbit);

    FOri("%j", 'j', 0, ios_defs::eofbit);
    FOri("%Ej", 'j', 'E', ios_defs::eofbit);
    FOri("j",   'j', 'E', ios_defs::strfailbit);
    FOri("%Oj", 'j', 'O', ios_defs::eofbit);
    FOri("j",   'j', 'O', ios_defs::strfailbit);

    FOri("%m", 'm',  0, ios_defs::eofbit);
    FOri("%Om", 'm', 'O', ios_defs::eofbit);
    FOri("%Em", 'm', 'E', ios_defs::eofbit);
    FOri("m",   'm', 'E', ios_defs::strfailbit);
    FOri("m",   'm', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("33", 'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri("33", 'M', 'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri("三十三", 'M', 'O', ios_defs::eofbit).m_minute, 33);
    FOri("%EM", 'M', 'E', ios_defs::eofbit);
    FOri("M",   'M', 'E', ios_defs::strfailbit);
    FOri("M",   'M', 'O', ios_defs::strfailbit);

    FOri("\n",   'n',  0,  ios_defs::eofbit);
    FOri("x",    'n',  0,  ios_defs::goodbit);
    FOri("\n",   'n', 'E', ios_defs::strfailbit);
    FOri("%En",  'n', 'E', ios_defs::eofbit);
    FOri("n",    'n', 'O', ios_defs::strfailbit);
    FOri("%On",  'n', 'O', ios_defs::eofbit);

    FOri("\t",   't',  0,  ios_defs::eofbit);
    FOri("x",    't',  0,  ios_defs::goodbit);
    FOri("\t",   't', 'E', ios_defs::strfailbit);
    FOri("%Et",  't', 'E', ios_defs::eofbit);
    FOri("n",    't', 'O', ios_defs::strfailbit);
    FOri("%Ot",  't', 'O', ios_defs::eofbit);

    EXPECT_EQ(FHms("01 午後", "%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(FHms("01 午前", "%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(FOri("午後", 'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(FOri("午前", 'p', 0, ios_defs::eofbit).m_is_pm, false);
    FOri("%Ep", 'p', 'E', ios_defs::eofbit);
    FOri("p",   'p', 'E', ios_defs::strfailbit);
    FOri("%Op", 'p', 'O', ios_defs::eofbit);
    FOri("p",   'p', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms("午後01時33分18秒", "%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("%Er", 'r', 'E', ios_defs::eofbit);
    FOri("r",   'r', 'E', ios_defs::strfailbit);
    FOri("%Or", 'r', 'O', ios_defs::eofbit);
    FOri("r",   'r', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms("13:33", "%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri("%ER", 'R', 'E', ios_defs::eofbit);
    FOri("R",   'R', 'E', ios_defs::strfailbit);
    FOri("%OR", 'R', 'O', ios_defs::eofbit);
    FOri("R",   'R', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri("18", 'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri("18", 'S', 'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri("十八", 'S', 'O', ios_defs::eofbit).m_second, 18);
    FOri("%ES", 'S', 'E', ios_defs::eofbit);
    FOri("S",   'S', 'E', ios_defs::strfailbit);
    FOri("S",   'S', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms("13時33分18秒", "%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(FHms("13時33分18秒", "%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("X",   'X', 'E', ios_defs::strfailbit);
    FOri("%OX", 'X', 'O', ios_defs::eofbit);
    FOri("X",   'X', 'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms("13:33:18", "%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("%ET", 'T', 'E', ios_defs::eofbit);
    FOri("T",   'T', 'E', ios_defs::strfailbit);
    FOri("%OT", 'T', 'O', ios_defs::eofbit);
    FOri("T",   'T', 'O', ios_defs::strfailbit);

    FOri("%u", 'u', 0,   ios_defs::eofbit);
    FOri("%Ou", 'u', 'O', ios_defs::eofbit);
    FOri("%Eu", 'u', 'E', ios_defs::eofbit);
    FOri("u",   'u', 'E', ios_defs::strfailbit);
    FOri("u",   'u', 'O', ios_defs::strfailbit);

    FOri("%g", 'g', 0, ios_defs::eofbit);
    FOri("%Eg", 'g', 'E', ios_defs::eofbit);
    FOri("g",   'g', 'E', ios_defs::strfailbit);
    FOri("%Og", 'g', 'O', ios_defs::eofbit);
    FOri("g",   'g', 'O', ios_defs::strfailbit);

    FOri("%G", 'G', 0, ios_defs::eofbit);
    FOri("%EG", 'G', 'E', ios_defs::eofbit);
    FOri("G",   'G', 'E', ios_defs::strfailbit);
    FOri("%OG", 'G', 'O', ios_defs::eofbit);
    FOri("G",   'G', 'O', ios_defs::strfailbit);

    FOri("%U", 'U', 0,   ios_defs::eofbit);
    FOri("%OU", 'U', 'O', ios_defs::eofbit);
    FOri("%EU", 'U', 'E', ios_defs::eofbit);
    FOri("U",   'U', 'E', ios_defs::strfailbit);
    FOri("U",   'U', 'O', ios_defs::strfailbit);

    FOri("%W", 'W', 0,   ios_defs::eofbit);
    FOri("%OW", 'W', 'O', ios_defs::eofbit);
    FOri("%EW", 'W', 'E', ios_defs::eofbit);
    FOri("W",   'W', 'E', ios_defs::strfailbit);
    FOri("W",   'W', 'O', ios_defs::strfailbit);

    FOri("%V", 'V', 0,   ios_defs::eofbit);
    FOri("%OV", 'V', 'O',   ios_defs::eofbit);
    FOri("54",  'V', 'O', ios_defs::strfailbit);
    FOri("%EV", 'V', 'E', ios_defs::eofbit);
    FOri("V",   'V', 'E', ios_defs::strfailbit);
    FOri("V",   'V', 'O', ios_defs::strfailbit);

    FOri("%w", 'w', 0,   ios_defs::eofbit);
    FOri("%Ow", 'w', 'O', ios_defs::eofbit);
    FOri("%Ew", 'w', 'E', ios_defs::eofbit);
    FOri("w",   'w', 'E', ios_defs::strfailbit);
    FOri("w",   'w', 'O', ios_defs::strfailbit);

    FOri("%y", 'y', 0,   ios_defs::eofbit);
    FOri("%Ey", 'y', 'E', ios_defs::eofbit);
    FOri("%Oy", 'y', 'O', ios_defs::eofbit);
    FOri("y",  'y', 'E', ios_defs::strfailbit);
    FOri("y",  'y', 'O', ios_defs::strfailbit);

    FOri("%Y", 'Y', 0,   ios_defs::eofbit);
    FOri("%EY", 'Y', 'E', ios_defs::eofbit);
    FOri("Y",   'Y', 'E', ios_defs::strfailbit);
    FOri("%OY", 'Y', 'O', ios_defs::eofbit);
    FOri("Y",   'Y', 'O', ios_defs::strfailbit);

    FOri("%Z", 'Z', 0, ios_defs::eofbit);
    FOri("%EZ", 'Z', 'E', ios_defs::eofbit);
    FOri("Z",   'Z', 'E', ios_defs::strfailbit);
    FOri("%OZ", 'Z', 'O', ios_defs::eofbit);
    FOri("Z",   'Z', 'O', ios_defs::strfailbit);

    FOri("%z", 'z', 0, ios_defs::eofbit);
    FOri("%Ez", 'z', 'E', ios_defs::eofbit);
    FOri("z",  'z', 'E', ios_defs::strfailbit);
    FOri("%Oz", 'z', 'O', ios_defs::eofbit);
    FOri("z",  'z', 'O', ios_defs::strfailbit);
}

TEST(TimeioChar, AValueThatIsNotAValidTimeIsRejected)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char>>("C"));
    std::string res;

    // put(year_month_day) with invalid date (line 1173)
    {
        auto invalid_ymd = year_month_day{year{2024}, month{2}, day{30}};
        EXPECT_THROW(obj.put(std::back_inserter(res), invalid_ymd, std::string_view("%F")), stream_error);
    }

    // put(hh_mm_ss) with negative total duration (line 1214)
    {
        hh_mm_ss<seconds> invalid_hms{seconds{-1}};
        EXPECT_THROW(obj.put(std::back_inserter(res), invalid_hms, std::string_view("%T")), stream_error);
    }

    // put(std::tm) with out-of-range field: month=-1 (line 1271)
    {
        std::tm bad_tm{};
        bad_tm.tm_year = 124; bad_tm.tm_mon = -1;
        bad_tm.tm_mday = 1; bad_tm.tm_hour = 0; bad_tm.tm_min = 0; bad_tm.tm_sec = 0;
        EXPECT_THROW(obj.put(std::back_inserter(res), bad_tm, std::string_view("%F")), stream_error);
    }

    // put(std::tm) with valid fields but invalid date: Feb 30 (line 1275)
    {
        std::tm bad_tm{};
        bad_tm.tm_year = 124; bad_tm.tm_mon = 1; bad_tm.tm_mday = 30;
        bad_tm.tm_hour = 0; bad_tm.tm_min = 0; bad_tm.tm_sec = 0;
        EXPECT_THROW(obj.put(std::back_inserter(res), bad_tm, std::string_view("%F")), stream_error);
    }

    // put(std::tm) with a tm_year whose +1900 would overflow int: rejected, not UB.
    // The "not UB" half is what the fix is about, and only the sanitizer build can see it:
    // the wrapped sum lands in [-2^31, -2^31+1899], entirely below year::min(), so the
    // pre-fix code rejects exactly the same tm values and every VERIFY below still passes.
    // What catches a regression is the signed-overflow report, i.e. MODE=sanitizer with
    // UBSAN_OPTIONS=halt_on_error=1 (which the CI sets).
    {
        for (int bad_year : {std::numeric_limits<int>::max(), std::numeric_limits<int>::min(),
                             30868, -34668})
        {
            std::tm bad_tm{};
            bad_tm.tm_year = bad_year; bad_tm.tm_mon = 0; bad_tm.tm_mday = 1;
            bad_tm.tm_hour = 0; bad_tm.tm_min = 0; bad_tm.tm_sec = 0;
            EXPECT_THROW(obj.put(std::back_inserter(res), bad_tm, std::string_view("%Y")), stream_error);
        }
    }

    // put(std::tm) at the year bounds themselves: still accepted
    {
        std::tm edge_tm{};
        edge_tm.tm_mon = 0; edge_tm.tm_mday = 1;
        edge_tm.tm_hour = 0; edge_tm.tm_min = 0; edge_tm.tm_sec = 0;

        edge_tm.tm_year = static_cast<int>(year::max()) - 1900;
        res.clear(); obj.put(std::back_inserter(res), edge_tm, std::string_view("%Y"));
        EXPECT_EQ(res, "32767");

        edge_tm.tm_year = static_cast<int>(year::min()) - 1900;
        res.clear(); obj.put(std::back_inserter(res), edge_tm, std::string_view("%Y"));
        EXPECT_EQ(res, "-32767");
    }

    // put(year_month_day) with negative year: %Y and %C output sign (lines 2860-2861, 2543-2544)
    {
        auto neg_ymd = year_month_day{year{-1}, month{1}, day{1}};
        res.clear(); obj.put(std::back_inserter(res), neg_ymd, std::string_view("%Y"));
        EXPECT_EQ(res, "-0001");
        res.clear(); obj.put(std::back_inserter(res), neg_ymd, std::string_view("%C"));
        EXPECT_EQ(res, "-01");
    }

    // put(year_month_day) for date in ISO year -1: %G output sign (lines 2608-2609)
    // Jan 1, year 0 is a Saturday; Thu of that ISO week is Dec 30, year -1 -> G=-0001
    {
        auto early_ymd = year_month_day{year{0}, month{1}, day{1}};
        res.clear(); obj.put(std::back_inserter(res), early_ymd, std::string_view("%G"));
        EXPECT_EQ(res, "-0001");
    }

    // put(zoned_time) with positive offset: %z outputs '+' (line 2883)
    {
        auto tp = create_zoned_time(2024, 9, 4, 12, 0, 0, "Asia/Tokyo");
        res.clear(); obj.put(std::back_inserter(res), tp, std::string_view("%z"));
        EXPECT_EQ(res, "+0900");
    }
}

TEST(TimeioChar, ADateIsAssembledFromWhicheverFieldsWereParsed)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char>>("C"));
    timeio obj_ja(std::make_shared<timeio_conf<char>>("ja_JP.UTF-8"));
    timeio obj_zh_tw(std::make_shared<timeio_conf<char>>("zh_TW.UTF-8"));

    // operator year_month_day() throws for invalid reconstructed date (line 126)
    // Feb 30 parses successfully but is not a valid calendar date
    {
        auto ctx = CheckGet(obj, "02 30", "%m %d", ios_defs::eofbit);
        EXPECT_THROW(auto ymd = ctx_to<year_month_day>(ctx); (void)ymd, stream_error);
    }

    // Era deduction: m_have_mon=true, m_have_mday=false, match found (lines 224-241)
    // 令和6 January: est_year=2024, Jan is within 令和 era -> deduced_year=2024
    {
        auto ctx = CheckGet(obj_ja, "令和6 01", "%EC%Ey %m", ios_defs::eofbit);
        auto ymd = ctx_to<year_month_day>(ctx);
        EXPECT_TRUE(ymd.year() == year{2024} && ymd.month() == month{1});
    }

    // Era deduction: m_have_mon=true, m_have_mday=false, nothing matches (lines 245-246)
    // 平成31 May: est_year=2019, May 2019 past 平成 end (Apr 30) -> from_year=1990
    {
        auto ctx = CheckGet(obj_ja, "平成31 05", "%EC%Ey %m", ios_defs::eofbit);
        auto ymd = ctx_to<year_month_day>(ctx);
        EXPECT_TRUE(ymd.year() == year{1990} && ymd.month() == month{5});
    }

    // Era deduction: m_have_mon=true, m_have_mday=true, nothing matches (line 220)
    // 平成31 May 1: May 1, 2019 past 平成 end (Apr 30) -> from_year=1990
    {
        auto ctx = CheckGet(obj_ja, "平成31 05 01", "%EC%Ey %m %d", ios_defs::eofbit);
        auto ymd = ctx_to<year_month_day>(ctx);
        EXPECT_EQ((ymd), (year_month_day{year{1990}, month{5}, day{1}}));
    }

    // Backward eras use the same era-year syntax but move toward the past. The
    // year-only cases exercise the range check that cannot assume from_year <=
    // to_year; the complete date also checks direction during reconstruction.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "紀元前100", "%EC%Ey",
                                            ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{-99});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "紀元前100 02 03", "%EC%Ey %m %d",
                                            ios_defs::eofbit);
        EXPECT_EQ((ymd), (year_month_day{year{-99}, month{2}, day{3}}));
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_zh_tw, "民前100", "%EC%Ey",
                                            ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1812});
    }

    // Era name with no era year (%EC on its own). The era name match sets no m_have_*
    // year flag, so this is the one deduction path that reads the era items outside the
    // %Ey branch above (line 332). 平成 rather than 令和 is what proves the era items
    // narrowed by the name are what pick the year: the unnarrowed table starts at 令和.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "令和", "%EC", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{2020});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成", "%EC", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1990});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "昭和", "%EC", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1927});
    }

    // The month and day the format string does leave open still come from the parse,
    // and from the fallback, respectively.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "令和 05 01", "%EC %m %d",
                                            ios_defs::eofbit);
        EXPECT_EQ((ymd), (year_month_day{year{2020}, month{5}, day{1}}));
    }

    // The era name beats the date hint for the year, while the hint still supplies the
    // month and day.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{2023}, month{9}, day{17}});
        const std::string in = "平成";
        EXPECT_EQ(obj_ja.get(in.begin(), in.end(), ctx, "%EC"), in.end());
        EXPECT_EQ((ctx_to<year_month_day>(ctx)), (year_month_day{year{1990}, month{9}, day{17}}));
    }

    // A format string that names no era must leave the whole fallback date alone. An era
    // locale used to rewrite the year to the first era's from_year even though neither
    // the input nor the format said anything about the year.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{2023}, month{9}, day{17}});
        const std::string in = "01:02";
        EXPECT_EQ(obj_ja.get(in.begin(), in.end(), ctx, "%H:%M"), in.end());
        EXPECT_EQ((ctx_to<year_month_day>(ctx)), (year_month_day{year{2023}, month{9}, day{17}}));
        // No %E specifier was seen, so the locale's era table was never copied in.
        EXPECT_TRUE(ctx.m_era_items.empty());
    }

    // A 元年 (first year of an era) form matches an era entry of its own, one whose
    // format carries no %Ey. The year therefore has to come from the era items, and only
    // the entry belonging to the format that actually matched holds the right from_year.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "令和元年", "%EY", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{2019});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成元年", "%EY", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1989});
    }

    // The ordinary era-year form goes through %Ey and must be unaffected.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "令和2年", "%EY", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{2020});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成3年", "%EY", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1991});
    }

    // %Ex expands to the locale's era date format, which reaches %EY the same way.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "令和元年06月15日", "%Ex",
                                            ios_defs::eofbit);
        EXPECT_EQ((ymd), (year_month_day{year{2019}, month{6}, day{15}}));
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成元年06月15日", "%Ex",
                                            ios_defs::eofbit);
        EXPECT_EQ((ymd), (year_month_day{year{1989}, month{6}, day{15}}));
    }

    // A modifier belongs to one conversion specification and does not carry over to the
    // rest of the format string. POSIX attaches era semantics to %EC/%Ey/%EY only, so an
    // unmodified %y/%C/%Y after an %E specifier is still the plain Gregorian form -- even
    // though the era table has been filled in by then. Reading the table alone (rather
    // than the modifier) turns each of these into an era parse, and the ones below fail
    // outright: %y would take 4 digits as an era year and then find no era whose range
    // holds it.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成 88", "%EC %y", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1988});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成 19", "%EC %C", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1900});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成 1988", "%EC %Y", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1988});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成31 19 88", "%EC%Ey %C %y",
                                            ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1988});
    }

    // %Oy is the year within century in the locale's alternative numeric symbols; the O
    // modifier has nothing to do with eras. ja_JP defines no alternative digits, so per
    // POSIX ("if the alternative ... does not exist in the current locale, the unmodified
    // field descriptor is used") it reads ordinary digits.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成 88", "%EC %Oy", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1988});
    }

    // The other half of that POSIX sentence: with no era data at all, the E-modified
    // specifiers degrade to their unmodified meanings instead of failing.
    {
        auto ymd = CheckGet<year_month_day>(obj, "19", "%EC", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1900});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj, "88", "%Ey", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1988});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj, "1988", "%EY", ios_defs::eofbit);
        EXPECT_EQ(ymd.year(), year{1988});
    }

    // Negative yday via U-week+wday (lines 304-305)
    // 2024 Jan1=Monday(1), U=0, w=0(Sunday): yday=-1 -> Dec 31, 2023
    {
        auto ymd = CheckGet<year_month_day>(obj, "2024 0 0", "%Y %U %w", ios_defs::eofbit);
        EXPECT_EQ((ymd), (year_month_day{year{2023}, month{12}, day{31}}));
    }

    // Overflow yday via U-week+wday (lines 309-310)
    // 2024 Jan1=Monday(1), U=53, w=6(Saturday): yday=376 -> Jan 11, 2025
    {
        auto ymd = CheckGet<year_month_day>(obj, "2024 53 6", "%Y %U %w", ios_defs::eofbit);
        EXPECT_EQ((ymd), (year_month_day{year{2025}, month{1}, day{11}}));
    }

    // Week-only path (no wday): lines 343-370
    {
        // 2024 U=36 (normal): yday=252 -> Sep 9, 2024
        auto ymd1 = CheckGet<year_month_day>(obj, "2024 36", "%Y %U", ios_defs::eofbit);
        EXPECT_EQ((ymd1), (year_month_day{year{2024}, month{9}, day{9}}));

        // 2024 W=0: yday=-7 -> Dec 25, 2023 (lines 354-358)
        auto ymd2 = CheckGet<year_month_day>(obj, "2024 0", "%Y %W", ios_defs::eofbit);
        EXPECT_EQ((ymd2), (year_month_day{year{2023}, month{12}, day{25}}));

        // 2024 U=53: yday=371 -> Jan 6, 2025 (lines 359-362)
        auto ymd3 = CheckGet<year_month_day>(obj, "2024 53", "%Y %U", ios_defs::eofbit);
        EXPECT_EQ((ymd3), (year_month_day{year{2025}, month{1}, day{6}}));
    }

    // Format string ends after E/O modifier with no following specifier (lines 1510-1511)
    CheckGet(obj, "x", "%E", ios_defs::strfailbit);

    // %a/%A tree match failure (lines 1542-1543)
    CheckGet(obj, "xyz", 'a', (char)0, ios_defs::strfailbit);

    // %b/%B/%h tree match failure (lines 1564-1565)
    CheckGet(obj, "xyz", 'b', (char)0, ios_defs::strfailbit);

    // %e with leading space (line 1655)
    EXPECT_EQ(CheckGet(obj, " 4", 'e', (char)0, ios_defs::eofbit).m_mday, 4);

    // %p AM/PM tree miss (lines 1823-1824)
    CheckGet(obj, "xyz", 'p', (char)0, ios_defs::strfailbit);

    // %Ey era year out of range: all era items pruned (lines 2035-2036)
    // 平成32: delta=30 exceeds 平成 range=29 -> pruned -> stream_error
    CheckGet(obj_ja, "平成32", "%EC%Ey", ios_defs::strfailbit);

    // %z failures
    CheckGet(obj, "abc",   'z', (char)0, ios_defs::strfailbit); // not Z/+/- (line 2144)
    CheckGet(obj, "+",     'z', (char)0, ios_defs::strfailbit); // sign+EOF (line 2150)
    CheckGet(obj, "+123",  'z', (char)0, ios_defs::strfailbit); // 3 digits not 2 or 4 (line 2167)
    CheckGet(obj, "+2500", 'z', (char)0, ios_defs::strfailbit); // hour>=24 (line 2172)

    // bad_parse_format modifier mismatch (lines 2218-2219)
    // Format %Ej: %j rejects E modifier -> bad_parse_format; input "%Eb": '%','E' consumed but 'b'!='j'
    CheckGet(obj, "%Eb", 'j', 'E', ios_defs::strfailbit);

    // Format tail: trailing whitespace consumed at end of input (line 2233)
    CheckGet(obj, "Sep", std::string("%b "), ios_defs::eofbit);

    // Format tail: trailing %n consumed at end of input (lines 2237-2239)
    CheckGet(obj, "Sep", std::string("%b%n"), ios_defs::eofbit);

    // Format tail: non-%n/t specifier causes break at line 2238, then succ=false
    CheckGet(obj, "Sep", std::string("%b%z"), ios_defs::strfailbit);

    // Format tail: bare '%' at format end (next==cend) causes break at line 2236
    CheckGet(obj, "Sep", std::string("%b%"), ios_defs::strfailbit);
}

TEST(TimeioChar, AParsedContextConvertsToEveryShapeItCanFill)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char>>("C"));

    // set_hint(year_month_day): fields the format string does not parse keep the hint
    // instead of falling back to the wall clock.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{1969}, month{7}, day{20}});
        const std::string in = "09";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, 'm', 0), in.end());
        EXPECT_EQ((ctx_to<year_month_day>(ctx)), (year_month_day{year{1969}, month{9}, day{20}}));
    }

    // A parsed field still wins over the hint.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{1969}, month{7}, day{20}});
        const std::string in = "2024 03 04";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, "%Y %m %d"), in.end());
        EXPECT_EQ((ctx_to<year_month_day>(ctx)), (year_month_day{year{2024}, month{3}, day{4}}));
    }

    // The hint does not supply the year within the century %C leaves open: as in POSIX
    // strptime that year is 0, so %C=18 is 1800 whatever the hint says. Month and day,
    // which %C says nothing about at all, still come from the hint.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{1969}, month{7}, day{20}});
        const std::string in = "18";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, "%C"), in.end());
        EXPECT_EQ((ctx_to<year_month_day>(ctx)), (year_month_day{year{1800}, month{7}, day{20}}));
    }

    // %C together with %y still combines the two into a full year.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{1969}, month{7}, day{20}});
        const std::string in = "18 24";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, "%C %y"), in.end());
        EXPECT_EQ(ctx_to<year_month_day>(ctx).year(), year{1824});
    }

    // A hinted day that cannot exist in the parsed month gives way to the month's last
    // day instead of failing the conversion: the hint only fills in what the format
    // string is silent about, so it must not break an otherwise well-formed parse.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{2020}, month{1}, day{31}});
        const std::string in = "02";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, 'm', 0), in.end());
        EXPECT_EQ((ctx_to<year_month_day>(ctx)), (year_month_day{year{2020}, month{2}, day{29}}));
    }

    // The same when it is the year that is parsed and the hinted day is February 29.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{2020}, month{2}, day{29}});
        const std::string in = "2021";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, 'Y', 0), in.end());
        EXPECT_EQ((ctx_to<year_month_day>(ctx)), (year_month_day{year{2021}, month{2}, day{28}}));
    }

    // A day that fits needs no adjustment.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{2020}, month{1}, day{31}});
        const std::string in = "03";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, 'm', 0), in.end());
        EXPECT_EQ((ctx_to<year_month_day>(ctx)), (year_month_day{year{2020}, month{3}, day{31}}));
    }

    // A day that really was parsed does not give way: February 31 stays invalid and the
    // conversion reports it.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{2020}, month{2}, day{15}});
        const std::string in = "31";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, 'd', 0), in.end());
        EXPECT_THROW((void)ctx_to<year_month_day>(ctx), stream_error);
    }

    // set_hint(hh_mm_ss): unparsed time fields keep the hint.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(hh_mm_ss{hours{13} + minutes{45} + seconds{7}});
        const std::string in = "22";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, 'H', 0), in.end());
        auto hms = ctx_to<hh_mm_ss<seconds>>(ctx);
        EXPECT_TRUE(hms.hours() == hours{22} && hms.minutes() == minutes{45}
               && hms.seconds() == seconds{7});
    }

    // Any duration precision is accepted; finer than a second truncates toward zero.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(hh_mm_ss{milliseconds{(1 * 3600 + 2 * 60 + 3) * 1000 + 999}});
        auto hms = ctx_to<hh_mm_ss<seconds>>(ctx);
        EXPECT_TRUE(hms.hours() == hours{1} && hms.minutes() == minutes{2}
               && hms.seconds() == seconds{3});
    }

    // A negative hint wraps into the day rather than producing garbage components.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(hh_mm_ss{-(hours{1} + minutes{2} + seconds{3})});
        auto hms = ctx_to<hh_mm_ss<seconds>>(ctx);
        EXPECT_TRUE(hms.hours() == hours{22} && hms.minutes() == minutes{57}
               && hms.seconds() == seconds{57});
    }

    // A hint beyond one day is reduced modulo a day.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(hh_mm_ss{hours{49} + minutes{1}});
        auto hms = ctx_to<hh_mm_ss<seconds>>(ctx);
        EXPECT_TRUE(hms.hours() == hours{1} && hms.minutes() == minutes{1});
    }

    // set_hint(const time_zone*) replaces the UTC fallback when %Z was never parsed,
    // and nullptr restores it.
    {
        time_parse_context<char> ctx;
        EXPECT_EQ(ctx_to<const time_zone*>(ctx), locate_zone("UTC"));
        ctx.set_hint(locate_zone("Asia/Shanghai"));
        EXPECT_EQ(ctx_to<const time_zone*>(ctx), locate_zone("Asia/Shanghai"));
        ctx.set_hint(static_cast<const time_zone*>(nullptr));
        EXPECT_EQ(ctx_to<const time_zone*>(ctx), locate_zone("UTC"));
    }

    // A parsed %Z wins over the zone hint.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(locate_zone("Asia/Shanghai"));
        const std::string in = "America/Los_Angeles";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, 'Z', 0), in.end());
        EXPECT_EQ(ctx_to<const time_zone*>(ctx), locate_zone("America/Los_Angeles"));
    }

    // reset() keeps its "restore to default-constructed" meaning: every hint is wiped,
    // so the date falls back to the current year again.
    {
        time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{1969}, month{7}, day{20}});
        ctx.set_hint(hh_mm_ss{hours{13} + minutes{45}});
        ctx.set_hint(locate_zone("Asia/Shanghai"));
        ctx.reset();
        EXPECT_EQ(ctx, time_parse_context<char>{});
        EXPECT_EQ(ctx_to<const time_zone*>(ctx), locate_zone("UTC"));

        auto now_year = year_month_day{floor<days>(system_clock::now())}.year();
        EXPECT_EQ(ctx_to<year_month_day>(ctx).year(), now_year);
    }

    // The setters are constrained, not silently ignored, when a field group is inactive.
    // The checks go through concepts rather than `static_assert(!requires ...)`, which GCC
    // reports as a hard error in a non-template context instead of a failed constraint.
    {
        static_assert(can_hint_date<time_parse_context<char, true, true, tz_level::zone>>);
        static_assert(can_hint_time<time_parse_context<char, true, true, tz_level::zone>>);
        static_assert(can_hint_zone<time_parse_context<char, true, true, tz_level::zone>>);
        static_assert(!can_hint_date<time_parse_context<char, false, true, tz_level::zone>>);
        static_assert(!can_hint_time<time_parse_context<char, true, false, tz_level::zone>>);
        static_assert(!can_hint_zone<time_parse_context<char, true, true, tz_level::none>>);
    }
}

// A format string ending in a lone '%' -- or in a lone '%E' / '%O' modifier -- introduces no
// specifier, so there is nothing to convert. It follows the same rule this facet already uses
// for a specifier it does not recognize (see the "unknown format" path, which emits '%' plus
// the rest verbatim): put writes the '%' out and get matches it back as a literal. Handling
// the two sides alike is what keeps the round-trip invariant -- whatever put writes, get reads
// back with the same format string. put previously dropped the '%' silently while get rejected
// it, so put succeeded on output get could never read.
TEST(TimeioChar, ALoneOrUnknownSpecifierIsEchoedVerbatim)
{
    timeio obj(std::make_shared<timeio_conf<char>>("C"));
    const std::tm t = calendar_time(124, 0, 15, 1, 2, 3, 1, 14, 0);

    struct { const char* fmt; const char* want; } cases[] = {
        {"%Y%", "2024%"},   // a lone '%' after a real specifier
        {"%",   "%"},       // nothing but the lone '%'
        {"a%",  "a%"},      // a lone '%' after literal text
        {"%E",  "%E"},      // a lone 'E' modifier with no specifier to modify
        {"%O",  "%O"},      // ditto for 'O'
        {"%%",  "%"},       // control: an escaped '%' still collapses to one
        {"%Q",  "%Q"},      // control: an unrecognized specifier is already emitted verbatim
    };

    for (const auto& c : cases)
    {
        std::string res;
        obj.put(std::back_inserter(res), t, std::string_view(c.fmt));
        EXPECT_EQ(res, c.want);

        // The round trip: get consumes exactly what put produced, using the same format.
        time_parse_context<char> ctx;
        EXPECT_EQ(obj.get(res.begin(), res.end(), ctx, std::string_view(c.fmt)), res.end());
    }

    // get still rejects input that lacks the literal '%' the format asks for, so the
    // agreement above is a real match rather than the trailing '%' being ignored.
    {
        const std::string in = "2024";
        time_parse_context<char> ctx;
        EXPECT_THROW(obj.get(in.begin(), in.end(), ctx, std::string_view("%Y%")), stream_error);
    }
}

// A format string that came from the locale rather than from the caller is normalized on the way
// in (timeio_conf<char>::normalize_time_format), because the verbatim-echo rule exercised by
// put_19 is wrong for it: the caller wrote only "%c" or "%x", so echoing a specifier this facet
// does not implement turns a gap here into corrupt user-visible output -- and get reads that
// echo back as a literal, so the round trip silently loses the field. glibc's locale data uses
// extensions this facet does not implement ("%-d" no-pad, "%l", "%k", "%P").
//
// Part A pins the set of specifiers the facet implements, which is what the normalizer keeps.
// It derives the set from the facet's own behaviour rather than restating it: an unimplemented
// specifier is echoed verbatim (put_19), so "output == format" means "unsupported". If a
// specifier is ever added to or removed from put/get without the normalizer's table following,
// this half fails.
//
// Part B then asserts no locale's composite formats can reach put holding anything outside that
// set. It can only check locales this machine has installed, so it reports how many it saw --
// a run where only "C" is present proves little, and says so.
TEST(TimeioChar, TheLocaleCompositeFormatsHoldOnlySupportedSpecifiers)
{
    // %Z and %z need a value that carries a zone; with a std::tm they degrade to a literal by
    // design, which part A would misread as "unsupported".
    const auto zt = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    const std::string supported = "%ABCDFGHIMRSTUVWXYZabcdeghjmnprtuwxyz";

    auto emits = [&](char spec) {
        timeio obj(std::make_shared<timeio_conf<char>>("C"));
        const std::string fmt = std::string("%") + spec;
        std::string res;
        obj.put(std::back_inserter(res), zt, std::string_view(fmt));
        return res != fmt;
    };

    // Part A.
    for (char spec : supported)
        EXPECT_TRUE(emits(spec));

    // Controls: characters outside the set must still be echoed, or "echoed == unsupported"
    // would be vacuous and part A would pass for any set at all.
    for (char spec : std::string("QLfikloqsv"))
        EXPECT_FALSE(emits(spec));

    // Part B.
    const char* const names[] = {
        "C", "C.UTF-8", "en_US.UTF-8", "zh_CN.UTF-8", "de_DE.UTF-8", "de_CH.UTF-8",
        "fr_CA.UTF-8", "ps_AF.UTF-8", "tr_TR.UTF-8", "en_HK.UTF-8", "zh_CN.GBK",
        "cs_CZ.ISO-8859-2", "ru_RU.KOI8-R",
    };

    // Rejects a format string holding any sequence outside '%' [E|O]? <supported>.
    auto all_supported = [&](const std::string& fmt) {
        for (std::size_t i = 0; i < fmt.size(); ++i)
        {
            if (fmt[i] != '%') continue;
            std::size_t j = i + 1;
            if (j < fmt.size() && (fmt[j] == 'E' || fmt[j] == 'O')) ++j;
            if (j >= fmt.size()) return false;
            if (supported.find(fmt[j]) == std::string::npos) return false;
            i = j;
        }
        return true;
    };

    int checked = 0;
    for (const char* name : names)
    {
        std::shared_ptr<timeio_conf<char>> conf;
        try { conf = std::make_shared<timeio_conf<char>>(name); }
        catch (...) { continue; }   // not installed here, or its data is self-contradictory
        ++checked;

        EXPECT_TRUE(all_supported(conf->date_format()));
        EXPECT_TRUE(all_supported(conf->era_date_format()));
        EXPECT_TRUE(all_supported(conf->time_format()));
        EXPECT_TRUE(all_supported(conf->era_time_format()));
        EXPECT_TRUE(all_supported(conf->date_time_format()));
        EXPECT_TRUE(all_supported(conf->era_date_time_format()));
        EXPECT_TRUE(all_supported(conf->am_pm_format()));
    }
    EXPECT_TRUE(checked >= 2);   // C and C.UTF-8 are always there; more is better

    // The control for part B: the checker must reject what the locales are being cleared of.
    EXPECT_FALSE(all_supported("%-d.%-m.%Y"));
    EXPECT_FALSE(all_supported("%l:%M %P"));
    EXPECT_FALSE(all_supported("%k:%M"));
    EXPECT_TRUE(all_supported("%d.%m.%Y"));
    EXPECT_TRUE(all_supported("%EY %Oe"));

    RecordProperty("candidate_locales_seen", checked);
}

// put(sys_time) and put(local_time, offset): both carry an offset, and they part company on the
// zone. %z always emits for either; %Z has "UTC" for a sys_time, which is why that type reads
// back at tz_level::zone, and nothing at all for a local_time, which is what tz_level::offset
// exists for -- there %Z degrades to a literal on both sides.
TEST(TimeioChar, AZoneIsWrittenFromWhatTheValueCarries)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char>>("C"));
    std::string res;

    const sys_time<seconds> st{
        sys_days{year{2024}/month{9}/day{4}} + hours{13} + minutes{33} + seconds{18}};
    const local_time<seconds> lt{
        local_days{year{2024}/month{9}/day{4}} + hours{13} + minutes{33} + seconds{18}};

    {
        res.clear(); obj.put(std::back_inserter(res), st, std::string_view("%F %T %z %Z"));
        EXPECT_EQ(res, "2024-09-04 13:33:18 +0000 UTC");
    }

    // %Z is the abbreviation the value supplies, not an IANA name: only a zoned_time has one.
    {
        auto zt = create_zoned_time(2024, 9, 4, 13, 33, 18, "Etc/UTC");
        res.clear(); obj.put(std::back_inserter(res), zt, std::string_view("%Z"));
        EXPECT_EQ(res, "Etc/UTC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), lt, hours{9}, std::string_view("%F %T %z %Z"));
        EXPECT_EQ(res, "2024-09-04 13:33:18 +0900 %Z");   // %Z degrades: no abbreviation to write

        res.clear();
        obj.put(std::back_inserter(res), lt, -(hours{5} + minutes{30}), std::string_view("%z"));
        EXPECT_EQ(res, "-0530");

        // %z has no room for seconds, so a sub-minute offset truncates towards zero. The sign
        // is written before the truncation, which is why a small negative offset reads "-0000".
        res.clear(); obj.put(std::back_inserter(res), lt, seconds{59}, std::string_view("%z"));
        EXPECT_EQ(res, "+0000");
        res.clear(); obj.put(std::back_inserter(res), lt, seconds{-59}, std::string_view("%z"));
        EXPECT_EQ(res, "-0000");
    }

    // A date outside year's range is rejected rather than clamped, as it is for the other values.
    {
        EXPECT_THROW(obj.put(std::back_inserter(res),
                             sys_time<seconds>{sys_days{year::max()/month{12}/day{31}} + days{1}},
                             std::string_view("%F")),
                     stream_error);
        EXPECT_THROW(obj.put(std::back_inserter(res),
                             local_time<seconds>{local_days{year::max()/month{12}/day{31}} + days{1}},
                             seconds{0}, std::string_view("%F")),
                     stream_error);
    }

    // Round trips through the offset tier: what put writes, get reads back to the same instant.
    {
        res.clear(); obj.put(std::back_inserter(res), st, std::string_view("%F %T %z"));
        EXPECT_EQ(res, "2024-09-04 13:33:18 +0000");

        time_parse_context<char, true, true, tz_level::offset> ctx;
        EXPECT_EQ(obj.get(res.begin(), res.end(), ctx, std::string_view("%F %T %z")), res.end());
        sys_time<seconds> back{};
        ctx.convert_to(back);
        EXPECT_EQ(back, st);
    }
    {
        res.clear(); obj.put(std::back_inserter(res), lt, hours{8}, std::string_view("%F %T %z"));
        EXPECT_EQ(res, "2024-09-04 13:33:18 +0800");

        time_parse_context<char, true, true, tz_level::offset> ctx;
        EXPECT_EQ(obj.get(res.begin(), res.end(), ctx, std::string_view("%F %T %z")), res.end());
        sys_time<seconds>   back_sys{};
        local_time<seconds> back_local{};
        ctx.convert_to(back_sys);
        ctx.convert_to(back_local);
        EXPECT_EQ(back_local, lt);         // the wall time, offset dropped
        EXPECT_EQ(back_sys, st - hours{8}); // the instant, offset applied
    }
}

// The tz_level::zone tier as a std::tm reaches it: %z and %Z both parse, the offset and the
// zone text are kept verbatim, and convert_to(std::tm&) writes both back.
TEST(TimeioChar, AZoneOrOffsetIsParsedIntoTheContext)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char>>("C"));

    using ctx_zone = time_parse_context<char, true, true, tz_level::zone>;
    auto parse = [&obj](const std::string& in, const char* fmt)
    {
        ctx_zone ctx;
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, std::string_view(fmt)), in.end());
        return ctx;
    };
    auto rejects = [&obj](const std::string& in, const char* fmt)
    {
        ctx_zone ctx;
        try { obj.get(in.begin(), in.end(), ctx, std::string_view(fmt)); }
        catch (stream_error&) { return true; }
        return false;
    };

    // All four spellings %z accepts.
    EXPECT_EQ(parse("+0800", "%z").m_offset, minutes{480});
    EXPECT_EQ(parse("-0530", "%z").m_offset, -minutes{330});
    EXPECT_EQ(parse("+08", "%z").m_offset, minutes{480});
    EXPECT_EQ(parse("+08:30", "%z").m_offset, minutes{510});
    EXPECT_EQ(parse("Z", "%z").m_offset, minutes{0});
    EXPECT_TRUE(parse("Z", "%z").m_have_offset);
    EXPECT_EQ(parse("-0000", "%z").m_offset, minutes{0});

    // A ':' not followed by a digit is put back rather than swallowed, so the two-digit form
    // still matches and the ':' is left for the format string to consume as a literal.
    EXPECT_EQ(parse("+08:", "%z:").m_offset, minutes{480});

    EXPECT_TRUE(rejects("0800", "%z"));      // no sign
    EXPECT_TRUE(rejects("+8", "%z"));        // one digit
    EXPECT_TRUE(rejects("+080", "%z"));      // three digits
    EXPECT_TRUE(rejects("+2400", "%z"));     // hour out of range
    EXPECT_TRUE(rejects("+0060", "%z"));     // minute out of range
    EXPECT_TRUE(rejects("+", "%z"));         // sign then nothing

    // %Z keeps the text and resolves nothing at this tier, but it does file it by what the
    // tzdb says it is: a name the database knows lands in m_zone_name, a bare abbreviation in
    // m_zone_abbrev, and the eight tokens that are both land in both.
    { auto r = parse("America/Los_Angeles", "%Z");
      EXPECT_TRUE(zone_is(r.m_zone_name, "America/Los_Angeles") && r.m_zone_abbrev == nullptr); }
    { auto r = parse("PST", "%Z");
      EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    { auto r = parse("EST", "%Z");
      EXPECT_TRUE(zone_is(r.m_zone_name, "EST") && zone_is(r.m_zone_abbrev, "EST")); }

    // Link names parse too -- locate_zone accepts them, so the trie carries them.
    EXPECT_TRUE(zone_is(parse("US/Pacific", "%Z").m_zone_name, "US/Pacific"));
    EXPECT_TRUE(zone_is(parse("Asia/Calcutta", "%Z").m_zone_name, "Asia/Calcutta"));
    EXPECT_TRUE(zone_is(parse("Japan", "%Z").m_zone_name, "Japan"));

    // The unknown-zone token is a fourth case: it parses, but it names nothing. The text is
    // empty rather than absent, which is what lets convert_to(std::tm&) blank tm_zone.
    { auto r = parse("UNKNOWN", "%Z");
      EXPECT_TRUE(r.m_zone_name == nullptr && r.m_zone_abbrev != nullptr && *r.m_zone_abbrev == '\0'); }
    EXPECT_TRUE(rejects("America/Los_Angexes", "%Z"));

    // convert_to(minutes): the parsed offset first, then the hint, and an error with neither.
    {
        minutes off{};
        parse("+0800", "%z").convert_to(off);
        EXPECT_EQ(off, minutes{480});

        ctx_zone ctx;
        EXPECT_THROW(ctx.convert_to(off), stream_error);
        ctx.set_hint(minutes{-60});
        ctx.convert_to(off);
        EXPECT_EQ(off, minutes{-60});
    }

    // The offset reaches a std::tm through tm_gmtoff and the zone through tm_zone, where the
    // platform has those members. tm_zone can be written because the text it points at lives
    // in the time-zone trie, which outlives every parse -- the field has no release call, so
    // nothing shorter-lived may go in it.
    if constexpr (requires (std::tm t) { t.tm_gmtoff; })
    {
        std::tm out{};
        parse("2024-09-04 13:33:18 +0800 PST", "%F %T %z %Z").convert_to(out);
        EXPECT_EQ(out.tm_gmtoff, 8 * 3600);
        if constexpr (requires (std::tm t) { t.tm_zone; })
        {
            EXPECT_TRUE(zone_is(out.tm_zone, "PST"));

            // A zone name goes in as readily as an abbreviation: put wrote tm_zone out
            // verbatim, so get has to put it back the same way.
            std::tm named{};
            parse("2024-09-04 13:33:18 +0800 America/Los_Angeles", "%F %T %z %Z").convert_to(named);
            EXPECT_TRUE(zone_is(named.tm_zone, "America/Los_Angeles"));

            // The unknown-zone token blanks the field rather than leaving it alone. Leaving it
            // would keep a stale name the text just said was not there, and the next put would
            // write that name back out.
            std::tm blanked{};
            blanked.tm_zone = "STALE";
            parse("2024-09-04 13:33:18 +0800 UNKNOWN", "%F %T %z %Z").convert_to(blanked);
            EXPECT_TRUE(zone_is(blanked.tm_zone, ""));

            // No %Z at all is the third state, and the only one that leaves the field be.
            std::tm untouched{};
            untouched.tm_zone = "KEPT";
            parse("2024-09-04 13:33:18 +0800", "%F %T %z").convert_to(untouched);
            EXPECT_TRUE(zone_is(untouched.tm_zone, "KEPT"));

            // put -> get -> put closes for all three.
            for (const char* z : {"PST", "America/Los_Angeles", ""})
            {
                std::tm t{};
                t.tm_year = 124; t.tm_mon = 8; t.tm_mday = 4;
                t.tm_hour = 13;  t.tm_min = 33; t.tm_sec = 18;
                t.tm_gmtoff = 8 * 3600;
                t.tm_zone = z;

                std::string once;
                obj.put(std::back_inserter(once), t, std::string_view("%F %T %z %Z"));

                std::tm back{};
                parse(once, "%F %T %z %Z").convert_to(back);

                std::string twice;
                obj.put(std::back_inserter(twice), back, std::string_view("%F %T %z %Z"));
                EXPECT_EQ(once, twice);
            }
        }

        // Only a *parsed* %z is written back. With none, the field keeps whatever the caller
        // had there -- an offset hint does not reach it either.
        std::tm keep{};
        keep.tm_gmtoff = 12345;
        ctx_zone ctx;
        ctx.set_hint(minutes{60});
        const std::string in = "2024-09-04 13:33:18";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T")), in.end());
        ctx.convert_to(keep);
        EXPECT_EQ(keep.tm_gmtoff, 12345);

        // The round trip io_manip.h documents for a std::tm.
        std::tm src{};
        src.tm_year = 124; src.tm_mon = 8; src.tm_mday = 4;
        src.tm_hour = 13; src.tm_min = 33; src.tm_sec = 18;
        src.tm_gmtoff = -7 * 3600;
        std::string res;
        obj.put(std::back_inserter(res), src, std::string_view("%F %T %z"));
        EXPECT_EQ(res, "2024-09-04 13:33:18 -0700");

        std::tm back{};
        ctx_zone ctx2;
        EXPECT_EQ(obj.get(res.begin(), res.end(), ctx2, std::string_view("%F %T %z")), res.end());
        ctx2.convert_to(back);
        EXPECT_EQ(back.tm_gmtoff, -7 * 3600);
    }
}

// convert_to(sys_time&) resolves the instant in a fixed order, and convert_to(local_time&)
// resolves nothing at all.
TEST(TimeioChar, AParsedZoneAndOffsetResolveToAnInstant)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char>>("C"));

    const sys_time<seconds>   noon_utc{sys_days{year{2024}/month{9}/day{4}} + hours{12}};
    const local_time<seconds> noon_local{local_days{year{2024}/month{9}/day{4}} + hours{12}};

    using ctx_zone = time_parse_context<char, true, true, tz_level::zone>;
    auto parse_zone = [&obj](const std::string& in, const char* fmt)
    {
        ctx_zone ctx;
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, std::string_view(fmt)), in.end());
        return ctx;
    };

    sys_time<seconds> st{};

    // 1. A parsed offset pins the instant.
    parse_zone("2024-09-04 12:00:00 +0800", "%F %T %z").convert_to(st);
    EXPECT_EQ(st, noon_utc - hours{8});

    // 2. With no offset, a parsed IANA name converts the wall time.
    parse_zone("2024-09-04 12:00:00 Asia/Tokyo", "%F %T %Z").convert_to(st);
    EXPECT_EQ(st, noon_utc - hours{9});

    // ...and it beats an offset hint, because parsed data outranks a fallback.
    {
        ctx_zone ctx;
        ctx.set_hint(minutes{120});
        const std::string in = "2024-09-04 12:00:00 Asia/Tokyo";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T %Z")), in.end());
        ctx.convert_to(st);
        EXPECT_EQ(st, noon_utc - hours{9});
    }

    // 3. Nothing parsed: the offset hint, which in turn beats the zone hint and the UTC default.
    {
        ctx_zone ctx;
        ctx.set_hint(minutes{120});
        ctx.set_hint(locate_zone("Asia/Tokyo"));
        const std::string in = "2024-09-04 12:00:00";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T")), in.end());
        ctx.convert_to(st);
        EXPECT_EQ(st, noon_utc - hours{2});
    }

    // 4. No offset hint either: the zone hint, and failing that UTC.
    {
        ctx_zone ctx;
        ctx.set_hint(locate_zone("Asia/Tokyo"));
        const std::string in = "2024-09-04 12:00:00";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T")), in.end());
        ctx.convert_to(st);
        EXPECT_EQ(st, noon_utc - hours{9});
    }
    {
        ctx_zone ctx;
        const std::string in = "2024-09-04 12:00:00";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T")), in.end());
        ctx.convert_to(st);
        EXPECT_EQ(st, noon_utc);
    }

    // At tz_level::offset there is no zone to fall back on, so a missing offset is an error
    // rather than an implicit UTC.
    {
        time_parse_context<char, true, true, tz_level::offset> ctx;
        const std::string in = "2024-09-04 12:00:00";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T")), in.end());
        EXPECT_THROW(ctx.convert_to(st), stream_error);
        ctx.set_hint(minutes{-240});
        ctx.convert_to(st);
        EXPECT_EQ(st, noon_utc + hours{4});
    }

    // The offset pins the instant and the zone must agree about it (D5). Disagreement is an
    // error, not a silent preference for one of the two.
    {
        zoned_time<seconds> zt{};
        parse_zone("2024-09-04 12:00:00 +0900 Asia/Tokyo", "%F %T %z %Z").convert_to(zt);
        EXPECT_EQ(zt.get_sys_time(), noon_utc - hours{9});

        EXPECT_THROW(parse_zone("2024-09-04 12:00:00 +0800 Asia/Tokyo", "%F %T %z %Z").convert_to(zt), stream_error);
        // An abbreviation that names no single zone is an error even with an offset present:
        // the offset does not excuse the ambiguity, it only would have pinned the instant.
        EXPECT_THROW(parse_zone("2024-09-04 12:00:00 -0700 PST", "%F %T %z %Z").convert_to(zt), stream_error);
    }

    // convert_to(local_time&) takes the wall time as parsed and drops the zone, at every tier.
    {
        local_time<seconds> lt{};
        parse_zone("2024-09-04 12:00:00 +0800", "%F %T %z").convert_to(lt);
        EXPECT_EQ(lt, noon_local);

        lt = {};
        parse_zone("2024-09-04 12:00:00 Asia/Tokyo", "%F %T %Z").convert_to(lt);
        EXPECT_EQ(lt, noon_local);

        lt = {};
        time_parse_context<char, true, true, tz_level::none> ctx;
        const std::string in = "2024-09-04 12:00:00";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T")), in.end());
        ctx.convert_to(lt);
        EXPECT_EQ(lt, noon_local);
    }
}

// The set of abbreviations %Z accepts. put copies std::tm::tm_zone out verbatim and
// localtime() can produce the abbreviation of any instant, so whatever put can write, get
// has to take back; the trie is built by walking every transition of every zone for exactly
// that reason. Sampling the database at one instant instead yields only the abbreviation in
// effect then, which is what these cases pin down.
TEST(TimeioChar, EveryZoneAbbreviationTheDatabaseKnowsIsParsed)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char>>("C"));

    auto parses = [&obj](const std::string& in)
    {
        time_parse_context<char, true, true, tz_level::zone> ctx;
        try { return obj.get(in.begin(), in.end(), ctx, std::string_view("%Z")) == in.end(); }
        catch (stream_error&) { return false; }
    };

    // Daylight-saving abbreviations, both hemispheres.
    EXPECT_TRUE(parses("PDT"));
    EXPECT_TRUE(parses("EDT"));
    EXPECT_TRUE(parses("CEST"));
    EXPECT_TRUE(parses("ACDT"));
    EXPECT_TRUE(parses("NZST"));

    // Retired decades ago, and still what localtime() reports for a 1943 instant.
    EXPECT_TRUE(parses("EWT"));

    // Standard-time abbreviations and full zone names.
    EXPECT_TRUE(parses("PST"));
    EXPECT_TRUE(parses("UTC"));
    EXPECT_TRUE(parses("America/Los_Angeles"));

    // The same invariant over the whole database rather than a hand-picked list.
    {
        const sys_seconds jan{sys_days{2024y / January / 15}};
        const sys_seconds jul{sys_days{2024y / July / 15}};
        int checked = 0;
        for (const auto& zone : get_tzdb().zones)
            for (auto instant : {jan, jul})
            {
                const std::string abbrev = zone.get_info(instant).abbrev;
                if (abbrev.empty()) continue;
                EXPECT_TRUE(parses(abbrev));
                ++checked;
            }
        EXPECT_TRUE(checked > 500);
    }

    // What must keep failing. Walking the whole history also admits two-letter entries from
    // the pre-war era, so a longest match can stop short of the input's end; whatever follows
    // %Z in the format still catches that, and this pins the cost down to %Z-at-the-end.
    EXPECT_FALSE(parses("XYZ"));
    {
        time_parse_context<char, true, true, tz_level::zone> ctx;
        const std::string in = "ATL 11:22";
        EXPECT_THROW(obj.get(in.begin(), in.end(), ctx, std::string_view("%Z %H:%M")), stream_error);
    }

    // Matching longest-first against known names is also what delimits %Z, which is why a
    // specifier may follow it with no separator. Scanning a character class could not: the
    // class has to hold '+' and '-' for abbreviations like "+08", and would swallow the %z.
    {
        time_parse_context<char, true, true, tz_level::zone> ctx;
        const std::string in = "UTC+0800";
        EXPECT_EQ(obj.get(in.begin(), in.end(), ctx, std::string_view("%Z%z")), in.end());
        EXPECT_EQ(ctx.m_offset, minutes{480});
    }

    // The round trip the abbreviation set exists for.
    if constexpr (requires (std::tm t) { t.tm_zone; t.tm_gmtoff; })
    {
        std::tm t{};
        t.tm_year = 43; t.tm_mon = 6; t.tm_mday = 1;
        t.tm_hour = 12; t.tm_min = 0; t.tm_sec = 0;
        t.tm_gmtoff = -4 * 3600;
        t.tm_zone = "EWT";

        std::string res;
        obj.put(std::back_inserter(res), t, std::string_view("%F %T %z %Z"));
        EXPECT_EQ(res, "1943-07-01 12:00:00 -0400 EWT");

        time_parse_context<char, true, true, tz_level::zone> ctx;
        EXPECT_EQ(obj.get(res.begin(), res.end(), ctx, std::string_view("%F %T %z %Z")), res.end());
        EXPECT_EQ(ctx.m_offset, -minutes{240});
    }
}

// The two tiers pinned apart. Whether %Z parses is the tier's decision and nothing else's:
// tz_level::offset matches it literally, which is exactly what put degrades it to for a value
// with no zone to name, and tz_level::zone parses it against the trie. Neither tier looks at
// what the trie happens to contain to decide which of the two it is doing.
TEST(TimeioChar, TheZoneTierDecidesHowAZoneNameIsParsed)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char>>("C"));

    auto off_ok = [&obj](const std::string& in, const char* fmt)
    {
        time_parse_context<char, true, true, tz_level::offset> ctx;
        try { return obj.get(in.begin(), in.end(), ctx, std::string_view(fmt)) == in.end(); }
        catch (stream_error&) { return false; }
    };
    auto zone_ok = [&obj](const std::string& in, const char* fmt)
    {
        time_parse_context<char, true, true, tz_level::zone> ctx;
        try { return obj.get(in.begin(), in.end(), ctx, std::string_view(fmt)) == in.end(); }
        catch (stream_error&) { return false; }
    };

    // The literal %Z, which is what put writes when the value has no zone to offer.
    EXPECT_TRUE(off_ok("%Z", "%Z"));
    EXPECT_FALSE(zone_ok("%Z", "%Z"));

    // A real zone token parses at tz_level::zone and only there. At tz_level::offset the format
    // is asking for the two characters %Z, which "UTC" is not -- put never writes a zone token
    // for a value that parses at that tier, so there is nothing to read back.
    EXPECT_TRUE(zone_ok("UTC", "%Z"));
    EXPECT_FALSE(off_ok("UTC", "%Z"));
    EXPECT_TRUE(zone_ok("PDT", "%Z"));
    EXPECT_FALSE(off_ok("PDT", "%Z"));

    // A run of letters the database does not know is rejected at both, for different reasons:
    // no trie entry at one tier, no literal match at the other.
    EXPECT_FALSE(zone_ok("XYZ", "%Z"));
    EXPECT_FALSE(off_ok("XYZ", "%Z"));

    // The literal is for *this* specifier, not for any percent sequence.
    EXPECT_FALSE(off_ok("%z", "%Z"));
    EXPECT_FALSE(off_ok("%Q", "%Z"));

    // The round trip it exists for: a std::tm with no zone, through a format carrying %Z. Each
    // platform reads it back at the tier its own std::tm sits at. With tm_zone the field exists
    // but names nothing, so put writes the unknown-zone token and the zone tier reads it back;
    // without the extension members the type has no zone at all, put degrades %Z to a literal,
    // and the tiers below zone match that literal. Either way it closes.
    {
        std::tm t{};
        t.tm_year = 124; t.tm_mon = 8; t.tm_mday = 4;
        t.tm_hour = 13; t.tm_min = 33; t.tm_sec = 18;

        std::string res;
        obj.put(std::back_inserter(res), t, std::string_view("%F %T %Z"));
#ifdef __USE_MISC
        EXPECT_EQ(res, "2024-09-04 13:33:18 UNKNOWN");
        EXPECT_TRUE(zone_ok(res, "%F %T %Z"));
#else
        EXPECT_EQ(res, "2024-09-04 13:33:18 %Z");
        EXPECT_TRUE(off_ok(res, "%F %T %Z"));
#endif
    }

    // The same round trip through a locale whose own %c carries %Z, which is how this reaches
    // a caller who never wrote %Z: put_time(&t, "%c") on a tm that get_time filled in.
    {
        timeio us(std::make_shared<timeio_conf<char>>("en_US.UTF-8"));
        std::tm t{};
        t.tm_year = 124; t.tm_mon = 8; t.tm_mday = 4;
        t.tm_hour = 13; t.tm_min = 33; t.tm_sec = 18;

        std::string res;
        us.put(std::back_inserter(res), t, std::string_view("%c"));

        time_parse_context<char, true, true, tz_level::zone> ctx;
        EXPECT_EQ(us.get(res.begin(), res.end(), ctx, std::string_view("%c")), res.end());
    }
}

// expand_format vs put: the same table, read twice. expand_and_filter's switch is a compile-time
// mirror of do_put's, and this is what holds the two together -- for every value type, every
// specifier and every modifier, expand_format drops exactly what put degrades to a literal.
// Without this the two could drift apart silently, since neither one calls the other.
TEST(TimeioChar, ExpandFormatAgreesWithWhatPutWrites)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char>>("C"));

    std::string specs = "%";
    for (char c = 'a'; c <= 'z'; ++c) specs += c;
    for (char c = 'A'; c <= 'Z'; ++c) specs += c;
    const std::string modifiers("\0EO", 3);

    // "%E" and "%O" are not a modifier plus a specifier but a format string cut short after the
    // modifier, which both sides pass through unchanged by design; they are asserted separately
    // below rather than swept here.
    auto truncated = [](char spec, char mod)
    { return mod == 0 && (spec == 'E' || spec == 'O'); };

    // put echoes '%', the modifier and the specifier character unchanged when the value cannot
    // supply it; expand_format is expected to return an empty string for exactly those.
    auto agree = [&](auto&& emit, char spec, char mod, const std::string& expanded) {
        std::string literal = "%";
        if (mod) literal += mod;
        literal += spec;

        std::string res;
        emit(res, literal);
        return expanded.empty() == (res == literal);
    };

    int checked = 0;

    // zoned_time: everything a value can carry, zone identity included.
    {
        const auto zt = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");
        for (char spec : specs)
            for (char mod : modifiers)
            {
                if (truncated(spec, mod)) continue;
                auto emit = [&](std::string& out, const std::string& fmt)
                { obj.put(std::back_inserter(out), zt, std::string_view(fmt)); };
                EXPECT_TRUE(agree(emit, spec, mod, obj.expand_format<decltype(zt)>(spec, mod)));
                ++checked;
            }
    }

    // sys_time: an instant in UTC. %z writes +0000 and %Z writes UTC, so neither is dropped.
    {
        const sys_time<seconds> st{
            sys_days{year{2024}/month{9}/day{4}} + hours{13} + minutes{33} + seconds{18}};
        for (char spec : specs)
            for (char mod : modifiers)
            {
                if (truncated(spec, mod)) continue;
                auto emit = [&](std::string& out, const std::string& fmt)
                { obj.put(std::back_inserter(out), st, std::string_view(fmt)); };
                EXPECT_TRUE(agree(emit, spec, mod, obj.expand_format<decltype(st)>(spec, mod)));
                ++checked;
            }
    }

    // local_time: an offset but no zone identity, so %z stays and %Z goes. The single-character
    // put cannot reach this overload (the offset is an extra argument), hence the string form.
    {
        const local_time<seconds> lt{
            local_days{year{2024}/month{9}/day{4}} + hours{13} + minutes{33} + seconds{18}};
        for (char spec : specs)
            for (char mod : modifiers)
            {
                if (truncated(spec, mod)) continue;
                auto emit = [&](std::string& out, const std::string& fmt)
                { obj.put(std::back_inserter(out), lt, hours{-8}, std::string_view(fmt)); };
                EXPECT_TRUE(agree(emit, spec, mod, obj.expand_format<decltype(lt)>(spec, mod)));
                ++checked;
            }
    }

    // year_month_day: date and weekday only.
    {
        const year_month_day ymd{year{2024}/month{9}/day{4}};
        for (char spec : specs)
            for (char mod : modifiers)
            {
                if (truncated(spec, mod)) continue;
                auto emit = [&](std::string& out, const std::string& fmt)
                { obj.put(std::back_inserter(out), ymd, std::string_view(fmt)); };
                EXPECT_TRUE(agree(emit, spec, mod, obj.expand_format<year_month_day>(spec, mod)));
                ++checked;
            }
    }

    // hh_mm_ss: time of day only.
    {
        const hh_mm_ss<seconds> hms{hours{13} + minutes{33} + seconds{18}};
        for (char spec : specs)
            for (char mod : modifiers)
            {
                if (truncated(spec, mod)) continue;
                auto emit = [&](std::string& out, const std::string& fmt)
                { obj.put(std::back_inserter(out), hms, std::string_view(fmt)); };
                EXPECT_TRUE(agree(emit, spec, mod, obj.expand_format<decltype(hms)>(spec, mod)));
                ++checked;
            }
    }

    // std::tm: the zone fields exist only if the platform's tm carries them. tm_zone is filled
    // in here because expand_format judges the type -- for a tm whose tm_zone is empty put
    // degrades %Z while expand_format keeps it, which is the documented difference.
    {
        std::tm t{};
        t.tm_year = 124; t.tm_mon = 8; t.tm_mday = 4;
        t.tm_hour = 13; t.tm_min = 33; t.tm_sec = 18;
#ifdef __USE_MISC
        t.tm_gmtoff = -28800;
        t.tm_zone = "PST";
#endif
        for (char spec : specs)
            for (char mod : modifiers)
            {
                if (truncated(spec, mod)) continue;
                auto emit = [&](std::string& out, const std::string& fmt)
                { obj.put(std::back_inserter(out), t, std::string_view(fmt)); };
                EXPECT_TRUE(agree(emit, spec, mod, obj.expand_format<std::tm>(spec, mod)));
                ++checked;
            }
    }

    EXPECT_EQ(checked, 6 * (53 * 3 - 2));

    // The pair held out of the sweep: a format cut short after its modifier is passed through
    // unchanged on both sides, so expand_format keeps it rather than reading past the end.
    EXPECT_EQ(obj.expand_format<year_month_day>("%E"), "%E");
    EXPECT_EQ(obj.expand_format<year_month_day>("%O"), "%O");
    EXPECT_EQ(obj.expand_format<year_month_day>("%Y %E"), "%Y %E");
    {
        std::string res;
        obj.put(std::back_inserter(res), year_month_day{year{2024}/month{9}/day{4}},
                std::string_view("%E"));
        EXPECT_EQ(res, "%E");
    }

    // The control: the cross-check would pass vacuously if expand_format simply kept everything,
    // so pin down that it really does drop, and really does keep.
    EXPECT_TRUE(obj.expand_format<hh_mm_ss<seconds>>('Y').empty());
    EXPECT_TRUE(obj.expand_format<year_month_day>('H').empty());
    EXPECT_TRUE(obj.expand_format<local_time<seconds>>('Z').empty());
    EXPECT_FALSE(obj.expand_format<local_time<seconds>>('z').empty());
    EXPECT_FALSE(obj.expand_format<year_month_day>('Y').empty());
    EXPECT_FALSE(obj.expand_format<hh_mm_ss<seconds>>('H').empty());
}

// The two halves of expand_format that the specifier-by-specifier cross-check cannot see:
// compound specifiers being replaced by their contents, and a dropped specifier taking one
// adjacent separator with it.
TEST(TimeioChar, ExpandFormatResolvesTheCompositeSpecifiers)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char>>("C"));

    using LT = local_time<seconds>;
    using ST = sys_time<seconds>;
    using HMS = hh_mm_ss<seconds>;

    // Fixed compounds expand whole, and vanish whole when the value cannot supply them.
    EXPECT_EQ(obj.expand_format<year_month_day>("%F"), "%Y-%m-%d");
    EXPECT_EQ(obj.expand_format<year_month_day>("%D"), "%m/%d/%y");
    EXPECT_EQ(obj.expand_format<HMS>("%T"), "%H:%M:%S");
    EXPECT_EQ(obj.expand_format<HMS>("%R"), "%H:%M");
    EXPECT_EQ(obj.expand_format<HMS>("%F %T"), "%H:%M:%S");
    EXPECT_EQ(obj.expand_format<year_month_day>("%F %T"), "%Y-%m-%d");

    // An unsuppliable compound is dropped whole rather than expanded, or its contents would
    // come back as a trail of orphaned punctuation.
    EXPECT_TRUE(obj.expand_format<HMS>("%c").empty());
    EXPECT_TRUE(obj.expand_format<HMS>("%x").empty());
    EXPECT_TRUE(obj.expand_format<year_month_day>("%c").empty());
    EXPECT_TRUE(obj.expand_format<year_month_day>("%X").empty());

    // Locale compounds expand to what the locale actually holds, expanded in turn.
    EXPECT_EQ(obj.expand_format<ST>("%X"), obj.expand_format<ST>(obj.time_format()));
    EXPECT_EQ(obj.expand_format<ST>("%x"), obj.expand_format<ST>(obj.date_format()));

    // Separators: a dropped specifier must not leave its punctuation behind.
    EXPECT_EQ(obj.expand_format<HMS>("%T %Z"), "%H:%M:%S");
    EXPECT_EQ(obj.expand_format<HMS>("%m/%d/%Y %T"), "%H:%M:%S");
    EXPECT_EQ(obj.expand_format<HMS>("%Y-%m-%d"), "");
    EXPECT_EQ(obj.expand_format<LT>("%T %Z"), "%H:%M:%S");
    EXPECT_EQ(obj.expand_format<LT>("%T %z"), "%H:%M:%S %z");

    // Only ASCII-ish punctuation and whitespace count as separators; a letter never does, so a
    // CJK unit character is not eaten along with the field it follows.
    EXPECT_EQ(obj.expand_format<LT>("%S\xe7\xa7\x92 %Z"), "%S\xe7\xa7\x92");

    // Bracket groups are all-or-nothing: a group emptied by filtering takes its brackets with
    // it, a group that keeps something keeps them, and brackets the format left unpaired or
    // empty on its own are never touched.
    EXPECT_EQ(obj.expand_format<HMS>("%T (%Z)"), "%H:%M:%S");
    EXPECT_EQ(obj.expand_format<HMS>("%T [%Z]"), "%H:%M:%S");
    EXPECT_EQ(obj.expand_format<HMS>("%T {%Z}"), "%H:%M:%S");
    EXPECT_EQ(obj.expand_format<HMS>("%T (%Z, %z)"), "%H:%M:%S");
    EXPECT_EQ(obj.expand_format<HMS>("%T ((%Z))"), "%H:%M:%S");
    EXPECT_EQ(obj.expand_format<HMS>("%T (%Z, x)"), "%H:%M:%S (x)");
    EXPECT_EQ(obj.expand_format<HMS>("%T (a (%Z) b)"), "%H:%M:%S (a b)");
    EXPECT_EQ(obj.expand_format<HMS>("%T ()"), "%H:%M:%S ()");
    EXPECT_EQ(obj.expand_format<HMS>("%T :-)"), "%H:%M:%S :-)");
    EXPECT_EQ(obj.expand_format<HMS>("(x) %Z"), "(x)");

    // A lone trailing % is kept, matching put; %% is a literal and never a specifier.
    EXPECT_EQ(obj.expand_format<year_month_day>("a%"), "a%");
    EXPECT_EQ(obj.expand_format<year_month_day>("%%Z"), "%%Z");
    EXPECT_EQ(obj.expand_format<year_month_day>("%%"), "%%");

    // What the whole thing was built for: a locale whose %c carries a %Z, run against a value
    // that has no zone identity to put in it.
    {
        timeio us(std::make_shared<timeio_conf<char>>("en_US.UTF-8"));

        const std::string zoned = us.expand_format<ST>("%c");
        const std::string bare  = us.expand_format<LT>("%c");

        // The premise of the test: this locale's %c really does reach a %Z.
        EXPECT_TRUE(timeio<char>::contains_specifier(us.date_time_format(), 'Z') ||
               timeio<char>::contains_specifier(us.am_pm_format(), 'Z'));

        EXPECT_TRUE(timeio<char>::contains_specifier(zoned, 'Z'));
        EXPECT_FALSE(timeio<char>::contains_specifier(bare, 'Z'));

        // Nothing but the zone field and its separator differ between the two.
        EXPECT_TRUE(bare.size() < zoned.size());
        EXPECT_EQ(zoned.compare(0, bare.size(), bare), 0);

        // And the expansion is a format string that really works: putting through it must not
        // leave a literal %Z in the output.
        const local_time<seconds> lt{
            local_days{year{2024}/month{9}/day{4}} + hours{13} + minutes{33} + seconds{18}};
        std::string res;
        us.put(std::back_inserter(res), lt, hours{-8}, std::string_view(bare));
        EXPECT_EQ(res.find('%'), std::string::npos);
    }
}

// contains_specifier: the question a caller asks of an expansion before deciding to append a
// field of their own. A plain find() answers it wrongly, which is why this exists.
TEST(TimeioChar, ExpandFormatKeepsAZoneSpecifierAValueCanSupply)
{
    using namespace std::chrono;

    using tio = timeio<char>;

    EXPECT_TRUE(tio::contains_specifier("%Z", 'Z'));
    EXPECT_TRUE(tio::contains_specifier("a %T %Z b", 'Z'));
    EXPECT_FALSE(tio::contains_specifier("xZ", 'Z'));
    EXPECT_FALSE(tio::contains_specifier("", 'Z'));

    // The trap: "%%Z" is a literal % followed by a literal Z, and every further % flips it back.
    EXPECT_FALSE(tio::contains_specifier("%%Z", 'Z'));
    EXPECT_TRUE(tio::contains_specifier("%%%Z", 'Z'));
    EXPECT_FALSE(tio::contains_specifier("%%%%Z", 'Z'));
    EXPECT_TRUE(tio::contains_specifier("%%%%%Z", 'Z'));

    // The control: a find() would report a hit on all four of those.
    EXPECT_NE(std::string_view("%%Z").find("%Z"), std::string_view::npos);

    // Modifiers are part of the identity, not decoration.
    EXPECT_TRUE(tio::contains_specifier("%EY", 'Y', 'E'));
    EXPECT_FALSE(tio::contains_specifier("%EY", 'Y'));
    EXPECT_FALSE(tio::contains_specifier("%Y", 'Y', 'E'));
    EXPECT_FALSE(tio::contains_specifier("%OY", 'Y', 'E'));
    EXPECT_TRUE(tio::contains_specifier("%OY %EY", 'Y', 'E'));

    // A truncated tail is not a match, and does not read past the end.
    EXPECT_FALSE(tio::contains_specifier("%", 'Z'));
    EXPECT_FALSE(tio::contains_specifier("%E", 'Y', 'E'));
    EXPECT_FALSE(tio::contains_specifier("abc%", 'Z'));

    // The flow it exists for: expand, ask, append.
    {
        timeio us(std::make_shared<timeio_conf<char>>("en_US.UTF-8"));
        using LT = local_time<seconds>;

        std::string fmt = us.expand_format<LT>("%c");
        EXPECT_FALSE(tio::contains_specifier(fmt, 'Z'));
        fmt += " %Z";
        EXPECT_TRUE(tio::contains_specifier(fmt, 'Z'));
    }
}

TEST(TimeioChar, AnUnknownZoneIsATokenOfItsOwn)
{
    using namespace std::chrono;
    using tio = timeio<char>;

    timeio obj(std::make_shared<timeio_conf<char>>("C"));

    // The token the put side writes when the field exists but names nothing.
    EXPECT_EQ(ft_basic<tio>::s_unknown_zone, "UNKNOWN");

    std::tm tp = calendar_time(2024 - 1900, 9 - 1, 4, 13, 33, 18, 3, 247, 0);

    // Whatever put writes for %Z must parse back through the trie: that is the whole
    // reason the token is registered there rather than being print-only.
    {
        std::string res;
        obj.put(std::back_inserter(res), tp, "%Y-%m-%d %H:%M:%S %Z");
        EXPECT_EQ(res, "2024-09-04 13:33:18 UNKNOWN");

        time_parse_context<char, true, true, tz_level::zone> ctx;
        auto it = obj.get(res.cbegin(), res.cend(), ctx, "%Y-%m-%d %H:%M:%S %Z");
        EXPECT_EQ(it, res.cend());

        std::tm out{};
        out.tm_zone = "PRESET";
        ctx.convert_to(out);
        EXPECT_EQ(out.tm_year, 2024 - 1900);
        EXPECT_TRUE(out.tm_hour == 13 && out.tm_min == 33 && out.tm_sec == 18);

        // Parsing UNKNOWN records no zone, but it does not leave tm_zone alone either: the
        // text said outright that there is no zone, so the preset name is cleared rather
        // than surviving to be written back out by the next put.
#ifdef __USE_MISC
        EXPECT_TRUE(out.tm_zone != nullptr && *out.tm_zone == '\0');

        // And that is what closes the loop -- putting the parsed tm back reproduces the
        // token, where keeping "PRESET" would have written that name instead.
        std::string again;
        obj.put(std::back_inserter(again), out, "%Y-%m-%d %H:%M:%S %Z");
        EXPECT_EQ(again, res);
#endif
    }

    // A real abbreviation still round-trips, and is not swallowed by the new branch.
#ifdef __USE_MISC
    {
        std::tm named = tp;
        named.tm_zone = "PST";

        std::string res;
        obj.put(std::back_inserter(res), named, "%H:%M:%S %Z");
        EXPECT_EQ(res, "13:33:18 PST");

        time_parse_context<char, false, true, tz_level::zone> ctx;
        auto it = obj.get(res.cbegin(), res.cend(), ctx, "%H:%M:%S %Z");
        EXPECT_EQ(it, res.cend());
    }
#endif

    // expand_format keeps %Z for std::tm and drops it for the zone-less types, and that
    // claim now matches put exactly: every specifier it keeps, put can fill.
    {
        timeio us(std::make_shared<timeio_conf<char>>("en_US.UTF-8"));

        const std::string tm_fmt = us.expand_format<std::tm>("%c");
        const std::string lt_fmt = us.expand_format<local_time<seconds>>("%c");

#ifdef __USE_MISC
        EXPECT_TRUE(tio::contains_specifier(tm_fmt, 'Z'));
#endif
        EXPECT_FALSE(tio::contains_specifier(lt_fmt, 'Z'));

        // Nothing survives the filter that put would degrade: no literal % in the output.
        std::string res;
        us.put(std::back_inserter(res), tp, std::string_view(tm_fmt));
        EXPECT_EQ(res.find('%'), std::string::npos);
#ifdef __USE_MISC
        EXPECT_NE(res.find("UNKNOWN"), std::string::npos);
#endif
    }
}

TEST(TimeioChar, ACompositeFormatCannotExpandIntoItself)
{
    using namespace std::chrono;

    // The unrigged conf is accepted -- as is every real locale the other tests build, which
    // is the standing proof that this check does not reject actual locale data.
    EXPECT_FALSE(rejects([](rigged_conf&) {}));

    // Direct self-reference, one per compound. Each of these is the D_T_FMT == "%c" bug.
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_dt     = "%c";  }));
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_era_dt = "%Ec"; }));
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_d      = "%x";  }));
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_era_d  = "%Ex"; }));
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_t      = "%X";  }));
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_era_t  = "%EX"; }));
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_r      = "%r";  }));

    // The specifier need not sit alone, and brackets do not shield it -- a group's content
    // is expanded like anything else.
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_dt = "%Y-%m-%d %c"; }));
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_dt = "[%c]"; }));

    // Indirect cycles: two hops, three hops, and one routed through %r.
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_d = "%X"; c.m_t = "%x"; }));
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_dt = "%x"; c.m_d = "%X"; c.m_t = "%c"; }));
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_t = "%r"; c.m_r = "%X"; }));

    // The era and non-era tables are separate nodes, so a cycle can run through both.
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_dt = "%Ex"; c.m_era_d = "%c"; }));

    // %EY expands the matching era's format, so an era format naming itself is a cycle...
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_eras = {one_era("%EY")}; }));
    // ...and so is one that gets back to %EY through a locale compound.
    EXPECT_TRUE(rejects([](rigged_conf& c) { c.m_dt = "%EY"; c.m_eras = {one_era("%c")}; }));
    // An era format that terminates is fine, even though %EY reaches it.
    EXPECT_FALSE(rejects([](rigged_conf& c) { c.m_dt = "%EY"; c.m_eras = {one_era("%Y")}; }));

    // Non-cycles a sloppier scan would flag. %%c is an escaped percent plus a literal c;
    // %Oc / %Ox / %OX / %Er / %Or degrade to literals in put and get and never recurse; a
    // trailing % or bare modifier has no specifier at all.
    EXPECT_FALSE(rejects([](rigged_conf& c) { c.m_dt = "%%c"; }));
    EXPECT_FALSE(rejects([](rigged_conf& c) { c.m_dt = "%Oc"; }));
    EXPECT_FALSE(rejects([](rigged_conf& c) { c.m_d  = "%Ox"; }));
    EXPECT_FALSE(rejects([](rigged_conf& c) { c.m_t  = "%OX"; }));
    EXPECT_FALSE(rejects([](rigged_conf& c) { c.m_r  = "%Er"; }));
    EXPECT_FALSE(rejects([](rigged_conf& c) { c.m_r  = "%Or"; }));
    EXPECT_FALSE(rejects([](rigged_conf& c) { c.m_dt = "%";   }));
    EXPECT_FALSE(rejects([](rigged_conf& c) { c.m_dt = "%E";  }));

    // A DAG with a shared node: %c reaches %r through both %x and %X. A two-colour DFS
    // would call the second arrival at %r a cycle; the grey/black split is what keeps a
    // diamond legal.
    EXPECT_FALSE(rejects([](rigged_conf& c)
    {
        c.m_dt = "%x %X";
        c.m_d  = "%r";
        c.m_t  = "%r";
        c.m_r  = "%H:%M";
    }));

    // A rigged but acyclic conf still works end to end, and expand_format shows the chain
    // was really followed rather than merely tolerated.
    {
        auto conf = std::make_shared<rigged_conf>();
        conf->m_dt = "%x @ %X";
        conf->m_d  = "%Y-%m-%d";
        conf->m_t  = "%r";
        conf->m_r  = "%H:%M";

        timeio<char> obj(conf);

        const sys_time<seconds> st{
            sys_days{year{2024}/month{9}/day{4}} + hours{13} + minutes{33} + seconds{18}};

        EXPECT_EQ(obj.expand_format<sys_time<seconds>>("%c"), "%Y-%m-%d @ %H:%M");

        std::string res;
        obj.put(std::back_inserter(res), st, std::string_view("%c"));
        EXPECT_EQ(res, "2024-09-04 @ 13:33");
    }
}

TEST(TimeioChar, AnOffsetOutsideItsRangeIsRejected)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char>>("C"));

    // %z can only spell four digits of +/-hhmm, so an offset it cannot express is pinned
    // to the widest one the parse side accepts rather than being rejected or truncated.
    constexpr long max_off = 23L * 3600 + 59 * 60 + 59;   // 23:59:59

    auto put_z = [&](long gmtoff)
    {
        std::tm tp = calendar_time(2024 - 1900, 9 - 1, 4, 13, 33, 18, 3, 247, 0);
        tp.tm_gmtoff = gmtoff;
        tp.tm_zone = "CST";
        std::string res;
        obj.put(std::back_inserter(res), tp, "%z");
        return res;
    };

    // In range, %z is exact to the minute.
    EXPECT_EQ(put_z(0), "+0000");
    EXPECT_EQ(put_z(3600), "+0100");
    EXPECT_EQ(put_z(-19800), "-0530");
    EXPECT_EQ(put_z(max_off), "+2359");
    EXPECT_EQ(put_z(-max_off), "-2359");

    // One second past the bound, and far past it, both clamp. 400 hours used to come out
    // as "+0000" because only the low four digits of hhmm survived.
    EXPECT_EQ(put_z(86400), "+2359");
    EXPECT_EQ(put_z(-86400), "-2359");
    EXPECT_EQ(put_z(86400L * 400), "+2359");
    EXPECT_EQ(put_z(-86400L * 400), "-2359");

    // The extremes are what made the old code negate INT_MIN.
    EXPECT_EQ(put_z(std::numeric_limits<int>::max()), "+2359");
    EXPECT_EQ(put_z(std::numeric_limits<int>::min()), "-2359");
    EXPECT_EQ(put_z(std::numeric_limits<long>::max()), "+2359");
    EXPECT_EQ(put_z(std::numeric_limits<long>::min()), "-2359");

    // 2^31 narrows to INT_MIN, so clamping after the cast instead of before would print a
    // positive offset with a minus sign. This is the case that pins the order.
    EXPECT_EQ(put_z(2147483648L), "+2359");
    EXPECT_EQ(put_z(-2147483648L), "-2359");

    // The specifier has minute resolution: seconds are dropped, not rejected. Historical
    // LMT offsets really do carry them (Europe/Amsterdam was +00:19:32).
    EXPECT_EQ(put_z(1172), "+0019");
    EXPECT_EQ(put_z(-1172), "-0019");

    // Whatever put writes has to parse back, which is what fixes the bound at 23:59:59.
    for (long off : {0L, 3600L, -19800L, max_off, -max_off, 86400L, 2147483648L})
    {
        const std::string text = put_z(off);
        time_parse_context<char, true, true, tz_level::zone> ctx;
        auto it = obj.get(text.cbegin(), text.cend(), ctx, "%z");
        EXPECT_EQ(it, text.cend());

        std::tm out{};
        ctx.convert_to(out);
        const long clamped = off > max_off ? max_off : off < -max_off ? -max_off : off;
        EXPECT_EQ(out.tm_gmtoff, (clamped / 60) * 60);
    }

    // The local_time overload takes its offset straight from the caller, so it needs the
    // same guard; seconds is 64-bit there, which is how a value wraps to the wrong sign.
    auto put_z_local = [&](seconds off)
    {
        std::string res;
        obj.put(std::back_inserter(res),
                local_time<seconds>{seconds{1725456798}}, off,
                std::basic_string_view<char>{"%z"});
        return res;
    };
    EXPECT_EQ(put_z_local(seconds{0}), "+0000");
    EXPECT_EQ(put_z_local(seconds{3600}), "+0100");
    EXPECT_EQ(put_z_local(seconds{2147483648LL}), "+2359");
    EXPECT_EQ(put_z_local(seconds{-2147483648LL}), "-2359");
    EXPECT_EQ(put_z_local(seconds{4294967296LL}), "+2359");
}
