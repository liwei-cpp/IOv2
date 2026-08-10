#include <string>
#include <utility>
#include <device/mem_device.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

namespace
{
    template <template<typename, typename> class T>
    T<IOv2::mem_device<char>, char> make()
    {
        return T<IOv2::mem_device<char>, char>{IOv2::mem_device{""}, IOv2::locale<char>("C")};
    }

    IOv2::abs_flusher* as_flusher(auto& s) { return static_cast<IOv2::abs_flusher*>(&s); }
}

// The tie edge is a node's outgoing edge in the tie graph, bound to object identity, so it is
// deliberately not carried across a copy or a move: every special member resets the
// destination's tie to null, and a move clears the source's as well.
void test_ostream_tie_1()
{
    dump_info("Test ostream tie case 1 (copy/move do not carry the tie edge)...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto target = make<IOv2::ostream>();

        // copy construction
        {
            auto src = make<T>();
            src.tie(as_flusher(target));
            VERIFY(src.tie() == as_flusher(target));
            auto dst = src;                              // NOLINT(performance-unnecessary-copy-initialization)
            VERIFY(dst.tie() == nullptr);
            VERIFY(src.tie() == as_flusher(target));     // a copy leaves the source alone
        }
        // move construction: destination null, source cleared
        {
            auto src = make<T>();
            src.tie(as_flusher(target));
            auto dst = std::move(src);
            VERIFY(dst.tie() == nullptr);
            VERIFY(src.tie() == nullptr);                // NOLINT(bugprone-use-after-move)
        }
        // copy assignment
        {
            auto src = make<T>();
            auto dst = make<T>();
            src.tie(as_flusher(target));
            dst.tie(as_flusher(target));
            dst = src;
            VERIFY(dst.tie() == nullptr);
            VERIFY(src.tie() == as_flusher(target));
        }
        // move assignment: destination null, source cleared
        {
            auto src = make<T>();
            auto dst = make<T>();
            src.tie(as_flusher(target));
            dst.tie(as_flusher(target));
            dst = std::move(src);
            VERIFY(dst.tie() == nullptr);
            VERIFY(src.tie() == nullptr);                // NOLINT(bugprone-use-after-move)
        }
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
    helper.operator()<IOv2::istream>();   // a pure input stream can be tied, just not tied to

    dump_info("Done\n");
}

// Regression test for the defect this behavior exists to prevent. Assignment used to carry the
// source's tie edge straight into the destination, bypassing tie()'s cycle check entirely; the
// sequence below closed the cycle a->c->a without a single tie() call, after which any tie()
// anywhere in the process spun forever while holding the process-wide tie graph lock.
void test_ostream_tie_2()
{
    dump_info("Test ostream tie case 2 (assignment cannot smuggle in a cycle)...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto a = make<T>();
        auto b = make<T>();
        auto c = make<T>();
        auto d = make<T>();

        b.tie(as_flusher(c));      // b -> c
        c.tie(as_flusher(a));      // c -> a

        a = b;                     // used to make a -> c, closing a -> c -> a
        VERIFY(a.tie() == nullptr);

        d.tie(as_flusher(a));      // used to spin forever walking the cycle
        VERIFY(d.tie() == as_flusher(a));

        // Same story for move assignment.
        auto e = make<T>();
        auto f = make<T>();
        auto g = make<T>();
        f.tie(as_flusher(g));
        g.tie(as_flusher(e));
        e = std::move(f);
        VERIFY(e.tie() == nullptr);

        d.tie(nullptr);
        g.tie(nullptr);
        c.tie(nullptr);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// tie() still rejects a request that would form a cycle, and leaves the existing tie untouched
// when it does. A self-tie is the length-1 case. The rejection is reported the way every other
// failure in this library is: strfailbit, and a throw only when that bit is in the mask.
void test_ostream_tie_3()
{
    dump_info("Test ostream tie case 3 (cycle requests are rejected)...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto a = make<T>();
        auto b = make<T>();
        auto c = make<T>();

        // self-tie: a cycle of length 1. The default mask is empty, so nothing is thrown.
        a.tie(as_flusher(a));
        VERIFY(a.rdstate() & IOv2::ios_defs::strfailbit);
        VERIFY(!a);
        VERIFY(a.tie() == nullptr);
        a.clear();

        // a -> b -> c, then c -> a would close the loop
        a.tie(as_flusher(b));
        b.tie(as_flusher(c));

        c.tie(as_flusher(a));
        VERIFY(c.rdstate() & IOv2::ios_defs::strfailbit);
        VERIFY(c.tie() == nullptr);      // the existing tie is left unchanged
        c.clear();

        // the legal chain is untouched
        VERIFY(a.tie() == as_flusher(b));
        VERIFY(b.tie() == as_flusher(c));

        // with strfailbit in the mask the rejection throws, and still commits nothing
        c.exceptions(IOv2::ios_defs::strfailbit);
        bool threw = false;
        try { c.tie(as_flusher(a)); }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
        VERIFY(c.tie() == nullptr);
        c.exceptions(IOv2::ios_defs::goodbit);
        c.clear();

        // a legal tie still works and returns the pointer stored before the call
        VERIFY(a.tie(nullptr) == as_flusher(b));
        VERIFY(a.tie() == nullptr);
        b.tie(nullptr);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// Self-assignment is short-circuited by the concrete stream classes, so it never reaches the
// mix-in's operator= and the tie edge survives -- unlike every other copy or move. Dropping one
// of those short-circuits would silently start untying on self-assignment, so pin the behavior.
void test_ostream_tie_4()
{
    dump_info("Test ostream tie case 4 (self-assignment keeps the tie edge)...");

    auto helper = []<template<typename, typename> class T>()
    {
        auto target = make<IOv2::ostream>();

        auto a = make<T>();
        a.tie(as_flusher(target));

        // Aliased through a reference to keep -Wself-move / -Wself-assign-overloaded quiet.
        auto& alias = a;

        a = std::move(alias);
        VERIFY(a.tie() == as_flusher(target));

        a = alias;
        VERIFY(a.tie() == as_flusher(target));

        a.tie(nullptr);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
    helper.operator()<IOv2::istream>();

    dump_info("Done\n");
}

void test_ostream_tie()
{
    test_ostream_tie_1();
    test_ostream_tie_2();
    test_ostream_tie_3();
    test_ostream_tie_4();
}
