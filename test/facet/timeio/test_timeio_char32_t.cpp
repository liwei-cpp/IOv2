// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same conversion-specifier contract as test_timeio_char.cpp for char32_t.  The
 * tables are the same tables: what a specifier produces depends on the locale
 * and on what the value can supply, not on the type the field is written in,
 * and these cases are here to say that the instantiation changes none of it.
 *
 * The cases that read the C library's words, the locale database or the zone
 * trie see the same data whatever the character type is, so they stay in the
 * narrow file.
 */
#include <IOv2/facet/timeio.h>
#include <IOv2/facet/timeio_details.h>

#include <IOv2/common/defs.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/streambuf.h>
#include <IOv2/io/streambuf_iterator.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <ctime>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

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

    timeio<char32_t> facet_for(const char* loc)
    {
        return timeio<char32_t>(std::make_shared<timeio_conf<char32_t>>(loc));
    }

    template <typename TVal, typename... TSpec>
    std::u32string put_one(const timeio<char32_t>& obj, const TVal& tp, TSpec... spec)
    {
        std::u32string res;
        obj.put(std::back_inserter(res), tp, spec...);
        return res;
    }

    // One conversion specifier, the modifier applied to it, and what the facet
    // writes.  A specifier the value cannot supply comes back as the format text
    // that asked for it, which is why so many rows read "%Ea" and the like.
    struct conversion
    {
        char32_t    spec;
        char32_t    mod;
        const char32_t* expected;
    };

    template <typename TVal, std::size_t N>
    void expect_conversions(const timeio<char32_t>& obj, const TVal& tp, const conversion (&table)[N])
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
    template <typename T = time_parse_context<char32_t>, bool HaveDate = true, bool HaveTime = true,
              tz_level TzLevel = tz_level::zone, typename... TFmt>
    T run_get(const timeio<char32_t>& obj, const std::u32string& input,
              ios_defs::iostate err_exp, TFmt... fmt)
    {
        time_parse_context<char32_t, HaveDate, HaveTime, TzLevel> ctx1, ctx2, ctx3;
        std::list<char32_t> lst(input.begin(), input.end());
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
    template <typename T = time_parse_context<char32_t>, bool HaveDate = true, bool HaveTime = true,
              tz_level TzLevel = tz_level::zone>
    T CheckGet(const timeio<char32_t>& obj, const std::u32string& input, char fmt, char modif,
               ios_defs::iostate err_exp)
    {
        SCOPED_TRACE(::testing::PrintToString(input) + " | %"
                     + (modif ? std::string(1, static_cast<char>(modif)) : std::string())
                     + static_cast<char>(fmt));
        return run_get<T, HaveDate, HaveTime, TzLevel>(obj, input, err_exp, fmt, modif);
    }

    template <typename T = time_parse_context<char32_t>, bool HaveDate = true, bool HaveTime = true,
              tz_level TzLevel = tz_level::zone>
    T CheckGet(const timeio<char32_t>& obj, const std::u32string& input, const std::u32string& fmt,
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
TEST(TimeioChar32, CLocaleWritesEveryConversionSpecifier)
{
    const timeio<char32_t> obj = facet_for("C");
    const auto             tp  = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    static const conversion kConversions[] = {
        {U'%', 0, U"%"},
        {U'a', 0, U"Wed"},
        {U'a', U'E', U"%Ea"},
        {U'a', U'O', U"%Oa"},
        {U'A', 0, U"Wednesday"},
        {U'A', U'E', U"%EA"},
        {U'A', U'O', U"%OA"},
        {U'b', 0, U"Sep"},
        {U'b', U'E', U"%Eb"},
        {U'b', U'O', U"%Ob"},
        {U'h', 0, U"Sep"},
        {U'h', U'E', U"%Eh"},
        {U'h', U'O', U"%Oh"},
        {U'B', 0, U"September"},
        {U'B', U'E', U"%EB"},
        {U'B', U'O', U"%OB"},
        {U'c', 0, U"Wed Sep  4 13:33:18 2024"},
        {U'c', U'E', U"Wed Sep  4 13:33:18 2024"},
        {U'c', U'O', U"%Oc"},
        {U'C', 0, U"20"},
        {U'C', U'E', U"20"},
        {U'C', U'O', U"%OC"},
        {U'x', 0, U"09/04/24"},
        {U'x', U'E', U"09/04/24"},
        {U'x', U'O', U"%Ox"},
        {U'D', 0, U"09/04/24"},
        {U'D', U'E', U"%ED"},
        {U'D', U'O', U"%OD"},
        {U'd', 0, U"04"},
        {U'd', U'E', U"%Ed"},
        {U'd', U'O', U"04"},
        {U'e', 0, U" 4"},
        {U'e', U'E', U"%Ee"},
        {U'e', U'O', U" 4"},
        {U'F', 0, U"2024-09-04"},
        {U'F', U'E', U"%EF"},
        {U'F', U'O', U"%OF"},
        {U'H', 0, U"13"},
        {U'H', U'E', U"%EH"},
        {U'H', U'O', U"13"},
        {U'I', 0, U"01"},
        {U'I', U'E', U"%EI"},
        {U'I', U'O', U"01"},
        {U'j', 0, U"248"},
        {U'j', U'E', U"%Ej"},
        {U'j', U'O', U"%Oj"},
        {U'M', 0, U"33"},
        {U'M', U'E', U"%EM"},
        {U'M', U'O', U"33"},
        {U'm', 0, U"09"},
        {U'm', U'E', U"%Em"},
        {U'm', U'O', U"09"},
        {U'n', 0, U"\n"},
        {U'n', U'E', U"%En"},
        {U'n', U'O', U"%On"},
        {U'p', 0, U"PM"},
        {U'p', U'E', U"%Ep"},
        {U'p', U'O', U"%Op"},
        {U'R', 0, U"13:33"},
        {U'R', U'E', U"%ER"},
        {U'R', U'O', U"%OR"},
        {U'r', 0, U"01:33:18 PM"},
        {U'r', U'E', U"%Er"},
        {U'r', U'O', U"%Or"},
        {U'S', 0, U"18"},
        {U'S', U'E', U"%ES"},
        {U'S', U'O', U"18"},
        {U'X', 0, U"13:33:18"},
        {U'X', U'E', U"13:33:18"},
        {U'X', U'O', U"%OX"},
        {U'T', 0, U"13:33:18"},
        {U'T', U'E', U"%ET"},
        {U'T', U'O', U"%OT"},
        {U't', 0, U"\t"},
        {U't', U'E', U"%Et"},
        {U't', U'O', U"%Ot"},
        {U'u', 0, U"3"},
        {U'u', U'E', U"%Eu"},
        {U'u', U'O', U"3"},
        {U'U', 0, U"35"},
        {U'U', U'E', U"%EU"},
        {U'U', U'O', U"35"},
        {U'V', 0, U"36"},
        {U'V', U'E', U"%EV"},
        {U'V', U'O', U"36"},
        {U'g', 0, U"24"},
        {U'g', U'E', U"%Eg"},
        {U'g', U'O', U"%Og"},
        {U'G', 0, U"2024"},
        {U'G', U'E', U"%EG"},
        {U'G', U'O', U"%OG"},
        {U'W', 0, U"36"},
        {U'W', U'E', U"%EW"},
        {U'W', U'O', U"36"},
        {U'w', 0, U"3"},
        {U'w', U'E', U"%Ew"},
        {U'w', U'O', U"3"},
        {U'Y', 0, U"2024"},
        {U'Y', U'E', U"2024"},
        {U'Y', U'O', U"%OY"},
        {U'y', 0, U"24"},
        {U'y', U'E', U"24"},
        {U'y', U'O', U"24"},
        {U'Z', 0, U"America/Los_Angeles"},
        {U'Z', U'E', U"%EZ"},
        {U'Z', U'O', U"%OZ"},
        {U'z', 0, U"-0700"},
        {U'z', U'E', U"%Ez"},
        {U'z', U'O', U"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// The same instant and the same specifiers under zh_CN, where the words and the
// composite layouts differ but the rules about what a value can supply do not.
TEST(TimeioChar32, ChineseWritesEveryConversionSpecifier)
{
    const timeio<char32_t> obj = facet_for("zh_CN.UTF-8");
    const auto             tp  = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    static const conversion kConversions[] = {
        {U'%', 0, U"%"},
        {U'a', 0, U"三"},
        {U'a', U'E', U"%Ea"},
        {U'a', U'O', U"%Oa"},
        {U'A', 0, U"星期三"},
        {U'A', U'E', U"%EA"},
        {U'A', U'O', U"%OA"},
        {U'b', 0, U"9月"},
        {U'b', U'E', U"%Eb"},
        {U'b', U'O', U"%Ob"},
        {U'h', 0, U"9月"},
        {U'h', U'E', U"%Eh"},
        {U'h', U'O', U"%Oh"},
        {U'B', 0, U"九月"},
        {U'B', U'E', U"%EB"},
        {U'B', U'O', U"%OB"},
        {U'c', 0, U"2024年09月04日 星期三 13时33分18秒"},
        {U'c', U'E', U"2024年09月04日 星期三 13时33分18秒"},
        {U'c', U'O', U"%Oc"},
        {U'C', 0, U"20"},
        {U'C', U'E', U"20"},
        {U'C', U'O', U"%OC"},
        {U'x', 0, U"2024年09月04日"},
        {U'x', U'E', U"2024年09月04日"},
        {U'x', U'O', U"%Ox"},
        {U'D', 0, U"09/04/24"},
        {U'D', U'E', U"%ED"},
        {U'D', U'O', U"%OD"},
        {U'd', 0, U"04"},
        {U'd', U'E', U"%Ed"},
        {U'd', U'O', U"04"},
        {U'e', 0, U" 4"},
        {U'e', U'E', U"%Ee"},
        {U'e', U'O', U" 4"},
        {U'F', 0, U"2024-09-04"},
        {U'F', U'E', U"%EF"},
        {U'F', U'O', U"%OF"},
        {U'H', 0, U"13"},
        {U'H', U'E', U"%EH"},
        {U'H', U'O', U"13"},
        {U'I', 0, U"01"},
        {U'I', U'E', U"%EI"},
        {U'I', U'O', U"01"},
        {U'j', 0, U"248"},
        {U'j', U'E', U"%Ej"},
        {U'j', U'O', U"%Oj"},
        {U'M', 0, U"33"},
        {U'M', U'E', U"%EM"},
        {U'M', U'O', U"33"},
        {U'm', 0, U"09"},
        {U'm', U'E', U"%Em"},
        {U'm', U'O', U"09"},
        {U'n', 0, U"\n"},
        {U'n', U'E', U"%En"},
        {U'n', U'O', U"%On"},
        {U'p', 0, U"下午"},
        {U'p', U'E', U"%Ep"},
        {U'p', U'O', U"%Op"},
        {U'R', 0, U"13:33"},
        {U'R', U'E', U"%ER"},
        {U'R', U'O', U"%OR"},
        {U'r', 0, U"下午 01时33分18秒"},
        {U'r', U'E', U"%Er"},
        {U'r', U'O', U"%Or"},
        {U'S', 0, U"18"},
        {U'S', U'E', U"%ES"},
        {U'S', U'O', U"18"},
        {U'X', 0, U"13时33分18秒"},
        {U'X', U'E', U"13时33分18秒"},
        {U'X', U'O', U"%OX"},
        {U'T', 0, U"13:33:18"},
        {U'T', U'E', U"%ET"},
        {U'T', U'O', U"%OT"},
        {U't', 0, U"\t"},
        {U't', U'E', U"%Et"},
        {U't', U'O', U"%Ot"},
        {U'u', 0, U"3"},
        {U'u', U'E', U"%Eu"},
        {U'u', U'O', U"3"},
        {U'U', 0, U"35"},
        {U'U', U'E', U"%EU"},
        {U'U', U'O', U"35"},
        {U'V', 0, U"36"},
        {U'V', U'E', U"%EV"},
        {U'V', U'O', U"36"},
        {U'g', 0, U"24"},
        {U'g', U'E', U"%Eg"},
        {U'g', U'O', U"%Og"},
        {U'G', 0, U"2024"},
        {U'G', U'E', U"%EG"},
        {U'G', U'O', U"%OG"},
        {U'W', 0, U"36"},
        {U'W', U'E', U"%EW"},
        {U'W', U'O', U"36"},
        {U'w', 0, U"3"},
        {U'w', U'E', U"%Ew"},
        {U'w', U'O', U"3"},
        {U'Y', 0, U"2024"},
        {U'Y', U'E', U"2024"},
        {U'Y', U'O', U"%OY"},
        {U'y', 0, U"24"},
        {U'y', U'E', U"24"},
        {U'y', U'O', U"24"},
        {U'Z', 0, U"America/Los_Angeles"},
        {U'Z', U'E', U"%EZ"},
        {U'Z', U'O', U"%OZ"},
        {U'z', 0, U"-0700"},
        {U'z', U'E', U"%Ez"},
        {U'z', U'O', U"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// And under ja_JP, which is the locale with an era representation, so %EC, %Ey
// and %EY are the rows to look at here.
TEST(TimeioChar32, JapaneseWritesEveryConversionSpecifier)
{
    const timeio<char32_t> obj = facet_for("ja_JP.UTF-8");
    const auto             tp  = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    static const conversion kConversions[] = {
        {U'%', 0, U"%"},
        {U'a', 0, U"水"},
        {U'a', U'E', U"%Ea"},
        {U'a', U'O', U"%Oa"},
        {U'A', 0, U"水曜日"},
        {U'A', U'E', U"%EA"},
        {U'A', U'O', U"%OA"},
        {U'b', 0, U" 9月"},
        {U'b', U'E', U"%Eb"},
        {U'b', U'O', U"%Ob"},
        {U'h', 0, U" 9月"},
        {U'h', U'E', U"%Eh"},
        {U'h', U'O', U"%Oh"},
        {U'B', 0, U"9月"},
        {U'B', U'E', U"%EB"},
        {U'B', U'O', U"%OB"},
        {U'c', 0, U"2024年09月04日 13時33分18秒"},
        {U'c', U'E', U"令和6年09月04日 13時33分18秒"},
        {U'c', U'O', U"%Oc"},
        {U'C', 0, U"20"},
        {U'C', U'E', U"令和"},
        {U'C', U'O', U"%OC"},
        {U'x', 0, U"2024年09月04日"},
        {U'x', U'E', U"令和6年09月04日"},
        {U'x', U'O', U"%Ox"},
        {U'D', 0, U"09/04/24"},
        {U'D', U'E', U"%ED"},
        {U'D', U'O', U"%OD"},
        {U'd', 0, U"04"},
        {U'd', U'E', U"%Ed"},
        {U'd', U'O', U"四"},
        {U'e', 0, U" 4"},
        {U'e', U'E', U"%Ee"},
        {U'e', U'O', U"四"},
        {U'F', 0, U"2024-09-04"},
        {U'F', U'E', U"%EF"},
        {U'F', U'O', U"%OF"},
        {U'H', 0, U"13"},
        {U'H', U'E', U"%EH"},
        {U'H', U'O', U"十三"},
        {U'I', 0, U"01"},
        {U'I', U'E', U"%EI"},
        {U'I', U'O', U"一"},
        {U'j', 0, U"248"},
        {U'j', U'E', U"%Ej"},
        {U'j', U'O', U"%Oj"},
        {U'M', 0, U"33"},
        {U'M', U'E', U"%EM"},
        {U'M', U'O', U"三十三"},
        {U'm', 0, U"09"},
        {U'm', U'E', U"%Em"},
        {U'm', U'O', U"九"},
        {U'n', 0, U"\n"},
        {U'n', U'E', U"%En"},
        {U'n', U'O', U"%On"},
        {U'p', 0, U"午後"},
        {U'p', U'E', U"%Ep"},
        {U'p', U'O', U"%Op"},
        {U'R', 0, U"13:33"},
        {U'R', U'E', U"%ER"},
        {U'R', U'O', U"%OR"},
        {U'r', 0, U"午後01時33分18秒"},
        {U'r', U'E', U"%Er"},
        {U'r', U'O', U"%Or"},
        {U'S', 0, U"18"},
        {U'S', U'E', U"%ES"},
        {U'S', U'O', U"十八"},
        {U'X', 0, U"13時33分18秒"},
        {U'X', U'E', U"13時33分18秒"},
        {U'X', U'O', U"%OX"},
        {U'T', 0, U"13:33:18"},
        {U'T', U'E', U"%ET"},
        {U'T', U'O', U"%OT"},
        {U't', 0, U"\t"},
        {U't', U'E', U"%Et"},
        {U't', U'O', U"%Ot"},
        {U'u', 0, U"3"},
        {U'u', U'E', U"%Eu"},
        {U'u', U'O', U"三"},
        {U'U', 0, U"35"},
        {U'U', U'E', U"%EU"},
        {U'U', U'O', U"三十五"},
        {U'V', 0, U"36"},
        {U'V', U'E', U"%EV"},
        {U'V', U'O', U"三十六"},
        {U'g', 0, U"24"},
        {U'g', U'E', U"%Eg"},
        {U'g', U'O', U"%Og"},
        {U'G', 0, U"2024"},
        {U'G', U'E', U"%EG"},
        {U'G', U'O', U"%OG"},
        {U'W', 0, U"36"},
        {U'W', U'E', U"%EW"},
        {U'W', U'O', U"三十六"},
        {U'w', 0, U"3"},
        {U'w', U'E', U"%Ew"},
        {U'w', U'O', U"三"},
        {U'Y', 0, U"2024"},
        {U'Y', U'E', U"令和6年"},
        {U'Y', U'O', U"%OY"},
        {U'y', 0, U"24"},
        {U'y', U'E', U"6"},
        {U'y', U'O', U"二十四"},
        {U'Z', 0, U"America/Los_Angeles"},
        {U'Z', U'E', U"%EZ"},
        {U'Z', U'O', U"%OZ"},
        {U'z', 0, U"-0700"},
        {U'z', U'E', U"%Ez"},
        {U'z', U'O', U"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// A year_month_day is a date and nothing else, so every specifier that asks for a
// time of day or a zone comes back as the text that asked for it.
TEST(TimeioChar32, ADateWritesEveryConversionSpecifierItCanSupply)
{
    using namespace std::chrono;
    const timeio<char32_t>   obj = facet_for("ja_JP.UTF-8");
    const year_month_day tp{year{2024}, month{9}, day{4}};

    static const conversion kConversions[] = {
        {U'%', 0, U"%"},
        {U'a', 0, U"水"},
        {U'a', U'E', U"%Ea"},
        {U'a', U'O', U"%Oa"},
        {U'A', 0, U"水曜日"},
        {U'A', U'E', U"%EA"},
        {U'A', U'O', U"%OA"},
        {U'b', 0, U" 9月"},
        {U'b', U'E', U"%Eb"},
        {U'b', U'O', U"%Ob"},
        {U'h', 0, U" 9月"},
        {U'h', U'E', U"%Eh"},
        {U'h', U'O', U"%Oh"},
        {U'B', 0, U"9月"},
        {U'B', U'E', U"%EB"},
        {U'B', U'O', U"%OB"},
        {U'c', 0, U"%c"},
        {U'c', U'E', U"%Ec"},
        {U'c', U'O', U"%Oc"},
        {U'C', 0, U"20"},
        {U'C', U'E', U"令和"},
        {U'C', U'O', U"%OC"},
        {U'x', 0, U"2024年09月04日"},
        {U'x', U'E', U"令和6年09月04日"},
        {U'x', U'O', U"%Ox"},
        {U'D', 0, U"09/04/24"},
        {U'D', U'E', U"%ED"},
        {U'D', U'O', U"%OD"},
        {U'd', 0, U"04"},
        {U'd', U'E', U"%Ed"},
        {U'd', U'O', U"四"},
        {U'e', 0, U" 4"},
        {U'e', U'E', U"%Ee"},
        {U'e', U'O', U"四"},
        {U'F', 0, U"2024-09-04"},
        {U'F', U'E', U"%EF"},
        {U'F', U'O', U"%OF"},
        {U'H', 0, U"%H"},
        {U'H', U'E', U"%EH"},
        {U'H', U'O', U"%OH"},
        {U'I', 0, U"%I"},
        {U'I', U'E', U"%EI"},
        {U'I', U'O', U"%OI"},
        {U'j', 0, U"248"},
        {U'j', U'E', U"%Ej"},
        {U'j', U'O', U"%Oj"},
        {U'M', 0, U"%M"},
        {U'M', U'E', U"%EM"},
        {U'M', U'O', U"%OM"},
        {U'm', 0, U"09"},
        {U'm', U'E', U"%Em"},
        {U'm', U'O', U"九"},
        {U'n', 0, U"\n"},
        {U'n', U'E', U"%En"},
        {U'n', U'O', U"%On"},
        {U'p', 0, U"%p"},
        {U'p', U'E', U"%Ep"},
        {U'p', U'O', U"%Op"},
        {U'R', 0, U"%R"},
        {U'R', U'E', U"%ER"},
        {U'R', U'O', U"%OR"},
        {U'r', 0, U"%r"},
        {U'r', U'E', U"%Er"},
        {U'r', U'O', U"%Or"},
        {U'S', 0, U"%S"},
        {U'S', U'E', U"%ES"},
        {U'S', U'O', U"%OS"},
        {U'X', 0, U"%X"},
        {U'X', U'E', U"%EX"},
        {U'X', U'O', U"%OX"},
        {U'T', 0, U"%T"},
        {U'T', U'E', U"%ET"},
        {U'T', U'O', U"%OT"},
        {U't', 0, U"\t"},
        {U't', U'E', U"%Et"},
        {U't', U'O', U"%Ot"},
        {U'u', 0, U"3"},
        {U'u', U'E', U"%Eu"},
        {U'u', U'O', U"三"},
        {U'U', 0, U"35"},
        {U'U', U'E', U"%EU"},
        {U'U', U'O', U"三十五"},
        {U'V', 0, U"36"},
        {U'V', U'E', U"%EV"},
        {U'V', U'O', U"三十六"},
        {U'g', 0, U"24"},
        {U'g', U'E', U"%Eg"},
        {U'g', U'O', U"%Og"},
        {U'G', 0, U"2024"},
        {U'G', U'E', U"%EG"},
        {U'G', U'O', U"%OG"},
        {U'W', 0, U"36"},
        {U'W', U'E', U"%EW"},
        {U'W', U'O', U"三十六"},
        {U'w', 0, U"3"},
        {U'w', U'E', U"%Ew"},
        {U'w', U'O', U"三"},
        {U'Y', 0, U"2024"},
        {U'Y', U'E', U"令和6年"},
        {U'Y', U'O', U"%OY"},
        {U'y', 0, U"24"},
        {U'y', U'E', U"6"},
        {U'y', U'O', U"二十四"},
        {U'Z', 0, U"%Z"},
        {U'Z', U'E', U"%EZ"},
        {U'Z', U'O', U"%OZ"},
        {U'z', 0, U"%z"},
        {U'z', U'E', U"%Ez"},
        {U'z', U'O', U"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// An hh_mm_ss is the mirror image: it has a time of day and no date at all.
TEST(TimeioChar32, ATimeOfDayWritesEveryConversionSpecifierItCanSupply)
{
    using namespace std::chrono;
    const timeio<char32_t>    obj = facet_for("ja_JP.UTF-8");
    const hh_mm_ss<seconds> tp{hours{13} + minutes{33} + seconds{18}};

    static const conversion kConversions[] = {
        {U'%', 0, U"%"},
        {U'a', 0, U"%a"},
        {U'a', U'E', U"%Ea"},
        {U'a', U'O', U"%Oa"},
        {U'A', 0, U"%A"},
        {U'A', U'E', U"%EA"},
        {U'A', U'O', U"%OA"},
        {U'b', 0, U"%b"},
        {U'b', U'E', U"%Eb"},
        {U'b', U'O', U"%Ob"},
        {U'h', 0, U"%h"},
        {U'h', U'E', U"%Eh"},
        {U'h', U'O', U"%Oh"},
        {U'B', 0, U"%B"},
        {U'B', U'E', U"%EB"},
        {U'B', U'O', U"%OB"},
        {U'c', 0, U"%c"},
        {U'c', U'E', U"%Ec"},
        {U'c', U'O', U"%Oc"},
        {U'x', 0, U"%x"},
        {U'x', U'E', U"%Ex"},
        {U'x', U'O', U"%Ox"},
        {U'D', 0, U"%D"},
        {U'D', U'E', U"%ED"},
        {U'D', U'O', U"%OD"},
        {U'd', 0, U"%d"},
        {U'd', U'E', U"%Ed"},
        {U'd', U'O', U"%Od"},
        {U'e', 0, U"%e"},
        {U'e', U'E', U"%Ee"},
        {U'e', U'O', U"%Oe"},
        {U'F', 0, U"%F"},
        {U'F', U'E', U"%EF"},
        {U'F', U'O', U"%OF"},
        {U'H', 0, U"13"},
        {U'H', U'E', U"%EH"},
        {U'H', U'O', U"十三"},
        {U'I', 0, U"01"},
        {U'I', U'E', U"%EI"},
        {U'I', U'O', U"一"},
        {U'j', 0, U"%j"},
        {U'j', U'E', U"%Ej"},
        {U'j', U'O', U"%Oj"},
        {U'M', 0, U"33"},
        {U'M', U'E', U"%EM"},
        {U'M', U'O', U"三十三"},
        {U'm', 0, U"%m"},
        {U'm', U'E', U"%Em"},
        {U'm', U'O', U"%Om"},
        {U'n', 0, U"\n"},
        {U'n', U'E', U"%En"},
        {U'n', U'O', U"%On"},
        {U'p', 0, U"午後"},
        {U'p', U'E', U"%Ep"},
        {U'p', U'O', U"%Op"},
        {U'R', 0, U"13:33"},
        {U'R', U'E', U"%ER"},
        {U'R', U'O', U"%OR"},
        {U'r', 0, U"午後01時33分18秒"},
        {U'r', U'E', U"%Er"},
        {U'r', U'O', U"%Or"},
        {U'S', 0, U"18"},
        {U'S', U'E', U"%ES"},
        {U'S', U'O', U"十八"},
        {U'X', 0, U"13時33分18秒"},
        {U'X', U'E', U"13時33分18秒"},
        {U'X', U'O', U"%OX"},
        {U'T', 0, U"13:33:18"},
        {U'T', U'E', U"%ET"},
        {U'T', U'O', U"%OT"},
        {U't', 0, U"\t"},
        {U't', U'E', U"%Et"},
        {U't', U'O', U"%Ot"},
        {U'u', 0, U"%u"},
        {U'u', U'E', U"%Eu"},
        {U'u', U'O', U"%Ou"},
        {U'U', 0, U"%U"},
        {U'U', U'E', U"%EU"},
        {U'U', U'O', U"%OU"},
        {U'V', 0, U"%V"},
        {U'V', U'E', U"%EV"},
        {U'V', U'O', U"%OV"},
        {U'g', 0, U"%g"},
        {U'g', U'E', U"%Eg"},
        {U'g', U'O', U"%Og"},
        {U'G', 0, U"%G"},
        {U'G', U'E', U"%EG"},
        {U'G', U'O', U"%OG"},
        {U'W', 0, U"%W"},
        {U'W', U'E', U"%EW"},
        {U'W', U'O', U"%OW"},
        {U'w', 0, U"%w"},
        {U'w', U'E', U"%Ew"},
        {U'w', U'O', U"%Ow"},
        {U'Y', 0, U"%Y"},
        {U'Y', U'E', U"%EY"},
        {U'Y', U'O', U"%OY"},
        {U'y', 0, U"%y"},
        {U'y', U'E', U"%Ey"},
        {U'y', U'O', U"%Oy"},
        {U'Z', 0, U"%Z"},
        {U'Z', U'E', U"%EZ"},
        {U'Z', U'O', U"%OZ"},
        {U'z', 0, U"%z"},
        {U'z', U'E', U"%Ez"},
        {U'z', U'O', U"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// A std::tm carries both halves, so almost everything is available; what it does
// not carry is a zone, which the cases after this one take up.
TEST(TimeioChar32, ABrokenDownTimeWritesEveryConversionSpecifier)
{
    const timeio<char32_t> obj = facet_for("ja_JP.UTF-8");
    const std::tm          tp  = calendar_time(2024 - 1900, 9 - 1, 4, 13, 33, 18, 0, 0, 0);

    static const conversion kConversions[] = {
        {U'%', 0, U"%"},
        {U'a', 0, U"水"},
        {U'a', U'E', U"%Ea"},
        {U'a', U'O', U"%Oa"},
        {U'A', 0, U"水曜日"},
        {U'A', U'E', U"%EA"},
        {U'A', U'O', U"%OA"},
        {U'b', 0, U" 9月"},
        {U'b', U'E', U"%Eb"},
        {U'b', U'O', U"%Ob"},
        {U'h', 0, U" 9月"},
        {U'h', U'E', U"%Eh"},
        {U'h', U'O', U"%Oh"},
        {U'B', 0, U"9月"},
        {U'B', U'E', U"%EB"},
        {U'B', U'O', U"%OB"},
        {U'c', 0, U"2024年09月04日 13時33分18秒"},
        {U'c', U'E', U"令和6年09月04日 13時33分18秒"},
        {U'c', U'O', U"%Oc"},
        {U'C', 0, U"20"},
        {U'C', U'E', U"令和"},
        {U'C', U'O', U"%OC"},
        {U'x', 0, U"2024年09月04日"},
        {U'x', U'E', U"令和6年09月04日"},
        {U'x', U'O', U"%Ox"},
        {U'D', 0, U"09/04/24"},
        {U'D', U'E', U"%ED"},
        {U'D', U'O', U"%OD"},
        {U'd', 0, U"04"},
        {U'd', U'E', U"%Ed"},
        {U'd', U'O', U"四"},
        {U'e', 0, U" 4"},
        {U'e', U'E', U"%Ee"},
        {U'e', U'O', U"四"},
        {U'F', 0, U"2024-09-04"},
        {U'F', U'E', U"%EF"},
        {U'F', U'O', U"%OF"},
        {U'H', 0, U"13"},
        {U'H', U'E', U"%EH"},
        {U'H', U'O', U"十三"},
        {U'I', 0, U"01"},
        {U'I', U'E', U"%EI"},
        {U'I', U'O', U"一"},
        {U'j', 0, U"248"},
        {U'j', U'E', U"%Ej"},
        {U'j', U'O', U"%Oj"},
        {U'M', 0, U"33"},
        {U'M', U'E', U"%EM"},
        {U'M', U'O', U"三十三"},
        {U'm', 0, U"09"},
        {U'm', U'E', U"%Em"},
        {U'm', U'O', U"九"},
        {U'n', 0, U"\n"},
        {U'n', U'E', U"%En"},
        {U'n', U'O', U"%On"},
        {U'p', 0, U"午後"},
        {U'p', U'E', U"%Ep"},
        {U'p', U'O', U"%Op"},
        {U'R', 0, U"13:33"},
        {U'R', U'E', U"%ER"},
        {U'R', U'O', U"%OR"},
        {U'r', 0, U"午後01時33分18秒"},
        {U'r', U'E', U"%Er"},
        {U'r', U'O', U"%Or"},
        {U'S', 0, U"18"},
        {U'S', U'E', U"%ES"},
        {U'S', U'O', U"十八"},
        {U'X', 0, U"13時33分18秒"},
        {U'X', U'E', U"13時33分18秒"},
        {U'X', U'O', U"%OX"},
        {U'T', 0, U"13:33:18"},
        {U'T', U'E', U"%ET"},
        {U'T', U'O', U"%OT"},
        {U't', 0, U"\t"},
        {U't', U'E', U"%Et"},
        {U't', U'O', U"%Ot"},
        {U'u', 0, U"3"},
        {U'u', U'E', U"%Eu"},
        {U'u', U'O', U"三"},
        {U'U', 0, U"35"},
        {U'U', U'E', U"%EU"},
        {U'U', U'O', U"三十五"},
        {U'V', 0, U"36"},
        {U'V', U'E', U"%EV"},
        {U'V', U'O', U"三十六"},
        {U'g', 0, U"24"},
        {U'g', U'E', U"%Eg"},
        {U'g', U'O', U"%Og"},
        {U'G', 0, U"2024"},
        {U'G', U'E', U"%EG"},
        {U'G', U'O', U"%OG"},
        {U'W', 0, U"36"},
        {U'W', U'E', U"%EW"},
        {U'W', U'O', U"三十六"},
        {U'w', 0, U"3"},
        {U'w', U'E', U"%Ew"},
        {U'w', U'O', U"三"},
        {U'Y', 0, U"2024"},
        {U'Y', U'E', U"令和6年"},
        {U'Y', U'O', U"%OY"},
        {U'y', 0, U"24"},
        {U'y', U'E', U"6"},
        {U'y', U'O', U"二十四"},
        {U'Z', 0, U"UNKNOWN"},
        {U'Z', U'E', U"%EZ"},
        {U'Z', U'O', U"%OZ"},
        {U'z', 0, U"+0000"},
        {U'z', U'E', U"%Ez"},
        {U'z', U'O', U"%Oz"},
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

// %Z is the one specifier a std::tm answers from a field rather than from the
// calendar, and the field may be empty two different ways.  calendar_time leaves
// tm_zone null; a caller can just as well set it to "".  Neither names a zone.
TEST(TimeioChar32, ABrokenDownTimeNamesItsZoneOrSaysItCannot)
{
    const timeio<char32_t> obj = facet_for("ja_JP.UTF-8");
    const std::tm          tp  = calendar_time(2024 - 1900, 9 - 1, 4, 13, 33, 18, 0, 0, 0);

    EXPECT_EQ(put_one(obj, tp, U'Z'), U"UNKNOWN");
    EXPECT_EQ(put_one(obj, tp, U'z'), U"+0000");

#ifdef __USE_MISC
    std::tm named = tp;
    named.tm_zone = "PST";
    EXPECT_EQ(put_one(obj, named, U'Z'), U"PST");

    // An empty string is as nameless as a null pointer.
    named.tm_zone = "";
    EXPECT_EQ(put_one(obj, named, U'Z'), U"UNKNOWN");
#endif
}

// A format string is expanded one specifier at a time with the literal text
// between them passed through unchanged, so what it produces is exactly the
// concatenation of its pieces.  Stated that way the case needs no locale's words
// written down, and holds in every locale rather than in the one it was written
// for.
TEST(TimeioChar32, AFormatStringIsExpandedSpecifierBySpecifier)
{
    const auto tp = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    for (const char* loc : {"C", "de_DE.UTF-8", "en_HK.UTF-8", "fr_FR.UTF-8", "ja_JP.UTF-8"})
    {
        SCOPED_TRACE(loc);
        const timeio<char32_t> obj = facet_for(loc);

        EXPECT_EQ(put_one(obj, tp, std::u32string_view(U"%A, week %W of %B")),
                  put_one(obj, tp, U'A') + U", week " + put_one(obj, tp, U'W')
                                        + U" of " + put_one(obj, tp, U'B'));

        // Literal text alone, and a format that is nothing but literal text.
        EXPECT_EQ(put_one(obj, tp, std::u32string_view(U"[%Y]")), U"[" + put_one(obj, tp, U'Y') + U"]");
        EXPECT_EQ(put_one(obj, tp, std::u32string_view(U"no specifiers")), U"no specifiers");
    }
}

// put() writes through an iterator into whatever the caller supplied and returns
// where it stopped, so everything past that point has to be exactly as it was.
TEST(TimeioChar32, PutIntoAnExistingBufferReturnsWhereItStopped)
{
    const timeio<char32_t> obj = facet_for("C");
    const auto             tp  = create_zoned_time(1997, 6, 26, 12, 0, 0, "America/Los_Angeles");

    std::u32string buffer(50, U'.');
    const auto     end = obj.put(buffer.begin(), tp, std::u32string_view(U"%F %T"));
    EXPECT_EQ(std::u32string(buffer.begin(), end), U"1997-06-26 12:00:00");
    EXPECT_EQ(buffer.substr(19), std::u32string(31, U'.'));

    // The same for a single specifier, whose length the caller cannot know in
    // advance because it is a word the locale chose.
    std::u32string one(20, U'.');
    const auto     one_end = obj.put(one.begin(), tp, U'A');
    EXPECT_EQ(std::u32string(one.begin(), one_end), U"Thursday");
    EXPECT_EQ(one.substr(8), std::u32string(12, U'.'));
}

// The literal text in a format is part of what has to match: it is how the
// caller says which of several numbers is which.  Input past what the format
// asked for is left for whoever reads next.
TEST(TimeioChar32, AFormatStringMustMatchTheInputLiterally)
{
    const timeio<char32_t> obj = facet_for("C");

    const auto t = ctx_to<std::tm>(
        CheckGet(obj, U"on 2024-09-04 at 01:09:35", U"on %Y-%m-%d at %H:%M:%S", ios_defs::eofbit));
    EXPECT_EQ(t.tm_year, 124);
    EXPECT_EQ(t.tm_mon, 8);
    EXPECT_EQ(t.tm_mday, 4);
    EXPECT_EQ(t.tm_hour, 1);
    EXPECT_EQ(t.tm_min, 9);
    EXPECT_EQ(t.tm_sec, 35);

    // Literal text the input does not carry.
    CheckGet(obj, U"at 2024-09-04", U"on %Y-%m-%d", ios_defs::strfailbit);
    CheckGet(obj, U"2024-09-04", U"on %Y-%m-%d", ios_defs::strfailbit);

    // A '%' with nothing after it is not a specifier.
    CheckGet(obj, U"2024-09-04", U"%", ios_defs::strfailbit);

    // What the format did not ask for stays in the input.
    const auto rest = ctx_to<std::tm>(CheckGet(obj, U"2020  ", U"%Y", ios_defs::goodbit));
    EXPECT_EQ(rest.tm_year, 120);

    // A single specifier without a format string reads the same field.
    EXPECT_EQ(ctx_to<std::tm>(CheckGet(obj, U"2020", U'Y', 0, ios_defs::eofbit)).tm_year, 120);
}

// The words a locale writes are the words it reads.  Round-tripping through the
// facet's own output says that in every locale at once, without this file having
// to know how any of them spells a month.
TEST(TimeioChar32, TheNamesTheLocaleWritesAreTheNamesItReads)
{
    using namespace std::chrono;
    const year_month_day date{year{2014}, month{4}, day{14}};

    for (const char* loc : {"C", "de_DE.UTF-8", "es_ES.UTF-8", "fr_FR.UTF-8", "ja_JP.UTF-8"})
    {
        SCOPED_TRACE(loc);
        const timeio<char32_t> obj = facet_for(loc);

        for (const char32_t* fmt : {U"%A, %d. %B %Y", U"%a %d %b %Y", U"%A %j %Y"})
        {
            SCOPED_TRACE(::testing::PrintToString(fmt));
            const std::u32string written = put_one(obj, date, std::u32string_view(fmt));
            EXPECT_EQ(CheckGet<year_month_day>(obj, written, fmt, ios_defs::eofbit), date);
        }
    }
}

// The specifiers that carry only part of a date -- a week number and a weekday,
// a day of the year, a century and a two-digit year -- have to reassemble into
// the date they were written from.  Stated as a round trip it holds for every
// date rather than for a handful with hand-computed week numbers.
TEST(TimeioChar32, EveryDateReassemblesFromItsPartialSpecifiers)
{
    using namespace std::chrono;
    const timeio<char32_t> obj = facet_for("C");

    const char32_t* const formats[] = {
        U"%F", U"%Y-%m-%d", U"%d-%b-%Y", U"%C%y-%m-%d",
        U"%Y %U %w", U"%Y %W %w", U"%Y %W %a", U"%Y %U %A", U"%j %Y",
    };

    // 29 days apart, so the sweep lands on every weekday and crosses the turn of
    // each year, which is where the week-number rules disagree with each other.
    for (sys_days d = sys_days{2019y / January / 1}; d <= sys_days{2024y / December / 31};
         d += days{29})
    {
        const year_month_day date{d};
        for (const char32_t* fmt : formats)
        {
            SCOPED_TRACE(::testing::PrintToString(fmt) + " | "
                         + ::testing::PrintToString(put_one(obj, date, std::u32string_view(U"%F"))));
            const std::u32string written = put_one(obj, date, std::u32string_view(fmt));
            EXPECT_EQ(CheckGet<year_month_day>(obj, written, fmt, ios_defs::eofbit), date);
        }
    }
}

// %I is a clock face: it cannot tell noon from midnight on its own, and %p is
// what supplies the half of the day it belongs to.  Either order.
TEST(TimeioChar32, TheTwelveHourClockNeedsItsMeridiem)
{
    using namespace std::chrono;
    const timeio<char32_t> obj = facet_for("C");

    for (int hour = 0; hour < 24; ++hour)
    {
        SCOPED_TRACE(hour);
        const seconds          when = hours{hour} + minutes{5} + seconds{9};
        const hh_mm_ss<seconds> tp{when};

        for (const char32_t* fmt : {U"%I:%M:%S %p", U"%p%I:%M:%S", U"%r", U"%T"})
        {
            SCOPED_TRACE(::testing::PrintToString(fmt));
            const std::u32string written = put_one(obj, tp, std::u32string_view(fmt));
            const auto           back =
                CheckGet<hh_mm_ss<seconds>, false, true, tz_level::none>(obj, written, fmt,
                                                                         ios_defs::eofbit);
            EXPECT_EQ(back.to_duration(), when);
        }
    }

    // Without the meridiem the same field reads as the morning hour, because that
    // is the half of the day a clock face means when nothing says otherwise.
    const auto morning = ctx_to<std::tm>(CheckGet(obj, U"07:05:09", U"%I:%M:%S", ios_defs::eofbit));
    EXPECT_EQ(morning.tm_hour, 7);
}

TEST(TimeioChar32, ACompositeFormatCanExpandPastAConventionalStackBuffer)
{
    std::shared_ptr<timeio_conf<char32_t>> conf =
        std::make_shared<expanded_composite_conf<char32_t>>();
    timeio obj(conf);
    auto   zt = create_zoned_time(2022, 11, 17, 21, 47, 26, "America/Los_Angeles");

    std::u32string actual;
    obj.put(std::back_inserter(actual), zt, U'c');

    std::u32string expected(140, U'q');
    expected += U"-2022-11-17-21:47:26-";
    expected.append(20, U'z');

    EXPECT_GT(actual.size(), 128u);
    EXPECT_EQ(actual, expected);
}

TEST(TimeioChar32, TheCLocaleReadsEveryConversionSpecifier)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};

    timeio obj(std::make_shared<timeio_conf<char32_t>>("C"));
    CheckGet(obj, U"%",   U'%',  0,  ios_defs::eofbit);
    CheckGet(obj, U"x",   U'%',  0,  ios_defs::strfailbit);
    CheckGet(obj, U"%",   U'%', U'E', febit);
    CheckGet(obj, U"%E%", U'%', U'E', ios_defs::eofbit);
    CheckGet(obj, U"%",   U'%', U'O', febit);
    CheckGet(obj, U"%O%", U'%', U'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet(obj, U"Wed", U'a', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, U"%Ea", U'a', U'E', ios_defs::eofbit);
    CheckGet(obj, U"a",   U'a', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oa", U'a', U'O', ios_defs::eofbit);
    CheckGet(obj, U"a",   U'a', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"Wednesday", U'A', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, U"%EA", U'A', U'E', ios_defs::eofbit);
    CheckGet(obj, U"A",   U'A', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OA", U'A', U'O', ios_defs::eofbit);
    CheckGet(obj, U"A",   U'A', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"Sep", U'b', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, U"%Eb", U'b', U'E', ios_defs::eofbit);
    CheckGet(obj, U"b",   U'b', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Ob", U'b', U'O', ios_defs::eofbit);
    CheckGet(obj, U"b",   U'b', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"September", U'B', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, U"%EB", U'B', U'E', ios_defs::eofbit);
    CheckGet(obj, U"B",   U'B', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OB", U'B', U'O', ios_defs::eofbit);
    CheckGet(obj, U"B",   U'B', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"Sep", U'h', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, U"%Eh", U'h', U'E', ios_defs::eofbit);
    CheckGet(obj, U"h",   U'h', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oh", U'h', U'O', ios_defs::eofbit);
    CheckGet(obj, U"h",   U'h', U'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"Wed Sep  4 13:33:18 2024", U'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"Wed Sep  4 13:33:18 2024", U'c', U'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, U"c",   U'c', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oc", U'c', U'O', ios_defs::eofbit);
    CheckGet(obj, U"c",   U'c', U'O', ios_defs::strfailbit);


    EXPECT_EQ(CheckGet(obj, U"20", U'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(CheckGet(obj, U"20", U'C', U'E', ios_defs::eofbit).m_century, 20);
    CheckGet(obj, U"C",   U'C', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OC", U'C', U'O', ios_defs::eofbit);
    CheckGet(obj, U"C",   U'C', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"04", U'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, U"04", U'd', U'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, U"%Ed", U'd', U'E', ios_defs::eofbit);
    CheckGet(obj, U"d",   U'd', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"d",   U'd', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"4", U'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, U"4", U'e', U'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, U"%Ee", U'e', U'E', ios_defs::eofbit);
    CheckGet(obj, U"e",   U'e', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"e",   U'e', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024-09-04", U'F', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, U"%EF", U'F', U'E', ios_defs::eofbit);
    CheckGet(obj, U"F",   U'F', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OF", U'F', U'O', ios_defs::eofbit);
    CheckGet(obj, U"F",   U'F', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"09/04/24", U'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"09/04/24", U'x', U'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, U"x",   U'x', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Ox", U'x', U'O', ios_defs::eofbit);
    CheckGet(obj, U"x",   U'x', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"09/04/24", U'D', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, U"%ED", U'D', U'E', ios_defs::eofbit);
    CheckGet(obj, U"D",   U'D', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OD", U'D', U'O', ios_defs::eofbit);
    CheckGet(obj, U"D",   U'D', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"13", U'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, U"13", U'H', U'O', ios_defs::eofbit).m_hour, 13);
    CheckGet(obj, U"%EH", U'H', U'E', ios_defs::eofbit);
    CheckGet(obj, U"H",   U'H', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"H",   U'H', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"01", U'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, U"01", U'I', U'O', ios_defs::eofbit).m_hour, 1);
    CheckGet(obj, U"%EI", U'I', U'E', ios_defs::eofbit);
    CheckGet(obj, U"I",   U'I', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"I",   U'I', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"248", U'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 248", U"%Y %j", ios_defs::eofbit), check_date1);
    CheckGet(obj, U"%Ej", U'j', U'E', ios_defs::eofbit);
    CheckGet(obj, U"j",   U'j', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oj", U'j', U'O', ios_defs::eofbit);
    CheckGet(obj, U"j",   U'j', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"09", U'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, U"09", U'm', U'O', ios_defs::eofbit).m_month, 9);
    CheckGet(obj, U"%Em", U'm', U'E', ios_defs::eofbit);
    CheckGet(obj, U"m",   U'm', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"m",   U'm', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"33", U'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, U"33", U'M', U'O', ios_defs::eofbit).m_minute, 33);
    CheckGet(obj, U"%EM", U'M', U'E', ios_defs::eofbit);
    CheckGet(obj, U"M",   U'M', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"M",   U'M', U'O', ios_defs::strfailbit);

    CheckGet(obj, U"\n",   U'n',  0,  ios_defs::eofbit);
    CheckGet(obj, U"x",    U'n',  0,  ios_defs::goodbit);
    CheckGet(obj, U"\n",   U'n', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%En",  U'n', U'E', ios_defs::eofbit);
    CheckGet(obj, U"n",    U'n', U'O', ios_defs::strfailbit);
    CheckGet(obj, U"%On",  U'n', U'O', ios_defs::eofbit);

    CheckGet(obj, U"\t",   U't',  0,  ios_defs::eofbit);
    CheckGet(obj, U"x",    U't',  0,  ios_defs::goodbit);
    CheckGet(obj, U"\t",   U't', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Et",  U't', U'E', ios_defs::eofbit);
    CheckGet(obj, U"n",    U't', U'O', ios_defs::strfailbit);
    CheckGet(obj, U"%Ot",  U't', U'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 PM", U"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 AM", U"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(CheckGet(obj, U"PM", U'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(CheckGet(obj, U"AM", U'p', 0, ios_defs::eofbit).m_is_pm, false);
    CheckGet(obj, U"%Ep", U'p', U'E', ios_defs::eofbit);
    CheckGet(obj, U"p",   U'p', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Op", U'p', U'O', ios_defs::eofbit);
    CheckGet(obj, U"p",   U'p', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01:33:18 PM", U"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"%Er", U'r', U'E', ios_defs::eofbit);
    CheckGet(obj, U"r",   U'r', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Or", U'r', U'O', ios_defs::eofbit);
    CheckGet(obj, U"r",   U'r', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33", U"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, U"%ER", U'R', U'E', ios_defs::eofbit);
    CheckGet(obj, U"R",   U'R', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OR", U'R', U'O', ios_defs::eofbit);
    CheckGet(obj, U"R",   U'R', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"18", U'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, U"18", U'S', U'O', ios_defs::eofbit).m_second, 18);
    CheckGet(obj, U"%ES", U'S', U'E', ios_defs::eofbit);
    CheckGet(obj, U"S",   U'S', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"S",   U'S', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33:18", U"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33:18", U"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"X",   U'X', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OX", U'X', U'O', ios_defs::eofbit);
    CheckGet(obj, U"X",   U'X', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33:18", U"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"%ET", U'T', U'E', ios_defs::eofbit);
    CheckGet(obj, U"T",   U'T', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OT", U'T', U'O', ios_defs::eofbit);
    CheckGet(obj, U"T",   U'T', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"3", U'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, U"3", U'u', U'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, U"%Eu", U'u', U'E', ios_defs::eofbit);
    CheckGet(obj, U"u",   U'u', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"u",   U'u', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"24", U'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, U"%Eg", U'g', U'E', ios_defs::eofbit);
    CheckGet(obj, U"g",   U'g', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Og", U'g', U'O', ios_defs::eofbit);
    CheckGet(obj, U"g",   U'g', U'O', ios_defs::strfailbit);


    EXPECT_EQ(CheckGet(obj, U"2024", U'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, U"%EG", U'G', U'E', ios_defs::eofbit);
    CheckGet(obj, U"G",   U'G', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OG", U'G', U'O', ios_defs::eofbit);
    CheckGet(obj, U"G",   U'G', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 35 Wed", U"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 35 Wed", U"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, U"35", U'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(CheckGet(obj, U"35", U'U', U'O', ios_defs::eofbit).m_week_no, 35);
    CheckGet(obj, U"%EU", U'U', U'E', ios_defs::eofbit);
    CheckGet(obj, U"U",   U'U', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"U",   U'U', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 36 Wed", U"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 36 Wed", U"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, U"36", U'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(CheckGet(obj, U"36", U'W', U'O', ios_defs::eofbit).m_week_no, 36);
    CheckGet(obj, U"%EW", U'W', U'E', ios_defs::eofbit);
    CheckGet(obj, U"W",   U'W', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"W",   U'W', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"36", U'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    CheckGet(obj, U"54",  U'V', U'O', ios_defs::strfailbit);
    CheckGet(obj, U"36",  U'V', U'O', ios_defs::eofbit);
    CheckGet(obj, U"%EV", U'V', U'E', ios_defs::eofbit);
    CheckGet(obj, U"V",   U'V', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"V",   U'V', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"3", U'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, U"3", U'w', U'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, U"%Ew", U'w', U'E', ios_defs::eofbit);
    CheckGet(obj, U"w",   U'w', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"w",   U'w', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"24", U'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, U"24", U'y', U'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, U"24", U'y', U'O', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, U"y",  U'y', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"y",  U'y', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"2024", U'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, U"2024", U'Y', U'E', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, U"Y",   U'Y', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OY", U'Y', U'O', ios_defs::eofbit);
    CheckGet(obj, U"Y",   U'Y', U'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(CheckGet(obj, U"America/Los_Angeles", U'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, U"PST", U'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, U"America/Los_Angexes", U'Z', 0, ios_defs::strfailbit);
    CheckGet(obj, U"%EZ", U'Z', U'E', ios_defs::eofbit);
    CheckGet(obj, U"Z",   U'Z', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OZ", U'Z', U'O', ios_defs::eofbit);
    CheckGet(obj, U"Z",   U'Z', U'O', ios_defs::strfailbit);

    CheckGet(obj, U"Z", U'z', 0, ios_defs::eofbit);
    CheckGet(obj, U"+13", U'z', 0, ios_defs::eofbit);
    CheckGet(obj, U"-1110", U'z', 0, ios_defs::eofbit);
    CheckGet(obj, U"+11:10", U'z', 0, ios_defs::eofbit);
    CheckGet(obj, U"%Ez", U'z', U'E', ios_defs::eofbit);
    CheckGet(obj, U"z",  U'z', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oz", U'z', U'O', ios_defs::eofbit);
    CheckGet(obj, U"z",  U'z', U'O', ios_defs::strfailbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"1999-W52-6", U"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2019-W01-1", U"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"1999-W52-5", U"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"99-W52-6", U"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"19-W01-1", U"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"99-W52-5", U"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"20 24/09/04", U"%C %y/%m/%d", ios_defs::eofbit), check_date1);

    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((CheckGet<year_month_day>(obj, U"20 01 01", U"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar32, ChineseReadsEveryConversionSpecifier)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<char32_t>>("zh_CN.UTF-8"));

    CheckGet(obj, U"%",  U'%',  0,  ios_defs::eofbit);
    CheckGet(obj, U"x",  U'%',  0,  ios_defs::strfailbit);
    CheckGet(obj, U"%",  U'%', U'E', febit);
    CheckGet(obj, U"%E%", U'%', U'E', ios_defs::eofbit);
    CheckGet(obj, U"%",  U'%', U'O', febit);
    CheckGet(obj, U"%O%", U'%', U'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet(obj, U"三", U'a', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, U"%Ea", U'a', U'E', ios_defs::eofbit);
    CheckGet(obj, U"a",   U'a', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oa", U'a', U'O', ios_defs::eofbit);
    CheckGet(obj, U"a",   U'a', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"星期三", U'A', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, U"%EA", U'A', U'E', ios_defs::eofbit);
    CheckGet(obj, U"A",   U'A', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OA", U'A', U'O', ios_defs::eofbit);
    CheckGet(obj, U"A",   U'A', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"九月", U'b', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, U"%Eb", U'b', U'E', ios_defs::eofbit);
    CheckGet(obj, U"b",   U'b', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Ob", U'b', U'O', ios_defs::eofbit);
    CheckGet(obj, U"b",   U'b', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"九月", U'B', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, U"%EB", U'B', U'E', ios_defs::eofbit);
    CheckGet(obj, U"B",   U'B', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OB", U'B', U'O', ios_defs::eofbit);
    CheckGet(obj, U"B",   U'B', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"九月", U'h', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, U"%Eh", U'h', U'E', ios_defs::eofbit);
    CheckGet(obj, U"h",   U'h', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oh", U'h', U'O', ios_defs::eofbit);
    CheckGet(obj, U"h",   U'h', U'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024年09月04日 星期三 13时33分18秒", U'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024年09月04日 星期三 13时33分18秒", U'c', U'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, U"c",   U'c', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oc", U'c', U'O', ios_defs::eofbit);
    CheckGet(obj, U"c",   U'c', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"20", U'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(CheckGet(obj, U"20", U'C', U'E', ios_defs::eofbit).m_century, 20);
    CheckGet(obj, U"C",   U'C', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OC", U'C', U'O', ios_defs::eofbit);
    CheckGet(obj, U"C",   U'C', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"04", U'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, U"04", U'd', U'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, U"%Ed", U'd', U'E', ios_defs::eofbit);
    CheckGet(obj, U"d",   U'd', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"d",   U'd', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"4", U'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, U"4", U'e', U'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, U"%Ee", U'e', U'E', ios_defs::eofbit);
    CheckGet(obj, U"e",   U'e', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"e",   U'e', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024-09-04", U'F', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, U"%EF", U'F', U'E', ios_defs::eofbit);
    CheckGet(obj, U"F",   U'F', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OF", U'F', U'O', ios_defs::eofbit);
    CheckGet(obj, U"F",   U'F', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024年09月04日", U'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024年09月04日", U'x', U'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, U"x",   U'x', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Ox", U'x', U'O', ios_defs::eofbit);
    CheckGet(obj, U"x",   U'x', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"09/04/24", U'D', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, U"%ED", U'D', U'E', ios_defs::eofbit);
    CheckGet(obj, U"D",   U'D', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OD", U'D', U'O', ios_defs::eofbit);
    CheckGet(obj, U"D",   U'D', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"13", U'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, U"13", U'H', U'O', ios_defs::eofbit).m_hour, 13);
    CheckGet(obj, U"%EH", U'H', U'E', ios_defs::eofbit);
    CheckGet(obj, U"H",   U'H', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"H",   U'H', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"01", U'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, U"01", U'I', U'O', ios_defs::eofbit).m_hour, 1);
    CheckGet(obj, U"%EI", U'I', U'E', ios_defs::eofbit);
    CheckGet(obj, U"I",   U'I', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"I",   U'I', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"248", U'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 248", U"%Y %j", ios_defs::eofbit), check_date1);
    CheckGet(obj, U"%Ej", U'j', U'E', ios_defs::eofbit);
    CheckGet(obj, U"j",   U'j', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oj", U'j', U'O', ios_defs::eofbit);
    CheckGet(obj, U"j",   U'j', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"09", U'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, U"09", U'm', U'O', ios_defs::eofbit).m_month, 9);
    CheckGet(obj, U"%Em", U'm', U'E', ios_defs::eofbit);
    CheckGet(obj, U"m",   U'm', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"m",   U'm', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"33", U'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, U"33", U'M', U'O', ios_defs::eofbit).m_minute, 33);
    CheckGet(obj, U"%EM", U'M', U'E', ios_defs::eofbit);
    CheckGet(obj, U"M",   U'M', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"M",   U'M', U'O', ios_defs::strfailbit);

    CheckGet(obj, U"\n",   U'n',  0,  ios_defs::eofbit);
    CheckGet(obj, U"x",    U'n',  0,  ios_defs::goodbit);
    CheckGet(obj, U"\n",   U'n', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%En",  U'n', U'E', ios_defs::eofbit);
    CheckGet(obj, U"n",    U'n', U'O', ios_defs::strfailbit);
    CheckGet(obj, U"%On",  U'n', U'O', ios_defs::eofbit);

    CheckGet(obj, U"\t",   U't',  0,  ios_defs::eofbit);
    CheckGet(obj, U"x",    U't',  0,  ios_defs::goodbit);
    CheckGet(obj, U"\t",   U't', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Et",  U't', U'E', ios_defs::eofbit);
    CheckGet(obj, U"n",    U't', U'O', ios_defs::strfailbit);
    CheckGet(obj, U"%Ot",  U't', U'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 下午", U"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 上午", U"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(CheckGet(obj, U"下午", U'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(CheckGet(obj, U"上午", U'p', 0, ios_defs::eofbit).m_is_pm, false);
    CheckGet(obj, U"%Ep", U'p', U'E', ios_defs::eofbit);
    CheckGet(obj, U"p",   U'p', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Op", U'p', U'O', ios_defs::eofbit);
    CheckGet(obj, U"p",   U'p', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"下午 01时33分18秒", U"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"%Er", U'r', U'E', ios_defs::eofbit);
    CheckGet(obj, U"r",   U'r', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Or", U'r', U'O', ios_defs::eofbit);
    CheckGet(obj, U"r",   U'r', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33", U"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, U"%ER", U'R', U'E', ios_defs::eofbit);
    CheckGet(obj, U"R",   U'R', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OR", U'R', U'O', ios_defs::eofbit);
    CheckGet(obj, U"R",   U'R', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"18", U'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, U"18", U'S', U'O', ios_defs::eofbit).m_second, 18);
    CheckGet(obj, U"%ES", U'S', U'E', ios_defs::eofbit);
    CheckGet(obj, U"S",   U'S', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"S",   U'S', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13时33分18秒", U"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13时33分18秒", U"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"X",   U'X', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OX", U'X', U'O', ios_defs::eofbit);
    CheckGet(obj, U"X",   U'X', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33:18", U"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"%ET", U'T', U'E', ios_defs::eofbit);
    CheckGet(obj, U"T",   U'T', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OT", U'T', U'O', ios_defs::eofbit);
    CheckGet(obj, U"T",   U'T', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"3", U'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, U"3", U'u', U'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, U"%Eu", U'u', U'E', ios_defs::eofbit);
    CheckGet(obj, U"u",   U'u', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"u",   U'u', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"24", U'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, U"%Eg", U'g', U'E', ios_defs::eofbit);
    CheckGet(obj, U"g",   U'g', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Og", U'g', U'O', ios_defs::eofbit);
    CheckGet(obj, U"g",   U'g', U'O', ios_defs::strfailbit);


    EXPECT_EQ(CheckGet(obj, U"2024", U'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, U"%EG", U'G', U'E', ios_defs::eofbit);
    CheckGet(obj, U"G",   U'G', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OG", U'G', U'O', ios_defs::eofbit);
    CheckGet(obj, U"G",   U'G', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 35 三", U"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 35 三", U"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, U"35", U'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(CheckGet(obj, U"35", U'U', U'O', ios_defs::eofbit).m_week_no, 35);
    CheckGet(obj, U"%EU", U'U', U'E', ios_defs::eofbit);
    CheckGet(obj, U"U",   U'U', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"U",   U'U', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 36 三", U"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 36 三", U"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, U"36", U'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(CheckGet(obj, U"36", U'W', U'O', ios_defs::eofbit).m_week_no, 36);
    CheckGet(obj, U"%EW", U'W', U'E', ios_defs::eofbit);
    CheckGet(obj, U"W",   U'W', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"W",   U'W', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"36", U'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    CheckGet(obj, U"54",  U'V', U'O', ios_defs::strfailbit);
    CheckGet(obj, U"36",  U'V', U'O', ios_defs::eofbit);
    CheckGet(obj, U"%EV", U'V', U'E', ios_defs::eofbit);
    CheckGet(obj, U"V",   U'V', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"V",   U'V', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"3", U'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, U"3", U'w', U'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, U"%Ew", U'w', U'E', ios_defs::eofbit);
    CheckGet(obj, U"w",   U'w', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"w",   U'w', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"24", U'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, U"24", U'y', U'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, U"24", U'y', U'O', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, U"y",  U'y', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"y",  U'y', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"2024", U'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, U"2024", U'Y', U'E', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, U"Y",   U'Y', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OY", U'Y', U'O', ios_defs::eofbit);
    CheckGet(obj, U"Y",   U'Y', U'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(CheckGet(obj, U"America/Los_Angeles", U'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, U"PST", U'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, U"America/Los_Angexes", U'Z', 0, ios_defs::strfailbit);
    CheckGet(obj, U"%EZ", U'Z', U'E', ios_defs::eofbit);
    CheckGet(obj, U"Z",   U'Z', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OZ", U'Z', U'O', ios_defs::eofbit);
    CheckGet(obj, U"Z",   U'Z', U'O', ios_defs::strfailbit);

    CheckGet(obj, U"Z", U'z', 0, ios_defs::eofbit);
    CheckGet(obj, U"+13", U'z', 0, ios_defs::eofbit);
    CheckGet(obj, U"-1110", U'z', 0, ios_defs::eofbit);
    CheckGet(obj, U"+11:10", U'z', 0, ios_defs::eofbit);
    CheckGet(obj, U"%Ez", U'z', U'E', ios_defs::eofbit);
    CheckGet(obj, U"z",  U'z', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oz", U'z', U'O', ios_defs::eofbit);
    CheckGet(obj, U"z",  U'z', U'O', ios_defs::strfailbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"1999-W52-6", U"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2019-W01-1", U"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"1999-W52-5", U"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"99-W52-6", U"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"19-W01-1", U"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"99-W52-5", U"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"20 24/09/04", U"%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((CheckGet<year_month_day>(obj, U"20 01 01", U"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar32, JapaneseReadsEveryConversionSpecifier)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<char32_t>>("ja_JP.UTF-8"));

    CheckGet(obj, U"%",  U'%',  0,  ios_defs::eofbit);
    CheckGet(obj, U"x",  U'%',  0,  ios_defs::strfailbit);
    CheckGet(obj, U"%",  U'%', U'E', febit);
    CheckGet(obj, U"%E%", U'%', U'E', ios_defs::eofbit);
    CheckGet(obj, U"%",  U'%', U'O', febit);
    CheckGet(obj, U"%O%", U'%', U'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet(obj, U"水", U'a', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, U"%Ea", U'a', U'E', ios_defs::eofbit);
    CheckGet(obj, U"a",   U'a', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oa", U'a', U'O', ios_defs::eofbit);
    CheckGet(obj, U"a",   U'a', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"水曜日", U'A', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, U"%EA", U'A', U'E', ios_defs::eofbit);
    CheckGet(obj, U"A",   U'A', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OA", U'A', U'O', ios_defs::eofbit);
    CheckGet(obj, U"A",   U'A', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"9月", U'b', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, U"%Eb", U'b', U'E', ios_defs::eofbit);
    CheckGet(obj, U"b",   U'b', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Ob", U'b', U'O', ios_defs::eofbit);
    CheckGet(obj, U"b",   U'b', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"9月", U'B', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, U"%EB", U'B', U'E', ios_defs::eofbit);
    CheckGet(obj, U"B",   U'B', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OB", U'B', U'O', ios_defs::eofbit);
    CheckGet(obj, U"B",   U'B', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"9月", U'h', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, U"%Eh", U'h', U'E', ios_defs::eofbit);
    CheckGet(obj, U"h",   U'h', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oh", U'h', U'O', ios_defs::eofbit);
    CheckGet(obj, U"h",   U'h', U'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024年09月04日 13時33分18秒", U'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"令和6年09月04日 13時33分18秒", U'c', U'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"202409月04日 13時33分18秒", U'c', U'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, U"c",   U'c', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oc", U'c', U'O', ios_defs::eofbit);
    CheckGet(obj, U"c",   U'c', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"20", U'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"平成", U'C', U'E', ios_defs::eofbit).year(), std::chrono::year(1990));
    CheckGet(obj, U"C",   U'C', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OC", U'C', U'O', ios_defs::eofbit);
    CheckGet(obj, U"C",   U'C', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"04", U'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, U"04", U'd', U'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, U"四", U'd', U'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, U"%Ed", U'd', U'E', ios_defs::eofbit);
    CheckGet(obj, U"d",   U'd', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"d",   U'd', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"4", U'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, U"4", U'e', U'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, U"四", U'e', U'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, U"%Ee", U'e', U'E', ios_defs::eofbit);
    CheckGet(obj, U"e",   U'e', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"e",   U'e', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024-09-04", U'F', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, U"%EF", U'F', U'E', ios_defs::eofbit);
    CheckGet(obj, U"F",   U'F', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OF", U'F', U'O', ios_defs::eofbit);
    CheckGet(obj, U"F",   U'F', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024年09月04日", U'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"令和6年09月04日", U'x', U'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"202409月04日", U'x', U'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, U"x",   U'x', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Ox", U'x', U'O', ios_defs::eofbit);
    CheckGet(obj, U"x",   U'x', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"09/04/24", U'D', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, U"%ED", U'D', U'E', ios_defs::eofbit);
    CheckGet(obj, U"D",   U'D', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OD", U'D', U'O', ios_defs::eofbit);
    CheckGet(obj, U"D",   U'D', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"13", U'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, U"13", U'H', U'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, U"十三", U'H', U'O', ios_defs::eofbit).m_hour, 13);
    CheckGet(obj, U"%EH", U'H', U'E', ios_defs::eofbit);
    CheckGet(obj, U"H",   U'H', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"H",   U'H', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"01", U'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, U"01", U'I', U'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, U"一", U'I', U'O', ios_defs::eofbit).m_hour, 1);
    CheckGet(obj, U"%EI", U'I', U'E', ios_defs::eofbit);
    CheckGet(obj, U"I",   U'I', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"I",   U'I', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"248", U'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 248", U"%Y %j", ios_defs::eofbit), check_date1);
    CheckGet(obj, U"%Ej", U'j', U'E', ios_defs::eofbit);
    CheckGet(obj, U"j",   U'j', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oj", U'j', U'O', ios_defs::eofbit);
    CheckGet(obj, U"j",   U'j', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"09", U'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, U"09", U'm', U'O', ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, U"九", U'm', U'O', ios_defs::eofbit).m_month, 9);
    CheckGet(obj, U"%Em", U'm', U'E', ios_defs::eofbit);
    CheckGet(obj, U"m",   U'm', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"m",   U'm', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"33", U'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, U"33", U'M', U'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, U"三十三", U'M', U'O', ios_defs::eofbit).m_minute, 33);
    CheckGet(obj, U"%EM", U'M', U'E', ios_defs::eofbit);
    CheckGet(obj, U"M",   U'M', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"M",   U'M', U'O', ios_defs::strfailbit);

    CheckGet(obj, U"\n",   U'n',  0,  ios_defs::eofbit);
    CheckGet(obj, U"x",    U'n',  0,  ios_defs::goodbit);
    CheckGet(obj, U"\n",   U'n', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%En",  U'n', U'E', ios_defs::eofbit);
    CheckGet(obj, U"n",    U'n', U'O', ios_defs::strfailbit);
    CheckGet(obj, U"%On",  U'n', U'O', ios_defs::eofbit);

    CheckGet(obj, U"\t",   U't',  0,  ios_defs::eofbit);
    CheckGet(obj, U"x",    U't',  0,  ios_defs::goodbit);
    CheckGet(obj, U"\t",   U't', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Et",  U't', U'E', ios_defs::eofbit);
    CheckGet(obj, U"n",    U't', U'O', ios_defs::strfailbit);
    CheckGet(obj, U"%Ot",  U't', U'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 午後", U"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 午前", U"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(CheckGet(obj, U"午後", U'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(CheckGet(obj, U"午前", U'p', 0, ios_defs::eofbit).m_is_pm, false);
    CheckGet(obj, U"%Ep", U'p', U'E', ios_defs::eofbit);
    CheckGet(obj, U"p",   U'p', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Op", U'p', U'O', ios_defs::eofbit);
    CheckGet(obj, U"p",   U'p', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"午後01時33分18秒", U"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"%Er", U'r', U'E', ios_defs::eofbit);
    CheckGet(obj, U"r",   U'r', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Or", U'r', U'O', ios_defs::eofbit);
    CheckGet(obj, U"r",   U'r', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33", U"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, U"%ER", U'R', U'E', ios_defs::eofbit);
    CheckGet(obj, U"R",   U'R', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OR", U'R', U'O', ios_defs::eofbit);
    CheckGet(obj, U"R",   U'R', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"18", U'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, U"18", U'S', U'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, U"十八", U'S', U'O', ios_defs::eofbit).m_second, 18);
    CheckGet(obj, U"%ES", U'S', U'E', ios_defs::eofbit);
    CheckGet(obj, U"S",   U'S', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"S",   U'S', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13時33分18秒", U"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13時33分18秒", U"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"X",   U'X', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OX", U'X', U'O', ios_defs::eofbit);
    CheckGet(obj, U"X",   U'X', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33:18", U"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"%ET", U'T', U'E', ios_defs::eofbit);
    CheckGet(obj, U"T",   U'T', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OT", U'T', U'O', ios_defs::eofbit);
    CheckGet(obj, U"T",   U'T', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"3", U'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, U"3", U'u', U'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, U"三", U'u', U'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, U"%Eu", U'u', U'E', ios_defs::eofbit);
    CheckGet(obj, U"u",   U'u', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"u",   U'u', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"24", U'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, U"%Eg", U'g', U'E', ios_defs::eofbit);
    CheckGet(obj, U"g",   U'g', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Og", U'g', U'O', ios_defs::eofbit);
    CheckGet(obj, U"g",   U'g', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"2024", U'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, U"%EG", U'G', U'E', ios_defs::eofbit);
    CheckGet(obj, U"G",   U'G', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OG", U'G', U'O', ios_defs::eofbit);
    CheckGet(obj, U"G",   U'G', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 35 水", U"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 35 水", U"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 三十五 水", U"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, U"35", U'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(CheckGet(obj, U"35", U'U', U'O', ios_defs::eofbit).m_week_no, 35);
    CheckGet(obj, U"%EU", U'U', U'E', ios_defs::eofbit);
    CheckGet(obj, U"U",   U'U', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"U",   U'U', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 36 水", U"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 36 水", U"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2024 三十六 水", U"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, U"36", U'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(CheckGet(obj, U"36", U'W', U'O', ios_defs::eofbit).m_week_no, 36);
    CheckGet(obj, U"%EW", U'W', U'E', ios_defs::eofbit);
    CheckGet(obj, U"W",   U'W', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"W",   U'W', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"36", U'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(CheckGet(obj, U"36", U'V', U'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(CheckGet(obj, U"三十六", U'V', U'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    CheckGet(obj, U"54",  U'V', U'O', ios_defs::strfailbit);
    CheckGet(obj, U"%EV", U'V', U'E', ios_defs::eofbit);
    CheckGet(obj, U"V",   U'V', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"V",   U'V', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"3", U'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, U"3", U'w', U'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, U"三", U'w', U'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, U"%Ew", U'w', U'E', ios_defs::eofbit);
    CheckGet(obj, U"w",   U'w', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"w",   U'w', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"24", U'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"6", U'y', U'E', ios_defs::eofbit).year(), std::chrono::year(2024));
    EXPECT_EQ(CheckGet(obj, U"24", U'y', U'O', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, U"二十四", U'y', U'O', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, U"y",  U'y', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"y",  U'y', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, U"2024", U'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, U"2024", U'Y', U'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"平成3年", U'Y', U'E', ios_defs::eofbit).year(), std::chrono::year(1991));
    CheckGet(obj, U"Y",   U'Y', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OY", U'Y', U'O', ios_defs::eofbit);
    CheckGet(obj, U"Y",   U'Y', U'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(CheckGet(obj, U"America/Los_Angeles", U'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, U"PST", U'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, U"America/Los_Angexes", U'Z', 0, ios_defs::strfailbit);
    CheckGet(obj, U"%EZ", U'Z', U'E', ios_defs::eofbit);
    CheckGet(obj, U"Z",   U'Z', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%OZ", U'Z', U'O', ios_defs::eofbit);
    CheckGet(obj, U"Z",   U'Z', U'O', ios_defs::strfailbit);

    CheckGet(obj, U"Z", U'z', 0, ios_defs::eofbit);
    CheckGet(obj, U"+13", U'z', 0, ios_defs::eofbit);
    CheckGet(obj, U"-1110", U'z', 0, ios_defs::eofbit);
    CheckGet(obj, U"+11:10", U'z', 0, ios_defs::eofbit);
    CheckGet(obj, U"%Ez", U'z', U'E', ios_defs::eofbit);
    CheckGet(obj, U"z",  U'z', U'E', ios_defs::strfailbit);
    CheckGet(obj, U"%Oz", U'z', U'O', ios_defs::eofbit);
    CheckGet(obj, U"z",  U'z', U'O', ios_defs::strfailbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"1999-W52-6", U"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"2019-W01-1", U"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"1999-W52-5", U"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"99-W52-6", U"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"19-W01-1", U"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, U"99-W52-5", U"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, U"20 24/09/04", U"%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((CheckGet<year_month_day>(obj, U"20 01 01", U"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar32, AWeekdayOrMonthNameIsMatchedAgainstBothSpellings)
{
    timeio obj(std::make_shared<timeio_conf<char32_t>>("C"));
    {
        std::u32string input = U"Mon";
        std::u32string format = U"%a";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 1);
    }

    {
        std::u32string input = U"Tue ";
        std::u32string format = U"%a";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != U' '));
        EXPECT_EQ(time.tm_wday, 2);
    }

    {
        std::u32string input = U"Wednesday";
        std::u32string format = U"%a";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 3);
    }

    {
        std::u32string input = U"Thu";
        std::u32string format = U"%A";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 4);
    }

    {
        std::u32string input = U"Fri ";
        std::u32string format = U"%A";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != U' '));
        EXPECT_EQ(time.tm_wday, 5);
    }

    {
        std::u32string input = U"Saturday";
        std::u32string format = U"%A";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 6);
    }

    {
        std::u32string input = U"Feb";
        std::u32string format = U"%b";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 1);
    }

    {
        std::u32string input = U"Mar ";
        std::u32string format = U"%b";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != U' '));
        EXPECT_EQ(time.tm_mon, 2);
    }

    {
        std::u32string input = U"April";
        std::u32string format = U"%b";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 3);
    }

    {
        std::u32string input = U"May";
        std::u32string format = U"%B";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 4);
    }

    {
        std::u32string input = U"Jun ";
        std::u32string format = U"%B";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != U' '));
        EXPECT_EQ(time.tm_mon, 5);
    }

    {
        std::u32string input = U"July";
        std::u32string format = U"%B";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 6);
    }

    {
        std::u32string input = U"Aug";
        std::u32string format = U"%h";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 7);
    }

    {
        std::u32string input = U"May ";
        std::u32string format = U"%h";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != U' '));
        EXPECT_EQ(time.tm_mon, 4);
    }

    {
        std::u32string input = U"October";
        std::u32string format = U"%h";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 9);
    }

    // Other tests.
    {
        std::u32string input = U"2.";
        std::u32string format = U"%d.";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mday, 2);
    }

    {
        std::u32string input = U"0.";
        std::u32string format = U"%d.";

        time_parse_context<char32_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::u32string input = U"32.";
        std::u32string format = U"%d.";

        time_parse_context<char32_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::u32string input = U"5.";
        std::u32string format = U"%e.";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mday, 5);
    }

    {
        std::u32string input = U"06.";
        std::u32string format = U"%e.";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mday, 6);
    }

    {
        std::u32string input = U"0";
        std::u32string format = U"%e";

        time_parse_context<char32_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::u32string input = U"35";
        std::u32string format = U"%e";

        time_parse_context<char32_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    struct clock_case { const char32_t* input; int hour; int minute; };
    for (const clock_case tc : {
             clock_case{U"12:11AM", 0, 11},
             clock_case{U"03:14AM", 3, 14},
             clock_case{U"09:27AM", 9, 27},
             clock_case{U"12:29PM", 12, 29},
             clock_case{U"02:38PM", 14, 38},
             clock_case{U"09:52PM", 21, 52},
         })
    {
        std::u32string input(tc.input);
        time_parse_context<char32_t> ctx;
        const auto ret = obj.get(input.begin(), input.end(), ctx,
                                 std::u32string_view{U"%I:%M%p"});
        const auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_hour, tc.hour);
        EXPECT_EQ(time.tm_min, tc.minute);
    }

    {
        std::u32string input = U"08%46";
        std::u32string format = U"%H%%%S";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_hour, 8);
        EXPECT_EQ(time.tm_sec, 46);
    }

    {
        std::u32string input = U"29:14";
        std::u32string format = U"%H:%M";

        time_parse_context<char32_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::u32string input = U"Oct+tail";
        std::u32string format = U"%b+tail";

        time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mon, 9);
    }
}

TEST(TimeioChar32, JapaneseReadsEveryConversionSpecifierIntoADate)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<char32_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<char32_t, true, true, tz_level::none>, true, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(U"%",  U'%',  0,  ios_defs::eofbit);
    FOri(U"x",  U'%',  0,  ios_defs::strfailbit);
    FOri(U"%",  U'%', U'E', febit);
    FOri(U"%E%", U'%', U'E', ios_defs::eofbit);
    FOri(U"%",  U'%', U'O', febit);
    FOri(U"%O%", U'%', U'O', ios_defs::eofbit);

    EXPECT_EQ(FOri(U"水", U'a', 0, ios_defs::eofbit).m_wday, 3);
    FOri(U"%Ea", U'a', U'E', ios_defs::eofbit);
    FOri(U"a",   U'a', U'E', ios_defs::strfailbit);
    FOri(U"%Oa", U'a', U'O', ios_defs::eofbit);
    FOri(U"a",   U'a', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"水曜日", U'A', 0, ios_defs::eofbit).m_wday, 3);
    FOri(U"%EA", U'A', U'E', ios_defs::eofbit);
    FOri(U"A",   U'A', U'E', ios_defs::strfailbit);
    FOri(U"%OA", U'A', U'O', ios_defs::eofbit);
    FOri(U"A",   U'A', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"9月", U'b', 0, ios_defs::eofbit).m_month, 9);
    FOri(U"%Eb", U'b', U'E', ios_defs::eofbit);
    FOri(U"b",   U'b', U'E', ios_defs::strfailbit);
    FOri(U"%Ob", U'b', U'O', ios_defs::eofbit);
    FOri(U"b",   U'b', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"9月", U'B', 0, ios_defs::eofbit).m_month, 9);
    FOri(U"%EB", U'B', U'E', ios_defs::eofbit);
    FOri(U"B",   U'B', U'E', ios_defs::strfailbit);
    FOri(U"%OB", U'B', U'O', ios_defs::eofbit);
    FOri(U"B",   U'B', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"9月", U'h', 0, ios_defs::eofbit).m_month, 9);
    FOri(U"%Eh", U'h', U'E', ios_defs::eofbit);
    FOri(U"h",   U'h', U'E', ios_defs::strfailbit);
    FOri(U"%Oh", U'h', U'O', ios_defs::eofbit);
    FOri(U"h",   U'h', U'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(FYmd(U"2024年09月04日 13時33分18秒", U'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"令和6年09月04日 13時33分18秒", U'c', U'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"202409月04日 13時33分18秒", U'c', U'E', ios_defs::eofbit), check_date1);
    FOri(U"c",   U'c', U'E', ios_defs::strfailbit);
    FOri(U"%Oc", U'c', U'O', ios_defs::eofbit);
    FOri(U"c",   U'c', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"20", U'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(FYmd(U"平成", U'C', U'E', ios_defs::eofbit).year(), std::chrono::year(1990));
    FOri(U"C",   U'C', U'E', ios_defs::strfailbit);
    FOri(U"%OC", U'C', U'O', ios_defs::eofbit);
    FOri(U"C",   U'C', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"04", U'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(U"04", U'd', U'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(U"四", U'd', U'O', ios_defs::eofbit).m_mday, 4);
    FOri(U"%Ed", U'd', U'E', ios_defs::eofbit);
    FOri(U"d",   U'd', U'E', ios_defs::strfailbit);
    FOri(U"d",   U'd', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"4", U'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(U"4", U'e', U'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(U"四", U'e', U'O', ios_defs::eofbit).m_mday, 4);
    FOri(U"%Ee", U'e', U'E', ios_defs::eofbit);
    FOri(U"e",   U'e', U'E', ios_defs::strfailbit);
    FOri(U"e",   U'e', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(U"2024-09-04", U'F', 0, ios_defs::eofbit), check_date1);
    FOri(U"%EF", U'F', U'E', ios_defs::eofbit);
    FOri(U"F",   U'F', U'E', ios_defs::strfailbit);
    FOri(U"%OF", U'F', U'O', ios_defs::eofbit);
    FOri(U"F",   U'F', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(U"2024年09月04日", U'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"令和6年09月04日", U'x', U'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"202409月04日", U'x', U'E', ios_defs::eofbit), check_date1);
    FOri(U"x",   U'x', U'E', ios_defs::strfailbit);
    FOri(U"%Ox", U'x', U'O', ios_defs::eofbit);
    FOri(U"x",   U'x', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(U"09/04/24", U'D', 0, ios_defs::eofbit), check_date1);
    FOri(U"%ED", U'D', U'E', ios_defs::eofbit);
    FOri(U"D",   U'D', U'E', ios_defs::strfailbit);
    FOri(U"%OD", U'D', U'O', ios_defs::eofbit);
    FOri(U"D",   U'D', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"13", U'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(U"13", U'H', U'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(U"十三", U'H', U'O', ios_defs::eofbit).m_hour, 13);
    FOri(U"%EH", U'H', U'E', ios_defs::eofbit);
    FOri(U"H",   U'H', U'E', ios_defs::strfailbit);
    FOri(U"H",   U'H', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"01", U'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(U"01", U'I', U'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(U"一", U'I', U'O', ios_defs::eofbit).m_hour, 1);
    FOri(U"%EI", U'I', U'E', ios_defs::eofbit);
    FOri(U"I",   U'I', U'E', ios_defs::strfailbit);
    FOri(U"I",   U'I', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"248", U'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(FYmd(U"2024 248", U"%Y %j", ios_defs::eofbit), check_date1);
    FOri(U"%Ej", U'j', U'E', ios_defs::eofbit);
    FOri(U"j",   U'j', U'E', ios_defs::strfailbit);
    FOri(U"%Oj", U'j', U'O', ios_defs::eofbit);
    FOri(U"j",   U'j', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"09", U'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri(U"09", U'm', U'O', ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri(U"九", U'm', U'O', ios_defs::eofbit).m_month, 9);
    FOri(U"%Em", U'm', U'E', ios_defs::eofbit);
    FOri(U"m",   U'm', U'E', ios_defs::strfailbit);
    FOri(U"m",   U'm', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"33", U'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(U"33", U'M', U'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(U"三十三", U'M', U'O', ios_defs::eofbit).m_minute, 33);
    FOri(U"%EM", U'M', U'E', ios_defs::eofbit);
    FOri(U"M",   U'M', U'E', ios_defs::strfailbit);
    FOri(U"M",   U'M', U'O', ios_defs::strfailbit);

    FOri(U"\n",   U'n',  0,  ios_defs::eofbit);
    FOri(U"x",    U'n',  0,  ios_defs::goodbit);
    FOri(U"\n",   U'n', U'E', ios_defs::strfailbit);
    FOri(U"%En",  U'n', U'E', ios_defs::eofbit);
    FOri(U"n",    U'n', U'O', ios_defs::strfailbit);
    FOri(U"%On",  U'n', U'O', ios_defs::eofbit);

    FOri(U"\t",   U't',  0,  ios_defs::eofbit);
    FOri(U"x",    U't',  0,  ios_defs::goodbit);
    FOri(U"\t",   U't', U'E', ios_defs::strfailbit);
    FOri(U"%Et",  U't', U'E', ios_defs::eofbit);
    FOri(U"n",    U't', U'O', ios_defs::strfailbit);
    FOri(U"%Ot",  U't', U'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 午後", U"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 午前", U"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(FOri(U"午後", U'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(FOri(U"午前", U'p', 0, ios_defs::eofbit).m_is_pm, false);
    FOri(U"%Ep", U'p', U'E', ios_defs::eofbit);
    FOri(U"p",   U'p', U'E', ios_defs::strfailbit);
    FOri(U"%Op", U'p', U'O', ios_defs::eofbit);
    FOri(U"p",   U'p', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"午後01時33分18秒", U"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"%Er", U'r', U'E', ios_defs::eofbit);
    FOri(U"r",   U'r', U'E', ios_defs::strfailbit);
    FOri(U"%Or", U'r', U'O', ios_defs::eofbit);
    FOri(U"r",   U'r', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33", U"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(U"%ER", U'R', U'E', ios_defs::eofbit);
    FOri(U"R",   U'R', U'E', ios_defs::strfailbit);
    FOri(U"%OR", U'R', U'O', ios_defs::eofbit);
    FOri(U"R",   U'R', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"18", U'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(U"18", U'S', U'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(U"十八", U'S', U'O', ios_defs::eofbit).m_second, 18);
    FOri(U"%ES", U'S', U'E', ios_defs::eofbit);
    FOri(U"S",   U'S', U'E', ios_defs::strfailbit);
    FOri(U"S",   U'S', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13時33分18秒", U"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13時33分18秒", U"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"X",   U'X', U'E', ios_defs::strfailbit);
    FOri(U"%OX", U'X', U'O', ios_defs::eofbit);
    FOri(U"X",   U'X', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33:18", U"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"%ET", U'T', U'E', ios_defs::eofbit);
    FOri(U"T",   U'T', U'E', ios_defs::strfailbit);
    FOri(U"%OT", U'T', U'O', ios_defs::eofbit);
    FOri(U"T",   U'T', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"3", U'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(U"3", U'u', U'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(U"三", U'u', U'O', ios_defs::eofbit).m_wday, 3);
    FOri(U"%Eu", U'u', U'E', ios_defs::eofbit);
    FOri(U"u",   U'u', U'E', ios_defs::strfailbit);
    FOri(U"u",   U'u', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"24", U'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri(U"%Eg", U'g', U'E', ios_defs::eofbit);
    FOri(U"g",   U'g', U'E', ios_defs::strfailbit);
    FOri(U"%Og", U'g', U'O', ios_defs::eofbit);
    FOri(U"g",   U'g', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"2024", U'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri(U"%EG", U'G', U'E', ios_defs::eofbit);
    FOri(U"G",   U'G', U'E', ios_defs::strfailbit);
    FOri(U"%OG", U'G', U'O', ios_defs::eofbit);
    FOri(U"G",   U'G', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(U"2024 35 水", U"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"2024 35 水", U"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"2024 三十五 水", U"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri(U"35", U'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(FOri(U"35", U'U', U'O', ios_defs::eofbit).m_week_no, 35);
    FOri(U"%EU", U'U', U'E', ios_defs::eofbit);
    FOri(U"U",   U'U', U'E', ios_defs::strfailbit);
    FOri(U"U",   U'U', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(U"2024 36 水", U"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"2024 36 水", U"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"2024 三十六 水", U"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri(U"36", U'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(FOri(U"36", U'W', U'O', ios_defs::eofbit).m_week_no, 36);
    FOri(U"%EW", U'W', U'E', ios_defs::eofbit);
    FOri(U"W",   U'W', U'E', ios_defs::strfailbit);
    FOri(U"W",   U'W', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"36", U'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri(U"36", U'V', U'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri(U"三十六", U'V', U'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    FOri(U"54",  U'V', U'O', ios_defs::strfailbit);
    FOri(U"%EV", U'V', U'E', ios_defs::eofbit);
    FOri(U"V",   U'V', U'E', ios_defs::strfailbit);
    FOri(U"V",   U'V', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"3", U'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(U"3", U'w', U'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(U"三", U'w', U'O', ios_defs::eofbit).m_wday, 3);
    FOri(U"%Ew", U'w', U'E', ios_defs::eofbit);
    FOri(U"w",   U'w', U'E', ios_defs::strfailbit);
    FOri(U"w",   U'w', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"24", U'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd(U"6", U'y', U'E', ios_defs::eofbit).year(), std::chrono::year(2024));
    EXPECT_EQ(FOri(U"24", U'y', U'O', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri(U"二十四", U'y', U'O', ios_defs::eofbit).m_year, 2024);
    FOri(U"y",  U'y', U'E', ios_defs::strfailbit);
    FOri(U"y",  U'y', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"2024", U'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri(U"2024", U'Y', U'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd(U"平成3年", U'Y', U'E', ios_defs::eofbit).year(), std::chrono::year(1991));
    FOri(U"Y",   U'Y', U'E', ios_defs::strfailbit);
    FOri(U"%OY", U'Y', U'O', ios_defs::eofbit);
    FOri(U"Y",   U'Y', U'O', ios_defs::strfailbit);

    FOri(U"%Z", U'Z', 0, ios_defs::eofbit);
    FOri(U"%EZ", U'Z', U'E', ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'E', ios_defs::strfailbit);
    FOri(U"%OZ", U'Z', U'O', ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'O', ios_defs::strfailbit);

    FOri(U"%z", U'z', 0, ios_defs::eofbit);
    FOri(U"%Ez", U'z', U'E', ios_defs::eofbit);
    FOri(U"%Oz", U'z', U'O', ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(FYmd(U"1999-W52-6", U"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd(U"2019-W01-1", U"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd(U"1999-W52-5", U"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd(U"99-W52-6", U"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd(U"19-W01-1", U"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd(U"99-W52-5", U"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd(U"20 24/09/04", U"%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((FYmd(U"20 01 01", U"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar32, JapaneseReadsEveryConversionSpecifierIntoADateWithoutAZone)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<char32_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<char32_t, true, false, tz_level::none>, true, false, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, false, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(U"%",  U'%',  0,  ios_defs::eofbit);
    FOri(U"x",  U'%',  0,  ios_defs::strfailbit);
    FOri(U"%",  U'%', U'E', febit);
    FOri(U"%E%", U'%', U'E', ios_defs::eofbit);
    FOri(U"%",  U'%', U'O', febit);
    FOri(U"%O%", U'%', U'O', ios_defs::eofbit);

    EXPECT_EQ(FOri(U"水", U'a', 0, ios_defs::eofbit).m_wday, 3);
    FOri(U"%Ea", U'a', U'E', ios_defs::eofbit);
    FOri(U"a",   U'a', U'E', ios_defs::strfailbit);
    FOri(U"%Oa", U'a', U'O', ios_defs::eofbit);
    FOri(U"a",   U'a', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"水曜日", U'A', 0, ios_defs::eofbit).m_wday, 3);
    FOri(U"%EA", U'A', U'E', ios_defs::eofbit);
    FOri(U"A",   U'A', U'E', ios_defs::strfailbit);
    FOri(U"%OA", U'A', U'O', ios_defs::eofbit);
    FOri(U"A",   U'A', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"9月", U'b', 0, ios_defs::eofbit).m_month, 9);
    FOri(U"%Eb", U'b', U'E', ios_defs::eofbit);
    FOri(U"b",   U'b', U'E', ios_defs::strfailbit);
    FOri(U"%Ob", U'b', U'O', ios_defs::eofbit);
    FOri(U"b",   U'b', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"9月", U'B', 0, ios_defs::eofbit).m_month, 9);
    FOri(U"%EB", U'B', U'E', ios_defs::eofbit);
    FOri(U"B",   U'B', U'E', ios_defs::strfailbit);
    FOri(U"%OB", U'B', U'O', ios_defs::eofbit);
    FOri(U"B",   U'B', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"9月", U'h', 0, ios_defs::eofbit).m_month, 9);
    FOri(U"%Eh", U'h', U'E', ios_defs::eofbit);
    FOri(U"h",   U'h', U'E', ios_defs::strfailbit);
    FOri(U"%Oh", U'h', U'O', ios_defs::eofbit);
    FOri(U"h",   U'h', U'O', ios_defs::strfailbit);

    using namespace std::chrono;
    FYmd(U"%c", U'c', 0, ios_defs::eofbit);
    FYmd(U"%Ec", U'c', U'E', ios_defs::eofbit);
    FOri(U"c",   U'c', U'E', ios_defs::strfailbit);
    FOri(U"%Oc", U'c', U'O', ios_defs::eofbit);
    FOri(U"c",   U'c', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"20", U'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(FYmd(U"平成", U'C', U'E', ios_defs::eofbit).year(), std::chrono::year(1990));
    FOri(U"C",   U'C', U'E', ios_defs::strfailbit);
    FOri(U"%OC", U'C', U'O', ios_defs::eofbit);
    FOri(U"C",   U'C', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"04", U'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(U"04", U'd', U'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(U"四", U'd', U'O', ios_defs::eofbit).m_mday, 4);
    FOri(U"%Ed", U'd', U'E', ios_defs::eofbit);
    FOri(U"d",   U'd', U'E', ios_defs::strfailbit);
    FOri(U"d",   U'd', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"4", U'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(U"4", U'e', U'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(U"四", U'e', U'O', ios_defs::eofbit).m_mday, 4);
    FOri(U"%Ee", U'e', U'E', ios_defs::eofbit);
    FOri(U"e",   U'e', U'E', ios_defs::strfailbit);
    FOri(U"e",   U'e', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(U"2024-09-04", U'F', 0, ios_defs::eofbit), check_date1);
    FOri(U"%EF", U'F', U'E', ios_defs::eofbit);
    FOri(U"F",   U'F', U'E', ios_defs::strfailbit);
    FOri(U"%OF", U'F', U'O', ios_defs::eofbit);
    FOri(U"F",   U'F', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(U"2024年09月04日", U'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"令和6年09月04日", U'x', U'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"202409月04日", U'x', U'E', ios_defs::eofbit), check_date1);
    FOri(U"x",   U'x', U'E', ios_defs::strfailbit);
    FOri(U"%Ox", U'x', U'O', ios_defs::eofbit);
    FOri(U"x",   U'x', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(U"09/04/24", U'D', 0, ios_defs::eofbit), check_date1);
    FOri(U"%ED", U'D', U'E', ios_defs::eofbit);
    FOri(U"D",   U'D', U'E', ios_defs::strfailbit);
    FOri(U"%OD", U'D', U'O', ios_defs::eofbit);
    FOri(U"D",   U'D', U'O', ios_defs::strfailbit);

    FOri(U"%H", U'H', 0,   ios_defs::eofbit);
    FOri(U"%EH", U'H', U'E', ios_defs::eofbit);
    FOri(U"H",   U'H', U'E', ios_defs::strfailbit);
    FOri(U"H",   U'H', U'O', ios_defs::strfailbit);

    FOri(U"%I", U'I', 0,   ios_defs::eofbit);
    FOri(U"%EI", U'I', U'E', ios_defs::eofbit);
    FOri(U"I",   U'I', U'E', ios_defs::strfailbit);
    FOri(U"I",   U'I', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"248", U'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(FYmd(U"2024 248", U"%Y %j", ios_defs::eofbit), check_date1);
    FOri(U"%Ej", U'j', U'E', ios_defs::eofbit);
    FOri(U"j",   U'j', U'E', ios_defs::strfailbit);
    FOri(U"%Oj", U'j', U'O', ios_defs::eofbit);
    FOri(U"j",   U'j', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"09", U'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri(U"09", U'm', U'O', ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri(U"九", U'm', U'O', ios_defs::eofbit).m_month, 9);
    FOri(U"%Em", U'm', U'E', ios_defs::eofbit);
    FOri(U"m",   U'm', U'E', ios_defs::strfailbit);
    FOri(U"m",   U'm', U'O', ios_defs::strfailbit);

    FOri(U"%M", U'M', 0,   ios_defs::eofbit);
    FOri(U"%OM", U'M', U'O', ios_defs::eofbit);
    FOri(U"%EM", U'M', U'E', ios_defs::eofbit);
    FOri(U"M",   U'M', U'E', ios_defs::strfailbit);
    FOri(U"M",   U'M', U'O', ios_defs::strfailbit);

    FOri(U"\n",   U'n',  0,  ios_defs::eofbit);
    FOri(U"x",    U'n',  0,  ios_defs::goodbit);
    FOri(U"\n",   U'n', U'E', ios_defs::strfailbit);
    FOri(U"%En",  U'n', U'E', ios_defs::eofbit);
    FOri(U"n",    U'n', U'O', ios_defs::strfailbit);
    FOri(U"%On",  U'n', U'O', ios_defs::eofbit);

    FOri(U"\t",   U't',  0,  ios_defs::eofbit);
    FOri(U"x",    U't',  0,  ios_defs::goodbit);
    FOri(U"\t",   U't', U'E', ios_defs::strfailbit);
    FOri(U"%Et",  U't', U'E', ios_defs::eofbit);
    FOri(U"n",    U't', U'O', ios_defs::strfailbit);
    FOri(U"%Ot",  U't', U'O', ios_defs::eofbit);

    FOri(U"%p", U'p', 0, ios_defs::eofbit);
    FOri(U"%Ep", U'p', U'E', ios_defs::eofbit);
    FOri(U"p",   U'p', U'E', ios_defs::strfailbit);
    FOri(U"%Op", U'p', U'O', ios_defs::eofbit);
    FOri(U"p",   U'p', U'O', ios_defs::strfailbit);

    FOri(U"%r", U"%r",  ios_defs::eofbit);
    FOri(U"%Er", U'r', U'E', ios_defs::eofbit);
    FOri(U"r",   U'r', U'E', ios_defs::strfailbit);
    FOri(U"%Or", U'r', U'O', ios_defs::eofbit);
    FOri(U"r",   U'r', U'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33", U"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(U"%ER", U'R', U'E', ios_defs::eofbit);
    FOri(U"R",   U'R', U'E', ios_defs::strfailbit);
    FOri(U"%OR", U'R', U'O', ios_defs::eofbit);
    FOri(U"R",   U'R', U'O', ios_defs::strfailbit);

    FOri(U"%S", U'S', 0,   ios_defs::eofbit);
    FOri(U"%OS", U'S', U'O', ios_defs::eofbit);
    FOri(U"%ES", U'S', U'E', ios_defs::eofbit);
    FOri(U"S",   U'S', U'E', ios_defs::strfailbit);
    FOri(U"S",   U'S', U'O', ios_defs::strfailbit);

    FOri(U"%X", U"%X",  ios_defs::eofbit);
    FOri(U"%EX", U"%EX",  ios_defs::eofbit);
    FOri(U"X",   U'X', U'E', ios_defs::strfailbit);
    FOri(U"%OX", U'X', U'O', ios_defs::eofbit);
    FOri(U"X",   U'X', U'O', ios_defs::strfailbit);

    FOri(U"%T", U"%T",  ios_defs::eofbit);
    FOri(U"%ET", U'T', U'E', ios_defs::eofbit);
    FOri(U"T",   U'T', U'E', ios_defs::strfailbit);
    FOri(U"%OT", U'T', U'O', ios_defs::eofbit);
    FOri(U"T",   U'T', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"3", U'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(U"3", U'u', U'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(U"三", U'u', U'O', ios_defs::eofbit).m_wday, 3);
    FOri(U"%Eu", U'u', U'E', ios_defs::eofbit);
    FOri(U"u",   U'u', U'E', ios_defs::strfailbit);
    FOri(U"u",   U'u', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"24", U'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri(U"%Eg", U'g', U'E', ios_defs::eofbit);
    FOri(U"g",   U'g', U'E', ios_defs::strfailbit);
    FOri(U"%Og", U'g', U'O', ios_defs::eofbit);
    FOri(U"g",   U'g', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"2024", U'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri(U"%EG", U'G', U'E', ios_defs::eofbit);
    FOri(U"G",   U'G', U'E', ios_defs::strfailbit);
    FOri(U"%OG", U'G', U'O', ios_defs::eofbit);
    FOri(U"G",   U'G', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(U"2024 35 水", U"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"2024 35 水", U"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"2024 三十五 水", U"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri(U"35", U'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(FOri(U"35", U'U', U'O', ios_defs::eofbit).m_week_no, 35);
    FOri(U"%EU", U'U', U'E', ios_defs::eofbit);
    FOri(U"U",   U'U', U'E', ios_defs::strfailbit);
    FOri(U"U",   U'U', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(U"2024 36 水", U"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"2024 36 水", U"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(U"2024 三十六 水", U"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri(U"36", U'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(FOri(U"36", U'W', U'O', ios_defs::eofbit).m_week_no, 36);
    FOri(U"%EW", U'W', U'E', ios_defs::eofbit);
    FOri(U"W",   U'W', U'E', ios_defs::strfailbit);
    FOri(U"W",   U'W', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"36", U'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri(U"36", U'V', U'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri(U"三十六", U'V', U'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    FOri(U"54",  U'V', U'O', ios_defs::strfailbit);
    FOri(U"%EV", U'V', U'E', ios_defs::eofbit);
    FOri(U"V",   U'V', U'E', ios_defs::strfailbit);
    FOri(U"V",   U'V', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"3", U'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(U"3", U'w', U'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(U"三", U'w', U'O', ios_defs::eofbit).m_wday, 3);
    FOri(U"%Ew", U'w', U'E', ios_defs::eofbit);
    FOri(U"w",   U'w', U'E', ios_defs::strfailbit);
    FOri(U"w",   U'w', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"24", U'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd(U"6", U'y', U'E', ios_defs::eofbit).year(), std::chrono::year(2024));
    EXPECT_EQ(FOri(U"24", U'y', U'O', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri(U"二十四", U'y', U'O', ios_defs::eofbit).m_year, 2024);
    FOri(U"y",  U'y', U'E', ios_defs::strfailbit);
    FOri(U"y",  U'y', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"2024", U'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri(U"2024", U'Y', U'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd(U"平成3年", U'Y', U'E', ios_defs::eofbit).year(), std::chrono::year(1991));
    FOri(U"Y",   U'Y', U'E', ios_defs::strfailbit);
    FOri(U"%OY", U'Y', U'O', ios_defs::eofbit);
    FOri(U"Y",   U'Y', U'O', ios_defs::strfailbit);

    FOri(U"%Z", U'Z', 0, ios_defs::eofbit);
    FOri(U"%EZ", U'Z', U'E', ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'E', ios_defs::strfailbit);
    FOri(U"%OZ", U'Z', U'O', ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'O', ios_defs::strfailbit);

    FOri(U"%z", U'z', 0, ios_defs::eofbit);
    FOri(U"%Ez", U'z', U'E', ios_defs::eofbit);
    FOri(U"%Oz", U'z', U'O', ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(FYmd(U"1999-W52-6", U"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd(U"2019-W01-1", U"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd(U"1999-W52-5", U"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd(U"99-W52-6", U"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd(U"19-W01-1", U"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd(U"99-W52-5", U"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd(U"20 24/09/04", U"%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((FYmd(U"20 01 01", U"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar32, ATimeOfDayReadsEveryConversionSpecifierItCanSupply)
{
    timeio obj(std::make_shared<timeio_conf<char32_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<char32_t, false, true, tz_level::zone>, false, true, tz_level::zone>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, tz_level::zone>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(U"%",  U'%',  0,  ios_defs::eofbit);
    FOri(U"x",  U'%',  0,  ios_defs::strfailbit);
    FOri(U"%",  U'%', U'E', febit);
    FOri(U"%E%", U'%', U'E', ios_defs::eofbit);
    FOri(U"%",  U'%', U'O', febit);
    FOri(U"%O%", U'%', U'O', ios_defs::eofbit);

    FOri(U"%a", U'a', 0, ios_defs::eofbit);
    FOri(U"%Ea", U'a', U'E', ios_defs::eofbit);
    FOri(U"a",   U'a', U'E', ios_defs::strfailbit);
    FOri(U"%Oa", U'a', U'O', ios_defs::eofbit);
    FOri(U"a",   U'a', U'O', ios_defs::strfailbit);

    FOri(U"%A", U'A', 0, ios_defs::eofbit);
    FOri(U"%EA", U'A', U'E', ios_defs::eofbit);
    FOri(U"A",   U'A', U'E', ios_defs::strfailbit);
    FOri(U"%OA", U'A', U'O', ios_defs::eofbit);
    FOri(U"A",   U'A', U'O', ios_defs::strfailbit);

    FOri(U"%b", U'b', 0, ios_defs::eofbit);
    FOri(U"%Eb", U'b', U'E', ios_defs::eofbit);
    FOri(U"b",   U'b', U'E', ios_defs::strfailbit);
    FOri(U"%Ob", U'b', U'O', ios_defs::eofbit);
    FOri(U"b",   U'b', U'O', ios_defs::strfailbit);

    FOri(U"%B", U'B', 0, ios_defs::eofbit);
    FOri(U"%EB", U'B', U'E', ios_defs::eofbit);
    FOri(U"B",   U'B', U'E', ios_defs::strfailbit);
    FOri(U"%OB", U'B', U'O', ios_defs::eofbit);
    FOri(U"B",   U'B', U'O', ios_defs::strfailbit);

    FOri(U"%h", U'h', 0, ios_defs::eofbit);
    FOri(U"%Eh", U'h', U'E', ios_defs::eofbit);
    FOri(U"h",   U'h', U'E', ios_defs::strfailbit);
    FOri(U"%Oh", U'h', U'O', ios_defs::eofbit);
    FOri(U"h",   U'h', U'O', ios_defs::strfailbit);

    using namespace std::chrono;
    FOri(U"%c", U'c', 0, ios_defs::eofbit);
    FOri(U"%Ec", U'c', U'E', ios_defs::eofbit);
    FOri(U"c",   U'c', U'E', ios_defs::strfailbit);
    FOri(U"%Oc", U'c', U'O', ios_defs::eofbit);
    FOri(U"c",   U'c', U'O', ios_defs::strfailbit);

    FOri(U"%C", U'C', 0,   ios_defs::eofbit);
    FOri(U"%EC", U'C', U'E', ios_defs::eofbit);
    FOri(U"C",   U'C', U'E', ios_defs::strfailbit);
    FOri(U"%OC", U'C', U'O', ios_defs::eofbit);
    FOri(U"C",   U'C', U'O', ios_defs::strfailbit);

    FOri(U"%d", U'd', 0,   ios_defs::eofbit);
    FOri(U"%Od", U'd', U'O', ios_defs::eofbit);
    FOri(U"%Ed", U'd', U'E', ios_defs::eofbit);
    FOri(U"d",   U'd', U'E', ios_defs::strfailbit);
    FOri(U"d",   U'd', U'O', ios_defs::strfailbit);

    FOri(U"%e", U'e', 0,   ios_defs::eofbit);
    FOri(U"%Oe", U'e', U'O', ios_defs::eofbit);
    FOri(U"%Ee", U'e', U'E', ios_defs::eofbit);
    FOri(U"e",   U'e', U'E', ios_defs::strfailbit);
    FOri(U"e",   U'e', U'O', ios_defs::strfailbit);

    FOri(U"%F", U'F', 0, ios_defs::eofbit);
    FOri(U"%EF", U'F', U'E', ios_defs::eofbit);
    FOri(U"F",   U'F', U'E', ios_defs::strfailbit);
    FOri(U"%OF", U'F', U'O', ios_defs::eofbit);
    FOri(U"F",   U'F', U'O', ios_defs::strfailbit);

    FOri(U"%x", U'x', 0, ios_defs::eofbit);
    FOri(U"%Ex", U'x', U'E', ios_defs::eofbit);
    FOri(U"x",   U'x', U'E', ios_defs::strfailbit);
    FOri(U"%Ox", U'x', U'O', ios_defs::eofbit);
    FOri(U"x",   U'x', U'O', ios_defs::strfailbit);

    FOri(U"%D", U'D', 0, ios_defs::eofbit);
    FOri(U"%ED", U'D', U'E', ios_defs::eofbit);
    FOri(U"D",   U'D', U'E', ios_defs::strfailbit);
    FOri(U"%OD", U'D', U'O', ios_defs::eofbit);
    FOri(U"D",   U'D', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"13", U'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(U"13", U'H', U'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(U"十三", U'H', U'O', ios_defs::eofbit).m_hour, 13);
    FOri(U"%EH", U'H', U'E', ios_defs::eofbit);
    FOri(U"H",   U'H', U'E', ios_defs::strfailbit);
    FOri(U"H",   U'H', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"01", U'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(U"01", U'I', U'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(U"一", U'I', U'O', ios_defs::eofbit).m_hour, 1);
    FOri(U"%EI", U'I', U'E', ios_defs::eofbit);
    FOri(U"I",   U'I', U'E', ios_defs::strfailbit);
    FOri(U"I",   U'I', U'O', ios_defs::strfailbit);

    FOri(U"%j", U'j', 0, ios_defs::eofbit);
    FOri(U"%Ej", U'j', U'E', ios_defs::eofbit);
    FOri(U"j",   U'j', U'E', ios_defs::strfailbit);
    FOri(U"%Oj", U'j', U'O', ios_defs::eofbit);
    FOri(U"j",   U'j', U'O', ios_defs::strfailbit);

    FOri(U"%m", U'm',  0, ios_defs::eofbit);
    FOri(U"%Om", U'm', U'O', ios_defs::eofbit);
    FOri(U"%Em", U'm', U'E', ios_defs::eofbit);
    FOri(U"m",   U'm', U'E', ios_defs::strfailbit);
    FOri(U"m",   U'm', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"33", U'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(U"33", U'M', U'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(U"三十三", U'M', U'O', ios_defs::eofbit).m_minute, 33);
    FOri(U"%EM", U'M', U'E', ios_defs::eofbit);
    FOri(U"M",   U'M', U'E', ios_defs::strfailbit);
    FOri(U"M",   U'M', U'O', ios_defs::strfailbit);

    FOri(U"\n",   U'n',  0,  ios_defs::eofbit);
    FOri(U"x",    U'n',  0,  ios_defs::goodbit);
    FOri(U"\n",   U'n', U'E', ios_defs::strfailbit);
    FOri(U"%En",  U'n', U'E', ios_defs::eofbit);
    FOri(U"n",    U'n', U'O', ios_defs::strfailbit);
    FOri(U"%On",  U'n', U'O', ios_defs::eofbit);

    FOri(U"\t",   U't',  0,  ios_defs::eofbit);
    FOri(U"x",    U't',  0,  ios_defs::goodbit);
    FOri(U"\t",   U't', U'E', ios_defs::strfailbit);
    FOri(U"%Et",  U't', U'E', ios_defs::eofbit);
    FOri(U"n",    U't', U'O', ios_defs::strfailbit);
    FOri(U"%Ot",  U't', U'O', ios_defs::eofbit);

    EXPECT_EQ(FHms(U"01 午後", U"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(FHms(U"01 午前", U"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(FOri(U"午後", U'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(FOri(U"午前", U'p', 0, ios_defs::eofbit).m_is_pm, false);
    FOri(U"%Ep", U'p', U'E', ios_defs::eofbit);
    FOri(U"p",   U'p', U'E', ios_defs::strfailbit);
    FOri(U"%Op", U'p', U'O', ios_defs::eofbit);
    FOri(U"p",   U'p', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(U"午後01時33分18秒", U"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"%Er", U'r', U'E', ios_defs::eofbit);
    FOri(U"r",   U'r', U'E', ios_defs::strfailbit);
    FOri(U"%Or", U'r', U'O', ios_defs::eofbit);
    FOri(U"r",   U'r', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(U"13:33", U"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(U"%ER", U'R', U'E', ios_defs::eofbit);
    FOri(U"R",   U'R', U'E', ios_defs::strfailbit);
    FOri(U"%OR", U'R', U'O', ios_defs::eofbit);
    FOri(U"R",   U'R', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"18", U'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(U"18", U'S', U'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(U"十八", U'S', U'O', ios_defs::eofbit).m_second, 18);
    FOri(U"%ES", U'S', U'E', ios_defs::eofbit);
    FOri(U"S",   U'S', U'E', ios_defs::strfailbit);
    FOri(U"S",   U'S', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(U"13時33分18秒", U"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(FHms(U"13時33分18秒", U"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"X",   U'X', U'E', ios_defs::strfailbit);
    FOri(U"%OX", U'X', U'O', ios_defs::eofbit);
    FOri(U"X",   U'X', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(U"13:33:18", U"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"%ET", U'T', U'E', ios_defs::eofbit);
    FOri(U"T",   U'T', U'E', ios_defs::strfailbit);
    FOri(U"%OT", U'T', U'O', ios_defs::eofbit);
    FOri(U"T",   U'T', U'O', ios_defs::strfailbit);

    FOri(U"%u", U'u', 0,   ios_defs::eofbit);
    FOri(U"%Ou", U'u', U'O', ios_defs::eofbit);
    FOri(U"%Eu", U'u', U'E', ios_defs::eofbit);
    FOri(U"u",   U'u', U'E', ios_defs::strfailbit);
    FOri(U"u",   U'u', U'O', ios_defs::strfailbit);

    FOri(U"%g", U'g', 0, ios_defs::eofbit);
    FOri(U"%Eg", U'g', U'E', ios_defs::eofbit);
    FOri(U"g",   U'g', U'E', ios_defs::strfailbit);
    FOri(U"%Og", U'g', U'O', ios_defs::eofbit);
    FOri(U"g",   U'g', U'O', ios_defs::strfailbit);

    FOri(U"%G", U'G', 0, ios_defs::eofbit);
    FOri(U"%EG", U'G', U'E', ios_defs::eofbit);
    FOri(U"G",   U'G', U'E', ios_defs::strfailbit);
    FOri(U"%OG", U'G', U'O', ios_defs::eofbit);
    FOri(U"G",   U'G', U'O', ios_defs::strfailbit);

    FOri(U"%U", U'U', 0,   ios_defs::eofbit);
    FOri(U"%OU", U'U', U'O', ios_defs::eofbit);
    FOri(U"%EU", U'U', U'E', ios_defs::eofbit);
    FOri(U"U",   U'U', U'E', ios_defs::strfailbit);
    FOri(U"U",   U'U', U'O', ios_defs::strfailbit);

    FOri(U"%W", U'W', 0,   ios_defs::eofbit);
    FOri(U"%OW", U'W', U'O', ios_defs::eofbit);
    FOri(U"%EW", U'W', U'E', ios_defs::eofbit);
    FOri(U"W",   U'W', U'E', ios_defs::strfailbit);
    FOri(U"W",   U'W', U'O', ios_defs::strfailbit);

    FOri(U"%V", U'V', 0,   ios_defs::eofbit);
    FOri(U"%OV", U'V', U'O',   ios_defs::eofbit);
    FOri(U"54",  U'V', U'O', ios_defs::strfailbit);
    FOri(U"%EV", U'V', U'E', ios_defs::eofbit);
    FOri(U"V",   U'V', U'E', ios_defs::strfailbit);
    FOri(U"V",   U'V', U'O', ios_defs::strfailbit);

    FOri(U"%w", U'w', 0,   ios_defs::eofbit);
    FOri(U"%Ow", U'w', U'O', ios_defs::eofbit);
    FOri(U"%Ew", U'w', U'E', ios_defs::eofbit);
    FOri(U"w",   U'w', U'E', ios_defs::strfailbit);
    FOri(U"w",   U'w', U'O', ios_defs::strfailbit);

    FOri(U"%y", U'y', 0,   ios_defs::eofbit);
    FOri(U"%Ey", U'y', U'E', ios_defs::eofbit);
    FOri(U"%Oy", U'y', U'O', ios_defs::eofbit);
    FOri(U"y",  U'y', U'E', ios_defs::strfailbit);
    FOri(U"y",  U'y', U'O', ios_defs::strfailbit);

    FOri(U"%Y", U'Y', 0,   ios_defs::eofbit);
    FOri(U"%EY", U'Y', U'E', ios_defs::eofbit);
    FOri(U"Y",   U'Y', U'E', ios_defs::strfailbit);
    FOri(U"%OY", U'Y', U'O', ios_defs::eofbit);
    FOri(U"Y",   U'Y', U'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(FOri(U"America/Los_Angeles", U'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = FOri(U"PST", U'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    FOri(U"America/Los_Angexes", U'Z', 0, ios_defs::strfailbit);
    FOri(U"%EZ", U'Z', U'E', ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'E', ios_defs::strfailbit);
    FOri(U"%OZ", U'Z', U'O', ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'O', ios_defs::strfailbit);

    { auto r = FOri(U"+0800", U'z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_have_offset && r.m_offset == minutes{480}); }
    FOri(U"%z", U'z', 0, ios_defs::strfailbit);
    FOri(U"%Ez", U'z', U'E', ios_defs::eofbit);
    FOri(U"z",  U'z', U'E', ios_defs::strfailbit);
    FOri(U"%Oz", U'z', U'O', ios_defs::eofbit);
    FOri(U"z",  U'z', U'O', ios_defs::strfailbit);
}

TEST(TimeioChar32, ATimeOfDayReadsTheSameSpecifiersWithNoZoneTier)
{
    timeio obj(std::make_shared<timeio_conf<char32_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<char32_t, false, true, tz_level::none>, false, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(U"%",  U'%',  0,  ios_defs::eofbit);
    FOri(U"x",  U'%',  0,  ios_defs::strfailbit);
    FOri(U"%",  U'%', U'E', febit);
    FOri(U"%E%", U'%', U'E', ios_defs::eofbit);
    FOri(U"%",  U'%', U'O', febit);
    FOri(U"%O%", U'%', U'O', ios_defs::eofbit);

    FOri(U"%a", U'a', 0, ios_defs::eofbit);
    FOri(U"%Ea", U'a', U'E', ios_defs::eofbit);
    FOri(U"a",   U'a', U'E', ios_defs::strfailbit);
    FOri(U"%Oa", U'a', U'O', ios_defs::eofbit);
    FOri(U"a",   U'a', U'O', ios_defs::strfailbit);

    FOri(U"%A", U'A', 0, ios_defs::eofbit);
    FOri(U"%EA", U'A', U'E', ios_defs::eofbit);
    FOri(U"A",   U'A', U'E', ios_defs::strfailbit);
    FOri(U"%OA", U'A', U'O', ios_defs::eofbit);
    FOri(U"A",   U'A', U'O', ios_defs::strfailbit);

    FOri(U"%b", U'b', 0, ios_defs::eofbit);
    FOri(U"%Eb", U'b', U'E', ios_defs::eofbit);
    FOri(U"b",   U'b', U'E', ios_defs::strfailbit);
    FOri(U"%Ob", U'b', U'O', ios_defs::eofbit);
    FOri(U"b",   U'b', U'O', ios_defs::strfailbit);

    FOri(U"%B", U'B', 0, ios_defs::eofbit);
    FOri(U"%EB", U'B', U'E', ios_defs::eofbit);
    FOri(U"B",   U'B', U'E', ios_defs::strfailbit);
    FOri(U"%OB", U'B', U'O', ios_defs::eofbit);
    FOri(U"B",   U'B', U'O', ios_defs::strfailbit);

    FOri(U"%h", U'h', 0, ios_defs::eofbit);
    FOri(U"%Eh", U'h', U'E', ios_defs::eofbit);
    FOri(U"h",   U'h', U'E', ios_defs::strfailbit);
    FOri(U"%Oh", U'h', U'O', ios_defs::eofbit);
    FOri(U"h",   U'h', U'O', ios_defs::strfailbit);

    using namespace std::chrono;
    FOri(U"%c", U'c', 0, ios_defs::eofbit);
    FOri(U"%Ec", U'c', U'E', ios_defs::eofbit);
    FOri(U"c",   U'c', U'E', ios_defs::strfailbit);
    FOri(U"%Oc", U'c', U'O', ios_defs::eofbit);
    FOri(U"c",   U'c', U'O', ios_defs::strfailbit);

    FOri(U"%C", U'C', 0,   ios_defs::eofbit);
    FOri(U"%EC", U'C', U'E', ios_defs::eofbit);
    FOri(U"C",   U'C', U'E', ios_defs::strfailbit);
    FOri(U"%OC", U'C', U'O', ios_defs::eofbit);
    FOri(U"C",   U'C', U'O', ios_defs::strfailbit);

    FOri(U"%d", U'd', 0,   ios_defs::eofbit);
    FOri(U"%Od", U'd', U'O', ios_defs::eofbit);
    FOri(U"%Ed", U'd', U'E', ios_defs::eofbit);
    FOri(U"d",   U'd', U'E', ios_defs::strfailbit);
    FOri(U"d",   U'd', U'O', ios_defs::strfailbit);

    FOri(U"%e", U'e', 0,   ios_defs::eofbit);
    FOri(U"%Oe", U'e', U'O', ios_defs::eofbit);
    FOri(U"%Ee", U'e', U'E', ios_defs::eofbit);
    FOri(U"e",   U'e', U'E', ios_defs::strfailbit);
    FOri(U"e",   U'e', U'O', ios_defs::strfailbit);

    FOri(U"%F", U'F', 0, ios_defs::eofbit);
    FOri(U"%EF", U'F', U'E', ios_defs::eofbit);
    FOri(U"F",   U'F', U'E', ios_defs::strfailbit);
    FOri(U"%OF", U'F', U'O', ios_defs::eofbit);
    FOri(U"F",   U'F', U'O', ios_defs::strfailbit);

    FOri(U"%x", U'x', 0, ios_defs::eofbit);
    FOri(U"%Ex", U'x', U'E', ios_defs::eofbit);
    FOri(U"x",   U'x', U'E', ios_defs::strfailbit);
    FOri(U"%Ox", U'x', U'O', ios_defs::eofbit);
    FOri(U"x",   U'x', U'O', ios_defs::strfailbit);

    FOri(U"%D", U'D', 0, ios_defs::eofbit);
    FOri(U"%ED", U'D', U'E', ios_defs::eofbit);
    FOri(U"D",   U'D', U'E', ios_defs::strfailbit);
    FOri(U"%OD", U'D', U'O', ios_defs::eofbit);
    FOri(U"D",   U'D', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"13", U'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(U"13", U'H', U'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(U"十三", U'H', U'O', ios_defs::eofbit).m_hour, 13);
    FOri(U"%EH", U'H', U'E', ios_defs::eofbit);
    FOri(U"H",   U'H', U'E', ios_defs::strfailbit);
    FOri(U"H",   U'H', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"01", U'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(U"01", U'I', U'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(U"一", U'I', U'O', ios_defs::eofbit).m_hour, 1);
    FOri(U"%EI", U'I', U'E', ios_defs::eofbit);
    FOri(U"I",   U'I', U'E', ios_defs::strfailbit);
    FOri(U"I",   U'I', U'O', ios_defs::strfailbit);

    FOri(U"%j", U'j', 0, ios_defs::eofbit);
    FOri(U"%Ej", U'j', U'E', ios_defs::eofbit);
    FOri(U"j",   U'j', U'E', ios_defs::strfailbit);
    FOri(U"%Oj", U'j', U'O', ios_defs::eofbit);
    FOri(U"j",   U'j', U'O', ios_defs::strfailbit);

    FOri(U"%m", U'm',  0, ios_defs::eofbit);
    FOri(U"%Om", U'm', U'O', ios_defs::eofbit);
    FOri(U"%Em", U'm', U'E', ios_defs::eofbit);
    FOri(U"m",   U'm', U'E', ios_defs::strfailbit);
    FOri(U"m",   U'm', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"33", U'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(U"33", U'M', U'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(U"三十三", U'M', U'O', ios_defs::eofbit).m_minute, 33);
    FOri(U"%EM", U'M', U'E', ios_defs::eofbit);
    FOri(U"M",   U'M', U'E', ios_defs::strfailbit);
    FOri(U"M",   U'M', U'O', ios_defs::strfailbit);

    FOri(U"\n",   U'n',  0,  ios_defs::eofbit);
    FOri(U"x",    U'n',  0,  ios_defs::goodbit);
    FOri(U"\n",   U'n', U'E', ios_defs::strfailbit);
    FOri(U"%En",  U'n', U'E', ios_defs::eofbit);
    FOri(U"n",    U'n', U'O', ios_defs::strfailbit);
    FOri(U"%On",  U'n', U'O', ios_defs::eofbit);

    FOri(U"\t",   U't',  0,  ios_defs::eofbit);
    FOri(U"x",    U't',  0,  ios_defs::goodbit);
    FOri(U"\t",   U't', U'E', ios_defs::strfailbit);
    FOri(U"%Et",  U't', U'E', ios_defs::eofbit);
    FOri(U"n",    U't', U'O', ios_defs::strfailbit);
    FOri(U"%Ot",  U't', U'O', ios_defs::eofbit);

    EXPECT_EQ(FHms(U"01 午後", U"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(FHms(U"01 午前", U"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(FOri(U"午後", U'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(FOri(U"午前", U'p', 0, ios_defs::eofbit).m_is_pm, false);
    FOri(U"%Ep", U'p', U'E', ios_defs::eofbit);
    FOri(U"p",   U'p', U'E', ios_defs::strfailbit);
    FOri(U"%Op", U'p', U'O', ios_defs::eofbit);
    FOri(U"p",   U'p', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(U"午後01時33分18秒", U"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"%Er", U'r', U'E', ios_defs::eofbit);
    FOri(U"r",   U'r', U'E', ios_defs::strfailbit);
    FOri(U"%Or", U'r', U'O', ios_defs::eofbit);
    FOri(U"r",   U'r', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(U"13:33", U"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(U"%ER", U'R', U'E', ios_defs::eofbit);
    FOri(U"R",   U'R', U'E', ios_defs::strfailbit);
    FOri(U"%OR", U'R', U'O', ios_defs::eofbit);
    FOri(U"R",   U'R', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(U"18", U'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(U"18", U'S', U'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(U"十八", U'S', U'O', ios_defs::eofbit).m_second, 18);
    FOri(U"%ES", U'S', U'E', ios_defs::eofbit);
    FOri(U"S",   U'S', U'E', ios_defs::strfailbit);
    FOri(U"S",   U'S', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(U"13時33分18秒", U"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(FHms(U"13時33分18秒", U"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"X",   U'X', U'E', ios_defs::strfailbit);
    FOri(U"%OX", U'X', U'O', ios_defs::eofbit);
    FOri(U"X",   U'X', U'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(U"13:33:18", U"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"%ET", U'T', U'E', ios_defs::eofbit);
    FOri(U"T",   U'T', U'E', ios_defs::strfailbit);
    FOri(U"%OT", U'T', U'O', ios_defs::eofbit);
    FOri(U"T",   U'T', U'O', ios_defs::strfailbit);

    FOri(U"%u", U'u', 0,   ios_defs::eofbit);
    FOri(U"%Ou", U'u', U'O', ios_defs::eofbit);
    FOri(U"%Eu", U'u', U'E', ios_defs::eofbit);
    FOri(U"u",   U'u', U'E', ios_defs::strfailbit);
    FOri(U"u",   U'u', U'O', ios_defs::strfailbit);

    FOri(U"%g", U'g', 0, ios_defs::eofbit);
    FOri(U"%Eg", U'g', U'E', ios_defs::eofbit);
    FOri(U"g",   U'g', U'E', ios_defs::strfailbit);
    FOri(U"%Og", U'g', U'O', ios_defs::eofbit);
    FOri(U"g",   U'g', U'O', ios_defs::strfailbit);

    FOri(U"%G", U'G', 0, ios_defs::eofbit);
    FOri(U"%EG", U'G', U'E', ios_defs::eofbit);
    FOri(U"G",   U'G', U'E', ios_defs::strfailbit);
    FOri(U"%OG", U'G', U'O', ios_defs::eofbit);
    FOri(U"G",   U'G', U'O', ios_defs::strfailbit);

    FOri(U"%U", U'U', 0,   ios_defs::eofbit);
    FOri(U"%OU", U'U', U'O', ios_defs::eofbit);
    FOri(U"%EU", U'U', U'E', ios_defs::eofbit);
    FOri(U"U",   U'U', U'E', ios_defs::strfailbit);
    FOri(U"U",   U'U', U'O', ios_defs::strfailbit);

    FOri(U"%W", U'W', 0,   ios_defs::eofbit);
    FOri(U"%OW", U'W', U'O', ios_defs::eofbit);
    FOri(U"%EW", U'W', U'E', ios_defs::eofbit);
    FOri(U"W",   U'W', U'E', ios_defs::strfailbit);
    FOri(U"W",   U'W', U'O', ios_defs::strfailbit);

    FOri(U"%V", U'V', 0,   ios_defs::eofbit);
    FOri(U"%OV", U'V', U'O',   ios_defs::eofbit);
    FOri(U"54",  U'V', U'O', ios_defs::strfailbit);
    FOri(U"%EV", U'V', U'E', ios_defs::eofbit);
    FOri(U"V",   U'V', U'E', ios_defs::strfailbit);
    FOri(U"V",   U'V', U'O', ios_defs::strfailbit);

    FOri(U"%w", U'w', 0,   ios_defs::eofbit);
    FOri(U"%Ow", U'w', U'O', ios_defs::eofbit);
    FOri(U"%Ew", U'w', U'E', ios_defs::eofbit);
    FOri(U"w",   U'w', U'E', ios_defs::strfailbit);
    FOri(U"w",   U'w', U'O', ios_defs::strfailbit);

    FOri(U"%y", U'y', 0,   ios_defs::eofbit);
    FOri(U"%Ey", U'y', U'E', ios_defs::eofbit);
    FOri(U"%Oy", U'y', U'O', ios_defs::eofbit);
    FOri(U"y",  U'y', U'E', ios_defs::strfailbit);
    FOri(U"y",  U'y', U'O', ios_defs::strfailbit);

    FOri(U"%Y", U'Y', 0,   ios_defs::eofbit);
    FOri(U"%EY", U'Y', U'E', ios_defs::eofbit);
    FOri(U"Y",   U'Y', U'E', ios_defs::strfailbit);
    FOri(U"%OY", U'Y', U'O', ios_defs::eofbit);
    FOri(U"Y",   U'Y', U'O', ios_defs::strfailbit);

    FOri(U"%Z", U'Z', 0, ios_defs::eofbit);
    FOri(U"%EZ", U'Z', U'E', ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'E', ios_defs::strfailbit);
    FOri(U"%OZ", U'Z', U'O', ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'O', ios_defs::strfailbit);

    FOri(U"%z", U'z', 0, ios_defs::eofbit);
    FOri(U"%Ez", U'z', U'E', ios_defs::eofbit);
    FOri(U"z",  U'z', U'E', ios_defs::strfailbit);
    FOri(U"%Oz", U'z', U'O', ios_defs::eofbit);
    FOri(U"z",  U'z', U'O', ios_defs::strfailbit);
}

TEST(TimeioChar32, AValueThatIsNotAValidTimeIsRejected)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char32_t>>("C"));
    std::u32string res;

    // put(year_month_day) with invalid date (line 1173)
    {
        auto invalid_ymd = year_month_day{year{2024}, month{2}, day{30}};
        EXPECT_THROW(obj.put(std::back_inserter(res), invalid_ymd, std::u32string_view(U"%F")), stream_error);
    }

    // put(hh_mm_ss) with negative total duration (line 1214)
    {
        hh_mm_ss<seconds> invalid_hms{seconds{-1}};
        EXPECT_THROW(obj.put(std::back_inserter(res), invalid_hms, std::u32string_view(U"%T")), stream_error);
    }

    // put(std::tm) with out-of-range field: month=-1 (line 1271)
    {
        std::tm bad_tm{};
        bad_tm.tm_year = 124; bad_tm.tm_mon = -1;
        bad_tm.tm_mday = 1; bad_tm.tm_hour = 0; bad_tm.tm_min = 0; bad_tm.tm_sec = 0;
        EXPECT_THROW(obj.put(std::back_inserter(res), bad_tm, std::u32string_view(U"%F")), stream_error);
    }

    // put(std::tm) with valid fields but invalid date: Feb 30 (line 1275)
    {
        std::tm bad_tm{};
        bad_tm.tm_year = 124; bad_tm.tm_mon = 1; bad_tm.tm_mday = 30;
        bad_tm.tm_hour = 0; bad_tm.tm_min = 0; bad_tm.tm_sec = 0;
        EXPECT_THROW(obj.put(std::back_inserter(res), bad_tm, std::u32string_view(U"%F")), stream_error);
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
            EXPECT_THROW(obj.put(std::back_inserter(res), bad_tm, std::u32string_view(U"%Y")), stream_error);
        }
    }

    // put(std::tm) at the year bounds themselves: still accepted
    {
        std::tm edge_tm{};
        edge_tm.tm_mon = 0; edge_tm.tm_mday = 1;
        edge_tm.tm_hour = 0; edge_tm.tm_min = 0; edge_tm.tm_sec = 0;

        edge_tm.tm_year = static_cast<int>(year::max()) - 1900;
        res.clear(); obj.put(std::back_inserter(res), edge_tm, std::u32string_view(U"%Y"));
        EXPECT_EQ(res, U"32767");

        edge_tm.tm_year = static_cast<int>(year::min()) - 1900;
        res.clear(); obj.put(std::back_inserter(res), edge_tm, std::u32string_view(U"%Y"));
        EXPECT_EQ(res, U"-32767");
    }

    // put(year_month_day) with negative year: %Y and %C output sign (lines 2860-2861, 2543-2544)
    {
        auto neg_ymd = year_month_day{year{-1}, month{1}, day{1}};
        res.clear(); obj.put(std::back_inserter(res), neg_ymd, std::u32string_view(U"%Y"));
        EXPECT_EQ(res, U"-0001");
        res.clear(); obj.put(std::back_inserter(res), neg_ymd, std::u32string_view(U"%C"));
        EXPECT_EQ(res, U"-01");
    }

    // put(year_month_day) for date in ISO year -1: %G output sign (lines 2608-2609)
    // Jan 1, year 0 is a Saturday; Thu of that ISO week is Dec 30, year -1 -> G=-0001
    {
        auto early_ymd = year_month_day{year{0}, month{1}, day{1}};
        res.clear(); obj.put(std::back_inserter(res), early_ymd, std::u32string_view(U"%G"));
        EXPECT_EQ(res, U"-0001");
    }

    // put(zoned_time) with positive offset: %z outputs '+' (line 2883)
    {
        auto tp = create_zoned_time(2024, 9, 4, 12, 0, 0, "Asia/Tokyo");
        res.clear(); obj.put(std::back_inserter(res), tp, std::u32string_view(U"%z"));
        EXPECT_EQ(res, U"+0900");
    }
}

// A format string ending in a lone '%' -- or in a lone '%E' / '%O' modifier -- introduces no
// specifier, so there is nothing to convert. It follows the same rule this facet already uses
// for a specifier it does not recognize (see the "unknown format" path, which emits '%' plus
// the rest verbatim): put writes the '%' out and get matches it back as a literal. Handling
// the two sides alike is what keeps the round-trip invariant -- whatever put writes, get reads
// back with the same format string. put previously dropped the '%' silently while get rejected
// it, so put succeeded on output get could never read.
TEST(TimeioChar32, ALoneOrUnknownSpecifierIsEchoedVerbatim)
{
    timeio obj(std::make_shared<timeio_conf<char32_t>>("C"));
    const std::tm t = calendar_time(124, 0, 15, 1, 2, 3, 1, 14, 0);

    struct { const char32_t* fmt; const char32_t* want; } cases[] = {
        {U"%Y%", U"2024%"},   // a lone U'%' after a real specifier
        {U"%",   U"%"},       // nothing but the lone U'%'
        {U"a%",  U"a%"},      // a lone U'%' after literal text
        {U"%E",  U"%E"},      // a lone U'E' modifier with no specifier to modify
        {U"%O",  U"%O"},      // ditto for U'O'
        {U"%%",  U"%"},       // control: an escaped U'%' still collapses to one
        {U"%Q",  U"%Q"},      // control: an unrecognized specifier is already emitted verbatim
    };

    for (const auto& c : cases)
    {
        std::u32string res;
        obj.put(std::back_inserter(res), t, std::u32string_view(c.fmt));
        EXPECT_EQ(res, c.want);

        // The round trip: get consumes exactly what put produced, using the same format.
        time_parse_context<char32_t> ctx;
        EXPECT_EQ(obj.get(res.begin(), res.end(), ctx, std::u32string_view(c.fmt)), res.end());
    }

    // get still rejects input that lacks the literal '%' the format asks for, so the
    // agreement above is a real match rather than the trailing '%' being ignored.
    {
        const std::u32string in = U"2024";
        time_parse_context<char32_t> ctx;
        EXPECT_THROW(obj.get(in.begin(), in.end(), ctx, std::u32string_view(U"%Y%")), stream_error);
    }
}

// The two tiers pinned apart. Whether %Z parses is the tier's decision and nothing else's:
// tz_level::offset matches it literally, which is exactly what put degrades it to for a value
// with no zone to name, and tz_level::zone parses it against the trie. Neither tier looks at
// what the trie happens to contain to decide which of the two it is doing.
TEST(TimeioChar32, TheZoneTierDecidesHowAZoneNameIsParsed)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char32_t>>("C"));

    auto off_ok = [&obj](const std::u32string& in, const char32_t* fmt)
    {
        time_parse_context<char32_t, true, true, tz_level::offset> ctx;
        try { return obj.get(in.begin(), in.end(), ctx, std::u32string_view(fmt)) == in.end(); }
        catch (stream_error&) { return false; }
    };
    auto zone_ok = [&obj](const std::u32string& in, const char32_t* fmt)
    {
        time_parse_context<char32_t, true, true, tz_level::zone> ctx;
        try { return obj.get(in.begin(), in.end(), ctx, std::u32string_view(fmt)) == in.end(); }
        catch (stream_error&) { return false; }
    };

    // The literal %Z, which is what put writes when the value has no zone to offer.
    EXPECT_TRUE(off_ok(U"%Z", U"%Z"));
    EXPECT_FALSE(zone_ok(U"%Z", U"%Z"));

    // A real zone token parses at tz_level::zone and only there. At tz_level::offset the format
    // is asking for the two characters %Z, which "UTC" is not -- put never writes a zone token
    // for a value that parses at that tier, so there is nothing to read back.
    EXPECT_TRUE(zone_ok(U"UTC", U"%Z"));
    EXPECT_FALSE(off_ok(U"UTC", U"%Z"));
    EXPECT_TRUE(zone_ok(U"PDT", U"%Z"));
    EXPECT_FALSE(off_ok(U"PDT", U"%Z"));

    // A run of letters the database does not know is rejected at both, for different reasons:
    // no trie entry at one tier, no literal match at the other.
    EXPECT_FALSE(zone_ok(U"XYZ", U"%Z"));
    EXPECT_FALSE(off_ok(U"XYZ", U"%Z"));

    // The literal is for *this* specifier, not for any percent sequence.
    EXPECT_FALSE(off_ok(U"%z", U"%Z"));
    EXPECT_FALSE(off_ok(U"%Q", U"%Z"));

    // The round trip it exists for: a std::tm with no zone, through a format carrying %Z. Each
    // platform reads it back at the tier its own std::tm sits at. With tm_zone the field exists
    // but names nothing, so put writes the unknown-zone token and the zone tier reads it back;
    // without the extension members the type has no zone at all, put degrades %Z to a literal,
    // and the tiers below zone match that literal. Either way it closes.
    {
        std::tm t{};
        t.tm_year = 124; t.tm_mon = 8; t.tm_mday = 4;
        t.tm_hour = 13; t.tm_min = 33; t.tm_sec = 18;

        std::u32string res;
        obj.put(std::back_inserter(res), t, std::u32string_view(U"%F %T %Z"));
#ifdef __USE_MISC
        EXPECT_EQ(res, U"2024-09-04 13:33:18 UNKNOWN");
        EXPECT_TRUE(zone_ok(res, U"%F %T %Z"));
#else
        EXPECT_EQ(res, U"2024-09-04 13:33:18 %Z");
        EXPECT_TRUE(off_ok(res, U"%F %T %Z"));
#endif
    }

    // The same round trip through a locale whose own %c carries %Z, which is how this reaches
    // a caller who never wrote %Z: put_time(&t, "%c") on a tm that get_time filled in.
    {
        timeio us(std::make_shared<timeio_conf<char32_t>>("en_US.UTF-8"));
        std::tm t{};
        t.tm_year = 124; t.tm_mon = 8; t.tm_mday = 4;
        t.tm_hour = 13; t.tm_min = 33; t.tm_sec = 18;

        std::u32string res;
        us.put(std::back_inserter(res), t, std::u32string_view(U"%c"));

        time_parse_context<char32_t, true, true, tz_level::zone> ctx;
        EXPECT_EQ(us.get(res.begin(), res.end(), ctx, std::u32string_view(U"%c")), res.end());
    }
}
