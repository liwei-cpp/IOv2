#include <limits>
#include <stdexcept>
#include <system_error>
#include <string>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/ostream.h>
#include <common/dump_info.h>

void test_io_base_storage_1()
{
    dump_info("Test ios_base storage case 1...");

    IOv2::ios_base<char>::xalloc();
    IOv2::ios_base<char>::xalloc();
    IOv2::ios_base<wchar_t>::xalloc();
    auto x4 = IOv2::ios_base<wchar_t>::xalloc();

    IOv2::ostream out(IOv2::mem_device{"the element of crime, lars von trier"});
    out.get_pword(++x4); // should not crash

    dump_info("Done\n");
}

void test_io_base_storage_2()
{
    dump_info("Test ios_base storage case 2...");

    IOv2::ios_base<char> io;
    io.set_pword(1, nullptr);
    if (io.get_pword(1) != nullptr) throw std::runtime_error("ios_base storage check fail");
    
    int max = std::numeric_limits<int>::max() - 1;
    if (io.get_pword(max) != nullptr) throw std::runtime_error("ios_base storage check fail");

    dump_info("Done\n");
}

namespace
{
    class derived : public IOv2::ios_base<char>
    {
    public:
        derived() {}
    };
}

void test_io_base_storage_3()
{
    dump_info("Test ios_base storage case 3...");

    IOv2::ios_base<char> io;
    
    auto d = std::make_shared<derived>();
    io.set_pword(1, d);
    if (io.get_pword(1) != d) throw std::runtime_error("ios_base storage check fail");

    dump_info("Done\n");
}