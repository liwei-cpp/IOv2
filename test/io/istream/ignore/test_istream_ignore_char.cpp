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

void test_istream_ignore_char_1()
{
    dump_info("Test istream<char>::ignore case 1...");
    auto helper = []<template<typename, typename> class T>()
    {
        T is_00(IOv2::mem_device{""});
        T is_03(IOv2::mem_device{"soul eyes: john coltrane quartet"});
        T is_04(IOv2::mem_device{"soul eyes: john coltrane quartet"});

        IOv2::ios_defs::iostate state1, state2;

        // istream& read(char_type* s, streamsize n)
        char carray[60] = "";
        is_04.read(carray, 9);
        VERIFY( is_04.peek() == ':' );

        // istream& ignore(streamsize n = 1, int_type delim = traits::eof())
        state1 = is_04.rdstate();
        is_04.ignore();
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( is_04.peek() == ' ' );

        state1 = is_04.rdstate();
        is_04.ignore(0);
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( is_04.peek() == ' ' );

        state1 = is_04.rdstate();
        is_04.ignore(5, ' ');
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( is_04.peek() == 'j' );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_ignore_char_2()
{
    dump_info("Test istream<char>::ignore case 2...");
    auto prepare = [](std::string::size_type len, unsigned nchunks, char delim)
    {
        std::string ret;
        for (unsigned i = 0; i < nchunks; ++i)
        {
            for (std::string::size_type j = 0; j < len; ++j)
                ret.push_back('a' + rand() % 26);
            len *= 2;
            ret.push_back(delim);
        }
        return ret;
    };
    
    auto check = [](auto& stream, const std::string& str, unsigned nchunks, char delim)
    {
        std::string::size_type index = 0, index_new = 0;
        unsigned n = 0;

        while (true)
        {
            stream.ignore(std::numeric_limits<std::streamsize>::max(), delim);
            index_new = str.find(delim, index);
            index = index_new + 1;
            ++n;
            if (!stream.good()) break;
        }
        VERIFY( !stream.str_fail() );
        VERIFY( stream.eof() );
        VERIFY( n == nchunks );
    };

    auto helper = [&prepare, &check]<template<typename, typename> class T>()
    {
        const char filename[] = "istream_ignore.txt";
        const char delim = '|';
        const unsigned nchunks = 10;
        const std::string data = prepare(555, nchunks, delim);
        file_guard g(filename, data);
    
        T ifstrm(IOv2::ifile_device<char>{filename});
        check(ifstrm, data, nchunks, delim);
        ifstrm.detach().close();
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_ignore_char_3()
{
    dump_info("Test istream<char>::ignore case 3...");
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
        T ifstrm(IOv2::ifile_device<char>{"istream_unformatted-1.txt"});
        IOv2::ios_defs::iostate state1, state2;

        state1 = ifstrm.rdstate();
        VERIFY( state1 == IOv2::ios_defs::goodbit );
        VERIFY( ifstrm.peek() == '1' );
        state2 = ifstrm.rdstate();
        VERIFY( state1 == state2 );

        state1 = ifstrm.rdstate();
        ifstrm.ignore(1);
        state2 = ifstrm.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( ifstrm.peek() == '2' );

        state1 = ifstrm.rdstate();
        ifstrm.ignore(10);
        state2 = ifstrm.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( ifstrm.peek() == '1' );

        state1 = ifstrm.rdstate();
        ifstrm.ignore(100);
        state2 = ifstrm.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( ifstrm.peek() == '2' );

        state1 = ifstrm.rdstate();
        ifstrm.ignore(1000);
        state2 = ifstrm.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( ifstrm.peek() == '1' );

        state1 = ifstrm.rdstate();
        ifstrm.ignore(10000);
        state2 = ifstrm.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( ifstrm.peek() == '2' );

        state1 = ifstrm.rdstate();
        ifstrm.ignore(std::numeric_limits<std::streamsize>::max());
        state2 = ifstrm.rdstate();
        VERIFY( state1 != state2 );
        VERIFY( state2 == IOv2::ios_defs::eofbit );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_ignore_char_4()
{
    dump_info("Test istream<char>::ignore case 4...");

    auto helper = []<template<typename, typename> class T>()
    {
        T ss(IOv2::mem_device{"abcd" "\xFF" "1234ina donna coolbrith"});
        char c;
        ss >> c;
        VERIFY( c == 'a' );
        ss.ignore(8);
        ss >> c;
        VERIFY( c == 'i' );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_ignore_char_5()
{
    dump_info("Test istream<char>::ignore case 5...");

    auto helper = []<template<typename, typename> class T>()
    {
        T istr(IOv2::mem_device{"abcdefg\n"});

        istr.ignore(0);
        istr.ignore(0, 'b');

        istr.ignore();  // Advance to next position.
        istr.ignore(0, 'b');

        VERIFY(istr.peek() == 'b');
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_ignore_char_6()
{
    dump_info("Test istream<char>::ignore case 6...");

    auto helper = []<template<typename, typename> class T>()
    {
        {
            T s{IOv2::mem_device{" +   -"}};
            s.ignore(1, '+');
            VERIFY( s.get() == '+' );
            s.ignore(3, '-');
            VERIFY( s.get() == '-' );
        }

        {
            T s{IOv2::mem_device{".+...-"}};
            s.ignore(1, '+');
            VERIFY( s.get() == '+' );
            s.ignore(3, '-');
            VERIFY( s.get() == '-' );
        }
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_ignore_char_7()
{
    dump_info("Test istream<char>::ignore case 7...");

    auto helper = []<template<typename, typename> class T>()
    {
        {
            T s(IOv2::mem_device{"  "});
            s.ignore(2, '+');
            VERIFY( (bool)s );
            VERIFY( !s.get().has_value() );
            VERIFY( s.eof() );
        }
    
        {
            T s(IOv2::mem_device{"  "});
            s.ignore(2);
            VERIFY( (bool)s );
            VERIFY( !s.get().has_value() );
            VERIFY( s.eof() );
        }
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}
