#include <string>
#include <io/io_base.h>
#include <support/dump_info.h>
#include <support/verify.h>

void test_io_state_and_exp_1()
{
    dump_info("Test io_state_and_exp case 1...");
    {
        IOv2::io_state_and_exp ios_01;
        VERIFY(ios_01.exceptions() == IOv2::ios_defs::goodbit);
    }
    {
        IOv2::io_state_and_exp ios_01;
        try
        {
            ios_01.exceptions(IOv2::ios_defs::cvtfailbit);
        }
        catch(...)
        {
            dump_info("Unreachable code\n");
            std::abort();
        }
        auto iostate02 = ios_01.exceptions();
        VERIFY(iostate02 == IOv2::ios_defs::cvtfailbit);
    }
    {
        IOv2::ios_defs::iostate iostate02 = IOv2::ios_defs::goodbit;
        IOv2::io_state_and_exp ios_01;
        ios_01.clear(IOv2::ios_defs::cvtfailbit);
        try
        {
            ios_01.exceptions(IOv2::ios_defs::cvtfailbit);
            dump_info("Unreachable code\n");
            std::abort();
        }
        catch (IOv2::cvt_error&)
        {
            iostate02 = ios_01.exceptions();
        }
        catch(...)
        {
            dump_info("Unreachable code\n");
            std::abort();
        }
        VERIFY(iostate02 == IOv2::ios_defs::cvtfailbit);
    }
    dump_info("Done\n");
}

void test_io_state_and_exp_2()
{
    dump_info("Test io_state_and_exp case 2...");

    IOv2::io_state_and_exp stream;
    try
    {
        stream.setstate(IOv2::ios_defs::cvtfailbit);
        stream.exceptions(IOv2::ios_defs::cvtfailbit);
        dump_info("Unreachable code\n");
        std::abort();
    }
    catch (...)
    {
        // Don't clear.
    }
    
    try
    {
        // Calls clear(rdstate()), which throws in this case.
        stream.setstate(IOv2::ios_defs::goodbit);
        dump_info("Unreachable code\n");
        std::abort();
    }
    catch (...) {}

    dump_info("Done\n");
}

void test_io_state_and_exp_handle_exception_eof_1()
{
    dump_info("Test io_state_and_exp::handle_exception with eof_error case 1...");

    // Regression test: handle_exception() routes a caught eof_error through
    // setstate(eofbit) -> clear(). When exceptions(eofbit) is enabled, this
    // must actually raise a notification exception, exactly like the
    // devfailbit/cvtfailbit/strfailbit/otherfailbit categories already do.
    // Previously, clear()'s eofbit branch was guarded by
    // `!std::current_exception()`, which is always false while inside
    // handle_exception's own catch block, so the exception was silently
    // swallowed.
    {
        IOv2::io_state_and_exp stream;
        stream.exceptions(IOv2::ios_defs::eofbit);

        bool threw = false;
        try
        {
            stream.handle_exception(std::make_exception_ptr(IOv2::eof_error{}));
        }
        catch (IOv2::eof_error&)
        {
            threw = true;
        }
        catch (...)
        {
            dump_info("Unreachable code\n");
            std::abort();
        }

        VERIFY(threw);
        VERIFY(stream.eof());
    }

    // Without exceptions(eofbit) enabled, the state bit is still set but no
    // exception should be raised.
    {
        IOv2::io_state_and_exp stream;

        try
        {
            stream.handle_exception(std::make_exception_ptr(IOv2::eof_error{}));
        }
        catch (...)
        {
            dump_info("Unreachable code\n");
            std::abort();
        }

        VERIFY(stream.eof());
    }

    dump_info("Done\n");
}

void test_io_state_and_exp_clear_exceptions_1()
{
    dump_info("Test io_state_and_exp::clear exception routing case 1...");
    using namespace IOv2;

    // For each failure category, clear() must, when the corresponding exception
    // mask bit is enabled, either re-throw the exception previously stashed by
    // handle_exception(), or, when none was stashed, throw a fresh category
    // exception.

    // --- devfailbit: stashed exception is re-thrown ---
    {
        io_state_and_exp s;
        s.exceptions(ios_defs::devfailbit);
        bool threw = false;
        try
        {
            s.handle_exception(std::make_exception_ptr(device_error("stashed dev")));
        }
        catch (const device_error& e)
        {
            threw = (std::string(e.what()) == "stashed dev");
        }
        VERIFY(threw);
        VERIFY(s.dev_fail());
    }

    // --- devfailbit: no stashed exception -> fresh device_error ---
    {
        io_state_and_exp s;
        s.exceptions(ios_defs::devfailbit);
        bool threw = false;
        try
        {
            s.setstate(ios_defs::devfailbit);
        }
        catch (const device_error&)
        {
            threw = true;
        }
        VERIFY(threw);
    }

    // --- cvtfailbit: stashed exception is re-thrown ---
    {
        io_state_and_exp s;
        s.exceptions(ios_defs::cvtfailbit);
        bool threw = false;
        try
        {
            s.handle_exception(std::make_exception_ptr(cvt_error("stashed cvt")));
        }
        catch (const cvt_error& e)
        {
            threw = (std::string(e.what()) == "stashed cvt");
        }
        VERIFY(threw);
        VERIFY(s.cvt_fail());
    }

    // --- strfailbit: stashed exception is re-thrown ---
    {
        io_state_and_exp s;
        s.exceptions(ios_defs::strfailbit);
        bool threw = false;
        try
        {
            s.handle_exception(std::make_exception_ptr(stream_error("stashed str")));
        }
        catch (const stream_error& e)
        {
            threw = (std::string(e.what()) == "stashed str");
        }
        VERIFY(threw);
        VERIFY(s.str_fail());
    }

    // --- strfailbit: no stashed exception -> fresh stream_error ---
    {
        io_state_and_exp s;
        s.exceptions(ios_defs::strfailbit);
        bool threw = false;
        try
        {
            s.setstate(ios_defs::strfailbit);
        }
        catch (const stream_error&)
        {
            threw = true;
        }
        VERIFY(threw);
    }

    // --- otherfailbit: no stashed exception -> fresh stream_error ---
    {
        io_state_and_exp s;
        s.exceptions(ios_defs::otherfailbit);
        bool threw = false;
        try
        {
            s.setstate(ios_defs::otherfailbit);
        }
        catch (const stream_error&)
        {
            threw = true;
        }
        VERIFY(threw);
        VERIFY(s.other_fail());
    }

    dump_info("Done\n");
}

void test_io_state_and_exp()
{
    test_io_state_and_exp_1();
    test_io_state_and_exp_2();
    test_io_state_and_exp_handle_exception_eof_1();
    test_io_state_and_exp_clear_exceptions_1();
}