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

void test_timeio_char8_t_put_1()
{
    dump_info("Test timeio<char8_t> put 1...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("C"));
    auto tp = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    std::u8string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, u8'%');
        VERIFY(res == u8"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a');
        VERIFY(res == u8"Wed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'E');
        VERIFY(res == u8"%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'O');
        VERIFY(res == u8"%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A');
        VERIFY(res == u8"Wednesday");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'E');
        VERIFY(res == u8"%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'O');
        VERIFY(res == u8"%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b');
        VERIFY(res == u8"Sep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'E');
        VERIFY(res == u8"%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'O');
        VERIFY(res == u8"%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h');
        VERIFY(res == u8"Sep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'E');
        VERIFY(res == u8"%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'O');
        VERIFY(res == u8"%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B');
        VERIFY(res == u8"September");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'E');
        VERIFY(res == u8"%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'O');
        VERIFY(res == u8"%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c');
        VERIFY(res == u8"09/04/24 13:33:18 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'E');
        VERIFY(res == u8"09/04/24 13:33:18 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'O');
        VERIFY(res == u8"%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C');
        VERIFY(res == u8"20");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'E');
        VERIFY(res == u8"20");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'O');
        VERIFY(res == u8"%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x');
        VERIFY(res == u8"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'E');
        VERIFY(res == u8"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'O');
        VERIFY(res == u8"%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D');
        VERIFY(res == u8"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'E');
        VERIFY(res == u8"%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'O');
        VERIFY(res == u8"%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd');
        VERIFY(res == u8"04");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'E');
        VERIFY(res == u8"%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'O');
        VERIFY(res == u8"04");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e');
        VERIFY(res == u8" 4");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'E');
        VERIFY(res == u8"%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'O');
        VERIFY(res == u8" 4");
    }
      
    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F');
        VERIFY(res == u8"2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'E');
        VERIFY(res == u8"%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'O');
        VERIFY(res == u8"%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H');
        VERIFY(res == u8"13");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'E');
        VERIFY(res == u8"%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'O');
        VERIFY(res == u8"13");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I');
        VERIFY(res == u8"01");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'E');
        VERIFY(res == u8"%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'O');
        VERIFY(res == u8"01");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j');
        VERIFY(res == u8"248");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'E');
        VERIFY(res == u8"%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'O');
        VERIFY(res == u8"%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M');
        VERIFY(res == u8"33");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'E');
        VERIFY(res == u8"%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'O');
        VERIFY(res == u8"33");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm');
        VERIFY(res == u8"09");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'E');
        VERIFY(res == u8"%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'O');
        VERIFY(res == u8"09");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n');
        VERIFY(res == u8"\n");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'E');
        VERIFY(res == u8"%En");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'O');
        VERIFY(res == u8"%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p');
        VERIFY(res == u8"PM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'E');
        VERIFY(res == u8"%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'O');
        VERIFY(res == u8"%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R');
        VERIFY(res == u8"13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'E');
        VERIFY(res == u8"%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'O');
        VERIFY(res == u8"%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r');
        VERIFY(res == u8"01:33:18 PM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'E');
        VERIFY(res == u8"%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'O');
        VERIFY(res == u8"%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S');
        VERIFY(res == u8"18");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'E');
        VERIFY(res == u8"%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'O');
        VERIFY(res == u8"18");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X');
        VERIFY(res == u8"13:33:18 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'E');
        VERIFY(res == u8"13:33:18 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'O');
        VERIFY(res == u8"%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T');
        VERIFY(res == u8"13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'E');
        VERIFY(res == u8"%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'O');
        VERIFY(res == u8"%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8't');
        VERIFY(res == u8"\t");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'E');
        VERIFY(res == u8"%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'O');
        VERIFY(res == u8"%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u');
        VERIFY(res == u8"3");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'E');
        VERIFY(res == u8"%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'O');
        VERIFY(res == u8"3");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U');
        VERIFY(res == u8"35");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'E');
        VERIFY(res == u8"%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'O');
        VERIFY(res == u8"35");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V');
        VERIFY(res == u8"36");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'E');
        VERIFY(res == u8"%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'O');
        VERIFY(res == u8"36");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g');
        VERIFY(res == u8"24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'E');
        VERIFY(res == u8"%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'O');
        VERIFY(res == u8"%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G');
        VERIFY(res == u8"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'E');
        VERIFY(res == u8"%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'O');
        VERIFY(res == u8"%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W');
        VERIFY(res == u8"36");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'E');
        VERIFY(res == u8"%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'O');
        VERIFY(res == u8"36");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w');
        VERIFY(res == u8"3");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'E');
        VERIFY(res == u8"%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'O');
        VERIFY(res == u8"3");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y');
        VERIFY(res == u8"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'E');
        VERIFY(res == u8"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'O');
        VERIFY(res == u8"%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y');
        VERIFY(res == u8"24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'E');
        VERIFY(res == u8"24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'O');
        VERIFY(res == u8"24");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z');
        VERIFY(res == u8"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'E');
        VERIFY(res == u8"%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'O');
        VERIFY(res == u8"%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z');
        VERIFY(res == u8"-0700");
        VERIFY(!(res.empty()));
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'E');
        VERIFY(res == u8"%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'O');
        VERIFY(res == u8"%Oz");
    }    

    dump_info("Done\n");
}

void test_timeio_char8_t_put_2()
{
    dump_info("Test timeio<char8_t> put 2...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("zh_CN.UTF-8"));
    auto tp = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    std::u8string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, u8'%');
        VERIFY(res == u8"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a');
        VERIFY(res == u8"三");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'E');
        VERIFY(res == u8"%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'O');
        VERIFY(res == u8"%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A');
        VERIFY(res == u8"星期三");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'E');
        VERIFY(res == u8"%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'O');
        VERIFY(res == u8"%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b');
        VERIFY(res == u8"9月");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'E');
        VERIFY(res == u8"%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'O');
        VERIFY(res == u8"%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h');
        VERIFY(res == u8"9月");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'E');
        VERIFY(res == u8"%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'O');
        VERIFY(res == u8"%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B');
        VERIFY(res == u8"九月");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'E');
        VERIFY(res == u8"%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'O');
        VERIFY(res == u8"%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c');
        VERIFY(res == u8"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'E');
        VERIFY(res == u8"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'O');
        VERIFY(res == u8"%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C');
        VERIFY(res == u8"20");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'E');
        VERIFY(res == u8"20");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'O');
        VERIFY(res == u8"%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x');
        VERIFY(res == u8"2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'E');
        VERIFY(res == u8"2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'O');
        VERIFY(res == u8"%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D');
        VERIFY(res == u8"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'E');
        VERIFY(res == u8"%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'O');
        VERIFY(res == u8"%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd');
        VERIFY(res == u8"04");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'E');
        VERIFY(res == u8"%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'O');
        VERIFY(res == u8"04");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e');
        VERIFY(res == u8" 4");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'E');
        VERIFY(res == u8"%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'O');
        VERIFY(res == u8" 4");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F');
        VERIFY(res == u8"2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'E');
        VERIFY(res == u8"%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'O');
        VERIFY(res == u8"%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H');
        VERIFY(res == u8"13");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'E');
        VERIFY(res == u8"%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'O');
        VERIFY(res == u8"13");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I');
        VERIFY(res == u8"01");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'E');
        VERIFY(res == u8"%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'O');
        VERIFY(res == u8"01");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j');
        VERIFY(res == u8"248");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'E');
        VERIFY(res == u8"%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'O');
        VERIFY(res == u8"%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M');
        VERIFY(res == u8"33");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'E');
        VERIFY(res == u8"%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'O');
        VERIFY(res == u8"33");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm');
        VERIFY(res == u8"09");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'E');
        VERIFY(res == u8"%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'O');
        VERIFY(res == u8"09");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n');
        VERIFY(res == u8"\n");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'E');
        VERIFY(res == u8"%En");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'O');
        VERIFY(res == u8"%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p');
        VERIFY(res == u8"下午");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'E');
        VERIFY(res == u8"%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'O');
        VERIFY(res == u8"%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R');
        VERIFY(res == u8"13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'E');
        VERIFY(res == u8"%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'O');
        VERIFY(res == u8"%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r');
        VERIFY(res == u8"下午 01时33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'E');
        VERIFY(res == u8"%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'O');
        VERIFY(res == u8"%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S');
        VERIFY(res == u8"18");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'E');
        VERIFY(res == u8"%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'O');
        VERIFY(res == u8"18");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X');
        VERIFY(res == u8"13时33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'E');
        VERIFY(res == u8"13时33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'O');
        VERIFY(res == u8"%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T');
        VERIFY(res == u8"13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'E');
        VERIFY(res == u8"%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'O');
        VERIFY(res == u8"%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8't');
        VERIFY(res == u8"\t");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'E');
        VERIFY(res == u8"%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'O');
        VERIFY(res == u8"%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u');
        VERIFY(res == u8"3");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'E');
        VERIFY(res == u8"%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'O');
        VERIFY(res == u8"3");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U');
        VERIFY(res == u8"35");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'E');
        VERIFY(res == u8"%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'O');
        VERIFY(res == u8"35");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V');
        VERIFY(res == u8"36");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'E');
        VERIFY(res == u8"%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'O');
        VERIFY(res == u8"36");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g');
        VERIFY(res == u8"24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'E');
        VERIFY(res == u8"%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'O');
        VERIFY(res == u8"%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G');
        VERIFY(res == u8"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'E');
        VERIFY(res == u8"%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'O');
        VERIFY(res == u8"%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W');
        VERIFY(res == u8"36");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'E');
        VERIFY(res == u8"%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'O');
        VERIFY(res == u8"36");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w');
        VERIFY(res == u8"3");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'E');
        VERIFY(res == u8"%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'O');
        VERIFY(res == u8"3");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y');
        VERIFY(res == u8"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'E');
        VERIFY(res == u8"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'O');
        VERIFY(res == u8"%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y');
        VERIFY(res == u8"24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'E');
        VERIFY(res == u8"24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'O');
        VERIFY(res == u8"24");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z');
        VERIFY(res == u8"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'E');
        VERIFY(res == u8"%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'O');
        VERIFY(res == u8"%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z');
        VERIFY(res == u8"-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'E');
        VERIFY(res == u8"%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'O');
        VERIFY(res == u8"%Oz");
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_put_3()
{
    dump_info("Test timeio<char8_t> put 3...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("ja_JP.UTF-8"));
    auto tp = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    std::u8string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, u8'%');
        VERIFY(res == u8"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a');
        VERIFY(res == u8"水");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'E');
        VERIFY(res == u8"%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'O');
        VERIFY(res == u8"%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A');
        VERIFY(res == u8"水曜日");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'E');
        VERIFY(res == u8"%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'O');
        VERIFY(res == u8"%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b');
        VERIFY(res == u8" 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'E');
        VERIFY(res == u8"%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'O');
        VERIFY(res == u8"%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h');
        VERIFY(res == u8" 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'E');
        VERIFY(res == u8"%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'O');
        VERIFY(res == u8"%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B');
        VERIFY(res == u8"9月");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'E');
        VERIFY(res == u8"%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'O');
        VERIFY(res == u8"%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c');
        VERIFY(res == u8"2024年09月04日 13時33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'E');
        VERIFY(res == u8"令和6年09月04日 13時33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'O');
        VERIFY(res == u8"%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C');
        VERIFY(res == u8"20");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'E');
        VERIFY(res == u8"令和");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'O');
        VERIFY(res == u8"%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x');
        VERIFY(res == u8"2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'E');
        VERIFY(res == u8"令和6年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'O');
        VERIFY(res == u8"%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D');
        VERIFY(res == u8"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'E');
        VERIFY(res == u8"%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'O');
        VERIFY(res == u8"%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd');
        VERIFY(res == u8"04");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'E');
        VERIFY(res == u8"%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'O');
        VERIFY(res == u8"四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e');
        VERIFY(res == u8" 4");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'E');
        VERIFY(res == u8"%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'O');
        VERIFY(res == u8"四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F');
        VERIFY(res == u8"2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'E');
        VERIFY(res == u8"%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'O');
        VERIFY(res == u8"%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H');
        VERIFY(res == u8"13");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'E');
        VERIFY(res == u8"%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'O');
        VERIFY(res == u8"十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I');
        VERIFY(res == u8"01");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'E');
        VERIFY(res == u8"%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'O');
        VERIFY(res == u8"一");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j');
        VERIFY(res == u8"248");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'E');
        VERIFY(res == u8"%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'O');
        VERIFY(res == u8"%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M');
        VERIFY(res == u8"33");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'E');
        VERIFY(res == u8"%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'O');
        VERIFY(res == u8"三十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm');
        VERIFY(res == u8"09");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'E');
        VERIFY(res == u8"%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'O');
        VERIFY(res == u8"九");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n');
        VERIFY(res == u8"\n");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'E');
        VERIFY(res == u8"%En");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'O');
        VERIFY(res == u8"%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p');
        VERIFY(res == u8"午後");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'E');
        VERIFY(res == u8"%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'O');
        VERIFY(res == u8"%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R');
        VERIFY(res == u8"13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'E');
        VERIFY(res == u8"%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'O');
        VERIFY(res == u8"%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r');
        VERIFY(res == u8"午後01時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'E');
        VERIFY(res == u8"%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'O');
        VERIFY(res == u8"%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S');
        VERIFY(res == u8"18");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'E');
        VERIFY(res == u8"%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'O');
        VERIFY(res == u8"十八");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X');
        VERIFY(res == u8"13時33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'E');
        VERIFY(res == u8"13時33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'O');
        VERIFY(res == u8"%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T');
        VERIFY(res == u8"13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'E');
        VERIFY(res == u8"%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'O');
        VERIFY(res == u8"%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8't');
        VERIFY(res == u8"\t");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'E');
        VERIFY(res == u8"%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'O');
        VERIFY(res == u8"%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u');
        VERIFY(res == u8"3");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'E');
        VERIFY(res == u8"%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'O');
        VERIFY(res == u8"三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U');
        VERIFY(res == u8"35");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'E');
        VERIFY(res == u8"%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'O');
        VERIFY(res == u8"三十五");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V');
        VERIFY(res == u8"36");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'E');
        VERIFY(res == u8"%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'O');
        VERIFY(res == u8"三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g');
        VERIFY(res == u8"24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'E');
        VERIFY(res == u8"%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'O');
        VERIFY(res == u8"%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G');
        VERIFY(res == u8"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'E');
        VERIFY(res == u8"%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'O');
        VERIFY(res == u8"%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W');
        VERIFY(res == u8"36");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'E');
        VERIFY(res == u8"%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'O');
        VERIFY(res == u8"三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w');
        VERIFY(res == u8"3");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'E');
        VERIFY(res == u8"%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'O');
        VERIFY(res == u8"三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y');
        VERIFY(res == u8"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'E');
        VERIFY(res == u8"令和6年");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'O');
        VERIFY(res == u8"%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y');
        VERIFY(res == u8"24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'E');
        VERIFY(res == u8"6");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'O');
        VERIFY(res == u8"二十四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z');
        VERIFY(res == u8"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'E');
        VERIFY(res == u8"%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'O');
        VERIFY(res == u8"%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z');
        VERIFY(res == u8"-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'E');
        VERIFY(res == u8"%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'O');
        VERIFY(res == u8"%Oz");
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_put_4()
{
    dump_info("Test timeio<char8_t> put 4...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("C"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::u8string oss;
    {
        obj.put(std::back_inserter(oss), time1, u8'a');
        VERIFY(oss == u8"Sun");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, u8'x');
        VERIFY(oss == u8"04/04/71");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, u8'X');
        VERIFY(oss == u8"12:00:00 America/Los_Angeles");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, u8'x', u8'E');
        VERIFY(oss == u8"04/04/71");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, u8'X', u8'E');
        VERIFY(oss == u8"12:00:00 America/Los_Angeles");
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_put_5()
{
    dump_info("Test timeio<char8_t> put 5...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("de_DE.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::u8string oss;
    {
        obj.put(std::back_inserter(oss), time1, u8'a');
        VERIFY(!((oss != u8"Son") && (oss != u8"So")));
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'x');
        VERIFY(oss == u8"04.04.1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'X');
        VERIFY(oss == u8"12:00:00 America/Los_Angeles");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'x', u8'E');
        VERIFY(oss == u8"04.04.1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'X', u8'E');
        VERIFY(oss == u8"12:00:00 America/Los_Angeles");
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_put_6()
{
    dump_info("Test timeio<char8_t> put 6...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("en_HK.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::u8string oss;
    {
        obj.put(std::back_inserter(oss), time1, u8'a');
        VERIFY(oss == u8"Sun");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'x');
        VERIFY(oss == u8"Sunday, April 04, 1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'X');
        VERIFY(oss.find(u8"12:00:00") != std::u8string::npos);
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'x', u8'E');
        VERIFY(oss == u8"Sunday, April 04, 1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'X', u8'E');
        VERIFY(oss.find(u8"12:00:00") != std::u8string::npos);
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_put_7()
{
    dump_info("Test timeio<char8_t> put 7...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("es_ES.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::u8string oss;
    {
        obj.put(std::back_inserter(oss), time1, u8'a');
        VERIFY(oss == u8"dom");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'x');
        VERIFY(oss == u8"04/04/71");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'X');
        VERIFY(oss == u8"12:00:00 America/Los_Angeles");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'x', u8'E');
        VERIFY(oss == u8"04/04/71");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'X', u8'E');
        VERIFY(oss == u8"12:00:00 America/Los_Angeles");
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_put_8()
{
    dump_info("Test timeio<char8_t> put 8...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("C"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::u8string date = u8"%A, the second of %B";
    const std::u8string date_ex = u8"%Ex";
    std::u8string oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        VERIFY(oss == u8"Sunday, the second of April");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        VERIFY(oss != oss2);
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_put_9()
{
    dump_info("Test timeio<char8_t> put 9...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("de_DE.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::u8string date = u8"%A, the second of %B";
    const std::u8string date_ex = u8"%Ex";
    std::u8string oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        VERIFY(oss == u8"Sonntag, the second of April");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        VERIFY(oss != oss2);
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_put_10()
{
    dump_info("Test timeio<char8_t> put 10...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("en_HK.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::u8string date = u8"%A, the second of %B";
    const std::u8string date_ex = u8"%Ex";
    std::u8string oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        VERIFY(oss == u8"Sunday, the second of April");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        VERIFY(oss != oss2);
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_put_11()
{
    dump_info("Test timeio<char8_t> put 11...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("fr_FR.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::u8string date = u8"%A, the second of %B";
    const std::u8string date_ex = u8"%Ex";
    std::u8string oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        VERIFY(oss == u8"dimanche, the second of avril");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        VERIFY(oss != oss2);
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_put_12()
{
    dump_info("Test timeio<char8_t> put 12...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("C"));
    auto time_sanity = create_zoned_time(1997, 6, 26, 12, 0, 0, "America/Los_Angeles");

    std::u8string res(50, 'x');
    const std::u8string date = u8"%T, %A, the second of %B, %Y";
        
    auto ret1 = obj.put(res.begin(), time_sanity, date);
    std::u8string sanity1(res.begin(), ret1);
    VERIFY(res == u8"12:00:00, Thursday, the second of June, 1997xxxxxx");
    VERIFY(sanity1 == u8"12:00:00, Thursday, the second of June, 1997");

    dump_info("Done\n");
}

void test_timeio_char8_t_put_13()
{
    dump_info("Test timeio<char8_t> put 13...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("C"));
    auto time_sanity = create_zoned_time(1997, 6, 24, 12, 0, 0, "America/Los_Angeles");

    std::u8string res(50, 'x');

    auto ret1 = obj.put(res.begin(), time_sanity, 'A');
    std::u8string sanity1(res.begin(), ret1);
    VERIFY(res == u8"Tuesdayxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    VERIFY(sanity1 == u8"Tuesday");

    dump_info("Done\n");
}

void test_timeio_char8_t_put_14()
{
    dump_info("Test timeio<char8_t> put 14...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("ta_IN.UTF-8"));
    const tm time1 = test_tm(0, 0, 12, 4, 3, 71, 0, 93, 0);
    auto zt = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::u8string res;
    obj.put(std::back_inserter(res), zt, 'c');

    char8_t time_buffer[128];
    setlocale(LC_ALL, "ta_IN");
    std::strftime((char*)time_buffer, 128, "%c", &time1);
    setlocale(LC_ALL, "C");

    VERIFY(time_buffer + std::u8string(u8" America/Los_Angeles") == res);

    dump_info("Done\n");
}

void test_timeio_char8_t_put_15()
{
    dump_info("Test timeio<char8_t> put 15...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("ja_JP.UTF-8"));
    using namespace std::chrono;
    year_month_day tp{year{2024}, month{9}, day{4}};

    std::u8string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, u8'%');
        VERIFY(res == u8"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a');
        VERIFY(res == u8"水");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'E');
        VERIFY(res == u8"%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'O');
        VERIFY(res == u8"%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A');
        VERIFY(res == u8"水曜日");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'E');
        VERIFY(res == u8"%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'O');
        VERIFY(res == u8"%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b');
        VERIFY(res == u8" 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'E');
        VERIFY(res == u8"%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'O');
        VERIFY(res == u8"%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h');
        VERIFY(res == u8" 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'E');
        VERIFY(res == u8"%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'O');
        VERIFY(res == u8"%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B');
        VERIFY(res == u8"9月");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'E');
        VERIFY(res == u8"%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'O');
        VERIFY(res == u8"%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c');
        VERIFY(res == u8"%c");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'E');
        VERIFY(res == u8"%Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'O');
        VERIFY(res == u8"%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C');
        VERIFY(res == u8"20");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'E');
        VERIFY(res == u8"令和");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'O');
        VERIFY(res == u8"%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x');
        VERIFY(res == u8"2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'E');
        VERIFY(res == u8"令和6年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'O');
        VERIFY(res == u8"%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D');
        VERIFY(res == u8"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'E');
        VERIFY(res == u8"%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'O');
        VERIFY(res == u8"%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd');
        VERIFY(res == u8"04");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'E');
        VERIFY(res == u8"%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'O');
        VERIFY(res == u8"四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e');
        VERIFY(res == u8" 4");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'E');
        VERIFY(res == u8"%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'O');
        VERIFY(res == u8"四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F');
        VERIFY(res == u8"2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'E');
        VERIFY(res == u8"%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'O');
        VERIFY(res == u8"%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H');
        VERIFY(res == u8"%H");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'E');
        VERIFY(res == u8"%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'O');
        VERIFY(res == u8"%OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I');
        VERIFY(res == u8"%I");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'E');
        VERIFY(res == u8"%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'O');
        VERIFY(res == u8"%OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j');
        VERIFY(res == u8"248");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'E');
        VERIFY(res == u8"%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'O');
        VERIFY(res == u8"%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M');
        VERIFY(res == u8"%M");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'E');
        VERIFY(res == u8"%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'O');
        VERIFY(res == u8"%OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm');
        VERIFY(res == u8"09");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'E');
        VERIFY(res == u8"%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'O');
        VERIFY(res == u8"九");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n');
        VERIFY(res == u8"\n");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'E');
        VERIFY(res == u8"%En");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'O');
        VERIFY(res == u8"%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p');
        VERIFY(res == u8"%p");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'E');
        VERIFY(res == u8"%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'O');
        VERIFY(res == u8"%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R');
        VERIFY(res == u8"%R");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'E');
        VERIFY(res == u8"%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'O');
        VERIFY(res == u8"%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r');
        VERIFY(res == u8"%r");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'E');
        VERIFY(res == u8"%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'O');
        VERIFY(res == u8"%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S');
        VERIFY(res == u8"%S");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'E');
        VERIFY(res == u8"%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'O');
        VERIFY(res == u8"%OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X');
        VERIFY(res == u8"%X");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'E');
        VERIFY(res == u8"%EX");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'O');
        VERIFY(res == u8"%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T');
        VERIFY(res == u8"%T");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'E');
        VERIFY(res == u8"%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'O');
        VERIFY(res == u8"%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8't');
        VERIFY(res == u8"\t");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'E');
        VERIFY(res == u8"%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'O');
        VERIFY(res == u8"%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u');
        VERIFY(res == u8"3");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'E');
        VERIFY(res == u8"%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'O');
        VERIFY(res == u8"三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U');
        VERIFY(res == u8"35");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'E');
        VERIFY(res == u8"%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'O');
        VERIFY(res == u8"三十五");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V');
        VERIFY(res == u8"36");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'E');
        VERIFY(res == u8"%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'O');
        VERIFY(res == u8"三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g');
        VERIFY(res == u8"24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'E');
        VERIFY(res == u8"%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'O');
        VERIFY(res == u8"%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G');
        VERIFY(res == u8"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'E');
        VERIFY(res == u8"%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'O');
        VERIFY(res == u8"%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W');
        VERIFY(res == u8"36");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'E');
        VERIFY(res == u8"%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'O');
        VERIFY(res == u8"三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w');
        VERIFY(res == u8"3");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'E');
        VERIFY(res == u8"%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'O');
        VERIFY(res == u8"三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y');
        VERIFY(res == u8"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'E');
        VERIFY(res == u8"令和6年");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'O');
        VERIFY(res == u8"%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y');
        VERIFY(res == u8"24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'E');
        VERIFY(res == u8"6");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'O');
        VERIFY(res == u8"二十四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z');
        VERIFY(res == u8"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'E');
        VERIFY(res == u8"%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'O');
        VERIFY(res == u8"%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z');
        VERIFY(res == u8"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'E');
        VERIFY(res == u8"%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'O');
        VERIFY(res == u8"%Oz");
    }
    dump_info("Done\n");
}

void test_timeio_char8_t_put_16()
{
    dump_info("Test timeio<char8_t> put 16...");

    using namespace std::chrono;
    seconds time_sec = hours{13} + minutes{33} + seconds{18};
    std::chrono::hh_mm_ss tp{time_sec};

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("ja_JP.UTF-8"));
    std::u8string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, u8'%');
        VERIFY(res == u8"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a');
        VERIFY(res == u8"%a");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'E');
        VERIFY(res == u8"%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'O');
        VERIFY(res == u8"%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A');
        VERIFY(res == u8"%A");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'E');
        VERIFY(res == u8"%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'O');
        VERIFY(res == u8"%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b');
        VERIFY(res == u8"%b");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'E');
        VERIFY(res == u8"%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'O');
        VERIFY(res == u8"%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h');
        VERIFY(res == u8"%h");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'E');
        VERIFY(res == u8"%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'O');
        VERIFY(res == u8"%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B');
        VERIFY(res == u8"%B");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'E');
        VERIFY(res == u8"%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'O');
        VERIFY(res == u8"%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c');
        VERIFY(res == u8"%c");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'E');
        VERIFY(res == u8"%Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'O');
        VERIFY(res == u8"%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x');
        VERIFY(res == u8"%x");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'E');
        VERIFY(res == u8"%Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'O');
        VERIFY(res == u8"%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D');
        VERIFY(res == u8"%D");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'E');
        VERIFY(res == u8"%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'O');
        VERIFY(res == u8"%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd');
        VERIFY(res == u8"%d");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'E');
        VERIFY(res == u8"%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'O');
        VERIFY(res == u8"%Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e');
        VERIFY(res == u8"%e");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'E');
        VERIFY(res == u8"%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'O');
        VERIFY(res == u8"%Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F');
        VERIFY(res == u8"%F");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'E');
        VERIFY(res == u8"%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'O');
        VERIFY(res == u8"%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H');
        VERIFY(res == u8"13");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'E');
        VERIFY(res == u8"%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'O');
        VERIFY(res == u8"十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I');
        VERIFY(res == u8"01");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'E');
        VERIFY(res == u8"%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'O');
        VERIFY(res == u8"一");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j');
        VERIFY(res == u8"%j");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'E');
        VERIFY(res == u8"%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'O');
        VERIFY(res == u8"%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M');
        VERIFY(res == u8"33");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'E');
        VERIFY(res == u8"%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'O');
        VERIFY(res == u8"三十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm');
        VERIFY(res == u8"%m");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'E');
        VERIFY(res == u8"%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'O');
        VERIFY(res == u8"%Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n');
        VERIFY(res == u8"\n");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'E');
        VERIFY(res == u8"%En");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'O');
        VERIFY(res == u8"%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p');
        VERIFY(res == u8"午後");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'E');
        VERIFY(res == u8"%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'O');
        VERIFY(res == u8"%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R');
        VERIFY(res == u8"13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'E');
        VERIFY(res == u8"%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'O');
        VERIFY(res == u8"%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r');
        VERIFY(res == u8"午後01時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'E');
        VERIFY(res == u8"%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'O');
        VERIFY(res == u8"%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S');
        VERIFY(res == u8"18");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'E');
        VERIFY(res == u8"%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'O');
        VERIFY(res == u8"十八");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X');
        VERIFY(res == u8"13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'E');
        VERIFY(res == u8"13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'O');
        VERIFY(res == u8"%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T');
        VERIFY(res == u8"13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'E');
        VERIFY(res == u8"%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'O');
        VERIFY(res == u8"%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8't');
        VERIFY(res == u8"\t");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'E');
        VERIFY(res == u8"%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'O');
        VERIFY(res == u8"%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u');
        VERIFY(res == u8"%u");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'E');
        VERIFY(res == u8"%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'O');
        VERIFY(res == u8"%Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U');
        VERIFY(res == u8"%U");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'E');
        VERIFY(res == u8"%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'O');
        VERIFY(res == u8"%OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V');
        VERIFY(res == u8"%V");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'E');
        VERIFY(res == u8"%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'O');
        VERIFY(res == u8"%OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g');
        VERIFY(res == u8"%g");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'E');
        VERIFY(res == u8"%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'O');
        VERIFY(res == u8"%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G');
        VERIFY(res == u8"%G");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'E');
        VERIFY(res == u8"%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'O');
        VERIFY(res == u8"%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W');
        VERIFY(res == u8"%W");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'E');
        VERIFY(res == u8"%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'O');
        VERIFY(res == u8"%OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w');
        VERIFY(res == u8"%w");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'E');
        VERIFY(res == u8"%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'O');
        VERIFY(res == u8"%Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y');
        VERIFY(res == u8"%Y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'E');
        VERIFY(res == u8"%EY");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'O');
        VERIFY(res == u8"%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y');
        VERIFY(res == u8"%y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'E');
        VERIFY(res == u8"%Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'O');
        VERIFY(res == u8"%Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z'); VERIFY(res == u8"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'E');
        VERIFY(res == u8"%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'O');
        VERIFY(res == u8"%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z'); VERIFY(res == u8"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'E');
        VERIFY(res == u8"%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'O');
        VERIFY(res == u8"%Oz");
    }
    dump_info("Done\n");
}

void test_timeio_char8_t_put_17()
{
    dump_info("Test timeio<char8_t> put 17...");
    std::tm tp = test_tm(18, 33, 13, 4, 9 - 1, 2024 - 1900, 0, 0, 0);
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("ja_JP.UTF-8"));

    std::u8string res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, u8'%');
        VERIFY(res == u8"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a');
        VERIFY(res == u8"水");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'E');
        VERIFY(res == u8"%Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'O');
        VERIFY(res == u8"%Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A');
        VERIFY(res == u8"水曜日");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'E');
        VERIFY(res == u8"%EA");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'O');
        VERIFY(res == u8"%OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b');
        VERIFY(res == u8" 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'E');
        VERIFY(res == u8"%Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'O');
        VERIFY(res == u8"%Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h');
        VERIFY(res == u8" 9月");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'E');
        VERIFY(res == u8"%Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'O');
        VERIFY(res == u8"%Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B');
        VERIFY(res == u8"9月");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'E');
        VERIFY(res == u8"%EB");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'O');
        VERIFY(res == u8"%OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c');
        VERIFY(res == u8"2024年09月04日 13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'E');
        VERIFY(res == u8"令和6年09月04日 13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'O');
        VERIFY(res == u8"%Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C');
        VERIFY(res == u8"20");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'E');
        VERIFY(res == u8"令和");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'O');
        VERIFY(res == u8"%OC");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x');
        VERIFY(res == u8"2024年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'E');
        VERIFY(res == u8"令和6年09月04日");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'O');
        VERIFY(res == u8"%Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D');
        VERIFY(res == u8"09/04/24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'E');
        VERIFY(res == u8"%ED");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'O');
        VERIFY(res == u8"%OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd');
        VERIFY(res == u8"04");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'E');
        VERIFY(res == u8"%Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'O');
        VERIFY(res == u8"四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e');
        VERIFY(res == u8" 4");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'E');
        VERIFY(res == u8"%Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'O');
        VERIFY(res == u8"四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F');
        VERIFY(res == u8"2024-09-04");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'E');
        VERIFY(res == u8"%EF");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'O');
        VERIFY(res == u8"%OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H');
        VERIFY(res == u8"13");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'E');
        VERIFY(res == u8"%EH");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'O');
        VERIFY(res == u8"十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I');
        VERIFY(res == u8"01");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'E');
        VERIFY(res == u8"%EI");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'O');
        VERIFY(res == u8"一");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j');
        VERIFY(res == u8"248");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'E');
        VERIFY(res == u8"%Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'O');
        VERIFY(res == u8"%Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M');
        VERIFY(res == u8"33");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'E');
        VERIFY(res == u8"%EM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'O');
        VERIFY(res == u8"三十三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm');
        VERIFY(res == u8"09");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'E');
        VERIFY(res == u8"%Em");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'O');
        VERIFY(res == u8"九");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n');
        VERIFY(res == u8"\n");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'E');
        VERIFY(res == u8"%En");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'O');
        VERIFY(res == u8"%On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p');
        VERIFY(res == u8"午後");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'E');
        VERIFY(res == u8"%Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'O');
        VERIFY(res == u8"%Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R');
        VERIFY(res == u8"13:33");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'E');
        VERIFY(res == u8"%ER");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'O');
        VERIFY(res == u8"%OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r');
        VERIFY(res == u8"午後01時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'E');
        VERIFY(res == u8"%Er");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'O');
        VERIFY(res == u8"%Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S');
        VERIFY(res == u8"18");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'E');
        VERIFY(res == u8"%ES");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'O');
        VERIFY(res == u8"十八");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X');
        VERIFY(res == u8"13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'E');
        VERIFY(res == u8"13時33分18秒");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'O');
        VERIFY(res == u8"%OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T');
        VERIFY(res == u8"13:33:18");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'E');
        VERIFY(res == u8"%ET");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'O');
        VERIFY(res == u8"%OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8't');
        VERIFY(res == u8"\t");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'E');
        VERIFY(res == u8"%Et");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'O');
        VERIFY(res == u8"%Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u');
        VERIFY(res == u8"3");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'E');
        VERIFY(res == u8"%Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'O');
        VERIFY(res == u8"三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U');
        VERIFY(res == u8"35");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'E');
        VERIFY(res == u8"%EU");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'O');
        VERIFY(res == u8"三十五");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V');
        VERIFY(res == u8"36");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'E');
        VERIFY(res == u8"%EV");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'O');
        VERIFY(res == u8"三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g');
        VERIFY(res == u8"24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'E');
        VERIFY(res == u8"%Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'O');
        VERIFY(res == u8"%Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G');
        VERIFY(res == u8"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'E');
        VERIFY(res == u8"%EG");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'O');
        VERIFY(res == u8"%OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W');
        VERIFY(res == u8"36");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'E');
        VERIFY(res == u8"%EW");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'O');
        VERIFY(res == u8"三十六");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w');
        VERIFY(res == u8"3");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'E');
        VERIFY(res == u8"%Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'O');
        VERIFY(res == u8"三");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y');
        VERIFY(res == u8"2024");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'E');
        VERIFY(res == u8"令和6年");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'O');
        VERIFY(res == u8"%OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y');
        VERIFY(res == u8"24");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'E');
        VERIFY(res == u8"6");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'O');
        VERIFY(res == u8"二十四");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z'); VERIFY(res == u8"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'E');
        VERIFY(res == u8"%EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'O');
        VERIFY(res == u8"%OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z'); VERIFY(res == u8"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'E');
        VERIFY(res == u8"%Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'O');
        VERIFY(res == u8"%Oz");
    }
    dump_info("Done\n");
}

namespace
{
    constexpr static IOv2::ios_defs::iostate febit = IOv2::ios_defs::eofbit | IOv2::ios_defs::strfailbit;

    template <typename T = IOv2::time_parse_context<char8_t>, bool HaveDate = true, bool HaveTime = true, bool HaveTimeZone = true>
    T CheckGet(const IOv2::timeio<char8_t>& obj, const std::u8string& input,
               char8_t fmt, char8_t modif,
               IOv2::ios_defs::iostate err_exp, size_t consume_exp = (size_t)-1)
    {
        if (consume_exp == (size_t)-1) consume_exp = input.size();
        IOv2::time_parse_context<char8_t, HaveDate, HaveTime, HaveTimeZone> ctx1, ctx2, ctx3;
        if (err_exp == IOv2::ios_defs::goodbit)
        {
            VERIFY(obj.get(input.begin(), input.end(), ctx1, fmt, modif) != input.end());
            {
                std::list<char8_t> lst_input(input.begin(), input.end());
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
                std::list<char8_t> lst_input(input.begin(), input.end());
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
                std::list<char8_t> lst_input(input.begin(), input.end());
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

    template <typename T = IOv2::time_parse_context<char8_t>, bool HaveDate = true, bool HaveTime = true, bool HaveTimeZone = true>
    T CheckGet(const IOv2::timeio<char8_t>& obj, const std::u8string& input,
               const std::u8string& fmt,
               IOv2::ios_defs::iostate err_exp, size_t consume_exp = (size_t)-1)
    {
        if (consume_exp == (size_t)-1) consume_exp = input.size();
        IOv2::time_parse_context<char8_t, HaveDate, HaveTime, HaveTimeZone> ctx1, ctx2, ctx3;
        if (err_exp == IOv2::ios_defs::goodbit)
        {
            VERIFY(obj.get(input.begin(), input.end(), ctx1, fmt) != input.end());
            {
                std::list<char8_t> lst_input(input.begin(), input.end());
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
                std::list<char8_t> lst_input(input.begin(), input.end());
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
                std::list<char8_t> lst_input(input.begin(), input.end());
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

void test_timeio_char8_t_get_1()
{
    dump_info("Test timeio<char8_t> get 1...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("C"));
    CheckGet(obj, u8"%",   u8'%',  0,  IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"x",   u8'%',  0,  IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%",   u8'%', u8'E', febit);
    CheckGet(obj, u8"%E%", u8'%', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"%",   u8'%', u8'O', febit);
    CheckGet(obj, u8"%O%", u8'%', u8'O', IOv2::ios_defs::eofbit);

    VERIFY(CheckGet(obj, u8"Wed", u8'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, u8"%Ea", u8'a', u8'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, u8"a",   u8'a', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oa", u8'a', u8'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, u8"a",   u8'a', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"Wednesday", u8'A', 0, IOv2::ios_defs::eofbit, 9).m_wday == 3);
    CheckGet(obj, u8"%EA", u8'A', u8'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, u8"A",   u8'A', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OA", u8'A', u8'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, u8"A",   u8'A', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"Sep", u8'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, u8"%Eb", u8'b', u8'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, u8"b",   u8'b', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Ob", u8'b', u8'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, u8"b",   u8'b', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"September", u8'B', 0, IOv2::ios_defs::eofbit, 9).m_month == 9);
    CheckGet(obj, u8"%EB", u8'B', u8'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, u8"B",   u8'B', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OB", u8'B', u8'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, u8"B",   u8'B', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"Sep", u8'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, u8"%Eh", u8'h', u8'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, u8"h",   u8'h', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oh", u8'h', u8'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, u8"h",   u8'h', u8'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(CheckGet<year_month_day>(obj, u8"09/04/24 13:33:18 America/Los_Angeles", u8'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"09/04/24 13:33:18 America/Los_Angeles", u8'c', u8'E', IOv2::ios_defs::eofbit, 17) == check_date1);
    CheckGet(obj, u8"c",   u8'c', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oc", u8'c', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"c",   u8'c', u8'O', IOv2::ios_defs::strfailbit, 0);


    VERIFY(CheckGet(obj, u8"20", u8'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(CheckGet(obj, u8"20", u8'C', u8'E', IOv2::ios_defs::eofbit).m_century == 20);
    CheckGet(obj, u8"C",   u8'C', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OC", u8'C', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"C",   u8'C', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"04", u8'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, u8"04", u8'd', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, u8"%Ed", u8'd', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"d",   u8'd', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"d",   u8'd', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"4", u8'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, u8"4", u8'e', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, u8"%Ee", u8'e', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"e",   u8'e', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"e",   u8'e', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"2024-09-04", u8'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"%EF", u8'F', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"F",   u8'F', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OF", u8'F', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"F",   u8'F', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"09/04/24", u8'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"09/04/24", u8'x', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"x",   u8'x', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Ox", u8'x', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"x",   u8'x', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"09/04/24", u8'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"%ED", u8'D', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"D",   u8'D', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OD", u8'D', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"D",   u8'D', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"13", u8'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, u8"13", u8'H', u8'O', IOv2::ios_defs::eofbit).m_hour == 13);
    CheckGet(obj, u8"%EH", u8'H', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"H",   u8'H', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"H",   u8'H', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"01", u8'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, u8"01", u8'I', u8'O', IOv2::ios_defs::eofbit).m_hour == 1);
    CheckGet(obj, u8"%EI", u8'I', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"I",   u8'I', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"I",   u8'I', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"248", u8'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024 248", u8"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"%Ej", u8'j', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"j",   u8'j', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oj", u8'j', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"j",   u8'j', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"09", u8'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, u8"09", u8'm', u8'O', IOv2::ios_defs::eofbit).m_month == 9);
    CheckGet(obj, u8"%Em", u8'm', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"m",   u8'm', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"m",   u8'm', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"33", u8'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, u8"33", u8'M', u8'O', IOv2::ios_defs::eofbit).m_minute == 33);
    CheckGet(obj, u8"%EM", u8'M', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"M",   u8'M', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"M",   u8'M', u8'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, u8"\n",   u8'n',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, u8"x",    u8'n',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, u8"\n",   u8'n', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%En",  u8'n', u8'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, u8"n",    u8'n', u8'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%On",  u8'n', u8'O', IOv2::ios_defs::eofbit, 3);

    CheckGet(obj, u8"\t",   u8't',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, u8"x",    u8't',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, u8"\t",   u8't', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Et",  u8't', u8'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, u8"n",    u8't', u8'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Ot",  u8't', u8'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 PM", u8"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 AM", u8"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(CheckGet(obj, u8"PM", u8'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(CheckGet(obj, u8"AM", u8'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    CheckGet(obj, u8"%Ep", u8'p', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"p",   u8'p', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Op", u8'p', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"p",   u8'p', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01:33:18 PM", u8"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"%Er", u8'r', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"r",   u8'r', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Or", u8'r', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"r",   u8'r', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33", u8"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, u8"%ER", u8'R', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"R",   u8'R', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OR", u8'R', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"R",   u8'R', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"18", u8'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, u8"18", u8'S', u8'O', IOv2::ios_defs::eofbit).m_second == 18);
    CheckGet(obj, u8"%ES", u8'S', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"S",   u8'S', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"S",   u8'S', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33:18 America/Los_Angeles", u8"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33:18 America/Los_Angeles", u8"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"X",   u8'X', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OX", u8'X', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"X",   u8'X', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33:18", u8"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"%ET", u8'T', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"T",   u8'T', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OT", u8'T', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"T",   u8'T', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"3", u8'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, u8"3", u8'u', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, u8"%Eu", u8'u', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"u",   u8'u', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"u",   u8'u', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"24", u8'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, u8"%Eg", u8'g', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"g",   u8'g', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Og", u8'g', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"g",   u8'g', u8'O', IOv2::ios_defs::strfailbit, 0);


    VERIFY(CheckGet(obj, u8"2024", u8'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, u8"%EG", u8'G', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"G",   u8'G', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OG", u8'G', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"G",   u8'G', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"2024 35 Wed", u8"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024 35 Wed", u8"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, u8"35", u8'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(CheckGet(obj, u8"35", u8'U', u8'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    CheckGet(obj, u8"%EU", u8'U', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"u8",   u8'U', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"u8",   u8'U', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"2024 36 Wed", u8"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024 36 Wed", u8"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, u8"36", u8'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(CheckGet(obj, u8"36", u8'W', u8'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    CheckGet(obj, u8"%EW", u8'W', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"W",   u8'W', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"W",   u8'W', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"36", u8'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    CheckGet(obj, u8"54",  u8'V', u8'O', IOv2::ios_defs::strfailbit, 1);
    CheckGet(obj, u8"36",  u8'V', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"%EV", u8'V', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"V",   u8'V', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"V",   u8'V', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"3", u8'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, u8"3", u8'w', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, u8"%Ew", u8'w', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"w",   u8'w', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"w",   u8'w', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"24", u8'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, u8"24", u8'y', u8'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, u8"24", u8'y', u8'O', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, u8"y",  u8'y', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"y",  u8'y', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"2024", u8'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, u8"2024", u8'Y', u8'E', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, u8"Y",   u8'Y', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OY", u8'Y', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"Y",   u8'Y', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"America/Los_Angeles", u8'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    VERIFY(CheckGet(obj, u8"PST", u8'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
    CheckGet(obj, u8"America/Los_Angexes", u8'Z', 0, IOv2::ios_defs::strfailbit);
    CheckGet(obj, u8"%EZ", u8'Z', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"Z",   u8'Z', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OZ", u8'Z', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"Z",   u8'Z', u8'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, u8"Z", u8'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"+13", u8'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"-1110", u8'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"+11:10", u8'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"%Ez", 'z', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"z",  'z', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oz", 'z', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"z",  'z', u8'O', IOv2::ios_defs::strfailbit, 0);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(CheckGet<year_month_day>(obj, u8"1999-W52-6", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, u8"2019-W01-1", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, u8"1999-W52-5", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, u8"99-W52-6", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, u8"19-W01-1", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, u8"99-W52-5", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, u8"20 24/09/04", u8"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);

    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(CheckGet<year_month_day>(obj, u8"20 01 01", u8"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }
    dump_info("Done\n");
}

void test_timeio_char8_t_get_2()
{
    dump_info("Test timeio<char8_t> get 2...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("zh_CN.UTF-8"));

    CheckGet(obj, u8"%",  u8'%',  0,  IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"x",  u8'%',  0,  IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%",  u8'%', u8'E', febit);
    CheckGet(obj, u8"%E%", u8'%', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"%",  u8'%', u8'O', febit);
    CheckGet(obj, u8"%O%", u8'%', u8'O', IOv2::ios_defs::eofbit);

    VERIFY(CheckGet(obj, u8"三", u8'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, u8"%Ea", u8'a', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"a",   u8'a', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oa", u8'a', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"a",   u8'a', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"星期三", u8'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, u8"%EA", u8'A', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"A",   u8'A', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OA", u8'A', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"A",   u8'A', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"九月", u8'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, u8"%Eb", u8'b', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"b",   u8'b', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Ob", u8'b', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"b",   u8'b', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"九月", u8'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, u8"%EB", u8'B', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"B",   u8'B', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OB", u8'B', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"B",   u8'B', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"九月", u8'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, u8"%Eh", u8'h', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"h",   u8'h', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oh", u8'h', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"h",   u8'h', u8'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(CheckGet<year_month_day>(obj, u8"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles", u8'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles", u8'c', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"c",   u8'c', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oc", u8'c', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"c",   u8'c', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"20", u8'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(CheckGet(obj, u8"20", u8'C', u8'E', IOv2::ios_defs::eofbit).m_century == 20);
    CheckGet(obj, u8"C",   u8'C', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OC", u8'C', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"C",   u8'C', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"04", u8'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, u8"04", u8'd', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, u8"%Ed", u8'd', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"d",   u8'd', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"d",   u8'd', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"4", u8'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, u8"4", u8'e', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, u8"%Ee", u8'e', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"e",   u8'e', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"e",   u8'e', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"2024-09-04", u8'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"%EF", u8'F', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"F",   u8'F', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OF", u8'F', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"F",   u8'F', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"2024年09月04日", u8'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024年09月04日", u8'x', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"x",   u8'x', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Ox", u8'x', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"x",   u8'x', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"09/04/24", u8'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"%ED", u8'D', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"D",   u8'D', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OD", u8'D', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"D",   u8'D', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"13", u8'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, u8"13", u8'H', u8'O', IOv2::ios_defs::eofbit).m_hour == 13);
    CheckGet(obj, u8"%EH", u8'H', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"H",   u8'H', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"H",   u8'H', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"01", u8'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, u8"01", u8'I', u8'O', IOv2::ios_defs::eofbit).m_hour == 1);
    CheckGet(obj, u8"%EI", u8'I', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"I",   u8'I', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"I",   u8'I', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"248", u8'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024 248", u8"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"%Ej", u8'j', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"j",   u8'j', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oj", u8'j', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"j",   u8'j', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"09", u8'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, u8"09", u8'm', u8'O', IOv2::ios_defs::eofbit).m_month == 9);
    CheckGet(obj, u8"%Em", u8'm', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"m",   u8'm', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"m",   u8'm', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"33", u8'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, u8"33", u8'M', u8'O', IOv2::ios_defs::eofbit).m_minute == 33);
    CheckGet(obj, u8"%EM", u8'M', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"M",   u8'M', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"M",   u8'M', u8'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, u8"\n",   u8'n',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, u8"x",    u8'n',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, u8"\n",   u8'n', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%En",  u8'n', u8'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, u8"n",    u8'n', u8'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%On",  u8'n', u8'O', IOv2::ios_defs::eofbit, 3);

    CheckGet(obj, u8"\t",   u8't',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, u8"x",    u8't',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, u8"\t",   u8't', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Et",  u8't', u8'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, u8"n",    u8't', u8'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Ot",  u8't', u8'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 下午", u8"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 上午", u8"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(CheckGet(obj, u8"下午", u8'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(CheckGet(obj, u8"上午", u8'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    CheckGet(obj, u8"%Ep", u8'p', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"p",   u8'p', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Op", u8'p', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"p",   u8'p', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"下午 01时33分18秒", u8"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"%Er", u8'r', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"r",   u8'r', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Or", u8'r', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"r",   u8'r', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33", u8"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, u8"%ER", u8'R', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"R",   u8'R', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OR", u8'R', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"R",   u8'R', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"18", u8'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, u8"18", u8'S', u8'O', IOv2::ios_defs::eofbit).m_second == 18);
    CheckGet(obj, u8"%ES", u8'S', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"S",   u8'S', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"S",   u8'S', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13时33分18秒 America/Los_Angeles", u8"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13时33分18秒 America/Los_Angeles", u8"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"X",   u8'X', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OX", u8'X', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"X",   u8'X', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33:18", u8"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"%ET", u8'T', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"T",   u8'T', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OT", u8'T', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"T",   u8'T', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"3", u8'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, u8"3", u8'u', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, u8"%Eu", u8'u', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"u",   u8'u', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"u",   u8'u', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"24", u8'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, u8"%Eg", u8'g', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"g",   u8'g', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Og", u8'g', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"g",   u8'g', u8'O', IOv2::ios_defs::strfailbit, 0);


    VERIFY(CheckGet(obj, u8"2024", u8'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, u8"%EG", u8'G', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"G",   u8'G', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OG", u8'G', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"G",   u8'G', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"2024 35 三", u8"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024 35 三", u8"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, u8"35", u8'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(CheckGet(obj, u8"35", u8'U', u8'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    CheckGet(obj, u8"%EU", u8'U', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"u8",   u8'U', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"u8",   u8'U', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"2024 36 三", u8"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024 36 三", u8"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, u8"36", u8'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(CheckGet(obj, u8"36", u8'W', u8'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    CheckGet(obj, u8"%EW", u8'W', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"W",   u8'W', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"W",   u8'W', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"36", u8'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    CheckGet(obj, u8"54",  u8'V', u8'O', IOv2::ios_defs::strfailbit, 1);
    CheckGet(obj, u8"36",  u8'V', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"%EV", u8'V', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"V",   u8'V', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"V",   u8'V', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"3", u8'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, u8"3", u8'w', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, u8"%Ew", u8'w', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"w",   u8'w', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"w",   u8'w', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"24", u8'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, u8"24", u8'y', u8'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, u8"24", u8'y', u8'O', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, u8"y",  u8'y', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"y",  u8'y', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"2024", u8'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, u8"2024", u8'Y', u8'E', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, u8"Y",   u8'Y', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OY", u8'Y', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"Y",   u8'Y', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"America/Los_Angeles", u8'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    VERIFY(CheckGet(obj, u8"PST", u8'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
    CheckGet(obj, u8"America/Los_Angexes", u8'Z', 0, IOv2::ios_defs::strfailbit);
    CheckGet(obj, u8"%EZ", u8'Z', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"Z",   u8'Z', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OZ", u8'Z', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"Z",   u8'Z', u8'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, u8"Z", u8'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"+13", u8'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"-1110", u8'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"+11:10", u8'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"%Ez", u8'z', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"z",  u8'z', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oz", u8'z', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"z",  u8'z', u8'O', IOv2::ios_defs::strfailbit, 0);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(CheckGet<year_month_day>(obj, u8"1999-W52-6", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, u8"2019-W01-1", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, u8"1999-W52-5", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, u8"99-W52-6", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, u8"19-W01-1", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, u8"99-W52-5", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, u8"20 24/09/04", u8"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(CheckGet<year_month_day>(obj, u8"20 01 01", u8"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_get_3()
{
    dump_info("Test timeio<char8_t> get 3...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("ja_JP.UTF-8"));

    CheckGet(obj, u8"%",  u8'%',  0,  IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"x",  u8'%',  0,  IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%",  u8'%', u8'E', febit);
    CheckGet(obj, u8"%E%", u8'%', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"%",  u8'%', u8'O', febit);
    CheckGet(obj, u8"%O%", u8'%', u8'O', IOv2::ios_defs::eofbit);

    VERIFY(CheckGet(obj, u8"水", u8'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, u8"%Ea", u8'a', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"a",   u8'a', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oa", u8'a', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"a",   u8'a', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"水曜日", u8'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, u8"%EA", u8'A', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"A",   u8'A', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OA", u8'A', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"A",   u8'A', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"9月", u8'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, u8"%Eb", u8'b', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"b",   u8'b', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Ob", u8'b', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"b",   u8'b', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"9月", u8'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, u8"%EB", u8'B', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"B",   u8'B', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OB", u8'B', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"B",   u8'B', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"9月", u8'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, u8"%Eh", u8'h', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"h",   u8'h', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oh", u8'h', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"h",   u8'h', u8'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(CheckGet<year_month_day>(obj, u8"2024年09月04日 13時33分18秒 America/Los_Angeles", u8'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"令和6年09月04日 13時33分18秒 America/Los_Angeles", u8'c', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"202409月04日 13時33分18秒 America/Los_Angeles", u8'c', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"c",   u8'c', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oc", u8'c', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"c",   u8'c', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"20", 'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(CheckGet<year_month_day>(obj, u8"平成", u8'C', u8'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1990));
    CheckGet(obj, u8"C",   u8'C', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OC", u8'C', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"C",   u8'C', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"04", u8'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, u8"04", u8'd', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, u8"四", u8'd', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, u8"%Ed", u8'd', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"d",   u8'd', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"d",   u8'd', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"4", u8'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, u8"4", u8'e', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, u8"四", u8'e', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, u8"%Ee", u8'e', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"e",   u8'e', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"e",   u8'e', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"2024-09-04", u8'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"%EF", u8'F', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"F",   u8'F', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OF", u8'F', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"F",   u8'F', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"2024年09月04日", u8'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"令和6年09月04日", u8'x', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"202409月04日", u8'x', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"x",   u8'x', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Ox", u8'x', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"x",   u8'x', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"09/04/24", u8'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"%ED", u8'D', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"D",   u8'D', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OD", u8'D', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"D",   u8'D', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"13", u8'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, u8"13", u8'H', u8'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, u8"十三", u8'H', u8'O', IOv2::ios_defs::eofbit).m_hour == 13);
    CheckGet(obj, u8"%EH", u8'H', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"H",   u8'H', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"H",   u8'H', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"01", u8'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, u8"01", u8'I', u8'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, u8"一", u8'I', u8'O', IOv2::ios_defs::eofbit).m_hour == 1);
    CheckGet(obj, u8"%EI", u8'I', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"I",   u8'I', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"I",   u8'I', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"248", u8'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024 248", u8"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, u8"%Ej", u8'j', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"j",   u8'j', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oj", u8'j', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"j",   u8'j', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"09", u8'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, u8"09", u8'm', u8'O', IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, u8"九", u8'm', u8'O', IOv2::ios_defs::eofbit).m_month == 9);
    CheckGet(obj, u8"%Em", u8'm', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"m",   u8'm', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"m",   u8'm', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"33", u8'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, u8"33", u8'M', u8'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, u8"三十三", u8'M', u8'O', IOv2::ios_defs::eofbit).m_minute == 33);
    CheckGet(obj, u8"%EM", u8'M', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"M",   u8'M', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"M",   u8'M', u8'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, u8"\n",   u8'n',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, u8"x",    u8'n',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, u8"\n",   u8'n', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%En",  u8'n', u8'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, u8"n",    u8'n', u8'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%On",  u8'n', u8'O', IOv2::ios_defs::eofbit, 3);

    CheckGet(obj, u8"\t",   u8't',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, u8"x",    u8't',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, u8"\t",   u8't', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Et",  u8't', u8'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, u8"n",    u8't', u8'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Ot",  u8't', u8'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 午後", u8"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 午前", u8"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(CheckGet(obj, u8"午後", u8'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(CheckGet(obj, u8"午前", u8'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    CheckGet(obj, u8"%Ep", u8'p', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"p",   u8'p', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Op", u8'p', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"p",   u8'p', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"午後01時33分18秒", u8"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"%Er", u8'r', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"r",   u8'r', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Or", u8'r', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"r",   u8'r', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33", u8"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, u8"%ER", u8'R', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"R",   u8'R', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OR", u8'R', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"R",   u8'R', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"18", u8'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, u8"18", u8'S', u8'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, u8"十八", u8'S', u8'O', IOv2::ios_defs::eofbit).m_second == 18);
    CheckGet(obj, u8"%ES", u8'S', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"S",   u8'S', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"S",   u8'S', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13時33分18秒 America/Los_Angeles", u8"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13時33分18秒 America/Los_Angeles", u8"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"X",   u8'X', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OX", u8'X', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"X",   u8'X', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33:18", u8"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, u8"%ET", u8'T', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"T",   u8'T', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OT", u8'T', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"T",   u8'T', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"3", u8'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, u8"3", u8'u', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, u8"三", u8'u', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, u8"%Eu", u8'u', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"u",   u8'u', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"u",   u8'u', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"24", u8'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, u8"%Eg", u8'g', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"g",   u8'g', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Og", u8'g', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"g",   u8'g', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"2024", u8'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, u8"%EG", u8'G', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"G",   u8'G', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OG", u8'G', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"G",   u8'G', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"2024 35 水", u8"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024 35 水", u8"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024 三十五 水", u8"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, u8"35", u8'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(CheckGet(obj, u8"35", u8'U', u8'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    CheckGet(obj, u8"%EU", u8'U', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"u8",   u8'U', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"u8",   u8'U', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, u8"2024 36 水", u8"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024 36 水", u8"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, u8"2024 三十六 水", u8"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, u8"36", u8'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(CheckGet(obj, u8"36", u8'W', u8'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    CheckGet(obj, u8"%EW", u8'W', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"W",   u8'W', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"W",   u8'W', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"36", u8'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(CheckGet(obj, u8"36", u8'V', u8'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(CheckGet(obj, u8"三十六", 'V', u8'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    CheckGet(obj, u8"54",  u8'V', u8'O', IOv2::ios_defs::strfailbit, 1);
    CheckGet(obj, u8"%EV", u8'V', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"V",   u8'V', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"V",   u8'V', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"3", u8'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, u8"3", u8'w', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, u8"三", u8'w', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, u8"%Ew", u8'w', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"w",   u8'w', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"w",   u8'w', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"24", u8'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet<year_month_day>(obj, u8"6", u8'y', u8'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(2024));
    VERIFY(CheckGet(obj, u8"24", u8'y', u8'O', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, u8"二十四", u8'y', u8'O', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, u8"y",  u8'y', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"y",  u8'y', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"2024", u8'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, u8"2024", u8'Y', u8'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet<year_month_day>(obj, u8"平成3年", u8'Y', u8'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1991));
    CheckGet(obj, u8"Y",   u8'Y', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OY", u8'Y', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"Y",   u8'Y', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, u8"America/Los_Angeles", u8'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    VERIFY(CheckGet(obj, u8"PST", u8'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
    CheckGet(obj, u8"America/Los_Angexes", u8'Z', 0, IOv2::ios_defs::strfailbit);
    CheckGet(obj, u8"%EZ", u8'Z', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"Z",   u8'Z', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%OZ", u8'Z', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"Z",   u8'Z', u8'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, u8"Z", u8'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"+13", u8'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"-1110", u8'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"+11:10", u8'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"%Ez", u8'z', u8'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"z",  u8'z', u8'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, u8"%Oz", u8'z', u8'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, u8"z",  u8'z', u8'O', IOv2::ios_defs::strfailbit, 0);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(CheckGet<year_month_day>(obj, u8"1999-W52-6", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, u8"2019-W01-1", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, u8"1999-W52-5", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, u8"99-W52-6", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, u8"19-W01-1", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, u8"99-W52-5", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, u8"20 24/09/04", u8"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(CheckGet<year_month_day>(obj, u8"20 01 01", u8"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_get_4()
{
    dump_info("Test timeio<char8_t> get 4...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("C"));
    {
        std::u8string input = u8"d 2014-04-14 01:09:35";
        std::u8string format = u8"d %Y-%m-%d %H:%M:%S";
        
        IOv2::time_parse_context<char8_t> ctx;
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
        std::u8string input = u8"2020  ";
        std::u8string format = u8"%Y";
        
        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret != input.end());
        VERIFY(time.tm_year == 120);
    }

    {
        std::u8string input = u8"2014-04-14 01:09:35";
        std::u8string format = u8"%";
        
        IOv2::time_parse_context<char8_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::u8string input = u8"2020";
        
        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, 'Y');
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_year == 120);
        VERIFY(ret == input.end());
    }

    {
        std::u8string input = u8"year: 1970";
        std::u8string format = u8"jahr: %Y";
        
        IOv2::time_parse_context<char8_t> ctx;
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

void test_timeio_char8_t_get_5()
{
    dump_info("Test timeio<char8_t> get 5...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("de_DE.UTF-8"));
    {
        std::u8string input = u8"Montag, den 14. April 2014";
        std::u8string format = u8"%A, den %d. %B %Y";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 114);
        VERIFY(time.tm_mon == 3);
        VERIFY(time.tm_wday == 1);
        VERIFY(time.tm_mday == 14);
    }
    {
        std::u8string input = u8"Mittwoch";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, 'A');
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 3);
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_get_6()
{
    dump_info("Test timeio<char8_t> get 6...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("C"));
    {
        std::u8string input = u8"Mon";
        std::u8string format = u8"%a";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 1);
    }

    {
        std::u8string input = u8"Tue ";
        std::u8string format = u8"%a";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_wday == 2);
    }

    {
        std::u8string input = u8"Wednesday";
        std::u8string format = u8"%a";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 3);
    }

    {
        std::u8string input = u8"Thu";
        std::u8string format = u8"%A";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 4);
    }

    {
        std::u8string input = u8"Fri ";
        std::u8string format = u8"%A";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_wday == 5);
    }

    {
        std::u8string input = u8"Saturday";
        std::u8string format = u8"%A";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 6);
    }

    {
        std::u8string input = u8"Feb";
        std::u8string format = u8"%b";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 1);
    }

    {
        std::u8string input = u8"Mar ";
        std::u8string format = u8"%b";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_mon == 2);
    }

    {
        std::u8string input = u8"April";
        std::u8string format = u8"%b";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 3);
    }

    {
        std::u8string input = u8"May";
        std::u8string format = u8"%B";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 4);
    }

    {
        std::u8string input = u8"Jun ";
        std::u8string format = u8"%B";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_mon == 5);
    }

    {
        std::u8string input = u8"July";
        std::u8string format = u8"%B";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 6);
    }

    {
        std::u8string input = u8"Aug";
        std::u8string format = u8"%h";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 7);
    }

    {
        std::u8string input = u8"May ";
        std::u8string format = u8"%h";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_mon == 4);
    }

    {
        std::u8string input = u8"October";
        std::u8string format = u8"%h";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 9);
    }

    // Other tests.
    {
        std::u8string input = u8"2.";
        std::u8string format = u8"%d.";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mday == 2);
    }

    {
        std::u8string input = u8"0.";
        std::u8string format = u8"%d.";

        IOv2::time_parse_context<char8_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::u8string input = u8"32.";
        std::u8string format = u8"%d.";

        IOv2::time_parse_context<char8_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::u8string input = u8"5.";
        std::u8string format = u8"%e.";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_mday == 5);
    }

    {
        std::u8string input = u8"06.";
        std::u8string format = u8"%e.";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_mday == 6);
    }

    {
        std::u8string input = u8"0";
        std::u8string format = u8"%e";

        IOv2::time_parse_context<char8_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::u8string input = u8"35";
        std::u8string format = u8"%e";

        IOv2::time_parse_context<char8_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::u8string input = u8"12:00AM";
        std::u8string format = u8"%I:%M%p";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 0);
        VERIFY(time.tm_min == 0);
    }

    {
        std::u8string input = u8"12:37AM";
        std::u8string format = u8"%I:%M%p";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 0);
        VERIFY(time.tm_min == 37);
    }

    {
        std::u8string input = u8"01:25AM";
        std::u8string format = u8"%I:%M%p";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 1);
        VERIFY(time.tm_min == 25);
    }

    {
        std::u8string input = u8"12:00PM";
        std::u8string format = u8"%I:%M%p";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 12);
        VERIFY(time.tm_min == 0);
    }

    {
        std::u8string input = u8"12:42PM";
        std::u8string format = u8"%I:%M%p";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 12);
        VERIFY(time.tm_min == 42);
    }

    {
        std::u8string input = u8"07:23PM";
        std::u8string format = u8"%I:%M%p";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 19);
        VERIFY(time.tm_min == 23);
    }

    {
        std::u8string input = u8"17%20";
        std::u8string format = u8"%H%%%M";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 17);
        VERIFY(time.tm_min == 20);
    }

    {
        std::u8string input = u8"24:30";
        std::u8string format = u8"%H:%M";

        IOv2::time_parse_context<char8_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::u8string input = u8"Novembur";
        std::u8string format = u8"%bembur";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_mon == 10);
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_get_7()
{
    dump_info("Test timeio<char8_t> get 7...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("C"));
    {
        std::u8string input = u8"PM01:38:12";
        std::u8string format = u8"%p%I:%M:%S";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_hour == 13);
        VERIFY(time.tm_min == 38);
        VERIFY(time.tm_sec == 12);
    }

    {
        std::u8string input = u8"05 37";
        std::u8string format = u8"%C %y";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 537 - 1900);
    }

    {
        std::u8string input = u8"68";
        std::u8string format = u8"%y";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2068 - 1900);
    }

    {
        std::u8string input = u8"69";
        std::u8string format = u8"%y";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 1969 - 1900);
    }

    {
        std::u8string input = u8"03-Feb-2003";
        std::u8string format = u8"%d-%b-%Y";

        IOv2::time_parse_context<char8_t> ctx;
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
        std::u8string input = u8"16-Dec-2020";
        std::u8string format = u8"%d-%b-%Y";

        IOv2::time_parse_context<char8_t> ctx;
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
        std::u8string input = u8"16-Dec-2021";
        std::u8string format = u8"%d-%b-%Y";

        IOv2::time_parse_context<char8_t> ctx;
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
        std::u8string input = u8"253 2020";
        std::u8string format = u8"%j %Y";

        IOv2::time_parse_context<char8_t> ctx;
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
        std::u8string input = u8"233 2021";
        std::u8string format = u8"%j %Y";

        IOv2::time_parse_context<char8_t> ctx;
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
        std::u8string input = u8"2020 23 3";
        std::u8string format = u8"%Y %U %w";

        IOv2::time_parse_context<char8_t> ctx;
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
        std::u8string input = u8"2020 23 3";
        std::u8string format = u8"%Y %W %w";

        IOv2::time_parse_context<char8_t> ctx;
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
        std::u8string input = u8"2021 43 Fri";
        std::u8string format = u8"%Y %W %a";

        IOv2::time_parse_context<char8_t> ctx;
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
        std::u8string input = u8"2024 23 3";
        std::u8string format = u8"%Y %U %w";

        IOv2::time_parse_context<char8_t> ctx;
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
        std::u8string input = u8"2024 23 3";
        std::u8string format = u8"%Y %W %w";

        IOv2::time_parse_context<char8_t> ctx;
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

void test_timeio_char8_t_get_8()
{
    dump_info("Test timeio<char8_t> get 8...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("C"));
    {
        std::u8string input = u8"01:38:12 PM";
        std::u8string format = u8"%I:%M:%S %p";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_hour == 13);
        VERIFY(time.tm_min == 38);
        VERIFY(time.tm_sec == 12);
    }
        
    {
        std::u8string input = u8"11:17:42 PM";
        std::u8string format = u8"%r";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_hour == 23);
        VERIFY(time.tm_min == 17);
        VERIFY(time.tm_sec == 42);
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_get_9()
{
    dump_info("Test timeio<char8_t> get 9...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char8_t, true, true, false>, true, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(u8"%",  u8'%',  0,  IOv2::ios_defs::eofbit);
    FOri(u8"x",  u8'%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%",  u8'%', u8'E', febit);
    FOri(u8"%E%", u8'%', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"%",  u8'%', u8'O', febit);
    FOri(u8"%O%", u8'%', u8'O', IOv2::ios_defs::eofbit);

    VERIFY(FOri(u8"水", u8'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri(u8"%Ea", u8'a', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oa", u8'a', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"水曜日", u8'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri(u8"%EA", u8'A', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OA", u8'A', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"9月", u8'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(u8"%Eb", u8'b', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Ob", u8'b', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"9月", u8'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(u8"%EB", u8'B', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OB", u8'B', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"9月", u8'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(u8"%Eh", u8'h', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oh", u8'h', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(FYmd(u8"2024年09月04日 13時33分18秒", u8'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"令和6年09月04日 13時33分18秒", u8'c', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"202409月04日 13時33分18秒", u8'c', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    FOri(u8"c",   u8'c', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oc", u8'c', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"20", u8'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(FYmd(u8"平成", u8'C', u8'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1990));
    FOri(u8"C",   u8'C', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OC", u8'C', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"C",   u8'C', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"04", u8'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(u8"04", u8'd', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(u8"四", u8'd', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri(u8"%Ed", u8'd', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"d",   u8'd', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"d",   u8'd', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"4", u8'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(u8"4", u8'e', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(u8"四", u8'e', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri(u8"%Ee", u8'e', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"e",   u8'e', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"e",   u8'e', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(u8"2024-09-04", u8'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri(u8"%EF", u8'F', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OF", u8'F', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(u8"2024年09月04日", u8'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"令和6年09月04日", u8'x', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"202409月04日", u8'x', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    FOri(u8"x",   u8'x', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Ox", u8'x', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"x",   u8'x', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(u8"09/04/24", u8'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri(u8"%ED", u8'D', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OD", u8'D', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"13", u8'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(u8"13", u8'H', u8'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(u8"十三", u8'H', u8'O', IOv2::ios_defs::eofbit).m_hour == 13);
    FOri(u8"%EH", u8'H', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"H",   u8'H', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"H",   u8'H', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"01", u8'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(u8"01", u8'I', u8'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(u8"一", u8'I', u8'O', IOv2::ios_defs::eofbit).m_hour == 1);
    FOri(u8"%EI", u8'I', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"I",   u8'I', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"I",   u8'I', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"248", u8'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(FYmd(u8"2024 248", u8"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    FOri(u8"%Ej", u8'j', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oj", u8'j', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"09", u8'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri(u8"09", u8'm', u8'O', IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri(u8"九", u8'm', u8'O', IOv2::ios_defs::eofbit).m_month == 9);
    FOri(u8"%Em", u8'm', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"m",   u8'm', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"m",   u8'm', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"33", u8'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(u8"33", u8'M', u8'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(u8"三十三", u8'M', u8'O', IOv2::ios_defs::eofbit).m_minute == 33);
    FOri(u8"%EM", u8'M', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"M",   u8'M', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"M",   u8'M', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"\n",   u8'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(u8"x",    u8'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(u8"\n",   u8'n', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%En",  u8'n', u8'E', IOv2::ios_defs::eofbit, 3);
    FOri(u8"n",    u8'n', u8'O', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%On",  u8'n', u8'O', IOv2::ios_defs::eofbit, 3);

    FOri(u8"\t",   u8't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(u8"x",    u8't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(u8"\t",   u8't', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Et",  u8't', u8'E', IOv2::ios_defs::eofbit, 3);
    FOri(u8"n",    u8't', u8'O', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Ot",  u8't', u8'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 午後", u8"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"01 午前", u8"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(FOri(u8"午後", u8'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(FOri(u8"午前", u8'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    FOri(u8"%Ep", u8'p', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Op", u8'p', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"午後01時33分18秒", u8"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"%Er", u8'r', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Or", u8'r', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33", u8"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(u8"%ER", u8'R', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OR", u8'R', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"18", u8'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(u8"18", u8'S', u8'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(u8"十八", u8'S', u8'O', IOv2::ios_defs::eofbit).m_second == 18);
    FOri(u8"%ES", u8'S', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"S",   u8'S', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"S",   u8'S', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13時33分18秒 America/Los_Angeles", u8"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13時33分18秒 America/Los_Angeles", u8"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"X",   u8'X', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OX", u8'X', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"X",   u8'X', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33:18", u8"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"%ET", u8'T', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OT", u8'T', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"3", u8'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(u8"3", u8'u', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(u8"三", u8'u', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri(u8"%Eu", u8'u', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"u",   u8'u', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"u",   u8'u', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"24", u8'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri(u8"%Eg", u8'g', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Og", u8'g', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"2024", u8'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri(u8"%EG", u8'G', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OG", u8'G', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(u8"2024 35 水", u8"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"2024 35 水", u8"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"2024 三十五 水", u8"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri(u8"35", u8'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(FOri(u8"35", u8'U', u8'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    FOri(u8"%EU", u8'U', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"u8",   u8'U', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"u8",   u8'U', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(u8"2024 36 水", u8"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"2024 36 水", u8"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"2024 三十六 水", u8"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri(u8"36", u8'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(FOri(u8"36", u8'W', u8'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    FOri(u8"%EW", u8'W', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"W",   u8'W', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"W",   u8'W', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"36", u8'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri(u8"36", u8'V', u8'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri(u8"三十六", u8'V', u8'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    FOri(u8"54",  u8'V', u8'O', IOv2::ios_defs::strfailbit, 1);
    FOri(u8"%EV", u8'V', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"V",   u8'V', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"V",   u8'V', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"3", u8'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(u8"3", u8'w', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(u8"三", u8'w', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri(u8"%Ew", u8'w', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"w",   u8'w', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"w",   u8'w', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"24", u8'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd(u8"6", u8'y', u8'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(2024));
    VERIFY(FOri(u8"24", u8'y', u8'O', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri(u8"二十四", u8'y', u8'O', IOv2::ios_defs::eofbit).m_year == 2024);
    FOri(u8"y",  u8'y', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"y",  u8'y', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"2024", u8'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri(u8"2024", u8'Y', u8'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd(u8"平成3年", u8'Y', u8'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1991));
    FOri(u8"Y",   u8'Y', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OY", u8'Y', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"Y",   u8'Y', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%Z", u8'Z', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%EZ", u8'Z', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OZ", u8'Z', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%z", u8'z', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Ez", u8'z', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"%Oz", u8'z', u8'O', IOv2::ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(FYmd(u8"1999-W52-6", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd(u8"2019-W01-1", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd(u8"1999-W52-5", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd(u8"99-W52-6", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd(u8"19-W01-1", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd(u8"99-W52-5", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd(u8"20 24/09/04", u8"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(FYmd(u8"20 01 01", u8"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_get_10()
{
    dump_info("Test timeio<char8_t> get 10...");
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char8_t, true, false, false>, true, false, false>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, false, false>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(u8"%",  u8'%',  0,  IOv2::ios_defs::eofbit);
    FOri(u8"x",  u8'%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%",  u8'%', u8'E', febit);
    FOri(u8"%E%", u8'%', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"%",  u8'%', u8'O', febit);
    FOri(u8"%O%", u8'%', u8'O', IOv2::ios_defs::eofbit);

    VERIFY(FOri(u8"水", u8'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri(u8"%Ea", u8'a', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oa", u8'a', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"水曜日", u8'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri(u8"%EA", u8'A', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OA", u8'A', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"9月", u8'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(u8"%Eb", u8'b', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Ob", u8'b', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"9月", u8'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(u8"%EB", u8'B', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OB", u8'B', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"9月", u8'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(u8"%Eh", u8'h', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oh", u8'h', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    FYmd(u8"%c", u8'c', 0, IOv2::ios_defs::eofbit);
    FYmd(u8"%Ec", u8'c', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oc", u8'c', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"20", u8'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(FYmd(u8"平成", u8'C', u8'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1990));
    FOri(u8"C",   u8'C', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OC", u8'C', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"C",   u8'C', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"04", u8'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(u8"04", u8'd', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(u8"四", u8'd', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri(u8"%Ed", u8'd', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"d",   u8'd', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"d",   u8'd', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"4", u8'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(u8"4", u8'e', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(u8"四", u8'e', u8'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri(u8"%Ee", u8'e', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"e",   u8'e', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"e",   u8'e', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(u8"2024-09-04", u8'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri(u8"%EF", 'F', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"F",   'F', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OF", 'F', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"F",   'F', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(u8"2024年09月04日", u8'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"令和6年09月04日", u8'x', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"202409月04日", u8'x', u8'E', IOv2::ios_defs::eofbit) == check_date1);
    FOri(u8"x",   u8'x', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Ox", u8'x', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"x",   u8'x', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(u8"09/04/24", u8'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri(u8"%ED", u8'D', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OD", u8'D', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%H", u8'H', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%EH", u8'H', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"H",   u8'H', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"H",   u8'H', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%I", u8'I', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%EI", u8'I', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"I",   u8'I', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"I",   u8'I', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"248", u8'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(FYmd(u8"2024 248", u8"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    FOri(u8"%Ej", u8'j', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oj", u8'j', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"09", u8'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri(u8"09", u8'm', u8'O', IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri(u8"九", u8'm', u8'O', IOv2::ios_defs::eofbit).m_month == 9);
    FOri(u8"%Em", u8'm', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"m",   u8'm', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"m",   u8'm', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%M", u8'M', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%OM", u8'M', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%EM", u8'M', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"M",   u8'M', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"M",   u8'M', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"\n",   u8'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(u8"x",    u8'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(u8"\n",   u8'n', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%En",  u8'n', u8'E', IOv2::ios_defs::eofbit, 3);
    FOri(u8"n",    u8'n', u8'O', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%On",  u8'n', u8'O', IOv2::ios_defs::eofbit, 3);

    FOri(u8"\t",   u8't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(u8"x",    u8't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(u8"\t",   u8't', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Et",  u8't', u8'E', IOv2::ios_defs::eofbit, 3);
    FOri(u8"n",    u8't', u8'O', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Ot",  u8't', u8'O', IOv2::ios_defs::eofbit, 3);

    FOri(u8"%p", u8'p', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Ep", u8'p', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Op", u8'p', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%r", u8"%r",  IOv2::ios_defs::eofbit);
    FOri(u8"%Er", u8'r', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Or", u8'r', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, u8"13:33", u8"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(u8"%ER", u8'R', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OR", u8'R', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%S", u8'S', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%OS", u8'S', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%ES", u8'S', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"S",   u8'S', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"S",   u8'S', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%X", u8"%X",  IOv2::ios_defs::eofbit);
    FOri(u8"%EX", u8"%EX",  IOv2::ios_defs::eofbit);
    FOri(u8"X",   u8'X', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OX", u8'X', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"X",   u8'X', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%T", u8"%T",  IOv2::ios_defs::eofbit);
    FOri(u8"%ET", u8'T', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OT", u8'T', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"3", u8'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(u8"3", u8'u', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(u8"三", u8'u', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri(u8"%Eu", u8'u', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"u",   u8'u', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"u",   u8'u', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"24", u8'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri(u8"%Eg", u8'g', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Og", u8'g', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"2024", u8'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri(u8"%EG", u8'G', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OG", u8'G', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(u8"2024 35 水", u8"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"2024 35 水", u8"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"2024 三十五 水", u8"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri(u8"35", u8'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(FOri(u8"35", u8'U', u8'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    FOri(u8"%EU", u8'U', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"u8",   u8'U', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"u8",   u8'U', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(u8"2024 36 水", u8"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"2024 36 水", u8"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(u8"2024 三十六 水", u8"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri(u8"36", u8'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(FOri(u8"36", u8'W', u8'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    FOri(u8"%EW", u8'W', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"W",   u8'W', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"W",   u8'W', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"36", u8'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri(u8"36", u8'V', u8'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri(u8"三十六", u8'V', u8'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    FOri(u8"54",  u8'V', u8'O', IOv2::ios_defs::strfailbit, 1);
    FOri(u8"%EV", u8'V', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"V",   u8'V', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"V",   u8'V', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"3", u8'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(u8"3", u8'w', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(u8"三", u8'w', u8'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri(u8"%Ew", u8'w', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"w",   u8'w', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"w",   u8'w', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"24", u8'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd(u8"6", u8'y', u8'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(2024));
    VERIFY(FOri(u8"24", u8'y', u8'O', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri(u8"二十四", u8'y', u8'O', IOv2::ios_defs::eofbit).m_year == 2024);
    FOri(u8"y",  u8'y', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"y",  u8'y', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"2024", u8'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri(u8"2024", u8'Y', u8'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd(u8"平成3年", u8'Y', u8'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1991));
    FOri(u8"Y",   u8'Y', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OY", u8'Y', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"Y",   u8'Y', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%Z", u8'Z', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%EZ", u8'Z', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OZ", u8'Z', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%z", u8'z', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Ez", u8'z', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"%Oz", u8'z', u8'O', IOv2::ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(FYmd(u8"1999-W52-6", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd(u8"2019-W01-1", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd(u8"1999-W52-5", u8"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd(u8"99-W52-6", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd(u8"19-W01-1", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd(u8"99-W52-5", u8"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd(u8"20 24/09/04", u8"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(FYmd(u8"20 01 01", u8"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_char8_t_get_11()
{
    dump_info("Test timeio<char8_t> get 11...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char8_t, false, true, true>, false, true, true>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, true>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(u8"%",  u8'%',  0,  IOv2::ios_defs::eofbit);
    FOri(u8"x",  u8'%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%",  u8'%', u8'E', febit);
    FOri(u8"%E%", u8'%', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"%",  u8'%', u8'O', febit);
    FOri(u8"%O%", u8'%', u8'O', IOv2::ios_defs::eofbit);

    FOri(u8"%a", u8'a', 0, IOv2::ios_defs::eofbit, 3);
    FOri(u8"%Ea", u8'a', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oa", u8'a', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%A", u8'A', 0, IOv2::ios_defs::eofbit, 3);
    FOri(u8"%EA", u8'A', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OA", u8'A', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%b", u8'b', 0, IOv2::ios_defs::eofbit, 3);
    FOri(u8"%Eb", u8'b', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Ob", u8'b', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%B", u8'B', 0, IOv2::ios_defs::eofbit, 3);
    FOri(u8"%EB", u8'B', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OB", u8'B', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%h", u8'h', 0, IOv2::ios_defs::eofbit, 3);
    FOri(u8"%Eh", u8'h', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oh", u8'h', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    FOri(u8"%c", u8'c', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Ec", u8'c', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oc", u8'c', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%C", u8'C', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%EC", u8'C', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"C",   u8'C', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OC", u8'C', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"C",   u8'C', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%d", u8'd', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%Od", u8'd', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%Ed", u8'd', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"d",   u8'd', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"d",   u8'd', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%e", u8'e', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%Oe", u8'e', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%Ee", u8'e', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"e",   u8'e', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"e",   u8'e', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%F", u8'F', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%EF", u8'F', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OF", u8'F', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%x", u8'x', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Ex", u8'x', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"x",   u8'x', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Ox", u8'x', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"x",   u8'x', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%D", u8'D', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%ED", u8'D', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OD", u8'D', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"13", u8'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(u8"13", u8'H', u8'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(u8"十三", u8'H', u8'O', IOv2::ios_defs::eofbit).m_hour == 13);
    FOri(u8"%EH", u8'H', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"H",   u8'H', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"H",   u8'H', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"01", u8'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(u8"01", u8'I', u8'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(u8"一", u8'I', u8'O', IOv2::ios_defs::eofbit).m_hour == 1);
    FOri(u8"%EI", u8'I', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"I",   u8'I', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"I",   u8'I', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%j", u8'j', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Ej", u8'j', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oj", u8'j', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%m", u8'm',  0, IOv2::ios_defs::eofbit);
    FOri(u8"%Om", u8'm', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%Em", u8'm', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"m",   u8'm', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"m",   u8'm', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"33", u8'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(u8"33", u8'M', u8'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(u8"三十三", u8'M', u8'O', IOv2::ios_defs::eofbit).m_minute == 33);
    FOri(u8"%EM", u8'M', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"M",   u8'M', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"M",   u8'M', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"\n",   u8'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(u8"x",    u8'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(u8"\n",   u8'n', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%En",  u8'n', u8'E', IOv2::ios_defs::eofbit, 3);
    FOri(u8"n",    u8'n', u8'O', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%On",  u8'n', u8'O', IOv2::ios_defs::eofbit, 3);

    FOri(u8"\t",   u8't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(u8"x",    u8't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(u8"\t",   u8't', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Et",  u8't', u8'E', IOv2::ios_defs::eofbit, 3);
    FOri(u8"n",    u8't', u8'O', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Ot",  u8't', u8'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(FHms(u8"01 午後", u8"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(FHms(u8"01 午前", u8"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(FOri(u8"午後", u8'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(FOri(u8"午前", u8'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    FOri(u8"%Ep", u8'p', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Op", u8'p', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(u8"午後01時33分18秒", u8"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"%Er", u8'r', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Or", u8'r', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(u8"13:33", u8"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(u8"%ER", u8'R', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OR", u8'R', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"18", u8'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(u8"18", u8'S', u8'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(u8"十八", u8'S', u8'O', IOv2::ios_defs::eofbit).m_second == 18);
    FOri(u8"%ES", u8'S', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"S",   u8'S', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"S",   u8'S', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(u8"13時33分18秒 America/Los_Angeles", u8"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(FHms(u8"13時33分18秒 America/Los_Angeles", u8"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"X",   u8'X', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OX", u8'X', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"X",   u8'X', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(u8"13:33:18", u8"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"%ET", u8'T', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OT", u8'T', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%u", u8'u', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%Ou", u8'u', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%Eu", u8'u', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"u",   u8'u', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"u",   u8'u', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%g", u8'g', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Eg", u8'g', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Og", u8'g', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%G", u8'G', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%EG", u8'G', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OG", u8'G', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%U", u8'U', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%OU", u8'U', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%EU", u8'U', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"u8",   u8'U', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"u8",   u8'U', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%W", u8'W', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%OW", u8'W', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%EW", u8'W', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"W",   u8'W', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"W",   u8'W', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%V", u8'V', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%OV", u8'V', u8'O',   IOv2::ios_defs::eofbit);
    FOri(u8"54",  u8'V', u8'O', IOv2::ios_defs::strfailbit, 1);
    FOri(u8"%EV", u8'V', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"V",   u8'V', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"V",   u8'V', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%w", u8'w', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%Ow", u8'w', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%Ew", u8'w', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"w",   u8'w', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"w",   u8'w', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%y", u8'y', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%Ey", u8'y', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"%Oy", u8'y', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"y",  u8'y', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"y",  u8'y', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%Y", u8'Y', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%EY", u8'Y', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"Y",   u8'Y', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OY", u8'Y', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"Y",   u8'Y', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"America/Los_Angeles", u8'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    VERIFY(FOri(u8"PST", u8'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
    FOri(u8"America/Los_Angexes", u8'Z', 0, IOv2::ios_defs::strfailbit);
    FOri(u8"%EZ", u8'Z', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OZ", u8'Z', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%z", u8'z', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Ez", u8'z', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"z",  u8'z', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oz", u8'z', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"z",  u8'z', u8'O', IOv2::ios_defs::strfailbit, 0);

    dump_info("Done\n");
}

void test_timeio_char8_t_get_12()
{
    dump_info("Test timeio<char8_t> get 12...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char8_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char8_t, false, true, false>, false, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(u8"%",  u8'%',  0,  IOv2::ios_defs::eofbit);
    FOri(u8"x",  u8'%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%",  u8'%', u8'E', febit);
    FOri(u8"%E%", u8'%', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"%",  u8'%', u8'O', febit);
    FOri(u8"%O%", u8'%', u8'O', IOv2::ios_defs::eofbit);

    FOri(u8"%a", u8'a', 0, IOv2::ios_defs::eofbit, 3);
    FOri(u8"%Ea", u8'a', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oa", u8'a', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"a",   u8'a', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%A", u8'A', 0, IOv2::ios_defs::eofbit, 3);
    FOri(u8"%EA", u8'A', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OA", u8'A', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"A",   u8'A', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%b", u8'b', 0, IOv2::ios_defs::eofbit, 3);
    FOri(u8"%Eb", u8'b', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Ob", u8'b', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"b",   u8'b', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%B", u8'B', 0, IOv2::ios_defs::eofbit, 3);
    FOri(u8"%EB", u8'B', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OB", u8'B', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"B",   u8'B', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%h", u8'h', 0, IOv2::ios_defs::eofbit, 3);
    FOri(u8"%Eh", u8'h', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oh", u8'h', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"h",   u8'h', u8'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    FOri(u8"%c", u8'c', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Ec", u8'c', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oc", u8'c', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"c",   u8'c', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%C", u8'C', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%EC", u8'C', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"C",   u8'C', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OC", u8'C', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"C",   u8'C', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%d", u8'd', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%Od", u8'd', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%Ed", u8'd', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"d",   u8'd', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"d",   u8'd', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%e", u8'e', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%Oe", u8'e', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%Ee", u8'e', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"e",   u8'e', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"e",   u8'e', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%F", u8'F', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%EF", u8'F', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OF", u8'F', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"F",   u8'F', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%x", u8'x', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Ex", u8'x', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"x",   u8'x', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Ox", u8'x', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"x",   u8'x', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%D", u8'D', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%ED", u8'D', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OD", u8'D', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"D",   u8'D', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"13", u8'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(u8"13", u8'H', u8'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(u8"十三", u8'H', u8'O', IOv2::ios_defs::eofbit).m_hour == 13);
    FOri(u8"%EH", u8'H', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"H",   u8'H', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"H",   u8'H', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"01", u8'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(u8"01", u8'I', u8'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(u8"一", u8'I', u8'O', IOv2::ios_defs::eofbit).m_hour == 1);
    FOri(u8"%EI", u8'I', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"I",   u8'I', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"I",   u8'I', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%j", u8'j', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Ej", u8'j', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oj", u8'j', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"j",   u8'j', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%m", u8'm',  0, IOv2::ios_defs::eofbit);
    FOri(u8"%Om", u8'm', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%Em", u8'm', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"m",   u8'm', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"m",   u8'm', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"33", u8'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(u8"33", u8'M', u8'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(u8"三十三", 'M', u8'O', IOv2::ios_defs::eofbit).m_minute == 33);
    FOri(u8"%EM", u8'M', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"M",   u8'M', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"M",   u8'M', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"\n",   u8'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(u8"x",    u8'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(u8"\n",   u8'n', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%En",  u8'n', u8'E', IOv2::ios_defs::eofbit, 3);
    FOri(u8"n",    u8'n', u8'O', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%On",  u8'n', u8'O', IOv2::ios_defs::eofbit, 3);

    FOri(u8"\t",   u8't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(u8"x",    u8't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(u8"\t",   u8't', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Et",  u8't', u8'E', IOv2::ios_defs::eofbit, 3);
    FOri(u8"n",    u8't', u8'O', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Ot",  u8't', u8'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(FHms(u8"01 午後", u8"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(FHms(u8"01 午前", u8"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(FOri(u8"午後", u8'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(FOri(u8"午前", u8'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    FOri(u8"%Ep", u8'p', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Op", u8'p', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"p",   u8'p', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(u8"午後01時33分18秒", u8"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"%Er", u8'r', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Or", u8'r', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"r",   u8'r', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(u8"13:33", u8"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(u8"%ER", u8'R', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OR", u8'R', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"R",   u8'R', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(u8"18", u8'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(u8"18", u8'S', u8'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(u8"十八", u8'S', u8'O', IOv2::ios_defs::eofbit).m_second == 18);
    FOri(u8"%ES", u8'S', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"S",   u8'S', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"S",   u8'S', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(u8"13時33分18秒", u8"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(FHms(u8"13時33分18秒", u8"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"X",   u8'X', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OX", u8'X', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"X",   u8'X', u8'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(u8"13:33:18", u8"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(u8"%ET", u8'T', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OT", u8'T', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"T",   u8'T', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%u", u8'u', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%Ou", u8'u', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%Eu", u8'u', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"u",   u8'u', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"u",   u8'u', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%g", u8'g', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Eg", u8'g', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Og", u8'g', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"g",   u8'g', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%G", u8'G', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%EG", u8'G', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OG", u8'G', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"G",   u8'G', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%U", u8'U', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%OU", u8'U', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%EU", u8'U', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"u8",   u8'U', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"u8",   u8'U', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%W", u8'W', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%OW", u8'W', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%EW", u8'W', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"W",   u8'W', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"W",   u8'W', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%V", u8'V', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%OV", u8'V', u8'O',   IOv2::ios_defs::eofbit);
    FOri(u8"54",  u8'V', u8'O', IOv2::ios_defs::strfailbit, 1);
    FOri(u8"%EV", u8'V', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"V",   u8'V', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"V",   u8'V', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%w", u8'w', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%Ow", u8'w', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"%Ew", u8'w', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"w",   u8'w', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"w",   u8'w', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%y", u8'y', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%Ey", u8'y', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"%Oy", u8'y', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"y",  u8'y', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"y",  u8'y', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%Y", u8'Y', 0,   IOv2::ios_defs::eofbit);
    FOri(u8"%EY", u8'Y', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"Y",   u8'Y', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OY", u8'Y', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"Y",   u8'Y', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%Z", u8'Z', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%EZ", u8'Z', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%OZ", u8'Z', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"Z",   u8'Z', u8'O', IOv2::ios_defs::strfailbit, 0);

    FOri(u8"%z", u8'z', 0, IOv2::ios_defs::eofbit);
    FOri(u8"%Ez", u8'z', u8'E', IOv2::ios_defs::eofbit);
    FOri(u8"z",  u8'z', u8'E', IOv2::ios_defs::strfailbit, 0);
    FOri(u8"%Oz", u8'z', u8'O', IOv2::ios_defs::eofbit);
    FOri(u8"z",  u8'z', u8'O', IOv2::ios_defs::strfailbit, 0);

    dump_info("Done\n");
}