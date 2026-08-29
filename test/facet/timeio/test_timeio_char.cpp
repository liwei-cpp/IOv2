#include <chrono>
#include <type_traits>
#include <functional>
#include <limits>
#include <list>
#include <facet/timeio.h>
#include <io/streambuf_iterator.h>

#include <support/dump_info.h>
#include <support/verify.h>
namespace
{
    // m_zone_name / m_zone_abbrev point into the time-zone trie rather than owning a string,
    // so a null pointer -- not an empty one -- is what "the field was not parsed" looks like.
    bool zone_is(const char* p, std::string_view s) { return p != nullptr && std::string_view{p} == s; }

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

    std::tm test_tm(int sec, int min, int hour, int mday, int mon, int year, int wday, int yday, int isdst)
    {
        static std::tm tmp;
        tmp.tm_sec = sec;
        tmp.tm_min = min;
        tmp.tm_hour = hour;
        tmp.tm_mday = mday;
        tmp.tm_mon = mon;
        tmp.tm_year = year;
        tmp.tm_wday = wday;
        tmp.tm_yday = yday;
        tmp.tm_isdst = isdst;
        return tmp;
    }

    auto create_zoned_time(int y, unsigned m, unsigned d, int h, int min, int s, const std::string& tz)
    {
        using namespace std::chrono;

        local_time<seconds> lt = local_days{year{y}/month{m}/day{d}} + hours{h} + minutes{min} + seconds{s};

        // Combine into a zoned_time
        return zoned_time{locate_zone(tz), lt};
    }
}

void test_timeio_char_put_1()
{
    dump_info("Test timeio<char> put 1...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    auto tp = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    std::string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, '%');
        VERIFY(res == "%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'a');
        VERIFY(res == "Wed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'E');
        VERIFY(res == "%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'O');
        VERIFY(res == "%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'A');
        VERIFY(res == "Wednesday");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'E');
        VERIFY(res == "%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'O');
        VERIFY(res == "%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'b');
        VERIFY(res == "Sep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'E');
        VERIFY(res == "%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'O');
        VERIFY(res == "%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'h');
        VERIFY(res == "Sep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'E');
        VERIFY(res == "%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'O');
        VERIFY(res == "%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'B');
        VERIFY(res == "September");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'E');
        VERIFY(res == "%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'O');
        VERIFY(res == "%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'c');
        VERIFY(res == "Wed Sep  4 13:33:18 2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        VERIFY(res == "Wed Sep  4 13:33:18 2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'O');
        VERIFY(res == "%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'C');
        VERIFY(res == "20");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'E');
        VERIFY(res == "20");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'O');
        VERIFY(res == "%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'x');
        VERIFY(res == "09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'E');
        VERIFY(res == "09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'O');
        VERIFY(res == "%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'D');
        VERIFY(res == "09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'E');
        VERIFY(res == "%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'O');
        VERIFY(res == "%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'd');
        VERIFY(res == "04");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'E');
        VERIFY(res == "%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'O');
        VERIFY(res == "04");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'e');
        VERIFY(res == " 4");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'E');
        VERIFY(res == "%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'O');
        VERIFY(res == " 4");
    }
      
    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'F');
        VERIFY(res == "2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'E');
        VERIFY(res == "%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'O');
        VERIFY(res == "%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'H');
        VERIFY(res == "13");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'E');
        VERIFY(res == "%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'O');
        VERIFY(res == "13");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'I');
        VERIFY(res == "01");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'E');
        VERIFY(res == "%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'O');
        VERIFY(res == "01");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'j');
        VERIFY(res == "248");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'E');
        VERIFY(res == "%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'O');
        VERIFY(res == "%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'M');
        VERIFY(res == "33");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'E');
        VERIFY(res == "%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'O');
        VERIFY(res == "33");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'm');
        VERIFY(res == "09");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'E');
        VERIFY(res == "%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'O');
        VERIFY(res == "09");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'n');
        VERIFY(res == "\n");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'E');
        VERIFY(res == "%En");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'O');
        VERIFY(res == "%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'p');
        VERIFY(res == "PM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'E');
        VERIFY(res == "%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'O');
        VERIFY(res == "%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'R');
        VERIFY(res == "13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'E');
        VERIFY(res == "%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'O');
        VERIFY(res == "%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'r');
        VERIFY(res == "01:33:18 PM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'E');
        VERIFY(res == "%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'O');
        VERIFY(res == "%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'S');
        VERIFY(res == "18");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'E');
        VERIFY(res == "%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'O');
        VERIFY(res == "18");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'X');
        VERIFY(res == "13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        VERIFY(res == "13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'O');
        VERIFY(res == "%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'T');
        VERIFY(res == "13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'E');
        VERIFY(res == "%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'O');
        VERIFY(res == "%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 't');
        VERIFY(res == "\t");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'E');
        VERIFY(res == "%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'O');
        VERIFY(res == "%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'u');
        VERIFY(res == "3");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'E');
        VERIFY(res == "%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'O');
        VERIFY(res == "3");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'U');
        VERIFY(res == "35");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'E');
        VERIFY(res == "%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'O');
        VERIFY(res == "35");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'V');
        VERIFY(res == "36");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'E');
        VERIFY(res == "%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'O');
        VERIFY(res == "36");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'g');
        VERIFY(res == "24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'E');
        VERIFY(res == "%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'O');
        VERIFY(res == "%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'G');
        VERIFY(res == "2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'E');
        VERIFY(res == "%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'O');
        VERIFY(res == "%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'W');
        VERIFY(res == "36");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'E');
        VERIFY(res == "%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'O');
        VERIFY(res == "36");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'w');
        VERIFY(res == "3");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'E');
        VERIFY(res == "%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'O');
        VERIFY(res == "3");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y');
        VERIFY(res == "2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'E');
        VERIFY(res == "2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'O');
        VERIFY(res == "%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'y');
        VERIFY(res == "24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'E');
        VERIFY(res == "24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'O');
        VERIFY(res == "24");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z');
        VERIFY(res == "America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'E');
        VERIFY(res == "%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'O');
        VERIFY(res == "%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'z');
        VERIFY(res == "-0700");
        VERIFY(!(res.empty()));
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'E');
        VERIFY(res == "%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'O');
        VERIFY(res == "%Oz");
    }    

    dump_info("Done\n");
}

void test_timeio_char_put_2()
{
    dump_info("Test timeio<char> put 2...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("zh_CN.UTF-8"));
    auto tp = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    std::string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, '%');
        VERIFY(res == "%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'a');
        VERIFY(res == "三");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'E');
        VERIFY(res == "%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'O');
        VERIFY(res == "%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'A');
        VERIFY(res == "星期三");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'E');
        VERIFY(res == "%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'O');
        VERIFY(res == "%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'b');
        VERIFY(res == "9月");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'E');
        VERIFY(res == "%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'O');
        VERIFY(res == "%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'h');
        VERIFY(res == "9月");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'E');
        VERIFY(res == "%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'O');
        VERIFY(res == "%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'B');
        VERIFY(res == "九月");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'E');
        VERIFY(res == "%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'O');
        VERIFY(res == "%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'c');
        VERIFY(res == "2024年09月04日 星期三 13时33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        VERIFY(res == "2024年09月04日 星期三 13时33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'O');
        VERIFY(res == "%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'C');
        VERIFY(res == "20");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'E');
        VERIFY(res == "20");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'O');
        VERIFY(res == "%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'x');
        VERIFY(res == "2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'E');
        VERIFY(res == "2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'O');
        VERIFY(res == "%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'D');
        VERIFY(res == "09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'E');
        VERIFY(res == "%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'O');
        VERIFY(res == "%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'd');
        VERIFY(res == "04");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'E');
        VERIFY(res == "%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'O');
        VERIFY(res == "04");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'e');
        VERIFY(res == " 4");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'E');
        VERIFY(res == "%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'O');
        VERIFY(res == " 4");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'F');
        VERIFY(res == "2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'E');
        VERIFY(res == "%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'O');
        VERIFY(res == "%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'H');
        VERIFY(res == "13");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'E');
        VERIFY(res == "%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'O');
        VERIFY(res == "13");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'I');
        VERIFY(res == "01");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'E');
        VERIFY(res == "%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'O');
        VERIFY(res == "01");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'j');
        VERIFY(res == "248");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'E');
        VERIFY(res == "%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'O');
        VERIFY(res == "%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'M');
        VERIFY(res == "33");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'E');
        VERIFY(res == "%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'O');
        VERIFY(res == "33");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'm');
        VERIFY(res == "09");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'E');
        VERIFY(res == "%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'O');
        VERIFY(res == "09");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'n');
        VERIFY(res == "\n");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'E');
        VERIFY(res == "%En");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'O');
        VERIFY(res == "%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'p');
        VERIFY(res == "下午");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'E');
        VERIFY(res == "%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'O');
        VERIFY(res == "%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'R');
        VERIFY(res == "13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'E');
        VERIFY(res == "%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'O');
        VERIFY(res == "%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'r');
        VERIFY(res == "下午 01时33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'E');
        VERIFY(res == "%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'O');
        VERIFY(res == "%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'S');
        VERIFY(res == "18");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'E');
        VERIFY(res == "%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'O');
        VERIFY(res == "18");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'X');
        VERIFY(res == "13时33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        VERIFY(res == "13时33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'O');
        VERIFY(res == "%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'T');
        VERIFY(res == "13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'E');
        VERIFY(res == "%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'O');
        VERIFY(res == "%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 't');
        VERIFY(res == "\t");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'E');
        VERIFY(res == "%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'O');
        VERIFY(res == "%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'u');
        VERIFY(res == "3");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'E');
        VERIFY(res == "%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'O');
        VERIFY(res == "3");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'U');
        VERIFY(res == "35");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'E');
        VERIFY(res == "%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'O');
        VERIFY(res == "35");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'V');
        VERIFY(res == "36");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'E');
        VERIFY(res == "%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'O');
        VERIFY(res == "36");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'g');
        VERIFY(res == "24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'E');
        VERIFY(res == "%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'O');
        VERIFY(res == "%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'G');
        VERIFY(res == "2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'E');
        VERIFY(res == "%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'O');
        VERIFY(res == "%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'W');
        VERIFY(res == "36");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'E');
        VERIFY(res == "%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'O');
        VERIFY(res == "36");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'w');
        VERIFY(res == "3");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'E');
        VERIFY(res == "%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'O');
        VERIFY(res == "3");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y');
        VERIFY(res == "2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'E');
        VERIFY(res == "2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'O');
        VERIFY(res == "%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'y');
        VERIFY(res == "24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'E');
        VERIFY(res == "24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'O');
        VERIFY(res == "24");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z');
        VERIFY(res == "America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'E');
        VERIFY(res == "%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'O');
        VERIFY(res == "%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'z');
        VERIFY(res == "-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'E');
        VERIFY(res == "%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'O');
        VERIFY(res == "%Oz");
    }

    dump_info("Done\n");
}

void test_timeio_char_put_3()
{
    dump_info("Test timeio<char> put 3...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));
    auto tp = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    std::string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, '%');
        VERIFY(res == "%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'a');
        VERIFY(res == "水");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'E');
        VERIFY(res == "%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'O');
        VERIFY(res == "%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'A');
        VERIFY(res == "水曜日");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'E');
        VERIFY(res == "%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'O');
        VERIFY(res == "%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'b');
        VERIFY(res == " 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'E');
        VERIFY(res == "%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'O');
        VERIFY(res == "%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'h');
        VERIFY(res == " 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'E');
        VERIFY(res == "%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'O');
        VERIFY(res == "%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'B');
        VERIFY(res == "9月");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'E');
        VERIFY(res == "%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'O');
        VERIFY(res == "%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'c');
        VERIFY(res == "2024年09月04日 13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        VERIFY(res == "令和6年09月04日 13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'O');
        VERIFY(res == "%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'C');
        VERIFY(res == "20");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'E');
        VERIFY(res == "令和");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'O');
        VERIFY(res == "%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'x');
        VERIFY(res == "2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'E');
        VERIFY(res == "令和6年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'O');
        VERIFY(res == "%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'D');
        VERIFY(res == "09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'E');
        VERIFY(res == "%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'O');
        VERIFY(res == "%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'd');
        VERIFY(res == "04");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'E');
        VERIFY(res == "%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'O');
        VERIFY(res == "四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'e');
        VERIFY(res == " 4");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'E');
        VERIFY(res == "%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'O');
        VERIFY(res == "四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'F');
        VERIFY(res == "2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'E');
        VERIFY(res == "%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'O');
        VERIFY(res == "%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'H');
        VERIFY(res == "13");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'E');
        VERIFY(res == "%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'O');
        VERIFY(res == "十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'I');
        VERIFY(res == "01");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'E');
        VERIFY(res == "%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'O');
        VERIFY(res == "一");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'j');
        VERIFY(res == "248");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'E');
        VERIFY(res == "%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'O');
        VERIFY(res == "%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'M');
        VERIFY(res == "33");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'E');
        VERIFY(res == "%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'O');
        VERIFY(res == "三十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'm');
        VERIFY(res == "09");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'E');
        VERIFY(res == "%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'O');
        VERIFY(res == "九");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'n');
        VERIFY(res == "\n");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'E');
        VERIFY(res == "%En");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'O');
        VERIFY(res == "%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'p');
        VERIFY(res == "午後");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'E');
        VERIFY(res == "%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'O');
        VERIFY(res == "%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'R');
        VERIFY(res == "13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'E');
        VERIFY(res == "%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'O');
        VERIFY(res == "%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'r');
        VERIFY(res == "午後01時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'E');
        VERIFY(res == "%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'O');
        VERIFY(res == "%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'S');
        VERIFY(res == "18");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'E');
        VERIFY(res == "%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'O');
        VERIFY(res == "十八");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'X');
        VERIFY(res == "13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        VERIFY(res == "13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'O');
        VERIFY(res == "%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'T');
        VERIFY(res == "13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'E');
        VERIFY(res == "%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'O');
        VERIFY(res == "%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 't');
        VERIFY(res == "\t");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'E');
        VERIFY(res == "%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'O');
        VERIFY(res == "%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'u');
        VERIFY(res == "3");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'E');
        VERIFY(res == "%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'O');
        VERIFY(res == "三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'U');
        VERIFY(res == "35");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'E');
        VERIFY(res == "%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'O');
        VERIFY(res == "三十五");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'V');
        VERIFY(res == "36");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'E');
        VERIFY(res == "%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'O');
        VERIFY(res == "三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'g');
        VERIFY(res == "24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'E');
        VERIFY(res == "%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'O');
        VERIFY(res == "%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'G');
        VERIFY(res == "2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'E');
        VERIFY(res == "%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'O');
        VERIFY(res == "%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'W');
        VERIFY(res == "36");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'E');
        VERIFY(res == "%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'O');
        VERIFY(res == "三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'w');
        VERIFY(res == "3");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'E');
        VERIFY(res == "%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'O');
        VERIFY(res == "三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y');
        VERIFY(res == "2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'E');
        VERIFY(res == "令和6年");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'O');
        VERIFY(res == "%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'y');
        VERIFY(res == "24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'E');
        VERIFY(res == "6");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'O');
        VERIFY(res == "二十四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z');
        VERIFY(res == "America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'E');
        VERIFY(res == "%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'O');
        VERIFY(res == "%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'z');
        VERIFY(res == "-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'E');
        VERIFY(res == "%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'O');
        VERIFY(res == "%Oz");
    }

    dump_info("Done\n");
}

void test_timeio_char_put_4()
{
    dump_info("Test timeio<char> put 4...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::string oss;
    {
        obj.put(std::back_inserter(oss), time1, 'a');
        VERIFY(oss == "Sun");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, 'x');
        VERIFY(oss == "04/04/71");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, 'X');
        VERIFY(oss == "12:00:00");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, 'x', 'E');
        VERIFY(oss == "04/04/71");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, 'X', 'E');
        VERIFY(oss == "12:00:00");
    }

    dump_info("Done\n");
}

void test_timeio_char_put_5()
{
    dump_info("Test timeio<char> put 5...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("de_DE.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::string oss;
    {
        obj.put(std::back_inserter(oss), time1, 'a');
        VERIFY(!((oss != "Son") && (oss != "So")));
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x');
        VERIFY(oss == "04.04.1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X');
        VERIFY(oss == "12:00:00");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x', 'E');
        VERIFY(oss == "04.04.1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X', 'E');
        VERIFY(oss == "12:00:00");
    }

    dump_info("Done\n");
}

void test_timeio_char_put_6()
{
    dump_info("Test timeio<char> put 6...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("en_HK.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::string oss;
    {
        obj.put(std::back_inserter(oss), time1, 'a');
        VERIFY(oss == "Sun");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x');
        VERIFY(oss == "Sunday, April 04, 1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X');
        VERIFY(oss.find("12:00:00") != std::string::npos);
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x', 'E');
        VERIFY(oss == "Sunday, April 04, 1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X', 'E');
        VERIFY(oss.find("12:00:00") != std::string::npos);
    }

    dump_info("Done\n");
}

void test_timeio_char_put_7()
{
    dump_info("Test timeio<char> put 7...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("es_ES.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::string oss;
    {
        obj.put(std::back_inserter(oss), time1, 'a');
        VERIFY(oss == "dom");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x');
        VERIFY(oss == "04/04/71");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X');
        VERIFY(oss == "12:00:00");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x', 'E');
        VERIFY(oss == "04/04/71");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X', 'E');
        VERIFY(oss == "12:00:00");
    }

    dump_info("Done\n");
}

void test_timeio_char_put_8()
{
    dump_info("Test timeio<char> put 8...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::string date = "%A, the second of %B";
    const std::string date_ex = "%Ex";
    std::string oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        VERIFY(oss == "Sunday, the second of April");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        VERIFY(oss != oss2);
    }

    dump_info("Done\n");
}

void test_timeio_char_put_9()
{
    dump_info("Test timeio<char> put 9...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("de_DE.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::string date = "%A, the second of %B";
    const std::string date_ex = "%Ex";
    std::string oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        VERIFY(oss == "Sonntag, the second of April");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        VERIFY(oss != oss2);
    }

    dump_info("Done\n");
}

void test_timeio_char_put_10()
{
    dump_info("Test timeio<char> put 10...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("en_HK.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::string date = "%A, the second of %B";
    const std::string date_ex = "%Ex";
    std::string oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        VERIFY(oss == "Sunday, the second of April");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        VERIFY(oss != oss2);
    }

    dump_info("Done\n");
}

void test_timeio_char_put_11()
{
    dump_info("Test timeio<char> put 11...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("fr_FR.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::string date = "%A, the second of %B";
    const std::string date_ex = "%Ex";
    std::string oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        VERIFY(oss == "dimanche, the second of avril");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        VERIFY(oss != oss2);
    }

    dump_info("Done\n");
}

void test_timeio_char_put_12()
{
    dump_info("Test timeio<char> put 12...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    auto time_sanity = create_zoned_time(1997, 6, 26, 12, 0, 0, "America/Los_Angeles");

    std::string res(50, 'x');
    const std::string date = "%T, %A, the second of %B, %Y";
        
    auto ret1 = obj.put(res.begin(), time_sanity, date);
    std::string sanity1(res.begin(), ret1);
    VERIFY(res == "12:00:00, Thursday, the second of June, 1997xxxxxx");
    VERIFY(sanity1 == "12:00:00, Thursday, the second of June, 1997");

    dump_info("Done\n");
}

void test_timeio_char_put_13()
{
    dump_info("Test timeio<char> put 13...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    auto time_sanity = create_zoned_time(1997, 6, 24, 12, 0, 0, "America/Los_Angeles");

    std::string res(50, 'x');

    auto ret1 = obj.put(res.begin(), time_sanity, 'A');
    std::string sanity1(res.begin(), ret1);
    VERIFY(res == "Tuesdayxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    VERIFY(sanity1 == "Tuesday");

    dump_info("Done\n");
}

void test_timeio_char_put_14()
{
    dump_info("Test timeio<char> put 14...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ta_IN.UTF-8"));
    const tm time1 = test_tm(0, 0, 12, 4, 3, 71, 0, 93, 0);
    auto zt = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::string res;
    obj.put(std::back_inserter(res), zt, 'c');

    char time_buffer[128];
    setlocale(LC_ALL, "ta_IN");
    std::strftime(time_buffer, 128, "%c", &time1);
    setlocale(LC_ALL, "C");

    VERIFY(time_buffer == res);

    dump_info("Done\n");
}

void test_timeio_char_put_15()
{
    dump_info("Test timeio<char> put 15...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));
    using namespace std::chrono;
    year_month_day tp{year{2024}, month{9}, day{4}};

    std::string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, '%');
        VERIFY(res == "%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'a');
        VERIFY(res == "水");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'E');
        VERIFY(res == "%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'O');
        VERIFY(res == "%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'A');
        VERIFY(res == "水曜日");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'E');
        VERIFY(res == "%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'O');
        VERIFY(res == "%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'b');
        VERIFY(res == " 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'E');
        VERIFY(res == "%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'O');
        VERIFY(res == "%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'h');
        VERIFY(res == " 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'E');
        VERIFY(res == "%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'O');
        VERIFY(res == "%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'B');
        VERIFY(res == "9月");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'E');
        VERIFY(res == "%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'O');
        VERIFY(res == "%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'c');
        VERIFY(res == "%c");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        VERIFY(res == "%Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'O');
        VERIFY(res == "%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'C');
        VERIFY(res == "20");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'E');
        VERIFY(res == "令和");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'O');
        VERIFY(res == "%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'x');
        VERIFY(res == "2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'E');
        VERIFY(res == "令和6年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'O');
        VERIFY(res == "%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'D');
        VERIFY(res == "09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'E');
        VERIFY(res == "%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'O');
        VERIFY(res == "%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'd');
        VERIFY(res == "04");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'E');
        VERIFY(res == "%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'O');
        VERIFY(res == "四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'e');
        VERIFY(res == " 4");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'E');
        VERIFY(res == "%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'O');
        VERIFY(res == "四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'F');
        VERIFY(res == "2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'E');
        VERIFY(res == "%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'O');
        VERIFY(res == "%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'H');
        VERIFY(res == "%H");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'E');
        VERIFY(res == "%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'O');
        VERIFY(res == "%OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'I');
        VERIFY(res == "%I");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'E');
        VERIFY(res == "%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'O');
        VERIFY(res == "%OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'j');
        VERIFY(res == "248");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'E');
        VERIFY(res == "%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'O');
        VERIFY(res == "%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'M');
        VERIFY(res == "%M");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'E');
        VERIFY(res == "%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'O');
        VERIFY(res == "%OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'm');
        VERIFY(res == "09");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'E');
        VERIFY(res == "%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'O');
        VERIFY(res == "九");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'n');
        VERIFY(res == "\n");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'E');
        VERIFY(res == "%En");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'O');
        VERIFY(res == "%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'p');
        VERIFY(res == "%p");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'E');
        VERIFY(res == "%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'O');
        VERIFY(res == "%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'R');
        VERIFY(res == "%R");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'E');
        VERIFY(res == "%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'O');
        VERIFY(res == "%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'r');
        VERIFY(res == "%r");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'E');
        VERIFY(res == "%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'O');
        VERIFY(res == "%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'S');
        VERIFY(res == "%S");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'E');
        VERIFY(res == "%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'O');
        VERIFY(res == "%OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'X');
        VERIFY(res == "%X");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        VERIFY(res == "%EX");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'O');
        VERIFY(res == "%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'T');
        VERIFY(res == "%T");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'E');
        VERIFY(res == "%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'O');
        VERIFY(res == "%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 't');
        VERIFY(res == "\t");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'E');
        VERIFY(res == "%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'O');
        VERIFY(res == "%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'u');
        VERIFY(res == "3");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'E');
        VERIFY(res == "%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'O');
        VERIFY(res == "三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'U');
        VERIFY(res == "35");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'E');
        VERIFY(res == "%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'O');
        VERIFY(res == "三十五");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'V');
        VERIFY(res == "36");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'E');
        VERIFY(res == "%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'O');
        VERIFY(res == "三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'g');
        VERIFY(res == "24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'E');
        VERIFY(res == "%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'O');
        VERIFY(res == "%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'G');
        VERIFY(res == "2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'E');
        VERIFY(res == "%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'O');
        VERIFY(res == "%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'W');
        VERIFY(res == "36");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'E');
        VERIFY(res == "%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'O');
        VERIFY(res == "三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'w');
        VERIFY(res == "3");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'E');
        VERIFY(res == "%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'O');
        VERIFY(res == "三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y');
        VERIFY(res == "2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'E');
        VERIFY(res == "令和6年");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'O');
        VERIFY(res == "%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'y');
        VERIFY(res == "24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'E');
        VERIFY(res == "6");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'O');
        VERIFY(res == "二十四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z');
        VERIFY(res == "%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'E');
        VERIFY(res == "%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'O');
        VERIFY(res == "%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'z');
        VERIFY(res == "%z");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'E');
        VERIFY(res == "%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'O');
        VERIFY(res == "%Oz");
    }
    dump_info("Done\n");
}

void test_timeio_char_put_16()
{
    dump_info("Test timeio<char> put 16...");

    using namespace std::chrono;
    seconds time_sec = hours{13} + minutes{33} + seconds{18};
    std::chrono::hh_mm_ss tp{time_sec};

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));
    std::string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, '%');
        VERIFY(res == "%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'a');
        VERIFY(res == "%a");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'E');
        VERIFY(res == "%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'O');
        VERIFY(res == "%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'A');
        VERIFY(res == "%A");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'E');
        VERIFY(res == "%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'O');
        VERIFY(res == "%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'b');
        VERIFY(res == "%b");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'E');
        VERIFY(res == "%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'O');
        VERIFY(res == "%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'h');
        VERIFY(res == "%h");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'E');
        VERIFY(res == "%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'O');
        VERIFY(res == "%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'B');
        VERIFY(res == "%B");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'E');
        VERIFY(res == "%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'O');
        VERIFY(res == "%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'c');
        VERIFY(res == "%c");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        VERIFY(res == "%Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'O');
        VERIFY(res == "%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'x');
        VERIFY(res == "%x");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'E');
        VERIFY(res == "%Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'O');
        VERIFY(res == "%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'D');
        VERIFY(res == "%D");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'E');
        VERIFY(res == "%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'O');
        VERIFY(res == "%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'd');
        VERIFY(res == "%d");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'E');
        VERIFY(res == "%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'O');
        VERIFY(res == "%Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'e');
        VERIFY(res == "%e");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'E');
        VERIFY(res == "%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'O');
        VERIFY(res == "%Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'F');
        VERIFY(res == "%F");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'E');
        VERIFY(res == "%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'O');
        VERIFY(res == "%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'H');
        VERIFY(res == "13");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'E');
        VERIFY(res == "%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'O');
        VERIFY(res == "十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'I');
        VERIFY(res == "01");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'E');
        VERIFY(res == "%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'O');
        VERIFY(res == "一");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'j');
        VERIFY(res == "%j");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'E');
        VERIFY(res == "%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'O');
        VERIFY(res == "%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'M');
        VERIFY(res == "33");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'E');
        VERIFY(res == "%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'O');
        VERIFY(res == "三十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'm');
        VERIFY(res == "%m");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'E');
        VERIFY(res == "%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'O');
        VERIFY(res == "%Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'n');
        VERIFY(res == "\n");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'E');
        VERIFY(res == "%En");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'O');
        VERIFY(res == "%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'p');
        VERIFY(res == "午後");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'E');
        VERIFY(res == "%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'O');
        VERIFY(res == "%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'R');
        VERIFY(res == "13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'E');
        VERIFY(res == "%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'O');
        VERIFY(res == "%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'r');
        VERIFY(res == "午後01時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'E');
        VERIFY(res == "%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'O');
        VERIFY(res == "%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'S');
        VERIFY(res == "18");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'E');
        VERIFY(res == "%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'O');
        VERIFY(res == "十八");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'X');
        VERIFY(res == "13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        VERIFY(res == "13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'O');
        VERIFY(res == "%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'T');
        VERIFY(res == "13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'E');
        VERIFY(res == "%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'O');
        VERIFY(res == "%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 't');
        VERIFY(res == "\t");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'E');
        VERIFY(res == "%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'O');
        VERIFY(res == "%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'u');
        VERIFY(res == "%u");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'E');
        VERIFY(res == "%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'O');
        VERIFY(res == "%Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'U');
        VERIFY(res == "%U");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'E');
        VERIFY(res == "%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'O');
        VERIFY(res == "%OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'V');
        VERIFY(res == "%V");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'E');
        VERIFY(res == "%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'O');
        VERIFY(res == "%OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'g');
        VERIFY(res == "%g");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'E');
        VERIFY(res == "%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'O');
        VERIFY(res == "%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'G');
        VERIFY(res == "%G");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'E');
        VERIFY(res == "%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'O');
        VERIFY(res == "%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'W');
        VERIFY(res == "%W");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'E');
        VERIFY(res == "%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'O');
        VERIFY(res == "%OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'w');
        VERIFY(res == "%w");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'E');
        VERIFY(res == "%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'O');
        VERIFY(res == "%Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y');
        VERIFY(res == "%Y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'E');
        VERIFY(res == "%EY");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'O');
        VERIFY(res == "%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'y');
        VERIFY(res == "%y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'E');
        VERIFY(res == "%Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'O');
        VERIFY(res == "%Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z'); VERIFY(res == "%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'E');
        VERIFY(res == "%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'O');
        VERIFY(res == "%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'z'); VERIFY(res == "%z");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'E');
        VERIFY(res == "%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'O');
        VERIFY(res == "%Oz");
    }
    dump_info("Done\n");
}

void test_timeio_char_put_17()
{
    dump_info("Test timeio<char> put 17...");
    std::tm tp = test_tm(18, 33, 13, 4, 9 - 1, 2024 - 1900, 0, 0, 0);
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));

    std::string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, '%');
        VERIFY(res == "%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'a');
        VERIFY(res == "水");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'E');
        VERIFY(res == "%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'O');
        VERIFY(res == "%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'A');
        VERIFY(res == "水曜日");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'E');
        VERIFY(res == "%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'O');
        VERIFY(res == "%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'b');
        VERIFY(res == " 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'E');
        VERIFY(res == "%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'O');
        VERIFY(res == "%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'h');
        VERIFY(res == " 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'E');
        VERIFY(res == "%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'O');
        VERIFY(res == "%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'B');
        VERIFY(res == "9月");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'E');
        VERIFY(res == "%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'O');
        VERIFY(res == "%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'c');
        VERIFY(res == "2024年09月04日 13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        VERIFY(res == "令和6年09月04日 13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'O');
        VERIFY(res == "%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'C');
        VERIFY(res == "20");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'E');
        VERIFY(res == "令和");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'O');
        VERIFY(res == "%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'x');
        VERIFY(res == "2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'E');
        VERIFY(res == "令和6年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'O');
        VERIFY(res == "%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'D');
        VERIFY(res == "09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'E');
        VERIFY(res == "%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'O');
        VERIFY(res == "%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'd');
        VERIFY(res == "04");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'E');
        VERIFY(res == "%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'O');
        VERIFY(res == "四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'e');
        VERIFY(res == " 4");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'E');
        VERIFY(res == "%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'O');
        VERIFY(res == "四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'F');
        VERIFY(res == "2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'E');
        VERIFY(res == "%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'O');
        VERIFY(res == "%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'H');
        VERIFY(res == "13");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'E');
        VERIFY(res == "%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'O');
        VERIFY(res == "十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'I');
        VERIFY(res == "01");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'E');
        VERIFY(res == "%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'O');
        VERIFY(res == "一");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'j');
        VERIFY(res == "248");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'E');
        VERIFY(res == "%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'O');
        VERIFY(res == "%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'M');
        VERIFY(res == "33");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'E');
        VERIFY(res == "%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'O');
        VERIFY(res == "三十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'm');
        VERIFY(res == "09");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'E');
        VERIFY(res == "%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'O');
        VERIFY(res == "九");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'n');
        VERIFY(res == "\n");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'E');
        VERIFY(res == "%En");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'O');
        VERIFY(res == "%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'p');
        VERIFY(res == "午後");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'E');
        VERIFY(res == "%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'O');
        VERIFY(res == "%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'R');
        VERIFY(res == "13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'E');
        VERIFY(res == "%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'O');
        VERIFY(res == "%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'r');
        VERIFY(res == "午後01時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'E');
        VERIFY(res == "%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'O');
        VERIFY(res == "%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'S');
        VERIFY(res == "18");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'E');
        VERIFY(res == "%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'O');
        VERIFY(res == "十八");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'X');
        VERIFY(res == "13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        VERIFY(res == "13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'O');
        VERIFY(res == "%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'T');
        VERIFY(res == "13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'E');
        VERIFY(res == "%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'O');
        VERIFY(res == "%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 't');
        VERIFY(res == "\t");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'E');
        VERIFY(res == "%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'O');
        VERIFY(res == "%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'u');
        VERIFY(res == "3");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'E');
        VERIFY(res == "%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'O');
        VERIFY(res == "三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'U');
        VERIFY(res == "35");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'E');
        VERIFY(res == "%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'O');
        VERIFY(res == "三十五");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'V');
        VERIFY(res == "36");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'E');
        VERIFY(res == "%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'O');
        VERIFY(res == "三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'g');
        VERIFY(res == "24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'E');
        VERIFY(res == "%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'O');
        VERIFY(res == "%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'G');
        VERIFY(res == "2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'E');
        VERIFY(res == "%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'O');
        VERIFY(res == "%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'W');
        VERIFY(res == "36");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'E');
        VERIFY(res == "%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'O');
        VERIFY(res == "三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'w');
        VERIFY(res == "3");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'E');
        VERIFY(res == "%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'O');
        VERIFY(res == "三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y');
        VERIFY(res == "2024");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'E');
        VERIFY(res == "令和6年");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'O');
        VERIFY(res == "%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'y');
        VERIFY(res == "24");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'E');
        VERIFY(res == "6");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'O');
        VERIFY(res == "二十四");
    }

    {
        // test_tm leaves tm_zone null: %Z has a field to fill but no name to fill it with.
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z'); VERIFY(res == "UNKNOWN");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'E');
        VERIFY(res == "%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'O');
        VERIFY(res == "%OZ");

#ifdef __USE_MISC
        std::tm named = tp;
        named.tm_zone = "PST";
        res.clear(); obj.put(std::back_inserter(res), named, 'Z'); VERIFY(res == "PST");

        // An empty string is as nameless as a null pointer.
        named.tm_zone = "";
        res.clear(); obj.put(std::back_inserter(res), named, 'Z'); VERIFY(res == "UNKNOWN");
#endif
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'z'); VERIFY(res == "+0000");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'E');
        VERIFY(res == "%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'O');
        VERIFY(res == "%Oz");
    }
    dump_info("Done\n");
}

namespace
{
    constexpr static IOv2::ios_defs::iostate febit = IOv2::ios_defs::eofbit | IOv2::ios_defs::strfailbit;

    template <typename T = IOv2::time_parse_context<char>, bool HaveDate = true, bool HaveTime = true, IOv2::tz_level TzLevel = IOv2::tz_level::zone>
    T CheckGet(const IOv2::timeio<char>& obj, const std::string& input,
               char fmt, char modif,
               IOv2::ios_defs::iostate err_exp, size_t consume_exp = (size_t)-1)
    {
        if (consume_exp == (size_t)-1) consume_exp = input.size();
        IOv2::time_parse_context<char, HaveDate, HaveTime, TzLevel> ctx1, ctx2, ctx3;
        if (err_exp == IOv2::ios_defs::goodbit)
        {
            VERIFY(obj.get(input.begin(), input.end(), ctx1, fmt, modif) != input.end());
            {
                std::list<char> lst_input(input.begin(), input.end());
                VERIFY(obj.get(lst_input.begin(), lst_input.end(), ctx2, fmt, modif) != lst_input.end());
                VERIFY(ctx2 == ctx1);
            }
            {
                IOv2::streambuf sb(IOv2::mem_device{input});
                auto beg = IOv2::istreambuf_iterator(sb);
                VERIFY(obj.get(beg, std::default_sentinel, ctx3, fmt, modif) != std::default_sentinel);
                VERIFY(ctx3 == ctx1);
            }
        }
        else if (err_exp == IOv2::ios_defs::eofbit)
        {
            VERIFY(obj.get(input.begin(), input.end(), ctx1, fmt, modif) == input.end());
            {
                std::list<char> lst_input(input.begin(), input.end());
                VERIFY(obj.get(lst_input.begin(), lst_input.end(), ctx2, fmt, modif) == lst_input.end());
                VERIFY(ctx2 == ctx1);
            }
            {
                IOv2::streambuf sb(IOv2::mem_device{input});
                auto beg = IOv2::istreambuf_iterator(sb);
                VERIFY(obj.get(beg, std::default_sentinel, ctx3, fmt, modif) == std::default_sentinel);
                VERIFY(ctx3 == ctx1);
            }
        }
        else if (err_exp & IOv2::ios_defs::strfailbit)
        {
            try
            {
                obj.get(input.begin(), input.end(), ctx1, fmt, modif);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}

            try
            {
                std::list<char> lst_input(input.begin(), input.end());
                obj.get(lst_input.begin(), lst_input.end(), ctx1, fmt, modif);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}

            try
            {
                IOv2::streambuf sb(IOv2::mem_device{input});
                auto beg = IOv2::istreambuf_iterator(sb);
                obj.get(beg, std::default_sentinel, ctx1, fmt, modif);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}            
        }
        else
        {
            dump_info("unreachable code");
            std::abort();
        }
        return ctx_to<T>(ctx1);
    }

    template <typename T = IOv2::time_parse_context<char>, bool HaveDate = true, bool HaveTime = true, IOv2::tz_level TzLevel = IOv2::tz_level::zone>
    T CheckGet(const IOv2::timeio<char>& obj, const std::string& input,
               const std::string& fmt,
               IOv2::ios_defs::iostate err_exp, size_t consume_exp = (size_t)-1)
    {
        if (consume_exp == (size_t)-1) consume_exp = input.size();
        IOv2::time_parse_context<char, HaveDate, HaveTime, TzLevel> ctx1, ctx2, ctx3;
        if (err_exp == IOv2::ios_defs::goodbit)
        {
            VERIFY(obj.get(input.begin(), input.end(), ctx1, fmt) != input.end());
            {
                std::list<char> lst_input(input.begin(), input.end());
                VERIFY(obj.get(lst_input.begin(), lst_input.end(), ctx2, fmt) != lst_input.end());
                VERIFY(ctx2 == ctx1);
            }
            {
                IOv2::streambuf sb(IOv2::mem_device{input});
                auto beg = IOv2::istreambuf_iterator(sb);
                VERIFY(obj.get(beg, std::default_sentinel, ctx3, fmt) != std::default_sentinel);
                VERIFY(ctx3 == ctx1);
            }
        }
        else if (err_exp == IOv2::ios_defs::eofbit)
        {
            VERIFY(obj.get(input.begin(), input.end(), ctx1, fmt) == input.end());
            {
                std::list<char> lst_input(input.begin(), input.end());
                VERIFY(obj.get(lst_input.begin(), lst_input.end(), ctx2, fmt) == lst_input.end());
                VERIFY(ctx2 == ctx1);
            }
            {
                IOv2::streambuf sb(IOv2::mem_device{input});
                auto beg = IOv2::istreambuf_iterator(sb);
                VERIFY(obj.get(beg, std::default_sentinel, ctx3, fmt) == std::default_sentinel);
                VERIFY(ctx3 == ctx1);
            }
        }
        else if (err_exp & IOv2::ios_defs::strfailbit)
        {
            try
            {
                obj.get(input.begin(), input.end(), ctx1, fmt);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}

            try
            {
                std::list<char> lst_input(input.begin(), input.end());
                obj.get(lst_input.begin(), lst_input.end(), ctx1, fmt);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}

            try
            {
                IOv2::streambuf sb(IOv2::mem_device{input});
                auto beg = IOv2::istreambuf_iterator(sb);
                obj.get(beg, std::default_sentinel, ctx1, fmt);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}            
        }
        else
        {
            dump_info("unreachable code");
            std::abort();
        }
        return ctx_to<T>(ctx1);
    }
}

void test_timeio_char_get_1()
{
    dump_info("Test timeio<char> get 1...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    CheckGet(obj, "%",   '%',  0,  IOv2::ios_defs::eofbit);
    CheckGet(obj, "x",   '%',  0,  IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%",   '%', 'E', febit);
    CheckGet(obj, "%E%", '%', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "%",   '%', 'O', febit);
    CheckGet(obj, "%O%", '%', 'O', IOv2::ios_defs::eofbit);

    VERIFY(CheckGet(obj, "Wed", 'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, "%Ea", 'a', 'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, "a",   'a', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oa", 'a', 'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, "a",   'a', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "Wednesday", 'A', 0, IOv2::ios_defs::eofbit, 9).m_wday == 3);
    CheckGet(obj, "%EA", 'A', 'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, "A",   'A', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OA", 'A', 'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, "A",   'A', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "Sep", 'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, "%Eb", 'b', 'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, "b",   'b', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Ob", 'b', 'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, "b",   'b', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "September", 'B', 0, IOv2::ios_defs::eofbit, 9).m_month == 9);
    CheckGet(obj, "%EB", 'B', 'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, "B",   'B', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OB", 'B', 'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, "B",   'B', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "Sep", 'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, "%Eh", 'h', 'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, "h",   'h', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oh", 'h', 'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, "h",   'h', 'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(CheckGet<year_month_day>(obj, "Wed Sep  4 13:33:18 2024", 'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "Wed Sep  4 13:33:18 2024", 'c', 'E', IOv2::ios_defs::eofbit, 17) == check_date1);
    CheckGet(obj, "c",   'c', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oc", 'c', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "c",   'c', 'O', IOv2::ios_defs::strfailbit, 0);


    VERIFY(CheckGet(obj, "20", 'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(CheckGet(obj, "20", 'C', 'E', IOv2::ios_defs::eofbit).m_century == 20);
    CheckGet(obj, "C",   'C', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OC", 'C', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "C",   'C', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "04", 'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, "04", 'd', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, "%Ed", 'd', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "d",   'd', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "d",   'd', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "4", 'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, "4", 'e', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, "%Ee", 'e', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "e",   'e', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "e",   'e', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "2024-09-04", 'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "%EF", 'F', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "F",   'F', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OF", 'F', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "F",   'F', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "09/04/24", 'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "09/04/24", 'x', 'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "x",   'x', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Ox", 'x', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "x",   'x', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "09/04/24", 'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "%ED", 'D', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "D",   'D', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OD", 'D', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "D",   'D', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "13", 'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, "13", 'H', 'O', IOv2::ios_defs::eofbit).m_hour == 13);
    CheckGet(obj, "%EH", 'H', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "H",   'H', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "H",   'H', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "01", 'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, "01", 'I', 'O', IOv2::ios_defs::eofbit).m_hour == 1);
    CheckGet(obj, "%EI", 'I', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "I",   'I', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "I",   'I', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "248", 'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(CheckGet<year_month_day>(obj, "2024 248", "%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "%Ej", 'j', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "j",   'j', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oj", 'j', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "j",   'j', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "09", 'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, "09", 'm', 'O', IOv2::ios_defs::eofbit).m_month == 9);
    CheckGet(obj, "%Em", 'm', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "m",   'm', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "m",   'm', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "33", 'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, "33", 'M', 'O', IOv2::ios_defs::eofbit).m_minute == 33);
    CheckGet(obj, "%EM", 'M', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "M",   'M', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "M",   'M', 'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, "\n",   'n',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, "x",    'n',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, "\n",   'n', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%En",  'n', 'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, "n",    'n', 'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%On",  'n', 'O', IOv2::ios_defs::eofbit, 3);

    CheckGet(obj, "\t",   't',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, "x",    't',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, "\t",   't', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Et",  't', 'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, "n",    't', 'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Ot",  't', 'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 PM", "%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 AM", "%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(CheckGet(obj, "PM", 'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(CheckGet(obj, "AM", 'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    CheckGet(obj, "%Ep", 'p', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "p",   'p', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Op", 'p', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "p",   'p', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01:33:18 PM", "%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "%Er", 'r', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "r",   'r', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Or", 'r', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "r",   'r', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33", "%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, "%ER", 'R', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "R",   'R', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OR", 'R', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "R",   'R', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "18", 'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, "18", 'S', 'O', IOv2::ios_defs::eofbit).m_second == 18);
    CheckGet(obj, "%ES", 'S', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "S",   'S', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "S",   'S', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18", "%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18", "%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "X",   'X', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OX", 'X', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "X",   'X', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18", "%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "%ET", 'T', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "T",   'T', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OT", 'T', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "T",   'T', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "3", 'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, "3", 'u', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, "%Eu", 'u', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "u",   'u', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "u",   'u', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "24", 'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, "%Eg", 'g', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "g",   'g', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Og", 'g', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "g",   'g', 'O', IOv2::ios_defs::strfailbit, 0);


    VERIFY(CheckGet(obj, "2024", 'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, "%EG", 'G', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "G",   'G', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OG", 'G', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "G",   'G', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "2024 35 Wed", "%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "2024 35 Wed", "%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, "35", 'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(CheckGet(obj, "35", 'U', 'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    CheckGet(obj, "%EU", 'U', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "U",   'U', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "U",   'U', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "2024 36 Wed", "%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "2024 36 Wed", "%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, "36", 'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(CheckGet(obj, "36", 'W', 'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    CheckGet(obj, "%EW", 'W', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "W",   'W', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "W",   'W', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "36", 'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    CheckGet(obj, "54",  'V', 'O', IOv2::ios_defs::strfailbit, 1);
    CheckGet(obj, "36",  'V', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "%EV", 'V', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "V",   'V', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "V",   'V', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "3", 'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, "3", 'w', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, "%Ew", 'w', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "w",   'w', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "w",   'w', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "24", 'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, "24", 'y', 'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, "24", 'y', 'O', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, "y",  'y', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "y",  'y', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "2024", 'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, "2024", 'Y', 'E', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, "Y",   'Y', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OY", 'Y', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "Y",   'Y', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(zone_is(CheckGet(obj, "America/Los_Angeles", 'Z', 0, IOv2::ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, "PST", 'Z', 0, IOv2::ios_defs::eofbit); VERIFY(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, "America/Los_Angexes", 'Z', 0, IOv2::ios_defs::strfailbit);
    CheckGet(obj, "%EZ", 'Z', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "Z",   'Z', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OZ", 'Z', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "Z",   'Z', 'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, "Z", 'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, "+13", 'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, "-1110", 'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, "+11:10", 'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, "%Ez", 'z', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "z",  'z', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oz", 'z', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "z",  'z', 'O', IOv2::ios_defs::strfailbit, 0);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(CheckGet<year_month_day>(obj, "1999-W52-6", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, "2019-W01-1", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, "1999-W52-5", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, "99-W52-6", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, "19-W01-1", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, "99-W52-5", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, "20 24/09/04", "%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);

    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    VERIFY(CheckGet<year_month_day>(obj, "20 01 01", "%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}});
    dump_info("Done\n");
}

void test_timeio_char_get_2()
{
    dump_info("Test timeio<char> get 2...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("zh_CN.UTF-8"));

    CheckGet(obj, "%",  '%',  0,  IOv2::ios_defs::eofbit);
    CheckGet(obj, "x",  '%',  0,  IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%",  '%', 'E', febit);
    CheckGet(obj, "%E%", '%', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "%",  '%', 'O', febit);
    CheckGet(obj, "%O%", '%', 'O', IOv2::ios_defs::eofbit);

    VERIFY(CheckGet(obj, "三", 'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, "%Ea", 'a', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "a",   'a', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oa", 'a', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "a",   'a', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "星期三", 'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, "%EA", 'A', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "A",   'A', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OA", 'A', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "A",   'A', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "九月", 'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, "%Eb", 'b', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "b",   'b', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Ob", 'b', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "b",   'b', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "九月", 'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, "%EB", 'B', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "B",   'B', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OB", 'B', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "B",   'B', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "九月", 'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, "%Eh", 'h', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "h",   'h', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oh", 'h', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "h",   'h', 'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(CheckGet<year_month_day>(obj, "2024年09月04日 星期三 13时33分18秒", 'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "2024年09月04日 星期三 13时33分18秒", 'c', 'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "c",   'c', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oc", 'c', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "c",   'c', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "20", 'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(CheckGet(obj, "20", 'C', 'E', IOv2::ios_defs::eofbit).m_century == 20);
    CheckGet(obj, "C",   'C', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OC", 'C', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "C",   'C', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "04", 'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, "04", 'd', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, "%Ed", 'd', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "d",   'd', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "d",   'd', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "4", 'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, "4", 'e', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, "%Ee", 'e', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "e",   'e', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "e",   'e', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "2024-09-04", 'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "%EF", 'F', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "F",   'F', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OF", 'F', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "F",   'F', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "2024年09月04日", 'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "2024年09月04日", 'x', 'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "x",   'x', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Ox", 'x', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "x",   'x', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "09/04/24", 'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "%ED", 'D', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "D",   'D', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OD", 'D', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "D",   'D', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "13", 'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, "13", 'H', 'O', IOv2::ios_defs::eofbit).m_hour == 13);
    CheckGet(obj, "%EH", 'H', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "H",   'H', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "H",   'H', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "01", 'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, "01", 'I', 'O', IOv2::ios_defs::eofbit).m_hour == 1);
    CheckGet(obj, "%EI", 'I', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "I",   'I', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "I",   'I', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "248", 'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(CheckGet<year_month_day>(obj, "2024 248", "%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "%Ej", 'j', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "j",   'j', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oj", 'j', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "j",   'j', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "09", 'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, "09", 'm', 'O', IOv2::ios_defs::eofbit).m_month == 9);
    CheckGet(obj, "%Em", 'm', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "m",   'm', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "m",   'm', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "33", 'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, "33", 'M', 'O', IOv2::ios_defs::eofbit).m_minute == 33);
    CheckGet(obj, "%EM", 'M', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "M",   'M', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "M",   'M', 'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, "\n",   'n',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, "x",    'n',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, "\n",   'n', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%En",  'n', 'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, "n",    'n', 'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%On",  'n', 'O', IOv2::ios_defs::eofbit, 3);

    CheckGet(obj, "\t",   't',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, "x",    't',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, "\t",   't', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Et",  't', 'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, "n",    't', 'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Ot",  't', 'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 下午", "%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 上午", "%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(CheckGet(obj, "下午", 'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(CheckGet(obj, "上午", 'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    CheckGet(obj, "%Ep", 'p', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "p",   'p', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Op", 'p', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "p",   'p', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "下午 01时33分18秒", "%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "%Er", 'r', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "r",   'r', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Or", 'r', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "r",   'r', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33", "%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, "%ER", 'R', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "R",   'R', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OR", 'R', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "R",   'R', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "18", 'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, "18", 'S', 'O', IOv2::ios_defs::eofbit).m_second == 18);
    CheckGet(obj, "%ES", 'S', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "S",   'S', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "S",   'S', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13时33分18秒", "%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13时33分18秒", "%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "X",   'X', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OX", 'X', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "X",   'X', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18", "%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "%ET", 'T', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "T",   'T', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OT", 'T', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "T",   'T', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "3", 'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, "3", 'u', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, "%Eu", 'u', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "u",   'u', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "u",   'u', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "24", 'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, "%Eg", 'g', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "g",   'g', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Og", 'g', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "g",   'g', 'O', IOv2::ios_defs::strfailbit, 0);


    VERIFY(CheckGet(obj, "2024", 'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, "%EG", 'G', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "G",   'G', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OG", 'G', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "G",   'G', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "2024 35 三", "%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "2024 35 三", "%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, "35", 'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(CheckGet(obj, "35", 'U', 'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    CheckGet(obj, "%EU", 'U', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "U",   'U', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "U",   'U', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "2024 36 三", "%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "2024 36 三", "%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, "36", 'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(CheckGet(obj, "36", 'W', 'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    CheckGet(obj, "%EW", 'W', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "W",   'W', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "W",   'W', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "36", 'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    CheckGet(obj, "54",  'V', 'O', IOv2::ios_defs::strfailbit, 1);
    CheckGet(obj, "36",  'V', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "%EV", 'V', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "V",   'V', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "V",   'V', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "3", 'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, "3", 'w', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, "%Ew", 'w', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "w",   'w', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "w",   'w', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "24", 'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, "24", 'y', 'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, "24", 'y', 'O', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, "y",  'y', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "y",  'y', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "2024", 'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, "2024", 'Y', 'E', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, "Y",   'Y', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OY", 'Y', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "Y",   'Y', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(zone_is(CheckGet(obj, "America/Los_Angeles", 'Z', 0, IOv2::ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, "PST", 'Z', 0, IOv2::ios_defs::eofbit); VERIFY(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, "America/Los_Angexes", 'Z', 0, IOv2::ios_defs::strfailbit);
    CheckGet(obj, "%EZ", 'Z', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "Z",   'Z', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OZ", 'Z', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "Z",   'Z', 'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, "Z", 'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, "+13", 'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, "-1110", 'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, "+11:10", 'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, "%Ez", 'z', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "z",  'z', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oz", 'z', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "z",  'z', 'O', IOv2::ios_defs::strfailbit, 0);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(CheckGet<year_month_day>(obj, "1999-W52-6", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, "2019-W01-1", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, "1999-W52-5", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, "99-W52-6", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, "19-W01-1", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, "99-W52-5", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, "20 24/09/04", "%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    VERIFY(CheckGet<year_month_day>(obj, "20 01 01", "%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}});

    dump_info("Done\n");
}

void test_timeio_char_get_3()
{
    dump_info("Test timeio<char> get 3...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));

    CheckGet(obj, "%",  '%',  0,  IOv2::ios_defs::eofbit);
    CheckGet(obj, "x",  '%',  0,  IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%",  '%', 'E', febit);
    CheckGet(obj, "%E%", '%', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "%",  '%', 'O', febit);
    CheckGet(obj, "%O%", '%', 'O', IOv2::ios_defs::eofbit);

    VERIFY(CheckGet(obj, "水", 'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, "%Ea", 'a', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "a",   'a', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oa", 'a', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "a",   'a', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "水曜日", 'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, "%EA", 'A', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "A",   'A', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OA", 'A', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "A",   'A', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "9月", 'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, "%Eb", 'b', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "b",   'b', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Ob", 'b', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "b",   'b', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "9月", 'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, "%EB", 'B', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "B",   'B', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OB", 'B', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "B",   'B', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "9月", 'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, "%Eh", 'h', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "h",   'h', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oh", 'h', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "h",   'h', 'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(CheckGet<year_month_day>(obj, "2024年09月04日 13時33分18秒", 'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "令和6年09月04日 13時33分18秒", 'c', 'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "202409月04日 13時33分18秒", 'c', 'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "c",   'c', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oc", 'c', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "c",   'c', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "20", 'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(CheckGet<year_month_day>(obj, "平成", 'C', 'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1990));
    CheckGet(obj, "C",   'C', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OC", 'C', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "C",   'C', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "04", 'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, "04", 'd', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, "四", 'd', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, "%Ed", 'd', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "d",   'd', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "d",   'd', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "4", 'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, "4", 'e', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, "四", 'e', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, "%Ee", 'e', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "e",   'e', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "e",   'e', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "2024-09-04", 'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "%EF", 'F', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "F",   'F', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OF", 'F', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "F",   'F', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "2024年09月04日", 'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "令和6年09月04日", 'x', 'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "202409月04日", 'x', 'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "x",   'x', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Ox", 'x', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "x",   'x', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "09/04/24", 'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "%ED", 'D', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "D",   'D', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OD", 'D', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "D",   'D', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "13", 'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, "13", 'H', 'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, "十三", 'H', 'O', IOv2::ios_defs::eofbit).m_hour == 13);
    CheckGet(obj, "%EH", 'H', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "H",   'H', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "H",   'H', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "01", 'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, "01", 'I', 'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, "一", 'I', 'O', IOv2::ios_defs::eofbit).m_hour == 1);
    CheckGet(obj, "%EI", 'I', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "I",   'I', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "I",   'I', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "248", 'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(CheckGet<year_month_day>(obj, "2024 248", "%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, "%Ej", 'j', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "j",   'j', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oj", 'j', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "j",   'j', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "09", 'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, "09", 'm', 'O', IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, "九", 'm', 'O', IOv2::ios_defs::eofbit).m_month == 9);
    CheckGet(obj, "%Em", 'm', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "m",   'm', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "m",   'm', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "33", 'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, "33", 'M', 'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, "三十三", 'M', 'O', IOv2::ios_defs::eofbit).m_minute == 33);
    CheckGet(obj, "%EM", 'M', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "M",   'M', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "M",   'M', 'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, "\n",   'n',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, "x",    'n',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, "\n",   'n', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%En",  'n', 'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, "n",    'n', 'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%On",  'n', 'O', IOv2::ios_defs::eofbit, 3);

    CheckGet(obj, "\t",   't',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, "x",    't',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, "\t",   't', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Et",  't', 'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, "n",    't', 'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Ot",  't', 'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 午後", "%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 午前", "%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(CheckGet(obj, "午後", 'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(CheckGet(obj, "午前", 'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    CheckGet(obj, "%Ep", 'p', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "p",   'p', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Op", 'p', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "p",   'p', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "午後01時33分18秒", "%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "%Er", 'r', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "r",   'r', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Or", 'r', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "r",   'r', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33", "%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, "%ER", 'R', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "R",   'R', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OR", 'R', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "R",   'R', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "18", 'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, "18", 'S', 'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, "十八", 'S', 'O', IOv2::ios_defs::eofbit).m_second == 18);
    CheckGet(obj, "%ES", 'S', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "S",   'S', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "S",   'S', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13時33分18秒", "%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13時33分18秒", "%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "X",   'X', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OX", 'X', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "X",   'X', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18", "%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, "%ET", 'T', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "T",   'T', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OT", 'T', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "T",   'T', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "3", 'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, "3", 'u', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, "三", 'u', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, "%Eu", 'u', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "u",   'u', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "u",   'u', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "24", 'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, "%Eg", 'g', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "g",   'g', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Og", 'g', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "g",   'g', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "2024", 'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, "%EG", 'G', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "G",   'G', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OG", 'G', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "G",   'G', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "2024 35 水", "%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "2024 35 水", "%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "2024 三十五 水", "%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, "35", 'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(CheckGet(obj, "35", 'U', 'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    CheckGet(obj, "%EU", 'U', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "U",   'U', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "U",   'U', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, "2024 36 水", "%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "2024 36 水", "%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "2024 三十六 水", "%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, "36", 'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(CheckGet(obj, "36", 'W', 'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    CheckGet(obj, "%EW", 'W', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "W",   'W', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "W",   'W', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "36", 'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(CheckGet(obj, "36", 'V', 'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(CheckGet(obj, "三十六", 'V', 'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    CheckGet(obj, "54",  'V', 'O', IOv2::ios_defs::strfailbit, 1);
    CheckGet(obj, "%EV", 'V', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "V",   'V', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "V",   'V', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "3", 'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, "3", 'w', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, "三", 'w', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, "%Ew", 'w', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "w",   'w', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "w",   'w', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "24", 'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet<year_month_day>(obj, "6", 'y', 'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(2024));
    VERIFY(CheckGet(obj, "24", 'y', 'O', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, "二十四", 'y', 'O', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, "y",  'y', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "y",  'y', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, "2024", 'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, "2024", 'Y', 'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet<year_month_day>(obj, "平成3年", 'Y', 'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1991));
    CheckGet(obj, "Y",   'Y', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OY", 'Y', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "Y",   'Y', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(zone_is(CheckGet(obj, "America/Los_Angeles", 'Z', 0, IOv2::ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = CheckGet(obj, "PST", 'Z', 0, IOv2::ios_defs::eofbit); VERIFY(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    CheckGet(obj, "America/Los_Angexes", 'Z', 0, IOv2::ios_defs::strfailbit);
    CheckGet(obj, "%EZ", 'Z', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "Z",   'Z', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%OZ", 'Z', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "Z",   'Z', 'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, "Z", 'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, "+13", 'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, "-1110", 'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, "+11:10", 'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, "%Ez", 'z', 'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, "z",  'z', 'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, "%Oz", 'z', 'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, "z",  'z', 'O', IOv2::ios_defs::strfailbit, 0);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(CheckGet<year_month_day>(obj, "1999-W52-6", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, "2019-W01-1", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, "1999-W52-5", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, "99-W52-6", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, "19-W01-1", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, "99-W52-5", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, "20 24/09/04", "%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    VERIFY(CheckGet<year_month_day>(obj, "20 01 01", "%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}});

    dump_info("Done\n");
}

void test_timeio_char_get_4()
{
    dump_info("Test timeio<char> get 4...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    {
        std::string input = "d 2014-04-14 01:09:35";
        std::string format = "d %Y-%m-%d %H:%M:%S";
        
        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 114);
        VERIFY(time.tm_mon == 3);
        VERIFY(time.tm_mday == 14);
        VERIFY(time.tm_hour == 1);
        VERIFY(time.tm_min == 9);
        VERIFY(time.tm_sec == 35);
    }

    {
        std::string input = "2020  ";
        std::string format = "%Y";
        
        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret != input.end());
        VERIFY(time.tm_year == 120);
    }

    {
        std::string input = "2014-04-14 01:09:35";
        std::string format = "%";
        
        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::string input = "2020";
        
        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, 'Y');
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_year == 120);
        VERIFY(ret == input.end());
    }

    {
        std::string input = "year: 1970";
        std::string format = "jahr: %Y";
        
        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    dump_info("Done\n");
}

void test_timeio_char_get_5()
{
    dump_info("Test timeio<char> get 5...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("de_DE.UTF-8"));
    {
        std::string input = "Montag, den 14. April 2014";
        std::string format = "%A, den %d. %B %Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 114);
        VERIFY(time.tm_mon == 3);
        VERIFY(time.tm_wday == 1);
        VERIFY(time.tm_mday == 14);
    }
    {
        std::string input = "Mittwoch";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, 'A');
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 3);
    }

    dump_info("Done\n");
}

void test_timeio_char_get_6()
{
    dump_info("Test timeio<char> get 6...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    {
        std::string input = "Mon";
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 1);
    }

    {
        std::string input = "Tue ";
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_wday == 2);
    }

    {
        std::string input = "Wednesday";
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 3);
    }

    {
        std::string input = "Thu";
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 4);
    }

    {
        std::string input = "Fri ";
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_wday == 5);
    }

    {
        std::string input = "Saturday";
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 6);
    }

    {
        std::string input = "Feb";
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 1);
    }

    {
        std::string input = "Mar ";
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_mon == 2);
    }

    {
        std::string input = "April";
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 3);
    }

    {
        std::string input = "May";
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 4);
    }

    {
        std::string input = "Jun ";
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_mon == 5);
    }

    {
        std::string input = "July";
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 6);
    }

    {
        std::string input = "Aug";
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 7);
    }

    {
        std::string input = "May ";
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_mon == 4);
    }

    {
        std::string input = "October";
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 9);
    }

    // Other tests.
    {
        std::string input = "2.";
        std::string format = "%d.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mday == 2);
    }

    {
        std::string input = "0.";
        std::string format = "%d.";

        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::string input = "32.";
        std::string format = "%d.";

        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::string input = "5.";
        std::string format = "%e.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_mday == 5);
    }

    {
        std::string input = "06.";
        std::string format = "%e.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_mday == 6);
    }

    {
        std::string input = "0";
        std::string format = "%e";

        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::string input = "35";
        std::string format = "%e";

        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::string input = "12:00AM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 0);
        VERIFY(time.tm_min == 0);
    }

    {
        std::string input = "12:37AM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 0);
        VERIFY(time.tm_min == 37);
    }

    {
        std::string input = "01:25AM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 1);
        VERIFY(time.tm_min == 25);
    }

    {
        std::string input = "12:00PM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 12);
        VERIFY(time.tm_min == 0);
    }

    {
        std::string input = "12:42PM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 12);
        VERIFY(time.tm_min == 42);
    }

    {
        std::string input = "07:23PM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 19);
        VERIFY(time.tm_min == 23);
    }

    {
        std::string input = "17%20";
        std::string format = "%H%%%M";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 17);
        VERIFY(time.tm_min == 20);
    }

    {
        std::string input = "24:30";
        std::string format = "%H:%M";

        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::string input = "Novembur";
        std::string format = "%bembur";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_mon == 10);
    }

    dump_info("Done\n");
}

void test_timeio_char_get_7()
{
    dump_info("Test timeio<char> get 7...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    {
        std::string input = "PM01:38:12";
        std::string format = "%p%I:%M:%S";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_hour == 13);
        VERIFY(time.tm_min == 38);
        VERIFY(time.tm_sec == 12);
    }

    {
        std::string input = "05 37";
        std::string format = "%C %y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 537 - 1900);
    }

    {
        std::string input = "68";
        std::string format = "%y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2068 - 1900);
    }

    {
        std::string input = "69";
        std::string format = "%y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 1969 - 1900);
    }

    {
        std::string input = "03-Feb-2003";
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2003 - 1900);
        VERIFY(time.tm_mon == 1);
        VERIFY(time.tm_mday == 3);
        VERIFY(time.tm_wday == 1);
        VERIFY(time.tm_yday == 33);
    }

    {
        std::string input = "16-Dec-2020";
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2020 - 1900);
        VERIFY(time.tm_mon == 11);
        VERIFY(time.tm_mday == 16);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 350);
    }

    {
        std::string input = "16-Dec-2021";
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2021 - 1900);
        VERIFY(time.tm_mon == 11);
        VERIFY(time.tm_mday == 16);
        VERIFY(time.tm_wday == 4);
        VERIFY(time.tm_yday == 349);
    }

    {
        std::string input = "253 2020";
        std::string format = "%j %Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2020 - 1900);
        VERIFY(time.tm_mon == 8);
        VERIFY(time.tm_mday == 9);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 252);
    }

    {
        std::string input = "233 2021";
        std::string format = "%j %Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2021 - 1900);
        VERIFY(time.tm_mon == 7);
        VERIFY(time.tm_mday == 21);
        VERIFY(time.tm_wday == 6);
        VERIFY(time.tm_yday == 232);
    }

    {
        std::string input = "2020 23 3";
        std::string format = "%Y %U %w";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2020 - 1900);
        VERIFY(time.tm_mon == 5);
        VERIFY(time.tm_mday == 10);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 161);
    }

    {
        std::string input = "2020 23 3";
        std::string format = "%Y %W %w";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2020 - 1900);
        VERIFY(time.tm_mon == 5);
        VERIFY(time.tm_mday == 10);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 161);
    }

    {
        std::string input = "2021 43 Fri";
        std::string format = "%Y %W %a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2021 - 1900);
        VERIFY(time.tm_mon == 9);
        VERIFY(time.tm_mday == 29);
        VERIFY(time.tm_wday == 5);
        VERIFY(time.tm_yday == 301);
    }

    {
        std::string input = "2024 23 3";
        std::string format = "%Y %U %w";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2024 - 1900);
        VERIFY(time.tm_mon == 5);
        VERIFY(time.tm_mday == 12);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 163);
    }

    {
        std::string input = "2024 23 3";
        std::string format = "%Y %W %w";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2024 - 1900);
        VERIFY(time.tm_mon == 5);
        VERIFY(time.tm_mday == 5);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 156);
    }

    dump_info("Done\n");
}

void test_timeio_char_get_8()
{
    dump_info("Test timeio<char> get 8...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    {
        std::string input = "01:38:12 PM";
        std::string format = "%I:%M:%S %p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_hour == 13);
        VERIFY(time.tm_min == 38);
        VERIFY(time.tm_sec == 12);
    }
        
    {
        std::string input = "11:17:42 PM";
        std::string format = "%r";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_hour == 23);
        VERIFY(time.tm_min == 17);
        VERIFY(time.tm_sec == 42);
    }

    dump_info("Done\n");
}

void test_timeio_char_get_9()
{
    dump_info("Test timeio<char> get 9...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    {
        using namespace IOv2;
        streambuf sb(mem_device{"d 2014-04-14 01:09:35"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "d %Y-%m-%d %H:%M:%S";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 114);
        VERIFY(time.tm_mon == 3);
        VERIFY(time.tm_mday == 14);
        VERIFY(time.tm_hour == 1);
        VERIFY(time.tm_min == 9);
        VERIFY(time.tm_sec == 35);
    }

    {
        using namespace IOv2;
        streambuf sb(mem_device{"2020  "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret != std::default_sentinel);
        VERIFY(time.tm_year == 120);
    }

    {
        using namespace IOv2;
        streambuf sb(mem_device{"2014-04-14 01:09:35"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%";

        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(beg, std::default_sentinel, ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        using namespace IOv2;
        streambuf sb(mem_device{"2020"});
        auto beg = istreambuf_iterator(sb);

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, 'Y');
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_year == 120);
        VERIFY(ret == std::default_sentinel);
    }

    {
        using namespace IOv2;
        streambuf sb(mem_device{"year: 1970"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "jahr: %Y";

        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(beg, std::default_sentinel, ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    dump_info("Done\n");
}

void test_timeio_char_get_10()
{
    dump_info("Test timeio<char> get 10...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("de_DE.UTF-8"));

    {
        using namespace IOv2;
        streambuf sb(mem_device{"Montag, den 14. April 2014"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A, den %d. %B %Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 114);
        VERIFY(time.tm_mon == 3);
        VERIFY(time.tm_wday == 1);
        VERIFY(time.tm_mday == 14);
    }
    {
        using namespace IOv2;
        streambuf sb(mem_device{"Mittwoch"});
        auto beg = istreambuf_iterator(sb);

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, 'A');
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_wday == 3);
    }

    dump_info("Done\n");
}

void test_timeio_char_get_11()
{
    dump_info("Test timeio<char> get 11...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    using namespace IOv2;
    {
        streambuf sb(mem_device{"Mon"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_wday == 1);
    }

    {
        streambuf sb(mem_device{"Tue "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(!((ret == std::default_sentinel) || (*ret != ' ')));
        VERIFY(time.tm_wday == 2);
    }

    {
        streambuf sb(mem_device{"Wednesday"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_wday == 3);
    }

    {
        streambuf sb(mem_device{"Thu"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_wday == 4);
    }

    {
        streambuf sb(mem_device{"Fri "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(!((ret == std::default_sentinel) || (*ret != ' ')));
        VERIFY(time.tm_wday == 5);
    }

    {
        streambuf sb(mem_device{"Saturday"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_wday == 6);
    }

    {
        streambuf sb(mem_device{"Feb"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_mon == 1);
    }

    {
        streambuf sb(mem_device{"Mar "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(!((ret == std::default_sentinel) || (*ret != ' ')));
        VERIFY(time.tm_mon == 2);
    }

    {
        streambuf sb(mem_device{"April"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_mon == 3);
    }

    {
        streambuf sb(mem_device{"May"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_mon == 4);
    }

    {
        streambuf sb(mem_device{"Jun "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(!((ret == std::default_sentinel) || (*ret != ' ')));
        VERIFY(time.tm_mon == 5);
    }

    {
        streambuf sb(mem_device{"July"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_mon == 6);
    }

    {
        streambuf sb(mem_device{"Aug"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_mon == 7);
    }

    {
        streambuf sb(mem_device{"May "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(!((ret == std::default_sentinel) || (*ret != ' ')));
        VERIFY(time.tm_mon == 4);
    }

    {
        streambuf sb(mem_device{"October"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_mon == 9);
    }

    // Other tests.
    {
        streambuf sb(mem_device{"2."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_mday == 2);
    }

    {
        streambuf sb(mem_device{"0."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d.";

        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(beg, std::default_sentinel, ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        streambuf sb(mem_device{"32."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d.";

        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(beg, std::default_sentinel, ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        streambuf sb(mem_device{"5."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%e.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        VERIFY(ret == std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_mday == 5);
    }

    {
        streambuf sb(mem_device{"06."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%e.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        VERIFY(ret == std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_mday == 6);
    }

    {
        streambuf sb(mem_device{"0"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%e";

        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(beg, std::default_sentinel, ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        streambuf sb(mem_device{"35"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%e";

        IOv2::time_parse_context<char> ctx;
        try
        {
            obj.get(beg, std::default_sentinel, ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        streambuf sb(mem_device{"12:00AM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        VERIFY(ret == std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 0);
        VERIFY(time.tm_min == 0);
    }

    {
        streambuf sb(mem_device{"12:37AM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        VERIFY(ret == std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 0);
        VERIFY(time.tm_min == 37);
    }

    {
        streambuf sb(mem_device{"01:25AM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        VERIFY(ret == std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 1);
        VERIFY(time.tm_min == 25);
    }

    {
        streambuf sb(mem_device{"12:00PM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        VERIFY(ret == std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 12);
        VERIFY(time.tm_min == 0);
    }

    {
        streambuf sb(mem_device{"12:42PM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        VERIFY(ret == std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 12);
        VERIFY(time.tm_min == 42);
    }

    {
        streambuf sb(mem_device{"07:23PM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        VERIFY(ret == std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 19);
        VERIFY(time.tm_min == 23);
    }

    {
        streambuf sb(mem_device{"17%20"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%H%%%M";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        VERIFY(ret == std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_hour == 17);
        VERIFY(time.tm_min == 20);
    }

    {
        streambuf sb(mem_device{"24:30"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%H:%M";

        IOv2::time_parse_context<char> ctx;
        auto it = beg;
        try
        {
            it = obj.get(beg, std::default_sentinel, ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        VERIFY(!((it == std::default_sentinel) || (*it != '4')));
    }

    {
        streambuf sb(mem_device{"Novembur"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%bembur";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        VERIFY(ret == std::default_sentinel);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(time.tm_mon == 10);
    }

    dump_info("Done\n");
}

void test_timeio_char_get_12()
{
    dump_info("Test timeio<char> get 12...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));

    using namespace IOv2;
    {
        streambuf sb(mem_device{"PM01:38:12"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%p%I:%M:%S";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_hour == 13);
        VERIFY(time.tm_min == 38);
        VERIFY(time.tm_sec == 12);
    }

    {
        streambuf sb(mem_device{"05 37"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%C %y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 537 - 1900);
    }

    {
        streambuf sb(mem_device{"68"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 2068 - 1900);
    }

    {
        streambuf sb(mem_device{"69"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 1969 - 1900);
    }

    {
        streambuf sb(mem_device{"03-Feb-2003"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 2003 - 1900);
        VERIFY(time.tm_mon == 1);
        VERIFY(time.tm_mday == 3);
        VERIFY(time.tm_wday == 1);
        VERIFY(time.tm_yday == 33);
    }

    {
        streambuf sb(mem_device{"16-Dec-2020"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 2020 - 1900);
        VERIFY(time.tm_mon == 11);
        VERIFY(time.tm_mday == 16);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 350);
    }

    {
        streambuf sb(mem_device{"16-Dec-2021"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 2021 - 1900);
        VERIFY(time.tm_mon == 11);
        VERIFY(time.tm_mday == 16);
        VERIFY(time.tm_wday == 4);
        VERIFY(time.tm_yday == 349);
    }

    {
        streambuf sb(mem_device{"253 2020"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%j %Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 2020 - 1900);
        VERIFY(time.tm_mon == 8);
        VERIFY(time.tm_mday == 9);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 252);
    }

    {
        streambuf sb(mem_device{"233 2021"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%j %Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 2021 - 1900);
        VERIFY(time.tm_mon == 7);
        VERIFY(time.tm_mday == 21);
        VERIFY(time.tm_wday == 6);
        VERIFY(time.tm_yday == 232);
    }

    {
        streambuf sb(mem_device{"2020 23 3"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%Y %U %w";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 2020 - 1900);
        VERIFY(time.tm_mon == 5);
        VERIFY(time.tm_mday == 10);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 161);
    }

    {
        streambuf sb(mem_device{"2020 23 3"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%Y %W %w";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 2020 - 1900);
        VERIFY(time.tm_mon == 5);
        VERIFY(time.tm_mday == 10);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 161);
    }

    {
        streambuf sb(mem_device{"2021 43 Fri"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%Y %W %a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 2021 - 1900);
        VERIFY(time.tm_mon == 9);
        VERIFY(time.tm_mday == 29);
        VERIFY(time.tm_wday == 5);
        VERIFY(time.tm_yday == 301);
    }

    {
        streambuf sb(mem_device{"2024 23 3"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%Y %U %w";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 2024 - 1900);
        VERIFY(time.tm_mon == 5);
        VERIFY(time.tm_mday == 12);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 163);
    }

    {
        streambuf sb(mem_device{"2024 23 3"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%Y %W %w";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 2024 - 1900);
        VERIFY(time.tm_mon == 5);
        VERIFY(time.tm_mday == 5);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 156);
    }

    dump_info("Done\n");
}

void test_timeio_char_get_13()
{
    dump_info("Test timeio<char> get 13...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    using namespace IOv2;
    {
        streambuf sb(mem_device{"01:38:12 PM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%I:%M:%S %p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_hour == 13);
        VERIFY(time.tm_min == 38);
        VERIFY(time.tm_sec == 12);
    }

    {
        streambuf sb(mem_device{"11:17:42 PM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%r";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = ctx_to<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_hour == 23);
        VERIFY(time.tm_min == 17);
        VERIFY(time.tm_sec == 42);
    }

    dump_info("Done\n");
}

void test_timeio_char_get_14()
{
    dump_info("Test timeio<char> get 14...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char, true, true, IOv2::tz_level::none>, true, true, IOv2::tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, true, IOv2::tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri("%",  '%',  0,  IOv2::ios_defs::eofbit);
    FOri("x",  '%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri("%",  '%', 'E', febit);
    FOri("%E%", '%', 'E', IOv2::ios_defs::eofbit);
    FOri("%",  '%', 'O', febit);
    FOri("%O%", '%', 'O', IOv2::ios_defs::eofbit);

    VERIFY(FOri("水", 'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri("%Ea", 'a', 'E', IOv2::ios_defs::eofbit);
    FOri("a",   'a', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oa", 'a', 'O', IOv2::ios_defs::eofbit);
    FOri("a",   'a', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("水曜日", 'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri("%EA", 'A', 'E', IOv2::ios_defs::eofbit);
    FOri("A",   'A', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OA", 'A', 'O', IOv2::ios_defs::eofbit);
    FOri("A",   'A', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("9月", 'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri("%Eb", 'b', 'E', IOv2::ios_defs::eofbit);
    FOri("b",   'b', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Ob", 'b', 'O', IOv2::ios_defs::eofbit);
    FOri("b",   'b', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("9月", 'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri("%EB", 'B', 'E', IOv2::ios_defs::eofbit);
    FOri("B",   'B', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OB", 'B', 'O', IOv2::ios_defs::eofbit);
    FOri("B",   'B', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("9月", 'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri("%Eh", 'h', 'E', IOv2::ios_defs::eofbit);
    FOri("h",   'h', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oh", 'h', 'O', IOv2::ios_defs::eofbit);
    FOri("h",   'h', 'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(FYmd("2024年09月04日 13時33分18秒", 'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("令和6年09月04日 13時33分18秒", 'c', 'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("202409月04日 13時33分18秒", 'c', 'E', IOv2::ios_defs::eofbit) == check_date1);
    FOri("c",   'c', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oc", 'c', 'O', IOv2::ios_defs::eofbit);
    FOri("c",   'c', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("20", 'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(FYmd("平成", 'C', 'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1990));
    FOri("C",   'C', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OC", 'C', 'O', IOv2::ios_defs::eofbit);
    FOri("C",   'C', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("04", 'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri("04", 'd', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri("四", 'd', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri("%Ed", 'd', 'E', IOv2::ios_defs::eofbit);
    FOri("d",   'd', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("d",   'd', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("4", 'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri("4", 'e', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri("四", 'e', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri("%Ee", 'e', 'E', IOv2::ios_defs::eofbit);
    FOri("e",   'e', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("e",   'e', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd("2024-09-04", 'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri("%EF", 'F', 'E', IOv2::ios_defs::eofbit);
    FOri("F",   'F', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OF", 'F', 'O', IOv2::ios_defs::eofbit);
    FOri("F",   'F', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd("2024年09月04日", 'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("令和6年09月04日", 'x', 'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("202409月04日", 'x', 'E', IOv2::ios_defs::eofbit) == check_date1);
    FOri("x",   'x', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Ox", 'x', 'O', IOv2::ios_defs::eofbit);
    FOri("x",   'x', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd("09/04/24", 'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri("%ED", 'D', 'E', IOv2::ios_defs::eofbit);
    FOri("D",   'D', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OD", 'D', 'O', IOv2::ios_defs::eofbit);
    FOri("D",   'D', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("13", 'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri("13", 'H', 'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri("十三", 'H', 'O', IOv2::ios_defs::eofbit).m_hour == 13);
    FOri("%EH", 'H', 'E', IOv2::ios_defs::eofbit);
    FOri("H",   'H', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("H",   'H', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("01", 'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri("01", 'I', 'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri("一", 'I', 'O', IOv2::ios_defs::eofbit).m_hour == 1);
    FOri("%EI", 'I', 'E', IOv2::ios_defs::eofbit);
    FOri("I",   'I', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("I",   'I', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("248", 'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(FYmd("2024 248", "%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    FOri("%Ej", 'j', 'E', IOv2::ios_defs::eofbit);
    FOri("j",   'j', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oj", 'j', 'O', IOv2::ios_defs::eofbit);
    FOri("j",   'j', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("09", 'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri("09", 'm', 'O', IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri("九", 'm', 'O', IOv2::ios_defs::eofbit).m_month == 9);
    FOri("%Em", 'm', 'E', IOv2::ios_defs::eofbit);
    FOri("m",   'm', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("m",   'm', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("33", 'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri("33", 'M', 'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri("三十三", 'M', 'O', IOv2::ios_defs::eofbit).m_minute == 33);
    FOri("%EM", 'M', 'E', IOv2::ios_defs::eofbit);
    FOri("M",   'M', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("M",   'M', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("\n",   'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri("x",    'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri("\n",   'n', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%En",  'n', 'E', IOv2::ios_defs::eofbit, 3);
    FOri("n",    'n', 'O', IOv2::ios_defs::strfailbit, 0);
    FOri("%On",  'n', 'O', IOv2::ios_defs::eofbit, 3);

    FOri("\t",   't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri("x",    't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri("\t",   't', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Et",  't', 'E', IOv2::ios_defs::eofbit, 3);
    FOri("n",    't', 'O', IOv2::ios_defs::strfailbit, 0);
    FOri("%Ot",  't', 'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 午後", "%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "01 午前", "%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(FOri("午後", 'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(FOri("午前", 'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    FOri("%Ep", 'p', 'E', IOv2::ios_defs::eofbit);
    FOri("p",   'p', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Op", 'p', 'O', IOv2::ios_defs::eofbit);
    FOri("p",   'p', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "午後01時33分18秒", "%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("%Er", 'r', 'E', IOv2::ios_defs::eofbit);
    FOri("r",   'r', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Or", 'r', 'O', IOv2::ios_defs::eofbit);
    FOri("r",   'r', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33", "%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri("%ER", 'R', 'E', IOv2::ios_defs::eofbit);
    FOri("R",   'R', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OR", 'R', 'O', IOv2::ios_defs::eofbit);
    FOri("R",   'R', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("18", 'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri("18", 'S', 'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri("十八", 'S', 'O', IOv2::ios_defs::eofbit).m_second == 18);
    FOri("%ES", 'S', 'E', IOv2::ios_defs::eofbit);
    FOri("S",   'S', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("S",   'S', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13時33分18秒", "%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13時33分18秒", "%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("X",   'X', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OX", 'X', 'O', IOv2::ios_defs::eofbit);
    FOri("X",   'X', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18", "%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("%ET", 'T', 'E', IOv2::ios_defs::eofbit);
    FOri("T",   'T', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OT", 'T', 'O', IOv2::ios_defs::eofbit);
    FOri("T",   'T', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("3", 'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri("3", 'u', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri("三", 'u', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri("%Eu", 'u', 'E', IOv2::ios_defs::eofbit);
    FOri("u",   'u', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("u",   'u', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("24", 'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri("%Eg", 'g', 'E', IOv2::ios_defs::eofbit);
    FOri("g",   'g', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Og", 'g', 'O', IOv2::ios_defs::eofbit);
    FOri("g",   'g', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("2024", 'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri("%EG", 'G', 'E', IOv2::ios_defs::eofbit);
    FOri("G",   'G', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OG", 'G', 'O', IOv2::ios_defs::eofbit);
    FOri("G",   'G', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd("2024 35 水", "%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("2024 35 水", "%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("2024 三十五 水", "%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri("35", 'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(FOri("35", 'U', 'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    FOri("%EU", 'U', 'E', IOv2::ios_defs::eofbit);
    FOri("U",   'U', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("U",   'U', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd("2024 36 水", "%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("2024 36 水", "%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("2024 三十六 水", "%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri("36", 'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(FOri("36", 'W', 'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    FOri("%EW", 'W', 'E', IOv2::ios_defs::eofbit);
    FOri("W",   'W', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("W",   'W', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("36", 'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri("36", 'V', 'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri("三十六", 'V', 'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    FOri("54",  'V', 'O', IOv2::ios_defs::strfailbit, 1);
    FOri("%EV", 'V', 'E', IOv2::ios_defs::eofbit);
    FOri("V",   'V', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("V",   'V', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("3", 'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri("3", 'w', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri("三", 'w', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri("%Ew", 'w', 'E', IOv2::ios_defs::eofbit);
    FOri("w",   'w', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("w",   'w', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("24", 'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd("6", 'y', 'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(2024));
    VERIFY(FOri("24", 'y', 'O', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri("二十四", 'y', 'O', IOv2::ios_defs::eofbit).m_year == 2024);
    FOri("y",  'y', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("y",  'y', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("2024", 'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri("2024", 'Y', 'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd("平成3年", 'Y', 'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1991));
    FOri("Y",   'Y', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OY", 'Y', 'O', IOv2::ios_defs::eofbit);
    FOri("Y",   'Y', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%Z", 'Z', 0, IOv2::ios_defs::eofbit);
    FOri("%EZ", 'Z', 'E', IOv2::ios_defs::eofbit);
    FOri("Z",   'Z', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OZ", 'Z', 'O', IOv2::ios_defs::eofbit);
    FOri("Z",   'Z', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%z", 'z', 0, IOv2::ios_defs::eofbit);
    FOri("%Ez", 'z', 'E', IOv2::ios_defs::eofbit);
    FOri("%Oz", 'z', 'O', IOv2::ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(FYmd("1999-W52-6", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd("2019-W01-1", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd("1999-W52-5", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd("99-W52-6", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd("19-W01-1", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd("99-W52-5", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd("20 24/09/04", "%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    VERIFY(FYmd("20 01 01", "%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}});

    dump_info("Done\n");
}

void test_timeio_char_get_15()
{
    dump_info("Test timeio<char> get 15...");
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char, true, false, IOv2::tz_level::none>, true, false, IOv2::tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, false, IOv2::tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri("%",  '%',  0,  IOv2::ios_defs::eofbit);
    FOri("x",  '%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri("%",  '%', 'E', febit);
    FOri("%E%", '%', 'E', IOv2::ios_defs::eofbit);
    FOri("%",  '%', 'O', febit);
    FOri("%O%", '%', 'O', IOv2::ios_defs::eofbit);

    VERIFY(FOri("水", 'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri("%Ea", 'a', 'E', IOv2::ios_defs::eofbit);
    FOri("a",   'a', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oa", 'a', 'O', IOv2::ios_defs::eofbit);
    FOri("a",   'a', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("水曜日", 'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri("%EA", 'A', 'E', IOv2::ios_defs::eofbit);
    FOri("A",   'A', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OA", 'A', 'O', IOv2::ios_defs::eofbit);
    FOri("A",   'A', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("9月", 'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri("%Eb", 'b', 'E', IOv2::ios_defs::eofbit);
    FOri("b",   'b', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Ob", 'b', 'O', IOv2::ios_defs::eofbit);
    FOri("b",   'b', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("9月", 'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri("%EB", 'B', 'E', IOv2::ios_defs::eofbit);
    FOri("B",   'B', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OB", 'B', 'O', IOv2::ios_defs::eofbit);
    FOri("B",   'B', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("9月", 'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri("%Eh", 'h', 'E', IOv2::ios_defs::eofbit);
    FOri("h",   'h', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oh", 'h', 'O', IOv2::ios_defs::eofbit);
    FOri("h",   'h', 'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    FYmd("%c", 'c', 0, IOv2::ios_defs::eofbit);
    FYmd("%Ec", 'c', 'E', IOv2::ios_defs::eofbit);
    FOri("c",   'c', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oc", 'c', 'O', IOv2::ios_defs::eofbit);
    FOri("c",   'c', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("20", 'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(FYmd("平成", 'C', 'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1990));
    FOri("C",   'C', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OC", 'C', 'O', IOv2::ios_defs::eofbit);
    FOri("C",   'C', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("04", 'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri("04", 'd', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri("四", 'd', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri("%Ed", 'd', 'E', IOv2::ios_defs::eofbit);
    FOri("d",   'd', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("d",   'd', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("4", 'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri("4", 'e', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri("四", 'e', 'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri("%Ee", 'e', 'E', IOv2::ios_defs::eofbit);
    FOri("e",   'e', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("e",   'e', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd("2024-09-04", 'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri("%EF", 'F', 'E', IOv2::ios_defs::eofbit);
    FOri("F",   'F', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OF", 'F', 'O', IOv2::ios_defs::eofbit);
    FOri("F",   'F', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd("2024年09月04日", 'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("令和6年09月04日", 'x', 'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("202409月04日", 'x', 'E', IOv2::ios_defs::eofbit) == check_date1);
    FOri("x",   'x', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Ox", 'x', 'O', IOv2::ios_defs::eofbit);
    FOri("x",   'x', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd("09/04/24", 'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri("%ED", 'D', 'E', IOv2::ios_defs::eofbit);
    FOri("D",   'D', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OD", 'D', 'O', IOv2::ios_defs::eofbit);
    FOri("D",   'D', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%H", 'H', 0,   IOv2::ios_defs::eofbit);
    FOri("%EH", 'H', 'E', IOv2::ios_defs::eofbit);
    FOri("H",   'H', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("H",   'H', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%I", 'I', 0,   IOv2::ios_defs::eofbit);
    FOri("%EI", 'I', 'E', IOv2::ios_defs::eofbit);
    FOri("I",   'I', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("I",   'I', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("248", 'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(FYmd("2024 248", "%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    FOri("%Ej", 'j', 'E', IOv2::ios_defs::eofbit);
    FOri("j",   'j', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oj", 'j', 'O', IOv2::ios_defs::eofbit);
    FOri("j",   'j', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("09", 'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri("09", 'm', 'O', IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri("九", 'm', 'O', IOv2::ios_defs::eofbit).m_month == 9);
    FOri("%Em", 'm', 'E', IOv2::ios_defs::eofbit);
    FOri("m",   'm', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("m",   'm', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%M", 'M', 0,   IOv2::ios_defs::eofbit);
    FOri("%OM", 'M', 'O', IOv2::ios_defs::eofbit);
    FOri("%EM", 'M', 'E', IOv2::ios_defs::eofbit);
    FOri("M",   'M', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("M",   'M', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("\n",   'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri("x",    'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri("\n",   'n', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%En",  'n', 'E', IOv2::ios_defs::eofbit, 3);
    FOri("n",    'n', 'O', IOv2::ios_defs::strfailbit, 0);
    FOri("%On",  'n', 'O', IOv2::ios_defs::eofbit, 3);

    FOri("\t",   't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri("x",    't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri("\t",   't', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Et",  't', 'E', IOv2::ios_defs::eofbit, 3);
    FOri("n",    't', 'O', IOv2::ios_defs::strfailbit, 0);
    FOri("%Ot",  't', 'O', IOv2::ios_defs::eofbit, 3);

    FOri("%p", 'p', 0, IOv2::ios_defs::eofbit);
    FOri("%Ep", 'p', 'E', IOv2::ios_defs::eofbit);
    FOri("p",   'p', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Op", 'p', 'O', IOv2::ios_defs::eofbit);
    FOri("p",   'p', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%r", "%r",  IOv2::ios_defs::eofbit);
    FOri("%Er", 'r', 'E', IOv2::ios_defs::eofbit);
    FOri("r",   'r', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Or", 'r', 'O', IOv2::ios_defs::eofbit);
    FOri("r",   'r', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33", "%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri("%ER", 'R', 'E', IOv2::ios_defs::eofbit);
    FOri("R",   'R', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OR", 'R', 'O', IOv2::ios_defs::eofbit);
    FOri("R",   'R', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%S", 'S', 0,   IOv2::ios_defs::eofbit);
    FOri("%OS", 'S', 'O', IOv2::ios_defs::eofbit);
    FOri("%ES", 'S', 'E', IOv2::ios_defs::eofbit);
    FOri("S",   'S', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("S",   'S', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%X", "%X",  IOv2::ios_defs::eofbit);
    FOri("%EX", "%EX",  IOv2::ios_defs::eofbit);
    FOri("X",   'X', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OX", 'X', 'O', IOv2::ios_defs::eofbit);
    FOri("X",   'X', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%T", "%T",  IOv2::ios_defs::eofbit);
    FOri("%ET", 'T', 'E', IOv2::ios_defs::eofbit);
    FOri("T",   'T', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OT", 'T', 'O', IOv2::ios_defs::eofbit);
    FOri("T",   'T', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("3", 'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri("3", 'u', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri("三", 'u', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri("%Eu", 'u', 'E', IOv2::ios_defs::eofbit);
    FOri("u",   'u', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("u",   'u', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("24", 'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri("%Eg", 'g', 'E', IOv2::ios_defs::eofbit);
    FOri("g",   'g', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Og", 'g', 'O', IOv2::ios_defs::eofbit);
    FOri("g",   'g', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("2024", 'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri("%EG", 'G', 'E', IOv2::ios_defs::eofbit);
    FOri("G",   'G', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OG", 'G', 'O', IOv2::ios_defs::eofbit);
    FOri("G",   'G', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd("2024 35 水", "%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("2024 35 水", "%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("2024 三十五 水", "%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri("35", 'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(FOri("35", 'U', 'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    FOri("%EU", 'U', 'E', IOv2::ios_defs::eofbit);
    FOri("U",   'U', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("U",   'U', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd("2024 36 水", "%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("2024 36 水", "%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd("2024 三十六 水", "%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri("36", 'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(FOri("36", 'W', 'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    FOri("%EW", 'W', 'E', IOv2::ios_defs::eofbit);
    FOri("W",   'W', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("W",   'W', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("36", 'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri("36", 'V', 'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri("三十六", 'V', 'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    FOri("54",  'V', 'O', IOv2::ios_defs::strfailbit, 1);
    FOri("%EV", 'V', 'E', IOv2::ios_defs::eofbit);
    FOri("V",   'V', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("V",   'V', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("3", 'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri("3", 'w', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri("三", 'w', 'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri("%Ew", 'w', 'E', IOv2::ios_defs::eofbit);
    FOri("w",   'w', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("w",   'w', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("24", 'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd("6", 'y', 'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(2024));
    VERIFY(FOri("24", 'y', 'O', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri("二十四", 'y', 'O', IOv2::ios_defs::eofbit).m_year == 2024);
    FOri("y",  'y', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("y",  'y', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("2024", 'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri("2024", 'Y', 'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd("平成3年", 'Y', 'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1991));
    FOri("Y",   'Y', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OY", 'Y', 'O', IOv2::ios_defs::eofbit);
    FOri("Y",   'Y', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%Z", 'Z', 0, IOv2::ios_defs::eofbit);
    FOri("%EZ", 'Z', 'E', IOv2::ios_defs::eofbit);
    FOri("Z",   'Z', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OZ", 'Z', 'O', IOv2::ios_defs::eofbit);
    FOri("Z",   'Z', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%z", 'z', 0, IOv2::ios_defs::eofbit);
    FOri("%Ez", 'z', 'E', IOv2::ios_defs::eofbit);
    FOri("%Oz", 'z', 'O', IOv2::ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(FYmd("1999-W52-6", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd("2019-W01-1", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd("1999-W52-5", "%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd("99-W52-6", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd("19-W01-1", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd("99-W52-5", "%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd("20 24/09/04", "%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    // %C with no year within the century: the year within the century is 0, as in
    // POSIX strptime -- not the wall-clock year, and not whatever the parse context
    // happens to fall back to.
    VERIFY(FYmd("20 01 01", "%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}});

    dump_info("Done\n");
}

void test_timeio_char_get_16()
{
    dump_info("Test timeio<char> get 16...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char, false, true, IOv2::tz_level::zone>, false, true, IOv2::tz_level::zone>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, IOv2::tz_level::zone>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri("%",  '%',  0,  IOv2::ios_defs::eofbit);
    FOri("x",  '%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri("%",  '%', 'E', febit);
    FOri("%E%", '%', 'E', IOv2::ios_defs::eofbit);
    FOri("%",  '%', 'O', febit);
    FOri("%O%", '%', 'O', IOv2::ios_defs::eofbit);

    FOri("%a", 'a', 0, IOv2::ios_defs::eofbit, 3);
    FOri("%Ea", 'a', 'E', IOv2::ios_defs::eofbit);
    FOri("a",   'a', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oa", 'a', 'O', IOv2::ios_defs::eofbit);
    FOri("a",   'a', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%A", 'A', 0, IOv2::ios_defs::eofbit, 3);
    FOri("%EA", 'A', 'E', IOv2::ios_defs::eofbit);
    FOri("A",   'A', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OA", 'A', 'O', IOv2::ios_defs::eofbit);
    FOri("A",   'A', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%b", 'b', 0, IOv2::ios_defs::eofbit, 3);
    FOri("%Eb", 'b', 'E', IOv2::ios_defs::eofbit);
    FOri("b",   'b', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Ob", 'b', 'O', IOv2::ios_defs::eofbit);
    FOri("b",   'b', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%B", 'B', 0, IOv2::ios_defs::eofbit, 3);
    FOri("%EB", 'B', 'E', IOv2::ios_defs::eofbit);
    FOri("B",   'B', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OB", 'B', 'O', IOv2::ios_defs::eofbit);
    FOri("B",   'B', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%h", 'h', 0, IOv2::ios_defs::eofbit, 3);
    FOri("%Eh", 'h', 'E', IOv2::ios_defs::eofbit);
    FOri("h",   'h', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oh", 'h', 'O', IOv2::ios_defs::eofbit);
    FOri("h",   'h', 'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    FOri("%c", 'c', 0, IOv2::ios_defs::eofbit);
    FOri("%Ec", 'c', 'E', IOv2::ios_defs::eofbit);
    FOri("c",   'c', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oc", 'c', 'O', IOv2::ios_defs::eofbit);
    FOri("c",   'c', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%C", 'C', 0,   IOv2::ios_defs::eofbit);
    FOri("%EC", 'C', 'E', IOv2::ios_defs::eofbit);
    FOri("C",   'C', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OC", 'C', 'O', IOv2::ios_defs::eofbit);
    FOri("C",   'C', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%d", 'd', 0,   IOv2::ios_defs::eofbit);
    FOri("%Od", 'd', 'O', IOv2::ios_defs::eofbit);
    FOri("%Ed", 'd', 'E', IOv2::ios_defs::eofbit);
    FOri("d",   'd', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("d",   'd', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%e", 'e', 0,   IOv2::ios_defs::eofbit);
    FOri("%Oe", 'e', 'O', IOv2::ios_defs::eofbit);
    FOri("%Ee", 'e', 'E', IOv2::ios_defs::eofbit);
    FOri("e",   'e', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("e",   'e', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%F", 'F', 0, IOv2::ios_defs::eofbit);
    FOri("%EF", 'F', 'E', IOv2::ios_defs::eofbit);
    FOri("F",   'F', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OF", 'F', 'O', IOv2::ios_defs::eofbit);
    FOri("F",   'F', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%x", 'x', 0, IOv2::ios_defs::eofbit);
    FOri("%Ex", 'x', 'E', IOv2::ios_defs::eofbit);
    FOri("x",   'x', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Ox", 'x', 'O', IOv2::ios_defs::eofbit);
    FOri("x",   'x', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%D", 'D', 0, IOv2::ios_defs::eofbit);
    FOri("%ED", 'D', 'E', IOv2::ios_defs::eofbit);
    FOri("D",   'D', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OD", 'D', 'O', IOv2::ios_defs::eofbit);
    FOri("D",   'D', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("13", 'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri("13", 'H', 'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri("十三", 'H', 'O', IOv2::ios_defs::eofbit).m_hour == 13);
    FOri("%EH", 'H', 'E', IOv2::ios_defs::eofbit);
    FOri("H",   'H', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("H",   'H', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("01", 'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri("01", 'I', 'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri("一", 'I', 'O', IOv2::ios_defs::eofbit).m_hour == 1);
    FOri("%EI", 'I', 'E', IOv2::ios_defs::eofbit);
    FOri("I",   'I', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("I",   'I', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%j", 'j', 0, IOv2::ios_defs::eofbit);
    FOri("%Ej", 'j', 'E', IOv2::ios_defs::eofbit);
    FOri("j",   'j', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oj", 'j', 'O', IOv2::ios_defs::eofbit);
    FOri("j",   'j', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%m", 'm',  0, IOv2::ios_defs::eofbit);
    FOri("%Om", 'm', 'O', IOv2::ios_defs::eofbit);
    FOri("%Em", 'm', 'E', IOv2::ios_defs::eofbit);
    FOri("m",   'm', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("m",   'm', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("33", 'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri("33", 'M', 'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri("三十三", 'M', 'O', IOv2::ios_defs::eofbit).m_minute == 33);
    FOri("%EM", 'M', 'E', IOv2::ios_defs::eofbit);
    FOri("M",   'M', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("M",   'M', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("\n",   'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri("x",    'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri("\n",   'n', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%En",  'n', 'E', IOv2::ios_defs::eofbit, 3);
    FOri("n",    'n', 'O', IOv2::ios_defs::strfailbit, 0);
    FOri("%On",  'n', 'O', IOv2::ios_defs::eofbit, 3);

    FOri("\t",   't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri("x",    't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri("\t",   't', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Et",  't', 'E', IOv2::ios_defs::eofbit, 3);
    FOri("n",    't', 'O', IOv2::ios_defs::strfailbit, 0);
    FOri("%Ot",  't', 'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(FHms("01 午後", "%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(FHms("01 午前", "%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(FOri("午後", 'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(FOri("午前", 'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    FOri("%Ep", 'p', 'E', IOv2::ios_defs::eofbit);
    FOri("p",   'p', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Op", 'p', 'O', IOv2::ios_defs::eofbit);
    FOri("p",   'p', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms("午後01時33分18秒", "%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("%Er", 'r', 'E', IOv2::ios_defs::eofbit);
    FOri("r",   'r', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Or", 'r', 'O', IOv2::ios_defs::eofbit);
    FOri("r",   'r', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms("13:33", "%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri("%ER", 'R', 'E', IOv2::ios_defs::eofbit);
    FOri("R",   'R', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OR", 'R', 'O', IOv2::ios_defs::eofbit);
    FOri("R",   'R', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("18", 'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri("18", 'S', 'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri("十八", 'S', 'O', IOv2::ios_defs::eofbit).m_second == 18);
    FOri("%ES", 'S', 'E', IOv2::ios_defs::eofbit);
    FOri("S",   'S', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("S",   'S', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms("13時33分18秒", "%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(FHms("13時33分18秒", "%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("X",   'X', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OX", 'X', 'O', IOv2::ios_defs::eofbit);
    FOri("X",   'X', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms("13:33:18", "%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("%ET", 'T', 'E', IOv2::ios_defs::eofbit);
    FOri("T",   'T', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OT", 'T', 'O', IOv2::ios_defs::eofbit);
    FOri("T",   'T', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%u", 'u', 0,   IOv2::ios_defs::eofbit);
    FOri("%Ou", 'u', 'O', IOv2::ios_defs::eofbit);
    FOri("%Eu", 'u', 'E', IOv2::ios_defs::eofbit);
    FOri("u",   'u', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("u",   'u', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%g", 'g', 0, IOv2::ios_defs::eofbit);
    FOri("%Eg", 'g', 'E', IOv2::ios_defs::eofbit);
    FOri("g",   'g', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Og", 'g', 'O', IOv2::ios_defs::eofbit);
    FOri("g",   'g', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%G", 'G', 0, IOv2::ios_defs::eofbit);
    FOri("%EG", 'G', 'E', IOv2::ios_defs::eofbit);
    FOri("G",   'G', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OG", 'G', 'O', IOv2::ios_defs::eofbit);
    FOri("G",   'G', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%U", 'U', 0,   IOv2::ios_defs::eofbit);
    FOri("%OU", 'U', 'O', IOv2::ios_defs::eofbit);
    FOri("%EU", 'U', 'E', IOv2::ios_defs::eofbit);
    FOri("U",   'U', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("U",   'U', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%W", 'W', 0,   IOv2::ios_defs::eofbit);
    FOri("%OW", 'W', 'O', IOv2::ios_defs::eofbit);
    FOri("%EW", 'W', 'E', IOv2::ios_defs::eofbit);
    FOri("W",   'W', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("W",   'W', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%V", 'V', 0,   IOv2::ios_defs::eofbit);
    FOri("%OV", 'V', 'O',   IOv2::ios_defs::eofbit);
    FOri("54",  'V', 'O', IOv2::ios_defs::strfailbit, 1);
    FOri("%EV", 'V', 'E', IOv2::ios_defs::eofbit);
    FOri("V",   'V', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("V",   'V', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%w", 'w', 0,   IOv2::ios_defs::eofbit);
    FOri("%Ow", 'w', 'O', IOv2::ios_defs::eofbit);
    FOri("%Ew", 'w', 'E', IOv2::ios_defs::eofbit);
    FOri("w",   'w', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("w",   'w', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%y", 'y', 0,   IOv2::ios_defs::eofbit);
    FOri("%Ey", 'y', 'E', IOv2::ios_defs::eofbit);
    FOri("%Oy", 'y', 'O', IOv2::ios_defs::eofbit);
    FOri("y",  'y', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("y",  'y', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%Y", 'Y', 0,   IOv2::ios_defs::eofbit);
    FOri("%EY", 'Y', 'E', IOv2::ios_defs::eofbit);
    FOri("Y",   'Y', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OY", 'Y', 'O', IOv2::ios_defs::eofbit);
    FOri("Y",   'Y', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(zone_is(FOri("America/Los_Angeles", 'Z', 0, IOv2::ios_defs::eofbit).m_zone_name, "America/Los_Angeles"));
    { auto r = FOri("PST", 'Z', 0, IOv2::ios_defs::eofbit); VERIFY(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    FOri("America/Los_Angexes", 'Z', 0, IOv2::ios_defs::strfailbit);
    FOri("%EZ", 'Z', 'E', IOv2::ios_defs::eofbit);
    FOri("Z",   'Z', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OZ", 'Z', 'O', IOv2::ios_defs::eofbit);
    FOri("Z",   'Z', 'O', IOv2::ios_defs::strfailbit, 0);

    { auto r = FOri("+0800", 'z', 0, IOv2::ios_defs::eofbit); VERIFY(r.m_have_offset && r.m_offset == minutes{480}); }
    FOri("%z", 'z', 0, IOv2::ios_defs::strfailbit);
    FOri("%Ez", 'z', 'E', IOv2::ios_defs::eofbit);
    FOri("z",  'z', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oz", 'z', 'O', IOv2::ios_defs::eofbit);
    FOri("z",  'z', 'O', IOv2::ios_defs::strfailbit, 0);

    dump_info("Done\n");
}

void test_timeio_char_get_17()
{
    dump_info("Test timeio<char> get 17...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char, false, true, IOv2::tz_level::none>, false, true, IOv2::tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, IOv2::tz_level::none>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri("%",  '%',  0,  IOv2::ios_defs::eofbit);
    FOri("x",  '%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri("%",  '%', 'E', febit);
    FOri("%E%", '%', 'E', IOv2::ios_defs::eofbit);
    FOri("%",  '%', 'O', febit);
    FOri("%O%", '%', 'O', IOv2::ios_defs::eofbit);

    FOri("%a", 'a', 0, IOv2::ios_defs::eofbit, 3);
    FOri("%Ea", 'a', 'E', IOv2::ios_defs::eofbit);
    FOri("a",   'a', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oa", 'a', 'O', IOv2::ios_defs::eofbit);
    FOri("a",   'a', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%A", 'A', 0, IOv2::ios_defs::eofbit, 3);
    FOri("%EA", 'A', 'E', IOv2::ios_defs::eofbit);
    FOri("A",   'A', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OA", 'A', 'O', IOv2::ios_defs::eofbit);
    FOri("A",   'A', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%b", 'b', 0, IOv2::ios_defs::eofbit, 3);
    FOri("%Eb", 'b', 'E', IOv2::ios_defs::eofbit);
    FOri("b",   'b', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Ob", 'b', 'O', IOv2::ios_defs::eofbit);
    FOri("b",   'b', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%B", 'B', 0, IOv2::ios_defs::eofbit, 3);
    FOri("%EB", 'B', 'E', IOv2::ios_defs::eofbit);
    FOri("B",   'B', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OB", 'B', 'O', IOv2::ios_defs::eofbit);
    FOri("B",   'B', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%h", 'h', 0, IOv2::ios_defs::eofbit, 3);
    FOri("%Eh", 'h', 'E', IOv2::ios_defs::eofbit);
    FOri("h",   'h', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oh", 'h', 'O', IOv2::ios_defs::eofbit);
    FOri("h",   'h', 'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    FOri("%c", 'c', 0, IOv2::ios_defs::eofbit);
    FOri("%Ec", 'c', 'E', IOv2::ios_defs::eofbit);
    FOri("c",   'c', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oc", 'c', 'O', IOv2::ios_defs::eofbit);
    FOri("c",   'c', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%C", 'C', 0,   IOv2::ios_defs::eofbit);
    FOri("%EC", 'C', 'E', IOv2::ios_defs::eofbit);
    FOri("C",   'C', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OC", 'C', 'O', IOv2::ios_defs::eofbit);
    FOri("C",   'C', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%d", 'd', 0,   IOv2::ios_defs::eofbit);
    FOri("%Od", 'd', 'O', IOv2::ios_defs::eofbit);
    FOri("%Ed", 'd', 'E', IOv2::ios_defs::eofbit);
    FOri("d",   'd', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("d",   'd', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%e", 'e', 0,   IOv2::ios_defs::eofbit);
    FOri("%Oe", 'e', 'O', IOv2::ios_defs::eofbit);
    FOri("%Ee", 'e', 'E', IOv2::ios_defs::eofbit);
    FOri("e",   'e', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("e",   'e', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%F", 'F', 0, IOv2::ios_defs::eofbit);
    FOri("%EF", 'F', 'E', IOv2::ios_defs::eofbit);
    FOri("F",   'F', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OF", 'F', 'O', IOv2::ios_defs::eofbit);
    FOri("F",   'F', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%x", 'x', 0, IOv2::ios_defs::eofbit);
    FOri("%Ex", 'x', 'E', IOv2::ios_defs::eofbit);
    FOri("x",   'x', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Ox", 'x', 'O', IOv2::ios_defs::eofbit);
    FOri("x",   'x', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%D", 'D', 0, IOv2::ios_defs::eofbit);
    FOri("%ED", 'D', 'E', IOv2::ios_defs::eofbit);
    FOri("D",   'D', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OD", 'D', 'O', IOv2::ios_defs::eofbit);
    FOri("D",   'D', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("13", 'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri("13", 'H', 'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri("十三", 'H', 'O', IOv2::ios_defs::eofbit).m_hour == 13);
    FOri("%EH", 'H', 'E', IOv2::ios_defs::eofbit);
    FOri("H",   'H', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("H",   'H', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("01", 'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri("01", 'I', 'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri("一", 'I', 'O', IOv2::ios_defs::eofbit).m_hour == 1);
    FOri("%EI", 'I', 'E', IOv2::ios_defs::eofbit);
    FOri("I",   'I', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("I",   'I', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%j", 'j', 0, IOv2::ios_defs::eofbit);
    FOri("%Ej", 'j', 'E', IOv2::ios_defs::eofbit);
    FOri("j",   'j', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oj", 'j', 'O', IOv2::ios_defs::eofbit);
    FOri("j",   'j', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%m", 'm',  0, IOv2::ios_defs::eofbit);
    FOri("%Om", 'm', 'O', IOv2::ios_defs::eofbit);
    FOri("%Em", 'm', 'E', IOv2::ios_defs::eofbit);
    FOri("m",   'm', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("m",   'm', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("33", 'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri("33", 'M', 'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri("三十三", 'M', 'O', IOv2::ios_defs::eofbit).m_minute == 33);
    FOri("%EM", 'M', 'E', IOv2::ios_defs::eofbit);
    FOri("M",   'M', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("M",   'M', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("\n",   'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri("x",    'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri("\n",   'n', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%En",  'n', 'E', IOv2::ios_defs::eofbit, 3);
    FOri("n",    'n', 'O', IOv2::ios_defs::strfailbit, 0);
    FOri("%On",  'n', 'O', IOv2::ios_defs::eofbit, 3);

    FOri("\t",   't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri("x",    't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri("\t",   't', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Et",  't', 'E', IOv2::ios_defs::eofbit, 3);
    FOri("n",    't', 'O', IOv2::ios_defs::strfailbit, 0);
    FOri("%Ot",  't', 'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(FHms("01 午後", "%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(FHms("01 午前", "%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(FOri("午後", 'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(FOri("午前", 'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    FOri("%Ep", 'p', 'E', IOv2::ios_defs::eofbit);
    FOri("p",   'p', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Op", 'p', 'O', IOv2::ios_defs::eofbit);
    FOri("p",   'p', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms("午後01時33分18秒", "%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("%Er", 'r', 'E', IOv2::ios_defs::eofbit);
    FOri("r",   'r', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Or", 'r', 'O', IOv2::ios_defs::eofbit);
    FOri("r",   'r', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms("13:33", "%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri("%ER", 'R', 'E', IOv2::ios_defs::eofbit);
    FOri("R",   'R', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OR", 'R', 'O', IOv2::ios_defs::eofbit);
    FOri("R",   'R', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri("18", 'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri("18", 'S', 'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri("十八", 'S', 'O', IOv2::ios_defs::eofbit).m_second == 18);
    FOri("%ES", 'S', 'E', IOv2::ios_defs::eofbit);
    FOri("S",   'S', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("S",   'S', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms("13時33分18秒", "%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(FHms("13時33分18秒", "%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("X",   'X', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OX", 'X', 'O', IOv2::ios_defs::eofbit);
    FOri("X",   'X', 'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms("13:33:18", "%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri("%ET", 'T', 'E', IOv2::ios_defs::eofbit);
    FOri("T",   'T', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OT", 'T', 'O', IOv2::ios_defs::eofbit);
    FOri("T",   'T', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%u", 'u', 0,   IOv2::ios_defs::eofbit);
    FOri("%Ou", 'u', 'O', IOv2::ios_defs::eofbit);
    FOri("%Eu", 'u', 'E', IOv2::ios_defs::eofbit);
    FOri("u",   'u', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("u",   'u', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%g", 'g', 0, IOv2::ios_defs::eofbit);
    FOri("%Eg", 'g', 'E', IOv2::ios_defs::eofbit);
    FOri("g",   'g', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Og", 'g', 'O', IOv2::ios_defs::eofbit);
    FOri("g",   'g', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%G", 'G', 0, IOv2::ios_defs::eofbit);
    FOri("%EG", 'G', 'E', IOv2::ios_defs::eofbit);
    FOri("G",   'G', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OG", 'G', 'O', IOv2::ios_defs::eofbit);
    FOri("G",   'G', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%U", 'U', 0,   IOv2::ios_defs::eofbit);
    FOri("%OU", 'U', 'O', IOv2::ios_defs::eofbit);
    FOri("%EU", 'U', 'E', IOv2::ios_defs::eofbit);
    FOri("U",   'U', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("U",   'U', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%W", 'W', 0,   IOv2::ios_defs::eofbit);
    FOri("%OW", 'W', 'O', IOv2::ios_defs::eofbit);
    FOri("%EW", 'W', 'E', IOv2::ios_defs::eofbit);
    FOri("W",   'W', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("W",   'W', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%V", 'V', 0,   IOv2::ios_defs::eofbit);
    FOri("%OV", 'V', 'O',   IOv2::ios_defs::eofbit);
    FOri("54",  'V', 'O', IOv2::ios_defs::strfailbit, 1);
    FOri("%EV", 'V', 'E', IOv2::ios_defs::eofbit);
    FOri("V",   'V', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("V",   'V', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%w", 'w', 0,   IOv2::ios_defs::eofbit);
    FOri("%Ow", 'w', 'O', IOv2::ios_defs::eofbit);
    FOri("%Ew", 'w', 'E', IOv2::ios_defs::eofbit);
    FOri("w",   'w', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("w",   'w', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%y", 'y', 0,   IOv2::ios_defs::eofbit);
    FOri("%Ey", 'y', 'E', IOv2::ios_defs::eofbit);
    FOri("%Oy", 'y', 'O', IOv2::ios_defs::eofbit);
    FOri("y",  'y', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("y",  'y', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%Y", 'Y', 0,   IOv2::ios_defs::eofbit);
    FOri("%EY", 'Y', 'E', IOv2::ios_defs::eofbit);
    FOri("Y",   'Y', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OY", 'Y', 'O', IOv2::ios_defs::eofbit);
    FOri("Y",   'Y', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%Z", 'Z', 0, IOv2::ios_defs::eofbit);
    FOri("%EZ", 'Z', 'E', IOv2::ios_defs::eofbit);
    FOri("Z",   'Z', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%OZ", 'Z', 'O', IOv2::ios_defs::eofbit);
    FOri("Z",   'Z', 'O', IOv2::ios_defs::strfailbit, 0);

    FOri("%z", 'z', 0, IOv2::ios_defs::eofbit);
    FOri("%Ez", 'z', 'E', IOv2::ios_defs::eofbit);
    FOri("z",  'z', 'E', IOv2::ios_defs::strfailbit, 0);
    FOri("%Oz", 'z', 'O', IOv2::ios_defs::eofbit);
    FOri("z",  'z', 'O', IOv2::ios_defs::strfailbit, 0);

    dump_info("Done\n");
}

void test_timeio_char_put_18()
{
    dump_info("Test timeio<char> put 18...");
    using namespace std::chrono;

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    std::string res;

    // put(year_month_day) with invalid date (line 1173)
    {
        auto invalid_ymd = year_month_day{year{2024}, month{2}, day{30}};
        bool threw = false;
        try { obj.put(std::back_inserter(res), invalid_ymd, std::string_view("%F")); }
        catch (IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    // put(hh_mm_ss) with negative total duration (line 1214)
    {
        hh_mm_ss<seconds> invalid_hms{seconds{-1}};
        bool threw = false;
        try { obj.put(std::back_inserter(res), invalid_hms, std::string_view("%T")); }
        catch (IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    // put(std::tm) with out-of-range field: month=-1 (line 1271)
    {
        std::tm bad_tm{};
        bad_tm.tm_year = 124; bad_tm.tm_mon = -1;
        bad_tm.tm_mday = 1; bad_tm.tm_hour = 0; bad_tm.tm_min = 0; bad_tm.tm_sec = 0;
        bool threw = false;
        try { obj.put(std::back_inserter(res), bad_tm, std::string_view("%F")); }
        catch (IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    // put(std::tm) with valid fields but invalid date: Feb 30 (line 1275)
    {
        std::tm bad_tm{};
        bad_tm.tm_year = 124; bad_tm.tm_mon = 1; bad_tm.tm_mday = 30;
        bad_tm.tm_hour = 0; bad_tm.tm_min = 0; bad_tm.tm_sec = 0;
        bool threw = false;
        try { obj.put(std::back_inserter(res), bad_tm, std::string_view("%F")); }
        catch (IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
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
            bool threw = false;
            try { obj.put(std::back_inserter(res), bad_tm, std::string_view("%Y")); }
            catch (IOv2::stream_error&) { threw = true; }
            VERIFY(threw);
        }
    }

    // put(std::tm) at the year bounds themselves: still accepted
    {
        std::tm edge_tm{};
        edge_tm.tm_mon = 0; edge_tm.tm_mday = 1;
        edge_tm.tm_hour = 0; edge_tm.tm_min = 0; edge_tm.tm_sec = 0;

        edge_tm.tm_year = static_cast<int>(year::max()) - 1900;
        res.clear(); obj.put(std::back_inserter(res), edge_tm, std::string_view("%Y"));
        VERIFY(res == "32767");

        edge_tm.tm_year = static_cast<int>(year::min()) - 1900;
        res.clear(); obj.put(std::back_inserter(res), edge_tm, std::string_view("%Y"));
        VERIFY(res == "-32767");
    }

    // put(year_month_day) with negative year: %Y and %C output sign (lines 2860-2861, 2543-2544)
    {
        auto neg_ymd = year_month_day{year{-1}, month{1}, day{1}};
        res.clear(); obj.put(std::back_inserter(res), neg_ymd, std::string_view("%Y"));
        VERIFY(res == "-0001");
        res.clear(); obj.put(std::back_inserter(res), neg_ymd, std::string_view("%C"));
        VERIFY(res == "-01");
    }

    // put(year_month_day) for date in ISO year -1: %G output sign (lines 2608-2609)
    // Jan 1, year 0 is a Saturday; Thu of that ISO week is Dec 30, year -1 -> G=-0001
    {
        auto early_ymd = year_month_day{year{0}, month{1}, day{1}};
        res.clear(); obj.put(std::back_inserter(res), early_ymd, std::string_view("%G"));
        VERIFY(res == "-0001");
    }

    // put(zoned_time) with positive offset: %z outputs '+' (line 2883)
    {
        auto tp = create_zoned_time(2024, 9, 4, 12, 0, 0, "Asia/Tokyo");
        res.clear(); obj.put(std::back_inserter(res), tp, std::string_view("%z"));
        VERIFY(res == "+0900");
    }

    dump_info("Done\n");
}

void test_timeio_char_get_18()
{
    dump_info("Test timeio<char> get 18...");
    using namespace std::chrono;

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    IOv2::timeio obj_ja(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));
    IOv2::timeio obj_zh_tw(std::make_shared<IOv2::timeio_conf<char>>("zh_TW.UTF-8"));

    // operator year_month_day() throws for invalid reconstructed date (line 126)
    // Feb 30 parses successfully but is not a valid calendar date
    {
        auto ctx = CheckGet(obj, "02 30", "%m %d", IOv2::ios_defs::eofbit);
        bool threw = false;
        try { auto ymd = ctx_to<year_month_day>(ctx); (void)ymd; }
        catch (IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    // Era deduction: m_have_mon=true, m_have_mday=false, match found (lines 224-241)
    // 令和6 January: est_year=2024, Jan is within 令和 era -> deduced_year=2024
    {
        auto ctx = CheckGet(obj_ja, "令和6 01", "%EC%Ey %m", IOv2::ios_defs::eofbit);
        auto ymd = ctx_to<year_month_day>(ctx);
        VERIFY(ymd.year() == year{2024} && ymd.month() == month{1});
    }

    // Era deduction: m_have_mon=true, m_have_mday=false, nothing matches (lines 245-246)
    // 平成31 May: est_year=2019, May 2019 past 平成 end (Apr 30) -> from_year=1990
    {
        auto ctx = CheckGet(obj_ja, "平成31 05", "%EC%Ey %m", IOv2::ios_defs::eofbit);
        auto ymd = ctx_to<year_month_day>(ctx);
        VERIFY(ymd.year() == year{1990} && ymd.month() == month{5});
    }

    // Era deduction: m_have_mon=true, m_have_mday=true, nothing matches (line 220)
    // 平成31 May 1: May 1, 2019 past 平成 end (Apr 30) -> from_year=1990
    {
        auto ctx = CheckGet(obj_ja, "平成31 05 01", "%EC%Ey %m %d", IOv2::ios_defs::eofbit);
        auto ymd = ctx_to<year_month_day>(ctx);
        VERIFY(ymd == year_month_day{year{1990}, month{5}, day{1}});
    }

    // Backward eras use the same era-year syntax but move toward the past. The
    // year-only cases exercise the range check that cannot assume from_year <=
    // to_year; the complete date also checks direction during reconstruction.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "紀元前100", "%EC%Ey",
                                            IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{-99});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "紀元前100 02 03", "%EC%Ey %m %d",
                                            IOv2::ios_defs::eofbit);
        VERIFY(ymd == year_month_day{year{-99}, month{2}, day{3}});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_zh_tw, "民前100", "%EC%Ey",
                                            IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1812});
    }

    // Era name with no era year (%EC on its own). The era name match sets no m_have_*
    // year flag, so this is the one deduction path that reads the era items outside the
    // %Ey branch above (line 332). 平成 rather than 令和 is what proves the era items
    // narrowed by the name are what pick the year: the unnarrowed table starts at 令和.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "令和", "%EC", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{2020});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成", "%EC", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1990});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "昭和", "%EC", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1927});
    }

    // The month and day the format string does leave open still come from the parse,
    // and from the fallback, respectively.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "令和 05 01", "%EC %m %d",
                                            IOv2::ios_defs::eofbit);
        VERIFY(ymd == year_month_day{year{2020}, month{5}, day{1}});
    }

    // The era name beats the date hint for the year, while the hint still supplies the
    // month and day.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{2023}, month{9}, day{17}});
        const std::string in = "平成";
        VERIFY(obj_ja.get(in.begin(), in.end(), ctx, "%EC") == in.end());
        VERIFY(ctx_to<year_month_day>(ctx)
               == year_month_day{year{1990}, month{9}, day{17}});
    }

    // A format string that names no era must leave the whole fallback date alone. An era
    // locale used to rewrite the year to the first era's from_year even though neither
    // the input nor the format said anything about the year.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{2023}, month{9}, day{17}});
        const std::string in = "01:02";
        VERIFY(obj_ja.get(in.begin(), in.end(), ctx, "%H:%M") == in.end());
        VERIFY(ctx_to<year_month_day>(ctx)
               == year_month_day{year{2023}, month{9}, day{17}});
        // No %E specifier was seen, so the locale's era table was never copied in.
        VERIFY(ctx.m_era_items.empty());
    }

    // A 元年 (first year of an era) form matches an era entry of its own, one whose
    // format carries no %Ey. The year therefore has to come from the era items, and only
    // the entry belonging to the format that actually matched holds the right from_year.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "令和元年", "%EY", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{2019});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成元年", "%EY", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1989});
    }

    // The ordinary era-year form goes through %Ey and must be unaffected.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "令和2年", "%EY", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{2020});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成3年", "%EY", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1991});
    }

    // %Ex expands to the locale's era date format, which reaches %EY the same way.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "令和元年06月15日", "%Ex",
                                            IOv2::ios_defs::eofbit);
        VERIFY(ymd == year_month_day{year{2019}, month{6}, day{15}});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成元年06月15日", "%Ex",
                                            IOv2::ios_defs::eofbit);
        VERIFY(ymd == year_month_day{year{1989}, month{6}, day{15}});
    }

    // A modifier belongs to one conversion specification and does not carry over to the
    // rest of the format string. POSIX attaches era semantics to %EC/%Ey/%EY only, so an
    // unmodified %y/%C/%Y after an %E specifier is still the plain Gregorian form -- even
    // though the era table has been filled in by then. Reading the table alone (rather
    // than the modifier) turns each of these into an era parse, and the ones below fail
    // outright: %y would take 4 digits as an era year and then find no era whose range
    // holds it.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成 88", "%EC %y", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1988});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成 19", "%EC %C", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1900});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成 1988", "%EC %Y", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1988});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成31 19 88", "%EC%Ey %C %y",
                                            IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1988});
    }

    // %Oy is the year within century in the locale's alternative numeric symbols; the O
    // modifier has nothing to do with eras. ja_JP defines no alternative digits, so per
    // POSIX ("if the alternative ... does not exist in the current locale, the unmodified
    // field descriptor is used") it reads ordinary digits.
    {
        auto ymd = CheckGet<year_month_day>(obj_ja, "平成 88", "%EC %Oy", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1988});
    }

    // The other half of that POSIX sentence: with no era data at all, the E-modified
    // specifiers degrade to their unmodified meanings instead of failing.
    {
        auto ymd = CheckGet<year_month_day>(obj, "19", "%EC", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1900});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj, "88", "%Ey", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1988});
    }
    {
        auto ymd = CheckGet<year_month_day>(obj, "1988", "%EY", IOv2::ios_defs::eofbit);
        VERIFY(ymd.year() == year{1988});
    }

    // Negative yday via U-week+wday (lines 304-305)
    // 2024 Jan1=Monday(1), U=0, w=0(Sunday): yday=-1 -> Dec 31, 2023
    {
        auto ymd = CheckGet<year_month_day>(obj, "2024 0 0", "%Y %U %w", IOv2::ios_defs::eofbit);
        VERIFY(ymd == year_month_day{year{2023}, month{12}, day{31}});
    }

    // Overflow yday via U-week+wday (lines 309-310)
    // 2024 Jan1=Monday(1), U=53, w=6(Saturday): yday=376 -> Jan 11, 2025
    {
        auto ymd = CheckGet<year_month_day>(obj, "2024 53 6", "%Y %U %w", IOv2::ios_defs::eofbit);
        VERIFY(ymd == year_month_day{year{2025}, month{1}, day{11}});
    }

    // Week-only path (no wday): lines 343-370
    {
        // 2024 U=36 (normal): yday=252 -> Sep 9, 2024
        auto ymd1 = CheckGet<year_month_day>(obj, "2024 36", "%Y %U", IOv2::ios_defs::eofbit);
        VERIFY(ymd1 == year_month_day{year{2024}, month{9}, day{9}});

        // 2024 W=0: yday=-7 -> Dec 25, 2023 (lines 354-358)
        auto ymd2 = CheckGet<year_month_day>(obj, "2024 0", "%Y %W", IOv2::ios_defs::eofbit);
        VERIFY(ymd2 == year_month_day{year{2023}, month{12}, day{25}});

        // 2024 U=53: yday=371 -> Jan 6, 2025 (lines 359-362)
        auto ymd3 = CheckGet<year_month_day>(obj, "2024 53", "%Y %U", IOv2::ios_defs::eofbit);
        VERIFY(ymd3 == year_month_day{year{2025}, month{1}, day{6}});
    }

    // Format string ends after E/O modifier with no following specifier (lines 1510-1511)
    CheckGet(obj, "x", "%E", IOv2::ios_defs::strfailbit, 0);

    // %a/%A tree match failure (lines 1542-1543)
    CheckGet(obj, "xyz", 'a', (char)0, IOv2::ios_defs::strfailbit, 0);

    // %b/%B/%h tree match failure (lines 1564-1565)
    CheckGet(obj, "xyz", 'b', (char)0, IOv2::ios_defs::strfailbit, 0);

    // %e with leading space (line 1655)
    VERIFY(CheckGet(obj, " 4", 'e', (char)0, IOv2::ios_defs::eofbit).m_mday == 4);

    // %p AM/PM tree miss (lines 1823-1824)
    CheckGet(obj, "xyz", 'p', (char)0, IOv2::ios_defs::strfailbit, 0);

    // %Ey era year out of range: all era items pruned (lines 2035-2036)
    // 平成32: delta=30 exceeds 平成 range=29 -> pruned -> stream_error
    CheckGet(obj_ja, "平成32", "%EC%Ey", IOv2::ios_defs::strfailbit);

    // %z failures
    CheckGet(obj, "abc",   'z', (char)0, IOv2::ios_defs::strfailbit, 0); // not Z/+/- (line 2144)
    CheckGet(obj, "+",     'z', (char)0, IOv2::ios_defs::strfailbit, 0); // sign+EOF (line 2150)
    CheckGet(obj, "+123",  'z', (char)0, IOv2::ios_defs::strfailbit, 0); // 3 digits not 2 or 4 (line 2167)
    CheckGet(obj, "+2500", 'z', (char)0, IOv2::ios_defs::strfailbit, 0); // hour>=24 (line 2172)

    // bad_parse_format modifier mismatch (lines 2218-2219)
    // Format %Ej: %j rejects E modifier -> bad_parse_format; input "%Eb": '%','E' consumed but 'b'!='j'
    CheckGet(obj, "%Eb", 'j', 'E', IOv2::ios_defs::strfailbit, 0);

    // Format tail: trailing whitespace consumed at end of input (line 2233)
    CheckGet(obj, "Sep", std::string("%b "), IOv2::ios_defs::eofbit);

    // Format tail: trailing %n consumed at end of input (lines 2237-2239)
    CheckGet(obj, "Sep", std::string("%b%n"), IOv2::ios_defs::eofbit);

    // Format tail: non-%n/t specifier causes break at line 2238, then succ=false
    CheckGet(obj, "Sep", std::string("%b%z"), IOv2::ios_defs::strfailbit);

    // Format tail: bare '%' at format end (next==cend) causes break at line 2236
    CheckGet(obj, "Sep", std::string("%b%"), IOv2::ios_defs::strfailbit);

    dump_info("Done\n");
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

void test_timeio_char_get_19()
{
    dump_info("Test timeio<char> get 19...");
    using namespace std::chrono;

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));

    // set_hint(year_month_day): fields the format string does not parse keep the hint
    // instead of falling back to the wall clock.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{1969}, month{7}, day{20}});
        const std::string in = "09";
        VERIFY(obj.get(in.begin(), in.end(), ctx, 'm', 0) == in.end());
        VERIFY(ctx_to<year_month_day>(ctx)
               == year_month_day{year{1969}, month{9}, day{20}});
    }

    // A parsed field still wins over the hint.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{1969}, month{7}, day{20}});
        const std::string in = "2024 03 04";
        VERIFY(obj.get(in.begin(), in.end(), ctx, "%Y %m %d") == in.end());
        VERIFY(ctx_to<year_month_day>(ctx)
               == year_month_day{year{2024}, month{3}, day{4}});
    }

    // The hint does not supply the year within the century %C leaves open: as in POSIX
    // strptime that year is 0, so %C=18 is 1800 whatever the hint says. Month and day,
    // which %C says nothing about at all, still come from the hint.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{1969}, month{7}, day{20}});
        const std::string in = "18";
        VERIFY(obj.get(in.begin(), in.end(), ctx, "%C") == in.end());
        VERIFY(ctx_to<year_month_day>(ctx)
               == year_month_day{year{1800}, month{7}, day{20}});
    }

    // %C together with %y still combines the two into a full year.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{1969}, month{7}, day{20}});
        const std::string in = "18 24";
        VERIFY(obj.get(in.begin(), in.end(), ctx, "%C %y") == in.end());
        VERIFY(ctx_to<year_month_day>(ctx).year() == year{1824});
    }

    // A hinted day that cannot exist in the parsed month gives way to the month's last
    // day instead of failing the conversion: the hint only fills in what the format
    // string is silent about, so it must not break an otherwise well-formed parse.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{2020}, month{1}, day{31}});
        const std::string in = "02";
        VERIFY(obj.get(in.begin(), in.end(), ctx, 'm', 0) == in.end());
        VERIFY(ctx_to<year_month_day>(ctx)
               == year_month_day{year{2020}, month{2}, day{29}});
    }

    // The same when it is the year that is parsed and the hinted day is February 29.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{2020}, month{2}, day{29}});
        const std::string in = "2021";
        VERIFY(obj.get(in.begin(), in.end(), ctx, 'Y', 0) == in.end());
        VERIFY(ctx_to<year_month_day>(ctx)
               == year_month_day{year{2021}, month{2}, day{28}});
    }

    // A day that fits needs no adjustment.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{2020}, month{1}, day{31}});
        const std::string in = "03";
        VERIFY(obj.get(in.begin(), in.end(), ctx, 'm', 0) == in.end());
        VERIFY(ctx_to<year_month_day>(ctx)
               == year_month_day{year{2020}, month{3}, day{31}});
    }

    // A day that really was parsed does not give way: February 31 stays invalid and the
    // conversion reports it.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{2020}, month{2}, day{15}});
        const std::string in = "31";
        VERIFY(obj.get(in.begin(), in.end(), ctx, 'd', 0) == in.end());
        bool threw = false;
        try { (void)ctx_to<year_month_day>(ctx); }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    // set_hint(hh_mm_ss): unparsed time fields keep the hint.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(hh_mm_ss{hours{13} + minutes{45} + seconds{7}});
        const std::string in = "22";
        VERIFY(obj.get(in.begin(), in.end(), ctx, 'H', 0) == in.end());
        auto hms = ctx_to<hh_mm_ss<seconds>>(ctx);
        VERIFY(hms.hours() == hours{22} && hms.minutes() == minutes{45}
               && hms.seconds() == seconds{7});
    }

    // Any duration precision is accepted; finer than a second truncates toward zero.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(hh_mm_ss{milliseconds{(1 * 3600 + 2 * 60 + 3) * 1000 + 999}});
        auto hms = ctx_to<hh_mm_ss<seconds>>(ctx);
        VERIFY(hms.hours() == hours{1} && hms.minutes() == minutes{2}
               && hms.seconds() == seconds{3});
    }

    // A negative hint wraps into the day rather than producing garbage components.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(hh_mm_ss{-(hours{1} + minutes{2} + seconds{3})});
        auto hms = ctx_to<hh_mm_ss<seconds>>(ctx);
        VERIFY(hms.hours() == hours{22} && hms.minutes() == minutes{57}
               && hms.seconds() == seconds{57});
    }

    // A hint beyond one day is reduced modulo a day.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(hh_mm_ss{hours{49} + minutes{1}});
        auto hms = ctx_to<hh_mm_ss<seconds>>(ctx);
        VERIFY(hms.hours() == hours{1} && hms.minutes() == minutes{1});
    }

    // set_hint(const time_zone*) replaces the UTC fallback when %Z was never parsed,
    // and nullptr restores it.
    {
        IOv2::time_parse_context<char> ctx;
        VERIFY(ctx_to<const time_zone*>(ctx) == locate_zone("UTC"));
        ctx.set_hint(locate_zone("Asia/Shanghai"));
        VERIFY(ctx_to<const time_zone*>(ctx) == locate_zone("Asia/Shanghai"));
        ctx.set_hint(static_cast<const time_zone*>(nullptr));
        VERIFY(ctx_to<const time_zone*>(ctx) == locate_zone("UTC"));
    }

    // A parsed %Z wins over the zone hint.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(locate_zone("Asia/Shanghai"));
        const std::string in = "America/Los_Angeles";
        VERIFY(obj.get(in.begin(), in.end(), ctx, 'Z', 0) == in.end());
        VERIFY(ctx_to<const time_zone*>(ctx) == locate_zone("America/Los_Angeles"));
    }

    // reset() keeps its "restore to default-constructed" meaning: every hint is wiped,
    // so the date falls back to the current year again.
    {
        IOv2::time_parse_context<char> ctx;
        ctx.set_hint(year_month_day{year{1969}, month{7}, day{20}});
        ctx.set_hint(hh_mm_ss{hours{13} + minutes{45}});
        ctx.set_hint(locate_zone("Asia/Shanghai"));
        ctx.reset();
        VERIFY(ctx == IOv2::time_parse_context<char>{});
        VERIFY(ctx_to<const time_zone*>(ctx) == locate_zone("UTC"));

        auto now_year = year_month_day{floor<days>(system_clock::now())}.year();
        VERIFY(ctx_to<year_month_day>(ctx).year() == now_year);
    }

    // The setters are constrained, not silently ignored, when a field group is inactive.
    // The checks go through concepts rather than `static_assert(!requires ...)`, which GCC
    // reports as a hard error in a non-template context instead of a failed constraint.
    {
        static_assert(can_hint_date<IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone>>);
        static_assert(can_hint_time<IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone>>);
        static_assert(can_hint_zone<IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone>>);
        static_assert(!can_hint_date<IOv2::time_parse_context<char, false, true, IOv2::tz_level::zone>>);
        static_assert(!can_hint_time<IOv2::time_parse_context<char, true, false, IOv2::tz_level::zone>>);
        static_assert(!can_hint_zone<IOv2::time_parse_context<char, true, true, IOv2::tz_level::none>>);
    }

    dump_info("Done\n");
}

// A format string ending in a lone '%' -- or in a lone '%E' / '%O' modifier -- introduces no
// specifier, so there is nothing to convert. It follows the same rule this facet already uses
// for a specifier it does not recognize (see the "unknown format" path, which emits '%' plus
// the rest verbatim): put writes the '%' out and get matches it back as a literal. Handling
// the two sides alike is what keeps the round-trip invariant -- whatever put writes, get reads
// back with the same format string. put previously dropped the '%' silently while get rejected
// it, so put succeeded on output get could never read.
void test_timeio_char_put_19()
{
    dump_info("Test timeio<char> put 19...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    const std::tm t = test_tm(3, 2, 1, 15, 0, 124, 1, 14, 0);

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
        VERIFY(res == c.want);

        // The round trip: get consumes exactly what put produced, using the same format.
        IOv2::time_parse_context<char> ctx;
        VERIFY(obj.get(res.begin(), res.end(), ctx, std::string_view(c.fmt)) == res.end());
    }

    // get still rejects input that lacks the literal '%' the format asks for, so the
    // agreement above is a real match rather than the trailing '%' being ignored.
    {
        const std::string in = "2024";
        IOv2::time_parse_context<char> ctx;
        bool threw = false;
        try { obj.get(in.begin(), in.end(), ctx, std::string_view("%Y%")); }
        catch (IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
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
void test_timeio_char_put_20()
{
    dump_info("Test timeio<char> put 20...");

    // %Z and %z need a value that carries a zone; with a std::tm they degrade to a literal by
    // design, which part A would misread as "unsupported".
    const auto zt = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    const std::string supported = "%ABCDFGHIMRSTUVWXYZabcdeghjmnprtuwxyz";

    auto emits = [&](char spec) {
        IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
        const std::string fmt = std::string("%") + spec;
        std::string res;
        obj.put(std::back_inserter(res), zt, std::string_view(fmt));
        return res != fmt;
    };

    // Part A.
    for (char spec : supported)
        VERIFY(emits(spec));

    // Controls: characters outside the set must still be echoed, or "echoed == unsupported"
    // would be vacuous and part A would pass for any set at all.
    for (char spec : std::string("QLfikloqsv"))
        VERIFY(!emits(spec));

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
        std::shared_ptr<IOv2::timeio_conf<char>> conf;
        try { conf = std::make_shared<IOv2::timeio_conf<char>>(name); }
        catch (...) { continue; }   // not installed here, or its data is self-contradictory
        ++checked;

        VERIFY(all_supported(conf->date_format()));
        VERIFY(all_supported(conf->era_date_format()));
        VERIFY(all_supported(conf->time_format()));
        VERIFY(all_supported(conf->era_time_format()));
        VERIFY(all_supported(conf->date_time_format()));
        VERIFY(all_supported(conf->era_date_time_format()));
        VERIFY(all_supported(conf->am_pm_format()));
    }
    VERIFY(checked >= 2);   // C and C.UTF-8 are always there; more is better

    // The control for part B: the checker must reject what the locales are being cleared of.
    VERIFY(!all_supported("%-d.%-m.%Y"));
    VERIFY(!all_supported("%l:%M %P"));
    VERIFY(!all_supported("%k:%M"));
    VERIFY(all_supported("%d.%m.%Y"));
    VERIFY(all_supported("%EY %Oe"));

    dump_info("Done\n");
    dump_info("  (part B saw " + std::to_string(checked) + " of "
              + std::to_string(sizeof(names) / sizeof(*names)) + " candidate locales)\n");
}

// put(sys_time) and put(local_time, offset): both carry an offset, and they part company on the
// zone. %z always emits for either; %Z has "UTC" for a sys_time, which is why that type reads
// back at tz_level::zone, and nothing at all for a local_time, which is what tz_level::offset
// exists for -- there %Z degrades to a literal on both sides.
void test_timeio_char_put_21()
{
    dump_info("Test timeio<char> put 21...");
    using namespace std::chrono;

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    std::string res;

    const sys_time<seconds> st{
        sys_days{year{2024}/month{9}/day{4}} + hours{13} + minutes{33} + seconds{18}};
    const local_time<seconds> lt{
        local_days{year{2024}/month{9}/day{4}} + hours{13} + minutes{33} + seconds{18}};

    {
        res.clear(); obj.put(std::back_inserter(res), st, std::string_view("%F %T %z %Z"));
        VERIFY(res == "2024-09-04 13:33:18 +0000 UTC");
    }

    // %Z is the abbreviation the value supplies, not an IANA name: only a zoned_time has one.
    {
        auto zt = create_zoned_time(2024, 9, 4, 13, 33, 18, "Etc/UTC");
        res.clear(); obj.put(std::back_inserter(res), zt, std::string_view("%Z"));
        VERIFY(res == "Etc/UTC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), lt, hours{9}, std::string_view("%F %T %z %Z"));
        VERIFY(res == "2024-09-04 13:33:18 +0900 %Z");   // %Z degrades: no abbreviation to write

        res.clear();
        obj.put(std::back_inserter(res), lt, -(hours{5} + minutes{30}), std::string_view("%z"));
        VERIFY(res == "-0530");

        // %z has no room for seconds, so a sub-minute offset truncates towards zero. The sign
        // is written before the truncation, which is why a small negative offset reads "-0000".
        res.clear(); obj.put(std::back_inserter(res), lt, seconds{59}, std::string_view("%z"));
        VERIFY(res == "+0000");
        res.clear(); obj.put(std::back_inserter(res), lt, seconds{-59}, std::string_view("%z"));
        VERIFY(res == "-0000");
    }

    // A date outside year's range is rejected rather than clamped, as it is for the other values.
    {
        for (int i = 0; i < 2; ++i)
        {
            bool threw = false;
            try
            {
                if (i == 0)
                    obj.put(std::back_inserter(res),
                            sys_time<seconds>{sys_days{year::max()/month{12}/day{31}} + days{1}},
                            std::string_view("%F"));
                else
                    obj.put(std::back_inserter(res),
                            local_time<seconds>{local_days{year::max()/month{12}/day{31}} + days{1}},
                            seconds{0}, std::string_view("%F"));
            }
            catch (IOv2::stream_error&) { threw = true; }
            VERIFY(threw);
        }
    }

    // Round trips through the offset tier: what put writes, get reads back to the same instant.
    {
        res.clear(); obj.put(std::back_inserter(res), st, std::string_view("%F %T %z"));
        VERIFY(res == "2024-09-04 13:33:18 +0000");

        IOv2::time_parse_context<char, true, true, IOv2::tz_level::offset> ctx;
        VERIFY(obj.get(res.begin(), res.end(), ctx, std::string_view("%F %T %z")) == res.end());
        sys_time<seconds> back{};
        ctx.convert_to(back);
        VERIFY(back == st);
    }
    {
        res.clear(); obj.put(std::back_inserter(res), lt, hours{8}, std::string_view("%F %T %z"));
        VERIFY(res == "2024-09-04 13:33:18 +0800");

        IOv2::time_parse_context<char, true, true, IOv2::tz_level::offset> ctx;
        VERIFY(obj.get(res.begin(), res.end(), ctx, std::string_view("%F %T %z")) == res.end());
        sys_time<seconds>   back_sys{};
        local_time<seconds> back_local{};
        ctx.convert_to(back_sys);
        ctx.convert_to(back_local);
        VERIFY(back_local == lt);         // the wall time, offset dropped
        VERIFY(back_sys == st - hours{8}); // the instant, offset applied
    }

    dump_info("Done\n");
}

// The tz_level::zone tier as a std::tm reaches it: %z and %Z both parse, the offset and the
// zone text are kept verbatim, and convert_to(std::tm&) writes both back.
void test_timeio_char_get_20()
{
    dump_info("Test timeio<char> get 20...");
    using namespace std::chrono;

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));

    using ctx_zone = IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone>;
    auto parse = [&obj](const std::string& in, const char* fmt)
    {
        ctx_zone ctx;
        VERIFY(obj.get(in.begin(), in.end(), ctx, std::string_view(fmt)) == in.end());
        return ctx;
    };
    auto rejects = [&obj](const std::string& in, const char* fmt)
    {
        ctx_zone ctx;
        try { obj.get(in.begin(), in.end(), ctx, std::string_view(fmt)); }
        catch (IOv2::stream_error&) { return true; }
        return false;
    };

    // All four spellings %z accepts.
    VERIFY(parse("+0800", "%z").m_offset == minutes{480});
    VERIFY(parse("-0530", "%z").m_offset == -minutes{330});
    VERIFY(parse("+08", "%z").m_offset == minutes{480});
    VERIFY(parse("+08:30", "%z").m_offset == minutes{510});
    VERIFY(parse("Z", "%z").m_offset == minutes{0});
    VERIFY(parse("Z", "%z").m_have_offset);
    VERIFY(parse("-0000", "%z").m_offset == minutes{0});

    // A ':' not followed by a digit is put back rather than swallowed, so the two-digit form
    // still matches and the ':' is left for the format string to consume as a literal.
    VERIFY(parse("+08:", "%z:").m_offset == minutes{480});

    VERIFY(rejects("0800", "%z"));      // no sign
    VERIFY(rejects("+8", "%z"));        // one digit
    VERIFY(rejects("+080", "%z"));      // three digits
    VERIFY(rejects("+2400", "%z"));     // hour out of range
    VERIFY(rejects("+0060", "%z"));     // minute out of range
    VERIFY(rejects("+", "%z"));         // sign then nothing

    // %Z keeps the text and resolves nothing at this tier, but it does file it by what the
    // tzdb says it is: a name the database knows lands in m_zone_name, a bare abbreviation in
    // m_zone_abbrev, and the eight tokens that are both land in both.
    { auto r = parse("America/Los_Angeles", "%Z");
      VERIFY(zone_is(r.m_zone_name, "America/Los_Angeles") && r.m_zone_abbrev == nullptr); }
    { auto r = parse("PST", "%Z");
      VERIFY(r.m_zone_name == nullptr && zone_is(r.m_zone_abbrev, "PST")); }
    { auto r = parse("EST", "%Z");
      VERIFY(zone_is(r.m_zone_name, "EST") && zone_is(r.m_zone_abbrev, "EST")); }

    // Link names parse too -- locate_zone accepts them, so the trie carries them.
    VERIFY(zone_is(parse("US/Pacific", "%Z").m_zone_name, "US/Pacific"));
    VERIFY(zone_is(parse("Asia/Calcutta", "%Z").m_zone_name, "Asia/Calcutta"));
    VERIFY(zone_is(parse("Japan", "%Z").m_zone_name, "Japan"));

    // The unknown-zone token is a fourth case: it parses, but it names nothing. The text is
    // empty rather than absent, which is what lets convert_to(std::tm&) blank tm_zone.
    { auto r = parse("UNKNOWN", "%Z");
      VERIFY(r.m_zone_name == nullptr && r.m_zone_abbrev != nullptr && *r.m_zone_abbrev == '\0'); }
    VERIFY(rejects("America/Los_Angexes", "%Z"));

    // convert_to(minutes): the parsed offset first, then the hint, and an error with neither.
    {
        minutes off{};
        parse("+0800", "%z").convert_to(off);
        VERIFY(off == minutes{480});

        ctx_zone ctx;
        bool threw = false;
        try { ctx.convert_to(off); } catch (IOv2::stream_error&) { threw = true; }
        VERIFY(threw);

        ctx.set_hint(minutes{-60});
        ctx.convert_to(off);
        VERIFY(off == minutes{-60});
    }

    // The offset reaches a std::tm through tm_gmtoff and the zone through tm_zone, where the
    // platform has those members. tm_zone can be written because the text it points at lives
    // in the time-zone trie, which outlives every parse -- the field has no release call, so
    // nothing shorter-lived may go in it.
    if constexpr (requires (std::tm t) { t.tm_gmtoff; })
    {
        std::tm out{};
        parse("2024-09-04 13:33:18 +0800 PST", "%F %T %z %Z").convert_to(out);
        VERIFY(out.tm_gmtoff == 8 * 3600);
        if constexpr (requires (std::tm t) { t.tm_zone; })
        {
            VERIFY(zone_is(out.tm_zone, "PST"));

            // A zone name goes in as readily as an abbreviation: put wrote tm_zone out
            // verbatim, so get has to put it back the same way.
            std::tm named{};
            parse("2024-09-04 13:33:18 +0800 America/Los_Angeles", "%F %T %z %Z").convert_to(named);
            VERIFY(zone_is(named.tm_zone, "America/Los_Angeles"));

            // The unknown-zone token blanks the field rather than leaving it alone. Leaving it
            // would keep a stale name the text just said was not there, and the next put would
            // write that name back out.
            std::tm blanked{};
            blanked.tm_zone = "STALE";
            parse("2024-09-04 13:33:18 +0800 UNKNOWN", "%F %T %z %Z").convert_to(blanked);
            VERIFY(zone_is(blanked.tm_zone, ""));

            // No %Z at all is the third state, and the only one that leaves the field be.
            std::tm untouched{};
            untouched.tm_zone = "KEPT";
            parse("2024-09-04 13:33:18 +0800", "%F %T %z").convert_to(untouched);
            VERIFY(zone_is(untouched.tm_zone, "KEPT"));

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
                VERIFY(once == twice);
            }
        }

        // Only a *parsed* %z is written back. With none, the field keeps whatever the caller
        // had there -- an offset hint does not reach it either.
        std::tm keep{};
        keep.tm_gmtoff = 12345;
        ctx_zone ctx;
        ctx.set_hint(minutes{60});
        const std::string in = "2024-09-04 13:33:18";
        VERIFY(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T")) == in.end());
        ctx.convert_to(keep);
        VERIFY(keep.tm_gmtoff == 12345);

        // The round trip io_manip.h documents for a std::tm.
        std::tm src{};
        src.tm_year = 124; src.tm_mon = 8; src.tm_mday = 4;
        src.tm_hour = 13; src.tm_min = 33; src.tm_sec = 18;
        src.tm_gmtoff = -7 * 3600;
        std::string res;
        obj.put(std::back_inserter(res), src, std::string_view("%F %T %z"));
        VERIFY(res == "2024-09-04 13:33:18 -0700");

        std::tm back{};
        ctx_zone ctx2;
        VERIFY(obj.get(res.begin(), res.end(), ctx2, std::string_view("%F %T %z")) == res.end());
        ctx2.convert_to(back);
        VERIFY(back.tm_gmtoff == -7 * 3600);
    }

    dump_info("Done\n");
}

// convert_to(sys_time&) resolves the instant in a fixed order, and convert_to(local_time&)
// resolves nothing at all.
void test_timeio_char_get_21()
{
    dump_info("Test timeio<char> get 21...");
    using namespace std::chrono;

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));

    const sys_time<seconds>   noon_utc{sys_days{year{2024}/month{9}/day{4}} + hours{12}};
    const local_time<seconds> noon_local{local_days{year{2024}/month{9}/day{4}} + hours{12}};

    using ctx_zone = IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone>;
    auto parse_zone = [&obj](const std::string& in, const char* fmt)
    {
        ctx_zone ctx;
        VERIFY(obj.get(in.begin(), in.end(), ctx, std::string_view(fmt)) == in.end());
        return ctx;
    };

    sys_time<seconds> st{};

    // 1. A parsed offset pins the instant.
    parse_zone("2024-09-04 12:00:00 +0800", "%F %T %z").convert_to(st);
    VERIFY(st == noon_utc - hours{8});

    // 2. With no offset, a parsed IANA name converts the wall time.
    parse_zone("2024-09-04 12:00:00 Asia/Tokyo", "%F %T %Z").convert_to(st);
    VERIFY(st == noon_utc - hours{9});

    // ...and it beats an offset hint, because parsed data outranks a fallback.
    {
        ctx_zone ctx;
        ctx.set_hint(minutes{120});
        const std::string in = "2024-09-04 12:00:00 Asia/Tokyo";
        VERIFY(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T %Z")) == in.end());
        ctx.convert_to(st);
        VERIFY(st == noon_utc - hours{9});
    }

    // 3. Nothing parsed: the offset hint, which in turn beats the zone hint and the UTC default.
    {
        ctx_zone ctx;
        ctx.set_hint(minutes{120});
        ctx.set_hint(locate_zone("Asia/Tokyo"));
        const std::string in = "2024-09-04 12:00:00";
        VERIFY(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T")) == in.end());
        ctx.convert_to(st);
        VERIFY(st == noon_utc - hours{2});
    }

    // 4. No offset hint either: the zone hint, and failing that UTC.
    {
        ctx_zone ctx;
        ctx.set_hint(locate_zone("Asia/Tokyo"));
        const std::string in = "2024-09-04 12:00:00";
        VERIFY(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T")) == in.end());
        ctx.convert_to(st);
        VERIFY(st == noon_utc - hours{9});
    }
    {
        ctx_zone ctx;
        const std::string in = "2024-09-04 12:00:00";
        VERIFY(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T")) == in.end());
        ctx.convert_to(st);
        VERIFY(st == noon_utc);
    }

    // At tz_level::offset there is no zone to fall back on, so a missing offset is an error
    // rather than an implicit UTC.
    {
        IOv2::time_parse_context<char, true, true, IOv2::tz_level::offset> ctx;
        const std::string in = "2024-09-04 12:00:00";
        VERIFY(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T")) == in.end());
        bool threw = false;
        try { ctx.convert_to(st); } catch (IOv2::stream_error&) { threw = true; }
        VERIFY(threw);

        ctx.set_hint(minutes{-240});
        ctx.convert_to(st);
        VERIFY(st == noon_utc + hours{4});
    }

    // The offset pins the instant and the zone must agree about it (D5). Disagreement is an
    // error, not a silent preference for one of the two.
    {
        zoned_time<seconds> zt{};
        parse_zone("2024-09-04 12:00:00 +0900 Asia/Tokyo", "%F %T %z %Z").convert_to(zt);
        VERIFY(zt.get_sys_time() == noon_utc - hours{9});

        bool threw = false;
        try { parse_zone("2024-09-04 12:00:00 +0800 Asia/Tokyo", "%F %T %z %Z").convert_to(zt); }
        catch (IOv2::stream_error&) { threw = true; }
        VERIFY(threw);

        // An abbreviation that names no single zone is an error even with an offset present:
        // the offset does not excuse the ambiguity, it only would have pinned the instant.
        threw = false;
        try { parse_zone("2024-09-04 12:00:00 -0700 PST", "%F %T %z %Z").convert_to(zt); }
        catch (IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    // convert_to(local_time&) takes the wall time as parsed and drops the zone, at every tier.
    {
        local_time<seconds> lt{};
        parse_zone("2024-09-04 12:00:00 +0800", "%F %T %z").convert_to(lt);
        VERIFY(lt == noon_local);

        lt = {};
        parse_zone("2024-09-04 12:00:00 Asia/Tokyo", "%F %T %Z").convert_to(lt);
        VERIFY(lt == noon_local);

        lt = {};
        IOv2::time_parse_context<char, true, true, IOv2::tz_level::none> ctx;
        const std::string in = "2024-09-04 12:00:00";
        VERIFY(obj.get(in.begin(), in.end(), ctx, std::string_view("%F %T")) == in.end());
        ctx.convert_to(lt);
        VERIFY(lt == noon_local);
    }

    dump_info("Done\n");
}

// The set of abbreviations %Z accepts. put copies std::tm::tm_zone out verbatim and
// localtime() can produce the abbreviation of any instant, so whatever put can write, get
// has to take back; the trie is built by walking every transition of every zone for exactly
// that reason. Sampling the database at one instant instead yields only the abbreviation in
// effect then, which is what these cases pin down.
void test_timeio_char_get_22()
{
    dump_info("Test timeio<char> get 22...");
    using namespace std::chrono;

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));

    auto parses = [&obj](const std::string& in)
    {
        IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone> ctx;
        try { return obj.get(in.begin(), in.end(), ctx, std::string_view("%Z")) == in.end(); }
        catch (IOv2::stream_error&) { return false; }
    };

    // Daylight-saving abbreviations, both hemispheres.
    VERIFY(parses("PDT"));
    VERIFY(parses("EDT"));
    VERIFY(parses("CEST"));
    VERIFY(parses("ACDT"));
    VERIFY(parses("NZST"));

    // Retired decades ago, and still what localtime() reports for a 1943 instant.
    VERIFY(parses("EWT"));

    // Standard-time abbreviations and full zone names.
    VERIFY(parses("PST"));
    VERIFY(parses("UTC"));
    VERIFY(parses("America/Los_Angeles"));

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
                VERIFY(parses(abbrev));
                ++checked;
            }
        VERIFY(checked > 500);
    }

    // What must keep failing. Walking the whole history also admits two-letter entries from
    // the pre-war era, so a longest match can stop short of the input's end; whatever follows
    // %Z in the format still catches that, and this pins the cost down to %Z-at-the-end.
    VERIFY(!parses("XYZ"));
    {
        IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone> ctx;
        const std::string in = "ATL 11:22";
        bool threw = false;
        try { obj.get(in.begin(), in.end(), ctx, std::string_view("%Z %H:%M")); }
        catch (IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    // Matching longest-first against known names is also what delimits %Z, which is why a
    // specifier may follow it with no separator. Scanning a character class could not: the
    // class has to hold '+' and '-' for abbreviations like "+08", and would swallow the %z.
    {
        IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone> ctx;
        const std::string in = "UTC+0800";
        VERIFY(obj.get(in.begin(), in.end(), ctx, std::string_view("%Z%z")) == in.end());
        VERIFY(ctx.m_offset == minutes{480});
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
        VERIFY(res == "1943-07-01 12:00:00 -0400 EWT");

        IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone> ctx;
        VERIFY(obj.get(res.begin(), res.end(), ctx, std::string_view("%F %T %z %Z")) == res.end());
        VERIFY(ctx.m_offset == -minutes{240});
    }

    dump_info("Done\n");
}

// The two tiers pinned apart. Whether %Z parses is the tier's decision and nothing else's:
// tz_level::offset matches it literally, which is exactly what put degrades it to for a value
// with no zone to name, and tz_level::zone parses it against the trie. Neither tier looks at
// what the trie happens to contain to decide which of the two it is doing.
void test_timeio_char_get_23()
{
    dump_info("Test timeio<char> get 23...");
    using namespace std::chrono;

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));

    auto off_ok = [&obj](const std::string& in, const char* fmt)
    {
        IOv2::time_parse_context<char, true, true, IOv2::tz_level::offset> ctx;
        try { return obj.get(in.begin(), in.end(), ctx, std::string_view(fmt)) == in.end(); }
        catch (IOv2::stream_error&) { return false; }
    };
    auto zone_ok = [&obj](const std::string& in, const char* fmt)
    {
        IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone> ctx;
        try { return obj.get(in.begin(), in.end(), ctx, std::string_view(fmt)) == in.end(); }
        catch (IOv2::stream_error&) { return false; }
    };

    // The literal %Z, which is what put writes when the value has no zone to offer.
    VERIFY(off_ok("%Z", "%Z"));
    VERIFY(!zone_ok("%Z", "%Z"));

    // A real zone token parses at tz_level::zone and only there. At tz_level::offset the format
    // is asking for the two characters %Z, which "UTC" is not -- put never writes a zone token
    // for a value that parses at that tier, so there is nothing to read back.
    VERIFY(zone_ok("UTC", "%Z"));
    VERIFY(!off_ok("UTC", "%Z"));
    VERIFY(zone_ok("PDT", "%Z"));
    VERIFY(!off_ok("PDT", "%Z"));

    // A run of letters the database does not know is rejected at both, for different reasons:
    // no trie entry at one tier, no literal match at the other.
    VERIFY(!zone_ok("XYZ", "%Z"));
    VERIFY(!off_ok("XYZ", "%Z"));

    // The literal is for *this* specifier, not for any percent sequence.
    VERIFY(!off_ok("%z", "%Z"));
    VERIFY(!off_ok("%Q", "%Z"));

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
        VERIFY(res == "2024-09-04 13:33:18 UNKNOWN");
        VERIFY(zone_ok(res, "%F %T %Z"));
#else
        VERIFY(res == "2024-09-04 13:33:18 %Z");
        VERIFY(off_ok(res, "%F %T %Z"));
#endif
    }

    // The same round trip through a locale whose own %c carries %Z, which is how this reaches
    // a caller who never wrote %Z: put_time(&t, "%c") on a tm that get_time filled in.
    {
        IOv2::timeio us(std::make_shared<IOv2::timeio_conf<char>>("en_US.UTF-8"));
        std::tm t{};
        t.tm_year = 124; t.tm_mon = 8; t.tm_mday = 4;
        t.tm_hour = 13; t.tm_min = 33; t.tm_sec = 18;

        std::string res;
        us.put(std::back_inserter(res), t, std::string_view("%c"));

        IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone> ctx;
        VERIFY(us.get(res.begin(), res.end(), ctx, std::string_view("%c")) == res.end());
    }

    dump_info("Done\n");
}

// expand_format vs put: the same table, read twice. expand_and_filter's switch is a compile-time
// mirror of do_put's, and this is what holds the two together -- for every value type, every
// specifier and every modifier, expand_format drops exactly what put degrades to a literal.
// Without this the two could drift apart silently, since neither one calls the other.
void test_timeio_char_expand_1()
{
    dump_info("Test timeio<char> expand 1...");
    using namespace std::chrono;

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));

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
                VERIFY(agree(emit, spec, mod, obj.expand_format<decltype(zt)>(spec, mod)));
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
                VERIFY(agree(emit, spec, mod, obj.expand_format<decltype(st)>(spec, mod)));
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
                VERIFY(agree(emit, spec, mod, obj.expand_format<decltype(lt)>(spec, mod)));
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
                VERIFY(agree(emit, spec, mod, obj.expand_format<year_month_day>(spec, mod)));
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
                VERIFY(agree(emit, spec, mod, obj.expand_format<decltype(hms)>(spec, mod)));
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
                VERIFY(agree(emit, spec, mod, obj.expand_format<std::tm>(spec, mod)));
                ++checked;
            }
    }

    VERIFY(checked == 6 * (53 * 3 - 2));

    // The pair held out of the sweep: a format cut short after its modifier is passed through
    // unchanged on both sides, so expand_format keeps it rather than reading past the end.
    VERIFY(obj.expand_format<year_month_day>("%E") == "%E");
    VERIFY(obj.expand_format<year_month_day>("%O") == "%O");
    VERIFY(obj.expand_format<year_month_day>("%Y %E") == "%Y %E");
    {
        std::string res;
        obj.put(std::back_inserter(res), year_month_day{year{2024}/month{9}/day{4}},
                std::string_view("%E"));
        VERIFY(res == "%E");
    }

    // The control: the cross-check would pass vacuously if expand_format simply kept everything,
    // so pin down that it really does drop, and really does keep.
    VERIFY(obj.expand_format<hh_mm_ss<seconds>>('Y').empty());
    VERIFY(obj.expand_format<year_month_day>('H').empty());
    VERIFY(obj.expand_format<local_time<seconds>>('Z').empty());
    VERIFY(!obj.expand_format<local_time<seconds>>('z').empty());
    VERIFY(!obj.expand_format<year_month_day>('Y').empty());
    VERIFY(!obj.expand_format<hh_mm_ss<seconds>>('H').empty());

    dump_info("Done\n");
}

// The two halves of expand_format that the specifier-by-specifier cross-check cannot see:
// compound specifiers being replaced by their contents, and a dropped specifier taking one
// adjacent separator with it.
void test_timeio_char_expand_2()
{
    dump_info("Test timeio<char> expand 2...");
    using namespace std::chrono;

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));

    using LT = local_time<seconds>;
    using ST = sys_time<seconds>;
    using HMS = hh_mm_ss<seconds>;

    // Fixed compounds expand whole, and vanish whole when the value cannot supply them.
    VERIFY(obj.expand_format<year_month_day>("%F") == "%Y-%m-%d");
    VERIFY(obj.expand_format<year_month_day>("%D") == "%m/%d/%y");
    VERIFY(obj.expand_format<HMS>("%T") == "%H:%M:%S");
    VERIFY(obj.expand_format<HMS>("%R") == "%H:%M");
    VERIFY(obj.expand_format<HMS>("%F %T") == "%H:%M:%S");
    VERIFY(obj.expand_format<year_month_day>("%F %T") == "%Y-%m-%d");

    // An unsuppliable compound is dropped whole rather than expanded, or its contents would
    // come back as a trail of orphaned punctuation.
    VERIFY(obj.expand_format<HMS>("%c").empty());
    VERIFY(obj.expand_format<HMS>("%x").empty());
    VERIFY(obj.expand_format<year_month_day>("%c").empty());
    VERIFY(obj.expand_format<year_month_day>("%X").empty());

    // Locale compounds expand to what the locale actually holds, expanded in turn.
    VERIFY(obj.expand_format<ST>("%X") == obj.expand_format<ST>(obj.time_format()));
    VERIFY(obj.expand_format<ST>("%x") == obj.expand_format<ST>(obj.date_format()));

    // Separators: a dropped specifier must not leave its punctuation behind.
    VERIFY(obj.expand_format<HMS>("%T %Z") == "%H:%M:%S");
    VERIFY(obj.expand_format<HMS>("%m/%d/%Y %T") == "%H:%M:%S");
    VERIFY(obj.expand_format<HMS>("%Y-%m-%d") == "");
    VERIFY(obj.expand_format<LT>("%T %Z") == "%H:%M:%S");
    VERIFY(obj.expand_format<LT>("%T %z") == "%H:%M:%S %z");

    // Only ASCII-ish punctuation and whitespace count as separators; a letter never does, so a
    // CJK unit character is not eaten along with the field it follows.
    VERIFY(obj.expand_format<LT>("%S\xe7\xa7\x92 %Z") == "%S\xe7\xa7\x92");

    // Bracket groups are all-or-nothing: a group emptied by filtering takes its brackets with
    // it, a group that keeps something keeps them, and brackets the format left unpaired or
    // empty on its own are never touched.
    VERIFY(obj.expand_format<HMS>("%T (%Z)") == "%H:%M:%S");
    VERIFY(obj.expand_format<HMS>("%T [%Z]") == "%H:%M:%S");
    VERIFY(obj.expand_format<HMS>("%T {%Z}") == "%H:%M:%S");
    VERIFY(obj.expand_format<HMS>("%T (%Z, %z)") == "%H:%M:%S");
    VERIFY(obj.expand_format<HMS>("%T ((%Z))") == "%H:%M:%S");
    VERIFY(obj.expand_format<HMS>("%T (%Z, x)") == "%H:%M:%S (x)");
    VERIFY(obj.expand_format<HMS>("%T (a (%Z) b)") == "%H:%M:%S (a b)");
    VERIFY(obj.expand_format<HMS>("%T ()") == "%H:%M:%S ()");
    VERIFY(obj.expand_format<HMS>("%T :-)") == "%H:%M:%S :-)");
    VERIFY(obj.expand_format<HMS>("(x) %Z") == "(x)");

    // A lone trailing % is kept, matching put; %% is a literal and never a specifier.
    VERIFY(obj.expand_format<year_month_day>("a%") == "a%");
    VERIFY(obj.expand_format<year_month_day>("%%Z") == "%%Z");
    VERIFY(obj.expand_format<year_month_day>("%%") == "%%");

    // What the whole thing was built for: a locale whose %c carries a %Z, run against a value
    // that has no zone identity to put in it.
    {
        IOv2::timeio us(std::make_shared<IOv2::timeio_conf<char>>("en_US.UTF-8"));

        const std::string zoned = us.expand_format<ST>("%c");
        const std::string bare  = us.expand_format<LT>("%c");

        // The premise of the test: this locale's %c really does reach a %Z.
        VERIFY(IOv2::timeio<char>::contains_specifier(us.date_time_format(), 'Z') ||
               IOv2::timeio<char>::contains_specifier(us.am_pm_format(), 'Z'));

        VERIFY(IOv2::timeio<char>::contains_specifier(zoned, 'Z'));
        VERIFY(!IOv2::timeio<char>::contains_specifier(bare, 'Z'));

        // Nothing but the zone field and its separator differ between the two.
        VERIFY(bare.size() < zoned.size());
        VERIFY(zoned.compare(0, bare.size(), bare) == 0);

        // And the expansion is a format string that really works: putting through it must not
        // leave a literal %Z in the output.
        const local_time<seconds> lt{
            local_days{year{2024}/month{9}/day{4}} + hours{13} + minutes{33} + seconds{18}};
        std::string res;
        us.put(std::back_inserter(res), lt, hours{-8}, std::string_view(bare));
        VERIFY(res.find('%') == std::string::npos);
    }

    dump_info("Done\n");
}

// contains_specifier: the question a caller asks of an expansion before deciding to append a
// field of their own. A plain find() answers it wrongly, which is why this exists.
void test_timeio_char_expand_3()
{
    dump_info("Test timeio<char> expand 3...");
    using namespace std::chrono;

    using tio = IOv2::timeio<char>;

    VERIFY(tio::contains_specifier("%Z", 'Z'));
    VERIFY(tio::contains_specifier("a %T %Z b", 'Z'));
    VERIFY(!tio::contains_specifier("xZ", 'Z'));
    VERIFY(!tio::contains_specifier("", 'Z'));

    // The trap: "%%Z" is a literal % followed by a literal Z, and every further % flips it back.
    VERIFY(!tio::contains_specifier("%%Z", 'Z'));
    VERIFY(tio::contains_specifier("%%%Z", 'Z'));
    VERIFY(!tio::contains_specifier("%%%%Z", 'Z'));
    VERIFY(tio::contains_specifier("%%%%%Z", 'Z'));

    // The control: a find() would report a hit on all four of those.
    VERIFY(std::string_view("%%Z").find("%Z") != std::string_view::npos);

    // Modifiers are part of the identity, not decoration.
    VERIFY(tio::contains_specifier("%EY", 'Y', 'E'));
    VERIFY(!tio::contains_specifier("%EY", 'Y'));
    VERIFY(!tio::contains_specifier("%Y", 'Y', 'E'));
    VERIFY(!tio::contains_specifier("%OY", 'Y', 'E'));
    VERIFY(tio::contains_specifier("%OY %EY", 'Y', 'E'));

    // A truncated tail is not a match, and does not read past the end.
    VERIFY(!tio::contains_specifier("%", 'Z'));
    VERIFY(!tio::contains_specifier("%E", 'Y', 'E'));
    VERIFY(!tio::contains_specifier("abc%", 'Z'));

    // The flow it exists for: expand, ask, append.
    {
        IOv2::timeio us(std::make_shared<IOv2::timeio_conf<char>>("en_US.UTF-8"));
        using LT = local_time<seconds>;

        std::string fmt = us.expand_format<LT>("%c");
        VERIFY(!tio::contains_specifier(fmt, 'Z'));
        fmt += " %Z";
        VERIFY(tio::contains_specifier(fmt, 'Z'));
    }

    dump_info("Done\n");
}

void test_timeio_char_unknown_zone_1()
{
    dump_info("Test timeio<char> unknown zone 1...");
    using namespace std::chrono;
    using tio = IOv2::timeio<char>;

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));

    // The token the put side writes when the field exists but names nothing.
    VERIFY(IOv2::ft_basic<tio>::s_unknown_zone == "UNKNOWN");

    std::tm tp = test_tm(18, 33, 13, 4, 9 - 1, 2024 - 1900, 3, 247, 0);

    // Whatever put writes for %Z must parse back through the trie: that is the whole
    // reason the token is registered there rather than being print-only.
    {
        std::string res;
        obj.put(std::back_inserter(res), tp, "%Y-%m-%d %H:%M:%S %Z");
        VERIFY(res == "2024-09-04 13:33:18 UNKNOWN");

        IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone> ctx;
        auto it = obj.get(res.cbegin(), res.cend(), ctx, "%Y-%m-%d %H:%M:%S %Z");
        VERIFY(it == res.cend());

        std::tm out{};
        out.tm_zone = "PRESET";
        ctx.convert_to(out);
        VERIFY(out.tm_year == 2024 - 1900);
        VERIFY(out.tm_hour == 13 && out.tm_min == 33 && out.tm_sec == 18);

        // Parsing UNKNOWN records no zone, but it does not leave tm_zone alone either: the
        // text said outright that there is no zone, so the preset name is cleared rather
        // than surviving to be written back out by the next put.
#ifdef __USE_MISC
        VERIFY(out.tm_zone != nullptr && *out.tm_zone == '\0');

        // And that is what closes the loop -- putting the parsed tm back reproduces the
        // token, where keeping "PRESET" would have written that name instead.
        std::string again;
        obj.put(std::back_inserter(again), out, "%Y-%m-%d %H:%M:%S %Z");
        VERIFY(again == res);
#endif
    }

    // A real abbreviation still round-trips, and is not swallowed by the new branch.
#ifdef __USE_MISC
    {
        std::tm named = tp;
        named.tm_zone = "PST";

        std::string res;
        obj.put(std::back_inserter(res), named, "%H:%M:%S %Z");
        VERIFY(res == "13:33:18 PST");

        IOv2::time_parse_context<char, false, true, IOv2::tz_level::zone> ctx;
        auto it = obj.get(res.cbegin(), res.cend(), ctx, "%H:%M:%S %Z");
        VERIFY(it == res.cend());
    }
#endif

    // expand_format keeps %Z for std::tm and drops it for the zone-less types, and that
    // claim now matches put exactly: every specifier it keeps, put can fill.
    {
        IOv2::timeio us(std::make_shared<IOv2::timeio_conf<char>>("en_US.UTF-8"));

        const std::string tm_fmt = us.expand_format<std::tm>("%c");
        const std::string lt_fmt = us.expand_format<local_time<seconds>>("%c");

#ifdef __USE_MISC
        VERIFY(tio::contains_specifier(tm_fmt, 'Z'));
#endif
        VERIFY(!tio::contains_specifier(lt_fmt, 'Z'));

        // Nothing survives the filter that put would degrade: no literal % in the output.
        std::string res;
        us.put(std::back_inserter(res), tp, std::string_view(tm_fmt));
        VERIFY(res.find('%') == std::string::npos);
#ifdef __USE_MISC
        VERIFY(res.find("UNKNOWN") != std::string::npos);
#endif
    }

    dump_info("Done\n");
}

namespace
{
    // Locale data no real locale would produce: each compound format string is a plain
    // assignable member, so the constructor's cycle check can be driven directly.
    struct rigged_conf : IOv2::timeio_conf<char>
    {
        using base = IOv2::timeio_conf<char>;
        using era_entry = IOv2::ft_basic<IOv2::timeio<char>>::era_entry;

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
            IOv2::timeio<char> obj(conf);
            (void)obj;
            return false;
        }
        catch (const std::runtime_error&) { return true; }
    }
}

void test_timeio_char_recursion_1()
{
    dump_info("Test timeio<char> format recursion check 1...");

    using namespace std::chrono;

    // The unrigged conf is accepted -- as is every real locale the other tests build, which
    // is the standing proof that this check does not reject actual locale data.
    VERIFY(!rejects([](rigged_conf&) {}));

    // Direct self-reference, one per compound. Each of these is the D_T_FMT == "%c" bug.
    VERIFY(rejects([](rigged_conf& c) { c.m_dt     = "%c";  }));
    VERIFY(rejects([](rigged_conf& c) { c.m_era_dt = "%Ec"; }));
    VERIFY(rejects([](rigged_conf& c) { c.m_d      = "%x";  }));
    VERIFY(rejects([](rigged_conf& c) { c.m_era_d  = "%Ex"; }));
    VERIFY(rejects([](rigged_conf& c) { c.m_t      = "%X";  }));
    VERIFY(rejects([](rigged_conf& c) { c.m_era_t  = "%EX"; }));
    VERIFY(rejects([](rigged_conf& c) { c.m_r      = "%r";  }));

    // The specifier need not sit alone, and brackets do not shield it -- a group's content
    // is expanded like anything else.
    VERIFY(rejects([](rigged_conf& c) { c.m_dt = "%Y-%m-%d %c"; }));
    VERIFY(rejects([](rigged_conf& c) { c.m_dt = "[%c]"; }));

    // Indirect cycles: two hops, three hops, and one routed through %r.
    VERIFY(rejects([](rigged_conf& c) { c.m_d = "%X"; c.m_t = "%x"; }));
    VERIFY(rejects([](rigged_conf& c) { c.m_dt = "%x"; c.m_d = "%X"; c.m_t = "%c"; }));
    VERIFY(rejects([](rigged_conf& c) { c.m_t = "%r"; c.m_r = "%X"; }));

    // The era and non-era tables are separate nodes, so a cycle can run through both.
    VERIFY(rejects([](rigged_conf& c) { c.m_dt = "%Ex"; c.m_era_d = "%c"; }));

    // %EY expands the matching era's format, so an era format naming itself is a cycle...
    VERIFY(rejects([](rigged_conf& c) { c.m_eras = {one_era("%EY")}; }));
    // ...and so is one that gets back to %EY through a locale compound.
    VERIFY(rejects([](rigged_conf& c) { c.m_dt = "%EY"; c.m_eras = {one_era("%c")}; }));
    // An era format that terminates is fine, even though %EY reaches it.
    VERIFY(!rejects([](rigged_conf& c) { c.m_dt = "%EY"; c.m_eras = {one_era("%Y")}; }));

    // Non-cycles a sloppier scan would flag. %%c is an escaped percent plus a literal c;
    // %Oc / %Ox / %OX / %Er / %Or degrade to literals in put and get and never recurse; a
    // trailing % or bare modifier has no specifier at all.
    VERIFY(!rejects([](rigged_conf& c) { c.m_dt = "%%c"; }));
    VERIFY(!rejects([](rigged_conf& c) { c.m_dt = "%Oc"; }));
    VERIFY(!rejects([](rigged_conf& c) { c.m_d  = "%Ox"; }));
    VERIFY(!rejects([](rigged_conf& c) { c.m_t  = "%OX"; }));
    VERIFY(!rejects([](rigged_conf& c) { c.m_r  = "%Er"; }));
    VERIFY(!rejects([](rigged_conf& c) { c.m_r  = "%Or"; }));
    VERIFY(!rejects([](rigged_conf& c) { c.m_dt = "%";   }));
    VERIFY(!rejects([](rigged_conf& c) { c.m_dt = "%E";  }));

    // A DAG with a shared node: %c reaches %r through both %x and %X. A two-colour DFS
    // would call the second arrival at %r a cycle; the grey/black split is what keeps a
    // diamond legal.
    VERIFY(!rejects([](rigged_conf& c)
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

        IOv2::timeio<char> obj(conf);

        const sys_time<seconds> st{
            sys_days{year{2024}/month{9}/day{4}} + hours{13} + minutes{33} + seconds{18}};

        VERIFY(obj.expand_format<sys_time<seconds>>("%c") == "%Y-%m-%d @ %H:%M");

        std::string res;
        obj.put(std::back_inserter(res), st, std::string_view("%c"));
        VERIFY(res == "2024-09-04 @ 13:33");
    }

    dump_info("Done\n");
}

void test_timeio_char_offset_clamp_1()
{
    dump_info("Test timeio<char> %z offset clamp 1...");
    using namespace std::chrono;

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));

    // %z can only spell four digits of +/-hhmm, so an offset it cannot express is pinned
    // to the widest one the parse side accepts rather than being rejected or truncated.
    constexpr long max_off = 23L * 3600 + 59 * 60 + 59;   // 23:59:59

    auto put_z = [&](long gmtoff)
    {
        std::tm tp = test_tm(18, 33, 13, 4, 9 - 1, 2024 - 1900, 3, 247, 0);
        tp.tm_gmtoff = gmtoff;
        tp.tm_zone = "CST";
        std::string res;
        obj.put(std::back_inserter(res), tp, "%z");
        return res;
    };

    // In range, %z is exact to the minute.
    VERIFY(put_z(0)        == "+0000");
    VERIFY(put_z(3600)     == "+0100");
    VERIFY(put_z(-19800)   == "-0530");
    VERIFY(put_z(max_off)  == "+2359");
    VERIFY(put_z(-max_off) == "-2359");

    // One second past the bound, and far past it, both clamp. 400 hours used to come out
    // as "+0000" because only the low four digits of hhmm survived.
    VERIFY(put_z(86400)          == "+2359");
    VERIFY(put_z(-86400)         == "-2359");
    VERIFY(put_z(86400L * 400)   == "+2359");
    VERIFY(put_z(-86400L * 400)  == "-2359");

    // The extremes are what made the old code negate INT_MIN.
    VERIFY(put_z(std::numeric_limits<int>::max())  == "+2359");
    VERIFY(put_z(std::numeric_limits<int>::min())  == "-2359");
    VERIFY(put_z(std::numeric_limits<long>::max()) == "+2359");
    VERIFY(put_z(std::numeric_limits<long>::min()) == "-2359");

    // 2^31 narrows to INT_MIN, so clamping after the cast instead of before would print a
    // positive offset with a minus sign. This is the case that pins the order.
    VERIFY(put_z(2147483648L)  == "+2359");
    VERIFY(put_z(-2147483648L) == "-2359");

    // The specifier has minute resolution: seconds are dropped, not rejected. Historical
    // LMT offsets really do carry them (Europe/Amsterdam was +00:19:32).
    VERIFY(put_z(1172)  == "+0019");
    VERIFY(put_z(-1172) == "-0019");

    // Whatever put writes has to parse back, which is what fixes the bound at 23:59:59.
    for (long off : {0L, 3600L, -19800L, max_off, -max_off, 86400L, 2147483648L})
    {
        const std::string text = put_z(off);
        IOv2::time_parse_context<char, true, true, IOv2::tz_level::zone> ctx;
        auto it = obj.get(text.cbegin(), text.cend(), ctx, "%z");
        VERIFY(it == text.cend());

        std::tm out{};
        ctx.convert_to(out);
        const long clamped = off > max_off ? max_off : off < -max_off ? -max_off : off;
        VERIFY(out.tm_gmtoff == (clamped / 60) * 60);
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
    VERIFY(put_z_local(seconds{0})           == "+0000");
    VERIFY(put_z_local(seconds{3600})        == "+0100");
    VERIFY(put_z_local(seconds{2147483648LL})  == "+2359");
    VERIFY(put_z_local(seconds{-2147483648LL}) == "-2359");
    VERIFY(put_z_local(seconds{4294967296LL})  == "+2359");

    dump_info("Done\n");
}
