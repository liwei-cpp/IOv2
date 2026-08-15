#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <string>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <support/dump_info.h>
#include <support/verify.h>

namespace
{
    template <typename C>
    struct tabby_mctype : IOv2::ctype_conf<C>
    {
        tabby_mctype()
            : IOv2::ctype_conf<C>("C") {}
        virtual C widen(char c) const override
        {
            return (c == ' ') ? '\t' : c;
        }
    };
}

void test_io_base_char_fill_1()
{
    dump_info("Test ios_base<char> fill case 1...");
    {
        IOv2::ostream out{IOv2::mem_device{""}};
        IOv2::locale<char> loc = IOv2::locale<char>().involve(std::make_shared<tabby_mctype<char>>());
        out.locale(loc);
        
        // Imbuing a new locale doesn't affect fill().
        VERIFY(out.fill() == ' ');
        out.fill('*');
        out.locale(IOv2::locale<char>{});
        VERIFY(out.fill() == '*');
    }

    dump_info("Done\n");
}

void test_io_base_wchar_t_fill_1()
{
    dump_info("Test ios_base<wchar_t> fill case 1...");
    {
        IOv2::ostream out{IOv2::mem_device{L""}};
        IOv2::locale<wchar_t> loc = IOv2::locale<wchar_t>().involve(std::make_shared<tabby_mctype<wchar_t>>());
        out.locale(loc);
        
        // Imbuing a new locale doesn't affect fill().
        VERIFY(out.fill() == L' ');
        out.fill(L'*');
        out.locale(IOv2::locale<wchar_t>{});
        VERIFY(out.fill() == L'*');
    }

    dump_info("Done\n");
}

void test_io_base_char_fill_2()
{
    dump_info("Test ios_base<char> fill case 2 (fill that would change the value read)...");

    // A fill character is rejected only where padding is actually written, and the
    // rejection reaches the stream as an ordinary formatting failure: strfailbit, and
    // an exception only if that bit is masked in.
    {
        IOv2::ostream out{IOv2::mem_device{""}};
        out << IOv2::setfill('0');

        // Nothing to pad, so the sticky fill is never written and never vetted.
        out << 42;
        VERIFY(out.good());

        // Zero-padding a negative number to the right would write "00000-42", which
        // reads as no number at all.
        out << IOv2::setw(8) << IOv2::right << -42;
        VERIFY(!out.good());
        VERIFY(out.rdstate() & IOv2::ios_defs::strfailbit);

        out.clear();
        // `internal` puts the same zeros where they read as leading zeros.
        out << IOv2::setw(8) << IOv2::internal << -42;
        VERIFY(out.good());

        auto [dev, err] = out.detach();
        VERIFY(dev.str() == "42-0000042");
    }

    // With strfailbit masked in, the same rejection is reported as an exception.
    {
        IOv2::ostream out{IOv2::mem_device{""}};
        out.exceptions(IOv2::ios_defs::strfailbit);
        out << IOv2::setfill('9') << IOv2::setw(8);
        try
        {
            out << 42;
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        VERIFY(out.rdstate() & IOv2::ios_defs::strfailbit);
    }

    dump_info("Done\n");
}
