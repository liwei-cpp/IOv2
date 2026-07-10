#include <stdexcept>
#include <tuple>
#include <device/mem_device.h>

#include <common/dump_info.h>
#include <common/verify.h>

void test_mem_device_char8_t_gen_1()
{
    dump_info("Test mem_device<char8_t> general 1...");
    static_assert(IOv2::io_device<IOv2::mem_device<char8_t>>);
    static_assert(std::is_same_v<IOv2::mem_device<char8_t>::char_type, char8_t>);
    
    using CheckType = IOv2::mem_device<char8_t>;
    static_assert(IOv2::dev_cpt::support_positioning<CheckType>);
    static_assert(IOv2::dev_cpt::support_put<CheckType>);
    static_assert(IOv2::dev_cpt::support_get<CheckType>);
    dump_info("Done\n");
}

void test_mem_device_char8_t_gen_2()
{
    dump_info("Test mem_device<char8_t> general 2...");
    
    IOv2::mem_device<char8_t> sbuf;
    VERIFY(sbuf.str().empty());
    
    const std::u8string str = u8"This is my boomstick!";
    IOv2::mem_device<char8_t> sbuf2(str);
    VERIFY(sbuf2.str() == str);
    
    char8_t ch;
    VERIFY(sbuf2.dget(&ch, 1) == 1 && ch == str[0]);
    sbuf2.dput(u8"Y", 1);

    dump_info("Done\n");
}

void test_mem_device_char8_t_gen_3()
{
    dump_info("Test mem_device<char8_t> general 3...");

    {
        IOv2::mem_device<char8_t> obj;
        VERIFY(obj.str() == u8"");
        VERIFY(obj.dtell() == 0);

        obj = IOv2::mem_device{u8"Hello world"};
        VERIFY(obj.str() == u8"Hello world");
        VERIFY(obj.dtell() == 0);
    }
    
    {
        std::u8string ref = u8"Hello world";
        
        IOv2::mem_device<char8_t> obj(ref);
        VERIFY(obj.str() == ref);
        VERIFY(obj.dtell() == 0);

        ref += u8"123";
        obj = IOv2::mem_device{ref};
        VERIFY(obj.str() == ref);
        VERIFY(obj.dtell() == 0);
    }

    dump_info("Done\n");
}

void test_mem_device_char8_t_in_1()
{
    dump_info("Test mem_device<char8_t> input case 1...");

    IOv2::mem_device<char8_t> obj(u8"12");
    char8_t ch;
    VERIFY(obj.dget(&ch, 1) == 1 && ch == u8'1');
    VERIFY(obj.dtell() == 1);
    
    VERIFY(obj.dget(&ch, 1) == 1 && ch == u8'2');
    VERIFY(obj.dtell() == 2);

    VERIFY(obj.dget(&ch, 1) == 0);
    VERIFY(obj.dtell() == 2);

    dump_info("Done\n");
}

void test_mem_device_char8_t_in_2()
{
    dump_info("Test mem_device<char8_t> input case 2...");
    {
        IOv2::mem_device<char8_t> obj(u8"12345");
        char8_t buf[5];
        size_t read_num = obj.dget(buf, 5);
        VERIFY(read_num == 5);
        VERIFY(buf[0] == u8'1');
        VERIFY(buf[1] == u8'2');
        VERIFY(buf[2] == u8'3');
        VERIFY(buf[3] == u8'4');
        VERIFY(buf[4] == u8'5');
        VERIFY(obj.dtell() == 5);
    }
    
    {
        IOv2::mem_device<char8_t> obj(u8"12345");
        char8_t buf[5];
        size_t read_num = obj.dget(buf, 3);
        VERIFY(read_num == 3);
        VERIFY(buf[0] == u8'1');
        VERIFY(buf[1] == u8'2');
        VERIFY(buf[2] == u8'3');
        VERIFY(obj.dtell() == 3);
        
        read_num = obj.dget(buf, 2);
        VERIFY(read_num == 2);
        VERIFY(buf[0] == u8'4');
        VERIFY(buf[1] == u8'5');
        VERIFY(obj.dtell() == 5);
    }

    {
        IOv2::mem_device<char8_t> obj(u8"12345");
        char8_t buf[10];
        size_t read_num = obj.dget(buf, 10);
        VERIFY(read_num == 5);
        VERIFY(buf[0] == u8'1');
        VERIFY(buf[1] == u8'2');
        VERIFY(buf[2] == u8'3');
        VERIFY(buf[3] == u8'4');
        VERIFY(buf[4] == u8'5');
        VERIFY(obj.dtell() == 5);
        
        read_num = obj.dget(buf, 10);
        VERIFY(read_num == 0);
    }

    {
        IOv2::mem_device<char8_t> obj(u8"12345");
        char8_t buf[5];
        size_t read_num = obj.dget(buf, 3);
        VERIFY(read_num == 3);
        VERIFY(buf[0] == u8'1');
        VERIFY(buf[1] == u8'2');
        VERIFY(buf[2] == u8'3');
        VERIFY(obj.dtell() == 3);
        
        read_num = obj.dget(buf, 5);
        VERIFY(read_num == 2);
        VERIFY(buf[0] == u8'4');
        VERIFY(buf[1] == u8'5');
        VERIFY(obj.dtell() == 5);
        
        read_num = obj.dget(buf, 10);
        VERIFY(read_num == 0);
    }
    
    dump_info("Done\n");
}


void test_mem_device_char8_t_in_3()
{
    dump_info("Test mem_device<char8_t> input case 3...");

    {
        IOv2::mem_device<char8_t> obj(u8"12345");
        obj.dseek(3);
        char8_t buf[5];
        size_t read_num = obj.dget(buf, 5);
        VERIFY(read_num == 2);
        VERIFY(buf[0] == u8'4');
        VERIFY(buf[1] == u8'5');

        obj.dseek(obj.dtell() - 1);
        read_num = obj.dget(buf, 5);
        VERIFY(read_num == 1);
        VERIFY(buf[0] == u8'5');
    }
    
    {
        IOv2::mem_device<char8_t> obj(u8"12345");
        char8_t ch;
        
        obj.dseek(2);
        VERIFY(obj.dget(&ch, 1) == 1 && ch == u8'3');
        
        obj.dseek(1);
        VERIFY(obj.dget(&ch, 1) == 1 && ch == u8'2');
        
        obj.dseek(obj.dtell() + 2);
        VERIFY(obj.dget(&ch, 1) == 1 && ch == u8'5');
        
        obj.drseek(3);
        VERIFY(obj.dget(&ch, 1) == 1 && ch == u8'3');
    }
    
    {
        IOv2::mem_device<char8_t> obj(u8"12345");
        FAIL_SEEK(obj, 100);
        VERIFY(obj.dtell() == 0);
        
        obj.dseek(3);

        FAIL_SEEK(obj, 100);
        VERIFY(obj.dtell() == 3);
        
        FAIL_SEEK(obj, 100);
        VERIFY(obj.dtell() == 3);
        
        FAIL_SEEK(obj, -100);
        VERIFY(obj.dtell() == 3);

        FAIL_SEEK(obj, obj.dtell() + 100);
        VERIFY(obj.dtell() == 3);
        
        FAIL_SEEK(obj, obj.dtell() - 100);
        VERIFY(obj.dtell() == 3);

        FAIL_RSEEK(obj, -100);
        VERIFY(obj.dtell() == 3);
        
        FAIL_RSEEK(obj, 100);
        VERIFY(obj.dtell() == 3);
    }
    
    dump_info("Done\n");
}

void test_mem_device_char8_t_out_1()
{
    dump_info("Test mem_device<char8_t> output case 1...");

    IOv2::mem_device<char8_t> obj(u8"12"); obj.drseek(0);
    obj.dput(u8"x", 1);
    VERIFY(obj.str() == u8"12x");
    VERIFY(obj.dtell() == 3);

    obj = IOv2::mem_device(u8"12");
    obj.dput(u8"x", 1);
    VERIFY(obj.str() == u8"x2");
    VERIFY(obj.dtell() == 1);
    
    obj = IOv2::mem_device<char8_t>{};
    obj.dput(u8"y", 1);
    VERIFY(obj.str() == u8"y");
    VERIFY(obj.dtell() == 1);
    
    dump_info("Done\n");
}

void test_mem_device_char8_t_out_2()
{
    dump_info("Test mem_device<char8_t> output case 2...");

    {
        IOv2::mem_device<char8_t> obj;
        obj.dput(u8"12345", 5);
        VERIFY(obj.str() == u8"12345");
        VERIFY(obj.dtell() == 5);
    }
    
    {
        IOv2::mem_device<char8_t> obj;
        obj.dput(nullptr, 0);
        VERIFY(obj.str() == u8"");
        VERIFY(obj.dtell() == 0);
    }
    
    {
        IOv2::mem_device<char8_t> obj;
        obj.dput(u8"123", 3);
        VERIFY(obj.str() == u8"123");
        VERIFY(obj.dtell() == 3);

        obj.dput(u8"45", 2);
        VERIFY(obj.str() == u8"12345");
        VERIFY(obj.dtell() == 5);
    }

    {
        IOv2::mem_device<char8_t> obj;
        obj.dput(u8"123", 3);
        VERIFY(obj.str() == u8"123");

        obj.dput(u8"x", 1);
        VERIFY(obj.str() == u8"123x");

        obj.dput(u8"45", 2);
        VERIFY(obj.str() == u8"123x45");
        VERIFY(obj.dtell() == 6);
    }
    
    dump_info("Done\n");
}

void test_mem_device_char8_t_out_3()
{
    dump_info("Test mem_device<char8_t> output case 3...");

    {
        IOv2::mem_device<char8_t> obj(u8"12345");
        obj.dseek(3);
        obj.dput(u8"ab", 2);
        VERIFY(obj.str() == u8"123ab");
      
        obj.dseek(obj.dtell() - 1);
        obj.dput(u8"x", 1);
        VERIFY(obj.str() == u8"123ax");
    }
    
    {
        IOv2::mem_device<char8_t> obj(u8"12345");
        obj.dseek(2);
        obj.dput(u8"x", 1);
        VERIFY(obj.str() == u8"12x45");
        
        obj.dseek(1);
        obj.dput(u8"y", 1);
        VERIFY(obj.str() == u8"1yx45");
        
        obj.dseek(obj.dtell() + 2);
        obj.dput(u8"z", 1);
        VERIFY(obj.str() == u8"1yx4z");
        
        obj.drseek(3);
        obj.dput(u8"a", 1);
        VERIFY(obj.str() == u8"1ya4z");
    }
    
    {
        IOv2::mem_device<char8_t> obj(u8"12345"); obj.drseek(0);
        FAIL_SEEK(obj, 100);
        VERIFY(obj.dtell() == 5L);
        
        obj.dseek(3);

        FAIL_SEEK(obj, 100);
        VERIFY(obj.dtell() == 3L);
        
        FAIL_SEEK(obj, 100);
        VERIFY(obj.dtell() == 3L);
        
        FAIL_SEEK(obj, -100);
        VERIFY(obj.dtell() == 3L);
        
        FAIL_SEEK(obj, obj.dtell() + 100);
        VERIFY(obj.dtell() == 3L);

        FAIL_SEEK(obj, obj.dtell() - 100);
        VERIFY(obj.dtell() == 3L);
        
        FAIL_RSEEK(obj, -100);
        VERIFY(obj.dtell() == 3L);

        FAIL_RSEEK(obj, 100);
        VERIFY(obj.dtell() == 3L);
    }
    
    dump_info("Done\n");
}

void test_mem_device_char8_t_io_1()
{
    dump_info("Test mem_device<char8_t> input/output case 1...");
    {
        IOv2::mem_device<char8_t> obj;
        VERIFY(obj.dtell() == 0L);
        
        obj.dput(u8"123", 3);
        VERIFY(obj.dtell() == 3L);
        
        obj.dseek(0);
        char8_t ch;
        VERIFY(obj.dget(&ch, 1) == 1 && ch == u8'1');
        VERIFY(obj.dtell() == 1L);
        
        obj.drseek(0);
        obj.dput(u8"x", 1);
        VERIFY(obj.dtell() == 4L);
    }

    {
        IOv2::mem_device<char8_t> obj(u8"12345");
        VERIFY(obj.dtell() == 0L);
        
        char8_t buf[5] = {0};
        size_t read_num = obj.dget(buf, 4);
        VERIFY(read_num == 4);
        VERIFY(obj.dtell() == 4L);
        
        obj.drseek(0);
        obj.dput(u8"123", 3);
        VERIFY(obj.dtell() == 8L);
        
        obj.dseek(4);
        read_num = obj.dget(buf, 5);
        VERIFY(read_num == 4);
        VERIFY(obj.dtell() == 8L);
    }

    dump_info("Done\n");
}

void test_mem_device_char8_t_io_2()
{
    dump_info("Test mem_device<char8_t> input/output case 2...");
    {
        IOv2::mem_device<char8_t> obj(u8"12345");
        VERIFY(obj.dtell() == 0L);

        FAIL_SEEK(obj, 10);
        VERIFY(obj.dtell() == 0L);

        obj.drseek(0);
        obj.dput(u8"abcde", 5);
        obj.dseek(10);
        VERIFY(obj.dtell() == 10L);
        
        obj.dseek(3);
        VERIFY(obj.dtell() == 3L);
        
        obj.dput(u8"xxxx", 4);
        VERIFY(obj.str() == u8"123xxxxcde");
    }

    dump_info("Done\n");
}
