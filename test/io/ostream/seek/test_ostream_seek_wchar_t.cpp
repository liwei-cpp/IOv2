#include <limits>
#include <stdexcept>
#include <string>
#include <cvt/code_cvt.h>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/file_guard.h>
#include <support/verify.h>

void test_ostream_seek_wchar_t_1()
{
    dump_info("Test ostream<wchar_t>::seek case 1...");

    // trunc is spelled out so that both devices create the file: ofile_device opens "w" either
    // way, but file_device without it opens "r+" and would fail on a file that does not exist.
    auto helper = []<template <typename, typename> class T,
                                typename TDevice>()
    {
        file_guard g1("istream_seeks-3.txt");
        T ofstrm{TDevice{"istream_seeks-3.txt", IOv2::file_open_flag::trunc},
                 IOv2::code_cvt_creator<char, wchar_t>("C")};
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

                stream.seek(begin.value());
            }
            VERIFY(stream.good());
        }(ofstrm);
        auto [odev, oerr] = ofstrm.detach();
        odev.close();

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
            VERIFY( i == times );
            VERIFY( stream.rdstate() == IOv2::ios_defs::eofbit );
        }(ifstrm);
        auto [idev, ierr] = ifstrm.detach();
        idev.close();
    };

    helper.template operator()<IOv2::ostream, IOv2::ofile_device<char>>();
    helper.template operator()<IOv2::iostream, IOv2::file_device<char>>();

    dump_info("Done\n");
}

void test_ostream_seek_wchar_t_2()
{
    dump_info("Test ostream<wchar_t>::seek case 2...");

    IOv2::iostream stream{IOv2::mem_device{L""}} ;
    VERIFY((bool)stream);
        
    constexpr int times = 10;
    const wchar_t* s = L" lootpack, peanut butter wolf, rob swift, madlib, quasimoto";

    // write_rewind
    [&s, &stream]()
    {
        for (int j = 0; j < times; j++) 
        {
            auto begin = stream.tell();

            for (int i = 0; i < times; ++i)
                stream << j << '-' << i << s << '\n';

            stream.seek(begin.value());
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
        VERIFY( i == times );
        VERIFY( stream.rdstate() == IOv2::ios_defs::eofbit );
    }();

    dump_info("Done\n");
}