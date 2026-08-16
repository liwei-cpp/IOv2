#include <cstddef>
#include <limits>
#include <string>
#include <device/mem_device.h>
#include <io/traits/char_and_str.h>
#include <io/traits/arithmetic.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

namespace
{
    // Applies setbase(base) and returns what the stream then makes of 255. The stream is
    // required to stay good throughout: no value of `base` is an error.
    template <typename TBase>
    std::string based(TBase base)
    {
        IOv2::ostream oss{IOv2::mem_device{""}};
        oss.locale(IOv2::locale<char>("C"));

        oss << IOv2::setbase(base) << 255;
        VERIFY(oss.good());

        auto [dev, err] = oss.detach();
        return dev.str();
    }

    IOv2::ios_defs::fmtflags base_of(std::ptrdiff_t base)
    {
        IOv2::ostream oss{IOv2::mem_device{""}};
        oss.locale(IOv2::locale<char>("C"));

        oss << IOv2::setbase(base);
        VERIFY(oss.good());

        return oss.flags() & IOv2::ios_defs::basefield;
    }
}

// 8, 10 and 16 select oct / dec / hex; every other value clears basefield, which is not an
// error and leaves the stream good.
void test_io_base_manipulators_setbase_char_1()
{
    dump_info("Test ios_base<char> setbase case 1...");

    VERIFY(base_of(8)  == IOv2::ios_defs::oct);
    VERIFY(base_of(10) == IOv2::ios_defs::dec);
    VERIFY(base_of(16) == IOv2::ios_defs::hex);

    VERIFY(based(8)  == "377");
    VERIFY(based(10) == "255");
    VERIFY(based(16) == "ff");

    // Anything else clears basefield outright. Output is then decimal, same as dec, but the
    // flag state is distinct from what setbase(10) leaves behind.
    for (std::ptrdiff_t base : {std::ptrdiff_t{0}, std::ptrdiff_t{1}, std::ptrdiff_t{2},
                                std::ptrdiff_t{7}, std::ptrdiff_t{9}, std::ptrdiff_t{15},
                                std::ptrdiff_t{17}, std::ptrdiff_t{-8}})
    {
        VERIFY(base_of(base) == IOv2::ios_defs::fmtflags(0));
        VERIFY(based(base) == "255");
    }

    dump_info("Done\n");
}

// The parameter is a std::ptrdiff_t so that a wider argument cannot be truncated onto 8, 10
// or 16 and thereby look like it selected that base.
void test_io_base_manipulators_setbase_char_2()
{
    dump_info("Test ios_base<char> setbase case 2...");

    // Each of these has the low 32 bits of a valid base. Narrowed to int they would select
    // oct / dec / hex; kept whole they are just "some other value".
    VERIFY(base_of(8LL  + (1LL << 32)) == IOv2::ios_defs::fmtflags(0));
    VERIFY(base_of(10LL + (1LL << 32)) == IOv2::ios_defs::fmtflags(0));
    VERIFY(base_of(16LL + (1LL << 32)) == IOv2::ios_defs::fmtflags(0));

    VERIFY(based(8LL  + (1LL << 32)) == "255");
    VERIFY(based(10LL + (1LL << 32)) == "255");
    VERIFY(based(16LL + (1LL << 32)) == "255");

    // The stored value is the one that was passed in, not a truncation of it.
    VERIFY(IOv2::setbase(8LL + (1LL << 32)).m_base == 8LL + (1LL << 32));
    VERIFY(IOv2::setbase(-1).m_base == -1);

    // Extreme values are stored and applied like any other non-base, without failing.
    VERIFY(base_of(std::numeric_limits<std::ptrdiff_t>::max()) == IOv2::ios_defs::fmtflags(0));
    VERIFY(base_of(std::numeric_limits<std::ptrdiff_t>::min()) == IOv2::ios_defs::fmtflags(0));

    dump_info("Done\n");
}

// setbase never fails, so a stream with strfailbit in its exception mask sees nothing thrown
// and stays good, and a later insertion still goes out.
void test_io_base_manipulators_setbase_char_3()
{
    dump_info("Test ios_base<char> setbase case 3...");

    IOv2::ostream oss{IOv2::mem_device{""}};
    oss.locale(IOv2::locale<char>("C"));
    oss.exceptions(IOv2::ios_defs::strfailbit);

    bool caught = false;
    try { oss << IOv2::setbase(7) << 255 << IOv2::setbase(16) << 255; }
    catch (const IOv2::stream_error&) { caught = true; }

    VERIFY(!caught);
    VERIFY(oss.good());

    oss.flush();
    VERIFY(oss.device().str() == "255ff");

    dump_info("Done\n");
}

// The extraction direction goes through the same apply(): 16 reads hex, and a cleared
// basefield makes the base come from the text itself.
void test_io_base_manipulators_setbase_char_4()
{
    dump_info("Test ios_base<char> setbase case 4...");

    // The last token in each device runs to the end of the input, so eofbit is set by the
    // time the value is out; what matters here is that nothing *failed*.
    {
        IOv2::istream iss{IOv2::mem_device{std::string("ff")}};
        iss.locale(IOv2::locale<char>("C"));

        long v = 0;
        iss >> IOv2::setbase(16) >> v;
        VERIFY(!iss.str_fail());
        VERIFY(v == 255);
    }

    // setbase(0) clears basefield, so each token is read by its own prefix.
    {
        IOv2::istream iss{IOv2::mem_device{std::string("0x1f 017 42")}};
        iss.locale(IOv2::locale<char>("C"));

        long hex = 0, oct = 0, dec = 0;
        iss >> IOv2::setbase(0) >> hex >> oct >> dec;
        VERIFY(!iss.str_fail());
        VERIFY(hex == 31);
        VERIFY(oct == 15);
        VERIFY(dec == 42);
    }

    // A truncating argument must not turn this into a hex read.
    {
        IOv2::istream iss{IOv2::mem_device{std::string("017")}};
        iss.locale(IOv2::locale<char>("C"));

        long v = 0;
        iss >> IOv2::setbase(16LL + (1LL << 32)) >> v;
        VERIFY(!iss.str_fail());
        VERIFY(v == 15);        // read as octal by its leading 0, not as 0x17
    }

    dump_info("Done\n");
}

void test_io_base_manipulators_setbase_wchar_t_1()
{
    dump_info("Test ios_base<wchar_t> setbase case 1...");

    IOv2::ostream woss{IOv2::mem_device{L""}};
    woss.locale(IOv2::locale<wchar_t>("C"));

    woss << IOv2::setbase(16) << 255;
    VERIFY((woss.flags() & IOv2::ios_defs::basefield) == IOv2::ios_defs::hex);

    woss << L' ' << IOv2::setbase(16LL + (1LL << 32)) << 255;
    VERIFY((woss.flags() & IOv2::ios_defs::basefield) == IOv2::ios_defs::fmtflags(0));

    VERIFY(woss.good());
    auto [dev, err] = woss.detach();
    VERIFY(dev.str() == L"ff 255");

    dump_info("Done\n");
}
