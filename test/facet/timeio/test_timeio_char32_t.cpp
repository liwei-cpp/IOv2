#include <chrono>
#include <functional>
#include <list>
#include <facet/timeio.h>
#include <io/streambuf_iterator.h>

#include <common/dump_info.h>
#include <common/verify.h>
namespace
{
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

void test_timeio_char32_t_put_1()
{
    dump_info("Test timeio<char32_t> put 1...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("C"));
    auto tp = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    std::u32string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, U'%');
        VERIFY(res == U"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'a');
        VERIFY(res == U"Wed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'E');
        VERIFY(res == U"%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'O');
        VERIFY(res == U"%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'A');
        VERIFY(res == U"Wednesday");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'E');
        VERIFY(res == U"%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'O');
        VERIFY(res == U"%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'b');
        VERIFY(res == U"Sep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'E');
        VERIFY(res == U"%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'O');
        VERIFY(res == U"%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'h');
        VERIFY(res == U"Sep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'E');
        VERIFY(res == U"%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'O');
        VERIFY(res == U"%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'B');
        VERIFY(res == U"September");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'E');
        VERIFY(res == U"%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'O');
        VERIFY(res == U"%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'c');
        VERIFY(res == U"09/04/24 13:33:18 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'E');
        VERIFY(res == U"09/04/24 13:33:18 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'O');
        VERIFY(res == U"%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'C');
        VERIFY(res == U"20");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'E');
        VERIFY(res == U"20");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'O');
        VERIFY(res == U"%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'x');
        VERIFY(res == U"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'E');
        VERIFY(res == U"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'O');
        VERIFY(res == U"%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'D');
        VERIFY(res == U"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'E');
        VERIFY(res == U"%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'O');
        VERIFY(res == U"%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'd');
        VERIFY(res == U"04");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'E');
        VERIFY(res == U"%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'O');
        VERIFY(res == U"04");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'e');
        VERIFY(res == U" 4");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'E');
        VERIFY(res == U"%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'O');
        VERIFY(res == U" 4");
    }
      
    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'F');
        VERIFY(res == U"2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'E');
        VERIFY(res == U"%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'O');
        VERIFY(res == U"%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'H');
        VERIFY(res == U"13");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'E');
        VERIFY(res == U"%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'O');
        VERIFY(res == U"13");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'I');
        VERIFY(res == U"01");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'E');
        VERIFY(res == U"%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'O');
        VERIFY(res == U"01");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'j');
        VERIFY(res == U"248");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'E');
        VERIFY(res == U"%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'O');
        VERIFY(res == U"%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'M');
        VERIFY(res == U"33");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'E');
        VERIFY(res == U"%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'O');
        VERIFY(res == U"33");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'm');
        VERIFY(res == U"09");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'E');
        VERIFY(res == U"%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'O');
        VERIFY(res == U"09");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'n');
        VERIFY(res == U"\n");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'E');
        VERIFY(res == U"%En");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'O');
        VERIFY(res == U"%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'p');
        VERIFY(res == U"PM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'E');
        VERIFY(res == U"%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'O');
        VERIFY(res == U"%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'R');
        VERIFY(res == U"13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'E');
        VERIFY(res == U"%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'O');
        VERIFY(res == U"%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'r');
        VERIFY(res == U"01:33:18 PM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'E');
        VERIFY(res == U"%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'O');
        VERIFY(res == U"%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'S');
        VERIFY(res == U"18");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'E');
        VERIFY(res == U"%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'O');
        VERIFY(res == U"18");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'X');
        VERIFY(res == U"13:33:18 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'E');
        VERIFY(res == U"13:33:18 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'O');
        VERIFY(res == U"%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'T');
        VERIFY(res == U"13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'E');
        VERIFY(res == U"%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'O');
        VERIFY(res == U"%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U't');
        VERIFY(res == U"\t");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'E');
        VERIFY(res == U"%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'O');
        VERIFY(res == U"%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'u');
        VERIFY(res == U"3");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'E');
        VERIFY(res == U"%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'O');
        VERIFY(res == U"3");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'U');
        VERIFY(res == U"35");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'E');
        VERIFY(res == U"%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'O');
        VERIFY(res == U"35");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'V');
        VERIFY(res == U"36");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'E');
        VERIFY(res == U"%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'O');
        VERIFY(res == U"36");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'g');
        VERIFY(res == U"24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'E');
        VERIFY(res == U"%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'O');
        VERIFY(res == U"%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'G');
        VERIFY(res == U"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'E');
        VERIFY(res == U"%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'O');
        VERIFY(res == U"%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'W');
        VERIFY(res == U"36");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'E');
        VERIFY(res == U"%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'O');
        VERIFY(res == U"36");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'w');
        VERIFY(res == U"3");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'E');
        VERIFY(res == U"%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'O');
        VERIFY(res == U"3");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y');
        VERIFY(res == U"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'E');
        VERIFY(res == U"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'O');
        VERIFY(res == U"%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'y');
        VERIFY(res == U"24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'E');
        VERIFY(res == U"24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'O');
        VERIFY(res == U"24");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z');
        VERIFY(res == U"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'E');
        VERIFY(res == U"%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'O');
        VERIFY(res == U"%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'z');
        VERIFY(res == U"-0700");
        VERIFY(!(res.empty()));
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'E');
        VERIFY(res == U"%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'O');
        VERIFY(res == U"%Oz");
    }    

    dump_info("Done\n");
}

void test_timeio_char32_t_put_2()
{
    dump_info("Test timeio<char32_t> put 2...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("zh_CN.UTF-8"));
    auto tp = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    std::u32string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, U'%');
        VERIFY(res == U"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'a');
        VERIFY(res == U"三");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'E');
        VERIFY(res == U"%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'O');
        VERIFY(res == U"%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'A');
        VERIFY(res == U"星期三");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'E');
        VERIFY(res == U"%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'O');
        VERIFY(res == U"%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'b');
        VERIFY(res == U"9月");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'E');
        VERIFY(res == U"%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'O');
        VERIFY(res == U"%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'h');
        VERIFY(res == U"9月");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'E');
        VERIFY(res == U"%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'O');
        VERIFY(res == U"%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'B');
        VERIFY(res == U"九月");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'E');
        VERIFY(res == U"%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'O');
        VERIFY(res == U"%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'c');
        VERIFY(res == U"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'E');
        VERIFY(res == U"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'O');
        VERIFY(res == U"%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'C');
        VERIFY(res == U"20");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'E');
        VERIFY(res == U"20");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'O');
        VERIFY(res == U"%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'x');
        VERIFY(res == U"2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'E');
        VERIFY(res == U"2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'O');
        VERIFY(res == U"%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'D');
        VERIFY(res == U"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'E');
        VERIFY(res == U"%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'O');
        VERIFY(res == U"%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'd');
        VERIFY(res == U"04");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'E');
        VERIFY(res == U"%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'O');
        VERIFY(res == U"04");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'e');
        VERIFY(res == U" 4");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'E');
        VERIFY(res == U"%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'O');
        VERIFY(res == U" 4");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'F');
        VERIFY(res == U"2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'E');
        VERIFY(res == U"%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'O');
        VERIFY(res == U"%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'H');
        VERIFY(res == U"13");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'E');
        VERIFY(res == U"%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'O');
        VERIFY(res == U"13");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'I');
        VERIFY(res == U"01");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'E');
        VERIFY(res == U"%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'O');
        VERIFY(res == U"01");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'j');
        VERIFY(res == U"248");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'E');
        VERIFY(res == U"%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'O');
        VERIFY(res == U"%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'M');
        VERIFY(res == U"33");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'E');
        VERIFY(res == U"%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'O');
        VERIFY(res == U"33");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'm');
        VERIFY(res == U"09");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'E');
        VERIFY(res == U"%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'O');
        VERIFY(res == U"09");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'n');
        VERIFY(res == U"\n");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'E');
        VERIFY(res == U"%En");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'O');
        VERIFY(res == U"%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'p');
        VERIFY(res == U"下午");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'E');
        VERIFY(res == U"%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'O');
        VERIFY(res == U"%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'R');
        VERIFY(res == U"13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'E');
        VERIFY(res == U"%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'O');
        VERIFY(res == U"%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'r');
        VERIFY(res == U"下午 01时33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'E');
        VERIFY(res == U"%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'O');
        VERIFY(res == U"%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'S');
        VERIFY(res == U"18");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'E');
        VERIFY(res == U"%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'O');
        VERIFY(res == U"18");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'X');
        VERIFY(res == U"13时33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'E');
        VERIFY(res == U"13时33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'O');
        VERIFY(res == U"%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'T');
        VERIFY(res == U"13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'E');
        VERIFY(res == U"%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'O');
        VERIFY(res == U"%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U't');
        VERIFY(res == U"\t");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'E');
        VERIFY(res == U"%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'O');
        VERIFY(res == U"%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'u');
        VERIFY(res == U"3");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'E');
        VERIFY(res == U"%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'O');
        VERIFY(res == U"3");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'U');
        VERIFY(res == U"35");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'E');
        VERIFY(res == U"%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'O');
        VERIFY(res == U"35");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'V');
        VERIFY(res == U"36");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'E');
        VERIFY(res == U"%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'O');
        VERIFY(res == U"36");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'g');
        VERIFY(res == U"24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'E');
        VERIFY(res == U"%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'O');
        VERIFY(res == U"%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'G');
        VERIFY(res == U"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'E');
        VERIFY(res == U"%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'O');
        VERIFY(res == U"%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'W');
        VERIFY(res == U"36");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'E');
        VERIFY(res == U"%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'O');
        VERIFY(res == U"36");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'w');
        VERIFY(res == U"3");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'E');
        VERIFY(res == U"%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'O');
        VERIFY(res == U"3");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y');
        VERIFY(res == U"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'E');
        VERIFY(res == U"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'O');
        VERIFY(res == U"%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'y');
        VERIFY(res == U"24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'E');
        VERIFY(res == U"24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'O');
        VERIFY(res == U"24");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z');
        VERIFY(res == U"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'E');
        VERIFY(res == U"%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'O');
        VERIFY(res == U"%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'z');
        VERIFY(res == U"-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'E');
        VERIFY(res == U"%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'O');
        VERIFY(res == U"%Oz");
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_put_3()
{
    dump_info("Test timeio<char32_t> put 3...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("ja_JP.UTF-8"));
    auto tp = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    std::u32string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, U'%');
        VERIFY(res == U"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'a');
        VERIFY(res == U"水");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'E');
        VERIFY(res == U"%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'O');
        VERIFY(res == U"%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'A');
        VERIFY(res == U"水曜日");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'E');
        VERIFY(res == U"%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'O');
        VERIFY(res == U"%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'b');
        VERIFY(res == U" 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'E');
        VERIFY(res == U"%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'O');
        VERIFY(res == U"%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'h');
        VERIFY(res == U" 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'E');
        VERIFY(res == U"%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'O');
        VERIFY(res == U"%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'B');
        VERIFY(res == U"9月");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'E');
        VERIFY(res == U"%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'O');
        VERIFY(res == U"%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'c');
        VERIFY(res == U"2024年09月04日 13時33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'E');
        VERIFY(res == U"令和6年09月04日 13時33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'O');
        VERIFY(res == U"%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'C');
        VERIFY(res == U"20");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'E');
        VERIFY(res == U"令和");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'O');
        VERIFY(res == U"%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'x');
        VERIFY(res == U"2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'E');
        VERIFY(res == U"令和6年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'O');
        VERIFY(res == U"%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'D');
        VERIFY(res == U"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'E');
        VERIFY(res == U"%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'O');
        VERIFY(res == U"%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'd');
        VERIFY(res == U"04");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'E');
        VERIFY(res == U"%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'O');
        VERIFY(res == U"四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'e');
        VERIFY(res == U" 4");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'E');
        VERIFY(res == U"%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'O');
        VERIFY(res == U"四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'F');
        VERIFY(res == U"2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'E');
        VERIFY(res == U"%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'O');
        VERIFY(res == U"%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'H');
        VERIFY(res == U"13");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'E');
        VERIFY(res == U"%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'O');
        VERIFY(res == U"十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'I');
        VERIFY(res == U"01");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'E');
        VERIFY(res == U"%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'O');
        VERIFY(res == U"一");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'j');
        VERIFY(res == U"248");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'E');
        VERIFY(res == U"%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'O');
        VERIFY(res == U"%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'M');
        VERIFY(res == U"33");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'E');
        VERIFY(res == U"%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'O');
        VERIFY(res == U"三十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'm');
        VERIFY(res == U"09");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'E');
        VERIFY(res == U"%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'O');
        VERIFY(res == U"九");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'n');
        VERIFY(res == U"\n");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'E');
        VERIFY(res == U"%En");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'O');
        VERIFY(res == U"%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'p');
        VERIFY(res == U"午後");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'E');
        VERIFY(res == U"%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'O');
        VERIFY(res == U"%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'R');
        VERIFY(res == U"13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'E');
        VERIFY(res == U"%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'O');
        VERIFY(res == U"%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'r');
        VERIFY(res == U"午後01時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'E');
        VERIFY(res == U"%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'O');
        VERIFY(res == U"%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'S');
        VERIFY(res == U"18");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'E');
        VERIFY(res == U"%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'O');
        VERIFY(res == U"十八");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'X');
        VERIFY(res == U"13時33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'E');
        VERIFY(res == U"13時33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'O');
        VERIFY(res == U"%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'T');
        VERIFY(res == U"13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'E');
        VERIFY(res == U"%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'O');
        VERIFY(res == U"%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U't');
        VERIFY(res == U"\t");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'E');
        VERIFY(res == U"%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'O');
        VERIFY(res == U"%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'u');
        VERIFY(res == U"3");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'E');
        VERIFY(res == U"%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'O');
        VERIFY(res == U"三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'U');
        VERIFY(res == U"35");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'E');
        VERIFY(res == U"%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'O');
        VERIFY(res == U"三十五");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'V');
        VERIFY(res == U"36");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'E');
        VERIFY(res == U"%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'O');
        VERIFY(res == U"三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'g');
        VERIFY(res == U"24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'E');
        VERIFY(res == U"%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'O');
        VERIFY(res == U"%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'G');
        VERIFY(res == U"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'E');
        VERIFY(res == U"%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'O');
        VERIFY(res == U"%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'W');
        VERIFY(res == U"36");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'E');
        VERIFY(res == U"%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'O');
        VERIFY(res == U"三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'w');
        VERIFY(res == U"3");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'E');
        VERIFY(res == U"%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'O');
        VERIFY(res == U"三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y');
        VERIFY(res == U"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'E');
        VERIFY(res == U"令和6年");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'O');
        VERIFY(res == U"%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'y');
        VERIFY(res == U"24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'E');
        VERIFY(res == U"6");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'O');
        VERIFY(res == U"二十四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z');
        VERIFY(res == U"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'E');
        VERIFY(res == U"%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'O');
        VERIFY(res == U"%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'z');
        VERIFY(res == U"-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'E');
        VERIFY(res == U"%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'O');
        VERIFY(res == U"%Oz");
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_put_4()
{
    dump_info("Test timeio<char32_t> put 4...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("C"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::u32string oss;
    {
        obj.put(std::back_inserter(oss), time1, U'a');
        VERIFY(oss == U"Sun");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, U'x');
        VERIFY(oss == U"04/04/71");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, U'X');
        VERIFY(oss == U"12:00:00 America/Los_Angeles");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, U'x', U'E');
        VERIFY(oss == U"04/04/71");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, U'X', U'E');
        VERIFY(oss == U"12:00:00 America/Los_Angeles");
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_put_5()
{
    dump_info("Test timeio<char32_t> put 5...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("de_DE.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::u32string oss;
    {
        obj.put(std::back_inserter(oss), time1, U'a');
        VERIFY(!((oss != U"Son") && (oss != U"So")));
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'x');
        VERIFY(oss == U"04.04.1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'X');
        VERIFY(oss == U"12:00:00 America/Los_Angeles");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'x', U'E');
        VERIFY(oss == U"04.04.1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'X', U'E');
        VERIFY(oss == U"12:00:00 America/Los_Angeles");
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_put_6()
{
    dump_info("Test timeio<char32_t> put 6...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("en_HK.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::u32string oss;
    {
        obj.put(std::back_inserter(oss), time1, U'a');
        VERIFY(oss == U"Sun");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'x');
        VERIFY(oss == U"Sunday, April 04, 1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'X');
        VERIFY(oss.find(U"12:00:00") != std::u32string::npos);
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'x', U'E');
        VERIFY(oss == U"Sunday, April 04, 1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'X', U'E');
        VERIFY(oss.find(U"12:00:00") != std::u32string::npos);
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_put_7()
{
    dump_info("Test timeio<char32_t> put 7...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("es_ES.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::u32string oss;
    {
        obj.put(std::back_inserter(oss), time1, U'a');
        VERIFY(oss == U"dom");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'x');
        VERIFY(oss == U"04/04/71");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'X');
        VERIFY(oss == U"12:00:00 America/Los_Angeles");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'x', U'E');
        VERIFY(oss == U"04/04/71");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'X', U'E');
        VERIFY(oss == U"12:00:00 America/Los_Angeles");
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_put_8()
{
    dump_info("Test timeio<char32_t> put 8...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("C"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::u32string date = U"%A, the second of %B";
    const std::u32string date_ex = U"%Ex";
    std::u32string oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        VERIFY(oss == U"Sunday, the second of April");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        VERIFY(oss != oss2);
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_put_9()
{
    dump_info("Test timeio<char32_t> put 9...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("de_DE.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::u32string date = U"%A, the second of %B";
    const std::u32string date_ex = U"%Ex";
    std::u32string oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        VERIFY(oss == U"Sonntag, the second of April");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        VERIFY(oss != oss2);
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_put_10()
{
    dump_info("Test timeio<char32_t> put 10...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("en_HK.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::u32string date = U"%A, the second of %B";
    const std::u32string date_ex = U"%Ex";
    std::u32string oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        VERIFY(oss == U"Sunday, the second of April");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        VERIFY(oss != oss2);
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_put_11()
{
    dump_info("Test timeio<char32_t> put 11...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("fr_FR.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::u32string date = U"%A, the second of %B";
    const std::u32string date_ex = U"%Ex";
    std::u32string oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        VERIFY(oss == U"dimanche, the second of avril");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        VERIFY(oss != oss2);
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_put_12()
{
    dump_info("Test timeio<char32_t> put 12...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("C"));
    auto time_sanity = create_zoned_time(1997, 6, 26, 12, 0, 0, "America/Los_Angeles");

    std::u32string res(50, 'x');
    const std::u32string date = U"%T, %A, the second of %B, %Y";
        
    auto ret1 = obj.put(res.begin(), time_sanity, date);
    std::u32string sanity1(res.begin(), ret1);
    VERIFY(res == U"12:00:00, Thursday, the second of June, 1997xxxxxx");
    VERIFY(sanity1 == U"12:00:00, Thursday, the second of June, 1997");

    dump_info("Done\n");
}

void test_timeio_char32_t_put_13()
{
    dump_info("Test timeio<char32_t> put 13...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("C"));
    auto time_sanity = create_zoned_time(1997, 6, 24, 12, 0, 0, "America/Los_Angeles");

    std::u32string res(50, 'x');

    auto ret1 = obj.put(res.begin(), time_sanity, 'A');
    std::u32string sanity1(res.begin(), ret1);
    VERIFY(res == U"Tuesdayxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    VERIFY(sanity1 == U"Tuesday");

    dump_info("Done\n");
}

void test_timeio_char32_t_put_14()
{
    dump_info("Test timeio<char32_t> put 14...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("ta_IN.UTF-8"));
    const tm time1 = test_tm(0, 0, 12, 4, 3, 71, 0, 93, 0);
    auto zt = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::u32string res;
    obj.put(std::back_inserter(res), zt, 'c');

    char time_buffer[128];
    setlocale(LC_ALL, "ta_IN");
    std::strftime(time_buffer, 128, "%c", &time1);
    setlocale(LC_ALL, "C");

    VERIFY(IOv2::detail::to_u32string(time_buffer, "ta_IN.UTF-8") + std::u32string(U" America/Los_Angeles") == res);

    dump_info("Done\n");
}

void test_timeio_char32_t_put_15()
{
    dump_info("Test timeio<char32_t> put 15...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("ja_JP.UTF-8"));
    using namespace std::chrono;
    year_month_day tp{year{2024}, month{9}, day{4}};

    std::u32string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, U'%');
        VERIFY(res == U"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'a');
        VERIFY(res == U"水");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'E');
        VERIFY(res == U"%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'O');
        VERIFY(res == U"%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'A');
        VERIFY(res == U"水曜日");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'E');
        VERIFY(res == U"%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'O');
        VERIFY(res == U"%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'b');
        VERIFY(res == U" 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'E');
        VERIFY(res == U"%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'O');
        VERIFY(res == U"%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'h');
        VERIFY(res == U" 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'E');
        VERIFY(res == U"%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'O');
        VERIFY(res == U"%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'B');
        VERIFY(res == U"9月");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'E');
        VERIFY(res == U"%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'O');
        VERIFY(res == U"%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'c');
        VERIFY(res == U"%c");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'E');
        VERIFY(res == U"%Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'O');
        VERIFY(res == U"%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'C');
        VERIFY(res == U"20");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'E');
        VERIFY(res == U"令和");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'O');
        VERIFY(res == U"%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'x');
        VERIFY(res == U"2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'E');
        VERIFY(res == U"令和6年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'O');
        VERIFY(res == U"%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'D');
        VERIFY(res == U"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'E');
        VERIFY(res == U"%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'O');
        VERIFY(res == U"%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'd');
        VERIFY(res == U"04");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'E');
        VERIFY(res == U"%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'O');
        VERIFY(res == U"四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'e');
        VERIFY(res == U" 4");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'E');
        VERIFY(res == U"%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'O');
        VERIFY(res == U"四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'F');
        VERIFY(res == U"2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'E');
        VERIFY(res == U"%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'O');
        VERIFY(res == U"%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'H');
        VERIFY(res == U"%H");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'E');
        VERIFY(res == U"%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'O');
        VERIFY(res == U"%OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'I');
        VERIFY(res == U"%I");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'E');
        VERIFY(res == U"%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'O');
        VERIFY(res == U"%OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'j');
        VERIFY(res == U"248");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'E');
        VERIFY(res == U"%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'O');
        VERIFY(res == U"%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'M');
        VERIFY(res == U"%M");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'E');
        VERIFY(res == U"%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'O');
        VERIFY(res == U"%OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'm');
        VERIFY(res == U"09");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'E');
        VERIFY(res == U"%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'O');
        VERIFY(res == U"九");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'n');
        VERIFY(res == U"\n");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'E');
        VERIFY(res == U"%En");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'O');
        VERIFY(res == U"%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'p');
        VERIFY(res == U"%p");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'E');
        VERIFY(res == U"%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'O');
        VERIFY(res == U"%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'R');
        VERIFY(res == U"%R");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'E');
        VERIFY(res == U"%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'O');
        VERIFY(res == U"%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'r');
        VERIFY(res == U"%r");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'E');
        VERIFY(res == U"%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'O');
        VERIFY(res == U"%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'S');
        VERIFY(res == U"%S");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'E');
        VERIFY(res == U"%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'O');
        VERIFY(res == U"%OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'X');
        VERIFY(res == U"%X");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'E');
        VERIFY(res == U"%EX");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'O');
        VERIFY(res == U"%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'T');
        VERIFY(res == U"%T");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'E');
        VERIFY(res == U"%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'O');
        VERIFY(res == U"%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U't');
        VERIFY(res == U"\t");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'E');
        VERIFY(res == U"%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'O');
        VERIFY(res == U"%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'u');
        VERIFY(res == U"3");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'E');
        VERIFY(res == U"%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'O');
        VERIFY(res == U"三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'U');
        VERIFY(res == U"35");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'E');
        VERIFY(res == U"%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'O');
        VERIFY(res == U"三十五");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'V');
        VERIFY(res == U"36");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'E');
        VERIFY(res == U"%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'O');
        VERIFY(res == U"三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'g');
        VERIFY(res == U"24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'E');
        VERIFY(res == U"%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'O');
        VERIFY(res == U"%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'G');
        VERIFY(res == U"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'E');
        VERIFY(res == U"%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'O');
        VERIFY(res == U"%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'W');
        VERIFY(res == U"36");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'E');
        VERIFY(res == U"%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'O');
        VERIFY(res == U"三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'w');
        VERIFY(res == U"3");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'E');
        VERIFY(res == U"%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'O');
        VERIFY(res == U"三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y');
        VERIFY(res == U"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'E');
        VERIFY(res == U"令和6年");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'O');
        VERIFY(res == U"%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'y');
        VERIFY(res == U"24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'E');
        VERIFY(res == U"6");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'O');
        VERIFY(res == U"二十四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z');
        VERIFY(res == U"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'E');
        VERIFY(res == U"%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'O');
        VERIFY(res == U"%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'z');
        VERIFY(res == U"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'E');
        VERIFY(res == U"%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'O');
        VERIFY(res == U"%Oz");
    }
    dump_info("Done\n");
}

void test_timeio_char32_t_put_16()
{
    dump_info("Test timeio<char32_t> put 16...");

    using namespace std::chrono;
    seconds time_sec = hours{13} + minutes{33} + seconds{18};
    std::chrono::hh_mm_ss tp{time_sec};

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("ja_JP.UTF-8"));
    std::u32string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, U'%');
        VERIFY(res == U"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'a');
        VERIFY(res == U"%a");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'E');
        VERIFY(res == U"%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'O');
        VERIFY(res == U"%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'A');
        VERIFY(res == U"%A");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'E');
        VERIFY(res == U"%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'O');
        VERIFY(res == U"%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'b');
        VERIFY(res == U"%b");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'E');
        VERIFY(res == U"%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'O');
        VERIFY(res == U"%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'h');
        VERIFY(res == U"%h");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'E');
        VERIFY(res == U"%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'O');
        VERIFY(res == U"%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'B');
        VERIFY(res == U"%B");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'E');
        VERIFY(res == U"%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'O');
        VERIFY(res == U"%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'c');
        VERIFY(res == U"%c");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'E');
        VERIFY(res == U"%Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'O');
        VERIFY(res == U"%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'x');
        VERIFY(res == U"%x");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'E');
        VERIFY(res == U"%Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'O');
        VERIFY(res == U"%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'D');
        VERIFY(res == U"%D");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'E');
        VERIFY(res == U"%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'O');
        VERIFY(res == U"%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'd');
        VERIFY(res == U"%d");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'E');
        VERIFY(res == U"%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'O');
        VERIFY(res == U"%Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'e');
        VERIFY(res == U"%e");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'E');
        VERIFY(res == U"%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'O');
        VERIFY(res == U"%Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'F');
        VERIFY(res == U"%F");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'E');
        VERIFY(res == U"%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'O');
        VERIFY(res == U"%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'H');
        VERIFY(res == U"13");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'E');
        VERIFY(res == U"%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'O');
        VERIFY(res == U"十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'I');
        VERIFY(res == U"01");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'E');
        VERIFY(res == U"%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'O');
        VERIFY(res == U"一");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'j');
        VERIFY(res == U"%j");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'E');
        VERIFY(res == U"%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'O');
        VERIFY(res == U"%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'M');
        VERIFY(res == U"33");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'E');
        VERIFY(res == U"%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'O');
        VERIFY(res == U"三十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'm');
        VERIFY(res == U"%m");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'E');
        VERIFY(res == U"%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'O');
        VERIFY(res == U"%Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'n');
        VERIFY(res == U"\n");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'E');
        VERIFY(res == U"%En");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'O');
        VERIFY(res == U"%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'p');
        VERIFY(res == U"午後");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'E');
        VERIFY(res == U"%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'O');
        VERIFY(res == U"%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'R');
        VERIFY(res == U"13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'E');
        VERIFY(res == U"%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'O');
        VERIFY(res == U"%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'r');
        VERIFY(res == U"午後01時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'E');
        VERIFY(res == U"%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'O');
        VERIFY(res == U"%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'S');
        VERIFY(res == U"18");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'E');
        VERIFY(res == U"%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'O');
        VERIFY(res == U"十八");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'X');
        VERIFY(res == U"13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'E');
        VERIFY(res == U"13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'O');
        VERIFY(res == U"%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'T');
        VERIFY(res == U"13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'E');
        VERIFY(res == U"%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'O');
        VERIFY(res == U"%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U't');
        VERIFY(res == U"\t");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'E');
        VERIFY(res == U"%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'O');
        VERIFY(res == U"%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'u');
        VERIFY(res == U"%u");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'E');
        VERIFY(res == U"%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'O');
        VERIFY(res == U"%Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'U');
        VERIFY(res == U"%U");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'E');
        VERIFY(res == U"%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'O');
        VERIFY(res == U"%OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'V');
        VERIFY(res == U"%V");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'E');
        VERIFY(res == U"%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'O');
        VERIFY(res == U"%OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'g');
        VERIFY(res == U"%g");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'E');
        VERIFY(res == U"%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'O');
        VERIFY(res == U"%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'G');
        VERIFY(res == U"%G");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'E');
        VERIFY(res == U"%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'O');
        VERIFY(res == U"%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'W');
        VERIFY(res == U"%W");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'E');
        VERIFY(res == U"%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'O');
        VERIFY(res == U"%OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'w');
        VERIFY(res == U"%w");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'E');
        VERIFY(res == U"%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'O');
        VERIFY(res == U"%Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y');
        VERIFY(res == U"%Y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'E');
        VERIFY(res == U"%EY");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'O');
        VERIFY(res == U"%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'y');
        VERIFY(res == U"%y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'E');
        VERIFY(res == U"%Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'O');
        VERIFY(res == U"%Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z'); VERIFY(res == U"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'E');
        VERIFY(res == U"%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'O');
        VERIFY(res == U"%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'z'); VERIFY(res == U"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'E');
        VERIFY(res == U"%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'O');
        VERIFY(res == U"%Oz");
    }
    dump_info("Done\n");
}

void test_timeio_char32_t_put_17()
{
    dump_info("Test timeio<char32_t> put 17...");
    std::tm tp = test_tm(18, 33, 13, 4, 9 - 1, 2024 - 1900, 0, 0, 0);
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("ja_JP.UTF-8"));

    std::u32string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, U'%');
        VERIFY(res == U"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'a');
        VERIFY(res == U"水");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'E');
        VERIFY(res == U"%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'O');
        VERIFY(res == U"%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'A');
        VERIFY(res == U"水曜日");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'E');
        VERIFY(res == U"%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'O');
        VERIFY(res == U"%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'b');
        VERIFY(res == U" 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'E');
        VERIFY(res == U"%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'O');
        VERIFY(res == U"%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'h');
        VERIFY(res == U" 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'E');
        VERIFY(res == U"%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'O');
        VERIFY(res == U"%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'B');
        VERIFY(res == U"9月");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'E');
        VERIFY(res == U"%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'O');
        VERIFY(res == U"%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'c');
        VERIFY(res == U"2024年09月04日 13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'E');
        VERIFY(res == U"令和6年09月04日 13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'O');
        VERIFY(res == U"%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'C');
        VERIFY(res == U"20");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'E');
        VERIFY(res == U"令和");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'O');
        VERIFY(res == U"%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'x');
        VERIFY(res == U"2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'E');
        VERIFY(res == U"令和6年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'O');
        VERIFY(res == U"%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'D');
        VERIFY(res == U"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'E');
        VERIFY(res == U"%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'O');
        VERIFY(res == U"%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'd');
        VERIFY(res == U"04");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'E');
        VERIFY(res == U"%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'O');
        VERIFY(res == U"四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'e');
        VERIFY(res == U" 4");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'E');
        VERIFY(res == U"%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'O');
        VERIFY(res == U"四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'F');
        VERIFY(res == U"2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'E');
        VERIFY(res == U"%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'O');
        VERIFY(res == U"%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'H');
        VERIFY(res == U"13");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'E');
        VERIFY(res == U"%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'O');
        VERIFY(res == U"十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'I');
        VERIFY(res == U"01");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'E');
        VERIFY(res == U"%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'O');
        VERIFY(res == U"一");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'j');
        VERIFY(res == U"248");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'E');
        VERIFY(res == U"%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'O');
        VERIFY(res == U"%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'M');
        VERIFY(res == U"33");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'E');
        VERIFY(res == U"%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'O');
        VERIFY(res == U"三十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'm');
        VERIFY(res == U"09");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'E');
        VERIFY(res == U"%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'O');
        VERIFY(res == U"九");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'n');
        VERIFY(res == U"\n");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'E');
        VERIFY(res == U"%En");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'O');
        VERIFY(res == U"%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'p');
        VERIFY(res == U"午後");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'E');
        VERIFY(res == U"%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'O');
        VERIFY(res == U"%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'R');
        VERIFY(res == U"13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'E');
        VERIFY(res == U"%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'O');
        VERIFY(res == U"%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'r');
        VERIFY(res == U"午後01時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'E');
        VERIFY(res == U"%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'O');
        VERIFY(res == U"%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'S');
        VERIFY(res == U"18");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'E');
        VERIFY(res == U"%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'O');
        VERIFY(res == U"十八");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'X');
        VERIFY(res == U"13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'E');
        VERIFY(res == U"13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'O');
        VERIFY(res == U"%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'T');
        VERIFY(res == U"13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'E');
        VERIFY(res == U"%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'O');
        VERIFY(res == U"%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U't');
        VERIFY(res == U"\t");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'E');
        VERIFY(res == U"%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'O');
        VERIFY(res == U"%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'u');
        VERIFY(res == U"3");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'E');
        VERIFY(res == U"%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'O');
        VERIFY(res == U"三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'U');
        VERIFY(res == U"35");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'E');
        VERIFY(res == U"%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'O');
        VERIFY(res == U"三十五");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'V');
        VERIFY(res == U"36");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'E');
        VERIFY(res == U"%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'O');
        VERIFY(res == U"三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'g');
        VERIFY(res == U"24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'E');
        VERIFY(res == U"%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'O');
        VERIFY(res == U"%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'G');
        VERIFY(res == U"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'E');
        VERIFY(res == U"%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'O');
        VERIFY(res == U"%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'W');
        VERIFY(res == U"36");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'E');
        VERIFY(res == U"%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'O');
        VERIFY(res == U"三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'w');
        VERIFY(res == U"3");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'E');
        VERIFY(res == U"%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'O');
        VERIFY(res == U"三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y');
        VERIFY(res == U"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'E');
        VERIFY(res == U"令和6年");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'O');
        VERIFY(res == U"%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'y');
        VERIFY(res == U"24");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'E');
        VERIFY(res == U"6");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'O');
        VERIFY(res == U"二十四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z'); VERIFY(res == U"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'E');
        VERIFY(res == U"%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'O');
        VERIFY(res == U"%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'z'); VERIFY(res == U"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'E');
        VERIFY(res == U"%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'O');
        VERIFY(res == U"%Oz");
    }
    dump_info("Done\n");
}

namespace
{
    constexpr static IOv2::ios_defs::iostate febit = IOv2::ios_defs::eofbit | IOv2::ios_defs::strfailbit;

    template <typename T = IOv2::time_parse_context<char32_t>, bool HaveDate = true, bool HaveTime = true, bool HaveTimeZone = true>
    T CheckGet(const IOv2::timeio<char32_t>& obj, const std::u32string& input,
               char32_t fmt, char32_t modif,
               IOv2::ios_defs::iostate err_exp, size_t consume_exp = (size_t)-1)
    {
        if (consume_exp == (size_t)-1) consume_exp = input.size();
        IOv2::time_parse_context<char32_t, HaveDate, HaveTime, HaveTimeZone> ctx1, ctx2, ctx3;
        if (err_exp == IOv2::ios_defs::goodbit)
        {
            VERIFY(obj.get(input.begin(), input.end(), ctx1, fmt, modif) != input.end());
            {
                std::list<char32_t> lst_input(input.begin(), input.end());
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
                std::list<char32_t> lst_input(input.begin(), input.end());
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
                std::list<char32_t> lst_input(input.begin(), input.end());
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
        return static_cast<T>(ctx1);
    }

    template <typename T = IOv2::time_parse_context<char32_t>, bool HaveDate = true, bool HaveTime = true, bool HaveTimeZone = true>
    T CheckGet(const IOv2::timeio<char32_t>& obj, const std::u32string& input,
               const std::u32string& fmt,
               IOv2::ios_defs::iostate err_exp, size_t consume_exp = (size_t)-1)
    {
        if (consume_exp == (size_t)-1) consume_exp = input.size();
        IOv2::time_parse_context<char32_t, HaveDate, HaveTime, HaveTimeZone> ctx1, ctx2, ctx3;
        if (err_exp == IOv2::ios_defs::goodbit)
        {
            VERIFY(obj.get(input.begin(), input.end(), ctx1, fmt) != input.end());
            {
                std::list<char32_t> lst_input(input.begin(), input.end());
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
                std::list<char32_t> lst_input(input.begin(), input.end());
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
                std::list<char32_t> lst_input(input.begin(), input.end());
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
        return static_cast<T>(ctx1);
    }
}

void test_timeio_char32_t_get_1()
{
    dump_info("Test timeio<char32_t> get 1...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("C"));
    CheckGet(obj, U"%",   U'%',  0,  IOv2::ios_defs::eofbit);
    CheckGet(obj, U"x",   U'%',  0,  IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%",   U'%', U'E', febit);
    CheckGet(obj, U"%E%", U'%', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"%",   U'%', U'O', febit);
    CheckGet(obj, U"%O%", U'%', U'O', IOv2::ios_defs::eofbit);

    VERIFY(CheckGet(obj, U"Wed", U'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, U"%Ea", U'a', U'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, U"a",   U'a', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oa", U'a', U'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, U"a",   U'a', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"Wednesday", U'A', 0, IOv2::ios_defs::eofbit, 9).m_wday == 3);
    CheckGet(obj, U"%EA", U'A', U'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, U"A",   U'A', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OA", U'A', U'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, U"A",   U'A', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"Sep", U'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, U"%Eb", U'b', U'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, U"b",   U'b', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Ob", U'b', U'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, U"b",   U'b', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"September", U'B', 0, IOv2::ios_defs::eofbit, 9).m_month == 9);
    CheckGet(obj, U"%EB", U'B', U'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, U"B",   U'B', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OB", U'B', U'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, U"B",   U'B', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"Sep", U'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, U"%Eh", U'h', U'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, U"h",   U'h', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oh", U'h', U'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, U"h",   U'h', U'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(CheckGet<year_month_day>(obj, U"09/04/24 13:33:18 America/Los_Angeles", U'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"09/04/24 13:33:18 America/Los_Angeles", U'c', U'E', IOv2::ios_defs::eofbit, 17) == check_date1);
    CheckGet(obj, U"c",   U'c', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oc", U'c', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"c",   U'c', U'O', IOv2::ios_defs::strfailbit, 0);


    VERIFY(CheckGet(obj, U"20", U'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(CheckGet(obj, U"20", U'C', U'E', IOv2::ios_defs::eofbit).m_century == 20);
    CheckGet(obj, U"C",   U'C', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OC", U'C', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"C",   U'C', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"04", U'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, U"04", U'd', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, U"%Ed", U'd', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"d",   U'd', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"d",   U'd', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"4", U'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, U"4", U'e', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, U"%Ee", U'e', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"e",   U'e', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"e",   U'e', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"2024-09-04", U'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"%EF", U'F', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"F",   U'F', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OF", U'F', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"F",   U'F', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"09/04/24", U'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"09/04/24", U'x', U'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"x",   U'x', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Ox", U'x', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"x",   U'x', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"09/04/24", U'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"%ED", U'D', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"D",   U'D', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OD", U'D', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"D",   U'D', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"13", U'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, U"13", U'H', U'O', IOv2::ios_defs::eofbit).m_hour == 13);
    CheckGet(obj, U"%EH", U'H', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"H",   U'H', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"H",   U'H', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"01", U'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, U"01", U'I', U'O', IOv2::ios_defs::eofbit).m_hour == 1);
    CheckGet(obj, U"%EI", U'I', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"I",   U'I', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"I",   U'I', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"248", U'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(CheckGet<year_month_day>(obj, U"2024 248", U"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"%Ej", U'j', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"j",   U'j', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oj", U'j', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"j",   U'j', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"09", U'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, U"09", U'm', U'O', IOv2::ios_defs::eofbit).m_month == 9);
    CheckGet(obj, U"%Em", U'm', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"m",   U'm', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"m",   U'm', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"33", U'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, U"33", U'M', U'O', IOv2::ios_defs::eofbit).m_minute == 33);
    CheckGet(obj, U"%EM", U'M', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"M",   U'M', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"M",   U'M', U'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, U"\n",   U'n',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, U"x",    U'n',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, U"\n",   U'n', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%En",  U'n', U'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, U"n",    U'n', U'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%On",  U'n', U'O', IOv2::ios_defs::eofbit, 3);

    CheckGet(obj, U"\t",   U't',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, U"x",    U't',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, U"\t",   U't', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Et",  U't', U'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, U"n",    U't', U'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Ot",  U't', U'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 PM", U"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 AM", U"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(CheckGet(obj, U"PM", U'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(CheckGet(obj, U"AM", U'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    CheckGet(obj, U"%Ep", U'p', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"p",   U'p', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Op", U'p', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"p",   U'p', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01:33:18 PM", U"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"%Er", U'r', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"r",   U'r', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Or", U'r', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"r",   U'r', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33", U"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, U"%ER", U'R', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"R",   U'R', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OR", U'R', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"R",   U'R', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"18", U'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, U"18", U'S', U'O', IOv2::ios_defs::eofbit).m_second == 18);
    CheckGet(obj, U"%ES", U'S', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"S",   U'S', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"S",   U'S', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33:18 America/Los_Angeles", U"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33:18 America/Los_Angeles", U"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"X",   U'X', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OX", U'X', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"X",   U'X', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33:18", U"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"%ET", U'T', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"T",   U'T', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OT", U'T', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"T",   U'T', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"3", U'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, U"3", U'u', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, U"%Eu", U'u', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"u",   U'u', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"u",   U'u', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"24", U'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, U"%Eg", U'g', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"g",   U'g', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Og", U'g', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"g",   U'g', U'O', IOv2::ios_defs::strfailbit, 0);


    VERIFY(CheckGet(obj, U"2024", U'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, U"%EG", U'G', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"G",   U'G', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OG", U'G', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"G",   U'G', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"2024 35 Wed", U"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"2024 35 Wed", U"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, U"35", U'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(CheckGet(obj, U"35", U'U', U'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    CheckGet(obj, U"%EU", U'U', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"U",   U'U', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"U",   U'U', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"2024 36 Wed", U"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"2024 36 Wed", U"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, U"36", U'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(CheckGet(obj, U"36", U'W', U'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    CheckGet(obj, U"%EW", U'W', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"W",   U'W', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"W",   U'W', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"36", U'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    CheckGet(obj, U"54",  U'V', U'O', IOv2::ios_defs::strfailbit, 1);
    CheckGet(obj, U"36",  U'V', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"%EV", U'V', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"V",   U'V', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"V",   U'V', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"3", U'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, U"3", U'w', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, U"%Ew", U'w', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"w",   U'w', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"w",   U'w', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"24", U'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, U"24", U'y', U'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, U"24", U'y', U'O', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, U"y",  U'y', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"y",  U'y', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"2024", U'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, U"2024", U'Y', U'E', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, U"Y",   U'Y', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OY", U'Y', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"Y",   U'Y', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"America/Los_Angeles", U'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    VERIFY(CheckGet(obj, U"PST", U'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
    CheckGet(obj, U"America/Los_Angexes", U'Z', 0, IOv2::ios_defs::strfailbit);
    CheckGet(obj, U"%EZ", U'Z', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"Z",   U'Z', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OZ", U'Z', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"Z",   U'Z', U'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, U"Z", U'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, U"+13", U'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, U"-1110", U'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, U"+11:10", U'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, U"%Ez", 'z', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"z",  'z', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oz", 'z', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"z",  'z', U'O', IOv2::ios_defs::strfailbit, 0);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(CheckGet<year_month_day>(obj, U"1999-W52-6", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, U"2019-W01-1", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, U"1999-W52-5", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, U"99-W52-6", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, U"19-W01-1", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, U"99-W52-5", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, U"20 24/09/04", U"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);

    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(CheckGet<year_month_day>(obj, U"20 01 01", U"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }
    dump_info("Done\n");
}

void test_timeio_char32_t_get_2()
{
    dump_info("Test timeio<char32_t> get 2...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("zh_CN.UTF-8"));

    CheckGet(obj, U"%",  U'%',  0,  IOv2::ios_defs::eofbit);
    CheckGet(obj, U"x",  U'%',  0,  IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%",  U'%', U'E', febit);
    CheckGet(obj, U"%E%", U'%', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"%",  U'%', U'O', febit);
    CheckGet(obj, U"%O%", U'%', U'O', IOv2::ios_defs::eofbit);

    VERIFY(CheckGet(obj, U"三", U'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, U"%Ea", U'a', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"a",   U'a', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oa", U'a', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"a",   U'a', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"星期三", U'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, U"%EA", U'A', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"A",   U'A', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OA", U'A', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"A",   U'A', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"九月", U'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, U"%Eb", U'b', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"b",   U'b', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Ob", U'b', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"b",   U'b', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"九月", U'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, U"%EB", U'B', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"B",   U'B', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OB", U'B', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"B",   U'B', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"九月", U'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, U"%Eh", U'h', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"h",   U'h', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oh", U'h', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"h",   U'h', U'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(CheckGet<year_month_day>(obj, U"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles", U'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles", U'c', U'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"c",   U'c', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oc", U'c', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"c",   U'c', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"20", U'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(CheckGet(obj, U"20", U'C', U'E', IOv2::ios_defs::eofbit).m_century == 20);
    CheckGet(obj, U"C",   U'C', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OC", U'C', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"C",   U'C', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"04", U'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, U"04", U'd', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, U"%Ed", U'd', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"d",   U'd', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"d",   U'd', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"4", U'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, U"4", U'e', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, U"%Ee", U'e', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"e",   U'e', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"e",   U'e', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"2024-09-04", U'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"%EF", U'F', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"F",   U'F', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OF", U'F', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"F",   U'F', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"2024年09月04日", U'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"2024年09月04日", U'x', U'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"x",   U'x', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Ox", U'x', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"x",   U'x', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"09/04/24", U'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"%ED", U'D', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"D",   U'D', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OD", U'D', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"D",   U'D', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"13", U'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, U"13", U'H', U'O', IOv2::ios_defs::eofbit).m_hour == 13);
    CheckGet(obj, U"%EH", U'H', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"H",   U'H', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"H",   U'H', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"01", U'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, U"01", U'I', U'O', IOv2::ios_defs::eofbit).m_hour == 1);
    CheckGet(obj, U"%EI", U'I', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"I",   U'I', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"I",   U'I', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"248", U'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(CheckGet<year_month_day>(obj, U"2024 248", U"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"%Ej", U'j', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"j",   U'j', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oj", U'j', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"j",   U'j', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"09", U'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, U"09", U'm', U'O', IOv2::ios_defs::eofbit).m_month == 9);
    CheckGet(obj, U"%Em", U'm', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"m",   U'm', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"m",   U'm', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"33", U'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, U"33", U'M', U'O', IOv2::ios_defs::eofbit).m_minute == 33);
    CheckGet(obj, U"%EM", U'M', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"M",   U'M', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"M",   U'M', U'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, U"\n",   U'n',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, U"x",    U'n',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, U"\n",   U'n', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%En",  U'n', U'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, U"n",    U'n', U'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%On",  U'n', U'O', IOv2::ios_defs::eofbit, 3);

    CheckGet(obj, U"\t",   U't',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, U"x",    U't',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, U"\t",   U't', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Et",  U't', U'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, U"n",    U't', U'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Ot",  U't', U'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 下午", U"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 上午", U"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(CheckGet(obj, U"下午", U'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(CheckGet(obj, U"上午", U'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    CheckGet(obj, U"%Ep", U'p', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"p",   U'p', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Op", U'p', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"p",   U'p', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"下午 01时33分18秒", U"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"%Er", U'r', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"r",   U'r', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Or", U'r', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"r",   U'r', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33", U"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, U"%ER", U'R', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"R",   U'R', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OR", U'R', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"R",   U'R', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"18", U'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, U"18", U'S', U'O', IOv2::ios_defs::eofbit).m_second == 18);
    CheckGet(obj, U"%ES", U'S', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"S",   U'S', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"S",   U'S', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13时33分18秒 America/Los_Angeles", U"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13时33分18秒 America/Los_Angeles", U"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"X",   U'X', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OX", U'X', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"X",   U'X', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33:18", U"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"%ET", U'T', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"T",   U'T', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OT", U'T', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"T",   U'T', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"3", U'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, U"3", U'u', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, U"%Eu", U'u', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"u",   U'u', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"u",   U'u', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"24", U'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, U"%Eg", U'g', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"g",   U'g', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Og", U'g', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"g",   U'g', U'O', IOv2::ios_defs::strfailbit, 0);


    VERIFY(CheckGet(obj, U"2024", U'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, U"%EG", U'G', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"G",   U'G', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OG", U'G', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"G",   U'G', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"2024 35 三", U"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"2024 35 三", U"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, U"35", U'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(CheckGet(obj, U"35", U'U', U'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    CheckGet(obj, U"%EU", U'U', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"U",   U'U', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"U",   U'U', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"2024 36 三", U"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"2024 36 三", U"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, U"36", U'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(CheckGet(obj, U"36", U'W', U'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    CheckGet(obj, U"%EW", U'W', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"W",   U'W', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"W",   U'W', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"36", U'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    CheckGet(obj, U"54",  U'V', U'O', IOv2::ios_defs::strfailbit, 1);
    CheckGet(obj, U"36",  U'V', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"%EV", U'V', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"V",   U'V', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"V",   U'V', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"3", U'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, U"3", U'w', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, U"%Ew", U'w', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"w",   U'w', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"w",   U'w', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"24", U'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, U"24", U'y', U'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, U"24", U'y', U'O', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, U"y",  U'y', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"y",  U'y', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"2024", U'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, U"2024", U'Y', U'E', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, U"Y",   U'Y', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OY", U'Y', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"Y",   U'Y', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"America/Los_Angeles", U'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    VERIFY(CheckGet(obj, U"PST", U'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
    CheckGet(obj, U"America/Los_Angexes", U'Z', 0, IOv2::ios_defs::strfailbit);
    CheckGet(obj, U"%EZ", U'Z', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"Z",   U'Z', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OZ", U'Z', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"Z",   U'Z', U'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, U"Z", U'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, U"+13", U'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, U"-1110", U'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, U"+11:10", U'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, U"%Ez", U'z', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"z",  U'z', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oz", U'z', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"z",  U'z', U'O', IOv2::ios_defs::strfailbit, 0);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(CheckGet<year_month_day>(obj, U"1999-W52-6", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, U"2019-W01-1", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, U"1999-W52-5", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, U"99-W52-6", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, U"19-W01-1", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, U"99-W52-5", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, U"20 24/09/04", U"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(CheckGet<year_month_day>(obj, U"20 01 01", U"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_get_3()
{
    dump_info("Test timeio<char32_t> get 3...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("ja_JP.UTF-8"));

    CheckGet(obj, U"%",  U'%',  0,  IOv2::ios_defs::eofbit);
    CheckGet(obj, U"x",  U'%',  0,  IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%",  U'%', U'E', febit);
    CheckGet(obj, U"%E%", U'%', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"%",  U'%', U'O', febit);
    CheckGet(obj, U"%O%", U'%', U'O', IOv2::ios_defs::eofbit);

    VERIFY(CheckGet(obj, U"水", U'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, U"%Ea", U'a', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"a",   U'a', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oa", U'a', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"a",   U'a', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"水曜日", U'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, U"%EA", U'A', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"A",   U'A', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OA", U'A', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"A",   U'A', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"9月", U'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, U"%Eb", U'b', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"b",   U'b', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Ob", U'b', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"b",   U'b', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"9月", U'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, U"%EB", U'B', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"B",   U'B', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OB", U'B', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"B",   U'B', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"9月", U'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, U"%Eh", U'h', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"h",   U'h', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oh", U'h', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"h",   U'h', U'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(CheckGet<year_month_day>(obj, U"2024年09月04日 13時33分18秒 America/Los_Angeles", U'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"令和6年09月04日 13時33分18秒 America/Los_Angeles", U'c', U'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"202409月04日 13時33分18秒 America/Los_Angeles", U'c', U'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"c",   U'c', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oc", U'c', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"c",   U'c', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"20", 'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(CheckGet<year_month_day>(obj, U"平成", U'C', U'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1990));
    CheckGet(obj, U"C",   U'C', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OC", U'C', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"C",   U'C', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"04", U'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, U"04", U'd', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, U"四", U'd', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, U"%Ed", U'd', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"d",   U'd', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"d",   U'd', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"4", U'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, U"4", U'e', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, U"四", U'e', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, U"%Ee", U'e', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"e",   U'e', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"e",   U'e', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"2024-09-04", U'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"%EF", U'F', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"F",   U'F', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OF", U'F', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"F",   U'F', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"2024年09月04日", U'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"令和6年09月04日", U'x', U'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"202409月04日", U'x', U'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"x",   U'x', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Ox", U'x', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"x",   U'x', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"09/04/24", U'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"%ED", U'D', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"D",   U'D', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OD", U'D', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"D",   U'D', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"13", U'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, U"13", U'H', U'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, U"十三", U'H', U'O', IOv2::ios_defs::eofbit).m_hour == 13);
    CheckGet(obj, U"%EH", U'H', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"H",   U'H', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"H",   U'H', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"01", U'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, U"01", U'I', U'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, U"一", U'I', U'O', IOv2::ios_defs::eofbit).m_hour == 1);
    CheckGet(obj, U"%EI", U'I', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"I",   U'I', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"I",   U'I', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"248", U'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(CheckGet<year_month_day>(obj, U"2024 248", U"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, U"%Ej", U'j', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"j",   U'j', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oj", U'j', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"j",   U'j', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"09", U'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, U"09", U'm', U'O', IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, U"九", U'm', U'O', IOv2::ios_defs::eofbit).m_month == 9);
    CheckGet(obj, U"%Em", U'm', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"m",   U'm', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"m",   U'm', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"33", U'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, U"33", U'M', U'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, U"三十三", U'M', U'O', IOv2::ios_defs::eofbit).m_minute == 33);
    CheckGet(obj, U"%EM", U'M', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"M",   U'M', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"M",   U'M', U'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, U"\n",   U'n',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, U"x",    U'n',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, U"\n",   U'n', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%En",  U'n', U'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, U"n",    U'n', U'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%On",  U'n', U'O', IOv2::ios_defs::eofbit, 3);

    CheckGet(obj, U"\t",   U't',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, U"x",    U't',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, U"\t",   U't', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Et",  U't', U'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, U"n",    U't', U'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Ot",  U't', U'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 午後", U"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 午前", U"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(CheckGet(obj, U"午後", U'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(CheckGet(obj, U"午前", U'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    CheckGet(obj, U"%Ep", U'p', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"p",   U'p', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Op", U'p', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"p",   U'p', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"午後01時33分18秒", U"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"%Er", U'r', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"r",   U'r', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Or", U'r', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"r",   U'r', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33", U"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, U"%ER", U'R', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"R",   U'R', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OR", U'R', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"R",   U'R', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"18", U'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, U"18", U'S', U'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, U"十八", U'S', U'O', IOv2::ios_defs::eofbit).m_second == 18);
    CheckGet(obj, U"%ES", U'S', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"S",   U'S', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"S",   U'S', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13時33分18秒 America/Los_Angeles", U"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13時33分18秒 America/Los_Angeles", U"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"X",   U'X', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OX", U'X', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"X",   U'X', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33:18", U"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, U"%ET", U'T', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"T",   U'T', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OT", U'T', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"T",   U'T', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"3", U'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, U"3", U'u', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, U"三", U'u', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, U"%Eu", U'u', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"u",   U'u', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"u",   U'u', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"24", U'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, U"%Eg", U'g', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"g",   U'g', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Og", U'g', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"g",   U'g', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"2024", U'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, U"%EG", U'G', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"G",   U'G', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OG", U'G', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"G",   U'G', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"2024 35 水", U"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"2024 35 水", U"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"2024 三十五 水", U"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, U"35", U'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(CheckGet(obj, U"35", U'U', U'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    CheckGet(obj, U"%EU", U'U', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"U",   U'U', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"U",   U'U', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, U"2024 36 水", U"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"2024 36 水", U"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, U"2024 三十六 水", U"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, U"36", U'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(CheckGet(obj, U"36", U'W', U'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    CheckGet(obj, U"%EW", U'W', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"W",   U'W', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"W",   U'W', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"36", U'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(CheckGet(obj, U"36", U'V', U'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(CheckGet(obj, U"三十六", 'V', U'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    CheckGet(obj, U"54",  U'V', U'O', IOv2::ios_defs::strfailbit, 1);
    CheckGet(obj, U"%EV", U'V', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"V",   U'V', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"V",   U'V', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"3", U'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, U"3", U'w', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, U"三", U'w', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, U"%Ew", U'w', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"w",   U'w', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"w",   U'w', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"24", U'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet<year_month_day>(obj, U"6", U'y', U'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(2024));
    VERIFY(CheckGet(obj, U"24", U'y', U'O', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, U"二十四", U'y', U'O', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, U"y",  U'y', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"y",  U'y', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"2024", U'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, U"2024", U'Y', U'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet<year_month_day>(obj, U"平成3年", U'Y', U'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1991));
    CheckGet(obj, U"Y",   U'Y', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OY", U'Y', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"Y",   U'Y', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, U"America/Los_Angeles", U'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    VERIFY(CheckGet(obj, U"PST", U'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
    CheckGet(obj, U"America/Los_Angexes", U'Z', 0, IOv2::ios_defs::strfailbit);
    CheckGet(obj, U"%EZ", U'Z', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"Z",   U'Z', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%OZ", U'Z', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"Z",   U'Z', U'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, U"Z", U'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, U"+13", U'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, U"-1110", U'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, U"+11:10", U'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, U"%Ez", U'z', U'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"z",  U'z', U'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, U"%Oz", U'z', U'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, U"z",  U'z', U'O', IOv2::ios_defs::strfailbit, 0);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(CheckGet<year_month_day>(obj, U"1999-W52-6", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, U"2019-W01-1", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, U"1999-W52-5", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, U"99-W52-6", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, U"19-W01-1", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, U"99-W52-5", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, U"20 24/09/04", U"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(CheckGet<year_month_day>(obj, U"20 01 01", U"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_get_4()
{
    dump_info("Test timeio<char32_t> get 4...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("C"));
    {
        std::u32string input = U"d 2014-04-14 01:09:35";
        std::u32string format = U"d %Y-%m-%d %H:%M:%S";
        
        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 114);
        VERIFY(time.tm_mon == 3);
        VERIFY(time.tm_mday == 14);
        VERIFY(time.tm_hour == 1);
        VERIFY(time.tm_min == 9);
        VERIFY(time.tm_sec == 35);
    }

    {
        std::u32string input = U"2020  ";
        std::u32string format = U"%Y";
        
        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret != input.end());
        VERIFY(time.tm_year == 120);
    }

    {
        std::u32string input = U"2014-04-14 01:09:35";
        std::u32string format = U"%";
        
        IOv2::time_parse_context<char32_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::u32string input = U"2020";
        
        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, 'Y');
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_year == 120);
        VERIFY(ret == input.end());
    }

    {
        std::u32string input = U"year: 1970";
        std::u32string format = U"jahr: %Y";
        
        IOv2::time_parse_context<char32_t> ctx;
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

void test_timeio_char32_t_get_5()
{
    dump_info("Test timeio<char32_t> get 5...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("de_DE.UTF-8"));
    {
        std::u32string input = U"Montag, den 14. April 2014";
        std::u32string format = U"%A, den %d. %B %Y";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 114);
        VERIFY(time.tm_mon == 3);
        VERIFY(time.tm_wday == 1);
        VERIFY(time.tm_mday == 14);
    }
    {
        std::u32string input = U"Mittwoch";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, 'A');
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 3);
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_get_6()
{
    dump_info("Test timeio<char32_t> get 6...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("C"));
    {
        std::u32string input = U"Mon";
        std::u32string format = U"%a";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 1);
    }

    {
        std::u32string input = U"Tue ";
        std::u32string format = U"%a";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_wday == 2);
    }

    {
        std::u32string input = U"Wednesday";
        std::u32string format = U"%a";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 3);
    }

    {
        std::u32string input = U"Thu";
        std::u32string format = U"%A";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 4);
    }

    {
        std::u32string input = U"Fri ";
        std::u32string format = U"%A";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_wday == 5);
    }

    {
        std::u32string input = U"Saturday";
        std::u32string format = U"%A";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 6);
    }

    {
        std::u32string input = U"Feb";
        std::u32string format = U"%b";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 1);
    }

    {
        std::u32string input = U"Mar ";
        std::u32string format = U"%b";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_mon == 2);
    }

    {
        std::u32string input = U"April";
        std::u32string format = U"%b";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 3);
    }

    {
        std::u32string input = U"May";
        std::u32string format = U"%B";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 4);
    }

    {
        std::u32string input = U"Jun ";
        std::u32string format = U"%B";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_mon == 5);
    }

    {
        std::u32string input = U"July";
        std::u32string format = U"%B";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 6);
    }

    {
        std::u32string input = U"Aug";
        std::u32string format = U"%h";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 7);
    }

    {
        std::u32string input = U"May ";
        std::u32string format = U"%h";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_mon == 4);
    }

    {
        std::u32string input = U"October";
        std::u32string format = U"%h";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 9);
    }

    // Other tests.
    {
        std::u32string input = U"2.";
        std::u32string format = U"%d.";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mday == 2);
    }

    {
        std::u32string input = U"0.";
        std::u32string format = U"%d.";

        IOv2::time_parse_context<char32_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::u32string input = U"32.";
        std::u32string format = U"%d.";

        IOv2::time_parse_context<char32_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::u32string input = U"5.";
        std::u32string format = U"%e.";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_mday == 5);
    }

    {
        std::u32string input = U"06.";
        std::u32string format = U"%e.";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_mday == 6);
    }

    {
        std::u32string input = U"0";
        std::u32string format = U"%e";

        IOv2::time_parse_context<char32_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::u32string input = U"35";
        std::u32string format = U"%e";

        IOv2::time_parse_context<char32_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::u32string input = U"12:00AM";
        std::u32string format = U"%I:%M%p";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 0);
        VERIFY(time.tm_min == 0);
    }

    {
        std::u32string input = U"12:37AM";
        std::u32string format = U"%I:%M%p";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 0);
        VERIFY(time.tm_min == 37);
    }

    {
        std::u32string input = U"01:25AM";
        std::u32string format = U"%I:%M%p";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 1);
        VERIFY(time.tm_min == 25);
    }

    {
        std::u32string input = U"12:00PM";
        std::u32string format = U"%I:%M%p";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 12);
        VERIFY(time.tm_min == 0);
    }

    {
        std::u32string input = U"12:42PM";
        std::u32string format = U"%I:%M%p";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 12);
        VERIFY(time.tm_min == 42);
    }

    {
        std::u32string input = U"07:23PM";
        std::u32string format = U"%I:%M%p";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 19);
        VERIFY(time.tm_min == 23);
    }

    {
        std::u32string input = U"17%20";
        std::u32string format = U"%H%%%M";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 17);
        VERIFY(time.tm_min == 20);
    }

    {
        std::u32string input = U"24:30";
        std::u32string format = U"%H:%M";

        IOv2::time_parse_context<char32_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::u32string input = U"Novembur";
        std::u32string format = U"%bembur";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_mon == 10);
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_get_7()
{
    dump_info("Test timeio<char32_t> get 7...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("C"));
    {
        std::u32string input = U"PM01:38:12";
        std::u32string format = U"%p%I:%M:%S";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_hour == 13);
        VERIFY(time.tm_min == 38);
        VERIFY(time.tm_sec == 12);
    }

    {
        std::u32string input = U"05 37";
        std::u32string format = U"%C %y";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 537 - 1900);
    }

    {
        std::u32string input = U"68";
        std::u32string format = U"%y";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2068 - 1900);
    }

    {
        std::u32string input = U"69";
        std::u32string format = U"%y";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 1969 - 1900);
    }

    {
        std::u32string input = U"03-Feb-2003";
        std::u32string format = U"%d-%b-%Y";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2003 - 1900);
        VERIFY(time.tm_mon == 1);
        VERIFY(time.tm_mday == 3);
        VERIFY(time.tm_wday == 1);
        VERIFY(time.tm_yday == 33);
    }

    {
        std::u32string input = U"16-Dec-2020";
        std::u32string format = U"%d-%b-%Y";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2020 - 1900);
        VERIFY(time.tm_mon == 11);
        VERIFY(time.tm_mday == 16);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 350);
    }

    {
        std::u32string input = U"16-Dec-2021";
        std::u32string format = U"%d-%b-%Y";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2021 - 1900);
        VERIFY(time.tm_mon == 11);
        VERIFY(time.tm_mday == 16);
        VERIFY(time.tm_wday == 4);
        VERIFY(time.tm_yday == 349);
    }

    {
        std::u32string input = U"253 2020";
        std::u32string format = U"%j %Y";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2020 - 1900);
        VERIFY(time.tm_mon == 8);
        VERIFY(time.tm_mday == 9);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 252);
    }

    {
        std::u32string input = U"233 2021";
        std::u32string format = U"%j %Y";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2021 - 1900);
        VERIFY(time.tm_mon == 7);
        VERIFY(time.tm_mday == 21);
        VERIFY(time.tm_wday == 6);
        VERIFY(time.tm_yday == 232);
    }

    {
        std::u32string input = U"2020 23 3";
        std::u32string format = U"%Y %U %w";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2020 - 1900);
        VERIFY(time.tm_mon == 5);
        VERIFY(time.tm_mday == 10);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 161);
    }

    {
        std::u32string input = U"2020 23 3";
        std::u32string format = U"%Y %W %w";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2020 - 1900);
        VERIFY(time.tm_mon == 5);
        VERIFY(time.tm_mday == 10);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 161);
    }

    {
        std::u32string input = U"2021 43 Fri";
        std::u32string format = U"%Y %W %a";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2021 - 1900);
        VERIFY(time.tm_mon == 9);
        VERIFY(time.tm_mday == 29);
        VERIFY(time.tm_wday == 5);
        VERIFY(time.tm_yday == 301);
    }

    {
        std::u32string input = U"2024 23 3";
        std::u32string format = U"%Y %U %w";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2024 - 1900);
        VERIFY(time.tm_mon == 5);
        VERIFY(time.tm_mday == 12);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 163);
    }

    {
        std::u32string input = U"2024 23 3";
        std::u32string format = U"%Y %W %w";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2024 - 1900);
        VERIFY(time.tm_mon == 5);
        VERIFY(time.tm_mday == 5);
        VERIFY(time.tm_wday == 3);
        VERIFY(time.tm_yday == 156);
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_get_8()
{
    dump_info("Test timeio<char32_t> get 8...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("C"));
    {
        std::u32string input = U"01:38:12 PM";
        std::u32string format = U"%I:%M:%S %p";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_hour == 13);
        VERIFY(time.tm_min == 38);
        VERIFY(time.tm_sec == 12);
    }
        
    {
        std::u32string input = U"11:17:42 PM";
        std::u32string format = U"%r";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_hour == 23);
        VERIFY(time.tm_min == 17);
        VERIFY(time.tm_sec == 42);
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_get_9()
{
    dump_info("Test timeio<char32_t> get 9...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char32_t, true, true, false>, true, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(U"%",  U'%',  0,  IOv2::ios_defs::eofbit);
    FOri(U"x",  U'%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri(U"%",  U'%', U'E', febit);
    FOri(U"%E%", U'%', U'E', IOv2::ios_defs::eofbit);
    FOri(U"%",  U'%', U'O', febit);
    FOri(U"%O%", U'%', U'O', IOv2::ios_defs::eofbit);

    VERIFY(FOri(U"水", U'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri(U"%Ea", U'a', U'E', IOv2::ios_defs::eofbit);
    FOri(U"a",   U'a', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oa", U'a', U'O', IOv2::ios_defs::eofbit);
    FOri(U"a",   U'a', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"水曜日", U'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri(U"%EA", U'A', U'E', IOv2::ios_defs::eofbit);
    FOri(U"A",   U'A', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OA", U'A', U'O', IOv2::ios_defs::eofbit);
    FOri(U"A",   U'A', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"9月", U'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(U"%Eb", U'b', U'E', IOv2::ios_defs::eofbit);
    FOri(U"b",   U'b', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Ob", U'b', U'O', IOv2::ios_defs::eofbit);
    FOri(U"b",   U'b', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"9月", U'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(U"%EB", U'B', U'E', IOv2::ios_defs::eofbit);
    FOri(U"B",   U'B', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OB", U'B', U'O', IOv2::ios_defs::eofbit);
    FOri(U"B",   U'B', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"9月", U'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(U"%Eh", U'h', U'E', IOv2::ios_defs::eofbit);
    FOri(U"h",   U'h', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oh", U'h', U'O', IOv2::ios_defs::eofbit);
    FOri(U"h",   U'h', U'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(FYmd(U"2024年09月04日 13時33分18秒", U'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"令和6年09月04日 13時33分18秒", U'c', U'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"202409月04日 13時33分18秒", U'c', U'E', IOv2::ios_defs::eofbit) == check_date1);
    FOri(U"c",   U'c', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oc", U'c', U'O', IOv2::ios_defs::eofbit);
    FOri(U"c",   U'c', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"20", U'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(FYmd(U"平成", U'C', U'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1990));
    FOri(U"C",   U'C', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OC", U'C', U'O', IOv2::ios_defs::eofbit);
    FOri(U"C",   U'C', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"04", U'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(U"04", U'd', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(U"四", U'd', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri(U"%Ed", U'd', U'E', IOv2::ios_defs::eofbit);
    FOri(U"d",   U'd', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"d",   U'd', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"4", U'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(U"4", U'e', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(U"四", U'e', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri(U"%Ee", U'e', U'E', IOv2::ios_defs::eofbit);
    FOri(U"e",   U'e', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"e",   U'e', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(U"2024-09-04", U'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri(U"%EF", U'F', U'E', IOv2::ios_defs::eofbit);
    FOri(U"F",   U'F', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OF", U'F', U'O', IOv2::ios_defs::eofbit);
    FOri(U"F",   U'F', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(U"2024年09月04日", U'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"令和6年09月04日", U'x', U'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"202409月04日", U'x', U'E', IOv2::ios_defs::eofbit) == check_date1);
    FOri(U"x",   U'x', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Ox", U'x', U'O', IOv2::ios_defs::eofbit);
    FOri(U"x",   U'x', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(U"09/04/24", U'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri(U"%ED", U'D', U'E', IOv2::ios_defs::eofbit);
    FOri(U"D",   U'D', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OD", U'D', U'O', IOv2::ios_defs::eofbit);
    FOri(U"D",   U'D', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"13", U'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(U"13", U'H', U'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(U"十三", U'H', U'O', IOv2::ios_defs::eofbit).m_hour == 13);
    FOri(U"%EH", U'H', U'E', IOv2::ios_defs::eofbit);
    FOri(U"H",   U'H', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"H",   U'H', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"01", U'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(U"01", U'I', U'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(U"一", U'I', U'O', IOv2::ios_defs::eofbit).m_hour == 1);
    FOri(U"%EI", U'I', U'E', IOv2::ios_defs::eofbit);
    FOri(U"I",   U'I', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"I",   U'I', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"248", U'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(FYmd(U"2024 248", U"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    FOri(U"%Ej", U'j', U'E', IOv2::ios_defs::eofbit);
    FOri(U"j",   U'j', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oj", U'j', U'O', IOv2::ios_defs::eofbit);
    FOri(U"j",   U'j', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"09", U'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri(U"09", U'm', U'O', IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri(U"九", U'm', U'O', IOv2::ios_defs::eofbit).m_month == 9);
    FOri(U"%Em", U'm', U'E', IOv2::ios_defs::eofbit);
    FOri(U"m",   U'm', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"m",   U'm', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"33", U'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(U"33", U'M', U'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(U"三十三", U'M', U'O', IOv2::ios_defs::eofbit).m_minute == 33);
    FOri(U"%EM", U'M', U'E', IOv2::ios_defs::eofbit);
    FOri(U"M",   U'M', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"M",   U'M', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"\n",   U'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(U"x",    U'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(U"\n",   U'n', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%En",  U'n', U'E', IOv2::ios_defs::eofbit, 3);
    FOri(U"n",    U'n', U'O', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%On",  U'n', U'O', IOv2::ios_defs::eofbit, 3);

    FOri(U"\t",   U't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(U"x",    U't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(U"\t",   U't', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Et",  U't', U'E', IOv2::ios_defs::eofbit, 3);
    FOri(U"n",    U't', U'O', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Ot",  U't', U'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 午後", U"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"01 午前", U"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(FOri(U"午後", U'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(FOri(U"午前", U'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    FOri(U"%Ep", U'p', U'E', IOv2::ios_defs::eofbit);
    FOri(U"p",   U'p', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Op", U'p', U'O', IOv2::ios_defs::eofbit);
    FOri(U"p",   U'p', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"午後01時33分18秒", U"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"%Er", U'r', U'E', IOv2::ios_defs::eofbit);
    FOri(U"r",   U'r', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Or", U'r', U'O', IOv2::ios_defs::eofbit);
    FOri(U"r",   U'r', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33", U"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(U"%ER", U'R', U'E', IOv2::ios_defs::eofbit);
    FOri(U"R",   U'R', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OR", U'R', U'O', IOv2::ios_defs::eofbit);
    FOri(U"R",   U'R', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"18", U'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(U"18", U'S', U'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(U"十八", U'S', U'O', IOv2::ios_defs::eofbit).m_second == 18);
    FOri(U"%ES", U'S', U'E', IOv2::ios_defs::eofbit);
    FOri(U"S",   U'S', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"S",   U'S', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13時33分18秒 America/Los_Angeles", U"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13時33分18秒 America/Los_Angeles", U"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"X",   U'X', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OX", U'X', U'O', IOv2::ios_defs::eofbit);
    FOri(U"X",   U'X', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33:18", U"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"%ET", U'T', U'E', IOv2::ios_defs::eofbit);
    FOri(U"T",   U'T', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OT", U'T', U'O', IOv2::ios_defs::eofbit);
    FOri(U"T",   U'T', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"3", U'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(U"3", U'u', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(U"三", U'u', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri(U"%Eu", U'u', U'E', IOv2::ios_defs::eofbit);
    FOri(U"u",   U'u', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"u",   U'u', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"24", U'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri(U"%Eg", U'g', U'E', IOv2::ios_defs::eofbit);
    FOri(U"g",   U'g', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Og", U'g', U'O', IOv2::ios_defs::eofbit);
    FOri(U"g",   U'g', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"2024", U'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri(U"%EG", U'G', U'E', IOv2::ios_defs::eofbit);
    FOri(U"G",   U'G', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OG", U'G', U'O', IOv2::ios_defs::eofbit);
    FOri(U"G",   U'G', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(U"2024 35 水", U"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"2024 35 水", U"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"2024 三十五 水", U"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri(U"35", U'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(FOri(U"35", U'U', U'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    FOri(U"%EU", U'U', U'E', IOv2::ios_defs::eofbit);
    FOri(U"U",   U'U', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"U",   U'U', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(U"2024 36 水", U"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"2024 36 水", U"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"2024 三十六 水", U"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri(U"36", U'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(FOri(U"36", U'W', U'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    FOri(U"%EW", U'W', U'E', IOv2::ios_defs::eofbit);
    FOri(U"W",   U'W', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"W",   U'W', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"36", U'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri(U"36", U'V', U'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri(U"三十六", U'V', U'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    FOri(U"54",  U'V', U'O', IOv2::ios_defs::strfailbit, 1);
    FOri(U"%EV", U'V', U'E', IOv2::ios_defs::eofbit);
    FOri(U"V",   U'V', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"V",   U'V', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"3", U'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(U"3", U'w', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(U"三", U'w', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri(U"%Ew", U'w', U'E', IOv2::ios_defs::eofbit);
    FOri(U"w",   U'w', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"w",   U'w', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"24", U'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd(U"6", U'y', U'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(2024));
    VERIFY(FOri(U"24", U'y', U'O', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri(U"二十四", U'y', U'O', IOv2::ios_defs::eofbit).m_year == 2024);
    FOri(U"y",  U'y', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"y",  U'y', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"2024", U'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri(U"2024", U'Y', U'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd(U"平成3年", U'Y', U'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1991));
    FOri(U"Y",   U'Y', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OY", U'Y', U'O', IOv2::ios_defs::eofbit);
    FOri(U"Y",   U'Y', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%Z", U'Z', 0, IOv2::ios_defs::eofbit);
    FOri(U"%EZ", U'Z', U'E', IOv2::ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OZ", U'Z', U'O', IOv2::ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%z", U'z', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Ez", U'z', U'E', IOv2::ios_defs::eofbit);
    FOri(U"%Oz", U'z', U'O', IOv2::ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(FYmd(U"1999-W52-6", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd(U"2019-W01-1", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd(U"1999-W52-5", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd(U"99-W52-6", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd(U"19-W01-1", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd(U"99-W52-5", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd(U"20 24/09/04", U"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(FYmd(U"20 01 01", U"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_get_10()
{
    dump_info("Test timeio<char32_t> get 10...");
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char32_t, true, false, false>, true, false, false>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, false, false>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(U"%",  U'%',  0,  IOv2::ios_defs::eofbit);
    FOri(U"x",  U'%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri(U"%",  U'%', U'E', febit);
    FOri(U"%E%", U'%', U'E', IOv2::ios_defs::eofbit);
    FOri(U"%",  U'%', U'O', febit);
    FOri(U"%O%", U'%', U'O', IOv2::ios_defs::eofbit);

    VERIFY(FOri(U"水", U'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri(U"%Ea", U'a', U'E', IOv2::ios_defs::eofbit);
    FOri(U"a",   U'a', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oa", U'a', U'O', IOv2::ios_defs::eofbit);
    FOri(U"a",   U'a', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"水曜日", U'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri(U"%EA", U'A', U'E', IOv2::ios_defs::eofbit);
    FOri(U"A",   U'A', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OA", U'A', U'O', IOv2::ios_defs::eofbit);
    FOri(U"A",   U'A', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"9月", U'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(U"%Eb", U'b', U'E', IOv2::ios_defs::eofbit);
    FOri(U"b",   U'b', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Ob", U'b', U'O', IOv2::ios_defs::eofbit);
    FOri(U"b",   U'b', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"9月", U'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(U"%EB", U'B', U'E', IOv2::ios_defs::eofbit);
    FOri(U"B",   U'B', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OB", U'B', U'O', IOv2::ios_defs::eofbit);
    FOri(U"B",   U'B', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"9月", U'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(U"%Eh", U'h', U'E', IOv2::ios_defs::eofbit);
    FOri(U"h",   U'h', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oh", U'h', U'O', IOv2::ios_defs::eofbit);
    FOri(U"h",   U'h', U'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    FYmd(U"%c", U'c', 0, IOv2::ios_defs::eofbit);
    FYmd(U"%Ec", U'c', U'E', IOv2::ios_defs::eofbit);
    FOri(U"c",   U'c', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oc", U'c', U'O', IOv2::ios_defs::eofbit);
    FOri(U"c",   U'c', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"20", U'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(FYmd(U"平成", U'C', U'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1990));
    FOri(U"C",   U'C', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OC", U'C', U'O', IOv2::ios_defs::eofbit);
    FOri(U"C",   U'C', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"04", U'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(U"04", U'd', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(U"四", U'd', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri(U"%Ed", U'd', U'E', IOv2::ios_defs::eofbit);
    FOri(U"d",   U'd', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"d",   U'd', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"4", U'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(U"4", U'e', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(U"四", U'e', U'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri(U"%Ee", U'e', U'E', IOv2::ios_defs::eofbit);
    FOri(U"e",   U'e', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"e",   U'e', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(U"2024-09-04", U'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri(U"%EF", 'F', U'E', IOv2::ios_defs::eofbit);
    FOri(U"F",   'F', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OF", 'F', U'O', IOv2::ios_defs::eofbit);
    FOri(U"F",   'F', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(U"2024年09月04日", U'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"令和6年09月04日", U'x', U'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"202409月04日", U'x', U'E', IOv2::ios_defs::eofbit) == check_date1);
    FOri(U"x",   U'x', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Ox", U'x', U'O', IOv2::ios_defs::eofbit);
    FOri(U"x",   U'x', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(U"09/04/24", U'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri(U"%ED", U'D', U'E', IOv2::ios_defs::eofbit);
    FOri(U"D",   U'D', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OD", U'D', U'O', IOv2::ios_defs::eofbit);
    FOri(U"D",   U'D', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%H", U'H', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%EH", U'H', U'E', IOv2::ios_defs::eofbit);
    FOri(U"H",   U'H', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"H",   U'H', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%I", U'I', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%EI", U'I', U'E', IOv2::ios_defs::eofbit);
    FOri(U"I",   U'I', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"I",   U'I', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"248", U'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(FYmd(U"2024 248", U"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    FOri(U"%Ej", U'j', U'E', IOv2::ios_defs::eofbit);
    FOri(U"j",   U'j', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oj", U'j', U'O', IOv2::ios_defs::eofbit);
    FOri(U"j",   U'j', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"09", U'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri(U"09", U'm', U'O', IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri(U"九", U'm', U'O', IOv2::ios_defs::eofbit).m_month == 9);
    FOri(U"%Em", U'm', U'E', IOv2::ios_defs::eofbit);
    FOri(U"m",   U'm', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"m",   U'm', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%M", U'M', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%OM", U'M', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%EM", U'M', U'E', IOv2::ios_defs::eofbit);
    FOri(U"M",   U'M', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"M",   U'M', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"\n",   U'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(U"x",    U'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(U"\n",   U'n', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%En",  U'n', U'E', IOv2::ios_defs::eofbit, 3);
    FOri(U"n",    U'n', U'O', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%On",  U'n', U'O', IOv2::ios_defs::eofbit, 3);

    FOri(U"\t",   U't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(U"x",    U't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(U"\t",   U't', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Et",  U't', U'E', IOv2::ios_defs::eofbit, 3);
    FOri(U"n",    U't', U'O', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Ot",  U't', U'O', IOv2::ios_defs::eofbit, 3);

    FOri(U"%p", U'p', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Ep", U'p', U'E', IOv2::ios_defs::eofbit);
    FOri(U"p",   U'p', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Op", U'p', U'O', IOv2::ios_defs::eofbit);
    FOri(U"p",   U'p', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%r", U"%r",  IOv2::ios_defs::eofbit);
    FOri(U"%Er", U'r', U'E', IOv2::ios_defs::eofbit);
    FOri(U"r",   U'r', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Or", U'r', U'O', IOv2::ios_defs::eofbit);
    FOri(U"r",   U'r', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, U"13:33", U"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(U"%ER", U'R', U'E', IOv2::ios_defs::eofbit);
    FOri(U"R",   U'R', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OR", U'R', U'O', IOv2::ios_defs::eofbit);
    FOri(U"R",   U'R', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%S", U'S', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%OS", U'S', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%ES", U'S', U'E', IOv2::ios_defs::eofbit);
    FOri(U"S",   U'S', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"S",   U'S', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%X", U"%X",  IOv2::ios_defs::eofbit);
    FOri(U"%EX", U"%EX",  IOv2::ios_defs::eofbit);
    FOri(U"X",   U'X', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OX", U'X', U'O', IOv2::ios_defs::eofbit);
    FOri(U"X",   U'X', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%T", U"%T",  IOv2::ios_defs::eofbit);
    FOri(U"%ET", U'T', U'E', IOv2::ios_defs::eofbit);
    FOri(U"T",   U'T', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OT", U'T', U'O', IOv2::ios_defs::eofbit);
    FOri(U"T",   U'T', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"3", U'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(U"3", U'u', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(U"三", U'u', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri(U"%Eu", U'u', U'E', IOv2::ios_defs::eofbit);
    FOri(U"u",   U'u', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"u",   U'u', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"24", U'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri(U"%Eg", U'g', U'E', IOv2::ios_defs::eofbit);
    FOri(U"g",   U'g', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Og", U'g', U'O', IOv2::ios_defs::eofbit);
    FOri(U"g",   U'g', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"2024", U'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri(U"%EG", U'G', U'E', IOv2::ios_defs::eofbit);
    FOri(U"G",   U'G', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OG", U'G', U'O', IOv2::ios_defs::eofbit);
    FOri(U"G",   U'G', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(U"2024 35 水", U"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"2024 35 水", U"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"2024 三十五 水", U"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri(U"35", U'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(FOri(U"35", U'U', U'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    FOri(U"%EU", U'U', U'E', IOv2::ios_defs::eofbit);
    FOri(U"U",   U'U', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"U",   U'U', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(U"2024 36 水", U"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"2024 36 水", U"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(U"2024 三十六 水", U"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri(U"36", U'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(FOri(U"36", U'W', U'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    FOri(U"%EW", U'W', U'E', IOv2::ios_defs::eofbit);
    FOri(U"W",   U'W', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"W",   U'W', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"36", U'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri(U"36", U'V', U'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri(U"三十六", U'V', U'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    FOri(U"54",  U'V', U'O', IOv2::ios_defs::strfailbit, 1);
    FOri(U"%EV", U'V', U'E', IOv2::ios_defs::eofbit);
    FOri(U"V",   U'V', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"V",   U'V', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"3", U'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(U"3", U'w', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(U"三", U'w', U'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri(U"%Ew", U'w', U'E', IOv2::ios_defs::eofbit);
    FOri(U"w",   U'w', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"w",   U'w', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"24", U'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd(U"6", U'y', U'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(2024));
    VERIFY(FOri(U"24", U'y', U'O', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri(U"二十四", U'y', U'O', IOv2::ios_defs::eofbit).m_year == 2024);
    FOri(U"y",  U'y', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"y",  U'y', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"2024", U'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri(U"2024", U'Y', U'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd(U"平成3年", U'Y', U'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1991));
    FOri(U"Y",   U'Y', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OY", U'Y', U'O', IOv2::ios_defs::eofbit);
    FOri(U"Y",   U'Y', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%Z", U'Z', 0, IOv2::ios_defs::eofbit);
    FOri(U"%EZ", U'Z', U'E', IOv2::ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OZ", U'Z', U'O', IOv2::ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%z", U'z', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Ez", U'z', U'E', IOv2::ios_defs::eofbit);
    FOri(U"%Oz", U'z', U'O', IOv2::ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(FYmd(U"1999-W52-6", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd(U"2019-W01-1", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd(U"1999-W52-5", U"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd(U"99-W52-6", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd(U"19-W01-1", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd(U"99-W52-5", U"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd(U"20 24/09/04", U"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(FYmd(U"20 01 01", U"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_char32_t_get_11()
{
    dump_info("Test timeio<char32_t> get 11...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char32_t, false, true, true>, false, true, true>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, true>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(U"%",  U'%',  0,  IOv2::ios_defs::eofbit);
    FOri(U"x",  U'%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri(U"%",  U'%', U'E', febit);
    FOri(U"%E%", U'%', U'E', IOv2::ios_defs::eofbit);
    FOri(U"%",  U'%', U'O', febit);
    FOri(U"%O%", U'%', U'O', IOv2::ios_defs::eofbit);

    FOri(U"%a", U'a', 0, IOv2::ios_defs::eofbit, 3);
    FOri(U"%Ea", U'a', U'E', IOv2::ios_defs::eofbit);
    FOri(U"a",   U'a', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oa", U'a', U'O', IOv2::ios_defs::eofbit);
    FOri(U"a",   U'a', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%A", U'A', 0, IOv2::ios_defs::eofbit, 3);
    FOri(U"%EA", U'A', U'E', IOv2::ios_defs::eofbit);
    FOri(U"A",   U'A', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OA", U'A', U'O', IOv2::ios_defs::eofbit);
    FOri(U"A",   U'A', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%b", U'b', 0, IOv2::ios_defs::eofbit, 3);
    FOri(U"%Eb", U'b', U'E', IOv2::ios_defs::eofbit);
    FOri(U"b",   U'b', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Ob", U'b', U'O', IOv2::ios_defs::eofbit);
    FOri(U"b",   U'b', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%B", U'B', 0, IOv2::ios_defs::eofbit, 3);
    FOri(U"%EB", U'B', U'E', IOv2::ios_defs::eofbit);
    FOri(U"B",   U'B', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OB", U'B', U'O', IOv2::ios_defs::eofbit);
    FOri(U"B",   U'B', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%h", U'h', 0, IOv2::ios_defs::eofbit, 3);
    FOri(U"%Eh", U'h', U'E', IOv2::ios_defs::eofbit);
    FOri(U"h",   U'h', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oh", U'h', U'O', IOv2::ios_defs::eofbit);
    FOri(U"h",   U'h', U'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    FOri(U"%c", U'c', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Ec", U'c', U'E', IOv2::ios_defs::eofbit);
    FOri(U"c",   U'c', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oc", U'c', U'O', IOv2::ios_defs::eofbit);
    FOri(U"c",   U'c', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%C", U'C', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%EC", U'C', U'E', IOv2::ios_defs::eofbit);
    FOri(U"C",   U'C', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OC", U'C', U'O', IOv2::ios_defs::eofbit);
    FOri(U"C",   U'C', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%d", U'd', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%Od", U'd', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%Ed", U'd', U'E', IOv2::ios_defs::eofbit);
    FOri(U"d",   U'd', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"d",   U'd', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%e", U'e', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%Oe", U'e', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%Ee", U'e', U'E', IOv2::ios_defs::eofbit);
    FOri(U"e",   U'e', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"e",   U'e', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%F", U'F', 0, IOv2::ios_defs::eofbit);
    FOri(U"%EF", U'F', U'E', IOv2::ios_defs::eofbit);
    FOri(U"F",   U'F', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OF", U'F', U'O', IOv2::ios_defs::eofbit);
    FOri(U"F",   U'F', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%x", U'x', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Ex", U'x', U'E', IOv2::ios_defs::eofbit);
    FOri(U"x",   U'x', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Ox", U'x', U'O', IOv2::ios_defs::eofbit);
    FOri(U"x",   U'x', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%D", U'D', 0, IOv2::ios_defs::eofbit);
    FOri(U"%ED", U'D', U'E', IOv2::ios_defs::eofbit);
    FOri(U"D",   U'D', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OD", U'D', U'O', IOv2::ios_defs::eofbit);
    FOri(U"D",   U'D', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"13", U'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(U"13", U'H', U'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(U"十三", U'H', U'O', IOv2::ios_defs::eofbit).m_hour == 13);
    FOri(U"%EH", U'H', U'E', IOv2::ios_defs::eofbit);
    FOri(U"H",   U'H', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"H",   U'H', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"01", U'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(U"01", U'I', U'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(U"一", U'I', U'O', IOv2::ios_defs::eofbit).m_hour == 1);
    FOri(U"%EI", U'I', U'E', IOv2::ios_defs::eofbit);
    FOri(U"I",   U'I', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"I",   U'I', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%j", U'j', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Ej", U'j', U'E', IOv2::ios_defs::eofbit);
    FOri(U"j",   U'j', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oj", U'j', U'O', IOv2::ios_defs::eofbit);
    FOri(U"j",   U'j', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%m", U'm',  0, IOv2::ios_defs::eofbit);
    FOri(U"%Om", U'm', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%Em", U'm', U'E', IOv2::ios_defs::eofbit);
    FOri(U"m",   U'm', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"m",   U'm', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"33", U'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(U"33", U'M', U'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(U"三十三", U'M', U'O', IOv2::ios_defs::eofbit).m_minute == 33);
    FOri(U"%EM", U'M', U'E', IOv2::ios_defs::eofbit);
    FOri(U"M",   U'M', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"M",   U'M', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"\n",   U'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(U"x",    U'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(U"\n",   U'n', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%En",  U'n', U'E', IOv2::ios_defs::eofbit, 3);
    FOri(U"n",    U'n', U'O', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%On",  U'n', U'O', IOv2::ios_defs::eofbit, 3);

    FOri(U"\t",   U't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(U"x",    U't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(U"\t",   U't', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Et",  U't', U'E', IOv2::ios_defs::eofbit, 3);
    FOri(U"n",    U't', U'O', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Ot",  U't', U'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(FHms(U"01 午後", U"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(FHms(U"01 午前", U"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(FOri(U"午後", U'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(FOri(U"午前", U'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    FOri(U"%Ep", U'p', U'E', IOv2::ios_defs::eofbit);
    FOri(U"p",   U'p', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Op", U'p', U'O', IOv2::ios_defs::eofbit);
    FOri(U"p",   U'p', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(U"午後01時33分18秒", U"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"%Er", U'r', U'E', IOv2::ios_defs::eofbit);
    FOri(U"r",   U'r', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Or", U'r', U'O', IOv2::ios_defs::eofbit);
    FOri(U"r",   U'r', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(U"13:33", U"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(U"%ER", U'R', U'E', IOv2::ios_defs::eofbit);
    FOri(U"R",   U'R', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OR", U'R', U'O', IOv2::ios_defs::eofbit);
    FOri(U"R",   U'R', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"18", U'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(U"18", U'S', U'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(U"十八", U'S', U'O', IOv2::ios_defs::eofbit).m_second == 18);
    FOri(U"%ES", U'S', U'E', IOv2::ios_defs::eofbit);
    FOri(U"S",   U'S', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"S",   U'S', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(U"13時33分18秒 America/Los_Angeles", U"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(FHms(U"13時33分18秒 America/Los_Angeles", U"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"X",   U'X', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OX", U'X', U'O', IOv2::ios_defs::eofbit);
    FOri(U"X",   U'X', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(U"13:33:18", U"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"%ET", U'T', U'E', IOv2::ios_defs::eofbit);
    FOri(U"T",   U'T', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OT", U'T', U'O', IOv2::ios_defs::eofbit);
    FOri(U"T",   U'T', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%u", U'u', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%Ou", U'u', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%Eu", U'u', U'E', IOv2::ios_defs::eofbit);
    FOri(U"u",   U'u', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"u",   U'u', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%g", U'g', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Eg", U'g', U'E', IOv2::ios_defs::eofbit);
    FOri(U"g",   U'g', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Og", U'g', U'O', IOv2::ios_defs::eofbit);
    FOri(U"g",   U'g', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%G", U'G', 0, IOv2::ios_defs::eofbit);
    FOri(U"%EG", U'G', U'E', IOv2::ios_defs::eofbit);
    FOri(U"G",   U'G', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OG", U'G', U'O', IOv2::ios_defs::eofbit);
    FOri(U"G",   U'G', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%U", U'U', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%OU", U'U', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%EU", U'U', U'E', IOv2::ios_defs::eofbit);
    FOri(U"U",   U'U', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"U",   U'U', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%W", U'W', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%OW", U'W', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%EW", U'W', U'E', IOv2::ios_defs::eofbit);
    FOri(U"W",   U'W', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"W",   U'W', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%V", U'V', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%OV", U'V', U'O',   IOv2::ios_defs::eofbit);
    FOri(U"54",  U'V', U'O', IOv2::ios_defs::strfailbit, 1);
    FOri(U"%EV", U'V', U'E', IOv2::ios_defs::eofbit);
    FOri(U"V",   U'V', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"V",   U'V', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%w", U'w', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%Ow", U'w', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%Ew", U'w', U'E', IOv2::ios_defs::eofbit);
    FOri(U"w",   U'w', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"w",   U'w', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%y", U'y', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%Ey", U'y', U'E', IOv2::ios_defs::eofbit);
    FOri(U"%Oy", U'y', U'O', IOv2::ios_defs::eofbit);
    FOri(U"y",  U'y', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"y",  U'y', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%Y", U'Y', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%EY", U'Y', U'E', IOv2::ios_defs::eofbit);
    FOri(U"Y",   U'Y', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OY", U'Y', U'O', IOv2::ios_defs::eofbit);
    FOri(U"Y",   U'Y', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"America/Los_Angeles", U'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    VERIFY(FOri(U"PST", U'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
    FOri(U"America/Los_Angexes", U'Z', 0, IOv2::ios_defs::strfailbit);
    FOri(U"%EZ", U'Z', U'E', IOv2::ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OZ", U'Z', U'O', IOv2::ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%z", U'z', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Ez", U'z', U'E', IOv2::ios_defs::eofbit);
    FOri(U"z",  U'z', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oz", U'z', U'O', IOv2::ios_defs::eofbit);
    FOri(U"z",  U'z', U'O', IOv2::ios_defs::strfailbit, 0);

    dump_info("Done\n");
}

void test_timeio_char32_t_get_12()
{
    dump_info("Test timeio<char32_t> get 12...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char32_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char32_t, false, true, false>, false, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(U"%",  U'%',  0,  IOv2::ios_defs::eofbit);
    FOri(U"x",  U'%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri(U"%",  U'%', U'E', febit);
    FOri(U"%E%", U'%', U'E', IOv2::ios_defs::eofbit);
    FOri(U"%",  U'%', U'O', febit);
    FOri(U"%O%", U'%', U'O', IOv2::ios_defs::eofbit);

    FOri(U"%a", U'a', 0, IOv2::ios_defs::eofbit, 3);
    FOri(U"%Ea", U'a', U'E', IOv2::ios_defs::eofbit);
    FOri(U"a",   U'a', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oa", U'a', U'O', IOv2::ios_defs::eofbit);
    FOri(U"a",   U'a', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%A", U'A', 0, IOv2::ios_defs::eofbit, 3);
    FOri(U"%EA", U'A', U'E', IOv2::ios_defs::eofbit);
    FOri(U"A",   U'A', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OA", U'A', U'O', IOv2::ios_defs::eofbit);
    FOri(U"A",   U'A', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%b", U'b', 0, IOv2::ios_defs::eofbit, 3);
    FOri(U"%Eb", U'b', U'E', IOv2::ios_defs::eofbit);
    FOri(U"b",   U'b', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Ob", U'b', U'O', IOv2::ios_defs::eofbit);
    FOri(U"b",   U'b', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%B", U'B', 0, IOv2::ios_defs::eofbit, 3);
    FOri(U"%EB", U'B', U'E', IOv2::ios_defs::eofbit);
    FOri(U"B",   U'B', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OB", U'B', U'O', IOv2::ios_defs::eofbit);
    FOri(U"B",   U'B', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%h", U'h', 0, IOv2::ios_defs::eofbit, 3);
    FOri(U"%Eh", U'h', U'E', IOv2::ios_defs::eofbit);
    FOri(U"h",   U'h', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oh", U'h', U'O', IOv2::ios_defs::eofbit);
    FOri(U"h",   U'h', U'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    FOri(U"%c", U'c', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Ec", U'c', U'E', IOv2::ios_defs::eofbit);
    FOri(U"c",   U'c', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oc", U'c', U'O', IOv2::ios_defs::eofbit);
    FOri(U"c",   U'c', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%C", U'C', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%EC", U'C', U'E', IOv2::ios_defs::eofbit);
    FOri(U"C",   U'C', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OC", U'C', U'O', IOv2::ios_defs::eofbit);
    FOri(U"C",   U'C', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%d", U'd', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%Od", U'd', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%Ed", U'd', U'E', IOv2::ios_defs::eofbit);
    FOri(U"d",   U'd', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"d",   U'd', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%e", U'e', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%Oe", U'e', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%Ee", U'e', U'E', IOv2::ios_defs::eofbit);
    FOri(U"e",   U'e', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"e",   U'e', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%F", U'F', 0, IOv2::ios_defs::eofbit);
    FOri(U"%EF", U'F', U'E', IOv2::ios_defs::eofbit);
    FOri(U"F",   U'F', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OF", U'F', U'O', IOv2::ios_defs::eofbit);
    FOri(U"F",   U'F', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%x", U'x', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Ex", U'x', U'E', IOv2::ios_defs::eofbit);
    FOri(U"x",   U'x', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Ox", U'x', U'O', IOv2::ios_defs::eofbit);
    FOri(U"x",   U'x', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%D", U'D', 0, IOv2::ios_defs::eofbit);
    FOri(U"%ED", U'D', U'E', IOv2::ios_defs::eofbit);
    FOri(U"D",   U'D', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OD", U'D', U'O', IOv2::ios_defs::eofbit);
    FOri(U"D",   U'D', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"13", U'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(U"13", U'H', U'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(U"十三", U'H', U'O', IOv2::ios_defs::eofbit).m_hour == 13);
    FOri(U"%EH", U'H', U'E', IOv2::ios_defs::eofbit);
    FOri(U"H",   U'H', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"H",   U'H', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"01", U'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(U"01", U'I', U'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(U"一", U'I', U'O', IOv2::ios_defs::eofbit).m_hour == 1);
    FOri(U"%EI", U'I', U'E', IOv2::ios_defs::eofbit);
    FOri(U"I",   U'I', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"I",   U'I', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%j", U'j', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Ej", U'j', U'E', IOv2::ios_defs::eofbit);
    FOri(U"j",   U'j', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oj", U'j', U'O', IOv2::ios_defs::eofbit);
    FOri(U"j",   U'j', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%m", U'm',  0, IOv2::ios_defs::eofbit);
    FOri(U"%Om", U'm', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%Em", U'm', U'E', IOv2::ios_defs::eofbit);
    FOri(U"m",   U'm', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"m",   U'm', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"33", U'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(U"33", U'M', U'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(U"三十三", 'M', U'O', IOv2::ios_defs::eofbit).m_minute == 33);
    FOri(U"%EM", U'M', U'E', IOv2::ios_defs::eofbit);
    FOri(U"M",   U'M', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"M",   U'M', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"\n",   U'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(U"x",    U'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(U"\n",   U'n', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%En",  U'n', U'E', IOv2::ios_defs::eofbit, 3);
    FOri(U"n",    U'n', U'O', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%On",  U'n', U'O', IOv2::ios_defs::eofbit, 3);

    FOri(U"\t",   U't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(U"x",    U't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(U"\t",   U't', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Et",  U't', U'E', IOv2::ios_defs::eofbit, 3);
    FOri(U"n",    U't', U'O', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Ot",  U't', U'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(FHms(U"01 午後", U"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(FHms(U"01 午前", U"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(FOri(U"午後", U'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(FOri(U"午前", U'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    FOri(U"%Ep", U'p', U'E', IOv2::ios_defs::eofbit);
    FOri(U"p",   U'p', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Op", U'p', U'O', IOv2::ios_defs::eofbit);
    FOri(U"p",   U'p', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(U"午後01時33分18秒", U"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"%Er", U'r', U'E', IOv2::ios_defs::eofbit);
    FOri(U"r",   U'r', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Or", U'r', U'O', IOv2::ios_defs::eofbit);
    FOri(U"r",   U'r', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(U"13:33", U"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(U"%ER", U'R', U'E', IOv2::ios_defs::eofbit);
    FOri(U"R",   U'R', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OR", U'R', U'O', IOv2::ios_defs::eofbit);
    FOri(U"R",   U'R', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(U"18", U'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(U"18", U'S', U'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(U"十八", U'S', U'O', IOv2::ios_defs::eofbit).m_second == 18);
    FOri(U"%ES", U'S', U'E', IOv2::ios_defs::eofbit);
    FOri(U"S",   U'S', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"S",   U'S', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(U"13時33分18秒", U"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(FHms(U"13時33分18秒", U"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"X",   U'X', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OX", U'X', U'O', IOv2::ios_defs::eofbit);
    FOri(U"X",   U'X', U'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(U"13:33:18", U"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(U"%ET", U'T', U'E', IOv2::ios_defs::eofbit);
    FOri(U"T",   U'T', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OT", U'T', U'O', IOv2::ios_defs::eofbit);
    FOri(U"T",   U'T', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%u", U'u', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%Ou", U'u', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%Eu", U'u', U'E', IOv2::ios_defs::eofbit);
    FOri(U"u",   U'u', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"u",   U'u', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%g", U'g', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Eg", U'g', U'E', IOv2::ios_defs::eofbit);
    FOri(U"g",   U'g', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Og", U'g', U'O', IOv2::ios_defs::eofbit);
    FOri(U"g",   U'g', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%G", U'G', 0, IOv2::ios_defs::eofbit);
    FOri(U"%EG", U'G', U'E', IOv2::ios_defs::eofbit);
    FOri(U"G",   U'G', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OG", U'G', U'O', IOv2::ios_defs::eofbit);
    FOri(U"G",   U'G', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%U", U'U', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%OU", U'U', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%EU", U'U', U'E', IOv2::ios_defs::eofbit);
    FOri(U"U",   U'U', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"U",   U'U', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%W", U'W', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%OW", U'W', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%EW", U'W', U'E', IOv2::ios_defs::eofbit);
    FOri(U"W",   U'W', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"W",   U'W', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%V", U'V', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%OV", U'V', U'O',   IOv2::ios_defs::eofbit);
    FOri(U"54",  U'V', U'O', IOv2::ios_defs::strfailbit, 1);
    FOri(U"%EV", U'V', U'E', IOv2::ios_defs::eofbit);
    FOri(U"V",   U'V', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"V",   U'V', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%w", U'w', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%Ow", U'w', U'O', IOv2::ios_defs::eofbit);
    FOri(U"%Ew", U'w', U'E', IOv2::ios_defs::eofbit);
    FOri(U"w",   U'w', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"w",   U'w', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%y", U'y', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%Ey", U'y', U'E', IOv2::ios_defs::eofbit);
    FOri(U"%Oy", U'y', U'O', IOv2::ios_defs::eofbit);
    FOri(U"y",  U'y', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"y",  U'y', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%Y", U'Y', 0,   IOv2::ios_defs::eofbit);
    FOri(U"%EY", U'Y', U'E', IOv2::ios_defs::eofbit);
    FOri(U"Y",   U'Y', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OY", U'Y', U'O', IOv2::ios_defs::eofbit);
    FOri(U"Y",   U'Y', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%Z", U'Z', 0, IOv2::ios_defs::eofbit);
    FOri(U"%EZ", U'Z', U'E', IOv2::ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%OZ", U'Z', U'O', IOv2::ios_defs::eofbit);
    FOri(U"Z",   U'Z', U'O', IOv2::ios_defs::strfailbit, 0);

    FOri(U"%z", U'z', 0, IOv2::ios_defs::eofbit);
    FOri(U"%Ez", U'z', U'E', IOv2::ios_defs::eofbit);
    FOri(U"z",  U'z', U'E', IOv2::ios_defs::strfailbit, 0);
    FOri(U"%Oz", U'z', U'O', IOv2::ios_defs::eofbit);
    FOri(U"z",  U'z', U'O', IOv2::ios_defs::strfailbit, 0);

    dump_info("Done\n");
}