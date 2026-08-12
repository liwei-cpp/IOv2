#include <cstdint>
#include <string>
#include <type_traits>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <support/dump_info.h>
#include <support/verify.h>

namespace
{
// ---------------------------------------------------------------------------------------------
// fmtflags and iostate are two bitmask types, not two spellings of one integer.
//
// As typedefs for uint16_t/uint8_t they converted freely into each other and accepted any integer,
// so setiosflags(strfailbit) -- a state bit handed to a formatting interface -- compiled and set
// boolalpha, the two constants sharing bit 0. They are unscoped enums now, closed under the bitwise
// operators; being unscoped, the conversion out to bool that `if (state & eofbit)` relies on is
// unchanged.
// ---------------------------------------------------------------------------------------------
using fmt = IOv2::ios_defs::fmtflags;
using ist = IOv2::ios_defs::iostate;

template <typename T> concept setf_takes        = requires (IOv2::ios_state<char>& s, T v) { s.setf(v); };
template <typename T> concept clear_takes       = requires (IOv2::ios_state<char>& s, T v) { s.clear(v); };
template <typename T> concept setiosflags_takes = requires (T v) { IOv2::setiosflags(v); };

static_assert(std::is_enum_v<fmt> && std::is_same_v<std::underlying_type_t<fmt>, std::uint16_t>);
static_assert(std::is_enum_v<ist> && std::is_same_v<std::underlying_type_t<ist>, std::uint8_t>);

// Unscoped, so the outward conversions the old typedefs allowed still hold.
static_assert(std::is_convertible_v<fmt, bool>);
static_assert(std::is_convertible_v<ist, unsigned>);

// Nothing converts inward, and neither converts to the other.
static_assert(!std::is_convertible_v<int, fmt> && !std::is_convertible_v<int, ist>);
static_assert(!std::is_convertible_v<ist, fmt> && !std::is_convertible_v<fmt, ist>);

static_assert( setf_takes<fmt>         && !setf_takes<ist>         && !setf_takes<int>);
static_assert( clear_takes<ist>        && !clear_takes<fmt>        && !clear_takes<int>);
static_assert( setiosflags_takes<fmt>  && !setiosflags_takes<ist>  && !setiosflags_takes<int>);

// The operators keep a result in its own type, with the values the masks always had.
static_assert(std::is_same_v<decltype(IOv2::ios_defs::left | IOv2::ios_defs::right), fmt>);
static_assert(std::is_same_v<decltype(~IOv2::ios_defs::eofbit), ist>);
static_assert((IOv2::ios_defs::left | IOv2::ios_defs::right | IOv2::ios_defs::internal)
              == IOv2::ios_defs::adjustfield);
static_assert((IOv2::ios_defs::adjustfield & ~IOv2::ios_defs::left)
              == (IOv2::ios_defs::right | IOv2::ios_defs::internal));
static_assert((IOv2::ios_defs::skipws | IOv2::ios_defs::dec) == fmt{0x1002});
}

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

void test_ios_state_bitmask_types_1()
{
    dump_info("Test ios_state bitmask types case 1...");

    IOv2::ios_state<char> s;
    VERIFY(s.flags() == (IOv2::ios_defs::skipws | IOv2::ios_defs::dec));
    VERIFY(s.setf(IOv2::ios_defs::boolalpha) == (IOv2::ios_defs::skipws | IOv2::ios_defs::dec));
    VERIFY(s.flags() == (IOv2::ios_defs::skipws | IOv2::ios_defs::dec | IOv2::ios_defs::boolalpha));

    s.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
    VERIFY((s.flags() & IOv2::ios_defs::basefield) == IOv2::ios_defs::hex);
    s.unsetf(IOv2::ios_defs::boolalpha);
    VERIFY(!(s.flags() & IOv2::ios_defs::boolalpha));
    VERIFY(s.flags(IOv2::ios_defs::skipws) == (IOv2::ios_defs::skipws | IOv2::ios_defs::hex));
    VERIFY(s.flags() == IOv2::ios_defs::skipws);

    VERIFY(s.good());
    s.setstate(IOv2::ios_defs::strfailbit | IOv2::ios_defs::eofbit);
    VERIFY(s.rdstate() == (IOv2::ios_defs::strfailbit | IOv2::ios_defs::eofbit));
    VERIFY(s.str_fail() && s.eof() && !s.good() && !static_cast<bool>(s));
    s.unset_state(IOv2::ios_defs::strfailbit);
    VERIFY(s.rdstate() == IOv2::ios_defs::eofbit);
    VERIFY(static_cast<bool>(s));
    s.clear();
    VERIFY(s.good() && s.rdstate() == IOv2::ios_defs::goodbit);

    dump_info("Done\n");
}

void test_ios_state()
{
    test_ios_state_1();
    test_ios_state_2();
    test_ios_state_handle_exception_eof_1();
    test_ios_state_clear_exceptions_1();
    test_ios_state_unwinding_1();
    test_ios_state_bitmask_types_1();
}