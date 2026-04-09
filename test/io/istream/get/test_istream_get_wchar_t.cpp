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

void test_istream_get_wchar_t_1()
{
    dump_info("Test istream<wchar_t>::get case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const wchar_t str_lit01[] = 
        L"   sun*ra \n\t\t\t   & his arkestra, featuring john gilmore: \n"
        L"                         "
            L"jazz in silhouette: images and forecasts of tomorrow";

        std::wstring str01(str_lit01);

        T is_00(IOv2::mem_device{L""});
        T is_04(IOv2::mem_device{str01});

        IOv2::ios_defs::iostate statefail, stateeof;
        statefail = IOv2::ios_defs::strfailbit;
        stateeof = IOv2::ios_defs::eofbit;
        wchar_t carray1[400] = L"";

        // int_type get()
        // istream& get(char*, streamsize, char delim)
        // istream& get(char*, streamsize)
        size_t gcount = is_00.template get<IOv2::keep_sep, IOv2::no_zt>(carray1, 1) - carray1;
        VERIFY(is_00.rdstate() & statefail);
        VERIFY(gcount == 0);

        auto next_pos = is_04.template get<IOv2::keep_sep, IOv2::no_zt>(carray1, 3); *next_pos = 0;
        gcount = next_pos - carray1;
        VERIFY((is_04.rdstate() & statefail) == 0);
        VERIFY(std::wstring(L"   ") == std::wstring(carray1));
        VERIFY(gcount == 3);

        is_04.clear();
        next_pos = is_04.template get<IOv2::keep_sep, IOv2::no_zt>(carray1 + 3, 199); *next_pos = 0;
        gcount = next_pos - carray1 - 3;
        VERIFY(gcount == 7);
        VERIFY((is_04.rdstate() & statefail) == 0);
        VERIFY((is_04.rdstate() & stateeof) == 0);
        VERIFY(str01.substr(0, 10) == std::wstring(carray1));

        is_04.clear();
        next_pos = is_04.template get<IOv2::keep_sep, IOv2::no_zt>(carray1, 199); *next_pos = 0;
        gcount = next_pos - carray1;
        VERIFY(gcount == 0);
        VERIFY((is_04.rdstate() & stateeof) == 0);
        VERIFY(is_04.rdstate() & statefail);
        is_04.clear();
        next_pos = is_04.template get<IOv2::keep_sep, IOv2::no_zt>(carray1, 199, '['); *next_pos = 0;
        gcount = next_pos - carray1;
        VERIFY(gcount == 125);
        VERIFY(is_04.rdstate() & stateeof);
        VERIFY((is_04.rdstate() & statefail) == 0);
        is_04.clear();  
        next_pos = is_04.template get<IOv2::keep_sep, IOv2::no_zt>(carray1, 199); *next_pos = 0;
        gcount = next_pos - carray1;
        VERIFY(gcount == 0);
        VERIFY(is_04.rdstate() & stateeof);
        VERIFY(is_04.rdstate() & statefail);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_get_wchar_t_2()
{
    dump_info("Test istream<wchar_t>::get case 2...");

    auto helper = []<template<typename, typename> class T>()
    {
        std::string data = []()
        {
            std::string res;
            for (size_t i = 0; i < 1500; ++i)
                res.append("1234567890\n");
            return res;
        }();
    
        file_guard g("istream_unformatted-1.txt", data);
        T infile(IOv2::ifile_device<char>{"istream_unformatted-1.txt"},
                 IOv2::code_cvt_creator<char, wchar_t>("C"));
        VERIFY((bool)infile);
        while (infile)
        {
            wchar_t line[1024];
            while(infile.peek() == L'\n')
                infile.get();
            *(infile.template get<IOv2::keep_sep, IOv2::no_zt>(line, 1023)) = 0;
            VERIFY((std::wstring(line) == L"1234567890") ||
                   (std::wstring(line) == L""));
        }
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}