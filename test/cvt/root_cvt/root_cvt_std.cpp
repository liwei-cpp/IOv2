#include <typeinfo>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/std_device.h>

#include <common/dump_info.h>
#include <common/stdio_guard.h>

void test_root_cvt_std_gen_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<std_device> general case 1...");
    
    {
        using CheckType = root_cvt<std_device<STDIN_FILENO>, false>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, std_device<STDIN_FILENO>>);
        static_assert(std::is_same_v<CheckType::internal_type, char>);
        static_assert(std::is_same_v<CheckType::external_type, char>);

        static_assert(!cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = root_cvt<std_device<STDOUT_FILENO>, true>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, std_device<STDOUT_FILENO>>);
        static_assert(std::is_same_v<CheckType::internal_type, char>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(!cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }

    dump_info("Done\n");
}

void test_root_cvt_std_gen_2()
{
    using namespace IOv2;
    dump_info("Test root_cvt<std_device> general case 2...");

    auto helper1 = [](auto& obj)
    {
        oguard<true> g;
        if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<std_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put("hello", 5);
        auto obj2(std::move(obj));
        
        obj2.put(" world", 6);
        obj2.detach();
        if (g.contents() != "hello world") throw std::runtime_error("root_cvt<std_device> move constructor response incorrect");
    };
    {
        auto obj1 = make_root_cvt<true>(std_device<STDOUT_FILENO>{});
        helper1(obj1);

        runtime_cvt obj2(make_root_cvt<true>(std_device<STDOUT_FILENO>{}));
        helper1(obj2);
    }

    auto helper2 = []<typename T>(T& obj)
    {
        oguard<true> g;
        if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<std_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put("hello", 5);
        T obj2{make_root_cvt<true>(std_device<STDOUT_FILENO>{})};
        obj2 = std::move(obj);
        
        obj2.put(" world", 6);
        obj2.attach(std_device<STDOUT_FILENO>{});
        if (g.contents() != "hello world") throw std::runtime_error("root_cvt<std_device> move assignment response incorrect");
    };

    {
        auto obj1 = make_root_cvt<true>(std_device<STDOUT_FILENO>{});
        helper2(obj1);

        runtime_cvt obj2(make_root_cvt<true>(std_device<STDOUT_FILENO>{}));
        helper2(obj2);
    }
    dump_info("Done\n");
}

void test_root_cvt_std_gen_3()
{
    using namespace IOv2;
    dump_info("Test root_cvt<std_device> general case 3...");

    auto helper1 = [](auto& obj)
    {
        iguard g("hello world");
        std::string str; str.resize(5);
        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<std_device>::bos response incorrect");
        obj.main_cont_beg();
        if (obj.get(str.data(), 5) != 5) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        if (str != "hello") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        
        auto obj2(std::move(obj));
        str.resize(6);
        if (obj2.get(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        if (str != " world") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
    };
    {
        auto obj1 = make_root_cvt<true>(std_device<STDIN_FILENO>{});
        helper1(obj1);

        runtime_cvt obj2(make_root_cvt<true>(std_device<STDIN_FILENO>{}));
        helper1(obj2);
    }
    
    auto helper2 = []<typename T>(T& obj)
    {
        iguard g("hello world");
        std::string str; str.resize(5);
        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<std_device>::bos response incorrect");
        obj.main_cont_beg();
        if (obj.get(str.data(), 5) != 5) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        if (str != "hello") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        
        T obj2{make_root_cvt<true>(std_device<STDIN_FILENO>{})};
        obj2 = std::move(obj);
        str.resize(6);
        if (obj2.get(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        if (str != " world") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
    };
    {
        auto obj1 = make_root_cvt<true>(std_device<STDIN_FILENO>{});
        helper2(obj1);

        runtime_cvt obj2(make_root_cvt<true>(std_device<STDIN_FILENO>{}));
        helper2(obj2);
    }
    dump_info("Done\n");
}

void test_root_cvt_std_get_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<std_device>::get case 1...");

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
        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<std_device>::bos response incorrect");
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
        if (total_count != 4102) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        if (cur_pos != out_buf + 4102) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        for (size_t i = 0; i < 4102; ++i)
            if (out_buf[i] != e_lit[i]) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
    };

    {
        iguard g(e_lit);
        auto obj = make_root_cvt<true>(std_device<STDIN_FILENO>{});
        helper(obj);
    }
    {
        iguard g(e_lit);
        runtime_cvt obj2{make_root_cvt<true>(std_device<STDIN_FILENO>{})};
        helper(obj2);
    }

    dump_info("Done\n");
}

void test_root_cvt_std_get_nra_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<std_device>::get_nra case 1...");

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
        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<std_device>::bos response incorrect");
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
        if (total_count != 4102) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        if (cur_pos != out_buf + 4102) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
        for (size_t i = 0; i < 4102; ++i)
            if (out_buf[i] != e_lit[i]) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
    };

    {
        iguard g(e_lit);
        auto obj = make_root_cvt<false>(std_device<STDIN_FILENO>{});
        helper(obj);
    }
    {
        iguard g(e_lit);
        runtime_cvt obj2{make_root_cvt<false>(std_device<STDIN_FILENO>{})};
        helper(obj2);
    }

    dump_info("Done\n");
}

void test_root_cvt_std_put_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<std_device>::put case 1...");

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
        if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<std_device>::bos response incorrect");
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
        if (cur_pos != e_lit.data() + 4102) throw std::runtime_error("root_cvt<std_device>::put response incorrect");
        obj.flush();
    };

    {
        oguard<true> g;
        {
            auto obj = make_root_cvt<false>(std_device<STDOUT_FILENO>{});
            helper(obj);
        }
        if (g.contents() != e_lit) throw std::runtime_error("root_cvt<std_device>::put_bos fail");
    }

    {
        oguard<false> g;
        auto obj = make_root_cvt<false>(std_device<STDERR_FILENO>{});
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("root_cvt<std_device>::put_bos fail");
    }
    
    {
        oguard<true> g;
        {
            runtime_cvt obj(make_root_cvt<false>(std_device<STDOUT_FILENO>{}));
            helper(obj);
        }
        if (g.contents() != e_lit) throw std::runtime_error("root_cvt<std_device>::put_bos fail");
    }

    {
        oguard<false> g;
        runtime_cvt obj(make_root_cvt<false>(std_device<STDERR_FILENO>{}));
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("root_cvt<std_device>::put_bos fail");
    }

    dump_info("Done\n");
}