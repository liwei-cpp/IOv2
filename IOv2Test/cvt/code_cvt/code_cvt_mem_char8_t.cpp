#include <cvt/code_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <common/dump_info.h>
#include <common/verify.h>

void test_code_cvt_mem_char8_t_gen_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>> general case 1...");
    
    {
        using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
        static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
        static_assert(std::is_same_v<CheckType::external_type, char8_t>);
        
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(cvt_cpt::support_io_switch<CheckType>);
    }

    {
        using CheckType = code_cvt<root_cvt<mem_device<char8_t>, false>, char32_t>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
        static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
        static_assert(std::is_same_v<CheckType::external_type, char8_t>);
    }

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_gen_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>> general case 2...");

    std::u8string e_lit; e_lit.resize(4102);
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
            if (obj2.tell() != total_count) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
            if (s == 0) break;
            obj = obj2;
        }
    
        if (cur_pos - out_buf.data() != 4102 / 7 * 3) throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            if (*it++ != U'李') throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
            if (*it++ != U'伟') throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
            if (*it++ != (i / 3) % 127 + 1) throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
        }
    };

    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;

    CheckType obj1{make_root_cvt<true>(mem_device(e_lit))};
    helper(obj1);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(e_lit))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_gen_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>> general case 3...");

    std::u8string e_lit; e_lit.resize(4102);
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
            if (obj2.tell() != total_count) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
            if (s == 0) break;
            obj = std::move(obj2);
        }
    
        if (cur_pos - out_buf.data() != 4102 / 7 * 3) throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            if (*it++ != U'李') throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
            if (*it++ != U'伟') throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
            if (*it++ != (i / 3) % 127 + 1) throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
        }
    };

    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj1{make_root_cvt<true>(mem_device(e_lit))};
    helper(obj1);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(e_lit))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_gen_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>> general case 4...");

    std::u8string e_lit; e_lit.resize(4102);
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
            if (obj2.tell() != total_count) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
            obj = obj2;
        }
        obj.flush();

        if (obj.device().str() != e_lit) throw std::runtime_error("code_cvt<memory<char8_t>>::put response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8""))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_gen_5()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>> general case 5...");

    std::u8string e_lit; e_lit.resize(4102);
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
            if (obj2.tell() != total_count) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
            obj = std::move(obj2);
        }
        obj.flush();

        if (obj.device().str() != e_lit) throw std::runtime_error("code_cvt<memory<char8_t>>::put response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8""))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_bos_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::bos case 1...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::bos fail");

        if (obj.device().dtell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::bos fail");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8""))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_bos_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::bos case 2...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        
        char32_t c = 0;
        if ((obj.get(&c, 1) != 1) || (c != U'1')) throw std::runtime_error("code_cvt<memory<char8_t>>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != U'2')) throw std::runtime_error("code_cvt<memory<char8_t>>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != U'3')) throw std::runtime_error("code_cvt<memory<char8_t>>::get_nra fail");

        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::bos fail");

        if (obj.detach().dtell() != 12) throw std::runtime_error("code_cvt<...char...>::bos fail");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    std::u8string info;
    info += u8'1'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'2'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'3'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'4'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'5'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    CheckType obj(make_root_cvt<true>(mem_device(info)));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(info))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_bos_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::bos case 3...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        char32_t c = 0;
        if ((obj.get(&c, 1) != 1) || (c != U'李')) throw std::runtime_error("code_cvt<memory<char8_t>>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != U'd')) throw std::runtime_error("code_cvt<memory<char8_t>>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != U'伟')) throw std::runtime_error("code_cvt<memory<char8_t>>::get_nra fail");
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::bos fail");

        if (obj.detach().dtell() != 12) throw std::runtime_error("code_cvt<memory<char8_t>>::bos fail");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    std::u8string info;
    info += u8'\x4e'; info += u8'\x67'; info += u8'\x00'; info += u8'\x00';
    info += u8'd';    info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'\x1f'; info += u8'\x4f'; info += u8'\x00'; info += u8'\x00';
    info += u8'c';    info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'p';    info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'p';    info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    CheckType obj(make_root_cvt<true>(mem_device(info)));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(info))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_bos_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::bos case 4...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        char32_t buf[] = U"123";
        obj.put(buf, 3);
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::bos fail");

        const auto& dev = obj.device();
        if (dev.dtell() != 12) throw std::runtime_error("code_cvt<memory<char8_t>>::bos fail");
        
        std::u8string info;
        info += '1'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += '2'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += '3'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        if (dev.str() != info) throw std::runtime_error("code_cvt<memory<char8_t>>::bos fail");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8""))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_bos_5()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::bos case 5...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");

        char32_t buf[] = U"李d伟";
        obj.put(buf, 3);
        obj.main_cont_beg();

        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::bos fail");

        auto& dev = obj.device();
        if (dev.dtell() != 12) throw std::runtime_error("code_cvt<memory<char8_t>>::bos fail");
        std::u8string info;
        info += u8'\x4e'; info += u8'\x67'; info += u8'\x00'; info += u8'\x00';
        info += u8'd';    info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += u8'\x1f'; info += u8'\x4f'; info += u8'\x00'; info += u8'\x00';
        if (dev.str() != info) throw std::runtime_error("code_cvt<memory<char8_t>>::bos fail");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8""))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_get_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::get case 1...");

    std::u8string e_lit; e_lit.resize(4102);
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
            if (obj.tell() != total_count) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
            if (s == 0) break;
        }
    
        if (cur_pos - out_buf.data() != 4102 / 7 * 3) throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            if (*it++ != U'李') throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
            if (*it++ != U'伟') throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
            if (*it++ != (i / 3) % 127 + 1) throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
        }
    };

    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj{make_root_cvt<true>(mem_device(e_lit))};
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(e_lit))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_get_nra_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::get_nra case 1...");

    std::u8string e_lit; e_lit.resize(4102);
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
            if (obj.tell() != total_count) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
            if (s == 0) break;
        }
    
        if (cur_pos - out_buf.data() != 4102 / 7 * 3) throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            if (*it++ != U'李') throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
            if (*it++ != U'伟') throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
            if (*it++ != (i / 3) % 127 + 1) throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
        }
    };

    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, false>, char32_t>;
    CheckType obj{make_root_cvt<false>(mem_device(e_lit))};
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<false>(mem_device(e_lit))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_put_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::put case 1...");

    std::u8string e_lit; e_lit.resize(4102);
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
            if (obj.tell() != total_count) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        }
        obj.flush();

        if (obj.device().str() != e_lit) throw std::runtime_error("code_cvt<memory<char8_t>>::put response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8""))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_put_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::put case 2...");

    std::u8string e_lit; e_lit.resize(4102);
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
        obj.flush();

        if (obj.device().str() != e_lit) throw std::runtime_error("code_cvt<memory<char8_t>>::put response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8""))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_flush_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::flush case 1...");

    std::u8string e_lit; e_lit.resize(4102);
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
        if (dev.str().size() >= e_lit.size()) throw std::runtime_error("code_cvt<memory<char8_t>>::flush response incorrect");
        obj.flush();
        if (dev.str().size() != e_lit.size()) throw std::runtime_error("code_cvt<memory<char8_t>>::flush response incorrect");
        if (obj.tell() != i_lit.size()) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
    
        if (dev.str() != e_lit) throw std::runtime_error("code_cvt<memory<char8_t>>::put response incorrect");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8""))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_flush_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::flush case 2...");

    std::u8string e_lit; e_lit.resize(4102);
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
            
            size_t ori_len = obj.device().str().size();
            if (obj.tell() != total_count + dest_size) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
            obj.flush();
            total_count += dest_size;
            if (obj.device().str().size() == ori_len) throw std::runtime_error("code_cvt<memory<char8_t>>::flush response incorrect");
            if (obj.tell() != total_count) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        }
        obj.flush();

        if (obj.device().str() != e_lit) throw std::runtime_error("code_cvt<memory<char8_t>>::put response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8""))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_seek_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::seek case 1...");
    
    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        FAIL_SEEK(obj, 100);
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        FAIL_SEEK(obj, 1);
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        obj.seek(0);
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        
        std::u32string str; str.resize(6);
        if (obj.get(str.data(), 6) != 6) throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
        if (str != U"123456") throw std::runtime_error("code_cvt<memory<char8_t>>::get response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"123456")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8"123456"))}};
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_seek_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::seek case 2...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        char32_t ch = U'李';
        obj.put(&ch, 1);
        if (obj.tell() != 1) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        ch = U'x';
        obj.put(&ch, 1);
        if (obj.tell() != 2) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        ch = U'伟';
        obj.put(&ch, 1);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");

        FAIL_SEEK(obj, 100);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        FAIL_SEEK(obj, 1);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        FAIL_SEEK(obj, 0);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        
        char32_t c[] = U"xy";
        obj.put(c, 2);
        obj.flush();

        if (obj.device().str() != u8"李x伟xy") std::runtime_error("code_cvt<memory<char8_t>> response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8""))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_rseek_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::rseek case 1...");
    
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
        if (obj.device().str() != u8"123456") std::runtime_error("code_cvt<memory<char8_t>> response incorrect");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"123456")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8"123456"))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_rseek_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::rseek case 2...");
    
    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        char32_t ch = U'李';
        obj.put(&ch, 1);
        if (obj.tell() != 1) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        ch = U'x';
        obj.put(&ch, 1);
        if (obj.tell() != 2) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        ch = U'伟';
        obj.put(&ch, 1);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");

        FAIL_RSEEK(obj, 100);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        FAIL_RSEEK(obj, 1);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        FAIL_RSEEK(obj, 1);
        if (obj.tell() != 3) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        
        char32_t c[] = U"xy";
        obj.put(c, 2);
        obj.flush();
    
        if (obj.device().str() != u8"李x伟xy") std::runtime_error("code_cvt<memory<char8_t>> response incorrect");
    };

    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8""))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_io_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>> io case 1...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");

        char32_t bos_str[] = U"abcdefgh";
        obj.put(bos_str, 8);
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::tell fail");
        
        char32_t content1[] = U"12345";
        obj.put(content1, 5);
        FAIL_SEEK(obj, 0);
        
        std::u32string get_content; get_content.resize(3);
        if (obj.get(get_content.data(), 3) != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::get_nra fail");
        if (obj.tell() != 5) throw std::runtime_error("code_cvt<memory<char8_t>>::tell fail");
    
        char32_t content2[] = U"78";
        obj.put(content2, 2);
        if (obj.tell() != 7) throw std::runtime_error("code_cvt<memory<char8_t>>::tell fail");
        obj.flush();
    
        obj.switch_to_get();
        obj.seek(0);
        get_content.resize(3);
        if (obj.get(get_content.data(), 3) != 3) throw std::runtime_error("code_cvt<memory<char8_t>>::get_nra fail");
        if (get_content != U"123") throw std::runtime_error("code_cvt<memory<char8_t>>::get_nra fail");
        try
        {
            obj.switch_to_put();
            dump_info("un-reachable logic");
            std::abort();
        }
        catch(...) {}
    
        std::u8string info;
        info += u8'a'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += u8'b'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += u8'c'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += u8'd'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += u8'e'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += u8'f'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += u8'g'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += u8'h'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += u8"1234578";
        if (obj.device().str() != info) throw std::runtime_error("code_cvt<memory<char8_t>> response incorrect");
    };
    
    using CheckType = code_cvt<root_cvt<mem_device<char8_t>, true>, char32_t>;
    CheckType obj(make_root_cvt<true>(mem_device(u8"")));
    helper(obj);

    runtime_cvt obj2{CheckType{make_root_cvt<true>(mem_device(u8""))}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_io_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>> io case 2...");

    std::u8string info;
    info += u8'a'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'b'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'c'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'd'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'e'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'f'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'g'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'h'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8"12345";

    auto helper = [&info](auto& obj)
    {
        std::u32string bos_str; bos_str.resize(8);
        if (obj.bos() != io_status::input) throw std::runtime_error("code_cvt<mem_device>::bos response incorrect");
        if (obj.get(bos_str.data(), 8) != 8) throw std::runtime_error("code_cvt<memory<char8_t>>::get_nra fail");
        if (bos_str != U"abcdefgh") throw std::runtime_error("code_cvt<memory<char8_t>>::get_nra fail");
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::tell fail");

        std::u32string content_str; content_str.resize(2);
        if (obj.get(content_str.data(), 2) != 2) throw std::runtime_error("code_cvt<memory<char8_t>>:get fail");
        if (content_str != U"12") throw std::runtime_error("code_cvt<memory<char8_t>>:get fail");

        try
        {
            char32_t content1[] = U"67";
            obj.put(content1, 2);
            dump_info("Unreachable code...");
            exit(-1);
        }
        catch (...)
        {
        }
        if (obj.tell() != 2) throw std::runtime_error("code_cvt<memory<char8_t>>::tell fail");

        obj.seek(0);
        if (obj.tell() != 0) throw std::runtime_error("code_cvt<memory<char8_t>>::tell fail");

        content_str.resize(10);
        if (obj.get(content_str.data(), 10) != 5) throw std::runtime_error("code_cvt<...char8_t...>:get fail");
        if (content_str.substr(0, 5) != U"12345") throw std::runtime_error("code_cvt<...char8_t...>:get fail");

        if (obj.device().str() != info) throw std::runtime_error("code_cvt<...char8_t...> response incorrect");
    };

    code_cvt_creator<char8_t, char32_t> creator;
    auto obj = creator.create(make_root_cvt<false>(mem_device(info)));
    helper(obj);

    runtime_cvt obj2(creator.create(make_root_cvt<false>(mem_device(info))));
    helper(obj2);

    dump_info("Done\n");
}