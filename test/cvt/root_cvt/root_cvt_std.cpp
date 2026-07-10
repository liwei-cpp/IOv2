#include <typeinfo>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/std_device.h>

#include <common/dump_info.h>
#include <common/stdio_guard.h>
#include <common/verify.h>

void test_root_cvt_std_gen_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<std_device> general case 1...");
    
    {
        using CheckType = no_rb_root_cvt<std_device<STDIN_FILENO>>;
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
        using CheckType = rb_root_cvt<std_device<STDOUT_FILENO>>;
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("hello", 5);
        auto obj2(std::move(obj));
        
        obj2.put(" world", 6);
        obj2.detach();
        VERIFY(g.contents() == "hello world");
    };
    {
        auto obj1 = rb_root_cvt{std_device<STDOUT_FILENO>{}};
        helper1(obj1);

        runtime_cvt obj2(rb_root_cvt{std_device<STDOUT_FILENO>{}});
        helper1(obj2);
    }

    auto helper2 = []<typename T>(T& obj)
    {
        oguard<true> g;
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("hello", 5);
        T obj2{rb_root_cvt{std_device<STDOUT_FILENO>{}}};
        obj2 = std::move(obj);
        
        obj2.put(" world", 6);
        obj2.attach(std_device<STDOUT_FILENO>{});
        VERIFY(g.contents() == "hello world");
    };

    {
        auto obj1 = rb_root_cvt{std_device<STDOUT_FILENO>{}};
        helper2(obj1);

        runtime_cvt obj2(rb_root_cvt{std_device<STDOUT_FILENO>{}});
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
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        VERIFY(obj.get(str.data(), 5) == 5);
        VERIFY(str == "hello");
        
        auto obj2(std::move(obj));
        str.resize(6);
        VERIFY(obj2.get(str.data(), 6) == 6);
        VERIFY(str == " world");
    };
    {
        auto obj1 = rb_root_cvt{std_device<STDIN_FILENO>{}};
        helper1(obj1);

        runtime_cvt obj2(rb_root_cvt{std_device<STDIN_FILENO>{}});
        helper1(obj2);
    }
    
    auto helper2 = []<typename T>(T& obj)
    {
        iguard g("hello world");
        std::string str; str.resize(5);
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        VERIFY(obj.get(str.data(), 5) == 5);
        VERIFY(str == "hello");
        
        T obj2{rb_root_cvt{std_device<STDIN_FILENO>{}}};
        obj2 = std::move(obj);
        str.resize(6);
        VERIFY(obj2.get(str.data(), 6) == 6);
        VERIFY(str == " world");
    };
    {
        auto obj1 = rb_root_cvt{std_device<STDIN_FILENO>{}};
        helper2(obj1);

        runtime_cvt obj2(rb_root_cvt{std_device<STDIN_FILENO>{}});
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
        iguard g(e_lit);
        auto obj = rb_root_cvt{std_device<STDIN_FILENO>{}};
        helper(obj);
    }
    {
        iguard g(e_lit);
        runtime_cvt obj2{rb_root_cvt{std_device<STDIN_FILENO>{}}};
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
        iguard g(e_lit);
        auto obj = no_rb_root_cvt{std_device<STDIN_FILENO>{}};
        helper(obj);
    }
    {
        iguard g(e_lit);
        runtime_cvt obj2{no_rb_root_cvt{std_device<STDIN_FILENO>{}}};
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
        obj.flush();
    };

    {
        oguard<true> g;
        {
            auto obj = no_rb_root_cvt{std_device<STDOUT_FILENO>{}};
            helper(obj);
        }
        VERIFY(g.contents() == e_lit);
    }

    {
        oguard<false> g;
        auto obj = no_rb_root_cvt{std_device<STDERR_FILENO>{}};
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }
    
    {
        oguard<true> g;
        {
            runtime_cvt obj(no_rb_root_cvt{std_device<STDOUT_FILENO>{}});
            helper(obj);
        }
        VERIFY(g.contents() == e_lit);
    }

    {
        oguard<false> g;
        runtime_cvt obj(no_rb_root_cvt{std_device<STDERR_FILENO>{}});
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }

    dump_info("Done\n");
}