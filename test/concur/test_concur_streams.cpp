#include <string>
#include <thread>
#include <vector>

#include <device/mem_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
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
