#include <stdexcept>
#include <string>
#include <device/file_device.h>
#include <device/mem_device.h>
#include <io/fp_defs/char_and_str.h>
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
    struct writer<TChar, dummy_type>
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

    // unwinding branch: the operation body itself throws (writer throws foobar) while
    // unitbuf is set, so the out_sentry destructor runs during stack unwinding and the
    // device flush also fails. The destructor must swallow that failure and never throw
    // during unwinding (no std::terminate). operator<< then catches the writer's foobar
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
