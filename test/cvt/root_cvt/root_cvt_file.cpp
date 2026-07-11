#include <limits>
#include <list>
#include <iterator>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/file_device.h>

#include <support/dump_info.h>
#include <support/file_guard.h>
#include <support/verify.h>

namespace {
// Minimal write-only device that can be told to throw on the next dput().
// Used to exercise the catch(...) blocks in move-assignment and destructor.
struct throw_write_device {
    using char_type = char;
    bool should_throw = false;

    void dput(const char_type*, size_t) {
        if (should_throw) throw IOv2::device_error("forced throw");
    }
    void dflush() {}
};
} // namespace

void test_root_cvt_file_gen_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> general case 1...");
    
    {
        using CheckType = no_rb_root_cvt<basic_file_device<true, true, char>>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, basic_file_device<true, true, char>>);
        static_assert(std::is_same_v<CheckType::internal_type, char>);
        static_assert(std::is_same_v<CheckType::external_type, char>);

        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = rb_root_cvt<basic_file_device<true, true, char8_t>>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, basic_file_device<true, true, char8_t>>);
        static_assert(std::is_same_v<CheckType::internal_type, char8_t>);
        static_assert(std::is_same_v<CheckType::external_type, char8_t>);
        
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = no_rb_root_cvt<basic_file_device<true, false, char>>;
        static_assert(!cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = rb_root_cvt<basic_file_device<false, true, char>>;
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(!cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }

    dump_info("Done\n");
}

void test_root_cvt_file_gen_2()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> general case 2...");
    
    auto helper1 = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        auto obj2(std::move(obj));
        
        obj2.put(" world", 6);
    };

    {
        file_guard g1("test_file", "hello");
        basic_file_device<true, true, char> dev("test_file");
        dev.drseek(0);
        auto obj = rb_root_cvt{std::move(dev)};
        helper1(obj);
        VERIFY(g1.contents() == "hello world");
    }
    
    {
        file_guard g1("test_file", "hello");
        basic_file_device<false, true, char> dev("test_file");
        dev.drseek(0);
        auto obj = rb_root_cvt{std::move(dev)};
        helper1(obj);
        VERIFY(g1.contents() == " world");
    }
    
    {
        file_guard g1("test_file", "hello");
        basic_file_device<true, true, char> dev("test_file");
        dev.drseek(0);
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper1(obj);
        VERIFY(g1.contents() == "hello world");
    }
    
    {
        file_guard g1("test_file", "hello");
        basic_file_device<false, true, char> dev("test_file");
        dev.drseek(0);
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper1(obj);
        VERIFY(g1.contents() == " world");
    }

    auto helper2 = []<typename T>(T& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();

        using dev_type = typename T::device_type;
        T obj2{rb_root_cvt{dev_type{}}};
        obj2 = std::move(obj);
        obj2.put(" world", 6);
    };

    {
        file_guard g1("test_file", "hello");
        basic_file_device<true, true, char> dev("test_file");
        dev.drseek(0);
        auto obj = rb_root_cvt{std::move(dev)};
        helper2(obj);
        VERIFY(g1.contents() == "hello world");
    }
    
    {
        file_guard g1("test_file", "hello");
        basic_file_device<false, true, char> dev("test_file");
        dev.drseek(0);
        auto obj = rb_root_cvt{std::move(dev)};
        helper2(obj);
        VERIFY(g1.contents() == " world");
    }
    
    {
        file_guard g1("test_file", "hello");
        basic_file_device<true, true, char> dev("test_file");
        dev.drseek(0);
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper2(obj);
        VERIFY(g1.contents() == "hello world");
    }
    
    {
        file_guard g1("test_file", "hello");
        basic_file_device<false, true, char> dev("test_file");
        dev.drseek(0);
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper2(obj);
        VERIFY(g1.contents() == " world");
    }

    dump_info("Done\n");
}

void test_root_cvt_file_gen_3()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> general case 3...");

    auto helper1 = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        std::string str; str.resize(5);
        VERIFY(obj.get(str.data(), 5) == 5);
        VERIFY(str == "hello");
        VERIFY(obj.tell() == 5);

        auto obj2(std::move(obj));
        VERIFY(obj2.tell() == 5);
        str.resize(6);
        VERIFY(obj2.get(str.data(), 6) == 6);
        VERIFY(str == " world");
        VERIFY(obj2.tell() == 11);
    };
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, true, char> dev("test_file");
        auto obj = rb_root_cvt{std::move(dev)};
        helper1(obj);
    }
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, false, char> dev("test_file");
        auto obj = rb_root_cvt{std::move(dev)};
        helper1(obj);
    }
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, true, char> dev("test_file");
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper1(obj);
    }
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, false, char> dev("test_file");
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper1(obj);
    }

    auto helper2 = []<typename T>(T& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        std::string str; str.resize(5);
        VERIFY(obj.get(str.data(), 5) == 5);
        VERIFY(str == "hello");
        VERIFY(obj.tell() == 5);

        using dev_type = typename T::device_type;
        T obj2{rb_root_cvt{dev_type{}}};
        obj2 = std::move(obj);

        VERIFY(obj2.tell() == 5);
        str.resize(6);
        VERIFY(obj2.get(str.data(), 6) == 6);
        VERIFY(str == " world");
        VERIFY(obj2.tell() == 11);
    };

    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, true, char> dev("test_file");
        auto obj = rb_root_cvt{std::move(dev)};
        helper2(obj);
    }
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, false, char> dev("test_file");
        auto obj = rb_root_cvt{std::move(dev)};
        helper2(obj);
    }
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, true, char> dev("test_file");
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper2(obj);
    }
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, false, char> dev("test_file");
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper2(obj);
    }

    dump_info("Done\n");
}

void test_root_cvt_file_get_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device>::get case 1...");

    std::string e_lit; e_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = '\xE6';
        e_lit[i+1] = '\x9D';
        e_lit[i+2] = '\x8E';
        e_lit[i+3] = '\xE4';
        e_lit[i+4] = '\xBC';
        e_lit[i+5] = '\x9F';
        e_lit[i+6] = (i / 7) % 127 + 1;
    }
    
    auto helper = [&e_lit](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        char out_buf[4102];
        size_t total_count = 0;
        char* cur_pos = out_buf;
        int out_buffer_id = 0;
        while (true)
        {
            size_t dest_size = std::min<size_t>(4102 - total_count, out_buffer_size[out_buffer_id++]);
            auto s = obj.get(cur_pos, dest_size);
            out_buffer_id %= std::size(out_buffer_size);
            cur_pos += s;
            total_count += s;
            if (s == 0) break;
        }
        VERIFY(total_count == 4102);
        VERIFY(cur_pos == out_buf + 4102);
        for (size_t i = 0; i < 4102; ++i)
            VERIFY(out_buf[i] == e_lit[i]);
    };

    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, true, char> dev("test_file");
        auto obj = rb_root_cvt{std::move(dev)};
        helper(obj);
    }
    
    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, false, char> dev("test_file");
        auto obj = rb_root_cvt{std::move(dev)};
        helper(obj);
    }
    
    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, true, char> dev("test_file");
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, false, char> dev("test_file");
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }

    dump_info("Done\n");
}

void test_root_cvt_file_get_nra_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device>::get_nra case 1...");

    std::string e_lit; e_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = '\xE6';
        e_lit[i+1] = '\x9D';
        e_lit[i+2] = '\x8E';
        e_lit[i+3] = '\xE4';
        e_lit[i+4] = '\xBC';
        e_lit[i+5] = '\x9F';
        e_lit[i+6] = (i / 7) % 127 + 1;
    }
    
    auto helper = [&e_lit](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        char out_buf[4102];
        size_t total_count = 0;
        char* cur_pos = out_buf;
        int out_buffer_id = 0;
        while (true)
        {
            size_t dest_size = std::min<size_t>(4102 - total_count, out_buffer_size[out_buffer_id++]);
            auto s = obj.get(cur_pos, dest_size);
            out_buffer_id %= std::size(out_buffer_size);
            cur_pos += s;
            total_count += s;
            if (s == 0) break;
        }
        VERIFY(total_count == 4102);
        VERIFY(cur_pos == out_buf + 4102);
        for (size_t i = 0; i < 4102; ++i)
            VERIFY(out_buf[i] == e_lit[i]);
    };

    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, true, char> dev("test_file");
        auto obj = no_rb_root_cvt{std::move(dev)};
        helper(obj);
    }
    
    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, false, char> dev("test_file");
        auto obj = no_rb_root_cvt{std::move(dev)};
        helper(obj);
    }

    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, true, char> dev("test_file");
        auto tmp = no_rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, false, char> dev("test_file");
        auto tmp = no_rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }

    dump_info("Done\n");
}

void test_root_cvt_file_put_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device>::put case 1...");

    std::string e_lit; e_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = '\xE6';
        e_lit[i+1] = '\x9D';
        e_lit[i+2] = '\x8E';
        e_lit[i+3] = '\xE4';
        e_lit[i+4] = '\xBC';
        e_lit[i+5] = '\x9F';
        e_lit[i+6] = (i / 7) % 127 + 1;
    }

    auto helper = [&e_lit](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        size_t buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};

        char* cur_pos = e_lit.data();
        int buffer_id = 0;
        while (cur_pos < e_lit.data() + 4102)
        {
            size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], e_lit.data() + 4102 - cur_pos);
            obj.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;
        }
        VERIFY(cur_pos == e_lit.data() + 4102);
    };

    {
        file_guard g1("test_file", "");
        basic_file_device<true, true, char> dev("test_file", file_open_flag::trunc);
        auto obj = rb_root_cvt{std::move(dev)};
        helper(obj);
        obj.detach();
        VERIFY(g1.contents() == e_lit);
    }
    
    {
        file_guard g1("test_file");
        basic_file_device<false, true, char> dev("test_file");
        auto obj = rb_root_cvt{std::move(dev)};
        helper(obj);
        obj.detach();
        VERIFY(g1.contents() == e_lit);
    }
    
    {
        file_guard g1("test_file", "");
        basic_file_device<true, true, char> dev("test_file", file_open_flag::trunc);
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        obj.detach();
        VERIFY(g1.contents() == e_lit);
    }
    
    {
        file_guard g1("test_file");
        basic_file_device<false, true, char> dev("test_file");
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        obj.detach();
        VERIFY(g1.contents() == e_lit);
    }

    dump_info("Done\n");
}

void test_root_cvt_file_seek_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device>::seek case 1...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        
        obj.seek(3);
        VERIFY(obj.tell() == 3);
        
        char ch = 0;
        VERIFY((obj.get(&ch, 1) == 1) && (ch == '4'));
        
        obj.rseek(3);
        VERIFY(obj.tell() == 2);
        VERIFY((obj.get(&ch, 1) == 1) && (ch == '3'));
    };
    
    {
        file_guard g1("test_file", "12345");
        basic_file_device<true, true, char> dev("test_file");
        auto obj = rb_root_cvt{std::move(dev)};
        helper(obj);
    }
    
    {
        file_guard g1("test_file", "12345");
        basic_file_device<true, false, char> dev("test_file");
        auto obj = rb_root_cvt{std::move(dev)};
        helper(obj);
    }

    {
        file_guard g1("test_file", "12345");
        basic_file_device<true, true, char> dev("test_file");
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", "12345");
        basic_file_device<true, false, char> dev("test_file");
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }

    dump_info("Done\n");
}

void test_root_cvt_file_seek_2()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device>::seek case 2...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();

        char c = 0;
        VERIFY((obj.get(&c, 1) == 1) && (c == '1'));
        VERIFY((obj.get(&c, 1) == 1) && (c == '2'));
        VERIFY((obj.get(&c, 1) == 1) && (c == '3'));

        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        obj.seek(3);
        VERIFY(obj.tell() == 3);
        
        char ch = 0;
        VERIFY((obj.get(&ch, 1) == 1) && (ch == 'd'));

        obj.rseek(3);
        VERIFY(obj.tell() == 4);
        VERIFY((obj.get(&ch, 1) == 1) && (ch == 'e'));

        FAIL_RSEEK(obj, 60);
        VERIFY(obj.tell() == 5);

        FAIL_RSEEK(obj, 9);
        VERIFY(obj.tell() == 5);
    };

    {
        file_guard g1("test_file", "123abcdefg");
        basic_file_device<true, true, char> dev("test_file");
        auto obj = rb_root_cvt{std::move(dev)};
        helper(obj);
    }
    
    {
        file_guard g1("test_file", "123abcdefg");
        basic_file_device<true, false, char> dev("test_file");
        auto obj = rb_root_cvt{std::move(dev)};
        helper(obj);
    }

    {
        file_guard g1("test_file", "123abcdefg");
        basic_file_device<true, true, char> dev("test_file");
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", "123abcdefg");
        basic_file_device<true, false, char> dev("test_file");
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }

    dump_info("Done\n");
}

void test_root_cvt_file_reset_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device>::reset case 1...");

    auto helper = [](auto& obj, auto& g1, auto& g2)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        
        obj.put("hello", 5);
        obj.main_cont_beg();
        obj.put(" world", 6);
        VERIFY(obj.tell() == 6);
        
        obj.attach(basic_file_device<false, true, char>("test_file2"));
        VERIFY(g1.contents() == "hello world");
        
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        
        VERIFY(obj.tell() == 0);
        obj.put("liwei", 5);
        obj.main_cont_beg();
        obj.put(" cpp", 4);
        VERIFY(obj.tell() == 4);
        obj.flush();
        obj.device().dflush();

        VERIFY(g2.contents() == "liwei cpp");
    };
    
    {
        file_guard g1("test_file1", "");
        file_guard g2("test_file2", "");
        basic_file_device<false, true, char> dev("test_file1");
        auto obj = rb_root_cvt{std::move(dev)};
        helper(obj, g1, g2);
    }
    
    {
        file_guard g1("test_file1", "");
        file_guard g2("test_file2", "");
        basic_file_device<false, true, char> dev("test_file1");
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj, g1, g2);
    }

    dump_info("Done\n");
}

void test_root_cvt_file_device_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device>::device case 1...");

    auto helper = []<typename T>(T& obj)
    {
        using device_type = typename T::device_type;

        file_guard g1("test_file1", "");
        device_type f1("test_file1", file_open_flag::trunc);
        obj.attach(std::move(f1));
        obj.bos(); obj.main_cont_beg();
        obj.put("abc", 3);

        auto [detach1_dev, detach1_err] = obj.detach();
        f1 = std::move(detach1_dev);
        file_guard g2("test_file2", "");
        obj.attach(device_type("test_file2", file_open_flag::trunc));
        obj.bos(); obj.main_cont_beg();
        obj.put("123", 3);

        auto [detach2_dev, detach2_err] = obj.detach();
        device_type f2 = std::move(detach2_dev);
        obj.attach(std::move(f1));
        obj.bos(); obj.main_cont_beg();
        obj.put("def", 3);

        auto [detach3_dev, detach3_err] = obj.detach();
        f1 = std::move(detach3_dev);
        f1.close();
        f2.close();
        VERIFY(g1.contents() == "abcdef");
        VERIFY(g2.contents() == "123");
    };
    {
        basic_file_device<false, true, char> dev;
        auto obj = rb_root_cvt{std::move(dev)};
        helper(obj);
    }
    
    {
        basic_file_device<false, true, char> dev;
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }

    {
        basic_file_device<true, true, char> dev;
        auto obj = rb_root_cvt{std::move(dev)};
        helper(obj);
    }
    
    {
        basic_file_device<true, true, char> dev;
        auto tmp = rb_root_cvt{std::move(dev)};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }
    dump_info("Done\n");
}

void test_root_cvt_file_detach_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device>::detach case 1...");

    auto helper = []<typename T>(T& obj)
    {
        obj.bos(); obj.main_cont_beg();
        char ch = 0;
        obj.get(&ch, 1);
        VERIFY(ch == '1');
        VERIFY(obj.device().dtell() == 8);
        
        auto [dev, err] = obj.detach();
        VERIFY(dev.dtell() == 1);
    };

    file_guard g("test_file1", "12345678");
    {
        auto obj = rb_root_cvt{ifile_device<char>("test_file1")};
        helper(obj);
    }
    
    {
        runtime_cvt obj(rb_root_cvt{ifile_device<char>("test_file1")});
        helper(obj);
    }
    dump_info("Done\n");
}

void test_root_cvt_file_detach_2()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device>::detach case 2...");

    auto helper = []<typename T>(T& obj)
    {
        obj.bos(); obj.main_cont_beg();
        obj.put("123", 3);
        VERIFY(obj.device().dtell() == 0);
        
        auto [dev, err] = obj.detach();
        VERIFY(dev.dtell() == 3);
    };

    file_guard g("test_file1", "");
    {
        auto obj = rb_root_cvt{ofile_device<char>("test_file1")};
        helper(obj);
    }
    
    {
        runtime_cvt obj(rb_root_cvt{ofile_device<char>("test_file1")});
        helper(obj);
    }
    dump_info("Done\n");
}

void test_root_cvt_file_attach_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device>::attach case 1...");

    auto helper = []<typename T>(T& obj)
    {
        obj.bos(); obj.main_cont_beg();
        char ch = 0;
        obj.get(&ch, 1);
        VERIFY(ch == '1');
        VERIFY(obj.device().dtell() == 8);

        file_guard g("test_file2", "abcde");
        auto [dev, err] = obj.detach();
        obj.attach(ifile_device<char>("test_file2"));
        VERIFY(dev.dtell() == 1);
    };

    file_guard g("test_file1", "12345678");
    {
        auto obj = rb_root_cvt{ifile_device<char>("test_file1")};
        helper(obj);
    }
    
    {
        runtime_cvt obj(rb_root_cvt{ifile_device<char>("test_file1")});
        helper(obj);
    }
    dump_info("Done\n");
}

void test_root_cvt_file_attach_2()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device>::attach case 2...");

    auto helper = []<typename T>(T& obj)
    {
        obj.bos(); obj.main_cont_beg();
        obj.put("123", 3);
        VERIFY(obj.device().dtell() == 0);

        file_guard g("test_file2", "");
        auto [dev, err] = obj.detach();
        obj.attach(ofile_device<char>("test_file2"));
        VERIFY(dev.dtell() == 3);
    };

    file_guard g("test_file1", "");
    {
        auto obj = rb_root_cvt{ofile_device<char>("test_file1")};
        helper(obj);
    }

    {
        runtime_cvt obj(rb_root_cvt{ofile_device<char>("test_file1")});
        helper(obj);
    }
    dump_info("Done\n");
}

// is_eof() for generic root_cvt<file_device> — all five branches
void test_root_cvt_file_eof_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> is_eof case 1...");

    using RWDev = basic_file_device<true, true, char>;

    auto helper = [](auto& obj_input_rb, auto& obj_input_nrb,
                     auto& obj_output_nonempty, auto& obj_output_empty,
                     auto& obj_neutral)
    {
        // Branch: input mode, buffered data remains → false (no device query)
        {
            char ch = 0;
            obj_input_rb.get(&ch, 1);  // HasInBuffer=true: loads file into buffer, consumes 1
            VERIFY(!obj_input_rb.is_eof());
        }

        // Branch: input mode, buffer exhausted → query device deof()
        {
            char buf[2];
            obj_input_nrb.get(buf, 2);  // HasInBuffer=false: direct read; device now at EOF
            VERIFY(obj_input_nrb.is_eof());
        }

        // Branch: output mode, non-empty write buffer → flush first, then deof()
        {
            obj_output_nonempty.put("hello", 5);  // buffer holds 5 bytes (not yet flushed)
            VERIFY(obj_output_nonempty.is_eof());  // flushes, device at end → true
        }

        // Branch: output mode, empty write buffer → deof() without flush
        {
            // m_buf_cur == m_buffer.data() (nothing written)
            VERIFY(obj_output_empty.is_eof());  // no flush; empty file → true
        }

        // Branch: neutral mode → throws cvt_error
        {
            bool threw = false;
            try { obj_neutral.is_eof(); } catch (const cvt_error&) { threw = true; }
            VERIFY(threw);
        }
    };

    {
        file_guard g1("test_file1", "hello world");
        file_guard g2("test_file2", "hi");
        file_guard g3("test_file3", "");
        file_guard g4("test_file4", "");
        file_guard g5("test_file5", "hello");

        auto o1 = rb_root_cvt{RWDev("test_file1")};
        VERIFY(o1.bos() == io_status::input); o1.main_cont_beg();

        auto o2 = no_rb_root_cvt{RWDev("test_file2")};
        VERIFY(o2.bos() == io_status::input); o2.main_cont_beg();

        auto o3 = rb_root_cvt{RWDev("test_file3", file_open_flag::trunc)};
        VERIFY(o3.bos() == io_status::output); o3.main_cont_beg();

        auto o4 = rb_root_cvt{RWDev("test_file4", file_open_flag::trunc)};
        VERIFY(o4.bos() == io_status::output); o4.main_cont_beg();

        auto o5 = rb_root_cvt{RWDev("test_file5")};
        // No bos() → neutral

        helper(o1, o2, o3, o4, o5);
    }

    dump_info("Done\n");
}

// Mode-switching paths in generic root_cvt<file_device>
void test_root_cvt_file_mode_switch_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> mode switch case 1...");

    using RWDev = basic_file_device<true, true, char>;

    // switch_to_get(): neutral → input  (default branch)
    {
        file_guard g("test_file", "hello");
        auto obj = rb_root_cvt{RWDev("test_file")};
        // io_status = neutral; get() calls switch_to_get() → hits default case
        char ch = 0;
        VERIFY(obj.get(&ch, 1) == 1 && ch == 'h');
    }

    // switch_to_get(): output → flush → input
    {
        file_guard g("test_file", "");
        auto obj = rb_root_cvt{RWDev("test_file", file_open_flag::trunc)};
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("ABC", 3);           // buffer holds "ABC"; not yet on disk
        char ch = 0;
        size_t n = obj.get(&ch, 1);  // switch_to_get: flush "ABC", enter input, device at EOF
        VERIFY(n == 0);              // device now at end of "ABC" → nothing to read
        obj.detach();                // close file so g.contents() sees the data
        VERIFY(g.contents() == "ABC"); // flush did happen
    }

    // switch_to_put(): neutral → output  (default branch)
    {
        file_guard g("test_file", "");
        auto obj = rb_root_cvt{RWDev("test_file", file_open_flag::trunc)};
        // io_status = neutral; put() calls switch_to_put() → hits default case
        obj.put("X", 1);
        obj.detach();  // flush + close so g.contents() sees the data
        VERIFY(g.contents() == "X");
    }

    // switch_to_put(): input, no unconsumed buffer → just set output (no seek needed)
    // HasInBuffer=false ensures m_buf_cur always equals m_buf_end
    {
        file_guard g("test_file", "hello");
        auto obj = no_rb_root_cvt{RWDev("test_file")};
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        char buf[5];
        obj.get(buf, 5);  // direct device read; device at pos 5; m_buf_cur == m_buf_end
        obj.put("!", 1);  // switch_to_put: no buffered data → just set output
        obj.detach();     // flush + close
        VERIFY(g.contents() == "hello!");
    }

    // switch_to_put(): input, unconsumed buffered data → seek(tell()) to sync device
    {
        file_guard g("test_file", "hello world");
        auto obj = rb_root_cvt{RWDev("test_file")};
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        char ch = 0;
        obj.get(&ch, 1);      // loads 11 bytes into buffer; device at 11; consumes 1
        VERIFY(ch == 'h');
        VERIFY(obj.tell() == 1); // = device(11) - bos_len(0) - buffered(10)
        // 10 unconsumed bytes sit in the read buffer
        obj.put("X", 1);      // switch_to_put: seek(1) syncs device; then write "X"
        obj.detach();         // flush + close
        VERIFY(g.contents() == "hXllo world"); // byte at stream-pos 1 ('e') overwritten
    }

    dump_info("Done\n");
}

// Error-throw paths: wrong io_status for tell / get / put
void test_root_cvt_file_error_paths_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> error paths case 1...");

    // tell() in neutral state → throws cvt_error
    {
        file_guard g("test_file", "hello");
        auto obj = rb_root_cvt{basic_file_device<true, true, char>("test_file")};
        bool threw = false;
        try { (void)obj.tell(); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // get() on a get-only device in neutral state → throws
    // (support_put=false: switch_to_get not compiled; status check fires)
    {
        file_guard g("test_file", "hello");
        auto obj = rb_root_cvt{ifile_device<char>("test_file")};
        bool threw = false;
        try { char ch = 0; obj.get(&ch, 1); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // put() on a put-only device in neutral state → throws
    // (support_get=false: switch_to_put not compiled; status check fires)
    {
        file_guard g("test_file", "");
        auto obj = rb_root_cvt{ofile_device<char>("test_file")};
        bool threw = false;
        try { obj.put("hi", 2); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}

// seek() position-overflow detection for generic root_cvt<file_device>
void test_root_cvt_file_seek_overflow_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> seek overflow case 1...");

    // After get() + main_cont_beg(), m_bos_len = device(5) - buffered(4) = 1.
    // Seeking SIZE_MAX triggers: SIZE_MAX > SIZE_MAX - 1 → overflow throw.
    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        char ch = 0;
        obj.get(&ch, 1);      // rb: loads 5 bytes into buffer; device at 5; consumes 1
        obj.main_cont_beg();  // m_bos_len = 5 - 4 = 1
        bool threw = false;
        try {
            obj.seek(std::numeric_limits<size_t>::max()); // SIZE_MAX > SIZE_MAX - 1
        } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    };

    {
        file_guard g("test_file", "hello");
        auto obj = rb_root_cvt{basic_file_device<true, true, char>("test_file")};
        helper(obj);
    }
    {
        file_guard g("test_file", "hello");
        auto tmp = rb_root_cvt{basic_file_device<true, true, char>("test_file")};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }

    dump_info("Done\n");
}
// adjust() and retrieve() are no-ops on the generic template but must be reachable.
void test_root_cvt_file_adjust_retrieve_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> adjust/retrieve...");

    file_guard g("test_file", "hello");
    auto obj = rb_root_cvt{ifile_device<char>("test_file")};
    obj.bos(); obj.main_cont_beg();

    cvt_behavior beh;
    obj.adjust(beh);

    cvt_status stat;
    obj.retrieve(stat);

    dump_info("Done\n");
}

// put() with to_size == remain (exact fill) and large-write bypass path.
void test_root_cvt_file_put_exact_and_large_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> put exact-fill and large-bypass...");

    constexpr size_t BUF = rb_root_cvt<ofile_device<char>>::s_buffer_length;

    // Exact fill: to_size == remain == s_buffer_length (lines 739-742)
    {
        file_guard g("test_file", "");
        std::string data(BUF, 'x');
        {
            auto obj = rb_root_cvt{ofile_device<char>("test_file")};
            obj.bos(); obj.main_cont_beg();
            obj.put(data.data(), BUF);
        } // dtor flushes and closes file
        VERIFY(g.contents() == data);
    }

    // Large bypass: buf_used > 0 AND to_size >= s_buffer_length/2 (lines 751-752)
    {
        file_guard g("test_file", "");
        std::string small(100, 'a');
        std::string large(BUF, 'b');
        {
            auto obj = rb_root_cvt{ofile_device<char>("test_file")};
            obj.bos(); obj.main_cont_beg();
            obj.put(small.data(), small.size());
            obj.put(large.data(), large.size());
        } // dtor flushes and closes file
        VERIFY(g.contents() == small + large);
    }

    dump_info("Done\n");
}

// seek() and rseek() in output mode must flush the buffer first.
void test_root_cvt_file_seek_rseek_output_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> seek/rseek in output mode...");

    // seek() flushes first (line 856)
    {
        file_guard g("test_file", "");
        auto obj = rb_root_cvt{ofile_device<char>("test_file")};
        obj.bos();
        obj.main_cont_beg();
        obj.put("hello", 5);
        obj.seek(0);
        VERIFY(g.contents() == "hello");
    }

    // rseek() flushes first (line 893)
    {
        file_guard g("test_file", "");
        auto obj = rb_root_cvt{ofile_device<char>("test_file")};
        obj.bos();
        obj.main_cont_beg();
        obj.put("hello world", 11);
        obj.rseek(6);
        VERIFY(obj.tell() == 5);
    }

    dump_info("Done\n");
}

// rseek() when m_bos_len > device.dsize() must throw (line 898).
void test_root_cvt_file_rseek_bos_overflow_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> rseek bos_len-overflow...");

    file_guard g1("test_file1", "12345678");
    file_guard g2("test_file2", "ab");
    auto obj = rb_root_cvt{ifile_device<char>("test_file1")};
    obj.bos();
    char buf[3];
    obj.get(buf, 3);     // buffer filled (8 bytes), returns 3; dtell=8, buffered=5
    obj.main_cont_beg(); // m_bos_len = 8 - 5 = 3
    obj.device() = ifile_device<char>("test_file2"); // dsize()=2 < m_bos_len=3
    bool threw = false;
    try { obj.rseek(0); } catch (const cvt_error&) { threw = true; }
    VERIFY(threw);

    dump_info("Done\n");
}

// switch_to_get() when already in input and switch_to_put() when already in output
// are both no-ops (lines 926-927, 964-965).
void test_root_cvt_file_switch_noop_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> switch_to_get/put no-op...");

    // switch_to_get() already input → no-op (lines 926-927)
    {
        file_guard g("test_file", "hello");
        auto obj = rb_root_cvt{file_device<char>("test_file")};
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        obj.switch_to_get();
        char buf[5] = {};
        VERIFY(obj.get(buf, 5) == 5);
        VERIFY(std::string(buf, 5) == "hello");
    }

    // switch_to_put() already output → no-op (lines 964-965)
    {
        file_guard g("test_file", "");
        auto obj = rb_root_cvt{file_device<char>("test_file", file_open_flag::trunc)};
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.switch_to_put();
        obj.put("world", 5);
        obj.detach();  // close file so g.contents() sees the data
        VERIFY(g.contents() == "world");
    }

    dump_info("Done\n");
}

// catch(...) in move-assignment (line 307) and destructor (line 350) must swallow
// exceptions thrown by flush() without propagating them.
void test_root_cvt_file_flush_exception_catch_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt flush exception caught in move-assign and destructor...");

    // Move assignment: flush on *this throws → caught silently (line 307)
    {
        auto obj = rb_root_cvt{throw_write_device{}};
        obj.bos(); obj.main_cont_beg();
        obj.put("hello", 5);
        obj.device().should_throw = true;

        auto obj2 = rb_root_cvt{throw_write_device{}};
        obj2.bos(); obj2.main_cont_beg();
        obj = std::move(obj2);
    }

    // Destructor: flush throws → caught, no std::terminate (line 350)
    {
        auto obj = rb_root_cvt{throw_write_device{}};
        obj.bos(); obj.main_cont_beg();
        obj.put("hello", 5);
        obj.device().should_throw = true;
    }

    dump_info("Done\n");
}
