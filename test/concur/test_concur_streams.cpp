// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/io/utilities/ostream_operators.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Stream-level concurrency tests. These exist mainly to give ThreadSanitizer
// (the gcc-tsan preset) real concurrent executions to inspect: the stream layer
// promises that a single operation is serialized by io_mutex(), and nothing here
// may report a race.
// Deliberately limited to the stream layer -- devices, converters and most facets
// document themselves as "concurrency is handled at a higher level", so driving them
// concurrently would flag races that are by design out of contract.
namespace
{
    constexpr int kThreads = 4;
    constexpr int kIters   = 300;

    template <typename F>
    void spawn(F f, int n = kThreads)
    {
        std::vector<std::thread> ts;
        for (int i = 0; i < n; ++i) ts.emplace_back([f, i] { f(i); });
        for (auto& t : ts) t.join();
    }
}

TEST(Concur, ConcurrentOutputOnOneOstream)
{
    using namespace IOv2;

    ostream os(mem_device<char>{});
    spawn([&os](int id)
    {
        for (int i = 0; i < kIters; ++i)
        {
            os << "abc" << i << '\n';
            os.put('x');
            os.write("def", 3);
        }
        (void)id;
    });
    EXPECT_TRUE(static_cast<bool>(os));
}

TEST(Concur, ConcurrentFlushAgainstWrites)
{
    using namespace IOv2;

    ostream os(mem_device<char>{});
    spawn([&os](int id)
    {
        for (int i = 0; i < kIters; ++i)
        {
            if (id % 2) os.flush();
            else        os << "payload" << i;
        }
    });
    EXPECT_TRUE(static_cast<bool>(os));
}

TEST(Concur, SentrylessOperationsBesideFormattedIO)
{
    using namespace IOv2;

    // tell/seek/rseek and the locale setter build no sentry, so they carry their own
    // io_mutex() lock; they must not race the formatted I/O running beside them.
    iostream ios(mem_device<char>{std::string(4096, 'z')});
    spawn([&ios](int id)
    {
        for (int i = 0; i < kIters; ++i)
        {
            switch (id % 4)
            {
                case 0: ios << "w" << i;                       break;
                case 1: (void)ios.tell();                      break;
                case 2: ios.seek(0); ios.rseek(0);             break;
                case 3: (void)ios.locale(IOv2::locale<char>{}); break;
            }
        }
    });
}

TEST(Concur, ConcurrentStreamStateAccess)
{
    using namespace IOv2;

    // Regression test: the state bits and the exception_ptr saved per failure category
    // form one invariant updated under m_state_mutex, while rdstate() reads them
    // lock-free. Before that, a plain bool(is)/good()/eof() in one thread raced the
    // clear() that seek() performs in another -- and clear() also resets refcounted
    // exception_ptrs, so the race could corrupt those refcounts rather than merely
    // return a stale bit.
    istream is(mem_device<char>{std::string(64, 'a') + " b c d"});
    spawn([&is](int id)
    {
        volatile bool sink = false;
        for (int i = 0; i < kIters; ++i)
        {
            switch (id % 4)
            {
                case 0: { std::string s; is >> s; is.seek(0); }        break;
                case 1: sink = static_cast<bool>(is);                  break;
                case 2: sink = is.good() || is.eof();                  break;
                case 3: is.clear(); is.setstate(IOv2::ios_defs::eofbit); break;
            }
        }
        (void)sink;
    });
}

TEST(Concur, TieInBothDirectionsNeverFormsACycle)
{
    using namespace IOv2;

    // Concurrent A.tie(B) / B.tie(A): tie_graph_mutex() fuses cycle detection and
    // commit, so a cycle can never form. A rejected request sets strfailbit and must
    // leave the previous tie untouched; with the default empty mask it does not throw,
    // so the streams are cleared each round to keep the I/O case exercising real output.
    ostream a(mem_device<char>{});
    ostream b(mem_device<char>{});
    ostream c(mem_device<char>{});
    abs_flusher* pa = &a;
    abs_flusher* pb = &b;
    abs_flusher* pc = &c;

    spawn([&](int id)
    {
        for (int i = 0; i < kIters; ++i)
        {
            switch (id % 4)
            {
                case 0: a.tie(pb); a.clear();             break;
                case 1: b.tie(pa); b.clear();             break;
                case 2: c.tie(pa); a.tie(pc);
                        c.clear(); a.clear();             break;
                case 3: a << "tied" << i;                 break;   // drives tie()->flush()
            }
        }
    });

    a.tie(nullptr);
    b.tie(nullptr);
    c.tie(nullptr);
}

TEST(Concur, ConcurrentDirectionSwitching)
{
    using namespace IOv2;

    // iostream::switch_to_get/switch_to_put are ordinary operations that mutate shared
    // buffer state: base_streambuf::switch_to_*() repositions the converter, clears
    // m_read_buf (a std::deque) and flips the converter's direction flag. The sentries
    // call those very same functions -- but from inside io_mutex() -- so the explicit
    // entry points must hold it too, or the two paths race on that deque.
    //
    // The reads below are what make the race reachable: switch_to_put() only does its
    // interesting work (tell -> seek -> clear) while the read buffer is non-empty.
    iostream ios(mem_device<char>{std::string(4096, 'q') + " tail"});
    spawn([&ios](int id)
    {
        for (int i = 0; i < kIters; ++i)
        {
            switch (id % 4)
            {
                case 0: ios.switch_to_get();                     break;
                case 1: ios.switch_to_put();                     break;
                case 2: { std::string s; ios >> s; ios.seek(0); } break;  // in_sentry -> switch_to_get
                case 3: ios << "sw" << i;                        break;  // out_sentry -> switch_to_put
            }
        }
    });
}

TEST(Concur, EndlAgainstTheLocaleSetter)
{
    using namespace IOv2;

    // endl has to widen '\n', which means reading the stream's locale. The locale setter
    // move-assigns m_locale while holding io_mutex(), and locale's own move-assignment
    // takes no lock at all, so that read must happen under the same lock -- reading it
    // outside is a plain data race on locale's two maps.
    //
    // It is also why endl must not route "flush this time" through the format flags: that
    // read-modify-write straddles the put(), so two threads can each observe the other's
    // intermediate state and one of them silently skips its flush. m_flags is atomic, so
    // that one is a lost update rather than a data race and TSan cannot see it; the only
    // thing checked here is that the flags are not left disturbed afterwards.
    const locale<char> loc("C");
    ostream os(mem_device<char>{});
    EXPECT_EQ(os.flags() & ios_defs::unitbuf, 0);

    spawn([&os, &loc](int id)
    {
        for (int i = 0; i < kIters; ++i)
        {
            switch (id % 4)
            {
                case 0:
                case 1: os << IOv2::endl;        break;
                case 2: (void)os.locale(loc);    break;
                case 3: os << "x" << i;          break;
            }
        }
    });

    // A stream that started without unitbuf must not come out of this with it set.
    EXPECT_EQ(os.flags() & ios_defs::unitbuf, 0);
}

TEST(Concur, ConcurrentPwordAndCallbackAccess)
{
    using namespace IOv2;

    // ios_base's pword storage (m_pwords, an unordered_map) and its callback list
    // (m_callbacks, a forward_list) are plain containers, not atomics. The locale(loc)
    // setter reaches both through access_callbacks() while holding io_mutex(), but
    // set_pword()/get_pword()/register_callback() are public entry points on ios_base --
    // which owns no lock -- so they touch the very same containers with no
    // synchronization. A mutex only excludes when every accessor takes it; here only one
    // side does, and an insert that rehashes on that side against a concurrent find() on
    // the other walks a freed bucket array rather than merely returning a stale value.
    ostream os(mem_device<char>{});
    const std::size_t id  = os.xalloc();
    const std::size_t id2 = os.xalloc();

    // Without a registered callback, locale(loc) iterates an empty list and never touches
    // m_pwords at all -- the setter has to actually write to the map for this to bite.
    const ios_base<char>::event_callback refresh =
        [](const IOv2::locale<char>&, std::shared_ptr<void>)
        { return std::make_shared<int>(1); };
    os.register_callback(refresh, id);

    spawn([&](int tid)
    {
        for (int i = 0; i < kIters; ++i)
        {
            switch (tid % 4)
            {
                // Holds io_mutex(); access_callbacks() iterates m_callbacks and
                // inserts/erases in m_pwords.
                case 0: (void)os.locale(IOv2::locale<char>{});      break;
                // The three below take no lock at all.
                case 1: os.set_pword(id, std::make_shared<int>(i)); break;
                case 2: (void)os.get_pword(id);                     break;
                case 3: os.register_callback(refresh, id2);         break;
            }
        }
    });

    EXPECT_TRUE(static_cast<bool>(os));
}

// A tie flush goes through try_flush(), which never waits for the target's lock. The two
// cases below both used to hang. The visible-edge AB-BA (two sync() guards taken in
// opposite orders, no tie involved) is deliberately NOT covered: that one is the caller's
// own lock-order bug.
TEST(Concur, TieFlushDoesNotWaitForABlockedTarget)
{
    using namespace IOv2;

    // A thread parked on the tie target's sync() must not stall a writer on a stream
    // tied to it.
    ostream target(mem_device<char>{});
    ostream writer(mem_device<char>{});
    writer.tie(&target);

    std::atomic<bool> release{false};
    std::atomic<bool> written{false};

    std::thread holder([&]
    {
        IOv2::sync guard(target);
        while (!release.load()) std::this_thread::yield();
    });

    std::thread w([&]
    {
        writer << "x";
        written.store(true);
    });

    w.join();                   // hangs here before the fix
    EXPECT_TRUE(written.load());
    release.store(true);
    holder.join();

    writer.tie(nullptr);
}

TEST(Concur, HiddenEdgeLockOrderInversionCompletes)
{
    using namespace IOv2;

    // The hidden-edge AB-BA -- X tied to Q, Y tied to P, one thread holding P and the
    // other Q -- completes instead of deadlocking.
    ostream p(mem_device<char>{});
    ostream q(mem_device<char>{});
    ostream x(mem_device<char>{});
    ostream y(mem_device<char>{});
    x.tie(&q);
    y.tie(&p);

    std::atomic<int> ready{0};
    std::atomic<int> done{0};

    auto body = [&ready, &done](auto& held, auto& used, int v)
    {
        IOv2::sync guard(held);
        ready.fetch_add(1);
        while (ready.load() < 2) std::this_thread::yield();
        used << v;
        done.fetch_add(1);
    };

    std::thread t1([&] { body(p, x, 1); });
    std::thread t2([&] { body(q, y, 2); });
    t1.join();                  // hangs here before the fix
    t2.join();
    EXPECT_EQ(done.load(), 2);

    x.tie(nullptr);
    y.tie(nullptr);
}

TEST(Concur, AssignmentToATieTarget)
{
    using namespace IOv2;

    // Assignment replaces m_streambuf wholesale, destroying the converter a concurrent tie
    // flush is about to walk into -- runtime_cvt checks its impl pointer for null and then
    // dereferences it, so the window between the two is a use-after-free. The written contract
    // ("no other thread may operate on this stream") cannot be honored here even in principle:
    // the flush is library-initiated and invisible to the caller, fired by a sentry on some
    // other stream. Hence the destination is covered by its own io_mutex().
    // The writers spin until the mutator signals done rather than running a fixed count: a
    // mutator iteration costs far more than a put(), so a fixed count lets the writers retire
    // early and leaves the tail of the run single-threaded.
    ostream target(mem_device<char>{});
    ostream writer(mem_device<char>{});
    writer.tie(&target);

    const ostream src(mem_device<char>{});
    std::atomic<bool> done{false};
    spawn([&](int id)
    {
        if (id != 0)
        {
            while (!done.load(std::memory_order_relaxed))
                writer.put('x');                                 // drives target.try_flush()
            return;
        }

        for (int i = 0; i < kIters; ++i)
        {
            if (i % 2) target = src;                             // copy assignment
            else       target = ostream(mem_device<char>{});     // move assignment
        }
        done.store(true, std::memory_order_relaxed);
    });

    EXPECT_TRUE(static_cast<bool>(writer));
    EXPECT_TRUE(static_cast<bool>(target));
    writer.tie(nullptr);
}

TEST(Concur, CopyOfATieTarget)
{
    using namespace IOv2;

    // The mirror image: copying reads the target's device, buffer and cursors while a tie flush
    // runs on that same target. The symptom of a torn read is not a crash but a bad snapshot,
    // so surviving the run proves nothing; the copy's contents are asserted instead. Only one
    // thread ever writes to `target`, which makes the expectation exact: after the i-th put, a
    // flushed copy must hold exactly i+1 'y's.
    ostream target(mem_device<char>{});
    ostream writer(mem_device<char>{});
    writer.tie(&target);

    std::atomic<bool> done{false};
    spawn([&](int id)
    {
        if (id != 0)
        {
            while (!done.load(std::memory_order_relaxed))
                writer.put('x');
            return;
        }

        for (int i = 0; i < kIters; ++i)
        {
            target.put('y');
            auto copy = target;                  // NOLINT(performance-unnecessary-copy-initialization)
            copy.flush();
            auto [dev, err] = copy.detach();
            EXPECT_FALSE(err);
            EXPECT_EQ(dev.str(), std::string(static_cast<std::size_t>(i) + 1, 'y'));
        }
        done.store(true, std::memory_order_relaxed);
    });

    EXPECT_TRUE(static_cast<bool>(writer));
    EXPECT_TRUE(static_cast<bool>(target));
    writer.tie(nullptr);
}

TEST(Concur, CopyAgainstStateWrites)
{
    using namespace IOv2;

    // Copy construction reads the source's whole state component -- the bits, the exception
    // mask and the four exception_ptrs -- while another thread writes exactly those. The two
    // sides used to take different mutexes (io_mutex() for the copy, a separate state mutex
    // for the writer), so they never excluded each other: a refcount race on
    // std::exception_ptr, which can double-free or leak. handle_exception() is what puts a
    // real exception_ptr in there; clear() is what releases it, so the pointer churns on
    // every iteration. Nothing here asserts a value -- the run exists to give TSan the
    // interleaving.
    ostream source(mem_device<char>{});

    std::atomic<bool> done{false};
    spawn([&](int id)
    {
        if (id != 0)
        {
            while (!done.load(std::memory_order_relaxed))
            {
                source.handle_exception(std::make_exception_ptr(device_error("boom")));
                // strfailbit is never set here, so the mask write cannot make clear() throw.
                source.exceptions(ios_defs::strfailbit);
                source.clear();
                source.exceptions(ios_defs::goodbit);
            }
            return;
        }

        for (int i = 0; i < kIters; ++i)
        {
            auto copy = source;              // NOLINT(performance-unnecessary-copy-initialization)
            (void)copy.rdstate();
            (void)copy.exceptions();
        }
        done.store(true, std::memory_order_relaxed);
    });

    source.clear();
    EXPECT_TRUE(static_cast<bool>(source));
}

TEST(Concur, MoveAssignmentAgainstStateWrites)
{
    using namespace IOv2;

    // The other half of the same hole, entered from the destination side: move assignment
    // replaces the destination's state under the destination's io_mutex(), while another
    // thread writes that state under the old state mutex. Moving *from* a stream another
    // thread may still be using stays out of contract, so every source here is a temporary
    // this thread alone can see.
    ostream dest(mem_device<char>{});

    std::atomic<bool> done{false};
    spawn([&](int id)
    {
        if (id != 0)
        {
            while (!done.load(std::memory_order_relaxed))
            {
                dest.handle_exception(std::make_exception_ptr(device_error("boom")));
                dest.clear();
            }
            return;
        }

        for (int i = 0; i < kIters; ++i)
            dest = ostream(mem_device<char>{});

        done.store(true, std::memory_order_relaxed);
    });

    dest.clear();
    EXPECT_TRUE(static_cast<bool>(dest));
}

TEST(Concur, AttachDetachOnATieTarget)
{
    using namespace IOv2;

    // The same window entered through a different door: detach() guts m_streambuf and attach()
    // rebuilds it, both while a tie flush may be walking it. With both sides under io_mutex()
    // the undefined behavior becomes the silent failure that is already documented -- the flush
    // finds a stream with no device and sets a bit the writer's sentry swallows. attach()
    // clears the state before installing, so the last one leaves `target` good again.
    ostream target(mem_device<char>{});
    ostream writer(mem_device<char>{});
    writer.tie(&target);

    std::atomic<bool> done{false};
    spawn([&](int id)
    {
        if (id != 0)
        {
            while (!done.load(std::memory_order_relaxed))
                writer.put('x');
            return;
        }

        for (int i = 0; i < kIters; ++i)
        {
            auto [dev, err] = target.detach();
            target.attach(std::move(dev));
        }
        done.store(true, std::memory_order_relaxed);
    });

    EXPECT_TRUE(static_cast<bool>(writer));
    EXPECT_TRUE(static_cast<bool>(target));
    writer.tie(nullptr);
}

TEST(Concur, CrossedConcurrencyOnOneIostream)
{
    using namespace IOv2;

    // The three mutators that each already have a case of their own, now aimed at ONE iostream
    // at the same time: assignment (replaces m_streambuf wholesale), direction switching
    // (repositions the converter and clears the read buffer), and a library-initiated tie flush
    // (walks that same converter from a sentry on another stream). Each pair is already covered
    // -- assign x tie flush by AssignmentToATieTarget, direction thrash alone by
    // ConcurrentDirectionSwitching -- but a window that only opens when all three interleave
    // shows up in neither, and only a bidirectional stream can host all three.
    //
    // The device carries a long run of one character with a delimiter at the end, so extraction
    // leaves the read buffer non-empty: switch_to_put() only does its interesting work
    // (tell -> seek -> clear the deque) from that state.
    iostream target(mem_device<char>{std::string(4096, 'q') + " tail"});
    ostream writer(mem_device<char>{});
    writer.tie(&target);

    const iostream src(mem_device<char>{std::string(4096, 'r') + " tail"});
    std::atomic<bool> done{false};
    spawn([&](int id)
    {
        if (id != 0)
        {
            while (!done.load(std::memory_order_relaxed))
            {
                switch (id)
                {
                    case 1: target.switch_to_get(); target.switch_to_put(); break;
                    case 2: writer.put('x');                                break;   // tie flush
                    default:
                    {
                        std::string s;
                        target >> s;            // in_sentry  -> switch_to_get
                        target.seek(0);
                        target << 'z';          // out_sentry -> switch_to_put
                        break;
                    }
                }
            }
            return;
        }

        for (int i = 0; i < kIters; ++i)
        {
            if (i % 2) target = src;                                              // copy
            else       target = iostream(mem_device<char>{std::string(64, 'w')}); // move
        }
        done.store(true, std::memory_order_relaxed);
    });

    EXPECT_TRUE(static_cast<bool>(writer));
    writer.tie(nullptr);
}
