#include <cstddef>
#include <limits>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_base.h>
#include <io/ostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

namespace
{
    template <typename TFn>
    bool throws_stream_error(TFn&& fn)
    {
        try { fn(); }
        catch (const IOv2::stream_error&) { return true; }
        return false;
    }
}

// The setter rejects negative widths, including the ones that arrive already
// wrapped as unsigned, and leaves the previous width in place when it does.
void test_io_base_width_1()
{
    dump_info("Test ios_base<char> width case 1...");

    IOv2::ios_base<char> ios;
    VERIFY(ios.width() == 0);

    VERIFY(ios.width(12) == 0);
    VERIFY(ios.width() == 12u);

    VERIFY(throws_stream_error([&ios]{ ios.width(-1); }));
    VERIFY(ios.width() == 12u);

    {
        int n = -3;
        VERIFY(throws_stream_error([&ios, n]{ ios.width(n); }));
    }
    {
        int total = 10;
        std::string label(20, 'x');
        VERIFY(throws_stream_error([&ios, total, &label]{ ios.width(total - label.size()); }));
    }
    {
        size_t w = size_t(10) - size_t(20);
        VERIFY(throws_stream_error([&ios, w]{ ios.width(w); }));
    }
    {
        size_t w = size_t(1) << 63;
        VERIFY(throws_stream_error([&ios, w]{ ios.width(w); }));
    }
    {
        size_t w = std::numeric_limits<size_t>::max();
        VERIFY(throws_stream_error([&ios, w]{ ios.width(w); }));
    }

    VERIFY(ios.width() == 12u);
}

// Valid widths round-trip exactly, including ones a 32-bit field could not hold.
void test_io_base_width_2()
{
    dump_info("Test ios_base<char> width case 2...");

    IOv2::ios_base<char> ios;

    VERIFY(ios.width(0) == 0);
    VERIFY(ios.width() == 0);

    ios.width(50000000);
    VERIFY(ios.width() == 50000000u);

    ios.width(std::ptrdiff_t(1) << 40);
    VERIFY(ios.width() == (size_t(1) << 40));

    const auto pmax = std::numeric_limits<std::ptrdiff_t>::max();
    VERIFY(ios.width(pmax) == (size_t(1) << 40));
    VERIFY(ios.width() == static_cast<size_t>(pmax));

    // A width read back from the getter can be fed to the setter unchanged.
    ios.width(static_cast<std::ptrdiff_t>(ios.width()));
    VERIFY(ios.width() == static_cast<size_t>(pmax));

    ios.width(0);
    VERIFY(ios.width() == 0);
}

// A rejected width does not disturb the stream it was called on.
void test_io_base_width_3()
{
    dump_info("Test ios_base<char> width case 3...");

    IOv2::ostream oss{IOv2::mem_device{""}};
    oss.locale(IOv2::locale<char>("C"));

    oss << "ab";

    VERIFY(throws_stream_error([&oss]{ oss.width(-1); }));
    VERIFY(oss.good());
    VERIFY(oss.width() == 0);

    oss << "cd";
    oss.flush();
    VERIFY(oss.device().str() == "abcd");
}

void test_io_base_width_wchar_t_1()
{
    dump_info("Test ios_base<wchar_t> width case 1...");

    IOv2::ios_base<wchar_t> ios;

    ios.width(7);
    VERIFY(ios.width() == 7u);

    VERIFY(throws_stream_error([&ios]{ ios.width(-1); }));
    VERIFY(ios.width() == 7u);

    ios.width(std::ptrdiff_t(1) << 40);
    VERIFY(ios.width() == (size_t(1) << 40));
    ios.width(0);
}
