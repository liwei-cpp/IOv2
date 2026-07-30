#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <facet/ctype.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/file_guard.h>
#include <support/verify.h>

void test_istream_ws_char_1()
{
    dump_info("Test istream<char> with ws case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str01(" santa barbara ");

        // template<_CharT, _Traits>
        //  basic_istream<_CharT, _Traits>& ws(basic_istream<_Char, _Traits>& is)
        T iss01{IOv2::mem_device{str01}};
        T iss02{IOv2::mem_device{str01}};

        std::string str04;
        std::string str05;
        iss01 >> str04;
        VERIFY( str04.size() != str01.size() );
        VERIFY( str04 == "santa" );

        iss02 >> IOv2::ws;
        iss02 >> str05;
        VERIFY( str05.size() != str01.size() );
        VERIFY( str05 == "santa" );
        VERIFY( str05 == str04 );

        iss01 >> str04;
        VERIFY( str04.size() != str01.size() );
        VERIFY( str04 == "barbara" );

        iss02 >> IOv2::ws;
        iss02 >> str05;
        VERIFY( str05.size() != str01.size() );
        VERIFY( str05 == "barbara" );
        VERIFY( str05 == str04 );

        VERIFY( (bool)iss01 );
        VERIFY( (bool)iss02 );
        VERIFY( !iss01.eof() );
        VERIFY( !iss02.eof() );

        iss01 >> IOv2::ws;
        VERIFY( (bool)iss01 );
        VERIFY( iss01.eof() );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// Regression for the std::function manipulator being shadowed on the extraction side. A
// std::function manipulator held in a NON-CONST lvalue must dispatch to the manipulator
// overload. Before the generic value operator>> was constrained (requires is_reader_def<...>),
// such an lvalue bound to operator>>(T&, TValue&) instead and failed to compile ("No parse
// method provided"). The invocation counter proves dispatch reached the manipulator (the
// generic operator would never invoke the callable). Also exercises the sibling
// function-pointer form.
//
// The callable now takes ios_base<char>& rather than the concrete stream: the overloads taking
// a stream are gone, because a callable taking a stream can do I/O while its type carries no
// direction, so on a bidirectional stream it could be applied through either operator. The
// ios_base<char>& form is direction-free by construction -- ios_base exposes no streambuf and
// no device, so such a manipulator cannot do I/O at all. Anything that does need I/O derives
// from in_manip / out_manip instead.
void test_istream_function_manip_char_1()
{
    dump_info("Test istream<char> std::function manipulator via operator>> case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T iss{IOv2::mem_device{std::string("hello world")}};

        int calls = 0;
        std::function<void(IOv2::ios_base<char>&)> manip =
            [&calls](IOv2::ios_base<char>&){ ++calls; };

        iss >> manip;                 // the previously-broken path
        VERIFY( calls == 1 );

        iss >> manip >> manip;        // operator>> returns the stream, so manipulators chain
        VERIFY( calls == 3 );

        // sibling function-pointer form: operator>>(T&, void(*)(ios_base<char>&))
        static int fcalls;
        fcalls = 0;
        iss >> +[](IOv2::ios_base<char>&){ ++fcalls; };
        VERIFY( fcalls == 1 );

        // the generic value operator>> still extracts real values afterwards
        std::string tok;
        iss >> tok;
        VERIFY( tok == "hello" );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// A null manipulator (null function pointer / empty std::function) passed to operator>>
// must be rejected: the operator throws stream_error, its own handler categorizes it into
// strfailbit, and -- with no exception mask set -- returns the stream without throwing.
// This exercises the error branch of both surviving manipulator overloads. The tag-object
// manipulators need no such branch: an object cannot be null.
void test_istream_null_manip_char_1()
{
    dump_info("Test istream<char> null manipulator via operator>> case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T iss{IOv2::mem_device{std::string("hello world")}};

        // overload: operator>>(T&, void(*)(ios_base<char>&))
        iss >> static_cast<void(*)(IOv2::ios_base<char>&)>(nullptr);
        VERIFY( iss.rdstate() & IOv2::ios_defs::strfailbit );
        iss.clear();

        // overload: operator>>(T&, const std::function<void(ios_base<char>&)>&)
        std::function<void(IOv2::ios_base<char>&)> empty_base_fn;
        iss >> empty_base_fn;
        VERIFY( iss.rdstate() & IOv2::ios_defs::strfailbit );
        iss.clear();

        // same overload, non-null: the callable runs against the stream's ios_base
        int base_calls = 0;
        std::function<void(IOv2::ios_base<char>&)> base_fn =
            [&base_calls](IOv2::ios_base<char>&){ ++base_calls; };
        iss >> base_fn;
        VERIFY( base_calls == 1 );
        VERIFY( !(iss.rdstate() & IOv2::ios_defs::strfailbit) );

        // stream is still usable after all the rejected manipulators
        std::string tok;
        iss >> tok;
        VERIFY( tok == "hello" );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// The input sentry classifies leading whitespace through the ctype facet. With that facet
// removed from the locale, the sentry's skip step throws stream_error ("no ctype facet"),
// which is reported as strfailbit; the follow-up validity check inside the sentry then
// re-throws on the now-failed stream. Both the ws manipulator and skipws-enabled
// extraction drive this path. With no exception mask set nothing escapes to the caller.
void test_istream_ws_no_ctype_char_1()
{
    dump_info("Test istream<char> sentry with no ctype facet case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const auto loc = IOv2::locale<char>("C").remove<IOv2::ctype_conf<char>>();

        // ws manipulator: sentry constructed with noskip == false -> needs the ctype facet
        T iss{IOv2::mem_device{std::string("  42")}, loc};
        iss >> IOv2::ws;
        VERIFY( iss.str_fail() );

        // skipws-enabled extraction takes the same sentry skip path
        T iss2{IOv2::mem_device{std::string("  42")}, loc};
        int v = 0;
        iss2 >> v;
        VERIFY( iss2.str_fail() );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

namespace
{
// A bare abs_flusher tie target whose flush() throws, to exercise the input sentry's
// pre-lock "flush the tied stream" step and its swallowing catch(...).
struct ThrowingTie : public IOv2::abs_flusher
{
    int flushed = 0;
    void flush() override { ++flushed; throw IOv2::stream_error("tied flush boom"); }
};
}

// An istream can have a tied stream; the input sentry flushes it before acquiring the lock.
// When that flush throws, the sentry swallows it (catch(...)) and extraction proceeds
// normally. Verifies the tied stream was flushed and the value still reads back.
void test_istream_tied_flush_char_1()
{
    dump_info("Test istream<char> tied-stream flush throw case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        ThrowingTie tt;
        T iss{IOv2::mem_device{std::string("42 rest")}, IOv2::locale<char>("C")};
        iss.tie(&tt);

        int v = 0;
        iss >> v;                     // sentry flushes tt -> throws -> swallowed
        VERIFY( tt.flushed >= 1 );
        VERIFY( v == 42 );
        VERIFY( iss.good() );

        iss.tie(nullptr);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}


namespace
{
using DirIn_c   = IOv2::istream<IOv2::mem_device<char>, char>;
using DirOut_c  = IOv2::ostream<IOv2::mem_device<char>, char>;
using DirBoth_c = IOv2::iostream<IOv2::mem_device<char>, char>;

template <typename S, typename M> concept can_extract = requires (S& s, const M& m) { s >> m; };
template <typename S, typename M> concept can_insert  = requires (S& s, const M& m) { s << m; };

// A manipulator legal in both directions derives from both tags. The deleted overloads carry
// an exclusion clause for exactly this case -- without it both the real and the deleted
// overload would be viable and neither would subsume the other, leaving such a manipulator
// ambiguous, and therefore unusable, in EITHER direction. No library manipulator of this shape
// existed before the flag manipulators grew tags, so it is asserted here directly.
struct both_manip : IOv2::in_manip, IOv2::out_manip
{
    template <typename T> void operator () (T&) const {}
};

// Same, but counting its invocations, for the runtime half of the check. It lives here rather
// than inside the test function because a local class cannot have a member template.
struct counting_manip : IOv2::in_manip, IOv2::out_manip
{
    int* m_n;
    template <typename T> void operator () (T&) const { ++*m_n; }
};

// The point of the direction tags: on a bidirectional stream, where istream_type and
// ostream_type are both satisfied, the wrong direction must not compile. Before the tags the
// manipulators were void(*)(T&) function templates whose direction lived only in a constraint
// -- and a constraint is not part of a function's type -- so `io << ws` and `io >> endl` both
// compiled and silently did the opposite of what they read like.
static_assert(  can_extract<DirBoth_c, IOv2::_Ws>    && !can_insert<DirBoth_c, IOv2::_Ws> );
static_assert(  can_insert<DirBoth_c, IOv2::_Endl>   && !can_extract<DirBoth_c, IOv2::_Endl> );
static_assert(  can_insert<DirBoth_c, IOv2::_Ends>   && !can_extract<DirBoth_c, IOv2::_Ends> );
static_assert(  can_insert<DirBoth_c, IOv2::_Flush>  && !can_extract<DirBoth_c, IOv2::_Flush> );

// Unidirectional streams keep their own direction and gain nothing in the other one.
static_assert(  can_extract<DirIn_c, IOv2::_Ws>      && !can_insert<DirIn_c, IOv2::_Ws> );
static_assert( !can_extract<DirIn_c, IOv2::_Endl>    && !can_insert<DirIn_c, IOv2::_Endl> );
static_assert(  can_insert<DirOut_c, IOv2::_Endl>    && !can_extract<DirOut_c, IOv2::_Endl> );
static_assert( !can_extract<DirOut_c, IOv2::_Ws>     && !can_insert<DirOut_c, IOv2::_Ws> );

// Manipulators tagged both ways stay usable both ways.
static_assert(  can_insert<DirBoth_c, both_manip>    &&  can_extract<DirBoth_c, both_manip> );
static_assert(  can_insert<DirBoth_c, IOv2::_Setw>   &&  can_extract<DirBoth_c, IOv2::_Setw> );
static_assert(  can_insert<DirBoth_c, IOv2::_Setfill<char>>
             && can_extract<DirBoth_c, IOv2::_Setfill<char>> );

// The tag and the accepted stream type agree: ws will not take a pure output stream, endl
// will not take a pure input one. (A mismatched _Setfill char type cannot be probed this way
// -- the unconstrained fallback stays viable and reports through its static_assert instead.)
static_assert(  std::invocable<const IOv2::_Ws&,   DirIn_c&>  );
static_assert( !std::invocable<const IOv2::_Ws&,   DirOut_c&> );
static_assert(  std::invocable<const IOv2::_Endl&, DirOut_c&> );
static_assert( !std::invocable<const IOv2::_Endl&, DirIn_c&>  );
}

// Runtime companion to the direction static_asserts above: the surviving direction still works
// on a bidirectional stream, and the standard's direct-call form ws(is) -- which is why the tag
// keeps an operator() -- behaves like the operator form.
void test_istream_manip_direction_char_1()
{
    dump_info("Test istream<char> manipulator direction case 1...");

    DirBoth_c iss{IOv2::mem_device{std::string("  santa")}};
    iss >> IOv2::ws;
    std::string tok;
    iss >> tok;
    VERIFY( tok == "santa" );

    DirBoth_c iss2{IOv2::mem_device{std::string("  santa")}};
    IOv2::ws(iss2);                   // direct-call form, as in std::ws(is)
    std::string tok2;
    iss2 >> tok2;
    VERIFY( tok2 == "santa" );

    // A manipulator tagged both ways runs from either operator.
    int calls = 0;
    DirBoth_c iss3{IOv2::mem_device{std::string("x")}};
    iss3 >> counting_manip{{}, {}, &calls};
    iss3 << counting_manip{{}, {}, &calls};
    VERIFY( calls == 2 );

    dump_info("Done\n");
}
