// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Deriving from ostream, and the concept that says what a derived type must
 * still be.
 *
 * Two things have to survive derivation. The inherited operator<< must return
 * the derived type, or a chain falls back to the base after the first
 * insertion; and a derived operator<< must be able to sit alongside the
 * inherited ones without hiding them.
 *
 * The rest of the file is about the boundaries of ostream_type: what a type has
 * to carry to satisfy it, and the two shapes that must not.
 */
#include <common/defs.h>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/iostream.h>
#include <io/ostream.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>
#include <io/traits/char_and_str.h>
#include <io/utilities/ostream_operators.h>
#include <io/utilities/stream_common_operators.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace
{
struct DevOstream : public IOv2::ostream<IOv2::mem_device<char>, char>
{
    int x = 20;
};

struct DevIOstream : public IOv2::ostream<IOv2::mem_device<char>, char>
{
    int x = 50;
};
}

TEST(OstreamDerive, InsertionReturnsTheDerivedTypeNotTheBase)
{
    DevOstream obj1;
    // make sure the << operator should return DevOstream object
    EXPECT_EQ((obj1 << "hello").x, 20);

    DevIOstream obj2;
    // make sure the << operator should return DevIOstream object
    EXPECT_EQ((obj2 << "hello").x, 50);
}

namespace
{
struct Level {
    std::string val;
};

class MyLogger : public IOv2::ostream<IOv2::mem_device<char>, char>
{
public:
    using IOv2::ostream<IOv2::mem_device<char>, char>::ostream;

    MyLogger& operator<<(const Level& l) {
        *this << "[" << l.val << "] ";
        return *this;
    }
};
}

// A derived operator<< taking its own type has to coexist with the inherited
// ones, so a chain can alternate between them.
TEST(OstreamDerive, ADerivedInserterChainsWithTheInheritedOnes)
{
    MyLogger logger;
    logger << Level{"DEBUG"} << "User login\n"
           << Level{"WARN"} << "something happened";

    auto [dev, err] = logger.detach();
    EXPECT_EQ(dev.str(), "[DEBUG] User login\n[WARN] something happened");
}

namespace
{
// A tie target that is a bare abs_flusher, not a stream_common_operators. Used to drive two
// otherwise-hard-to-reach branches:
//   * ThrowingFlusher's flush fails. try_flush() is noexcept by contract, so the failure is
//     absorbed by the target itself -- mirroring what out_flusher<T> does with
//     handle_exception<true>() -- and must never reach the initiating stream.
//   * a bare abs_flusher makes tie()'s cycle-detection walk dynamic_cast to
//     stream_common_operators* -> null -> break (the non-stream node case).
struct ThrowingFlusher : public IOv2::abs_flusher
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

struct QuietFlusher : public IOv2::abs_flusher
{
    int flushed = 0;
    void try_flush() noexcept override { ++flushed; }
};

// The contract itself: a tie flush can never throw into the sentry.
static_assert(noexcept(std::declval<IOv2::abs_flusher&>().try_flush()));
}

// Tie an ostream to a bare abs_flusher and drive output. Two effects are checked:
//   * the sentry flushes the tied target before locking; when that flush fails, the failure
//     stays on the target and the insertion still succeeds (ThrowingFlusher case).
//   * tie() accepts a non-stream flusher node: its cycle-detection walk dynamic_casts the
//     target to stream_common_operators*, gets null, and breaks (both cases reach it).
TEST(OstreamDerive, ATiedBareFlusherIsFlushedAndItsFailureStaysThere)
{
    auto helper = []<template<typename, typename> class T>()
    {
        {
            ThrowingFlusher tf;
            T oss{IOv2::mem_device{std::string("")}};
            oss.tie(&tf);                 // non-stream node -> cycle walk breaks
            oss << "x";                   // sentry flushes tf -> fails -> absorbed by the target
            EXPECT_GE( tf.flushed, 1 );
            EXPECT_TRUE( tf.failed );     // the failure was recorded on the target
            EXPECT_TRUE( oss.good() );    // and must not fail the initiating stream
            oss.tie(nullptr);
            auto [dev, err] = oss.detach();
            EXPECT_EQ( dev.str(), "x" );
        }
        {
            QuietFlusher qf;
            T oss{IOv2::mem_device{std::string("")}};
            oss.tie(&qf);
            oss << "y";                   // tied flush succeeds
            EXPECT_GE( qf.flushed, 1 );
            EXPECT_TRUE( oss.good() );
            oss.tie(nullptr);
        }
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
}

namespace
{
// ostream_type requires ios_state, because code constrained by the concept calls
// handle_exception() and operator bool directly (out_sentry's destructor). Without the clause
// those calls only fail when the template body is instantiated, which bypasses the fallback
// overload's short diagnostic -- the operators that consume manipulators only check callability
// with std::invocable, a declaration-level check.
struct StatelessOs : IOv2::ios_base<char>
                   , IOv2::stream_common_operators
                   , IOv2::ostream_operators<char>
{
    using char_type = char;
    using out_sentry_type = IOv2::out_sentry<StatelessOs, false>;
    // ostream_type also demands the iterator type: o_iter() is private, so the insertion
    // concepts have only this alias to probe with. Nothing here can actually do I/O, so any
    // well-formed ostreambuf_iterator will do.
    using out_iter_type = IOv2::ostreambuf_iterator<IOv2::ostreambuf<IOv2::mem_device<char>, char>>;
    IOv2::locale<char> m_locale;
};

// The same thing with the state component: ios_state<char> derives from
// ios_base<char>, so it replaces that base rather than joining it. Everything else is
// unchanged, so this pair isolates the clause.
struct StatefulOs : IOv2::ios_state<char>
                  , IOv2::stream_common_operators
                  , IOv2::ostream_operators<char>
{
    using char_type = char;
    using out_sentry_type = IOv2::out_sentry<StatefulOs, false>;
    using out_iter_type = IOv2::ostreambuf_iterator<IOv2::ostreambuf<IOv2::mem_device<char>, char>>;
    IOv2::locale<char> m_locale;
};

// Deriving from ios_base<char> again on top of ios_state<char> gives two ios_base
// subobjects. That is what the concept's now-redundant derived_from<T, ios_base<char_type>>
// clause is there to reject: base ambiguity makes the conversion, and so the clause, false.
// This build would reject the shape one step earlier via -Winaccessible-base; the warning is
// silenced here so the concept itself is what gets tested.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winaccessible-base"
struct DoubleBaseOs : StatefulOs, IOv2::ios_base<char> {};
#pragma GCC diagnostic pop

static_assert( IOv2::ostream_type<IOv2::ostream<IOv2::mem_device<char>, char>> );
static_assert( IOv2::ostream_type<IOv2::iostream<IOv2::mem_device<char>, char>> );
static_assert(!IOv2::ostream_type<StatelessOs> );
static_assert( IOv2::ostream_type<StatefulOs> );
static_assert(!IOv2::ostream_type<DoubleBaseOs> );
}
