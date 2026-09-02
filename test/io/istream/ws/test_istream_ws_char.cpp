// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The ws manipulator, and around it the rules that decide which manipulators an
 * istream<char> will accept at all.
 *
 * ws is the whole of unformatted whitespace skipping: it discards characters
 * for as long as the locale calls them whitespace and stops on the first one it
 * does not, or on the end of the input -- which is an end, not a failure, so
 * eofbit is set and the stream stays usable-looking. That is the one part of ws
 * a caller can get wrong, because every other extraction that skips whitespace
 * does fail when it finds nothing after it.
 *
 * The rest of the file is about dispatch rather than about ws itself. IOv2
 * decides a manipulator's direction by which io_traits member exists, not by a
 * constraint on the stream, because a bidirectional stream satisfies both
 * constraints and would accept a manipulator through the wrong operator. The
 * static_asserts at the bottom are that rule stated as a matrix; the function
 * pointer cases are the one manipulator shape that goes around io_traits
 * entirely and so has to be checked by hand.
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
#include <io/utilities/istream_operators.h>
#include <io/utilities/ostream_operators.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>

using namespace IOv2;

// Every character the "C" locale classifies as whitespace, so that "whitespace"
// is not silently narrowed to the space character.
TEST(IstreamWsChar, WsDiscardsEveryWhitespaceCharacterAndStopsOnTheFirstOther)
{
    auto expect_skipped = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::string(" \t\n\v\f\r x")}};

        is >> ws;
        EXPECT_EQ(is.peek(), 'x');
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
    };

    expect_skipped.operator()<istream>();
    expect_skipped.operator()<iostream>();
}

// Nothing to skip is not an error and not a no-op worth reporting: the position
// and the state both stay where they were.
TEST(IstreamWsChar, WsOnANonWhitespaceCharacterChangesNothing)
{
    auto expect_unchanged = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::string("abc")}};

        const ios_defs::iostate before = is.rdstate();
        is >> ws;
        EXPECT_EQ(is.rdstate(), before);
        EXPECT_EQ(is.peek(), 'a');

        // Repeating it is still nothing.
        is >> ws >> ws;
        EXPECT_EQ(is.peek(), 'a');
    };

    expect_unchanged.operator()<istream>();
    expect_unchanged.operator()<iostream>();
}

// Running off the end while skipping is the end of the input, which ws reports
// with eofbit alone. It does not fail -- unlike a formatted extraction, which
// skips whitespace only in order to find something after it and fails when it
// does not. A caller that checks the stream after ws must therefore look at
// eof(), not at the boolean conversion.
TEST(IstreamWsChar, WsAtTheEndSetsEndOfFileWithoutFailing)
{
    auto expect_end = []<template <typename, typename> class T>()
    {
        {
            T all_space{mem_device{std::string("   \t\n")}};
            all_space >> ws;
            EXPECT_TRUE(all_space.eof());
            EXPECT_FALSE(all_space.rdstate() & ios_defs::strfailbit);
            EXPECT_TRUE(static_cast<bool>(all_space));
        }
        {
            // Nothing at all is the same answer.
            T empty{mem_device{std::string("")}};
            empty >> ws;
            EXPECT_TRUE(empty.eof());
            EXPECT_FALSE(empty.rdstate() & ios_defs::strfailbit);
        }
        {
            // Whereas an extraction over the same input fails, because it needed
            // a value and the whitespace was only in the way.
            T all_space{mem_device{std::string("   \t\n")}, locale<char>("C")};
            int v = 0;
            all_space >> v;
            EXPECT_TRUE(all_space.rdstate() & ios_defs::strfailbit);
        }
    };

    expect_end.operator()<istream>();
    expect_end.operator()<iostream>();
}

// With skipws off an extraction stops dead on leading whitespace; ws is then
// the caller's way to get past it, which is what the manipulator is for.
TEST(IstreamWsChar, WsSkipsWhereExtractionWillNotWhenSkipwsIsOff)
{
    auto expect_manual = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::string("  42  7")}, locale<char>("C")};
        is.unsetf(ios_defs::skipws);

        int v = 0;
        is >> v;
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
        is.clear();

        is >> ws >> v;
        EXPECT_EQ(v, 42);

        // And again for the next one, since skipws is still off.
        is >> ws >> v;
        EXPECT_EQ(v, 7);
    };

    expect_manual.operator()<istream>();
    expect_manual.operator()<iostream>();
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
TEST(IstreamWsChar, AFunctionPointerManipulatorBeatsTheGenericExtraction)
{
    auto expect_dispatched = []<template <typename, typename> class T>()
    {
        T iss{mem_device{std::string("hello world")}};

        static int calls;
        calls = 0;
        void (*manip)(ios_base<char>&) = [](ios_base<char>&) { ++calls; };

        iss >> manip;                 // a non-const lvalue must still reach this overload
        EXPECT_EQ(calls, 1);

        iss >> manip >> manip;        // operator>> returns the stream, so manipulators chain
        EXPECT_EQ(calls, 3);

        // a capture-less lambda reaches the same overload once decayed with unary +
        iss >> +[](ios_base<char>&) { ++calls; };
        EXPECT_EQ(calls, 4);

        // the generic value operator>> still extracts real values afterwards
        std::string tok;
        iss >> tok;
        EXPECT_EQ(tok, "hello");
    };

    expect_dispatched.operator()<istream>();
    expect_dispatched.operator()<iostream>();
}

// A null function-pointer manipulator passed to operator>> must be rejected: the operator
// throws stream_error, its own handler categorizes it into strfailbit, and -- with no
// exception mask set -- returns the stream without throwing. The tag-object manipulators
// need no such branch: an object cannot be null.
TEST(IstreamWsChar, ANullFunctionPointerManipulatorIsRejected)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        T iss{mem_device{std::string("hello world")}};

        // overload: operator>>(T&, void(*)(ios_base<char>&))
        EXPECT_NO_THROW(iss >> static_cast<void (*)(ios_base<char>&)>(nullptr));
        EXPECT_TRUE(iss.rdstate() & ios_defs::strfailbit);
        iss.clear();

        // same overload, non-null: the callable runs against the stream's ios_base
        static int base_calls;
        base_calls = 0;
        iss >> +[](ios_base<char>&) { ++base_calls; };
        EXPECT_EQ(base_calls, 1);
        EXPECT_FALSE(iss.rdstate() & ios_defs::strfailbit);

        // stream is still usable after the rejected manipulator
        std::string tok;
        iss >> tok;
        EXPECT_EQ(tok, "hello");
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

// The input sentry classifies leading whitespace through the ctype facet. With that facet
// removed from the locale, the sentry's skip step throws stream_error ("no ctype facet"),
// which is reported as strfailbit; the follow-up validity check inside the sentry then
// re-throws on the now-failed stream. Both the ws manipulator and skipws-enabled
// extraction drive this path. With no exception mask set nothing escapes to the caller.
TEST(IstreamWsChar, TheSentryFailsWhenTheLocaleHasNoCtypeFacet)
{
    auto expect_failed = []<template <typename, typename> class T>()
    {
        const auto loc = locale<char>("C").remove<ctype_conf<char>>();

        // ws manipulator: sentry constructed with noskip == false -> needs the ctype facet
        T iss{mem_device{std::string("  42")}, loc};
        EXPECT_NO_THROW(iss >> ws);
        EXPECT_TRUE(iss.str_fail());

        // skipws-enabled extraction takes the same sentry skip path
        T iss2{mem_device{std::string("  42")}, loc};
        int v = 0;
        EXPECT_NO_THROW(iss2 >> v);
        EXPECT_TRUE(iss2.str_fail());
    };

    expect_failed.operator()<istream>();
    expect_failed.operator()<iostream>();
}

namespace
{
// A bare abs_flusher tie target whose flush fails, to exercise the input sentry's pre-lock
// "flush the tied stream" step. try_flush() is noexcept by contract, so the failure is
// absorbed here and recorded on the target itself -- mirroring what out_flusher<T> does with
// handle_exception<true>(). It must never reach the initiating stream, which would otherwise
// have the target's failure misattributed to it.
struct ThrowingTie : public abs_flusher
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

// The contract itself: a tie flush can never throw into the sentry.
static_assert(noexcept(std::declval<abs_flusher&>().try_flush()));
}

// An istream can have a tied stream; the input sentry flushes it before acquiring the lock.
// When that flush fails, the failure stays on the target and extraction proceeds normally.
TEST(IstreamWsChar, AFailingTiedFlushStaysOnTheTiedStream)
{
    auto expect_absorbed = []<template <typename, typename> class T>()
    {
        ThrowingTie tt;
        T iss{mem_device{std::string("42 rest")}, locale<char>("C")};
        iss.tie(&tt);

        int v = 0;
        iss >> v;                     // sentry flushes tt -> fails -> absorbed by the target
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
using DirIn_c   = istream<mem_device<char>, char>;
using DirOut_c  = ostream<mem_device<char>, char>;
using DirBoth_c = iostream<mem_device<char>, char>;

// These probe the stream form of io_traits directly -- the rung the insertion / extraction
// operators try first for a manipulator. `requires { s >> m; }` would answer a broader question:
// it also folds in the operator's other rungs and the function-pointer manipulator overload, so a
// failure would no longer point at the stream form itself.
template <typename S, typename M>
concept can_extract = requires (S& s, const M& m)
{ io_traits<typename S::char_type, M>::sread(s, m); };

template <typename S, typename M>
concept can_insert = requires (S& s, const M& m)
{ io_traits<typename S::char_type, M>::swrite(s, m); };

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
static_assert(  can_extract<DirBoth_c, ws_t>    && !can_insert<DirBoth_c, ws_t> );
static_assert(  can_insert<DirBoth_c, endl_t>   && !can_extract<DirBoth_c, endl_t> );
static_assert(  can_insert<DirBoth_c, ends_t>   && !can_extract<DirBoth_c, ends_t> );
static_assert(  can_insert<DirBoth_c, flush_t>  && !can_extract<DirBoth_c, flush_t> );

// Unidirectional streams keep their own direction and gain nothing in the other one.
static_assert(  can_extract<DirIn_c, ws_t>      && !can_insert<DirIn_c, ws_t> );
static_assert( !can_extract<DirIn_c, endl_t>    && !can_insert<DirIn_c, endl_t> );
static_assert(  can_insert<DirOut_c, endl_t>    && !can_extract<DirOut_c, endl_t> );
static_assert( !can_extract<DirOut_c, ws_t>     && !can_insert<DirOut_c, ws_t> );

// Manipulators tagged both ways stay usable both ways.
static_assert(  can_insert<DirBoth_c, both_manip>  &&  can_extract<DirBoth_c, both_manip> );
static_assert(  can_insert<DirBoth_c, setw_t>      &&  can_extract<DirBoth_c, setw_t> );
static_assert(  can_insert<DirBoth_c, setfill_t<char>>
             && can_extract<DirBoth_c, setfill_t<char>> );

// A setfill_t whose character type does not match the stream is rejected in both directions:
// io_traits carries the same_as constraint on its members precisely so this stays visible to a
// requires-expression rather than erroring inside the body.
static_assert( !can_insert<DirBoth_c, setfill_t<wchar_t>>
            && !can_extract<DirBoth_c, setfill_t<wchar_t>> );
}

// Runtime companion to the direction static_asserts above: the surviving direction still works
// on a bidirectional stream.
TEST(IstreamWsChar, TheSurvivingDirectionStillRunsOnABidirectionalStream)
{
    DirBoth_c iss{mem_device{std::string("  token")}};
    iss >> ws;
    std::string tok;
    iss >> tok;
    EXPECT_EQ(tok, "token");

    // A manipulator tagged both ways runs from either operator.
    int       calls = 0;
    DirBoth_c iss3{mem_device{std::string("x")}};
    iss3 >> counting_manip{&calls};
    iss3 << counting_manip{&calls};
    EXPECT_EQ(calls, 2);
}
