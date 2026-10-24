// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The tie edge on an output stream.
 *
 * A tie is a node's outgoing edge in a process-wide graph, and the whole point
 * of the tests here is what happens at the edges of that model: the edge is
 * bound to object identity so it must not ride along on a copy or a move, and a
 * cycle in the graph would make a flush walk forever while holding the graph
 * lock, so cycles have to be impossible to create by any route -- including the
 * routes that do not go through tie() at all.
 *
 * A tie flush follows the whole chain, furthest link first, and each link is
 * best-effort on its own: a link that is locked or failed is skipped without
 * stopping the flush from reaching the links beyond it.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/io/utilities/ostream_operators.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <support/failing_device.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <utility>

namespace
{
    template <template<typename, typename> class T>
    T<IOv2::mem_device<char>, char> make(const std::string& init = "")
    {
        return T<IOv2::mem_device<char>, char>{IOv2::mem_device{init}, IOv2::locale<char>("C")};
    }

    IOv2::tie_target* as_tie_target(auto& s) { return static_cast<IOv2::tie_target*>(&s); }

    // A stream whose dflush() throws, so that a flush reaching it shows up as dev_fail().
    using failing_ostream = IOv2::ostream<failing_device<char>, char>;

    failing_ostream make_failing()
    {
        return failing_ostream{failing_device<char>{std::string(""), true}, IOv2::locale<char>("C")};
    }

    // The end of a chain: counts its flushes and runs a probe at each one. Test double only;
    // users must not derive from tie_target (see its @warning).
    struct ProbeFlusher : public IOv2::tie_target
    {
        int flushed = 0;
        std::function<void()> probe;

        void try_flush() noexcept override
        {
            ++flushed;
            if (probe)
                probe();
        }
    };

    // One I/O on s in whichever direction it supports, so that its sentry runs the tie flush.
    void drive(auto& s)
    {
        if constexpr (requires { s.put('x'); })
            s.put('x');
        else
            s.get();
    }
}

// Every special member resets the destination's tie to null, and a move clears the source's
// as well.
TEST(OstreamTie, CopyAndMoveDoNotCarryTheTieEdge)
{
    auto helper = []<template<typename, typename> class T>()
    {
        auto target = make<IOv2::ostream>();

        // copy construction
        {
            auto src = make<T>();
            src.tie(as_tie_target(target));
            EXPECT_EQ(src.tie(), as_tie_target(target));
            auto dst = src;                              // NOLINT(performance-unnecessary-copy-initialization)
            EXPECT_EQ(dst.tie(), nullptr);
            EXPECT_EQ(src.tie(), as_tie_target(target));  // a copy leaves the source alone
        }
        // move construction: destination null, source cleared
        {
            auto src = make<T>();
            src.tie(as_tie_target(target));
            auto dst = std::move(src);
            EXPECT_EQ(dst.tie(), nullptr);
            EXPECT_EQ(src.tie(), nullptr);                // NOLINT(bugprone-use-after-move)
        }
        // copy assignment
        {
            auto src = make<T>();
            auto dst = make<T>();
            src.tie(as_tie_target(target));
            dst.tie(as_tie_target(target));
            dst = src;
            EXPECT_EQ(dst.tie(), nullptr);
            EXPECT_EQ(src.tie(), as_tie_target(target));
        }
        // move assignment: destination null, source cleared
        {
            auto src = make<T>();
            auto dst = make<T>();
            src.tie(as_tie_target(target));
            dst.tie(as_tie_target(target));
            dst = std::move(src);
            EXPECT_EQ(dst.tie(), nullptr);
            EXPECT_EQ(src.tie(), nullptr);                // NOLINT(bugprone-use-after-move)
        }
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
    helper.operator()<IOv2::istream>();   // a pure input stream can be tied, just not tied to

}

// Regression test for the defect this behavior exists to prevent. Assignment used to carry the
// source's tie edge straight into the destination, bypassing tie()'s cycle check entirely; the
// sequence below closed the cycle a->c->a without a single tie() call, after which any tie()
// anywhere in the process spun forever while holding the process-wide tie graph lock.
TEST(OstreamTie, AssignmentCannotSmuggleInACycle)
{
    auto helper = []<template<typename, typename> class T>()
    {
        auto a = make<T>();
        auto b = make<T>();
        auto c = make<T>();
        auto d = make<T>();

        b.tie(as_tie_target(c));   // b -> c
        c.tie(as_tie_target(a));   // c -> a

        a = b;                     // used to make a -> c, closing a -> c -> a
        EXPECT_EQ(a.tie(), nullptr);

        d.tie(as_tie_target(a));   // used to spin forever walking the cycle
        EXPECT_EQ(d.tie(), as_tie_target(a));

        // Same story for move assignment.
        auto e = make<T>();
        auto f = make<T>();
        auto g = make<T>();
        f.tie(as_tie_target(g));
        g.tie(as_tie_target(e));
        e = std::move(f);
        EXPECT_EQ(e.tie(), nullptr);

        d.tie(nullptr);
        g.tie(nullptr);
        c.tie(nullptr);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

}

// tie() still rejects a request that would form a cycle, and leaves the existing tie untouched
// when it does. A self-tie is the length-1 case. The rejection is reported the way every other
// failure in this library is: strfailbit, and a throw only when that bit is in the mask.
TEST(OstreamTie, CycleRequestsAreRejectedAndCommitNothing)
{
    auto helper = []<template<typename, typename> class T>()
    {
        auto a = make<T>();
        auto b = make<T>();
        auto c = make<T>();

        // self-tie: a cycle of length 1. The default mask is empty, so nothing is thrown.
        a.tie(as_tie_target(a));
        EXPECT_TRUE(a.rdstate() & IOv2::ios_defs::strfailbit);
        EXPECT_FALSE(static_cast<bool>(a));
        EXPECT_EQ(a.tie(), nullptr);
        a.clear();

        // a -> b -> c, then c -> a would close the loop
        a.tie(as_tie_target(b));
        b.tie(as_tie_target(c));

        c.tie(as_tie_target(a));
        EXPECT_TRUE(c.rdstate() & IOv2::ios_defs::strfailbit);
        EXPECT_EQ(c.tie(), nullptr);      // the existing tie is left unchanged
        c.clear();

        // the legal chain is untouched
        EXPECT_EQ(a.tie(), as_tie_target(b));
        EXPECT_EQ(b.tie(), as_tie_target(c));

        // with strfailbit in the mask the rejection throws, and still commits nothing
        c.exceptions(IOv2::ios_defs::strfailbit);
        EXPECT_THROW(c.tie(as_tie_target(a)), IOv2::stream_error);
        EXPECT_EQ(c.tie(), nullptr);
        c.exceptions(IOv2::ios_defs::goodbit);
        c.clear();

        // a legal tie still works and returns the pointer stored before the call
        EXPECT_EQ(a.tie(nullptr), as_tie_target(b));
        EXPECT_EQ(a.tie(), nullptr);
        b.tie(nullptr);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

}

// Self-assignment is short-circuited by the concrete stream classes, so it never reaches the
// mix-in's operator= and the tie edge survives -- unlike every other copy or move. Dropping one
// of those short-circuits would silently start untying on self-assignment, so pin the behavior.
TEST(OstreamTie, SelfAssignmentKeepsTheTieEdge)
{
    auto helper = []<template<typename, typename> class T>()
    {
        auto target = make<IOv2::ostream>();

        auto a = make<T>();
        a.tie(as_tie_target(target));

        // Aliased through a reference to keep -Wself-move / -Wself-assign-overloaded quiet.
        auto& alias = a;

        a = std::move(alias);
        EXPECT_EQ(a.tie(), as_tie_target(target));

        a = alias;
        EXPECT_EQ(a.tie(), as_tie_target(target));

        a.tie(nullptr);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
    helper.operator()<IOv2::istream>();

}

// A tie flush belongs to the target. Its exception mask cannot throw into the
// unrelated initiating stream, so the target records the original failure and
// the initiator proceeds normally.
TEST(OstreamTie, AFailedTieFlushIsRecordedOnlyOnTheTarget)
{
    IOv2::ostream<failing_device<char>, char> target(
        failing_device<char>{std::string(""), true}, IOv2::locale<char>("C"));
    auto writer = make<IOv2::ostream>();

    target.exceptions(IOv2::ios_defs::devfailbit);
    writer.tie(as_tie_target(target));
    EXPECT_NO_THROW(writer.put('x'));

    EXPECT_TRUE(writer.good());
    EXPECT_TRUE(target.dev_fail());

    writer.tie(nullptr);
    target.exceptions(IOv2::ios_defs::goodbit);
    target.clear();
}

// The hidden tie edge is best-effort: if another thread owns the target's I/O
// lock, the initiator tries briefly and then continues instead of waiting.
TEST(OstreamTie, ATieFlushDoesNotWaitForTheTargetsLock)
{
    auto target = make<IOv2::ostream>();
    auto writer = make<IOv2::ostream>();
    writer.tie(as_tie_target(target));

    std::atomic<bool> locked{false};
    std::atomic<bool> release{false};
    std::thread holder([&]
    {
        IOv2::sync guard(target);
        locked.store(true);
        while (!release.load())
            std::this_thread::yield();
    });

    while (!locked.load())
        std::this_thread::yield();

    writer.put('x');
    EXPECT_TRUE(writer.good());

    release.store(true);
    holder.join();
    writer.tie(nullptr);
}

// A tie is chained: an I/O flushes its target, the target's target and so on, furthest link
// first -- the order "flush the tie before flushing" gives at every link. A failure on a link
// stays on that link.
TEST(OstreamTie, AnIoFlushesTheWholeChainFurthestLinkFirst)
{
    auto helper = []<template<typename, typename> class T>()
    {
        auto b = make_failing();
        ProbeFlusher c;
        bool b_flushed_first = false;
        c.probe = [&] { b_flushed_first = b.dev_fail(); };

        auto a = make<T>("y");          // input for the istream initiator
        a.tie(as_tie_target(b));
        b.tie(&c);

        drive(a);
        EXPECT_EQ(c.flushed, 1);
        EXPECT_FALSE(b_flushed_first);
        EXPECT_TRUE(b.dev_fail());      // b was flushed, after c
        EXPECT_TRUE(a.good());          // and its failure did not reach a

        a.tie(nullptr);
        b.tie(nullptr);
        b.clear();
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
    helper.operator()<IOv2::istream>();
}

// The same with real streams all the way down, so that every link goes through
// out_tie_target::try_flush() and tied_to(): the flush reaches the third stream.
TEST(OstreamTie, AFlushCrossesEveryRealStreamOnTheChain)
{
    auto a = make<IOv2::ostream>();
    auto b = make<IOv2::ostream>();
    auto c = make_failing();
    a.tie(as_tie_target(b));
    b.tie(as_tie_target(c));

    a.put('x');
    EXPECT_TRUE(c.dev_fail());
    EXPECT_TRUE(a.good());
    EXPECT_TRUE(b.good());

    a.tie(nullptr);
    b.tie(nullptr);
    c.clear();
}

// Each link is best-effort on its own: a link whose lock another thread holds is skipped, and the
// flush still reaches the links beyond it.
TEST(OstreamTie, ALockedLinkIsSkippedAndTheLinksBeyondItAreStillFlushed)
{
    auto a = make<IOv2::ostream>();
    auto b = make_failing();
    ProbeFlusher c;
    a.tie(as_tie_target(b));
    b.tie(&c);

    std::atomic<bool> locked{false};
    std::atomic<bool> release{false};
    std::thread holder([&]
    {
        IOv2::sync guard(b);
        locked.store(true);
        while (!release.load())
            std::this_thread::yield();
    });

    while (!locked.load())
        std::this_thread::yield();

    a.put('x');
    EXPECT_EQ(c.flushed, 1);
    EXPECT_FALSE(b.dev_fail());         // skipped, not attempted

    release.store(true);
    holder.join();

    a.put('x');                         // control: with the lock free, b is flushed too
    EXPECT_EQ(c.flushed, 2);
    EXPECT_TRUE(b.dev_fail());

    a.tie(nullptr);
    b.tie(nullptr);
    b.clear();
}

// A link in a failed state refuses its own flush, as flush() always does on a failed stream, but
// still passes the flush on down the chain.
TEST(OstreamTie, AFailedLinkStillPassesTheFlushOn)
{
    auto a = make<IOv2::ostream>();
    auto b = make<IOv2::ostream>();
    ProbeFlusher c;
    a.tie(as_tie_target(b));
    b.tie(&c);

    b.setstate(IOv2::ios_defs::devfailbit);
    a.put('x');
    EXPECT_EQ(c.flushed, 1);
    EXPECT_TRUE(b.rdstate() & IOv2::ios_defs::strfailbit);   // b's own flush was refused
    EXPECT_TRUE(a.good());

    a.tie(nullptr);
    b.tie(nullptr);
    b.clear();
}
