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

void test_timeio_wchar_t_put_1()
{
    dump_info("Test timeio<wchar_t> put 1...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("C"));
    auto tp = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    std::wstring res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, L'%');
        if (res != L"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'a');
        if (res != L"Wed") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, L'a', L'E');
        if (res != L"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, L'a', L'O');
        if (res != L"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'A');
        if (res != L"Wednesday") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, L'A', L'E');
        if (res != L"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, L'A', L'O');
        if (res != L"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'b');
        if (res != L"Sep") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, L'b', L'E');
        if (res != L"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, L'b', L'O');
        if (res != L"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'h');
        if (res != L"Sep") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, L'h', L'E');
        if (res != L"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, L'h', L'O');
        if (res != L"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'B');
        if (res != L"September") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, L'B', L'E');
        if (res != L"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, L'B', L'O');
        if (res != L"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'c');
        if (res != L"09/04/24 13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, L'c', L'E');
        if (res != L"09/04/24 13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, L'c', L'O');
        if (res != L"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'C');
        if (res != L"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, L'C', L'E');
        if (res != L"20") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, L'C', L'O');
        if (res != L"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'x');
        if (res != L"09/04/24") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, L'x', L'E');
        if (res != L"09/04/24") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, L'x', L'O');
        if (res != L"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'D');
        if (res != L"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, L'D', L'E');
        if (res != L"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, L'D', L'O');
        if (res != L"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'd');
        if (res != L"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, L'd', L'E');
        if (res != L"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, L'd', L'O');
        if (res != L"04") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'e');
        if (res != L" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, L'e', L'E');
        if (res != L"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, L'e', L'O');
        if (res != L" 4") throw std::runtime_error("timeio::put fail for Oe");
    }
      
    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'F');
        if (res != L"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, L'F', L'E');
        if (res != L"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, L'F', L'O');
        if (res != L"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'H');
        if (res != L"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, L'H', L'E');
        if (res != L"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, L'H', L'O');
        if (res != L"13") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'I');
        if (res != L"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, L'I', L'E');
        if (res != L"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, L'I', L'O');
        if (res != L"01") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'j');
        if (res != L"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, L'j', L'E');
        if (res != L"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, L'j', L'O');
        if (res != L"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'M');
        if (res != L"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, L'M', L'E');
        if (res != L"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, L'M', L'O');
        if (res != L"33") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'm');
        if (res != L"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, L'm', L'E');
        if (res != L"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, L'm', L'O');
        if (res != L"09") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'n');
        if (res != L"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, L'n', L'E');
        if (res != L"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, L'n', L'O');
        if (res != L"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'p');
        if (res != L"PM") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, L'p', L'E');
        if (res != L"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, L'p', L'O');
        if (res != L"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'R');
        if (res != L"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, L'R', L'E');
        if (res != L"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, L'R', L'O');
        if (res != L"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'r');
        if (res != L"01:33:18 PM") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, L'r', L'E');
        if (res != L"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, L'r', L'O');
        if (res != L"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'S');
        if (res != L"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, L'S', L'E');
        if (res != L"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, L'S', L'O');
        if (res != L"18") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'X');
        if (res != L"13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, L'X', L'E');
        if (res != L"13:33:18 America/Los_Angeles") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, L'X', L'O');
        if (res != L"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'T');
        if (res != L"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, L'T', L'E');
        if (res != L"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, L'T', L'O');
        if (res != L"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L't');
        if (res != L"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, L't', L'E');
        if (res != L"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, L't', L'O');
        if (res != L"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'u');
        if (res != L"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, L'u', L'E');
        if (res != L"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, L'u', L'O');
        if (res != L"3") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'U');
        if (res != L"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, L'U', L'E');
        if (res != L"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, L'U', L'O');
        if (res != L"35") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'V');
        if (res != L"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, L'V', L'E');
        if (res != L"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, L'V', L'O');
        if (res != L"36") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'g');
        if (res != L"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, L'g', L'E');
        if (res != L"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, L'g', L'O');
        if (res != L"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'G');
        if (res != L"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, L'G', L'E');
        if (res != L"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, L'G', L'O');
        if (res != L"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'W');
        if (res != L"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, L'W', L'E');
        if (res != L"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, L'W', L'O');
        if (res != L"36") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'w');
        if (res != L"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, L'w', L'E');
        if (res != L"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, L'w', L'O');
        if (res != L"3") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y');
        if (res != L"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y', L'E');
        if (res != L"2024") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y', L'O');
        if (res != L"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'y');
        if (res != L"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, L'y', L'E');
        if (res != L"24") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, L'y', L'O');
        if (res != L"24") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z');
        VERIFY(res == L"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z', L'E');
        if (res != L"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z', L'O');
        if (res != L"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'z');
        VERIFY(res == L"-0700");
        if (res.empty()) throw std::runtime_error("timeio::put fail for z");
        res.clear(); obj.put(std::back_inserter(res), tp, L'z', L'E');
        if (res != L"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, L'z', L'O');
        if (res != L"%Oz") throw std::runtime_error("timeio::put fail for Oz");
    }    

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_2()
{
    dump_info("Test timeio<wchar_t> put 2...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("zh_CN.UTF-8"));
    auto tp = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    std::wstring res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, L'%');
        if (res != L"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'a');
        if (res != L"三") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, L'a', L'E');
        if (res != L"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, L'a', L'O');
        if (res != L"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'A');
        if (res != L"星期三") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, L'A', L'E');
        if (res != L"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, L'A', L'O');
        if (res != L"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'b');
        if (res != L"9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, L'b', L'E');
        if (res != L"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, L'b', L'O');
        if (res != L"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'h');
        if (res != L"9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, L'h', L'E');
        if (res != L"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, L'h', L'O');
        if (res != L"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'B');
        if (res != L"九月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, L'B', L'E');
        if (res != L"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, L'B', L'O');
        if (res != L"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'c');
        if (res != L"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, L'c', L'E');
        if (res != L"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, L'c', L'O');
        if (res != L"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'C');
        if (res != L"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, L'C', L'E');
        if (res != L"20") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, L'C', L'O');
        if (res != L"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'x');
        if (res != L"2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, L'x', L'E');
        if (res != L"2024年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, L'x', L'O');
        if (res != L"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'D');
        if (res != L"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, L'D', L'E');
        if (res != L"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, L'D', L'O');
        if (res != L"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'd');
        if (res != L"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, L'd', L'E');
        if (res != L"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, L'd', L'O');
        if (res != L"04") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'e');
        if (res != L" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, L'e', L'E');
        if (res != L"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, L'e', L'O');
        if (res != L" 4") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'F');
        if (res != L"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, L'F', L'E');
        if (res != L"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, L'F', L'O');
        if (res != L"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'H');
        if (res != L"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, L'H', L'E');
        if (res != L"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, L'H', L'O');
        if (res != L"13") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'I');
        if (res != L"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, L'I', L'E');
        if (res != L"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, L'I', L'O');
        if (res != L"01") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'j');
        if (res != L"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, L'j', L'E');
        if (res != L"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, L'j', L'O');
        if (res != L"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'M');
        if (res != L"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, L'M', L'E');
        if (res != L"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, L'M', L'O');
        if (res != L"33") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'm');
        if (res != L"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, L'm', L'E');
        if (res != L"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, L'm', L'O');
        if (res != L"09") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'n');
        if (res != L"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, L'n', L'E');
        if (res != L"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, L'n', L'O');
        if (res != L"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'p');
        if (res != L"下午") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, L'p', L'E');
        if (res != L"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, L'p', L'O');
        if (res != L"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'R');
        if (res != L"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, L'R', L'E');
        if (res != L"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, L'R', L'O');
        if (res != L"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'r');
        if (res != L"下午 01时33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, L'r', L'E');
        if (res != L"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, L'r', L'O');
        if (res != L"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'S');
        if (res != L"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, L'S', L'E');
        if (res != L"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, L'S', L'O');
        if (res != L"18") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'X');
        if (res != L"13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, L'X', L'E');
        if (res != L"13时33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, L'X', L'O');
        if (res != L"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'T');
        if (res != L"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, L'T', L'E');
        if (res != L"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, L'T', L'O');
        if (res != L"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L't');
        if (res != L"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, L't', L'E');
        if (res != L"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, L't', L'O');
        if (res != L"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'u');
        if (res != L"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, L'u', L'E');
        if (res != L"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, L'u', L'O');
        if (res != L"3") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'U');
        if (res != L"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, L'U', L'E');
        if (res != L"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, L'U', L'O');
        if (res != L"35") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'V');
        if (res != L"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, L'V', L'E');
        if (res != L"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, L'V', L'O');
        if (res != L"36") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'g');
        if (res != L"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, L'g', L'E');
        if (res != L"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, L'g', L'O');
        if (res != L"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'G');
        if (res != L"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, L'G', L'E');
        if (res != L"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, L'G', L'O');
        if (res != L"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'W');
        if (res != L"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, L'W', L'E');
        if (res != L"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, L'W', L'O');
        if (res != L"36") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'w');
        if (res != L"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, L'w', L'E');
        if (res != L"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, L'w', L'O');
        if (res != L"3") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y');
        if (res != L"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y', L'E');
        if (res != L"2024") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y', L'O');
        if (res != L"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'y');
        if (res != L"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, L'y', L'E');
        if (res != L"24") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, L'y', L'O');
        if (res != L"24") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z');
        VERIFY(res == L"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z', L'E');
        if (res != L"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z', L'O');
        if (res != L"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'z');
        VERIFY(res == L"-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, L'z', L'E');
        if (res != L"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, L'z', L'O');
        if (res != L"%Oz") throw std::runtime_error("timeio::put fail for Oz");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_3()
{
    dump_info("Test timeio<wchar_t> put 3...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("ja_JP.UTF-8"));
    auto tp = create_zoned_time(2024, 9, 4, 13, 33, 18, "America/Los_Angeles");

    std::wstring res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, L'%');
        if (res != L"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'a');
        if (res != L"水") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, L'a', L'E');
        if (res != L"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, L'a', L'O');
        if (res != L"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'A');
        if (res != L"水曜日") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, L'A', L'E');
        if (res != L"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, L'A', L'O');
        if (res != L"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'b');
        if (res != L" 9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, L'b', L'E');
        if (res != L"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, L'b', L'O');
        if (res != L"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'h');
        if (res != L" 9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, L'h', L'E');
        if (res != L"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, L'h', L'O');
        if (res != L"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'B');
        if (res != L"9月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, L'B', L'E');
        if (res != L"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, L'B', L'O');
        if (res != L"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'c');
        if (res != L"2024年09月04日 13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, L'c', L'E');
        if (res != L"令和6年09月04日 13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, L'c', L'O');
        if (res != L"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'C');
        if (res != L"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, L'C', L'E');
        if (res != L"令和") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, L'C', L'O');
        if (res != L"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'x');
        if (res != L"2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, L'x', L'E');
        if (res != L"令和6年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, L'x', L'O');
        if (res != L"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'D');
        if (res != L"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, L'D', L'E');
        if (res != L"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, L'D', L'O');
        if (res != L"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'd');
        if (res != L"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, L'd', L'E');
        if (res != L"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, L'd', L'O');
        if (res != L"四") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'e');
        if (res != L" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, L'e', L'E');
        if (res != L"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, L'e', L'O');
        if (res != L"四") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'F');
        if (res != L"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, L'F', L'E');
        if (res != L"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, L'F', L'O');
        if (res != L"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'H');
        if (res != L"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, L'H', L'E');
        if (res != L"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, L'H', L'O');
        if (res != L"十三") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'I');
        if (res != L"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, L'I', L'E');
        if (res != L"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, L'I', L'O');
        if (res != L"一") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'j');
        if (res != L"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, L'j', L'E');
        if (res != L"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, L'j', L'O');
        if (res != L"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'M');
        if (res != L"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, L'M', L'E');
        if (res != L"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, L'M', L'O');
        if (res != L"三十三") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'm');
        if (res != L"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, L'm', L'E');
        if (res != L"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, L'm', L'O');
        if (res != L"九") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'n');
        if (res != L"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, L'n', L'E');
        if (res != L"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, L'n', L'O');
        if (res != L"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'p');
        if (res != L"午後") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, L'p', L'E');
        if (res != L"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, L'p', L'O');
        if (res != L"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'R');
        if (res != L"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, L'R', L'E');
        if (res != L"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, L'R', L'O');
        if (res != L"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'r');
        if (res != L"午後01時33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, L'r', L'E');
        if (res != L"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, L'r', L'O');
        if (res != L"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'S');
        if (res != L"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, L'S', L'E');
        if (res != L"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, L'S', L'O');
        if (res != L"十八") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'X');
        if (res != L"13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, L'X', L'E');
        if (res != L"13時33分18秒 America/Los_Angeles") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, L'X', L'O');
        if (res != L"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'T');
        if (res != L"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, L'T', L'E');
        if (res != L"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, L'T', L'O');
        if (res != L"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L't');
        if (res != L"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, L't', L'E');
        if (res != L"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, L't', L'O');
        if (res != L"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'u');
        if (res != L"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, L'u', L'E');
        if (res != L"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, L'u', L'O');
        if (res != L"三") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'U');
        if (res != L"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, L'U', L'E');
        if (res != L"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, L'U', L'O');
        if (res != L"三十五") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'V');
        if (res != L"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, L'V', L'E');
        if (res != L"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, L'V', L'O');
        if (res != L"三十六") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'g');
        if (res != L"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, L'g', L'E');
        if (res != L"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, L'g', L'O');
        if (res != L"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'G');
        if (res != L"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, L'G', L'E');
        if (res != L"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, L'G', L'O');
        if (res != L"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'W');
        if (res != L"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, L'W', L'E');
        if (res != L"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, L'W', L'O');
        if (res != L"三十六") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'w');
        if (res != L"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, L'w', L'E');
        if (res != L"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, L'w', L'O');
        if (res != L"三") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y');
        if (res != L"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y', L'E');
        if (res != L"令和6年") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y', L'O');
        if (res != L"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'y');
        if (res != L"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, L'y', L'E');
        if (res != L"6") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, L'y', L'O');
        if (res != L"二十四") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z');
        VERIFY(res == L"America/Los_Angeles");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z', L'E');
        if (res != L"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z', L'O');
        if (res != L"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'z');
        VERIFY(res == L"-0700");
        res.clear(); obj.put(std::back_inserter(res), tp, L'z', L'E');
        if (res != L"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, L'z', L'O');
        if (res != L"%Oz") throw std::runtime_error("timeio::put fail for Oz");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_4()
{
    dump_info("Test timeio<wchar_t> put 4...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("C"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::wstring oss;
    {
        obj.put(std::back_inserter(oss), time1, L'a');
        if (oss != L"Sun") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, L'x');
        if (oss != L"04/04/71") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, L'X');
        if (oss != L"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, L'x', L'E');
        if (oss != L"04/04/71") throw std::runtime_error("timeio::put fail");
    }

    {
        oss.clear();
        obj.put(std::back_inserter(oss), time1, L'X', L'E');
        if (oss != L"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_5()
{
    dump_info("Test timeio<wchar_t> put 5...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("de_DE.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::wstring oss;
    {
        obj.put(std::back_inserter(oss), time1, L'a');
        if ((oss != L"Son") && (oss != L"So")) throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, L'x');
        if (oss != L"04.04.1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, L'X');
        if (oss != L"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, L'x', L'E');
        if (oss != L"04.04.1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, L'X', L'E');
        if (oss != L"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_6()
{
    dump_info("Test timeio<wchar_t> put 6...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("en_HK.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::wstring oss;
    {
        obj.put(std::back_inserter(oss), time1, L'a');
        if (oss != L"Sun") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, L'x');
        if (oss != L"Sunday, April 04, 1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, L'X');
        if (oss.find(L"12:00:00") == std::wstring::npos) throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, L'x', L'E');
        if (oss != L"Sunday, April 04, 1971") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, L'X', L'E');
        if (oss.find(L"12:00:00") == std::wstring::npos) throw std::runtime_error("timeio::put fail");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_7()
{
    dump_info("Test timeio<wchar_t> put 7...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("es_ES.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::wstring oss;
    {
        obj.put(std::back_inserter(oss), time1, L'a');
        if (oss != L"dom") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, L'x');
        if (oss != L"04/04/71") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, L'X');
        if (oss != L"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, L'x', L'E');
        if (oss != L"04/04/71") throw std::runtime_error("timeio::put fail");
    }
    {
        oss.clear(); obj.put(std::back_inserter(oss), time1, L'X', L'E');
        if (oss != L"12:00:00 America/Los_Angeles") throw std::runtime_error("timeio::put fail");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_8()
{
    dump_info("Test timeio<wchar_t> put 8...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("C"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::wstring date = L"%A, the second of %B";
    const std::wstring date_ex = L"%Ex";
    std::wstring oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        if (oss != L"Sunday, the second of April") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_9()
{
    dump_info("Test timeio<wchar_t> put 9...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("de_DE.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::wstring date = L"%A, the second of %B";
    const std::wstring date_ex = L"%Ex";
    std::wstring oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        if (oss != L"Sonntag, the second of April") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_10()
{
    dump_info("Test timeio<wchar_t> put 10...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("en_HK.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::wstring date = L"%A, the second of %B";
    const std::wstring date_ex = L"%Ex";
    std::wstring oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        if (oss != L"Sunday, the second of April") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_11()
{
    dump_info("Test timeio<wchar_t> put 11...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("fr_FR.UTF-8"));
    auto time1 = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    const std::wstring date = L"%A, the second of %B";
    const std::wstring date_ex = L"%Ex";
    std::wstring oss, oss2;
    {
        obj.put(std::back_inserter(oss), time1, date);
        if (oss != L"dimanche, the second of avril") throw std::runtime_error("timeio::put fail");
    }
    {
        obj.put(std::back_inserter(oss2), time1, date_ex);
        if (oss == oss2) throw std::runtime_error("timeio::put fail");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_12()
{
    dump_info("Test timeio<wchar_t> put 12...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("C"));
    auto time_sanity = create_zoned_time(1997, 6, 26, 12, 0, 0, "America/Los_Angeles");

    std::wstring res(50, 'x');
    const std::wstring date = L"%T, %A, the second of %B, %Y";
        
    auto ret1 = obj.put(res.begin(), time_sanity, date);
    std::wstring sanity1(res.begin(), ret1);
    if (res != L"12:00:00, Thursday, the second of June, 1997xxxxxx") throw std::runtime_error("timeio::put fail");
    if (sanity1 != L"12:00:00, Thursday, the second of June, 1997") throw std::runtime_error("timeio::put fail");

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_13()
{
    dump_info("Test timeio<wchar_t> put 13...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("C"));
    auto time_sanity = create_zoned_time(1997, 6, 24, 12, 0, 0, "America/Los_Angeles");

    std::wstring res(50, 'x');

    auto ret1 = obj.put(res.begin(), time_sanity, 'A');
    std::wstring sanity1(res.begin(), ret1);
    if (res != L"Tuesdayxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx") throw std::runtime_error("timeio::put fail");
    if (sanity1 != L"Tuesday") throw std::runtime_error("timeio::put fail");

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_14()
{
    dump_info("Test timeio<wchar_t> put 14...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("ta_IN.UTF-8"));
    const tm time1 = test_tm(0, 0, 12, 4, 3, 71, 0, 93, 0);
    auto zt = create_zoned_time(1971, 4, 4, 12, 0, 0, "America/Los_Angeles");

    std::wstring res;
    obj.put(std::back_inserter(res), zt, 'c');

    char time_buffer[128];
    setlocale(LC_ALL, "ta_IN");
    std::strftime(time_buffer, 128, "%c", &time1);
    setlocale(LC_ALL, "C");

    if (IOv2::to_wstring(time_buffer, "ta_IN.UTF-8") + std::wstring(L" America/Los_Angeles") != res) throw std::runtime_error("timeio::put fail");

    dump_info("Done\n");
}

void test_timeio_wchar_t_put_15()
{
    dump_info("Test timeio<wchar_t> put 15...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("ja_JP.UTF-8"));
    using namespace std::chrono;
    year_month_day tp{year{2024}, month{9}, day{4}};

    std::wstring res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, L'%');
        VERIFY(res == L"%");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'a');
        if (res != L"水") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, L'a', L'E');
        if (res != L"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, L'a', L'O');
        if (res != L"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'A');
        if (res != L"水曜日") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, L'A', L'E');
        if (res != L"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, L'A', L'O');
        if (res != L"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'b');
        if (res != L" 9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, L'b', L'E');
        if (res != L"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, L'b', L'O');
        if (res != L"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'h');
        if (res != L" 9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, L'h', L'E');
        if (res != L"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, L'h', L'O');
        if (res != L"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'B');
        if (res != L"9月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, L'B', L'E');
        if (res != L"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, L'B', L'O');
        if (res != L"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'c');
        if (res != L"%c") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, L'c', L'E');
        if (res != L"%Ec") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, L'c', L'O');
        if (res != L"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'C');
        if (res != L"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, L'C', L'E');
        if (res != L"令和") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, L'C', L'O');
        if (res != L"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'x');
        if (res != L"2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, L'x', L'E');
        if (res != L"令和6年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, L'x', L'O');
        if (res != L"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'D');
        if (res != L"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, L'D', L'E');
        if (res != L"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, L'D', L'O');
        if (res != L"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'd');
        if (res != L"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, L'd', L'E');
        if (res != L"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, L'd', L'O');
        if (res != L"四") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'e');
        if (res != L" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, L'e', L'E');
        if (res != L"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, L'e', L'O');
        if (res != L"四") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'F');
        if (res != L"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, L'F', L'E');
        if (res != L"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, L'F', L'O');
        if (res != L"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'H');
        if (res != L"%H") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, L'H', L'E');
        if (res != L"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, L'H', L'O');
        if (res != L"%OH") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'I');
        if (res != L"%I") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, L'I', L'E');
        if (res != L"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, L'I', L'O');
        if (res != L"%OI") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'j');
        if (res != L"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, L'j', L'E');
        if (res != L"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, L'j', L'O');
        if (res != L"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'M');
        if (res != L"%M") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, L'M', L'E');
        if (res != L"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, L'M', L'O');
        if (res != L"%OM") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'm');
        if (res != L"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, L'm', L'E');
        if (res != L"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, L'm', L'O');
        if (res != L"九") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'n');
        if (res != L"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, L'n', L'E');
        if (res != L"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, L'n', L'O');
        if (res != L"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'p');
        if (res != L"%p") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, L'p', L'E');
        if (res != L"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, L'p', L'O');
        if (res != L"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'R');
        if (res != L"%R") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, L'R', L'E');
        if (res != L"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, L'R', L'O');
        if (res != L"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'r');
        if (res != L"%r") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, L'r', L'E');
        if (res != L"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, L'r', L'O');
        if (res != L"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'S');
        if (res != L"%S") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, L'S', L'E');
        if (res != L"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, L'S', L'O');
        if (res != L"%OS") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'X');
        if (res != L"%X") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, L'X', L'E');
        if (res != L"%EX") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, L'X', L'O');
        if (res != L"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'T');
        if (res != L"%T") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, L'T', L'E');
        if (res != L"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, L'T', L'O');
        if (res != L"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L't');
        if (res != L"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, L't', L'E');
        if (res != L"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, L't', L'O');
        if (res != L"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'u');
        if (res != L"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, L'u', L'E');
        if (res != L"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, L'u', L'O');
        if (res != L"三") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'U');
        if (res != L"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, L'U', L'E');
        if (res != L"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, L'U', L'O');
        if (res != L"三十五") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'V');
        if (res != L"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, L'V', L'E');
        if (res != L"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, L'V', L'O');
        if (res != L"三十六") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'g');
        if (res != L"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, L'g', L'E');
        if (res != L"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, L'g', L'O');
        if (res != L"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'G');
        if (res != L"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, L'G', L'E');
        if (res != L"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, L'G', L'O');
        if (res != L"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'W');
        if (res != L"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, L'W', L'E');
        if (res != L"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, L'W', L'O');
        if (res != L"三十六") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'w');
        if (res != L"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, L'w', L'E');
        if (res != L"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, L'w', L'O');
        if (res != L"三") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y');
        if (res != L"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y', L'E');
        if (res != L"令和6年") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y', L'O');
        if (res != L"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'y');
        if (res != L"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, L'y', L'E');
        if (res != L"6") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, L'y', L'O');
        if (res != L"二十四") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z');
        VERIFY(res == L"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z', L'E');
        if (res != L"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z', L'O');
        if (res != L"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'z');
        VERIFY(res == L"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, L'z', L'E');
        if (res != L"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, L'z', L'O');
        if (res != L"%Oz") throw std::runtime_error("timeio::put fail for Oz");
    }
    dump_info("Done\n");
}

void test_timeio_wchar_t_put_16()
{
    dump_info("Test timeio<wchar_t> put 16...");

    using namespace std::chrono;
    seconds time_sec = hours{13} + minutes{33} + seconds{18};
    std::chrono::hh_mm_ss tp{time_sec};

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("ja_JP.UTF-8"));
    std::wstring res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, L'%');
        if (res != L"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'a');
        if (res != L"%a") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, L'a', L'E');
        if (res != L"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, L'a', L'O');
        if (res != L"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'A');
        if (res != L"%A") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, L'A', L'E');
        if (res != L"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, L'A', L'O');
        if (res != L"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'b');
        if (res != L"%b") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, L'b', L'E');
        if (res != L"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, L'b', L'O');
        if (res != L"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'h');
        if (res != L"%h") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, L'h', L'E');
        if (res != L"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, L'h', L'O');
        if (res != L"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'B');
        if (res != L"%B") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, L'B', L'E');
        if (res != L"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, L'B', L'O');
        if (res != L"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'c');
        if (res != L"%c") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, L'c', L'E');
        if (res != L"%Ec") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, L'c', L'O');
        if (res != L"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'x');
        if (res != L"%x") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, L'x', L'E');
        if (res != L"%Ex") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, L'x', L'O');
        if (res != L"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'D');
        if (res != L"%D") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, L'D', L'E');
        if (res != L"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, L'D', L'O');
        if (res != L"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'd');
        if (res != L"%d") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, L'd', L'E');
        if (res != L"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, L'd', L'O');
        if (res != L"%Od") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'e');
        if (res != L"%e") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, L'e', L'E');
        if (res != L"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, L'e', L'O');
        if (res != L"%Oe") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'F');
        if (res != L"%F") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, L'F', L'E');
        if (res != L"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, L'F', L'O');
        if (res != L"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'H');
        if (res != L"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, L'H', L'E');
        if (res != L"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, L'H', L'O');
        if (res != L"十三") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'I');
        if (res != L"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, L'I', L'E');
        if (res != L"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, L'I', L'O');
        if (res != L"一") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'j');
        if (res != L"%j") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, L'j', L'E');
        if (res != L"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, L'j', L'O');
        if (res != L"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'M');
        if (res != L"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, L'M', L'E');
        if (res != L"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, L'M', L'O');
        if (res != L"三十三") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'm');
        if (res != L"%m") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, L'm', L'E');
        if (res != L"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, L'm', L'O');
        if (res != L"%Om") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'n');
        if (res != L"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, L'n', L'E');
        if (res != L"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, L'n', L'O');
        if (res != L"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'p');
        if (res != L"午後") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, L'p', L'E');
        if (res != L"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, L'p', L'O');
        if (res != L"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'R');
        if (res != L"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, L'R', L'E');
        if (res != L"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, L'R', L'O');
        if (res != L"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'r');
        if (res != L"午後01時33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, L'r', L'E');
        if (res != L"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, L'r', L'O');
        if (res != L"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'S');
        if (res != L"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, L'S', L'E');
        if (res != L"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, L'S', L'O');
        if (res != L"十八") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'X');
        if (res != L"13時33分18秒") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, L'X', L'E');
        if (res != L"13時33分18秒") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, L'X', L'O');
        if (res != L"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'T');
        if (res != L"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, L'T', L'E');
        if (res != L"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, L'T', L'O');
        if (res != L"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L't');
        if (res != L"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, L't', L'E');
        if (res != L"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, L't', L'O');
        if (res != L"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'u');
        if (res != L"%u") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, L'u', L'E');
        if (res != L"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, L'u', L'O');
        if (res != L"%Ou") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'U');
        if (res != L"%U") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, L'U', L'E');
        if (res != L"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, L'U', L'O');
        if (res != L"%OU") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'V');
        if (res != L"%V") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, L'V', L'E');
        if (res != L"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, L'V', L'O');
        if (res != L"%OV") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'g');
        if (res != L"%g") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, L'g', L'E');
        if (res != L"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, L'g', L'O');
        if (res != L"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'G');
        if (res != L"%G") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, L'G', L'E');
        if (res != L"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, L'G', L'O');
        if (res != L"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'W');
        if (res != L"%W") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, L'W', L'E');
        if (res != L"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, L'W', L'O');
        if (res != L"%OW") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'w');
        if (res != L"%w") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, L'w', L'E');
        if (res != L"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, L'w', L'O');
        if (res != L"%Ow") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y');
        if (res != L"%Y") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y', L'E');
        if (res != L"%EY") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y', L'O');
        if (res != L"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'y');
        if (res != L"%y") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, L'y', L'E');
        if (res != L"%Ey") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, L'y', L'O');
        if (res != L"%Oy") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z'); VERIFY(res == L"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z', L'E');
        if (res != L"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z', L'O');
        if (res != L"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'z'); VERIFY(res == L"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, L'z', L'E');
        if (res != L"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, L'z', L'O');
        if (res != L"%Oz") throw std::runtime_error("timeio::put fail for Oz");
    }
    dump_info("Done\n");
}

void test_timeio_wchar_t_put_17()
{
    dump_info("Test timeio<wchar_t> put 17...");
    std::tm tp = test_tm(18, 33, 13, 4, 9 - 1, 2024 - 1900, 0, 0, 0);
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("ja_JP.UTF-8"));

    std::wstring res;
    {
        res.clear();
        obj.put(std::back_inserter(res), tp, L'%');
        if (res != L"%") throw std::runtime_error("timeio::put fail for %");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'a');
        if (res != L"水") throw std::runtime_error("timeio::put fail for a");
        res.clear(); obj.put(std::back_inserter(res), tp, L'a', L'E');
        if (res != L"%Ea") throw std::runtime_error("timeio::put fail for Ea");
        res.clear(); obj.put(std::back_inserter(res), tp, L'a', L'O');
        if (res != L"%Oa") throw std::runtime_error("timeio::put fail for Oa");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'A');
        if (res != L"水曜日") throw std::runtime_error("timeio::put fail for A");
        res.clear(); obj.put(std::back_inserter(res), tp, L'A', L'E');
        if (res != L"%EA") throw std::runtime_error("timeio::put fail for EA");
        res.clear(); obj.put(std::back_inserter(res), tp, L'A', L'O');
        if (res != L"%OA") throw std::runtime_error("timeio::put fail for OA");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'b');
        if (res != L" 9月") throw std::runtime_error("timeio::put fail for b");
        res.clear(); obj.put(std::back_inserter(res), tp, L'b', L'E');
        if (res != L"%Eb") throw std::runtime_error("timeio::put fail for Eb");
        res.clear(); obj.put(std::back_inserter(res), tp, L'b', L'O');
        if (res != L"%Ob") throw std::runtime_error("timeio::put fail for Ob");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'h');
        if (res != L" 9月") throw std::runtime_error("timeio::put fail for h");
        res.clear(); obj.put(std::back_inserter(res), tp, L'h', L'E');
        if (res != L"%Eh") throw std::runtime_error("timeio::put fail for Eh");
        res.clear(); obj.put(std::back_inserter(res), tp, L'h', L'O');
        if (res != L"%Oh") throw std::runtime_error("timeio::put fail for Oh");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'B');
        if (res != L"9月") throw std::runtime_error("timeio::put fail for B");
        res.clear(); obj.put(std::back_inserter(res), tp, L'B', L'E');
        if (res != L"%EB") throw std::runtime_error("timeio::put fail for EB");
        res.clear(); obj.put(std::back_inserter(res), tp, L'B', L'O');
        if (res != L"%OB") throw std::runtime_error("timeio::put fail for OB");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'c');
        if (res != L"2024年09月04日 13時33分18秒") throw std::runtime_error("timeio::put fail for c");
        res.clear(); obj.put(std::back_inserter(res), tp, L'c', L'E');
        if (res != L"令和6年09月04日 13時33分18秒") throw std::runtime_error("timeio::put fail for Ec");
        res.clear(); obj.put(std::back_inserter(res), tp, L'c', L'O');
        if (res != L"%Oc") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'C');
        if (res != L"20") throw std::runtime_error("timeio::put fail for C");
        res.clear(); obj.put(std::back_inserter(res), tp, L'C', L'E');
        if (res != L"令和") throw std::runtime_error("timeio::put fail for EC");
        res.clear(); obj.put(std::back_inserter(res), tp, L'C', L'O');
        if (res != L"%OC") throw std::runtime_error("timeio::put fail for Oc");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'x');
        if (res != L"2024年09月04日") throw std::runtime_error("timeio::put fail for x");
        res.clear(); obj.put(std::back_inserter(res), tp, L'x', L'E');
        if (res != L"令和6年09月04日") throw std::runtime_error("timeio::put fail for Ex");
        res.clear(); obj.put(std::back_inserter(res), tp, L'x', L'O');
        if (res != L"%Ox") throw std::runtime_error("timeio::put fail for Ox");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'D');
        if (res != L"09/04/24") throw std::runtime_error("timeio::put fail for D");
        res.clear(); obj.put(std::back_inserter(res), tp, L'D', L'E');
        if (res != L"%ED") throw std::runtime_error("timeio::put fail for ED");
        res.clear(); obj.put(std::back_inserter(res), tp, L'D', L'O');
        if (res != L"%OD") throw std::runtime_error("timeio::put fail for OD");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'd');
        if (res != L"04") throw std::runtime_error("timeio::put fail for d");
        res.clear(); obj.put(std::back_inserter(res), tp, L'd', L'E');
        if (res != L"%Ed") throw std::runtime_error("timeio::put fail for Ed");
        res.clear(); obj.put(std::back_inserter(res), tp, L'd', L'O');
        if (res != L"四") throw std::runtime_error("timeio::put fail for Od");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'e');
        if (res != L" 4") throw std::runtime_error("timeio::put fail for e");
        res.clear(); obj.put(std::back_inserter(res), tp, L'e', L'E');
        if (res != L"%Ee") throw std::runtime_error("timeio::put fail for Ee");
        res.clear(); obj.put(std::back_inserter(res), tp, L'e', L'O');
        if (res != L"四") throw std::runtime_error("timeio::put fail for Oe");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'F');
        if (res != L"2024-09-04") throw std::runtime_error("timeio::put fail for F");
        res.clear(); obj.put(std::back_inserter(res), tp, L'F', L'E');
        if (res != L"%EF") throw std::runtime_error("timeio::put fail for EF");
        res.clear(); obj.put(std::back_inserter(res), tp, L'F', L'O');
        if (res != L"%OF") throw std::runtime_error("timeio::put fail for OF");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'H');
        if (res != L"13") throw std::runtime_error("timeio::put fail for H");
        res.clear(); obj.put(std::back_inserter(res), tp, L'H', L'E');
        if (res != L"%EH") throw std::runtime_error("timeio::put fail for EH");
        res.clear(); obj.put(std::back_inserter(res), tp, L'H', L'O');
        if (res != L"十三") throw std::runtime_error("timeio::put fail for OH");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'I');
        if (res != L"01") throw std::runtime_error("timeio::put fail for I");
        res.clear(); obj.put(std::back_inserter(res), tp, L'I', L'E');
        if (res != L"%EI") throw std::runtime_error("timeio::put fail for EI");
        res.clear(); obj.put(std::back_inserter(res), tp, L'I', L'O');
        if (res != L"一") throw std::runtime_error("timeio::put fail for OI");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'j');
        if (res != L"248") throw std::runtime_error("timeio::put fail for j");
        res.clear(); obj.put(std::back_inserter(res), tp, L'j', L'E');
        if (res != L"%Ej") throw std::runtime_error("timeio::put fail for Ej");
        res.clear(); obj.put(std::back_inserter(res), tp, L'j', L'O');
        if (res != L"%Oj") throw std::runtime_error("timeio::put fail for Oj");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'M');
        if (res != L"33") throw std::runtime_error("timeio::put fail for M");
        res.clear(); obj.put(std::back_inserter(res), tp, L'M', L'E');
        if (res != L"%EM") throw std::runtime_error("timeio::put fail for EM");
        res.clear(); obj.put(std::back_inserter(res), tp, L'M', L'O');
        if (res != L"三十三") throw std::runtime_error("timeio::put fail for OM");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'm');
        if (res != L"09") throw std::runtime_error("timeio::put fail for m");
        res.clear(); obj.put(std::back_inserter(res), tp, L'm', L'E');
        if (res != L"%Em") throw std::runtime_error("timeio::put fail for Em");
        res.clear(); obj.put(std::back_inserter(res), tp, L'm', L'O');
        if (res != L"九") throw std::runtime_error("timeio::put fail for Om");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'n');
        if (res != L"\n") throw std::runtime_error("timeio::put fail for n");
        res.clear(); obj.put(std::back_inserter(res), tp, L'n', L'E');
        if (res != L"%En") throw std::runtime_error("timeio::put fail for En");
        res.clear(); obj.put(std::back_inserter(res), tp, L'n', L'O');
        if (res != L"%On") throw std::runtime_error("timeio::put fail for On");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'p');
        if (res != L"午後") throw std::runtime_error("timeio::put fail for p");
        res.clear(); obj.put(std::back_inserter(res), tp, L'p', L'E');
        if (res != L"%Ep") throw std::runtime_error("timeio::put fail for Ep");
        res.clear(); obj.put(std::back_inserter(res), tp, L'p', L'O');
        if (res != L"%Op") throw std::runtime_error("timeio::put fail for Op");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'R');
        if (res != L"13:33") throw std::runtime_error("timeio::put fail for R");
        res.clear(); obj.put(std::back_inserter(res), tp, L'R', L'E');
        if (res != L"%ER") throw std::runtime_error("timeio::put fail for ER");
        res.clear(); obj.put(std::back_inserter(res), tp, L'R', L'O');
        if (res != L"%OR") throw std::runtime_error("timeio::put fail for OR");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'r');
        if (res != L"午後01時33分18秒") throw std::runtime_error("timeio::put fail for r");
        res.clear(); obj.put(std::back_inserter(res), tp, L'r', L'E');
        if (res != L"%Er") throw std::runtime_error("timeio::put fail for Er");
        res.clear(); obj.put(std::back_inserter(res), tp, L'r', L'O');
        if (res != L"%Or") throw std::runtime_error("timeio::put fail for Or");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'S');
        if (res != L"18") throw std::runtime_error("timeio::put fail for S");
        res.clear(); obj.put(std::back_inserter(res), tp, L'S', L'E');
        if (res != L"%ES") throw std::runtime_error("timeio::put fail for ES");
        res.clear(); obj.put(std::back_inserter(res), tp, L'S', L'O');
        if (res != L"十八") throw std::runtime_error("timeio::put fail for OS");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'X');
        if (res != L"13時33分18秒") throw std::runtime_error("timeio::put fail for X");
        res.clear(); obj.put(std::back_inserter(res), tp, L'X', L'E');
        if (res != L"13時33分18秒") throw std::runtime_error("timeio::put fail for EX");
        res.clear(); obj.put(std::back_inserter(res), tp, L'X', L'O');
        if (res != L"%OX") throw std::runtime_error("timeio::put fail for OX");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'T');
        if (res != L"13:33:18") throw std::runtime_error("timeio::put fail for T");
        res.clear(); obj.put(std::back_inserter(res), tp, L'T', L'E');
        if (res != L"%ET") throw std::runtime_error("timeio::put fail for ET");
        res.clear(); obj.put(std::back_inserter(res), tp, L'T', L'O');
        if (res != L"%OT") throw std::runtime_error("timeio::put fail for OT");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L't');
        if (res != L"\t") throw std::runtime_error("timeio::put fail for t");
        res.clear(); obj.put(std::back_inserter(res), tp, L't', L'E');
        if (res != L"%Et") throw std::runtime_error("timeio::put fail for Et");
        res.clear(); obj.put(std::back_inserter(res), tp, L't', L'O');
        if (res != L"%Ot") throw std::runtime_error("timeio::put fail for Ot");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'u');
        if (res != L"3") throw std::runtime_error("timeio::put fail for u");
        res.clear(); obj.put(std::back_inserter(res), tp, L'u', L'E');
        if (res != L"%Eu") throw std::runtime_error("timeio::put fail for Eu");
        res.clear(); obj.put(std::back_inserter(res), tp, L'u', L'O');
        if (res != L"三") throw std::runtime_error("timeio::put fail for Ou");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'U');
        if (res != L"35") throw std::runtime_error("timeio::put fail for U");
        res.clear(); obj.put(std::back_inserter(res), tp, L'U', L'E');
        if (res != L"%EU") throw std::runtime_error("timeio::put fail for EU");
        res.clear(); obj.put(std::back_inserter(res), tp, L'U', L'O');
        if (res != L"三十五") throw std::runtime_error("timeio::put fail for OU");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'V');
        if (res != L"36") throw std::runtime_error("timeio::put fail for V");
        res.clear(); obj.put(std::back_inserter(res), tp, L'V', L'E');
        if (res != L"%EV") throw std::runtime_error("timeio::put fail for EV");
        res.clear(); obj.put(std::back_inserter(res), tp, L'V', L'O');
        if (res != L"三十六") throw std::runtime_error("timeio::put fail for OV");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'g');
        if (res != L"24") throw std::runtime_error("timeio::put fail for g");
        res.clear(); obj.put(std::back_inserter(res), tp, L'g', L'E');
        if (res != L"%Eg") throw std::runtime_error("timeio::put fail for Eg");
        res.clear(); obj.put(std::back_inserter(res), tp, L'g', L'O');
        if (res != L"%Og") throw std::runtime_error("timeio::put fail for Og");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'G');
        if (res != L"2024") throw std::runtime_error("timeio::put fail for G");
        res.clear(); obj.put(std::back_inserter(res), tp, L'G', L'E');
        if (res != L"%EG") throw std::runtime_error("timeio::put fail for EG");
        res.clear(); obj.put(std::back_inserter(res), tp, L'G', L'O');
        if (res != L"%OG") throw std::runtime_error("timeio::put fail for OG");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'W');
        if (res != L"36") throw std::runtime_error("timeio::put fail for W");
        res.clear(); obj.put(std::back_inserter(res), tp, L'W', L'E');
        if (res != L"%EW") throw std::runtime_error("timeio::put fail for EW");
        res.clear(); obj.put(std::back_inserter(res), tp, L'W', L'O');
        if (res != L"三十六") throw std::runtime_error("timeio::put fail for OW");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'w');
        if (res != L"3") throw std::runtime_error("timeio::put fail for w");
        res.clear(); obj.put(std::back_inserter(res), tp, L'w', L'E');
        if (res != L"%Ew") throw std::runtime_error("timeio::put fail for Ew");
        res.clear(); obj.put(std::back_inserter(res), tp, L'w', L'O');
        if (res != L"三") throw std::runtime_error("timeio::put fail for Ow");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y');
        if (res != L"2024") throw std::runtime_error("timeio::put fail for Y");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y', L'E');
        if (res != L"令和6年") throw std::runtime_error("timeio::put fail for EY");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Y', L'O');
        if (res != L"%OY") throw std::runtime_error("timeio::put fail for OY");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'y');
        if (res != L"24") throw std::runtime_error("timeio::put fail for y");
        res.clear(); obj.put(std::back_inserter(res), tp, L'y', L'E');
        if (res != L"6") throw std::runtime_error("timeio::put fail for Ey");
        res.clear(); obj.put(std::back_inserter(res), tp, L'y', L'O');
        if (res != L"二十四") throw std::runtime_error("timeio::put fail for Oy");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z'); VERIFY(res == L"%Z");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z', L'E');
        if (res != L"%EZ") throw std::runtime_error("timeio::put fail for EZ");
        res.clear(); obj.put(std::back_inserter(res), tp, L'Z', L'O');
        if (res != L"%OZ") throw std::runtime_error("timeio::put fail for OZ");
    }

    {
        res.clear(); obj.put(std::back_inserter(res), tp, L'z'); VERIFY(res == L"%z");
        res.clear(); obj.put(std::back_inserter(res), tp, L'z', L'E');
        if (res != L"%Ez") throw std::runtime_error("timeio::put fail for Ez");
        res.clear(); obj.put(std::back_inserter(res), tp, L'z', L'O');
        if (res != L"%Oz") throw std::runtime_error("timeio::put fail for Oz");
    }
    dump_info("Done\n");
}

namespace
{
    constexpr static IOv2::ios_defs::iostate febit = IOv2::ios_defs::eofbit | IOv2::ios_defs::strfailbit;

    template <typename T = IOv2::time_parse_context<wchar_t>, bool HaveDate = true, bool HaveTime = true, bool HaveTimeZone = true>
    T CheckGet(const IOv2::timeio<wchar_t>& obj, const std::wstring& input,
               wchar_t fmt, wchar_t modif,
               IOv2::ios_defs::iostate err_exp, size_t consume_exp = (size_t)-1)
    {
        if (consume_exp == (size_t)-1) consume_exp = input.size();
        IOv2::time_parse_context<wchar_t, HaveDate, HaveTime, HaveTimeZone> ctx1, ctx2, ctx3;
        if (err_exp == IOv2::ios_defs::goodbit)
        {
            VERIFY(obj.get(input.begin(), input.end(), ctx1, fmt, modif) != input.end());
            {
                std::list<wchar_t> lst_input(input.begin(), input.end());
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
                std::list<wchar_t> lst_input(input.begin(), input.end());
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
                std::list<wchar_t> lst_input(input.begin(), input.end());
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

    template <typename T = IOv2::time_parse_context<wchar_t>, bool HaveDate = true, bool HaveTime = true, bool HaveTimeZone = true>
    T CheckGet(const IOv2::timeio<wchar_t>& obj, const std::wstring& input,
               const std::wstring& fmt,
               IOv2::ios_defs::iostate err_exp, size_t consume_exp = (size_t)-1)
    {
        if (consume_exp == (size_t)-1) consume_exp = input.size();
        IOv2::time_parse_context<wchar_t, HaveDate, HaveTime, HaveTimeZone> ctx1, ctx2, ctx3;
        if (err_exp == IOv2::ios_defs::goodbit)
        {
            VERIFY(obj.get(input.begin(), input.end(), ctx1, fmt) != input.end());
            {
                std::list<wchar_t> lst_input(input.begin(), input.end());
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
                std::list<wchar_t> lst_input(input.begin(), input.end());
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
                std::list<wchar_t> lst_input(input.begin(), input.end());
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

void test_timeio_wchar_t_get_1()
{
    dump_info("Test timeio<wchar_t> get 1...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("C"));
    CheckGet(obj, L"%",   L'%',  0,  IOv2::ios_defs::eofbit);
    CheckGet(obj, L"x",   L'%',  0,  IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%",   L'%', L'E', febit);
    CheckGet(obj, L"%E%", L'%', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"%",   L'%', L'O', febit);
    CheckGet(obj, L"%O%", L'%', L'O', IOv2::ios_defs::eofbit);

    VERIFY(CheckGet(obj, L"Wed", L'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, L"%Ea", L'a', L'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, L"a",   L'a', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oa", L'a', L'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, L"a",   L'a', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"Wednesday", L'A', 0, IOv2::ios_defs::eofbit, 9).m_wday == 3);
    CheckGet(obj, L"%EA", L'A', L'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, L"A",   L'A', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OA", L'A', L'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, L"A",   L'A', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"Sep", L'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, L"%Eb", L'b', L'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, L"b",   L'b', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Ob", L'b', L'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, L"b",   L'b', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"September", L'B', 0, IOv2::ios_defs::eofbit, 9).m_month == 9);
    CheckGet(obj, L"%EB", L'B', L'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, L"B",   L'B', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OB", L'B', L'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, L"B",   L'B', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"Sep", L'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, L"%Eh", L'h', L'E', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, L"h",   L'h', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oh", L'h', L'O', IOv2::ios_defs::eofbit,  3);
    CheckGet(obj, L"h",   L'h', L'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(CheckGet<year_month_day>(obj, L"09/04/24 13:33:18 America/Los_Angeles", L'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"09/04/24 13:33:18 America/Los_Angeles", L'c', L'E', IOv2::ios_defs::eofbit, 17) == check_date1);
    CheckGet(obj, L"c",   L'c', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oc", L'c', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"c",   L'c', L'O', IOv2::ios_defs::strfailbit, 0);


    VERIFY(CheckGet(obj, L"20", L'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(CheckGet(obj, L"20", L'C', L'E', IOv2::ios_defs::eofbit).m_century == 20);
    CheckGet(obj, L"C",   L'C', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OC", L'C', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"C",   L'C', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"04", L'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, L"04", L'd', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, L"%Ed", L'd', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"d",   L'd', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"d",   L'd', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"4", L'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, L"4", L'e', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, L"%Ee", L'e', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"e",   L'e', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"e",   L'e', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"2024-09-04", L'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"%EF", L'F', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"F",   L'F', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OF", L'F', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"F",   L'F', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"09/04/24", L'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"09/04/24", L'x', L'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"x",   L'x', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Ox", L'x', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"x",   L'x', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"09/04/24", L'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"%ED", L'D', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"D",   L'D', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OD", L'D', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"D",   L'D', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"13", L'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, L"13", L'H', L'O', IOv2::ios_defs::eofbit).m_hour == 13);
    CheckGet(obj, L"%EH", L'H', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"H",   L'H', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"H",   L'H', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"01", L'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, L"01", L'I', L'O', IOv2::ios_defs::eofbit).m_hour == 1);
    CheckGet(obj, L"%EI", L'I', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"I",   L'I', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"I",   L'I', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"248", L'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(CheckGet<year_month_day>(obj, L"2024 248", L"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"%Ej", L'j', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"j",   L'j', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oj", L'j', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"j",   L'j', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"09", L'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, L"09", L'm', L'O', IOv2::ios_defs::eofbit).m_month == 9);
    CheckGet(obj, L"%Em", L'm', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"m",   L'm', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"m",   L'm', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"33", L'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, L"33", L'M', L'O', IOv2::ios_defs::eofbit).m_minute == 33);
    CheckGet(obj, L"%EM", L'M', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"M",   L'M', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"M",   L'M', L'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, L"\n",   L'n',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, L"x",    L'n',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, L"\n",   L'n', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%En",  L'n', L'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, L"n",    L'n', L'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%On",  L'n', L'O', IOv2::ios_defs::eofbit, 3);

    CheckGet(obj, L"\t",   L't',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, L"x",    L't',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, L"\t",   L't', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Et",  L't', L'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, L"n",    L't', L'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Ot",  L't', L'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 PM", L"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 AM", L"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(CheckGet(obj, L"PM", L'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(CheckGet(obj, L"AM", L'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    CheckGet(obj, L"%Ep", L'p', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"p",   L'p', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Op", L'p', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"p",   L'p', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01:33:18 PM", L"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"%Er", L'r', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"r",   L'r', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Or", L'r', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"r",   L'r', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33", L"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, L"%ER", L'R', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"R",   L'R', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OR", L'R', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"R",   L'R', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"18", L'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, L"18", L'S', L'O', IOv2::ios_defs::eofbit).m_second == 18);
    CheckGet(obj, L"%ES", L'S', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"S",   L'S', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"S",   L'S', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33:18 America/Los_Angeles", L"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33:18 America/Los_Angeles", L"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"X",   L'X', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OX", L'X', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"X",   L'X', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33:18", L"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"%ET", L'T', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"T",   L'T', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OT", L'T', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"T",   L'T', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"3", L'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, L"3", L'u', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, L"%Eu", L'u', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"u",   L'u', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"u",   L'u', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"24", L'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, L"%Eg", L'g', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"g",   L'g', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Og", L'g', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"g",   L'g', L'O', IOv2::ios_defs::strfailbit, 0);


    VERIFY(CheckGet(obj, L"2024", L'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, L"%EG", L'G', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"G",   L'G', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OG", L'G', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"G",   L'G', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"2024 35 Wed", L"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"2024 35 Wed", L"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, L"35", L'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(CheckGet(obj, L"35", L'U', L'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    CheckGet(obj, L"%EU", L'U', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"U",   L'U', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"U",   L'U', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"2024 36 Wed", L"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"2024 36 Wed", L"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, L"36", L'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(CheckGet(obj, L"36", L'W', L'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    CheckGet(obj, L"%EW", L'W', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"W",   L'W', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"W",   L'W', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"36", L'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    CheckGet(obj, L"54",  L'V', L'O', IOv2::ios_defs::strfailbit, 1);
    CheckGet(obj, L"36",  L'V', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"%EV", L'V', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"V",   L'V', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"V",   L'V', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"3", L'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, L"3", L'w', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, L"%Ew", L'w', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"w",   L'w', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"w",   L'w', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"24", L'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, L"24", L'y', L'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, L"24", L'y', L'O', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, L"y",  L'y', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"y",  L'y', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"2024", L'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, L"2024", L'Y', L'E', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, L"Y",   L'Y', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OY", L'Y', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"Y",   L'Y', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"America/Los_Angeles", L'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    VERIFY(CheckGet(obj, L"PST", L'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
    CheckGet(obj, L"America/Los_Angexes", L'Z', 0, IOv2::ios_defs::strfailbit);
    CheckGet(obj, L"%EZ", L'Z', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"Z",   L'Z', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OZ", L'Z', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"Z",   L'Z', L'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, L"Z", L'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, L"+13", L'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, L"-1110", L'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, L"+11:10", L'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, L"%Ez", 'z', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"z",  'z', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oz", 'z', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"z",  'z', L'O', IOv2::ios_defs::strfailbit, 0);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(CheckGet<year_month_day>(obj, L"1999-W52-6", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, L"2019-W01-1", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, L"1999-W52-5", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, L"99-W52-6", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, L"19-W01-1", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, L"99-W52-5", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, L"20 24/09/04", L"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);

    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(CheckGet<year_month_day>(obj, L"20 01 01", L"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }
    dump_info("Done\n");
}

void test_timeio_wchar_t_get_2()
{
    dump_info("Test timeio<wchar_t> get 2...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("zh_CN.UTF-8"));

    CheckGet(obj, L"%",  L'%',  0,  IOv2::ios_defs::eofbit);
    CheckGet(obj, L"x",  L'%',  0,  IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%",  L'%', L'E', febit);
    CheckGet(obj, L"%E%", L'%', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"%",  L'%', L'O', febit);
    CheckGet(obj, L"%O%", L'%', L'O', IOv2::ios_defs::eofbit);

    VERIFY(CheckGet(obj, L"三", L'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, L"%Ea", L'a', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"a",   L'a', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oa", L'a', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"a",   L'a', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"星期三", L'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, L"%EA", L'A', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"A",   L'A', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OA", L'A', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"A",   L'A', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"九月", L'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, L"%Eb", L'b', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"b",   L'b', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Ob", L'b', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"b",   L'b', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"九月", L'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, L"%EB", L'B', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"B",   L'B', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OB", L'B', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"B",   L'B', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"九月", L'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, L"%Eh", L'h', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"h",   L'h', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oh", L'h', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"h",   L'h', L'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(CheckGet<year_month_day>(obj, L"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles", L'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"2024年09月04日 星期三 13时33分18秒 America/Los_Angeles", L'c', L'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"c",   L'c', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oc", L'c', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"c",   L'c', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"20", L'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(CheckGet(obj, L"20", L'C', L'E', IOv2::ios_defs::eofbit).m_century == 20);
    CheckGet(obj, L"C",   L'C', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OC", L'C', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"C",   L'C', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"04", L'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, L"04", L'd', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, L"%Ed", L'd', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"d",   L'd', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"d",   L'd', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"4", L'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, L"4", L'e', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, L"%Ee", L'e', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"e",   L'e', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"e",   L'e', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"2024-09-04", L'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"%EF", L'F', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"F",   L'F', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OF", L'F', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"F",   L'F', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"2024年09月04日", L'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"2024年09月04日", L'x', L'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"x",   L'x', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Ox", L'x', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"x",   L'x', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"09/04/24", L'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"%ED", L'D', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"D",   L'D', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OD", L'D', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"D",   L'D', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"13", L'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, L"13", L'H', L'O', IOv2::ios_defs::eofbit).m_hour == 13);
    CheckGet(obj, L"%EH", L'H', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"H",   L'H', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"H",   L'H', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"01", L'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, L"01", L'I', L'O', IOv2::ios_defs::eofbit).m_hour == 1);
    CheckGet(obj, L"%EI", L'I', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"I",   L'I', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"I",   L'I', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"248", L'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(CheckGet<year_month_day>(obj, L"2024 248", L"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"%Ej", L'j', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"j",   L'j', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oj", L'j', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"j",   L'j', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"09", L'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, L"09", L'm', L'O', IOv2::ios_defs::eofbit).m_month == 9);
    CheckGet(obj, L"%Em", L'm', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"m",   L'm', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"m",   L'm', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"33", L'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, L"33", L'M', L'O', IOv2::ios_defs::eofbit).m_minute == 33);
    CheckGet(obj, L"%EM", L'M', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"M",   L'M', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"M",   L'M', L'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, L"\n",   L'n',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, L"x",    L'n',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, L"\n",   L'n', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%En",  L'n', L'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, L"n",    L'n', L'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%On",  L'n', L'O', IOv2::ios_defs::eofbit, 3);

    CheckGet(obj, L"\t",   L't',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, L"x",    L't',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, L"\t",   L't', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Et",  L't', L'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, L"n",    L't', L'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Ot",  L't', L'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 下午", L"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 上午", L"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(CheckGet(obj, L"下午", L'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(CheckGet(obj, L"上午", L'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    CheckGet(obj, L"%Ep", L'p', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"p",   L'p', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Op", L'p', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"p",   L'p', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"下午 01时33分18秒", L"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"%Er", L'r', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"r",   L'r', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Or", L'r', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"r",   L'r', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33", L"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, L"%ER", L'R', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"R",   L'R', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OR", L'R', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"R",   L'R', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"18", L'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, L"18", L'S', L'O', IOv2::ios_defs::eofbit).m_second == 18);
    CheckGet(obj, L"%ES", L'S', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"S",   L'S', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"S",   L'S', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13时33分18秒 America/Los_Angeles", L"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13时33分18秒 America/Los_Angeles", L"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"X",   L'X', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OX", L'X', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"X",   L'X', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33:18", L"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"%ET", L'T', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"T",   L'T', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OT", L'T', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"T",   L'T', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"3", L'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, L"3", L'u', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, L"%Eu", L'u', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"u",   L'u', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"u",   L'u', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"24", L'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, L"%Eg", L'g', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"g",   L'g', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Og", L'g', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"g",   L'g', L'O', IOv2::ios_defs::strfailbit, 0);


    VERIFY(CheckGet(obj, L"2024", L'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, L"%EG", L'G', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"G",   L'G', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OG", L'G', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"G",   L'G', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"2024 35 三", L"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"2024 35 三", L"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, L"35", L'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(CheckGet(obj, L"35", L'U', L'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    CheckGet(obj, L"%EU", L'U', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"U",   L'U', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"U",   L'U', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"2024 36 三", L"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"2024 36 三", L"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, L"36", L'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(CheckGet(obj, L"36", L'W', L'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    CheckGet(obj, L"%EW", L'W', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"W",   L'W', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"W",   L'W', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"36", L'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    CheckGet(obj, L"54",  L'V', L'O', IOv2::ios_defs::strfailbit, 1);
    CheckGet(obj, L"36",  L'V', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"%EV", L'V', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"V",   L'V', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"V",   L'V', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"3", L'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, L"3", L'w', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, L"%Ew", L'w', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"w",   L'w', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"w",   L'w', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"24", L'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, L"24", L'y', L'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, L"24", L'y', L'O', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, L"y",  L'y', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"y",  L'y', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"2024", L'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, L"2024", L'Y', L'E', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, L"Y",   L'Y', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OY", L'Y', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"Y",   L'Y', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"America/Los_Angeles", L'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    VERIFY(CheckGet(obj, L"PST", L'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
    CheckGet(obj, L"America/Los_Angexes", L'Z', 0, IOv2::ios_defs::strfailbit);
    CheckGet(obj, L"%EZ", L'Z', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"Z",   L'Z', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OZ", L'Z', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"Z",   L'Z', L'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, L"Z", L'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, L"+13", L'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, L"-1110", L'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, L"+11:10", L'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, L"%Ez", L'z', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"z",  L'z', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oz", L'z', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"z",  L'z', L'O', IOv2::ios_defs::strfailbit, 0);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(CheckGet<year_month_day>(obj, L"1999-W52-6", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, L"2019-W01-1", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, L"1999-W52-5", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, L"99-W52-6", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, L"19-W01-1", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, L"99-W52-5", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, L"20 24/09/04", L"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(CheckGet<year_month_day>(obj, L"20 01 01", L"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_get_3()
{
    dump_info("Test timeio<wchar_t> get 3...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("ja_JP.UTF-8"));

    CheckGet(obj, L"%",  L'%',  0,  IOv2::ios_defs::eofbit);
    CheckGet(obj, L"x",  L'%',  0,  IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%",  L'%', L'E', febit);
    CheckGet(obj, L"%E%", L'%', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"%",  L'%', L'O', febit);
    CheckGet(obj, L"%O%", L'%', L'O', IOv2::ios_defs::eofbit);

    VERIFY(CheckGet(obj, L"水", L'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, L"%Ea", L'a', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"a",   L'a', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oa", L'a', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"a",   L'a', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"水曜日", L'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    CheckGet(obj, L"%EA", L'A', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"A",   L'A', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OA", L'A', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"A",   L'A', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"9月", L'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, L"%Eb", L'b', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"b",   L'b', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Ob", L'b', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"b",   L'b', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"9月", L'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, L"%EB", L'B', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"B",   L'B', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OB", L'B', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"B",   L'B', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"9月", L'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    CheckGet(obj, L"%Eh", L'h', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"h",   L'h', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oh", L'h', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"h",   L'h', L'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(CheckGet<year_month_day>(obj, L"2024年09月04日 13時33分18秒 America/Los_Angeles", L'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"令和6年09月04日 13時33分18秒 America/Los_Angeles", L'c', L'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"202409月04日 13時33分18秒 America/Los_Angeles", L'c', L'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"c",   L'c', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oc", L'c', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"c",   L'c', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"20", 'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(CheckGet<year_month_day>(obj, L"平成", L'C', L'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1990));
    CheckGet(obj, L"C",   L'C', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OC", L'C', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"C",   L'C', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"04", L'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, L"04", L'd', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, L"四", L'd', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, L"%Ed", L'd', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"d",   L'd', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"d",   L'd', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"4", L'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, L"4", L'e', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(CheckGet(obj, L"四", L'e', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    CheckGet(obj, L"%Ee", L'e', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"e",   L'e', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"e",   L'e', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"2024-09-04", L'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"%EF", L'F', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"F",   L'F', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OF", L'F', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"F",   L'F', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"2024年09月04日", L'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"令和6年09月04日", L'x', L'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"202409月04日", L'x', L'E', IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"x",   L'x', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Ox", L'x', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"x",   L'x', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"09/04/24", L'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"%ED", L'D', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"D",   L'D', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OD", L'D', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"D",   L'D', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"13", L'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, L"13", L'H', L'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(CheckGet(obj, L"十三", L'H', L'O', IOv2::ios_defs::eofbit).m_hour == 13);
    CheckGet(obj, L"%EH", L'H', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"H",   L'H', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"H",   L'H', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"01", L'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, L"01", L'I', L'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(CheckGet(obj, L"一", L'I', L'O', IOv2::ios_defs::eofbit).m_hour == 1);
    CheckGet(obj, L"%EI", L'I', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"I",   L'I', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"I",   L'I', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"248", L'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(CheckGet<year_month_day>(obj, L"2024 248", L"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    CheckGet(obj, L"%Ej", L'j', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"j",   L'j', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oj", L'j', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"j",   L'j', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"09", L'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, L"09", L'm', L'O', IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(CheckGet(obj, L"九", L'm', L'O', IOv2::ios_defs::eofbit).m_month == 9);
    CheckGet(obj, L"%Em", L'm', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"m",   L'm', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"m",   L'm', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"33", L'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, L"33", L'M', L'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(CheckGet(obj, L"三十三", L'M', L'O', IOv2::ios_defs::eofbit).m_minute == 33);
    CheckGet(obj, L"%EM", L'M', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"M",   L'M', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"M",   L'M', L'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, L"\n",   L'n',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, L"x",    L'n',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, L"\n",   L'n', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%En",  L'n', L'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, L"n",    L'n', L'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%On",  L'n', L'O', IOv2::ios_defs::eofbit, 3);

    CheckGet(obj, L"\t",   L't',  0,  IOv2::ios_defs::eofbit, 1);
    CheckGet(obj, L"x",    L't',  0,  IOv2::ios_defs::goodbit, 0);
    CheckGet(obj, L"\t",   L't', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Et",  L't', L'E', IOv2::ios_defs::eofbit, 3);
    CheckGet(obj, L"n",    L't', L'O', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Ot",  L't', L'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 午後", L"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 午前", L"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(CheckGet(obj, L"午後", L'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(CheckGet(obj, L"午前", L'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    CheckGet(obj, L"%Ep", L'p', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"p",   L'p', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Op", L'p', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"p",   L'p', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"午後01時33分18秒", L"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"%Er", L'r', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"r",   L'r', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Or", L'r', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"r",   L'r', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33", L"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    CheckGet(obj, L"%ER", L'R', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"R",   L'R', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OR", L'R', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"R",   L'R', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"18", L'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, L"18", L'S', L'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(CheckGet(obj, L"十八", L'S', L'O', IOv2::ios_defs::eofbit).m_second == 18);
    CheckGet(obj, L"%ES", L'S', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"S",   L'S', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"S",   L'S', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13時33分18秒 America/Los_Angeles", L"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13時33分18秒 America/Los_Angeles", L"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"X",   L'X', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OX", L'X', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"X",   L'X', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33:18", L"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    CheckGet(obj, L"%ET", L'T', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"T",   L'T', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OT", L'T', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"T",   L'T', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"3", L'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, L"3", L'u', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, L"三", L'u', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, L"%Eu", L'u', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"u",   L'u', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"u",   L'u', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"24", L'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, L"%Eg", L'g', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"g",   L'g', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Og", L'g', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"g",   L'g', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"2024", L'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    CheckGet(obj, L"%EG", L'G', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"G",   L'G', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OG", L'G', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"G",   L'G', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"2024 35 水", L"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"2024 35 水", L"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"2024 三十五 水", L"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, L"35", L'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(CheckGet(obj, L"35", L'U', L'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    CheckGet(obj, L"%EU", L'U', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"U",   L'U', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"U",   L'U', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<year_month_day>(obj, L"2024 36 水", L"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"2024 36 水", L"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet<year_month_day>(obj, L"2024 三十六 水", L"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(CheckGet(obj, L"36", L'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(CheckGet(obj, L"36", L'W', L'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    CheckGet(obj, L"%EW", L'W', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"W",   L'W', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"W",   L'W', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"36", L'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(CheckGet(obj, L"36", L'V', L'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(CheckGet(obj, L"三十六", 'V', L'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    CheckGet(obj, L"54",  L'V', L'O', IOv2::ios_defs::strfailbit, 1);
    CheckGet(obj, L"%EV", L'V', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"V",   L'V', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"V",   L'V', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"3", L'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, L"3", L'w', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(CheckGet(obj, L"三", L'w', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    CheckGet(obj, L"%Ew", L'w', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"w",   L'w', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"w",   L'w', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"24", L'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet<year_month_day>(obj, L"6", L'y', L'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(2024));
    VERIFY(CheckGet(obj, L"24", L'y', L'O', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, L"二十四", L'y', L'O', IOv2::ios_defs::eofbit).m_year == 2024);
    CheckGet(obj, L"y",  L'y', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"y",  L'y', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"2024", L'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet(obj, L"2024", L'Y', L'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(CheckGet<year_month_day>(obj, L"平成3年", L'Y', L'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1991));
    CheckGet(obj, L"Y",   L'Y', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OY", L'Y', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"Y",   L'Y', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet(obj, L"America/Los_Angeles", L'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    VERIFY(CheckGet(obj, L"PST", L'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
    CheckGet(obj, L"America/Los_Angexes", L'Z', 0, IOv2::ios_defs::strfailbit);
    CheckGet(obj, L"%EZ", L'Z', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"Z",   L'Z', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%OZ", L'Z', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"Z",   L'Z', L'O', IOv2::ios_defs::strfailbit, 0);

    CheckGet(obj, L"Z", L'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, L"+13", L'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, L"-1110", L'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, L"+11:10", L'z', 0, IOv2::ios_defs::eofbit);
    CheckGet(obj, L"%Ez", L'z', L'E', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"z",  L'z', L'E', IOv2::ios_defs::strfailbit, 0);
    CheckGet(obj, L"%Oz", L'z', L'O', IOv2::ios_defs::eofbit);
    CheckGet(obj, L"z",  L'z', L'O', IOv2::ios_defs::strfailbit, 0);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(CheckGet<year_month_day>(obj, L"1999-W52-6", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, L"2019-W01-1", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, L"1999-W52-5", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, L"99-W52-6", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(CheckGet<year_month_day>(obj, L"19-W01-1", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(CheckGet<year_month_day>(obj, L"99-W52-5", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(CheckGet<year_month_day>(obj, L"20 24/09/04", L"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(CheckGet<year_month_day>(obj, L"20 01 01", L"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_get_4()
{
    dump_info("Test timeio<wchar_t> get 4...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("C"));
    {
        std::wstring input = L"d 2014-04-14 01:09:35";
        std::wstring format = L"d %Y-%m-%d %H:%M:%S";
        
        IOv2::time_parse_context<wchar_t> ctx;
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
        std::wstring input = L"2020  ";
        std::wstring format = L"%Y";
        
        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret == input.end()) throw std::runtime_error("timeio::get_4 fail.");
        if (time.tm_year != 120) throw std::runtime_error("timeio::get_4 fail.");
    }

    {
        std::wstring input = L"2014-04-14 01:09:35";
        std::wstring format = L"%";
        
        IOv2::time_parse_context<wchar_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::wstring input = L"2020";
        
        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, 'Y');
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_year != 120) throw std::runtime_error("timeio::get_4 fail.");
        if (ret != input.end()) throw std::runtime_error("timeio::get_4 fail.");
    }

    {
        std::wstring input = L"year: 1970";
        std::wstring format = L"jahr: %Y";
        
        IOv2::time_parse_context<wchar_t> ctx;
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

void test_timeio_wchar_t_get_5()
{
    dump_info("Test timeio<wchar_t> get 5...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("de_DE.UTF-8"));
    {
        std::wstring input = L"Montag, den 14. April 2014";
        std::wstring format = L"%A, den %d. %B %Y";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_year != 114) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_wday != 1) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_mday != 14) throw std::runtime_error("timeio::get_5 fail.");
    }
    {
        std::wstring input = L"Mittwoch";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, 'A');
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_5 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_5 fail.");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_get_6()
{
    dump_info("Test timeio<wchar_t> get 6...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("C"));
    {
        std::wstring input = L"Mon";
        std::wstring format = L"%a";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 1) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"Tue ";
        std::wstring format = L"%a";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 2) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"Wednesday";
        std::wstring format = L"%a";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 3) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"Thu";
        std::wstring format = L"%A";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"Fri ";
        std::wstring format = L"%A";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"Saturday";
        std::wstring format = L"%A";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_wday != 6) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"Feb";
        std::wstring format = L"%b";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 1) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"Mar ";
        std::wstring format = L"%b";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 2) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"April";
        std::wstring format = L"%b";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 3) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"May";
        std::wstring format = L"%B";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"Jun ";
        std::wstring format = L"%B";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"July";
        std::wstring format = L"%B";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 6) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"Aug";
        std::wstring format = L"%h";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 7) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"May ";
        std::wstring format = L"%h";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if ((ret == input.end()) || (*ret != ' ')) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 4) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"October";
        std::wstring format = L"%h";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mon != 9) throw std::runtime_error("timeio::get_6 fail.");
    }

    // Other tests.
    {
        std::wstring input = L"2.";
        std::wstring format = L"%d.";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_mday != 2) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"0.";
        std::wstring format = L"%d.";

        IOv2::time_parse_context<wchar_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::wstring input = L"32.";
        std::wstring format = L"%d.";

        IOv2::time_parse_context<wchar_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::wstring input = L"5.";
        std::wstring format = L"%e.";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mday != 5) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"06.";
        std::wstring format = L"%e.";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mday != 6) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"0";
        std::wstring format = L"%e";

        IOv2::time_parse_context<wchar_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::wstring input = L"35";
        std::wstring format = L"%e";

        IOv2::time_parse_context<wchar_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::wstring input = L"12:00AM";
        std::wstring format = L"%I:%M%p";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 0) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 0) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"12:37AM";
        std::wstring format = L"%I:%M%p";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 0) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 37) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"01:25AM";
        std::wstring format = L"%I:%M%p";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 1) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 25) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"12:00PM";
        std::wstring format = L"%I:%M%p";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 12) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 0) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"12:42PM";
        std::wstring format = L"%I:%M%p";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 12) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 42) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"07:23PM";
        std::wstring format = L"%I:%M%p";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 19) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 23) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"17%20";
        std::wstring format = L"%H%%%M";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_hour != 17) throw std::runtime_error("timeio::get_6 fail.");
        if (time.tm_min != 20) throw std::runtime_error("timeio::get_6 fail.");
    }

    {
        std::wstring input = L"24:30";
        std::wstring format = L"%H:%M";

        IOv2::time_parse_context<wchar_t> ctx;
        try
        {
            obj.get(input.begin(), input.end(), ctx, format);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    {
        std::wstring input = L"Novembur";
        std::wstring format = L"%bembur";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        if (ret != input.end()) throw std::runtime_error("timeio::get_6 fail.");
        auto time = static_cast<std::tm>(ctx);
        if (time.tm_mon != 10) throw std::runtime_error("timeio::get_6 fail.");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_get_7()
{
    dump_info("Test timeio<wchar_t> get 7...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("C"));
    {
        std::wstring input = L"PM01:38:12";
        std::wstring format = L"%p%I:%M:%S";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_hour != 13) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_min != 38) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_sec != 12) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::wstring input = L"05 37";
        std::wstring format = L"%C %y";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 537 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::wstring input = L"68";
        std::wstring format = L"%y";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 2068 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::wstring input = L"69";
        std::wstring format = L"%y";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_7 fail.");
        if (time.tm_year != 1969 - 1900) throw std::runtime_error("timeio::get_7 fail.");
    }

    {
        std::wstring input = L"03-Feb-2003";
        std::wstring format = L"%d-%b-%Y";

        IOv2::time_parse_context<wchar_t> ctx;
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
        std::wstring input = L"16-Dec-2020";
        std::wstring format = L"%d-%b-%Y";

        IOv2::time_parse_context<wchar_t> ctx;
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
        std::wstring input = L"16-Dec-2021";
        std::wstring format = L"%d-%b-%Y";

        IOv2::time_parse_context<wchar_t> ctx;
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
        std::wstring input = L"253 2020";
        std::wstring format = L"%j %Y";

        IOv2::time_parse_context<wchar_t> ctx;
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
        std::wstring input = L"233 2021";
        std::wstring format = L"%j %Y";

        IOv2::time_parse_context<wchar_t> ctx;
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
        std::wstring input = L"2020 23 3";
        std::wstring format = L"%Y %U %w";

        IOv2::time_parse_context<wchar_t> ctx;
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
        std::wstring input = L"2020 23 3";
        std::wstring format = L"%Y %W %w";

        IOv2::time_parse_context<wchar_t> ctx;
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
        std::wstring input = L"2021 43 Fri";
        std::wstring format = L"%Y %W %a";

        IOv2::time_parse_context<wchar_t> ctx;
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
        std::wstring input = L"2024 23 3";
        std::wstring format = L"%Y %U %w";

        IOv2::time_parse_context<wchar_t> ctx;
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
        std::wstring input = L"2024 23 3";
        std::wstring format = L"%Y %W %w";

        IOv2::time_parse_context<wchar_t> ctx;
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

void test_timeio_wchar_t_get_8()
{
    dump_info("Test timeio<wchar_t> get 8...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("C"));
    {
        std::wstring input = L"01:38:12 PM";
        std::wstring format = L"%I:%M:%S %p";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_hour != 13) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_min != 38) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_sec != 12) throw std::runtime_error("timeio::get_8 fail.");
    }
        
    {
        std::wstring input = L"11:17:42 PM";
        std::wstring format = L"%r";

        IOv2::time_parse_context<wchar_t> ctx;
        auto ret = obj.get(input.begin(), input.end(), ctx, format);
        auto time = static_cast<std::tm>(ctx);
        if (ret != input.end()) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_hour != 23) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_min != 17) throw std::runtime_error("timeio::get_8 fail.");
        if (time.tm_sec != 42) throw std::runtime_error("timeio::get_8 fail.");
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_get_9()
{
    dump_info("Test timeio<wchar_t> get 9...");

    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<wchar_t, true, true, false>, true, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(L"%",  L'%',  0,  IOv2::ios_defs::eofbit);
    FOri(L"x",  L'%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri(L"%",  L'%', L'E', febit);
    FOri(L"%E%", L'%', L'E', IOv2::ios_defs::eofbit);
    FOri(L"%",  L'%', L'O', febit);
    FOri(L"%O%", L'%', L'O', IOv2::ios_defs::eofbit);

    VERIFY(FOri(L"水", L'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri(L"%Ea", L'a', L'E', IOv2::ios_defs::eofbit);
    FOri(L"a",   L'a', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oa", L'a', L'O', IOv2::ios_defs::eofbit);
    FOri(L"a",   L'a', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"水曜日", L'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri(L"%EA", L'A', L'E', IOv2::ios_defs::eofbit);
    FOri(L"A",   L'A', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OA", L'A', L'O', IOv2::ios_defs::eofbit);
    FOri(L"A",   L'A', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"9月", L'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(L"%Eb", L'b', L'E', IOv2::ios_defs::eofbit);
    FOri(L"b",   L'b', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Ob", L'b', L'O', IOv2::ios_defs::eofbit);
    FOri(L"b",   L'b', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"9月", L'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(L"%EB", L'B', L'E', IOv2::ios_defs::eofbit);
    FOri(L"B",   L'B', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OB", L'B', L'O', IOv2::ios_defs::eofbit);
    FOri(L"B",   L'B', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"9月", L'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(L"%Eh", L'h', L'E', IOv2::ios_defs::eofbit);
    FOri(L"h",   L'h', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oh", L'h', L'O', IOv2::ios_defs::eofbit);
    FOri(L"h",   L'h', L'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    VERIFY(FYmd(L"2024年09月04日 13時33分18秒", L'c', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"令和6年09月04日 13時33分18秒", L'c', L'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"202409月04日 13時33分18秒", L'c', L'E', IOv2::ios_defs::eofbit) == check_date1);
    FOri(L"c",   L'c', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oc", L'c', L'O', IOv2::ios_defs::eofbit);
    FOri(L"c",   L'c', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"20", L'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(FYmd(L"平成", L'C', L'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1990));
    FOri(L"C",   L'C', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OC", L'C', L'O', IOv2::ios_defs::eofbit);
    FOri(L"C",   L'C', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"04", L'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(L"04", L'd', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(L"四", L'd', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri(L"%Ed", L'd', L'E', IOv2::ios_defs::eofbit);
    FOri(L"d",   L'd', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"d",   L'd', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"4", L'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(L"4", L'e', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(L"四", L'e', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri(L"%Ee", L'e', L'E', IOv2::ios_defs::eofbit);
    FOri(L"e",   L'e', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"e",   L'e', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(L"2024-09-04", L'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri(L"%EF", L'F', L'E', IOv2::ios_defs::eofbit);
    FOri(L"F",   L'F', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OF", L'F', L'O', IOv2::ios_defs::eofbit);
    FOri(L"F",   L'F', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(L"2024年09月04日", L'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"令和6年09月04日", L'x', L'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"202409月04日", L'x', L'E', IOv2::ios_defs::eofbit) == check_date1);
    FOri(L"x",   L'x', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Ox", L'x', L'O', IOv2::ios_defs::eofbit);
    FOri(L"x",   L'x', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(L"09/04/24", L'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri(L"%ED", L'D', L'E', IOv2::ios_defs::eofbit);
    FOri(L"D",   L'D', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OD", L'D', L'O', IOv2::ios_defs::eofbit);
    FOri(L"D",   L'D', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"13", L'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(L"13", L'H', L'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(L"十三", L'H', L'O', IOv2::ios_defs::eofbit).m_hour == 13);
    FOri(L"%EH", L'H', L'E', IOv2::ios_defs::eofbit);
    FOri(L"H",   L'H', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"H",   L'H', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"01", L'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(L"01", L'I', L'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(L"一", L'I', L'O', IOv2::ios_defs::eofbit).m_hour == 1);
    FOri(L"%EI", L'I', L'E', IOv2::ios_defs::eofbit);
    FOri(L"I",   L'I', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"I",   L'I', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"248", L'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(FYmd(L"2024 248", L"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    FOri(L"%Ej", L'j', L'E', IOv2::ios_defs::eofbit);
    FOri(L"j",   L'j', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oj", L'j', L'O', IOv2::ios_defs::eofbit);
    FOri(L"j",   L'j', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"09", L'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri(L"09", L'm', L'O', IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri(L"九", L'm', L'O', IOv2::ios_defs::eofbit).m_month == 9);
    FOri(L"%Em", L'm', L'E', IOv2::ios_defs::eofbit);
    FOri(L"m",   L'm', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"m",   L'm', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"33", L'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(L"33", L'M', L'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(L"三十三", L'M', L'O', IOv2::ios_defs::eofbit).m_minute == 33);
    FOri(L"%EM", L'M', L'E', IOv2::ios_defs::eofbit);
    FOri(L"M",   L'M', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"M",   L'M', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"\n",   L'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(L"x",    L'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(L"\n",   L'n', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%En",  L'n', L'E', IOv2::ios_defs::eofbit, 3);
    FOri(L"n",    L'n', L'O', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%On",  L'n', L'O', IOv2::ios_defs::eofbit, 3);

    FOri(L"\t",   L't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(L"x",    L't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(L"\t",   L't', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Et",  L't', L'E', IOv2::ios_defs::eofbit, 3);
    FOri(L"n",    L't', L'O', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Ot",  L't', L'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 午後", L"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"01 午前", L"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(FOri(L"午後", L'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(FOri(L"午前", L'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    FOri(L"%Ep", L'p', L'E', IOv2::ios_defs::eofbit);
    FOri(L"p",   L'p', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Op", L'p', L'O', IOv2::ios_defs::eofbit);
    FOri(L"p",   L'p', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"午後01時33分18秒", L"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"%Er", L'r', L'E', IOv2::ios_defs::eofbit);
    FOri(L"r",   L'r', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Or", L'r', L'O', IOv2::ios_defs::eofbit);
    FOri(L"r",   L'r', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33", L"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(L"%ER", L'R', L'E', IOv2::ios_defs::eofbit);
    FOri(L"R",   L'R', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OR", L'R', L'O', IOv2::ios_defs::eofbit);
    FOri(L"R",   L'R', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"18", L'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(L"18", L'S', L'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(L"十八", L'S', L'O', IOv2::ios_defs::eofbit).m_second == 18);
    FOri(L"%ES", L'S', L'E', IOv2::ios_defs::eofbit);
    FOri(L"S",   L'S', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"S",   L'S', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13時33分18秒 America/Los_Angeles", L"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13時33分18秒 America/Los_Angeles", L"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"X",   L'X', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OX", L'X', L'O', IOv2::ios_defs::eofbit);
    FOri(L"X",   L'X', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33:18", L"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"%ET", L'T', L'E', IOv2::ios_defs::eofbit);
    FOri(L"T",   L'T', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OT", L'T', L'O', IOv2::ios_defs::eofbit);
    FOri(L"T",   L'T', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"3", L'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(L"3", L'u', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(L"三", L'u', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri(L"%Eu", L'u', L'E', IOv2::ios_defs::eofbit);
    FOri(L"u",   L'u', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"u",   L'u', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"24", L'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri(L"%Eg", L'g', L'E', IOv2::ios_defs::eofbit);
    FOri(L"g",   L'g', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Og", L'g', L'O', IOv2::ios_defs::eofbit);
    FOri(L"g",   L'g', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"2024", L'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri(L"%EG", L'G', L'E', IOv2::ios_defs::eofbit);
    FOri(L"G",   L'G', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OG", L'G', L'O', IOv2::ios_defs::eofbit);
    FOri(L"G",   L'G', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(L"2024 35 水", L"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"2024 35 水", L"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"2024 三十五 水", L"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri(L"35", L'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(FOri(L"35", L'U', L'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    FOri(L"%EU", L'U', L'E', IOv2::ios_defs::eofbit);
    FOri(L"U",   L'U', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"U",   L'U', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(L"2024 36 水", L"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"2024 36 水", L"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"2024 三十六 水", L"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri(L"36", L'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(FOri(L"36", L'W', L'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    FOri(L"%EW", L'W', L'E', IOv2::ios_defs::eofbit);
    FOri(L"W",   L'W', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"W",   L'W', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"36", L'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri(L"36", L'V', L'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri(L"三十六", L'V', L'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    FOri(L"54",  L'V', L'O', IOv2::ios_defs::strfailbit, 1);
    FOri(L"%EV", L'V', L'E', IOv2::ios_defs::eofbit);
    FOri(L"V",   L'V', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"V",   L'V', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"3", L'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(L"3", L'w', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(L"三", L'w', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri(L"%Ew", L'w', L'E', IOv2::ios_defs::eofbit);
    FOri(L"w",   L'w', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"w",   L'w', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"24", L'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd(L"6", L'y', L'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(2024));
    VERIFY(FOri(L"24", L'y', L'O', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri(L"二十四", L'y', L'O', IOv2::ios_defs::eofbit).m_year == 2024);
    FOri(L"y",  L'y', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"y",  L'y', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"2024", L'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri(L"2024", L'Y', L'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd(L"平成3年", L'Y', L'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1991));
    FOri(L"Y",   L'Y', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OY", L'Y', L'O', IOv2::ios_defs::eofbit);
    FOri(L"Y",   L'Y', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%Z", L'Z', 0, IOv2::ios_defs::eofbit);
    FOri(L"%EZ", L'Z', L'E', IOv2::ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OZ", L'Z', L'O', IOv2::ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%z", L'z', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Ez", L'z', L'E', IOv2::ios_defs::eofbit);
    FOri(L"%Oz", L'z', L'O', IOv2::ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(FYmd(L"1999-W52-6", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd(L"2019-W01-1", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd(L"1999-W52-5", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd(L"99-W52-6", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd(L"19-W01-1", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd(L"99-W52-5", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd(L"20 24/09/04", L"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(FYmd(L"20 01 01", L"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_get_10()
{
    dump_info("Test timeio<wchar_t> get 10...");
    std::chrono::year_month_day check_date1{std::chrono::year{2024}, std::chrono::month{9}, std::chrono::day{4}};
    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<wchar_t, true, false, false>, true, false, false>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FYmd = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::year_month_day, true, false, false>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(L"%",  L'%',  0,  IOv2::ios_defs::eofbit);
    FOri(L"x",  L'%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri(L"%",  L'%', L'E', febit);
    FOri(L"%E%", L'%', L'E', IOv2::ios_defs::eofbit);
    FOri(L"%",  L'%', L'O', febit);
    FOri(L"%O%", L'%', L'O', IOv2::ios_defs::eofbit);

    VERIFY(FOri(L"水", L'a', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri(L"%Ea", L'a', L'E', IOv2::ios_defs::eofbit);
    FOri(L"a",   L'a', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oa", L'a', L'O', IOv2::ios_defs::eofbit);
    FOri(L"a",   L'a', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"水曜日", L'A', 0, IOv2::ios_defs::eofbit, 3).m_wday == 3);
    FOri(L"%EA", L'A', L'E', IOv2::ios_defs::eofbit);
    FOri(L"A",   L'A', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OA", L'A', L'O', IOv2::ios_defs::eofbit);
    FOri(L"A",   L'A', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"9月", L'b', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(L"%Eb", L'b', L'E', IOv2::ios_defs::eofbit);
    FOri(L"b",   L'b', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Ob", L'b', L'O', IOv2::ios_defs::eofbit);
    FOri(L"b",   L'b', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"9月", L'B', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(L"%EB", L'B', L'E', IOv2::ios_defs::eofbit);
    FOri(L"B",   L'B', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OB", L'B', L'O', IOv2::ios_defs::eofbit);
    FOri(L"B",   L'B', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"9月", L'h', 0, IOv2::ios_defs::eofbit, 3).m_month == 9);
    FOri(L"%Eh", L'h', L'E', IOv2::ios_defs::eofbit);
    FOri(L"h",   L'h', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oh", L'h', L'O', IOv2::ios_defs::eofbit);
    FOri(L"h",   L'h', L'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    FYmd(L"%c", L'c', 0, IOv2::ios_defs::eofbit);
    FYmd(L"%Ec", L'c', L'E', IOv2::ios_defs::eofbit);
    FOri(L"c",   L'c', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oc", L'c', L'O', IOv2::ios_defs::eofbit);
    FOri(L"c",   L'c', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"20", L'C', 0,   IOv2::ios_defs::eofbit).m_century == 20);
    VERIFY(FYmd(L"平成", L'C', L'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1990));
    FOri(L"C",   L'C', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OC", L'C', L'O', IOv2::ios_defs::eofbit);
    FOri(L"C",   L'C', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"04", L'd', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(L"04", L'd', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(L"四", L'd', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri(L"%Ed", L'd', L'E', IOv2::ios_defs::eofbit);
    FOri(L"d",   L'd', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"d",   L'd', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"4", L'e', 0,   IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(L"4", L'e', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    VERIFY(FOri(L"四", L'e', L'O', IOv2::ios_defs::eofbit).m_mday == 4);
    FOri(L"%Ee", L'e', L'E', IOv2::ios_defs::eofbit);
    FOri(L"e",   L'e', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"e",   L'e', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(L"2024-09-04", L'F', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri(L"%EF", 'F', L'E', IOv2::ios_defs::eofbit);
    FOri(L"F",   'F', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OF", 'F', L'O', IOv2::ios_defs::eofbit);
    FOri(L"F",   'F', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(L"2024年09月04日", L'x', 0, IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"令和6年09月04日", L'x', L'E', IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"202409月04日", L'x', L'E', IOv2::ios_defs::eofbit) == check_date1);
    FOri(L"x",   L'x', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Ox", L'x', L'O', IOv2::ios_defs::eofbit);
    FOri(L"x",   L'x', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(L"09/04/24", L'D', 0, IOv2::ios_defs::eofbit) == check_date1);
    FOri(L"%ED", L'D', L'E', IOv2::ios_defs::eofbit);
    FOri(L"D",   L'D', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OD", L'D', L'O', IOv2::ios_defs::eofbit);
    FOri(L"D",   L'D', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%H", L'H', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%EH", L'H', L'E', IOv2::ios_defs::eofbit);
    FOri(L"H",   L'H', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"H",   L'H', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%I", L'I', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%EI", L'I', L'E', IOv2::ios_defs::eofbit);
    FOri(L"I",   L'I', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"I",   L'I', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"248", L'j', 0, IOv2::ios_defs::eofbit).m_yday == 247);
    VERIFY(FYmd(L"2024 248", L"%Y %j", IOv2::ios_defs::eofbit) == check_date1);
    FOri(L"%Ej", L'j', L'E', IOv2::ios_defs::eofbit);
    FOri(L"j",   L'j', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oj", L'j', L'O', IOv2::ios_defs::eofbit);
    FOri(L"j",   L'j', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"09", L'm',  0, IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri(L"09", L'm', L'O', IOv2::ios_defs::eofbit).m_month == 9);
    VERIFY(FOri(L"九", L'm', L'O', IOv2::ios_defs::eofbit).m_month == 9);
    FOri(L"%Em", L'm', L'E', IOv2::ios_defs::eofbit);
    FOri(L"m",   L'm', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"m",   L'm', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%M", L'M', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%OM", L'M', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%EM", L'M', L'E', IOv2::ios_defs::eofbit);
    FOri(L"M",   L'M', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"M",   L'M', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"\n",   L'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(L"x",    L'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(L"\n",   L'n', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%En",  L'n', L'E', IOv2::ios_defs::eofbit, 3);
    FOri(L"n",    L'n', L'O', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%On",  L'n', L'O', IOv2::ios_defs::eofbit, 3);

    FOri(L"\t",   L't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(L"x",    L't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(L"\t",   L't', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Et",  L't', L'E', IOv2::ios_defs::eofbit, 3);
    FOri(L"n",    L't', L'O', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Ot",  L't', L'O', IOv2::ios_defs::eofbit, 3);

    FOri(L"%p", L'p', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Ep", L'p', L'E', IOv2::ios_defs::eofbit);
    FOri(L"p",   L'p', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Op", L'p', L'O', IOv2::ios_defs::eofbit);
    FOri(L"p",   L'p', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%r", L"%r",  IOv2::ios_defs::eofbit);
    FOri(L"%Er", L'r', L'E', IOv2::ios_defs::eofbit);
    FOri(L"r",   L'r', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Or", L'r', L'O', IOv2::ios_defs::eofbit);
    FOri(L"r",   L'r', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>>(obj, L"13:33", L"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(L"%ER", L'R', L'E', IOv2::ios_defs::eofbit);
    FOri(L"R",   L'R', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OR", L'R', L'O', IOv2::ios_defs::eofbit);
    FOri(L"R",   L'R', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%S", L'S', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%OS", L'S', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%ES", L'S', L'E', IOv2::ios_defs::eofbit);
    FOri(L"S",   L'S', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"S",   L'S', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%X", L"%X",  IOv2::ios_defs::eofbit);
    FOri(L"%EX", L"%EX",  IOv2::ios_defs::eofbit);
    FOri(L"X",   L'X', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OX", L'X', L'O', IOv2::ios_defs::eofbit);
    FOri(L"X",   L'X', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%T", L"%T",  IOv2::ios_defs::eofbit);
    FOri(L"%ET", L'T', L'E', IOv2::ios_defs::eofbit);
    FOri(L"T",   L'T', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OT", L'T', L'O', IOv2::ios_defs::eofbit);
    FOri(L"T",   L'T', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"3", L'u', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(L"3", L'u', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(L"三", L'u', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri(L"%Eu", L'u', L'E', IOv2::ios_defs::eofbit);
    FOri(L"u",   L'u', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"u",   L'u', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"24", L'g', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri(L"%Eg", L'g', L'E', IOv2::ios_defs::eofbit);
    FOri(L"g",   L'g', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Og", L'g', L'O', IOv2::ios_defs::eofbit);
    FOri(L"g",   L'g', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"2024", L'G', 0, IOv2::ios_defs::eofbit).m_iso_8601_year == 2024);
    FOri(L"%EG", L'G', L'E', IOv2::ios_defs::eofbit);
    FOri(L"G",   L'G', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OG", L'G', L'O', IOv2::ios_defs::eofbit);
    FOri(L"G",   L'G', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(L"2024 35 水", L"%Y %U %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"2024 35 水", L"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"2024 三十五 水", L"%Y %OU %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri(L"35", L'U', 0,   IOv2::ios_defs::eofbit).m_week_no == 35);
    VERIFY(FOri(L"35", L'U', L'O', IOv2::ios_defs::eofbit).m_week_no == 35);
    FOri(L"%EU", L'U', L'E', IOv2::ios_defs::eofbit);
    FOri(L"U",   L'U', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"U",   L'U', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FYmd(L"2024 36 水", L"%Y %W %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"2024 36 水", L"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FYmd(L"2024 三十六 水", L"%Y %OW %a", IOv2::ios_defs::eofbit) == check_date1);
    VERIFY(FOri(L"36", L'W', 0,   IOv2::ios_defs::eofbit).m_week_no == 36);
    VERIFY(FOri(L"36", L'W', L'O', IOv2::ios_defs::eofbit).m_week_no == 36);
    FOri(L"%EW", L'W', L'E', IOv2::ios_defs::eofbit);
    FOri(L"W",   L'W', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"W",   L'W', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"36", L'V', 0,   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri(L"36", L'V', L'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    VERIFY(FOri(L"三十六", L'V', L'O',   IOv2::ios_defs::eofbit).m_iso_8601_week == 36);
    FOri(L"54",  L'V', L'O', IOv2::ios_defs::strfailbit, 1);
    FOri(L"%EV", L'V', L'E', IOv2::ios_defs::eofbit);
    FOri(L"V",   L'V', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"V",   L'V', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"3", L'w', 0,   IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(L"3", L'w', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    VERIFY(FOri(L"三", L'w', L'O', IOv2::ios_defs::eofbit).m_wday == 3);
    FOri(L"%Ew", L'w', L'E', IOv2::ios_defs::eofbit);
    FOri(L"w",   L'w', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"w",   L'w', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"24", L'y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd(L"6", L'y', L'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(2024));
    VERIFY(FOri(L"24", L'y', L'O', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri(L"二十四", L'y', L'O', IOv2::ios_defs::eofbit).m_year == 2024);
    FOri(L"y",  L'y', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"y",  L'y', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"2024", L'Y', 0,   IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FOri(L"2024", L'Y', L'E', IOv2::ios_defs::eofbit).m_year == 2024);
    VERIFY(FYmd(L"平成3年", L'Y', L'E', IOv2::ios_defs::eofbit).year() == std::chrono::year(1991));
    FOri(L"Y",   L'Y', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OY", L'Y', L'O', IOv2::ios_defs::eofbit);
    FOri(L"Y",   L'Y', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%Z", L'Z', 0, IOv2::ios_defs::eofbit);
    FOri(L"%EZ", L'Z', L'E', IOv2::ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OZ", L'Z', L'O', IOv2::ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%z", L'z', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Ez", L'z', L'E', IOv2::ios_defs::eofbit);
    FOri(L"%Oz", L'z', L'O', IOv2::ios_defs::eofbit);

    std::chrono::year_month_day check_date2{std::chrono::year{2000}, std::chrono::month{1}, std::chrono::day{1}};
    std::chrono::year_month_day check_date3{std::chrono::year{2018}, std::chrono::month{12}, std::chrono::day{31}};
    std::chrono::year_month_day check_date4{std::chrono::year{1999}, std::chrono::month{12}, std::chrono::day{31}};

    VERIFY(FYmd(L"1999-W52-6", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd(L"2019-W01-1", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd(L"1999-W52-5", L"%G-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd(L"99-W52-6", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date2);
    VERIFY(FYmd(L"19-W01-1", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date3);
    VERIFY(FYmd(L"99-W52-5", L"%g-W%V-%u", IOv2::ios_defs::eofbit) == check_date4);

    VERIFY(FYmd(L"20 24/09/04", L"%C %y/%m/%d", IOv2::ios_defs::eofbit) == check_date1);
    {
        auto now = floor<days>(system_clock::now());
        year_month_day ymd{now};
        VERIFY(FYmd(L"20 01 01", L"%C %m %d", IOv2::ios_defs::eofbit) == year_month_day{ymd.year(), std::chrono::month{1}, std::chrono::day{1}});
    }

    dump_info("Done\n");
}

void test_timeio_wchar_t_get_11()
{
    dump_info("Test timeio<wchar_t> get 11...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<wchar_t, false, true, true>, false, true, true>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, true>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(L"%",  L'%',  0,  IOv2::ios_defs::eofbit);
    FOri(L"x",  L'%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri(L"%",  L'%', L'E', febit);
    FOri(L"%E%", L'%', L'E', IOv2::ios_defs::eofbit);
    FOri(L"%",  L'%', L'O', febit);
    FOri(L"%O%", L'%', L'O', IOv2::ios_defs::eofbit);

    FOri(L"%a", L'a', 0, IOv2::ios_defs::eofbit, 3);
    FOri(L"%Ea", L'a', L'E', IOv2::ios_defs::eofbit);
    FOri(L"a",   L'a', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oa", L'a', L'O', IOv2::ios_defs::eofbit);
    FOri(L"a",   L'a', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%A", L'A', 0, IOv2::ios_defs::eofbit, 3);
    FOri(L"%EA", L'A', L'E', IOv2::ios_defs::eofbit);
    FOri(L"A",   L'A', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OA", L'A', L'O', IOv2::ios_defs::eofbit);
    FOri(L"A",   L'A', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%b", L'b', 0, IOv2::ios_defs::eofbit, 3);
    FOri(L"%Eb", L'b', L'E', IOv2::ios_defs::eofbit);
    FOri(L"b",   L'b', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Ob", L'b', L'O', IOv2::ios_defs::eofbit);
    FOri(L"b",   L'b', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%B", L'B', 0, IOv2::ios_defs::eofbit, 3);
    FOri(L"%EB", L'B', L'E', IOv2::ios_defs::eofbit);
    FOri(L"B",   L'B', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OB", L'B', L'O', IOv2::ios_defs::eofbit);
    FOri(L"B",   L'B', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%h", L'h', 0, IOv2::ios_defs::eofbit, 3);
    FOri(L"%Eh", L'h', L'E', IOv2::ios_defs::eofbit);
    FOri(L"h",   L'h', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oh", L'h', L'O', IOv2::ios_defs::eofbit);
    FOri(L"h",   L'h', L'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    FOri(L"%c", L'c', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Ec", L'c', L'E', IOv2::ios_defs::eofbit);
    FOri(L"c",   L'c', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oc", L'c', L'O', IOv2::ios_defs::eofbit);
    FOri(L"c",   L'c', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%C", L'C', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%EC", L'C', L'E', IOv2::ios_defs::eofbit);
    FOri(L"C",   L'C', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OC", L'C', L'O', IOv2::ios_defs::eofbit);
    FOri(L"C",   L'C', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%d", L'd', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%Od", L'd', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%Ed", L'd', L'E', IOv2::ios_defs::eofbit);
    FOri(L"d",   L'd', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"d",   L'd', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%e", L'e', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%Oe", L'e', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%Ee", L'e', L'E', IOv2::ios_defs::eofbit);
    FOri(L"e",   L'e', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"e",   L'e', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%F", L'F', 0, IOv2::ios_defs::eofbit);
    FOri(L"%EF", L'F', L'E', IOv2::ios_defs::eofbit);
    FOri(L"F",   L'F', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OF", L'F', L'O', IOv2::ios_defs::eofbit);
    FOri(L"F",   L'F', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%x", L'x', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Ex", L'x', L'E', IOv2::ios_defs::eofbit);
    FOri(L"x",   L'x', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Ox", L'x', L'O', IOv2::ios_defs::eofbit);
    FOri(L"x",   L'x', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%D", L'D', 0, IOv2::ios_defs::eofbit);
    FOri(L"%ED", L'D', L'E', IOv2::ios_defs::eofbit);
    FOri(L"D",   L'D', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OD", L'D', L'O', IOv2::ios_defs::eofbit);
    FOri(L"D",   L'D', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"13", L'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(L"13", L'H', L'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(L"十三", L'H', L'O', IOv2::ios_defs::eofbit).m_hour == 13);
    FOri(L"%EH", L'H', L'E', IOv2::ios_defs::eofbit);
    FOri(L"H",   L'H', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"H",   L'H', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"01", L'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(L"01", L'I', L'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(L"一", L'I', L'O', IOv2::ios_defs::eofbit).m_hour == 1);
    FOri(L"%EI", L'I', L'E', IOv2::ios_defs::eofbit);
    FOri(L"I",   L'I', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"I",   L'I', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%j", L'j', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Ej", L'j', L'E', IOv2::ios_defs::eofbit);
    FOri(L"j",   L'j', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oj", L'j', L'O', IOv2::ios_defs::eofbit);
    FOri(L"j",   L'j', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%m", L'm',  0, IOv2::ios_defs::eofbit);
    FOri(L"%Om", L'm', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%Em", L'm', L'E', IOv2::ios_defs::eofbit);
    FOri(L"m",   L'm', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"m",   L'm', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"33", L'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(L"33", L'M', L'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(L"三十三", L'M', L'O', IOv2::ios_defs::eofbit).m_minute == 33);
    FOri(L"%EM", L'M', L'E', IOv2::ios_defs::eofbit);
    FOri(L"M",   L'M', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"M",   L'M', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"\n",   L'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(L"x",    L'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(L"\n",   L'n', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%En",  L'n', L'E', IOv2::ios_defs::eofbit, 3);
    FOri(L"n",    L'n', L'O', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%On",  L'n', L'O', IOv2::ios_defs::eofbit, 3);

    FOri(L"\t",   L't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(L"x",    L't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(L"\t",   L't', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Et",  L't', L'E', IOv2::ios_defs::eofbit, 3);
    FOri(L"n",    L't', L'O', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Ot",  L't', L'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(FHms(L"01 午後", L"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(FHms(L"01 午前", L"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(FOri(L"午後", L'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(FOri(L"午前", L'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    FOri(L"%Ep", L'p', L'E', IOv2::ios_defs::eofbit);
    FOri(L"p",   L'p', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Op", L'p', L'O', IOv2::ios_defs::eofbit);
    FOri(L"p",   L'p', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(L"午後01時33分18秒", L"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"%Er", L'r', L'E', IOv2::ios_defs::eofbit);
    FOri(L"r",   L'r', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Or", L'r', L'O', IOv2::ios_defs::eofbit);
    FOri(L"r",   L'r', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(L"13:33", L"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(L"%ER", L'R', L'E', IOv2::ios_defs::eofbit);
    FOri(L"R",   L'R', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OR", L'R', L'O', IOv2::ios_defs::eofbit);
    FOri(L"R",   L'R', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"18", L'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(L"18", L'S', L'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(L"十八", L'S', L'O', IOv2::ios_defs::eofbit).m_second == 18);
    FOri(L"%ES", L'S', L'E', IOv2::ios_defs::eofbit);
    FOri(L"S",   L'S', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"S",   L'S', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(L"13時33分18秒 America/Los_Angeles", L"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(FHms(L"13時33分18秒 America/Los_Angeles", L"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"X",   L'X', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OX", L'X', L'O', IOv2::ios_defs::eofbit);
    FOri(L"X",   L'X', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(L"13:33:18", L"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"%ET", L'T', L'E', IOv2::ios_defs::eofbit);
    FOri(L"T",   L'T', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OT", L'T', L'O', IOv2::ios_defs::eofbit);
    FOri(L"T",   L'T', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%u", L'u', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%Ou", L'u', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%Eu", L'u', L'E', IOv2::ios_defs::eofbit);
    FOri(L"u",   L'u', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"u",   L'u', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%g", L'g', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Eg", L'g', L'E', IOv2::ios_defs::eofbit);
    FOri(L"g",   L'g', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Og", L'g', L'O', IOv2::ios_defs::eofbit);
    FOri(L"g",   L'g', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%G", L'G', 0, IOv2::ios_defs::eofbit);
    FOri(L"%EG", L'G', L'E', IOv2::ios_defs::eofbit);
    FOri(L"G",   L'G', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OG", L'G', L'O', IOv2::ios_defs::eofbit);
    FOri(L"G",   L'G', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%U", L'U', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%OU", L'U', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%EU", L'U', L'E', IOv2::ios_defs::eofbit);
    FOri(L"U",   L'U', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"U",   L'U', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%W", L'W', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%OW", L'W', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%EW", L'W', L'E', IOv2::ios_defs::eofbit);
    FOri(L"W",   L'W', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"W",   L'W', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%V", L'V', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%OV", L'V', L'O',   IOv2::ios_defs::eofbit);
    FOri(L"54",  L'V', L'O', IOv2::ios_defs::strfailbit, 1);
    FOri(L"%EV", L'V', L'E', IOv2::ios_defs::eofbit);
    FOri(L"V",   L'V', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"V",   L'V', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%w", L'w', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%Ow", L'w', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%Ew", L'w', L'E', IOv2::ios_defs::eofbit);
    FOri(L"w",   L'w', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"w",   L'w', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%y", L'y', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%Ey", L'y', L'E', IOv2::ios_defs::eofbit);
    FOri(L"%Oy", L'y', L'O', IOv2::ios_defs::eofbit);
    FOri(L"y",  L'y', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"y",  L'y', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%Y", L'Y', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%EY", L'Y', L'E', IOv2::ios_defs::eofbit);
    FOri(L"Y",   L'Y', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OY", L'Y', L'O', IOv2::ios_defs::eofbit);
    FOri(L"Y",   L'Y', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"America/Los_Angeles", L'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "America/Los_Angeles");
    VERIFY(FOri(L"PST", L'Z', 0, IOv2::ios_defs::eofbit).m_zone_name == "");
    FOri(L"America/Los_Angexes", L'Z', 0, IOv2::ios_defs::strfailbit);
    FOri(L"%EZ", L'Z', L'E', IOv2::ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OZ", L'Z', L'O', IOv2::ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%z", L'z', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Ez", L'z', L'E', IOv2::ios_defs::eofbit);
    FOri(L"z",  L'z', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oz", L'z', L'O', IOv2::ios_defs::eofbit);
    FOri(L"z",  L'z', L'O', IOv2::ios_defs::strfailbit, 0);

    dump_info("Done\n");
}

void test_timeio_wchar_t_get_12()
{
    dump_info("Test timeio<wchar_t> get 12...");

    IOv2::timeio obj(std::make_shared<IOv2::timeio_conf<wchar_t>>("ja_JP.UTF-8"));
    auto FOri = [&obj](auto&&... args)
    {
        return CheckGet<IOv2::time_parse_context<wchar_t, false, true, false>, false, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    auto FHms = [&obj](auto&&... args)
    {
        return CheckGet<std::chrono::hh_mm_ss<std::chrono::seconds>, false, true, false>(obj, std::forward<decltype(args)>(args)...);
    };

    FOri(L"%",  L'%',  0,  IOv2::ios_defs::eofbit);
    FOri(L"x",  L'%',  0,  IOv2::ios_defs::strfailbit, 0);
    FOri(L"%",  L'%', L'E', febit);
    FOri(L"%E%", L'%', L'E', IOv2::ios_defs::eofbit);
    FOri(L"%",  L'%', L'O', febit);
    FOri(L"%O%", L'%', L'O', IOv2::ios_defs::eofbit);

    FOri(L"%a", L'a', 0, IOv2::ios_defs::eofbit, 3);
    FOri(L"%Ea", L'a', L'E', IOv2::ios_defs::eofbit);
    FOri(L"a",   L'a', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oa", L'a', L'O', IOv2::ios_defs::eofbit);
    FOri(L"a",   L'a', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%A", L'A', 0, IOv2::ios_defs::eofbit, 3);
    FOri(L"%EA", L'A', L'E', IOv2::ios_defs::eofbit);
    FOri(L"A",   L'A', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OA", L'A', L'O', IOv2::ios_defs::eofbit);
    FOri(L"A",   L'A', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%b", L'b', 0, IOv2::ios_defs::eofbit, 3);
    FOri(L"%Eb", L'b', L'E', IOv2::ios_defs::eofbit);
    FOri(L"b",   L'b', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Ob", L'b', L'O', IOv2::ios_defs::eofbit);
    FOri(L"b",   L'b', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%B", L'B', 0, IOv2::ios_defs::eofbit, 3);
    FOri(L"%EB", L'B', L'E', IOv2::ios_defs::eofbit);
    FOri(L"B",   L'B', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OB", L'B', L'O', IOv2::ios_defs::eofbit);
    FOri(L"B",   L'B', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%h", L'h', 0, IOv2::ios_defs::eofbit, 3);
    FOri(L"%Eh", L'h', L'E', IOv2::ios_defs::eofbit);
    FOri(L"h",   L'h', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oh", L'h', L'O', IOv2::ios_defs::eofbit);
    FOri(L"h",   L'h', L'O', IOv2::ios_defs::strfailbit, 0);

    using namespace std::chrono;
    FOri(L"%c", L'c', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Ec", L'c', L'E', IOv2::ios_defs::eofbit);
    FOri(L"c",   L'c', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oc", L'c', L'O', IOv2::ios_defs::eofbit);
    FOri(L"c",   L'c', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%C", L'C', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%EC", L'C', L'E', IOv2::ios_defs::eofbit);
    FOri(L"C",   L'C', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OC", L'C', L'O', IOv2::ios_defs::eofbit);
    FOri(L"C",   L'C', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%d", L'd', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%Od", L'd', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%Ed", L'd', L'E', IOv2::ios_defs::eofbit);
    FOri(L"d",   L'd', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"d",   L'd', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%e", L'e', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%Oe", L'e', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%Ee", L'e', L'E', IOv2::ios_defs::eofbit);
    FOri(L"e",   L'e', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"e",   L'e', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%F", L'F', 0, IOv2::ios_defs::eofbit);
    FOri(L"%EF", L'F', L'E', IOv2::ios_defs::eofbit);
    FOri(L"F",   L'F', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OF", L'F', L'O', IOv2::ios_defs::eofbit);
    FOri(L"F",   L'F', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%x", L'x', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Ex", L'x', L'E', IOv2::ios_defs::eofbit);
    FOri(L"x",   L'x', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Ox", L'x', L'O', IOv2::ios_defs::eofbit);
    FOri(L"x",   L'x', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%D", L'D', 0, IOv2::ios_defs::eofbit);
    FOri(L"%ED", L'D', L'E', IOv2::ios_defs::eofbit);
    FOri(L"D",   L'D', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OD", L'D', L'O', IOv2::ios_defs::eofbit);
    FOri(L"D",   L'D', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"13", L'H', 0,   IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(L"13", L'H', L'O', IOv2::ios_defs::eofbit).m_hour == 13);
    VERIFY(FOri(L"十三", L'H', L'O', IOv2::ios_defs::eofbit).m_hour == 13);
    FOri(L"%EH", L'H', L'E', IOv2::ios_defs::eofbit);
    FOri(L"H",   L'H', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"H",   L'H', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"01", L'I', 0,   IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(L"01", L'I', L'O', IOv2::ios_defs::eofbit).m_hour == 1);
    VERIFY(FOri(L"一", L'I', L'O', IOv2::ios_defs::eofbit).m_hour == 1);
    FOri(L"%EI", L'I', L'E', IOv2::ios_defs::eofbit);
    FOri(L"I",   L'I', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"I",   L'I', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%j", L'j', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Ej", L'j', L'E', IOv2::ios_defs::eofbit);
    FOri(L"j",   L'j', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oj", L'j', L'O', IOv2::ios_defs::eofbit);
    FOri(L"j",   L'j', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%m", L'm',  0, IOv2::ios_defs::eofbit);
    FOri(L"%Om", L'm', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%Em", L'm', L'E', IOv2::ios_defs::eofbit);
    FOri(L"m",   L'm', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"m",   L'm', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"33", L'M', 0,   IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(L"33", L'M', L'O', IOv2::ios_defs::eofbit).m_minute == 33);
    VERIFY(FOri(L"三十三", 'M', L'O', IOv2::ios_defs::eofbit).m_minute == 33);
    FOri(L"%EM", L'M', L'E', IOv2::ios_defs::eofbit);
    FOri(L"M",   L'M', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"M",   L'M', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"\n",   L'n',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(L"x",    L'n',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(L"\n",   L'n', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%En",  L'n', L'E', IOv2::ios_defs::eofbit, 3);
    FOri(L"n",    L'n', L'O', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%On",  L'n', L'O', IOv2::ios_defs::eofbit, 3);

    FOri(L"\t",   L't',  0,  IOv2::ios_defs::eofbit, 1);
    FOri(L"x",    L't',  0,  IOv2::ios_defs::goodbit, 0);
    FOri(L"\t",   L't', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Et",  L't', L'E', IOv2::ios_defs::eofbit, 3);
    FOri(L"n",    L't', L'O', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Ot",  L't', L'O', IOv2::ios_defs::eofbit, 3);

    VERIFY(FHms(L"01 午後", L"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(13));
    VERIFY(FHms(L"01 午前", L"%I %p",  IOv2::ios_defs::eofbit).hours() == std::chrono::hours(1));
    VERIFY(FOri(L"午後", L'p', 0, IOv2::ios_defs::eofbit).m_is_pm == true);
    VERIFY(FOri(L"午前", L'p', 0, IOv2::ios_defs::eofbit).m_is_pm == false);
    FOri(L"%Ep", L'p', L'E', IOv2::ios_defs::eofbit);
    FOri(L"p",   L'p', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Op", L'p', L'O', IOv2::ios_defs::eofbit);
    FOri(L"p",   L'p', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(L"午後01時33分18秒", L"%r",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"%Er", L'r', L'E', IOv2::ios_defs::eofbit);
    FOri(L"r",   L'r', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Or", L'r', L'O', IOv2::ios_defs::eofbit);
    FOri(L"r",   L'r', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(L"13:33", L"%R",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33}}.to_duration());
    FOri(L"%ER", L'R', L'E', IOv2::ios_defs::eofbit);
    FOri(L"R",   L'R', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OR", L'R', L'O', IOv2::ios_defs::eofbit);
    FOri(L"R",   L'R', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FOri(L"18", L'S', 0,   IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(L"18", L'S', L'O', IOv2::ios_defs::eofbit).m_second == 18);
    VERIFY(FOri(L"十八", L'S', L'O', IOv2::ios_defs::eofbit).m_second == 18);
    FOri(L"%ES", L'S', L'E', IOv2::ios_defs::eofbit);
    FOri(L"S",   L'S', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"S",   L'S', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(L"13時33分18秒", L"%X",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    VERIFY(FHms(L"13時33分18秒", L"%EX",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"X",   L'X', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OX", L'X', L'O', IOv2::ios_defs::eofbit);
    FOri(L"X",   L'X', L'O', IOv2::ios_defs::strfailbit, 0);

    VERIFY(FHms(L"13:33:18", L"%T",  IOv2::ios_defs::eofbit).to_duration()
           == std::chrono::hh_mm_ss<std::chrono::seconds>{std::chrono::hours{13} + std::chrono::minutes{33} + std::chrono::seconds{18}}.to_duration());
    FOri(L"%ET", L'T', L'E', IOv2::ios_defs::eofbit);
    FOri(L"T",   L'T', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OT", L'T', L'O', IOv2::ios_defs::eofbit);
    FOri(L"T",   L'T', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%u", L'u', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%Ou", L'u', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%Eu", L'u', L'E', IOv2::ios_defs::eofbit);
    FOri(L"u",   L'u', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"u",   L'u', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%g", L'g', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Eg", L'g', L'E', IOv2::ios_defs::eofbit);
    FOri(L"g",   L'g', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Og", L'g', L'O', IOv2::ios_defs::eofbit);
    FOri(L"g",   L'g', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%G", L'G', 0, IOv2::ios_defs::eofbit);
    FOri(L"%EG", L'G', L'E', IOv2::ios_defs::eofbit);
    FOri(L"G",   L'G', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OG", L'G', L'O', IOv2::ios_defs::eofbit);
    FOri(L"G",   L'G', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%U", L'U', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%OU", L'U', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%EU", L'U', L'E', IOv2::ios_defs::eofbit);
    FOri(L"U",   L'U', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"U",   L'U', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%W", L'W', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%OW", L'W', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%EW", L'W', L'E', IOv2::ios_defs::eofbit);
    FOri(L"W",   L'W', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"W",   L'W', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%V", L'V', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%OV", L'V', L'O',   IOv2::ios_defs::eofbit);
    FOri(L"54",  L'V', L'O', IOv2::ios_defs::strfailbit, 1);
    FOri(L"%EV", L'V', L'E', IOv2::ios_defs::eofbit);
    FOri(L"V",   L'V', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"V",   L'V', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%w", L'w', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%Ow", L'w', L'O', IOv2::ios_defs::eofbit);
    FOri(L"%Ew", L'w', L'E', IOv2::ios_defs::eofbit);
    FOri(L"w",   L'w', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"w",   L'w', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%y", L'y', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%Ey", L'y', L'E', IOv2::ios_defs::eofbit);
    FOri(L"%Oy", L'y', L'O', IOv2::ios_defs::eofbit);
    FOri(L"y",  L'y', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"y",  L'y', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%Y", L'Y', 0,   IOv2::ios_defs::eofbit);
    FOri(L"%EY", L'Y', L'E', IOv2::ios_defs::eofbit);
    FOri(L"Y",   L'Y', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OY", L'Y', L'O', IOv2::ios_defs::eofbit);
    FOri(L"Y",   L'Y', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%Z", L'Z', 0, IOv2::ios_defs::eofbit);
    FOri(L"%EZ", L'Z', L'E', IOv2::ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%OZ", L'Z', L'O', IOv2::ios_defs::eofbit);
    FOri(L"Z",   L'Z', L'O', IOv2::ios_defs::strfailbit, 0);

    FOri(L"%z", L'z', 0, IOv2::ios_defs::eofbit);
    FOri(L"%Ez", L'z', L'E', IOv2::ios_defs::eofbit);
    FOri(L"z",  L'z', L'E', IOv2::ios_defs::strfailbit, 0);
    FOri(L"%Oz", L'z', L'O', IOv2::ios_defs::eofbit);
    FOri(L"z",  L'z', L'O', IOv2::ios_defs::strfailbit, 0);

    dump_info("Done\n");
}