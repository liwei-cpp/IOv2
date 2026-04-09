#include <stdexcept>
#include <tuple>
#include <device/mem_device.h>

#include <common/dump_info.h>
#include <common/verify.h>

void test_mem_device_char_gen_1()
{
    dump_info("Test mem_device<char> general 1...");
    static_assert(IOv2::io_device<IOv2::mem_device<char>>);
    static_assert(std::is_same_v<IOv2::mem_device<char>::char_type, char>);
    
    using CheckType = IOv2::mem_device<char>;
    static_assert(IOv2::dev_cpt::support_positioning<CheckType>);
    static_assert(IOv2::dev_cpt::support_put<CheckType>);
    static_assert(IOv2::dev_cpt::support_get<CheckType>);
    dump_info("Done\n");
}

void test_mem_device_char_gen_2()
{
    dump_info("Test mem_device<char> general 2...");

    IOv2::mem_device<char> sbuf;
    VERIFY(sbuf.str().empty());
    
    const std::string str = "This is my boomstick!";
    IOv2::mem_device sbuf2(str);
    VERIFY(sbuf2.str() == str);
    
    char ch = 0;
    VERIFY(sbuf2.dget(&ch, 1) == 1);
    VERIFY(ch == str[0]);
    sbuf2.dput("Y", 1);

    dump_info("Done\n");
}

void test_mem_device_char_gen_3()
{
    dump_info("Test mem_device<char> general 3...");
    {
        IOv2::mem_device<char> obj;
        if (obj.str() != "") throw std::runtime_error("mem_device<char> input constructor fail");
        if (obj.dtell() != 0) throw std::runtime_error("mem_device<char> input tell fail");

        obj = IOv2::mem_device{"Hello world"};
        if (obj.str() != "Hello world") throw std::runtime_error("mem_device<char> input str() fail");
        if (obj.dtell() != 0) throw std::runtime_error("mem_device<char> input tell fail");
    }
    
    {
        std::string ref = "Hello world";
        
        IOv2::mem_device obj(ref);
        if (obj.str() != ref) throw std::runtime_error("mem_device<char> input constructor fail");
        if (obj.dtell() != 0) throw std::runtime_error("mem_device<char> input tell fail");

        ref += "123";
        obj = IOv2::mem_device{ref};
        if (obj.str() != ref) throw std::runtime_error("mem_device<char> input str() fail");
        if (obj.dtell() != 0) throw std::runtime_error("mem_device<char> input tell fail");
    }
    dump_info("Done\n");
}

void test_mem_device_char_in_1()
{
    dump_info("Test mem_device<char> input case 1...");
    
    IOv2::mem_device obj("12");
    VERIFY(!obj.deos());
    char ch = 0;
    VERIFY(obj.dget(&ch, 1) == 1);
    VERIFY(ch == '1');
    VERIFY(obj.dtell() == 1);
    VERIFY(!obj.deos());

    if ((obj.dget(&ch, 1) != 1) || (ch != '2')) throw std::runtime_error("mem_device<char> input get() fail");
    if (obj.dtell() != 2) throw std::runtime_error("mem_device<char> input tell fail");
    VERIFY(obj.deos());

    if (obj.dget(&ch, 1) != 0) throw std::runtime_error("mem_device<char> input get() fail");
    if (obj.dtell() != 2) throw std::runtime_error("mem_device<char> input tell fail");
    VERIFY(obj.deos());
    
    dump_info("Done\n");
}

void test_mem_device_char_in_2()
{
    dump_info("Test mem_device<char> input case 2...");
    {
        IOv2::mem_device obj("12345");
        char buf[5];
        size_t read_num = obj.dget(buf, 5);
        if (read_num != 5) throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[0] != '1') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[1] != '2') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[2] != '3') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[3] != '4') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[4] != '5') throw std::runtime_error("mem_device<char> input get() fail");
        if (obj.dtell() != 5) throw std::runtime_error("mem_device<char> input tell fail");
    }
    
    {
        IOv2::mem_device obj("12345");
        char buf[5];
        size_t read_num = obj.dget(buf, 3);
        if (read_num != 3) throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[0] != '1') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[1] != '2') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[2] != '3') throw std::runtime_error("mem_device<char> input get() fail");
        if (obj.dtell() != 3) throw std::runtime_error("mem_device<char> input tell fail");

        read_num = obj.dget(buf, 2);
        if (read_num != 2) throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[0] != '4') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[1] != '5') throw std::runtime_error("mem_device<char> input get() fail");
        if (obj.dtell() != 5) throw std::runtime_error("mem_device<char> input tell fail");
    }

    {
        IOv2::mem_device obj("12345");
        char buf[10];
        size_t read_num = obj.dget(buf, 10);
        if (read_num != 5) throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[0] != '1') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[1] != '2') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[2] != '3') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[3] != '4') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[4] != '5') throw std::runtime_error("mem_device<char> input get() fail");
        if (obj.dtell() != 5) throw std::runtime_error("mem_device<char> input tell fail");

        read_num = obj.dget(buf, 10);
        if (read_num != 0) throw std::runtime_error("mem_device<char> input get() fail");
    }

    {
        IOv2::mem_device obj("12345");
        char buf[5];
        size_t read_num = obj.dget(buf, 3);
        if (read_num != 3) throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[0] != '1') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[1] != '2') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[2] != '3') throw std::runtime_error("mem_device<char> input get() fail");
        if (obj.dtell() != 3) throw std::runtime_error("mem_device<char> input tell fail");

        read_num = obj.dget(buf, 5);
        if (read_num != 2) throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[0] != '4') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[1] != '5') throw std::runtime_error("mem_device<char> input get() fail");
        if (obj.dtell() != 5) throw std::runtime_error("mem_device<char> input tell fail");

        read_num = obj.dget(buf, 10);
        if (read_num != 0) throw std::runtime_error("mem_device<char> input get() fail");
    }
    
    dump_info("Done\n");
}

void test_mem_device_char_in_3()
{
    dump_info("Test mem_device<char> input case 3...");

    {
        IOv2::mem_device obj("12345");
        obj.dseek(3);
        char buf[5];
        size_t read_num = obj.dget(buf, 5);
        if (read_num != 2) throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[0] != '4') throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[1] != '5') throw std::runtime_error("mem_device<char> input get() fail");
      
        obj.dseek(obj.dtell() - 1);
        read_num = obj.dget(buf, 5);
        if (read_num != 1) throw std::runtime_error("mem_device<char> input get() fail");
        if (buf[0] != '5') throw std::runtime_error("mem_device<char> input get() fail");
    }
    
    {
        IOv2::mem_device obj("12345");
        char ch;
        
        obj.dseek(2);
        
        if ((obj.dget(&ch, 1) != 1) || (ch != '3')) throw std::runtime_error("mem_device<char> input get() fail");
        
        obj.dseek(1);
        if ((obj.dget(&ch, 1) != 1) || (ch != '2')) throw std::runtime_error("mem_device<char> input get() fail");
        
        obj.dseek(obj.dtell() + 2);
        if ((obj.dget(&ch, 1) != 1) || (ch != '5')) throw std::runtime_error("mem_device<char> input get() fail");
        
        obj.drseek(3);
        if ((obj.dget(&ch, 1) != 1) || (ch != '3')) throw std::runtime_error("mem_device<char> input get() fail");
    }
    
    {
        IOv2::mem_device obj("12345");
        FAIL_SEEK(obj, 100);
        if (obj.dtell() != 0L) throw std::runtime_error("mem_device<char> input tell() fail");
        
        obj.dseek(3);

        FAIL_SEEK(obj, 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> input tell() fail");
        
        FAIL_SEEK(obj, 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> input tell() fail");
        
        FAIL_SEEK(obj, -100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> input tell() fail");

        FAIL_SEEK(obj, obj.dtell() + 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> input tell() fail");
        
        FAIL_SEEK(obj, obj.dtell() - 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> input tell() fail");
        
        FAIL_RSEEK(obj, -100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> input tell() fail");
        
        FAIL_RSEEK(obj, 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> input tell() fail");
    }

    dump_info("Done\n");
}

void test_mem_device_char_out_1()
{
    dump_info("Test mem_device<char> output case 1...");

    IOv2::mem_device obj("12"); obj.drseek(0);
    VERIFY(obj.deos());
    obj.dput("x", 1);
    VERIFY(obj.deos());
    VERIFY(obj.str() == "12x");
    VERIFY(obj.dtell() == 3);
    
    obj = IOv2::mem_device("12");
    VERIFY(!obj.deos());
    obj.dput("x", 1);
    VERIFY(!obj.deos());
    if (obj.str() != "x2") throw std::runtime_error("mem_device<char> output put() fail");
    if (obj.dtell() != 1) throw std::runtime_error("mem_device<char> output tell fail");

    obj = IOv2::mem_device("");
    VERIFY(obj.deos());
    obj.dput("y", 1);
    VERIFY(obj.deos());
    if (obj.str() != "y") throw std::runtime_error("mem_device<char> output put() fail");
    if (obj.dtell() != 1) throw std::runtime_error("mem_device<char> output tell fail");
    
    dump_info("Done\n");
}

void test_mem_device_char_out_2()
{
    dump_info("Test mem_device<char> output case 2...");

    {
        IOv2::mem_device<char> obj;
        obj.dput("12345", 5);
        if (obj.str() != "12345") throw std::runtime_error("mem_device<char> output put() fail");
        if (obj.dtell() != 5) throw std::runtime_error("mem_device<char> output tell fail");
    }
    
    {
        IOv2::mem_device<char> obj;
        obj.dput(nullptr, 0);
        if (obj.str() != "") throw std::runtime_error("mem_device<char> output put() fail");
        if (obj.dtell() != 0) throw std::runtime_error("mem_device<char> output tell fail");
    }
    
    {
        IOv2::mem_device<char> obj;
        obj.dput("123", 3);
        if (obj.str() != "123") throw std::runtime_error("mem_device<char> output put() fail");
        if (obj.dtell() != 3) throw std::runtime_error("mem_device<char> output tell fail");

        obj.dput("45", 2);
        if (obj.str() != "12345") throw std::runtime_error("mem_device<char> output put() fail");
        if (obj.dtell() != 5) throw std::runtime_error("mem_device<char> output tell fail");
    }

    {
        IOv2::mem_device<char> obj;
        obj.dput("123", 3);
        if (obj.str() != "123") throw std::runtime_error("mem_device<char> output put() fail");

        obj.dput("x", 1);
        if (obj.str() != "123x") throw std::runtime_error("mem_device<char> output put() fail");

        obj.dput("45", 2);
        if (obj.str() != "123x45") throw std::runtime_error("mem_device<char> output put() fail");
        if (obj.dtell() != 6) throw std::runtime_error("mem_device<char> output tell fail");
    }
    
    dump_info("Done\n");
}

void test_mem_device_char_out_3()
{
    dump_info("Test mem_device<char> output case 3...");

    {
        IOv2::mem_device obj("12345");
        obj.dseek(3);
        obj.dput("ab", 2);
        if (obj.str() != "123ab") throw std::runtime_error("mem_device<char> output put() fail");
      
        obj.dseek(obj.dtell() - 1);
        obj.dput("x", 1);
        if (obj.str() != "123ax") throw std::runtime_error("mem_device<char> output put() fail");
    }
    
    {
        IOv2::mem_device obj("12345");
        obj.dseek(2);
        obj.dput("x", 1);
        if (obj.str() != "12x45") throw std::runtime_error("mem_device<char> output put() fail");
        
        obj.dseek(1);
        obj.dput("y", 1);
        if (obj.str() != "1yx45") throw std::runtime_error("mem_device<char> output put() fail");
        
        obj.dseek(obj.dtell() + 2);
        obj.dput("z", 1);
        if (obj.str() != "1yx4z") throw std::runtime_error("mem_device<char> output put() fail");
        
        obj.drseek(3);
        obj.dput("a", 1);
        if (obj.str() != "1ya4z") throw std::runtime_error("mem_device<char> output put() fail");
    }
    
    {
        IOv2::mem_device obj("12345"); obj.drseek(0);
        FAIL_SEEK(obj, 100);
        if (obj.dtell() != 5L) throw std::runtime_error("mem_device<char> output tell() fail");
        
        obj.dseek(3);

        FAIL_SEEK(obj, 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> output tell() fail");
        
        FAIL_SEEK(obj, 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> output tell() fail");
        
        FAIL_SEEK(obj, -100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> output tell() fail");

        FAIL_SEEK(obj, obj.dtell() + 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> output tell() fail");
        
        FAIL_SEEK(obj, obj.dtell() - 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> output tell() fail");
        
        FAIL_RSEEK(obj, -100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> output tell() fail");
        
        FAIL_RSEEK(obj, 100);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> output tell() fail");
    }
    
    dump_info("Done\n");
}

void test_mem_device_char_io_1()
{
    dump_info("Test mem_device<char> input/output case 1...");
    {
        IOv2::mem_device<char> obj;
        if (obj.dtell() != 0L) throw std::runtime_error("mem_device<char> IO tell() fail");
        
        obj.dput("123", 3);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> IO tell() fail");
        
        obj.dseek(0);
        char ch;
        if ((obj.dget(&ch, 1) != 1) || (ch != '1')) throw std::runtime_error("mem_device<char> input get() fail");
        if (obj.dtell() != 1L) throw std::runtime_error("mem_device<char> IO tell() fail");
        
        obj.drseek(0);
        obj.dput("x", 1);
        if (obj.dtell() != 4L) throw std::runtime_error("mem_device<char> IO tell() fail");
    }
    
    {
        IOv2::mem_device obj("12345");
        if (obj.dtell() != 0L) throw std::runtime_error("mem_device<char> IO tell() fail");
        
        char buf[5] = {0};
        size_t read_num = obj.dget(buf, 4);
        if (read_num != 4) throw std::runtime_error("mem_device<char> IO get() fail");
        if (obj.dtell() != 4L) throw std::runtime_error("mem_device<char> IO tell() fail");
        
        obj.drseek(0);
        obj.dput("123", 3);
        if (obj.dtell() != 8L) throw std::runtime_error("mem_device<char> IO tell() fail");
        
        obj.dseek(4);
        read_num = obj.dget(buf, 5);
        if (read_num != 4) throw std::runtime_error("mem_device<char> IO get() fail");
        if (obj.dtell() != 8L) throw std::runtime_error("mem_device<char> IO tell() fail");
    }

    dump_info("Done\n");
}

void test_mem_device_char_io_2()
{
    dump_info("Test mem_device<char> input/output case 2...");
    {
        IOv2::mem_device obj("12345");
        if (obj.dtell() != 0L) throw std::runtime_error("mem_device<char> IO tell() fail");

        FAIL_SEEK(obj, 10);
        if (obj.dtell() != 0L) throw std::runtime_error("mem_device<char> IO tell() fail");

        obj.drseek(0);
        obj.dput("abcde", 5);
        obj.dseek(10);
        if (obj.dtell() != 10L) throw std::runtime_error("mem_device<char> IO tell() fail");
        
        obj.dseek(3);
        if (obj.dtell() != 3L) throw std::runtime_error("mem_device<char> IO tell() fail");
        
        obj.dput("xxxx", 4);
        if (obj.str() != "123xxxxcde") throw std::runtime_error("mem_device<char> IO put() fail");
    }

    dump_info("Done\n");
}
