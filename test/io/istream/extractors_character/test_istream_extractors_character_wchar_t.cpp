#include <limits>
#include <stdexcept>
#include <string>
#include <cvt/code_cvt.h>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/file_guard.h>
#include <common/verify.h>

void test_istream_extractors_character_wchar_t_1()
{
    dump_info("Test istream<wchar_t> operator>> (character) case 1...");
    auto helper = []<template<typename, typename> class T>()
    {
        const std::wstring str_02(L"coltrane playing 'softly as a morning sunrise'");
        const std::wstring str_03(L"coltrane");

        T is_01(IOv2::mem_device{L""});
        T is_02(IOv2::mem_device{str_02});
        IOv2::ios_defs::iostate state1, state2, statefail;
        statefail = IOv2::ios_defs::strfailbit;

        // template<_CharT, _Traits>
        //  basic_istream& operator>>(istream&, _CharT*)
        const int n = 20;
        wchar_t array1[n];
        array1[0] = L'\0';

        state1 = is_01.rdstate();
        is_01 >> array1;   // should snake 0 characters, not alter stream state
        state2 = is_01.rdstate();
        VERIFY(state1 != state2);
        VERIFY(state2 & statefail);

        state1 = is_02.rdstate();
        is_02 >> array1;   // should snake "coltrane"
        state2 = is_02.rdstate();
        VERIFY(state1 == state2);
        VERIFY((state2 & statefail) == 0);
        VERIFY(array1[str_03.size() - 1] == L'e');
        array1[str_03.size()] = L'\0';
        VERIFY(str_03.compare(0, str_03.size(), array1) == 0);
        auto int1 = is_02.peek(); // should be ' '
        VERIFY(int1 == L' ');

        state1 = is_02.rdstate();
        is_02 >> array1;   // should snake "playing" as sentry "eats" ws
        state2 = is_02.rdstate();
        int1 = is_02.peek(); // should be ' '
        VERIFY(int1 == L' ');
        VERIFY(state1 == state2);
        VERIFY((state2 & statefail) == 0);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_character_wchar_t_2()
{
    dump_info("Test istream<wchar_t> operator>> (character) case 2...");
    auto helper = []<template<typename, typename> class T>()
    {
        const std::wstring str_02(L"or coltrane playing tunji with jimmy garrison");

        T is_01(IOv2::mem_device{L""});
        T is_02(IOv2::mem_device{str_02});
        IOv2::ios_defs::iostate state1, state2, statefail;
        statefail = IOv2::ios_defs::strfailbit;

        // template<_CharT, _Traits>
        //  basic_istream& operator>>(istream&, _CharT&)
        wchar_t c1 = L'c', c2 = L'c';
        state1 = is_01.rdstate();
        is_01 >> c1;   
        state2 = is_01.rdstate();
        VERIFY(state1 != state2);
        VERIFY(c1 == c2);
        VERIFY(state2 & statefail);

        state1 = is_02.rdstate();
        is_02 >> c1;   
        state2 = is_02.rdstate();
        VERIFY(state1 == state2);
        VERIFY(c1 == L'o');
        is_02 >> c1;
        is_02 >> c1;
        VERIFY(c1 == L'c');
        VERIFY((state2 & statefail) == 0);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_character_wchar_t_3()
{
    dump_info("Test istream<wchar_t> operator>> (character) case 3...");
    auto helper = []<template<typename, typename> class T>()
    {
        const std::wstring str_02(L"coltrane playing 'softly as a morning sunrise'");
        const std::wstring str_03(L"coltran");
        T is_02(IOv2::mem_device{str_02});
        IOv2::ios_defs::iostate state1, state2, statefail;
        statefail = IOv2::ios_defs::strfailbit;

        // template<_CharT, _Traits>
        //  basic_istream& operator>>(istream&, _CharT*)
        const int n = 20;
        wchar_t array1[n];

        // testing with width() control enabled.
        is_02.width(8);
        state1 = is_02.rdstate();
        is_02 >> array1;   // should snake "coltran"
        state2 = is_02.rdstate();
        VERIFY(state1 == state2);
        VERIFY(str_03.compare(0, str_03.size(), array1) == 0);

        is_02.width(1);
        state1 = is_02.rdstate();
        is_02 >> array1;   // should snake nothing, set failbit
        state2 = is_02.rdstate();
        VERIFY(state1 != state2);
        VERIFY(state2 == statefail);
        VERIFY(array1[0] == L'\0');

        is_02.width(8);
        is_02.clear();
        state1 = is_02.rdstate();
        VERIFY(!state1);
        is_02 >> array1;   // should snake "e"
        state2 = is_02.rdstate();
        VERIFY(state1 == state2);
        VERIFY(array1[0] == L'e');

        // testing for correct exception setting
        T is_03(IOv2::mem_device{L"   impulse!!"});
        T is_04(IOv2::mem_device{L"   impulse!!"});

        is_03 >> array1;
        VERIFY(std::wstring(array1) == L"impulse!!");
        VERIFY(is_03.rdstate() == IOv2::ios_defs::eofbit);

        is_04.width(9);
        is_04 >> array1;
        VERIFY(std::wstring(array1) == L"impulse!");
        VERIFY(is_04.rdstate() == 0);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_character_wchar_t_4()
{
    dump_info("Test istream<wchar_t> operator>> (character) case 4...");
    auto prepare = [](std::wstring::size_type len, unsigned nchunks)
    {
        std::wstring ret;
        for (unsigned i = 0; i < nchunks; ++i)
        {
            for (std::string::size_type j = 0; j < len; ++j)
                ret.push_back('a' + rand() % 26);
            len *= 2;
            ret.push_back(' ');
        }
        return ret;
    };
    
    auto check = [](auto& stream, const std::wstring& str, unsigned nchunks)
    {
        wchar_t* chunk = new wchar_t[str.size()];
        memset(chunk, 'X', str.size());

        std::string::size_type index = 0, index_new = 0;
        unsigned n = 0;

        while (stream >> chunk)
        {
            index_new = str.find(' ', index);
            VERIFY(str.compare(index, index_new - index, chunk) == 0);
            index = index_new + 1;
            ++n;
            memset(chunk, 'X', str.size());
        }
        VERIFY(stream.eof());
        VERIFY(n == nchunks);

        delete[] chunk;
    };

    auto helper = [&prepare, &check]<template<typename, typename> class T>()
    {
        std::string filename = "inserters_extractors-4.txt";
        file_guard g(filename);

        const unsigned nchunks = 10;
        const std::wstring data = prepare(666, nchunks);
        IOv2::ostream ofstream(IOv2::ofile_device<char>{filename},
                            IOv2::code_cvt_creator<char, wchar_t>("zh_CN.UTF-8"));
        ofstream.write(data.data(), data.size());
        ofstream.detach().close();

        T ifstrm(IOv2::ifile_device<char>{filename},
                 IOv2::code_cvt_creator<char, wchar_t>("zh_CN.UTF-8"));
        check(ifstrm, data, nchunks);
        ifstrm.detach().close();
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_character_wchar_t_5()
{
    dump_info("Test istream<wchar_t> operator>> (character) case 5...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::wstring str_01(L"Consoli ");
        T is_01(IOv2::mem_device{str_01});

        IOv2::ios_defs::iostate state1, state2;

        wchar_t array1[10];

        is_01.width(-60);
        state1 = is_01.rdstate();
        is_01 >> array1;
        state2 = is_01.rdstate();

        VERIFY(state1 == state2);
        VERIFY(str_01.compare(0, 7, array1) == 0);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_character_wchar_t_6()
{
    dump_info("Test istream<wchar_t> operator>> (character) case 6...");
    auto helper = []<template<typename, typename> class T>()
    {
        T in(IOv2::mem_device{L"abc d e"});

        wchar_t pc[3];
        in >> pc;
        VERIFY(in.good());
        VERIFY(pc[0] == L'a' && pc[1] == L'b' && pc[2] == L'\0');

        pc[2] = L'#';
        in >> pc;
        VERIFY(in.good());
        VERIFY(pc[0] == L'c' && pc[1] == L'\0' && pc[2] == L'#');

        pc[2] = L'#';
        in >> pc;
        VERIFY(in.good());
        VERIFY(pc[0] == L'd' && pc[1] == L'\0' && pc[2] == L'#');

        pc[2] = '#';
        in >> pc;
        VERIFY(in.eof());
        VERIFY(pc[0] == L'e' && pc[1] == L'\0' && pc[2] == L'#');
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_character_wchar_t_7()
{
    dump_info("Test istream<wchar_t> operator>> (character) case 7...");
    auto helper = []<template<typename, typename> class T>()
    {
        wchar_t buf[5] = {L'x', L'x', L'x', L'x', L'x'};
        std::wstring s(L"  four");

        T in(IOv2::mem_device{s});
        in >> buf;
        VERIFY(in.eof());
        VERIFY(buf[4] == L'\0');
        VERIFY(std::wstring(buf) == L"four");

        in.clear();
        in.attach(IOv2::mem_device{s});
        for (int i = 0; i < 5; ++i)
            buf[i] = L'x';
        in.width(5);
        in >> buf;
        VERIFY(in.eof());
        VERIFY(std::wstring(buf) == L"four");
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}
