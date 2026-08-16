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

void test_istream_ws_wchar_t_1()
{
    dump_info("Test istream<wchar_t> with ws case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::wstring str01(L" santa barbara ");

        // template<_CharT, _Traits>
        //  basic_istream<_CharT, _Traits>& ws(basic_istream<_Char, _Traits>& is)
        T iss01{IOv2::mem_device{str01}};
        T iss02{IOv2::mem_device{str01}};

        std::wstring str04;
        std::wstring str05;
        iss01 >> str04;
        VERIFY( str04.size() != str01.size() );
        VERIFY( str04 == L"santa" );

        iss02 >> IOv2::ws;
        iss02 >> str05;
        VERIFY( str05.size() != str01.size() );
        VERIFY( str05 == L"santa" );
        VERIFY( str05 == str04 );

        iss01 >> str04;
        VERIFY( str04.size() != str01.size() );
        VERIFY( str04 == L"barbara" );

        iss02 >> IOv2::ws;
        iss02 >> str05;
        VERIFY( str05.size() != str01.size() );
        VERIFY( str05 == L"barbara" );
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

// wchar_t counterpart of test_istream_function_manip_char_1: a function-pointer manipulator
// in a NON-CONST lvalue must dispatch to operator>>(T&, void(*)(ios_base<wchar_t>&)) rather
// than being shadowed by the generic value operator>>.
void test_istream_function_manip_wchar_t_1()
{
    dump_info("Test istream<wchar_t> function-pointer manipulator via operator>> case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T iss{IOv2::mem_device{std::wstring(L"hello world")}};

        static int calls;
        calls = 0;
        void (*manip)(IOv2::ios_base<wchar_t>&) = [](IOv2::ios_base<wchar_t>&){ ++calls; };

        iss >> manip;
        VERIFY( calls == 1 );

        iss >> manip >> manip;        // operator>> returns the stream, so manipulators chain
        VERIFY( calls == 3 );

        // a capture-less lambda reaches the same overload once decayed with unary +
        iss >> +[](IOv2::ios_base<wchar_t>&){ ++calls; };
        VERIFY( calls == 4 );

        // the generic value operator>> still extracts real values afterwards
        std::wstring tok;
        iss >> tok;
        VERIFY( tok == L"hello" );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// wchar_t counterpart of test_istream_null_manip_char_1: a null manipulator must be
// rejected, leaving strfailbit set (no mask -> no throw). The tag-object manipulators need
// no such branch: an object cannot be null.
void test_istream_null_manip_wchar_t_1()
{
    dump_info("Test istream<wchar_t> null manipulator via operator>> case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T iss{IOv2::mem_device{std::wstring(L"hello world")}};

        iss >> static_cast<void(*)(IOv2::ios_base<wchar_t>&)>(nullptr);
        VERIFY( iss.rdstate() & IOv2::ios_defs::strfailbit );
        iss.clear();

        static int base_calls;
        base_calls = 0;
        iss >> +[](IOv2::ios_base<wchar_t>&){ ++base_calls; };
        VERIFY( base_calls == 1 );
        VERIFY( !(iss.rdstate() & IOv2::ios_defs::strfailbit) );

        std::wstring tok;
        iss >> tok;
        VERIFY( tok == L"hello" );
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// wchar_t counterpart of test_istream_ws_no_ctype_char_1: removing the ctype facet makes
// the sentry's whitespace-skip fail with stream_error -> strfailbit (no mask -> no throw).
void test_istream_ws_no_ctype_wchar_t_1()
{
    dump_info("Test istream<wchar_t> sentry with no ctype facet case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        const auto loc = IOv2::locale<wchar_t>("C").remove<IOv2::ctype_conf<wchar_t>>();

        T iss{IOv2::mem_device{std::wstring(L"  42")}, loc};
        iss >> IOv2::ws;
        VERIFY( iss.str_fail() );

        T iss2{IOv2::mem_device{std::wstring(L"  42")}, loc};
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
// wchar_t counterpart: a bare abs_flusher tie target whose flush fails. try_flush() is
// noexcept by contract, so the failure is absorbed here and recorded on the target itself.
struct ThrowingTieW : public IOv2::abs_flusher
{
    int  flushed = 0;
    bool failed  = false;
    void try_flush() noexcept override
    {
        ++flushed;
        try { throw IOv2::stream_error("tied flush boom"); }
        catch (...) { failed = true; }
    }
};
}

// wchar_t counterpart of test_istream_tied_flush_char_1: the input sentry flushes the tied
// stream before locking; a failing flush stays on the target and extraction still succeeds.
void test_istream_tied_flush_wchar_t_1()
{
    dump_info("Test istream<wchar_t> tied-stream flush throw case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        ThrowingTieW tt;
        T iss{IOv2::mem_device{std::wstring(L"42 rest")}, IOv2::locale<wchar_t>("C")};
        iss.tie(&tt);

        int v = 0;
        iss >> v;
        VERIFY( tt.flushed >= 1 );
        VERIFY( tt.failed );          // the failure was recorded on the target
        VERIFY( v == 42 );
        VERIFY( iss.good() );         // and not on the initiator

        iss.tie(nullptr);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}


namespace
{
using DirIn_w   = IOv2::istream<IOv2::mem_device<wchar_t>, wchar_t>;
using DirOut_w  = IOv2::ostream<IOv2::mem_device<wchar_t>, wchar_t>;
using DirBoth_w = IOv2::iostream<IOv2::mem_device<wchar_t>, wchar_t>;

// See the char counterpart for why the io_traits members are probed directly rather than
// through `s >> m` / `s << m`.
template <typename S, typename M>
concept can_extract_w = requires (S& s, const M& m)
{ IOv2::io_traits<typename S::char_type, M>::sread(s, m); };

template <typename S, typename M>
concept can_insert_w = requires (S& s, const M& m)
{ IOv2::io_traits<typename S::char_type, M>::swrite(s, m); };

// wchar_t counterpart of the char direction matrix. Which io_traits member exists is
// character-type agnostic, so the wrong direction has to be rejected here too.
static_assert(  can_extract_w<DirBoth_w, IOv2::ws_t>   && !can_insert_w<DirBoth_w, IOv2::ws_t> );
static_assert(  can_insert_w<DirBoth_w, IOv2::endl_t>  && !can_extract_w<DirBoth_w, IOv2::endl_t> );
static_assert(  can_insert_w<DirBoth_w, IOv2::ends_t>  && !can_extract_w<DirBoth_w, IOv2::ends_t> );
static_assert(  can_insert_w<DirBoth_w, IOv2::flush_t> && !can_extract_w<DirBoth_w, IOv2::flush_t> );

static_assert(  can_extract_w<DirIn_w, IOv2::ws_t>     && !can_insert_w<DirIn_w, IOv2::ws_t> );
static_assert( !can_extract_w<DirIn_w, IOv2::endl_t>   && !can_insert_w<DirIn_w, IOv2::endl_t> );
static_assert(  can_insert_w<DirOut_w, IOv2::endl_t>   && !can_extract_w<DirOut_w, IOv2::endl_t> );
static_assert( !can_extract_w<DirOut_w, IOv2::ws_t>    && !can_insert_w<DirOut_w, IOv2::ws_t> );

// setfill is the one manipulator whose character type must match the stream's exactly. That
// requirement used to live in the parameter type setfill_t<typename T::char_type>; it is now a
// requires-clause on both io_traits members, so a mismatch is rejected at the declaration
// rather than inside the body.
static_assert(  can_insert_w<DirBoth_w, IOv2::setfill_t<wchar_t>>
             && can_extract_w<DirBoth_w, IOv2::setfill_t<wchar_t>> );
static_assert( !can_insert_w<DirBoth_w, IOv2::setfill_t<char>>
            && !can_extract_w<DirBoth_w, IOv2::setfill_t<char>> );
}

// wchar_t counterpart of test_istream_manip_direction_char_1.
void test_istream_manip_direction_wchar_t_1()
{
    dump_info("Test istream<wchar_t> manipulator direction case 1...");

    DirBoth_w iss{IOv2::mem_device{std::wstring(L"  santa")}};
    iss >> IOv2::ws;
    std::wstring tok;
    iss >> tok;
    VERIFY( tok == L"santa" );

    dump_info("Done\n");
}
