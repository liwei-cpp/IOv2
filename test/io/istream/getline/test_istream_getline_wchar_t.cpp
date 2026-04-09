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
#include <common/dump_info.h>
#include <common/file_guard.h>
#include <common/verify.h>

void test_istream_getline_wchar_t_1()
{
    dump_info("Test istream<wchar_t>::getline case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const wchar_t str_lit01[] = L"\t\t\t    sun*ra \n"
            L"                            "
            L"and his myth science arkestra present\n"
            L"                            "
            L"angles and demons @ play\n"
            L"                            "
            L"the nubians of plutonia";

        std::wstring str01(str_lit01);

        T is_00(IOv2::mem_device{L""});
        T is_04(IOv2::mem_device{str01});

        IOv2::ios_defs::iostate state1, state2, statefail, stateeof;
        statefail = IOv2::ios_defs::strfailbit;
        stateeof = IOv2::ios_defs::eofbit;
        wchar_t carray1[400] = L"";

        // istream& getline(char* s, streamsize n, char delim)
        // istream& getline(char* s, streamsize n)
        state1 = is_00.rdstate();
        is_00.template get<IOv2::cons_sep, IOv2::app_zt>(carray1, 20, L'*');
        state2 = is_00.rdstate();
        // make sure failbit was set, since we couldn't extract
        // from the null streambuf...
        VERIFY(state1 != state2);
        VERIFY(state2 & statefail);

        state1 = is_04.rdstate();
        size_t gcount = is_04.template get<IOv2::cons_sep, IOv2::app_zt>(carray1, 1, L'\t') - carray1; // extracts, throws away
        state2 = is_04.rdstate();
        VERIFY(gcount == 1);
        VERIFY(state1 == state2);
        VERIFY(state1 == 0);
        VERIFY(std::wstring(L"") == std::wstring(carray1));

        state1 = is_04.rdstate();
        gcount = is_04.template get<IOv2::cons_sep, IOv2::app_zt>(carray1, 20, L'*') - carray1;
        state2 = is_04.rdstate();  
        VERIFY(gcount == 10);
        VERIFY(state1 == state2);
        VERIFY(state1 == 0);
        VERIFY(std::wstring(L"\t\t    sun") == std::wstring(carray1));

        state1 = is_04.rdstate();
        gcount = is_04.template get<IOv2::cons_sep, IOv2::app_zt>(carray1, 20) - carray1;
        state2 = is_04.rdstate();
        VERIFY(gcount == 4);
        VERIFY(state1 == state2);
        VERIFY(state1 == 0);
        VERIFY(std::wstring(L"ra ") == std::wstring(carray1));

        state1 = is_04.rdstate();
        gcount = is_04.template get<IOv2::cons_sep, IOv2::app_zt>(carray1, 65) - carray1;
        state2 = is_04.rdstate();
        VERIFY(gcount == 65);
        VERIFY(state1 != state2);
        VERIFY(state2 == statefail);
        VERIFY(std::wstring(L"                            and his myth science arkestra presen")
               == std::wstring(carray1));

        is_04.clear();
        state1 = is_04.rdstate();
        gcount = is_04.template get<IOv2::cons_sep, IOv2::app_zt>(carray1, 120, L'|') - carray1;
        state2 = is_04.rdstate();
        VERIFY(gcount == 107);
        VERIFY(state1 != state2);
        VERIFY(state2 == stateeof);

        is_04.clear();
        state1 = is_04.rdstate();
        gcount = is_04.template get<IOv2::cons_sep, IOv2::app_zt>(carray1, 100, L'|') - carray1;
        state2 = is_04.rdstate();
        VERIFY(gcount == 1);
        VERIFY(state1 != state2);
        VERIFY(state2 & statefail);
        VERIFY(state2 & stateeof);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_getline_wchar_t_2()
{
    dump_info("Test istream<wchar_t>::getline case 2...");

    auto helper = []<template<typename, typename> class T>()
    {
        const wchar_t* charray = L"\n"
            L"a\n"
            L"aa\n"
            L"aaa\n"
            L"aaaa\n"
            L"aaaaa\n"
            L"aaaaaa\n"
            L"aaaaaaa\n"
            L"aaaaaaaa\n"
            L"aaaaaaaaa\n"
            L"aaaaaaaaaa\n"
            L"aaaaaaaaaaa\n"
            L"aaaaaaaaaaaa\n"
            L"aaaaaaaaaaaaa\n"
            L"aaaaaaaaaaaaaa\n";

        const std::streamsize it = 5;
        std::size_t blen = std::wcslen(charray);
        std::size_t br = 0;

        wchar_t tmp[it];
        T ifs(IOv2::mem_device{charray});
        VERIFY((bool)ifs);

        while (true)
        {
            size_t gcount = ifs.template get<IOv2::cons_sep, IOv2::app_zt>(tmp, it) - tmp;
            br += gcount - 1;
            if (ifs.eof())
            {
                // Just sanity checks to make sure we've extracted the same
                // number of chars that were in the streambuf
                VERIFY(br + 15 == blen);
                break;
            }
            else if (ifs.str_fail())
            {
                // delimiter not read
                //
                // either
                // -> extracted no characters
                // or
                // -> n - 1 characters are stored
                ifs.clear(ifs.rdstate() & ~IOv2::ios_defs::strfailbit);
                VERIFY((gcount == 0) || (std::wcslen(tmp) == it - 1));
                VERIFY((bool)ifs);
            }
            else
            {
                // delimiter was read.
                //
                // -> strlen(__s) < n - 1 
                // -> delimiter was seen -> gcount() > strlen(__s)
                VERIFY(gcount == static_cast<size_t>(std::wcslen(tmp) + 1));
            }
        }

        ifs.clear(ifs.rdstate() & ~IOv2::ios_defs::eofbit);
        auto gcount = ifs.template get<IOv2::cons_sep, IOv2::app_zt>(tmp, it) - tmp;
        VERIFY((ifs.str_fail()) && (gcount == 1));
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_getline_wchar_t_3()
{
    dump_info("Test istream<wchar_t>::getline case 3...");

    auto helper = []<template<typename, typename> class T>()
    {
        const size_t it = 5;
        wchar_t tmp[it];
        const wchar_t* str_lit = L"abcd\n";

        T istr(IOv2::mem_device{str_lit});
        size_t gcount = istr.template get<IOv2::cons_sep, IOv2::app_zt>(tmp, it) - tmp;
        VERIFY(gcount == 5);
        VERIFY(std::wcslen(tmp) == 4);
        VERIFY(!istr.str_fail());
        VERIFY(istr.eof());

        istr.clear(istr.rdstate() & ~IOv2::ios_defs::eofbit);
        wchar_t c = L'z';
        istr.get(c);
        VERIFY(c == L'z');
        VERIFY(istr.eof());
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_getline_wchar_t_4()
{
    dump_info("Test istream<wchar_t>::getline case 4...");
    auto helper = []<template<typename, typename> class T>()
    {
        T is(IOv2::mem_device{L"1234567890abcdefghij"});
        typedef std::char_traits<wchar_t>   traits_type;

        wchar_t buffer[10];
        std::fill_n(buffer, 10, L'X');

        size_t gcount = is.template get<IOv2::cons_sep, IOv2::app_zt>(buffer, sizeof(buffer) / sizeof(wchar_t), L'0') - buffer;
        VERIFY(gcount == 10);
        VERIFY(is.rdstate() == IOv2::ios_defs::goodbit);
        VERIFY(traits_type::compare(buffer, L"123456789\0", sizeof(buffer) / sizeof(wchar_t)) == 0);

        is.clear();
        std::fill_n(buffer, 10, L'X');
        gcount = is.template get<IOv2::cons_sep, IOv2::app_zt>(buffer, sizeof(buffer) / sizeof(wchar_t)) - buffer;
        VERIFY(gcount == 10);
        VERIFY(is.rdstate() == IOv2::ios_defs::strfailbit);
        VERIFY(traits_type::compare(buffer, L"abcdefghi\0", sizeof(buffer) / sizeof(wchar_t)) == 0);

        is.clear();
        std::fill_n(buffer, 10, L'X');
        gcount = is.template get<IOv2::cons_sep, IOv2::app_zt>(buffer, sizeof(buffer) / sizeof(wchar_t)) - buffer;
        VERIFY(gcount == 2);
        VERIFY(is.rdstate() == IOv2::ios_defs::eofbit);
        VERIFY(traits_type::compare(buffer, L"j\0XXXXXXXX", sizeof(buffer) / sizeof(wchar_t)) == 0);

        is.clear();
        std::fill_n(buffer, 10, L'X');
        gcount = is.template get<IOv2::cons_sep, IOv2::app_zt>(buffer, sizeof(buffer) / sizeof(wchar_t)) - buffer;
        VERIFY(gcount == 1);
        VERIFY(is.rdstate() == (IOv2::ios_defs::eofbit | IOv2::ios_defs::strfailbit));
        VERIFY(traits_type::compare(buffer, L"\0XXXXXXXXX", sizeof(buffer) / sizeof(wchar_t)) == 0);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_getline_wchar_t_5()
{
    dump_info("Test istream<wchar_t>::getline case 5...");
    auto prepare = [](std::string::size_type len, unsigned nchunks, char delim)
    {
        std::string ret;
        std::wstring wret;
        for (unsigned i = 0; i < nchunks; ++i)
        {
            for (std::string::size_type j = 0; j < len; ++j)
            {
                char c = 'a' + rand() % 26;
                ret.push_back(c);
                wret.push_back(c);
            }
            len *= 2;
            ret.push_back(delim);
            wret.push_back(delim);
        }
        return std::pair{ret, wret};
    };

    auto check = [](auto& stream, const std::wstring& str, unsigned nchunks, char delim)
    {
        wchar_t buf[1000000];
        std::wstring::size_type index = 0, index_new = 0;
        unsigned n = 0;

        size_t gcount = 0;
        while (true)
        {
            gcount = stream.template get<IOv2::cons_sep, IOv2::app_zt>(buf, sizeof(buf), delim) - buf;
            if (!stream) break;

            index_new = str.find(delim, index);
            VERIFY( gcount == (size_t)(index_new - index) + 1);
            VERIFY( !str.compare(index, index_new - index, buf) );
            index = index_new + 1;
            ++n;
        }
        VERIFY( gcount == 1 );
        VERIFY( stream.eof() );
        VERIFY( n == nchunks );
    };
    
    auto helper = [&prepare, &check]<template<typename, typename> class T>()
    {
        const char filename[] = "istream_getline.txt";
        const char delim = '|';
        const unsigned nchunks = 10;

        auto [data, wdata] = prepare.operator()(777, nchunks, delim);
        file_guard g(filename, data);

        T ifstrm(IOv2::ifile_device<char>{filename},
                 IOv2::code_cvt_creator<char, wchar_t>("C"));
        check(ifstrm, wdata, nchunks, delim);
        ifstrm.detach().close();
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_getline_wchar_t_6()
{
    dump_info("Test istream<wchar_t>::getline case 6...");

    auto helper = []<template<typename, typename> class T>()
    {
        T istr01(IOv2::mem_device{L""});
        T istr02(IOv2::mem_device{L""});
        wchar_t buf02[2] = L"*" ;

        istr01.peek();
        VERIFY( istr01.eof() );
    
        VERIFY( istr01.template get<IOv2::cons_sep, IOv2::app_zt>(buf02, 0) == buf02 );
        VERIFY( !istr01.str_fail() );

        istr02.peek();
        VERIFY( istr02.eof() );
        VERIFY( istr02.template get<IOv2::cons_sep, IOv2::app_zt>(buf02, 1) == buf02 + 1);
        VERIFY( istr02.str_fail() );
        VERIFY( buf02[0] == wchar_t{} );  
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}