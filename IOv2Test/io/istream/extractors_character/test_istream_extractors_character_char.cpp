#include <limits>
#include <stdexcept>
#include <string>
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

void test_istream_extractors_character_char_1()
{
    dump_info("Test istream<char> operator>> (character) case 1...");
    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str_02("coltrane playing 'softly as a morning sunrise'");
        const std::string str_03("coltrane");

        T is_01(IOv2::mem_device{""});
        T is_02(IOv2::mem_device{str_02});
        IOv2::ios_defs::iostate state1, state2, statefail;
        statefail = IOv2::ios_defs::strfailbit;

        // template<_CharT, _Traits>
        //  basic_istream& operator>>(istream&, _CharT*)
        const int n = 20;
        char array1[n];
        array1[0] = '\0';

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
        VERIFY(array1[str_03.size() - 1] == 'e');
        array1[str_03.size()] = '\0';
        VERIFY(str_03.compare(0, str_03.size(), array1) == 0);
        auto int1 = is_02.peek(); // should be ' '
        VERIFY(int1 == ' ');

        state1 = is_02.rdstate();
        is_02 >> array1;   // should snake "playing" as sentry "eats" ws
        state2 = is_02.rdstate();
        int1 = is_02.peek(); // should be ' '
        VERIFY(int1 == ' ');
        VERIFY(state1 == state2);
        VERIFY((state2 & statefail) == 0);

        // template<_CharT, _Traits>
        //  basic_istream& operator>>(istream&, unsigned char*)
        unsigned char array2[n];
        state1 = is_02.rdstate();
        is_02 >> array2;   // should snake 'softly
        state2 = is_02.rdstate();
        VERIFY(state1 == state2);
        VERIFY((state2 & statefail) == 0);
        VERIFY(array2[0] == '\'');
        VERIFY(array2[1] == 's');
        VERIFY(array2[6] == 'y');
        int1 = is_02.peek(); // should be ' '
        VERIFY(int1 == ' ');

        // template<_CharT, _Traits>
        //  basic_istream& operator>>(istream&, signed char*)
        signed char array3[n];
        state1 = is_02.rdstate();
        is_02 >> array3;   // should snake "as"
        state2 = is_02.rdstate();
        VERIFY(state1 == state2);
        VERIFY((state2 & statefail) == 0);
        VERIFY(array3[0] == 'a');
        VERIFY(array3[1] == 's');
        int1 = is_02.peek(); // should be ' '
        VERIFY(int1 == ' ');
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_character_char_2()
{
    dump_info("Test istream<char> operator>> (character) case 2...");
    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str_02("or coltrane playing tunji with jimmy garrison");

        T is_01(IOv2::mem_device{""});
        T is_02(IOv2::mem_device{str_02});
        IOv2::ios_defs::iostate state1, state2, statefail;
        statefail = IOv2::ios_defs::strfailbit;

        // template<_CharT, _Traits>
        //  basic_istream& operator>>(istream&, _CharT&)
        char c1 = 'c', c2 = 'c';
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
        VERIFY(c1 == 'o');
        is_02 >> c1;
        is_02 >> c1;
        VERIFY(c1 == 'c');
        VERIFY((state2 & statefail) == 0);

        // template<_CharT, _Traits>
        //  basic_istream& operator>>(istream&, unsigned char&)
        unsigned char uc1 = 'c';
        state1 = is_02.rdstate();
        is_02 >> uc1;
        state2 = is_02.rdstate();
        VERIFY(state1 == state2);
        VERIFY(uc1 == 'o');
        is_02 >> uc1;
        is_02 >> uc1;
        VERIFY(uc1 == 't');

        // template<_CharT, _Traits>
        //  basic_istream& operator>>(istream&, signed char&)
        signed char sc1 = 'c';
        state1 = is_02.rdstate();
        is_02 >> sc1;   
        state2 = is_02.rdstate();
        VERIFY(state1 == state2);
        VERIFY(sc1 == 'r');
        is_02 >> sc1;
        is_02 >> sc1;
        VERIFY(sc1 == 'n');
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_character_char_3()
{
    dump_info("Test istream<char> operator>> (character) case 3...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str_02("coltrane playing 'softly as a morning sunrise'");
        const std::string str_03("coltran");
        T is_02(IOv2::mem_device{str_02});
        IOv2::ios_defs::iostate state1, state2, statefail;
        statefail = IOv2::ios_defs::strfailbit;

        // template<_CharT, _Traits>
        //  basic_istream& operator>>(istream&, _CharT*)
        const int n = 20;
        char array1[n];

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
        VERIFY(array1[0] == '\0');

        is_02.width(8);
        is_02.clear();
        state1 = is_02.rdstate();
        VERIFY(!state1);
        is_02 >> array1;   // should snake "e"
        state2 = is_02.rdstate();
        VERIFY(state1 == state2);
        VERIFY(array1[0] == 'e');

        // testing for correct exception setting
        T is_03(IOv2::mem_device{"   impulse!!"});
        T is_04(IOv2::mem_device{"   impulse!!"});

        is_03 >> array1;
        VERIFY(std::string(array1) == "impulse!!");
        VERIFY(is_03.rdstate() == IOv2::ios_defs::eofbit);

        is_04.width(9);
        is_04 >> array1;
        VERIFY(std::string(array1) == "impulse!");
        VERIFY(is_04.rdstate() == 0);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_character_char_4()
{
    dump_info("Test istream<char> operator>> (character) case 4...");
    auto prepare = [](std::string::size_type len, unsigned nchunks)
    {
        std::string ret;
        for (unsigned i = 0; i < nchunks; ++i)
        {
            for (std::string::size_type j = 0; j < len; ++j)
                ret.push_back('a' + rand() % 26);
            len *= 2;
            ret.push_back(' ');
        }
        return ret;
    };
    
    auto check = [](auto& stream, const std::string& str, unsigned nchunks)
    {
        char* chunk = new char[str.size()];
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
        const std::string data = prepare(666, nchunks);
        IOv2::ostream ofstream(IOv2::ofile_device<char>{filename});
        ofstream.write(data.data(), data.size());
        ofstream.detach().close();

        T ifstrm(IOv2::ifile_device<char>{filename});
        check(ifstrm, data, nchunks);
        ifstrm.detach().close();
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_character_char_5()
{
    dump_info("Test istream<char> operator>> (character) case 5...");
    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str_01("Consoli ");
        T is_01(IOv2::mem_device{str_01});

        IOv2::ios_defs::iostate state1, state2;

        char array1[10];

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

void test_istream_extractors_character_char_6()
{
    dump_info("Test istream<char> operator>> (character) case 6...");
    auto helper = []<template<typename, typename> class T>()
    {
        T in(IOv2::mem_device{"abcdefghi jk l"});

        char pc[3];
        in >> pc;
        VERIFY(in.good());
        VERIFY(pc[0] == 'a' && pc[1] == 'b' && pc[2] == '\0');

        signed char sc[4];
        in >> sc;
        VERIFY(in.good());
        VERIFY(sc[0] == 'c' && sc[1] == 'd' && sc[2] == 'e' && sc[3] == '\0');

        unsigned char uc[4];
        in >> uc;
        VERIFY(in.good());
        VERIFY(uc[0] == 'f' && uc[1] == 'g' && uc[2] == 'h' && uc[3] == '\0');

        pc[2] = '#';
        in >> pc;
        VERIFY(in.good());
        VERIFY(pc[0] == 'i' && pc[1] == '\0' && pc[2] == '#');

        in >> pc;
        VERIFY(in.good());
        VERIFY(pc[0] == 'j' && pc[1] == 'k' && pc[2] == '\0');

        pc[2] = '#';
        in >> pc;
        VERIFY(in.eof());
        VERIFY(pc[0] == 'l' && pc[1] == '\0' && pc[2] == '#');
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_character_char_7()
{
    dump_info("Test istream<char> operator>> (character) case 7...");
    auto helper = []<template<typename, typename> class T>()
    {
        char buf[5] = {'x', 'x', 'x', 'x', 'x'};
        std::string s("  four");
    
        T in(IOv2::mem_device{s});
        in >> buf;
        VERIFY(in.eof());
        VERIFY(buf[4] == '\0');
        VERIFY(std::string(buf) == "four");

        in.clear();
        in.attach(IOv2::mem_device{s});
        for (int i = 0; i < 5; ++i)
            buf[i] = 'x';
        in.width(5);
        in >> buf;
        VERIFY(in.eof());
        VERIFY(std::string(buf) == "four");
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}
