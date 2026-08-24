#include <chrono>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <device/std_device.h>
#include <support/file_guard.h>
#include <support/stdio_guard.h>
#include <support/verify.h>

void test_istream_read_char_1()
{
    dump_info("Test istream<char>::read case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str_02("soul eyes: john coltrane quartet");
        T is_00{IOv2::mem_device{""}};
        T is_03{IOv2::mem_device{str_02}};
        T is_04{IOv2::mem_device{str_02}};
        IOv2::ios_defs::iostate state1, state2, statefail, stateeof;
        statefail = IOv2::ios_defs::strfailbit;
        stateeof = IOv2::ios_defs::eofbit;

        // istream& read(char_type* s, streamsize n)
        char carray[60] = "";
        state1 = is_04.rdstate();
        is_04.read(carray, 0);
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );

        state1 = is_04.rdstate();
        is_04.read(carray, 9);
        state2 = is_04.rdstate();
        VERIFY( state1 == state2 );
        VERIFY( !std::strncmp(carray, "soul eyes", 9) );
        VERIFY( is_04.peek() == ':' );

        state1 = is_03.rdstate();
        is_03.read(carray, 60);
        state2 = is_03.rdstate();
        VERIFY( state1 != state2 );
        VERIFY( static_cast<bool>(state2 & stateeof) ); 
        VERIFY( static_cast<bool>(state2 & statefail) ); 
        VERIFY( !std::strncmp(carray, "soul eyes: john coltrane quartet", 35) );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_read_char_2()
{
    dump_info("Test istream<char>::read case 2...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str_00("Red_Garland_Qunitet-Soul_Junction");
        std::vector<char> c_array(str_00.size() + 4);

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

void test_istream_read_char_3()
{
    dump_info("Test istream<char>::read case 3...");

    auto helper = []<template<typename, typename> class T>()
    {
        T iss(IOv2::mem_device{"Juana Briones"});
        char tab[13];
        iss.read(tab, 13);
        if (!iss)
            VERIFY( false );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_read_char_4()
{
    dump_info("Test istream<char>::read case 4 (short read at EOF x exception mask)...");

    auto helper = []<template<typename, typename> class T>()
    {
        // eofbit masked: short read throws stream_error, which unwinds through the in_sentry
        // destructor. The destructor must not throw during unwinding (no std::terminate);
        // eofbit gets set, and the masked eofbit then surfaces as a normal exception from
        // read()'s own handler. Reaching the assertions proves there was no terminate.
        {
            T s{IOv2::mem_device{std::string("ab")}, IOv2::locale<char>("C")};
            s.exceptions(IOv2::ios_defs::eofbit);
            char buf[8] = {};
            bool caught = false;
            try { s.read(buf, 5); }
            catch (...) { caught = true; }
            VERIFY(caught);
            VERIFY(s.rdstate() & IOv2::ios_defs::strfailbit);
            VERIFY(s.rdstate() & IOv2::ios_defs::eofbit);
        }
        // no mask: short read sets strfailbit + eofbit and returns without throwing.
        {
            T s{IOv2::mem_device{std::string("ab")}, IOv2::locale<char>("C")};
            char buf[8] = {};
            bool threw = false;
            try { s.read(buf, 5); }
            catch (...) { threw = true; }
            VERIFY(!threw);
            VERIFY(s.rdstate() & IOv2::ios_defs::strfailbit);
            VERIFY(s.rdstate() & IOv2::ios_defs::eofbit);
        }
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_read_char_5()
{
    dump_info("Test istream<char>::read case 5 (no wait past the request)...");

    // A pipe holding "42\n" with its write end still open: the data is all there, but the
    // stream has not ended. read(buf, 3) consumes exactly the buffered characters, so
    // nothing may go on to ask the device whether more is coming -- that question can only
    // be answered by waiting for the writer, and no caller asked it.
    //
    // The write end is closed after a delay rather than left open, so a regression waits
    // for that instead of hanging: the elapsed-time check then fails cleanly.
    pipe_iguard g("42\n");
    std::thread closer([&g]{ std::this_thread::sleep_for(std::chrono::seconds(3)); g.close_write(); });

    IOv2::istream is{IOv2::std_device<STDIN_FILENO>{}};
    char buf[3] = {};
    const auto t0 = std::chrono::steady_clock::now();
    is.read(buf, 3);
    const auto elapsed = std::chrono::steady_clock::now() - t0;
    closer.join();

    VERIFY(std::string(buf, 3) == "42\n");
    VERIFY(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() < 1000);

    dump_info("Done\n");
}

void test_istream_read_char_6()
{
    dump_info("Test istream<char>::read case 6 (null destination buffer)...");

    auto helper = []<template<typename, typename> class T>()
    {
        // read into a null buffer with a non-zero count: the sentry succeeds, then read
        // rejects the null pointer with stream_error -> strfailbit. With no exception mask
        // set it does not throw, and the returned pointer is s + gcount == nullptr.
        T s{IOv2::mem_device{std::string("abc")}, IOv2::locale<char>("C")};
        bool threw = false;
        char* ret = nullptr;
        try { ret = s.read(nullptr, 5); }
        catch (...) { threw = true; }
        VERIFY( !threw );
        VERIFY( ret == nullptr );
        VERIFY( s.rdstate() & IOv2::ios_defs::strfailbit );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_read_char_7()
{
    dump_info("Test istream<char>::read case 7 (negative character count)...");

    auto helper = []<template<typename, typename> class T>()
    {
        // The count is a signed ptrdiff_t precisely so that a negative value survives the
        // call instead of arriving as SIZE_MAX. read() rejects it with stream_error ->
        // strfailbit, and -- the point of the test -- writes nothing into the caller's
        // buffer, which a SIZE_MAX count would have overrun.
        for (const std::ptrdiff_t n : {std::ptrdiff_t{-1},
                                       std::numeric_limits<std::ptrdiff_t>::min()})
        {
            T s{IOv2::mem_device{std::string(4096, 'x')}, IOv2::locale<char>("C")};
            char buf[8];
            for (char& ch : buf) ch = '#';
            bool threw = false;
            char* ret = nullptr;
            try { ret = s.read(buf, n); }
            catch (...) { threw = true; }
            VERIFY( !threw );
            VERIFY( ret == buf );
            VERIFY( s.rdstate() & IOv2::ios_defs::strfailbit );
            for (const char c : buf)
                VERIFY( c == '#' );
            // The rejection happens before anything is extracted.
            s.clear();
            VERIFY( s.peek() == 'x' );
        }

        // A masked strfailbit turns the same rejection into an exception.
        {
            T s{IOv2::mem_device{std::string("abc")}, IOv2::locale<char>("C")};
            s.exceptions(IOv2::ios_defs::strfailbit);
            char buf[8];
            for (char& ch : buf) ch = '#';
            bool threw = false;
            try { s.read(buf, -1); }
            catch (...) { threw = true; }
            VERIFY( threw );
            VERIFY( s.rdstate() & IOv2::ios_defs::strfailbit );
            for (const char c : buf)
                VERIFY( c == '#' );
        }
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}
