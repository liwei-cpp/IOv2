#include <cvt/crypt/chacha20_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <common/dump_info.h>

void test_chacha20_cvt_wchar_t_gen_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt<wchar_t> general case 1...");
    
    {
        using CheckType = Crypt::chacha20_cvt<root_cvt<mem_device<char>, true>, wchar_t>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = Crypt::chacha20_cvt<root_cvt<mem_device<char8_t>, false>, wchar_t>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
        static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
        static_assert(std::is_same_v<CheckType::external_type, char8_t>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }

    dump_info("Done\n");
}

void test_chacha20_cvt_wchar_t_gen_2()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt<wchar_t> general case 2...");

    std::wstring e_lit; e_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = L'\xE6';
        e_lit[i+1] = L'\x9D';
        e_lit[i+2] = L'\x8E';
        e_lit[i+3] = L'\xE4';
        e_lit[i+4] = L'\xBC';
        e_lit[i+5] = L'\x9F';
        e_lit[i+6] = (i / 7) % 127 + 1;
    }

    auto helper = [&e_lit]<typename T>(T& obj)
    {
        Crypt::chacha20_cvt_creator<wchar_t> creator("liwei");
        std::string enc_msg;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("chacha20::bos response incorrect");
            obj.main_cont_beg();
    
            size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            size_t total_count = 0;
            wchar_t* cur_pos = e_lit.data();
            int buffer_id = 0;
            while (total_count < 4102)
            {
                auto obj2(std::move(obj));
                size_t dest_size = std::min<size_t>(4102 - total_count, buffer_size[buffer_id++]);
                obj2.put(cur_pos, dest_size);
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
                total_count += dest_size;
                obj = std::move(obj2);
            }
    
            auto dev = obj.detach();
            enc_msg = dev.str();
        }
    
        {
            T local_obj(creator.create(make_root_cvt<true>(mem_device(enc_msg))));
            if (local_obj.bos() != io_status::input) throw std::runtime_error("chacha20::bos response incorrect");
            local_obj.main_cont_beg();
            
            size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
        
            std::wstring buf; buf.resize(4102 * 2);
            size_t total_count = 0;
            wchar_t* cur_pos = buf.data();
            int out_buffer_id = 0;
            while (true)
            {
                auto obj2(std::move(local_obj));
                size_t dest_size = std::min<size_t>(4102 * 2 - total_count, out_buffer_size[out_buffer_id++]);
                auto s = obj2.get(cur_pos, dest_size);
                out_buffer_id %= std::size(out_buffer_size);
                cur_pos += s;
                total_count += s;
                if (s == 0) break;
                local_obj = std::move(obj2);
            }
        
            if (cur_pos - buf.data() != 4102) throw std::runtime_error("code_cvt<memory<char>>::get response incorrect");
            buf.resize(4102);
            if (buf != e_lit) throw std::runtime_error("chacha20 io response incorrect");
        }
    };

    Crypt::chacha20_cvt_creator<wchar_t> creator("liwei");
    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj);

    runtime_cvt obj2(creator.create(make_root_cvt<true>(mem_device(""))));
    helper(obj2);

    dump_info("Done\n");
}

void test_chacha20_cvt_wchar_t_put_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt<wchar_t>::put case 1...");

    std::wstring e_lit; e_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = L'\xE6';
        e_lit[i+1] = L'\x9D';
        e_lit[i+2] = L'\x8E';
        e_lit[i+3] = L'\xE4';
        e_lit[i+4] = L'\xBC';
        e_lit[i+5] = L'\x9F';
        e_lit[i+6] = (i / 7) % 127 + 1;
    }

    auto out_helper = [&e_lit](auto& obj1, auto& obj2)
    {
        auto helper = [&e_lit](auto& obj)
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("chacha20::bos response incorrect");
            obj.main_cont_beg();
    
            size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            size_t total_count = 0;
            auto cur_pos = e_lit.data();
            int buffer_id = 0;
            while (total_count < 4102)
            {
                size_t dest_size = std::min<size_t>(4102 - total_count, buffer_size[buffer_id++]);
                obj.put(cur_pos, dest_size);
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
                total_count += dest_size;
            }
    
            auto dev = obj.detach();
            return dev.str();
        };
        
        std::string r1 = helper(obj1);
        std::string r2 = helper(obj2);
        if (r1 == r2) throw std::runtime_error("chacha20::put response incorrect");
    };

    Crypt::chacha20_cvt_creator<wchar_t> creator("liwei");
    {
        auto obj1 = creator.create(make_root_cvt<true>(mem_device("")));
        auto obj2 = creator.create(make_root_cvt<true>(mem_device("")));
        out_helper(obj1, obj2);
    }
    {
        runtime_cvt obj1(creator.create(make_root_cvt<true>(mem_device(""))));
        runtime_cvt obj2(creator.create(make_root_cvt<true>(mem_device(""))));
        out_helper(obj1, obj2);
    }
    dump_info("Done\n");
}

void test_chacha20_cvt_wchar_t_io_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt<wchar_t> io case 1...");

    std::wstring e_lit; e_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = L'\xE6';
        e_lit[i+1] = L'\x9D';
        e_lit[i+2] = L'\x8E';
        e_lit[i+3] = L'\xE4';
        e_lit[i+4] = L'\xBC';
        e_lit[i+5] = L'\x9F';
        e_lit[i+6] = (i / 7) % 127 + 1;
    }

    auto helper = [&e_lit]<typename T>(T& obj)
    {
        Crypt::chacha20_cvt_creator<wchar_t> creator("liwei");
        std::string enc_msg;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("chacha20_cvt::bos response incorrect");
            obj.main_cont_beg();
    
            size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            size_t total_count = 0;
            wchar_t* cur_pos = e_lit.data();
            int buffer_id = 0;
            while (total_count < 4102)
            {
                size_t dest_size = std::min<size_t>(4102 - total_count, buffer_size[buffer_id++]);
                obj.put(cur_pos, dest_size);
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
                total_count += dest_size;
            }
    
            auto dev = obj.detach();
            enc_msg = dev.str();
        }
    
        {
            T local_obj(creator.create(make_root_cvt<true>(mem_device(enc_msg))));
            if (local_obj.bos() != io_status::input) throw std::runtime_error("chacha20_cvt::bos response incorrect");
            local_obj.main_cont_beg();
            
            std::wstring buf;
            buf.resize(4102 * 2);
            if (local_obj.get(buf.data(), buf.size()) != 4102) throw std::runtime_error("chacha20_cvt::get response incorrect");
            buf.resize(4102);
            if (buf != e_lit) throw std::runtime_error("chacha20_cvt io response incorrect");
        }
    };

    Crypt::chacha20_cvt_creator<wchar_t> creator("liwei");
    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj);

    runtime_cvt obj2(creator.create(make_root_cvt<true>(mem_device(""))));
    helper(obj2);
    
    dump_info("Done\n");
}