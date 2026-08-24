#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/file_guard.h>
#include <support/verify.h>

void test_istream_get_char_1()
{
    dump_info("Test istream<char>::get case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const char str_lit01[] = 
        "   sun*ra \n\t\t\t   & his arkestra, featuring john gilmore: \n"
        "                         "
            "jazz in silhouette: images and forecasts of tomorrow";

        std::string str01(str_lit01);

        T is_00(IOv2::mem_device{""});
        T is_04(IOv2::mem_device{str01});

        IOv2::ios_defs::iostate statefail, stateeof;
        statefail = IOv2::ios_defs::strfailbit;
        stateeof = IOv2::ios_defs::eofbit;
        char carray1[400] = "";

        // int_type get()
        // istream& get(char*, streamsize, char delim)
        // istream& get(char*, streamsize)
        size_t gcount = is_00.template get<IOv2::keep_sep, IOv2::no_zt>(carray1, 1) - carray1;
        VERIFY(is_00.rdstate() & statefail);
        VERIFY(gcount == 0);

        auto next_pos = is_04.template get<IOv2::keep_sep, IOv2::no_zt>(carray1, 3); *next_pos = 0;
        gcount = next_pos - carray1;
        VERIFY((is_04.rdstate() & statefail) == 0);
        VERIFY(std::string("   ") == std::string(carray1));
        VERIFY(gcount == 3);

        is_04.clear();
        next_pos = is_04.template get<IOv2::keep_sep, IOv2::no_zt>(carray1 + 3, 199); *next_pos = 0;
        gcount = next_pos - carray1 - 3;
        VERIFY(gcount == 7);
        VERIFY((is_04.rdstate() & statefail) == 0);
        VERIFY((is_04.rdstate() & stateeof) == 0);
        VERIFY(str01.substr(0, 10) == std::string(carray1));

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

void test_istream_get_char_2()
{
    dump_info("Test istream<char>::get case 2...");

    auto helper = []<template<typename, typename> class T,
                               typename TDevice>()
    {
        std::string data = []()
        {
            std::string res;
            for (size_t i = 0; i < 1500; ++i)
                res.append("1234567890\n");
            return res;
        }();

        file_guard g("istream_unformatted-1.txt", data);
        T infile(TDevice{"istream_unformatted-1.txt"});
        VERIFY((bool)infile);
        while (infile)
        {
            char line[1024];
            while(infile.peek() == '\n')
                infile.get();
            *(infile.template get<IOv2::keep_sep, IOv2::no_zt>(line, 1023)) = 0;
            VERIFY((std::string(line) == "1234567890") ||
                   (std::string(line) == ""));
        }
    };

    helper.operator()<IOv2::istream, IOv2::ifile_device<char>>();
    helper.operator()<IOv2::iostream, IOv2::file_device<char>>();

    dump_info("Done\n");
}
void test_istream_get_char_3()
{
    dump_info("Test istream<char>::get case 3 (EOF x exception mask)...");

    auto helper = []<template<typename, typename> class T>()
    {
        // eofbit masked: get()/get(char&) at EOF throw eof_error; eofbit set.
        {
            T s{IOv2::mem_device{std::string("")}, IOv2::locale<char>("C")};
            s.exceptions(IOv2::ios_defs::eofbit);
            bool threw = false;
            try { (void)s.get(); }
            catch (const IOv2::eof_error&) { threw = true; }
            VERIFY(threw);
            VERIFY(s.eof());
        }
        {
            T s{IOv2::mem_device{std::string("")}, IOv2::locale<char>("C")};
            s.exceptions(IOv2::ios_defs::eofbit);
            char c = 'Z';
            bool threw = false;
            try { s.get(c); }
            catch (const IOv2::eof_error&) { threw = true; }
            VERIFY(threw);
            VERIFY(s.eof());
        }
        // eofbit unmasked (default): no throw, eofbit set (regression).
        {
            T s{IOv2::mem_device{std::string("")}, IOv2::locale<char>("C")};
            auto c = s.get();
            VERIFY(!c.has_value());
            VERIFY(s.eof());
        }
        {
            T s{IOv2::mem_device{std::string("")}, IOv2::locale<char>("C")};
            char c = 'Z';
            s.get(c);
            VERIFY(c == 'Z');
            VERIFY(s.eof());
        }
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_get_char_4()
{
    dump_info("Test istream<char>::get case 4 (negative buffer size)...");

    auto helper = []<template<typename, typename> class T>()
    {
        // Same contract as read(): the capacity is a signed ptrdiff_t so that a negative
        // value is rejected here rather than arriving as SIZE_MAX and filling the caller's
        // buffer until the delimiter or EOF.
        for (const std::ptrdiff_t n : {std::ptrdiff_t{-1},
                                       std::numeric_limits<std::ptrdiff_t>::min()})
        {
            T s{IOv2::mem_device{std::string(4096, 'x')}, IOv2::locale<char>("C")};
            char buf[8];
            for (char& ch : buf) ch = '#';
            bool threw = false;
            char* ret = nullptr;
            try { ret = s.template get<IOv2::keep_sep, IOv2::app_zt>(buf, n, '\n'); }
            catch (...) { threw = true; }
            VERIFY( !threw );
            VERIFY( ret == buf );
            VERIFY( s.rdstate() & IOv2::ios_defs::strfailbit );
            // app_zt must not treat a negative capacity as room for the terminator.
            for (const char c : buf)
                VERIFY( c == '#' );
            s.clear();
            VERIFY( s.peek() == 'x' );
        }

        // The two-argument overload widens '\n' first, then forwards; it must reject the
        // negative capacity just the same, and leave the buffer alone on the way through.
        {
            T s{IOv2::mem_device{std::string(4096, 'x')}, IOv2::locale<char>("C")};
            char buf[8];
            for (char& ch : buf) ch = '#';
            bool threw = false;
            char* ret = nullptr;
            try { ret = s.template get<IOv2::cons_sep, IOv2::app_zt>(buf, -1); }
            catch (...) { threw = true; }
            VERIFY( !threw );
            VERIFY( ret == buf );
            VERIFY( s.rdstate() & IOv2::ios_defs::strfailbit );
            for (const char c : buf)
                VERIFY( c == '#' );
        }

        // Zero stays a separate, already-documented rejection: it must keep behaving as
        // before rather than being folded into the negative case.
        {
            T s{IOv2::mem_device{std::string("abc")}, IOv2::locale<char>("C")};
            char buf[8];
            for (char& ch : buf) ch = '#';
            s.template get<IOv2::keep_sep, IOv2::app_zt>(buf, 0, '\n');
            VERIFY( s.rdstate() & IOv2::ios_defs::strfailbit );
            for (const char c : buf)
                VERIFY( c == '#' );
        }
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}
