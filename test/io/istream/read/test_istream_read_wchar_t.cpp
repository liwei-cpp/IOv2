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

void test_istream_read_wchar_t_1()
{
    dump_info("Test istream<wchar_t>::read case 1...");
    
    auto helper = []<template<typename, typename> class T>()
    {
        const std::wstring str_02(L"soul eyes: john coltrane quartet");
        T is_00{IOv2::mem_device{L""}};
        T is_03{IOv2::mem_device{str_02}};
        T is_04{IOv2::mem_device{str_02}};
        IOv2::ios_defs::iostate state1, state2, statefail, stateeof;
        statefail = IOv2::ios_defs::strfailbit;
        stateeof = IOv2::ios_defs::eofbit;

        // istream& read(char_type* s, streamsize n)
        wchar_t carray[60] = L"";
        state1 = is_04.rdstate();
        is_04.read(carray, 0);
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );

        state1 = is_04.rdstate();
        is_04.read(carray, 9);
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( !std::wcsncmp(carray, L"soul eyes", 9) );
        VERIFY( is_04.peek() == ':' );

        state1 = is_03.rdstate();
        is_03.read(carray, 60);
        state2 = is_03.rdstate();
        VERIFY( state1 != state2 );
        VERIFY( static_cast<bool>(state2 & stateeof) ); 
        VERIFY( static_cast<bool>(state2 & statefail) ); 
        VERIFY( !std::wcsncmp(carray, L"soul eyes: john coltrane quartet", 35) );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_read_wchar_t_2()
{
    dump_info("Test istream<wchar_t>::read case 2...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::wstring str_00(L"Red_Garland_Qunitet-Soul_Junction");
        std::vector<wchar_t> c_array(str_00.size() + 4);

        T is_00(IOv2::mem_device{str_00});
        IOv2::ios_defs::iostate state1, statefail, stateeof;
        statefail = IOv2::ios_defs::strfailbit;
        stateeof = IOv2::ios_defs::eofbit;

        state1 = stateeof | statefail;
        size_t gcount = is_00.read(c_array.data(), str_00.size() + 1) - c_array.data();
        VERIFY( gcount == str_00.size() );
        VERIFY( is_00.rdstate() == state1 );

        is_00.read(c_array.data(), str_00.size());
        VERIFY( is_00.rdstate() == state1 );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_read_wchar_t_3()
{
    dump_info("Test istream<wchar_t>::read case 3...");

    auto helper = []<template<typename, typename> class T>()
    {
        T iss(IOv2::mem_device{L"Juana Briones"});
        wchar_t tab[13];
        iss.read(tab, 13);
        if (!iss)
            VERIFY( false );
    };
    
    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}
