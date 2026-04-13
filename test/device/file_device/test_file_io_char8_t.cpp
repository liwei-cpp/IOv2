#include <cstring>
#include <fstream>
#include <stdexcept>
#include <tuple>
#include <device/file_device.h>

#include <common/dump_info.h>
#include <common/file_guard.h>
#include <common/verify.h>

void test_file_device_char8_t_gen_1()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t> general 1...");
    static_assert(IOv2::io_device<IOv2::basic_file_device<true, false, char8_t>>);
    static_assert(IOv2::io_device<IOv2::basic_file_device<false, true, char8_t>>);
    static_assert(IOv2::io_device<IOv2::basic_file_device<true, true, char8_t>>);

    static_assert(std::is_same_v<IOv2::basic_file_device<true, false, char8_t>::char_type, char8_t>);
    static_assert(std::is_same_v<IOv2::basic_file_device<false, true, char8_t>::char_type, char8_t>);
    static_assert(std::is_same_v<IOv2::basic_file_device<true, true, char8_t>::char_type, char8_t>);

    {
        using CheckType = IOv2::basic_file_device<true, false, char8_t>;
        static_assert(IOv2::io_device<CheckType>);
        static_assert(IOv2::dev_cpt::support_positioning<CheckType>);
        static_assert(!IOv2::dev_cpt::support_put<CheckType>);
        static_assert(IOv2::dev_cpt::support_get<CheckType>);
    }
    
    {
        using CheckType = IOv2::basic_file_device<false, true, char8_t>;
        static_assert(IOv2::io_device<CheckType>);
        static_assert(IOv2::dev_cpt::support_positioning<CheckType>);
        static_assert(IOv2::dev_cpt::support_put<CheckType>);
        static_assert(!IOv2::dev_cpt::support_get<CheckType>);
    }
    
    {
        using CheckType = IOv2::basic_file_device<true, true, char8_t>;
        static_assert(IOv2::io_device<CheckType>);
        static_assert(IOv2::dev_cpt::support_positioning<CheckType>);
        static_assert(IOv2::dev_cpt::support_put<CheckType>);
        static_assert(IOv2::dev_cpt::support_get<CheckType>);
    }
    dump_info("Done\n");
}

void test_file_device_char8_t_close_1()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::close 1...");

    const char name_01[] = "filebuf_members-1.tst";    
    const char name_02[] = "filebuf_members-1.txt";
    const char* name_03 = "filebuf_members-3";

    basic_file_device<true, false, char8_t> fb_01;
    basic_file_device<false, true, char8_t> fb_02;
    basic_file_device<true, true, char8_t> fb_03;
    
    // bool is_open()
    if (fb_01.is_open()) throw std::runtime_error("file_device::is_open fails");
    if (fb_02.is_open()) throw std::runtime_error("file_device::is_open fails");
    if (fb_03.is_open()) throw std::runtime_error("file_device::is_open fails");

    {
        file_guard g1(name_01, "abcde");
        fb_01 = basic_file_device<true, false, char8_t>(name_01);
        if (!fb_01.is_open()) throw std::runtime_error("file_device::is_open fails");
    }

    {
        file_guard g1(name_02, "");
        fb_02 = basic_file_device<false, true, char8_t>(name_02, file_open_flag::trunc);
        if (!fb_02.is_open()) throw std::runtime_error("file_device::reset fails");
        
        file_guard g2(name_03);
        fb_03 = basic_file_device<true, true, char8_t>(name_03, file_open_flag::trunc);
        if (!fb_03.is_open()) throw std::runtime_error("file_device::reset fails");
        
        fb_02.close();
        if (fb_02.is_open()) throw std::runtime_error("file_device::is_open fails");
        
        fb_03.close();
        if (fb_02.is_open()) throw std::runtime_error("file_device::is_open fails");
        
        fb_03.close();
        if (fb_02.is_open()) throw std::runtime_error("file_device::is_open fails");
    }

    dump_info("Done\n");
}

void test_file_device_char8_t_close_2()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::close 2...");
    
    const char name_01[] = "filebuf_virtuals-1.txt"; // file with data in it
    const char name_02[] = "filebuf_virtuals-2.txt"; // empty file, need to create

    // 'in'
    char8_t buffer[] = u8"xxxxxxxxxx";
    {
        file_guard g1(name_01, "axxxxxxxxxx");
        basic_file_device<true, false, char8_t> fb_01(name_01);

        char8_t ch;
        if (fb_01.dget(&ch, 1) != 1) throw std::runtime_error("file_device::get fail");
        if (fb_01.dget(buffer, sizeof(buffer) - 1) != sizeof(buffer) - 1) throw std::runtime_error("file_device::get fails");
        
        fb_01.close();
        if (fb_01.dget(&ch, 1) != 0) throw std::runtime_error("file_device::get fail");
        if (fb_01.dget(buffer, sizeof(buffer)) != 0)  throw std::runtime_error("file_device::get fails");
    }
    
    // 'out'
    {
        file_guard g(name_02);
        basic_file_device<false, true, char8_t> fb_02(name_02);
        fb_02.dput(u8"T", 1);
        fb_02.dput(buffer, sizeof(buffer) - 1);

        fb_02.close();
        try
        {
            fb_02.dput(u8"T", 1);
            dump_info("unreachable code");
            std::abort();
        }
        catch(...) {}
        
        try
        {
            fb_02.dput(buffer, sizeof(buffer) - 1);
            dump_info("unreachable code");
            std::abort();
        }
        catch(...) {}
    }

    dump_info("Done\n");
}

void test_file_device_char8_t_is_open_1()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::is_open 1...");
    
    const char name_01[] = "filebuf_members-1.tst";
    const char name_02[] = "filebuf_members-1.txt";
    const char* name_03 = "filebuf_members-3"; // empty file, need to create

    basic_file_device<true, false, char8_t> fb_01;
    basic_file_device<true, true, char8_t> fb_02;
    basic_file_device<false, true, char8_t> fb_03;
    if (fb_01.is_open()) throw std::runtime_error("file_device::is_open fails");
    if (fb_02.is_open()) throw std::runtime_error("file_device::is_open fails");
    if (fb_03.is_open()) throw std::runtime_error("file_device::is_open fails");
    
    file_guard g0(name_01, "abcde");
    fb_01 = basic_file_device<true, false, char8_t>(name_01);
    if (!fb_01.is_open()) throw std::runtime_error("file_device::is_open fails");
    
    file_guard g1(name_02, "axxxxxxxxxx");
    fb_02 = basic_file_device<true, true, char8_t>(name_02, file_open_flag::trunc);
    if (!fb_02.is_open()) throw std::runtime_error("file_device::is_open fails");
    
    file_guard g2(name_03);
    fb_03 = basic_file_device<false, true, char8_t>(name_03, file_open_flag::trunc);
    if (!fb_03.is_open()) throw std::runtime_error("file_device::is_open fails");
    
    fb_01.close();
    fb_02.close();
    fb_03.close();
    if (fb_01.is_open()) throw std::runtime_error("file_device::is_open fails");
    if (fb_02.is_open()) throw std::runtime_error("file_device::is_open fails");
    if (fb_03.is_open()) throw std::runtime_error("file_device::is_open fails");

    dump_info("Done\n");
}

void test_file_device_char8_t_is_open_2()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::is_open 2...");

    const char* name = "tmp_file5";
    file_guard g1(name);

    basic_file_device<false, true, char8_t> scratch_file_1(name, file_open_flag::trunc);
    scratch_file_1.close();

    basic_file_device<true, false, char8_t> scratch_file_2(name);
    if (!scratch_file_2.is_open()) std::runtime_error("file_device::reset fails");

    dump_info("Done\n");
}

void test_file_device_char8_t_get_1()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::get 1...");
    
    const char name_01[] = "sgetc.txt"; // file with data in it
    file_guard g1(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
    
    basic_file_device<true, false, char8_t> fb_01(name_01);
    
    char8_t ch;
    if ((fb_01.dget(&ch, 1) != 1) || (ch != u8'/')) throw std::runtime_error("file_device::get fail");
    if ((fb_01.dget(&ch, 1) != 1) || (ch != u8'/')) throw std::runtime_error("file_device::get fail");
    if ((fb_01.dget(&ch, 1) != 1) || (ch != u8' ')) throw std::runtime_error("file_device::get fail");
    if ((fb_01.dget(&ch, 1) != 1) || (ch != u8'9')) throw std::runtime_error("file_device::get fail");
    if ((fb_01.dget(&ch, 1) != 1) || (ch != u8'9')) throw std::runtime_error("file_device::get fail");
    if ((fb_01.dget(&ch, 1) != 1) || (ch != u8'0')) throw std::runtime_error("file_device::get fail");
    
    auto read_position = fb_01.dtell();
    if (read_position == 0) throw std::runtime_error("file_device::tell fails");

    dump_info("Done\n");
}

void test_file_device_char8_t_get_2()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::get 2...");
    
    const char name_01[] = "sgetc.txt"; // file with data in it
    const char name_03[] = "tmp_sbumpc_1io.tst"; // empty file, need to create
    
    // in | out 1
    {
        file_guard g1(name_03);
        basic_file_device<true, true, char8_t> fb_03(name_03, file_open_flag::trunc);
        if (fb_03.dtell() != 0) throw std::runtime_error("file_device::tell fails");
        
        char8_t ch;
        if (fb_03.dget(&ch, 1) != 0) throw std::runtime_error("file_device::get fail");
        if (fb_03.dtell() != 0) throw std::runtime_error("file_device::tell fails");
    }

    // in | out 2
    {
        file_guard g1(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
        basic_file_device<true, true, char8_t> fb_01(name_01);
        if (fb_01.dtell() != 0) throw std::runtime_error("file_device::tell fails");
        
        char8_t ch;
        if ((fb_01.dget(&ch, 1) != 1) || (ch != u8'/')) throw std::runtime_error("file_device::get fail");
        if ((fb_01.dget(&ch, 1) != 1) || (ch != u8'/')) throw std::runtime_error("file_device::get fail");
        if ((fb_01.dget(&ch, 1) != 1) || (ch != u8' ')) throw std::runtime_error("file_device::get fail");
        if ((fb_01.dget(&ch, 1) != 1) || (ch != u8'9')) throw std::runtime_error("file_device::get fail");
        if ((fb_01.dget(&ch, 1) != 1) || (ch != u8'9')) throw std::runtime_error("file_device::get fail");
        if ((fb_01.dget(&ch, 1) != 1) || (ch != u8'0')) throw std::runtime_error("file_device::get fail");
        
        if (fb_01.dtell() != 6) throw std::runtime_error("file_device::tell fails");
    }

    dump_info("Done\n");
}

void test_file_device_char8_t_get_3()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::get 3...");
    
    const char name_06[] = "filebuf_virtuals-6.txt"; // empty file, need to create
    file_guard g(name_06);
    
    basic_file_device<true, true, char8_t> fbuf(name_06, file_open_flag::trunc);
    fbuf.dput(u8"crazy bees!", 11);
    fbuf.dseek(0);
    
    char8_t ch;
    fbuf.dget(&ch, 1);
    if ((fbuf.dget(&ch, 1) != 1) || (ch != u8'r')) throw std::runtime_error("file_device::get fail");
    if ((fbuf.dget(&ch, 1) != 1) || (ch != u8'a')) throw std::runtime_error("file_device::get fail");

    dump_info("Done\n");
}

void test_file_device_char8_t_seek_1()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::seek 1...");

    const char name_01[] = "seekpos.txt"; // file with data in it
    
    char8_t ch;
    {
        // in 
        file_guard g1(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
        basic_file_device<true, false, char8_t> fb(name_01);
        
        fb.dseek(79);
        if ((fb.dget(&ch, 1) != 1) || (ch != 't')) throw std::runtime_error("file_device::get fail");
    }
    
    {
        // io
        file_guard g1(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
        basic_file_device<true, true, char8_t> fb(name_01);
        if (fb.dtell() != 0) throw std::runtime_error("file_device::tell fails");
        
        // beg
        fb.dseek(79);
        if ((fb.dget(&ch, 1) != 1) || (ch != 't')) throw std::runtime_error("file_device::get fail");

        // cur
        fb.dseek(fb.dtell() - 1);
        auto pt_3 = fb.dtell();
        fb.dput(u8"\n", 1);
        fb.dseek(pt_3);
        if ((fb.dget(&ch, 1) != 1) || (ch != '\n')) throw std::runtime_error("file_device::get fail");
        
        // end
        fb.drseek(0);
        fb.dput(u8"\nof the wonderful things he does!!\nok", 37);
        if (fb.dtell() == 0) throw std::runtime_error("file_device::tell fails");
    }

    dump_info("Done\n");
}

void test_file_device_char8_t_seek_2()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::seek 2...");
    
    const char name_01[] = "seekoff.txt";
    file_guard g(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
    
    basic_file_device<true, false, char8_t> fb(name_01);
    
    // beg
    fb.dseek(2);
    if (fb.dtell() != 2) throw std::runtime_error("faile_io::seek fails");
    
    char8_t ch;
    fb.dget(&ch, 1);
    if ((fb.dget(&ch, 1) != 1) || (ch != u8'9')) throw std::runtime_error("file_device::get fail");
    fb.dseek(4);
    if ((fb.dget(&ch, 1) != 1) || (ch != u8'9')) throw std::runtime_error("file_device::get fail");


    // cur
    fb.dseek(fb.dtell() + 2);
    if (fb.dtell() != 7) throw std::runtime_error("faile_io::seek fails");
    if ((fb.dget(&ch, 1) != 1) || (ch != u8'1')) throw std::runtime_error("file_device::get fail");
    fb.dseek(fb.dtell());
    if (fb.dtell() != 8) throw std::runtime_error("faile_io::seek fails");
    if ((fb.dget(&ch, 1) != 1) || (ch != u8'7')) throw std::runtime_error("file_device::get fail");

    // end
    fb.drseek(0);
    if (fb.dget(&ch, 1) != 0) throw std::runtime_error("file_device::get fail");
    fb.drseek(1);
    if ((fb.dget(&ch, 1) != 1) || (ch != u8'c')) throw std::runtime_error("file_device::get fail");

    dump_info("Done\n");
}

void test_file_device_char8_t_seek_3()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::seek 3...");
    
    const char name_01[] = "seekoff.txt";
    file_guard g(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");

    // in | out
    basic_file_device<true, true, char8_t> fb(name_01);

    if (fb.dtell() != 0) throw std::runtime_error("file_device::tell fails");

    //beg
    fb.dseek(3);
    if (fb.dtell() != 3) throw std::runtime_error("file_device::tell fails");
    
    char8_t ch;
    if ((fb.dget(&ch, 1) != 1) || (ch != u8'9')) throw std::runtime_error("file_device::get fail");
    
    fb.dseek(3);
    fb.dput(u8"\n", 1);
    fb.dseek(4);
    if ((fb.dget(&ch, 1) != 1) || (ch != u8'9')) throw std::runtime_error("file_device::get fail");

    // cur
    fb.dseek(fb.dtell() + 2);
    if (fb.dtell() != 7) throw std::runtime_error("file_device::tell fails");
    if ((fb.dget(&ch, 1) != 1) || (ch != u8'1')) throw std::runtime_error("file_device::get fail");
    fb.dseek(fb.dtell());
    fb.dput(u8"x", 1);
    fb.dput(u8"\n", 1);

    // end
    fb.drseek(0);
    fb.dput(u8"\n", 1);
    fb.dput(u8"because because because. . .", 28);
    
    fb.drseek(1);
    if ((fb.dget(&ch, 1) != 1) || (ch != '.')) throw std::runtime_error("file_device::get fail");
    
    dump_info("Done\n");
}

void test_file_device_char8_t_seek_4()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::seek 4...");
    
    const char name_01[] = "seekoff.txt";
    file_guard g(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");

    // out
    basic_file_device<false, true, char8_t> fb(name_01);
    if (fb.dtell() != 0) throw std::runtime_error("file_device::tell fails");

    // beg
    fb.dseek(2);
    if (fb.dtell() != 2) throw std::runtime_error("file_device::tell fails");

    // cur
    fb.dseek(fb.dtell() + 2);
    if (fb.dtell() != 4) throw std::runtime_error("file_device::tell fails");
    fb.dseek(fb.dtell());
    fb.dput(u8"x", 1);
    fb.dput(u8"\n", 1);
    
    // end
    fb.drseek(0);
    fb.dput(u8"\n", 1);
    fb.dput(u8"because because because. . .", 28);

    dump_info("Done\n");
}

void test_file_device_char8_t_seek_5()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::seek 5...");

    const char name_01[] = "filebuf_virtuals-1.tst"; // file with data in it
    
    {
        file_guard g(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
        basic_file_device<true, false, char8_t> fb(name_01);
        
        fb.dseek(0);
        fb.dseek(0);
    }
    
    {
        basic_file_device<true, false, char8_t> fb;
        FAIL_SEEK(fb, 0);
        FAIL_SEEK(fb, 0);
    }

    dump_info("Done\n");
}

void test_file_device_char8_t_seek_6()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::seek 6...");

    const char name_01[] = "filebuf_virtuals-1.tst"; // file with data in it
    
    {
        file_guard g(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
        basic_file_device<true, true, char8_t> fb(name_01);
        
        fb.dseek(0);
        fb.dseek(0);
    }
    
    {
        basic_file_device<true, true, char8_t> fb;
        FAIL_SEEK(fb, 0);
        FAIL_SEEK(fb, 0);
    }

    dump_info("Done\n");
}

void test_file_device_char8_t_seek_7()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::seek 7...");

    const char name_01[] = "filebuf_virtuals-1.tst"; // file with data in it
    
    {
        file_guard g(name_01, "// 990117 bkoz\n// test functionality of basic_filebuf for char_type == char\n// this is a data file for 27filebuf.cc");
        basic_file_device<false, true, char8_t> fb(name_01);
        
        fb.dseek(0);
        fb.dseek(0);
    }
    
    {
        basic_file_device<false, true, char8_t> fb;
        FAIL_SEEK(fb, 0);
        FAIL_SEEK(fb, 0);
    }

    dump_info("Done\n");
}

void test_file_device_char8_t_seek_8()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t>::seek 8...");

    const char name[] = "tmp_seekoff-4.tst";

    char8_t buf[12];
    file_guard g(name);
    basic_file_device<true, true, char8_t> fb(name, file_open_flag::trunc);
    fb.dput(u8"abcd", 4);
    
    fb.dseek(0);
    if (fb.dget(buf, 3) != 3) throw std::runtime_error("file_device::get fails");
    if (std::memcmp(buf, u8"abc", 3) != 0) throw std::runtime_error("file_device::get fails");
    
    // Check read => write without pubseekoff(0, ios_base::cur)
    fb.dput(u8"ef", 2);
    fb.dseek(0);
    if (fb.dget(buf, 5) != 5) throw std::runtime_error("file_device::get fails");
    if (std::memcmp(buf, u8"abcef", 5) != 0) throw std::runtime_error("file_device::get fails");
    
    fb.dseek(0);
    fb.dput(u8"gh", 2);

    // Check write => read without pubseekoff(0, ios_base::cur)
    if (fb.dget(buf, 3) != 3) throw std::runtime_error("file_device::get fails");
    if (std::memcmp(buf, u8"cef", 3) != 0) throw std::runtime_error("file_device::get fails");
    
    fb.dput(u8"ijkl", 4);
    
    fb.dseek(0);
    if (fb.dget(buf, 2) != 2) throw std::runtime_error("file_device::get fails");
    if (std::memcmp(buf, u8"gh", 2) != 0) throw std::runtime_error("file_device::get fails");

    fb.drseek(0);
    fb.dput(u8"mno", 3);

    fb.dseek(0);
    if (fb.dget(buf, 12) != 12) throw std::runtime_error("file_device::get fails");
    if (std::memcmp(buf, u8"ghcefijklmno", 12) != 0) throw std::runtime_error("file_device::get fails");
  
    dump_info("Done\n");
}

void test_file_device_char8_t_error_1()
{
    using namespace IOv2;

    dump_info("Test file_device<char8_t> error 1...");

    // Case 1: Constructor Read mode on non-existent file
    try
    {
        basic_file_device<true, false, char8_t> fb("non_existent_file.txt");
        VERIFY(false);
    }
    catch (const device_error&) {}

    // Case 2: try_open Read mode on non-existent file
    {
        auto res = ifile_device<char8_t>::try_open("non_existent_file.txt");
        VERIFY(!res);
        VERIFY(!res.error().empty());
    }

    // Case 3: Read/Write mode on non-existent file
    try
    {
        basic_file_device<true, true, char8_t> fb("non_existent_file.txt");
        VERIFY(false);
    }
    catch (const device_error&) {}

    // Case 4: Write mode with noreplace on existing file
    {
        file_guard g("existing_file.txt", "content");
        try
        {
            basic_file_device<false, true, char8_t> fb("existing_file.txt", file_open_flag::noreplace);
            VERIFY(false);
        }
        catch (const device_error&) {}
        
        auto res = ofile_device<char8_t>::try_open("existing_file.txt", file_open_flag::noreplace);
        VERIFY(!res);
    }

    dump_info("Done\n");
}
