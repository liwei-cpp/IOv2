#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/file_guard.h>
#include <common/verify.h>

void test_ostream_seek_char_1()
{
    dump_info("Test ostream<char>::seek case 1...");

    auto helper = []<template <typename, typename> class T>()
    {
        file_guard g1("istream_seeks-3.txt");
        T ofstrm{IOv2::ofile_device<char>{"istream_seeks-3.txt"}};
        VERIFY((bool)ofstrm);
        
        constexpr int times = 10;
        const char* s = " lootpack, peanut butter wolf, rob swift, madlib, quasimoto";

        // write_rewind
        [&s](auto& stream)
        {
            for (int j = 0; j < times; j++) 
            {
                auto begin = stream.tell();

                for (int i = 0; i < times; ++i)
                    stream << j << '-' << i << s << '\n';

                stream.seek(begin);
            }
            VERIFY(stream.good());
        }(ofstrm);
        ofstrm.detach().close();

        IOv2::istream ifstrm{IOv2::ifile_device<char>{"istream_seeks-3.txt"}};

        //check_contents
        [](auto& stream)
        {
            stream.clear();
            stream.seek(0);
            int i = 0;
            int loop = times * times + 2;
            while (i < loop)
            {
                stream.ignore(80, '\n');
                if (stream.good())
                    ++i;
                else
                    break;
            }
            VERIFY( i == times - 1 );
            VERIFY( stream.rdstate() == IOv2::ios_defs::eofbit );
        }(ifstrm);
        ifstrm.detach().close();
    };

    helper.template operator()<IOv2::ostream>();
    helper.template operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_seek_char_2()
{
    dump_info("Test ostream<char>::seek case 2...");

    IOv2::iostream stream{IOv2::mem_device{""}} ;
    VERIFY((bool)stream);
        
    constexpr int times = 10;
    const char* s = " lootpack, peanut butter wolf, rob swift, madlib, quasimoto";

    // write_rewind
    [&s, &stream]()
    {
        for (int j = 0; j < times; j++) 
        {
            auto begin = stream.tell();

            for (int i = 0; i < times; ++i)
                stream << j << '-' << i << s << '\n';

            stream.seek(begin);
        }
        VERIFY(stream.good());
    }();
    stream.seek(0);

    //check_contents
    [&stream]()
    {
        stream.clear();
        stream.seek(0);
        int i = 0;
        int loop = times * times + 2;
        while (i < loop)
        {
            stream.ignore(80, '\n');
            if (stream.good())
                ++i;
            else
                break;
        }
        VERIFY( i == times - 1 );
        VERIFY( stream.rdstate() == IOv2::ios_defs::eofbit );
    }();

    dump_info("Done\n");
}