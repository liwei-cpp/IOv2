#include <cvt/code_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <support/dump_info.h>
#include <support/verify.h>

void test_code_cvt_mem_char8_t_wchar_t_gen_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t> general case 1...");
    
    {
        using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
        static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
        static_assert(std::is_same_v<CheckType::external_type, char8_t>);
        
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(cvt_cpt::support_io_switch<CheckType>);
    }

    {
        using CheckType = code_cvt<no_rb_root_cvt<mem_device<char8_t>>, wchar_t>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
        static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
        static_assert(std::is_same_v<CheckType::external_type, char8_t>);
    }

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_gen_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t> general case 2...");

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
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
        std::vector<wchar_t> out_buf; out_buf.resize(4102);
        size_t total_count = 0;
        wchar_t* cur_pos = out_buf.data();
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
            VERIFY(*it++ == L'李');
            VERIFY(*it++ == L'伟');
            VERIFY(*it++ == (wchar_t)((i / 3) % 127 + 1));
        }
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;

    CheckType obj1{rb_root_cvt{mem_device(e_lit)}};
    helper(obj1);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(e_lit)}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_gen_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t> general case 3...");

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
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
        std::vector<wchar_t> out_buf; out_buf.resize(4102);
        size_t total_count = 0;
        wchar_t* cur_pos = out_buf.data();
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
            VERIFY(*it++ == L'李');
            VERIFY(*it++ == L'伟');
            VERIFY(*it++ == (wchar_t)((i / 3) % 127 + 1));
        }
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj1{rb_root_cvt{mem_device(e_lit)}};
    helper(obj1);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(e_lit)}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_gen_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t> general case 4...");

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
    std::wstring i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(L'李');
        i_lit.push_back(L'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        size_t total_count = 0;
        wchar_t* cur_pos = i_lit.data();
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
        obj.flush();

        VERIFY(obj.device().str() == e_lit);
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_gen_5()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t> general case 5...");

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
    std::wstring i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(L'李');
        i_lit.push_back(L'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        size_t total_count = 0;
        wchar_t* cur_pos = i_lit.data();
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
        obj.flush();

        VERIFY(obj.device().str() == e_lit);
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_bos_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::bos case 1...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        VERIFY(obj.device().dtell() == 0);
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_bos_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::bos case 2...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        
        wchar_t c = 0;
        VERIFY((obj.get(&c, 1) == 1) && (c == L'1'));
        VERIFY((obj.get(&c, 1) == 1) && (c == L'2'));
        VERIFY((obj.get(&c, 1) == 1) && (c == L'3'));

        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        auto [detach_dev, detach_err] = obj.detach();
        VERIFY(detach_dev.dtell() == 12);
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
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

void test_code_cvt_mem_char8_t_wchar_t_bos_3()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::bos case 3...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        wchar_t c = 0;
        VERIFY((obj.get(&c, 1) == 1) && (c == L'李'));
        VERIFY((obj.get(&c, 1) == 1) && (c == L'd'));
        VERIFY((obj.get(&c, 1) == 1) && (c == L'伟'));
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        auto [detach_dev, detach_err] = obj.detach();
        VERIFY(detach_dev.dtell() == 12);
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
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

void test_code_cvt_mem_char8_t_wchar_t_bos_4()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::bos case 4...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        wchar_t buf[] = L"123";
        obj.put(buf, 3);
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        const auto& dev = obj.device();
        VERIFY(dev.dtell() == 12);
        
        std::u8string info;
        info += '1'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += '2'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += '3'; info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        VERIFY(dev.str() == info);
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_bos_5()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::bos case 5...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);

        wchar_t buf[] = L"李d伟";
        obj.put(buf, 3);
        obj.main_cont_beg();

        VERIFY(obj.tell() == 0);

        auto& dev = obj.device();
        VERIFY(dev.dtell() == 12);
        std::u8string info;
        info += u8'\x4e'; info += u8'\x67'; info += u8'\x00'; info += u8'\x00';
        info += u8'd';    info += u8'\x00'; info += u8'\x00'; info += u8'\x00';
        info += u8'\x1f'; info += u8'\x4f'; info += u8'\x00'; info += u8'\x00';
        VERIFY(dev.str() == info);
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_get_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::get case 1...");

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
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
        std::vector<wchar_t> out_buf; out_buf.resize(4102);
        size_t total_count = 0;
        wchar_t* cur_pos = out_buf.data();
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
            VERIFY(*it++ == L'李');
            VERIFY(*it++ == L'伟');
            VERIFY(*it++ == (wchar_t)((i / 3) % 127 + 1));
        }
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj{rb_root_cvt{mem_device(e_lit)}};
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(e_lit)}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_get_nra_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::get_nra case 1...");

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
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
        std::vector<wchar_t> out_buf; out_buf.resize(4102);
        size_t total_count = 0;
        wchar_t* cur_pos = out_buf.data();
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
            VERIFY(*it++ == L'李');
            VERIFY(*it++ == L'伟');
            VERIFY(*it++ == (wchar_t)((i / 3) % 127 + 1));
        }
    };

    using CheckType = code_cvt<no_rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj{no_rb_root_cvt{mem_device(e_lit)}};
    helper(obj);

    runtime_cvt obj2{CheckType{no_rb_root_cvt{mem_device(e_lit)}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_put_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::put case 1...");

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
    std::wstring i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(L'李');
        i_lit.push_back(L'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        size_t total_count = 0;
        wchar_t* cur_pos = i_lit.data();
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
        obj.flush();

        VERIFY(obj.device().str() == e_lit);
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_put_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::put case 2...");

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
    std::wstring i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(L'李');
        i_lit.push_back(L'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());
        obj.flush();

        VERIFY(obj.device().str() == e_lit);
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_flush_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::flush case 1...");

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
    std::wstring i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(L'李');
        i_lit.push_back(L'伟');
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
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_flush_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::flush case 2...");

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
    std::wstring i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(L'李');
        i_lit.push_back(L'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        size_t total_count = 0;
        wchar_t* cur_pos = i_lit.data();
        int buffer_id = 0;
        while (total_count < 4102 / 7 * 3)
        {
            size_t dest_size = std::min<size_t>(4102 / 7 * 3 - total_count, buffer_size[buffer_id++]);
            size_t ori_len = obj.device().str().size();
            obj.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;

            VERIFY(obj.tell() == total_count + dest_size);
            obj.flush();
            total_count += dest_size;
            VERIFY(obj.device().str().size() != ori_len);
            VERIFY(obj.tell() == total_count);
        }
        obj.flush();

        VERIFY(obj.device().str() == e_lit);
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_seek_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::seek case 1...");
    
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
        
        std::wstring str; str.resize(6);
        VERIFY(obj.get(str.data(), 6) == 6);
        VERIFY(str == L"123456");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"123456")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"123456")}}};
    helper(obj2);
    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_seek_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::seek case 2...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        
        VERIFY(obj.tell() == 0);
        wchar_t ch = L'李';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 1);
        ch = L'x';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 2);
        ch = L'伟';
        obj.put(&ch, 1);
        VERIFY(obj.tell() == 3);

        FAIL_SEEK(obj, 100);
        VERIFY(obj.tell() == 3);
        FAIL_SEEK(obj, 1);
        VERIFY(obj.tell() == 3);
        FAIL_SEEK(obj, 0);
        VERIFY(obj.tell() == 3);
        
        wchar_t c[] = L"xy";
        obj.put(c, 2);
        obj.flush();

        if (obj.device().str() != u8"李x伟xy") std::runtime_error("code_cvt<memory<char8_t>> response incorrect");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_rseek_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::rseek case 1...");
    
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
        
        std::wstring str; str.resize(6);
        VERIFY(obj.get(str.data(), 6) == 6);
        if (obj.device().str() != u8"123456") std::runtime_error("code_cvt<memory<char8_t>> response incorrect");
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"123456")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"123456")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_rseek_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t>::rseek case 2...");
    
    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        
        VERIFY(obj.tell() == 0);
        wchar_t ch = U'李';
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
        
        wchar_t c[] = L"xy";
        obj.put(c, 2);
        obj.flush();
    
        if (obj.device().str() != u8"李x伟xy") std::runtime_error("code_cvt<memory<char8_t>> response incorrect");
    };

    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_io_1()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t> io case 1...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);

        wchar_t bos_str[] = L"abcdefgh";
        obj.put(bos_str, 8);
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);
        
        wchar_t content1[] = L"12345";
        obj.put(content1, 5);
        FAIL_SEEK(obj, 0);
        
        std::wstring get_content; get_content.resize(3);
        VERIFY(obj.get(get_content.data(), 3) == 0);
        VERIFY(obj.tell() == 5);
    
        wchar_t content2[] = L"78";
        obj.put(content2, 2);
        VERIFY(obj.tell() == 7);
        obj.flush();
    
        obj.switch_to_get();
        obj.seek(0);
        get_content.resize(3);
        VERIFY(obj.get(get_content.data(), 3) == 3);
        VERIFY(get_content == L"123");
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
        VERIFY(obj.device().str() == info);
    };
    
    using CheckType = code_cvt<rb_root_cvt<mem_device<char8_t>>, wchar_t>;
    CheckType obj(rb_root_cvt{mem_device(u8"")});
    helper(obj);

    runtime_cvt obj2{CheckType{rb_root_cvt{mem_device(u8"")}}};
    helper(obj2);

    dump_info("Done\n");
}

void test_code_cvt_mem_char8_t_wchar_t_io_2()
{
    using namespace IOv2;
    dump_info("Test code_cvt<memory<char8_t>, wchar_t> io case 2...");

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
        std::wstring bos_str; bos_str.resize(8);
        VERIFY(obj.bos() == io_status::input);
        VERIFY(obj.get(bos_str.data(), 8) == 8);
        VERIFY(bos_str == L"abcdefgh");
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        std::wstring content_str; content_str.resize(2);
        VERIFY(obj.get(content_str.data(), 2) == 2);
        VERIFY(content_str == L"12");

        try
        {
            wchar_t content1[] = L"67";
            obj.put(content1, 2);
            dump_info("Unreachable code...");
            exit(-1);
        }
        catch (...)
        {
        }
        VERIFY(obj.tell() == 2);

        obj.seek(0);
        VERIFY(obj.tell() == 0);

        content_str.resize(10);
        VERIFY(obj.get(content_str.data(), 10) == 5);
        VERIFY(content_str.substr(0, 5) == L"12345");

        VERIFY(obj.device().str() == info);
    };

    code_cvt_creator<char8_t, wchar_t> creator;
    auto obj = creator.create(no_rb_root_cvt{mem_device(info)});
    helper(obj);

    runtime_cvt obj2(creator.create(no_rb_root_cvt{mem_device(info)}));
    helper(obj2);

    dump_info("Done\n");
}