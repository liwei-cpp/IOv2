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

namespace
{
    // Writes `v` into a fresh stream under `base` with the given fill, width and
    // adjustment, and reports whether the insertion was accepted. `text` receives what
    // was written, so an accepted case can be checked for content as well.
    bool fill_accepted(char fill, IOv2::ios_defs::fmtflags base,
                       IOv2::ios_defs::fmtflags adjust, unsigned long v,
                       std::string& text, IOv2::ios_defs::fmtflags extra = {})
    {
        IOv2::ostream out{IOv2::mem_device{""}};
        out.setf(base, IOv2::ios_defs::basefield);
        out.setf(adjust, IOv2::ios_defs::adjustfield);
        if (extra != IOv2::ios_defs::fmtflags{}) out.setf(extra);
        out << IOv2::setfill(fill) << IOv2::setw(8) << v;
        const bool ok = out.good();
        auto [dev, err] = out.detach();
        text = dev.str();
        return ok;
    }
}

void test_io_base_char_fill_3()
{
    dump_info("Test ios_base<char> fill case 3 (the digit set follows basefield)...");

    const auto hex      = IOv2::ios_defs::hex;
    const auto oct      = IOv2::ios_defs::oct;
    const auto dec      = IOv2::ios_defs::dec;
    const auto right    = IOv2::ios_defs::right;
    const auto left     = IOv2::ios_defs::left;
    const auto internal = IOv2::ios_defs::internal;

    std::string text;

    // Under hex the six letter digits read as part of the value just as '1'-'9' do:
    // setfill('f') on 0xab would write "ffffffab", which reads back as 4294967211.
    // Both cases are rejected whatever `uppercase` says, because the extractor takes
    // mixed-case hex.
    for (char f : {'a', 'b', 'c', 'd', 'e', 'f', 'A', 'B', 'C', 'D', 'E', 'F'})
        for (auto adjust : {right, left, internal})
        {
            VERIFY(!fill_accepted(f, hex, adjust, 0xab, text));
            VERIFY(!fill_accepted(f, hex, adjust, 0xab, text, IOv2::ios_defs::uppercase));
        }

    // '0' keeps working where it reads as a leading zero, so the tightening above did
    // not swallow the one digit that is legitimate.
    VERIFY(fill_accepted('0', hex, right, 0xab, text));
    VERIFY(text == "000000ab");
    VERIFY(fill_accepted('0', hex, internal, 0xab, text));
    VERIFY(text == "000000ab");
    VERIFY(!fill_accepted('0', hex, left, 0xab, text));

    // With showbase the prefix sits between a right-adjusted fill and the digits, so
    // only `internal` still reads as leading zeros.
    VERIFY(!fill_accepted('0', hex, right, 0xab, text, IOv2::ios_defs::showbase));
    VERIFY(fill_accepted('0', hex, internal, 0xab, text, IOv2::ios_defs::showbase));
    VERIFY(text == "0x0000ab");

    // Outside hex those letters are not digits and stay usable.
    VERIFY(fill_accepted('f', dec, right, 42, text));
    VERIFY(text == "ffffff42");
    VERIFY(fill_accepted('f', oct, right, 0777, text));
    VERIFY(text == "fffff777");

    // The test asks whether a character looks like a digit to a reader, not whether the
    // base would accept it: '8' and '9' are not octal digits but "88888777" still reads
    // as a number, so they stay rejected under oct.
    for (char f : {'8', '9'})
        for (auto adjust : {right, left, internal})
            VERIFY(!fill_accepted(f, oct, adjust, 0777, text));

    // Ordinary non-digit fills are untouched in every base.
    for (auto base : {dec, oct, hex})
    {
        VERIFY(fill_accepted('*', base, right, 42, text));
        VERIFY(fill_accepted(' ', base, right, 42, text));
    }

    dump_info("Done\n");
}

void test_io_base_wchar_t_fill_2()
{
    dump_info("Test ios_base<wchar_t> fill case 2 (the digit set follows basefield)...");

    // The atom table is widened through the same ctype, so the criterion has to hold on
    // a wide stream too; char is the only width the cases above cover.
    {
        IOv2::ostream out{IOv2::mem_device{L""}};
        out.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
        out << IOv2::setfill(L'f') << IOv2::setw(8) << 0xabUL;
        VERIFY(!out.good());
        VERIFY(out.rdstate() & IOv2::ios_defs::strfailbit);
    }
    {
        IOv2::ostream out{IOv2::mem_device{L""}};
        out.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
        out << IOv2::setfill(L'0') << IOv2::setw(8) << 0xabUL;
        VERIFY(out.good());
        auto [dev, err] = out.detach();
        VERIFY(dev.str() == L"000000ab");
    }
    {
        IOv2::ostream out{IOv2::mem_device{L""}};
        out << IOv2::setfill(L'f') << IOv2::setw(8) << 42UL;
        VERIFY(out.good());
        auto [dev, err] = out.detach();
        VERIFY(dev.str() == L"ffffff42");
    }

    dump_info("Done\n");
}
