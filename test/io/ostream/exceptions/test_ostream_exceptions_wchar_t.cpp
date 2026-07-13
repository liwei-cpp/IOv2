#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/char_and_str.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/failing_device.h>
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

void test_ostream_exceptions_wchar_t_1()
{
    dump_info("Test ostream<wchar_t> exceptions case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T strm(IOv2::mem_device{L""});
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

void test_ostream_exceptions_wchar_t_2()
{
    dump_info("Test ostream<wchar_t> exceptions case 2...");

    auto helper = []<template<typename, typename> class T>()
    {
        T out(IOv2::mem_device{L""});
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

void test_ostream_exceptions_wchar_t_3()
{
    dump_info("Test ostream<wchar_t> exceptions case 3 (flush failure x exception mask)...");

    // devfailbit masked: a unitbuf stream whose device fails its flush reports the
    // failure through the out_sentry destructor. Because the destructor runs on a
    // normal scope exit (no unwinding), it routes the device_error through
    // handle_exception, which rethrows the original device_error to the caller and
    // leaves devfailbit set.
    {
        auto helper = []<template<typename, typename> class T>()
        {
            T out(failing_device<wchar_t>{std::wstring(L""), true}, IOv2::locale<wchar_t>("C"));
            out.setf(IOv2::ios_defs::unitbuf);
            out.exceptions(IOv2::ios_defs::devfailbit);
            bool caught = false;
            try { out.put(L'x'); }
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
            T out(failing_device<wchar_t>{std::wstring(L""), true}, IOv2::locale<wchar_t>("C"));
            out.setf(IOv2::ios_defs::unitbuf);
            bool threw = false;
            try { out.put(L'x'); }
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
            T out(failing_device<wchar_t>{std::wstring(L""), true}, IOv2::locale<wchar_t>("C"));
            out.setf(IOv2::ios_defs::unitbuf);
            out << dummy_type{};
            VERIFY(!out);
        };
        helper.operator()<IOv2::ostream>();
        helper.operator()<IOv2::iostream>();
    }

    dump_info("Done\n");
}