#include <cvt/code_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <common/dump_info.h>
#include <common/verify.h>

void test_code_cvt_mem_char_gen_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t> general case 1...");
    
    {
        using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = code_cvt<root_cvt<mem_device<char>, false>, char32_t>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
    }
    
    {
        using CheckType = code_cvt<root_cvt<mem_device<char>, true>, wchar_t>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
    }

    dump_info("Done\n");
}

void test_code_cvt_mem_char_gen_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t> general case 2...");

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
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
        std::vector<char32_t> out_buf; out_buf.resize(4102);
        size_t total_count = 0;
        char32_t* cur_pos = out_buf.data();
        int out_buffer_id = 0;
        while (true)
        {
            auto obj2(obj);
            size_t dest_size = std::min<size_t>(4102 - total_count, out_buffer_size[out_buffer_id++]);
            auto s = obj2.get(cur_pos, dest_size);
            out_buffer_id %= std::size(out_buffer_size);
            cur_pos += s;
            total_count += s;
            if (obj2.tell() != total_count) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
            if (s == 0) break;
            obj = obj2;
        }
    
        if (cur_pos - out_buf.data() != 4102 / 7 * 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            if (*it++ != U'李') throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
            if (*it++ != U'伟') throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
            if (*it++ != (i / 3) % 127 + 1) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
        }
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;

    CheckType obj1{make_root_cvt<true>(mem_device(e_lit)), "zh_CN.UTF-8"};
    helper(obj1);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(e_lit)), "zh_CN.UTF-8"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_gen_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t> general case 3...");

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
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
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
            if (obj2.tell() != total_count) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
            if (s == 0) break;
            obj = std::move(obj2);
        }
    
        if (cur_pos - out_buf.data() != 4102 / 7 * 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            if (*it++ != U'李') throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
            if (*it++ != U'伟') throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
            if (*it++ != (i / 3) % 127 + 1) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
        }
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1{make_root_cvt<true>(mem_device(e_lit)), "zh_CN.UTF-8"};
    helper(obj1);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(e_lit)), "zh_CN.UTF-8"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_gen_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t> general case 4...");

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

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        size_t total_count = 0;
        char32_t* cur_pos = i_lit.data();
        int buffer_id = 0;
        while (total_count < 4102 / 7 * 3)
        {
            auto obj2(obj);
            size_t dest_size = std::min<size_t>(4102 / 7 * 3 - total_count, buffer_size[buffer_id++]);
            obj2.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;
            total_count += dest_size;
            if (obj2.tell() != total_count) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
            obj = obj2;
        }

        auto dev = obj.detach();
        if (dev.str() != e_lit) throw std::runtime_error("code_cvt<memory<char>, char32_t>::put response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"};
    helper(obj1);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_gen_5()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t> general case 5...");

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

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
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
            if (obj2.tell() != total_count) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
            obj = std::move(obj2);
        }

        auto dev = obj.detach();
        if (dev.str() != e_lit) throw std::runtime_error("code_cvt<memory<char>, char32_t>::put response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8");
    helper(obj1);
    
    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_bos_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::bos case 1...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::bos fail");

        const auto& dev = obj.device();
        if (dev.dtell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::bos fail");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8");
    helper(obj1);
    
    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_bos_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::bos case 2...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        
        char32_t c = 0;
        if ((obj.get(&c, 1) != 1) || (c != U'1')) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != U'2')) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != U'3')) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");

        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::bos fail");

        const auto& dev = obj.device();
        if (dev.dtell() != 12) throw std::runtime_error("code_cvt<memory<char>, char32_t>::bos fail");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char>, false>, char32_t>;
    std::string info;
    info += '1'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '2'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '3'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '4'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '5'; info += '\x00'; info += '\x00'; info += '\x00';
    CheckType obj(make_root_cvt<false>(mem_device(info)), "zh_CN.UTF-8");
    helper(obj);
    
    runtime_cvt obj2{CheckType{make_root_cvt<false>(mem_device(info)), "zh_CN.UTF-8"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_bos_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::bos case 3...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        
        char32_t c = 0;
        if ((obj.get(&c, 1) != 1) || (c != U'李')) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != U'd')) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != U'伟')) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");

        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::bos fail");

        const auto& dev = obj.detach();
        if (dev.dtell() != 12) throw std::runtime_error("code_cvt<memory<char>, char32_t>::bos fail");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    std::string info;
    info += '\x4e'; info += '\x67'; info += '\x00'; info += '\x00';
    info += 'd'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '\x1f'; info += '\x4f'; info += '\x00'; info += '\x00';
    info += 'c'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'p'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'p'; info += '\x00'; info += '\x00'; info += '\x00';
    CheckType obj(make_root_cvt<true>(mem_device(info)), "zh_CN.UTF-8");
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(info)), "zh_CN.UTF-8"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_bos_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::bos case 4...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        char32_t buf[] = U"123";
        obj.put(buf, 3);
        obj.main_cont_beg();

        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::bos fail");

        const auto& dev = obj.device();
        if (dev.dtell() != 12) throw std::runtime_error("code_cvt<memory<char>, char32_t>::bos fail");
        
        std::string info;
        info += '1'; info += '\x00'; info += '\x00'; info += '\x00';
        info += '2'; info += '\x00'; info += '\x00'; info += '\x00';
        info += '3'; info += '\x00'; info += '\x00'; info += '\x00';
        if (dev.str() != info) throw std::runtime_error("code_cvt<memory<char>, char32_t>::bos fail");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8");
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"}};
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_bos_5()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::bos case 5...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");

        char32_t buf[] = U"李d伟";
        obj.put(buf, 3);
        obj.main_cont_beg();

        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::bos fail");

        const auto& dev = obj.device();
        if (dev.dtell() != 12) throw std::runtime_error("code_cvt<memory<char>, char32_t>::bos fail");
        std::string info;
        info += '\x4e'; info += '\x67'; info += '\x00'; info += '\x00';
        info += 'd'; info += '\x00'; info += '\x00'; info += '\x00';
        info += '\x1f'; info += '\x4f'; info += '\x00'; info += '\x00';
        if (dev.str() != info) throw std::runtime_error("code_cvt<memory<char>, char32_t>::bos fail");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8");
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"}};
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_get_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::get case 1...");

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
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
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
            if (obj.tell() != total_count) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
            if (s == 0) break;
        }
    
        if (cur_pos - out_buf.data() != 4102 / 7 * 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            if (*it++ != U'李') throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
            if (*it++ != U'伟') throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
            if (*it++ != (i / 3) % 127 + 1) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
        }
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj{make_root_cvt<true>(mem_device(e_lit)), "zh_CN.UTF-8"};
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(e_lit)), "zh_CN.UTF-8"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_get_nra_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::get_nra case 1...");

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
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
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
            if (obj.tell() != total_count) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
            if (s == 0) break;
        }
    
        if (cur_pos - out_buf.data() != 4102 / 7 * 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            if (*it++ != U'李') throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
            if (*it++ != U'伟') throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
            if (*it++ != (i / 3) % 127 + 1) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
        }
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, false>, char32_t>;
    CheckType obj{make_root_cvt<false>(mem_device(e_lit)), "zh_CN.UTF-8"};
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<false>(mem_device(e_lit)), "zh_CN.UTF-8"}};
    helper(obj2);
    
    dump_info("Done\n");
}

void test_code_cvt_mem_char_put_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::put case 1...");

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

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
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
            if (obj.tell() != total_count) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        }

        auto dev = obj.detach();
        if (dev.str() != e_lit) throw std::runtime_error("code_cvt<memory<char>, char32_t>::put response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8");
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"}};
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_put_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::put case 2...");

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

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());

        auto dev = obj.detach();
        if (dev.str() != e_lit) throw std::runtime_error("code_cvt<memory<char>, char32_t>::put response incorrect");        
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"}};
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_put_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::put case 3...");

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

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
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
            if (obj.tell() != total_count) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        }

        if (obj.detach().str() != e_lit) throw std::runtime_error("code_cvt<memory<char>, char32_t>::put response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, false>, char32_t>;
    CheckType obj(make_root_cvt<false>(mem_device("")), "zh_CN.UTF-8");
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<false>(mem_device("")), "zh_CN.UTF-8"}};
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_put_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::put case 4...");

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
    
    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());
        if (obj.detach().str() != e_lit) throw std::runtime_error("code_cvt<memory<char>, char32_t>::put response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"}};
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_flush_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::flush case 1...");

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
    
    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        const auto& dev = obj.device();
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        obj.put(i_lit.data(), i_lit.size());
        if (dev.str().size() >= e_lit.size()) throw std::runtime_error("code_cvt<memory<char>, char32_t>::flush response incorrect");
        obj.flush();
        if (dev.str().size() != e_lit.size()) throw std::runtime_error("code_cvt<memory<char>, char32_t>::flush response incorrect");
        if (obj.tell() != i_lit.size()) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");

        if (dev.str() != e_lit) throw std::runtime_error("code_cvt<memory<char>, char32_t>::put response incorrect");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8");
    helper(obj1);
    
    runtime_cvt obj2(CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_flush_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::flush case 2...");

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

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        const auto& dev = obj.device();
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
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
            
            size_t ori_len = dev.str().size();
            if (obj.tell() != total_count + dest_size) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
            obj.flush();
            total_count += dest_size;
            if (dev.str().size() == ori_len) throw std::runtime_error("code_cvt<memory<char>, char32_t>::flush response incorrect");
            if (obj.tell() != total_count) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        }
        obj.flush();
        if (dev.str() != e_lit) throw std::runtime_error("code_cvt<memory<char>, char32_t>::put response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8");
    helper(obj);

    runtime_cvt obj2(CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_flush_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::flush case 3...");

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

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        const auto& dev = obj.device();
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        obj.put(i_lit.data(), i_lit.size());
        obj.flush();
        if (dev.str().size() != e_lit.size()) throw std::runtime_error("code_cvt<memory<char>, char32_t>::flush response incorrect");
        if (obj.tell() != i_lit.size()) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
    
        if (dev.str() != e_lit) throw std::runtime_error("code_cvt<memory<char>, char32_t>::put response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2(CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_seek_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::seek case 1...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        FAIL_SEEK(obj, 100);
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        FAIL_SEEK(obj, 1);
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        obj.seek(0);
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");

        std::u32string str; str.resize(6);
        if (obj.get(str.data(), 6) != 6) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
        if (str != U"123456") throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("123456")), "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2(CheckType{make_root_cvt<true>(mem_device("123456")), "zh_CN.UTF-8"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_seek_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::seek case 2...");
    
    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        FAIL_SEEK(obj, 100);
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        obj.seek(1);
        if (obj.tell() != 1) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");

        std::u32string str; str.resize(6);
        if (obj.get(str.data(), 6) != 5) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
        str.resize(5);
        if (str != U"23456") throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("123456")), "C");
    helper(obj1);

    runtime_cvt obj2(CheckType{make_root_cvt<true>(mem_device("123456")), "C"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_seek_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::seek case 3...");
    
    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        char32_t ch = U'李';
        obj.put(&ch, 1);
        if (obj.tell() != 1) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        ch = U'x';
        obj.put(&ch, 1);
        if (obj.tell() != 2) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        ch = U'伟';
        obj.put(&ch, 1);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");

        FAIL_SEEK(obj, 100);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        FAIL_SEEK(obj, 1);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        FAIL_SEEK(obj, 0);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");

        char32_t c[] = U"xy";
        obj.put(c, 2);

        auto dev = obj.detach();
        if (dev.str() != "李x伟xy") throw std::runtime_error("code_cvt<memory<char>, char32_t> response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2(CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_seek_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::seek case 4...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        char32_t ch = U'a';
        obj.put(&ch, 1);
        if (obj.tell() != 1) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        ch = U'b';
        obj.put(&ch, 1);
        if (obj.tell() != 2) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        ch = U'c';
        obj.put(&ch, 1);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");

        FAIL_SEEK(obj, 100);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        obj.seek(1);
        if (obj.tell() != 1) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        
        char32_t c[] = U"xy";
        obj.put(c, 2);

        auto dev = obj.detach();
        if (dev.str() != "axy") throw std::runtime_error("code_cvt<memory<char>, char32_t> response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("")), "C");
    helper(obj1);
    
    runtime_cvt obj2(CheckType{make_root_cvt<true>(mem_device("")), "C"});
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_rseek_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::rseek case 1...");
    
    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        VERIFY(obj.tell() == 0);
        FAIL_RSEEK(obj, 100);
        VERIFY(obj.tell() == 0);
        FAIL_RSEEK(obj, 1);
        VERIFY(obj.tell() == 0);
        FAIL_RSEEK(obj, 0);
        VERIFY(obj.tell() == 0);

        std::u32string str; str.resize(6);
        VERIFY(obj.get(str.data(), 6) == 6);
        if (obj.device().str() != "123456") throw std::runtime_error("code_cvt<memory<char>, char32_t> response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("123456")), "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2(CheckType{make_root_cvt<true>(mem_device("123456")), "zh_CN.UTF-8"});
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_rseek_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::rseek case 2...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        FAIL_RSEEK(obj, 100);
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        obj.rseek(4);
        if (obj.tell() != 2) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
    
        std::u32string str; str.resize(6);
        if (obj.get(str.data(), 6) != 4) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
        str.resize(4);
        if (str != U"3456") throw std::runtime_error("code_cvt<memory<char>, char32_t>::get response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("123456")), "C");
    helper(obj1);

    runtime_cvt obj2(CheckType{make_root_cvt<true>(mem_device("123456")), "C"});
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_rseek_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::rseek case 3...");
    
    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        char32_t ch = U'李';
        obj.put(&ch, 1);
        if (obj.tell() != 1) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        ch = U'x';
        obj.put(&ch, 1);
        if (obj.tell() != 2) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        ch = U'伟';
        obj.put(&ch, 1);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");

        FAIL_RSEEK(obj, 100);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        FAIL_RSEEK(obj, 1);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        FAIL_RSEEK(obj, 1);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        
        char32_t c[] = U"xy";
        obj.put(c, 2);
        obj.flush();

        if (obj.device().str() != "李x伟xy") throw std::runtime_error("code_cvt<memory<char>, char32_t> response incorrect");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2(CheckType{make_root_cvt<true>(mem_device("")), "zh_CN.UTF-8"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_rseek_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::rseek case 4...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        char32_t ch = U'a';
        obj.put(&ch, 1);
        if (obj.tell() != 1) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        ch = U'b';
        obj.put(&ch, 1);
        if (obj.tell() != 2) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        ch = U'c';
        obj.put(&ch, 1);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");

        FAIL_RSEEK(obj, 100);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");
        obj.rseek(1);
        if (obj.tell() != 2) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell response incorrect");

        char32_t c[] = U"xy";
        obj.put(c, 2);
        obj.flush();

        if (obj.device().str() != "abxy") throw std::runtime_error("code_cvt<memory<char>, char32_t> response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, true>, char32_t>;
    CheckType obj1(make_root_cvt<true>(mem_device("")), "C");
    helper(obj1);

    runtime_cvt obj2(CheckType{make_root_cvt<true>(mem_device("")), "C"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_io_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t> io case 1...");
    
    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        
        char32_t bos_str[] = U"abcdefgh";
        obj.put(bos_str, 8);
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");
        
        char32_t content1[] = U"12345";
        obj.put(content1, 5);
        obj.seek(0);
        
        std::u32string get_content; get_content.resize(3);
        if (obj.get(get_content.data(), 3) != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get fail");
        if (get_content != U"123") throw std::runtime_error("code_cvt<memory<char>, char32_t>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");
    
        char32_t content2[] = U"78";
        obj.put(content2, 2);
        if (obj.tell() != 5) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");
        obj.flush();

        std::string info;
        info += 'a'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'b'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'c'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'd'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'e'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'f'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'g'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'h'; info += '\x00'; info += '\x00'; info += '\x00';
        info += "12378";
        if (obj.device().str() != info) throw std::runtime_error("code_cvt<memory<char>, char32_t> response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, false>, char32_t>;
    CheckType obj1(make_root_cvt<false>(mem_device("")), "C");
    helper(obj1);

    runtime_cvt obj2(CheckType{make_root_cvt<false>(mem_device("")), "C"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_io_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t> io case 2...");

    std::string info;
    info += 'a'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'b'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'c'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'd'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'e'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'f'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'g'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'h'; info += '\x00'; info += '\x00'; info += '\x00';
    info += "12345";

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        std::u32string bos_str; bos_str.resize(8);
        if (obj.get(bos_str.data(), 8) != 8) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        if (bos_str != U"abcdefgh") throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");

        char32_t content1[] = U"67";
        obj.put(content1, 2);
        if (obj.tell() != 2) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");

        std::u32string content_str; content_str.resize(2);
        if (obj.get(content_str.data(), 2) != 2) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        if (content_str != U"34") throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        if (obj.tell() != 4) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");

        obj.seek(0);
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");
        char32_t content2[] = U"QWER";
        obj.put(content2, 4);
        if (obj.tell() != 4) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");
        obj.flush();

        content_str.resize(4);
        if (obj.get(content_str.data(), 4) != 1) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        if (content_str[0] != U'5') throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");

        std::string aim_info;
        aim_info += 'a'; aim_info += '\x00'; aim_info += '\x00'; aim_info += '\x00';
        aim_info += 'b'; aim_info += '\x00'; aim_info += '\x00'; aim_info += '\x00';
        aim_info += 'c'; aim_info += '\x00'; aim_info += '\x00'; aim_info += '\x00';
        aim_info += 'd'; aim_info += '\x00'; aim_info += '\x00'; aim_info += '\x00';
        aim_info += 'e'; aim_info += '\x00'; aim_info += '\x00'; aim_info += '\x00';
        aim_info += 'f'; aim_info += '\x00'; aim_info += '\x00'; aim_info += '\x00';
        aim_info += 'g'; aim_info += '\x00'; aim_info += '\x00'; aim_info += '\x00';
        aim_info += 'h'; aim_info += '\x00'; aim_info += '\x00'; aim_info += '\x00';
        aim_info += "QWER5";
        if (obj.device().str() != aim_info) throw std::runtime_error("code_cvt<memory<char>, char32_t> response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char>, false>, char32_t>;
    CheckType obj1(make_root_cvt<false>(mem_device(info)), "C");
    helper(obj1);

    runtime_cvt obj2(CheckType{make_root_cvt<false>(mem_device(info)), "C"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_io_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t> io case 3...");
    
    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");

        char32_t bos_str[] = U"abcdefgh";
        obj.put(bos_str, 8);
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");

        char32_t content1[] = U"12345";
        obj.put(content1, 5);
        FAIL_SEEK(obj, 0);

        std::u32string get_content; get_content.resize(3);
        if (obj.get(get_content.data(), 3) != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        if (obj.tell() != 5) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");

        char32_t content2[] = U"78";
        obj.put(content2, 2);
        if (obj.tell() != 7) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");
        obj.flush();

        obj.switch_to_get();
        obj.seek(0);
        get_content.resize(3);
        if (obj.get(get_content.data(), 3) != 3) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        if (get_content != U"123") throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");

        try
        {
            obj.switch_to_put();
            dump_info("un-reachable logic");
            std::abort();
        }
        catch(...) {}

        std::string info;
        info += 'a'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'b'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'c'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'd'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'e'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'f'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'g'; info += '\x00'; info += '\x00'; info += '\x00';
        info += 'h'; info += '\x00'; info += '\x00'; info += '\x00';
        info += "1234578";
        if (obj.device().str() != info) throw std::runtime_error("code_cvt<memory<char>, char32_t> response incorrect");
    };

    code_cvt_creator<char, char32_t> creator("zh_CN.UTF-8");
    auto obj1 = creator.create(make_root_cvt<false>(mem_device("")));
    helper(obj1);

    auto obj2 = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj2);

    runtime_cvt obj3(creator.create(make_root_cvt<false>(mem_device(""))));
    helper(obj3);

    runtime_cvt obj4(creator.create(make_root_cvt<true>(mem_device(""))));
    helper(obj4);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_io_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t> io case 4...");
    
    std::string info;
    info += 'a'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'b'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'c'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'd'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'e'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'f'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'g'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'h'; info += '\x00'; info += '\x00'; info += '\x00';
    info += "12345";

    auto helper = [&info](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
    
        std::u32string bos_str; bos_str.resize(8);
        if (obj.get(bos_str.data(), 8) != 8) throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        if (bos_str != U"abcdefgh") throw std::runtime_error("code_cvt<memory<char>, char32_t>::get_nra fail");
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");
        
        std::u32string content_str; content_str.resize(2);
        if (obj.get(content_str.data(), 2) != 2) throw std::runtime_error("code_cvt<memory<char>, char32_t>:get fail");
        if (content_str != U"12") throw std::runtime_error("code_cvt<memory<char>, char32_t>:get fail");
    
        try
        {
            char32_t content1[] = U"67";
            obj.put(content1, 2);
            dump_info("Unreachable code...");
            exit(-1);
        }
        catch (...)
        {}
        
        if (obj.tell() != 2) throw std::runtime_error("code_cvt<memory<char>, char32_t>::tell fail");
        
        obj.seek(0);
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<...char...>::tell fail");
        
        content_str.resize(10);
        if (obj.get(content_str.data(), 10) != 5) throw std::runtime_error("code_cvt<...char...>:get fail");
        if (content_str.substr(0, 5) != U"12345") throw std::runtime_error("code_cvt<...char...>:get fail");
    
        if (obj.device().str() != info) throw std::runtime_error("code_cvt<...char...> response incorrect");
    };

    code_cvt_creator<char, char32_t> creator("zh_CN.UTF-8");
    auto obj1 = creator.create(make_root_cvt<false>(mem_device(info)));
    helper(obj1);

    auto obj2 = creator.create(make_root_cvt<true>(mem_device(info)));
    helper(obj2);
    
    runtime_cvt obj3(creator.create(make_root_cvt<false>(mem_device(info))));
    helper(obj3);

    runtime_cvt obj4(creator.create(make_root_cvt<true>(mem_device(info))));
    helper(obj4);

    dump_info("Done\n");
}
