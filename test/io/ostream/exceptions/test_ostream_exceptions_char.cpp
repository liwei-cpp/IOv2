#include <stdexcept>
#include <string>
#include <thread>
#include <device/file_device.h>
#include <device/mem_device.h>
#include <io/traits/char_and_str.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/failing_device.h>
#include <support/file_guard.h>
#include <support/verify.h>

namespace
{
    struct foobar: std::exception { };
    struct dummy_type {};
}

namespace IOv2
{
    template <typename TChar>
    struct io_traits<TChar, dummy_type>
    {
        template <typename TIter>
            requires (std::is_same_v<TChar, typename TIter::value_type>)
        static TIter swrite(TIter, ios_base<TChar>&, const locale<TChar>&, dummy_type)
        {
            throw foobar();
        }
    };
}

void test_ostream_exceptions_char_1()
{
    dump_info("Test ostream<char> exceptions case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T strm(IOv2::mem_device{""});
        strm.exceptions(IOv2::ios_defs::otherfailbit);
        try
        {
            strm << dummy_type{};
            dump_info("unreachable code");
            std::abort();
        }
        catch (const foobar&)
        {
            // the fail of strm will cause the stream_file to be set
            VERIFY(!strm);
        }
        catch (...)
        {
            VERIFY(false);
        }
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
    
    dump_info("Done\n");
}

void test_ostream_exceptions_char_2()
{
    dump_info("Test ostream<char> exceptions case 2...");

    auto helper = []<template<typename, typename> class T>()
    {
        T out(IOv2::mem_device{""});
        out.setf(IOv2::ios_defs::unitbuf);
        out.exceptions(IOv2::ios_defs::cvtfailbit);
        out << dummy_type{};
        VERIFY(!out);

        out.clear();
        VERIFY((bool)out);
        out.exceptions(IOv2::ios_defs::cvtfailbit);
        out << dummy_type{};
        VERIFY(!out);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_exceptions_char_3()
{
    dump_info("Test ostream<char> exceptions case 3 (flush failure x exception mask)...");

    // devfailbit masked: a unitbuf stream whose device fails its flush reports the
    // failure through the out_sentry destructor. Because the destructor runs on a
    // normal scope exit (no unwinding), it routes the device_error through
    // handle_exception, which rethrows the original device_error to the caller and
    // leaves devfailbit set.
    {
        auto helper = []<template<typename, typename> class T>()
        {
            T out(failing_device<char>{std::string(""), true});
            out.setf(IOv2::ios_defs::unitbuf);
            out.exceptions(IOv2::ios_defs::devfailbit);
            bool caught = false;
            try { out.put('x'); }
            catch (const IOv2::device_error&) { caught = true; }
            catch (...) { VERIFY(false); }
            VERIFY(caught);
            VERIFY(out.rdstate() & IOv2::ios_defs::devfailbit);
        };
        helper.operator()<IOv2::ostream>();
        helper.operator()<IOv2::iostream>();
    }

    // no mask: the same flush failure only sets devfailbit and does not throw.
    {
        auto helper = []<template<typename, typename> class T>()
        {
            T out(failing_device<char>{std::string(""), true});
            out.setf(IOv2::ios_defs::unitbuf);
            bool threw = false;
            try { out.put('x'); }
            catch (...) { threw = true; }
            VERIFY(!threw);
            VERIFY(out.rdstate() & IOv2::ios_defs::devfailbit);
        };
        helper.operator()<IOv2::ostream>();
        helper.operator()<IOv2::iostream>();
    }

    // unwinding branch: the operation body itself throws (swrite throws foobar) while
    // unitbuf is set, so the out_sentry destructor runs during stack unwinding and the
    // device flush also fails. The destructor must swallow that failure and never throw
    // during unwinding (no std::terminate). operator<< then catches swrite's foobar
    // and, with otherfailbit unmasked, only sets state, so control returns normally;
    // reaching the assertion proves there was no terminate and the stream failed.
    {
        auto helper = []<template<typename, typename> class T>()
        {
            T out(failing_device<char>{std::string(""), true});
            out.setf(IOv2::ios_defs::unitbuf);
            out << dummy_type{};
            VERIFY(!out);
        };
        helper.operator()<IOv2::ostream>();
        helper.operator()<IOv2::iostream>();
    }

    dump_info("Done\n");
}

// Copy assignment gives the strong exception guarantee. The copy is made into a temporary
// first, so a throw leaves the destination exactly as it was; only a move assignment, which
// is noexcept throughout, commits it. A file_device is move-only, so copying its converter
// kernel always throws -- which is what makes this observable at all.
void test_ostream_exceptions_char_4()
{
    dump_info("Test ostream<char> exceptions case 4...");

    const std::string f1 = "test_ostream_exceptions_char_4_1.txt";
    const std::string f2 = "test_ostream_exceptions_char_4_2.txt";

    // Output direction: a failed assignment must not leak the source's format state or
    // status bits into the destination, nor swap out its device.
    {
        file_guard g1(f1);
        file_guard g2(f2);

        auto helper = [&]<template<typename, typename> class T>()
        {
            T<IOv2::ofile_device<char>, char> src(IOv2::ofile_device<char>{f1});
            T<IOv2::ofile_device<char>, char> dst(IOv2::ofile_device<char>{f2});

            src.width(42);
            src.precision(9);
            src.fill('#');
            src.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
            src.setstate(IOv2::ios_defs::strfailbit);

            const auto w = dst.width();
            const auto p = dst.precision();
            const auto fl = dst.fill();
            const auto fg = dst.flags();
            const auto st = dst.rdstate();

            bool threw = false;
            try { dst = src; }
            catch (const IOv2::cvt_error&) { threw = true; }

            VERIFY(threw);
            VERIFY(dst.width() == w);
            VERIFY(dst.precision() == p);
            VERIFY(dst.fill() == fl);
            VERIFY(dst.flags() == fg);
            VERIFY(dst.rdstate() == st);
            VERIFY(static_cast<bool>(dst));

            // Still bound to its own device, and unpadded: a leaked width would show up here.
            dst << "abc";
            dst.flush();
        };

        helper.operator()<IOv2::ostream>();
        VERIFY(g2.contents() == "abc");
    }

    // Input direction, and a self-assignment: without the self-check the temporary copy
    // would throw on `s = s` even though nothing needs to happen.
    {
        file_guard g1(f1, std::string("hello world"));

        IOv2::istream<IOv2::ifile_device<char>, char> s(IOv2::ifile_device<char>{f1});
        s.width(11);

        // Through a pointer, so that the self-assignment survives to run time instead of
        // being rejected by -Wself-assign-overloaded.
        auto* self = &s;

        bool threw = false;
        try { s = *self; }
        catch (const IOv2::cvt_error&) { threw = true; }

        VERIFY(!threw);
        VERIFY(s.width() == 11u);
        VERIFY(static_cast<bool>(s));

        std::string got;
        s >> got;
        VERIFY(got == "hello");
    }

    // The happy path is unchanged: with a copyable device the assignment still copies.
    {
        auto helper = []<template<typename, typename> class T>()
        {
            T src(IOv2::mem_device{""});
            T dst(IOv2::mem_device{""});

            src.width(7);
            src.precision(3);
            src.fill('*');

            dst = src;

            VERIFY(dst.width() == 7u);
            VERIFY(dst.precision() == 3);
            VERIFY(dst.fill() == '*');

            auto* self = &dst;
            dst = *self;
            VERIFY(dst.width() == 7u);
            VERIFY(dst.precision() == 3);
        };

        helper.operator()<IOv2::ostream>();
        helper.operator()<IOv2::iostream>();
        helper.operator()<IOv2::istream>();
    }

    dump_info("Done\n");
}

// Copy construction reads the source under the source's io_mutex(), taken in a mem-initializer
// that delegates to a private constructor -- one full-expression, so the lock spans every
// subobject's initialization instead of being released after the first one. The other half of
// that idiom is unwinding: when copying a move-only converter kernel throws, the lock temporary
// must be destroyed too, or the source stays locked for good and every later tie flush on it
// silently skips. Checked from another thread, because io_mutex() is recursive and a try_lock()
// on the thread that leaked it would succeed and prove nothing.
void test_ostream_exceptions_char_5()
{
    dump_info("Test ostream<char> exceptions case 5...");

    const std::string f1 = "test_ostream_exceptions_char_5_1.txt";
    const std::string f2 = "test_ostream_exceptions_char_5_2.txt";

    auto unlocked = [](auto& s)
    {
        bool res = false;
        std::thread t([&s, &res]
        {
            if (s.io_mutex().try_lock())
            {
                res = true;
                s.io_mutex().unlock();
            }
        });
        t.join();
        return res;
    };

    {
        file_guard g1(f1);

        // trunc is spelled out so that both devices create the file: ofile_device opens "w"
        // either way, but file_device without it opens "r+" and would fail on a missing file.
        auto helper = [&]<template<typename, typename> class T,
                                    typename TDevice>()
        {
            T<TDevice, char> src(TDevice{f1, IOv2::file_open_flag::trunc});
            VERIFY(unlocked(src));

            bool threw = false;
            try { auto copy = src; (void)copy; }  // NOLINT(performance-unnecessary-copy-initialization)
            catch (const IOv2::cvt_error&) { threw = true; }

            VERIFY(threw);
            VERIFY(unlocked(src));
        };

        helper.operator()<IOv2::ostream, IOv2::ofile_device<char>>();
        helper.operator()<IOv2::iostream, IOv2::file_device<char>>();
    }

    {
        file_guard g2(f2, std::string("hello world"));

        IOv2::istream<IOv2::ifile_device<char>, char> src(IOv2::ifile_device<char>{f2});
        VERIFY(unlocked(src));

        bool threw = false;
        try { auto copy = src; (void)copy; }  // NOLINT(performance-unnecessary-copy-initialization)
        catch (const IOv2::cvt_error&) { threw = true; }

        VERIFY(threw);
        VERIFY(unlocked(src));

        // The source is still usable: a failed copy must not disturb it either.
        std::string got;
        src >> got;
        VERIFY(got == "hello");
    }

    dump_info("Done\n");
}

void test_ostream_exceptions_char_6()
{
    dump_info("Test ostream<char> exceptions case 6...");

    // flush() must report a failed stream the same way endl/ends do. It used to return early,
    // before its try block, so on a failed stream it flushed nothing, set no bit and threw
    // nothing -- bypassing the exception mask entirely and turning `os << flush` into a silent
    // no-op exactly when the caller had asked to be told about failures.

    // Without the bit in the mask the failure is recorded but not thrown, as everywhere else.
    {
        IOv2::ostream<IOv2::mem_device<char>, char> os(IOv2::mem_device<char>{});
        os.put('a');
        os.setstate(IOv2::ios_defs::strfailbit);

        bool threw = false;
        try { os.flush(); }
        catch (const IOv2::stream_error&) { threw = true; }

        VERIFY(!threw);
        VERIFY(os.rdstate() == IOv2::ios_defs::strfailbit);
    }

    // With the bit masked in, all four flush spellings must throw.
    {
        IOv2::ostream<IOv2::mem_device<char>, char> os(IOv2::mem_device<char>{});
        os.put('a');
        os.exceptions(IOv2::ios_defs::strfailbit);
        try { os.setstate(IOv2::ios_defs::strfailbit); }
        catch (const IOv2::stream_error&) {}
        VERIFY(!static_cast<bool>(os));

        auto throws = [&os](auto op)
        {
            bool threw = false;
            try { op(os); }
            catch (const IOv2::stream_error&) { threw = true; }
            return threw;
        };

        VERIFY(throws([](auto& s) { s.flush(); }));
        VERIFY(throws([](auto& s) { s << IOv2::flush; }));
        VERIFY(throws([](auto& s) { s << IOv2::endl; }));
        VERIFY(throws([](auto& s) { s << IOv2::ends; }));
    }

    // A good stream is untouched by all of this.
    {
        IOv2::ostream<IOv2::mem_device<char>, char> os(IOv2::mem_device<char>{});
        os.exceptions(IOv2::ios_defs::strfailbit);
        os.put('a');
        os.flush();
        os << IOv2::flush;

        VERIFY(static_cast<bool>(os));
        VERIFY(os.rdstate() == IOv2::ios_defs::goodbit);
        VERIFY(os.detach().first.str() == "a");
    }

    dump_info("Done\n");
}
