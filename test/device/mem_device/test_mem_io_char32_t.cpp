#include <stdexcept>
#include <tuple>
#include <device/mem_device.h>

#include <common/dump_info.h>
#include <common/verify.h>

void test_mem_device_char32_t_gen_1()
{
    dump_info("Test mem_device<char32_t> general 1...");
    static_assert(IOv2::io_device<IOv2::mem_device<char32_t>>);
    static_assert(std::is_same_v<IOv2::mem_device<char32_t>::char_type, char32_t>);
    
    using CheckType = IOv2::mem_device<char32_t>;
    static_assert(IOv2::dev_cpt::support_positioning<CheckType>);
    static_assert(IOv2::dev_cpt::support_put<CheckType>);
    static_assert(IOv2::dev_cpt::support_get<CheckType>);
    dump_info("Done\n");
}

void test_mem_device_char32_t_gen_2()
{
    dump_info("Test mem_device<char32_t> general 2...");
    
    IOv2::mem_device<char32_t> sbuf;
    if (!sbuf.str().empty()) throw std::runtime_error("mem_device<char32_t>::str() fails");
    
    const std::u32string str = U"This is my boomstick!";
    IOv2::mem_device<char32_t> sbuf2(str);
    if (sbuf2.str() != str) throw std::runtime_error("mem_device<char32_t>::str() fails");
    
    char32_t ch;
    if ((sbuf2.dget(&ch, 1) != 1) || (ch != str[0])) throw std::runtime_error("mem_device<char32_t> input get() fail");
    sbuf2.dput(U"Y", 1);

    dump_info("Done\n");
}

void test_mem_device_char32_t_gen_3()
{
    dump_info("Test mem_device<char32_t> general 3...");
    
    {
        IOv2::mem_device<char32_t> obj;
        if (obj.str() != U"") throw std::runtime_error("mem_device<char32_t> input constructor fail");
        if (obj.dtell() != 0) throw std::runtime_error("mem_device<char32_t> input tell fail");

        obj = IOv2::mem_device{U"Hello world"};
        if (obj.str() != U"Hello world") throw std::runtime_error("mem_device<char32_t> input str() fail");
        if (obj.dtell() != 0) throw std::runtime_error("mem_device<char32_t> input tell fail");
    }
    
    {
        std::u32string ref = U"Hello world";
        
        IOv2::mem_device<char32_t> obj(ref);
        if (obj.str() != ref) throw std::runtime_error("mem_device<char32_t> input constructor fail");
        if (obj.dtell() != 0) throw std::runtime_error("mem_device<char32_t> input tell fail");

        ref += U"123";
        obj = IOv2::mem_device{ref};
        if (obj.str() != ref) throw std::runtime_error("mem_device<char32_t> input str() fail");
        if (obj.dtell() != 0) throw std::runtime_error("mem_device<char32_t> input tell fail");
    }
    dump_info("Done\n");
}

void test_mem_device_char32_t_in_1()
{
    dump_info("Test mem_device<char32_t> input case 1...");
    IOv2::mem_device<char32_t> obj(U"12");
    char32_t ch = 0;
    if ((obj.dget(&ch, 1) != 1) || (ch != U'1')) throw std::runtime_error("mem_device<char32_t> input get() fail");
    if (obj.dtell() != 1) throw std::runtime_error("mem_device<char32_t> input tell fail");

    if ((obj.dget(&ch, 1) != 1) || (ch != U'2')) throw std::runtime_error("mem_device<char32_t> input get() fail");
    if (obj.dtell() != 2) throw std::runtime_error("mem_device<char32_t> input tell fail");

    if (obj.dget(&ch, 1) != 0) throw std::runtime_error("mem_device<char32_t> input get() fail");
    if (obj.dtell() != 2) throw std::runtime_error("mem_device<char32_t> input tell fail");

    dump_info("Done\n");
}

void test_mem_device_char32_t_in_2()
{
    dump_info("Test mem_device<char32_t> input case 2...");

    {
        IOv2::mem_device<char32_t> obj(U"12345");
        char32_t buf[5];
        size_t read_num = obj.dget(buf, 5);
        if (read_num != 5) throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[0] != U'1') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[1] != U'2') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[2] != U'3') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[3] != U'4') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[4] != U'5') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (obj.dtell() != 5) throw std::runtime_error("mem_device<char32_t> input tell fail");
    }
    
    {
        IOv2::mem_device<char32_t> obj(U"12345");
        char32_t buf[5];
        size_t read_num = obj.dget(buf, 3);
        if (read_num != 3) throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[0] != U'1') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[1] != U'2') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[2] != U'3') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (obj.dtell() != 3) throw std::runtime_error("mem_device<char32_t> input tell fail");

        read_num = obj.dget(buf, 2);
        if (read_num != 2) throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[0] != U'4') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[1] != U'5') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (obj.dtell() != 5) throw std::runtime_error("mem_device<char32_t> input tell fail");
    }

    {
        IOv2::mem_device<char32_t> obj(U"12345");
        char32_t buf[10];
        size_t read_num = obj.dget(buf, 10);
        if (read_num != 5) throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[0] != U'1') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[1] != U'2') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[2] != U'3') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[3] != U'4') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[4] != U'5') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (obj.dtell() != 5) throw std::runtime_error("mem_device<char32_t> input tell fail");

        read_num = obj.dget(buf, 10);
        if (read_num != 0) throw std::runtime_error("mem_device<char32_t> input get() fail");
    }

    {
        IOv2::mem_device<char32_t> obj(U"12345");
        char32_t buf[5];
        size_t read_num = obj.dget(buf, 3);
        if (read_num != 3) throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[0] != U'1') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[1] != U'2') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[2] != U'3') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (obj.dtell() != 3) throw std::runtime_error("mem_device<char32_t> input tell fail");
        
        read_num = obj.dget(buf, 5);
        if (read_num != 2) throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[0] != U'4') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[1] != U'5') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (obj.dtell() != 5) throw std::runtime_error("mem_device<char32_t> input tell fail");

        read_num = obj.dget(buf, 10);
        if (read_num != 0) throw std::runtime_error("mem_device<char32_t> input get() fail");
    }
    
    dump_info("Done\n");
}

void test_mem_device_char32_t_in_3()
{
    dump_info("Test mem_device<char32_t> input case 3...");

    {
        IOv2::mem_device<char32_t> obj(U"12345");
        obj.dseek(3);
        char32_t buf[5];
        size_t read_num = obj.dget(buf, 5);
        if (read_num != 2) throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[0] != U'4') throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[1] != U'5') throw std::runtime_error("mem_device<char32_t> input get() fail");
      
        obj.dseek(obj.dtell() - 1);
        read_num = obj.dget(buf, 5);
        if (read_num != 1) throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (buf[0] != U'5') throw std::runtime_error("mem_device<char32_t> input get() fail");
    }
    
    {
        IOv2::mem_device<char32_t> obj(U"12345");
        obj.dseek(2);
        
        char32_t ch = 0;
        if ((obj.dget(&ch, 1) != 1) || (ch != U'3')) throw std::runtime_error("mem_device<char32_t> input get() fail");
        
        obj.dseek(1);
        if ((obj.dget(&ch, 1) != 1) || (ch != U'2')) throw std::runtime_error("mem_device<char32_t> input get() fail");
        
        obj.dseek(obj.dtell() + 2);
        if ((obj.dget(&ch, 1) != 1) || (ch != U'5')) throw std::runtime_error("mem_device<char32_t> input get() fail");
        
        obj.drseek(3);
        if ((obj.dget(&ch, 1) != 1) || (ch != U'3')) throw std::runtime_error("mem_device<char32_t> input get() fail");
    }
    
    {
        IOv2::mem_device<char32_t> obj(U"12345");
        FAIL_SEEK(obj, 100);
        if (obj.dtell() != 0L) throw std::runtime_error("mem_device<char32_t> input tell() fail");
        
        obj.dseek(3);

        FAIL_SEEK(obj, 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> input tell() fail");

        FAIL_SEEK(obj, 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> input tell() fail");

        FAIL_SEEK(obj, -100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> input tell() fail");

        FAIL_SEEK(obj, obj.dtell() + 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> input tell() fail");

        FAIL_SEEK(obj, obj.dtell() - 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> input tell() fail");

        FAIL_RSEEK(obj, -100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> input tell() fail");

        FAIL_RSEEK(obj, 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> input tell() fail");
    }
    dump_info("Done\n");
}

void test_mem_device_char32_t_out_1()
{
    dump_info("Test mem_device<char32_t> output case 1...");

    IOv2::mem_device<char32_t> obj(U"12"); obj.drseek(0);
    obj.dput(U"x", 1);
    if (obj.str() != U"12x") throw std::runtime_error("mem_device<char32_t> output put() fail");
    if (obj.dtell() != 3) throw std::runtime_error("mem_device<char32_t> output tell() fail");

    obj = IOv2::mem_device(U"12");
    obj.dput(U"x", 1);
    if (obj.str() != U"x2") throw std::runtime_error("mem_device<char32_t> output put() fail");
    if (obj.dtell() != 1) throw std::runtime_error("mem_device<char32_t> output tell fail");
    
    obj = IOv2::mem_device<char32_t>{};
    obj.dput(U"y", 1);
    if (obj.str() != U"y") throw std::runtime_error("mem_device<char32_t> output put() fail");
    if (obj.dtell() != 1) throw std::runtime_error("mem_device<char32_t> output tell() fail");

    dump_info("Done\n");
}

void test_mem_device_char32_t_out_2()
{
    dump_info("Test mem_device<char32_t> output case 2...");

    {
        IOv2::mem_device<char32_t> obj;
        obj.dput(U"12345", 5);
        if (obj.str() != U"12345") throw std::runtime_error("mem_device<char32_t> output put() fail");
        if (obj.dtell() != 5) throw std::runtime_error("mem_device<char32_t> output tell() fail");
    }
    
    {
        IOv2::mem_device<char32_t> obj;
        obj.dput(nullptr, 0);
        if (obj.str() != U"") throw std::runtime_error("mem_device<char32_t> output put() fail");
        if (obj.dtell() != 0) throw std::runtime_error("mem_device<char32_t> output tell() fail");
    }
    
    {
        IOv2::mem_device<char32_t> obj;
        obj.dput(U"123", 3);
        if (obj.str() != U"123") throw std::runtime_error("mem_device<char32_t> output put() fail");
        if (obj.dtell() != 3) throw std::runtime_error("mem_device<char32_t> output tell() fail");

        obj.dput(U"45", 2);
        if (obj.str() != U"12345") throw std::runtime_error("mem_device<char32_t> output put() fail");
        if (obj.dtell() != 5) throw std::runtime_error("mem_device<char32_t> output tell() fail");
    }

    {
        IOv2::mem_device<char32_t> obj;
        obj.dput(U"123", 3);
        if (obj.str() != U"123") throw std::runtime_error("mem_device<char32_t> output put() fail");

        obj.dput(U"x", 1);
        if (obj.str() != U"123x") throw std::runtime_error("mem_device<char32_t> output put() fail");

        obj.dput(U"45", 2);
        if (obj.str() != U"123x45") throw std::runtime_error("mem_device<char32_t> output put() fail");

        if (obj.dtell() != 6) throw std::runtime_error("mem_device<char32_t> output tell() fail");
    }

    dump_info("Done\n");
}

void test_mem_device_char32_t_out_3()
{
    dump_info("Test mem_device<char32_t> output case 3...");

    {
        IOv2::mem_device<char32_t> obj(U"12345");
        obj.dseek(3);
        obj.dput(U"ab", 2);
        if (obj.str() != U"123ab") throw std::runtime_error("mem_device<char32_t> output put() fail");
      
        obj.dseek(obj.dtell() - 1);
        obj.dput(U"x", 1);
        if (obj.str() != U"123ax") throw std::runtime_error("mem_device<char32_t> output put() fail");
    }
    
    {
        IOv2::mem_device<char32_t> obj(U"12345");
        obj.dseek(2);
        obj.dput(U"x", 1);
        if (obj.str() != U"12x45") throw std::runtime_error("mem_device<char32_t> output put() fail");
        
        obj.dseek(1);
        obj.dput(U"y", 1);
        if (obj.str() != U"1yx45") throw std::runtime_error("mem_device<char32_t> output put() fail");
        
        obj.dseek(obj.dtell() + 2);
        obj.dput(U"z", 1);
        if (obj.str() != U"1yx4z") throw std::runtime_error("mem_device<char32_t> output put() fail");

        obj.drseek(3);
        obj.dput(U"a", 1);
        if (obj.str() != U"1ya4z") throw std::runtime_error("mem_device<char32_t> output put() fail");
    }
    
    {
        IOv2::mem_device<char32_t> obj(U"12345"); obj.drseek(0);
        FAIL_SEEK(obj, 100);
        if (obj.dtell() != 5L) throw std::runtime_error("mem_device<char32_t> output tell() fail");

        obj.dseek(3);

        FAIL_SEEK(obj, 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> output tell() fail");

        FAIL_SEEK(obj, 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> output tell() fail");

        FAIL_SEEK(obj, -100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> output tell() fail");

        FAIL_SEEK(obj, obj.dtell() + 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> output tell() fail");

        FAIL_SEEK(obj, obj.dtell() - 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> output tell() fail");

        FAIL_RSEEK(obj, -100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> output tell() fail");

        FAIL_RSEEK(obj, 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> output tell() fail");
    }

    dump_info("Done\n");
}

void test_mem_device_char32_t_io_1()
{
    dump_info("Test mem_device<char32_t> input/output case 1...");
    {
        IOv2::mem_device<char32_t> obj;
        if (obj.dtell() != 0L) throw std::runtime_error("mem_device<char32_t> IO tell() fail");
        
        obj.dput(U"123", 3);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> IO tell() fail");
        
        obj.dseek(0);
        char32_t ch = 0;
        if ((obj.dget(&ch, 1) != 1) || (ch != U'1')) throw std::runtime_error("mem_device<char32_t> input get() fail");
        if (obj.dtell() != 1L) throw std::runtime_error("mem_device<char32_t> IO tell() fail");
        
        obj.drseek(0);
        obj.dput(U"x", 1);
        if (obj.dtell() != 4L) throw std::runtime_error("mem_device<char32_t> IO tell() fail");
    }

    {
        IOv2::mem_device<char32_t> obj(U"12345");
        if (obj.dtell() != 0L) throw std::runtime_error("mem_device<char32_t> IO tell() fail");
        
        char32_t buf[5] = {0};
        size_t read_num = obj.dget(buf, 4);
        if (read_num != 4) throw std::runtime_error("mem_device<char32_t> IO get() fail");
        if (obj.dtell() != 4L) throw std::runtime_error("mem_device<char32_t> IO tell() fail");
        
        obj.drseek(0);
        obj.dput(U"123", 3);
        if (obj.dtell() != 8L) throw std::runtime_error("mem_device<char32_t> IO tell() fail");
        
        obj.dseek(4);
        read_num = obj.dget(buf, 5);
        if (read_num != 4) throw std::runtime_error("mem_device<char32_t> IO get() fail");
        if (obj.dtell() != 8L) throw std::runtime_error("mem_device<char32_t> IO tell() fail");
    }

    dump_info("Done\n");
}

void test_mem_device_char32_t_io_2()
{
    dump_info("Test mem_device<char32_t> input/output case 2...");
    {
        IOv2::mem_device<char32_t> obj(U"12345");
        if (obj.dtell() != 0L) throw std::runtime_error("mem_device<char32_t> IO tell() fail");

        FAIL_SEEK(obj, 10);
        if (obj.dtell() != 0L) throw std::runtime_error("mem_device<char32_t> IO tell() fail");

        obj.drseek(0);
        obj.dput(U"abcde", 5);
        obj.dseek(10);
        if (obj.dtell() != 10L) throw std::runtime_error("mem_device<char32_t> IO tell() fail");
        
        obj.dseek(3);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char32_t> IO tell() fail");
        
        obj.dput(U"xxxx", 4);
        if (obj.str() != U"123xxxxcde") throw std::runtime_error("mem_device<char32_t> IO put() fail");
    }

    dump_info("Done\n");
}

