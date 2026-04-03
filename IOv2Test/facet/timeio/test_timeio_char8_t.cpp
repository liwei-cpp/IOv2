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
        if (res != u8"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a');
        if (res != u8"Wed") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'E');
        if (res != u8"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'O');
        if (res != u8"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A');
        if (res != u8"Wednesday") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'E');
        if (res != u8"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'O');
        if (res != u8"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b');
        if (res != u8"Sep") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'E');
        if (res != u8"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'O');
        if (res != u8"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h');
        if (res != u8"Sep") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'E');
        if (res != u8"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'O');
        if (res != u8"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B');
        if (res != u8"September") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'E');
        if (res != u8"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'O');
        if (res != u8"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c');
        if (res != u8"09/04/24 13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'E');
        if (res != u8"09/04/24 13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'O');
        if (res != u8"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C');
        if (res != u8"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'E');
        if (res != u8"20") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'O');
        if (res != u8"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x');
        if (res != u8"09/04/24") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'E');
        if (res != u8"09/04/24") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'O');
        if (res != u8"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D');
        if (res != u8"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'E');
        if (res != u8"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'O');
        if (res != u8"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd');
        if (res != u8"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'E');
        if (res != u8"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'O');
        if (res != u8"04") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e');
        if (res != u8" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'E');
        if (res != u8"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'O');
        if (res != u8" 4") throw std::runtime_error("timeio::put fail for Oe");
    }
      
    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F');
        if (res != u8"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'E');
        if (res != u8"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'O');
        if (res != u8"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H');
        if (res != u8"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'E');
        if (res != u8"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'O');
        if (res != u8"13") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I');
        if (res != u8"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'E');
        if (res != u8"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'O');
        if (res != u8"01") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j');
        if (res != u8"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'E');
        if (res != u8"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'O');
        if (res != u8"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M');
        if (res != u8"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'E');
        if (res != u8"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'O');
        if (res != u8"33") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm');
        if (res != u8"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'E');
        if (res != u8"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'O');
        if (res != u8"09") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n');
        if (res != u8"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'E');
        if (res != u8"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'O');
        if (res != u8"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p');
        if (res != u8"PM") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'E');
        if (res != u8"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'O');
        if (res != u8"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R');
        if (res != u8"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'E');
        if (res != u8"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'O');
        if (res != u8"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r');
        if (res != u8"01:33:18 PM") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'E');
        if (res != u8"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'O');
        if (res != u8"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S');
        if (res != u8"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'E');
        if (res != u8"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'O');
        if (res != u8"18") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X');
        if (res != u8"13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'E');
        if (res != u8"13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'O');
        if (res != u8"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T');
        if (res != u8"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'E');
        if (res != u8"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'O');
        if (res != u8"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8't');
        if (res != u8"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'E');
        if (res != u8"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'O');
        if (res != u8"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'E');
        if (res != u8"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'O');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U');
        if (res != u8"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'E');
        if (res != u8"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'O');
        if (res != u8"35") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'E');
        if (res != u8"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'O');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'E');
        if (res != u8"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'O');
        if (res != u8"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G');
        if (res != u8"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'E');
        if (res != u8"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'O');
        if (res != u8"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'E');
        if (res != u8"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'O');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'E');
        if (res != u8"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'O');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y');
        if (res != u8"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'E');
        if (res != u8"2024") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'O');
        if (res != u8"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'E');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'O');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z');
        VERIFY(res == u8"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'E');
        if (res != u8"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'O');
        if (res != u8"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z');
        VERIFY(res == u8"-0700");
        if (res.empty()) throw std::runtime_error("timeio::put fail for z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'E');
        if (res != u8"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'O');
        if (res != u8"%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (res != u8"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a');
        if (res != u8"三") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'E');
        if (res != u8"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'O');
        if (res != u8"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A');
        if (res != u8"星期三") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'E');
        if (res != u8"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'O');
        if (res != u8"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b');
        if (res != u8"9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'E');
        if (res != u8"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'O');
        if (res != u8"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h');
        if (res != u8"9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'E');
        if (res != u8"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'O');
        if (res != u8"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B');
        if (res != u8"九月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'E');
        if (res != u8"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'O');
        if (res != u8"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c');
        if (res != u8"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'E');
        if (res != u8"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'O');
        if (res != u8"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C');
        if (res != u8"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'E');
        if (res != u8"20") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'O');
        if (res != u8"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x');
        if (res != u8"2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'E');
        if (res != u8"2024年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'O');
        if (res != u8"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D');
        if (res != u8"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'E');
        if (res != u8"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'O');
        if (res != u8"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd');
        if (res != u8"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'E');
        if (res != u8"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'O');
        if (res != u8"04") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e');
        if (res != u8" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'E');
        if (res != u8"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'O');
        if (res != u8" 4") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F');
        if (res != u8"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'E');
        if (res != u8"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'O');
        if (res != u8"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H');
        if (res != u8"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'E');
        if (res != u8"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'O');
        if (res != u8"13") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I');
        if (res != u8"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'E');
        if (res != u8"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'O');
        if (res != u8"01") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j');
        if (res != u8"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'E');
        if (res != u8"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'O');
        if (res != u8"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M');
        if (res != u8"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'E');
        if (res != u8"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'O');
        if (res != u8"33") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm');
        if (res != u8"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'E');
        if (res != u8"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'O');
        if (res != u8"09") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n');
        if (res != u8"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'E');
        if (res != u8"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'O');
        if (res != u8"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p');
        if (res != u8"下午") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'E');
        if (res != u8"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'O');
        if (res != u8"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R');
        if (res != u8"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'E');
        if (res != u8"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'O');
        if (res != u8"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r');
        if (res != u8"下午 01时33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'E');
        if (res != u8"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'O');
        if (res != u8"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S');
        if (res != u8"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'E');
        if (res != u8"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'O');
        if (res != u8"18") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X');
        if (res != u8"13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'E');
        if (res != u8"13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'O');
        if (res != u8"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T');
        if (res != u8"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'E');
        if (res != u8"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'O');
        if (res != u8"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8't');
        if (res != u8"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'E');
        if (res != u8"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'O');
        if (res != u8"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'E');
        if (res != u8"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'O');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U');
        if (res != u8"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'E');
        if (res != u8"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'O');
        if (res != u8"35") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'E');
        if (res != u8"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'O');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'E');
        if (res != u8"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'O');
        if (res != u8"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G');
        if (res != u8"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'E');
        if (res != u8"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'O');
        if (res != u8"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'E');
        if (res != u8"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'O');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'E');
        if (res != u8"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'O');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y');
        if (res != u8"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'E');
        if (res != u8"2024") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'O');
        if (res != u8"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'E');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'O');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z');
        VERIFY(res == u8"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'E');
        if (res != u8"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'O');
        if (res != u8"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z');
        VERIFY(res == u8"-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'E');
        if (res != u8"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'O');
        if (res != u8"%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (res != u8"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a');
        if (res != u8"水") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'E');
        if (res != u8"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'O');
        if (res != u8"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A');
        if (res != u8"水曜日") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'E');
        if (res != u8"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'O');
        if (res != u8"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b');
        if (res != u8" 9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'E');
        if (res != u8"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'O');
        if (res != u8"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h');
        if (res != u8" 9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'E');
        if (res != u8"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'O');
        if (res != u8"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B');
        if (res != u8"9月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'E');
        if (res != u8"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'O');
        if (res != u8"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c');
        if (res != u8"2024年09月04日 13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'E');
        if (res != u8"令和6年09月04日 13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'O');
        if (res != u8"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C');
        if (res != u8"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'E');
        if (res != u8"令和") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'O');
        if (res != u8"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x');
        if (res != u8"2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'E');
        if (res != u8"令和6年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'O');
        if (res != u8"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D');
        if (res != u8"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'E');
        if (res != u8"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'O');
        if (res != u8"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd');
        if (res != u8"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'E');
        if (res != u8"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'O');
        if (res != u8"四") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e');
        if (res != u8" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'E');
        if (res != u8"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'O');
        if (res != u8"四") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F');
        if (res != u8"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'E');
        if (res != u8"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'O');
        if (res != u8"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H');
        if (res != u8"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'E');
        if (res != u8"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'O');
        if (res != u8"十三") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I');
        if (res != u8"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'E');
        if (res != u8"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'O');
        if (res != u8"一") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j');
        if (res != u8"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'E');
        if (res != u8"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'O');
        if (res != u8"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M');
        if (res != u8"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'E');
        if (res != u8"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'O');
        if (res != u8"三十三") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm');
        if (res != u8"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'E');
        if (res != u8"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'O');
        if (res != u8"九") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n');
        if (res != u8"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'E');
        if (res != u8"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'O');
        if (res != u8"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p');
        if (res != u8"午後") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'E');
        if (res != u8"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'O');
        if (res != u8"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R');
        if (res != u8"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'E');
        if (res != u8"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'O');
        if (res != u8"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r');
        if (res != u8"午後01時33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'E');
        if (res != u8"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'O');
        if (res != u8"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S');
        if (res != u8"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'E');
        if (res != u8"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'O');
        if (res != u8"十八") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X');
        if (res != u8"13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'E');
        if (res != u8"13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'O');
        if (res != u8"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T');
        if (res != u8"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'E');
        if (res != u8"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'O');
        if (res != u8"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8't');
        if (res != u8"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'E');
        if (res != u8"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'O');
        if (res != u8"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'E');
        if (res != u8"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'O');
        if (res != u8"三") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U');
        if (res != u8"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'E');
        if (res != u8"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'O');
        if (res != u8"三十五") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'E');
        if (res != u8"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'O');
        if (res != u8"三十六") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'E');
        if (res != u8"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'O');
        if (res != u8"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G');
        if (res != u8"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'E');
        if (res != u8"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'O');
        if (res != u8"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'E');
        if (res != u8"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'O');
        if (res != u8"三十六") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'E');
        if (res != u8"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'O');
        if (res != u8"三") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y');
        if (res != u8"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'E');
        if (res != u8"令和6年") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'O');
        if (res != u8"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'E');
        if (res != u8"6") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'O');
        if (res != u8"二十四") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z');
        VERIFY(res == u8"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'E');
        if (res != u8"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'O');
        if (res != u8"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z');
        VERIFY(res == u8"-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'E');
        if (res != u8"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'O');
        if (res != u8"%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (oss != u8"Sun") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, u8'x');
        if (oss != u8"04/04/71") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, u8'X');
        if (oss != u8"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, u8'x', u8'E');
        if (oss != u8"04/04/71") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, u8'X', u8'E');
        if (oss != u8"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
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
        if ((oss != u8"Son") && (oss != u8"So")) throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'x');
        if (oss != u8"04.04.1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'X');
        if (oss != u8"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'x', u8'E');
        if (oss != u8"04.04.1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'X', u8'E');
        if (oss != u8"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
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
        if (oss != u8"Sun") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'x');
        if (oss != u8"Sunday, April 04, 1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'X');
        if (oss.find(u8"12:00:00") == std::u8string::npos) throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'x', u8'E');
        if (oss != u8"Sunday, April 04, 1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'X', u8'E');
        if (oss.find(u8"12:00:00") == std::u8string::npos) throw std::runtime_error("timeio::put fail");
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
        if (oss != u8"dom") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'x');
        if (oss != u8"04/04/71") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'X');
        if (oss != u8"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'x', u8'E');
        if (oss != u8"04/04/71") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, u8'X', u8'E');
        if (oss != u8"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
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
        if (oss != u8"Sunday, the second of April") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
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
        if (oss != u8"Sonntag, the second of April") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
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
        if (oss != u8"Sunday, the second of April") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
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
        if (oss != u8"dimanche, the second of avril") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
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
    if (res != u8"12:00:00, Thursday, the second of June, 1997xxxxxx") throw std::runtime_error("timeio::put fail");
    if (sanity1 != u8"12:00:00, Thursday, the second of June, 1997") throw std::runtime_error("timeio::put fail");

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
    if (res != u8"Tuesdayxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx") throw std::runtime_error("timeio::put fail");
    if (sanity1 != u8"Tuesday") throw std::runtime_error("timeio::put fail");

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

    if (time_buffer + std::u8string(u8" America/Los_Angeles") != res) throw std::runtime_error("timeio::put fail");

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
        if (res != u8"水") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'E');
        if (res != u8"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'O');
        if (res != u8"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A');
        if (res != u8"水曜日") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'E');
        if (res != u8"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'O');
        if (res != u8"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b');
        if (res != u8" 9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'E');
        if (res != u8"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'O');
        if (res != u8"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h');
        if (res != u8" 9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'E');
        if (res != u8"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'O');
        if (res != u8"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B');
        if (res != u8"9月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'E');
        if (res != u8"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'O');
        if (res != u8"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c');
        if (res != u8"%c") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'E');
        if (res != u8"%Ec") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'O');
        if (res != u8"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C');
        if (res != u8"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'E');
        if (res != u8"令和") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'O');
        if (res != u8"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x');
        if (res != u8"2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'E');
        if (res != u8"令和6年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'O');
        if (res != u8"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D');
        if (res != u8"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'E');
        if (res != u8"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'O');
        if (res != u8"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd');
        if (res != u8"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'E');
        if (res != u8"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'O');
        if (res != u8"四") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e');
        if (res != u8" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'E');
        if (res != u8"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'O');
        if (res != u8"四") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F');
        if (res != u8"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'E');
        if (res != u8"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'O');
        if (res != u8"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H');
        if (res != u8"%H") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'E');
        if (res != u8"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'O');
        if (res != u8"%OH") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I');
        if (res != u8"%I") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'E');
        if (res != u8"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'O');
        if (res != u8"%OI") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j');
        if (res != u8"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'E');
        if (res != u8"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'O');
        if (res != u8"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M');
        if (res != u8"%M") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'E');
        if (res != u8"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'O');
        if (res != u8"%OM") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm');
        if (res != u8"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'E');
        if (res != u8"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'O');
        if (res != u8"九") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n');
        if (res != u8"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'E');
        if (res != u8"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'O');
        if (res != u8"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p');
        if (res != u8"%p") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'E');
        if (res != u8"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'O');
        if (res != u8"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R');
        if (res != u8"%R") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'E');
        if (res != u8"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'O');
        if (res != u8"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r');
        if (res != u8"%r") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'E');
        if (res != u8"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'O');
        if (res != u8"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S');
        if (res != u8"%S") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'E');
        if (res != u8"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'O');
        if (res != u8"%OS") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X');
        if (res != u8"%X") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'E');
        if (res != u8"%EX") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'O');
        if (res != u8"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T');
        if (res != u8"%T") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'E');
        if (res != u8"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'O');
        if (res != u8"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8't');
        if (res != u8"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'E');
        if (res != u8"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'O');
        if (res != u8"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'E');
        if (res != u8"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'O');
        if (res != u8"三") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U');
        if (res != u8"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'E');
        if (res != u8"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'O');
        if (res != u8"三十五") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'E');
        if (res != u8"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'O');
        if (res != u8"三十六") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'E');
        if (res != u8"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'O');
        if (res != u8"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G');
        if (res != u8"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'E');
        if (res != u8"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'O');
        if (res != u8"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'E');
        if (res != u8"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'O');
        if (res != u8"三十六") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'E');
        if (res != u8"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'O');
        if (res != u8"三") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y');
        if (res != u8"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'E');
        if (res != u8"令和6年") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'O');
        if (res != u8"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'E');
        if (res != u8"6") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'O');
        if (res != u8"二十四") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z');
        VERIFY(res == u8"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'E');
        if (res != u8"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'O');
        if (res != u8"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z');
        VERIFY(res == u8"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'E');
        if (res != u8"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'O');
        if (res != u8"%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (res != u8"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a');
        if (res != u8"%a") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'E');
        if (res != u8"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'O');
        if (res != u8"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A');
        if (res != u8"%A") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'E');
        if (res != u8"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'O');
        if (res != u8"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b');
        if (res != u8"%b") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'E');
        if (res != u8"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'O');
        if (res != u8"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h');
        if (res != u8"%h") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'E');
        if (res != u8"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'O');
        if (res != u8"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B');
        if (res != u8"%B") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'E');
        if (res != u8"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'O');
        if (res != u8"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c');
        if (res != u8"%c") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'E');
        if (res != u8"%Ec") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'O');
        if (res != u8"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x');
        if (res != u8"%x") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'E');
        if (res != u8"%Ex") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'O');
        if (res != u8"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D');
        if (res != u8"%D") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'E');
        if (res != u8"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'O');
        if (res != u8"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd');
        if (res != u8"%d") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'E');
        if (res != u8"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'O');
        if (res != u8"%Od") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e');
        if (res != u8"%e") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'E');
        if (res != u8"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'O');
        if (res != u8"%Oe") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F');
        if (res != u8"%F") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'E');
        if (res != u8"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'O');
        if (res != u8"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H');
        if (res != u8"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'E');
        if (res != u8"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'O');
        if (res != u8"十三") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I');
        if (res != u8"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'E');
        if (res != u8"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'O');
        if (res != u8"一") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j');
        if (res != u8"%j") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'E');
        if (res != u8"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'O');
        if (res != u8"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M');
        if (res != u8"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'E');
        if (res != u8"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'O');
        if (res != u8"三十三") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm');
        if (res != u8"%m") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'E');
        if (res != u8"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'O');
        if (res != u8"%Om") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n');
        if (res != u8"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'E');
        if (res != u8"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'O');
        if (res != u8"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p');
        if (res != u8"午後") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'E');
        if (res != u8"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'O');
        if (res != u8"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R');
        if (res != u8"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'E');
        if (res != u8"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'O');
        if (res != u8"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r');
        if (res != u8"午後01時33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'E');
        if (res != u8"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'O');
        if (res != u8"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S');
        if (res != u8"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'E');
        if (res != u8"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'O');
        if (res != u8"十八") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X');
        if (res != u8"13時33分18秒") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'E');
        if (res != u8"13時33分18秒") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'O');
        if (res != u8"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T');
        if (res != u8"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'E');
        if (res != u8"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'O');
        if (res != u8"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8't');
        if (res != u8"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'E');
        if (res != u8"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'O');
        if (res != u8"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u');
        if (res != u8"%u") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'E');
        if (res != u8"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'O');
        if (res != u8"%Ou") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U');
        if (res != u8"%U") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'E');
        if (res != u8"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'O');
        if (res != u8"%OU") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V');
        if (res != u8"%V") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'E');
        if (res != u8"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'O');
        if (res != u8"%OV") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g');
        if (res != u8"%g") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'E');
        if (res != u8"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'O');
        if (res != u8"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G');
        if (res != u8"%G") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'E');
        if (res != u8"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'O');
        if (res != u8"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W');
        if (res != u8"%W") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'E');
        if (res != u8"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'O');
        if (res != u8"%OW") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w');
        if (res != u8"%w") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'E');
        if (res != u8"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'O');
        if (res != u8"%Ow") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y');
        if (res != u8"%Y") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'E');
        if (res != u8"%EY") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'O');
        if (res != u8"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y');
        if (res != u8"%y") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'E');
        if (res != u8"%Ey") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'O');
        if (res != u8"%Oy") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z'); VERIFY(res == u8"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'E');
        if (res != u8"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'O');
        if (res != u8"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z'); VERIFY(res == u8"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'E');
        if (res != u8"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'O');
        if (res != u8"%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (res != u8"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a');
        if (res != u8"水") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'E');
        if (res != u8"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'a', u8'O');
        if (res != u8"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A');
        if (res != u8"水曜日") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'E');
        if (res != u8"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'A', u8'O');
        if (res != u8"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b');
        if (res != u8" 9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'E');
        if (res != u8"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'b', u8'O');
        if (res != u8"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h');
        if (res != u8" 9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'E');
        if (res != u8"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'h', u8'O');
        if (res != u8"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B');
        if (res != u8"9月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'E');
        if (res != u8"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'B', u8'O');
        if (res != u8"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c');
        if (res != u8"2024年09月04日 13時33分18秒") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'E');
        if (res != u8"令和6年09月04日 13時33分18秒") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'c', u8'O');
        if (res != u8"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C');
        if (res != u8"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'E');
        if (res != u8"令和") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'C', u8'O');
        if (res != u8"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x');
        if (res != u8"2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'E');
        if (res != u8"令和6年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'x', u8'O');
        if (res != u8"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D');
        if (res != u8"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'E');
        if (res != u8"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'D', u8'O');
        if (res != u8"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd');
        if (res != u8"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'E');
        if (res != u8"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'd', u8'O');
        if (res != u8"四") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e');
        if (res != u8" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'E');
        if (res != u8"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'e', u8'O');
        if (res != u8"四") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F');
        if (res != u8"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'E');
        if (res != u8"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'F', u8'O');
        if (res != u8"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H');
        if (res != u8"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'E');
        if (res != u8"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'H', u8'O');
        if (res != u8"十三") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I');
        if (res != u8"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'E');
        if (res != u8"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'I', u8'O');
        if (res != u8"一") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j');
        if (res != u8"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'E');
        if (res != u8"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'j', u8'O');
        if (res != u8"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M');
        if (res != u8"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'E');
        if (res != u8"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'M', u8'O');
        if (res != u8"三十三") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm');
        if (res != u8"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'E');
        if (res != u8"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'm', u8'O');
        if (res != u8"九") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n');
        if (res != u8"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'E');
        if (res != u8"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'n', u8'O');
        if (res != u8"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p');
        if (res != u8"午後") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'E');
        if (res != u8"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'p', u8'O');
        if (res != u8"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R');
        if (res != u8"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'E');
        if (res != u8"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'R', u8'O');
        if (res != u8"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r');
        if (res != u8"午後01時33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'E');
        if (res != u8"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'r', u8'O');
        if (res != u8"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S');
        if (res != u8"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'E');
        if (res != u8"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'S', u8'O');
        if (res != u8"十八") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X');
        if (res != u8"13時33分18秒") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'E');
        if (res != u8"13時33分18秒") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'X', u8'O');
        if (res != u8"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T');
        if (res != u8"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'E');
        if (res != u8"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'T', u8'O');
        if (res != u8"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8't');
        if (res != u8"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'E');
        if (res != u8"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, u8't', u8'O');
        if (res != u8"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'E');
        if (res != u8"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'u', u8'O');
        if (res != u8"三") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U');
        if (res != u8"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'E');
        if (res != u8"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'U', u8'O');
        if (res != u8"三十五") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'E');
        if (res != u8"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'V', u8'O');
        if (res != u8"三十六") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'E');
        if (res != u8"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'g', u8'O');
        if (res != u8"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G');
        if (res != u8"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'E');
        if (res != u8"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'G', u8'O');
        if (res != u8"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W');
        if (res != u8"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'E');
        if (res != u8"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'W', u8'O');
        if (res != u8"三十六") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w');
        if (res != u8"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'E');
        if (res != u8"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'w', u8'O');
        if (res != u8"三") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y');
        if (res != u8"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'E');
        if (res != u8"令和6年") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Y', u8'O');
        if (res != u8"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y');
        if (res != u8"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'E');
        if (res != u8"6") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'y', u8'O');
        if (res != u8"二十四") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z'); VERIFY(res == u8"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'E');
        if (res != u8"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'Z', u8'O');
        if (res != u8"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z'); VERIFY(res == u8"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'E');
        if (res != u8"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, u8'z', u8'O');
        if (res != u8"%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_year != 114) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_mday != 14) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_hour != 1) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_min != 9) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_sec != 35) throw std::runtime_error("timeio::get_4 fail.");
    }

    {
        std::u8string input = u8"2020  ";
        std::u8string format = u8"%Y";
        
        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret == input.end()) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_year != 120) throw std::runtime_error("timeio::get_4 fail.");
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
        if (time.tm_year != 120) throw std::runtime_error("timeio::get_4 fail.");
        if (ret != input.end()) throw std::runtime_error("timeio::get_4 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_year != 114) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_wday != 1) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_mday != 14) throw std::runtime_error("timeio::get_5 fail.");
    }
    {
        std::u8string input = u8"Mittwoch";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, 'A');
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_5 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 1) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"Tue ";
        std::u8string format = u8"%a";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 2) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"Wednesday";
        std::u8string format = u8"%a";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"Thu";
        std::u8string format = u8"%A";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"Fri ";
        std::u8string format = u8"%A";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"Saturday";
        std::u8string format = u8"%A";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 6) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"Feb";
        std::u8string format = u8"%b";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 1) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"Mar ";
        std::u8string format = u8"%b";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 2) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"April";
        std::u8string format = u8"%b";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"May";
        std::u8string format = u8"%B";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"Jun ";
        std::u8string format = u8"%B";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"July";
        std::u8string format = u8"%B";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 6) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"Aug";
        std::u8string format = u8"%h";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 7) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"May ";
        std::u8string format = u8"%h";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"October";
        std::u8string format = u8"%h";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 9) throw std::runtime_error("timeio::get_6 fail.");
    }

    // Other tests.
    {
        std::u8string input = u8"2.";
        std::u8string format = u8"%d.";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mday != 2) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mday != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"06.";
        std::u8string format = u8"%e.";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mday != 6) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 0) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 0) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"12:37AM";
        std::u8string format = u8"%I:%M%p";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 0) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 37) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"01:25AM";
        std::u8string format = u8"%I:%M%p";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 1) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 25) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"12:00PM";
        std::u8string format = u8"%I:%M%p";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 12) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 0) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"12:42PM";
        std::u8string format = u8"%I:%M%p";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 12) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 42) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"07:23PM";
        std::u8string format = u8"%I:%M%p";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 19) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 23) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u8string input = u8"17%20";
        std::u8string format = u8"%H%%%M";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 17) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 20) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mon != 10) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_hour != 13) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_min != 38) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_sec != 12) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"05 37";
        std::u8string format = u8"%C %y";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 537 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"68";
        std::u8string format = u8"%y";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2068 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"69";
        std::u8string format = u8"%y";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 1969 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"03-Feb-2003";
        std::u8string format = u8"%d-%b-%Y";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2003 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 1) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 1) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 33) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"16-Dec-2020";
        std::u8string format = u8"%d-%b-%Y";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2020 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 11) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 16) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 350) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"16-Dec-2021";
        std::u8string format = u8"%d-%b-%Y";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2021 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 11) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 16) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 4) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 349) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"253 2020";
        std::u8string format = u8"%j %Y";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2020 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 8) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 9) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 252) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"233 2021";
        std::u8string format = u8"%j %Y";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2021 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 7) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 21) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 6) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 232) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"2020 23 3";
        std::u8string format = u8"%Y %U %w";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2020 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 10) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 161) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"2020 23 3";
        std::u8string format = u8"%Y %W %w";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2020 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 10) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 161) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"2021 43 Fri";
        std::u8string format = u8"%Y %W %a";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2021 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 9) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 29) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 5) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 301) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"2024 23 3";
        std::u8string format = u8"%Y %U %w";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2024 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 12) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 163) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u8string input = u8"2024 23 3";
        std::u8string format = u8"%Y %W %w";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2024 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 5) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 156) throw std::runtime_error("timeio::get_7 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_hour != 13) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_min != 38) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_sec != 12) throw std::runtime_error("timeio::get_8 fail.");
    }
        
    {
        std::u8string input = u8"11:17:42 PM";
        std::u8string format = u8"%r";

        IOv2::time_parse_context<char8_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_hour != 23) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_min != 17) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_sec != 42) throw std::runtime_error("timeio::get_8 fail.");
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