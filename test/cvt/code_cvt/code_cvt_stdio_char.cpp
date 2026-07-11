#include <cvt/code_cvt.h>
#include <cvt/code_cvt_stdio.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <device/std_device.h>

#include <support/dump_info.h>
#include <support/stdio_guard.h>
#include <support/verify.h>
void test_code_cvt_stdio_char_gen_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device> general case 1...");
    
    {
        using CheckType = code_cvt<rb_root_cvt<std_device<STDIN_FILENO>>, char32_t>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, std_device<STDIN_FILENO>>);
        static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        
        static_assert(!cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, std_device<STDOUT_FILENO>>);
        static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(!cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, wchar_t>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, std_device<STDERR_FILENO>>);
        static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(!cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }

    dump_info("Done\n");
}

void test_code_cvt_stdio_char_gen_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device> general case 2...");

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

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();

        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
        std::vector<char32_t> out_buf; out_buf.resize(4102);
        size_t total_count = 0;
        char32_t* cur_pos = out_buf.data();
        int out_buffer_id = 0;
        while (true)
        {
            auto obj2(std::move(obj));
            size_t dest_size = std::min<size_t>(4102 - total_count, out_buffer_size[out_buffer_id++]);
            auto s = obj2.get(cur_pos, dest_size);
            out_buffer_id %= std::size(out_buffer_size);
            cur_pos += s;
            total_count += s;
            if (s == 0) break;
            obj = std::move(obj2);
        }
    
        VERIFY(cur_pos - out_buf.data() == 4102 / 7 * 3);
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            VERIFY(*it++ == U'李');
            VERIFY(*it++ == U'伟');
            VERIFY(*it++ == (i / 3) % 127 + 1);
        }
    };

    using CheckType = code_cvt<rb_root_cvt<std_device<STDIN_FILENO>>, char32_t>;
    {
        iguard g(e_lit);
        CheckType obj{rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8"};
        helper(obj);
    }
    {
        iguard g(e_lit);
        CheckType tmp{rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8"};
        runtime_cvt obj{std::move(tmp)};
        helper(obj);
    }
    dump_info("Done\n");
}

void test_code_cvt_stdio_char_gen_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device> general case 3...");

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
    std::u32string i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(U'李');
        i_lit.push_back(U'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    auto helper = [&i_lit](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();

        size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        size_t total_count = 0;
        char32_t* cur_pos = i_lit.data();
        int buffer_id = 0;
        while (total_count < 4102 / 7 * 3)
        {
            auto obj2(std::move(obj));
            size_t dest_size = std::min<size_t>(4102 / 7 * 3 - total_count, buffer_size[buffer_id++]);
            obj2.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;
            total_count += dest_size;
            obj = std::move(obj2);
        }
        obj.flush();
    };

    {
        oguard<true> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        CheckType obj(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;
        CheckType obj(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }

    {
        oguard<true> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        CheckType tmp(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj{std::move(tmp)};
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;
        CheckType tmp(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj{std::move(tmp)};
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }

    dump_info("Done\n");
}

void test_code_cvt_stdio_char_bos_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device>::bos case 1...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
    };

    using CheckType = code_cvt<rb_root_cvt<std_device<STDIN_FILENO>>, char32_t>;
    {
        iguard g("12345");
        CheckType obj(rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
    }
    {
        iguard g("12345");
        CheckType tmp(rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }
    dump_info("Done\n");
}

void test_code_cvt_stdio_char_bos_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device>::bos case 2...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        
        char32_t c = 0;
        VERIFY((obj.get(&c, 1) == 1) && (c == '1'));
        VERIFY((obj.get(&c, 1) == 1) && (c == '2'));
        VERIFY((obj.get(&c, 1) == 1) && (c == '3'));

        obj.main_cont_beg();
    };

    std::string info;
    info += '1'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '2'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '3'; info += '\x00'; info += '\x00'; info += '\x00';
    
    using CheckType = code_cvt<rb_root_cvt<std_device<STDIN_FILENO>>, char32_t>;
    {
        iguard g(info);
        CheckType obj(rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
    }
    {
        iguard g(info);
        CheckType tmp(rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj{std::move(tmp)};
        helper(obj);
    }
    dump_info("Done\n");
}

void test_code_cvt_stdio_char_bos_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device>::bos case 3...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        
        char32_t c = 0;
        VERIFY((obj.get(&c, 1) == 1) && (c == U'李'));
        VERIFY((obj.get(&c, 1) == 1) && (c == U'd'));
        VERIFY((obj.get(&c, 1) == 1) && (c == U'伟'));

        obj.main_cont_beg();
    };
    
    std::string info;
    info += '\x4e'; info += '\x67'; info += '\x00'; info += '\x00';
    info += 'd'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '\x1f'; info += '\x4f'; info += '\x00'; info += '\x00';
    info += 'c'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'p'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'p'; info += '\x00'; info += '\x00'; info += '\x00';

    using CheckType = code_cvt<rb_root_cvt<std_device<STDIN_FILENO>>, char32_t>;
    {
        iguard g(info);
        CheckType obj(rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
    }
    {
        iguard g(info);
        CheckType tmp(rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj{std::move(tmp)};
        helper(obj);
    }

    dump_info("Done\n");
}

void test_code_cvt_stdio_char_bos_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device>::bos case 4...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
    };

    {
        oguard<true> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        CheckType obj(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
        VERIFY(g.contents() == "");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;
        CheckType obj(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
        VERIFY(g.contents() == "");
    }
    
    {
        oguard<true> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        CheckType tmp(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        VERIFY(g.contents() == "");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;
        CheckType tmp(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        VERIFY(g.contents() == "");
    }
    dump_info("Done\n");
}

void test_code_cvt_stdio_char_bos_5()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device>::bos case 5...");

    std::string info;
    info += '1'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '2'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '3'; info += '\x00'; info += '\x00'; info += '\x00';

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        
        char32_t buf[] = U"123";
        obj.put(buf, 3);

        obj.main_cont_beg();
    };

    {
        oguard<true> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        CheckType obj(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
        VERIFY(g.contents() == info);
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;
        CheckType obj(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
        VERIFY(g.contents() == info);
    }

    {
        oguard<true> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        CheckType tmp(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        VERIFY(g.contents() == info);
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;
        CheckType tmp(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        VERIFY(g.contents() == info);
    }

    dump_info("Done\n");
}

void test_code_cvt_stdio_char_bos_6()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device>::bos case 6...");

    std::string info;
    info += '\x4e'; info += '\x67'; info += '\x00'; info += '\x00';
    info += 'd'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '\x1f'; info += '\x4f'; info += '\x00'; info += '\x00';

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        char32_t buf[] = U"李d伟";
        obj.put(buf, 3);
        obj.main_cont_beg();
    };

    {
        oguard<true> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        CheckType obj(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
        VERIFY(g.contents() == info);
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;
        CheckType obj(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
        VERIFY(g.contents() == info);
    }
    
    {
        oguard<true> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        CheckType tmp(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        VERIFY(g.contents() == info);
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;
        CheckType tmp(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        VERIFY(g.contents() == info);
    }

    dump_info("Done\n");
}

void test_code_cvt_stdio_char_get_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device>::get case 1...");

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

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();

        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
        std::vector<char32_t> out_buf; out_buf.resize(4102);
        size_t total_count = 0;
        char32_t* cur_pos = out_buf.data();
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
    
        VERIFY(cur_pos - out_buf.data() == 4102 / 7 * 3);
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            VERIFY(*it++ == U'李');
            VERIFY(*it++ == U'伟');
            VERIFY(*it++ == (i / 3) % 127 + 1);
        }
    };

    using CheckType = code_cvt<rb_root_cvt<std_device<STDIN_FILENO>>, char32_t>;
    {
        iguard g(e_lit);
        CheckType obj{rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8"};
        helper(obj);
    }
    {
        iguard g(e_lit);
        CheckType tmp{rb_root_cvt{std_device<STDIN_FILENO>{}}, "zh_CN.UTF-8"};
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }

    dump_info("Done\n");
}

void test_code_cvt_stdio_char_put_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device>::put case 1...");

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
    std::u32string i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(U'李');
        i_lit.push_back(U'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    auto helper = [&i_lit](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();

        size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        size_t total_count = 0;
        char32_t* cur_pos = i_lit.data();
        int buffer_id = 0;
        while (total_count < 4102 / 7 * 3)
        {
            size_t dest_size = std::min<size_t>(4102 / 7 * 3 - total_count, buffer_size[buffer_id++]);
            obj.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;
            total_count += dest_size;
        }
        obj.flush();
    };

    {
        oguard<true> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        CheckType obj(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;
        CheckType obj(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }

    {
        oguard<true> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        CheckType tmp(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;
        CheckType tmp(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }

    dump_info("Done\n");
}

void test_code_cvt_stdio_char_put_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device>::put case 2...");

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
    std::u32string i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(U'李');
        i_lit.push_back(U'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    auto helper = [&i_lit](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();

        obj.put(i_lit.data(), i_lit.size());
        obj.flush();
    };

    {
        oguard<true> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        CheckType obj(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }

    {
        oguard<false> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;
        CheckType obj(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }

    {
        oguard<true> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDOUT_FILENO>>, char32_t>;
        CheckType tmp(rb_root_cvt{std_device<STDOUT_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }

    {
        oguard<false> g;
        using CheckType = code_cvt<rb_root_cvt<std_device<STDERR_FILENO>>, char32_t>;
        CheckType tmp(rb_root_cvt{std_device<STDERR_FILENO>{}}, "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        VERIFY(g.contents() == e_lit);
    }

    dump_info("Done\n");
}

// Covers code_cvt_stdio.h lines 191-212: adjust() with a code_cvt_switch policy.
void test_code_cvt_stdio_adjust_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt_stdio::adjust with code_cvt_switch...");

    using CvtType = code_cvt_stdio<rb_root_cvt<mem_device<char>>>;

    // Empty buffer → deof() true → bos() returns output
    CvtType obj{rb_root_cvt{mem_device(std::string{})}, "C"};

    obj.adjust(code_cvt_switch{"zh_CN.UTF-8"});  // covers lines 191-212

    code_cvt_access status;
    obj.retrieve(status);
    VERIFY(status.code == "zh_CN.UTF-8");

    dump_info("Done\n");
}

// Covers code_cvt_stdio.h line 194: adjust() throws when m_state is non-initial.
void test_code_cvt_stdio_adjust_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt_stdio::adjust throws with non-initial state...");

    // 0xE6 is first byte of a 3-byte UTF-8 sequence; mbrtowc returns -2 (incomplete)
    std::string partial;
    partial += '\xE6';

    using CvtType = code_cvt_stdio<rb_root_cvt<mem_device<char>>>;
    CvtType obj{rb_root_cvt{mem_device(partial)}, "zh_CN.UTF-8"};

    VERIFY(obj.bos() == io_status::input);
    obj.main_cont_beg();

    wchar_t buf[4];
    obj.get(buf, 4);  // reads 0xE6, m_state becomes non-initial

    try {
        obj.adjust(code_cvt_switch{"C"});
        throw std::runtime_error("test_code_cvt_stdio_adjust_2: expected throw");
    } catch (const cvt_error&) {}  // covers line 194

    dump_info("Done\n");
}

// Covers code_cvt_stdio.h lines 238-242: retrieve() with code_cvt_access.
void test_code_cvt_stdio_retrieve_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt_stdio::retrieve with code_cvt_access...");

    using CvtType = code_cvt_stdio<rb_root_cvt<mem_device<char>>>;
    CvtType obj{rb_root_cvt{mem_device(std::string{})}, "zh_CN.UTF-8"};

    code_cvt_access status;
    obj.retrieve(status);  // covers lines 238-242

    VERIFY(status.code == "zh_CN.UTF-8");

    dump_info("Done\n");
}

// Covers code_cvt_stdio.h line 243: retrieve() with unknown type delegates to base.
void test_code_cvt_stdio_retrieve_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt_stdio::retrieve with unknown status type...");

    using CvtType = code_cvt_stdio<rb_root_cvt<mem_device<char>>>;
    CvtType obj{rb_root_cvt{mem_device(std::string{})}, "C"};

    cvt_status s;  // not code_cvt_access → delegates to base (line 243)
    obj.retrieve(s);

    dump_info("Done\n");
}

// Covers code_cvt_stdio_creator::create().
void test_code_cvt_stdio_creator_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt_stdio_creator::create...");

    static_assert(cvt_creator<code_cvt_stdio_creator>);

    code_cvt_stdio_creator creator{"zh_CN.UTF-8"};
    auto obj = creator.create(rb_root_cvt{mem_device(std::string{})});

    code_cvt_access status;
    obj.retrieve(status);
    VERIFY(status.code == "zh_CN.UTF-8");

    dump_info("Done\n");
}
