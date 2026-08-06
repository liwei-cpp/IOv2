#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <device/mem_device.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

// Stream-level concurrency tests. These exist mainly to give ThreadSanitizer
// (MODE=tsan) real concurrent executions to inspect: the stream layer promises that a
// single operation is serialized by io_mutex(), and nothing here may report a race.
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

void test_concur_output_1()
{
    dump_info("Test concurrent output on one ostream case 1...");
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
    VERIFY(static_cast<bool>(os));

    dump_info("Done\n");
}

void test_concur_flush_1()
{
    dump_info("Test concurrent flush against writes case 1...");
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
    VERIFY(static_cast<bool>(os));

    dump_info("Done\n");
}

void test_concur_sentryless_1()
{
    dump_info("Test concurrent sentry-less operations case 1...");
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

    dump_info("Done\n");
}

void test_concur_state_1()
{
    dump_info("Test concurrent stream-state access case 1...");
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

    dump_info("Done\n");
}

void test_concur_tie_1()
{
    dump_info("Test concurrent tie() in both directions case 1...");
    using namespace IOv2;

    // Concurrent A.tie(B) / B.tie(A): tie_graph_mutex() fuses cycle detection and
    // commit, so a cycle can never form. A rejected request throws and must leave the
    // previous tie untouched.
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
            try
            {
                switch (id % 4)
                {
                    case 0: a.tie(pb);             break;
                    case 1: b.tie(pa);             break;
                    case 2: c.tie(pa); a.tie(pc);  break;
                    case 3: a << "tied" << i;      break;   // drives tie()->flush()
                }
            }
            catch (const stream_error&)
            {
            }
        }
    });

    a.tie(nullptr);
    b.tie(nullptr);
    c.tie(nullptr);

    dump_info("Done\n");
}

void test_concur_switch_1()
{
    dump_info("Test concurrent switch_to_get/switch_to_put case 1...");
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

    dump_info("Done\n");
}

void test_concur_endl_1()
{
    dump_info("Test concurrent endl against the locale setter case 1...");
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
    VERIFY((os.flags() & ios_defs::unitbuf) == 0);

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
    VERIFY((os.flags() & ios_defs::unitbuf) == 0);

    dump_info("Done\n");
}

void test_concur_pword_1()
{
    dump_info("Test concurrent pword/callback access case 1...");
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
    const size_t id  = os.xalloc();
    const size_t id2 = os.xalloc();

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

    VERIFY(static_cast<bool>(os));

    dump_info("Done\n");
}

void test_concur_tie_nonblocking_1()
{
    dump_info("Test tie flush never blocks case 1...");
    using namespace IOv2;

    // A tie flush goes through try_flush(), which never waits for the target's lock. Two
    // checks, both of which used to hang:
    //   * a thread parked on the tie target's sync() cannot stall a writer on a stream tied
    //     to it;
    //   * the hidden-edge AB-BA -- X tied to Q, Y tied to P, one thread holding P and the
    //     other Q -- completes instead of deadlocking.
    // The visible-edge AB-BA (two sync() guards taken in opposite orders, no tie involved)
    // is deliberately NOT covered: that one is the caller's own lock-order bug.
    {
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
        VERIFY(written.load());
        release.store(true);
        holder.join();

        writer.tie(nullptr);
    }

    {
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
        VERIFY(done.load() == 2);

        x.tie(nullptr);
        y.tie(nullptr);
    }

    dump_info("Done\n");
}

void test_concur_assign_tie_target_1()
{
    dump_info("Test concurrent assignment to a tie target case 1...");
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

    VERIFY(static_cast<bool>(writer));
    VERIFY(static_cast<bool>(target));
    writer.tie(nullptr);

    dump_info("Done\n");
}

void test_concur_copy_tie_source_1()
{
    dump_info("Test concurrent copy of a tie target case 1...");
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
            VERIFY(!err);
            VERIFY(dev.str() == std::string(static_cast<size_t>(i) + 1, 'y'));
        }
        done.store(true, std::memory_order_relaxed);
    });

    VERIFY(static_cast<bool>(writer));
    VERIFY(static_cast<bool>(target));
    writer.tie(nullptr);

    dump_info("Done\n");
}

void test_concur_attach_detach_1()
{
    dump_info("Test concurrent attach/detach on a tie target case 1...");
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

    VERIFY(static_cast<bool>(writer));
    VERIFY(static_cast<bool>(target));
    writer.tie(nullptr);

    dump_info("Done\n");
}
