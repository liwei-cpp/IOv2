#include <functional>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <facet/ctype.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

void test_ostream_endl_char_1()
{
    dump_info("Test ostream<char> with endl case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str01(" santa barbara ");    
        auto oss01 = T(IOv2::mem_device{" santa barbara "});
        auto oss02 = T(IOv2::mem_device{""});
        
        oss01 << IOv2::endl;
        VERIFY(oss01.device().str().size() == str01.size());

        oss02 << IOv2::endl;
        VERIFY(oss02.device().str().size() == 1);
    };
    
    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// Insertion-side companion to test_istream_function_manip_char_1. operator<< already
// resolved a std::function manipulator correctly (its generic takes const TValue&, so the
// manipulator overload wins on partial ordering), but the generic was likewise constrained
// (requires is_writer_def<...>) for symmetry; this guards that both manipulator forms keep
// dispatching after that change. The invocation counter proves the manipulator overload ran.
void test_ostream_function_manip_char_1()
{
    dump_info("Test ostream<char> std::function manipulator via operator<< case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto oss = T(IOv2::mem_device{std::string("")});
        int calls = 0;
        std::function<void(IOv2::ios_base<char>&)> manip =
            [&calls](IOv2::ios_base<char>&){ ++calls; };

        oss << manip;                 // std::function manipulator via operator<<
        VERIFY( calls == 1 );

        oss << manip << manip;        // operator<< returns the stream, so manipulators chain
        VERIFY( calls == 3 );

        // sibling function-pointer form: operator<<(T&, void(*)(ios_base<char>&))
        static int fcalls;
        fcalls = 0;
        oss << +[](IOv2::ios_base<char>&){ ++fcalls; };
        VERIFY( fcalls == 1 );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// A null manipulator (null function pointer / empty std::function) passed to operator<<
// must be rejected by every manipulator overload: the operator throws stream_error, its
// own handler categorizes it into strfailbit, and -- with no exception mask set -- returns
// the stream without throwing. This exercises the error branch of both surviving
// manipulator overloads. The tag-object manipulators need no such branch: an object
// cannot be null.
void test_ostream_null_manip_char_1()
{
    dump_info("Test ostream<char> null manipulator via operator<< case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto oss = T(IOv2::mem_device{std::string("")});

        // overload: operator<<(T&, void(*)(ios_base<char>&))
        oss << static_cast<void(*)(IOv2::ios_base<char>&)>(nullptr);
        VERIFY( oss.rdstate() & IOv2::ios_defs::strfailbit );
        oss.clear();

        // overload: operator<<(T&, const std::function<void(ios_base<char>&)>&)
        std::function<void(IOv2::ios_base<char>&)> empty_base_fn;
        oss << empty_base_fn;
        VERIFY( oss.rdstate() & IOv2::ios_defs::strfailbit );
        oss.clear();

        // same overload, non-null: the callable runs against the stream's ios_base
        int base_calls = 0;
        std::function<void(IOv2::ios_base<char>&)> base_fn =
            [&base_calls](IOv2::ios_base<char>&){ ++base_calls; };
        oss << base_fn;
        VERIFY( base_calls == 1 );
        VERIFY( !(oss.rdstate() & IOv2::ios_defs::strfailbit) );

        // stream is still usable after all the rejected manipulators
        oss << "ok";
        auto [dev, err] = oss.detach();
        VERIFY( dev.str() == "ok" );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_endl_char_2()
{
    dump_info("Test ostream<char> with endl case 2 (facet failure leaves the flags alone)...");

    auto helper = []<template<typename, typename> class T>()
    {
        // endl needs a ctype facet to widen '\n'; without one it fails. It used to set
        // unitbuf before that point and restore it only on the normal path, so the failure
        // left unitbuf set on the stream for good -- every later write then flushed to the
        // device, silently and permanently. endl no longer touches the flags at all.
        IOv2::locale<char> loc("C");
        T str(IOv2::mem_device{""}, loc.remove<IOv2::ctype_conf<char>>());

        const IOv2::ios_defs::fmtflags before = str.flags();
        VERIFY((before & IOv2::ios_defs::unitbuf) == 0);

        str << IOv2::endl;

        VERIFY(str.str_fail());                 // reported as a stream failure
        VERIFY(str.flags() == before);          // and nothing else was disturbed

        // The same holds when the failure is reported by throwing.
        T thr(IOv2::mem_device{""}, loc.remove<IOv2::ctype_conf<char>>());
        thr.exceptions(IOv2::ios_defs::strfailbit);
        const IOv2::ios_defs::fmtflags thr_before = thr.flags();

        bool threw = false;
        try { thr << IOv2::endl; }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
        VERIFY(thr.flags() == thr_before);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// endl/ends/flush are tag objects rather than function templates now; each keeps an operator()
// so the standard's direct-call form -- std::endl(os), std::flush(os) -- still works. The
// operator form is covered by test_ostream_endl_char_1; this covers the call form, and that a
// facet failure in the call form is reported on the stream instead of escaping. That last part
// is the one behaviour the old function template got wrong: it threw out of endl(os) with no
// state bit set and no regard for the exception mask, leaving the stream reporting good().
void test_ostream_manip_direct_call_char_1()
{
    dump_info("Test ostream<char> manipulator direct-call form case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto oss = T(IOv2::mem_device{std::string("")});
        IOv2::endl(oss);
        VERIFY( oss.device().str() == "\n" );
        VERIFY( oss.good() );

        IOv2::ends(oss);
        VERIFY( oss.device().str().size() == 2 );
        VERIFY( oss.good() );

        IOv2::flush(oss);
        VERIFY( oss.good() );

        auto bad = T(IOv2::mem_device{std::string("")},
                     IOv2::locale<char>("C").remove<IOv2::ctype_conf<char>>());
        IOv2::endl(bad);
        VERIFY( bad.str_fail() );
        VERIFY( !bad.good() );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}
