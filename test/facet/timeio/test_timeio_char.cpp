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
        VERIFY(res == "09/04/24 13:33:18 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        VERIFY(res == "09/04/24 13:33:18 America/Los_Angeles");
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
        VERIFY(res == "13:33:18 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        VERIFY(res == "13:33:18 America/Los_Angeles");
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
        VERIFY(res == "2024年09月04日 星期三 13时33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        VERIFY(res == "2024年09月04日 星期三 13时33分18秒 America/Los_Angeles");
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
        VERIFY(res == "13时33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        VERIFY(res == "13时33分18秒 America/Los_Angeles");
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
        VERIFY(res == "2024年09月04日 13時33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        VERIFY(res == "令和6年09月04日 13時33分18秒 America/Los_Angeles");
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
        VERIFY(res == "13時33分18秒 America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        VERIFY(res == "13時33分18秒 America/Los_Angeles");
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
        VERIFY(oss == "12:00:00 America/Los_Angeles");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, 'x', 'E');
        VERIFY(oss == "04/04/71");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, 'X', 'E');
        VERIFY(oss == "12:00:00 America/Los_Angeles");
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
        VERIFY(oss == "12:00:00 America/Los_Angeles");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x', 'E');
        VERIFY(oss == "04.04.1971");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X', 'E');
        VERIFY(oss == "12:00:00 America/Los_Angeles");
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
        VERIFY(oss == "12:00:00 America/Los_Angeles");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x', 'E');
        VERIFY(oss == "04/04/71");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X', 'E');
        VERIFY(oss == "12:00:00 America/Los_Angeles");
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

    VERIFY(time_buffer + std::string(" America/Los_Angeles") == res);

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

namespace
{
    constexpr static IOv2::ios_defs::iostate febit = IOv2::ios_defs::eofbit | IOv2::ios_defs::strfailbit;

    template <typename T = IOv2::time_parse_context<char>, bool HaveDate = true, bool HaveTime = true, bool HaveTimeZone = true>
    T CheckGet(const IOv2::timeio<char>& obj, const std::string& input,
               char fmt, char modif,
               IOv2::ios_defs::iostate err_exp, size_t consume_exp = (size_t)-1)
    {
        if (consume_exp == (size_t)-1) consume_exp = input.size();
        IOv2::time_parse_context<char, HaveDate, HaveTime, HaveTimeZone> ctx1, ctx2, ctx3;
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
        return static_cast<T>(ctx1);
    }

    template <typename T = IOv2::time_parse_context<char>, bool HaveDate = true, bool HaveTime = true, bool HaveTimeZone = true>
    T CheckGet(const IOv2::timeio<char>& obj, const std::string& input,
               const std::string& fmt,
               IOv2::ios_defs::iostate err_exp, size_t consume_exp = (size_t)-1)
    {
        if (consume_exp == (size_t)-1) consume_exp = input.size();
        IOv2::time_parse_context<char, HaveDate, HaveTime, HaveTimeZone> ctx1, ctx2, ctx3;
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
        return static_cast<T>(ctx1);
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
    VERIFY(CheckGet<year_month_day>(obj, "09/04/24 13:33:18 America/Los_Angeles", 'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "09/04/24 13:33:18 America/Los_Angeles", 'c', 'E', IOv2::ios_defs::eofbit, 17) == check_date1);
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

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18 America/Los_Angeles", "%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13:33:18 America/Los_Angeles", "%EX",  IOv2::ios_defs::eofbit).to_duration()
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

    VERIFY(CheckGet(obj, "America/Los_Angeles", 'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    { auto r = CheckGet(obj, "PST", 'Z', 0, IOv2::ios_defs::eofbit); VERIFY(r.m_zone_name == "" && r.m_zone_abbrev == "PST"); }
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

    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(CheckGet<year_month_day>(obj, "20 01 01", "%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }
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
    VERIFY(CheckGet<year_month_day>(obj, "2024年09月04日 星期三 13时33分18秒 America/Los_Angeles", 'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "2024年09月04日 星期三 13时33分18秒 America/Los_Angeles", 'c', 'E', IOv2::ios_defs::eofbit) == check_date1);
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

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13时33分18秒 America/Los_Angeles", "%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13时33分18秒 America/Los_Angeles", "%EX",  IOv2::ios_defs::eofbit).to_duration()
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

    VERIFY(CheckGet(obj, "America/Los_Angeles", 'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    { auto r = CheckGet(obj, "PST", 'Z', 0, IOv2::ios_defs::eofbit); VERIFY(r.m_zone_name == "" && r.m_zone_abbrev == "PST"); }
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
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(CheckGet<year_month_day>(obj, "20 01 01", "%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

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
    VERIFY(CheckGet<year_month_day>(obj, "2024年09月04日 13時33分18秒 America/Los_Angeles", 'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "令和6年09月04日 13時33分18秒 America/Los_Angeles", 'c', 'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, "202409月04日 13時33分18秒 America/Los_Angeles", 'c', 'E', IOv2::ios_defs::eofbit) == check_date1);
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

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13時33分18秒 America/Los_Angeles", "%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13時33分18秒 America/Los_Angeles", "%EX",  IOv2::ios_defs::eofbit).to_duration()
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

    VERIFY(CheckGet(obj, "America/Los_Angeles", 'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    { auto r = CheckGet(obj, "PST", 'Z', 0, IOv2::ios_defs::eofbit); VERIFY(r.m_zone_name == "" && r.m_zone_abbrev == "PST"); }
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
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(CheckGet<year_month_day>(obj, "20 01 01", "%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

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
        std::string input = "2020  ";
        std::string format = "%Y";
        
        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 1);
    }

    {
        std::string input = "Tue ";
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_wday == 2);
    }

    {
        std::string input = "Wednesday";
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 3);
    }

    {
        std::string input = "Thu";
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 4);
    }

    {
        std::string input = "Fri ";
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_wday == 5);
    }

    {
        std::string input = "Saturday";
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_wday == 6);
    }

    {
        std::string input = "Feb";
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 1);
    }

    {
        std::string input = "Mar ";
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_mon == 2);
    }

    {
        std::string input = "April";
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 3);
    }

    {
        std::string input = "May";
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 4);
    }

    {
        std::string input = "Jun ";
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_mon == 5);
    }

    {
        std::string input = "July";
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 6);
    }

    {
        std::string input = "Aug";
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 7);
    }

    {
        std::string input = "May ";
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == input.end()) || (*ret != ' ')));
        VERIFY(time.tm_mon == 4);
    }

    {
        std::string input = "October";
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_mon == 9);
    }

    // Other tests.
    {
        std::string input = "2.";
        std::string format = "%d.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_mday == 5);
    }

    {
        std::string input = "06.";
        std::string format = "%e.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 0);
        VERIFY(time.tm_min == 0);
    }

    {
        std::string input = "12:37AM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 0);
        VERIFY(time.tm_min == 37);
    }

    {
        std::string input = "01:25AM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 1);
        VERIFY(time.tm_min == 25);
    }

    {
        std::string input = "12:00PM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 12);
        VERIFY(time.tm_min == 0);
    }

    {
        std::string input = "12:42PM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 12);
        VERIFY(time.tm_min == 42);
    }

    {
        std::string input = "07:23PM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_hour == 19);
        VERIFY(time.tm_min == 23);
    }

    {
        std::string input = "17%20";
        std::string format = "%H%%%M";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        VERIFY(ret == input.end());
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 537 - 1900);
    }

    {
        std::string input = "68";
        std::string format = "%y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 2068 - 1900);
    }

    {
        std::string input = "69";
        std::string format = "%y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == input.end());
        VERIFY(time.tm_year == 1969 - 1900);
    }

    {
        std::string input = "03-Feb-2003";
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "16-Dec-2020";
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "16-Dec-2021";
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "253 2020";
        std::string format = "%j %Y";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "233 2021";
        std::string format = "%j %Y";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "2020 23 3";
        std::string format = "%Y %U %w";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "2020 23 3";
        std::string format = "%Y %W %w";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "2021 43 Fri";
        std::string format = "%Y %W %a";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "2024 23 3";
        std::string format = "%Y %U %w";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "2024 23 3";
        std::string format = "%Y %W %w";

        IOv2::time_parse_context<char> ctx;
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

void test_timeio_char_get_8()
{
    dump_info("Test timeio<char> get 8...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("C"));
    {
        std::string input = "01:38:12 PM";
        std::string format = "%I:%M:%S %p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_wday == 1);
    }

    {
        streambuf sb(mem_device{"Tue "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == std::default_sentinel) || (*ret != ' ')));
        VERIFY(time.tm_wday == 2);
    }

    {
        streambuf sb(mem_device{"Wednesday"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_wday == 3);
    }

    {
        streambuf sb(mem_device{"Thu"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_wday == 4);
    }

    {
        streambuf sb(mem_device{"Fri "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == std::default_sentinel) || (*ret != ' ')));
        VERIFY(time.tm_wday == 5);
    }

    {
        streambuf sb(mem_device{"Saturday"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_wday == 6);
    }

    {
        streambuf sb(mem_device{"Feb"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_mon == 1);
    }

    {
        streambuf sb(mem_device{"Mar "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == std::default_sentinel) || (*ret != ' ')));
        VERIFY(time.tm_mon == 2);
    }

    {
        streambuf sb(mem_device{"April"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_mon == 3);
    }

    {
        streambuf sb(mem_device{"May"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_mon == 4);
    }

    {
        streambuf sb(mem_device{"Jun "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == std::default_sentinel) || (*ret != ' ')));
        VERIFY(time.tm_mon == 5);
    }

    {
        streambuf sb(mem_device{"July"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_mon == 6);
    }

    {
        streambuf sb(mem_device{"Aug"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_mon == 7);
    }

    {
        streambuf sb(mem_device{"May "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(!((ret == std::default_sentinel) || (*ret != ' ')));
        VERIFY(time.tm_mon == 4);
    }

    {
        streambuf sb(mem_device{"October"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
        VERIFY(time.tm_mday == 5);
    }

    {
        streambuf sb(mem_device{"06."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%e.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        VERIFY(ret == std::default_sentinel);
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 537 - 1900);
    }

    {
        streambuf sb(mem_device{"68"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 2068 - 1900);
    }

    {
        streambuf sb(mem_device{"69"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        VERIFY(ret == std::default_sentinel);
        VERIFY(time.tm_year == 1969 - 1900);
    }

    {
        streambuf sb(mem_device{"03-Feb-2003"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        auto time = static_cast<std::tm>(ctx);
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
        return CheckGet<IOv2::time_parse_context<char, true, true, false>, true, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, true, false>(obj, std::forward<decltype(args)>(args)...);
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

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13時33分18秒 America/Los_Angeles", "%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, "13時33分18秒 America/Los_Angeles", "%EX",  IOv2::ios_defs::eofbit).to_duration()
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
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(FYmd("20 01 01", "%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_char_get_15()
{
    dump_info("Test timeio<char> get 15...");
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char, true, false, false>, true, false, false>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, false, false>(obj, std::forward<decltype(args)>(args)...);
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
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(FYmd("20 01 01", "%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_char_get_16()
{
    dump_info("Test timeio<char> get 16...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char, false, true, true>, false, true, true>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, true>(obj, std::forward<decltype(args)>(args)...);
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

    VERIFY(FHms("13時33分18秒 America/Los_Angeles", "%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(FHms("13時33分18秒 America/Los_Angeles", "%EX",  IOv2::ios_defs::eofbit).to_duration()
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

    VERIFY(FOri("America/Los_Angeles", 'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    { auto r = FOri("PST", 'Z', 0, IOv2::ios_defs::eofbit); VERIFY(r.m_zone_name == "" && r.m_zone_abbrev == "PST"); }
    FOri("America/Los_Angexes", 'Z', 0, IOv2::ios_defs::strfailbit);
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

void test_timeio_char_get_17()
{
    dump_info("Test timeio<char> get 17...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<char>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<char, false, true, false>, false, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, false>(obj, std::forward<decltype(args)>(args)...);
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

    // operator year_month_day() throws for invalid reconstructed date (line 126)
    // Feb 30 parses successfully but is not a valid calendar date
    {
        auto ctx = CheckGet(obj, "02 30", "%m %d", IOv2::ios_defs::eofbit);
        bool threw = false;
        try { auto ymd = static_cast<year_month_day>(ctx); (void)ymd; }
        catch (IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    // Era deduction: m_have_mon=true, m_have_mday=false, match found (lines 224-241)
    // 令和6 January: est_year=2024, Jan is within 令和 era -> deduced_year=2024
    {
        auto ctx = CheckGet(obj_ja, "令和6 01", "%EC%Ey %m", IOv2::ios_defs::eofbit);
        auto ymd = static_cast<year_month_day>(ctx);
        VERIFY(ymd.year() == year{2024} && ymd.month() == month{1});
    }

    // Era deduction: m_have_mon=true, m_have_mday=false, nothing matches (lines 245-246)
    // 平成31 May: est_year=2019, May 2019 past 平成 end (Apr 30) -> from_year=1990
    {
        auto ctx = CheckGet(obj_ja, "平成31 05", "%EC%Ey %m", IOv2::ios_defs::eofbit);
        auto ymd = static_cast<year_month_day>(ctx);
        VERIFY(ymd.year() == year{1990} && ymd.month() == month{5});
    }

    // Era deduction: m_have_mon=true, m_have_mday=true, nothing matches (line 220)
    // 平成31 May 1: May 1, 2019 past 平成 end (Apr 30) -> from_year=1990
    {
        auto ctx = CheckGet(obj_ja, "平成31 05 01", "%EC%Ey %m %d", IOv2::ios_defs::eofbit);
        auto ymd = static_cast<year_month_day>(ctx);
        VERIFY(ymd == year_month_day{year{1990}, month{5}, day{1}});
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