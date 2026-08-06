#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <facet/ctype.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
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

// A function pointer is the only manipulator shape that bypasses io_traits, and it must beat
// the generic operator>> -- whose parameter is a forwarding reference -- on overload
// resolution. The invocation counter proves dispatch reached the manipulator (the generic
// operator would never invoke the callable).
//
// The callable takes ios_base<char>& rather than the concrete stream: the overloads taking a
// stream are gone, because a callable taking a stream can do I/O while its type carries no
// direction, so on a bidirectional stream it could be applied through either operator. The
// ios_base<char>& form is direction-free by construction -- ios_base exposes no streambuf and
// no device, so such a manipulator cannot do I/O at all. Anything that does need I/O declares
// its direction through io_traits instead.
void test_istream_function_manip_char_1()
{
    dump_info("Test istream<char> function-pointer manipulator via operator>> case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T iss{IOv2::mem_device{std::string("hello world")}};

        static int calls;
        calls = 0;
        void (*manip)(IOv2::ios_base<char>&) = [](IOv2::ios_base<char>&){ ++calls; };

        iss >> manip;                 // a non-const lvalue must still reach this overload
        VERIFY( calls == 1 );

        iss >> manip >> manip;        // operator>> returns the stream, so manipulators chain
        VERIFY( calls == 3 );

        // a capture-less lambda reaches the same overload once decayed with unary +
        iss >> +[](IOv2::ios_base<char>&){ ++calls; };
        VERIFY( calls == 4 );

        // the generic value operator>> still extracts real values afterwards
        std::string tok;
        iss >> tok;
        VERIFY( tok == "hello" );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// A null function-pointer manipulator passed to operator>> must be rejected: the operator
// throws stream_error, its own handler categorizes it into strfailbit, and -- with no
// exception mask set -- returns the stream without throwing. The tag-object manipulators
// need no such branch: an object cannot be null.
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

        // same overload, non-null: the callable runs against the stream's ios_base
        static int base_calls;
        base_calls = 0;
        iss >> +[](IOv2::ios_base<char>&){ ++base_calls; };
        VERIFY( base_calls == 1 );
        VERIFY( !(iss.rdstate() & IOv2::ios_defs::strfailbit) );

        // stream is still usable after the rejected manipulator
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
    void try_flush() override { ++flushed; throw IOv2::stream_error("tied flush boom"); }
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

// These probe the stream form of io_traits directly -- the rung the insertion / extraction
// operators try first for a manipulator. `requires { s >> m; }` would answer a broader question:
// it also folds in the operator's other rungs and the function-pointer manipulator overload, so a
// failure would no longer point at the stream form itself.
template <typename S, typename M>
concept can_extract = requires (S& s, const M& m)
{ IOv2::io_traits<typename S::char_type, M>::sread(s, m); };

template <typename S, typename M>
concept can_insert = requires (S& s, const M& m)
{ IOv2::io_traits<typename S::char_type, M>::swrite(s, m); };

// A manipulator legal in both directions declares both members. No library manipulator outside
// io_manip.h has this shape, so one is defined here to pin the behaviour down.
struct both_manip
{
    template <typename T> void operator () (T&) const {}
};

// Same, but counting its invocations, for the runtime half of the check. It lives here rather
// than inside the test function because a local class cannot have a member template.
struct counting_manip
{
    int* m_n;
    template <typename T> void operator () (T&) const { ++*m_n; }
};
}

namespace IOv2
{
template <typename TChar>
struct io_traits<TChar, both_manip>
{
    template <ostream_type T> static void swrite(T& s, const both_manip& f) { f(s); }
    template <istream_type T> static void sread (T& s, const both_manip& f) { f(s); }
};

template <typename TChar>
struct io_traits<TChar, counting_manip>
{
    template <ostream_type T> static void swrite(T& s, const counting_manip& f) { f(s); }
    template <istream_type T> static void sread (T& s, const counting_manip& f) { f(s); }
};
}

namespace
{
// The point of putting the direction in io_traits: on a bidirectional stream, where
// istream_type and ostream_type are both satisfied, the wrong direction must not compile.
// A constraint alone cannot achieve that -- such a stream satisfies either one -- so the
// direction has to be which member exists.
static_assert(  can_extract<DirBoth_c, IOv2::ws_t>    && !can_insert<DirBoth_c, IOv2::ws_t> );
static_assert(  can_insert<DirBoth_c, IOv2::endl_t>   && !can_extract<DirBoth_c, IOv2::endl_t> );
static_assert(  can_insert<DirBoth_c, IOv2::ends_t>   && !can_extract<DirBoth_c, IOv2::ends_t> );
static_assert(  can_insert<DirBoth_c, IOv2::flush_t>  && !can_extract<DirBoth_c, IOv2::flush_t> );

// Unidirectional streams keep their own direction and gain nothing in the other one.
static_assert(  can_extract<DirIn_c, IOv2::ws_t>      && !can_insert<DirIn_c, IOv2::ws_t> );
static_assert( !can_extract<DirIn_c, IOv2::endl_t>    && !can_insert<DirIn_c, IOv2::endl_t> );
static_assert(  can_insert<DirOut_c, IOv2::endl_t>    && !can_extract<DirOut_c, IOv2::endl_t> );
static_assert( !can_extract<DirOut_c, IOv2::ws_t>     && !can_insert<DirOut_c, IOv2::ws_t> );

// Manipulators tagged both ways stay usable both ways.
static_assert(  can_insert<DirBoth_c, both_manip>    &&  can_extract<DirBoth_c, both_manip> );
static_assert(  can_insert<DirBoth_c, IOv2::setw_t>   &&  can_extract<DirBoth_c, IOv2::setw_t> );
static_assert(  can_insert<DirBoth_c, IOv2::setfill_t<char>>
             && can_extract<DirBoth_c, IOv2::setfill_t<char>> );

// A setfill_t whose character type does not match the stream is rejected in both directions:
// io_traits carries the same_as constraint on its members precisely so this stays visible to a
// requires-expression rather than erroring inside the body.
static_assert( !can_insert<DirBoth_c, IOv2::setfill_t<wchar_t>>
            && !can_extract<DirBoth_c, IOv2::setfill_t<wchar_t>> );
}

// Runtime companion to the direction static_asserts above: the surviving direction still works
// on a bidirectional stream.
void test_istream_manip_direction_char_1()
{
    dump_info("Test istream<char> manipulator direction case 1...");

    DirBoth_c iss{IOv2::mem_device{std::string("  santa")}};
    iss >> IOv2::ws;
    std::string tok;
    iss >> tok;
    VERIFY( tok == "santa" );

    // A manipulator tagged both ways runs from either operator.
    int calls = 0;
    DirBoth_c iss3{IOv2::mem_device{std::string("x")}};
    iss3 >> counting_manip{&calls};
    iss3 << counting_manip{&calls};
    VERIFY( calls == 2 );

    dump_info("Done\n");
}
