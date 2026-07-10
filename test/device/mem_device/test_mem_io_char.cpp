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
        VERIFY(obj.str() == "");
        VERIFY(obj.dtell() == 0);

        obj = IOv2::mem_device{"Hello world"};
        VERIFY(obj.str() == "Hello world");
        VERIFY(obj.dtell() == 0);
    }
    
    {
        std::string ref = "Hello world";
        
        IOv2::mem_device obj(ref);
        VERIFY(obj.str() == ref);
        VERIFY(obj.dtell() == 0);

        ref += "123";
        obj = IOv2::mem_device{ref};
        VERIFY(obj.str() == ref);
        VERIFY(obj.dtell() == 0);
    }
    dump_info("Done\n");
}

void test_mem_device_char_drseek_boundary()
{
    dump_info("Test mem_device<char> drseek boundary...");
    {
        IOv2::mem_device<char> obj("12345"); // size 5
        
        // Boundary: 0 (end of string)
        obj.drseek(0);
        VERIFY(obj.dtell() == 5);
        VERIFY(obj.deof());

        // Boundary: 5 (start of string)
        obj.drseek(5);
        VERIFY(obj.dtell() == 0);
        VERIFY(!obj.deof());

        // Normal: 3 (middle)
        obj.drseek(3);
        VERIFY(obj.dtell() == 2);
        
        // Out of boundary: 6
        FAIL_RSEEK(obj, 6);
        VERIFY(obj.dtell() == 2); // Position should not change on failure

        // Extreme: max size_t (would have underflowed before)
        FAIL_RSEEK(obj, std::numeric_limits<size_t>::max());
        VERIFY(obj.dtell() == 2);
        
        // Negative-like: (size_t)-1
        FAIL_RSEEK(obj, (size_t)-1);
        VERIFY(obj.dtell() == 2);
    }
    dump_info("Done\n");
}

void test_mem_device_char_in_1()
{
    dump_info("Test mem_device<char> input case 1...");
    
    IOv2::mem_device obj("12");
    VERIFY(!obj.deof());
    char ch = 0;
    VERIFY(obj.dget(&ch, 1) == 1);
    VERIFY(ch == '1');
    VERIFY(obj.dtell() == 1);
    VERIFY(!obj.deof());

    VERIFY(obj.dget(&ch, 1) == 1 && ch == '2');
    VERIFY(obj.dtell() == 2);
    VERIFY(obj.deof());

    VERIFY(obj.dget(&ch, 1) == 0);
    VERIFY(obj.dtell() == 2);
    VERIFY(obj.deof());
    
    dump_info("Done\n");
}

void test_mem_device_char_in_2()
{
    dump_info("Test mem_device<char> input case 2...");
    {
        IOv2::mem_device obj("12345");
        char buf[5];
        size_t read_num = obj.dget(buf, 5);
        VERIFY(read_num == 5);
        VERIFY(buf[0] == '1');
        VERIFY(buf[1] == '2');
        VERIFY(buf[2] == '3');
        VERIFY(buf[3] == '4');
        VERIFY(buf[4] == '5');
        VERIFY(obj.dtell() == 5);
    }
    
    {
        IOv2::mem_device obj("12345");
        char buf[5];
        size_t read_num = obj.dget(buf, 3);
        VERIFY(read_num == 3);
        VERIFY(buf[0] == '1');
        VERIFY(buf[1] == '2');
        VERIFY(buf[2] == '3');
        VERIFY(obj.dtell() == 3);

        read_num = obj.dget(buf, 2);
        VERIFY(read_num == 2);
        VERIFY(buf[0] == '4');
        VERIFY(buf[1] == '5');
        VERIFY(obj.dtell() == 5);
    }

    {
        IOv2::mem_device obj("12345");
        char buf[10];
        size_t read_num = obj.dget(buf, 10);
        VERIFY(read_num == 5);
        VERIFY(buf[0] == '1');
        VERIFY(buf[1] == '2');
        VERIFY(buf[2] == '3');
        VERIFY(buf[3] == '4');
        VERIFY(buf[4] == '5');
        VERIFY(obj.dtell() == 5);

        read_num = obj.dget(buf, 10);
        VERIFY(read_num == 0);
    }

    {
        IOv2::mem_device obj("12345");
        char buf[5];
        size_t read_num = obj.dget(buf, 3);
        VERIFY(read_num == 3);
        VERIFY(buf[0] == '1');
        VERIFY(buf[1] == '2');
        VERIFY(buf[2] == '3');
        VERIFY(obj.dtell() == 3);

        read_num = obj.dget(buf, 5);
        VERIFY(read_num == 2);
        VERIFY(buf[0] == '4');
        VERIFY(buf[1] == '5');
        VERIFY(obj.dtell() == 5);

        read_num = obj.dget(buf, 10);
        VERIFY(read_num == 0);
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
        VERIFY(read_num == 2);
        VERIFY(buf[0] == '4');
        VERIFY(buf[1] == '5');
      
        obj.dseek(obj.dtell() - 1);
        read_num = obj.dget(buf, 5);
        VERIFY(read_num == 1);
        VERIFY(buf[0] == '5');
    }
    
    {
        IOv2::mem_device obj("12345");
        char ch;
        
        obj.dseek(2);
        
        VERIFY(obj.dget(&ch, 1) == 1 && ch == '3');
        
        obj.dseek(1);
        VERIFY(obj.dget(&ch, 1) == 1 && ch == '2');
        
        obj.dseek(obj.dtell() + 2);
        VERIFY(obj.dget(&ch, 1) == 1 && ch == '5');
        
        obj.drseek(3);
        VERIFY(obj.dget(&ch, 1) == 1 && ch == '3');
    }
    
    {
        IOv2::mem_device obj("12345");
        FAIL_SEEK(obj, 100);
        VERIFY(obj.dtell() == 0L);
        
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

void test_mem_device_char_out_1()
{
    dump_info("Test mem_device<char> output case 1...");

    IOv2::mem_device obj("12"); obj.drseek(0);
    VERIFY(obj.deof());
    obj.dput("x", 1);
    VERIFY(obj.deof());
    VERIFY(obj.str() == "12x");
    VERIFY(obj.dtell() == 3);
    
    obj = IOv2::mem_device("12");
    VERIFY(!obj.deof());
    obj.dput("x", 1);
    VERIFY(!obj.deof());
    VERIFY(obj.str() == "x2");
    VERIFY(obj.dtell() == 1);

    obj = IOv2::mem_device("");
    VERIFY(obj.deof());
    obj.dput("y", 1);
    VERIFY(obj.deof());
    VERIFY(obj.str() == "y");
    VERIFY(obj.dtell() == 1);
    
    dump_info("Done\n");
}

void test_mem_device_char_out_2()
{
    dump_info("Test mem_device<char> output case 2...");

    {
        IOv2::mem_device<char> obj;
        obj.dput("12345", 5);
        VERIFY(obj.str() == "12345");
        VERIFY(obj.dtell() == 5);
    }
    
    {
        IOv2::mem_device<char> obj;
        obj.dput(nullptr, 0);
        VERIFY(obj.str() == "");
        VERIFY(obj.dtell() == 0);

        // Test dget edge cases
        VERIFY(obj.dget(nullptr, 0) == 0);
        
        try {
            obj.dget(nullptr, 1);
            throw std::runtime_error("mem_device<char> dget(nullptr, 1) should throw");
        } catch (const IOv2::device_error&) {}
    }
    
    {
        IOv2::mem_device<char> obj;
        obj.dput("123", 3);
        VERIFY(obj.str() == "123");
        VERIFY(obj.dtell() == 3);

        obj.dput("45", 2);
        VERIFY(obj.str() == "12345");
        VERIFY(obj.dtell() == 5);
    }

    {
        IOv2::mem_device<char> obj;
        obj.dput("123", 3);
        VERIFY(obj.str() == "123");

        obj.dput("x", 1);
        VERIFY(obj.str() == "123x");

        obj.dput("45", 2);
        VERIFY(obj.str() == "123x45");
        VERIFY(obj.dtell() == 6);
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
        VERIFY(obj.str() == "123ab");
      
        obj.dseek(obj.dtell() - 1);
        obj.dput("x", 1);
        VERIFY(obj.str() == "123ax");
    }
    
    {
        IOv2::mem_device obj("12345");
        obj.dseek(2);
        obj.dput("x", 1);
        VERIFY(obj.str() == "12x45");
        
        obj.dseek(1);
        obj.dput("y", 1);
        VERIFY(obj.str() == "1yx45");
        
        obj.dseek(obj.dtell() + 2);
        obj.dput("z", 1);
        VERIFY(obj.str() == "1yx4z");
        
        obj.drseek(3);
        obj.dput("a", 1);
        VERIFY(obj.str() == "1ya4z");
    }
    
    {
        IOv2::mem_device obj("12345"); obj.drseek(0);
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

void test_mem_device_char_io_1()
{
    dump_info("Test mem_device<char> input/output case 1...");
    {
        IOv2::mem_device<char> obj;
        VERIFY(obj.dtell() == 0L);
        
        obj.dput("123", 3);
        VERIFY(obj.dtell() == 3L);
        
        obj.dseek(0);
        char ch;
        VERIFY(obj.dget(&ch, 1) == 1 && ch == '1');
        VERIFY(obj.dtell() == 1L);
        
        obj.drseek(0);
        obj.dput("x", 1);
        VERIFY(obj.dtell() == 4L);
    }
    
    {
        IOv2::mem_device obj("12345");
        VERIFY(obj.dtell() == 0L);
        
        char buf[5] = {0};
        size_t read_num = obj.dget(buf, 4);
        VERIFY(read_num == 4);
        VERIFY(obj.dtell() == 4L);
        
        obj.drseek(0);
        obj.dput("123", 3);
        VERIFY(obj.dtell() == 8L);
        
        obj.dseek(4);
        read_num = obj.dget(buf, 5);
        VERIFY(read_num == 4);
        VERIFY(obj.dtell() == 8L);
    }

    dump_info("Done\n");
}

void test_mem_device_char_io_2()
{
    dump_info("Test mem_device<char> input/output case 2...");
    {
        IOv2::mem_device obj("12345");
        VERIFY(obj.dtell() == 0L);

        FAIL_SEEK(obj, 10);
        VERIFY(obj.dtell() == 0L);

        obj.drseek(0);
        obj.dput("abcde", 5);
        obj.dseek(10);
        VERIFY(obj.dtell() == 10L);
        
        obj.dseek(3);
        VERIFY(obj.dtell() == 3L);
        
        obj.dput("xxxx", 4);
        VERIFY(obj.str() == "123xxxxcde");
    }

    dump_info("Done\n");
}

void test_mem_device_char_dput_alias_growth()
{
    dump_info("Test mem_device<char> dput alias growth...");
    {
        // 1. Initialize data
        std::string initial_data = "OriginalData";
        IOv2::mem_device<char> obj(initial_data);
        
        // 2. Setup aliasing: ch points into the internal buffer
        // We want to write "Original" (first 8 chars) to the end.
        // The current size is 12, seeking to the end.
        obj.dseek(obj.dsize());
        const char* alias_ptr = obj.str().data(); // Points to "OriginalData"
        
        // 3. Perform put, which triggers growth (m_next_pos + 8 > m_str.size())
        // Your fix ensures this is safe by using a temporary buffer.
        obj.dput(alias_ptr, 8); 
        
        // 4. Verify results
        VERIFY(obj.str() == "OriginalDataOriginal");
        VERIFY(obj.dtell() == 20);
    }
    dump_info("Done\n");
}

void test_mem_device_char_extra()
{
    using namespace IOv2;
    dump_info("Test mem_device<char> extra coverage...");

    try {
        mem_device<char> obj((const char*)nullptr);
        VERIFY(false);
    } catch (const device_error&) {}

    {
        mem_device<char> d1("abc");
        d1.dseek(1);
        mem_device<char> d2(std::move(d1));
        VERIFY(d2.str() == "abc");
        VERIFY(d2.dtell() == 1);
        VERIFY(d1.dtell() == 0);
        VERIFY(d1.str().empty());
    }

    {
        mem_device<char> d1("abc");
        mem_device<char> d2("xyz");
        d2 = std::move(d1);
        VERIFY(d2.str() == "abc");
        VERIFY(d1.str().empty());
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
        d2 = std::move(d2);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
        VERIFY(d2.str() == "abc");
    }

    {
        mem_device<char> obj("abc");
        char ch;
        VERIFY(obj.dget(&ch, 0) == 0);
        try {
            obj.dget(nullptr, 1);
            VERIFY(false);
        } catch (const device_error&) {}
    }

    {
        mem_device<char> obj;
        obj.dput("a", 0);
        try {
            obj.dput(nullptr, 1);
            VERIFY(false);
        } catch (const device_error&) {}
        try {
            obj.dput("a", std::numeric_limits<size_t>::max());
            VERIFY(false);
        } catch (const device_error&) {}
    }

    {
        mem_device<char> obj("abc");
        try {
            obj.get_buf<true>(4);
            VERIFY(false);
        } catch (const device_error&) {}
        auto [ptr, len] = obj.get_buf<false>(5);
        VERIFY(len == 3);
    }

    {
        mem_device<char> obj("abc");
        obj.dseek(1);
        try {
            obj.get_rollback(0);
            VERIFY(false);
        } catch (const device_error&) {}
        try {
            obj.get_rollback(2);
            VERIFY(false);
        } catch (const device_error&) {}
        obj.get_rollback(1);
        VERIFY(obj.dtell() == 0);
    }

    {
        mem_device<char> obj;
        try {
            obj.put_rollback(1);
            VERIFY(false);
        } catch (const device_error&) {}
        obj.put_buf(5);
        try {
            obj.put_rollback(0);
            VERIFY(false);
        } catch (const device_error&) {}
        try {
            obj.put_rollback(6);
            VERIFY(false);
        } catch (const device_error&) {}
        obj.put_rollback(2);
        VERIFY(obj.dtell() == 3);
        try {
            obj.put_buf(std::numeric_limits<size_t>::max());
            VERIFY(false);
        } catch (const device_error&) {}
    }

    {
        mem_device<char> obj("abc");
        obj.dflush();
        VERIFY(obj.dsize() == 3);
    }

    dump_info("Done\n");
}

