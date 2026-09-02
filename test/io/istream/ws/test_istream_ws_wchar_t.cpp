// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same ws contract and the same manipulator-dispatch rules as
 * test_istream_ws_char.cpp for wchar_t.
 *
 * Which io_traits member exists is what carries a manipulator's direction, and
 * that is independent of the character type -- so the direction matrix has to
 * come out the same here, and setfill is still the one manipulator whose
 * character type must match the stream's. What this instantiation adds beyond
 * the repetition is the mismatch pointing the other way: setfill_t<char> is
 * what a wide stream must reject.
 */
#include <common/defs.h>
#include <device/mem_device.h>
#include <facet/ctype_details.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/utilities/ostream_operators.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

TEST(IstreamWsWchar, WsDiscardsEveryWhitespaceCharacterAndStopsOnTheFirstOther)
{
    auto expect_skipped = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::wstring(L" \t\n\v\f\r x")}};

        is >> ws;
        EXPECT_EQ(is.peek(), L'x');
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
    };

    expect_skipped.operator()<istream>();
    expect_skipped.operator()<iostream>();
}

TEST(IstreamWsWchar, WsOnANonWhitespaceCharacterChangesNothing)
{
    auto expect_unchanged = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::wstring(L"abc")}};

        const ios_defs::iostate before = is.rdstate();
        is >> ws;
        EXPECT_EQ(is.rdstate(), before);
        EXPECT_EQ(is.peek(), L'a');
    };

    expect_unchanged.operator()<istream>();
    expect_unchanged.operator()<iostream>();
}

TEST(IstreamWsWchar, WsAtTheEndSetsEndOfFileWithoutFailing)
{
    auto expect_end = []<template <typename, typename> class T>()
    {
        T all_space{mem_device{std::wstring(L"   \t\n")}};
        all_space >> ws;
        EXPECT_TRUE(all_space.eof());
        EXPECT_FALSE(all_space.rdstate() & ios_defs::strfailbit);
        EXPECT_TRUE(static_cast<bool>(all_space));
    };

    expect_end.operator()<istream>();
    expect_end.operator()<iostream>();
}

TEST(IstreamWsWchar, WsSkipsWhereExtractionWillNotWhenSkipwsIsOff)
{
    auto expect_manual = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::wstring(L"  42  7")}, locale<wchar_t>("C")};
        is.unsetf(ios_defs::skipws);

        int v = 0;
        is >> v;
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
        is.clear();

        is >> ws >> v;
        EXPECT_EQ(v, 42);

        is >> ws >> v;
        EXPECT_EQ(v, 7);
    };

    expect_manual.operator()<istream>();
    expect_manual.operator()<iostream>();
}

// wchar_t counterpart of the char case: a function-pointer manipulator in a NON-CONST lvalue
// must dispatch to operator>>(T&, void(*)(ios_base<wchar_t>&)) rather than being shadowed by
// the generic value operator>>.
TEST(IstreamWsWchar, AFunctionPointerManipulatorBeatsTheGenericExtraction)
{
    auto expect_dispatched = []<template <typename, typename> class T>()
    {
        T iss{mem_device{std::wstring(L"hello world")}};

        static int calls;
        calls = 0;
        void (*manip)(ios_base<wchar_t>&) = [](ios_base<wchar_t>&) { ++calls; };

        iss >> manip;
        EXPECT_EQ(calls, 1);

        iss >> manip >> manip;        // operator>> returns the stream, so manipulators chain
        EXPECT_EQ(calls, 3);

        // a capture-less lambda reaches the same overload once decayed with unary +
        iss >> +[](ios_base<wchar_t>&) { ++calls; };
        EXPECT_EQ(calls, 4);

        // the generic value operator>> still extracts real values afterwards
        std::wstring tok;
        iss >> tok;
        EXPECT_EQ(tok, L"hello");
    };

    expect_dispatched.operator()<istream>();
    expect_dispatched.operator()<iostream>();
}

// wchar_t counterpart: a null manipulator must be rejected, leaving strfailbit set (no mask
// -> no throw). The tag-object manipulators need no such branch: an object cannot be null.
TEST(IstreamWsWchar, ANullFunctionPointerManipulatorIsRejected)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        T iss{mem_device{std::wstring(L"hello world")}};

        EXPECT_NO_THROW(iss >> static_cast<void (*)(ios_base<wchar_t>&)>(nullptr));
        EXPECT_TRUE(iss.rdstate() & ios_defs::strfailbit);
        iss.clear();

        static int base_calls;
        base_calls = 0;
        iss >> +[](ios_base<wchar_t>&) { ++base_calls; };
        EXPECT_EQ(base_calls, 1);
        EXPECT_FALSE(iss.rdstate() & ios_defs::strfailbit);

        std::wstring tok;
        iss >> tok;
        EXPECT_EQ(tok, L"hello");
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

// wchar_t counterpart: removing the ctype facet makes the sentry's whitespace-skip fail with
// stream_error -> strfailbit (no mask -> no throw).
TEST(IstreamWsWchar, TheSentryFailsWhenTheLocaleHasNoCtypeFacet)
{
    auto expect_failed = []<template <typename, typename> class T>()
    {
        const auto loc = locale<wchar_t>("C").remove<ctype_conf<wchar_t>>();

        T iss{mem_device{std::wstring(L"  42")}, loc};
        EXPECT_NO_THROW(iss >> ws);
        EXPECT_TRUE(iss.str_fail());

        T iss2{mem_device{std::wstring(L"  42")}, loc};
        int v = 0;
        EXPECT_NO_THROW(iss2 >> v);
        EXPECT_TRUE(iss2.str_fail());
    };

    expect_failed.operator()<istream>();
    expect_failed.operator()<iostream>();
}

namespace
{
// wchar_t counterpart: a bare abs_flusher tie target whose flush fails. try_flush() is
// noexcept by contract, so the failure is absorbed here and recorded on the target itself.
struct ThrowingTieW : public abs_flusher
{
    int  flushed = 0;
    bool failed  = false;

    void try_flush() noexcept override
    {
        ++flushed;
        try { throw stream_error("tied flush boom"); }
        catch (...) { failed = true; }
    }
};
}

// wchar_t counterpart: the input sentry flushes the tied stream before locking; a failing
// flush stays on the target and extraction still succeeds.
TEST(IstreamWsWchar, AFailingTiedFlushStaysOnTheTiedStream)
{
    auto expect_absorbed = []<template <typename, typename> class T>()
    {
        ThrowingTieW tt;
        T iss{mem_device{std::wstring(L"42 rest")}, locale<wchar_t>("C")};
        iss.tie(&tt);

        int v = 0;
        iss >> v;
        EXPECT_GE(tt.flushed, 1);
        EXPECT_TRUE(tt.failed);       // the failure was recorded on the target
        EXPECT_EQ(v, 42);
        EXPECT_TRUE(iss.good());      // and not on the initiator

        iss.tie(nullptr);
    };

    expect_absorbed.operator()<istream>();
    expect_absorbed.operator()<iostream>();
}

namespace
{
using DirIn_w   = istream<mem_device<wchar_t>, wchar_t>;
using DirOut_w  = ostream<mem_device<wchar_t>, wchar_t>;
using DirBoth_w = iostream<mem_device<wchar_t>, wchar_t>;

// See the char counterpart for why the io_traits members are probed directly rather than
// through `s >> m` / `s << m`.
template <typename S, typename M>
concept can_extract_w = requires (S& s, const M& m)
{ io_traits<typename S::char_type, M>::sread(s, m); };

template <typename S, typename M>
concept can_insert_w = requires (S& s, const M& m)
{ io_traits<typename S::char_type, M>::swrite(s, m); };

// wchar_t counterpart of the char direction matrix. Which io_traits member exists is
// character-type agnostic, so the wrong direction has to be rejected here too.
static_assert(  can_extract_w<DirBoth_w, ws_t>   && !can_insert_w<DirBoth_w, ws_t> );
static_assert(  can_insert_w<DirBoth_w, endl_t>  && !can_extract_w<DirBoth_w, endl_t> );
static_assert(  can_insert_w<DirBoth_w, ends_t>  && !can_extract_w<DirBoth_w, ends_t> );
static_assert(  can_insert_w<DirBoth_w, flush_t> && !can_extract_w<DirBoth_w, flush_t> );

static_assert(  can_extract_w<DirIn_w, ws_t>     && !can_insert_w<DirIn_w, ws_t> );
static_assert( !can_extract_w<DirIn_w, endl_t>   && !can_insert_w<DirIn_w, endl_t> );
static_assert(  can_insert_w<DirOut_w, endl_t>   && !can_extract_w<DirOut_w, endl_t> );
static_assert( !can_extract_w<DirOut_w, ws_t>    && !can_insert_w<DirOut_w, ws_t> );

// setfill is the one manipulator whose character type must match the stream's exactly. That
// requirement used to live in the parameter type setfill_t<typename T::char_type>; it is now a
// requires-clause on both io_traits members, so a mismatch is rejected at the declaration
// rather than inside the body.
static_assert(  can_insert_w<DirBoth_w, setfill_t<wchar_t>>
             && can_extract_w<DirBoth_w, setfill_t<wchar_t>> );
static_assert( !can_insert_w<DirBoth_w, setfill_t<char>>
            && !can_extract_w<DirBoth_w, setfill_t<char>> );
}

// wchar_t counterpart of the runtime direction check.
TEST(IstreamWsWchar, TheSurvivingDirectionStillRunsOnABidirectionalStream)
{
    DirBoth_w iss{mem_device{std::wstring(L"  token")}};
    iss >> ws;
    std::wstring tok;
    iss >> tok;
    EXPECT_EQ(tok, L"token");
}
