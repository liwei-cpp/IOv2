#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/file_guard.h>
#include <common/verify.h>

namespace
{
struct DevIstream : public IOv2::istream<IOv2::mem_device<char>, char>
{
    using IOv2::istream<IOv2::mem_device<char>, char>::istream;
    int x = 20;
};

struct DevIOstream : public IOv2::istream<IOv2::mem_device<char>, char>
{
    using IOv2::istream<IOv2::mem_device<char>, char>::istream;
    int x = 50;
};
}

void test_istream_derive_1()
{
    dump_info("Test ostream derive case 1...");

    DevIstream obj1(IOv2::mem_device("hello"));
    // make sure the >> operator should return DevIstream object
    std::string str;
    VERIFY((obj1 >> str).x == 20);

    DevIOstream obj2(IOv2::mem_device{"hello"});
    // make sure the >> operator should return DevIOstream object
    VERIFY((obj2 >> str).x == 50);

    dump_info("Done\n");
}

void test_istream_derive()
{
    test_istream_derive_1();
}