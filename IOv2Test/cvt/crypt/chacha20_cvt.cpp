#include <cvt/crypt/chacha20_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <common/dump_info.h>

void test_chacha20_cvt_gen_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt general case 1...");
    
    {
        using CheckType = Crypt::chacha20_cvt<root_cvt<mem_device<char>, true>>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, char>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = Crypt::chacha20_cvt<root_cvt<mem_device<char8_t>, false>>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
        static_assert(std::is_same_v<CheckType::internal_type, char8_t>);
        static_assert(std::is_same_v<CheckType::external_type, char8_t>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }

    dump_info("Done\n");
}

void test_chacha20_cvt_gen_2()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt general case 2...");

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

    auto helper = [&e_lit]<typename T>(T& obj)
    {
        std::string enc_msg;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("chacha20::bos response incorrect");
            obj.main_cont_beg();
    
            size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            size_t total_count = 0;
            char* cur_pos = e_lit.data();
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
            T obj(Crypt::chacha20_cvt{make_root_cvt<true>(mem_device(enc_msg)), "liwei"});
            if (obj.bos() != io_status::input) throw std::runtime_error("chacha20::bos response incorrect");
            obj.main_cont_beg();
            
            size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
        
            std::string buf; buf.resize(4102 * 2);
            size_t total_count = 0;
            char* cur_pos = buf.data();
            int out_buffer_id = 0;
            while (true)
            {
                auto obj2(std::move(obj));
                size_t dest_size = std::min<size_t>(4102 * 2 - total_count, out_buffer_size[out_buffer_id++]);
                auto s = obj2.get(cur_pos, dest_size);
                out_buffer_id %= std::size(out_buffer_size);
                cur_pos += s;
                total_count += s;
                if (s == 0) break;
                obj = std::move(obj2);
            }
        
            if (cur_pos - buf.data() != 4102) throw std::runtime_error("code_cvt<memory<char>>::get response incorrect");
            buf.resize(4102);
            if (buf != e_lit) throw std::runtime_error("chacha20 io response incorrect");
        }
    };

    using CheckType = Crypt::chacha20_cvt<root_cvt<mem_device<char>, true>>;
    CheckType obj(make_root_cvt<true>(mem_device("")), "liwei");
    helper(obj);

    CheckType tmp(make_root_cvt<true>(mem_device("")), "liwei");
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_chacha20_cvt_put_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt::put case 1...");

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

    using CheckType = Crypt::chacha20_cvt<root_cvt<mem_device<char>, true>>;

    auto out_helper = [&e_lit](auto& obj1, auto& obj2)
    {
        auto helper = [&e_lit](auto& obj)
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("chacha20::bos response incorrect");
            obj.main_cont_beg();
    
            size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            size_t total_count = 0;
            char* cur_pos = e_lit.data();
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

    using CheckType = Crypt::chacha20_cvt<root_cvt<mem_device<char>, true>>;
    {
        CheckType obj1(make_root_cvt<true>(mem_device("")), "liwei");
        CheckType obj2(make_root_cvt<true>(mem_device("")), "liwei");
        out_helper(obj1, obj2);
    }
    {
        CheckType tmp1(make_root_cvt<true>(mem_device("")), "liwei");
        CheckType tmp2(make_root_cvt<true>(mem_device("")), "liwei");
        runtime_cvt obj1(std::move(tmp1));
        runtime_cvt obj2(std::move(tmp2));
        out_helper(obj1, obj2);
    }
    dump_info("Done\n");
}

void test_chacha20_cvt_io_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt io case 1...");

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

    auto helper = [&e_lit]<typename T>(T& obj)
    {
        std::string enc_msg;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("chacha20_cvt::bos response incorrect");
            obj.main_cont_beg();
    
            size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            size_t total_count = 0;
            char* cur_pos = e_lit.data();
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
            T obj(Crypt::chacha20_cvt{make_root_cvt<true>(mem_device(enc_msg)), "liwei"});
            if (obj.bos() != io_status::input) throw std::runtime_error("chacha20_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            std::string buf;
            buf.resize(4102 * 2);
            if (obj.get(buf.data(), buf.size()) != 4102) throw std::runtime_error("chacha20_cvt::get response incorrect");
            buf.resize(4102);
            if (buf != e_lit) throw std::runtime_error("chacha20_cvt io response incorrect");
        }
    };

    using CheckType = Crypt::chacha20_cvt<root_cvt<mem_device<char>, true>>;
    CheckType obj(make_root_cvt<true>(mem_device("")), "liwei");
    helper(obj);

    CheckType tmp(make_root_cvt<true>(mem_device("")), "liwei");
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);
    
    dump_info("Done\n");
}