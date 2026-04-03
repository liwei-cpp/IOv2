#include <cstring>

#include <device/std_device.h>

#include <common/dump_info.h>
#include <common/stdio_guard.h>

void test_std_device_gen_1()
{
    using namespace IOv2;

    dump_info("Test std_device general 1...");
    static_assert(IOv2::io_device<std_device<STDIN_FILENO>>);
    static_assert(IOv2::io_device<std_device<STDOUT_FILENO>>);
    static_assert(IOv2::io_device<std_device<STDERR_FILENO>>);

    static_assert(std::is_same_v<std_device<STDIN_FILENO>::char_type, char>);
    static_assert(std::is_same_v<std_device<STDOUT_FILENO>::char_type, char>);
    static_assert(std::is_same_v<std_device<STDERR_FILENO>::char_type, char>);

    {
        using CheckType = std_device<STDOUT_FILENO>;
        static_assert(!IOv2::dev_cpt::support_positioning<CheckType>);
        static_assert(IOv2::dev_cpt::support_put<CheckType>);
        static_assert(!IOv2::dev_cpt::support_get<CheckType>);
    }

    {
        using CheckType = std_device<STDERR_FILENO>;
        static_assert(!IOv2::dev_cpt::support_positioning<CheckType>);
        static_assert(IOv2::dev_cpt::support_put<CheckType>);
        static_assert(!IOv2::dev_cpt::support_get<CheckType>);
    }
    
    {
        using CheckType = std_device<STDIN_FILENO>;
        static_assert(!IOv2::dev_cpt::support_positioning<CheckType>);
        static_assert(!IOv2::dev_cpt::support_put<CheckType>);
        static_assert(IOv2::dev_cpt::support_get<CheckType>);
    }
    dump_info("Done\n");
}

void test_std_device_input_1()
{
    using namespace IOv2;

    dump_info("Test std_device input case 1...");
    
    const char* c_lit = "black pearl jasmine tea";
    
    std_device<STDIN_FILENO> obj;
    
    char buf[5] = {0};
    iguard g(c_lit);
    if (obj.dget(buf, 1) != 1 || buf[0] != c_lit[0]) throw std::runtime_error("std_device.dget() fails");
    if (obj.dget(buf, 1) != 1 || buf[0] != c_lit[1]) throw std::runtime_error("std_device.dget() fails");

    memset(buf, 'x', 5);
    if (obj.dget(buf, 5) != 5) throw std::runtime_error("std_device.dget() fails");
    if (memcmp(buf, c_lit + 2, 5) != 0) throw std::runtime_error("std_device.dget() fails");
    if (obj.dget(buf, 1) != 1 || buf[0] != c_lit[7]) throw std::runtime_error("std_device.dget() fails");

    dump_info("Done\n");
}

void test_std_device_output_1()
{
    using namespace IOv2;

    dump_info("Test std_device output case 1...");

    {
        oguard<true> g;
        std_device<STDOUT_FILENO> obj;
        if (!g.contents().empty()) throw std::runtime_error("test setup fails");
        
        obj.dput("a", 1);
        obj.dput("bcdef", 5);
        obj.dflush();
        if (g.contents() != "abcdef") throw std::runtime_error("std_device::put fails");
    }
    
    {
        oguard<false> g;
        std_device<STDERR_FILENO> obj;
        if (!g.contents().empty()) throw std::runtime_error("test setup fails");
        
        obj.dput("a", 1);
        obj.dput("bcdef", 5);
        obj.dflush();
        if (g.contents() != "abcdef") throw std::runtime_error("std_device::put fails");
    }

    dump_info("Done\n");
}

void test_std_device_output_2()
{
    using namespace IOv2;

    dump_info("Test std_device output case 2...");

    const std::string test_file = "out_test.txt";
    {
        oguard<true> g;
        std_device<STDOUT_FILENO> obj;
        if (!g.contents().empty()) throw std::runtime_error("test setup fails");
        
        obj.dput("a", 1);
        obj.dflush();
        obj.dput("bcdef", 5);
        if (g.contents()[0] != 'a') throw std::runtime_error("std_device::put fails");
        obj.dflush();
        if (g.contents() != "abcdef") throw std::runtime_error("std_device::put fails");
    }
    
    {
        oguard<false> g;
        std_device<STDERR_FILENO> obj;
        if (!g.contents().empty()) throw std::runtime_error("test setup fails");
        
        obj.dput("a", 1);
        obj.dflush();
        obj.dput("bcdef", 5);
        if (g.contents() != "abcdef") throw std::runtime_error("std_device::put fails");
    }

    dump_info("Done\n");
}