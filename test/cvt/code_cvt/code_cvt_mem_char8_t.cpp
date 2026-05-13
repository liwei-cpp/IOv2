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
        using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
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
        using CheckType = code_cvt<no_rb_root_cvt<mem_device<char8_t>>, char32_t>;
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;

    CheckType obj1{rb_root_cvt{mem_device(e_lit)}};
    helper(obj1);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(e_lit)}}};
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj1{rb_root_cvt{mem_device(e_lit)}};
    helper(obj1);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(e_lit)}}};
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
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
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
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

        auto [detach_dev, detach_err] = obj.detach();
        if (detach_dev.dtell() != 12) throw std::runtime_error("code_cvt<...char...>::bos fail");
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    std::u8string info;
    info += u8'1'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'2'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'3'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'4'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'5'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    CheckType obj(rb_root_cvt{mem_device(info)});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(info)}}};
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

        auto [detach_dev, detach_err] = obj.detach();
        if (detach_dev.dtell() != 12) throw std::runtime_error("code_cvt<memory<char8_t>>::bos fail");
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    std::u8string info;
    info += u8'\x4e'; info += u8'\x67'; info += u8'\x00'; info += u8'\x00';
    info += u8'd';    info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'\x1f'; info += u8'\x4f'; info += u8'\x00'; info += u8'\x00';
    info += u8'c';    info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'p';    info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    info += u8'p';    info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
    CheckType obj(rb_root_cvt{mem_device(info)});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(info)}}};
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
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
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
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj{rb_root_cvt{mem_device(e_lit)}};
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(e_lit)}}};
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

    using CheckType = code_cvt<no_rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj{no_rb_root_cvt{mem_device(e_lit)}};
    helper(obj);

    runtime_cvt obj2{CheckType{no_rb_root_cvt{mem_device(e_lit)}}};
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
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
        obj.flush();
        if (dev.str().size() != e_lit.size()) throw std::runtime_error("code_cvt<memory<char8_t>>::flush response incorrect");
        if (obj.tell() != i_lit.size()) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
    
        if (dev.str() != e_lit) throw std::runtime_error("code_cvt<memory<char8_t>>::put response incorrect");
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
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
            size_t ori_len = obj.device().str().size();
            obj.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;
            
            if (obj.tell() != total_count + dest_size) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
            obj.flush();
            total_count += dest_size;
            if (obj.device().str().size() == ori_len) throw std::runtime_error("code_cvt<memory<char8_t>>::flush response incorrect");
            if (obj.tell() != total_count) throw std::runtime_error("code_cvt<memory<char8_t>>::tell response incorrect");
        }
        obj.flush();

        if (obj.device().str() != e_lit) throw std::runtime_error("code_cvt<memory<char8_t>>::put response incorrect");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"123456")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"123456")}}};
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
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
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"123456")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"123456")}}};
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

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
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
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
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
    auto obj = creator.create(no_rb_root_cvt{mem_device(info)});
    helper(obj);

    runtime_cvt obj2(creator.create(no_rb_root_cvt{mem_device(info)}));
    helper(obj2);

    dump_info("Done\n");
}

// Covers line 480 (char8_t::is_state_dep), lines 531-532 (out_helper 2-byte),
// and lines 613-619 (in_helper 2-byte success path).
void test_code_cvt_mem_char8_t_gen_6()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>> general case 6 (2-byte UTF-8)...");

    // Direct kernel instantiation: covers is_state_dep() (line 480)
    {
        codecvt_kernel<char8_t, char32_t> k;
        VERIFY(!k.is_state_dep());
    }

    // é=U+00E9={0xC3,0xA9}, ñ=U+00F1={0xC3,0xB1}, µ=U+00B5={0xC2,0xB5}
    std::u8string e_lit;
    e_lit += char8_t(0xC3); e_lit += char8_t(0xA9);
    e_lit += char8_t(0xC3); e_lit += char8_t(0xB1);
    e_lit += char8_t(0xC2); e_lit += char8_t(0xB5);
    e_lit += char8_t(0x41);  // 'A'
    std::u32string i_lit = { U'é', U'ñ', U'µ', U'A' };

    auto helper_get = [&](auto& obj) {
        if (obj.bos() != io_status::input)
            throw std::runtime_error("test_code_cvt_mem_char8_t_gen_6: bos fail");
        obj.main_cont_beg();
        std::u32string out(4, 0);
        if (obj.get(out.data(), 4) != 4)
            throw std::runtime_error("test_code_cvt_mem_char8_t_gen_6: get count fail");
        if (out != i_lit)
            throw std::runtime_error("test_code_cvt_mem_char8_t_gen_6: get content fail");
    };

    auto helper_put = [&](auto& obj) {
        if (obj.bos() != io_status::output)
            throw std::runtime_error("test_code_cvt_mem_char8_t_gen_6: bos fail");
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());
        obj.flush();
        if (obj.device().str() != e_lit)
            throw std::runtime_error("test_code_cvt_mem_char8_t_gen_6: put content fail");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;

    CheckType obj1{rb_root_cvt{mem_device(e_lit)}};
    helper_get(obj1);
    runtime_cvt obj1r{CheckType{rb_root_cvt{mem_device(e_lit)}}};
    helper_get(obj1r);

    CheckType obj2{rb_root_cvt{mem_device(std::u8string{})}};
    helper_put(obj2);
    runtime_cvt obj2r{CheckType{rb_root_cvt{mem_device(std::u8string{})}}};
    helper_put(obj2r);

    dump_info("Done\n");
}

// Covers lines 540-548 (out_helper 4-byte) and lines 635, 637-646
// (in_helper 4-byte success path).
void test_code_cvt_mem_char8_t_gen_7()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>> general case 7 (4-byte UTF-8)...");

    // 😀=U+1F600={0xF0,0x9F,0x98,0x80}, 💩=U+1F4A9={0xF0,0x9F,0x92,0xA9}
    std::u8string e_lit;
    e_lit += char8_t(0xF0); e_lit += char8_t(0x9F);
    e_lit += char8_t(0x98); e_lit += char8_t(0x80);
    e_lit += char8_t(0xF0); e_lit += char8_t(0x9F);
    e_lit += char8_t(0x92); e_lit += char8_t(0xA9);
    e_lit += char8_t(0x42);  // 'B'
    std::u32string i_lit = { U'\U0001F600', U'\U0001F4A9', U'B' };

    auto helper_get = [&](auto& obj) {
        if (obj.bos() != io_status::input)
            throw std::runtime_error("test_code_cvt_mem_char8_t_gen_7: bos fail");
        obj.main_cont_beg();
        std::u32string out(3, 0);
        if (obj.get(out.data(), 3) != 3)
            throw std::runtime_error("test_code_cvt_mem_char8_t_gen_7: get count fail");
        if (out != i_lit)
            throw std::runtime_error("test_code_cvt_mem_char8_t_gen_7: get content fail");
    };

    auto helper_put = [&](auto& obj) {
        if (obj.bos() != io_status::output)
            throw std::runtime_error("test_code_cvt_mem_char8_t_gen_7: bos fail");
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());
        obj.flush();
        if (obj.device().str() != e_lit)
            throw std::runtime_error("test_code_cvt_mem_char8_t_gen_7: put content fail");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;

    CheckType obj1{rb_root_cvt{mem_device(e_lit)}};
    helper_get(obj1);
    runtime_cvt obj1r{CheckType{rb_root_cvt{mem_device(e_lit)}}};
    helper_get(obj1r);

    CheckType obj2{rb_root_cvt{mem_device(std::u8string{})}};
    helper_put(obj2);
    runtime_cvt obj2r{CheckType{rb_root_cvt{mem_device(std::u8string{})}}};
    helper_put(obj2r);

    dump_info("Done\n");
}

// Covers line 525 (out_helper surrogate rejected), lines 547-548
// (out_helper > 0x10FFFF rejected), and line 926 (put encoding error throw).
void test_code_cvt_mem_char8_t_put_err_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::put encoding error case 1...");

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;

    auto make_obj = [] {
        return CheckType{rb_root_cvt{mem_device(std::u8string{})}};
    };

    auto try_put = [](auto& obj, char32_t ch) {
        if (obj.bos() != io_status::output)
            throw std::runtime_error("test_code_cvt_mem_char8_t_put_err_1: bos fail");
        obj.main_cont_beg();
        try {
            obj.put(&ch, 1);
            throw std::runtime_error("test_code_cvt_mem_char8_t_put_err_1: expected throw");
        } catch (const cvt_error&) {}
    };

    // Surrogate U+D800 → line 525 → line 926
    { auto o = make_obj(); try_put(o, static_cast<char32_t>(0xD800U)); }
    // Surrogate U+DFFF → line 525 → line 926
    { auto o = make_obj(); try_put(o, static_cast<char32_t>(0xDFFFU)); }
    // > 0x10FFFF → lines 547-548 → line 926
    { auto o = make_obj(); try_put(o, static_cast<char32_t>(0x110000U)); }

    dump_info("Done\n");
}

// Covers lines 610-611 (invalid start byte), 614-615 (bad 2-byte continuation),
// 617 (overlong 2-byte), 626-627 (bad 3-byte continuation), 631 (surrogate in 3-byte),
// 641-642 (bad 4-byte continuation), 644 (4-byte out of range), 649 (byte >= 0xF8),
// and line 986 (get invalid sequence throw).
void test_code_cvt_mem_char8_t_get_err_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::get error case 1...");

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;

    auto run_err = [](const std::u8string& bad_bytes) {
        CheckType obj{rb_root_cvt{mem_device(bad_bytes)}};
        if (obj.bos() != io_status::input)
            throw std::runtime_error("test_code_cvt_mem_char8_t_get_err_1: bos fail");
        obj.main_cont_beg();
        char32_t buf[4];
        try {
            obj.get(buf, 4);
            throw std::runtime_error("test_code_cvt_mem_char8_t_get_err_1: expected throw");
        } catch (const cvt_error&) {}
    };

    // 0x80: continuation byte as start → lines 610-611
    run_err({ char8_t(0x80) });
    // {0xC3, 0x20}: bad continuation in 2-byte → lines 614-615
    run_err({ char8_t(0xC3), char8_t(0x20) });
    // {0xC0, 0x80}: overlong null (2-byte) → line 617
    run_err({ char8_t(0xC0), char8_t(0x80) });
    // {0xE6, 0x20, 0x8E}: bad c2 in 3-byte → lines 626-627
    run_err({ char8_t(0xE6), char8_t(0x20), char8_t(0x8E) });
    // {0xE6, 0x9D, 0x20}: bad c3 in 3-byte → lines 626-627
    run_err({ char8_t(0xE6), char8_t(0x9D), char8_t(0x20) });
    // {0xED, 0xA0, 0x80}: U+D800 encoded as 3-byte (surrogate) → line 631
    run_err({ char8_t(0xED), char8_t(0xA0), char8_t(0x80) });
    // {0xF0, 0x20, 0x80, 0x80}: bad c2 in 4-byte → lines 641-642
    run_err({ char8_t(0xF0), char8_t(0x20), char8_t(0x80), char8_t(0x80) });
    // {0xF4, 0x90, 0x80, 0x80}: U+110000 (> 0x10FFFF) → line 644
    run_err({ char8_t(0xF4), char8_t(0x90), char8_t(0x80), char8_t(0x80) });
    // {0xF0, 0x80, 0x80, 0x80}: U+0000 overlong 4-byte (< 0x10000) → line 644
    run_err({ char8_t(0xF0), char8_t(0x80), char8_t(0x80), char8_t(0x80) });
    // 0xFF: byte >= 0xF8 → line 649
    run_err({ char8_t(0xFF) });

    dump_info("Done\n");
}

// Covers line 612 (2-byte partial break), line 637 (4-byte partial break),
// and line 972 (get partial input sequence throw).
void test_code_cvt_mem_char8_t_get_partial_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>>::get partial sequence case 1...");

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;

    auto run_partial = [](const std::u8string& partial) {
        CheckType obj{rb_root_cvt{mem_device(partial)}};
        if (obj.bos() != io_status::input)
            throw std::runtime_error("test_code_cvt_mem_char8_t_get_partial_1: bos fail");
        obj.main_cont_beg();
        char32_t buf[4];
        try {
            obj.get(buf, 1);
            throw std::runtime_error("test_code_cvt_mem_char8_t_get_partial_1: expected throw");
        } catch (const cvt_error&) {}
    };

    // Single 2-byte start byte: line 612 break → line 972
    run_partial({ char8_t(0xC3) });
    // 3 bytes of a 4-byte sequence: line 637 break → line 972
    run_partial({ char8_t(0xF0), char8_t(0x9F), char8_t(0x98) });

    dump_info("Done\n");
}

// Covers lines 833-836 (code_cvt::attach), lines 1162-1163 (switch_to_put
// from output, no-op) and lines 1164-1167 (switch_to_put from neutral).
void test_code_cvt_attach_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt::attach and switch_to_put from neutral/output...");

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;

    CheckType obj{rb_root_cvt{mem_device(std::u8string{})}};

    if (obj.bos() != io_status::output)
        throw std::runtime_error("test_code_cvt_attach_1: bos fail");
    obj.main_cont_beg();
    char32_t ch = U'X';
    obj.put(&ch, 1);

    // switch_to_put when already in output: no-op (lines 1162-1163)
    obj.switch_to_put();
    VERIFY(obj.tell() == 1);

    // attach() resets to neutral (lines 833-836, close_stream)
    auto [old_dev, old_err] = obj.detach();
    obj.attach(mem_device(std::u8string{}));
    VERIFY(old_dev.str().size() >= 1);

    // switch_to_put from neutral (lines 1164-1167); skip bos() since
    // switch_to_put already established the direction
    obj.switch_to_put();
    obj.main_cont_beg();
    obj.put(&ch, 1);
    obj.flush();
    VERIFY(obj.device().str().size() == 1);  // 'X' (U+0058) = 1 UTF-8 byte

    dump_info("Done\n");
}

// Covers lines 1207-1208 (switch_to_get from input, no-op) and
// lines 1209-1212 (switch_to_get from neutral).
void test_code_cvt_switch_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt::switch_to_get from input/neutral...");

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, char32_t>;

    // Input: é (U+00E9) = {0xC3, 0xA9}
    std::u8string content = { char8_t(0xC3), char8_t(0xA9) };

    CheckType obj{rb_root_cvt{mem_device(content)}};

    if (obj.bos() != io_status::input)
        throw std::runtime_error("test_code_cvt_switch_1: bos fail");
    obj.main_cont_beg();

    // switch_to_get when already in input: no-op (lines 1207-1208)
    obj.switch_to_get();

    char32_t ch = 0;
    if (obj.get(&ch, 1) != 1 || ch != U'é')
        throw std::runtime_error("test_code_cvt_switch_1: get fail");

    // attach() resets to neutral
    obj.attach(mem_device(content));

    // switch_to_get from neutral (lines 1209-1212); skip bos() since
    // switch_to_get already established the direction
    obj.switch_to_get();
    obj.main_cont_beg();
    ch = 0;
    if (obj.get(&ch, 1) != 1 || ch != U'é')
        throw std::runtime_error("test_code_cvt_switch_1: get after neutral fail");

    dump_info("Done\n");
}

// Covers code_cvt.h lines 518-519 (out_helper reversed range),
// 521-522 (out_helper buffer < 4 bytes), 596-597 (in_helper reversed range).
void test_code_cvt_mem_char8_t_kernel_helpers_1()
{
    using namespace IOv2;
    dump_info("Test codecvt_kernel<char8_t,char32_t> invalid range and small buffer...");

    codecvt_kernel<char8_t, char32_t> kernel;

    // out_helper: to > to_end → throws (lines 518-519)
    {
        char8_t buf[4];
        char8_t* to = buf + 2;
        char8_t* to_end = buf;
        try {
            kernel.out_helper(U'A', to, to_end);
            throw std::runtime_error("out_helper reversed: expected throw");
        } catch (const cvt_error&) {}
    }

    // out_helper: 2-byte buffer (2 < 4) → false (lines 521-522)
    {
        char8_t buf[2];
        char8_t* to = buf;
        char8_t* to_end = buf + 2;
        bool r = kernel.out_helper(U'A', to, to_end);
        if (r) throw std::runtime_error("out_helper small buf: expected false");
    }

    // in_helper: from > from_end → throws (lines 596-597)
    {
        const char8_t input[] = u8"hello";
        const char8_t* from = input + 3;
        const char8_t* from_end = input;
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