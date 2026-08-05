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

// Insertion-side companion to test_istream_function_manip_char_1. A function pointer is the
// only manipulator shape that bypasses io_traits, and its parameter is a non-deduced context,
// so it has to beat the generic operator<< -- whose parameter is const TValue& -- on partial
// ordering. The invocation counter proves the manipulator overload ran.
void test_ostream_function_manip_char_1()
{
    dump_info("Test ostream<char> function-pointer manipulator via operator<< case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto oss = T(IOv2::mem_device{std::string("")});
        static int calls;
        calls = 0;
        void (*manip)(IOv2::ios_base<char>&) = [](IOv2::ios_base<char>&){ ++calls; };

        oss << manip;
        VERIFY( calls == 1 );

        oss << manip << manip;        // operator<< returns the stream, so manipulators chain
        VERIFY( calls == 3 );

        // a capture-less lambda reaches the same overload once decayed with unary +
        oss << +[](IOv2::ios_base<char>&){ ++calls; };
        VERIFY( calls == 4 );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// A null function-pointer manipulator passed to operator<< must be rejected: the operator
// throws stream_error, its own handler categorizes it into strfailbit, and -- with no
// exception mask set -- returns the stream without throwing. The tag-object manipulators
// need no such branch: an object cannot be null.
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

        // same overload, non-null: the callable runs against the stream's ios_base
        static int base_calls;
        base_calls = 0;
        oss << +[](IOv2::ios_base<char>&){ ++base_calls; };
        VERIFY( base_calls == 1 );
        VERIFY( !(oss.rdstate() & IOv2::ios_defs::strfailbit) );

        // stream is still usable after the rejected manipulator
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

// endl/ends/flush are tag objects whose whole behaviour lives in io_traits, and os << m is the
// only entry -- the standard's direct-call form, std::endl(os), does not exist here. endl is
// covered by the cases above; this covers ends and flush.
void test_ostream_manip_tag_char_1()
{
    dump_info("Test ostream<char> tag manipulators case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto oss = T(IOv2::mem_device{std::string("")});
        oss << IOv2::endl;
        VERIFY( oss.device().str() == "\n" );
        VERIFY( oss.good() );

        oss << IOv2::ends;
        VERIFY( oss.device().str().size() == 2 );
        VERIFY( oss.good() );

        oss << IOv2::flush;
        VERIFY( oss.good() );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}
