// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same conversion-specifier contract as test_timeio_char.cpp for wchar_t.  The
 * tables are the same tables: what a specifier produces depends on the locale
 * and on what the value can supply, not on the type the field is written in,
 * and these cases are here to say that the instantiation changes none of it.
 *
 * The cases that read the C library's words, the locale database or the zone
 * trie see the same data whatever the character type is, so they stay in the
 * narrow file.
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

    timeio<wchar_t> facet_for(const char* loc)
    {
        return timeio<wchar_t>(std::make_shared<timeio_conf<wchar_t>>(loc));
    }

    template <typename TVal, typename... TSpec>
    std::wstring put_one(const timeio<wchar_t>& obj, const TVal& tp, TSpec... spec)
    {
        std::wstring res;
        obj.put(std::back_inserter(res), tp, spec...);
        return res;
    }

    // One conversion specifier, the modifier applied to it, and what the facet
    // writes.  A specifier the value cannot supply comes back as the format text
    // that asked for it, which is why so many rows read "%Ea" and the like.
    struct conversion
    {
        wchar_t     spec;
        wchar_t     mod;
        const wchar_t* expected;
    };

    template <typename TVal, std::size_t N>
    void expect_conversions(const timeio<wchar_t>& obj, const TVal& tp, const conversion (&table)[N])
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
    template <typename T = time_parse_context<wchar_t>, bool HaveDate = true, bool HaveTime = true,
              tz_level TzLevel = tz_level::zone, typename... TFmt>
    T run_get(const timeio<wchar_t>& obj, const std::wstring& input,
              ios_defs::iostate err_exp, TFmt... fmt)
    {
        time_parse_context<wchar_t, HaveDate, HaveTime, TzLevel> ctx1, ctx2, ctx3;
        std::list<wchar_t> lst(input.begin(), input.end());
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
    template <typename T = time_parse_context<wchar_t>, bool HaveDate = true, bool HaveTime = true,
              tz_level TzLevel = tz_level::zone>
    T CheckGet(const timeio<wchar_t>& obj, const std::wstring& input, char fmt, char modif,
               ios_defs::iostate err_exp)
    {
        SCOPED_TRACE(::testing::PrintToString(input) + " | %"
                     + (modif ? std::string(1, static_cast<char>(modif)) : std::string())
                     + static_cast<char>(fmt));
        return run_get<T, HaveDate, HaveTime, TzLevel>(obj, input, err_exp, fmt, modif);
    }

    template <typename T = time_parse_context<wchar_t>, bool HaveDate = true, bool HaveTime = true,
              tz_level TzLevel = tz_level::zone>
    T CheckGet(const timeio<wchar_t>& obj, const std::wstring& input, const std::wstring& fmt,
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
TEST(TimeioWchar, CLocaleWritesEveryConversionSpecifier)
{
    const timeio<wchar_t> obj = facet_for("C");
    const auto            tp  = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    static const conversion kConversions[] = {
        {L'%', 0, L"%"},
        {L'a', 0, L"Wed"},
        {L'a', L'E', L"%Ea"},
        {L'a', L'O', L"%Oa"},
        {L'A', 0, L"Wednesday"},
        {L'A', L'E', L"%EA"},
        {L'A', L'O', L"%OA"},
        {L'b', 0, L"Sep"},
        {L'b', L'E', L"%Eb"},
        {L'b', L'O', L"%Ob"},
        {L'h', 0, L"Sep"},
        {L'h', L'E', L"%Eh"},
        {L'h', L'O', L"%Oh"},
        {L'B', 0, L"September"},
        {L'B', L'E', L"%EB"},
        {L'B', L'O', L"%OB"},
        {L'c', 0, L"Wed Sep  4 13:33:18 2024"},
        {L'c', L'E', L"Wed Sep  4 13:33:18 2024"},
        {L'c', L'O', L"%Oc"},
        {L'C', 0, L"20"},
        {L'C', L'E', L"20"},
        {L'C', L'O', L"%OC"},
        {L'x', 0, L"09/04/24"},
        {L'x', L'E', L"09/04/24"},
        {L'x', L'O', L"%Ox"},
        {L'D', 0, L"09/04/24"},
        {L'D', L'E', L"%ED"},
        {L'D', L'O', L"%OD"},
        {L'd', 0, L"04"},
        {L'd', L'E', L"%Ed"},
        {L'd', L'O', L"04"},
        {L'e', 0, L" 4"},
        {L'e', L'E', L"%Ee"},
        {L'e', L'O', L" 4"},
        {L'F', 0, L"2024-09-04"},
        {L'F', L'E', L"%EF"},
        {L'F', L'O', L"%OF"},
        {L'H', 0, L"13"},
        {L'H', L'E', L"%EH"},
        {L'H', L'O', L"13"},
        {L'I', 0, L"01"},
        {L'I', L'E', L"%EI"},
        {L'I', L'O', L"01"},
        {L'j', 0, L"248"},
        {L'j', L'E', L"%Ej"},
        {L'j', L'O', L"%Oj"},
        {L'M', 0, L"33"},
        {L'M', L'E', L"%EM"},
        {L'M', L'O', L"33"},
        {L'm', 0, L"09"},
        {L'm', L'E', L"%Em"},
        {L'm', L'O', L"09"},
        {L'n', 0, L"\n"},
        {L'n', L'E', L"%En"},
        {L'n', L'O', L"%On"},
        {L'p', 0, L"PM"},
        {L'p', L'E', L"%Ep"},
        {L'p', L'O', L"%Op"},
        {L'R', 0, L"13:33"},
        {L'R', L'E', L"%ER"},
        {L'R', L'O', L"%OR"},
        {L'r', 0, L"01:33:18 PM"},
        {L'r', L'E', L"%Er"},
        {L'r', L'O', L"%Or"},
        {L'S', 0, L"18"},
        {L'S', L'E', L"%ES"},
        {L'S', L'O', L"18"},
        {L'X', 0, L"13:33:18"},
        {L'X', L'E', L"13:33:18"},
        {L'X', L'O', L"%OX"},
        {L'T', 0, L"13:33:18"},
        {L'T', L'E', L"%ET"},
        {L'T', L'O', L"%OT"},
        {L't', 0, L"\t"},
        {L't', L'E', L"%Et"},
        {L't', L'O', L"%Ot"},
        {L'u', 0, L"3"},
        {L'u', L'E', L"%Eu"},
        {L'u', L'O', L"3"},
        {L'U', 0, L"35"},
        {L'U', L'E', L"%EU"},
        {L'U', L'O', L"35"},
        {L'V', 0, L"36"},
        {L'V', L'E', L"%EV"},
        {L'V', L'O', L"36"},
        {L'g', 0, L"24"},
        {L'g', L'E', L"%Eg"},
        {L'g', L'O', L"%Og"},
        {L'G', 0, L"2024"},
        {L'G', L'E', L"%EG"},
        {L'G', L'O', L"%OG"},
        {L'W', 0, L"36"},
        {L'W', L'E', L"%EW"},
        {L'W', L'O', L"36"},
        {L'w', 0, L"3"},
        {L'w', L'E', L"%Ew"},
        {L'w', L'O', L"3"},
        {L'Y', 0, L"2024"},
        {L'Y', L'E', L"2024"},
        {L'Y', L'O', L"%OY"},
        {L'y', 0, L"24"},
        {L'y', L'E', L"24"},
        {L'y', L'O', L"24"},
        {L'Z', 0, L"America/Los_Angeles"},
        {L'Z', L'E', L"%EZ"},
        {L'Z', L'O', L"%OZ"},
        {L'z', 0, L"-0700"},
        {L'z', L'E', L"%Ez"},
        {L'z', L'O', L"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// The same instant and the same specifiers under zh_CN, where the words and the
// composite layouts differ but the rules about what a value can supply do not.
TEST(TimeioWchar, ChineseWritesEveryConversionSpecifier)
{
    const timeio<wchar_t> obj = facet_for("zh_CN.UTF-8");
    const auto            tp  = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    static const conversion kConversions[] = {
        {L'%', 0, L"%"},
        {L'a', 0, L"三"},
        {L'a', L'E', L"%Ea"},
        {L'a', L'O', L"%Oa"},
        {L'A', 0, L"星期三"},
        {L'A', L'E', L"%EA"},
        {L'A', L'O', L"%OA"},
        {L'b', 0, L"9月"},
        {L'b', L'E', L"%Eb"},
        {L'b', L'O', L"%Ob"},
        {L'h', 0, L"9月"},
        {L'h', L'E', L"%Eh"},
        {L'h', L'O', L"%Oh"},
        {L'B', 0, L"九月"},
        {L'B', L'E', L"%EB"},
        {L'B', L'O', L"%OB"},
        {L'c', 0, L"2024年09月04日 星期三 13时33分18秒"},
        {L'c', L'E', L"2024年09月04日 星期三 13时33分18秒"},
        {L'c', L'O', L"%Oc"},
        {L'C', 0, L"20"},
        {L'C', L'E', L"20"},
        {L'C', L'O', L"%OC"},
        {L'x', 0, L"2024年09月04日"},
        {L'x', L'E', L"2024年09月04日"},
        {L'x', L'O', L"%Ox"},
        {L'D', 0, L"09/04/24"},
        {L'D', L'E', L"%ED"},
        {L'D', L'O', L"%OD"},
        {L'd', 0, L"04"},
        {L'd', L'E', L"%Ed"},
        {L'd', L'O', L"04"},
        {L'e', 0, L" 4"},
        {L'e', L'E', L"%Ee"},
        {L'e', L'O', L" 4"},
        {L'F', 0, L"2024-09-04"},
        {L'F', L'E', L"%EF"},
        {L'F', L'O', L"%OF"},
        {L'H', 0, L"13"},
        {L'H', L'E', L"%EH"},
        {L'H', L'O', L"13"},
        {L'I', 0, L"01"},
        {L'I', L'E', L"%EI"},
        {L'I', L'O', L"01"},
        {L'j', 0, L"248"},
        {L'j', L'E', L"%Ej"},
        {L'j', L'O', L"%Oj"},
        {L'M', 0, L"33"},
        {L'M', L'E', L"%EM"},
        {L'M', L'O', L"33"},
        {L'm', 0, L"09"},
        {L'm', L'E', L"%Em"},
        {L'm', L'O', L"09"},
        {L'n', 0, L"\n"},
        {L'n', L'E', L"%En"},
        {L'n', L'O', L"%On"},
        {L'p', 0, L"下午"},
        {L'p', L'E', L"%Ep"},
        {L'p', L'O', L"%Op"},
        {L'R', 0, L"13:33"},
        {L'R', L'E', L"%ER"},
        {L'R', L'O', L"%OR"},
        {L'r', 0, L"下午 01时33分18秒"},
        {L'r', L'E', L"%Er"},
        {L'r', L'O', L"%Or"},
        {L'S', 0, L"18"},
        {L'S', L'E', L"%ES"},
        {L'S', L'O', L"18"},
        {L'X', 0, L"13时33分18秒"},
        {L'X', L'E', L"13时33分18秒"},
        {L'X', L'O', L"%OX"},
        {L'T', 0, L"13:33:18"},
        {L'T', L'E', L"%ET"},
        {L'T', L'O', L"%OT"},
        {L't', 0, L"\t"},
        {L't', L'E', L"%Et"},
        {L't', L'O', L"%Ot"},
        {L'u', 0, L"3"},
        {L'u', L'E', L"%Eu"},
        {L'u', L'O', L"3"},
        {L'U', 0, L"35"},
        {L'U', L'E', L"%EU"},
        {L'U', L'O', L"35"},
        {L'V', 0, L"36"},
        {L'V', L'E', L"%EV"},
        {L'V', L'O', L"36"},
        {L'g', 0, L"24"},
        {L'g', L'E', L"%Eg"},
        {L'g', L'O', L"%Og"},
        {L'G', 0, L"2024"},
        {L'G', L'E', L"%EG"},
        {L'G', L'O', L"%OG"},
        {L'W', 0, L"36"},
        {L'W', L'E', L"%EW"},
        {L'W', L'O', L"36"},
        {L'w', 0, L"3"},
        {L'w', L'E', L"%Ew"},
        {L'w', L'O', L"3"},
        {L'Y', 0, L"2024"},
        {L'Y', L'E', L"2024"},
        {L'Y', L'O', L"%OY"},
        {L'y', 0, L"24"},
        {L'y', L'E', L"24"},
        {L'y', L'O', L"24"},
        {L'Z', 0, L"America/Los_Angeles"},
        {L'Z', L'E', L"%EZ"},
        {L'Z', L'O', L"%OZ"},
        {L'z', 0, L"-0700"},
        {L'z', L'E', L"%Ez"},
        {L'z', L'O', L"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// And under ja_JP, which is the locale with an era representation, so %EC, %Ey
// and %EY are the rows to look at here.
TEST(TimeioWchar, JapaneseWritesEveryConversionSpecifier)
{
    const timeio<wchar_t> obj = facet_for("ja_JP.UTF-8");
    const auto            tp  = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    static const conversion kConversions[] = {
        {L'%', 0, L"%"},
        {L'a', 0, L"水"},
        {L'a', L'E', L"%Ea"},
        {L'a', L'O', L"%Oa"},
        {L'A', 0, L"水曜日"},
        {L'A', L'E', L"%EA"},
        {L'A', L'O', L"%OA"},
        {L'b', 0, L" 9月"},
        {L'b', L'E', L"%Eb"},
        {L'b', L'O', L"%Ob"},
        {L'h', 0, L" 9月"},
        {L'h', L'E', L"%Eh"},
        {L'h', L'O', L"%Oh"},
        {L'B', 0, L"9月"},
        {L'B', L'E', L"%EB"},
        {L'B', L'O', L"%OB"},
        {L'c', 0, L"2024年09月04日 13時33分18秒"},
        {L'c', L'E', L"令和6年09月04日 13時33分18秒"},
        {L'c', L'O', L"%Oc"},
        {L'C', 0, L"20"},
        {L'C', L'E', L"令和"},
        {L'C', L'O', L"%OC"},
        {L'x', 0, L"2024年09月04日"},
        {L'x', L'E', L"令和6年09月04日"},
        {L'x', L'O', L"%Ox"},
        {L'D', 0, L"09/04/24"},
        {L'D', L'E', L"%ED"},
        {L'D', L'O', L"%OD"},
        {L'd', 0, L"04"},
        {L'd', L'E', L"%Ed"},
        {L'd', L'O', L"四"},
        {L'e', 0, L" 4"},
        {L'e', L'E', L"%Ee"},
        {L'e', L'O', L"四"},
        {L'F', 0, L"2024-09-04"},
        {L'F', L'E', L"%EF"},
        {L'F', L'O', L"%OF"},
        {L'H', 0, L"13"},
        {L'H', L'E', L"%EH"},
        {L'H', L'O', L"十三"},
        {L'I', 0, L"01"},
        {L'I', L'E', L"%EI"},
        {L'I', L'O', L"一"},
        {L'j', 0, L"248"},
        {L'j', L'E', L"%Ej"},
        {L'j', L'O', L"%Oj"},
        {L'M', 0, L"33"},
        {L'M', L'E', L"%EM"},
        {L'M', L'O', L"三十三"},
        {L'm', 0, L"09"},
        {L'm', L'E', L"%Em"},
        {L'm', L'O', L"九"},
        {L'n', 0, L"\n"},
        {L'n', L'E', L"%En"},
        {L'n', L'O', L"%On"},
        {L'p', 0, L"午後"},
        {L'p', L'E', L"%Ep"},
        {L'p', L'O', L"%Op"},
        {L'R', 0, L"13:33"},
        {L'R', L'E', L"%ER"},
        {L'R', L'O', L"%OR"},
        {L'r', 0, L"午後01時33分18秒"},
        {L'r', L'E', L"%Er"},
        {L'r', L'O', L"%Or"},
        {L'S', 0, L"18"},
        {L'S', L'E', L"%ES"},
        {L'S', L'O', L"十八"},
        {L'X', 0, L"13時33分18秒"},
        {L'X', L'E', L"13時33分18秒"},
        {L'X', L'O', L"%OX"},
        {L'T', 0, L"13:33:18"},
        {L'T', L'E', L"%ET"},
        {L'T', L'O', L"%OT"},
        {L't', 0, L"\t"},
        {L't', L'E', L"%Et"},
        {L't', L'O', L"%Ot"},
        {L'u', 0, L"3"},
        {L'u', L'E', L"%Eu"},
        {L'u', L'O', L"三"},
        {L'U', 0, L"35"},
        {L'U', L'E', L"%EU"},
        {L'U', L'O', L"三十五"},
        {L'V', 0, L"36"},
        {L'V', L'E', L"%EV"},
        {L'V', L'O', L"三十六"},
        {L'g', 0, L"24"},
        {L'g', L'E', L"%Eg"},
        {L'g', L'O', L"%Og"},
        {L'G', 0, L"2024"},
        {L'G', L'E', L"%EG"},
        {L'G', L'O', L"%OG"},
        {L'W', 0, L"36"},
        {L'W', L'E', L"%EW"},
        {L'W', L'O', L"三十六"},
        {L'w', 0, L"3"},
        {L'w', L'E', L"%Ew"},
        {L'w', L'O', L"三"},
        {L'Y', 0, L"2024"},
        {L'Y', L'E', L"令和6年"},
        {L'Y', L'O', L"%OY"},
        {L'y', 0, L"24"},
        {L'y', L'E', L"6"},
        {L'y', L'O', L"二十四"},
        {L'Z', 0, L"America/Los_Angeles"},
        {L'Z', L'E', L"%EZ"},
        {L'Z', L'O', L"%OZ"},
        {L'z', 0, L"-0700"},
        {L'z', L'E', L"%Ez"},
        {L'z', L'O', L"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// A year_month_day is a date and nothing else, so every specifier that asks for a
// time of day or a zone comes back as the text that asked for it.
TEST(TimeioWchar, ADateWritesEveryConversionSpecifierItCanSupply)
{
    using namespace std::chrono;
    const timeio<wchar_t>   obj = facet_for("ja_JP.UTF-8");
    const year_month_day tp{year{2024}, month{9}, day{4}};

    static const conversion kConversions[] = {
        {L'%', 0, L"%"},
        {L'a', 0, L"水"},
        {L'a', L'E', L"%Ea"},
        {L'a', L'O', L"%Oa"},
        {L'A', 0, L"水曜日"},
        {L'A', L'E', L"%EA"},
        {L'A', L'O', L"%OA"},
        {L'b', 0, L" 9月"},
        {L'b', L'E', L"%Eb"},
        {L'b', L'O', L"%Ob"},
        {L'h', 0, L" 9月"},
        {L'h', L'E', L"%Eh"},
        {L'h', L'O', L"%Oh"},
        {L'B', 0, L"9月"},
        {L'B', L'E', L"%EB"},
        {L'B', L'O', L"%OB"},
        {L'c', 0, L"%c"},
        {L'c', L'E', L"%Ec"},
        {L'c', L'O', L"%Oc"},
        {L'C', 0, L"20"},
        {L'C', L'E', L"令和"},
        {L'C', L'O', L"%OC"},
        {L'x', 0, L"2024年09月04日"},
        {L'x', L'E', L"令和6年09月04日"},
        {L'x', L'O', L"%Ox"},
        {L'D', 0, L"09/04/24"},
        {L'D', L'E', L"%ED"},
        {L'D', L'O', L"%OD"},
        {L'd', 0, L"04"},
        {L'd', L'E', L"%Ed"},
        {L'd', L'O', L"四"},
        {L'e', 0, L" 4"},
        {L'e', L'E', L"%Ee"},
        {L'e', L'O', L"四"},
        {L'F', 0, L"2024-09-04"},
        {L'F', L'E', L"%EF"},
        {L'F', L'O', L"%OF"},
        {L'H', 0, L"%H"},
        {L'H', L'E', L"%EH"},
        {L'H', L'O', L"%OH"},
        {L'I', 0, L"%I"},
        {L'I', L'E', L"%EI"},
        {L'I', L'O', L"%OI"},
        {L'j', 0, L"248"},
        {L'j', L'E', L"%Ej"},
        {L'j', L'O', L"%Oj"},
        {L'M', 0, L"%M"},
        {L'M', L'E', L"%EM"},
        {L'M', L'O', L"%OM"},
        {L'm', 0, L"09"},
        {L'm', L'E', L"%Em"},
        {L'm', L'O', L"九"},
        {L'n', 0, L"\n"},
        {L'n', L'E', L"%En"},
        {L'n', L'O', L"%On"},
        {L'p', 0, L"%p"},
        {L'p', L'E', L"%Ep"},
        {L'p', L'O', L"%Op"},
        {L'R', 0, L"%R"},
        {L'R', L'E', L"%ER"},
        {L'R', L'O', L"%OR"},
        {L'r', 0, L"%r"},
        {L'r', L'E', L"%Er"},
        {L'r', L'O', L"%Or"},
        {L'S', 0, L"%S"},
        {L'S', L'E', L"%ES"},
        {L'S', L'O', L"%OS"},
        {L'X', 0, L"%X"},
        {L'X', L'E', L"%EX"},
        {L'X', L'O', L"%OX"},
        {L'T', 0, L"%T"},
        {L'T', L'E', L"%ET"},
        {L'T', L'O', L"%OT"},
        {L't', 0, L"\t"},
        {L't', L'E', L"%Et"},
        {L't', L'O', L"%Ot"},
        {L'u', 0, L"3"},
        {L'u', L'E', L"%Eu"},
        {L'u', L'O', L"三"},
        {L'U', 0, L"35"},
        {L'U', L'E', L"%EU"},
        {L'U', L'O', L"三十五"},
        {L'V', 0, L"36"},
        {L'V', L'E', L"%EV"},
        {L'V', L'O', L"三十六"},
        {L'g', 0, L"24"},
        {L'g', L'E', L"%Eg"},
        {L'g', L'O', L"%Og"},
        {L'G', 0, L"2024"},
        {L'G', L'E', L"%EG"},
        {L'G', L'O', L"%OG"},
        {L'W', 0, L"36"},
        {L'W', L'E', L"%EW"},
        {L'W', L'O', L"三十六"},
        {L'w', 0, L"3"},
        {L'w', L'E', L"%Ew"},
        {L'w', L'O', L"三"},
        {L'Y', 0, L"2024"},
        {L'Y', L'E', L"令和6年"},
        {L'Y', L'O', L"%OY"},
        {L'y', 0, L"24"},
        {L'y', L'E', L"6"},
        {L'y', L'O', L"二十四"},
        {L'Z', 0, L"%Z"},
        {L'Z', L'E', L"%EZ"},
        {L'Z', L'O', L"%OZ"},
        {L'z', 0, L"%z"},
        {L'z', L'E', L"%Ez"},
        {L'z', L'O', L"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// An hh_mm_ss is the mirror image: it has a time of day and no date at all.
TEST(TimeioWchar, ATimeOfDayWritesEveryConversionSpecifierItCanSupply)
{
    using namespace std::chrono;
    const timeio<wchar_t>    obj = facet_for("ja_JP.UTF-8");
    const hh_mm_ss<seconds> tp{hours{13} + minutes{33} + seconds{18}};

    static const conversion kConversions[] = {
        {L'%', 0, L"%"},
        {L'a', 0, L"%a"},
        {L'a', L'E', L"%Ea"},
        {L'a', L'O', L"%Oa"},
        {L'A', 0, L"%A"},
        {L'A', L'E', L"%EA"},
        {L'A', L'O', L"%OA"},
        {L'b', 0, L"%b"},
        {L'b', L'E', L"%Eb"},
        {L'b', L'O', L"%Ob"},
        {L'h', 0, L"%h"},
        {L'h', L'E', L"%Eh"},
        {L'h', L'O', L"%Oh"},
        {L'B', 0, L"%B"},
        {L'B', L'E', L"%EB"},
        {L'B', L'O', L"%OB"},
        {L'c', 0, L"%c"},
        {L'c', L'E', L"%Ec"},
        {L'c', L'O', L"%Oc"},
        {L'x', 0, L"%x"},
        {L'x', L'E', L"%Ex"},
        {L'x', L'O', L"%Ox"},
        {L'D', 0, L"%D"},
        {L'D', L'E', L"%ED"},
        {L'D', L'O', L"%OD"},
        {L'd', 0, L"%d"},
        {L'd', L'E', L"%Ed"},
        {L'd', L'O', L"%Od"},
        {L'e', 0, L"%e"},
        {L'e', L'E', L"%Ee"},
        {L'e', L'O', L"%Oe"},
        {L'F', 0, L"%F"},
        {L'F', L'E', L"%EF"},
        {L'F', L'O', L"%OF"},
        {L'H', 0, L"13"},
        {L'H', L'E', L"%EH"},
        {L'H', L'O', L"十三"},
        {L'I', 0, L"01"},
        {L'I', L'E', L"%EI"},
        {L'I', L'O', L"一"},
        {L'j', 0, L"%j"},
        {L'j', L'E', L"%Ej"},
        {L'j', L'O', L"%Oj"},
        {L'M', 0, L"33"},
        {L'M', L'E', L"%EM"},
        {L'M', L'O', L"三十三"},
        {L'm', 0, L"%m"},
        {L'm', L'E', L"%Em"},
        {L'm', L'O', L"%Om"},
        {L'n', 0, L"\n"},
        {L'n', L'E', L"%En"},
        {L'n', L'O', L"%On"},
        {L'p', 0, L"午後"},
        {L'p', L'E', L"%Ep"},
        {L'p', L'O', L"%Op"},
        {L'R', 0, L"13:33"},
        {L'R', L'E', L"%ER"},
        {L'R', L'O', L"%OR"},
        {L'r', 0, L"午後01時33分18秒"},
        {L'r', L'E', L"%Er"},
        {L'r', L'O', L"%Or"},
        {L'S', 0, L"18"},
        {L'S', L'E', L"%ES"},
        {L'S', L'O', L"十八"},
        {L'X', 0, L"13時33分18秒"},
        {L'X', L'E', L"13時33分18秒"},
        {L'X', L'O', L"%OX"},
        {L'T', 0, L"13:33:18"},
        {L'T', L'E', L"%ET"},
        {L'T', L'O', L"%OT"},
        {L't', 0, L"\t"},
        {L't', L'E', L"%Et"},
        {L't', L'O', L"%Ot"},
        {L'u', 0, L"%u"},
        {L'u', L'E', L"%Eu"},
        {L'u', L'O', L"%Ou"},
        {L'U', 0, L"%U"},
        {L'U', L'E', L"%EU"},
        {L'U', L'O', L"%OU"},
        {L'V', 0, L"%V"},
        {L'V', L'E', L"%EV"},
        {L'V', L'O', L"%OV"},
        {L'g', 0, L"%g"},
        {L'g', L'E', L"%Eg"},
        {L'g', L'O', L"%Og"},
        {L'G', 0, L"%G"},
        {L'G', L'E', L"%EG"},
        {L'G', L'O', L"%OG"},
        {L'W', 0, L"%W"},
        {L'W', L'E', L"%EW"},
        {L'W', L'O', L"%OW"},
        {L'w', 0, L"%w"},
        {L'w', L'E', L"%Ew"},
        {L'w', L'O', L"%Ow"},
        {L'Y', 0, L"%Y"},
        {L'Y', L'E', L"%EY"},
        {L'Y', L'O', L"%OY"},
        {L'y', 0, L"%y"},
        {L'y', L'E', L"%Ey"},
        {L'y', L'O', L"%Oy"},
        {L'Z', 0, L"%Z"},
        {L'Z', L'E', L"%EZ"},
        {L'Z', L'O', L"%OZ"},
        {L'z', 0, L"%z"},
        {L'z', L'E', L"%Ez"},
        {L'z', L'O', L"%Oz"},
    };

    expect_conversions(obj, tp, kConversions);
}

// A std::tm carries both halves, so almost everything is available; what it does
// not carry is a zone, which the cases after this one take up.
TEST(TimeioWchar, ABrokenDownTimeWritesEveryConversionSpecifier)
{
    const timeio<wchar_t> obj = facet_for("ja_JP.UTF-8");
    const std::tm         tp  = calendar_time(2024 - 1900, 9 - 1, 4, 13, 33, 18, 0, 0, 0);

    static const conversion kConversions[] = {
        {L'%', 0, L"%"},
        {L'a', 0, L"水"},
        {L'a', L'E', L"%Ea"},
        {L'a', L'O', L"%Oa"},
        {L'A', 0, L"水曜日"},
        {L'A', L'E', L"%EA"},
        {L'A', L'O', L"%OA"},
        {L'b', 0, L" 9月"},
        {L'b', L'E', L"%Eb"},
        {L'b', L'O', L"%Ob"},
        {L'h', 0, L" 9月"},
        {L'h', L'E', L"%Eh"},
        {L'h', L'O', L"%Oh"},
        {L'B', 0, L"9月"},
        {L'B', L'E', L"%EB"},
        {L'B', L'O', L"%OB"},
        {L'c', 0, L"2024年09月04日 13時33分18秒"},
        {L'c', L'E', L"令和6年09月04日 13時33分18秒"},
        {L'c', L'O', L"%Oc"},
        {L'C', 0, L"20"},
        {L'C', L'E', L"令和"},
        {L'C', L'O', L"%OC"},
        {L'x', 0, L"2024年09月04日"},
        {L'x', L'E', L"令和6年09月04日"},
        {L'x', L'O', L"%Ox"},
        {L'D', 0, L"09/04/24"},
        {L'D', L'E', L"%ED"},
        {L'D', L'O', L"%OD"},
        {L'd', 0, L"04"},
        {L'd', L'E', L"%Ed"},
        {L'd', L'O', L"四"},
        {L'e', 0, L" 4"},
        {L'e', L'E', L"%Ee"},
        {L'e', L'O', L"四"},
        {L'F', 0, L"2024-09-04"},
        {L'F', L'E', L"%EF"},
        {L'F', L'O', L"%OF"},
        {L'H', 0, L"13"},
        {L'H', L'E', L"%EH"},
        {L'H', L'O', L"十三"},
        {L'I', 0, L"01"},
        {L'I', L'E', L"%EI"},
        {L'I', L'O', L"一"},
        {L'j', 0, L"248"},
        {L'j', L'E', L"%Ej"},
        {L'j', L'O', L"%Oj"},
        {L'M', 0, L"33"},
        {L'M', L'E', L"%EM"},
        {L'M', L'O', L"三十三"},
        {L'm', 0, L"09"},
        {L'm', L'E', L"%Em"},
        {L'm', L'O', L"九"},
        {L'n', 0, L"\n"},
        {L'n', L'E', L"%En"},
        {L'n', L'O', L"%On"},
        {L'p', 0, L"午後"},
        {L'p', L'E', L"%Ep"},
        {L'p', L'O', L"%Op"},
        {L'R', 0, L"13:33"},
        {L'R', L'E', L"%ER"},
        {L'R', L'O', L"%OR"},
        {L'r', 0, L"午後01時33分18秒"},
        {L'r', L'E', L"%Er"},
        {L'r', L'O', L"%Or"},
        {L'S', 0, L"18"},
        {L'S', L'E', L"%ES"},
        {L'S', L'O', L"十八"},
        {L'X', 0, L"13時33分18秒"},
        {L'X', L'E', L"13時33分18秒"},
        {L'X', L'O', L"%OX"},
        {L'T', 0, L"13:33:18"},
        {L'T', L'E', L"%ET"},
        {L'T', L'O', L"%OT"},
        {L't', 0, L"\t"},
        {L't', L'E', L"%Et"},
        {L't', L'O', L"%Ot"},
        {L'u', 0, L"3"},
        {L'u', L'E', L"%Eu"},
        {L'u', L'O', L"三"},
        {L'U', 0, L"35"},
        {L'U', L'E', L"%EU"},
        {L'U', L'O', L"三十五"},
        {L'V', 0, L"36"},
        {L'V', L'E', L"%EV"},
        {L'V', L'O', L"三十六"},
        {L'g', 0, L"24"},
        {L'g', L'E', L"%Eg"},
        {L'g', L'O', L"%Og"},
        {L'G', 0, L"2024"},
        {L'G', L'E', L"%EG"},
        {L'G', L'O', L"%OG"},
        {L'W', 0, L"36"},
        {L'W', L'E', L"%EW"},
        {L'W', L'O', L"三十六"},
        {L'w', 0, L"3"},
        {L'w', L'E', L"%Ew"},
        {L'w', L'O', L"三"},
        {L'Y', 0, L"2024"},
        {L'Y', L'E', L"令和6年"},
        {L'Y', L'O', L"%OY"},
        {L'y', 0, L"24"},
        {L'y', L'E', L"6"},
        {L'y', L'O', L"二十四"},
        {L'Z', 0, L"UNKNOWN"},
        {L'Z', L'E', L"%EZ"},
        {L'Z', L'O', L"%OZ"},
        {L'z', 0, L"+0000"},
        {L'z', L'E', L"%Ez"},
        {L'z', L'O', L"%Oz"},
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
TEST(TimeioWchar, ABrokenDownTimeNamesItsZoneOrSaysItCannot)
{
    const timeio<wchar_t> obj = facet_for("ja_JP.UTF-8");
    const std::tm         tp  = calendar_time(2024 - 1900, 9 - 1, 4, 13, 33, 18, 0, 0, 0);

    EXPECT_EQ(put_one(obj, tp, L'Z'), L"UNKNOWN");
    EXPECT_EQ(put_one(obj, tp, L'z'), L"+0000");

#ifdef __USE_MISC
    std::tm named = tp;
    named.tm_zone = "PST";
    EXPECT_EQ(put_one(obj, named, L'Z'), L"PST");

    // An empty string is as nameless as a null pointer.
    named.tm_zone = "";
    EXPECT_EQ(put_one(obj, named, L'Z'), L"UNKNOWN");
#endif
}

// A format string is expanded one specifier at a time with the literal text
// between them passed through unchanged, so what it produces is exactly the
// concatenation of its pieces.  Stated that way the case needs no locale's words
// written down, and holds in every locale rather than in the one it was written
// for.
TEST(TimeioWchar, AFormatStringIsExpandedSpecifierBySpecifier)
{
    const auto tp = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    for (const char* loc : {"C", "de_DE.UTF-8", "en_HK.UTF-8", "fr_FR.UTF-8", "ja_JP.UTF-8"})
    {
        SCOPED_TRACE(loc);
        const timeio<wchar_t> obj = facet_for(loc);

        EXPECT_EQ(put_one(obj, tp, std::wstring_view(L"%A, week %W of %B")),
                  put_one(obj, tp, L'A') + L", week " + put_one(obj, tp, L'W')
                                        + L" of " + put_one(obj, tp, L'B'));

        // Literal text alone, and a format that is nothing but literal text.
        EXPECT_EQ(put_one(obj, tp, std::wstring_view(L"[%Y]")), L"[" + put_one(obj, tp, L'Y') + L"]");
        EXPECT_EQ(put_one(obj, tp, std::wstring_view(L"no specifiers")), L"no specifiers");
    }
}

// put() writes through an iterator into whatever the caller supplied and returns
// where it stopped, so everything past that point has to be exactly as it was.
TEST(TimeioWchar, PutIntoAnExistingBufferReturnsWhereItStopped)
{
    const timeio<wchar_t> obj = facet_for("C");
    const auto            tp  = create_zoned_time(1997, 6, 26, 12, 0, 0, "America/Los_Angeles");

    std::wstring buffer(50, L'.');
    const auto   end = obj.put(buffer.begin(), tp, std::wstring_view(L"%F %T"));
    EXPECT_EQ(std::wstring(buffer.begin(), end), L"1997-06-26 12:00:00");
    EXPECT_EQ(buffer.substr(19), std::wstring(31, L'.'));

    // The same for a single specifier, whose length the caller cannot know in
    // advance because it is a word the locale chose.
    std::wstring one(20, L'.');
    const auto   one_end = obj.put(one.begin(), tp, L'A');
    EXPECT_EQ(std::wstring(one.begin(), one_end), L"Thursday");
    EXPECT_EQ(one.substr(8), std::wstring(12, L'.'));
}

// The literal text in a format is part of what has to match: it is how the
// caller says which of several numbers is which.  Input past what the format
// asked for is left for whoever reads next.
TEST(TimeioWchar, AFormatStringMustMatchTheInputLiterally)
{
    const timeio<wchar_t> obj = facet_for("C");

    const auto t = ctx_to<std::tm>(
        CheckGet(obj, L"on 2024-09-04 at 01:09:35", L"on %Y-%m-%d at %H:%M:%S", ios_defs::eofbit));
    EXPECT_EQ(t.tm_year, 124);
    EXPECT_EQ(t.tm_mon, 8);
    EXPECT_EQ(t.tm_mday, 4);
    EXPECT_EQ(t.tm_hour, 1);
    EXPECT_EQ(t.tm_min, 9);
    EXPECT_EQ(t.tm_sec, 35);

    // Literal text the input does not carry.
    CheckGet(obj, L"at 2024-09-04", L"on %Y-%m-%d", ios_defs::strfailbit);
    CheckGet(obj, L"2024-09-04", L"on %Y-%m-%d", ios_defs::strfailbit);

    // A '%' with nothing after it is not a specifier.
    CheckGet(obj, L"2024-09-04", L"%", ios_defs::strfailbit);

    // What the format did not ask for stays in the input.
    const auto rest = ctx_to<std::tm>(CheckGet(obj, L"2020  ", L"%Y", ios_defs::goodbit));
    EXPECT_EQ(rest.tm_year, 120);

    // A single specifier without a format string reads the same field.
    EXPECT_EQ(ctx_to<std::tm>(CheckGet(obj, L"2020", L'Y', 0, ios_defs::eofbit)).tm_year, 120);
}

// The words a locale writes are the words it reads.  Round-tripping through the
// facet's own output says that in every locale at once, without this file having
// to know how any of them spells a month.
TEST(TimeioWchar, TheNamesTheLocaleWritesAreTheNamesItReads)
{
    using namespace std::chrono;
    const year_month_day date{year{2014}, month{4}, day{14}};

    for (const char* loc : {"C", "de_DE.UTF-8", "es_ES.UTF-8", "fr_FR.UTF-8", "ja_JP.UTF-8"})
    {
        SCOPED_TRACE(loc);
        const timeio<wchar_t> obj = facet_for(loc);

        for (const wchar_t* fmt : {L"%A, %d. %B %Y", L"%a %d %b %Y", L"%A %j %Y"})
        {
            SCOPED_TRACE(::testing::PrintToString(fmt));
            const std::wstring written = put_one(obj, date, std::wstring_view(fmt));
            EXPECT_EQ(CheckGet<year_month_day>(obj, written, fmt, ios_defs::eofbit), date);
        }
    }
}

// The specifiers that carry only part of a date -- a week number and a weekday,
// a day of the year, a century and a two-digit year -- have to reassemble into
// the date they were written from.  Stated as a round trip it holds for every
// date rather than for a handful with hand-computed week numbers.
TEST(TimeioWchar, EveryDateReassemblesFromItsPartialSpecifiers)
{
    using namespace std::chrono;
    const timeio<wchar_t> obj = facet_for("C");

    const wchar_t* const formats[] = {
        L"%F", L"%Y-%m-%d", L"%d-%b-%Y", L"%C%y-%m-%d",
        L"%Y %U %w", L"%Y %W %w", L"%Y %W %a", L"%Y %U %A", L"%j %Y",
    };

    // 29 days apart, so the sweep lands on every weekday and crosses the turn of
    // each year, which is where the week-number rules disagree with each other.
    for (sys_days d = sys_days{2019y / January / 1}; d <= sys_days{2024y / December / 31};
         d += days{29})
    {
        const year_month_day date{d};
        for (const wchar_t* fmt : formats)
        {
            SCOPED_TRACE(::testing::PrintToString(fmt) + " | "
                         + ::testing::PrintToString(put_one(obj, date, std::wstring_view(L"%F"))));
            const std::wstring written = put_one(obj, date, std::wstring_view(fmt));
            EXPECT_EQ(CheckGet<year_month_day>(obj, written, fmt, ios_defs::eofbit), date);
        }
    }
}

// %I is a clock face: it cannot tell noon from midnight on its own, and %p is
// what supplies the half of the day it belongs to.  Either order.
TEST(TimeioWchar, TheTwelveHourClockNeedsItsMeridiem)
{
    using namespace std::chrono;
    const timeio<wchar_t> obj = facet_for("C");

    for (int hour = 0; hour < 24; ++hour)
    {
        SCOPED_TRACE(hour);
        const seconds          when = hours{hour} + minutes{5} + seconds{9};
        const hh_mm_ss<seconds> tp{when};

        for (const wchar_t* fmt : {L"%I:%M:%S %p", L"%p%I:%M:%S", L"%r", L"%T"})
        {
            SCOPED_TRACE(::testing::PrintToString(fmt));
            const std::wstring written = put_one(obj, tp, std::wstring_view(fmt));
            const auto         back =
                CheckGet<hh_mm_ss<seconds>, false, true, tz_level::none>(obj, written, fmt,
                                                                         ios_defs::eofbit);
            EXPECT_EQ(back.to_duration(), when);
        }
    }

    // Without the meridiem the same field reads as the morning hour, because that
    // is the half of the day a clock face means when nothing says otherwise.
    const auto morning = ctx_to<std::tm>(CheckGet(obj, L"07:05:09", L"%I:%M:%S", ios_defs::eofbit));
    EXPECT_EQ(morning.tm_hour, 7);
}

TEST(TimeioWchar, ACompositeFormatCanExpandPastAConventionalStackBuffer)
{
    std::shared_ptr<timeio_conf<wchar_t>> conf =
        std::make_shared<expanded_composite_conf<wchar_t>>();
    timeio obj(conf);
    auto   zt = create_zoned_time(2022, 11, 17, 21, 47, 26, "America/Los_Angeles");

    std::wstring actual;
    obj.put(std::back_inserter(actual), zt, L'c');

    std::wstring expected(140, L'q');
    expected += L"-2022-11-17-21:47:26-";
    expected.append(20, L'z');

    EXPECT_GT(actual.size(), 128u);
    EXPECT_EQ(actual, expected);
}

TEST(TimeioWchar, TheCLocaleReadsEveryConversionSpecifier)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};

    timeio obj(std::make_shared<timeio_conf<wchar_t>>("C"));
    CheckGet(obj, L"%",   L'%',  0,  ios_defs::eofbit);
    CheckGet(obj, L"x",   L'%',  0,  ios_defs::strfailbit);
    CheckGet(obj, L"%",   L'%', L'E', febit);
    CheckGet(obj, L"%E%", L'%', L'E', ios_defs::eofbit);
    CheckGet(obj, L"%",   L'%', L'O', febit);
    CheckGet(obj, L"%O%", L'%', L'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet(obj, L"Wed", L'a', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, L"%Ea", L'a', L'E', ios_defs::eofbit);
    CheckGet(obj, L"a",   L'a', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oa", L'a', L'O', ios_defs::eofbit);
    CheckGet(obj, L"a",   L'a', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"Wednesday", L'A', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, L"%EA", L'A', L'E', ios_defs::eofbit);
    CheckGet(obj, L"A",   L'A', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OA", L'A', L'O', ios_defs::eofbit);
    CheckGet(obj, L"A",   L'A', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"Sep", L'b', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, L"%Eb", L'b', L'E', ios_defs::eofbit);
    CheckGet(obj, L"b",   L'b', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Ob", L'b', L'O', ios_defs::eofbit);
    CheckGet(obj, L"b",   L'b', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"September", L'B', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, L"%EB", L'B', L'E', ios_defs::eofbit);
    CheckGet(obj, L"B",   L'B', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OB", L'B', L'O', ios_defs::eofbit);
    CheckGet(obj, L"B",   L'B', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"Sep", L'h', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, L"%Eh", L'h', L'E', ios_defs::eofbit);
    CheckGet(obj, L"h",   L'h', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oh", L'h', L'O', ios_defs::eofbit);
    CheckGet(obj, L"h",   L'h', L'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"Wed Sep  4 13:33:18 2024", L'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"Wed Sep  4 13:33:18 2024", L'c', L'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, L"c",   L'c', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oc", L'c', L'O', ios_defs::eofbit);
    CheckGet(obj, L"c",   L'c', L'O', ios_defs::strfailbit);


    EXPECT_EQ(CheckGet(obj, L"20", L'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(CheckGet(obj, L"20", L'C', L'E', ios_defs::eofbit).m_century, 20);
    CheckGet(obj, L"C",   L'C', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OC", L'C', L'O', ios_defs::eofbit);
    CheckGet(obj, L"C",   L'C', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"04", L'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, L"04", L'd', L'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, L"%Ed", L'd', L'E', ios_defs::eofbit);
    CheckGet(obj, L"d",   L'd', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"d",   L'd', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"4", L'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, L"4", L'e', L'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, L"%Ee", L'e', L'E', ios_defs::eofbit);
    CheckGet(obj, L"e",   L'e', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"e",   L'e', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024-09-04", L'F', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, L"%EF", L'F', L'E', ios_defs::eofbit);
    CheckGet(obj, L"F",   L'F', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OF", L'F', L'O', ios_defs::eofbit);
    CheckGet(obj, L"F",   L'F', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"09/04/24", L'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"09/04/24", L'x', L'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, L"x",   L'x', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Ox", L'x', L'O', ios_defs::eofbit);
    CheckGet(obj, L"x",   L'x', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"09/04/24", L'D', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, L"%ED", L'D', L'E', ios_defs::eofbit);
    CheckGet(obj, L"D",   L'D', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OD", L'D', L'O', ios_defs::eofbit);
    CheckGet(obj, L"D",   L'D', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"13", L'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, L"13", L'H', L'O', ios_defs::eofbit).m_hour, 13);
    CheckGet(obj, L"%EH", L'H', L'E', ios_defs::eofbit);
    CheckGet(obj, L"H",   L'H', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"H",   L'H', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"01", L'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, L"01", L'I', L'O', ios_defs::eofbit).m_hour, 1);
    CheckGet(obj, L"%EI", L'I', L'E', ios_defs::eofbit);
    CheckGet(obj, L"I",   L'I', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"I",   L'I', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"248", L'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 248", L"%Y %j", ios_defs::eofbit), check_date1);
    CheckGet(obj, L"%Ej", L'j', L'E', ios_defs::eofbit);
    CheckGet(obj, L"j",   L'j', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oj", L'j', L'O', ios_defs::eofbit);
    CheckGet(obj, L"j",   L'j', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"09", L'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, L"09", L'm', L'O', ios_defs::eofbit).m_month, 9);
    CheckGet(obj, L"%Em", L'm', L'E', ios_defs::eofbit);
    CheckGet(obj, L"m",   L'm', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"m",   L'm', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"33", L'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, L"33", L'M', L'O', ios_defs::eofbit).m_minute, 33);
    CheckGet(obj, L"%EM", L'M', L'E', ios_defs::eofbit);
    CheckGet(obj, L"M",   L'M', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"M",   L'M', L'O', ios_defs::strfailbit);

    CheckGet(obj, L"\n",   L'n',  0,  ios_defs::eofbit);
    CheckGet(obj, L"x",    L'n',  0,  ios_defs::goodbit);
    CheckGet(obj, L"\n",   L'n', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%En",  L'n', L'E', ios_defs::eofbit);
    CheckGet(obj, L"n",    L'n', L'O', ios_defs::strfailbit);
    CheckGet(obj, L"%On",  L'n', L'O', ios_defs::eofbit);

    CheckGet(obj, L"\t",   L't',  0,  ios_defs::eofbit);
    CheckGet(obj, L"x",    L't',  0,  ios_defs::goodbit);
    CheckGet(obj, L"\t",   L't', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Et",  L't', L'E', ios_defs::eofbit);
    CheckGet(obj, L"n",    L't', L'O', ios_defs::strfailbit);
    CheckGet(obj, L"%Ot",  L't', L'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 PM", L"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 AM", L"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(CheckGet(obj, L"PM", L'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(CheckGet(obj, L"AM", L'p', 0, ios_defs::eofbit).m_is_pm, false);
    CheckGet(obj, L"%Ep", L'p', L'E', ios_defs::eofbit);
    CheckGet(obj, L"p",   L'p', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Op", L'p', L'O', ios_defs::eofbit);
    CheckGet(obj, L"p",   L'p', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01:33:18 PM", L"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"%Er", L'r', L'E', ios_defs::eofbit);
    CheckGet(obj, L"r",   L'r', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Or", L'r', L'O', ios_defs::eofbit);
    CheckGet(obj, L"r",   L'r', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33", L"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, L"%ER", L'R', L'E', ios_defs::eofbit);
    CheckGet(obj, L"R",   L'R', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OR", L'R', L'O', ios_defs::eofbit);
    CheckGet(obj, L"R",   L'R', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"18", L'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, L"18", L'S', L'O', ios_defs::eofbit).m_second, 18);
    CheckGet(obj, L"%ES", L'S', L'E', ios_defs::eofbit);
    CheckGet(obj, L"S",   L'S', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"S",   L'S', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33:18", L"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33:18", L"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"X",   L'X', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OX", L'X', L'O', ios_defs::eofbit);
    CheckGet(obj, L"X",   L'X', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33:18", L"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"%ET", L'T', L'E', ios_defs::eofbit);
    CheckGet(obj, L"T",   L'T', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OT", L'T', L'O', ios_defs::eofbit);
    CheckGet(obj, L"T",   L'T', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"3", L'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, L"3", L'u', L'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, L"%Eu", L'u', L'E', ios_defs::eofbit);
    CheckGet(obj, L"u",   L'u', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"u",   L'u', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"24", L'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, L"%Eg", L'g', L'E', ios_defs::eofbit);
    CheckGet(obj, L"g",   L'g', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Og", L'g', L'O', ios_defs::eofbit);
    CheckGet(obj, L"g",   L'g', L'O', ios_defs::strfailbit);


    EXPECT_EQ(CheckGet(obj, L"2024", L'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, L"%EG", L'G', L'E', ios_defs::eofbit);
    CheckGet(obj, L"G",   L'G', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OG", L'G', L'O', ios_defs::eofbit);
    CheckGet(obj, L"G",   L'G', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 35 Wed", L"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 35 Wed", L"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, L"35", L'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(CheckGet(obj, L"35", L'U', L'O', ios_defs::eofbit).m_week_no, 35);
    CheckGet(obj, L"%EU", L'U', L'E', ios_defs::eofbit);
    CheckGet(obj, L"U",   L'U', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"U",   L'U', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 36 Wed", L"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 36 Wed", L"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, L"36", L'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(CheckGet(obj, L"36", L'W', L'O', ios_defs::eofbit).m_week_no, 36);
    CheckGet(obj, L"%EW", L'W', L'E', ios_defs::eofbit);
    CheckGet(obj, L"W",   L'W', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"W",   L'W', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"36", L'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    CheckGet(obj, L"54",  L'V', L'O', ios_defs::strfailbit);
    CheckGet(obj, L"36",  L'V', L'O', ios_defs::eofbit);
    CheckGet(obj, L"%EV", L'V', L'E', ios_defs::eofbit);
    CheckGet(obj, L"V",   L'V', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"V",   L'V', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"3", L'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, L"3", L'w', L'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, L"%Ew", L'w', L'E', ios_defs::eofbit);
    CheckGet(obj, L"w",   L'w', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"w",   L'w', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"24", L'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, L"24", L'y', L'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, L"24", L'y', L'O', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, L"y",  L'y', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"y",  L'y', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"2024", L'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, L"2024", L'Y', L'E', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, L"Y",   L'Y', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OY", L'Y', L'O', ios_defs::eofbit);
    CheckGet(obj, L"Y",   L'Y', L'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(CheckGet(obj, L"America/Los_Angeles", L'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, L"PST", L'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, L"America/Los_Angexes", L'Z', 0, ios_defs::strfailbit);
    CheckGet(obj, L"%EZ", L'Z', L'E', ios_defs::eofbit);
    CheckGet(obj, L"Z",   L'Z', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OZ", L'Z', L'O', ios_defs::eofbit);
    CheckGet(obj, L"Z",   L'Z', L'O', ios_defs::strfailbit);

    CheckGet(obj, L"Z", L'z', 0, ios_defs::eofbit);
    CheckGet(obj, L"+13", L'z', 0, ios_defs::eofbit);
    CheckGet(obj, L"-1110", L'z', 0, ios_defs::eofbit);
    CheckGet(obj, L"+11:10", L'z', 0, ios_defs::eofbit);
    CheckGet(obj, L"%Ez", L'z', L'E', ios_defs::eofbit);
    CheckGet(obj, L"z",  L'z', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oz", L'z', L'O', ios_defs::eofbit);
    CheckGet(obj, L"z",  L'z', L'O', ios_defs::strfailbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"1999-W52-6", L"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2019-W01-1", L"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"1999-W52-5", L"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"99-W52-6", L"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"19-W01-1", L"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"99-W52-5", L"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"20 24/09/04", L"%C %y/%m/%d", ios_defs::eofbit), check_date1);

    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((CheckGet<year_month_day>(obj, L"20 01 01", L"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioWchar, ChineseReadsEveryConversionSpecifier)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<wchar_t>>("zh_CN.UTF-8"));

    CheckGet(obj, L"%",  L'%',  0,  ios_defs::eofbit);
    CheckGet(obj, L"x",  L'%',  0,  ios_defs::strfailbit);
    CheckGet(obj, L"%",  L'%', L'E', febit);
    CheckGet(obj, L"%E%", L'%', L'E', ios_defs::eofbit);
    CheckGet(obj, L"%",  L'%', L'O', febit);
    CheckGet(obj, L"%O%", L'%', L'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet(obj, L"三", L'a', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, L"%Ea", L'a', L'E', ios_defs::eofbit);
    CheckGet(obj, L"a",   L'a', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oa", L'a', L'O', ios_defs::eofbit);
    CheckGet(obj, L"a",   L'a', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"星期三", L'A', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, L"%EA", L'A', L'E', ios_defs::eofbit);
    CheckGet(obj, L"A",   L'A', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OA", L'A', L'O', ios_defs::eofbit);
    CheckGet(obj, L"A",   L'A', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"九月", L'b', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, L"%Eb", L'b', L'E', ios_defs::eofbit);
    CheckGet(obj, L"b",   L'b', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Ob", L'b', L'O', ios_defs::eofbit);
    CheckGet(obj, L"b",   L'b', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"九月", L'B', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, L"%EB", L'B', L'E', ios_defs::eofbit);
    CheckGet(obj, L"B",   L'B', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OB", L'B', L'O', ios_defs::eofbit);
    CheckGet(obj, L"B",   L'B', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"九月", L'h', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, L"%Eh", L'h', L'E', ios_defs::eofbit);
    CheckGet(obj, L"h",   L'h', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oh", L'h', L'O', ios_defs::eofbit);
    CheckGet(obj, L"h",   L'h', L'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024年09月04日 星期三 13时33分18秒", L'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024年09月04日 星期三 13时33分18秒", L'c', L'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, L"c",   L'c', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oc", L'c', L'O', ios_defs::eofbit);
    CheckGet(obj, L"c",   L'c', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"20", L'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(CheckGet(obj, L"20", L'C', L'E', ios_defs::eofbit).m_century, 20);
    CheckGet(obj, L"C",   L'C', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OC", L'C', L'O', ios_defs::eofbit);
    CheckGet(obj, L"C",   L'C', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"04", L'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, L"04", L'd', L'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, L"%Ed", L'd', L'E', ios_defs::eofbit);
    CheckGet(obj, L"d",   L'd', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"d",   L'd', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"4", L'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, L"4", L'e', L'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, L"%Ee", L'e', L'E', ios_defs::eofbit);
    CheckGet(obj, L"e",   L'e', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"e",   L'e', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024-09-04", L'F', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, L"%EF", L'F', L'E', ios_defs::eofbit);
    CheckGet(obj, L"F",   L'F', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OF", L'F', L'O', ios_defs::eofbit);
    CheckGet(obj, L"F",   L'F', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024年09月04日", L'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024年09月04日", L'x', L'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, L"x",   L'x', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Ox", L'x', L'O', ios_defs::eofbit);
    CheckGet(obj, L"x",   L'x', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"09/04/24", L'D', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, L"%ED", L'D', L'E', ios_defs::eofbit);
    CheckGet(obj, L"D",   L'D', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OD", L'D', L'O', ios_defs::eofbit);
    CheckGet(obj, L"D",   L'D', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"13", L'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, L"13", L'H', L'O', ios_defs::eofbit).m_hour, 13);
    CheckGet(obj, L"%EH", L'H', L'E', ios_defs::eofbit);
    CheckGet(obj, L"H",   L'H', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"H",   L'H', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"01", L'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, L"01", L'I', L'O', ios_defs::eofbit).m_hour, 1);
    CheckGet(obj, L"%EI", L'I', L'E', ios_defs::eofbit);
    CheckGet(obj, L"I",   L'I', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"I",   L'I', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"248", L'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 248", L"%Y %j", ios_defs::eofbit), check_date1);
    CheckGet(obj, L"%Ej", L'j', L'E', ios_defs::eofbit);
    CheckGet(obj, L"j",   L'j', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oj", L'j', L'O', ios_defs::eofbit);
    CheckGet(obj, L"j",   L'j', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"09", L'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, L"09", L'm', L'O', ios_defs::eofbit).m_month, 9);
    CheckGet(obj, L"%Em", L'm', L'E', ios_defs::eofbit);
    CheckGet(obj, L"m",   L'm', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"m",   L'm', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"33", L'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, L"33", L'M', L'O', ios_defs::eofbit).m_minute, 33);
    CheckGet(obj, L"%EM", L'M', L'E', ios_defs::eofbit);
    CheckGet(obj, L"M",   L'M', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"M",   L'M', L'O', ios_defs::strfailbit);

    CheckGet(obj, L"\n",   L'n',  0,  ios_defs::eofbit);
    CheckGet(obj, L"x",    L'n',  0,  ios_defs::goodbit);
    CheckGet(obj, L"\n",   L'n', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%En",  L'n', L'E', ios_defs::eofbit);
    CheckGet(obj, L"n",    L'n', L'O', ios_defs::strfailbit);
    CheckGet(obj, L"%On",  L'n', L'O', ios_defs::eofbit);

    CheckGet(obj, L"\t",   L't',  0,  ios_defs::eofbit);
    CheckGet(obj, L"x",    L't',  0,  ios_defs::goodbit);
    CheckGet(obj, L"\t",   L't', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Et",  L't', L'E', ios_defs::eofbit);
    CheckGet(obj, L"n",    L't', L'O', ios_defs::strfailbit);
    CheckGet(obj, L"%Ot",  L't', L'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 下午", L"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 上午", L"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(CheckGet(obj, L"下午", L'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(CheckGet(obj, L"上午", L'p', 0, ios_defs::eofbit).m_is_pm, false);
    CheckGet(obj, L"%Ep", L'p', L'E', ios_defs::eofbit);
    CheckGet(obj, L"p",   L'p', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Op", L'p', L'O', ios_defs::eofbit);
    CheckGet(obj, L"p",   L'p', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"下午 01时33分18秒", L"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"%Er", L'r', L'E', ios_defs::eofbit);
    CheckGet(obj, L"r",   L'r', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Or", L'r', L'O', ios_defs::eofbit);
    CheckGet(obj, L"r",   L'r', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33", L"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, L"%ER", L'R', L'E', ios_defs::eofbit);
    CheckGet(obj, L"R",   L'R', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OR", L'R', L'O', ios_defs::eofbit);
    CheckGet(obj, L"R",   L'R', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"18", L'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, L"18", L'S', L'O', ios_defs::eofbit).m_second, 18);
    CheckGet(obj, L"%ES", L'S', L'E', ios_defs::eofbit);
    CheckGet(obj, L"S",   L'S', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"S",   L'S', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13时33分18秒", L"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13时33分18秒", L"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"X",   L'X', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OX", L'X', L'O', ios_defs::eofbit);
    CheckGet(obj, L"X",   L'X', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33:18", L"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"%ET", L'T', L'E', ios_defs::eofbit);
    CheckGet(obj, L"T",   L'T', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OT", L'T', L'O', ios_defs::eofbit);
    CheckGet(obj, L"T",   L'T', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"3", L'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, L"3", L'u', L'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, L"%Eu", L'u', L'E', ios_defs::eofbit);
    CheckGet(obj, L"u",   L'u', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"u",   L'u', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"24", L'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, L"%Eg", L'g', L'E', ios_defs::eofbit);
    CheckGet(obj, L"g",   L'g', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Og", L'g', L'O', ios_defs::eofbit);
    CheckGet(obj, L"g",   L'g', L'O', ios_defs::strfailbit);


    EXPECT_EQ(CheckGet(obj, L"2024", L'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, L"%EG", L'G', L'E', ios_defs::eofbit);
    CheckGet(obj, L"G",   L'G', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OG", L'G', L'O', ios_defs::eofbit);
    CheckGet(obj, L"G",   L'G', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 35 三", L"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 35 三", L"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, L"35", L'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(CheckGet(obj, L"35", L'U', L'O', ios_defs::eofbit).m_week_no, 35);
    CheckGet(obj, L"%EU", L'U', L'E', ios_defs::eofbit);
    CheckGet(obj, L"U",   L'U', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"U",   L'U', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 36 三", L"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 36 三", L"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, L"36", L'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(CheckGet(obj, L"36", L'W', L'O', ios_defs::eofbit).m_week_no, 36);
    CheckGet(obj, L"%EW", L'W', L'E', ios_defs::eofbit);
    CheckGet(obj, L"W",   L'W', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"W",   L'W', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"36", L'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    CheckGet(obj, L"54",  L'V', L'O', ios_defs::strfailbit);
    CheckGet(obj, L"36",  L'V', L'O', ios_defs::eofbit);
    CheckGet(obj, L"%EV", L'V', L'E', ios_defs::eofbit);
    CheckGet(obj, L"V",   L'V', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"V",   L'V', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"3", L'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, L"3", L'w', L'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, L"%Ew", L'w', L'E', ios_defs::eofbit);
    CheckGet(obj, L"w",   L'w', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"w",   L'w', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"24", L'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, L"24", L'y', L'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, L"24", L'y', L'O', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, L"y",  L'y', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"y",  L'y', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"2024", L'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, L"2024", L'Y', L'E', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, L"Y",   L'Y', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OY", L'Y', L'O', ios_defs::eofbit);
    CheckGet(obj, L"Y",   L'Y', L'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(CheckGet(obj, L"America/Los_Angeles", L'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, L"PST", L'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, L"America/Los_Angexes", L'Z', 0, ios_defs::strfailbit);
    CheckGet(obj, L"%EZ", L'Z', L'E', ios_defs::eofbit);
    CheckGet(obj, L"Z",   L'Z', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OZ", L'Z', L'O', ios_defs::eofbit);
    CheckGet(obj, L"Z",   L'Z', L'O', ios_defs::strfailbit);

    CheckGet(obj, L"Z", L'z', 0, ios_defs::eofbit);
    CheckGet(obj, L"+13", L'z', 0, ios_defs::eofbit);
    CheckGet(obj, L"-1110", L'z', 0, ios_defs::eofbit);
    CheckGet(obj, L"+11:10", L'z', 0, ios_defs::eofbit);
    CheckGet(obj, L"%Ez", L'z', L'E', ios_defs::eofbit);
    CheckGet(obj, L"z",  L'z', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oz", L'z', L'O', ios_defs::eofbit);
    CheckGet(obj, L"z",  L'z', L'O', ios_defs::strfailbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"1999-W52-6", L"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2019-W01-1", L"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"1999-W52-5", L"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"99-W52-6", L"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"19-W01-1", L"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"99-W52-5", L"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"20 24/09/04", L"%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((CheckGet<year_month_day>(obj, L"20 01 01", L"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioWchar, JapaneseReadsEveryConversionSpecifier)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<wchar_t>>("ja_JP.UTF-8"));

    CheckGet(obj, L"%",  L'%',  0,  ios_defs::eofbit);
    CheckGet(obj, L"x",  L'%',  0,  ios_defs::strfailbit);
    CheckGet(obj, L"%",  L'%', L'E', febit);
    CheckGet(obj, L"%E%", L'%', L'E', ios_defs::eofbit);
    CheckGet(obj, L"%",  L'%', L'O', febit);
    CheckGet(obj, L"%O%", L'%', L'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet(obj, L"水", L'a', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, L"%Ea", L'a', L'E', ios_defs::eofbit);
    CheckGet(obj, L"a",   L'a', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oa", L'a', L'O', ios_defs::eofbit);
    CheckGet(obj, L"a",   L'a', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"水曜日", L'A', 0, ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, L"%EA", L'A', L'E', ios_defs::eofbit);
    CheckGet(obj, L"A",   L'A', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OA", L'A', L'O', ios_defs::eofbit);
    CheckGet(obj, L"A",   L'A', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"9月", L'b', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, L"%Eb", L'b', L'E', ios_defs::eofbit);
    CheckGet(obj, L"b",   L'b', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Ob", L'b', L'O', ios_defs::eofbit);
    CheckGet(obj, L"b",   L'b', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"9月", L'B', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, L"%EB", L'B', L'E', ios_defs::eofbit);
    CheckGet(obj, L"B",   L'B', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OB", L'B', L'O', ios_defs::eofbit);
    CheckGet(obj, L"B",   L'B', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"9月", L'h', 0, ios_defs::eofbit).m_month, 9);
    CheckGet(obj, L"%Eh", L'h', L'E', ios_defs::eofbit);
    CheckGet(obj, L"h",   L'h', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oh", L'h', L'O', ios_defs::eofbit);
    CheckGet(obj, L"h",   L'h', L'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024年09月04日 13時33分18秒", L'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"令和6年09月04日 13時33分18秒", L'c', L'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"202409月04日 13時33分18秒", L'c', L'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, L"c",   L'c', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oc", L'c', L'O', ios_defs::eofbit);
    CheckGet(obj, L"c",   L'c', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"20", L'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"平成", L'C', L'E', ios_defs::eofbit).year(), std::chrono::year(1990));
    CheckGet(obj, L"C",   L'C', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OC", L'C', L'O', ios_defs::eofbit);
    CheckGet(obj, L"C",   L'C', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"04", L'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, L"04", L'd', L'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, L"四", L'd', L'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, L"%Ed", L'd', L'E', ios_defs::eofbit);
    CheckGet(obj, L"d",   L'd', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"d",   L'd', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"4", L'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, L"4", L'e', L'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(CheckGet(obj, L"四", L'e', L'O', ios_defs::eofbit).m_mday, 4);
    CheckGet(obj, L"%Ee", L'e', L'E', ios_defs::eofbit);
    CheckGet(obj, L"e",   L'e', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"e",   L'e', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024-09-04", L'F', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, L"%EF", L'F', L'E', ios_defs::eofbit);
    CheckGet(obj, L"F",   L'F', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OF", L'F', L'O', ios_defs::eofbit);
    CheckGet(obj, L"F",   L'F', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024年09月04日", L'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"令和6年09月04日", L'x', L'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"202409月04日", L'x', L'E', ios_defs::eofbit), check_date1);
    CheckGet(obj, L"x",   L'x', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Ox", L'x', L'O', ios_defs::eofbit);
    CheckGet(obj, L"x",   L'x', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"09/04/24", L'D', 0, ios_defs::eofbit), check_date1);
    CheckGet(obj, L"%ED", L'D', L'E', ios_defs::eofbit);
    CheckGet(obj, L"D",   L'D', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OD", L'D', L'O', ios_defs::eofbit);
    CheckGet(obj, L"D",   L'D', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"13", L'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, L"13", L'H', L'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(CheckGet(obj, L"十三", L'H', L'O', ios_defs::eofbit).m_hour, 13);
    CheckGet(obj, L"%EH", L'H', L'E', ios_defs::eofbit);
    CheckGet(obj, L"H",   L'H', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"H",   L'H', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"01", L'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, L"01", L'I', L'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(CheckGet(obj, L"一", L'I', L'O', ios_defs::eofbit).m_hour, 1);
    CheckGet(obj, L"%EI", L'I', L'E', ios_defs::eofbit);
    CheckGet(obj, L"I",   L'I', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"I",   L'I', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"248", L'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 248", L"%Y %j", ios_defs::eofbit), check_date1);
    CheckGet(obj, L"%Ej", L'j', L'E', ios_defs::eofbit);
    CheckGet(obj, L"j",   L'j', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oj", L'j', L'O', ios_defs::eofbit);
    CheckGet(obj, L"j",   L'j', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"09", L'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, L"09", L'm', L'O', ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(CheckGet(obj, L"九", L'm', L'O', ios_defs::eofbit).m_month, 9);
    CheckGet(obj, L"%Em", L'm', L'E', ios_defs::eofbit);
    CheckGet(obj, L"m",   L'm', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"m",   L'm', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"33", L'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, L"33", L'M', L'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(CheckGet(obj, L"三十三", L'M', L'O', ios_defs::eofbit).m_minute, 33);
    CheckGet(obj, L"%EM", L'M', L'E', ios_defs::eofbit);
    CheckGet(obj, L"M",   L'M', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"M",   L'M', L'O', ios_defs::strfailbit);

    CheckGet(obj, L"\n",   L'n',  0,  ios_defs::eofbit);
    CheckGet(obj, L"x",    L'n',  0,  ios_defs::goodbit);
    CheckGet(obj, L"\n",   L'n', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%En",  L'n', L'E', ios_defs::eofbit);
    CheckGet(obj, L"n",    L'n', L'O', ios_defs::strfailbit);
    CheckGet(obj, L"%On",  L'n', L'O', ios_defs::eofbit);

    CheckGet(obj, L"\t",   L't',  0,  ios_defs::eofbit);
    CheckGet(obj, L"x",    L't',  0,  ios_defs::goodbit);
    CheckGet(obj, L"\t",   L't', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Et",  L't', L'E', ios_defs::eofbit);
    CheckGet(obj, L"n",    L't', L'O', ios_defs::strfailbit);
    CheckGet(obj, L"%Ot",  L't', L'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 午後", L"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 午前", L"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(CheckGet(obj, L"午後", L'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(CheckGet(obj, L"午前", L'p', 0, ios_defs::eofbit).m_is_pm, false);
    CheckGet(obj, L"%Ep", L'p', L'E', ios_defs::eofbit);
    CheckGet(obj, L"p",   L'p', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Op", L'p', L'O', ios_defs::eofbit);
    CheckGet(obj, L"p",   L'p', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"午後01時33分18秒", L"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"%Er", L'r', L'E', ios_defs::eofbit);
    CheckGet(obj, L"r",   L'r', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Or", L'r', L'O', ios_defs::eofbit);
    CheckGet(obj, L"r",   L'r', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33", L"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, L"%ER", L'R', L'E', ios_defs::eofbit);
    CheckGet(obj, L"R",   L'R', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OR", L'R', L'O', ios_defs::eofbit);
    CheckGet(obj, L"R",   L'R', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"18", L'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, L"18", L'S', L'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(CheckGet(obj, L"十八", L'S', L'O', ios_defs::eofbit).m_second, 18);
    CheckGet(obj, L"%ES", L'S', L'E', ios_defs::eofbit);
    CheckGet(obj, L"S",   L'S', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"S",   L'S', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13時33分18秒", L"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13時33分18秒", L"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"X",   L'X', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OX", L'X', L'O', ios_defs::eofbit);
    CheckGet(obj, L"X",   L'X', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33:18", L"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"%ET", L'T', L'E', ios_defs::eofbit);
    CheckGet(obj, L"T",   L'T', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OT", L'T', L'O', ios_defs::eofbit);
    CheckGet(obj, L"T",   L'T', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"3", L'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, L"3", L'u', L'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, L"三", L'u', L'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, L"%Eu", L'u', L'E', ios_defs::eofbit);
    CheckGet(obj, L"u",   L'u', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"u",   L'u', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"24", L'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, L"%Eg", L'g', L'E', ios_defs::eofbit);
    CheckGet(obj, L"g",   L'g', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Og", L'g', L'O', ios_defs::eofbit);
    CheckGet(obj, L"g",   L'g', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"2024", L'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    CheckGet(obj, L"%EG", L'G', L'E', ios_defs::eofbit);
    CheckGet(obj, L"G",   L'G', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OG", L'G', L'O', ios_defs::eofbit);
    CheckGet(obj, L"G",   L'G', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 35 水", L"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 35 水", L"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 三十五 水", L"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, L"35", L'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(CheckGet(obj, L"35", L'U', L'O', ios_defs::eofbit).m_week_no, 35);
    CheckGet(obj, L"%EU", L'U', L'E', ios_defs::eofbit);
    CheckGet(obj, L"U",   L'U', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"U",   L'U', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 36 水", L"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 36 水", L"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2024 三十六 水", L"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(CheckGet(obj, L"36", L'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(CheckGet(obj, L"36", L'W', L'O', ios_defs::eofbit).m_week_no, 36);
    CheckGet(obj, L"%EW", L'W', L'E', ios_defs::eofbit);
    CheckGet(obj, L"W",   L'W', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"W",   L'W', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"36", L'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(CheckGet(obj, L"36", L'V', L'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(CheckGet(obj, L"三十六", L'V', L'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    CheckGet(obj, L"54",  L'V', L'O', ios_defs::strfailbit);
    CheckGet(obj, L"%EV", L'V', L'E', ios_defs::eofbit);
    CheckGet(obj, L"V",   L'V', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"V",   L'V', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"3", L'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, L"3", L'w', L'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(CheckGet(obj, L"三", L'w', L'O', ios_defs::eofbit).m_wday, 3);
    CheckGet(obj, L"%Ew", L'w', L'E', ios_defs::eofbit);
    CheckGet(obj, L"w",   L'w', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"w",   L'w', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"24", L'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"6", L'y', L'E', ios_defs::eofbit).year(), std::chrono::year(2024));
    EXPECT_EQ(CheckGet(obj, L"24", L'y', L'O', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, L"二十四", L'y', L'O', ios_defs::eofbit).m_year, 2024);
    CheckGet(obj, L"y",  L'y', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"y",  L'y', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet(obj, L"2024", L'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet(obj, L"2024", L'Y', L'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"平成3年", L'Y', L'E', ios_defs::eofbit).year(), std::chrono::year(1991));
    CheckGet(obj, L"Y",   L'Y', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OY", L'Y', L'O', ios_defs::eofbit);
    CheckGet(obj, L"Y",   L'Y', L'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(CheckGet(obj, L"America/Los_Angeles", L'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, L"PST", L'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, L"America/Los_Angexes", L'Z', 0, ios_defs::strfailbit);
    CheckGet(obj, L"%EZ", L'Z', L'E', ios_defs::eofbit);
    CheckGet(obj, L"Z",   L'Z', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%OZ", L'Z', L'O', ios_defs::eofbit);
    CheckGet(obj, L"Z",   L'Z', L'O', ios_defs::strfailbit);

    CheckGet(obj, L"Z", L'z', 0, ios_defs::eofbit);
    CheckGet(obj, L"+13", L'z', 0, ios_defs::eofbit);
    CheckGet(obj, L"-1110", L'z', 0, ios_defs::eofbit);
    CheckGet(obj, L"+11:10", L'z', 0, ios_defs::eofbit);
    CheckGet(obj, L"%Ez", L'z', L'E', ios_defs::eofbit);
    CheckGet(obj, L"z",  L'z', L'E', ios_defs::strfailbit);
    CheckGet(obj, L"%Oz", L'z', L'O', ios_defs::eofbit);
    CheckGet(obj, L"z",  L'z', L'O', ios_defs::strfailbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"1999-W52-6", L"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"2019-W01-1", L"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"1999-W52-5", L"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"99-W52-6", L"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"19-W01-1", L"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(CheckGet<year_month_day>(obj, L"99-W52-5", L"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(CheckGet<year_month_day>(obj, L"20 24/09/04", L"%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((CheckGet<year_month_day>(obj, L"20 01 01", L"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioWchar, AWeekdayOrMonthNameIsMatchedAgainstBothSpellings)
{
    timeio obj(std::make_shared<timeio_conf<wchar_t>>("C"));
    {
        std::wstring input = L"Mon";
        std::wstring format = L"%a";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 1);
    }

    {
        std::wstring input = L"Tue ";
        std::wstring format = L"%a";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != L' '));
        EXPECT_EQ(time.tm_wday, 2);
    }

    {
        std::wstring input = L"Wednesday";
        std::wstring format = L"%a";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 3);
    }

    {
        std::wstring input = L"Thu";
        std::wstring format = L"%A";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 4);
    }

    {
        std::wstring input = L"Fri ";
        std::wstring format = L"%A";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != L' '));
        EXPECT_EQ(time.tm_wday, 5);
    }

    {
        std::wstring input = L"Saturday";
        std::wstring format = L"%A";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_wday, 6);
    }

    {
        std::wstring input = L"Feb";
        std::wstring format = L"%b";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 1);
    }

    {
        std::wstring input = L"Mar ";
        std::wstring format = L"%b";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != L' '));
        EXPECT_EQ(time.tm_mon, 2);
    }

    {
        std::wstring input = L"April";
        std::wstring format = L"%b";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 3);
    }

    {
        std::wstring input = L"May";
        std::wstring format = L"%B";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 4);
    }

    {
        std::wstring input = L"Jun ";
        std::wstring format = L"%B";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != L' '));
        EXPECT_EQ(time.tm_mon, 5);
    }

    {
        std::wstring input = L"July";
        std::wstring format = L"%B";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 6);
    }

    {
        std::wstring input = L"Aug";
        std::wstring format = L"%h";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 7);
    }

    {
        std::wstring input = L"May ";
        std::wstring format = L"%h";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_FALSE((ret == input.end()) || (*ret != L' '));
        EXPECT_EQ(time.tm_mon, 4);
    }

    {
        std::wstring input = L"October";
        std::wstring format = L"%h";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mon, 9);
    }

    // Other tests.
    {
        std::wstring input = L"2.";
        std::wstring format = L"%d.";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_mday, 2);
    }

    {
        std::wstring input = L"0.";
        std::wstring format = L"%d.";

        time_parse_context<wchar_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::wstring input = L"32.";
        std::wstring format = L"%d.";

        time_parse_context<wchar_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::wstring input = L"5.";
        std::wstring format = L"%e.";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mday, 5);
    }

    {
        std::wstring input = L"06.";
        std::wstring format = L"%e.";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mday, 6);
    }

    {
        std::wstring input = L"0";
        std::wstring format = L"%e";

        time_parse_context<wchar_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::wstring input = L"35";
        std::wstring format = L"%e";

        time_parse_context<wchar_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    struct clock_case { const wchar_t* input; int hour; int minute; };
    for (const clock_case tc : {
             clock_case{L"12:11AM", 0, 11},
             clock_case{L"03:14AM", 3, 14},
             clock_case{L"09:27AM", 9, 27},
             clock_case{L"12:29PM", 12, 29},
             clock_case{L"02:38PM", 14, 38},
             clock_case{L"09:52PM", 21, 52},
         })
    {
        std::wstring input(tc.input);
        time_parse_context<wchar_t> ctx;
        const auto ret = obj.get(input.begin(), input.end(), ctx,
                                 std::wstring_view{L"%I:%M%p"});
        const auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(ret, input.end());
        EXPECT_EQ(time.tm_hour, tc.hour);
        EXPECT_EQ(time.tm_min, tc.minute);
    }

    {
        std::wstring input = L"08%46";
        std::wstring format = L"%H%%%S";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_hour, 8);
        EXPECT_EQ(time.tm_sec, 46);
    }

    {
        std::wstring input = L"29:14";
        std::wstring format = L"%H:%M";

        time_parse_context<wchar_t> ctx;
        EXPECT_THROW(obj.get(input.begin(), input.end(), ctx, format), stream_error);
    }

    {
        std::wstring input = L"Oct+tail";
        std::wstring format = L"%b+tail";

        time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        EXPECT_EQ(ret, input.end());
        auto time = ctx_to<std::tm>(ctx);
        EXPECT_EQ(time.tm_mon, 9);
    }
}

TEST(TimeioWchar, JapaneseReadsEveryConversionSpecifierIntoADate)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<wchar_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<wchar_t, true, true, tz_level::none>, true, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(L"%",  L'%',  0,  ios_defs::eofbit);
    FOri(L"x",  L'%',  0,  ios_defs::strfailbit);
    FOri(L"%",  L'%', L'E', febit);
    FOri(L"%E%", L'%', L'E', ios_defs::eofbit);
    FOri(L"%",  L'%', L'O', febit);
    FOri(L"%O%", L'%', L'O', ios_defs::eofbit);

    EXPECT_EQ(FOri(L"水", L'a', 0, ios_defs::eofbit).m_wday, 3);
    FOri(L"%Ea", L'a', L'E', ios_defs::eofbit);
    FOri(L"a",   L'a', L'E', ios_defs::strfailbit);
    FOri(L"%Oa", L'a', L'O', ios_defs::eofbit);
    FOri(L"a",   L'a', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"水曜日", L'A', 0, ios_defs::eofbit).m_wday, 3);
    FOri(L"%EA", L'A', L'E', ios_defs::eofbit);
    FOri(L"A",   L'A', L'E', ios_defs::strfailbit);
    FOri(L"%OA", L'A', L'O', ios_defs::eofbit);
    FOri(L"A",   L'A', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"9月", L'b', 0, ios_defs::eofbit).m_month, 9);
    FOri(L"%Eb", L'b', L'E', ios_defs::eofbit);
    FOri(L"b",   L'b', L'E', ios_defs::strfailbit);
    FOri(L"%Ob", L'b', L'O', ios_defs::eofbit);
    FOri(L"b",   L'b', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"9月", L'B', 0, ios_defs::eofbit).m_month, 9);
    FOri(L"%EB", L'B', L'E', ios_defs::eofbit);
    FOri(L"B",   L'B', L'E', ios_defs::strfailbit);
    FOri(L"%OB", L'B', L'O', ios_defs::eofbit);
    FOri(L"B",   L'B', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"9月", L'h', 0, ios_defs::eofbit).m_month, 9);
    FOri(L"%Eh", L'h', L'E', ios_defs::eofbit);
    FOri(L"h",   L'h', L'E', ios_defs::strfailbit);
    FOri(L"%Oh", L'h', L'O', ios_defs::eofbit);
    FOri(L"h",   L'h', L'O', ios_defs::strfailbit);

    using namespace std::chrono;
    EXPECT_EQ(FYmd(L"2024年09月04日 13時33分18秒", L'c', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"令和6年09月04日 13時33分18秒", L'c', L'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"202409月04日 13時33分18秒", L'c', L'E', ios_defs::eofbit), check_date1);
    FOri(L"c",   L'c', L'E', ios_defs::strfailbit);
    FOri(L"%Oc", L'c', L'O', ios_defs::eofbit);
    FOri(L"c",   L'c', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"20", L'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(FYmd(L"平成", L'C', L'E', ios_defs::eofbit).year(), std::chrono::year(1990));
    FOri(L"C",   L'C', L'E', ios_defs::strfailbit);
    FOri(L"%OC", L'C', L'O', ios_defs::eofbit);
    FOri(L"C",   L'C', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"04", L'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(L"04", L'd', L'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(L"四", L'd', L'O', ios_defs::eofbit).m_mday, 4);
    FOri(L"%Ed", L'd', L'E', ios_defs::eofbit);
    FOri(L"d",   L'd', L'E', ios_defs::strfailbit);
    FOri(L"d",   L'd', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"4", L'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(L"4", L'e', L'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(L"四", L'e', L'O', ios_defs::eofbit).m_mday, 4);
    FOri(L"%Ee", L'e', L'E', ios_defs::eofbit);
    FOri(L"e",   L'e', L'E', ios_defs::strfailbit);
    FOri(L"e",   L'e', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(L"2024-09-04", L'F', 0, ios_defs::eofbit), check_date1);
    FOri(L"%EF", L'F', L'E', ios_defs::eofbit);
    FOri(L"F",   L'F', L'E', ios_defs::strfailbit);
    FOri(L"%OF", L'F', L'O', ios_defs::eofbit);
    FOri(L"F",   L'F', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(L"2024年09月04日", L'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"令和6年09月04日", L'x', L'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"202409月04日", L'x', L'E', ios_defs::eofbit), check_date1);
    FOri(L"x",   L'x', L'E', ios_defs::strfailbit);
    FOri(L"%Ox", L'x', L'O', ios_defs::eofbit);
    FOri(L"x",   L'x', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(L"09/04/24", L'D', 0, ios_defs::eofbit), check_date1);
    FOri(L"%ED", L'D', L'E', ios_defs::eofbit);
    FOri(L"D",   L'D', L'E', ios_defs::strfailbit);
    FOri(L"%OD", L'D', L'O', ios_defs::eofbit);
    FOri(L"D",   L'D', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"13", L'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(L"13", L'H', L'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(L"十三", L'H', L'O', ios_defs::eofbit).m_hour, 13);
    FOri(L"%EH", L'H', L'E', ios_defs::eofbit);
    FOri(L"H",   L'H', L'E', ios_defs::strfailbit);
    FOri(L"H",   L'H', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"01", L'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(L"01", L'I', L'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(L"一", L'I', L'O', ios_defs::eofbit).m_hour, 1);
    FOri(L"%EI", L'I', L'E', ios_defs::eofbit);
    FOri(L"I",   L'I', L'E', ios_defs::strfailbit);
    FOri(L"I",   L'I', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"248", L'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(FYmd(L"2024 248", L"%Y %j", ios_defs::eofbit), check_date1);
    FOri(L"%Ej", L'j', L'E', ios_defs::eofbit);
    FOri(L"j",   L'j', L'E', ios_defs::strfailbit);
    FOri(L"%Oj", L'j', L'O', ios_defs::eofbit);
    FOri(L"j",   L'j', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"09", L'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri(L"09", L'm', L'O', ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri(L"九", L'm', L'O', ios_defs::eofbit).m_month, 9);
    FOri(L"%Em", L'm', L'E', ios_defs::eofbit);
    FOri(L"m",   L'm', L'E', ios_defs::strfailbit);
    FOri(L"m",   L'm', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"33", L'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(L"33", L'M', L'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(L"三十三", L'M', L'O', ios_defs::eofbit).m_minute, 33);
    FOri(L"%EM", L'M', L'E', ios_defs::eofbit);
    FOri(L"M",   L'M', L'E', ios_defs::strfailbit);
    FOri(L"M",   L'M', L'O', ios_defs::strfailbit);

    FOri(L"\n",   L'n',  0,  ios_defs::eofbit);
    FOri(L"x",    L'n',  0,  ios_defs::goodbit);
    FOri(L"\n",   L'n', L'E', ios_defs::strfailbit);
    FOri(L"%En",  L'n', L'E', ios_defs::eofbit);
    FOri(L"n",    L'n', L'O', ios_defs::strfailbit);
    FOri(L"%On",  L'n', L'O', ios_defs::eofbit);

    FOri(L"\t",   L't',  0,  ios_defs::eofbit);
    FOri(L"x",    L't',  0,  ios_defs::goodbit);
    FOri(L"\t",   L't', L'E', ios_defs::strfailbit);
    FOri(L"%Et",  L't', L'E', ios_defs::eofbit);
    FOri(L"n",    L't', L'O', ios_defs::strfailbit);
    FOri(L"%Ot",  L't', L'O', ios_defs::eofbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 午後", L"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 午前", L"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(FOri(L"午後", L'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(FOri(L"午前", L'p', 0, ios_defs::eofbit).m_is_pm, false);
    FOri(L"%Ep", L'p', L'E', ios_defs::eofbit);
    FOri(L"p",   L'p', L'E', ios_defs::strfailbit);
    FOri(L"%Op", L'p', L'O', ios_defs::eofbit);
    FOri(L"p",   L'p', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"午後01時33分18秒", L"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"%Er", L'r', L'E', ios_defs::eofbit);
    FOri(L"r",   L'r', L'E', ios_defs::strfailbit);
    FOri(L"%Or", L'r', L'O', ios_defs::eofbit);
    FOri(L"r",   L'r', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33", L"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(L"%ER", L'R', L'E', ios_defs::eofbit);
    FOri(L"R",   L'R', L'E', ios_defs::strfailbit);
    FOri(L"%OR", L'R', L'O', ios_defs::eofbit);
    FOri(L"R",   L'R', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"18", L'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(L"18", L'S', L'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(L"十八", L'S', L'O', ios_defs::eofbit).m_second, 18);
    FOri(L"%ES", L'S', L'E', ios_defs::eofbit);
    FOri(L"S",   L'S', L'E', ios_defs::strfailbit);
    FOri(L"S",   L'S', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13時33分18秒", L"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13時33分18秒", L"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"X",   L'X', L'E', ios_defs::strfailbit);
    FOri(L"%OX", L'X', L'O', ios_defs::eofbit);
    FOri(L"X",   L'X', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33:18", L"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"%ET", L'T', L'E', ios_defs::eofbit);
    FOri(L"T",   L'T', L'E', ios_defs::strfailbit);
    FOri(L"%OT", L'T', L'O', ios_defs::eofbit);
    FOri(L"T",   L'T', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"3", L'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(L"3", L'u', L'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(L"三", L'u', L'O', ios_defs::eofbit).m_wday, 3);
    FOri(L"%Eu", L'u', L'E', ios_defs::eofbit);
    FOri(L"u",   L'u', L'E', ios_defs::strfailbit);
    FOri(L"u",   L'u', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"24", L'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri(L"%Eg", L'g', L'E', ios_defs::eofbit);
    FOri(L"g",   L'g', L'E', ios_defs::strfailbit);
    FOri(L"%Og", L'g', L'O', ios_defs::eofbit);
    FOri(L"g",   L'g', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"2024", L'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri(L"%EG", L'G', L'E', ios_defs::eofbit);
    FOri(L"G",   L'G', L'E', ios_defs::strfailbit);
    FOri(L"%OG", L'G', L'O', ios_defs::eofbit);
    FOri(L"G",   L'G', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(L"2024 35 水", L"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"2024 35 水", L"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"2024 三十五 水", L"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri(L"35", L'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(FOri(L"35", L'U', L'O', ios_defs::eofbit).m_week_no, 35);
    FOri(L"%EU", L'U', L'E', ios_defs::eofbit);
    FOri(L"U",   L'U', L'E', ios_defs::strfailbit);
    FOri(L"U",   L'U', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(L"2024 36 水", L"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"2024 36 水", L"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"2024 三十六 水", L"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri(L"36", L'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(FOri(L"36", L'W', L'O', ios_defs::eofbit).m_week_no, 36);
    FOri(L"%EW", L'W', L'E', ios_defs::eofbit);
    FOri(L"W",   L'W', L'E', ios_defs::strfailbit);
    FOri(L"W",   L'W', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"36", L'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri(L"36", L'V', L'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri(L"三十六", L'V', L'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    FOri(L"54",  L'V', L'O', ios_defs::strfailbit);
    FOri(L"%EV", L'V', L'E', ios_defs::eofbit);
    FOri(L"V",   L'V', L'E', ios_defs::strfailbit);
    FOri(L"V",   L'V', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"3", L'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(L"3", L'w', L'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(L"三", L'w', L'O', ios_defs::eofbit).m_wday, 3);
    FOri(L"%Ew", L'w', L'E', ios_defs::eofbit);
    FOri(L"w",   L'w', L'E', ios_defs::strfailbit);
    FOri(L"w",   L'w', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"24", L'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd(L"6", L'y', L'E', ios_defs::eofbit).year(), std::chrono::year(2024));
    EXPECT_EQ(FOri(L"24", L'y', L'O', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri(L"二十四", L'y', L'O', ios_defs::eofbit).m_year, 2024);
    FOri(L"y",  L'y', L'E', ios_defs::strfailbit);
    FOri(L"y",  L'y', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"2024", L'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri(L"2024", L'Y', L'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd(L"平成3年", L'Y', L'E', ios_defs::eofbit).year(), std::chrono::year(1991));
    FOri(L"Y",   L'Y', L'E', ios_defs::strfailbit);
    FOri(L"%OY", L'Y', L'O', ios_defs::eofbit);
    FOri(L"Y",   L'Y', L'O', ios_defs::strfailbit);

    FOri(L"%Z", L'Z', 0, ios_defs::eofbit);
    FOri(L"%EZ", L'Z', L'E', ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'E', ios_defs::strfailbit);
    FOri(L"%OZ", L'Z', L'O', ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'O', ios_defs::strfailbit);

    FOri(L"%z", L'z', 0, ios_defs::eofbit);
    FOri(L"%Ez", L'z', L'E', ios_defs::eofbit);
    FOri(L"%Oz", L'z', L'O', ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(FYmd(L"1999-W52-6", L"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd(L"2019-W01-1", L"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd(L"1999-W52-5", L"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd(L"99-W52-6", L"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd(L"19-W01-1", L"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd(L"99-W52-5", L"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd(L"20 24/09/04", L"%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((FYmd(L"20 01 01", L"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioWchar, JapaneseReadsEveryConversionSpecifierIntoADateWithoutAZone)
{
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    timeio obj(std::make_shared<timeio_conf<wchar_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<wchar_t, true, false, tz_level::none>, true, false, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, false, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(L"%",  L'%',  0,  ios_defs::eofbit);
    FOri(L"x",  L'%',  0,  ios_defs::strfailbit);
    FOri(L"%",  L'%', L'E', febit);
    FOri(L"%E%", L'%', L'E', ios_defs::eofbit);
    FOri(L"%",  L'%', L'O', febit);
    FOri(L"%O%", L'%', L'O', ios_defs::eofbit);

    EXPECT_EQ(FOri(L"水", L'a', 0, ios_defs::eofbit).m_wday, 3);
    FOri(L"%Ea", L'a', L'E', ios_defs::eofbit);
    FOri(L"a",   L'a', L'E', ios_defs::strfailbit);
    FOri(L"%Oa", L'a', L'O', ios_defs::eofbit);
    FOri(L"a",   L'a', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"水曜日", L'A', 0, ios_defs::eofbit).m_wday, 3);
    FOri(L"%EA", L'A', L'E', ios_defs::eofbit);
    FOri(L"A",   L'A', L'E', ios_defs::strfailbit);
    FOri(L"%OA", L'A', L'O', ios_defs::eofbit);
    FOri(L"A",   L'A', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"9月", L'b', 0, ios_defs::eofbit).m_month, 9);
    FOri(L"%Eb", L'b', L'E', ios_defs::eofbit);
    FOri(L"b",   L'b', L'E', ios_defs::strfailbit);
    FOri(L"%Ob", L'b', L'O', ios_defs::eofbit);
    FOri(L"b",   L'b', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"9月", L'B', 0, ios_defs::eofbit).m_month, 9);
    FOri(L"%EB", L'B', L'E', ios_defs::eofbit);
    FOri(L"B",   L'B', L'E', ios_defs::strfailbit);
    FOri(L"%OB", L'B', L'O', ios_defs::eofbit);
    FOri(L"B",   L'B', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"9月", L'h', 0, ios_defs::eofbit).m_month, 9);
    FOri(L"%Eh", L'h', L'E', ios_defs::eofbit);
    FOri(L"h",   L'h', L'E', ios_defs::strfailbit);
    FOri(L"%Oh", L'h', L'O', ios_defs::eofbit);
    FOri(L"h",   L'h', L'O', ios_defs::strfailbit);

    using namespace std::chrono;
    FYmd(L"%c", L'c', 0, ios_defs::eofbit);
    FYmd(L"%Ec", L'c', L'E', ios_defs::eofbit);
    FOri(L"c",   L'c', L'E', ios_defs::strfailbit);
    FOri(L"%Oc", L'c', L'O', ios_defs::eofbit);
    FOri(L"c",   L'c', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"20", L'C', 0,   ios_defs::eofbit).m_century, 20);
    EXPECT_EQ(FYmd(L"平成", L'C', L'E', ios_defs::eofbit).year(), std::chrono::year(1990));
    FOri(L"C",   L'C', L'E', ios_defs::strfailbit);
    FOri(L"%OC", L'C', L'O', ios_defs::eofbit);
    FOri(L"C",   L'C', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"04", L'd', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(L"04", L'd', L'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(L"四", L'd', L'O', ios_defs::eofbit).m_mday, 4);
    FOri(L"%Ed", L'd', L'E', ios_defs::eofbit);
    FOri(L"d",   L'd', L'E', ios_defs::strfailbit);
    FOri(L"d",   L'd', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"4", L'e', 0,   ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(L"4", L'e', L'O', ios_defs::eofbit).m_mday, 4);
    EXPECT_EQ(FOri(L"四", L'e', L'O', ios_defs::eofbit).m_mday, 4);
    FOri(L"%Ee", L'e', L'E', ios_defs::eofbit);
    FOri(L"e",   L'e', L'E', ios_defs::strfailbit);
    FOri(L"e",   L'e', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(L"2024-09-04", L'F', 0, ios_defs::eofbit), check_date1);
    FOri(L"%EF", L'F', L'E', ios_defs::eofbit);
    FOri(L"F",   L'F', L'E', ios_defs::strfailbit);
    FOri(L"%OF", L'F', L'O', ios_defs::eofbit);
    FOri(L"F",   L'F', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(L"2024年09月04日", L'x', 0, ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"令和6年09月04日", L'x', L'E', ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"202409月04日", L'x', L'E', ios_defs::eofbit), check_date1);
    FOri(L"x",   L'x', L'E', ios_defs::strfailbit);
    FOri(L"%Ox", L'x', L'O', ios_defs::eofbit);
    FOri(L"x",   L'x', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(L"09/04/24", L'D', 0, ios_defs::eofbit), check_date1);
    FOri(L"%ED", L'D', L'E', ios_defs::eofbit);
    FOri(L"D",   L'D', L'E', ios_defs::strfailbit);
    FOri(L"%OD", L'D', L'O', ios_defs::eofbit);
    FOri(L"D",   L'D', L'O', ios_defs::strfailbit);

    FOri(L"%H", L'H', 0,   ios_defs::eofbit);
    FOri(L"%EH", L'H', L'E', ios_defs::eofbit);
    FOri(L"H",   L'H', L'E', ios_defs::strfailbit);
    FOri(L"H",   L'H', L'O', ios_defs::strfailbit);

    FOri(L"%I", L'I', 0,   ios_defs::eofbit);
    FOri(L"%EI", L'I', L'E', ios_defs::eofbit);
    FOri(L"I",   L'I', L'E', ios_defs::strfailbit);
    FOri(L"I",   L'I', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"248", L'j', 0, ios_defs::eofbit).m_yday, 247);
    EXPECT_EQ(FYmd(L"2024 248", L"%Y %j", ios_defs::eofbit), check_date1);
    FOri(L"%Ej", L'j', L'E', ios_defs::eofbit);
    FOri(L"j",   L'j', L'E', ios_defs::strfailbit);
    FOri(L"%Oj", L'j', L'O', ios_defs::eofbit);
    FOri(L"j",   L'j', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"09", L'm',  0, ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri(L"09", L'm', L'O', ios_defs::eofbit).m_month, 9);
    EXPECT_EQ(FOri(L"九", L'm', L'O', ios_defs::eofbit).m_month, 9);
    FOri(L"%Em", L'm', L'E', ios_defs::eofbit);
    FOri(L"m",   L'm', L'E', ios_defs::strfailbit);
    FOri(L"m",   L'm', L'O', ios_defs::strfailbit);

    FOri(L"%M", L'M', 0,   ios_defs::eofbit);
    FOri(L"%OM", L'M', L'O', ios_defs::eofbit);
    FOri(L"%EM", L'M', L'E', ios_defs::eofbit);
    FOri(L"M",   L'M', L'E', ios_defs::strfailbit);
    FOri(L"M",   L'M', L'O', ios_defs::strfailbit);

    FOri(L"\n",   L'n',  0,  ios_defs::eofbit);
    FOri(L"x",    L'n',  0,  ios_defs::goodbit);
    FOri(L"\n",   L'n', L'E', ios_defs::strfailbit);
    FOri(L"%En",  L'n', L'E', ios_defs::eofbit);
    FOri(L"n",    L'n', L'O', ios_defs::strfailbit);
    FOri(L"%On",  L'n', L'O', ios_defs::eofbit);

    FOri(L"\t",   L't',  0,  ios_defs::eofbit);
    FOri(L"x",    L't',  0,  ios_defs::goodbit);
    FOri(L"\t",   L't', L'E', ios_defs::strfailbit);
    FOri(L"%Et",  L't', L'E', ios_defs::eofbit);
    FOri(L"n",    L't', L'O', ios_defs::strfailbit);
    FOri(L"%Ot",  L't', L'O', ios_defs::eofbit);

    FOri(L"%p", L'p', 0, ios_defs::eofbit);
    FOri(L"%Ep", L'p', L'E', ios_defs::eofbit);
    FOri(L"p",   L'p', L'E', ios_defs::strfailbit);
    FOri(L"%Op", L'p', L'O', ios_defs::eofbit);
    FOri(L"p",   L'p', L'O', ios_defs::strfailbit);

    FOri(L"%r", L"%r",  ios_defs::eofbit);
    FOri(L"%Er", L'r', L'E', ios_defs::eofbit);
    FOri(L"r",   L'r', L'E', ios_defs::strfailbit);
    FOri(L"%Or", L'r', L'O', ios_defs::eofbit);
    FOri(L"r",   L'r', L'O', ios_defs::strfailbit);

    EXPECT_EQ(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33", L"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(L"%ER", L'R', L'E', ios_defs::eofbit);
    FOri(L"R",   L'R', L'E', ios_defs::strfailbit);
    FOri(L"%OR", L'R', L'O', ios_defs::eofbit);
    FOri(L"R",   L'R', L'O', ios_defs::strfailbit);

    FOri(L"%S", L'S', 0,   ios_defs::eofbit);
    FOri(L"%OS", L'S', L'O', ios_defs::eofbit);
    FOri(L"%ES", L'S', L'E', ios_defs::eofbit);
    FOri(L"S",   L'S', L'E', ios_defs::strfailbit);
    FOri(L"S",   L'S', L'O', ios_defs::strfailbit);

    FOri(L"%X", L"%X",  ios_defs::eofbit);
    FOri(L"%EX", L"%EX",  ios_defs::eofbit);
    FOri(L"X",   L'X', L'E', ios_defs::strfailbit);
    FOri(L"%OX", L'X', L'O', ios_defs::eofbit);
    FOri(L"X",   L'X', L'O', ios_defs::strfailbit);

    FOri(L"%T", L"%T",  ios_defs::eofbit);
    FOri(L"%ET", L'T', L'E', ios_defs::eofbit);
    FOri(L"T",   L'T', L'E', ios_defs::strfailbit);
    FOri(L"%OT", L'T', L'O', ios_defs::eofbit);
    FOri(L"T",   L'T', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"3", L'u', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(L"3", L'u', L'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(L"三", L'u', L'O', ios_defs::eofbit).m_wday, 3);
    FOri(L"%Eu", L'u', L'E', ios_defs::eofbit);
    FOri(L"u",   L'u', L'E', ios_defs::strfailbit);
    FOri(L"u",   L'u', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"24", L'g', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri(L"%Eg", L'g', L'E', ios_defs::eofbit);
    FOri(L"g",   L'g', L'E', ios_defs::strfailbit);
    FOri(L"%Og", L'g', L'O', ios_defs::eofbit);
    FOri(L"g",   L'g', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"2024", L'G', 0, ios_defs::eofbit).m_iso_8601_year, 2024);
    FOri(L"%EG", L'G', L'E', ios_defs::eofbit);
    FOri(L"G",   L'G', L'E', ios_defs::strfailbit);
    FOri(L"%OG", L'G', L'O', ios_defs::eofbit);
    FOri(L"G",   L'G', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(L"2024 35 水", L"%Y %U %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"2024 35 水", L"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"2024 三十五 水", L"%Y %OU %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri(L"35", L'U', 0,   ios_defs::eofbit).m_week_no, 35);
    EXPECT_EQ(FOri(L"35", L'U', L'O', ios_defs::eofbit).m_week_no, 35);
    FOri(L"%EU", L'U', L'E', ios_defs::eofbit);
    FOri(L"U",   L'U', L'E', ios_defs::strfailbit);
    FOri(L"U",   L'U', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FYmd(L"2024 36 水", L"%Y %W %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"2024 36 水", L"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FYmd(L"2024 三十六 水", L"%Y %OW %a", ios_defs::eofbit), check_date1);
    EXPECT_EQ(FOri(L"36", L'W', 0,   ios_defs::eofbit).m_week_no, 36);
    EXPECT_EQ(FOri(L"36", L'W', L'O', ios_defs::eofbit).m_week_no, 36);
    FOri(L"%EW", L'W', L'E', ios_defs::eofbit);
    FOri(L"W",   L'W', L'E', ios_defs::strfailbit);
    FOri(L"W",   L'W', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"36", L'V', 0,   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri(L"36", L'V', L'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    EXPECT_EQ(FOri(L"三十六", L'V', L'O',   ios_defs::eofbit).m_iso_8601_week, 36);
    FOri(L"54",  L'V', L'O', ios_defs::strfailbit);
    FOri(L"%EV", L'V', L'E', ios_defs::eofbit);
    FOri(L"V",   L'V', L'E', ios_defs::strfailbit);
    FOri(L"V",   L'V', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"3", L'w', 0,   ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(L"3", L'w', L'O', ios_defs::eofbit).m_wday, 3);
    EXPECT_EQ(FOri(L"三", L'w', L'O', ios_defs::eofbit).m_wday, 3);
    FOri(L"%Ew", L'w', L'E', ios_defs::eofbit);
    FOri(L"w",   L'w', L'E', ios_defs::strfailbit);
    FOri(L"w",   L'w', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"24", L'y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd(L"6", L'y', L'E', ios_defs::eofbit).year(), std::chrono::year(2024));
    EXPECT_EQ(FOri(L"24", L'y', L'O', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri(L"二十四", L'y', L'O', ios_defs::eofbit).m_year, 2024);
    FOri(L"y",  L'y', L'E', ios_defs::strfailbit);
    FOri(L"y",  L'y', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"2024", L'Y', 0,   ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FOri(L"2024", L'Y', L'E', ios_defs::eofbit).m_year, 2024);
    EXPECT_EQ(FYmd(L"平成3年", L'Y', L'E', ios_defs::eofbit).year(), std::chrono::year(1991));
    FOri(L"Y",   L'Y', L'E', ios_defs::strfailbit);
    FOri(L"%OY", L'Y', L'O', ios_defs::eofbit);
    FOri(L"Y",   L'Y', L'O', ios_defs::strfailbit);

    FOri(L"%Z", L'Z', 0, ios_defs::eofbit);
    FOri(L"%EZ", L'Z', L'E', ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'E', ios_defs::strfailbit);
    FOri(L"%OZ", L'Z', L'O', ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'O', ios_defs::strfailbit);

    FOri(L"%z", L'z', 0, ios_defs::eofbit);
    FOri(L"%Ez", L'z', L'E', ios_defs::eofbit);
    FOri(L"%Oz", L'z', L'O', ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    EXPECT_EQ(FYmd(L"1999-W52-6", L"%G-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd(L"2019-W01-1", L"%G-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd(L"1999-W52-5", L"%G-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd(L"99-W52-6", L"%g-W%V-%u", ios_defs::eofbit), check_date2);
    EXPECT_EQ(FYmd(L"19-W01-1", L"%g-W%V-%u", ios_defs::eofbit), check_date3);
    EXPECT_EQ(FYmd(L"99-W52-5", L"%g-W%V-%u", ios_defs::eofbit), check_date4);

    EXPECT_EQ(FYmd(L"20 24/09/04", L"%C %y/%m/%d", ios_defs::eofbit), check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    EXPECT_EQ((FYmd(L"20 01 01", L"%C %m %d", ios_defs::eofbit)), (year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}}));
}

TEST(TimeioWchar, ATimeOfDayReadsEveryConversionSpecifierItCanSupply)
{
    timeio obj(std::make_shared<timeio_conf<wchar_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<wchar_t, false, true, tz_level::zone>, false, true, tz_level::zone>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, tz_level::zone>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(L"%",  L'%',  0,  ios_defs::eofbit);
    FOri(L"x",  L'%',  0,  ios_defs::strfailbit);
    FOri(L"%",  L'%', L'E', febit);
    FOri(L"%E%", L'%', L'E', ios_defs::eofbit);
    FOri(L"%",  L'%', L'O', febit);
    FOri(L"%O%", L'%', L'O', ios_defs::eofbit);

    FOri(L"%a", L'a', 0, ios_defs::eofbit);
    FOri(L"%Ea", L'a', L'E', ios_defs::eofbit);
    FOri(L"a",   L'a', L'E', ios_defs::strfailbit);
    FOri(L"%Oa", L'a', L'O', ios_defs::eofbit);
    FOri(L"a",   L'a', L'O', ios_defs::strfailbit);

    FOri(L"%A", L'A', 0, ios_defs::eofbit);
    FOri(L"%EA", L'A', L'E', ios_defs::eofbit);
    FOri(L"A",   L'A', L'E', ios_defs::strfailbit);
    FOri(L"%OA", L'A', L'O', ios_defs::eofbit);
    FOri(L"A",   L'A', L'O', ios_defs::strfailbit);

    FOri(L"%b", L'b', 0, ios_defs::eofbit);
    FOri(L"%Eb", L'b', L'E', ios_defs::eofbit);
    FOri(L"b",   L'b', L'E', ios_defs::strfailbit);
    FOri(L"%Ob", L'b', L'O', ios_defs::eofbit);
    FOri(L"b",   L'b', L'O', ios_defs::strfailbit);

    FOri(L"%B", L'B', 0, ios_defs::eofbit);
    FOri(L"%EB", L'B', L'E', ios_defs::eofbit);
    FOri(L"B",   L'B', L'E', ios_defs::strfailbit);
    FOri(L"%OB", L'B', L'O', ios_defs::eofbit);
    FOri(L"B",   L'B', L'O', ios_defs::strfailbit);

    FOri(L"%h", L'h', 0, ios_defs::eofbit);
    FOri(L"%Eh", L'h', L'E', ios_defs::eofbit);
    FOri(L"h",   L'h', L'E', ios_defs::strfailbit);
    FOri(L"%Oh", L'h', L'O', ios_defs::eofbit);
    FOri(L"h",   L'h', L'O', ios_defs::strfailbit);

    using namespace std::chrono;
    FOri(L"%c", L'c', 0, ios_defs::eofbit);
    FOri(L"%Ec", L'c', L'E', ios_defs::eofbit);
    FOri(L"c",   L'c', L'E', ios_defs::strfailbit);
    FOri(L"%Oc", L'c', L'O', ios_defs::eofbit);
    FOri(L"c",   L'c', L'O', ios_defs::strfailbit);

    FOri(L"%C", L'C', 0,   ios_defs::eofbit);
    FOri(L"%EC", L'C', L'E', ios_defs::eofbit);
    FOri(L"C",   L'C', L'E', ios_defs::strfailbit);
    FOri(L"%OC", L'C', L'O', ios_defs::eofbit);
    FOri(L"C",   L'C', L'O', ios_defs::strfailbit);

    FOri(L"%d", L'd', 0,   ios_defs::eofbit);
    FOri(L"%Od", L'd', L'O', ios_defs::eofbit);
    FOri(L"%Ed", L'd', L'E', ios_defs::eofbit);
    FOri(L"d",   L'd', L'E', ios_defs::strfailbit);
    FOri(L"d",   L'd', L'O', ios_defs::strfailbit);

    FOri(L"%e", L'e', 0,   ios_defs::eofbit);
    FOri(L"%Oe", L'e', L'O', ios_defs::eofbit);
    FOri(L"%Ee", L'e', L'E', ios_defs::eofbit);
    FOri(L"e",   L'e', L'E', ios_defs::strfailbit);
    FOri(L"e",   L'e', L'O', ios_defs::strfailbit);

    FOri(L"%F", L'F', 0, ios_defs::eofbit);
    FOri(L"%EF", L'F', L'E', ios_defs::eofbit);
    FOri(L"F",   L'F', L'E', ios_defs::strfailbit);
    FOri(L"%OF", L'F', L'O', ios_defs::eofbit);
    FOri(L"F",   L'F', L'O', ios_defs::strfailbit);

    FOri(L"%x", L'x', 0, ios_defs::eofbit);
    FOri(L"%Ex", L'x', L'E', ios_defs::eofbit);
    FOri(L"x",   L'x', L'E', ios_defs::strfailbit);
    FOri(L"%Ox", L'x', L'O', ios_defs::eofbit);
    FOri(L"x",   L'x', L'O', ios_defs::strfailbit);

    FOri(L"%D", L'D', 0, ios_defs::eofbit);
    FOri(L"%ED", L'D', L'E', ios_defs::eofbit);
    FOri(L"D",   L'D', L'E', ios_defs::strfailbit);
    FOri(L"%OD", L'D', L'O', ios_defs::eofbit);
    FOri(L"D",   L'D', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"13", L'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(L"13", L'H', L'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(L"十三", L'H', L'O', ios_defs::eofbit).m_hour, 13);
    FOri(L"%EH", L'H', L'E', ios_defs::eofbit);
    FOri(L"H",   L'H', L'E', ios_defs::strfailbit);
    FOri(L"H",   L'H', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"01", L'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(L"01", L'I', L'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(L"一", L'I', L'O', ios_defs::eofbit).m_hour, 1);
    FOri(L"%EI", L'I', L'E', ios_defs::eofbit);
    FOri(L"I",   L'I', L'E', ios_defs::strfailbit);
    FOri(L"I",   L'I', L'O', ios_defs::strfailbit);

    FOri(L"%j", L'j', 0, ios_defs::eofbit);
    FOri(L"%Ej", L'j', L'E', ios_defs::eofbit);
    FOri(L"j",   L'j', L'E', ios_defs::strfailbit);
    FOri(L"%Oj", L'j', L'O', ios_defs::eofbit);
    FOri(L"j",   L'j', L'O', ios_defs::strfailbit);

    FOri(L"%m", L'm',  0, ios_defs::eofbit);
    FOri(L"%Om", L'm', L'O', ios_defs::eofbit);
    FOri(L"%Em", L'm', L'E', ios_defs::eofbit);
    FOri(L"m",   L'm', L'E', ios_defs::strfailbit);
    FOri(L"m",   L'm', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"33", L'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(L"33", L'M', L'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(L"三十三", L'M', L'O', ios_defs::eofbit).m_minute, 33);
    FOri(L"%EM", L'M', L'E', ios_defs::eofbit);
    FOri(L"M",   L'M', L'E', ios_defs::strfailbit);
    FOri(L"M",   L'M', L'O', ios_defs::strfailbit);

    FOri(L"\n",   L'n',  0,  ios_defs::eofbit);
    FOri(L"x",    L'n',  0,  ios_defs::goodbit);
    FOri(L"\n",   L'n', L'E', ios_defs::strfailbit);
    FOri(L"%En",  L'n', L'E', ios_defs::eofbit);
    FOri(L"n",    L'n', L'O', ios_defs::strfailbit);
    FOri(L"%On",  L'n', L'O', ios_defs::eofbit);

    FOri(L"\t",   L't',  0,  ios_defs::eofbit);
    FOri(L"x",    L't',  0,  ios_defs::goodbit);
    FOri(L"\t",   L't', L'E', ios_defs::strfailbit);
    FOri(L"%Et",  L't', L'E', ios_defs::eofbit);
    FOri(L"n",    L't', L'O', ios_defs::strfailbit);
    FOri(L"%Ot",  L't', L'O', ios_defs::eofbit);

    EXPECT_EQ(FHms(L"01 午後", L"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(FHms(L"01 午前", L"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(FOri(L"午後", L'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(FOri(L"午前", L'p', 0, ios_defs::eofbit).m_is_pm, false);
    FOri(L"%Ep", L'p', L'E', ios_defs::eofbit);
    FOri(L"p",   L'p', L'E', ios_defs::strfailbit);
    FOri(L"%Op", L'p', L'O', ios_defs::eofbit);
    FOri(L"p",   L'p', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(L"午後01時33分18秒", L"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"%Er", L'r', L'E', ios_defs::eofbit);
    FOri(L"r",   L'r', L'E', ios_defs::strfailbit);
    FOri(L"%Or", L'r', L'O', ios_defs::eofbit);
    FOri(L"r",   L'r', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(L"13:33", L"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(L"%ER", L'R', L'E', ios_defs::eofbit);
    FOri(L"R",   L'R', L'E', ios_defs::strfailbit);
    FOri(L"%OR", L'R', L'O', ios_defs::eofbit);
    FOri(L"R",   L'R', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"18", L'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(L"18", L'S', L'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(L"十八", L'S', L'O', ios_defs::eofbit).m_second, 18);
    FOri(L"%ES", L'S', L'E', ios_defs::eofbit);
    FOri(L"S",   L'S', L'E', ios_defs::strfailbit);
    FOri(L"S",   L'S', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(L"13時33分18秒", L"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(FHms(L"13時33分18秒", L"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"X",   L'X', L'E', ios_defs::strfailbit);
    FOri(L"%OX", L'X', L'O', ios_defs::eofbit);
    FOri(L"X",   L'X', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(L"13:33:18", L"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"%ET", L'T', L'E', ios_defs::eofbit);
    FOri(L"T",   L'T', L'E', ios_defs::strfailbit);
    FOri(L"%OT", L'T', L'O', ios_defs::eofbit);
    FOri(L"T",   L'T', L'O', ios_defs::strfailbit);

    FOri(L"%u", L'u', 0,   ios_defs::eofbit);
    FOri(L"%Ou", L'u', L'O', ios_defs::eofbit);
    FOri(L"%Eu", L'u', L'E', ios_defs::eofbit);
    FOri(L"u",   L'u', L'E', ios_defs::strfailbit);
    FOri(L"u",   L'u', L'O', ios_defs::strfailbit);

    FOri(L"%g", L'g', 0, ios_defs::eofbit);
    FOri(L"%Eg", L'g', L'E', ios_defs::eofbit);
    FOri(L"g",   L'g', L'E', ios_defs::strfailbit);
    FOri(L"%Og", L'g', L'O', ios_defs::eofbit);
    FOri(L"g",   L'g', L'O', ios_defs::strfailbit);

    FOri(L"%G", L'G', 0, ios_defs::eofbit);
    FOri(L"%EG", L'G', L'E', ios_defs::eofbit);
    FOri(L"G",   L'G', L'E', ios_defs::strfailbit);
    FOri(L"%OG", L'G', L'O', ios_defs::eofbit);
    FOri(L"G",   L'G', L'O', ios_defs::strfailbit);

    FOri(L"%U", L'U', 0,   ios_defs::eofbit);
    FOri(L"%OU", L'U', L'O', ios_defs::eofbit);
    FOri(L"%EU", L'U', L'E', ios_defs::eofbit);
    FOri(L"U",   L'U', L'E', ios_defs::strfailbit);
    FOri(L"U",   L'U', L'O', ios_defs::strfailbit);

    FOri(L"%W", L'W', 0,   ios_defs::eofbit);
    FOri(L"%OW", L'W', L'O', ios_defs::eofbit);
    FOri(L"%EW", L'W', L'E', ios_defs::eofbit);
    FOri(L"W",   L'W', L'E', ios_defs::strfailbit);
    FOri(L"W",   L'W', L'O', ios_defs::strfailbit);

    FOri(L"%V", L'V', 0,   ios_defs::eofbit);
    FOri(L"%OV", L'V', L'O',   ios_defs::eofbit);
    FOri(L"54",  L'V', L'O', ios_defs::strfailbit);
    FOri(L"%EV", L'V', L'E', ios_defs::eofbit);
    FOri(L"V",   L'V', L'E', ios_defs::strfailbit);
    FOri(L"V",   L'V', L'O', ios_defs::strfailbit);

    FOri(L"%w", L'w', 0,   ios_defs::eofbit);
    FOri(L"%Ow", L'w', L'O', ios_defs::eofbit);
    FOri(L"%Ew", L'w', L'E', ios_defs::eofbit);
    FOri(L"w",   L'w', L'E', ios_defs::strfailbit);
    FOri(L"w",   L'w', L'O', ios_defs::strfailbit);

    FOri(L"%y", L'y', 0,   ios_defs::eofbit);
    FOri(L"%Ey", L'y', L'E', ios_defs::eofbit);
    FOri(L"%Oy", L'y', L'O', ios_defs::eofbit);
    FOri(L"y",  L'y', L'E', ios_defs::strfailbit);
    FOri(L"y",  L'y', L'O', ios_defs::strfailbit);

    FOri(L"%Y", L'Y', 0,   ios_defs::eofbit);
    FOri(L"%EY", L'Y', L'E', ios_defs::eofbit);
    FOri(L"Y",   L'Y', L'E', ios_defs::strfailbit);
    FOri(L"%OY", L'Y', L'O', ios_defs::eofbit);
    FOri(L"Y",   L'Y', L'O', ios_defs::strfailbit);

    EXPECT_TRUE(zone_is(FOri(L"America/Los_Angeles", L'Z', 0, ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = FOri(L"PST", L'Z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    FOri(L"America/Los_Angexes", L'Z', 0, ios_defs::strfailbit);
    FOri(L"%EZ", L'Z', L'E', ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'E', ios_defs::strfailbit);
    FOri(L"%OZ", L'Z', L'O', ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'O', ios_defs::strfailbit);

    { auto r = FOri(L"+0800", L'z', 0, ios_defs::eofbit); EXPECT_TRUE(r.m_have_offset && r.m_offset == minutes{480}); }
    FOri(L"%z", L'z', 0, ios_defs::strfailbit);
    FOri(L"%Ez", L'z', L'E', ios_defs::eofbit);
    FOri(L"z",  L'z', L'E', ios_defs::strfailbit);
    FOri(L"%Oz", L'z', L'O', ios_defs::eofbit);
    FOri(L"z",  L'z', L'O', ios_defs::strfailbit);
}

TEST(TimeioWchar, ATimeOfDayReadsTheSameSpecifiersWithNoZoneTier)
{
    timeio obj(std::make_shared<timeio_conf<wchar_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<time_parse_context<wchar_t, false, true, tz_level::none>, false, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(L"%",  L'%',  0,  ios_defs::eofbit);
    FOri(L"x",  L'%',  0,  ios_defs::strfailbit);
    FOri(L"%",  L'%', L'E', febit);
    FOri(L"%E%", L'%', L'E', ios_defs::eofbit);
    FOri(L"%",  L'%', L'O', febit);
    FOri(L"%O%", L'%', L'O', ios_defs::eofbit);

    FOri(L"%a", L'a', 0, ios_defs::eofbit);
    FOri(L"%Ea", L'a', L'E', ios_defs::eofbit);
    FOri(L"a",   L'a', L'E', ios_defs::strfailbit);
    FOri(L"%Oa", L'a', L'O', ios_defs::eofbit);
    FOri(L"a",   L'a', L'O', ios_defs::strfailbit);

    FOri(L"%A", L'A', 0, ios_defs::eofbit);
    FOri(L"%EA", L'A', L'E', ios_defs::eofbit);
    FOri(L"A",   L'A', L'E', ios_defs::strfailbit);
    FOri(L"%OA", L'A', L'O', ios_defs::eofbit);
    FOri(L"A",   L'A', L'O', ios_defs::strfailbit);

    FOri(L"%b", L'b', 0, ios_defs::eofbit);
    FOri(L"%Eb", L'b', L'E', ios_defs::eofbit);
    FOri(L"b",   L'b', L'E', ios_defs::strfailbit);
    FOri(L"%Ob", L'b', L'O', ios_defs::eofbit);
    FOri(L"b",   L'b', L'O', ios_defs::strfailbit);

    FOri(L"%B", L'B', 0, ios_defs::eofbit);
    FOri(L"%EB", L'B', L'E', ios_defs::eofbit);
    FOri(L"B",   L'B', L'E', ios_defs::strfailbit);
    FOri(L"%OB", L'B', L'O', ios_defs::eofbit);
    FOri(L"B",   L'B', L'O', ios_defs::strfailbit);

    FOri(L"%h", L'h', 0, ios_defs::eofbit);
    FOri(L"%Eh", L'h', L'E', ios_defs::eofbit);
    FOri(L"h",   L'h', L'E', ios_defs::strfailbit);
    FOri(L"%Oh", L'h', L'O', ios_defs::eofbit);
    FOri(L"h",   L'h', L'O', ios_defs::strfailbit);

    using namespace std::chrono;
    FOri(L"%c", L'c', 0, ios_defs::eofbit);
    FOri(L"%Ec", L'c', L'E', ios_defs::eofbit);
    FOri(L"c",   L'c', L'E', ios_defs::strfailbit);
    FOri(L"%Oc", L'c', L'O', ios_defs::eofbit);
    FOri(L"c",   L'c', L'O', ios_defs::strfailbit);

    FOri(L"%C", L'C', 0,   ios_defs::eofbit);
    FOri(L"%EC", L'C', L'E', ios_defs::eofbit);
    FOri(L"C",   L'C', L'E', ios_defs::strfailbit);
    FOri(L"%OC", L'C', L'O', ios_defs::eofbit);
    FOri(L"C",   L'C', L'O', ios_defs::strfailbit);

    FOri(L"%d", L'd', 0,   ios_defs::eofbit);
    FOri(L"%Od", L'd', L'O', ios_defs::eofbit);
    FOri(L"%Ed", L'd', L'E', ios_defs::eofbit);
    FOri(L"d",   L'd', L'E', ios_defs::strfailbit);
    FOri(L"d",   L'd', L'O', ios_defs::strfailbit);

    FOri(L"%e", L'e', 0,   ios_defs::eofbit);
    FOri(L"%Oe", L'e', L'O', ios_defs::eofbit);
    FOri(L"%Ee", L'e', L'E', ios_defs::eofbit);
    FOri(L"e",   L'e', L'E', ios_defs::strfailbit);
    FOri(L"e",   L'e', L'O', ios_defs::strfailbit);

    FOri(L"%F", L'F', 0, ios_defs::eofbit);
    FOri(L"%EF", L'F', L'E', ios_defs::eofbit);
    FOri(L"F",   L'F', L'E', ios_defs::strfailbit);
    FOri(L"%OF", L'F', L'O', ios_defs::eofbit);
    FOri(L"F",   L'F', L'O', ios_defs::strfailbit);

    FOri(L"%x", L'x', 0, ios_defs::eofbit);
    FOri(L"%Ex", L'x', L'E', ios_defs::eofbit);
    FOri(L"x",   L'x', L'E', ios_defs::strfailbit);
    FOri(L"%Ox", L'x', L'O', ios_defs::eofbit);
    FOri(L"x",   L'x', L'O', ios_defs::strfailbit);

    FOri(L"%D", L'D', 0, ios_defs::eofbit);
    FOri(L"%ED", L'D', L'E', ios_defs::eofbit);
    FOri(L"D",   L'D', L'E', ios_defs::strfailbit);
    FOri(L"%OD", L'D', L'O', ios_defs::eofbit);
    FOri(L"D",   L'D', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"13", L'H', 0,   ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(L"13", L'H', L'O', ios_defs::eofbit).m_hour, 13);
    EXPECT_EQ(FOri(L"十三", L'H', L'O', ios_defs::eofbit).m_hour, 13);
    FOri(L"%EH", L'H', L'E', ios_defs::eofbit);
    FOri(L"H",   L'H', L'E', ios_defs::strfailbit);
    FOri(L"H",   L'H', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"01", L'I', 0,   ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(L"01", L'I', L'O', ios_defs::eofbit).m_hour, 1);
    EXPECT_EQ(FOri(L"一", L'I', L'O', ios_defs::eofbit).m_hour, 1);
    FOri(L"%EI", L'I', L'E', ios_defs::eofbit);
    FOri(L"I",   L'I', L'E', ios_defs::strfailbit);
    FOri(L"I",   L'I', L'O', ios_defs::strfailbit);

    FOri(L"%j", L'j', 0, ios_defs::eofbit);
    FOri(L"%Ej", L'j', L'E', ios_defs::eofbit);
    FOri(L"j",   L'j', L'E', ios_defs::strfailbit);
    FOri(L"%Oj", L'j', L'O', ios_defs::eofbit);
    FOri(L"j",   L'j', L'O', ios_defs::strfailbit);

    FOri(L"%m", L'm',  0, ios_defs::eofbit);
    FOri(L"%Om", L'm', L'O', ios_defs::eofbit);
    FOri(L"%Em", L'm', L'E', ios_defs::eofbit);
    FOri(L"m",   L'm', L'E', ios_defs::strfailbit);
    FOri(L"m",   L'm', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"33", L'M', 0,   ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(L"33", L'M', L'O', ios_defs::eofbit).m_minute, 33);
    EXPECT_EQ(FOri(L"三十三", L'M', L'O', ios_defs::eofbit).m_minute, 33);
    FOri(L"%EM", L'M', L'E', ios_defs::eofbit);
    FOri(L"M",   L'M', L'E', ios_defs::strfailbit);
    FOri(L"M",   L'M', L'O', ios_defs::strfailbit);

    FOri(L"\n",   L'n',  0,  ios_defs::eofbit);
    FOri(L"x",    L'n',  0,  ios_defs::goodbit);
    FOri(L"\n",   L'n', L'E', ios_defs::strfailbit);
    FOri(L"%En",  L'n', L'E', ios_defs::eofbit);
    FOri(L"n",    L'n', L'O', ios_defs::strfailbit);
    FOri(L"%On",  L'n', L'O', ios_defs::eofbit);

    FOri(L"\t",   L't',  0,  ios_defs::eofbit);
    FOri(L"x",    L't',  0,  ios_defs::goodbit);
    FOri(L"\t",   L't', L'E', ios_defs::strfailbit);
    FOri(L"%Et",  L't', L'E', ios_defs::eofbit);
    FOri(L"n",    L't', L'O', ios_defs::strfailbit);
    FOri(L"%Ot",  L't', L'O', ios_defs::eofbit);

    EXPECT_EQ(FHms(L"01 午後", L"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(13));
    EXPECT_EQ(FHms(L"01 午前", L"%I %p",  ios_defs::eofbit).hours(), std::chrono::hours(1));
    EXPECT_EQ(FOri(L"午後", L'p', 0, ios_defs::eofbit).m_is_pm, true);
    EXPECT_EQ(FOri(L"午前", L'p', 0, ios_defs::eofbit).m_is_pm, false);
    FOri(L"%Ep", L'p', L'E', ios_defs::eofbit);
    FOri(L"p",   L'p', L'E', ios_defs::strfailbit);
    FOri(L"%Op", L'p', L'O', ios_defs::eofbit);
    FOri(L"p",   L'p', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(L"午後01時33分18秒", L"%r",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"%Er", L'r', L'E', ios_defs::eofbit);
    FOri(L"r",   L'r', L'E', ios_defs::strfailbit);
    FOri(L"%Or", L'r', L'O', ios_defs::eofbit);
    FOri(L"r",   L'r', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(L"13:33", L"%R",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(L"%ER", L'R', L'E', ios_defs::eofbit);
    FOri(L"R",   L'R', L'E', ios_defs::strfailbit);
    FOri(L"%OR", L'R', L'O', ios_defs::eofbit);
    FOri(L"R",   L'R', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FOri(L"18", L'S', 0,   ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(L"18", L'S', L'O', ios_defs::eofbit).m_second, 18);
    EXPECT_EQ(FOri(L"十八", L'S', L'O', ios_defs::eofbit).m_second, 18);
    FOri(L"%ES", L'S', L'E', ios_defs::eofbit);
    FOri(L"S",   L'S', L'E', ios_defs::strfailbit);
    FOri(L"S",   L'S', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(L"13時33分18秒", L"%X",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    EXPECT_EQ(FHms(L"13時33分18秒", L"%EX",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"X",   L'X', L'E', ios_defs::strfailbit);
    FOri(L"%OX", L'X', L'O', ios_defs::eofbit);
    FOri(L"X",   L'X', L'O', ios_defs::strfailbit);

    EXPECT_EQ(FHms(L"13:33:18", L"%T",  ios_defs::eofbit).to_duration(), std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"%ET", L'T', L'E', ios_defs::eofbit);
    FOri(L"T",   L'T', L'E', ios_defs::strfailbit);
    FOri(L"%OT", L'T', L'O', ios_defs::eofbit);
    FOri(L"T",   L'T', L'O', ios_defs::strfailbit);

    FOri(L"%u", L'u', 0,   ios_defs::eofbit);
    FOri(L"%Ou", L'u', L'O', ios_defs::eofbit);
    FOri(L"%Eu", L'u', L'E', ios_defs::eofbit);
    FOri(L"u",   L'u', L'E', ios_defs::strfailbit);
    FOri(L"u",   L'u', L'O', ios_defs::strfailbit);

    FOri(L"%g", L'g', 0, ios_defs::eofbit);
    FOri(L"%Eg", L'g', L'E', ios_defs::eofbit);
    FOri(L"g",   L'g', L'E', ios_defs::strfailbit);
    FOri(L"%Og", L'g', L'O', ios_defs::eofbit);
    FOri(L"g",   L'g', L'O', ios_defs::strfailbit);

    FOri(L"%G", L'G', 0, ios_defs::eofbit);
    FOri(L"%EG", L'G', L'E', ios_defs::eofbit);
    FOri(L"G",   L'G', L'E', ios_defs::strfailbit);
    FOri(L"%OG", L'G', L'O', ios_defs::eofbit);
    FOri(L"G",   L'G', L'O', ios_defs::strfailbit);

    FOri(L"%U", L'U', 0,   ios_defs::eofbit);
    FOri(L"%OU", L'U', L'O', ios_defs::eofbit);
    FOri(L"%EU", L'U', L'E', ios_defs::eofbit);
    FOri(L"U",   L'U', L'E', ios_defs::strfailbit);
    FOri(L"U",   L'U', L'O', ios_defs::strfailbit);

    FOri(L"%W", L'W', 0,   ios_defs::eofbit);
    FOri(L"%OW", L'W', L'O', ios_defs::eofbit);
    FOri(L"%EW", L'W', L'E', ios_defs::eofbit);
    FOri(L"W",   L'W', L'E', ios_defs::strfailbit);
    FOri(L"W",   L'W', L'O', ios_defs::strfailbit);

    FOri(L"%V", L'V', 0,   ios_defs::eofbit);
    FOri(L"%OV", L'V', L'O',   ios_defs::eofbit);
    FOri(L"54",  L'V', L'O', ios_defs::strfailbit);
    FOri(L"%EV", L'V', L'E', ios_defs::eofbit);
    FOri(L"V",   L'V', L'E', ios_defs::strfailbit);
    FOri(L"V",   L'V', L'O', ios_defs::strfailbit);

    FOri(L"%w", L'w', 0,   ios_defs::eofbit);
    FOri(L"%Ow", L'w', L'O', ios_defs::eofbit);
    FOri(L"%Ew", L'w', L'E', ios_defs::eofbit);
    FOri(L"w",   L'w', L'E', ios_defs::strfailbit);
    FOri(L"w",   L'w', L'O', ios_defs::strfailbit);

    FOri(L"%y", L'y', 0,   ios_defs::eofbit);
    FOri(L"%Ey", L'y', L'E', ios_defs::eofbit);
    FOri(L"%Oy", L'y', L'O', ios_defs::eofbit);
    FOri(L"y",  L'y', L'E', ios_defs::strfailbit);
    FOri(L"y",  L'y', L'O', ios_defs::strfailbit);

    FOri(L"%Y", L'Y', 0,   ios_defs::eofbit);
    FOri(L"%EY", L'Y', L'E', ios_defs::eofbit);
    FOri(L"Y",   L'Y', L'E', ios_defs::strfailbit);
    FOri(L"%OY", L'Y', L'O', ios_defs::eofbit);
    FOri(L"Y",   L'Y', L'O', ios_defs::strfailbit);

    FOri(L"%Z", L'Z', 0, ios_defs::eofbit);
    FOri(L"%EZ", L'Z', L'E', ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'E', ios_defs::strfailbit);
    FOri(L"%OZ", L'Z', L'O', ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'O', ios_defs::strfailbit);

    FOri(L"%z", L'z', 0, ios_defs::eofbit);
    FOri(L"%Ez", L'z', L'E', ios_defs::eofbit);
    FOri(L"z",  L'z', L'E', ios_defs::strfailbit);
    FOri(L"%Oz", L'z', L'O', ios_defs::eofbit);
    FOri(L"z",  L'z', L'O', ios_defs::strfailbit);
}

TEST(TimeioWchar, AValueThatIsNotAValidTimeIsRejected)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<wchar_t>>("C"));
    std::wstring res;

    // put(year_month_day) with invalid date (line 1173)
    {
        auto invalid_ymd = year_month_day{year{2024}, month{2}, day{30}};
        EXPECT_THROW(obj.put(std::back_inserter(res), invalid_ymd, std::wstring_view(L"%F")), stream_error);
    }

    // put(hh_mm_ss) with negative total duration (line 1214)
    {
        hh_mm_ss<seconds> invalid_hms{seconds{-1}};
        EXPECT_THROW(obj.put(std::back_inserter(res), invalid_hms, std::wstring_view(L"%T")), stream_error);
    }

    // put(std::tm) with out-of-range field: month=-1 (line 1271)
    {
        std::tm bad_tm{};
        bad_tm.tm_year = 124; bad_tm.tm_mon = -1;
        bad_tm.tm_mday = 1; bad_tm.tm_hour = 0; bad_tm.tm_min = 0; bad_tm.tm_sec = 0;
        EXPECT_THROW(obj.put(std::back_inserter(res), bad_tm, std::wstring_view(L"%F")), stream_error);
    }

    // put(std::tm) with valid fields but invalid date: Feb 30 (line 1275)
    {
        std::tm bad_tm{};
        bad_tm.tm_year = 124; bad_tm.tm_mon = 1; bad_tm.tm_mday = 30;
        bad_tm.tm_hour = 0; bad_tm.tm_min = 0; bad_tm.tm_sec = 0;
        EXPECT_THROW(obj.put(std::back_inserter(res), bad_tm, std::wstring_view(L"%F")), stream_error);
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
            EXPECT_THROW(obj.put(std::back_inserter(res), bad_tm, std::wstring_view(L"%Y")), stream_error);
        }
    }

    // put(std::tm) at the year bounds themselves: still accepted
    {
        std::tm edge_tm{};
        edge_tm.tm_mon = 0; edge_tm.tm_mday = 1;
        edge_tm.tm_hour = 0; edge_tm.tm_min = 0; edge_tm.tm_sec = 0;

        edge_tm.tm_year = static_cast<int>(year::max()) - 1900;
        res.clear(); obj.put(std::back_inserter(res), edge_tm, std::wstring_view(L"%Y"));
        EXPECT_EQ(res, L"32767");

        edge_tm.tm_year = static_cast<int>(year::min()) - 1900;
        res.clear(); obj.put(std::back_inserter(res), edge_tm, std::wstring_view(L"%Y"));
        EXPECT_EQ(res, L"-32767");
    }

    // put(year_month_day) with negative year: %Y and %C output sign (lines 2860-2861, 2543-2544)
    {
        auto neg_ymd = year_month_day{year{-1}, month{1}, day{1}};
        res.clear(); obj.put(std::back_inserter(res), neg_ymd, std::wstring_view(L"%Y"));
        EXPECT_EQ(res, L"-0001");
        res.clear(); obj.put(std::back_inserter(res), neg_ymd, std::wstring_view(L"%C"));
        EXPECT_EQ(res, L"-01");
    }

    // put(year_month_day) for date in ISO year -1: %G output sign (lines 2608-2609)
    // Jan 1, year 0 is a Saturday; Thu of that ISO week is Dec 30, year -1 -> G=-0001
    {
        auto early_ymd = year_month_day{year{0}, month{1}, day{1}};
        res.clear(); obj.put(std::back_inserter(res), early_ymd, std::wstring_view(L"%G"));
        EXPECT_EQ(res, L"-0001");
    }

    // put(zoned_time) with positive offset: %z outputs '+' (line 2883)
    {
        auto tp = create_zoned_time(2024, 9, 4, 12, 0, 0, "Asia/Tokyo");
        res.clear(); obj.put(std::back_inserter(res), tp, std::wstring_view(L"%z"));
        EXPECT_EQ(res, L"+0900");
    }
}

// A format string ending in a lone '%' -- or in a lone '%E' / '%O' modifier -- introduces no
// specifier, so there is nothing to convert. It follows the same rule this facet already uses
// for a specifier it does not recognize (see the "unknown format" path, which emits '%' plus
// the rest verbatim): put writes the '%' out and get matches it back as a literal. Handling
// the two sides alike is what keeps the round-trip invariant -- whatever put writes, get reads
// back with the same format string. put previously dropped the '%' silently while get rejected
// it, so put succeeded on output get could never read.
TEST(TimeioWchar, ALoneOrUnknownSpecifierIsEchoedVerbatim)
{
    timeio obj(std::make_shared<timeio_conf<wchar_t>>("C"));
    const std::tm t = calendar_time(124, 0, 15, 1, 2, 3, 1, 14, 0);

    struct { const wchar_t* fmt; const wchar_t* want; } cases[] = {
        {L"%Y%", L"2024%"},   // a lone L'%' after a real specifier
        {L"%",   L"%"},       // nothing but the lone L'%'
        {L"a%",  L"a%"},      // a lone L'%' after literal text
        {L"%E",  L"%E"},      // a lone L'E' modifier with no specifier to modify
        {L"%O",  L"%O"},      // ditto for L'O'
        {L"%%",  L"%"},       // control: an escaped L'%' still collapses to one
        {L"%Q",  L"%Q"},      // control: an unrecognized specifier is already emitted verbatim
    };

    for (const auto& c : cases)
    {
        std::wstring res;
        obj.put(std::back_inserter(res), t, std::wstring_view(c.fmt));
        EXPECT_EQ(res, c.want);

        // The round trip: get consumes exactly what put produced, using the same format.
        time_parse_context<wchar_t> ctx;
        EXPECT_EQ(obj.get(res.begin(), res.end(), ctx, std::wstring_view(c.fmt)), res.end());
    }

    // get still rejects input that lacks the literal '%' the format asks for, so the
    // agreement above is a real match rather than the trailing '%' being ignored.
    {
        const std::wstring in = L"2024";
        time_parse_context<wchar_t> ctx;
        EXPECT_THROW(obj.get(in.begin(), in.end(), ctx, std::wstring_view(L"%Y%")), stream_error);
    }
}

// The two tiers pinned apart. Whether %Z parses is the tier's decision and nothing else's:
// tz_level::offset matches it literally, which is exactly what put degrades it to for a value
// with no zone to name, and tz_level::zone parses it against the trie. Neither tier looks at
// what the trie happens to contain to decide which of the two it is doing.
TEST(TimeioWchar, TheZoneTierDecidesHowAZoneNameIsParsed)
{
    using namespace std::chrono;

    timeio obj(std::make_shared<timeio_conf<wchar_t>>("C"));

    auto off_ok = [&obj](const std::wstring& in, const wchar_t* fmt)
    {
        time_parse_context<wchar_t, true, true, tz_level::offset> ctx;
        try { return obj.get(in.begin(), in.end(), ctx, std::wstring_view(fmt)) == in.end(); }
        catch (stream_error&) { return false; }
    };
    auto zone_ok = [&obj](const std::wstring& in, const wchar_t* fmt)
    {
        time_parse_context<wchar_t, true, true, tz_level::zone> ctx;
        try { return obj.get(in.begin(), in.end(), ctx, std::wstring_view(fmt)) == in.end(); }
        catch (stream_error&) { return false; }
    };

    // The literal %Z, which is what put writes when the value has no zone to offer.
    EXPECT_TRUE(off_ok(L"%Z", L"%Z"));
    EXPECT_FALSE(zone_ok(L"%Z", L"%Z"));

    // A real zone token parses at tz_level::zone and only there. At tz_level::offset the format
    // is asking for the two characters %Z, which "UTC" is not -- put never writes a zone token
    // for a value that parses at that tier, so there is nothing to read back.
    EXPECT_TRUE(zone_ok(L"UTC", L"%Z"));
    EXPECT_FALSE(off_ok(L"UTC", L"%Z"));
    EXPECT_TRUE(zone_ok(L"PDT", L"%Z"));
    EXPECT_FALSE(off_ok(L"PDT", L"%Z"));

    // A run of letters the database does not know is rejected at both, for different reasons:
    // no trie entry at one tier, no literal match at the other.
    EXPECT_FALSE(zone_ok(L"XYZ", L"%Z"));
    EXPECT_FALSE(off_ok(L"XYZ", L"%Z"));

    // The literal is for *this* specifier, not for any percent sequence.
    EXPECT_FALSE(off_ok(L"%z", L"%Z"));
    EXPECT_FALSE(off_ok(L"%Q", L"%Z"));

    // The round trip it exists for: a std::tm with no zone, through a format carrying %Z. Each
    // platform reads it back at the tier its own std::tm sits at. With tm_zone the field exists
    // but names nothing, so put writes the unknown-zone token and the zone tier reads it back;
    // without the extension members the type has no zone at all, put degrades %Z to a literal,
    // and the tiers below zone match that literal. Either way it closes.
    {
        std::tm t{};
        t.tm_year = 124; t.tm_mon = 8; t.tm_mday = 4;
        t.tm_hour = 13; t.tm_min = 33; t.tm_sec = 18;

        std::wstring res;
        obj.put(std::back_inserter(res), t, std::wstring_view(L"%F %T %Z"));
#ifdef __USE_MISC
        EXPECT_EQ(res, L"2024-09-04 13:33:18 UNKNOWN");
        EXPECT_TRUE(zone_ok(res, L"%F %T %Z"));
#else
        EXPECT_EQ(res, L"2024-09-04 13:33:18 %Z");
        EXPECT_TRUE(off_ok(res, L"%F %T %Z"));
#endif
    }

    // The same round trip through a locale whose own %c carries %Z, which is how this reaches
    // a caller who never wrote %Z: put_time(&t, "%c") on a tm that get_time filled in.
    {
        timeio us(std::make_shared<timeio_conf<wchar_t>>("en_US.UTF-8"));
        std::tm t{};
        t.tm_year = 124; t.tm_mon = 8; t.tm_mday = 4;
        t.tm_hour = 13; t.tm_min = 33; t.tm_sec = 18;

        std::wstring res;
        us.put(std::back_inserter(res), t, std::wstring_view(L"%c"));

        time_parse_context<wchar_t, true, true, tz_level::zone> ctx;
        EXPECT_EQ(us.get(res.begin(), res.end(), ctx, std::wstring_view(L"%c")), res.end());
    }
}
