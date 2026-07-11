#include <limits>
#include <stdexcept>
#include <system_error>
#include <string>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/ostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

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
    VERIFY(io.get_pword(1) == nullptr);

    int max = std::numeric_limits<int>::max() - 1;
    VERIFY(io.get_pword(max) == nullptr);

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
    VERIFY(io.get_pword(1) == d);

    dump_info("Done\n");
}

void test_io_base_storage_4()
{
    dump_info("Test ios_base storage case 4...");

    // Exercise set_pword() on an id that already has an entry (the "else"
    // branch): replacing returns the previous value; setting nullptr erases the
    // entry and likewise returns the previous value.
    IOv2::ios_base<char> io;

    auto d1 = std::make_shared<derived>();
    auto d2 = std::make_shared<derived>();

    // First insert (id absent): returns nullptr.
    VERIFY(io.set_pword(1, d1) == nullptr);
    VERIFY(io.get_pword(1) == d1);

    // Replace an existing entry: returns the previous value, stores the new one.
    auto prev = io.set_pword(1, d2);
    VERIFY(prev == d1);
    VERIFY(io.get_pword(1) == d2);

    // Erase an existing entry via nullptr: returns the previous value, removes it.
    auto prev2 = io.set_pword(1, nullptr);
    VERIFY(prev2 == d2);
    VERIFY(io.get_pword(1) == nullptr);

    dump_info("Done\n");
}