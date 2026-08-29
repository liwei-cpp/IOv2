#include <cstddef>
#include <limits>
#include <string>
#include <device/mem_device.h>
#include <io/traits/char_and_str.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

namespace
{
    // setw() only stores its argument; the rejection happens when the manipulator is
    // applied to a stream, so it goes through the stream's error model like any other
    // failure. Returns true when applying the width was rejected.
    template <typename TStream, typename TWidth>
    bool rejects_width(TStream& s, TWidth n)
    {
        const size_t before = s.width();
        s << IOv2::setw(n);
        const bool rejected = s.str_fail() && s.width() == before;
        s.clear();
        return rejected;
    }
}

// Negative widths are rejected, including the ones that reach setw() already
// wrapped as unsigned.
void test_io_base_manipulators_setw_char_1()
{
    dump_info("Test ios_base<char> setw case 1...");

    IOv2::ostream oss{IOv2::mem_device{""}};
    oss.locale(IOv2::locale<char>("C"));

    VERIFY(rejects_width(oss, -1));

    {
        int n = -3;
        VERIFY(rejects_width(oss, n));
    }
    {
        long long n = -1;
        VERIFY(rejects_width(oss, n));
    }

    // The classic form: the subtraction wraps as unsigned before setw() is
    // called, so the value it receives is near 2^64 rather than negative.
    {
        int total = 10;
        std::string label(20, 'x');
        VERIFY(rejects_width(oss, total - label.size()));
    }
    // Same value, computed and stored first.
    {
        int total = 10;
        std::string label(20, 'x');
        size_t w = total - label.size();
        VERIFY(rejects_width(oss, w));
    }
    {
        size_t a = 10, b = 20;
        VERIFY(rejects_width(oss, a - b));
    }
    // A size_t at or above 2^63 maps to a negative ptrdiff_t and is rejected too.
    {
        size_t w = size_t(1) << 63;
        VERIFY(rejects_width(oss, w));
    }
    {
        size_t w = std::numeric_limits<size_t>::max();
        VERIFY(rejects_width(oss, w));
    }

    // Constructing the manipulator on its own never throws: the value is only stored.
    (void)IOv2::setw(-1);
    (void)IOv2::setw(std::numeric_limits<size_t>::max());

    dump_info("Done\n");
}

// A rejected width is reported through the stream, not thrown at the caller: the
// failure bit is set, the width is left alone, and the rest of the expression is
// skipped because the stream is now failed.
void test_io_base_manipulators_setw_char_2()
{
    dump_info("Test ios_base<char> setw case 2...");

    IOv2::ostream oss{IOv2::mem_device{""}};
    oss.locale(IOv2::locale<char>("C"));

    oss << "ab";
    VERIFY(oss.width() == 0);

    // No throw with the default (goodbit) exception mask.
    bool threw = false;
    try { oss << IOv2::setw(-1) << "cd"; }
    catch (const IOv2::stream_error&) { threw = true; }

    VERIFY(!threw);
    VERIFY(!oss.good());
    VERIFY(oss.str_fail());
    VERIFY(oss.width() == 0);

    // "cd" never made it out: the stream was already failed by then. Had the rejection
    // regressed, the insertion would have emitted ~2^64 fill characters instead.
    oss.clear();
    oss.flush();
    VERIFY(oss.device().str() == "ab");

    // With strfailbit in the exception mask the same failure propagates, as everywhere
    // else in the library.
    {
        IOv2::ostream throwing{IOv2::mem_device{""}};
        throwing.locale(IOv2::locale<char>("C"));
        throwing.exceptions(IOv2::ios_defs::strfailbit);

        bool caught = false;
        try { throwing << IOv2::setw(-1); }
        catch (const IOv2::stream_error&) { caught = true; }
        VERIFY(caught);
        VERIFY(throwing.str_fail());
    }

    dump_info("Done\n");
}

// Valid widths are stored exactly, including ones far beyond what a 32-bit
// field could hold; padding and the one-shot consumption still work.
void test_io_base_manipulators_setw_char_3()
{
    dump_info("Test ios_base<char> setw case 3...");

    IOv2::ostream oss{IOv2::mem_device{""}};
    oss.locale(IOv2::locale<char>("C"));

    oss << IOv2::setw(0);
    VERIFY(oss.width() == 0);

    // Applied but never inserted with: these only have to survive unchanged.
    oss << IOv2::setw(50000000);
    VERIFY(oss.width() == 50000000u);

    oss << IOv2::setw(size_t(1) << 40);
    VERIFY(oss.width() == (size_t(1) << 40));

    oss << IOv2::setw(std::numeric_limits<std::ptrdiff_t>::max());
    VERIFY(oss.width() == static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max()));

    oss.width(0);

    oss << IOv2::setfill('*') << IOv2::setw(6) << "ab";
    oss.flush();
    VERIFY(oss.device().str() == "****ab");
    VERIFY(oss.width() == 0);

    dump_info("Done\n");
}

// The extraction side takes width as an upper bound only, so an arbitrarily
// large one is legal and reads no more than the input holds.
void test_io_base_manipulators_setw_char_4()
{
    dump_info("Test ios_base<char> setw case 4...");

    {
        IOv2::istream iss{IOv2::mem_device{"short text"}};
        iss.locale(IOv2::locale<char>("C"));
        std::string s;
        iss >> IOv2::setw(1000000) >> s;
        VERIFY(s == "short");
        VERIFY(iss.width() == 0);
    }
    {
        IOv2::istream iss{IOv2::mem_device{"short text"}};
        iss.locale(IOv2::locale<char>("C"));
        std::string s;
        iss >> IOv2::setw(size_t(1) << 40) >> s;
        VERIFY(s == "short");
    }
    // width still tightens a character array's own bound.
    {
        IOv2::istream iss{IOv2::mem_device{"abcdefghij"}};
        iss.locale(IOv2::locale<char>("C"));
        char buf[8] = {};
        iss >> IOv2::setw(3) >> buf;
        VERIFY(std::string(buf) == "ab");
    }
    {
        IOv2::istream iss{IOv2::mem_device{"abcdefghij"}};
        iss.locale(IOv2::locale<char>("C"));
        char buf[8] = {};
        iss >> IOv2::setw(1000) >> buf;
        VERIFY(std::string(buf) == "abcdefg");
    }

    dump_info("Done\n");
}

void test_io_base_manipulators_setw_wchar_t_1()
{
    dump_info("Test ios_base<wchar_t> setw case 1...");

    IOv2::ostream woss{IOv2::mem_device{L""}};
    woss.locale(IOv2::locale<wchar_t>("C"));

    VERIFY(rejects_width(woss, -1));
    {
        int total = 10;
        std::wstring label(20, L'x');
        VERIFY(rejects_width(woss, total - label.size()));
    }

    woss << IOv2::setw(size_t(1) << 40);
    VERIFY(woss.width() == (size_t(1) << 40));
    woss.width(0);

    woss << IOv2::setfill(L'*') << IOv2::setw(6) << L"ab";
    woss.flush();
    VERIFY(woss.device().str() == L"****ab");
    VERIFY(woss.width() == 0);

    bool threw = false;
    try { woss << IOv2::setw(-1) << L"cd"; }
    catch (const IOv2::stream_error&) { threw = true; }
    VERIFY(!threw);
    VERIFY(woss.str_fail());
    woss.clear();
    woss.flush();
    VERIFY(woss.device().str() == L"****ab");

    dump_info("Done\n");
}
