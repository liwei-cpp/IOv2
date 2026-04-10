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
        if (res != "%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'a');
        if (res != "Wed") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'E');
        if (res != "%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'O');
        if (res != "%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'A');
        if (res != "Wednesday") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'E');
        if (res != "%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'O');
        if (res != "%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'b');
        if (res != "Sep") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'E');
        if (res != "%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'O');
        if (res != "%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'h');
        if (res != "Sep") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'E');
        if (res != "%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'O');
        if (res != "%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'B');
        if (res != "September") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'E');
        if (res != "%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'O');
        if (res != "%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'c');
        if (res != "09/04/24 13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        if (res != "09/04/24 13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'O');
        if (res != "%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'C');
        if (res != "20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'E');
        if (res != "20") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'O');
        if (res != "%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'x');
        if (res != "09/04/24") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'E');
        if (res != "09/04/24") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'O');
        if (res != "%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'D');
        if (res != "09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'E');
        if (res != "%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'O');
        if (res != "%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'd');
        if (res != "04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'E');
        if (res != "%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'O');
        if (res != "04") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'e');
        if (res != " 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'E');
        if (res != "%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'O');
        if (res != " 4") throw std::runtime_error("timeio::put fail for Oe");
    }
      
    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'F');
        if (res != "2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'E');
        if (res != "%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'O');
        if (res != "%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'H');
        if (res != "13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'E');
        if (res != "%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'O');
        if (res != "13") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'I');
        if (res != "01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'E');
        if (res != "%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'O');
        if (res != "01") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'j');
        if (res != "248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'E');
        if (res != "%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'O');
        if (res != "%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'M');
        if (res != "33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'E');
        if (res != "%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'O');
        if (res != "33") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'm');
        if (res != "09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'E');
        if (res != "%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'O');
        if (res != "09") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'n');
        if (res != "\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'E');
        if (res != "%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'O');
        if (res != "%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'p');
        if (res != "PM") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'E');
        if (res != "%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'O');
        if (res != "%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'R');
        if (res != "13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'E');
        if (res != "%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'O');
        if (res != "%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'r');
        if (res != "01:33:18 PM") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'E');
        if (res != "%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'O');
        if (res != "%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'S');
        if (res != "18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'E');
        if (res != "%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'O');
        if (res != "18") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'X');
        if (res != "13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        if (res != "13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'O');
        if (res != "%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'T');
        if (res != "13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'E');
        if (res != "%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'O');
        if (res != "%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 't');
        if (res != "\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'E');
        if (res != "%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'O');
        if (res != "%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'u');
        if (res != "3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'E');
        if (res != "%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'O');
        if (res != "3") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'U');
        if (res != "35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'E');
        if (res != "%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'O');
        if (res != "35") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'V');
        if (res != "36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'E');
        if (res != "%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'O');
        if (res != "36") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'g');
        if (res != "24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'E');
        if (res != "%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'O');
        if (res != "%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'G');
        if (res != "2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'E');
        if (res != "%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'O');
        if (res != "%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'W');
        if (res != "36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'E');
        if (res != "%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'O');
        if (res != "36") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'w');
        if (res != "3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'E');
        if (res != "%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'O');
        if (res != "3") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y');
        if (res != "2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'E');
        if (res != "2024") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'O');
        if (res != "%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'y');
        if (res != "24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'E');
        if (res != "24") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'O');
        if (res != "24") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z');
        VERIFY(res == "America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'E');
        if (res != "%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'O');
        if (res != "%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'z');
        VERIFY(res == "-0700");
        if (res.empty()) throw std::runtime_error("timeio::put fail for z");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'E');
        if (res != "%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'O');
        if (res != "%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (res != "%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'a');
        if (res != "三") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'E');
        if (res != "%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'O');
        if (res != "%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'A');
        if (res != "星期三") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'E');
        if (res != "%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'O');
        if (res != "%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'b');
        if (res != "9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'E');
        if (res != "%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'O');
        if (res != "%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'h');
        if (res != "9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'E');
        if (res != "%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'O');
        if (res != "%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'B');
        if (res != "九月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'E');
        if (res != "%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'O');
        if (res != "%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'c');
        if (res != "2024年09月04日 星期三 13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        if (res != "2024年09月04日 星期三 13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'O');
        if (res != "%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'C');
        if (res != "20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'E');
        if (res != "20") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'O');
        if (res != "%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'x');
        if (res != "2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'E');
        if (res != "2024年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'O');
        if (res != "%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'D');
        if (res != "09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'E');
        if (res != "%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'O');
        if (res != "%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'd');
        if (res != "04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'E');
        if (res != "%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'O');
        if (res != "04") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'e');
        if (res != " 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'E');
        if (res != "%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'O');
        if (res != " 4") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'F');
        if (res != "2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'E');
        if (res != "%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'O');
        if (res != "%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'H');
        if (res != "13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'E');
        if (res != "%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'O');
        if (res != "13") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'I');
        if (res != "01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'E');
        if (res != "%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'O');
        if (res != "01") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'j');
        if (res != "248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'E');
        if (res != "%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'O');
        if (res != "%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'M');
        if (res != "33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'E');
        if (res != "%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'O');
        if (res != "33") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'm');
        if (res != "09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'E');
        if (res != "%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'O');
        if (res != "09") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'n');
        if (res != "\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'E');
        if (res != "%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'O');
        if (res != "%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'p');
        if (res != "下午") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'E');
        if (res != "%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'O');
        if (res != "%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'R');
        if (res != "13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'E');
        if (res != "%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'O');
        if (res != "%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'r');
        if (res != "下午 01时33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'E');
        if (res != "%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'O');
        if (res != "%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'S');
        if (res != "18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'E');
        if (res != "%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'O');
        if (res != "18") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'X');
        if (res != "13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        if (res != "13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'O');
        if (res != "%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'T');
        if (res != "13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'E');
        if (res != "%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'O');
        if (res != "%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 't');
        if (res != "\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'E');
        if (res != "%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'O');
        if (res != "%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'u');
        if (res != "3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'E');
        if (res != "%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'O');
        if (res != "3") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'U');
        if (res != "35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'E');
        if (res != "%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'O');
        if (res != "35") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'V');
        if (res != "36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'E');
        if (res != "%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'O');
        if (res != "36") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'g');
        if (res != "24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'E');
        if (res != "%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'O');
        if (res != "%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'G');
        if (res != "2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'E');
        if (res != "%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'O');
        if (res != "%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'W');
        if (res != "36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'E');
        if (res != "%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'O');
        if (res != "36") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'w');
        if (res != "3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'E');
        if (res != "%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'O');
        if (res != "3") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y');
        if (res != "2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'E');
        if (res != "2024") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'O');
        if (res != "%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'y');
        if (res != "24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'E');
        if (res != "24") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'O');
        if (res != "24") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z');
        VERIFY(res == "America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'E');
        if (res != "%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'O');
        if (res != "%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'z');
        VERIFY(res == "-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'E');
        if (res != "%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'O');
        if (res != "%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (res != "%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'a');
        if (res != "水") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'E');
        if (res != "%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'O');
        if (res != "%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'A');
        if (res != "水曜日") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'E');
        if (res != "%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'O');
        if (res != "%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'b');
        if (res != " 9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'E');
        if (res != "%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'O');
        if (res != "%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'h');
        if (res != " 9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'E');
        if (res != "%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'O');
        if (res != "%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'B');
        if (res != "9月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'E');
        if (res != "%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'O');
        if (res != "%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'c');
        if (res != "2024年09月04日 13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        if (res != "令和6年09月04日 13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'O');
        if (res != "%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'C');
        if (res != "20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'E');
        if (res != "令和") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'O');
        if (res != "%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'x');
        if (res != "2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'E');
        if (res != "令和6年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'O');
        if (res != "%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'D');
        if (res != "09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'E');
        if (res != "%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'O');
        if (res != "%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'd');
        if (res != "04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'E');
        if (res != "%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'O');
        if (res != "四") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'e');
        if (res != " 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'E');
        if (res != "%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'O');
        if (res != "四") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'F');
        if (res != "2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'E');
        if (res != "%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'O');
        if (res != "%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'H');
        if (res != "13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'E');
        if (res != "%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'O');
        if (res != "十三") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'I');
        if (res != "01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'E');
        if (res != "%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'O');
        if (res != "一") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'j');
        if (res != "248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'E');
        if (res != "%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'O');
        if (res != "%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'M');
        if (res != "33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'E');
        if (res != "%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'O');
        if (res != "三十三") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'm');
        if (res != "09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'E');
        if (res != "%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'O');
        if (res != "九") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'n');
        if (res != "\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'E');
        if (res != "%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'O');
        if (res != "%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'p');
        if (res != "午後") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'E');
        if (res != "%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'O');
        if (res != "%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'R');
        if (res != "13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'E');
        if (res != "%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'O');
        if (res != "%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'r');
        if (res != "午後01時33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'E');
        if (res != "%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'O');
        if (res != "%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'S');
        if (res != "18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'E');
        if (res != "%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'O');
        if (res != "十八") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'X');
        if (res != "13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        if (res != "13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'O');
        if (res != "%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'T');
        if (res != "13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'E');
        if (res != "%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'O');
        if (res != "%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 't');
        if (res != "\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'E');
        if (res != "%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'O');
        if (res != "%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'u');
        if (res != "3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'E');
        if (res != "%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'O');
        if (res != "三") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'U');
        if (res != "35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'E');
        if (res != "%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'O');
        if (res != "三十五") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'V');
        if (res != "36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'E');
        if (res != "%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'O');
        if (res != "三十六") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'g');
        if (res != "24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'E');
        if (res != "%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'O');
        if (res != "%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'G');
        if (res != "2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'E');
        if (res != "%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'O');
        if (res != "%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'W');
        if (res != "36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'E');
        if (res != "%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'O');
        if (res != "三十六") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'w');
        if (res != "3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'E');
        if (res != "%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'O');
        if (res != "三") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y');
        if (res != "2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'E');
        if (res != "令和6年") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'O');
        if (res != "%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'y');
        if (res != "24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'E');
        if (res != "6") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'O');
        if (res != "二十四") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z');
        VERIFY(res == "America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'E');
        if (res != "%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'O');
        if (res != "%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'z');
        VERIFY(res == "-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'E');
        if (res != "%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'O');
        if (res != "%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (oss != "Sun") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, 'x');
        if (oss != "04/04/71") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, 'X');
        if (oss != "12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, 'x', 'E');
        if (oss != "04/04/71") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, 'X', 'E');
        if (oss != "12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
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
        if ((oss != "Son") && (oss != "So")) throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x');
        if (oss != "04.04.1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X');
        if (oss != "12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x', 'E');
        if (oss != "04.04.1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X', 'E');
        if (oss != "12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
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
        if (oss != "Sun") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x');
        if (oss != "Sunday, April 04, 1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X');
        if (oss.find("12:00:00") == std::string::npos) throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x', 'E');
        if (oss != "Sunday, April 04, 1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X', 'E');
        if (oss.find("12:00:00") == std::string::npos) throw std::runtime_error("timeio::put fail");
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
        if (oss != "dom") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x');
        if (oss != "04/04/71") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X');
        if (oss != "12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'x', 'E');
        if (oss != "04/04/71") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, 'X', 'E');
        if (oss != "12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
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
        if (oss != "Sunday, the second of April") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
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
        if (oss != "Sonntag, the second of April") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
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
        if (oss != "Sunday, the second of April") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
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
        if (oss != "dimanche, the second of avril") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
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
    if (res != "12:00:00, Thursday, the second of June, 1997xxxxxx") throw std::runtime_error("timeio::put fail");
    if (sanity1 != "12:00:00, Thursday, the second of June, 1997") throw std::runtime_error("timeio::put fail");

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
    if (res != "Tuesdayxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx") throw std::runtime_error("timeio::put fail");
    if (sanity1 != "Tuesday") throw std::runtime_error("timeio::put fail");

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

    if (time_buffer + std::string(" America/Los_Angeles") != res) throw std::runtime_error("timeio::put fail");

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
        if (res != "水") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'E');
        if (res != "%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'O');
        if (res != "%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'A');
        if (res != "水曜日") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'E');
        if (res != "%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'O');
        if (res != "%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'b');
        if (res != " 9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'E');
        if (res != "%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'O');
        if (res != "%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'h');
        if (res != " 9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'E');
        if (res != "%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'O');
        if (res != "%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'B');
        if (res != "9月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'E');
        if (res != "%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'O');
        if (res != "%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'c');
        if (res != "%c") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        if (res != "%Ec") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'O');
        if (res != "%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'C');
        if (res != "20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'E');
        if (res != "令和") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'O');
        if (res != "%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'x');
        if (res != "2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'E');
        if (res != "令和6年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'O');
        if (res != "%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'D');
        if (res != "09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'E');
        if (res != "%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'O');
        if (res != "%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'd');
        if (res != "04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'E');
        if (res != "%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'O');
        if (res != "四") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'e');
        if (res != " 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'E');
        if (res != "%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'O');
        if (res != "四") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'F');
        if (res != "2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'E');
        if (res != "%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'O');
        if (res != "%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'H');
        if (res != "%H") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'E');
        if (res != "%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'O');
        if (res != "%OH") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'I');
        if (res != "%I") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'E');
        if (res != "%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'O');
        if (res != "%OI") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'j');
        if (res != "248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'E');
        if (res != "%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'O');
        if (res != "%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'M');
        if (res != "%M") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'E');
        if (res != "%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'O');
        if (res != "%OM") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'm');
        if (res != "09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'E');
        if (res != "%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'O');
        if (res != "九") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'n');
        if (res != "\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'E');
        if (res != "%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'O');
        if (res != "%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'p');
        if (res != "%p") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'E');
        if (res != "%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'O');
        if (res != "%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'R');
        if (res != "%R") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'E');
        if (res != "%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'O');
        if (res != "%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'r');
        if (res != "%r") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'E');
        if (res != "%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'O');
        if (res != "%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'S');
        if (res != "%S") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'E');
        if (res != "%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'O');
        if (res != "%OS") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'X');
        if (res != "%X") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        if (res != "%EX") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'O');
        if (res != "%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'T');
        if (res != "%T") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'E');
        if (res != "%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'O');
        if (res != "%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 't');
        if (res != "\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'E');
        if (res != "%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'O');
        if (res != "%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'u');
        if (res != "3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'E');
        if (res != "%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'O');
        if (res != "三") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'U');
        if (res != "35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'E');
        if (res != "%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'O');
        if (res != "三十五") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'V');
        if (res != "36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'E');
        if (res != "%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'O');
        if (res != "三十六") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'g');
        if (res != "24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'E');
        if (res != "%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'O');
        if (res != "%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'G');
        if (res != "2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'E');
        if (res != "%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'O');
        if (res != "%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'W');
        if (res != "36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'E');
        if (res != "%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'O');
        if (res != "三十六") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'w');
        if (res != "3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'E');
        if (res != "%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'O');
        if (res != "三") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y');
        if (res != "2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'E');
        if (res != "令和6年") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'O');
        if (res != "%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'y');
        if (res != "24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'E');
        if (res != "6") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'O');
        if (res != "二十四") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z');
        VERIFY(res == "%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'E');
        if (res != "%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'O');
        if (res != "%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'z');
        VERIFY(res == "%z");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'E');
        if (res != "%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'O');
        if (res != "%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (res != "%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'a');
        if (res != "%a") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'E');
        if (res != "%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'O');
        if (res != "%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'A');
        if (res != "%A") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'E');
        if (res != "%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'O');
        if (res != "%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'b');
        if (res != "%b") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'E');
        if (res != "%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'O');
        if (res != "%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'h');
        if (res != "%h") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'E');
        if (res != "%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'O');
        if (res != "%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'B');
        if (res != "%B") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'E');
        if (res != "%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'O');
        if (res != "%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'c');
        if (res != "%c") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        if (res != "%Ec") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'O');
        if (res != "%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'x');
        if (res != "%x") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'E');
        if (res != "%Ex") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'O');
        if (res != "%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'D');
        if (res != "%D") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'E');
        if (res != "%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'O');
        if (res != "%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'd');
        if (res != "%d") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'E');
        if (res != "%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'O');
        if (res != "%Od") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'e');
        if (res != "%e") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'E');
        if (res != "%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'O');
        if (res != "%Oe") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'F');
        if (res != "%F") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'E');
        if (res != "%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'O');
        if (res != "%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'H');
        if (res != "13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'E');
        if (res != "%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'O');
        if (res != "十三") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'I');
        if (res != "01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'E');
        if (res != "%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'O');
        if (res != "一") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'j');
        if (res != "%j") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'E');
        if (res != "%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'O');
        if (res != "%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'M');
        if (res != "33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'E');
        if (res != "%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'O');
        if (res != "三十三") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'm');
        if (res != "%m") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'E');
        if (res != "%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'O');
        if (res != "%Om") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'n');
        if (res != "\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'E');
        if (res != "%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'O');
        if (res != "%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'p');
        if (res != "午後") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'E');
        if (res != "%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'O');
        if (res != "%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'R');
        if (res != "13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'E');
        if (res != "%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'O');
        if (res != "%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'r');
        if (res != "午後01時33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'E');
        if (res != "%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'O');
        if (res != "%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'S');
        if (res != "18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'E');
        if (res != "%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'O');
        if (res != "十八") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'X');
        if (res != "13時33分18秒") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        if (res != "13時33分18秒") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'O');
        if (res != "%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'T');
        if (res != "13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'E');
        if (res != "%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'O');
        if (res != "%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 't');
        if (res != "\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'E');
        if (res != "%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'O');
        if (res != "%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'u');
        if (res != "%u") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'E');
        if (res != "%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'O');
        if (res != "%Ou") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'U');
        if (res != "%U") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'E');
        if (res != "%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'O');
        if (res != "%OU") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'V');
        if (res != "%V") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'E');
        if (res != "%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'O');
        if (res != "%OV") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'g');
        if (res != "%g") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'E');
        if (res != "%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'O');
        if (res != "%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'G');
        if (res != "%G") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'E');
        if (res != "%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'O');
        if (res != "%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'W');
        if (res != "%W") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'E');
        if (res != "%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'O');
        if (res != "%OW") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'w');
        if (res != "%w") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'E');
        if (res != "%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'O');
        if (res != "%Ow") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y');
        if (res != "%Y") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'E');
        if (res != "%EY") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'O');
        if (res != "%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'y');
        if (res != "%y") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'E');
        if (res != "%Ey") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'O');
        if (res != "%Oy") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z'); VERIFY(res == "%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'E');
        if (res != "%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'O');
        if (res != "%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'z'); VERIFY(res == "%z");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'E');
        if (res != "%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'O');
        if (res != "%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
        if (res != "%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'a');
        if (res != "水") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'E');
        if (res != "%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, 'a', 'O');
        if (res != "%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'A');
        if (res != "水曜日") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'E');
        if (res != "%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, 'A', 'O');
        if (res != "%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'b');
        if (res != " 9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'E');
        if (res != "%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, 'b', 'O');
        if (res != "%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'h');
        if (res != " 9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'E');
        if (res != "%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, 'h', 'O');
        if (res != "%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'B');
        if (res != "9月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'E');
        if (res != "%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, 'B', 'O');
        if (res != "%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'c');
        if (res != "2024年09月04日 13時33分18秒") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'E');
        if (res != "令和6年09月04日 13時33分18秒") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, 'c', 'O');
        if (res != "%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'C');
        if (res != "20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'E');
        if (res != "令和") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, 'C', 'O');
        if (res != "%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'x');
        if (res != "2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'E');
        if (res != "令和6年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, 'x', 'O');
        if (res != "%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'D');
        if (res != "09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'E');
        if (res != "%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, 'D', 'O');
        if (res != "%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'd');
        if (res != "04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'E');
        if (res != "%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, 'd', 'O');
        if (res != "四") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'e');
        if (res != " 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'E');
        if (res != "%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, 'e', 'O');
        if (res != "四") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'F');
        if (res != "2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'E');
        if (res != "%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, 'F', 'O');
        if (res != "%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'H');
        if (res != "13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'E');
        if (res != "%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, 'H', 'O');
        if (res != "十三") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'I');
        if (res != "01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'E');
        if (res != "%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, 'I', 'O');
        if (res != "一") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'j');
        if (res != "248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'E');
        if (res != "%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, 'j', 'O');
        if (res != "%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'M');
        if (res != "33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'E');
        if (res != "%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, 'M', 'O');
        if (res != "三十三") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'm');
        if (res != "09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'E');
        if (res != "%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, 'm', 'O');
        if (res != "九") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'n');
        if (res != "\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'E');
        if (res != "%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, 'n', 'O');
        if (res != "%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'p');
        if (res != "午後") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'E');
        if (res != "%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, 'p', 'O');
        if (res != "%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'R');
        if (res != "13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'E');
        if (res != "%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, 'R', 'O');
        if (res != "%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'r');
        if (res != "午後01時33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'E');
        if (res != "%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, 'r', 'O');
        if (res != "%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'S');
        if (res != "18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'E');
        if (res != "%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, 'S', 'O');
        if (res != "十八") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'X');
        if (res != "13時33分18秒") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'E');
        if (res != "13時33分18秒") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, 'X', 'O');
        if (res != "%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'T');
        if (res != "13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'E');
        if (res != "%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, 'T', 'O');
        if (res != "%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 't');
        if (res != "\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'E');
        if (res != "%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, 't', 'O');
        if (res != "%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'u');
        if (res != "3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'E');
        if (res != "%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, 'u', 'O');
        if (res != "三") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'U');
        if (res != "35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'E');
        if (res != "%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, 'U', 'O');
        if (res != "三十五") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'V');
        if (res != "36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'E');
        if (res != "%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, 'V', 'O');
        if (res != "三十六") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'g');
        if (res != "24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'E');
        if (res != "%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, 'g', 'O');
        if (res != "%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'G');
        if (res != "2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'E');
        if (res != "%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, 'G', 'O');
        if (res != "%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'W');
        if (res != "36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'E');
        if (res != "%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, 'W', 'O');
        if (res != "三十六") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'w');
        if (res != "3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'E');
        if (res != "%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, 'w', 'O');
        if (res != "三") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y');
        if (res != "2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'E');
        if (res != "令和6年") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Y', 'O');
        if (res != "%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'y');
        if (res != "24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'E');
        if (res != "6") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, 'y', 'O');
        if (res != "二十四") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z'); VERIFY(res == "%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'E');
        if (res != "%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, 'Z', 'O');
        if (res != "%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, 'z'); VERIFY(res == "%z");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'E');
        if (res != "%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, 'z', 'O');
        if (res != "%Oz") throw std::runtime_error("timeio::put fail for Oz");
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
    VERIFY(CheckGet(obj, "PST", 'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
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
    VERIFY(CheckGet(obj, "PST", 'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
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
    VERIFY(CheckGet(obj, "PST", 'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_year != 114) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_mday != 14) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_hour != 1) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_min != 9) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_sec != 35) throw std::runtime_error("timeio::get_4 fail.");
    }

    {
        std::string input = "2020  ";
        std::string format = "%Y";
        
        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret == input.end()) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_year != 120) throw std::runtime_error("timeio::get_4 fail.");
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
        if (time.tm_year != 120) throw std::runtime_error("timeio::get_4 fail.");
        if (ret != input.end()) throw std::runtime_error("timeio::get_4 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_year != 114) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_wday != 1) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_mday != 14) throw std::runtime_error("timeio::get_5 fail.");
    }
    {
        std::string input = "Mittwoch";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, 'A');
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_5 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 1) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "Tue ";
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 2) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "Wednesday";
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "Thu";
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "Fri ";
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "Saturday";
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 6) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "Feb";
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 1) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "Mar ";
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 2) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "April";
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "May";
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "Jun ";
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "July";
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 6) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "Aug";
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 7) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "May ";
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "October";
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 9) throw std::runtime_error("timeio::get_6 fail.");
    }

    // Other tests.
    {
        std::string input = "2.";
        std::string format = "%d.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mday != 2) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mday != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "06.";
        std::string format = "%e.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mday != 6) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 0) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 0) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "12:37AM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 0) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 37) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "01:25AM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 1) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 25) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "12:00PM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 12) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 0) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "12:42PM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 12) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 42) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "07:23PM";
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 19) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 23) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::string input = "17%20";
        std::string format = "%H%%%M";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 17) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 20) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mon != 10) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_hour != 13) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_min != 38) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_sec != 12) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::string input = "05 37";
        std::string format = "%C %y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 537 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::string input = "68";
        std::string format = "%y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2068 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::string input = "69";
        std::string format = "%y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 1969 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::string input = "03-Feb-2003";
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "16-Dec-2020";
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "16-Dec-2021";
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "253 2020";
        std::string format = "%j %Y";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "233 2021";
        std::string format = "%j %Y";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "2020 23 3";
        std::string format = "%Y %U %w";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "2020 23 3";
        std::string format = "%Y %W %w";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "2021 43 Fri";
        std::string format = "%Y %W %a";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "2024 23 3";
        std::string format = "%Y %U %w";

        IOv2::time_parse_context<char> ctx;
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
        std::string input = "2024 23 3";
        std::string format = "%Y %W %w";

        IOv2::time_parse_context<char> ctx;
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
        if (ret != input.end()) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_hour != 13) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_min != 38) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_sec != 12) throw std::runtime_error("timeio::get_8 fail.");
    }
        
    {
        std::string input = "11:17:42 PM";
        std::string format = "%r";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_hour != 23) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_min != 17) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_sec != 42) throw std::runtime_error("timeio::get_8 fail.");
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
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_year != 114) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_mday != 14) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_hour != 1) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_min != 9) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_sec != 35) throw std::runtime_error("timeio::get_4 fail.");
    }

    {
        using namespace IOv2;
        streambuf sb(mem_device{"2020  "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret == std::default_sentinel) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_year != 120) throw std::runtime_error("timeio::get_4 fail.");
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
        if (time.tm_year != 120) throw std::runtime_error("timeio::get_4 fail.");
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_4 fail.");
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
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_year != 114) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_wday != 1) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_mday != 14) throw std::runtime_error("timeio::get_5 fail.");
    }
    {
        using namespace IOv2;
        streambuf sb(mem_device{"Mittwoch"});
        auto beg = istreambuf_iterator(sb);

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, 'A');
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_5 fail.");
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
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 1) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"Tue "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == std::default_sentinel) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 2) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"Wednesday"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"Thu"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"Fri "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == std::default_sentinel) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"Saturday"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%A";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 6) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"Feb"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 1) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"Mar "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == std::default_sentinel) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 2) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"April"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%b";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"May"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"Jun "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == std::default_sentinel) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"July"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%B";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 6) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"Aug"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 7) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"May "});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == std::default_sentinel) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"October"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%h";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 9) throw std::runtime_error("timeio::get_6 fail.");
    }

    // Other tests.
    {
        streambuf sb(mem_device{"2."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mday != 2) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mday != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"06."});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%e.";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mday != 6) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 0) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 0) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"12:37AM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 0) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 37) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"01:25AM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 1) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 25) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"12:00PM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 12) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 0) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"12:42PM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 12) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 42) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"07:23PM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%I:%M%p";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 19) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 23) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"17%20"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%H%%%M";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 17) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 20) throw std::runtime_error("timeio::get_6 fail.");
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
        if ((it == std::default_sentinel) || (*it != '4')) throw std::runtime_error("timeio_ext::get_6 fail.");
    }

    {
        streambuf sb(mem_device{"Novembur"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%bembur";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mon != 10) throw std::runtime_error("timeio::get_6 fail.");
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
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_hour != 13) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_min != 38) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_sec != 12) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"05 37"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%C %y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 537 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"68"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2068 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"69"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 1969 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"03-Feb-2003"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2003 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 1) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 1) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 33) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"16-Dec-2020"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2020 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 11) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 16) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 350) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"16-Dec-2021"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%d-%b-%Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2021 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 11) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 16) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 4) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 349) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"253 2020"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%j %Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2020 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 8) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 9) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 252) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"233 2021"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%j %Y";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2021 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 7) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 21) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 6) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 232) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"2020 23 3"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%Y %U %w";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2020 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 10) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 161) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"2020 23 3"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%Y %W %w";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2020 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 10) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 161) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"2021 43 Fri"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%Y %W %a";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2021 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 9) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 29) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 5) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 301) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"2024 23 3"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%Y %U %w";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2024 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 12) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 163) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        streambuf sb(mem_device{"2024 23 3"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%Y %W %w";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2024 - 1900) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_mday != 5) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_yday != 156) throw std::runtime_error("timeio::get_7 fail.");
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
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_hour != 13) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_min != 38) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_sec != 12) throw std::runtime_error("timeio::get_8 fail.");
    }

    {
        streambuf sb(mem_device{"11:17:42 PM"});
        auto beg = istreambuf_iterator(sb);
        std::string format = "%r";

        IOv2::time_parse_context<char> ctx;
        auto ret = obj.get(beg, std::default_sentinel, ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != std::default_sentinel) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_hour != 23) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_min != 17) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_sec != 42) throw std::runtime_error("timeio::get_8 fail.");
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
    VERIFY(FOri("PST", 'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
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