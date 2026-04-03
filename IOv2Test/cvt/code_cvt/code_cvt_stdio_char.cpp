#include <cvt/code_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/std_device.h>

#include <common/dump_info.h>
#include <common/stdio_guard.h>
void test_code_cvt_stdio_char_gen_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device> general case 1...");
    
    {
        using CheckType = code_cvt<root_cvt<std_device<STDIN_FILENO>, true>, char32_t>;
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
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
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
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, wchar_t>;
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
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<std_device>::bos response incorrect");
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
    
        if (cur_pos - out_buf.data() != 4102 / 7 * 3) throw std::runtime_error("code_cvt<std_device>::get response incorrect");
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            if (*it++ != U'李') throw std::runtime_error("code_cvt<std_device>::get response incorrect");
            if (*it++ != U'伟') throw std::runtime_error("code_cvt<std_device>::get response incorrect");
            if (*it++ != (i / 3) % 127 + 1) throw std::runtime_error("code_cvt<std_device>::get response incorrect");
        }
    };

    using CheckType = code_cvt<root_cvt<std_device<STDIN_FILENO>, true>, char32_t>;
    {
        iguard g(e_lit);
        CheckType obj{make_root_cvt<true>(std_device<STDIN_FILENO>{}), "zh_CN.UTF-8"};
        helper(obj);
    }
    {
        iguard g(e_lit);
        CheckType tmp{make_root_cvt<true>(std_device<STDIN_FILENO>{}), "zh_CN.UTF-8"};
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
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<std_device>::bos response incorrect");
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
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
        CheckType obj(make_root_cvt<true>(std_device<STDOUT_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("code_cvt<std_device>::put_bos fail");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, char32_t>;
        CheckType obj(make_root_cvt<true>(std_device<STDERR_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("code_cvt<std_device>::put_bos fail");
    }

    {
        oguard<true> g;
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
        CheckType tmp(make_root_cvt<true>(std_device<STDOUT_FILENO>{}), "zh_CN.UTF-8");
        runtime_cvt obj{std::move(tmp)};
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("code_cvt<std_device>::put_bos fail");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, char32_t>;
        CheckType tmp(make_root_cvt<true>(std_device<STDERR_FILENO>{}), "zh_CN.UTF-8");
        runtime_cvt obj{std::move(tmp)};
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("code_cvt<std_device>::put_bos fail");
    }

    dump_info("Done\n");
}

void test_code_cvt_stdio_char_bos_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<std_device>::bos case 1...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<std_device>::bos response incorrect");
        obj.main_cont_beg();
    };

    using CheckType = code_cvt<root_cvt<std_device<STDIN_FILENO>, true>, char32_t>;
    {
        iguard g("12345");
        CheckType obj(make_root_cvt<true>(std_device<STDIN_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
    }
    {
        iguard g("12345");
        CheckType tmp(make_root_cvt<true>(std_device<STDIN_FILENO>{}), "zh_CN.UTF-8");
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
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<std_device>::bos response incorrect");
        
        char32_t c = 0;
        if ((obj.get(&c, 1) != 1) || (c != '1')) throw std::runtime_error("code_cvt<std_device>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != '2')) throw std::runtime_error("code_cvt<std_device>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != '3')) throw std::runtime_error("code_cvt<std_device>::get_nra fail");

        obj.main_cont_beg();
    };

    std::string info;
    info += '1'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '2'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '3'; info += '\x00'; info += '\x00'; info += '\x00';
    
    using CheckType = code_cvt<root_cvt<std_device<STDIN_FILENO>, true>, char32_t>;
    {
        iguard g(info);
        CheckType obj(make_root_cvt<true>(std_device<STDIN_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
    }
    {
        iguard g(info);
        CheckType tmp(make_root_cvt<true>(std_device<STDIN_FILENO>{}), "zh_CN.UTF-8");
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
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<std_device>::bos response incorrect");
        
        char32_t c = 0;
        if ((obj.get(&c, 1) != 1) || (c != U'李')) throw std::runtime_error("code_cvt<std_device>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != U'd')) throw std::runtime_error("code_cvt<std_device>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != U'伟')) throw std::runtime_error("code_cvt<std_device>::get_nra fail");

        obj.main_cont_beg();
    };
    
    std::string info;
    info += '\x4e'; info += '\x67'; info += '\x00'; info += '\x00';
    info += 'd'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '\x1f'; info += '\x4f'; info += '\x00'; info += '\x00';
    info += 'c'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'p'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'p'; info += '\x00'; info += '\x00'; info += '\x00';

    using CheckType = code_cvt<root_cvt<std_device<STDIN_FILENO>, true>, char32_t>;
    {
        iguard g(info);
        CheckType obj(make_root_cvt<true>(std_device<STDIN_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
    }
    {
        iguard g(info);
        CheckType tmp(make_root_cvt<true>(std_device<STDIN_FILENO>{}), "zh_CN.UTF-8");
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
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<std_device>::bos response incorrect");
        obj.main_cont_beg();
    };

    {
        oguard<true> g;
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
        CheckType obj(make_root_cvt<true>(std_device<STDOUT_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
        if (g.contents() != "") throw std::runtime_error("code_cvt<std_device>::bos fail");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, char32_t>;
        CheckType obj(make_root_cvt<true>(std_device<STDERR_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
        if (g.contents() != "") throw std::runtime_error("code_cvt<std_device>::bos fail");
    }
    
    {
        oguard<true> g;
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
        CheckType tmp(make_root_cvt<true>(std_device<STDOUT_FILENO>{}), "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        if (g.contents() != "") throw std::runtime_error("code_cvt<std_device>::bos fail");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, char32_t>;
        CheckType tmp(make_root_cvt<true>(std_device<STDERR_FILENO>{}), "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        if (g.contents() != "") throw std::runtime_error("code_cvt<std_device>::bos fail");
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
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<std_device>::bos response incorrect");
        
        char32_t buf[] = U"123";
        obj.put(buf, 3);

        obj.main_cont_beg();
    };

    {
        oguard<true> g;
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
        CheckType obj(make_root_cvt<true>(std_device<STDOUT_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
        if (g.contents() != info) throw std::runtime_error("code_cvt<std_device>::bos fail");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, char32_t>;
        CheckType obj(make_root_cvt<true>(std_device<STDERR_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
        if (g.contents() != info) throw std::runtime_error("code_cvt<std_device>::bos fail");
    }

    {
        oguard<true> g;
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
        CheckType tmp(make_root_cvt<true>(std_device<STDOUT_FILENO>{}), "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        if (g.contents() != info) throw std::runtime_error("code_cvt<std_device>::bos fail");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, char32_t>;
        CheckType tmp(make_root_cvt<true>(std_device<STDERR_FILENO>{}), "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        if (g.contents() != info) throw std::runtime_error("code_cvt<std_device>::bos fail");
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
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<std_device>::bos response incorrect");
        char32_t buf[] = U"李d伟";
        obj.put(buf, 3);
        obj.main_cont_beg();
    };

    {
        oguard<true> g;
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
        CheckType obj(make_root_cvt<true>(std_device<STDOUT_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
        if (g.contents() != info) throw std::runtime_error("code_cvt<std_device>::bos fail");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, char32_t>;
        CheckType obj(make_root_cvt<true>(std_device<STDERR_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
        if (g.contents() != info) throw std::runtime_error("code_cvt<std_device>::bos fail");
    }
    
    {
        oguard<true> g;
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
        CheckType tmp(make_root_cvt<true>(std_device<STDOUT_FILENO>{}), "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        if (g.contents() != info) throw std::runtime_error("code_cvt<std_device>::bos fail");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, char32_t>;
        CheckType tmp(make_root_cvt<true>(std_device<STDERR_FILENO>{}), "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        if (g.contents() != info) throw std::runtime_error("code_cvt<std_device>::bos fail");
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
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<std_device>::bos response incorrect");
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
    
        if (cur_pos - out_buf.data() != 4102 / 7 * 3) throw std::runtime_error("code_cvt<std_device>::get response incorrect");
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            if (*it++ != U'李') throw std::runtime_error("code_cvt<std_device>::get response incorrect");
            if (*it++ != U'伟') throw std::runtime_error("code_cvt<std_device>::get response incorrect");
            if (*it++ != (i / 3) % 127 + 1) throw std::runtime_error("code_cvt<std_device>::get response incorrect");
        }
    };

    using CheckType = code_cvt<root_cvt<std_device<STDIN_FILENO>, true>, char32_t>;
    {
        iguard g(e_lit);
        CheckType obj{make_root_cvt<true>(std_device<STDIN_FILENO>{}), "zh_CN.UTF-8"};
        helper(obj);
    }
    {
        iguard g(e_lit);
        CheckType tmp{make_root_cvt<true>(std_device<STDIN_FILENO>{}), "zh_CN.UTF-8"};
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
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<std_device>::bos response incorrect");
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
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
        CheckType obj(make_root_cvt<true>(std_device<STDOUT_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("code_cvt<std_device>::put_bos fail");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, char32_t>;
        CheckType obj(make_root_cvt<true>(std_device<STDERR_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("code_cvt<std_device>::put_bos fail");
    }

    {
        oguard<true> g;
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
        CheckType tmp(make_root_cvt<true>(std_device<STDOUT_FILENO>{}), "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("code_cvt<std_device>::put_bos fail");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, char32_t>;
        CheckType tmp(make_root_cvt<true>(std_device<STDERR_FILENO>{}), "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("code_cvt<std_device>::put_bos fail");
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
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<std_device>::bos response incorrect");
        obj.main_cont_beg();

        obj.put(i_lit.data(), i_lit.size());
        obj.flush();
    };

    {
        oguard<true> g;
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
        CheckType obj(make_root_cvt<true>(std_device<STDOUT_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("code_cvt<std_device>::put_bos fail");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, char32_t>;
        CheckType obj(make_root_cvt<true>(std_device<STDERR_FILENO>{}), "zh_CN.UTF-8");
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("code_cvt<std_device>::put_bos fail");
    }

    {
        oguard<true> g;
        using CheckType = code_cvt<root_cvt<std_device<STDOUT_FILENO>, true>, char32_t>;
        CheckType tmp(make_root_cvt<true>(std_device<STDOUT_FILENO>{}), "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("code_cvt<std_device>::put_bos fail");
    }
    
    {
        oguard<false> g;
        using CheckType = code_cvt<root_cvt<std_device<STDERR_FILENO>, true>, char32_t>;
        CheckType tmp(make_root_cvt<true>(std_device<STDERR_FILENO>{}), "zh_CN.UTF-8");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
        if (g.contents() != e_lit) throw std::runtime_error("code_cvt<std_device>::put_bos fail");
    }

    dump_info("Done\n");
}
