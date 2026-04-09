#include <list>
#include <iterator>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/file_device.h>

#include <common/dump_info.h>
#include <common/file_guard.h>
#include <common/verify.h>

void test_root_cvt_file_gen_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> general case 1...");
    
    {
        using CheckType = root_cvt<basic_file_device<true, true, char>, false>;
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
        using CheckType = root_cvt<basic_file_device<true, true, char8_t>, true>;
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
        using CheckType = root_cvt<basic_file_device<true, false, char>, false>;
        static_assert(!cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = root_cvt<basic_file_device<false, true, char>, true>;
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
        if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<file_device>::bos response incorrect");
        obj.main_cont_beg();
        auto obj2(std::move(obj));
        
        obj2.put(" world", 6);
    };

    {
        file_guard g1("test_file", "hello");
        basic_file_device<true, true, char> dev("test_file");
        dev.drseek(0);
        auto obj = make_root_cvt<true>(std::move(dev));
        helper1(obj);
        if (g1.contents() != "hello world") throw std::runtime_error("root_cvt<file_device> output incorrect");
    }
    
    {
        file_guard g1("test_file", "hello");
        basic_file_device<false, true, char> dev("test_file");
        dev.drseek(0);
        auto obj = make_root_cvt<true>(std::move(dev));
        helper1(obj);
        if (g1.contents() != " world") throw std::runtime_error("root_cvt<file_device> output incorrect");
    }
    
    {
        file_guard g1("test_file", "hello");
        basic_file_device<true, true, char> dev("test_file");
        dev.drseek(0);
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper1(obj);
        if (g1.contents() != "hello world") throw std::runtime_error("root_cvt<file_device> output incorrect");
    }
    
    {
        file_guard g1("test_file", "hello");
        basic_file_device<false, true, char> dev("test_file");
        dev.drseek(0);
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper1(obj);
        if (g1.contents() != " world") throw std::runtime_error("root_cvt<file_device> output incorrect");
    }

    auto helper2 = []<typename T>(T& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        using dev_type = typename T::device_type;
        T obj2{make_root_cvt<true>(dev_type{})};
        obj2 = std::move(obj);
        obj2.put(" world", 6);
    };

    {
        file_guard g1("test_file", "hello");
        basic_file_device<true, true, char> dev("test_file");
        dev.drseek(0);
        auto obj = make_root_cvt<true>(std::move(dev));
        helper2(obj);
        if (g1.contents() != "hello world") throw std::runtime_error("root_cvt<file_device> output incorrect");
    }
    
    {
        file_guard g1("test_file", "hello");
        basic_file_device<false, true, char> dev("test_file");
        dev.drseek(0);
        auto obj = make_root_cvt<true>(std::move(dev));
        helper2(obj);
        if (g1.contents() != " world") throw std::runtime_error("root_cvt<file_device> output incorrect");
    }
    
    {
        file_guard g1("test_file", "hello");
        basic_file_device<true, true, char> dev("test_file");
        dev.drseek(0);
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper2(obj);
        if (g1.contents() != "hello world") throw std::runtime_error("root_cvt<file_device> output incorrect");
    }
    
    {
        file_guard g1("test_file", "hello");
        basic_file_device<false, true, char> dev("test_file");
        dev.drseek(0);
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper2(obj);
        if (g1.contents() != " world") throw std::runtime_error("root_cvt<file_device> output incorrect");
    }

    dump_info("Done\n");
}

void test_root_cvt_file_gen_3()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device> general case 3...");

    auto helper1 = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        std::string str; str.resize(5);
        if (obj.get(str.data(), 5) != 5) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        if (str != "hello") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        if (obj.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");

        auto obj2(std::move(obj));
        if (obj2.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
        str.resize(6);
        if (obj2.get(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        if (str != " world") throw std::runtime_error("root_cvt<mem_device> move constructor response incorrect");
        if (obj2.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
    };
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, true, char> dev("test_file");
        auto obj = make_root_cvt<true>(std::move(dev));
        helper1(obj);
    }
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, false, char> dev("test_file");
        auto obj = make_root_cvt<true>(std::move(dev));
        helper1(obj);
    }
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, true, char> dev("test_file");
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper1(obj);
    }
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, false, char> dev("test_file");
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper1(obj);
    }

    auto helper2 = []<typename T>(T& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        std::string str; str.resize(5);
        if (obj.get(str.data(), 5) != 5) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        if (str != "hello") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        if (obj.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");

        using dev_type = typename T::device_type;
        T obj2{make_root_cvt<true>(dev_type{})};
        obj2 = std::move(obj);

        if (obj2.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
        str.resize(6);
        if (obj2.get(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        if (str != " world") throw std::runtime_error("root_cvt<mem_device> move constructor response incorrect");
        if (obj2.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
    };

    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, true, char> dev("test_file");
        auto obj = make_root_cvt<true>(std::move(dev));
        helper2(obj);
    }
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, false, char> dev("test_file");
        auto obj = make_root_cvt<true>(std::move(dev));
        helper2(obj);
    }
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, true, char> dev("test_file");
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper2(obj);
    }
    
    {
        file_guard g1("test_file", "hello world");
        basic_file_device<true, false, char> dev("test_file");
        auto tmp = make_root_cvt<true>(std::move(dev));
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
        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<file_device>::bos response incorrect");
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
        if (total_count != 4102) throw std::runtime_error("root_cvt<file_device>::get response incorrect");
        if (cur_pos != out_buf + 4102) throw std::runtime_error("root_cvt<file_device>::get response incorrect");
        for (size_t i = 0; i < 4102; ++i)
            if (out_buf[i] != e_lit[i]) throw std::runtime_error("root_cvt<file_device>::get response incorrect");
    };

    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, true, char> dev("test_file");
        auto obj = make_root_cvt<true>(std::move(dev));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, false, char> dev("test_file");
        auto obj = make_root_cvt<true>(std::move(dev));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, true, char> dev("test_file");
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, false, char> dev("test_file");
        auto tmp = make_root_cvt<true>(std::move(dev));
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
        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<file_device>::bos response incorrect");
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
        if (total_count != 4102) throw std::runtime_error("root_cvt<file_device>::get response incorrect");
        if (cur_pos != out_buf + 4102) throw std::runtime_error("root_cvt<file_device>::get response incorrect");
        for (size_t i = 0; i < 4102; ++i)
            if (out_buf[i] != e_lit[i]) throw std::runtime_error("root_cvt<file_device>::get response incorrect");
    };

    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, true, char> dev("test_file");
        auto obj = make_root_cvt<false>(std::move(dev));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, false, char> dev("test_file");
        auto obj = make_root_cvt<false>(std::move(dev));
        helper(obj);
    }

    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, true, char> dev("test_file");
        auto tmp = make_root_cvt<false>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", e_lit);
        basic_file_device<true, false, char> dev("test_file");
        auto tmp = make_root_cvt<false>(std::move(dev));
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
        if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<file_device>::bos response incorrect");
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
        if (cur_pos != e_lit.data() + 4102) throw std::runtime_error("root_cvt<file_device>::put response incorrect");
    };

    {
        file_guard g1("test_file", "");
        basic_file_device<true, true, char> dev("test_file", file_open_flag::trunc);
        auto obj = make_root_cvt<true>(std::move(dev));
        helper(obj);
        obj.detach();
        if (g1.contents() != e_lit) throw std::runtime_error("root_cvt<file_device>::put response incorrect");
    }
    
    {
        file_guard g1("test_file");
        basic_file_device<false, true, char> dev("test_file");
        auto obj = make_root_cvt<true>(std::move(dev));
        helper(obj);
        obj.detach();
        if (g1.contents() != e_lit) throw std::runtime_error("root_cvt<file_device>::put response incorrect");
    }
    
    {
        file_guard g1("test_file", "");
        basic_file_device<true, true, char> dev("test_file", file_open_flag::trunc);
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        obj.detach();
        if (g1.contents() != e_lit) throw std::runtime_error("root_cvt<file_device>::put response incorrect");
    }
    
    {
        file_guard g1("test_file");
        basic_file_device<false, true, char> dev("test_file");
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        obj.detach();
        if (g1.contents() != e_lit) throw std::runtime_error("root_cvt<file_device>::put response incorrect");
    }

    dump_info("Done\n");
}

void test_root_cvt_file_seek_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<file_device>::seek case 1...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<file_device>::bos response incorrect");
        obj.main_cont_beg();
        
        obj.seek(3);
        if (obj.tell() != 3) throw std::runtime_error("root_cvt<file_device>::tell fail");
        
        char ch = 0;
        if ((obj.get(&ch, 1) != 1) || (ch != '4')) throw std::runtime_error("root_cvt<file_device>::get fail");
        
        obj.rseek(3);
        if (obj.tell() != 2) throw std::runtime_error("root_cvt<file_device>::tell fail");
        if ((obj.get(&ch, 1) != 1) || (ch != '3')) throw std::runtime_error("root_cvt<file_device>::get fail");
    };
    
    {
        file_guard g1("test_file", "12345");
        basic_file_device<true, true, char> dev("test_file");
        auto obj = make_root_cvt<true>(std::move(dev));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", "12345");
        basic_file_device<true, false, char> dev("test_file");
        auto obj = make_root_cvt<true>(std::move(dev));
        helper(obj);
    }

    {
        file_guard g1("test_file", "12345");
        basic_file_device<true, true, char> dev("test_file");
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", "12345");
        basic_file_device<true, false, char> dev("test_file");
        auto tmp = make_root_cvt<true>(std::move(dev));
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
        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<file_device>::bos response incorrect");
        obj.main_cont_beg();

        char c = 0;
        if ((obj.get(&c, 1) != 1) || (c != '1')) throw std::runtime_error("root_cvt<file_device>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != '2')) throw std::runtime_error("root_cvt<file_device>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != '3')) throw std::runtime_error("root_cvt<file_device>::get_nra fail");

        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("root_cvt<file_device>::get_bos fail");

        obj.seek(3);
        if (obj.tell() != 3) throw std::runtime_error("root_cvt<file_device>::tell fail");
        
        char ch = 0;
        if ((obj.get(&ch, 1) != 1) || (ch != 'd')) throw std::runtime_error("root_cvt<file_device>::get fail");

        obj.rseek(3);
        if (obj.tell() != 4) throw std::runtime_error("root_cvt<file_device>::tell fail");
        if ((obj.get(&ch, 1) != 1) || (ch != 'e')) throw std::runtime_error("root_cvt<file_device>::get fail");

        FAIL_RSEEK(obj, 60);
        if (obj.tell() != 5) throw std::runtime_error("root_cvt<file_device>::tell fail");

        FAIL_RSEEK(obj, 9);
        if (obj.tell() != 5) throw std::runtime_error("root_cvt<file_device>::tell fail");
    };

    {
        file_guard g1("test_file", "123abcdefg");
        basic_file_device<true, true, char> dev("test_file");
        auto obj = make_root_cvt<true>(std::move(dev));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", "123abcdefg");
        basic_file_device<true, false, char> dev("test_file");
        auto obj = make_root_cvt<true>(std::move(dev));
        helper(obj);
    }

    {
        file_guard g1("test_file", "123abcdefg");
        basic_file_device<true, true, char> dev("test_file");
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }
    
    {
        file_guard g1("test_file", "123abcdefg");
        basic_file_device<true, false, char> dev("test_file");
        auto tmp = make_root_cvt<true>(std::move(dev));
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
        if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<file_device>::bos response incorrect");
        obj.main_cont_beg();
        
        obj.put("hello", 5);
        obj.main_cont_beg();
        obj.put(" world", 6);
        if (obj.tell() != 6) throw std::runtime_error("root_cvt<file_device>::tell incorrect");
        
        obj.attach(basic_file_device<false, true, char>("test_file2"));
        if (g1.contents() != "hello world") throw std::runtime_error("root_cvt<file_device>::reset incorrect");
        
        if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<file_device>::bos response incorrect");
        obj.main_cont_beg();
        
        if (obj.tell() != 0) throw std::runtime_error("root_cvt<file_device>::tell incorrect");
        obj.put("liwei", 5);
        obj.main_cont_beg();
        obj.put(" cpp", 4);
        if (obj.tell() != 4) throw std::runtime_error("root_cvt<file_device>::tell incorrect");
        obj.flush();

        if (g2.contents() != "liwei cpp") throw std::runtime_error("root_cvt<file_device>::put incorrect");
    };
    
    {
        file_guard g1("test_file1", "");
        file_guard g2("test_file2", "");
        basic_file_device<false, true, char> dev("test_file1");
        auto obj = make_root_cvt<true>(std::move(dev));
        helper(obj, g1, g2);
    }
    
    {
        file_guard g1("test_file1", "");
        file_guard g2("test_file2", "");
        basic_file_device<false, true, char> dev("test_file1");
        auto tmp = make_root_cvt<true>(std::move(dev));
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

        f1 = obj.detach();
        file_guard g2("test_file2", "");
        obj.attach(device_type("test_file2", file_open_flag::trunc));
        obj.bos(); obj.main_cont_beg();
        obj.put("123", 3);
        
        device_type f2 = obj.detach();
        obj.attach(std::move(f1));
        obj.bos(); obj.main_cont_beg();
        obj.put("def", 3);
        
        f1 = obj.detach();
        f1.close();
        f2.close();
        if (g1.contents() != "abcdef") throw std::runtime_error("root_cvt<file_device> output incorrect");
        if (g2.contents() != "123") throw std::runtime_error("root_cvt<file_device> output incorrect");
    };
    {
        basic_file_device<false, true, char> dev;
        auto obj = make_root_cvt<true>(std::move(dev));
        helper(obj);
    }
    
    {
        basic_file_device<false, true, char> dev;
        auto tmp = make_root_cvt<true>(std::move(dev));
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }

    {
        basic_file_device<true, true, char> dev;
        auto obj = make_root_cvt<true>(std::move(dev));
        helper(obj);
    }
    
    {
        basic_file_device<true, true, char> dev;
        auto tmp = make_root_cvt<true>(std::move(dev));
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
        
        auto dev = obj.detach();
        VERIFY(dev.dtell() == 1);
    };

    file_guard g("test_file1", "12345678");
    {
        auto obj = make_root_cvt<true>(ifile_device<char>("test_file1"));
        helper(obj);
    }
    
    {
        runtime_cvt obj(make_root_cvt<true>(ifile_device<char>("test_file1")));
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
        
        auto dev = obj.detach();
        VERIFY(dev.dtell() == 3);
    };

    file_guard g("test_file1", "");
    {
        auto obj = make_root_cvt<true>(ofile_device<char>("test_file1"));
        helper(obj);
    }
    
    {
        runtime_cvt obj(make_root_cvt<true>(ofile_device<char>("test_file1")));
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

        file_guard g("test_file2", "");
        auto dev = obj.attach(ifile_device<char>("test_file2"));
        VERIFY(dev.dtell() == 1);
    };

    file_guard g("test_file1", "12345678");
    {
        auto obj = make_root_cvt<true>(ifile_device<char>("test_file1"));
        helper(obj);
    }
    
    {
        runtime_cvt obj(make_root_cvt<true>(ifile_device<char>("test_file1")));
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
        auto dev = obj.attach(ofile_device<char>("test_file2"));
        VERIFY(dev.dtell() == 3);
    };

    file_guard g("test_file1", "");
    {
        auto obj = make_root_cvt<true>(ofile_device<char>("test_file1"));
        helper(obj);
    }
    
    {
        runtime_cvt obj(make_root_cvt<true>(ofile_device<char>("test_file1")));
        helper(obj);
    }
    dump_info("Done\n");
}