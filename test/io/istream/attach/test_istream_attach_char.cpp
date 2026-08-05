#include <string>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/file_guard.h>
#include <support/verify.h>

// attach() resets the stream state before installing the device. The bits that were set
// describe what happened on the OLD device -- input read to the end, a parse that failed, a
// previous attach that installed an unusable device -- and none of that survives replacing it.
void test_istream_attach_state_char_1()
{
    dump_info("Test istream<char> attach clears state case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T is{IOv2::mem_device{std::string("ab")}};

        std::string tok;
        is >> tok;                                  // reads to the end -> eofbit
        VERIFY( tok == "ab" );
        VERIFY( is.eof() );

        is.attach(IOv2::mem_device{std::string("cd")});
        VERIFY( is.rdstate() == IOv2::ios_defs::goodbit );   // not just eofbit: everything

        std::string tok2;
        is >> tok2;
        VERIFY( tok2 == "cd" );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    // A parse failure is cleared too, not only eofbit.
    {
        IOv2::istream is{IOv2::mem_device{std::string("zz")}};
        int v = 0;
        is >> v;
        VERIFY( is.str_fail() );

        is.attach(IOv2::mem_device{std::string("42")});
        VERIFY( is.rdstate() == IOv2::ios_defs::goodbit );

        is >> v;
        VERIFY( !is.str_fail() && v == 42 );
    }

    dump_info("Done\n");
}

// A device that cannot be initialized is reported through the stream's error model rather than
// by throwing -- attach() is an ordinary member function, unlike a constructor, whose
// member-initializer exception C++ requires to propagate.
void test_istream_attach_failure_char_1()
{
    dump_info("Test istream<char> attach failure case 1...");

    const std::string path = "test_istream_attach_failure_char_1.txt";
    file_guard guard(path, std::string("zz"));

    IOv2::istream<IOv2::file_device<char>, char> is{IOv2::file_device<char>(path)};

    int v = 0;
    is >> v;                                        // "zz" is not a number -> strfailbit
    VERIFY( is.str_fail() );

    // A default-constructed file_device refers to no open file, so initializing the converter
    // over it throws. attach() must report that as state, not propagate it.
    bool threw = false;
    try { is.attach(IOv2::file_device<char>()); }
    catch (...) { threw = true; }
    VERIFY( !threw );
    VERIFY( is.dev_fail() );

    // And the state describes THIS attach only: the earlier strfailbit came from a device that
    // no longer exists. streambuf::attach() installs the new device first and initializes the
    // converter second, and it is the second step that throws, so clearing has to happen before
    // the replacement -- a clear placed after it would not run on this path at all.
    VERIFY( !is.str_fail() );
    VERIFY( is.rdstate() == IOv2::ios_defs::devfailbit );

    // Recovery: installing a working device revives the stream.
    is.attach(IOv2::file_device<char>(path));
    VERIFY( is.rdstate() == IOv2::ios_defs::goodbit );
    std::string tok;
    is >> tok;
    VERIFY( tok == "zz" );
    is.detach();

    dump_info("Done\n");
}
