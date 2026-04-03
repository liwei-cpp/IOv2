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
        if (res != U"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'a');
        if (res != U"Wed") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'E');
        if (res != U"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'O');
        if (res != U"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'A');
        if (res != U"Wednesday") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'E');
        if (res != U"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'O');
        if (res != U"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'b');
        if (res != U"Sep") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'E');
        if (res != U"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'O');
        if (res != U"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'h');
        if (res != U"Sep") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'E');
        if (res != U"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'O');
        if (res != U"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'B');
        if (res != U"September") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'E');
        if (res != U"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'O');
        if (res != U"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'c');
        if (res != U"09/04/24 13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'E');
        if (res != U"09/04/24 13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'O');
        if (res != U"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'C');
        if (res != U"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'E');
        if (res != U"20") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'O');
        if (res != U"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'x');
        if (res != U"09/04/24") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'E');
        if (res != U"09/04/24") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'O');
        if (res != U"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'D');
        if (res != U"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'E');
        if (res != U"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'O');
        if (res != U"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'd');
        if (res != U"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'E');
        if (res != U"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'O');
        if (res != U"04") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'e');
        if (res != U" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'E');
        if (res != U"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'O');
        if (res != U" 4") throw std::runtime_error("timeio::put fail for Oe");
    }
      
    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'F');
        if (res != U"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'E');
        if (res != U"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'O');
        if (res != U"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'H');
        if (res != U"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'E');
        if (res != U"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'O');
        if (res != U"13") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'I');
        if (res != U"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'E');
        if (res != U"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'O');
        if (res != U"01") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'j');
        if (res != U"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'E');
        if (res != U"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'O');
        if (res != U"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'M');
        if (res != U"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'E');
        if (res != U"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'O');
        if (res != U"33") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'm');
        if (res != U"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'E');
        if (res != U"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'O');
        if (res != U"09") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'n');
        if (res != U"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'E');
        if (res != U"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'O');
        if (res != U"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'p');
        if (res != U"PM") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'E');
        if (res != U"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'O');
        if (res != U"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'R');
        if (res != U"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'E');
        if (res != U"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'O');
        if (res != U"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'r');
        if (res != U"01:33:18 PM") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'E');
        if (res != U"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'O');
        if (res != U"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'S');
        if (res != U"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'E');
        if (res != U"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'O');
        if (res != U"18") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'X');
        if (res != U"13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'E');
        if (res != U"13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'O');
        if (res != U"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'T');
        if (res != U"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'E');
        if (res != U"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'O');
        if (res != U"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U't');
        if (res != U"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'E');
        if (res != U"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'O');
        if (res != U"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'u');
        if (res != U"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'E');
        if (res != U"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'O');
        if (res != U"3") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'U');
        if (res != U"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'E');
        if (res != U"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'O');
        if (res != U"35") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'V');
        if (res != U"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'E');
        if (res != U"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'O');
        if (res != U"36") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'g');
        if (res != U"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'E');
        if (res != U"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'O');
        if (res != U"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'G');
        if (res != U"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'E');
        if (res != U"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'O');
        if (res != U"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'W');
        if (res != U"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'E');
        if (res != U"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'O');
        if (res != U"36") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'w');
        if (res != U"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'E');
        if (res != U"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'O');
        if (res != U"3") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y');
        if (res != U"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'E');
        if (res != U"2024") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'O');
        if (res != U"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'y');
        if (res != U"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'E');
        if (res != U"24") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'O');
        if (res != U"24") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z');
        VERIFY(res == U"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'E');
        if (res != U"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'O');
        if (res != U"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'z');
        VERIFY(res == U"-0700");
        if (res.empty()) throw std::runtime_error("timeio::put fail for z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'E');
        if (res != U"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'O');
        if (res != U"%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (res != U"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'a');
        if (res != U"三") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'E');
        if (res != U"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'O');
        if (res != U"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'A');
        if (res != U"星期三") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'E');
        if (res != U"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'O');
        if (res != U"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'b');
        if (res != U"9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'E');
        if (res != U"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'O');
        if (res != U"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'h');
        if (res != U"9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'E');
        if (res != U"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'O');
        if (res != U"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'B');
        if (res != U"九月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'E');
        if (res != U"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'O');
        if (res != U"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'c');
        if (res != U"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'E');
        if (res != U"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'O');
        if (res != U"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'C');
        if (res != U"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'E');
        if (res != U"20") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'O');
        if (res != U"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'x');
        if (res != U"2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'E');
        if (res != U"2024年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'O');
        if (res != U"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'D');
        if (res != U"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'E');
        if (res != U"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'O');
        if (res != U"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'd');
        if (res != U"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'E');
        if (res != U"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'O');
        if (res != U"04") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'e');
        if (res != U" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'E');
        if (res != U"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'O');
        if (res != U" 4") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'F');
        if (res != U"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'E');
        if (res != U"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'O');
        if (res != U"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'H');
        if (res != U"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'E');
        if (res != U"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'O');
        if (res != U"13") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'I');
        if (res != U"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'E');
        if (res != U"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'O');
        if (res != U"01") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'j');
        if (res != U"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'E');
        if (res != U"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'O');
        if (res != U"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'M');
        if (res != U"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'E');
        if (res != U"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'O');
        if (res != U"33") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'm');
        if (res != U"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'E');
        if (res != U"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'O');
        if (res != U"09") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'n');
        if (res != U"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'E');
        if (res != U"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'O');
        if (res != U"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'p');
        if (res != U"下午") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'E');
        if (res != U"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'O');
        if (res != U"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'R');
        if (res != U"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'E');
        if (res != U"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'O');
        if (res != U"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'r');
        if (res != U"下午 01时33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'E');
        if (res != U"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'O');
        if (res != U"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'S');
        if (res != U"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'E');
        if (res != U"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'O');
        if (res != U"18") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'X');
        if (res != U"13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'E');
        if (res != U"13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'O');
        if (res != U"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'T');
        if (res != U"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'E');
        if (res != U"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'O');
        if (res != U"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U't');
        if (res != U"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'E');
        if (res != U"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'O');
        if (res != U"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'u');
        if (res != U"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'E');
        if (res != U"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'O');
        if (res != U"3") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'U');
        if (res != U"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'E');
        if (res != U"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'O');
        if (res != U"35") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'V');
        if (res != U"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'E');
        if (res != U"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'O');
        if (res != U"36") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'g');
        if (res != U"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'E');
        if (res != U"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'O');
        if (res != U"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'G');
        if (res != U"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'E');
        if (res != U"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'O');
        if (res != U"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'W');
        if (res != U"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'E');
        if (res != U"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'O');
        if (res != U"36") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'w');
        if (res != U"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'E');
        if (res != U"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'O');
        if (res != U"3") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y');
        if (res != U"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'E');
        if (res != U"2024") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'O');
        if (res != U"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'y');
        if (res != U"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'E');
        if (res != U"24") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'O');
        if (res != U"24") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z');
        VERIFY(res == U"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'E');
        if (res != U"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'O');
        if (res != U"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'z');
        VERIFY(res == U"-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'E');
        if (res != U"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'O');
        if (res != U"%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (res != U"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'a');
        if (res != U"水") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'E');
        if (res != U"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'O');
        if (res != U"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'A');
        if (res != U"水曜日") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'E');
        if (res != U"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'O');
        if (res != U"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'b');
        if (res != U" 9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'E');
        if (res != U"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'O');
        if (res != U"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'h');
        if (res != U" 9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'E');
        if (res != U"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'O');
        if (res != U"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'B');
        if (res != U"9月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'E');
        if (res != U"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'O');
        if (res != U"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'c');
        if (res != U"2024年09月04日 13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'E');
        if (res != U"令和6年09月04日 13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'O');
        if (res != U"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'C');
        if (res != U"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'E');
        if (res != U"令和") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'O');
        if (res != U"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'x');
        if (res != U"2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'E');
        if (res != U"令和6年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'O');
        if (res != U"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'D');
        if (res != U"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'E');
        if (res != U"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'O');
        if (res != U"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'd');
        if (res != U"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'E');
        if (res != U"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'O');
        if (res != U"四") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'e');
        if (res != U" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'E');
        if (res != U"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'O');
        if (res != U"四") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'F');
        if (res != U"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'E');
        if (res != U"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'O');
        if (res != U"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'H');
        if (res != U"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'E');
        if (res != U"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'O');
        if (res != U"十三") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'I');
        if (res != U"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'E');
        if (res != U"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'O');
        if (res != U"一") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'j');
        if (res != U"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'E');
        if (res != U"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'O');
        if (res != U"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'M');
        if (res != U"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'E');
        if (res != U"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'O');
        if (res != U"三十三") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'm');
        if (res != U"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'E');
        if (res != U"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'O');
        if (res != U"九") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'n');
        if (res != U"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'E');
        if (res != U"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'O');
        if (res != U"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'p');
        if (res != U"午後") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'E');
        if (res != U"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'O');
        if (res != U"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'R');
        if (res != U"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'E');
        if (res != U"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'O');
        if (res != U"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'r');
        if (res != U"午後01時33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'E');
        if (res != U"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'O');
        if (res != U"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'S');
        if (res != U"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'E');
        if (res != U"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'O');
        if (res != U"十八") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'X');
        if (res != U"13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'E');
        if (res != U"13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'O');
        if (res != U"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'T');
        if (res != U"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'E');
        if (res != U"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'O');
        if (res != U"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U't');
        if (res != U"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'E');
        if (res != U"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'O');
        if (res != U"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'u');
        if (res != U"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'E');
        if (res != U"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'O');
        if (res != U"三") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'U');
        if (res != U"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'E');
        if (res != U"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'O');
        if (res != U"三十五") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'V');
        if (res != U"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'E');
        if (res != U"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'O');
        if (res != U"三十六") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'g');
        if (res != U"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'E');
        if (res != U"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'O');
        if (res != U"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'G');
        if (res != U"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'E');
        if (res != U"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'O');
        if (res != U"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'W');
        if (res != U"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'E');
        if (res != U"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'O');
        if (res != U"三十六") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'w');
        if (res != U"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'E');
        if (res != U"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'O');
        if (res != U"三") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y');
        if (res != U"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'E');
        if (res != U"令和6年") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'O');
        if (res != U"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'y');
        if (res != U"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'E');
        if (res != U"6") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'O');
        if (res != U"二十四") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z');
        VERIFY(res == U"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'E');
        if (res != U"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'O');
        if (res != U"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'z');
        VERIFY(res == U"-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'E');
        if (res != U"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'O');
        if (res != U"%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (oss != U"Sun") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, U'x');
        if (oss != U"04/04/71") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, U'X');
        if (oss != U"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, U'x', U'E');
        if (oss != U"04/04/71") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, U'X', U'E');
        if (oss != U"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
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
        if ((oss != U"Son") && (oss != U"So")) throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'x');
        if (oss != U"04.04.1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'X');
        if (oss != U"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'x', U'E');
        if (oss != U"04.04.1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'X', U'E');
        if (oss != U"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
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
        if (oss != U"Sun") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'x');
        if (oss != U"Sunday, April 04, 1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'X');
        if (oss.find(U"12:00:00") == std::u32string::npos) throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'x', U'E');
        if (oss != U"Sunday, April 04, 1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'X', U'E');
        if (oss.find(U"12:00:00") == std::u32string::npos) throw std::runtime_error("timeio::put fail");
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
        if (oss != U"dom") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'x');
        if (oss != U"04/04/71") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'X');
        if (oss != U"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'x', U'E');
        if (oss != U"04/04/71") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, U'X', U'E');
        if (oss != U"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
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
        if (oss != U"Sunday, the second of April") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
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
        if (oss != U"Sonntag, the second of April") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
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
        if (oss != U"Sunday, the second of April") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
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
        if (oss != U"dimanche, the second of avril") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
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
    if (res != U"12:00:00, Thursday, the second of June, 1997xxxxxx") throw std::runtime_error("timeio::put fail");
    if (sanity1 != U"12:00:00, Thursday, the second of June, 1997") throw std::runtime_error("timeio::put fail");

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
    if (res != U"Tuesdayxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx") throw std::runtime_error("timeio::put fail");
    if (sanity1 != U"Tuesday") throw std::runtime_error("timeio::put fail");

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

    if (IOv2::to_u32string(time_buffer, "ta_IN.UTF-8") + std::u32string(U" America/Los_Angeles") != res) throw std::runtime_error("timeio::put fail");

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
        if (res != U"水") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'E');
        if (res != U"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'O');
        if (res != U"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'A');
        if (res != U"水曜日") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'E');
        if (res != U"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'O');
        if (res != U"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'b');
        if (res != U" 9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'E');
        if (res != U"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'O');
        if (res != U"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'h');
        if (res != U" 9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'E');
        if (res != U"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'O');
        if (res != U"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'B');
        if (res != U"9月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'E');
        if (res != U"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'O');
        if (res != U"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'c');
        if (res != U"%c") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'E');
        if (res != U"%Ec") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'O');
        if (res != U"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'C');
        if (res != U"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'E');
        if (res != U"令和") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'O');
        if (res != U"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'x');
        if (res != U"2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'E');
        if (res != U"令和6年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'O');
        if (res != U"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'D');
        if (res != U"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'E');
        if (res != U"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'O');
        if (res != U"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'd');
        if (res != U"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'E');
        if (res != U"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'O');
        if (res != U"四") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'e');
        if (res != U" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'E');
        if (res != U"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'O');
        if (res != U"四") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'F');
        if (res != U"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'E');
        if (res != U"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'O');
        if (res != U"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'H');
        if (res != U"%H") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'E');
        if (res != U"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'O');
        if (res != U"%OH") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'I');
        if (res != U"%I") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'E');
        if (res != U"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'O');
        if (res != U"%OI") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'j');
        if (res != U"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'E');
        if (res != U"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'O');
        if (res != U"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'M');
        if (res != U"%M") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'E');
        if (res != U"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'O');
        if (res != U"%OM") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'm');
        if (res != U"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'E');
        if (res != U"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'O');
        if (res != U"九") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'n');
        if (res != U"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'E');
        if (res != U"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'O');
        if (res != U"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'p');
        if (res != U"%p") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'E');
        if (res != U"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'O');
        if (res != U"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'R');
        if (res != U"%R") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'E');
        if (res != U"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'O');
        if (res != U"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'r');
        if (res != U"%r") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'E');
        if (res != U"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'O');
        if (res != U"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'S');
        if (res != U"%S") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'E');
        if (res != U"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'O');
        if (res != U"%OS") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'X');
        if (res != U"%X") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'E');
        if (res != U"%EX") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'O');
        if (res != U"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'T');
        if (res != U"%T") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'E');
        if (res != U"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'O');
        if (res != U"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U't');
        if (res != U"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'E');
        if (res != U"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'O');
        if (res != U"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'u');
        if (res != U"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'E');
        if (res != U"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'O');
        if (res != U"三") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'U');
        if (res != U"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'E');
        if (res != U"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'O');
        if (res != U"三十五") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'V');
        if (res != U"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'E');
        if (res != U"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'O');
        if (res != U"三十六") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'g');
        if (res != U"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'E');
        if (res != U"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'O');
        if (res != U"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'G');
        if (res != U"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'E');
        if (res != U"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'O');
        if (res != U"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'W');
        if (res != U"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'E');
        if (res != U"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'O');
        if (res != U"三十六") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'w');
        if (res != U"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'E');
        if (res != U"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'O');
        if (res != U"三") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y');
        if (res != U"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'E');
        if (res != U"令和6年") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'O');
        if (res != U"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'y');
        if (res != U"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'E');
        if (res != U"6") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'O');
        if (res != U"二十四") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z');
        VERIFY(res == U"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'E');
        if (res != U"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'O');
        if (res != U"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'z');
        VERIFY(res == U"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'E');
        if (res != U"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'O');
        if (res != U"%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (res != U"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'a');
        if (res != U"%a") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'E');
        if (res != U"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'O');
        if (res != U"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'A');
        if (res != U"%A") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'E');
        if (res != U"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'O');
        if (res != U"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'b');
        if (res != U"%b") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'E');
        if (res != U"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'O');
        if (res != U"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'h');
        if (res != U"%h") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'E');
        if (res != U"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'O');
        if (res != U"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'B');
        if (res != U"%B") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'E');
        if (res != U"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'O');
        if (res != U"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'c');
        if (res != U"%c") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'E');
        if (res != U"%Ec") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'O');
        if (res != U"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'x');
        if (res != U"%x") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'E');
        if (res != U"%Ex") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'O');
        if (res != U"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'D');
        if (res != U"%D") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'E');
        if (res != U"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'O');
        if (res != U"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'd');
        if (res != U"%d") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'E');
        if (res != U"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'O');
        if (res != U"%Od") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'e');
        if (res != U"%e") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'E');
        if (res != U"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'O');
        if (res != U"%Oe") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'F');
        if (res != U"%F") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'E');
        if (res != U"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'O');
        if (res != U"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'H');
        if (res != U"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'E');
        if (res != U"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'O');
        if (res != U"十三") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'I');
        if (res != U"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'E');
        if (res != U"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'O');
        if (res != U"一") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'j');
        if (res != U"%j") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'E');
        if (res != U"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'O');
        if (res != U"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'M');
        if (res != U"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'E');
        if (res != U"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'O');
        if (res != U"三十三") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'm');
        if (res != U"%m") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'E');
        if (res != U"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'O');
        if (res != U"%Om") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'n');
        if (res != U"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'E');
        if (res != U"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'O');
        if (res != U"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'p');
        if (res != U"午後") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'E');
        if (res != U"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'O');
        if (res != U"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'R');
        if (res != U"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'E');
        if (res != U"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'O');
        if (res != U"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'r');
        if (res != U"午後01時33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'E');
        if (res != U"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'O');
        if (res != U"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'S');
        if (res != U"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'E');
        if (res != U"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'O');
        if (res != U"十八") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'X');
        if (res != U"13時33分18秒") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'E');
        if (res != U"13時33分18秒") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'O');
        if (res != U"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'T');
        if (res != U"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'E');
        if (res != U"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'O');
        if (res != U"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U't');
        if (res != U"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'E');
        if (res != U"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'O');
        if (res != U"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'u');
        if (res != U"%u") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'E');
        if (res != U"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'O');
        if (res != U"%Ou") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'U');
        if (res != U"%U") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'E');
        if (res != U"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'O');
        if (res != U"%OU") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'V');
        if (res != U"%V") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'E');
        if (res != U"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'O');
        if (res != U"%OV") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'g');
        if (res != U"%g") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'E');
        if (res != U"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'O');
        if (res != U"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'G');
        if (res != U"%G") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'E');
        if (res != U"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'O');
        if (res != U"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'W');
        if (res != U"%W") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'E');
        if (res != U"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'O');
        if (res != U"%OW") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'w');
        if (res != U"%w") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'E');
        if (res != U"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'O');
        if (res != U"%Ow") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y');
        if (res != U"%Y") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'E');
        if (res != U"%EY") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'O');
        if (res != U"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'y');
        if (res != U"%y") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'E');
        if (res != U"%Ey") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'O');
        if (res != U"%Oy") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z'); VERIFY(res == U"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'E');
        if (res != U"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'O');
        if (res != U"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'z'); VERIFY(res == U"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'E');
        if (res != U"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'O');
        if (res != U"%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (res != U"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'a');
        if (res != U"水") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'E');
        if (res != U"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, U'a', U'O');
        if (res != U"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'A');
        if (res != U"水曜日") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'E');
        if (res != U"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, U'A', U'O');
        if (res != U"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'b');
        if (res != U" 9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'E');
        if (res != U"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, U'b', U'O');
        if (res != U"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'h');
        if (res != U" 9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'E');
        if (res != U"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, U'h', U'O');
        if (res != U"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'B');
        if (res != U"9月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'E');
        if (res != U"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, U'B', U'O');
        if (res != U"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'c');
        if (res != U"2024年09月04日 13時33分18秒") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'E');
        if (res != U"令和6年09月04日 13時33分18秒") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, U'c', U'O');
        if (res != U"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'C');
        if (res != U"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'E');
        if (res != U"令和") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, U'C', U'O');
        if (res != U"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'x');
        if (res != U"2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'E');
        if (res != U"令和6年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, U'x', U'O');
        if (res != U"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'D');
        if (res != U"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'E');
        if (res != U"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, U'D', U'O');
        if (res != U"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'd');
        if (res != U"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'E');
        if (res != U"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, U'd', U'O');
        if (res != U"四") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'e');
        if (res != U" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'E');
        if (res != U"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, U'e', U'O');
        if (res != U"四") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'F');
        if (res != U"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'E');
        if (res != U"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, U'F', U'O');
        if (res != U"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'H');
        if (res != U"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'E');
        if (res != U"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, U'H', U'O');
        if (res != U"十三") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'I');
        if (res != U"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'E');
        if (res != U"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, U'I', U'O');
        if (res != U"一") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'j');
        if (res != U"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'E');
        if (res != U"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, U'j', U'O');
        if (res != U"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'M');
        if (res != U"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'E');
        if (res != U"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, U'M', U'O');
        if (res != U"三十三") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'm');
        if (res != U"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'E');
        if (res != U"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, U'm', U'O');
        if (res != U"九") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'n');
        if (res != U"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'E');
        if (res != U"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, U'n', U'O');
        if (res != U"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'p');
        if (res != U"午後") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'E');
        if (res != U"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, U'p', U'O');
        if (res != U"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'R');
        if (res != U"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'E');
        if (res != U"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, U'R', U'O');
        if (res != U"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'r');
        if (res != U"午後01時33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'E');
        if (res != U"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, U'r', U'O');
        if (res != U"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'S');
        if (res != U"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'E');
        if (res != U"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, U'S', U'O');
        if (res != U"十八") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'X');
        if (res != U"13時33分18秒") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'E');
        if (res != U"13時33分18秒") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, U'X', U'O');
        if (res != U"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'T');
        if (res != U"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'E');
        if (res != U"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, U'T', U'O');
        if (res != U"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U't');
        if (res != U"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'E');
        if (res != U"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, U't', U'O');
        if (res != U"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'u');
        if (res != U"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'E');
        if (res != U"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, U'u', U'O');
        if (res != U"三") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'U');
        if (res != U"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'E');
        if (res != U"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, U'U', U'O');
        if (res != U"三十五") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'V');
        if (res != U"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'E');
        if (res != U"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, U'V', U'O');
        if (res != U"三十六") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'g');
        if (res != U"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'E');
        if (res != U"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, U'g', U'O');
        if (res != U"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'G');
        if (res != U"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'E');
        if (res != U"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, U'G', U'O');
        if (res != U"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'W');
        if (res != U"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'E');
        if (res != U"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, U'W', U'O');
        if (res != U"三十六") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'w');
        if (res != U"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'E');
        if (res != U"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, U'w', U'O');
        if (res != U"三") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y');
        if (res != U"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'E');
        if (res != U"令和6年") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Y', U'O');
        if (res != U"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'y');
        if (res != U"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'E');
        if (res != U"6") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, U'y', U'O');
        if (res != U"二十四") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z'); VERIFY(res == U"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'E');
        if (res != U"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, U'Z', U'O');
        if (res != U"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, U'z'); VERIFY(res == U"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'E');
        if (res != U"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, U'z', U'O');
        if (res != U"%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_year != 114) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_mday != 14) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_hour != 1) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_min != 9) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_sec != 35) throw std::runtime_error("timeio::get_4 fail.");
    }

    {
        std::u32string input = U"2020  ";
        std::u32string format = U"%Y";
        
        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret == input.end()) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_year != 120) throw std::runtime_error("timeio::get_4 fail.");
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
        if (time.tm_year != 120) throw std::runtime_error("timeio::get_4 fail.");
        if (ret != input.end()) throw std::runtime_error("timeio::get_4 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_year != 114) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_wday != 1) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_mday != 14) throw std::runtime_error("timeio::get_5 fail.");
    }
    {
        std::u32string input = U"Mittwoch";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, 'A');
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_5 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 1) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"Tue ";
        std::u32string format = U"%a";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 2) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"Wednesday";
        std::u32string format = U"%a";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"Thu";
        std::u32string format = U"%A";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"Fri ";
        std::u32string format = U"%A";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"Saturday";
        std::u32string format = U"%A";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 6) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"Feb";
        std::u32string format = U"%b";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 1) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"Mar ";
        std::u32string format = U"%b";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 2) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"April";
        std::u32string format = U"%b";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"May";
        std::u32string format = U"%B";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"Jun ";
        std::u32string format = U"%B";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"July";
        std::u32string format = U"%B";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 6) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"Aug";
        std::u32string format = U"%h";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 7) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"May ";
        std::u32string format = U"%h";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"October";
        std::u32string format = U"%h";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 9) throw std::runtime_error("timeio::get_6 fail.");
    }

    // Other tests.
    {
        std::u32string input = U"2.";
        std::u32string format = U"%d.";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mday != 2) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mday != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"06.";
        std::u32string format = U"%e.";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mday != 6) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 0) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 0) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"12:37AM";
        std::u32string format = U"%I:%M%p";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 0) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 37) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"01:25AM";
        std::u32string format = U"%I:%M%p";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 1) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 25) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"12:00PM";
        std::u32string format = U"%I:%M%p";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 12) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 0) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"12:42PM";
        std::u32string format = U"%I:%M%p";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 12) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 42) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"07:23PM";
        std::u32string format = U"%I:%M%p";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 19) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 23) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::u32string input = U"17%20";
        std::u32string format = U"%H%%%M";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 17) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 20) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mon != 10) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_hour != 13) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_min != 38) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_sec != 12) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u32string input = U"05 37";
        std::u32string format = U"%C %y";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 537 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u32string input = U"68";
        std::u32string format = U"%y";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2068 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u32string input = U"69";
        std::u32string format = U"%y";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 1969 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::u32string input = U"03-Feb-2003";
        std::u32string format = U"%d-%b-%Y";

        IOv2::time_parse_context<char32_t> ctx;
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
        std::u32string input = U"16-Dec-2020";
        std::u32string format = U"%d-%b-%Y";

        IOv2::time_parse_context<char32_t> ctx;
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
        std::u32string input = U"16-Dec-2021";
        std::u32string format = U"%d-%b-%Y";

        IOv2::time_parse_context<char32_t> ctx;
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
        std::u32string input = U"253 2020";
        std::u32string format = U"%j %Y";

        IOv2::time_parse_context<char32_t> ctx;
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
        std::u32string input = U"233 2021";
        std::u32string format = U"%j %Y";

        IOv2::time_parse_context<char32_t> ctx;
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
        std::u32string input = U"2020 23 3";
        std::u32string format = U"%Y %U %w";

        IOv2::time_parse_context<char32_t> ctx;
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
        std::u32string input = U"2020 23 3";
        std::u32string format = U"%Y %W %w";

        IOv2::time_parse_context<char32_t> ctx;
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
        std::u32string input = U"2021 43 Fri";
        std::u32string format = U"%Y %W %a";

        IOv2::time_parse_context<char32_t> ctx;
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
        std::u32string input = U"2024 23 3";
        std::u32string format = U"%Y %U %w";

        IOv2::time_parse_context<char32_t> ctx;
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
        std::u32string input = U"2024 23 3";
        std::u32string format = U"%Y %W %w";

        IOv2::time_parse_context<char32_t> ctx;
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_hour != 13) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_min != 38) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_sec != 12) throw std::runtime_error("timeio::get_8 fail.");
    }
        
    {
        std::u32string input = U"11:17:42 PM";
        std::u32string format = U"%r";

        IOv2::time_parse_context<char32_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_hour != 23) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_min != 17) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_sec != 42) throw std::runtime_error("timeio::get_8 fail.");
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