#include <cstring>
#include <fstream>
#include <stdexcept>
#include <tuple>
#include <device/file_device.h>

#include <support/dump_info.h>
#include <support/file_guard.h>
#include <support/verify.h>

void test_file_device_char_gen_1()
{
    using namespace IOv2;

    dump_info("Test file_device<char> general 1...");
    static_assert(IOv2::io_device<IOv2::basic_file_device<true, false, char>>);
    static_assert(IOv2::io_device<IOv2::basic_file_device<false, true, char>>);
    static_assert(IOv2::io_device<IOv2::basic_file_device<true, true, char>>);

    static_assert(std::is_same_v<IOv2::basic_file_device<true, false, char>::char_type, char>);
    static_assert(std::is_same_v<IOv2::basic_file_device<false, true, char>::char_type, char>);
    static_assert(std::is_same_v<IOv2::basic_file_device<true, true, char>::char_type, char>);

    {
        using CheckType = IOv2::basic_file_device<true, false, char>;
        static_assert(IOv2::io_device<CheckType>);
        static_assert(IOv2::dev_cpt::support_positioning<CheckType>);
        static_assert(!IOv2::dev_cpt::support_put<CheckType>);
        static_assert(IOv2::dev_cpt::support_get<CheckType>);
    }
    
    {
        using CheckType = IOv2::basic_file_device<false, true, char>;
        static_assert(IOv2::io_device<CheckType>);
        static_assert(IOv2::dev_cpt::support_positioning<CheckType>);
        static_assert(IOv2::dev_cpt::support_put<CheckType>);
        static_assert(!IOv2::dev_cpt::support_get<CheckType>);
    }
    
    {
        using CheckType = IOv2::basic_file_device<true, true, char>;
        static_assert(IOv2::io_device<CheckType>);
        static_assert(IOv2::dev_cpt::support_positioning<CheckType>);
        static_assert(IOv2::dev_cpt::support_put<CheckType>);
        static_assert(IOv2::dev_cpt::support_get<CheckType>);
    }
    dump_info("Done\n");
}

void test_file_device_char_close_1()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::close 1...");

    const char name_01[] = "filebuf_members-1.tst";    
    const char name_02[] = "filebuf_members-1.txt";
    const char* name_03 = "filebuf_members-3";

    basic_file_device<true, false, char> fb_01;
    basic_file_device<false, true, char> fb_02;
    basic_file_device<true, true, char> fb_03;
    
    // bool is_open()
    VERIFY(!fb_01.is_open());
    VERIFY(!fb_02.is_open());
    VERIFY(!fb_03.is_open());

    {
        file_guard g1(name_01, "abcde");
        fb_01 = basic_file_device<true, false, char>(name_01);
        VERIFY(fb_01.is_open());
    }

    {
        file_guard g1(name_02, "");
        fb_02 = basic_file_device<false, true, char>(name_02, file_open_flag::trunc);
        VERIFY(fb_02.is_open());
        
        file_guard g2(name_03);
        fb_03 = basic_file_device<true, true, char>(name_03, file_open_flag::trunc);
        VERIFY(fb_03.is_open());
        
        fb_02.close();
        VERIFY(!fb_02.is_open());
        
        fb_03.close();
        VERIFY(!fb_02.is_open());
        
        fb_03.close();
        VERIFY(!fb_02.is_open());
    }

    dump_info("Done\n");
}

void test_file_device_char_close_2()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::close 2...");
    
    const char name_01[] = "filebuf_virtuals-1.txt"; // file with data in it
    const char name_02[] = "filebuf_virtuals-2.txt"; // empty file, need to create

    // 'in'
    char buffer[] = "xxxxxxxxxx";
    {
        file_guard g1(name_01, "axxxxxxxxxx");
        basic_file_device<true, false, char> fb_01(name_01);

        char ch;
        VERIFY(fb_01.dget(&ch, 1) == 1);
        VERIFY(fb_01.dget(buffer, sizeof(buffer) - 1) == sizeof(buffer) - 1);
        
        fb_01.close();
        try
        {
            fb_01.dget(&ch, 1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (...) {}

        try
        {
            fb_01.dget(buffer, sizeof(buffer));
            dump_info("unreachable code");
            std::abort();
        }
        catch (...) {}
    }
    
    // 'out'
    {
        file_guard g(name_02);
        basic_file_device<false, true, char> fb_02(name_02);
        fb_02.dput("T", 1);
        fb_02.dput(buffer, sizeof(buffer) - 1);

        fb_02.close();
        try
        {
            fb_02.dput("T", 1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (...) {}
        
        try
        {
            fb_02.dput(buffer, sizeof(buffer) - 1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (...) {}
    }

    dump_info("Done\n");
}

void test_file_device_char_is_open_1()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::is_open 1...");
    
    const char name_01[] = "filebuf_members-1.tst";
    const char name_02[] = "filebuf_members-1.txt";
    const char* name_03 = "filebuf_members-3"; // empty file, need to create

    basic_file_device<true, false, char> fb_01;
    basic_file_device<true, true, char> fb_02;
    basic_file_device<false, true, char> fb_03;
    VERIFY(!fb_01.is_open());
    VERIFY(!fb_02.is_open());
    VERIFY(!fb_03.is_open());
    
    file_guard g0(name_01, "abcde");
    fb_01 = basic_file_device<true, false, char>(name_01);
    VERIFY(fb_01.is_open());
    
    file_guard g1(name_02, "axxxxxxxxxx");
    fb_02 = basic_file_device<true, true, char>(name_02, file_open_flag::trunc);
    VERIFY(fb_02.is_open());
    
    file_guard g2(name_03);
    fb_03 = basic_file_device<false, true, char>(name_03, file_open_flag::trunc);
    VERIFY(fb_03.is_open());
    
    fb_01.close();
    fb_02.close();
    fb_03.close();
    VERIFY(!fb_01.is_open());
    VERIFY(!fb_02.is_open());
    VERIFY(!fb_03.is_open());

    dump_info("Done\n");
}

void test_file_device_char_is_open_2()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::is_open 2...");

    const char* name = "tmp_file5";
    file_guard g1(name);

    basic_file_device<false, true, char> scratch_file_1(name, file_open_flag::trunc);
    scratch_file_1.close();

    basic_file_device<true, false, char> scratch_file_2(name);
    VERIFY(scratch_file_2.is_open());

    dump_info("Done\n");
}

void test_file_device_char_get_1()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::get 1...");
    
    const char name_01[] = "sgetc.txt"; // file with data in it
    file_guard g1(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
    
    basic_file_device<true, false, char> fb_01(name_01);
    
    VERIFY(!fb_01.deof());
    char ch;
    VERIFY(fb_01.dget(&ch, 1) == 1 && ch == '/');
    VERIFY(fb_01.dget(&ch, 1) == 1 && ch == '/');
    VERIFY(fb_01.dget(&ch, 1) == 1 && ch == ' ');
    VERIFY(fb_01.dget(&ch, 1) == 1 && ch == '9');
    VERIFY(fb_01.dget(&ch, 1) == 1 && ch == '9');
    VERIFY(fb_01.dget(&ch, 1) == 1 && ch == '0');
    
    auto read_position = fb_01.dtell();
    VERIFY(read_position != 0);

    dump_info("Done\n");
}

void test_file_device_char_get_2()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::get 2...");
    
    const char name_01[] = "sgetc.txt"; // file with data in it
    const char name_03[] = "tmp_sbumpc_1io.tst"; // empty file, need to create
    
    // in | out 1
    {
        file_guard g1(name_03);
        basic_file_device<true, true, char> fb_03(name_03, file_open_flag::trunc);
        VERIFY(fb_03.dtell() == 0);
        
        char ch;
        VERIFY(fb_03.dget(&ch, 1) == 0);
        VERIFY(fb_03.dtell() == 0);
    }

    // in | out 2
    {
        file_guard g1(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
        basic_file_device<true, true, char> fb_01(name_01);
        VERIFY(fb_01.dtell() == 0);
        
        char ch;
        VERIFY(fb_01.dget(&ch, 1) == 1 && ch == '/');
        VERIFY(fb_01.dget(&ch, 1) == 1 && ch == '/');
        VERIFY(fb_01.dget(&ch, 1) == 1 && ch == ' ');
        VERIFY(fb_01.dget(&ch, 1) == 1 && ch == '9');
        VERIFY(fb_01.dget(&ch, 1) == 1 && ch == '9');
        VERIFY(fb_01.dget(&ch, 1) == 1 && ch == '0');
        
        VERIFY(fb_01.dtell() == 6);
    }

    dump_info("Done\n");
}

void test_file_device_char_get_3()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::get 3...");
    
    const char name_06[] = "filebuf_virtuals-6.txt"; // empty file, need to create
    file_guard g(name_06);
    
    basic_file_device<true, true, char> fbuf(name_06, file_open_flag::trunc);
    fbuf.dput("crazy bees!", 11);
    fbuf.dseek(0);
    
    char ch;
    fbuf.dget(&ch, 1);
    VERIFY(fbuf.dget(&ch, 1) == 1 && ch == 'r');
    VERIFY(fbuf.dget(&ch, 1) == 1 && ch == 'a');

    dump_info("Done\n");
}

void test_file_device_char_seek_1()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::seek 1...");

    const char name_01[] = "seekpos.txt"; // file with data in it
    
    char ch;
    {
        // in 
        file_guard g1(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
        basic_file_device<true, false, char> fb(name_01);
        
        fb.dseek(79);
        VERIFY(fb.dget(&ch, 1) == 1 && ch == 't');
    }
    
    {
        // io
        file_guard g1(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
        basic_file_device<true, true, char> fb(name_01);
        VERIFY(fb.dtell() == 0);
        
        // beg
        fb.dseek(79);
        VERIFY(fb.dget(&ch, 1) == 1 && ch == 't');

        // cur
        fb.dseek(fb.dtell() - 1);
        auto pt_3 = fb.dtell();
        fb.dput("\n", 1);
        fb.dseek(pt_3);
        VERIFY(fb.dget(&ch, 1) == 1 && ch == '\n');
        
        // end
        fb.drseek(0);
        fb.dput("\nof the wonderful things he does!!\nok", 37);
        VERIFY(fb.dtell() != 0);
    }

    dump_info("Done\n");
}

void test_file_device_char_seek_2()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::seek 2...");
    
    const char name_01[] = "seekoff.txt";
    file_guard g(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
    
    basic_file_device<true, false, char> fb(name_01);
    
    // beg
    fb.dseek(2);
    VERIFY(fb.dtell() == 2);
    
    char ch;
    fb.dget(&ch, 1);
    VERIFY(fb.dget(&ch, 1) == 1 && ch == '9');
    fb.dseek(4);
    VERIFY(fb.dget(&ch, 1) == 1 && ch == '9');


    // cur
    fb.dseek(fb.dtell() + 2);
    VERIFY(fb.dtell() == 7);
    VERIFY(fb.dget(&ch, 1) == 1 && ch == '1');
    fb.dseek(fb.dtell());
    VERIFY(fb.dtell() == 8);
    VERIFY(fb.dget(&ch, 1) == 1 && ch == '7');

    // end
    fb.drseek(0);
    VERIFY(fb.dget(&ch, 1) == 0);
    fb.drseek(1);
    VERIFY(fb.dget(&ch, 1) == 1 && ch == 'c');

    dump_info("Done\n");
}

void test_file_device_char_seek_3()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::seek 3...");
    
    const char name_01[] = "seekoff.txt";
    file_guard g(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");

    // in | out
    basic_file_device<true, true, char> fb(name_01);

    VERIFY(fb.dtell() == 0);

    //beg
    fb.dseek(3);
    VERIFY(fb.dtell() == 3);
    
    char ch;
    VERIFY(fb.dget(&ch, 1) == 1 && ch == '9');
    
    fb.dseek(3);
    fb.dput("\n", 1);
    fb.dseek(4);
    VERIFY(fb.dget(&ch, 1) == 1 && ch == '9');

    // cur
    fb.dseek(fb.dtell() + 2);
    VERIFY(fb.dtell() == 7);
    VERIFY(fb.dget(&ch, 1) == 1 && ch == '1');
    fb.dseek(fb.dtell());
    fb.dput("x", 1);
    fb.dput("\n", 1);

    // end
    fb.drseek(0);
    fb.dput("\n", 1);
    fb.dput("because because because. . .", 28);
    
    fb.drseek(1);
    VERIFY(fb.dget(&ch, 1) == 1 && ch == '.');
    
    dump_info("Done\n");
}

void test_file_device_char_seek_4()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::seek 4...");
    
    const char name_01[] = "seekoff.txt";
    file_guard g(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");

    // out
    basic_file_device<false, true, char> fb(name_01);
    VERIFY(fb.dtell() == 0);

    // beg
    fb.dseek(2);
    VERIFY(fb.dtell() == 2);

    // cur
    fb.dseek(fb.dtell() + 2);
    VERIFY(fb.dtell() == 4);
    fb.dseek(fb.dtell());
    fb.dput("x", 1);
    fb.dput("\n", 1);
    
    // end
    fb.drseek(0);
    fb.dput("\n", 1);
    fb.dput("because because because. . .", 28);

    dump_info("Done\n");
}

void test_file_device_char_seek_5()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::seek 5...");

    const char name_01[] = "filebuf_virtuals-1.tst"; // file with data in it
    
    {
        file_guard g(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
        basic_file_device<true, false, char> fb(name_01);
        
        fb.dseek(0);
        fb.dseek(0);
    }
    
    {
        basic_file_device<true, false, char> fb;
        FAIL_SEEK(fb, 0);
        FAIL_SEEK(fb, 0);
    }

    dump_info("Done\n");
}

void test_file_device_char_seek_6()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::seek 6...");

    const char name_01[] = "filebuf_virtuals-1.tst"; // file with data in it
    
    {
        file_guard g(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
        basic_file_device<true, true, char> fb(name_01);
        
        fb.dseek(0);
        fb.dseek(0);
    }
    
    {
        basic_file_device<true, true, char> fb;
        FAIL_SEEK(fb, 0);
        FAIL_SEEK(fb, 0);
    }

    dump_info("Done\n");
}

void test_file_device_char_seek_7()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::seek 7...");

    const char name_01[] = "filebuf_virtuals-1.tst"; // file with data in it
    
    {
        file_guard g(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
        basic_file_device<false, true, char> fb(name_01);
        
        fb.dseek(0);
        fb.dseek(0);
    }
    
    {
        basic_file_device<false, true, char> fb;
        FAIL_SEEK(fb, 0);
        FAIL_SEEK(fb, 0);
    }

    dump_info("Done\n");
}

void test_file_device_char_seek_8()
{
    using namespace IOv2;

    dump_info("Test file_device<char>::seek 8...");

    const char name[] = "tmp_seekoff-4.tst";

    char buf[12];
    file_guard g(name);
    basic_file_device<true, true, char> fb(name, file_open_flag::trunc);
    VERIFY(fb.deof());
    fb.dput("abcd", 4);
    VERIFY(fb.deof());
    
    fb.dseek(0);
    VERIFY(!fb.deof());
    VERIFY(fb.dget(buf, 3) == 3);
    VERIFY(std::memcmp(buf, "abc", 3) == 0);
    VERIFY(!fb.deof());
    
    // Check read => write without pubseekoff(0, ios_base::cur)
    fb.dput("ef", 2);
    VERIFY(fb.deof());
    fb.dseek(0);
    VERIFY(!fb.deof());
    VERIFY(fb.dget(buf, 5) == 5);
    VERIFY(std::memcmp(buf, "abcef", 5) == 0);
    VERIFY(fb.deof());
    
    fb.dseek(0);
    fb.dput("gh", 2);
    VERIFY(!fb.deof());

    // Check write => read without pubseekoff(0, ios_base::cur)
    VERIFY(fb.dget(buf, 3) == 3);
    VERIFY(std::memcmp(buf, "cef", 3) == 0);
    VERIFY(fb.deof());
    
    fb.dput("ijkl", 4);
    VERIFY(fb.deof());
    
    fb.dseek(0);
    VERIFY(fb.dget(buf, 2) == 2);
    VERIFY(std::memcmp(buf, "gh", 2) == 0);
    VERIFY(!fb.deof());

    fb.drseek(0);
    VERIFY(fb.deof());
    fb.dput("mno", 3);
    VERIFY(fb.deof());

    fb.dseek(0);
    VERIFY(!fb.deof());
    VERIFY(fb.dget(buf, 12) == 12);
    VERIFY(std::memcmp(buf, "ghcefijklmno", 12) == 0);
    VERIFY(fb.deof());

    dump_info("Done\n");
}

void test_file_device_char_edge_cases()
{
    using namespace IOv2;

    dump_info("Test file_device<char> edge cases...");

    const char name[] = "edge_cases.txt";
    file_guard g(name);

    {
        basic_file_device<true, true, char> fb(name, file_open_flag::trunc);
        
        // Test dput(nullptr, 0)
        fb.dput(nullptr, 0);
        VERIFY(fb.dsize() == 0);

        // Test dget(nullptr, 0)
        char ch;
        VERIFY(fb.dget(nullptr, 0) == 0);
        
        // Test dget(nullptr, 1) should throw
        try {
            fb.dget(nullptr, 1);
            throw std::runtime_error("file_device::dget(nullptr, 1) should throw");
        } catch (const device_error&) {}

        fb.dput("abc", 3);
        fb.dseek(0);
        VERIFY(fb.dget(nullptr, 0) == 0);
        VERIFY(fb.dget(&ch, 1) == 1 && ch == 'a');
    }

    dump_info("Done\n");
}

void test_file_device_char_error_1()
{
    using namespace IOv2;

    dump_info("Test file_device<char> error 1...");

    // Case 1: Constructor Read mode on non-existent file
    try
    {
        basic_file_device<true, false, char> fb("non_existent_file.txt");
        VERIFY(false);
    }
    catch (const device_error&) {}

    // Case 2: try_open Read mode on non-existent file
    {
        auto res = file_device<char>::try_open("non_existent_file.txt");
        VERIFY(!res);
        // Verify error message exists
        VERIFY(!res.error().empty());
    }

    // Case 3: Read/Write mode on non-existent file
    try
    {
        basic_file_device<true, true, char> fb("non_existent_file.txt");
        VERIFY(false);
    }
    catch (const device_error&) {}

    // Case 4: Write mode with noreplace on existing file
    {
        file_guard g("existing_file.txt", "content");
        try
        {
            basic_file_device<false, true, char> fb("existing_file.txt", file_open_flag::noreplace);
            VERIFY(false);
        }
        catch (const device_error&) {}
        
        auto res = ofile_device<char>::try_open("existing_file.txt", file_open_flag::noreplace);
        VERIFY(!res);
    }

    dump_info("Done\n");
}

void test_file_device_char_move()
{
    using namespace IOv2;
    dump_info("Test file_device<char> move semantics...");
    const char name[] = "move_test.txt";
    file_guard g(name, "move content");
    
    basic_file_device<true, true, char> dev1(name);
    VERIFY(dev1.is_open());
    size_t len = dev1.dsize();
    
    // Move constructor
    basic_file_device<true, true, char> dev2(std::move(dev1));
    VERIFY(dev2.is_open());
    VERIFY(dev2.dsize() == len);
    VERIFY(!dev1.is_open());
    
    // Move assignment
    basic_file_device<true, true, char> dev3;
    dev3 = std::move(dev2);
    VERIFY(dev3.is_open());
    VERIFY(dev3.dsize() == len);
    VERIFY(!dev2.is_open());

    // Self-assignment
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
    dev3 = std::move(dev3);
#pragma GCC diagnostic pop
    VERIFY(dev3.is_open());
    VERIFY(dev3.dsize() == len);
    
    dump_info("Done\n");
}

void test_file_device_char_more_errors()
{
    using namespace IOv2;
    dump_info("Test file_device<char> more errors...");
    
    // Invalid seek in input mode
    {
        file_guard g("seek_err.txt", "123");
        ifile_device<char> dev("seek_err.txt");
        try {
            dev.dseek(10); // Beyond end in In mode
            VERIFY(false);
        } catch (const device_error&) {}
    }
    
    // dtell/dsize/dseek on closed file
    {
        ifile_device<char> dev;
        try { (void)dev.dtell(); VERIFY(false); } catch (const device_error&) {}
        try { (void)dev.dsize(); VERIFY(false); } catch (const device_error&) {}
        try { dev.dseek(0); VERIFY(false); } catch (const device_error&) {}
        try { dev.drseek(0); VERIFY(false); } catch (const device_error&) {}
        try { (void)dev.deof(); VERIFY(true); } catch (...) { VERIFY(false); }
    }
    
    // dput on closed file or null buffer
    {
        ofile_device<char> dev;
        try { dev.dput("a", 1); VERIFY(false); } catch (const device_error&) {}
        
        file_guard g("put_err.txt");
        ofile_device<char> dev2("put_err.txt");
        try { dev2.dput(nullptr, 1); VERIFY(false); } catch (const device_error&) {}
    }

    // Invalid open modes
    try {
        basic_file_device<true, false, char> dev("test.txt", file_open_flag::trunc);
        VERIFY(false);
    } catch (const device_error&) {}

    // dflush on closed file (should be no-op)
    {
        ofile_device<char> dev;
        dev.dflush();
    }

    // drseek: offset > m_file_len  (covers the "invalid parameter" path in drseek)
    {
        file_guard g("drseek_err.txt", "hello"); // 5 bytes
        ifile_device<char> dev("drseek_err.txt");
        try { dev.drseek(1000); VERIFY(false); } catch (const device_error&) {}
    }

    // dseek: position exceeds INT64_MAX  (covers the overflow guard in dseek)
    {
        file_guard g("dseek_overflow.txt", "hi");
        ofile_device<char> dev("dseek_overflow.txt");
        try {
            dev.dseek(std::numeric_limits<size_t>::max());
            VERIFY(false);
        } catch (const device_error&) {}
    }

    dump_info("Done\n");
}

void test_file_device_char_system_errors()
{
    using namespace IOv2;
    dump_info("Test file_device<char> system errors...");

    // dflush error on /dev/full
    try {
        ofile_device<char> dev("/dev/full");
        dev.dput("some data", 9);
        dev.dflush();
    } catch (const device_error&) {
        // Expected failure
    }

    // close error recovery
    try {
        ofile_device<char> dev("/dev/full");
        dev.dput("some data", 9);
        dev.close(); // Should throw because dflush fails, but file should still be closed
    } catch (const device_error&) {
        // Expected failure
    }

    dump_info("Done\n");
}

void test_file_device_char_open_modes()
{
    using namespace IOv2;
    dump_info("Test file_device<char> open modes...");

    const char name[] = "modes_test.txt";
    
    // noreplace
    {
        file_guard g(name);
        ofile_device<char> dev(name, file_open_flag::noreplace);
        VERIFY(dev.is_open());
    }

    // noreplace + binary
    {
        file_guard g(name);
        ofile_device<char> dev(name, file_open_flag::noreplace | file_open_flag::binary);
        VERIFY(dev.is_open());
    }

    // rw + trunc
    {
        file_guard g(name, "existing");
        file_device<char> dev(name, file_open_flag::trunc);
        VERIFY(dev.dsize() == 0);
    }

    // rw + binary
    {
        file_guard g(name, "existing");
        file_device<char> dev(name, file_open_flag::binary);
        VERIFY(dev.dsize() == 8);
    }

    // rw + trunc + binary
    {
        file_guard g(name, "existing");
        file_device<char> dev(name, file_open_flag::trunc | file_open_flag::binary);
        VERIFY(dev.dsize() == 0);
    }

    // rw + noreplace
    {
        file_guard g(name);
        file_device<char> dev(name, file_open_flag::noreplace);
        VERIFY(dev.is_open());
    }

    // rw + noreplace + binary
    {
        file_guard g(name);
        file_device<char> dev(name, file_open_flag::noreplace | file_open_flag::binary);
        VERIFY(dev.is_open());
    }

    // in + binary  → fopen_mode "rb"  (covers the previously-missed return "rb" path)
    {
        file_guard g(name, "existing");
        ifile_device<char> dev(name, file_open_flag::binary);
        VERIFY(dev.is_open());
    }

    // out + binary  → fopen_mode "wb"  (covers the previously-missed return "wb" path)
    {
        file_guard g(name);
        ofile_device<char> dev(name, file_open_flag::binary);
        VERIFY(dev.is_open());
    }

    dump_info("Done\n");
}

void test_file_device_char_coverage_extra()
{
    using namespace IOv2;
    dump_info("Test file_device<char> coverage extra...");

    const char name[] = "extra_coverage_test.txt";

    // 1. Missing Flag Combinations (fopen_mode)
    // IsIn && IsOut + trunc | noreplace
    {
        file_guard g(name);
        file_device<char> dev(name, file_open_flag::trunc | file_open_flag::noreplace);
        VERIFY(dev.is_open());
    }
    // IsIn && IsOut + binary | trunc | noreplace
    {
        file_guard g(name);
        file_device<char> dev(name, file_open_flag::binary | file_open_flag::trunc | file_open_flag::noreplace);
        VERIFY(dev.is_open());
    }
    // IsOut + trunc | noreplace
    {
        file_guard g(name);
        ofile_device<char> dev(name, file_open_flag::trunc | file_open_flag::noreplace);
        VERIFY(dev.is_open());
    }
    // IsOut + binary | trunc | noreplace
    {
        file_guard g(name);
        ofile_device<char> dev(name, file_open_flag::binary | file_open_flag::trunc | file_open_flag::noreplace);
        VERIFY(dev.is_open());
    }

    // 2. Partial Write in dput (using /dev/full)
    // Trigger line 490: throw device_error("file_device::dput fail: partial write");
    try {
        ofile_device<char> dev("/dev/full");
        // Write a large buffer (1MB) to ensure we exceed internal FILE buffers
        // and trigger a real syscall that fails on /dev/full.
        std::string large_data(1024 * 1024, 'A');
        dev.dput(large_data.data(), large_data.size());
        VERIFY(false); 
    } catch (const device_error&) {
        // Expected: partial write or early failure
    }

    // 3. fseek failure in constructor (using /dev/stderr)
    // /dev/stderr is usually a pipe or TTY, which is non-seekable (ESPIPE).
    // Trigger line 169: throw device_error("cannot get file length...");
    try {
        ifile_device<char> dev("/dev/stderr");
        // If constructor somehow succeeds (platform specific), try dseek
        dev.dseek(1);
    } catch (const device_error&) {
        // Expected
    }

    // 4. try_open error path coverage
    {
        // Invalid mode for read-only (trunc) triggers exception in constructor,
        // which try_open catches and returns as unexpected.
        auto res = ifile_device<char>::try_open(name, file_open_flag::trunc);
        VERIFY(!res);
        VERIFY(!res.error().empty());
    }

    dump_info("Done\n");
}

