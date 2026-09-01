/**
 * The tie edge on an output stream.
 *
 * A tie is a node's outgoing edge in a process-wide graph, and the whole point
 * of the tests here is what happens at the edges of that model: the edge is
 * bound to object identity so it must not ride along on a copy or a move, and a
 * cycle in the graph would make a flush walk forever while holding the graph
 * lock, so cycles have to be impossible to create by any route -- including the
 * routes that do not go through tie() at all.
 */
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>

#include <gtest/gtest.h>

#include <utility>

namespace
{
    template <template<typename, typename> class T>
    T<IOv2::mem_device<char>, char> make()
    {
        return T<IOv2::mem_device<char>, char>{IOv2::mem_device{""}, IOv2::locale<char>("C")};
    }

    IOv2::abs_flusher* as_flusher(auto& s) { return static_cast<IOv2::abs_flusher*>(&s); }
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
            src.tie(as_flusher(target));
            EXPECT_EQ(src.tie(), as_flusher(target));
            auto dst = src;                              // NOLINT(performance-unnecessary-copy-initialization)
            EXPECT_EQ(dst.tie(), nullptr);
            EXPECT_EQ(src.tie(), as_flusher(target));     // a copy leaves the source alone
        }
        // move construction: destination null, source cleared
        {
            auto src = make<T>();
            src.tie(as_flusher(target));
            auto dst = std::move(src);
            EXPECT_EQ(dst.tie(), nullptr);
            EXPECT_EQ(src.tie(), nullptr);                // NOLINT(bugprone-use-after-move)
        }
        // copy assignment
        {
            auto src = make<T>();
            auto dst = make<T>();
            src.tie(as_flusher(target));
            dst.tie(as_flusher(target));
            dst = src;
            EXPECT_EQ(dst.tie(), nullptr);
            EXPECT_EQ(src.tie(), as_flusher(target));
        }
        // move assignment: destination null, source cleared
        {
            auto src = make<T>();
            auto dst = make<T>();
            src.tie(as_flusher(target));
            dst.tie(as_flusher(target));
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

        b.tie(as_flusher(c));      // b -> c
        c.tie(as_flusher(a));      // c -> a

        a = b;                     // used to make a -> c, closing a -> c -> a
        EXPECT_EQ(a.tie(), nullptr);

        d.tie(as_flusher(a));      // used to spin forever walking the cycle
        EXPECT_EQ(d.tie(), as_flusher(a));

        // Same story for move assignment.
        auto e = make<T>();
        auto f = make<T>();
        auto g = make<T>();
        f.tie(as_flusher(g));
        g.tie(as_flusher(e));
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
        a.tie(as_flusher(a));
        EXPECT_TRUE(a.rdstate() & IOv2::ios_defs::strfailbit);
        EXPECT_FALSE(static_cast<bool>(a));
        EXPECT_EQ(a.tie(), nullptr);
        a.clear();

        // a -> b -> c, then c -> a would close the loop
        a.tie(as_flusher(b));
        b.tie(as_flusher(c));

        c.tie(as_flusher(a));
        EXPECT_TRUE(c.rdstate() & IOv2::ios_defs::strfailbit);
        EXPECT_EQ(c.tie(), nullptr);      // the existing tie is left unchanged
        c.clear();

        // the legal chain is untouched
        EXPECT_EQ(a.tie(), as_flusher(b));
        EXPECT_EQ(b.tie(), as_flusher(c));

        // with strfailbit in the mask the rejection throws, and still commits nothing
        c.exceptions(IOv2::ios_defs::strfailbit);
        EXPECT_THROW(c.tie(as_flusher(a)), IOv2::stream_error);
        EXPECT_EQ(c.tie(), nullptr);
        c.exceptions(IOv2::ios_defs::goodbit);
        c.clear();

        // a legal tie still works and returns the pointer stored before the call
        EXPECT_EQ(a.tie(nullptr), as_flusher(b));
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
        a.tie(as_flusher(target));

        // Aliased through a reference to keep -Wself-move / -Wself-assign-overloaded quiet.
        auto& alias = a;

        a = std::move(alias);
        EXPECT_EQ(a.tie(), as_flusher(target));

        a = alias;
        EXPECT_EQ(a.tie(), as_flusher(target));

        a.tie(nullptr);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
    helper.operator()<IOv2::istream>();

}
