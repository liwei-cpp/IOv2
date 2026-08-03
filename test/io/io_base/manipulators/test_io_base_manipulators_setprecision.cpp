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
    // setprecision() only stores its argument; the range check happens when the
    // manipulator is applied to a stream, so it goes through the stream's error model
    // like any other failure. Returns true when applying the precision was rejected.
    template <typename TStream, typename TPrec>
    bool rejects_precision(TStream& s, TPrec n)
    {
        const std::uint8_t before = s.precision();
        s << IOv2::setprecision(n);
        const bool rejected = s.str_fail() && s.precision() == before;
        s.clear();
        return rejected;
    }
}

// The whole 0..255 range is accepted and stored exactly; anything above it is rejected
// rather than wrapping into the low byte.
void test_io_base_manipulators_setprecision_char_1()
{
    dump_info("Test ios_base<char> setprecision case 1...");

    IOv2::ostream oss{IOv2::mem_device{""}};
    oss.locale(IOv2::locale<char>("C"));

    oss << IOv2::setprecision(0);
    VERIFY(oss.precision() == 0);

    oss << IOv2::setprecision(17);
    VERIFY(oss.precision() == 17);

    oss << IOv2::setprecision(255);
    VERIFY(oss.precision() == 255);

    // 300 would become 44 if it were narrowed instead of checked.
    VERIFY(rejects_precision(oss, 300));
    VERIFY(oss.precision() == 255);

    VERIFY(rejects_precision(oss, 256));
    VERIFY(rejects_precision(oss, std::numeric_limits<size_t>::max()));
    {
        size_t n = 1000;
        VERIFY(rejects_precision(oss, n));
    }

    // Constructing the manipulator on its own never throws: the value is only stored.
    (void)IOv2::setprecision(300);
    (void)IOv2::setprecision(std::numeric_limits<size_t>::max());
}

// An out-of-range precision is reported through the stream, not thrown at the caller.
void test_io_base_manipulators_setprecision_char_2()
{
    dump_info("Test ios_base<char> setprecision case 2...");

    IOv2::ostream oss{IOv2::mem_device{""}};
    oss.locale(IOv2::locale<char>("C"));

    oss << "ab";

    bool threw = false;
    try { oss << IOv2::setprecision(300) << "cd"; }
    catch (const IOv2::stream_error&) { threw = true; }

    VERIFY(!threw);
    VERIFY(!oss.good());
    VERIFY(oss.str_fail());

    // "cd" never made it out: the stream was already failed by then.
    oss.clear();
    oss.flush();
    VERIFY(oss.device().str() == "ab");

    // With strfailbit in the exception mask the same failure propagates.
    {
        IOv2::ostream throwing{IOv2::mem_device{""}};
        throwing.locale(IOv2::locale<char>("C"));
        throwing.exceptions(IOv2::ios_defs::strfailbit);

        bool caught = false;
        try { throwing << IOv2::setprecision(300); }
        catch (const IOv2::stream_error&) { caught = true; }
        VERIFY(caught);
        VERIFY(throwing.str_fail());
    }
}

// The extraction direction goes through the same check.
void test_io_base_manipulators_setprecision_char_3()
{
    dump_info("Test ios_base<char> setprecision case 3...");

    IOv2::istream iss{IOv2::mem_device{std::string("1.5")}};
    iss.locale(IOv2::locale<char>("C"));

    iss >> IOv2::setprecision(10);
    VERIFY(iss.precision() == 10);

    iss >> IOv2::setprecision(300);
    VERIFY(iss.str_fail());
    VERIFY(iss.precision() == 10);
    iss.clear();
}

void test_io_base_manipulators_setprecision_wchar_t_1()
{
    dump_info("Test ios_base<wchar_t> setprecision case 1...");

    IOv2::ostream woss{IOv2::mem_device{L""}};
    woss.locale(IOv2::locale<wchar_t>("C"));

    woss << IOv2::setprecision(255);
    VERIFY(woss.precision() == 255);

    VERIFY(rejects_precision(woss, 300));
    VERIFY(woss.precision() == 255);
}
