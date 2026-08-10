#include <string>
#include <io/io_base.h>
#include <support/dump_info.h>
#include <support/verify.h>

void test_ios_state_1()
{
    dump_info("Test ios_state case 1...");
    {
        IOv2::ios_state<char> ios_01;
        VERIFY(ios_01.exceptions() == IOv2::ios_defs::goodbit);
    }
    {
        IOv2::ios_state<char> ios_01;
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
        IOv2::ios_state<char> ios_01;
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

void test_ios_state_2()
{
    dump_info("Test ios_state case 2...");

    IOv2::ios_state<char> stream;
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

void test_ios_state_handle_exception_eof_1()
{
    dump_info("Test ios_state::handle_exception with eof_error case 1...");

    // Regression test: handle_exception() routes a caught eof_error through
    // setstate(eofbit) -> clear(). When exceptions(eofbit) is enabled, this
    // must actually raise a notification exception, exactly like the
    // devfailbit/cvtfailbit/strfailbit/otherfailbit categories already do.
    // Previously, clear()'s eofbit branch was guarded by
    // `!std::current_exception()`, which is always false while inside
    // handle_exception's own catch block, so the exception was silently
    // swallowed.
    {
        IOv2::ios_state<char> stream;
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
        IOv2::ios_state<char> stream;

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

void test_ios_state_clear_exceptions_1()
{
    dump_info("Test ios_state::clear exception routing case 1...");
    using namespace IOv2;

    // For each failure category, clear() must, when the corresponding exception
    // mask bit is enabled, either re-throw the exception previously stashed by
    // handle_exception(), or, when none was stashed, throw a fresh category
    // exception.

    // --- devfailbit: stashed exception is re-thrown ---
    {
        ios_state<char> s;
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
        ios_state<char> s;
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
        ios_state<char> s;
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
        ios_state<char> s;
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
        ios_state<char> s;
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
        ios_state<char> s;
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

// handle_exception must only set bits -- never throw -- while an exception is unwinding, even
// when the mask says the bit should throw. Otherwise a destructor that reports a failure through
// it would call std::terminate and lose the original exception.
void test_ios_state_unwinding_1()
{
    dump_info("Test ios_state::handle_exception during unwinding case 1...");

    // A destructor reporting a failure while another exception is in flight: bit set, no throw.
    {
        IOv2::ios_state<char> s;
        s.exceptions(IOv2::ios_defs::strfailbit);

        struct probe
        {
            IOv2::ios_state<char>& st;
            ~probe()
            {
                st.handle_exception(std::make_exception_ptr(IOv2::stream_error("unwind")));
            }
        };

        bool caught_original = false;
        try
        {
            probe p{s};
            throw 42;
        }
        catch (int)
        {
            caught_original = true;   // the original exception survives
        }
        VERIFY(caught_original);
        VERIFY(s.rdstate() & IOv2::ios_defs::strfailbit);
    }

    // The check is exact inside a catch handler: the exception being handled no longer counts,
    // so a mask-driven throw still happens there.
    {
        IOv2::ios_state<char> s;
        s.exceptions(IOv2::ios_defs::strfailbit);

        bool threw = false;
        try
        {
            try { throw 42; }
            catch (int)
            {
                s.handle_exception(std::make_exception_ptr(IOv2::stream_error("in handler")));
            }
        }
        catch (const IOv2::stream_error&)
        {
            threw = true;
        }
        VERIFY(threw);
        VERIFY(s.rdstate() & IOv2::ios_defs::strfailbit);
    }

    // Control: outside any unwinding the mask is honored as before.
    {
        IOv2::ios_state<char> s;
        s.exceptions(IOv2::ios_defs::strfailbit);

        bool threw = false;
        try { s.handle_exception(std::make_exception_ptr(IOv2::stream_error("plain"))); }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
        VERIFY(s.rdstate() & IOv2::ios_defs::strfailbit);
    }

    dump_info("Done\n");
}

void test_ios_state()
{
    test_ios_state_1();
    test_ios_state_2();
    test_ios_state_handle_exception_eof_1();
    test_ios_state_clear_exceptions_1();
    test_ios_state_unwinding_1();
}