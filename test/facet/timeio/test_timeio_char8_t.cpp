// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same conversion-specifier contract as test_timeio_char.cpp for char8_t.  The
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

    timeio<char8_t> facet_for(const char* loc)
    {
        return timeio<char8_t>(std::make_shared<timeio_conf<char8_t>>(loc));
    }

    template <typename TVal, typename... TSpec>
    std::u8string put_one(const timeio<char8_t>& obj, const TVal& tp, TSpec... spec)
    {
        std::u8string res;
        obj.put(std::back_inserter(res), tp, spec...);
        return res;
    }

    // One conversion specifier, the modifier applied to it, and what the facet
    // writes.  A specifier the value cannot supply comes back as the format text
    // that asked for it, which is why so many rows read "%Ea" and the like.
    struct conversion
    {
        char8_t     spec;
        char8_t     mod;
        const char8_t* expected;
    };

    template <typename TVal, std::size_t N>
    void expect_conversions(const timeio<char8_t>& obj, const TVal& tp, const conversion (&table)[N])
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
    template <typename T = time_parse_context<char8_t>, bool HaveDate = true, bool HaveTime = true,
              tz_level TzLevel = tz_level::zone, typename... TFmt>
    T run_get(const timeio<char8_t>& obj, const std::u8string& input,
              ios_defs::iostate err_exp, TFmt... fmt)
    {
        time_parse_context<char8_t, HaveDate, HaveTime, TzLevel> ctx1, ctx2, ctx3;
        std::list<char8_t> lst(input.begin(), input.end());
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
    template <typename T = time_parse_context<char8_t>, bool HaveDate = true, bool HaveTime = true,
              tz_level TzLevel = tz_level::zone>
    T CheckGet(const timeio<char8_t>& obj, const std::u8string& input, char fmt, char modif,
               ios_defs::iostate err_exp)
    {
        SCOPED_TRACE(::testing::PrintToString(input) + " | %"
                     + (modif ? std::string(1, static_cast<char>(modif)) : std::string())
                     + static_cast<char>(fmt));
        return run_get<T, HaveDate, HaveTime, TzLevel>(obj, input, err_exp, fmt, modif);
    }

    template <typename T = time_parse_context<char8_t>, bool HaveDate = true, bool HaveTime = true,
              tz_level TzLevel = tz_level::zone>
    T CheckGet(const timeio<char8_t>& obj, const std::u8string& input, const std::u8string& fmt,
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
TEST(TimeioChar8, CLocaleWritesEveryConversionSpecifier)
{
    const timeio<char8_t> obj = facet_for("C");
    const auto            tp  = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    static const conversion kConversions[] = {
        {u8'%', 0, u8"%"},
        {u8'a', 0, u8"Wed"},
        {u8'a', u8'E', u8"%Ea"},
        {u8'a', u8'O', u8"%Oa"},
        {u8'A', 0, u8"Wednesday"},
        {u8'A', u8'E', u8"%EA"},
        {u8'A', u8'O', u8"%OA"},
        {u8'b', 0, u8"Sep"},
        {u8'b', u8'E', u8"%Eb"},
        {u8'b', u8'O', u8"%Ob"},
        {u8'h', 0, u8"Sep"},
        {u8'h', u8'E', u8"%Eh"},
        {u8'h', u8'O', u8"%Oh"},
        {u8'B', 0, u8"September"},
        {u8'B', u8'E', u8"%EB"},
        {u8'B', u8'O', u8"%OB"},
        {u8'c', 0, u8"Wed Sep  4 13:33:18 2024"},
        {u8'c', u8'E', u8"Wed Sep  4 13:33:18 2024"},
        {u8'c', u8'O', u8"%Oc"},
        {u8'C', 0, u8"20"},
        {u8'C', u8'E', u8"20"},
        {u8'C', u8'O', u8"%OC"},
        {u8'x', 0, u8"09/04/24"},
        {u8'x', u8'E', u8"09/04/24"},
        {u8'x', u8'O', u8"%Ox"},
        {u8'D', 0, u8"09/04/24"},
        {u8'D', u8'E', u8"%ED"},
        {u8'D', u8'O', u8"%OD"},
        {u8'd', 0, u8"04"},
        {u8'd', u8'E', u8"%Ed"},
        {u8'd', u8'O', u8"04"},
        {u8'e', 0, u8" 4"},
        {u8'e', u8'E', u8"%Ee"},
        {u8'e', u8'O', u8" 4"},
        {u8'F', 0, u8"2024-09-04"},
        {u8'F', u8'E', u8"%EF"},
        {u8'F', u8'O', u8"%OF"},
        {u8'H', 0, u8"13"},
        {u8'H', u8'E', u8"%EH"},
        {u8'H', u8'O', u8"13"},
        {u8'I', 0, u8"01"},
        {u8'I', u8'E', u8"%EI"},
        {u8'I', u8'O', u8"01"},
        {u8'j', 0, u8"248"},
        {u8'j', u8'E', u8"%Ej"},
        {u8'j', u8'O', u8"%Oj"},
        {u8'M', 0, u8"33"},
        {u8'M', u8'E', u8"%EM"},
        {u8'M', u8'O', u8"33"},
        {u8'm', 0, u8"09"},
        {u8'm', u8'E', u8"%Em"},
        {u8'm', u8'O', u8"09"},
        {u8'n', 0, u8"\n"},
        {u8'n', u8'E', u8"%En"},
        {u8'n', u8'O', u8"%On"},
        {u8'p', 0, u8"PM"},
        {u8'p', u8'E', u8"%Ep"},
        {u8'p', u8'O', u8"%Op"},
        {u8'R', 0, u8"13:33"},
        {u8'R', u8'E', u8"%ER"},
        {u8'R', u8'O', u8"%OR"},
        {u8'r', 0, u8"01:33:18 PM"},
        {u8'r', u8'E', u8"%Er"},
        {u8'r', u8'O', u8"%Or"},
        {u8'S', 0, u8"18"},
        {u8'S', u8'E', u8"%ES"},
        {u8'S', u8'O', u8"18"},
        {u8'X', 0, u8"13:33:18"},
        {u8'X', u8'E', u8"13:33:18"},
        {u8'X', u8'O', u8"%OX"},
        {u8'T', 0, u8"13:33:18"},
        {u8'T', u8'E', u8"%ET"},
        {u8'T', u8'O', u8"%OT"},
        {u8't', 0, u8"\t"},
        {u8't', u8'E', u8"%Et"},
        {u8't', u8'O', u8"%Ot"},
        {u8'u', 0, u8"3"},
        {u8'u', u8'E', u8"%Eu"},
        {u8'u', u8'O', u8"3"},
        {u8'U', 0, u8"35"},
        {u8'U', u8'E', u8"%EU"},
        {u8'U', u8'O', u8"35"},
        {u8'V', 0, u8"36"},
        {u8'V', u8'E', u8"%EV"},
        {u8'V', u8'O', u8"36"},
        {u8'g', 0, u8"24"},
        {u8'g', u8'E', u8"%Eg"},
        {u8'g', u8'O', u8"%Og"},
        {u8'G', 0, u8"2024"},
        {u8'G', u8'E', u8"%EG"},
        {u8'G', u8'O', u8"%OG"},
        {u8'W', 0, u8"36"},
        {u8'W', u8'E', u8"%EW"},
        {u8'W', u8'O', u8"36"},
        {u8'w', 0, u8"3"},
        {u8'w', u8'E', u8"%Ew"},
        {u8'w', u8'O', u8"3"},
        {u8'Y', 0, u8"2024"},
        {u8'Y', u8'E', u8"2024"},
        {u8'Y', u8'O', u8"%OY"},
        {u8'y', 0, u8"24"},
        {u8'y', u8'E', u8"24"},
        {u8'y', u8'O', u8"24"},
        {u8'Z', 0, u8"America/Los_Angeles"},
        {u8'Z', u8'E', u8"%EZ"},
        {u8'Z', u8'O', u8"%OZ"},
        {u8'z', 0, u8"-0700"},
        {u8'z', u8'E', u8"%Ez"},
        {u8'z', u8'O', u8"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// The same instant and the same specifiers under zh_CN, where the words and the
// composite layouts differ but the rules about what a value can supply do not.
TEST(TimeioChar8, ChineseWritesEveryConversionSpecifier)
{
    const timeio<char8_t> obj = facet_for("zh_CN.UTF-8");
    const auto            tp  = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    static const conversion kConversions[] = {
        {u8'%', 0, u8"%"},
        {u8'a', 0, u8"三"},
        {u8'a', u8'E', u8"%Ea"},
        {u8'a', u8'O', u8"%Oa"},
        {u8'A', 0, u8"星期三"},
        {u8'A', u8'E', u8"%EA"},
        {u8'A', u8'O', u8"%OA"},
        {u8'b', 0, u8"9月"},
        {u8'b', u8'E', u8"%Eb"},
        {u8'b', u8'O', u8"%Ob"},
        {u8'h', 0, u8"9月"},
        {u8'h', u8'E', u8"%Eh"},
        {u8'h', u8'O', u8"%Oh"},
        {u8'B', 0, u8"九月"},
        {u8'B', u8'E', u8"%EB"},
        {u8'B', u8'O', u8"%OB"},
        {u8'c', 0, u8"2024年09月04日 星期三 13时33分18秒"},
        {u8'c', u8'E', u8"2024年09月04日 星期三 13时33分18秒"},
        {u8'c', u8'O', u8"%Oc"},
        {u8'C', 0, u8"20"},
        {u8'C', u8'E', u8"20"},
        {u8'C', u8'O', u8"%OC"},
        {u8'x', 0, u8"2024年09月04日"},
        {u8'x', u8'E', u8"2024年09月04日"},
        {u8'x', u8'O', u8"%Ox"},
        {u8'D', 0, u8"09/04/24"},
        {u8'D', u8'E', u8"%ED"},
        {u8'D', u8'O', u8"%OD"},
        {u8'd', 0, u8"04"},
        {u8'd', u8'E', u8"%Ed"},
        {u8'd', u8'O', u8"04"},
        {u8'e', 0, u8" 4"},
        {u8'e', u8'E', u8"%Ee"},
        {u8'e', u8'O', u8" 4"},
        {u8'F', 0, u8"2024-09-04"},
        {u8'F', u8'E', u8"%EF"},
        {u8'F', u8'O', u8"%OF"},
        {u8'H', 0, u8"13"},
        {u8'H', u8'E', u8"%EH"},
        {u8'H', u8'O', u8"13"},
        {u8'I', 0, u8"01"},
        {u8'I', u8'E', u8"%EI"},
        {u8'I', u8'O', u8"01"},
        {u8'j', 0, u8"248"},
        {u8'j', u8'E', u8"%Ej"},
        {u8'j', u8'O', u8"%Oj"},
        {u8'M', 0, u8"33"},
        {u8'M', u8'E', u8"%EM"},
        {u8'M', u8'O', u8"33"},
        {u8'm', 0, u8"09"},
        {u8'm', u8'E', u8"%Em"},
        {u8'm', u8'O', u8"09"},
        {u8'n', 0, u8"\n"},
        {u8'n', u8'E', u8"%En"},
        {u8'n', u8'O', u8"%On"},
        {u8'p', 0, u8"下午"},
        {u8'p', u8'E', u8"%Ep"},
        {u8'p', u8'O', u8"%Op"},
        {u8'R', 0, u8"13:33"},
        {u8'R', u8'E', u8"%ER"},
        {u8'R', u8'O', u8"%OR"},
        {u8'r', 0, u8"下午 01时33分18秒"},
        {u8'r', u8'E', u8"%Er"},
        {u8'r', u8'O', u8"%Or"},
        {u8'S', 0, u8"18"},
        {u8'S', u8'E', u8"%ES"},
        {u8'S', u8'O', u8"18"},
        {u8'X', 0, u8"13时33分18秒"},
        {u8'X', u8'E', u8"13时33分18秒"},
        {u8'X', u8'O', u8"%OX"},
        {u8'T', 0, u8"13:33:18"},
        {u8'T', u8'E', u8"%ET"},
        {u8'T', u8'O', u8"%OT"},
        {u8't', 0, u8"\t"},
        {u8't', u8'E', u8"%Et"},
        {u8't', u8'O', u8"%Ot"},
        {u8'u', 0, u8"3"},
        {u8'u', u8'E', u8"%Eu"},
        {u8'u', u8'O', u8"3"},
        {u8'U', 0, u8"35"},
        {u8'U', u8'E', u8"%EU"},
        {u8'U', u8'O', u8"35"},
        {u8'V', 0, u8"36"},
        {u8'V', u8'E', u8"%EV"},
        {u8'V', u8'O', u8"36"},
        {u8'g', 0, u8"24"},
        {u8'g', u8'E', u8"%Eg"},
        {u8'g', u8'O', u8"%Og"},
        {u8'G', 0, u8"2024"},
        {u8'G', u8'E', u8"%EG"},
        {u8'G', u8'O', u8"%OG"},
        {u8'W', 0, u8"36"},
        {u8'W', u8'E', u8"%EW"},
        {u8'W', u8'O', u8"36"},
        {u8'w', 0, u8"3"},
        {u8'w', u8'E', u8"%Ew"},
        {u8'w', u8'O', u8"3"},
        {u8'Y', 0, u8"2024"},
        {u8'Y', u8'E', u8"2024"},
        {u8'Y', u8'O', u8"%OY"},
        {u8'y', 0, u8"24"},
        {u8'y', u8'E', u8"24"},
        {u8'y', u8'O', u8"24"},
        {u8'Z', 0, u8"America/Los_Angeles"},
        {u8'Z', u8'E', u8"%EZ"},
        {u8'Z', u8'O', u8"%OZ"},
        {u8'z', 0, u8"-0700"},
        {u8'z', u8'E', u8"%Ez"},
        {u8'z', u8'O', u8"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// And under ja_JP, which is the locale with an era representation, so %EC, %Ey
// and %EY are the rows to look at here.
TEST(TimeioChar8, JapaneseWritesEveryConversionSpecifier)
{
    const timeio<char8_t> obj = facet_for("ja_JP.UTF-8");
    const auto            tp  = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    static const conversion kConversions[] = {
        {u8'%', 0, u8"%"},
        {u8'a', 0, u8"水"},
        {u8'a', u8'E', u8"%Ea"},
        {u8'a', u8'O', u8"%Oa"},
        {u8'A', 0, u8"水曜日"},
        {u8'A', u8'E', u8"%EA"},
        {u8'A', u8'O', u8"%OA"},
        {u8'b', 0, u8" 9月"},
        {u8'b', u8'E', u8"%Eb"},
        {u8'b', u8'O', u8"%Ob"},
        {u8'h', 0, u8" 9月"},
        {u8'h', u8'E', u8"%Eh"},
        {u8'h', u8'O', u8"%Oh"},
        {u8'B', 0, u8"9月"},
        {u8'B', u8'E', u8"%EB"},
        {u8'B', u8'O', u8"%OB"},
        {u8'c', 0, u8"2024年09月04日 13時33分18秒"},
        {u8'c', u8'E', u8"令和6年09月04日 13時33分18秒"},
        {u8'c', u8'O', u8"%Oc"},
        {u8'C', 0, u8"20"},
        {u8'C', u8'E', u8"令和"},
        {u8'C', u8'O', u8"%OC"},
        {u8'x', 0, u8"2024年09月04日"},
        {u8'x', u8'E', u8"令和6年09月04日"},
        {u8'x', u8'O', u8"%Ox"},
        {u8'D', 0, u8"09/04/24"},
        {u8'D', u8'E', u8"%ED"},
        {u8'D', u8'O', u8"%OD"},
        {u8'd', 0, u8"04"},
        {u8'd', u8'E', u8"%Ed"},
        {u8'd', u8'O', u8"四"},
        {u8'e', 0, u8" 4"},
        {u8'e', u8'E', u8"%Ee"},
        {u8'e', u8'O', u8"四"},
        {u8'F', 0, u8"2024-09-04"},
        {u8'F', u8'E', u8"%EF"},
        {u8'F', u8'O', u8"%OF"},
        {u8'H', 0, u8"13"},
        {u8'H', u8'E', u8"%EH"},
        {u8'H', u8'O', u8"十三"},
        {u8'I', 0, u8"01"},
        {u8'I', u8'E', u8"%EI"},
        {u8'I', u8'O', u8"一"},
        {u8'j', 0, u8"248"},
        {u8'j', u8'E', u8"%Ej"},
        {u8'j', u8'O', u8"%Oj"},
        {u8'M', 0, u8"33"},
        {u8'M', u8'E', u8"%EM"},
        {u8'M', u8'O', u8"三十三"},
        {u8'm', 0, u8"09"},
        {u8'm', u8'E', u8"%Em"},
        {u8'm', u8'O', u8"九"},
        {u8'n', 0, u8"\n"},
        {u8'n', u8'E', u8"%En"},
        {u8'n', u8'O', u8"%On"},
        {u8'p', 0, u8"午後"},
        {u8'p', u8'E', u8"%Ep"},
        {u8'p', u8'O', u8"%Op"},
        {u8'R', 0, u8"13:33"},
        {u8'R', u8'E', u8"%ER"},
        {u8'R', u8'O', u8"%OR"},
        {u8'r', 0, u8"午後01時33分18秒"},
        {u8'r', u8'E', u8"%Er"},
        {u8'r', u8'O', u8"%Or"},
        {u8'S', 0, u8"18"},
        {u8'S', u8'E', u8"%ES"},
        {u8'S', u8'O', u8"十八"},
        {u8'X', 0, u8"13時33分18秒"},
        {u8'X', u8'E', u8"13時33分18秒"},
        {u8'X', u8'O', u8"%OX"},
        {u8'T', 0, u8"13:33:18"},
        {u8'T', u8'E', u8"%ET"},
        {u8'T', u8'O', u8"%OT"},
        {u8't', 0, u8"\t"},
        {u8't', u8'E', u8"%Et"},
        {u8't', u8'O', u8"%Ot"},
        {u8'u', 0, u8"3"},
        {u8'u', u8'E', u8"%Eu"},
        {u8'u', u8'O', u8"三"},
        {u8'U', 0, u8"35"},
        {u8'U', u8'E', u8"%EU"},
        {u8'U', u8'O', u8"三十五"},
        {u8'V', 0, u8"36"},
        {u8'V', u8'E', u8"%EV"},
        {u8'V', u8'O', u8"三十六"},
        {u8'g', 0, u8"24"},
        {u8'g', u8'E', u8"%Eg"},
        {u8'g', u8'O', u8"%Og"},
        {u8'G', 0, u8"2024"},
        {u8'G', u8'E', u8"%EG"},
        {u8'G', u8'O', u8"%OG"},
        {u8'W', 0, u8"36"},
        {u8'W', u8'E', u8"%EW"},
        {u8'W', u8'O', u8"三十六"},
        {u8'w', 0, u8"3"},
        {u8'w', u8'E', u8"%Ew"},
        {u8'w', u8'O', u8"三"},
        {u8'Y', 0, u8"2024"},
        {u8'Y', u8'E', u8"令和6年"},
        {u8'Y', u8'O', u8"%OY"},
        {u8'y', 0, u8"24"},
        {u8'y', u8'E', u8"6"},
        {u8'y', u8'O', u8"二十四"},
        {u8'Z', 0, u8"America/Los_Angeles"},
        {u8'Z', u8'E', u8"%EZ"},
        {u8'Z', u8'O', u8"%OZ"},
        {u8'z', 0, u8"-0700"},
        {u8'z', u8'E', u8"%Ez"},
        {u8'z', u8'O', u8"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// A year_month_day is a date and nothing else, so every specifier that asks for a
// time of day or a zone comes back as the text that asked for it.
TEST(TimeioChar8, ADateWritesEveryConversionSpecifierItCanSupply)
{
    using namespace std::chrono;
    const timeio<char8_t>   obj = facet_for("ja_JP.UTF-8");
    const year_month_day tp{year{2024}, month{9}, day{4}};

    static const conversion kConversions[] = {
        {u8'%', 0, u8"%"},
        {u8'a', 0, u8"水"},
        {u8'a', u8'E', u8"%Ea"},
        {u8'a', u8'O', u8"%Oa"},
        {u8'A', 0, u8"水曜日"},
        {u8'A', u8'E', u8"%EA"},
        {u8'A', u8'O', u8"%OA"},
        {u8'b', 0, u8" 9月"},
        {u8'b', u8'E', u8"%Eb"},
        {u8'b', u8'O', u8"%Ob"},
        {u8'h', 0, u8" 9月"},
        {u8'h', u8'E', u8"%Eh"},
        {u8'h', u8'O', u8"%Oh"},
        {u8'B', 0, u8"9月"},
        {u8'B', u8'E', u8"%EB"},
        {u8'B', u8'O', u8"%OB"},
        {u8'c', 0, u8"%c"},
        {u8'c', u8'E', u8"%Ec"},
        {u8'c', u8'O', u8"%Oc"},
        {u8'C', 0, u8"20"},
        {u8'C', u8'E', u8"令和"},
        {u8'C', u8'O', u8"%OC"},
        {u8'x', 0, u8"2024年09月04日"},
        {u8'x', u8'E', u8"令和6年09月04日"},
        {u8'x', u8'O', u8"%Ox"},
        {u8'D', 0, u8"09/04/24"},
        {u8'D', u8'E', u8"%ED"},
        {u8'D', u8'O', u8"%OD"},
        {u8'd', 0, u8"04"},
        {u8'd', u8'E', u8"%Ed"},
        {u8'd', u8'O', u8"四"},
        {u8'e', 0, u8" 4"},
        {u8'e', u8'E', u8"%Ee"},
        {u8'e', u8'O', u8"四"},
        {u8'F', 0, u8"2024-09-04"},
        {u8'F', u8'E', u8"%EF"},
        {u8'F', u8'O', u8"%OF"},
        {u8'H', 0, u8"%H"},
        {u8'H', u8'E', u8"%EH"},
        {u8'H', u8'O', u8"%OH"},
        {u8'I', 0, u8"%I"},
        {u8'I', u8'E', u8"%EI"},
        {u8'I', u8'O', u8"%OI"},
        {u8'j', 0, u8"248"},
        {u8'j', u8'E', u8"%Ej"},
        {u8'j', u8'O', u8"%Oj"},
        {u8'M', 0, u8"%M"},
        {u8'M', u8'E', u8"%EM"},
        {u8'M', u8'O', u8"%OM"},
        {u8'm', 0, u8"09"},
        {u8'm', u8'E', u8"%Em"},
        {u8'm', u8'O', u8"九"},
        {u8'n', 0, u8"\n"},
        {u8'n', u8'E', u8"%En"},
        {u8'n', u8'O', u8"%On"},
        {u8'p', 0, u8"%p"},
        {u8'p', u8'E', u8"%Ep"},
        {u8'p', u8'O', u8"%Op"},
        {u8'R', 0, u8"%R"},
        {u8'R', u8'E', u8"%ER"},
        {u8'R', u8'O', u8"%OR"},
        {u8'r', 0, u8"%r"},
        {u8'r', u8'E', u8"%Er"},
        {u8'r', u8'O', u8"%Or"},
        {u8'S', 0, u8"%S"},
        {u8'S', u8'E', u8"%ES"},
        {u8'S', u8'O', u8"%OS"},
        {u8'X', 0, u8"%X"},
        {u8'X', u8'E', u8"%EX"},
        {u8'X', u8'O', u8"%OX"},
        {u8'T', 0, u8"%T"},
        {u8'T', u8'E', u8"%ET"},
        {u8'T', u8'O', u8"%OT"},
        {u8't', 0, u8"\t"},
        {u8't', u8'E', u8"%Et"},
        {u8't', u8'O', u8"%Ot"},
        {u8'u', 0, u8"3"},
        {u8'u', u8'E', u8"%Eu"},
        {u8'u', u8'O', u8"三"},
        {u8'U', 0, u8"35"},
        {u8'U', u8'E', u8"%EU"},
        {u8'U', u8'O', u8"三十五"},
        {u8'V', 0, u8"36"},
        {u8'V', u8'E', u8"%EV"},
        {u8'V', u8'O', u8"三十六"},
        {u8'g', 0, u8"24"},
        {u8'g', u8'E', u8"%Eg"},
        {u8'g', u8'O', u8"%Og"},
        {u8'G', 0, u8"2024"},
        {u8'G', u8'E', u8"%EG"},
        {u8'G', u8'O', u8"%OG"},
        {u8'W', 0, u8"36"},
        {u8'W', u8'E', u8"%EW"},
        {u8'W', u8'O', u8"三十六"},
        {u8'w', 0, u8"3"},
        {u8'w', u8'E', u8"%Ew"},
        {u8'w', u8'O', u8"三"},
        {u8'Y', 0, u8"2024"},
        {u8'Y', u8'E', u8"令和6年"},
        {u8'Y', u8'O', u8"%OY"},
        {u8'y', 0, u8"24"},
        {u8'y', u8'E', u8"6"},
        {u8'y', u8'O', u8"二十四"},
        {u8'Z', 0, u8"%Z"},
        {u8'Z', u8'E', u8"%EZ"},
        {u8'Z', u8'O', u8"%OZ"},
        {u8'z', 0, u8"%z"},
        {u8'z', u8'E', u8"%Ez"},
        {u8'z', u8'O', u8"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// An hh_mm_ss is the mirror image: it has a time of day and no date at all.
TEST(TimeioChar8, ATimeOfDayWritesEveryConversionSpecifierItCanSupply)
{
    using namespace std::chrono;
    const timeio<char8_t>    obj = facet_for("ja_JP.UTF-8");
    const hh_mm_ss<seconds> tp{hours{13} + minutes{33} + seconds{18}};

    static const conversion kConversions[] = {
        {u8'%', 0, u8"%"},
        {u8'a', 0, u8"%a"},
        {u8'a', u8'E', u8"%Ea"},
        {u8'a', u8'O', u8"%Oa"},
        {u8'A', 0, u8"%A"},
        {u8'A', u8'E', u8"%EA"},
        {u8'A', u8'O', u8"%OA"},
        {u8'b', 0, u8"%b"},
        {u8'b', u8'E', u8"%Eb"},
        {u8'b', u8'O', u8"%Ob"},
        {u8'h', 0, u8"%h"},
        {u8'h', u8'E', u8"%Eh"},
        {u8'h', u8'O', u8"%Oh"},
        {u8'B', 0, u8"%B"},
        {u8'B', u8'E', u8"%EB"},
        {u8'B', u8'O', u8"%OB"},
        {u8'c', 0, u8"%c"},
        {u8'c', u8'E', u8"%Ec"},
        {u8'c', u8'O', u8"%Oc"},
        {u8'x', 0, u8"%x"},
        {u8'x', u8'E', u8"%Ex"},
        {u8'x', u8'O', u8"%Ox"},
        {u8'D', 0, u8"%D"},
        {u8'D', u8'E', u8"%ED"},
        {u8'D', u8'O', u8"%OD"},
        {u8'd', 0, u8"%d"},
        {u8'd', u8'E', u8"%Ed"},
        {u8'd', u8'O', u8"%Od"},
        {u8'e', 0, u8"%e"},
        {u8'e', u8'E', u8"%Ee"},
        {u8'e', u8'O', u8"%Oe"},
        {u8'F', 0, u8"%F"},
        {u8'F', u8'E', u8"%EF"},
        {u8'F', u8'O', u8"%OF"},
        {u8'H', 0, u8"13"},
        {u8'H', u8'E', u8"%EH"},
        {u8'H', u8'O', u8"十三"},
        {u8'I', 0, u8"01"},
        {u8'I', u8'E', u8"%EI"},
        {u8'I', u8'O', u8"一"},
        {u8'j', 0, u8"%j"},
        {u8'j', u8'E', u8"%Ej"},
        {u8'j', u8'O', u8"%Oj"},
        {u8'M', 0, u8"33"},
        {u8'M', u8'E', u8"%EM"},
        {u8'M', u8'O', u8"三十三"},
        {u8'm', 0, u8"%m"},
        {u8'm', u8'E', u8"%Em"},
        {u8'm', u8'O', u8"%Om"},
        {u8'n', 0, u8"\n"},
        {u8'n', u8'E', u8"%En"},
        {u8'n', u8'O', u8"%On"},
        {u8'p', 0, u8"午後"},
        {u8'p', u8'E', u8"%Ep"},
        {u8'p', u8'O', u8"%Op"},
        {u8'R', 0, u8"13:33"},
        {u8'R', u8'E', u8"%ER"},
        {u8'R', u8'O', u8"%OR"},
        {u8'r', 0, u8"午後01時33分18秒"},
        {u8'r', u8'E', u8"%Er"},
        {u8'r', u8'O', u8"%Or"},
        {u8'S', 0, u8"18"},
        {u8'S', u8'E', u8"%ES"},
        {u8'S', u8'O', u8"十八"},
        {u8'X', 0, u8"13時33分18秒"},
        {u8'X', u8'E', u8"13時33分18秒"},
        {u8'X', u8'O', u8"%OX"},
        {u8'T', 0, u8"13:33:18"},
        {u8'T', u8'E', u8"%ET"},
        {u8'T', u8'O', u8"%OT"},
        {u8't', 0, u8"\t"},
        {u8't', u8'E', u8"%Et"},
        {u8't', u8'O', u8"%Ot"},
        {u8'u', 0, u8"%u"},
        {u8'u', u8'E', u8"%Eu"},
        {u8'u', u8'O', u8"%Ou"},
        {u8'U', 0, u8"%U"},
        {u8'U', u8'E', u8"%EU"},
        {u8'U', u8'O', u8"%OU"},
        {u8'V', 0, u8"%V"},
        {u8'V', u8'E', u8"%EV"},
        {u8'V', u8'O', u8"%OV"},
        {u8'g', 0, u8"%g"},
        {u8'g', u8'E', u8"%Eg"},
        {u8'g', u8'O', u8"%Og"},
        {u8'G', 0, u8"%G"},
        {u8'G', u8'E', u8"%EG"},
        {u8'G', u8'O', u8"%OG"},
        {u8'W', 0, u8"%W"},
        {u8'W', u8'E', u8"%EW"},
        {u8'W', u8'O', u8"%OW"},
        {u8'w', 0, u8"%w"},
        {u8'w', u8'E', u8"%Ew"},
        {u8'w', u8'O', u8"%Ow"},
        {u8'Y', 0, u8"%Y"},
        {u8'Y', u8'E', u8"%EY"},
        {u8'Y', u8'O', u8"%OY"},
        {u8'y', 0, u8"%y"},
        {u8'y', u8'E', u8"%Ey"},
        {u8'y', u8'O', u8"%Oy"},
        {u8'Z', 0, u8"%Z"},
        {u8'Z', u8'E', u8"%EZ"},
        {u8'Z', u8'O', u8"%OZ"},
        {u8'z', 0, u8"%z"},
        {u8'z', u8'E', u8"%Ez"},
        {u8'z', u8'O', u8"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// A std::tm carries both halves, so almost everything is available; what it does
// not carry is a zone, which the cases after this one take up.
TEST(TimeioChar8, ABrokenDownTimeWritesEveryConversionSpecifier)
{
    const timeio<char8_t> obj = facet_for("ja_JP.UTF-8");
    const std::tm         tp  = calendar_time(2024 - 1900, 9 - 1, 4, 13, 33, 18, 0, 0, 0);

    static const conversion kConversions[] = {
        {u8'%', 0, u8"%"},
        {u8'a', 0, u8"水"},
        {u8'a', u8'E', u8"%Ea"},
        {u8'a', u8'O', u8"%Oa"},
        {u8'A', 0, u8"水曜日"},
        {u8'A', u8'E', u8"%EA"},
        {u8'A', u8'O', u8"%OA"},
        {u8'b', 0, u8" 9月"},
        {u8'b', u8'E', u8"%Eb"},
        {u8'b', u8'O', u8"%Ob"},
        {u8'h', 0, u8" 9月"},
        {u8'h', u8'E', u8"%Eh"},
        {u8'h', u8'O', u8"%Oh"},
        {u8'B', 0, u8"9月"},
        {u8'B', u8'E', u8"%EB"},
        {u8'B', u8'O', u8"%OB"},
        {u8'c', 0, u8"2024年09月04日 13時33分18秒"},
        {u8'c', u8'E', u8"令和6年09月04日 13時33分18秒"},
        {u8'c', u8'O', u8"%Oc"},
        {u8'C', 0, u8"20"},
        {u8'C', u8'E', u8"令和"},
        {u8'C', u8'O', u8"%OC"},
        {u8'x', 0, u8"2024年09月04日"},
        {u8'x', u8'E', u8"令和6年09月04日"},
        {u8'x', u8'O', u8"%Ox"},
        {u8'D', 0, u8"09/04/24"},
        {u8'D', u8'E', u8"%ED"},
        {u8'D', u8'O', u8"%OD"},
        {u8'd', 0, u8"04"},
        {u8'd', u8'E', u8"%Ed"},
        {u8'd', u8'O', u8"四"},
        {u8'e', 0, u8" 4"},
        {u8'e', u8'E', u8"%Ee"},
        {u8'e', u8'O', u8"四"},
        {u8'F', 0, u8"2024-09-04"},
        {u8'F', u8'E', u8"%EF"},
        {u8'F', u8'O', u8"%OF"},
        {u8'H', 0, u8"13"},
        {u8'H', u8'E', u8"%EH"},
        {u8'H', u8'O', u8"十三"},
        {u8'I', 0, u8"01"},
        {u8'I', u8'E', u8"%EI"},
        {u8'I', u8'O', u8"一"},
        {u8'j', 0, u8"248"},
        {u8'j', u8'E', u8"%Ej"},
        {u8'j', u8'O', u8"%Oj"},
        {u8'M', 0, u8"33"},
        {u8'M', u8'E', u8"%EM"},
        {u8'M', u8'O', u8"三十三"},
        {u8'm', 0, u8"09"},
        {u8'm', u8'E', u8"%Em"},
        {u8'm', u8'O', u8"九"},
        {u8'n', 0, u8"\n"},
        {u8'n', u8'E', u8"%En"},
        {u8'n', u8'O', u8"%On"},
        {u8'p', 0, u8"午後"},
        {u8'p', u8'E', u8"%Ep"},
        {u8'p', u8'O', u8"%Op"},
        {u8'R', 0, u8"13:33"},
        {u8'R', u8'E', u8"%ER"},
        {u8'R', u8'O', u8"%OR"},
        {u8'r', 0, u8"午後01時33分18秒"},
        {u8'r', u8'E', u8"%Er"},
        {u8'r', u8'O', u8"%Or"},
        {u8'S', 0, u8"18"},
        {u8'S', u8'E', u8"%ES"},
        {u8'S', u8'O', u8"十八"},
        {u8'X', 0, u8"13時33分18秒"},
        {u8'X', u8'E', u8"13時33分18秒"},
        {u8'X', u8'O', u8"%OX"},
        {u8'T', 0, u8"13:33:18"},
        {u8'T', u8'E', u8"%ET"},
        {u8'T', u8'O', u8"%OT"},
        {u8't', 0, u8"\t"},
        {u8't', u8'E', u8"%Et"},
        {u8't', u8'O', u8"%Ot"},
        {u8'u', 0, u8"3"},
        {u8'u', u8'E', u8"%Eu"},
        {u8'u', u8'O', u8"三"},
        {u8'U', 0, u8"35"},
        {u8'U', u8'E', u8"%EU"},
        {u8'U', u8'O', u8"三十五"},
        {u8'V', 0, u8"36"},
        {u8'V', u8'E', u8"%EV"},
        {u8'V', u8'O', u8"三十六"},
        {u8'g', 0, u8"24"},
        {u8'g', u8'E', u8"%Eg"},
        {u8'g', u8'O', u8"%Og"},
        {u8'G', 0, u8"2024"},
        {u8'G', u8'E', u8"%EG"},
        {u8'G', u8'O', u8"%OG"},
        {u8'W', 0, u8"36"},
        {u8'W', u8'E', u8"%EW"},
        {u8'W', u8'O', u8"三十六"},
        {u8'w', 0, u8"3"},
        {u8'w', u8'E', u8"%Ew"},
        {u8'w', u8'O', u8"三"},
        {u8'Y', 0, u8"2024"},
        {u8'Y', u8'E', u8"令和6年"},
        {u8'Y', u8'O', u8"%OY"},
        {u8'y', 0, u8"24"},
        {u8'y', u8'E', u8"6"},
        {u8'y', u8'O', u8"二十四"},
        {u8'Z', 0, u8"UNKNOWN"},
        {u8'Z', u8'E', u8"%EZ"},
        {u8'Z', u8'O', u8"%OZ"},
        {u8'z', 0, u8"+0000"},
        {u8'z', u8'E', u8"%Ez"},
        {u8'z', u8'O', u8"%Oz"},
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
TEST(TimeioChar8, ABrokenDownTimeNamesItsZoneOrSaysItCannot)
{
    const timeio<char8_t> obj = facet_for("ja_JP.UTF-8");
    const std::tm         tp  = calendar_time(2024 - 1900, 9 - 1, 4, 13, 33, 18, 0, 0, 0);

    EXPECT_EQ(put_one(obj, tp, u8'Z'), u8"UNKNOWN");
    EXPECT_EQ(put_one(obj, tp, u8'z'), u8"+0000");

#ifdef __USE_MISC
    std::tm named = tp;
    named.tm_zone = "PST";
    EXPECT_EQ(put_one(obj, named, u8'Z'), u8"PST");

    // An empty string is as nameless as a null pointer.
    named.tm_zone = "";
    EXPECT_EQ(put_one(obj, named, u8'Z'), u8"UNKNOWN");
#endif
}

// A format string is expanded one specifier at a time with the literal text
// between them passed through unchanged, so what it produces is exactly the
// concatenation of its pieces.  Stated that way the case needs no locale's words
// written down, and holds in every locale rather than in the one it was written
// for.
TEST(TimeioChar8, AFormatStringIsExpandedSpecifierBySpecifier)
{
    const auto tp = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    for (const char* loc : {"C", "de_DE.UTF-8", "en_HK.UTF-8", "fr_FR.UTF-8", "ja_JP.UTF-8"})
    {
        SCOPED_TRACE(loc);
        const timeio<char8_t> obj = facet_for(loc);

        EXPECT_EQ(put_one(obj, tp, std::u8string_view(u8"%A, week %W of %B")),
                  put_one(obj, tp, u8'A') + u8", week " + put_one(obj, tp, u8'W')
                                        + u8" of " + put_one(obj, tp, u8'B'));

        // Literal text alone, and a format that is nothing but literal text.
        EXPECT_EQ(put_one(obj, tp, std::u8string_view(u8"[%Y]")), u8"[" + put_one(obj, tp, u8'Y') + u8"]");
        EXPECT_EQ(put_one(obj, tp, std::u8string_view(u8"no specifiers")), u8"no specifiers");
    }
}

// put() writes through an iterator into whatever the caller supplied and returns
// where it stopped, so everything past that point has to be exactly as it was.
TEST(TimeioChar8, PutIntoAnExistingBufferReturnsWhereItStopped)
{
    const timeio<char8_t> obj = facet_for("C");
    const auto            tp  = create_zoned_time(1997, 6, 26, 12, 0, 0, "America/Los_Angeles");

    std::u8string buffer(50, u8'.');
    const auto    end = obj.put(buffer.begin(), tp, std::u8string_view(u8"%F %T"));
    EXPECT_EQ(std::u8string(buffer.begin(), end), u8"1997-06-26 12:00:00");
    EXPECT_EQ(buffer.substr(19), std::u8string(31, u8'.'));

    // The same for a single specifier, whose length the caller cannot know in
    // advance because it is a word the locale chose.
    std::u8string one(20, u8'.');
    const auto    one_end = obj.put(one.begin(), tp, u8'A');
    EXPECT_EQ(std::u8string(one.begin(), one_end), u8"Thursday");
    EXPECT_EQ(one.substr(8), std::u8string(12, u8'.'));
}

// The literal text in a format is part of what has to match: it is how the
// caller says which of several numbers is which.  Input past what the format
// asked for is left for whoever reads next.
TEST(TimeioChar8, AFormatStringMustMatchTheInputLiterally)
{
    const timeio<char8_t> obj = facet_for("C");

    const auto t = ctx_to<std::tm>(
        CheckGet(obj, u8"on 2024-09-04 at 01:09:35", u8"on %Y-%m-%d at %H:%M:%S", ios_defs::eofbit));
    EXPECT_EQ(t.tm_year, 124);
    EXPECT_EQ(t.tm_mon, 8);
    EXPECT_EQ(t.tm_mday, 4);
    EXPECT_EQ(t.tm_hour, 1);
    EXPECT_EQ(t.tm_min, 9);
    EXPECT_EQ(t.tm_sec, 35);

    // Literal text the input does not carry.
    CheckGet(obj, u8"at 2024-09-04", u8"on %Y-%m-%d", ios_defs::strfailbit);
    CheckGet(obj, u8"2024-09-04", u8"on %Y-%m-%d", ios_defs::strfailbit);

    // A '%' with nothing after it is not a specifier.
    CheckGet(obj, u8"2024-09-04", u8"%", ios_defs::strfailbit);

    // What the format did not ask for stays in the input.
    const auto rest = ctx_to<std::tm>(CheckGet(obj, u8"2020  ", u8"%Y", ios_defs::goodbit));
    EXPECT_EQ(rest.tm_year, 120);

    // A single specifier without a format string reads the same field.
    EXPECT_EQ(ctx_to<std::tm>(CheckGet(obj, u8"2020", u8'Y', 0, ios_defs::eofbit)).tm_year, 120);
}

// The words a locale writes are the words it reads.  Round-tripping through the
// facet's own output says that in every locale at once, without this file having
// to know how any of them spells a month.
TEST(TimeioChar8, TheNamesTheLocaleWritesAreTheNamesItReads)
{
    using namespace std::chrono;
    const year_month_day date{year{2014}, month{4}, day{14}};

    for (const char* loc : {"C", "de_DE.UTF-8", "es_ES.UTF-8", "fr_FR.UTF-8", "ja_JP.UTF-8"})
    {
        SCOPED_TRACE(loc);
        const timeio<char8_t> obj = facet_for(loc);

        for (const char8_t* fmt : {u8"%A, %d. %B %Y", u8"%a %d %b %Y", u8"%A %j %Y"})
        {
            SCOPED_TRACE(::testing::PrintToString(fmt));
            const std::u8string written = put_one(obj, date, std::u8string_view(fmt));
            EXPECT_EQ(CheckGet<year_month_day>(obj, written, fmt, ios_defs::eofbit), date);
        }
    }
}

// The specifiers that carry only part of a date -- a week number and a weekday,
// a day of the year, a century and a two-digit year -- have to reassemble into
// the date they were written from.  Stated as a round trip it holds for every
// date rather than for a handful with hand-computed week numbers.
TEST(TimeioChar8, EveryDateReassemblesFromItsPartialSpecifiers)
{
    using namespace std::chrono;
    const timeio<char8_t> obj = facet_for("C");

    const char8_t* const formats[] = {
        u8"%F", u8"%Y-%m-%d", u8"%d-%b-%Y", u8"%C%y-%m-%d",
        u8"%Y %U %w", u8"%Y %W %w", u8"%Y %W %a", u8"%Y %U %A", u8"%j %Y",
    };

    // 29 days apart, so the sweep lands on every weekday and crosses the turn of
    // each year, which is where the week-number rules disagree with each other.
    for (sys_days d = sys_days{2019y / January / 1}; d <= sys_days{2024y / December / 31};
         d += days{29})
    {
        const year_month_day date{d};
        for (const char8_t* fmt : formats)
        {
            SCOPED_TRACE(::testing::PrintToString(fmt) + " | "
                         + ::testing::PrintToString(put_one(obj, date, std::u8string_view(u8"%F"))));
            const std::u8string written = put_one(obj, date, std::u8string_view(fmt));
            EXPECT_EQ(CheckGet<year_month_day>(obj, written, fmt, ios_defs::eofbit), date);
        }
    }
}

// %I is a clock face: it cannot tell noon from midnight on its own, and %p is
// what supplies the half of the day it belongs to.  Either order.
TEST(TimeioChar8, TheTwelveHourClockNeedsItsMeridiem)
{
    using namespace std::chrono;
    const timeio<char8_t> obj = facet_for("C");

    for (int hour = 0; hour < 24; ++hour)
    {
        SCOPED_TRACE(hour);
        const seconds          when = hours{hour} + minutes{5} + seconds{9};
        const hh_mm_ss<seconds> tp{when};

        for (const char8_t* fmt : {u8"%I:%M:%S %p", u8"%p%I:%M:%S", u8"%r", u8"%T"})
        {
            SCOPED_TRACE(::testing::PrintToString(fmt));
            const std::u8string written = put_one(obj, tp, std::u8string_view(fmt));
            const auto          back =
                CheckGet<hh_mm_ss<seconds>, false, true, tz_level::none>(obj, written, fmt,
                                                                         ios_defs::eofbit);
            EXPECT_EQ(back.to_duration(), when);
        }
    }

    // Without the meridiem the same field reads as the morning hour, because that
    // is the half of the day a clock face means when nothing says otherwise.
    const auto morning = ctx_to<std::tm>(CheckGet(obj, u8"07:05:09", u8"%I:%M:%S", ios_defs::eofbit));
    EXPECT_EQ(morning.tm_hour, 7);
}

TEST(TimeioChar8, ACompositeFormatCanExpandPastAConventionalStackBuffer)
{
    std::shared_ptr<timeio_conf<char8_t>> conf =
        std::make_shared<expanded_composite_conf<char8_t>>();
    timeio obj(conf);
    auto   zt = create_zoned_time(2022, 11, 17, 21, 47, 26, "America/Los_Angeles");

    std::u8string actual;
    obj.put(std::back_inserter(actual), zt, u8'c');

    std::u8string expected(140, u8'q');
    expected += u8"-2022-11-17-21:47:26-";
    expected.append(20, u8'z');

    EXPECT_GT(actual.size(), 128u);
    EXPECT_EQ(actual, expected);
}

TEST(TimeioChar8, TheCLocaleReadsEveryConversionSpecifier)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};

    timeio obj(std::make_shared<timeio_conf<char8_t>>("C"));
    CheckGet(obj, u8"%",   u8'%',  0,  ios_defs::eofbit);
    CheckGet(obj, u8"x",   u8'%',  0,  ios_defs::strfailbit);
    CheckGet(obj, u8"%",   u8'%', u8'E', febit);
    CheckGet(obj, u8"%E%", u8'%', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"%",   u8'%', u8'O', febit);
    CheckGet(obj, u8"%O%", u8'%', u8'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet(obj, u8"Wed", u8'a', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, u8"%Ea", u8'a', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"a",   u8'a', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oa", u8'a', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"a",   u8'a', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"Wednesday", u8'A', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, u8"%EA", u8'A', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"A",   u8'A', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OA", u8'A', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"A",   u8'A', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"Sep", u8'b', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, u8"%Eb", u8'b', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"b",   u8'b', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Ob", u8'b', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"b",   u8'b', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"September", u8'B', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, u8"%EB", u8'B', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"B",   u8'B', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OB", u8'B', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"B",   u8'B', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"Sep", u8'h', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, u8"%Eh", u8'h', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"h",   u8'h', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oh", u8'h', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"h",   u8'h', u8'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"Wed Sep  4 13:33:18 2024", u8'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"Wed Sep  4 13:33:18 2024", u8'c', u8'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"c",   u8'c', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oc", u8'c', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"c",   u8'c', u8'O', ios_defs::strfailbit);


    EXPECT_EQ(CheckGet(obj, u8"20", u8'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(CheckGet(obj, u8"20", u8'C', u8'E', ios_defs::eofbit).m_century, 20);
    CheckGet(obj, u8"C",   u8'C', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OC", u8'C', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"C",   u8'C', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"04", u8'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, u8"04", u8'd', u8'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, u8"%Ed", u8'd', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"d",   u8'd', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"d",   u8'd', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"4", u8'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, u8"4", u8'e', u8'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, u8"%Ee", u8'e', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"e",   u8'e', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"e",   u8'e', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024-09-04", u8'F', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"%EF", u8'F', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"F",   u8'F', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OF", u8'F', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"F",   u8'F', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"09/04/24", u8'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"09/04/24", u8'x', u8'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"x",   u8'x', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Ox", u8'x', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"x",   u8'x', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"09/04/24", u8'D', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"%ED", u8'D', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"D",   u8'D', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OD", u8'D', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"D",   u8'D', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"13", u8'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, u8"13", u8'H', u8'O', ios_defs::eofbit).m_hour, 13);
    CheckGet(obj, u8"%EH", u8'H', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"H",   u8'H', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"H",   u8'H', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"01", u8'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, u8"01", u8'I', u8'O', ios_defs::eofbit).m_hour, 1);
    CheckGet(obj, u8"%EI", u8'I', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"I",   u8'I', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"I",   u8'I', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"248", u8'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 248", u8"%Y %j", ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"%Ej", u8'j', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"j",   u8'j', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oj", u8'j', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"j",   u8'j', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"09", u8'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, u8"09", u8'm', u8'O', ios_defs::eofbit).m_month, 9);
    CheckGet(obj, u8"%Em", u8'm', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"m",   u8'm', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"m",   u8'm', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"33", u8'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, u8"33", u8'M', u8'O', ios_defs::eofbit).m_minute, 33);
    CheckGet(obj, u8"%EM", u8'M', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"M",   u8'M', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"M",   u8'M', u8'O', ios_defs::strfailbit);

    CheckGet(obj, u8"\n",   u8'n',  0,  ios_defs::eofbit);
    CheckGet(obj, u8"x",    u8'n',  0,  ios_defs::goodbit);
    CheckGet(obj, u8"\n",   u8'n', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%En",  u8'n', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"n",    u8'n', u8'O', ios_defs::strfailbit);
    CheckGet(obj, u8"%On",  u8'n', u8'O', ios_defs::eofbit);

    CheckGet(obj, u8"\t",   u8't',  0,  ios_defs::eofbit);
    CheckGet(obj, u8"x",    u8't',  0,  ios_defs::goodbit);
    CheckGet(obj, u8"\t",   u8't', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Et",  u8't', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"n",    u8't', u8'O', ios_defs::strfailbit);
    CheckGet(obj, u8"%Ot",  u8't', u8'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 PM", u8"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 AM", u8"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(CheckGet(obj, u8"PM", u8'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(CheckGet(obj, u8"AM", u8'p', 0, ios_defs::eofbit).m_is_pm, false);
    CheckGet(obj, u8"%Ep", u8'p', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"p",   u8'p', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Op", u8'p', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"p",   u8'p', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01:33:18 PM", u8"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"%Er", u8'r', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"r",   u8'r', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Or", u8'r', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"r",   u8'r', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33", u8"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, u8"%ER", u8'R', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"R",   u8'R', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OR", u8'R', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"R",   u8'R', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"18", u8'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, u8"18", u8'S', u8'O', ios_defs::eofbit).m_second, 18);
    CheckGet(obj, u8"%ES", u8'S', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"S",   u8'S', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"S",   u8'S', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33:18", u8"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33:18", u8"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"X",   u8'X', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OX", u8'X', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"X",   u8'X', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33:18", u8"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"%ET", u8'T', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"T",   u8'T', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OT", u8'T', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"T",   u8'T', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"3", u8'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, u8"3", u8'u', u8'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, u8"%Eu", u8'u', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"u",   u8'u', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"u",   u8'u', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"24", u8'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, u8"%Eg", u8'g', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"g",   u8'g', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Og", u8'g', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"g",   u8'g', u8'O', ios_defs::strfailbit);


    EXPECT_EQ(CheckGet(obj, u8"2024", u8'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, u8"%EG", u8'G', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"G",   u8'G', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OG", u8'G', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"G",   u8'G', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 35 Wed", u8"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 35 Wed", u8"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, u8"35", u8'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(CheckGet(obj, u8"35", u8'U', u8'O', ios_defs::eofbit).m_week_no, 35);
    CheckGet(obj, u8"%EU", u8'U', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"U",   u8'U', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"U",   u8'U', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 36 Wed", u8"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 36 Wed", u8"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, u8"36", u8'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(CheckGet(obj, u8"36", u8'W', u8'O', ios_defs::eofbit).m_week_no, 36);
    CheckGet(obj, u8"%EW", u8'W', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"W",   u8'W', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"W",   u8'W', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"36", u8'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    CheckGet(obj, u8"54",  u8'V', u8'O', ios_defs::strfailbit);
    CheckGet(obj, u8"36",  u8'V', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"%EV", u8'V', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"V",   u8'V', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"V",   u8'V', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"3", u8'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, u8"3", u8'w', u8'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, u8"%Ew", u8'w', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"w",   u8'w', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"w",   u8'w', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"24", u8'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, u8"24", u8'y', u8'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, u8"24", u8'y', u8'O', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, u8"y",  u8'y', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"y",  u8'y', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"2024", u8'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, u8"2024", u8'Y', u8'E', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, u8"Y",   u8'Y', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OY", u8'Y', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"Y",   u8'Y', u8'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(CheckGet(obj, u8"America/Los_Angeles", u8'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, u8"PST", u8'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, u8"America/Los_Angexes", u8'Z', 0, ios_defs::strfailbit);
    CheckGet(obj, u8"%EZ", u8'Z', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"Z",   u8'Z', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OZ", u8'Z', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"Z",   u8'Z', u8'O', ios_defs::strfailbit);

    CheckGet(obj, u8"Z", u8'z', 0, ios_defs::eofbit);
    CheckGet(obj, u8"+13", u8'z', 0, ios_defs::eofbit);
    CheckGet(obj, u8"-1110", u8'z', 0, ios_defs::eofbit);
    CheckGet(obj, u8"+11:10", u8'z', 0, ios_defs::eofbit);
    CheckGet(obj, u8"%Ez", u8'z', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"z",  u8'z', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oz", u8'z', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"z",  u8'z', u8'O', ios_defs::strfailbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"1999-W52-6", u8"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2019-W01-1", u8"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"1999-W52-5", u8"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"99-W52-6", u8"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"19-W01-1", u8"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"99-W52-5", u8"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"20 24/09/04", u8"%C %y/%m/%d", ios_defs::eofbit), check_date1);

    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((CheckGet<year_month_day>(obj, u8"20 01 01", u8"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar8, ChineseReadsEveryConversionSpecifier)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<char8_t>>("zh_CN.UTF-8"));

    CheckGet(obj, u8"%",  u8'%',  0,  ios_defs::eofbit);
    CheckGet(obj, u8"x",  u8'%',  0,  ios_defs::strfailbit);
    CheckGet(obj, u8"%",  u8'%', u8'E', febit);
    CheckGet(obj, u8"%E%", u8'%', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"%",  u8'%', u8'O', febit);
    CheckGet(obj, u8"%O%", u8'%', u8'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet(obj, u8"三", u8'a', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, u8"%Ea", u8'a', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"a",   u8'a', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oa", u8'a', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"a",   u8'a', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"星期三", u8'A', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, u8"%EA", u8'A', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"A",   u8'A', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OA", u8'A', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"A",   u8'A', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"九月", u8'b', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, u8"%Eb", u8'b', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"b",   u8'b', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Ob", u8'b', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"b",   u8'b', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"九月", u8'B', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, u8"%EB", u8'B', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"B",   u8'B', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OB", u8'B', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"B",   u8'B', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"九月", u8'h', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, u8"%Eh", u8'h', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"h",   u8'h', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oh", u8'h', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"h",   u8'h', u8'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024年09月04日 星期三 13时33分18秒", u8'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024年09月04日 星期三 13时33分18秒", u8'c', u8'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"c",   u8'c', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oc", u8'c', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"c",   u8'c', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"20", u8'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(CheckGet(obj, u8"20", u8'C', u8'E', ios_defs::eofbit).m_century, 20);
    CheckGet(obj, u8"C",   u8'C', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OC", u8'C', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"C",   u8'C', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"04", u8'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, u8"04", u8'd', u8'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, u8"%Ed", u8'd', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"d",   u8'd', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"d",   u8'd', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"4", u8'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, u8"4", u8'e', u8'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, u8"%Ee", u8'e', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"e",   u8'e', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"e",   u8'e', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024-09-04", u8'F', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"%EF", u8'F', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"F",   u8'F', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OF", u8'F', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"F",   u8'F', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024年09月04日", u8'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024年09月04日", u8'x', u8'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"x",   u8'x', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Ox", u8'x', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"x",   u8'x', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"09/04/24", u8'D', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"%ED", u8'D', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"D",   u8'D', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OD", u8'D', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"D",   u8'D', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"13", u8'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, u8"13", u8'H', u8'O', ios_defs::eofbit).m_hour, 13);
    CheckGet(obj, u8"%EH", u8'H', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"H",   u8'H', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"H",   u8'H', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"01", u8'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, u8"01", u8'I', u8'O', ios_defs::eofbit).m_hour, 1);
    CheckGet(obj, u8"%EI", u8'I', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"I",   u8'I', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"I",   u8'I', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"248", u8'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 248", u8"%Y %j", ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"%Ej", u8'j', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"j",   u8'j', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oj", u8'j', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"j",   u8'j', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"09", u8'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, u8"09", u8'm', u8'O', ios_defs::eofbit).m_month, 9);
    CheckGet(obj, u8"%Em", u8'm', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"m",   u8'm', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"m",   u8'm', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"33", u8'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, u8"33", u8'M', u8'O', ios_defs::eofbit).m_minute, 33);
    CheckGet(obj, u8"%EM", u8'M', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"M",   u8'M', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"M",   u8'M', u8'O', ios_defs::strfailbit);

    CheckGet(obj, u8"\n",   u8'n',  0,  ios_defs::eofbit);
    CheckGet(obj, u8"x",    u8'n',  0,  ios_defs::goodbit);
    CheckGet(obj, u8"\n",   u8'n', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%En",  u8'n', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"n",    u8'n', u8'O', ios_defs::strfailbit);
    CheckGet(obj, u8"%On",  u8'n', u8'O', ios_defs::eofbit);

    CheckGet(obj, u8"\t",   u8't',  0,  ios_defs::eofbit);
    CheckGet(obj, u8"x",    u8't',  0,  ios_defs::goodbit);
    CheckGet(obj, u8"\t",   u8't', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Et",  u8't', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"n",    u8't', u8'O', ios_defs::strfailbit);
    CheckGet(obj, u8"%Ot",  u8't', u8'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 下午", u8"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 上午", u8"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(CheckGet(obj, u8"下午", u8'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(CheckGet(obj, u8"上午", u8'p', 0, ios_defs::eofbit).m_is_pm, false);
    CheckGet(obj, u8"%Ep", u8'p', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"p",   u8'p', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Op", u8'p', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"p",   u8'p', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"下午 01时33分18秒", u8"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"%Er", u8'r', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"r",   u8'r', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Or", u8'r', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"r",   u8'r', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33", u8"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, u8"%ER", u8'R', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"R",   u8'R', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OR", u8'R', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"R",   u8'R', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"18", u8'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, u8"18", u8'S', u8'O', ios_defs::eofbit).m_second, 18);
    CheckGet(obj, u8"%ES", u8'S', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"S",   u8'S', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"S",   u8'S', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13时33分18秒", u8"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13时33分18秒", u8"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"X",   u8'X', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OX", u8'X', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"X",   u8'X', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33:18", u8"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"%ET", u8'T', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"T",   u8'T', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OT", u8'T', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"T",   u8'T', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"3", u8'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, u8"3", u8'u', u8'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, u8"%Eu", u8'u', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"u",   u8'u', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"u",   u8'u', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"24", u8'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, u8"%Eg", u8'g', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"g",   u8'g', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Og", u8'g', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"g",   u8'g', u8'O', ios_defs::strfailbit);


    EXPECT_EQ(CheckGet(obj, u8"2024", u8'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, u8"%EG", u8'G', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"G",   u8'G', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OG", u8'G', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"G",   u8'G', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 35 三", u8"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 35 三", u8"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, u8"35", u8'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(CheckGet(obj, u8"35", u8'U', u8'O', ios_defs::eofbit).m_week_no, 35);
    CheckGet(obj, u8"%EU", u8'U', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"U",   u8'U', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"U",   u8'U', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 36 三", u8"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 36 三", u8"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, u8"36", u8'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(CheckGet(obj, u8"36", u8'W', u8'O', ios_defs::eofbit).m_week_no, 36);
    CheckGet(obj, u8"%EW", u8'W', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"W",   u8'W', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"W",   u8'W', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"36", u8'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    CheckGet(obj, u8"54",  u8'V', u8'O', ios_defs::strfailbit);
    CheckGet(obj, u8"36",  u8'V', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"%EV", u8'V', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"V",   u8'V', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"V",   u8'V', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"3", u8'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, u8"3", u8'w', u8'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, u8"%Ew", u8'w', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"w",   u8'w', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"w",   u8'w', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"24", u8'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, u8"24", u8'y', u8'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, u8"24", u8'y', u8'O', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, u8"y",  u8'y', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"y",  u8'y', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"2024", u8'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, u8"2024", u8'Y', u8'E', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, u8"Y",   u8'Y', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OY", u8'Y', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"Y",   u8'Y', u8'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(CheckGet(obj, u8"America/Los_Angeles", u8'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, u8"PST", u8'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, u8"America/Los_Angexes", u8'Z', 0, ios_defs::strfailbit);
    CheckGet(obj, u8"%EZ", u8'Z', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"Z",   u8'Z', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OZ", u8'Z', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"Z",   u8'Z', u8'O', ios_defs::strfailbit);

    CheckGet(obj, u8"Z", u8'z', 0, ios_defs::eofbit);
    CheckGet(obj, u8"+13", u8'z', 0, ios_defs::eofbit);
    CheckGet(obj, u8"-1110", u8'z', 0, ios_defs::eofbit);
    CheckGet(obj, u8"+11:10", u8'z', 0, ios_defs::eofbit);
    CheckGet(obj, u8"%Ez", u8'z', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"z",  u8'z', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oz", u8'z', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"z",  u8'z', u8'O', ios_defs::strfailbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"1999-W52-6", u8"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2019-W01-1", u8"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"1999-W52-5", u8"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"99-W52-6", u8"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"19-W01-1", u8"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"99-W52-5", u8"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"20 24/09/04", u8"%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((CheckGet<year_month_day>(obj, u8"20 01 01", u8"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar8, JapaneseReadsEveryConversionSpecifier)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<char8_t>>("ja_JP.UTF-8"));

    CheckGet(obj, u8"%",  u8'%',  0,  ios_defs::eofbit);
    CheckGet(obj, u8"x",  u8'%',  0,  ios_defs::strfailbit);
    CheckGet(obj, u8"%",  u8'%', u8'E', febit);
    CheckGet(obj, u8"%E%", u8'%', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"%",  u8'%', u8'O', febit);
    CheckGet(obj, u8"%O%", u8'%', u8'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet(obj, u8"水", u8'a', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, u8"%Ea", u8'a', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"a",   u8'a', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oa", u8'a', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"a",   u8'a', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"水曜日", u8'A', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, u8"%EA", u8'A', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"A",   u8'A', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OA", u8'A', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"A",   u8'A', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"9月", u8'b', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, u8"%Eb", u8'b', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"b",   u8'b', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Ob", u8'b', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"b",   u8'b', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"9月", u8'B', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, u8"%EB", u8'B', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"B",   u8'B', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OB", u8'B', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"B",   u8'B', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"9月", u8'h', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, u8"%Eh", u8'h', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"h",   u8'h', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oh", u8'h', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"h",   u8'h', u8'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024年09月04日 13時33分18秒", u8'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"令和6年09月04日 13時33分18秒", u8'c', u8'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"202409月04日 13時33分18秒", u8'c', u8'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"c",   u8'c', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oc", u8'c', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"c",   u8'c', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"20", u8'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"平成", u8'C', u8'E', ios_defs::eofbit).year(), std::chrono::year(1990));
    CheckGet(obj, u8"C",   u8'C', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OC", u8'C', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"C",   u8'C', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"04", u8'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, u8"04", u8'd', u8'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, u8"四", u8'd', u8'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, u8"%Ed", u8'd', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"d",   u8'd', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"d",   u8'd', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"4", u8'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, u8"4", u8'e', u8'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, u8"四", u8'e', u8'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, u8"%Ee", u8'e', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"e",   u8'e', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"e",   u8'e', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024-09-04", u8'F', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"%EF", u8'F', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"F",   u8'F', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OF", u8'F', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"F",   u8'F', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024年09月04日", u8'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"令和6年09月04日", u8'x', u8'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"202409月04日", u8'x', u8'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"x",   u8'x', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Ox", u8'x', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"x",   u8'x', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"09/04/24", u8'D', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"%ED", u8'D', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"D",   u8'D', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OD", u8'D', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"D",   u8'D', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"13", u8'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, u8"13", u8'H', u8'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, u8"十三", u8'H', u8'O', ios_defs::eofbit).m_hour, 13);
    CheckGet(obj, u8"%EH", u8'H', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"H",   u8'H', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"H",   u8'H', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"01", u8'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, u8"01", u8'I', u8'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, u8"一", u8'I', u8'O', ios_defs::eofbit).m_hour, 1);
    CheckGet(obj, u8"%EI", u8'I', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"I",   u8'I', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"I",   u8'I', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"248", u8'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 248", u8"%Y %j", ios_defs::eofbit), check_date1);
    CheckGet(obj, u8"%Ej", u8'j', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"j",   u8'j', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oj", u8'j', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"j",   u8'j', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"09", u8'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, u8"09", u8'm', u8'O', ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, u8"九", u8'm', u8'O', ios_defs::eofbit).m_month, 9);
    CheckGet(obj, u8"%Em", u8'm', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"m",   u8'm', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"m",   u8'm', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"33", u8'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, u8"33", u8'M', u8'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, u8"三十三", u8'M', u8'O', ios_defs::eofbit).m_minute, 33);
    CheckGet(obj, u8"%EM", u8'M', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"M",   u8'M', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"M",   u8'M', u8'O', ios_defs::strfailbit);

    CheckGet(obj, u8"\n",   u8'n',  0,  ios_defs::eofbit);
    CheckGet(obj, u8"x",    u8'n',  0,  ios_defs::goodbit);
    CheckGet(obj, u8"\n",   u8'n', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%En",  u8'n', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"n",    u8'n', u8'O', ios_defs::strfailbit);
    CheckGet(obj, u8"%On",  u8'n', u8'O', ios_defs::eofbit);

    CheckGet(obj, u8"\t",   u8't',  0,  ios_defs::eofbit);
    CheckGet(obj, u8"x",    u8't',  0,  ios_defs::goodbit);
    CheckGet(obj, u8"\t",   u8't', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Et",  u8't', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"n",    u8't', u8'O', ios_defs::strfailbit);
    CheckGet(obj, u8"%Ot",  u8't', u8'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 午後", u8"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 午前", u8"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(CheckGet(obj, u8"午後", u8'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(CheckGet(obj, u8"午前", u8'p', 0, ios_defs::eofbit).m_is_pm, false);
    CheckGet(obj, u8"%Ep", u8'p', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"p",   u8'p', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Op", u8'p', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"p",   u8'p', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"午後01時33分18秒", u8"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"%Er", u8'r', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"r",   u8'r', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Or", u8'r', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"r",   u8'r', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33", u8"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, u8"%ER", u8'R', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"R",   u8'R', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OR", u8'R', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"R",   u8'R', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"18", u8'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, u8"18", u8'S', u8'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, u8"十八", u8'S', u8'O', ios_defs::eofbit).m_second, 18);
    CheckGet(obj, u8"%ES", u8'S', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"S",   u8'S', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"S",   u8'S', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13時33分18秒", u8"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13時33分18秒", u8"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"X",   u8'X', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OX", u8'X', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"X",   u8'X', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33:18", u8"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"%ET", u8'T', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"T",   u8'T', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OT", u8'T', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"T",   u8'T', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"3", u8'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, u8"3", u8'u', u8'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, u8"三", u8'u', u8'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, u8"%Eu", u8'u', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"u",   u8'u', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"u",   u8'u', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"24", u8'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, u8"%Eg", u8'g', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"g",   u8'g', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Og", u8'g', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"g",   u8'g', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"2024", u8'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, u8"%EG", u8'G', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"G",   u8'G', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OG", u8'G', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"G",   u8'G', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 35 水", u8"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 35 水", u8"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 三十五 水", u8"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, u8"35", u8'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(CheckGet(obj, u8"35", u8'U', u8'O', ios_defs::eofbit).m_week_no, 35);
    CheckGet(obj, u8"%EU", u8'U', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"U",   u8'U', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"U",   u8'U', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 36 水", u8"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 36 水", u8"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2024 三十六 水", u8"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, u8"36", u8'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(CheckGet(obj, u8"36", u8'W', u8'O', ios_defs::eofbit).m_week_no, 36);
    CheckGet(obj, u8"%EW", u8'W', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"W",   u8'W', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"W",   u8'W', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"36", u8'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(CheckGet(obj, u8"36", u8'V', u8'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(CheckGet(obj, u8"三十六", u8'V', u8'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    CheckGet(obj, u8"54",  u8'V', u8'O', ios_defs::strfailbit);
    CheckGet(obj, u8"%EV", u8'V', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"V",   u8'V', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"V",   u8'V', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"3", u8'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, u8"3", u8'w', u8'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, u8"三", u8'w', u8'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, u8"%Ew", u8'w', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"w",   u8'w', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"w",   u8'w', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"24", u8'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"6", u8'y', u8'E', ios_defs::eofbit).year(), std::chrono::year(2024));
    EXPECT_EQ(CheckGet(obj, u8"24", u8'y', u8'O', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, u8"二十四", u8'y', u8'O', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, u8"y",  u8'y', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"y",  u8'y', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, u8"2024", u8'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, u8"2024", u8'Y', u8'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"平成3年", u8'Y', u8'E', ios_defs::eofbit).year(), std::chrono::year(1991));
    CheckGet(obj, u8"Y",   u8'Y', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OY", u8'Y', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"Y",   u8'Y', u8'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(CheckGet(obj, u8"America/Los_Angeles", u8'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, u8"PST", u8'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, u8"America/Los_Angexes", u8'Z', 0, ios_defs::strfailbit);
    CheckGet(obj, u8"%EZ", u8'Z', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"Z",   u8'Z', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%OZ", u8'Z', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"Z",   u8'Z', u8'O', ios_defs::strfailbit);

    CheckGet(obj, u8"Z", u8'z', 0, ios_defs::eofbit);
    CheckGet(obj, u8"+13", u8'z', 0, ios_defs::eofbit);
    CheckGet(obj, u8"-1110", u8'z', 0, ios_defs::eofbit);
    CheckGet(obj, u8"+11:10", u8'z', 0, ios_defs::eofbit);
    CheckGet(obj, u8"%Ez", u8'z', u8'E', ios_defs::eofbit);
    CheckGet(obj, u8"z",  u8'z', u8'E', ios_defs::strfailbit);
    CheckGet(obj, u8"%Oz", u8'z', u8'O', ios_defs::eofbit);
    CheckGet(obj, u8"z",  u8'z', u8'O', ios_defs::strfailbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"1999-W52-6", u8"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"2019-W01-1", u8"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"1999-W52-5", u8"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"99-W52-6", u8"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"19-W01-1", u8"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"99-W52-5", u8"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, u8"20 24/09/04", u8"%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((CheckGet<year_month_day>(obj, u8"20 01 01", u8"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar8, AWeekdayOrMonthNameIsMatchedAgainstBothSpellings)
{
    timeio obj(std::make_shared<timeio_conf<char8_t>>("C"));
    {
        std::u8string input = u8"Mon";
        std::u8string format = u8"%a";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 1);
    }

    {
        std::u8string input = u8"Tue ";
        std::u8string format = u8"%a";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != u8' '));
        EXPECT_EQ(time.tm_wday, 2);
    }

    {
        std::u8string input = u8"Wednesday";
        std::u8string format = u8"%a";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 3);
    }

    {
        std::u8string input = u8"Thu";
        std::u8string format = u8"%A";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 4);
    }

    {
        std::u8string input = u8"Fri ";
        std::u8string format = u8"%A";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != u8' '));
        EXPECT_EQ(time.tm_wday, 5);
    }

    {
        std::u8string input = u8"Saturday";
        std::u8string format = u8"%A";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 6);
    }

    {
        std::u8string input = u8"Feb";
        std::u8string format = u8"%b";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 1);
    }

    {
        std::u8string input = u8"Mar ";
        std::u8string format = u8"%b";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != u8' '));
        EXPECT_EQ(time.tm_mon, 2);
    }

    {
        std::u8string input = u8"April";
        std::u8string format = u8"%b";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 3);
    }

    {
        std::u8string input = u8"May";
        std::u8string format = u8"%B";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 4);
    }

    {
        std::u8string input = u8"Jun ";
        std::u8string format = u8"%B";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != u8' '));
        EXPECT_EQ(time.tm_mon, 5);
    }

    {
        std::u8string input = u8"July";
        std::u8string format = u8"%B";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 6);
    }

    {
        std::u8string input = u8"Aug";
        std::u8string format = u8"%h";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 7);
    }

    {
        std::u8string input = u8"May ";
        std::u8string format = u8"%h";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != u8' '));
        EXPECT_EQ(time.tm_mon, 4);
    }

    {
        std::u8string input = u8"October";
        std::u8string format = u8"%h";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 9);
    }

    // Other tests.
    {
        std::u8string input = u8"2.";
        std::u8string format = u8"%d.";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mday, 2);
    }

    {
        std::u8string input = u8"0.";
        std::u8string format = u8"%d.";

        time_parse_context<char8_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::u8string input = u8"32.";
        std::u8string format = u8"%d.";

        time_parse_context<char8_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::u8string input = u8"5.";
        std::u8string format = u8"%e.";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mday, 5);
    }

    {
        std::u8string input = u8"06.";
        std::u8string format = u8"%e.";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mday, 6);
    }

    {
        std::u8string input = u8"0";
        std::u8string format = u8"%e";

        time_parse_context<char8_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::u8string input = u8"35";
        std::u8string format = u8"%e";

        time_parse_context<char8_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    struct clock_case { const char8_t* input; int hour; int minute; };
    for (const clock_case tc : {
             clock_case{u8"12:11AM", 0, 11},
             clock_case{u8"03:14AM", 3, 14},
             clock_case{u8"09:27AM", 9, 27},
             clock_case{u8"12:29PM", 12, 29},
             clock_case{u8"02:38PM", 14, 38},
             clock_case{u8"09:52PM", 21, 52},
         })
    {
        std::u8string input(tc.input);
        time_parse_context<char8_t> ctx;
        const auto ret = obj.get(input.begin(), input.end(), ctx,
                                 std::u8string_view{u8"%I:%M%p"});
        const auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_hour, tc.hour);
        EXPECT_EQ(time.tm_min, tc.minute);
    }

    {
        std::u8string input = u8"08%46";
        std::u8string format = u8"%H%%%S";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_hour, 8);
        EXPECT_EQ(time.tm_sec, 46);
    }

    {
        std::u8string input = u8"29:14";
        std::u8string format = u8"%H:%M";

        time_parse_context<char8_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::u8string input = u8"Oct+tail";
        std::u8string format = u8"%b+tail";

        time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mon, 9);
    }
}

TEST(TimeioChar8, JapaneseReadsEveryConversionSpecifierIntoADate)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<char8_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<char8_t, true, true, tz_level::none>, true, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(u8"%",  u8'%',  0,  ios_defs::eofbit);
    FOri(u8"x",  u8'%',  0,  ios_defs::strfailbit);
    FOri(u8"%",  u8'%', u8'E', febit);
    FOri(u8"%E%", u8'%', u8'E', ios_defs::eofbit);
    FOri(u8"%",  u8'%', u8'O', febit);
    FOri(u8"%O%", u8'%', u8'O', ios_defs::eofbit);

    EXPECT_EQ(FOri(u8"水", u8'a', 0, ios_defs::eofbit).m_wday, 3);
    FOri(u8"%Ea", u8'a', u8'E', ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oa", u8'a', u8'O', ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"水曜日", u8'A', 0, ios_defs::eofbit).m_wday, 3);
    FOri(u8"%EA", u8'A', u8'E', ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'E', ios_defs::strfailbit);
    FOri(u8"%OA", u8'A', u8'O', ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"9月", u8'b', 0, ios_defs::eofbit).m_month, 9);
    FOri(u8"%Eb", u8'b', u8'E', ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'E', ios_defs::strfailbit);
    FOri(u8"%Ob", u8'b', u8'O', ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"9月", u8'B', 0, ios_defs::eofbit).m_month, 9);
    FOri(u8"%EB", u8'B', u8'E', ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'E', ios_defs::strfailbit);
    FOri(u8"%OB", u8'B', u8'O', ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"9月", u8'h', 0, ios_defs::eofbit).m_month, 9);
    FOri(u8"%Eh", u8'h', u8'E', ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oh", u8'h', u8'O', ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(FYmd(u8"2024年09月04日 13時33分18秒", u8'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"令和6年09月04日 13時33分18秒", u8'c', u8'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"202409月04日 13時33分18秒", u8'c', u8'E', ios_defs::eofbit), check_date1);
    FOri(u8"c",   u8'c', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oc", u8'c', u8'O', ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"20", u8'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(FYmd(u8"平成", u8'C', u8'E', ios_defs::eofbit).year(), std::chrono::year(1990));
    FOri(u8"C",   u8'C', u8'E', ios_defs::strfailbit);
    FOri(u8"%OC", u8'C', u8'O', ios_defs::eofbit);
    FOri(u8"C",   u8'C', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"04", u8'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(u8"04", u8'd', u8'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(u8"四", u8'd', u8'O', ios_defs::eofbit).m_mday, 4);
    FOri(u8"%Ed", u8'd', u8'E', ios_defs::eofbit);
    FOri(u8"d",   u8'd', u8'E', ios_defs::strfailbit);
    FOri(u8"d",   u8'd', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"4", u8'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(u8"4", u8'e', u8'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(u8"四", u8'e', u8'O', ios_defs::eofbit).m_mday, 4);
    FOri(u8"%Ee", u8'e', u8'E', ios_defs::eofbit);
    FOri(u8"e",   u8'e', u8'E', ios_defs::strfailbit);
    FOri(u8"e",   u8'e', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(u8"2024-09-04", u8'F', 0, ios_defs::eofbit), check_date1);
    FOri(u8"%EF", u8'F', u8'E', ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'E', ios_defs::strfailbit);
    FOri(u8"%OF", u8'F', u8'O', ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(u8"2024年09月04日", u8'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"令和6年09月04日", u8'x', u8'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"202409月04日", u8'x', u8'E', ios_defs::eofbit), check_date1);
    FOri(u8"x",   u8'x', u8'E', ios_defs::strfailbit);
    FOri(u8"%Ox", u8'x', u8'O', ios_defs::eofbit);
    FOri(u8"x",   u8'x', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(u8"09/04/24", u8'D', 0, ios_defs::eofbit), check_date1);
    FOri(u8"%ED", u8'D', u8'E', ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'E', ios_defs::strfailbit);
    FOri(u8"%OD", u8'D', u8'O', ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"13", u8'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(u8"13", u8'H', u8'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(u8"十三", u8'H', u8'O', ios_defs::eofbit).m_hour, 13);
    FOri(u8"%EH", u8'H', u8'E', ios_defs::eofbit);
    FOri(u8"H",   u8'H', u8'E', ios_defs::strfailbit);
    FOri(u8"H",   u8'H', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"01", u8'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(u8"01", u8'I', u8'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(u8"一", u8'I', u8'O', ios_defs::eofbit).m_hour, 1);
    FOri(u8"%EI", u8'I', u8'E', ios_defs::eofbit);
    FOri(u8"I",   u8'I', u8'E', ios_defs::strfailbit);
    FOri(u8"I",   u8'I', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"248", u8'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(FYmd(u8"2024 248", u8"%Y %j", ios_defs::eofbit), check_date1);
    FOri(u8"%Ej", u8'j', u8'E', ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oj", u8'j', u8'O', ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"09", u8'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri(u8"09", u8'm', u8'O', ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri(u8"九", u8'm', u8'O', ios_defs::eofbit).m_month, 9);
    FOri(u8"%Em", u8'm', u8'E', ios_defs::eofbit);
    FOri(u8"m",   u8'm', u8'E', ios_defs::strfailbit);
    FOri(u8"m",   u8'm', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"33", u8'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(u8"33", u8'M', u8'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(u8"三十三", u8'M', u8'O', ios_defs::eofbit).m_minute, 33);
    FOri(u8"%EM", u8'M', u8'E', ios_defs::eofbit);
    FOri(u8"M",   u8'M', u8'E', ios_defs::strfailbit);
    FOri(u8"M",   u8'M', u8'O', ios_defs::strfailbit);

    FOri(u8"\n",   u8'n',  0,  ios_defs::eofbit);
    FOri(u8"x",    u8'n',  0,  ios_defs::goodbit);
    FOri(u8"\n",   u8'n', u8'E', ios_defs::strfailbit);
    FOri(u8"%En",  u8'n', u8'E', ios_defs::eofbit);
    FOri(u8"n",    u8'n', u8'O', ios_defs::strfailbit);
    FOri(u8"%On",  u8'n', u8'O', ios_defs::eofbit);

    FOri(u8"\t",   u8't',  0,  ios_defs::eofbit);
    FOri(u8"x",    u8't',  0,  ios_defs::goodbit);
    FOri(u8"\t",   u8't', u8'E', ios_defs::strfailbit);
    FOri(u8"%Et",  u8't', u8'E', ios_defs::eofbit);
    FOri(u8"n",    u8't', u8'O', ios_defs::strfailbit);
    FOri(u8"%Ot",  u8't', u8'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 午後", u8"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 午前", u8"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(FOri(u8"午後", u8'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(FOri(u8"午前", u8'p', 0, ios_defs::eofbit).m_is_pm, false);
    FOri(u8"%Ep", u8'p', u8'E', ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'E', ios_defs::strfailbit);
    FOri(u8"%Op", u8'p', u8'O', ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"午後01時33分18秒", u8"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"%Er", u8'r', u8'E', ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'E', ios_defs::strfailbit);
    FOri(u8"%Or", u8'r', u8'O', ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33", u8"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(u8"%ER", u8'R', u8'E', ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'E', ios_defs::strfailbit);
    FOri(u8"%OR", u8'R', u8'O', ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"18", u8'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(u8"18", u8'S', u8'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(u8"十八", u8'S', u8'O', ios_defs::eofbit).m_second, 18);
    FOri(u8"%ES", u8'S', u8'E', ios_defs::eofbit);
    FOri(u8"S",   u8'S', u8'E', ios_defs::strfailbit);
    FOri(u8"S",   u8'S', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13時33分18秒", u8"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13時33分18秒", u8"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"X",   u8'X', u8'E', ios_defs::strfailbit);
    FOri(u8"%OX", u8'X', u8'O', ios_defs::eofbit);
    FOri(u8"X",   u8'X', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33:18", u8"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"%ET", u8'T', u8'E', ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'E', ios_defs::strfailbit);
    FOri(u8"%OT", u8'T', u8'O', ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"3", u8'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(u8"3", u8'u', u8'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(u8"三", u8'u', u8'O', ios_defs::eofbit).m_wday, 3);
    FOri(u8"%Eu", u8'u', u8'E', ios_defs::eofbit);
    FOri(u8"u",   u8'u', u8'E', ios_defs::strfailbit);
    FOri(u8"u",   u8'u', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"24", u8'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri(u8"%Eg", u8'g', u8'E', ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'E', ios_defs::strfailbit);
    FOri(u8"%Og", u8'g', u8'O', ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"2024", u8'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri(u8"%EG", u8'G', u8'E', ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'E', ios_defs::strfailbit);
    FOri(u8"%OG", u8'G', u8'O', ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(u8"2024 35 水", u8"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"2024 35 水", u8"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"2024 三十五 水", u8"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri(u8"35", u8'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(FOri(u8"35", u8'U', u8'O', ios_defs::eofbit).m_week_no, 35);
    FOri(u8"%EU", u8'U', u8'E', ios_defs::eofbit);
    FOri(u8"U",   u8'U', u8'E', ios_defs::strfailbit);
    FOri(u8"U",   u8'U', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(u8"2024 36 水", u8"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"2024 36 水", u8"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"2024 三十六 水", u8"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri(u8"36", u8'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(FOri(u8"36", u8'W', u8'O', ios_defs::eofbit).m_week_no, 36);
    FOri(u8"%EW", u8'W', u8'E', ios_defs::eofbit);
    FOri(u8"W",   u8'W', u8'E', ios_defs::strfailbit);
    FOri(u8"W",   u8'W', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"36", u8'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri(u8"36", u8'V', u8'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri(u8"三十六", u8'V', u8'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    FOri(u8"54",  u8'V', u8'O', ios_defs::strfailbit);
    FOri(u8"%EV", u8'V', u8'E', ios_defs::eofbit);
    FOri(u8"V",   u8'V', u8'E', ios_defs::strfailbit);
    FOri(u8"V",   u8'V', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"3", u8'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(u8"3", u8'w', u8'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(u8"三", u8'w', u8'O', ios_defs::eofbit).m_wday, 3);
    FOri(u8"%Ew", u8'w', u8'E', ios_defs::eofbit);
    FOri(u8"w",   u8'w', u8'E', ios_defs::strfailbit);
    FOri(u8"w",   u8'w', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"24", u8'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd(u8"6", u8'y', u8'E', ios_defs::eofbit).year(), std::chrono::year(2024));
    EXPECT_EQ(FOri(u8"24", u8'y', u8'O', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri(u8"二十四", u8'y', u8'O', ios_defs::eofbit).m_year, 2024);
    FOri(u8"y",  u8'y', u8'E', ios_defs::strfailbit);
    FOri(u8"y",  u8'y', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"2024", u8'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri(u8"2024", u8'Y', u8'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd(u8"平成3年", u8'Y', u8'E', ios_defs::eofbit).year(), std::chrono::year(1991));
    FOri(u8"Y",   u8'Y', u8'E', ios_defs::strfailbit);
    FOri(u8"%OY", u8'Y', u8'O', ios_defs::eofbit);
    FOri(u8"Y",   u8'Y', u8'O', ios_defs::strfailbit);

    FOri(u8"%Z", u8'Z', 0, ios_defs::eofbit);
    FOri(u8"%EZ", u8'Z', u8'E', ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'E', ios_defs::strfailbit);
    FOri(u8"%OZ", u8'Z', u8'O', ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'O', ios_defs::strfailbit);

    FOri(u8"%z", u8'z', 0, ios_defs::eofbit);
    FOri(u8"%Ez", u8'z', u8'E', ios_defs::eofbit);
    FOri(u8"%Oz", u8'z', u8'O', ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(FYmd(u8"1999-W52-6", u8"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd(u8"2019-W01-1", u8"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd(u8"1999-W52-5", u8"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd(u8"99-W52-6", u8"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd(u8"19-W01-1", u8"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd(u8"99-W52-5", u8"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd(u8"20 24/09/04", u8"%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((FYmd(u8"20 01 01", u8"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar8, JapaneseReadsEveryConversionSpecifierIntoADateWithoutAZone)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<char8_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<char8_t, true, false, tz_level::none>, true, false, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, false, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(u8"%",  u8'%',  0,  ios_defs::eofbit);
    FOri(u8"x",  u8'%',  0,  ios_defs::strfailbit);
    FOri(u8"%",  u8'%', u8'E', febit);
    FOri(u8"%E%", u8'%', u8'E', ios_defs::eofbit);
    FOri(u8"%",  u8'%', u8'O', febit);
    FOri(u8"%O%", u8'%', u8'O', ios_defs::eofbit);

    EXPECT_EQ(FOri(u8"水", u8'a', 0, ios_defs::eofbit).m_wday, 3);
    FOri(u8"%Ea", u8'a', u8'E', ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oa", u8'a', u8'O', ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"水曜日", u8'A', 0, ios_defs::eofbit).m_wday, 3);
    FOri(u8"%EA", u8'A', u8'E', ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'E', ios_defs::strfailbit);
    FOri(u8"%OA", u8'A', u8'O', ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"9月", u8'b', 0, ios_defs::eofbit).m_month, 9);
    FOri(u8"%Eb", u8'b', u8'E', ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'E', ios_defs::strfailbit);
    FOri(u8"%Ob", u8'b', u8'O', ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"9月", u8'B', 0, ios_defs::eofbit).m_month, 9);
    FOri(u8"%EB", u8'B', u8'E', ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'E', ios_defs::strfailbit);
    FOri(u8"%OB", u8'B', u8'O', ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"9月", u8'h', 0, ios_defs::eofbit).m_month, 9);
    FOri(u8"%Eh", u8'h', u8'E', ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oh", u8'h', u8'O', ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'O', ios_defs::strfailbit);

    using namespace std::chrono;
    FYmd(u8"%c", u8'c', 0, ios_defs::eofbit);
    FYmd(u8"%Ec", u8'c', u8'E', ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oc", u8'c', u8'O', ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"20", u8'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(FYmd(u8"平成", u8'C', u8'E', ios_defs::eofbit).year(), std::chrono::year(1990));
    FOri(u8"C",   u8'C', u8'E', ios_defs::strfailbit);
    FOri(u8"%OC", u8'C', u8'O', ios_defs::eofbit);
    FOri(u8"C",   u8'C', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"04", u8'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(u8"04", u8'd', u8'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(u8"四", u8'd', u8'O', ios_defs::eofbit).m_mday, 4);
    FOri(u8"%Ed", u8'd', u8'E', ios_defs::eofbit);
    FOri(u8"d",   u8'd', u8'E', ios_defs::strfailbit);
    FOri(u8"d",   u8'd', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"4", u8'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(u8"4", u8'e', u8'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(u8"四", u8'e', u8'O', ios_defs::eofbit).m_mday, 4);
    FOri(u8"%Ee", u8'e', u8'E', ios_defs::eofbit);
    FOri(u8"e",   u8'e', u8'E', ios_defs::strfailbit);
    FOri(u8"e",   u8'e', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(u8"2024-09-04", u8'F', 0, ios_defs::eofbit), check_date1);
    FOri(u8"%EF", u8'F', u8'E', ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'E', ios_defs::strfailbit);
    FOri(u8"%OF", u8'F', u8'O', ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(u8"2024年09月04日", u8'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"令和6年09月04日", u8'x', u8'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"202409月04日", u8'x', u8'E', ios_defs::eofbit), check_date1);
    FOri(u8"x",   u8'x', u8'E', ios_defs::strfailbit);
    FOri(u8"%Ox", u8'x', u8'O', ios_defs::eofbit);
    FOri(u8"x",   u8'x', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(u8"09/04/24", u8'D', 0, ios_defs::eofbit), check_date1);
    FOri(u8"%ED", u8'D', u8'E', ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'E', ios_defs::strfailbit);
    FOri(u8"%OD", u8'D', u8'O', ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'O', ios_defs::strfailbit);

    FOri(u8"%H", u8'H', 0,   ios_defs::eofbit);
    FOri(u8"%EH", u8'H', u8'E', ios_defs::eofbit);
    FOri(u8"H",   u8'H', u8'E', ios_defs::strfailbit);
    FOri(u8"H",   u8'H', u8'O', ios_defs::strfailbit);

    FOri(u8"%I", u8'I', 0,   ios_defs::eofbit);
    FOri(u8"%EI", u8'I', u8'E', ios_defs::eofbit);
    FOri(u8"I",   u8'I', u8'E', ios_defs::strfailbit);
    FOri(u8"I",   u8'I', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"248", u8'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(FYmd(u8"2024 248", u8"%Y %j", ios_defs::eofbit), check_date1);
    FOri(u8"%Ej", u8'j', u8'E', ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oj", u8'j', u8'O', ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"09", u8'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri(u8"09", u8'm', u8'O', ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri(u8"九", u8'm', u8'O', ios_defs::eofbit).m_month, 9);
    FOri(u8"%Em", u8'm', u8'E', ios_defs::eofbit);
    FOri(u8"m",   u8'm', u8'E', ios_defs::strfailbit);
    FOri(u8"m",   u8'm', u8'O', ios_defs::strfailbit);

    FOri(u8"%M", u8'M', 0,   ios_defs::eofbit);
    FOri(u8"%OM", u8'M', u8'O', ios_defs::eofbit);
    FOri(u8"%EM", u8'M', u8'E', ios_defs::eofbit);
    FOri(u8"M",   u8'M', u8'E', ios_defs::strfailbit);
    FOri(u8"M",   u8'M', u8'O', ios_defs::strfailbit);

    FOri(u8"\n",   u8'n',  0,  ios_defs::eofbit);
    FOri(u8"x",    u8'n',  0,  ios_defs::goodbit);
    FOri(u8"\n",   u8'n', u8'E', ios_defs::strfailbit);
    FOri(u8"%En",  u8'n', u8'E', ios_defs::eofbit);
    FOri(u8"n",    u8'n', u8'O', ios_defs::strfailbit);
    FOri(u8"%On",  u8'n', u8'O', ios_defs::eofbit);

    FOri(u8"\t",   u8't',  0,  ios_defs::eofbit);
    FOri(u8"x",    u8't',  0,  ios_defs::goodbit);
    FOri(u8"\t",   u8't', u8'E', ios_defs::strfailbit);
    FOri(u8"%Et",  u8't', u8'E', ios_defs::eofbit);
    FOri(u8"n",    u8't', u8'O', ios_defs::strfailbit);
    FOri(u8"%Ot",  u8't', u8'O', ios_defs::eofbit);

    FOri(u8"%p", u8'p', 0, ios_defs::eofbit);
    FOri(u8"%Ep", u8'p', u8'E', ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'E', ios_defs::strfailbit);
    FOri(u8"%Op", u8'p', u8'O', ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'O', ios_defs::strfailbit);

    FOri(u8"%r", u8"%r",  ios_defs::eofbit);
    FOri(u8"%Er", u8'r', u8'E', ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'E', ios_defs::strfailbit);
    FOri(u8"%Or", u8'r', u8'O', ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33", u8"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(u8"%ER", u8'R', u8'E', ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'E', ios_defs::strfailbit);
    FOri(u8"%OR", u8'R', u8'O', ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'O', ios_defs::strfailbit);

    FOri(u8"%S", u8'S', 0,   ios_defs::eofbit);
    FOri(u8"%OS", u8'S', u8'O', ios_defs::eofbit);
    FOri(u8"%ES", u8'S', u8'E', ios_defs::eofbit);
    FOri(u8"S",   u8'S', u8'E', ios_defs::strfailbit);
    FOri(u8"S",   u8'S', u8'O', ios_defs::strfailbit);

    FOri(u8"%X", u8"%X",  ios_defs::eofbit);
    FOri(u8"%EX", u8"%EX",  ios_defs::eofbit);
    FOri(u8"X",   u8'X', u8'E', ios_defs::strfailbit);
    FOri(u8"%OX", u8'X', u8'O', ios_defs::eofbit);
    FOri(u8"X",   u8'X', u8'O', ios_defs::strfailbit);

    FOri(u8"%T", u8"%T",  ios_defs::eofbit);
    FOri(u8"%ET", u8'T', u8'E', ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'E', ios_defs::strfailbit);
    FOri(u8"%OT", u8'T', u8'O', ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"3", u8'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(u8"3", u8'u', u8'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(u8"三", u8'u', u8'O', ios_defs::eofbit).m_wday, 3);
    FOri(u8"%Eu", u8'u', u8'E', ios_defs::eofbit);
    FOri(u8"u",   u8'u', u8'E', ios_defs::strfailbit);
    FOri(u8"u",   u8'u', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"24", u8'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri(u8"%Eg", u8'g', u8'E', ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'E', ios_defs::strfailbit);
    FOri(u8"%Og", u8'g', u8'O', ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"2024", u8'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri(u8"%EG", u8'G', u8'E', ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'E', ios_defs::strfailbit);
    FOri(u8"%OG", u8'G', u8'O', ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(u8"2024 35 水", u8"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"2024 35 水", u8"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"2024 三十五 水", u8"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri(u8"35", u8'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(FOri(u8"35", u8'U', u8'O', ios_defs::eofbit).m_week_no, 35);
    FOri(u8"%EU", u8'U', u8'E', ios_defs::eofbit);
    FOri(u8"U",   u8'U', u8'E', ios_defs::strfailbit);
    FOri(u8"U",   u8'U', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(u8"2024 36 水", u8"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"2024 36 水", u8"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(u8"2024 三十六 水", u8"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri(u8"36", u8'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(FOri(u8"36", u8'W', u8'O', ios_defs::eofbit).m_week_no, 36);
    FOri(u8"%EW", u8'W', u8'E', ios_defs::eofbit);
    FOri(u8"W",   u8'W', u8'E', ios_defs::strfailbit);
    FOri(u8"W",   u8'W', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"36", u8'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri(u8"36", u8'V', u8'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri(u8"三十六", u8'V', u8'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    FOri(u8"54",  u8'V', u8'O', ios_defs::strfailbit);
    FOri(u8"%EV", u8'V', u8'E', ios_defs::eofbit);
    FOri(u8"V",   u8'V', u8'E', ios_defs::strfailbit);
    FOri(u8"V",   u8'V', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"3", u8'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(u8"3", u8'w', u8'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(u8"三", u8'w', u8'O', ios_defs::eofbit).m_wday, 3);
    FOri(u8"%Ew", u8'w', u8'E', ios_defs::eofbit);
    FOri(u8"w",   u8'w', u8'E', ios_defs::strfailbit);
    FOri(u8"w",   u8'w', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"24", u8'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd(u8"6", u8'y', u8'E', ios_defs::eofbit).year(), std::chrono::year(2024));
    EXPECT_EQ(FOri(u8"24", u8'y', u8'O', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri(u8"二十四", u8'y', u8'O', ios_defs::eofbit).m_year, 2024);
    FOri(u8"y",  u8'y', u8'E', ios_defs::strfailbit);
    FOri(u8"y",  u8'y', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"2024", u8'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri(u8"2024", u8'Y', u8'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd(u8"平成3年", u8'Y', u8'E', ios_defs::eofbit).year(), std::chrono::year(1991));
    FOri(u8"Y",   u8'Y', u8'E', ios_defs::strfailbit);
    FOri(u8"%OY", u8'Y', u8'O', ios_defs::eofbit);
    FOri(u8"Y",   u8'Y', u8'O', ios_defs::strfailbit);

    FOri(u8"%Z", u8'Z', 0, ios_defs::eofbit);
    FOri(u8"%EZ", u8'Z', u8'E', ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'E', ios_defs::strfailbit);
    FOri(u8"%OZ", u8'Z', u8'O', ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'O', ios_defs::strfailbit);

    FOri(u8"%z", u8'z', 0, ios_defs::eofbit);
    FOri(u8"%Ez", u8'z', u8'E', ios_defs::eofbit);
    FOri(u8"%Oz", u8'z', u8'O', ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(FYmd(u8"1999-W52-6", u8"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd(u8"2019-W01-1", u8"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd(u8"1999-W52-5", u8"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd(u8"99-W52-6", u8"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd(u8"19-W01-1", u8"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd(u8"99-W52-5", u8"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd(u8"20 24/09/04", u8"%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((FYmd(u8"20 01 01", u8"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioChar8, ATimeOfDayReadsEveryConversionSpecifierItCanSupply)
{
    timeio obj(std::make_shared<timeio_conf<char8_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<char8_t, false, true, tz_level::zone>, false, true, tz_level::zone>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, tz_level::zone>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(u8"%",  u8'%',  0,  ios_defs::eofbit);
    FOri(u8"x",  u8'%',  0,  ios_defs::strfailbit);
    FOri(u8"%",  u8'%', u8'E', febit);
    FOri(u8"%E%", u8'%', u8'E', ios_defs::eofbit);
    FOri(u8"%",  u8'%', u8'O', febit);
    FOri(u8"%O%", u8'%', u8'O', ios_defs::eofbit);

    FOri(u8"%a", u8'a', 0, ios_defs::eofbit);
    FOri(u8"%Ea", u8'a', u8'E', ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oa", u8'a', u8'O', ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'O', ios_defs::strfailbit);

    FOri(u8"%A", u8'A', 0, ios_defs::eofbit);
    FOri(u8"%EA", u8'A', u8'E', ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'E', ios_defs::strfailbit);
    FOri(u8"%OA", u8'A', u8'O', ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'O', ios_defs::strfailbit);

    FOri(u8"%b", u8'b', 0, ios_defs::eofbit);
    FOri(u8"%Eb", u8'b', u8'E', ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'E', ios_defs::strfailbit);
    FOri(u8"%Ob", u8'b', u8'O', ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'O', ios_defs::strfailbit);

    FOri(u8"%B", u8'B', 0, ios_defs::eofbit);
    FOri(u8"%EB", u8'B', u8'E', ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'E', ios_defs::strfailbit);
    FOri(u8"%OB", u8'B', u8'O', ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'O', ios_defs::strfailbit);

    FOri(u8"%h", u8'h', 0, ios_defs::eofbit);
    FOri(u8"%Eh", u8'h', u8'E', ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oh", u8'h', u8'O', ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'O', ios_defs::strfailbit);

    using namespace std::chrono;
    FOri(u8"%c", u8'c', 0, ios_defs::eofbit);
    FOri(u8"%Ec", u8'c', u8'E', ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oc", u8'c', u8'O', ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'O', ios_defs::strfailbit);

    FOri(u8"%C", u8'C', 0,   ios_defs::eofbit);
    FOri(u8"%EC", u8'C', u8'E', ios_defs::eofbit);
    FOri(u8"C",   u8'C', u8'E', ios_defs::strfailbit);
    FOri(u8"%OC", u8'C', u8'O', ios_defs::eofbit);
    FOri(u8"C",   u8'C', u8'O', ios_defs::strfailbit);

    FOri(u8"%d", u8'd', 0,   ios_defs::eofbit);
    FOri(u8"%Od", u8'd', u8'O', ios_defs::eofbit);
    FOri(u8"%Ed", u8'd', u8'E', ios_defs::eofbit);
    FOri(u8"d",   u8'd', u8'E', ios_defs::strfailbit);
    FOri(u8"d",   u8'd', u8'O', ios_defs::strfailbit);

    FOri(u8"%e", u8'e', 0,   ios_defs::eofbit);
    FOri(u8"%Oe", u8'e', u8'O', ios_defs::eofbit);
    FOri(u8"%Ee", u8'e', u8'E', ios_defs::eofbit);
    FOri(u8"e",   u8'e', u8'E', ios_defs::strfailbit);
    FOri(u8"e",   u8'e', u8'O', ios_defs::strfailbit);

    FOri(u8"%F", u8'F', 0, ios_defs::eofbit);
    FOri(u8"%EF", u8'F', u8'E', ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'E', ios_defs::strfailbit);
    FOri(u8"%OF", u8'F', u8'O', ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'O', ios_defs::strfailbit);

    FOri(u8"%x", u8'x', 0, ios_defs::eofbit);
    FOri(u8"%Ex", u8'x', u8'E', ios_defs::eofbit);
    FOri(u8"x",   u8'x', u8'E', ios_defs::strfailbit);
    FOri(u8"%Ox", u8'x', u8'O', ios_defs::eofbit);
    FOri(u8"x",   u8'x', u8'O', ios_defs::strfailbit);

    FOri(u8"%D", u8'D', 0, ios_defs::eofbit);
    FOri(u8"%ED", u8'D', u8'E', ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'E', ios_defs::strfailbit);
    FOri(u8"%OD", u8'D', u8'O', ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"13", u8'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(u8"13", u8'H', u8'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(u8"十三", u8'H', u8'O', ios_defs::eofbit).m_hour, 13);
    FOri(u8"%EH", u8'H', u8'E', ios_defs::eofbit);
    FOri(u8"H",   u8'H', u8'E', ios_defs::strfailbit);
    FOri(u8"H",   u8'H', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"01", u8'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(u8"01", u8'I', u8'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(u8"一", u8'I', u8'O', ios_defs::eofbit).m_hour, 1);
    FOri(u8"%EI", u8'I', u8'E', ios_defs::eofbit);
    FOri(u8"I",   u8'I', u8'E', ios_defs::strfailbit);
    FOri(u8"I",   u8'I', u8'O', ios_defs::strfailbit);

    FOri(u8"%j", u8'j', 0, ios_defs::eofbit);
    FOri(u8"%Ej", u8'j', u8'E', ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oj", u8'j', u8'O', ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'O', ios_defs::strfailbit);

    FOri(u8"%m", u8'm',  0, ios_defs::eofbit);
    FOri(u8"%Om", u8'm', u8'O', ios_defs::eofbit);
    FOri(u8"%Em", u8'm', u8'E', ios_defs::eofbit);
    FOri(u8"m",   u8'm', u8'E', ios_defs::strfailbit);
    FOri(u8"m",   u8'm', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"33", u8'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(u8"33", u8'M', u8'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(u8"三十三", u8'M', u8'O', ios_defs::eofbit).m_minute, 33);
    FOri(u8"%EM", u8'M', u8'E', ios_defs::eofbit);
    FOri(u8"M",   u8'M', u8'E', ios_defs::strfailbit);
    FOri(u8"M",   u8'M', u8'O', ios_defs::strfailbit);

    FOri(u8"\n",   u8'n',  0,  ios_defs::eofbit);
    FOri(u8"x",    u8'n',  0,  ios_defs::goodbit);
    FOri(u8"\n",   u8'n', u8'E', ios_defs::strfailbit);
    FOri(u8"%En",  u8'n', u8'E', ios_defs::eofbit);
    FOri(u8"n",    u8'n', u8'O', ios_defs::strfailbit);
    FOri(u8"%On",  u8'n', u8'O', ios_defs::eofbit);

    FOri(u8"\t",   u8't',  0,  ios_defs::eofbit);
    FOri(u8"x",    u8't',  0,  ios_defs::goodbit);
    FOri(u8"\t",   u8't', u8'E', ios_defs::strfailbit);
    FOri(u8"%Et",  u8't', u8'E', ios_defs::eofbit);
    FOri(u8"n",    u8't', u8'O', ios_defs::strfailbit);
    FOri(u8"%Ot",  u8't', u8'O', ios_defs::eofbit);

    EXPECT_EQ(FHms(u8"01 午後", u8"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(FHms(u8"01 午前", u8"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(FOri(u8"午後", u8'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(FOri(u8"午前", u8'p', 0, ios_defs::eofbit).m_is_pm, false);
    FOri(u8"%Ep", u8'p', u8'E', ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'E', ios_defs::strfailbit);
    FOri(u8"%Op", u8'p', u8'O', ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(u8"午後01時33分18秒", u8"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"%Er", u8'r', u8'E', ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'E', ios_defs::strfailbit);
    FOri(u8"%Or", u8'r', u8'O', ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(u8"13:33", u8"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(u8"%ER", u8'R', u8'E', ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'E', ios_defs::strfailbit);
    FOri(u8"%OR", u8'R', u8'O', ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"18", u8'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(u8"18", u8'S', u8'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(u8"十八", u8'S', u8'O', ios_defs::eofbit).m_second, 18);
    FOri(u8"%ES", u8'S', u8'E', ios_defs::eofbit);
    FOri(u8"S",   u8'S', u8'E', ios_defs::strfailbit);
    FOri(u8"S",   u8'S', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(u8"13時33分18秒", u8"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(FHms(u8"13時33分18秒", u8"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"X",   u8'X', u8'E', ios_defs::strfailbit);
    FOri(u8"%OX", u8'X', u8'O', ios_defs::eofbit);
    FOri(u8"X",   u8'X', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(u8"13:33:18", u8"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"%ET", u8'T', u8'E', ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'E', ios_defs::strfailbit);
    FOri(u8"%OT", u8'T', u8'O', ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'O', ios_defs::strfailbit);

    FOri(u8"%u", u8'u', 0,   ios_defs::eofbit);
    FOri(u8"%Ou", u8'u', u8'O', ios_defs::eofbit);
    FOri(u8"%Eu", u8'u', u8'E', ios_defs::eofbit);
    FOri(u8"u",   u8'u', u8'E', ios_defs::strfailbit);
    FOri(u8"u",   u8'u', u8'O', ios_defs::strfailbit);

    FOri(u8"%g", u8'g', 0, ios_defs::eofbit);
    FOri(u8"%Eg", u8'g', u8'E', ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'E', ios_defs::strfailbit);
    FOri(u8"%Og", u8'g', u8'O', ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'O', ios_defs::strfailbit);

    FOri(u8"%G", u8'G', 0, ios_defs::eofbit);
    FOri(u8"%EG", u8'G', u8'E', ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'E', ios_defs::strfailbit);
    FOri(u8"%OG", u8'G', u8'O', ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'O', ios_defs::strfailbit);

    FOri(u8"%U", u8'U', 0,   ios_defs::eofbit);
    FOri(u8"%OU", u8'U', u8'O', ios_defs::eofbit);
    FOri(u8"%EU", u8'U', u8'E', ios_defs::eofbit);
    FOri(u8"U",   u8'U', u8'E', ios_defs::strfailbit);
    FOri(u8"U",   u8'U', u8'O', ios_defs::strfailbit);

    FOri(u8"%W", u8'W', 0,   ios_defs::eofbit);
    FOri(u8"%OW", u8'W', u8'O', ios_defs::eofbit);
    FOri(u8"%EW", u8'W', u8'E', ios_defs::eofbit);
    FOri(u8"W",   u8'W', u8'E', ios_defs::strfailbit);
    FOri(u8"W",   u8'W', u8'O', ios_defs::strfailbit);

    FOri(u8"%V", u8'V', 0,   ios_defs::eofbit);
    FOri(u8"%OV", u8'V', u8'O',   ios_defs::eofbit);
    FOri(u8"54",  u8'V', u8'O', ios_defs::strfailbit);
    FOri(u8"%EV", u8'V', u8'E', ios_defs::eofbit);
    FOri(u8"V",   u8'V', u8'E', ios_defs::strfailbit);
    FOri(u8"V",   u8'V', u8'O', ios_defs::strfailbit);

    FOri(u8"%w", u8'w', 0,   ios_defs::eofbit);
    FOri(u8"%Ow", u8'w', u8'O', ios_defs::eofbit);
    FOri(u8"%Ew", u8'w', u8'E', ios_defs::eofbit);
    FOri(u8"w",   u8'w', u8'E', ios_defs::strfailbit);
    FOri(u8"w",   u8'w', u8'O', ios_defs::strfailbit);

    FOri(u8"%y", u8'y', 0,   ios_defs::eofbit);
    FOri(u8"%Ey", u8'y', u8'E', ios_defs::eofbit);
    FOri(u8"%Oy", u8'y', u8'O', ios_defs::eofbit);
    FOri(u8"y",  u8'y', u8'E', ios_defs::strfailbit);
    FOri(u8"y",  u8'y', u8'O', ios_defs::strfailbit);

    FOri(u8"%Y", u8'Y', 0,   ios_defs::eofbit);
    FOri(u8"%EY", u8'Y', u8'E', ios_defs::eofbit);
    FOri(u8"Y",   u8'Y', u8'E', ios_defs::strfailbit);
    FOri(u8"%OY", u8'Y', u8'O', ios_defs::eofbit);
    FOri(u8"Y",   u8'Y', u8'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(FOri(u8"America/Los_Angeles", u8'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = FOri(u8"PST", u8'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    FOri(u8"America/Los_Angexes", u8'Z', 0, ios_defs::strfailbit);
    FOri(u8"%EZ", u8'Z', u8'E', ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'E', ios_defs::strfailbit);
    FOri(u8"%OZ", u8'Z', u8'O', ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'O', ios_defs::strfailbit);

    { auto r = FOri(u8"+0800", u8'z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_have_offset && r.m_offset == minutes{480}); }
    FOri(u8"%z", u8'z', 0, ios_defs::strfailbit);
    FOri(u8"%Ez", u8'z', u8'E', ios_defs::eofbit);
    FOri(u8"z",  u8'z', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oz", u8'z', u8'O', ios_defs::eofbit);
    FOri(u8"z",  u8'z', u8'O', ios_defs::strfailbit);
}

TEST(TimeioChar8, ATimeOfDayReadsTheSameSpecifiersWithNoZoneTier)
{
    timeio obj(std::make_shared<timeio_conf<char8_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<char8_t, false, true, tz_level::none>, false, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(u8"%",  u8'%',  0,  ios_defs::eofbit);
    FOri(u8"x",  u8'%',  0,  ios_defs::strfailbit);
    FOri(u8"%",  u8'%', u8'E', febit);
    FOri(u8"%E%", u8'%', u8'E', ios_defs::eofbit);
    FOri(u8"%",  u8'%', u8'O', febit);
    FOri(u8"%O%", u8'%', u8'O', ios_defs::eofbit);

    FOri(u8"%a", u8'a', 0, ios_defs::eofbit);
    FOri(u8"%Ea", u8'a', u8'E', ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oa", u8'a', u8'O', ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'O', ios_defs::strfailbit);

    FOri(u8"%A", u8'A', 0, ios_defs::eofbit);
    FOri(u8"%EA", u8'A', u8'E', ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'E', ios_defs::strfailbit);
    FOri(u8"%OA", u8'A', u8'O', ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'O', ios_defs::strfailbit);

    FOri(u8"%b", u8'b', 0, ios_defs::eofbit);
    FOri(u8"%Eb", u8'b', u8'E', ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'E', ios_defs::strfailbit);
    FOri(u8"%Ob", u8'b', u8'O', ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'O', ios_defs::strfailbit);

    FOri(u8"%B", u8'B', 0, ios_defs::eofbit);
    FOri(u8"%EB", u8'B', u8'E', ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'E', ios_defs::strfailbit);
    FOri(u8"%OB", u8'B', u8'O', ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'O', ios_defs::strfailbit);

    FOri(u8"%h", u8'h', 0, ios_defs::eofbit);
    FOri(u8"%Eh", u8'h', u8'E', ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oh", u8'h', u8'O', ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'O', ios_defs::strfailbit);

    using namespace std::chrono;
    FOri(u8"%c", u8'c', 0, ios_defs::eofbit);
    FOri(u8"%Ec", u8'c', u8'E', ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oc", u8'c', u8'O', ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'O', ios_defs::strfailbit);

    FOri(u8"%C", u8'C', 0,   ios_defs::eofbit);
    FOri(u8"%EC", u8'C', u8'E', ios_defs::eofbit);
    FOri(u8"C",   u8'C', u8'E', ios_defs::strfailbit);
    FOri(u8"%OC", u8'C', u8'O', ios_defs::eofbit);
    FOri(u8"C",   u8'C', u8'O', ios_defs::strfailbit);

    FOri(u8"%d", u8'd', 0,   ios_defs::eofbit);
    FOri(u8"%Od", u8'd', u8'O', ios_defs::eofbit);
    FOri(u8"%Ed", u8'd', u8'E', ios_defs::eofbit);
    FOri(u8"d",   u8'd', u8'E', ios_defs::strfailbit);
    FOri(u8"d",   u8'd', u8'O', ios_defs::strfailbit);

    FOri(u8"%e", u8'e', 0,   ios_defs::eofbit);
    FOri(u8"%Oe", u8'e', u8'O', ios_defs::eofbit);
    FOri(u8"%Ee", u8'e', u8'E', ios_defs::eofbit);
    FOri(u8"e",   u8'e', u8'E', ios_defs::strfailbit);
    FOri(u8"e",   u8'e', u8'O', ios_defs::strfailbit);

    FOri(u8"%F", u8'F', 0, ios_defs::eofbit);
    FOri(u8"%EF", u8'F', u8'E', ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'E', ios_defs::strfailbit);
    FOri(u8"%OF", u8'F', u8'O', ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'O', ios_defs::strfailbit);

    FOri(u8"%x", u8'x', 0, ios_defs::eofbit);
    FOri(u8"%Ex", u8'x', u8'E', ios_defs::eofbit);
    FOri(u8"x",   u8'x', u8'E', ios_defs::strfailbit);
    FOri(u8"%Ox", u8'x', u8'O', ios_defs::eofbit);
    FOri(u8"x",   u8'x', u8'O', ios_defs::strfailbit);

    FOri(u8"%D", u8'D', 0, ios_defs::eofbit);
    FOri(u8"%ED", u8'D', u8'E', ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'E', ios_defs::strfailbit);
    FOri(u8"%OD", u8'D', u8'O', ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"13", u8'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(u8"13", u8'H', u8'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(u8"十三", u8'H', u8'O', ios_defs::eofbit).m_hour, 13);
    FOri(u8"%EH", u8'H', u8'E', ios_defs::eofbit);
    FOri(u8"H",   u8'H', u8'E', ios_defs::strfailbit);
    FOri(u8"H",   u8'H', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"01", u8'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(u8"01", u8'I', u8'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(u8"一", u8'I', u8'O', ios_defs::eofbit).m_hour, 1);
    FOri(u8"%EI", u8'I', u8'E', ios_defs::eofbit);
    FOri(u8"I",   u8'I', u8'E', ios_defs::strfailbit);
    FOri(u8"I",   u8'I', u8'O', ios_defs::strfailbit);

    FOri(u8"%j", u8'j', 0, ios_defs::eofbit);
    FOri(u8"%Ej", u8'j', u8'E', ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oj", u8'j', u8'O', ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'O', ios_defs::strfailbit);

    FOri(u8"%m", u8'm',  0, ios_defs::eofbit);
    FOri(u8"%Om", u8'm', u8'O', ios_defs::eofbit);
    FOri(u8"%Em", u8'm', u8'E', ios_defs::eofbit);
    FOri(u8"m",   u8'm', u8'E', ios_defs::strfailbit);
    FOri(u8"m",   u8'm', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"33", u8'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(u8"33", u8'M', u8'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(u8"三十三", u8'M', u8'O', ios_defs::eofbit).m_minute, 33);
    FOri(u8"%EM", u8'M', u8'E', ios_defs::eofbit);
    FOri(u8"M",   u8'M', u8'E', ios_defs::strfailbit);
    FOri(u8"M",   u8'M', u8'O', ios_defs::strfailbit);

    FOri(u8"\n",   u8'n',  0,  ios_defs::eofbit);
    FOri(u8"x",    u8'n',  0,  ios_defs::goodbit);
    FOri(u8"\n",   u8'n', u8'E', ios_defs::strfailbit);
    FOri(u8"%En",  u8'n', u8'E', ios_defs::eofbit);
    FOri(u8"n",    u8'n', u8'O', ios_defs::strfailbit);
    FOri(u8"%On",  u8'n', u8'O', ios_defs::eofbit);

    FOri(u8"\t",   u8't',  0,  ios_defs::eofbit);
    FOri(u8"x",    u8't',  0,  ios_defs::goodbit);
    FOri(u8"\t",   u8't', u8'E', ios_defs::strfailbit);
    FOri(u8"%Et",  u8't', u8'E', ios_defs::eofbit);
    FOri(u8"n",    u8't', u8'O', ios_defs::strfailbit);
    FOri(u8"%Ot",  u8't', u8'O', ios_defs::eofbit);

    EXPECT_EQ(FHms(u8"01 午後", u8"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(FHms(u8"01 午前", u8"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(FOri(u8"午後", u8'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(FOri(u8"午前", u8'p', 0, ios_defs::eofbit).m_is_pm, false);
    FOri(u8"%Ep", u8'p', u8'E', ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'E', ios_defs::strfailbit);
    FOri(u8"%Op", u8'p', u8'O', ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(u8"午後01時33分18秒", u8"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"%Er", u8'r', u8'E', ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'E', ios_defs::strfailbit);
    FOri(u8"%Or", u8'r', u8'O', ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(u8"13:33", u8"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(u8"%ER", u8'R', u8'E', ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'E', ios_defs::strfailbit);
    FOri(u8"%OR", u8'R', u8'O', ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(u8"18", u8'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(u8"18", u8'S', u8'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(u8"十八", u8'S', u8'O', ios_defs::eofbit).m_second, 18);
    FOri(u8"%ES", u8'S', u8'E', ios_defs::eofbit);
    FOri(u8"S",   u8'S', u8'E', ios_defs::strfailbit);
    FOri(u8"S",   u8'S', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(u8"13時33分18秒", u8"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(FHms(u8"13時33分18秒", u8"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"X",   u8'X', u8'E', ios_defs::strfailbit);
    FOri(u8"%OX", u8'X', u8'O', ios_defs::eofbit);
    FOri(u8"X",   u8'X', u8'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(u8"13:33:18", u8"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"%ET", u8'T', u8'E', ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'E', ios_defs::strfailbit);
    FOri(u8"%OT", u8'T', u8'O', ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'O', ios_defs::strfailbit);

    FOri(u8"%u", u8'u', 0,   ios_defs::eofbit);
    FOri(u8"%Ou", u8'u', u8'O', ios_defs::eofbit);
    FOri(u8"%Eu", u8'u', u8'E', ios_defs::eofbit);
    FOri(u8"u",   u8'u', u8'E', ios_defs::strfailbit);
    FOri(u8"u",   u8'u', u8'O', ios_defs::strfailbit);

    FOri(u8"%g", u8'g', 0, ios_defs::eofbit);
    FOri(u8"%Eg", u8'g', u8'E', ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'E', ios_defs::strfailbit);
    FOri(u8"%Og", u8'g', u8'O', ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'O', ios_defs::strfailbit);

    FOri(u8"%G", u8'G', 0, ios_defs::eofbit);
    FOri(u8"%EG", u8'G', u8'E', ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'E', ios_defs::strfailbit);
    FOri(u8"%OG", u8'G', u8'O', ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'O', ios_defs::strfailbit);

    FOri(u8"%U", u8'U', 0,   ios_defs::eofbit);
    FOri(u8"%OU", u8'U', u8'O', ios_defs::eofbit);
    FOri(u8"%EU", u8'U', u8'E', ios_defs::eofbit);
    FOri(u8"U",   u8'U', u8'E', ios_defs::strfailbit);
    FOri(u8"U",   u8'U', u8'O', ios_defs::strfailbit);

    FOri(u8"%W", u8'W', 0,   ios_defs::eofbit);
    FOri(u8"%OW", u8'W', u8'O', ios_defs::eofbit);
    FOri(u8"%EW", u8'W', u8'E', ios_defs::eofbit);
    FOri(u8"W",   u8'W', u8'E', ios_defs::strfailbit);
    FOri(u8"W",   u8'W', u8'O', ios_defs::strfailbit);

    FOri(u8"%V", u8'V', 0,   ios_defs::eofbit);
    FOri(u8"%OV", u8'V', u8'O',   ios_defs::eofbit);
    FOri(u8"54",  u8'V', u8'O', ios_defs::strfailbit);
    FOri(u8"%EV", u8'V', u8'E', ios_defs::eofbit);
    FOri(u8"V",   u8'V', u8'E', ios_defs::strfailbit);
    FOri(u8"V",   u8'V', u8'O', ios_defs::strfailbit);

    FOri(u8"%w", u8'w', 0,   ios_defs::eofbit);
    FOri(u8"%Ow", u8'w', u8'O', ios_defs::eofbit);
    FOri(u8"%Ew", u8'w', u8'E', ios_defs::eofbit);
    FOri(u8"w",   u8'w', u8'E', ios_defs::strfailbit);
    FOri(u8"w",   u8'w', u8'O', ios_defs::strfailbit);

    FOri(u8"%y", u8'y', 0,   ios_defs::eofbit);
    FOri(u8"%Ey", u8'y', u8'E', ios_defs::eofbit);
    FOri(u8"%Oy", u8'y', u8'O', ios_defs::eofbit);
    FOri(u8"y",  u8'y', u8'E', ios_defs::strfailbit);
    FOri(u8"y",  u8'y', u8'O', ios_defs::strfailbit);

    FOri(u8"%Y", u8'Y', 0,   ios_defs::eofbit);
    FOri(u8"%EY", u8'Y', u8'E', ios_defs::eofbit);
    FOri(u8"Y",   u8'Y', u8'E', ios_defs::strfailbit);
    FOri(u8"%OY", u8'Y', u8'O', ios_defs::eofbit);
    FOri(u8"Y",   u8'Y', u8'O', ios_defs::strfailbit);

    FOri(u8"%Z", u8'Z', 0, ios_defs::eofbit);
    FOri(u8"%EZ", u8'Z', u8'E', ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'E', ios_defs::strfailbit);
    FOri(u8"%OZ", u8'Z', u8'O', ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'O', ios_defs::strfailbit);

    FOri(u8"%z", u8'z', 0, ios_defs::eofbit);
    FOri(u8"%Ez", u8'z', u8'E', ios_defs::eofbit);
    FOri(u8"z",  u8'z', u8'E', ios_defs::strfailbit);
    FOri(u8"%Oz", u8'z', u8'O', ios_defs::eofbit);
    FOri(u8"z",  u8'z', u8'O', ios_defs::strfailbit);
}

TEST(TimeioChar8, AValueThatIsNotAValidTimeIsRejected)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char8_t>>("C"));
    std::u8string res;

    // put(year_month_day) with invalid date (line 1173)
    {
        auto invalid_ymd = year_month_day{year{2024}, month{2}, day{30}};
        EXPECT_THROW(obj.put(std::back_inserter(res), invalid_ymd, std::u8string_view(u8"%F")), stream_error);
    }

    // put(hh_mm_ss) with negative total duration (line 1214)
    {
        hh_mm_ss<seconds> invalid_hms{seconds{-1}};
        EXPECT_THROW(obj.put(std::back_inserter(res), invalid_hms, std::u8string_view(u8"%T")), stream_error);
    }

    // put(std::tm) with out-of-range field: month=-1 (line 1271)
    {
        std::tm bad_tm{};
        bad_tm.tm_year = 124; bad_tm.tm_mon = -1;
        bad_tm.tm_mday = 1; bad_tm.tm_hour = 0; bad_tm.tm_min = 0; bad_tm.tm_sec = 0;
        EXPECT_THROW(obj.put(std::back_inserter(res), bad_tm, std::u8string_view(u8"%F")), stream_error);
    }

    // put(std::tm) with valid fields but invalid date: Feb 30 (line 1275)
    {
        std::tm bad_tm{};
        bad_tm.tm_year = 124; bad_tm.tm_mon = 1; bad_tm.tm_mday = 30;
        bad_tm.tm_hour = 0; bad_tm.tm_min = 0; bad_tm.tm_sec = 0;
        EXPECT_THROW(obj.put(std::back_inserter(res), bad_tm, std::u8string_view(u8"%F")), stream_error);
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
            EXPECT_THROW(obj.put(std::back_inserter(res), bad_tm, std::u8string_view(u8"%Y")), stream_error);
        }
    }

    // put(std::tm) at the year bounds themselves: still accepted
    {
        std::tm edge_tm{};
        edge_tm.tm_mon = 0; edge_tm.tm_mday = 1;
        edge_tm.tm_hour = 0; edge_tm.tm_min = 0; edge_tm.tm_sec = 0;

        edge_tm.tm_year = static_cast<int>(year::max()) - 1900;
        res.clear(); obj.put(std::back_inserter(res), edge_tm, std::u8string_view(u8"%Y"));
        EXPECT_EQ(res, u8"32767");

        edge_tm.tm_year = static_cast<int>(year::min()) - 1900;
        res.clear(); obj.put(std::back_inserter(res), edge_tm, std::u8string_view(u8"%Y"));
        EXPECT_EQ(res, u8"-32767");
    }

    // put(year_month_day) with negative year: %Y and %C output sign (lines 2860-2861, 2543-2544)
    {
        auto neg_ymd = year_month_day{year{-1}, month{1}, day{1}};
        res.clear(); obj.put(std::back_inserter(res), neg_ymd, std::u8string_view(u8"%Y"));
        EXPECT_EQ(res, u8"-0001");
        res.clear(); obj.put(std::back_inserter(res), neg_ymd, std::u8string_view(u8"%C"));
        EXPECT_EQ(res, u8"-01");
    }

    // put(year_month_day) for date in ISO year -1: %G output sign (lines 2608-2609)
    // Jan 1, year 0 is a Saturday; Thu of that ISO week is Dec 30, year -1 -> G=-0001
    {
        auto early_ymd = year_month_day{year{0}, month{1}, day{1}};
        res.clear(); obj.put(std::back_inserter(res), early_ymd, std::u8string_view(u8"%G"));
        EXPECT_EQ(res, u8"-0001");
    }

    // put(zoned_time) with positive offset: %z outputs '+' (line 2883)
    {
        auto tp = create_zoned_time(2024, 9, 4, 12, 0, 0, "Asia/Tokyo");
        res.clear(); obj.put(std::back_inserter(res), tp, std::u8string_view(u8"%z"));
        EXPECT_EQ(res, u8"+0900");
    }
}

// A format string ending in a lone '%' -- or in a lone '%E' / '%O' modifier -- introduces no
// specifier, so there is nothing to convert. It follows the same rule this facet already uses
// for a specifier it does not recognize (see the "unknown format" path, which emits '%' plus
// the rest verbatim): put writes the '%' out and get matches it back as a literal. Handling
// the two sides alike is what keeps the round-trip invariant -- whatever put writes, get reads
// back with the same format string. put previously dropped the '%' silently while get rejected
// it, so put succeeded on output get could never read.
TEST(TimeioChar8, ALoneOrUnknownSpecifierIsEchoedVerbatim)
{
    timeio obj(std::make_shared<timeio_conf<char8_t>>("C"));
    const std::tm t = calendar_time(124, 0, 15, 1, 2, 3, 1, 14, 0);

    struct { const char8_t* fmt; const char8_t* want; } cases[] = {
        {u8"%Y%", u8"2024%"},   // a lone u8'%' after a real specifier
        {u8"%",   u8"%"},       // nothing but the lone u8'%'
        {u8"a%",  u8"a%"},      // a lone u8'%' after literal text
        {u8"%E",  u8"%E"},      // a lone u8'E' modifier with no specifier to modify
        {u8"%O",  u8"%O"},      // ditto for u8'O'
        {u8"%%",  u8"%"},       // control: an escaped u8'%' still collapses to one
        {u8"%Q",  u8"%Q"},      // control: an unrecognized specifier is already emitted verbatim
    };

    for (const auto& c : cases)
    {
        std::u8string res;
        obj.put(std::back_inserter(res), t, std::u8string_view(c.fmt));
        EXPECT_EQ(res, c.want);

        // The round trip: get consumes exactly what put produced, using the same format.
        time_parse_context<char8_t> ctx;
        EXPECT_EQ(obj.get(res.begin(), res.end(), ctx, std::u8string_view(c.fmt)), res.end());
    }

    // get still rejects input that lacks the literal '%' the format asks for, so the
    // agreement above is a real match rather than the trailing '%' being ignored.
    {
        const std::u8string in = u8"2024";
        time_parse_context<char8_t> ctx;
        EXPECT_THROW(obj.get(in.begin(), in.end(), ctx, std::u8string_view(u8"%Y%")), stream_error);
    }
}

// The two tiers pinned apart. Whether %Z parses is the tier's decision and nothing else's:
// tz_level::offset matches it literally, which is exactly what put degrades it to for a value
// with no zone to name, and tz_level::zone parses it against the trie. Neither tier looks at
// what the trie happens to contain to decide which of the two it is doing.
TEST(TimeioChar8, TheZoneTierDecidesHowAZoneNameIsParsed)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<char8_t>>("C"));

    auto off_ok = [&obj](const std::u8string& in, const char8_t* fmt)
    {
        time_parse_context<char8_t, true, true, tz_level::offset> ctx;
        try { return obj.get(in.begin(), in.end(), ctx, std::u8string_view(fmt)) == in.end(); }
        catch (stream_error&) { return false; }
    };
    auto zone_ok = [&obj](const std::u8string& in, const char8_t* fmt)
    {
        time_parse_context<char8_t, true, true, tz_level::zone> ctx;
        try { return obj.get(in.begin(), in.end(), ctx, std::u8string_view(fmt)) == in.end(); }
        catch (stream_error&) { return false; }
    };

    // The literal %Z, which is what put writes when the value has no zone to offer.
    EXPECT_TRUE(off_ok(u8"%Z", u8"%Z"));
    EXPECT_FALSE(zone_ok(u8"%Z", u8"%Z"));

    // A real zone token parses at tz_level::zone and only there. At tz_level::offset the format
    // is asking for the two characters %Z, which "UTC" is not -- put never writes a zone token
    // for a value that parses at that tier, so there is nothing to read back.
    EXPECT_TRUE(zone_ok(u8"UTC", u8"%Z"));
    EXPECT_FALSE(off_ok(u8"UTC", u8"%Z"));
    EXPECT_TRUE(zone_ok(u8"PDT", u8"%Z"));
    EXPECT_FALSE(off_ok(u8"PDT", u8"%Z"));

    // A run of letters the database does not know is rejected at both, for different reasons:
    // no trie entry at one tier, no literal match at the other.
    EXPECT_FALSE(zone_ok(u8"XYZ", u8"%Z"));
    EXPECT_FALSE(off_ok(u8"XYZ", u8"%Z"));

    // The literal is for *this* specifier, not for any percent sequence.
    EXPECT_FALSE(off_ok(u8"%z", u8"%Z"));
    EXPECT_FALSE(off_ok(u8"%Q", u8"%Z"));

    // The round trip it exists for: a std::tm with no zone, through a format carrying %Z. Each
    // platform reads it back at the tier its own std::tm sits at. With tm_zone the field exists
    // but names nothing, so put writes the unknown-zone token and the zone tier reads it back;
    // without the extension members the type has no zone at all, put degrades %Z to a literal,
    // and the tiers below zone match that literal. Either way it closes.
    {
        std::tm t{};
        t.tm_year = 124; t.tm_mon = 8; t.tm_mday = 4;
        t.tm_hour = 13; t.tm_min = 33; t.tm_sec = 18;

        std::u8string res;
        obj.put(std::back_inserter(res), t, std::u8string_view(u8"%F %T %Z"));
#ifdef __USE_MISC
        EXPECT_EQ(res, u8"2024-09-04 13:33:18 UNKNOWN");
        EXPECT_TRUE(zone_ok(res, u8"%F %T %Z"));
#else
        EXPECT_EQ(res, u8"2024-09-04 13:33:18 %Z");
        EXPECT_TRUE(off_ok(res, u8"%F %T %Z"));
#endif
    }

    // The same round trip through a locale whose own %c carries %Z, which is how this reaches
    // a caller who never wrote %Z: put_time(&t, "%c") on a tm that get_time filled in.
    {
        timeio us(std::make_shared<timeio_conf<char8_t>>("en_US.UTF-8"));
        std::tm t{};
        t.tm_year = 124; t.tm_mon = 8; t.tm_mday = 4;
        t.tm_hour = 13; t.tm_min = 33; t.tm_sec = 18;

        std::u8string res;
        us.put(std::back_inserter(res), t, std::u8string_view(u8"%c"));

        time_parse_context<char8_t, true, true, tz_level::zone> ctx;
        EXPECT_EQ(us.get(res.begin(), res.end(), ctx, std::u8string_view(u8"%c")), res.end());
    }
}
