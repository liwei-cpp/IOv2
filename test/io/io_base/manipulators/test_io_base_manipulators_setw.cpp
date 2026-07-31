#include <cstddef>
#include <limits>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

namespace
{
    // setw() throws while the argument is being evaluated, i.e. before any stream
    // is touched, so the check cannot go through a stream's state bits.
    template <typename TFn>
    bool throws_stream_error(TFn&& fn)
    {
        try { fn(); }
        catch (const IOv2::stream_error&) { return true; }
        return false;
    }
}

// Negative widths are rejected, including the ones that reach setw() already
// wrapped as unsigned.
void test_io_base_manipulators_setw_char_1()
{
    dump_info("Test ios_base<char> setw case 1...");

    VERIFY(throws_stream_error([]{ (void)IOv2::setw(-1); }));

    {
        int n = -3;
        VERIFY(throws_stream_error([n]{ (void)IOv2::setw(n); }));
    }
    {
        long long n = -1;
        VERIFY(throws_stream_error([n]{ (void)IOv2::setw(n); }));
    }

    // The classic form: the subtraction wraps as unsigned before setw() is
    // called, so the value it receives is near 2^64 rather than negative.
    {
        int total = 10;
        std::string label(20, 'x');
        VERIFY(throws_stream_error([total, &label]{ (void)IOv2::setw(total - label.size()); }));
    }
    // Same value, computed and stored first.
    {
        int total = 10;
        std::string label(20, 'x');
        size_t w = total - label.size();
        VERIFY(throws_stream_error([w]{ (void)IOv2::setw(w); }));
    }
    {
        size_t a = 10, b = 20;
        VERIFY(throws_stream_error([a, b]{ (void)IOv2::setw(a - b); }));
    }
    // A size_t at or above 2^63 maps to a negative ptrdiff_t and is rejected too.
    {
        size_t w = size_t(1) << 63;
        VERIFY(throws_stream_error([w]{ (void)IOv2::setw(w); }));
    }
    {
        size_t w = std::numeric_limits<size_t>::max();
        VERIFY(throws_stream_error([w]{ (void)IOv2::setw(w); }));
    }
}

// A rejected width leaves the stream completely untouched: the throw happens
// during argument evaluation, so the whole expression is abandoned.
void test_io_base_manipulators_setw_char_2()
{
    dump_info("Test ios_base<char> setw case 2...");

    IOv2::ostream oss{IOv2::mem_device{""}};
    oss.locale(IOv2::locale<char>("C"));

    oss << "ab";
    VERIFY(oss.width() == 0);

    // Checked on its own first: should the rejection ever regress, the insertion
    // below would emit ~2^64 fill characters and hang rather than fail here.
    VERIFY(throws_stream_error([]{ (void)IOv2::setw(-1); }));

    bool threw = false;
    try { oss << IOv2::setw(-1) << "cd"; }
    catch (const IOv2::stream_error&) { threw = true; }

    VERIFY(threw);
    VERIFY(oss.good());
    VERIFY(oss.width() == 0);

    oss.flush();
    VERIFY(oss.device().str() == "ab");
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
}

void test_io_base_manipulators_setw_wchar_t_1()
{
    dump_info("Test ios_base<wchar_t> setw case 1...");

    VERIFY(throws_stream_error([]{ (void)IOv2::setw(-1); }));
    {
        int total = 10;
        std::wstring label(20, L'x');
        VERIFY(throws_stream_error([total, &label]{ (void)IOv2::setw(total - label.size()); }));
    }

    IOv2::ostream woss{IOv2::mem_device{L""}};
    woss.locale(IOv2::locale<wchar_t>("C"));

    woss << IOv2::setw(size_t(1) << 40);
    VERIFY(woss.width() == (size_t(1) << 40));
    woss.width(0);

    woss << IOv2::setfill(L'*') << IOv2::setw(6) << L"ab";
    woss.flush();
    VERIFY(woss.device().str() == L"****ab");
    VERIFY(woss.width() == 0);

    // Same guard as in the char case: assert the rejection before an insertion
    // expression is allowed to rely on it.
    VERIFY(throws_stream_error([]{ (void)IOv2::setw(-1); }));

    bool threw = false;
    try { woss << IOv2::setw(-1) << L"cd"; }
    catch (const IOv2::stream_error&) { threw = true; }
    VERIFY(threw);
    VERIFY(woss.good());
    VERIFY(woss.device().str() == L"****ab");
}
