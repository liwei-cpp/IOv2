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
        using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
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
        using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
    }
    
    {
        using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, wchar_t>;
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
        VERIFY(obj.bos() == io_status::input);
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
            VERIFY(obj2.tell() == total_count);
            if (s == 0) break;
            obj = obj2;
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;

    CheckType obj1{rb_root_cvt{mem_device(e_lit)}, "zh_CN.UTF-8"};
    helper(obj1);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(e_lit)}, "zh_CN.UTF-8"}};
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
            VERIFY(obj2.tell() == total_count);
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1{rb_root_cvt{mem_device(e_lit)}, "zh_CN.UTF-8"};
    helper(obj1);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(e_lit)}, "zh_CN.UTF-8"}};
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
        VERIFY(obj.bos() == io_status::output);
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
            VERIFY(obj2.tell() == total_count);
            obj = obj2;
        }

        auto [dev, err] = obj.detach();
        VERIFY(dev.str() == e_lit);
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"};
    helper(obj1);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
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
            VERIFY(obj2.tell() == total_count);
            obj = std::move(obj2);
        }

        auto [dev, err] = obj.detach();
        VERIFY(dev.str() == e_lit);
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj1);
    
    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_bos_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::bos case 1...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        const auto& dev = obj.device();
        VERIFY(dev.dtell() == 0);
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj1);
    
    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_bos_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::bos case 2...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        
        char32_t c = 0;
        VERIFY((obj.get(&c, 1) == 1) && (c == U'1'));
        VERIFY((obj.get(&c, 1) == 1) && (c == U'2'));
        VERIFY((obj.get(&c, 1) == 1) && (c == U'3'));

        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        const auto& dev = obj.device();
        VERIFY(dev.dtell() == 12);
    };
    
    using CheckType = code_cvt<no_rb_root_cvt<mem_device<char>>, char32_t>;
    std::string info;
    info += '1'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '2'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '3'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '4'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '5'; info += '\x00'; info += '\x00'; info += '\x00';
    CheckType obj(no_rb_root_cvt{mem_device(info)}, "zh_CN.UTF-8");
    helper(obj);
    
    runtime_cvt obj2{CheckType{no_rb_root_cvt{mem_device(info)}, "zh_CN.UTF-8"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_bos_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::bos case 3...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        
        char32_t c = 0;
        VERIFY((obj.get(&c, 1) == 1) && (c == U'李'));
        VERIFY((obj.get(&c, 1) == 1) && (c == U'd'));
        VERIFY((obj.get(&c, 1) == 1) && (c == U'伟'));

        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        auto [dev, err] = obj.detach();
        VERIFY(dev.dtell() == 12);
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    std::string info;
    info += '\x4e'; info += '\x67'; info += '\x00'; info += '\x00';
    info += 'd'; info += '\x00'; info += '\x00'; info += '\x00';
    info += '\x1f'; info += '\x4f'; info += '\x00'; info += '\x00';
    info += 'c'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'p'; info += '\x00'; info += '\x00'; info += '\x00';
    info += 'p'; info += '\x00'; info += '\x00'; info += '\x00';
    CheckType obj(rb_root_cvt{mem_device(info)}, "zh_CN.UTF-8");
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(info)}, "zh_CN.UTF-8"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_bos_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::bos case 4...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        char32_t buf[] = U"123";
        obj.put(buf, 3);
        obj.main_cont_beg();

        VERIFY(obj.tell() == 0);

        const auto& dev = obj.device();
        VERIFY(dev.dtell() == 12);
        
        std::string info;
        info += '1'; info += '\x00'; info += '\x00'; info += '\x00';
        info += '2'; info += '\x00'; info += '\x00'; info += '\x00';
        info += '3'; info += '\x00'; info += '\x00'; info += '\x00';
        VERIFY(dev.str() == info);
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_bos_5()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::bos case 5...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);

        char32_t buf[] = U"李d伟";
        obj.put(buf, 3);
        obj.main_cont_beg();

        VERIFY(obj.tell() == 0);

        const auto& dev = obj.device();
        VERIFY(dev.dtell() == 12);
        std::string info;
        info += '\x4e'; info += '\x67'; info += '\x00'; info += '\x00';
        info += 'd'; info += '\x00'; info += '\x00'; info += '\x00';
        info += '\x1f'; info += '\x4f'; info += '\x00'; info += '\x00';
        VERIFY(dev.str() == info);
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
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
            VERIFY(obj.tell() == total_count);
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj{rb_root_cvt{mem_device(e_lit)}, "zh_CN.UTF-8"};
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(e_lit)}, "zh_CN.UTF-8"}};
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
            VERIFY(obj.tell() == total_count);
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

    using CheckType = code_cvt<no_rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj{no_rb_root_cvt{mem_device(e_lit)}, "zh_CN.UTF-8"};
    helper(obj);

    runtime_cvt obj2{CheckType{no_rb_root_cvt{mem_device(e_lit)}, "zh_CN.UTF-8"}};
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
            VERIFY(obj.tell() == total_count);
        }

        auto [dev, err] = obj.detach();
        VERIFY(dev.str() == e_lit);
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());

        auto [dev, err] = obj.detach();
        VERIFY(dev.str() == e_lit);        
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
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
            VERIFY(obj.tell() == total_count);
        }

        auto [detach_dev, detach_err] = obj.detach();
        VERIFY(detach_dev.str() == e_lit);
    };

    using CheckType = code_cvt<no_rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj(no_rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj);

    runtime_cvt obj2{CheckType{no_rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());
        auto [detach_dev, detach_err] = obj.detach();
        VERIFY(detach_dev.str() == e_lit);
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"}};
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();

        obj.put(i_lit.data(), i_lit.size());
        obj.flush();
        VERIFY(dev.str().size() == e_lit.size());
        VERIFY(obj.tell() == i_lit.size());

        VERIFY(dev.str() == e_lit);
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj1);
    
    runtime_cvt obj2(CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"});
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        size_t total_count = 0;
        char32_t* cur_pos = i_lit.data();
        int buffer_id = 0;
        while (total_count < 4102 / 7 * 3)
        {
            size_t dest_size = std::min<size_t>(4102 / 7 * 3 - total_count, buffer_size[buffer_id++]);
            size_t ori_len = dev.str().size();
            obj.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;
            
            VERIFY(obj.tell() == total_count + dest_size);
            obj.flush();
            total_count += dest_size;
            VERIFY(dev.str().size() != ori_len);
            VERIFY(obj.tell() == total_count);
        }
        obj.flush();
        VERIFY(dev.str() == e_lit);
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj);

    runtime_cvt obj2(CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"});
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();

        obj.put(i_lit.data(), i_lit.size());
        obj.flush();
        VERIFY(dev.str().size() == e_lit.size());
        VERIFY(obj.tell() == i_lit.size());
    
        VERIFY(dev.str() == e_lit);
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2(CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_seek_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::seek case 1...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();

        VERIFY(obj.tell() == 0);
        FAIL_SEEK(obj, 100);
        VERIFY(obj.tell() == 0);
        FAIL_SEEK(obj, 1);
        VERIFY(obj.tell() == 0);
        obj.seek(0);
        VERIFY(obj.tell() == 0);

        std::u32string str; str.resize(6);
        VERIFY(obj.get(str.data(), 6) == 6);
        VERIFY(str == U"123456");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("123456")}, "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2(CheckType{rb_root_cvt{mem_device("123456")}, "zh_CN.UTF-8"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_seek_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::seek case 2...");
    
    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();

        VERIFY(obj.tell() == 0);
        FAIL_SEEK(obj, 100);
        VERIFY(obj.tell() == 0);
        obj.seek(1);
        VERIFY(obj.tell() == 1);

        std::u32string str; str.resize(6);
        VERIFY(obj.get(str.data(), 6) == 5);
        str.resize(5);
        VERIFY(str == U"23456");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("123456")}, "C");
    helper(obj1);

    runtime_cvt obj2(CheckType{rb_root_cvt{mem_device("123456")}, "C"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_seek_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::seek case 3...");
    
    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();

        VERIFY(obj.tell() == 0);
        char32_t ch = U'李';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 1);
        ch = U'x';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 2);
        ch = U'伟';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 3);

        FAIL_SEEK(obj, 100);
        VERIFY(obj.tell() == 3);
        FAIL_SEEK(obj, 1);
        VERIFY(obj.tell() == 3);
        FAIL_SEEK(obj, 0);
        VERIFY(obj.tell() == 3);

        char32_t c[] = U"xy";
        obj.put(c, 2);

        auto [dev, err] = obj.detach();
        VERIFY(dev.str() == "李x伟xy");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2(CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_seek_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::seek case 4...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();

        VERIFY(obj.tell() == 0);
        char32_t ch = U'a';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 1);
        ch = U'b';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 2);
        ch = U'c';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 3);

        FAIL_SEEK(obj, 100);
        VERIFY(obj.tell() == 3);
        obj.seek(1);
        VERIFY(obj.tell() == 1);
        
        char32_t c[] = U"xy";
        obj.put(c, 2);

        auto [dev, err] = obj.detach();
        VERIFY(dev.str() == "axy");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("")}, "C");
    helper(obj1);
    
    runtime_cvt obj2(CheckType{rb_root_cvt{mem_device("")}, "C"});
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_rseek_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::rseek case 1...");
    
    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
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
        VERIFY(obj.device().str() == "123456");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("123456")}, "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2(CheckType{rb_root_cvt{mem_device("123456")}, "zh_CN.UTF-8"});
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_rseek_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::rseek case 2...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        
        VERIFY(obj.tell() == 0);
        FAIL_RSEEK(obj, 100);
        VERIFY(obj.tell() == 0);
        obj.rseek(4);
        VERIFY(obj.tell() == 2);
    
        std::u32string str; str.resize(6);
        VERIFY(obj.get(str.data(), 6) == 4);
        str.resize(4);
        VERIFY(str == U"3456");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("123456")}, "C");
    helper(obj1);

    runtime_cvt obj2(CheckType{rb_root_cvt{mem_device("123456")}, "C"});
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char_rseek_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::rseek case 3...");
    
    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();

        VERIFY(obj.tell() == 0);
        char32_t ch = U'李';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 1);
        ch = U'x';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 2);
        ch = U'伟';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 3);

        FAIL_RSEEK(obj, 100);
        VERIFY(obj.tell() == 3);
        FAIL_RSEEK(obj, 1);
        VERIFY(obj.tell() == 3);
        FAIL_RSEEK(obj, 1);
        VERIFY(obj.tell() == 3);
        
        char32_t c[] = U"xy";
        obj.put(c, 2);
        obj.flush();

        VERIFY(obj.device().str() == "李x伟xy");
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("")}, "zh_CN.UTF-8");
    helper(obj1);

    runtime_cvt obj2(CheckType{rb_root_cvt{mem_device("")}, "zh_CN.UTF-8"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_rseek_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::rseek case 4...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();

        VERIFY(obj.tell() == 0);
        char32_t ch = U'a';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 1);
        ch = U'b';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 2);
        ch = U'c';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 3);

        FAIL_RSEEK(obj, 100);
        VERIFY(obj.tell() == 3);
        obj.rseek(1);
        VERIFY(obj.tell() == 2);

        char32_t c[] = U"xy";
        obj.put(c, 2);
        obj.flush();

        VERIFY(obj.device().str() == "abxy");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(rb_root_cvt{mem_device("")}, "C");
    helper(obj1);

    runtime_cvt obj2(CheckType{rb_root_cvt{mem_device("")}, "C"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_io_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t> io case 1...");
    
    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        
        char32_t bos_str[] = U"abcdefgh";
        obj.put(bos_str, 8);
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);
        
        char32_t content1[] = U"12345";
        obj.put(content1, 5);
        obj.seek(0);
        
        std::u32string get_content; get_content.resize(3);
        VERIFY(obj.get(get_content.data(), 3) == 3);
        VERIFY(get_content == U"123");
        VERIFY(obj.tell() == 3);
    
        char32_t content2[] = U"78";
        obj.put(content2, 2);
        VERIFY(obj.tell() == 5);
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
        VERIFY(obj.device().str() == info);
    };

    using CheckType = code_cvt<no_rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(no_rb_root_cvt{mem_device("")}, "C");
    helper(obj1);

    runtime_cvt obj2(CheckType{no_rb_root_cvt{mem_device("")}, "C"});
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
        VERIFY(obj.bos() == io_status::input);
        std::u32string bos_str; bos_str.resize(8);
        VERIFY(obj.get(bos_str.data(), 8) == 8);
        VERIFY(bos_str == U"abcdefgh");
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        char32_t content1[] = U"67";
        obj.put(content1, 2);
        VERIFY(obj.tell() == 2);

        std::u32string content_str; content_str.resize(2);
        VERIFY(obj.get(content_str.data(), 2) == 2);
        VERIFY(content_str == U"34");
        VERIFY(obj.tell() == 4);

        obj.seek(0);
        VERIFY(obj.tell() == 0);
        char32_t content2[] = U"QWER";
        obj.put(content2, 4);
        VERIFY(obj.tell() == 4);
        obj.flush();

        content_str.resize(4);
        VERIFY(obj.get(content_str.data(), 4) == 1);
        VERIFY(content_str[0] == U'5');

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
        VERIFY(obj.device().str() == aim_info);
    };

    using CheckType = code_cvt<no_rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj1(no_rb_root_cvt{mem_device(info)}, "C");
    helper(obj1);

    runtime_cvt obj2(CheckType{no_rb_root_cvt{mem_device(info)}, "C"});
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char_io_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t> io case 3...");
    
    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);

        char32_t bos_str[] = U"abcdefgh";
        obj.put(bos_str, 8);
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        char32_t content1[] = U"12345";
        obj.put(content1, 5);
        FAIL_SEEK(obj, 0);

        std::u32string get_content; get_content.resize(3);
        VERIFY(obj.get(get_content.data(), 3) == 0);
        VERIFY(obj.tell() == 5);

        char32_t content2[] = U"78";
        obj.put(content2, 2);
        VERIFY(obj.tell() == 7);
        obj.flush();

        obj.switch_to_get();
        obj.seek(0);
        get_content.resize(3);
        VERIFY(obj.get(get_content.data(), 3) == 3);
        VERIFY(get_content == U"123");

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
        VERIFY(obj.device().str() == info);
    };

    code_cvt_creator<char, char32_t> creator("zh_CN.UTF-8");
    auto obj1 = creator.create(no_rb_root_cvt{mem_device("")});
    helper(obj1);

    auto obj2 = creator.create(rb_root_cvt{mem_device("")});
    helper(obj2);

    runtime_cvt obj3(creator.create(no_rb_root_cvt{mem_device("")}));
    helper(obj3);

    runtime_cvt obj4(creator.create(rb_root_cvt{mem_device("")}));
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
        VERIFY(obj.bos() == io_status::input);
    
        std::u32string bos_str; bos_str.resize(8);
        VERIFY(obj.get(bos_str.data(), 8) == 8);
        VERIFY(bos_str == U"abcdefgh");
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);
        
        std::u32string content_str; content_str.resize(2);
        VERIFY(obj.get(content_str.data(), 2) == 2);
        VERIFY(content_str == U"12");
    
        try
        {
            char32_t content1[] = U"67";
            obj.put(content1, 2);
            dump_info("Unreachable code...");
            exit(-1);
        }
        catch (...)
        {}
        
        VERIFY(obj.tell() == 2);
        
        obj.seek(0);
        VERIFY(obj.tell() == 0);
        
        content_str.resize(10);
        VERIFY(obj.get(content_str.data(), 10) == 5);
        VERIFY(content_str.substr(0, 5) == U"12345");
    
        VERIFY(obj.device().str() == info);
    };

    code_cvt_creator<char, char32_t> creator("zh_CN.UTF-8");
    auto obj1 = creator.create(no_rb_root_cvt{mem_device(info)});
    helper(obj1);

    auto obj2 = creator.create(rb_root_cvt{mem_device(info)});
    helper(obj2);
    
    runtime_cvt obj3(creator.create(no_rb_root_cvt{mem_device(info)}));
    helper(obj3);

    runtime_cvt obj4(creator.create(rb_root_cvt{mem_device(info)}));
    helper(obj4);

    dump_info("Done\n");
}

// Covers lines 267-268 (out_helper: wcrtomb returns -1, init_state + return false)
// and line 926 (code_cvt::put throws encoding error).
// Uses "C" locale (epc=1, ASCII only) and attempts to encode a Chinese character.
void test_code_cvt_mem_char_put_err_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::put encoding error case 1...");

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;

    auto try_put = [](auto& obj, char32_t ch) {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        try {
            obj.put(&ch, 1);
            throw std::runtime_error("test_code_cvt_mem_char_put_err_1: expected throw");
        } catch (const cvt_error&) {}
    };

    // In "C" locale, non-ASCII characters cannot be encoded (wcrtomb returns -1)
    { CheckType o{rb_root_cvt{mem_device("")}, "C"}; try_put(o, U'李'); }
    { CheckType o{rb_root_cvt{mem_device("")}, "C"}; try_put(o, U'伟'); }

    dump_info("Done\n");
}

// Covers lines 339-340, 341, 343-344, 346-347, 350, 354-356 in in_helper:
// null character ('\0') decoded via the special mbrtowc conv==0 path.
void test_code_cvt_mem_char_get_null_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::get null character case 1...");

    // Content: 'a', '\0', 'b'  —  embed null byte using explicit construction
    std::string buf;
    buf += 'a'; buf += '\0'; buf += 'b';

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;

    auto helper = [&](auto& obj) {
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        char32_t out[3] = { 1, 1, 1 };
        VERIFY(obj.get(out, 3) == 3);
        VERIFY(out[0] == U'a' && out[1] == U'\0' && out[2] == U'b');
    };

    CheckType obj1{rb_root_cvt{mem_device(buf)}, "C"};
    helper(obj1);
    runtime_cvt obj1r{CheckType{rb_root_cvt{mem_device(buf)}, "C"}};
    helper(obj1r);

    dump_info("Done\n");
}

// Covers line 328 (in_helper: mbrtowc returns -1 for invalid byte)
// and line 986 (code_cvt::get throws invalid external sequence).
void test_code_cvt_mem_char_get_err_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>, char32_t>::get invalid sequence case 1...");

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;

    // In "C" locale, 0xFF is not a valid single-byte character
    auto run_err = [](const std::string& bad_bytes) {
        CheckType obj{rb_root_cvt{mem_device(bad_bytes)}, "C"};
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        char32_t buf[4];
        try {
            obj.get(buf, 4);
            throw std::runtime_error("test_code_cvt_mem_char_get_err_1: expected throw");
        } catch (const cvt_error&) {}
    };

    std::string inv1; inv1 += '\xff';
    run_err(inv1);
    std::string inv2; inv2 += '\xfe';
    run_err(inv2);

    dump_info("Done\n");
}

namespace {
struct throw_on_put {
    using char_type = char;
    bool should_throw = false;
    void dput(const char_type*, size_t) {
        if (should_throw) throw IOv2::device_error("forced put error");
    }
    void dflush() {}
};
} // namespace

// Covers abs_cvt.h line 1335: early return in flush() when m_io_status != output.
void test_code_cvt_mem_char_flush_noop_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char>>: flush in non-output state is noop...");

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    std::string buf = "hello";

    CheckType obj{rb_root_cvt{mem_device(buf)}, "C"};
    obj.flush();  // neutral state: hits line 1335 early return

    VERIFY(obj.bos() == io_status::input);
    obj.main_cont_beg();
    obj.flush();  // input state: hits line 1335 early return again

    dump_info("Done\n");
}

// Covers abs_cvt.h lines 1344-1347: catch block sets m_is_tainted on flush failure.
void test_code_cvt_mem_char_flush_taint_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<throw_on_put>: flush catch block taints converter...");

    using CheckType = code_cvt<rb_root_cvt<throw_on_put>, char32_t>;
    CheckType obj{rb_root_cvt{throw_on_put{}}, "C"};

    VERIFY(obj.bos() == io_status::output);
    obj.main_cont_beg();

    char32_t ch = U'A';
    obj.put(&ch, 1);

    obj.device().should_throw = true;

    try {
        obj.flush();
        throw std::runtime_error("test_code_cvt_mem_char_flush_taint_1: expected throw");
    } catch (const device_error&) {}  // abs_cvt catch block fires, sets m_is_tainted

    // After taint, further flush should throw cvt_error via assert_not_tainted()
    try {
        obj.flush();
        throw std::runtime_error("test_code_cvt_mem_char_flush_taint_1: expected taint throw");
    } catch (const cvt_error&) {}

    dump_info("Done\n");
}

// Covers abs_cvt.h lines 1558-1561: set_tainted() marks the converter as tainted.
void test_code_cvt_mem_char_set_tainted_1()
{
    using namespace IOv2;
    dump_info("Test abs_cvt::set_tainted() marks converter tainted...");

    using BaseType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    struct taintable : BaseType {
        using BaseType::BaseType;
        void expose_set_tainted() { set_tainted(); }
    };

    std::string empty;
    taintable obj{rb_root_cvt{mem_device(empty)}, "C"};

    VERIFY(obj.bos() == io_status::output);
    obj.main_cont_beg();

    obj.expose_set_tainted();  // covers abs_cvt.h lines 1558-1561

    try {
        obj.flush();
        throw std::runtime_error("test_code_cvt_mem_char_set_tainted_1: expected throw");
    } catch (const cvt_error&) {}

    dump_info("Done\n");
}

// Covers code_cvt.h lines 258-259 (out_helper reversed range),
// 261-262 (out_helper zero-size buffer), 316-317 (in_helper reversed range).
void test_code_cvt_mem_char_kernel_helpers_1()
{
    using namespace IOv2;
    dump_info("Test codecvt_kernel<char,char32_t> invalid range and small buffer...");

    codecvt_kernel<char, char32_t> kernel("C");

    // out_helper: to > to_end → throws (lines 258-259)
    {
        char buf[4];
        char* to = buf + 2;
        char* to_end = buf;
        try {
            kernel.out_helper(U'A', to, to_end);
            throw std::runtime_error("out_helper reversed: expected throw");
        } catch (const cvt_error&) {}
    }

    // out_helper: 0-byte buffer (to_end - to = 0 < m_epc = 1) → false (line 262)
    {
        char buf[1];
        char* to = buf;
        char* to_end = buf;
        bool r = kernel.out_helper(U'A', to, to_end);
        VERIFY(!r);
    }

    // in_helper: from > from_end → throws (lines 316-317)
    {
        const char input[] = "hello";
        const char* from = input + 3;
        const char* from_end = input;
        char32_t out[4];
        char32_t* to = out;
        char32_t* to_end = out + 4;
        try {
            kernel.in_helper(from, from_end, to, to_end);
            throw std::runtime_error("in_helper reversed: expected throw");
        } catch (const cvt_error&) {}
    }

    dump_info("Done\n");
}

// Covers code_cvt.h line 1170: switch_to_put throws when m_state is non-initial.
void test_code_cvt_mem_char_switch_state_err_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt::switch_to_put fails when m_state is non-initial...");

    // 0xE6 is the first byte of a 3-byte UTF-8 sequence; mbrtowc returns -2 (incomplete)
    std::string partial;
    partial += '\xE6';

    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, char32_t>;
    CheckType obj{rb_root_cvt{mem_device(partial)}, "zh_CN.UTF-8"};

    VERIFY(obj.bos() == io_status::input);
    obj.main_cont_beg();

    char32_t buf[4];
    obj.get(buf, 4);  // reads 0xE6, mbrtowc returns -2, m_state becomes non-initial

    try {
        obj.switch_to_put();
        throw std::runtime_error("test_code_cvt_mem_char_switch_state_err_1: expected throw");
    } catch (const cvt_error&) {}  // covers line 1170

    dump_info("Done\n");
}
