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

void test_istream_putback_char_1()
{
    dump_info("Test istream<char>::putback case 1...");
    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str_02("soul eyes: john coltrane quartet");
        T is_00(IOv2::mem_device{str_02});
        T is_03(IOv2::mem_device{str_02});
        T is_04(IOv2::mem_device{str_02});

        IOv2::ios_defs::iostate state1, state2;

        // istream& putback(char c)
        is_04.ignore(30);
        is_04.clear();
        state1 = is_04.rdstate();
        is_04.putback('t');
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( is_04.peek() == 't' );

        is_04.clear();
        state1 = is_04.rdstate();
        is_04.putback('r');
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( is_04.peek() == 'r' );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// putback() on a stream that is already in a failed state: the input sentry rejects the
// invalid stream (throws stream_error), which putback's own try/catch routes through
// handle_exception (-> strfailbit). With no exception mask set nothing escapes. This drives
// the catch branch of istream_operators::putback().
void test_istream_putback_char_2()
{
    dump_info("Test istream<char>::putback case 2 (failed stream)...");

    auto helper = []<template<typename, typename> class T>()
    {
        T is{IOv2::mem_device{std::string("abc")}, IOv2::locale<char>("C")};

        int v = 0;
        is >> v;                      // non-numeric input -> strfailbit
        VERIFY( !is );

        bool threw = false;
        try { is.putback('z'); }      // sentry rejects the failed stream -> caught
        catch (...) { threw = true; }
        VERIFY( !threw );
        VERIFY( is.rdstate() & IOv2::ios_defs::strfailbit );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}
