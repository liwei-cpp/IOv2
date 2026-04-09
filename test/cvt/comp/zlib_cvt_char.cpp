#include <cvt/comp/zlib_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <common/dump_info.h>

namespace
{
    std::string s_e_lit = []()
    {
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
        return e_lit;
    }();
}

void test_zlib_cvt_gen_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt general case 1...");
    
    {
        using CheckType = Comp::zlib_cvt<root_cvt<mem_device<char>, true>>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, char>);
        static_assert(std::is_same_v<CheckType::external_type, char>);

        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = Comp::zlib_cvt<root_cvt<mem_device<char8_t>, false>>;
        static_assert(io_converter<CheckType>);
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

void test_zlib_cvt_gen_2()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt general case 2...");
    
    auto helper = []<typename T>(T& obj)
    {
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);
            
            auto obj2(obj);
            obj.put(s_e_lit.data() + 1024, 4102 - 1024);
            obj2.put(s_e_lit.data() + 1024, 4102 - 1024);
            
            auto dev1 = obj.detach();
            auto dev2 = obj2.detach();
            
            if (dev1.str() != dev2.str())
                throw std::runtime_error("zlib_cvt copy constructor response incorrect");
            compress_res = dev1.str();
        }
    
        {
            T obj2{Comp::zlib_cvt{make_root_cvt<true>(mem_device(compress_res)), 0}};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            std::string out_buf1; out_buf1.resize(4102);
            std::string out_buf2; out_buf2.resize(4102 - 1026);
    
            if (obj2.get(out_buf1.data(), 1026) != 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
            auto obj3(obj2);
            
            if (obj2.get(out_buf1.data() + 1026, 4102 - 1026) != 4102 - 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (obj3.get(out_buf2.data(), 4102 - 1026) != 4102 - 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
    
            if (out_buf1 != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (out_buf1.substr(1026) != out_buf2) throw std::runtime_error("zlib_cvt::copy constructor incorrect");
        }
    };

    Comp::zlib_cvt obj{make_root_cvt<true>(mem_device("")), 8};
    helper(obj);
    
    Comp::zlib_cvt tmp{make_root_cvt<true>(mem_device("")), 8};
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2);
    dump_info("Done\n");
}

void test_zlib_cvt_gen_3()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt general case 3...");

    auto helper = []<typename T>(T& obj)
    {
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);
            
            T obj2(Comp::zlib_cvt{make_root_cvt<true>(mem_device("")), 8});
            obj2 = obj;
            obj.put(s_e_lit.data() + 1024, 4102 - 1024);
            obj2.put(s_e_lit.data() + 1024, 4102 - 1024);
            
            auto dev1 = obj.detach();
            auto dev2 = obj2.detach();
            
            if (dev1.str() != dev2.str())
                throw std::runtime_error("zlib_cvt copy assignment response incorrect");
            compress_res = dev1.str();
        }
    
        {
            T obj2{Comp::zlib_cvt{make_root_cvt<true>(mem_device(compress_res)), 0}};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            std::string out_buf1; out_buf1.resize(4102);
            std::string out_buf2; out_buf2.resize(4102 - 1026);
    
            if (obj2.get(out_buf1.data(), 1026) != 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
    
            T obj3(Comp::zlib_cvt{make_root_cvt<true>(mem_device("")), 8});
            obj3 = obj2;
            
            if (obj2.get(out_buf1.data() + 1026, 4102 - 1026) != 4102 - 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (obj3.get(out_buf2.data(), 4102 - 1026) != 4102 - 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
    
            if (out_buf1 != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (out_buf1.substr(1026) != out_buf2) throw std::runtime_error("zlib_cvt::copy assignment incorrect");
        }
    };

    Comp::zlib_cvt obj{make_root_cvt<true>(mem_device("")), 8};
    helper(obj);
    
    Comp::zlib_cvt tmp{make_root_cvt<true>(mem_device("")), 8};
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_gen_4()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt general case 4...");

    auto helper = []<typename T>(T& obj)
    {
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);
    
            T obj2(std::move(obj));
            obj2.put(s_e_lit.data() + 1024, 4102 - 1024);
            auto dev = obj2.detach();
            
            compress_res = dev.str();
        }
    
        {
            T obj2{Comp::zlib_cvt{make_root_cvt<true>(mem_device(compress_res)), 0}};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            std::string out_buf; out_buf.resize(4102);
    
            if (obj2.get(out_buf.data(), 1026) != 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
            auto obj3(std::move(obj2));
            
            if (obj3.get(out_buf.data() + 1026, 4102 - 1026) != 4102 - 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
    
            if (out_buf != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
        }
    };

    Comp::zlib_cvt obj{make_root_cvt<true>(mem_device("")), 8};
    helper(obj);
    
    Comp::zlib_cvt tmp{make_root_cvt<true>(mem_device("")), 8};
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_gen_5()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt general case 5...");

    auto helper = []<typename T>(T& obj)
    {
        std::string compress_res;    
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);
    
            T obj2(Comp::zlib_cvt{make_root_cvt<true>(mem_device("")), 8});
            obj2 = std::move(obj);
            obj2.put(s_e_lit.data() + 1024, 4102 - 1024);
            auto dev = obj2.detach();
            
            compress_res = dev.str();
        }
    
        {
            T obj2(Comp::zlib_cvt{make_root_cvt<true>(mem_device(compress_res)), 0});
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            std::string out_buf; out_buf.resize(4102);
    
            if (obj2.get(out_buf.data(), 1026) != 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
            T obj3(Comp::zlib_cvt{make_root_cvt<true>(mem_device("")), 8});
            obj3 = std::move(obj2);
            
            if (obj3.get(out_buf.data() + 1026, 4102 - 1026) != 4102 - 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
    
            if (out_buf != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
        }
    };

    Comp::zlib_cvt obj{make_root_cvt<true>(mem_device("")), 8};
    helper(obj);
    
    Comp::zlib_cvt tmp{make_root_cvt<true>(mem_device("")), 8};
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_bos_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt::bos case 1...");

    auto helper = [](auto& obj)
    {
        const auto& dev = obj.device();
        if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
        obj.main_cont_beg();
        if (dev.str() != "\x78\x9c") throw std::runtime_error("zlib_cvt::put_bos fail");
    };

    using CheckType = Comp::zlib_cvt<root_cvt<mem_device<char>, true>>;
    CheckType obj(make_root_cvt<true>(mem_device("")), 6);
    helper(obj);
    
    CheckType tmp(make_root_cvt<true>(mem_device("")), 6);
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_bos_2()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt::bos case 2...");

    auto helper = [](auto& obj)
    {
        const auto& dev = obj.device();
        if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
        
        char buf[] = "123";
        obj.put(buf, 3);

        obj.main_cont_beg();
        if (dev.str() != "\x78\x9c""123") throw std::runtime_error("zlib_cvt::put_bos fail");
    };

    using CheckType = Comp::zlib_cvt<root_cvt<mem_device<char>, true>>;
    CheckType obj(make_root_cvt<true>(mem_device("")), 6);
    helper(obj);

    CheckType tmp(make_root_cvt<true>(mem_device("")), 6);
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_io_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt io case 1...");

    auto helper = []<typename T>(T& obj)
    {
        size_t buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            char* cur_pos = s_e_lit.data();
            int buffer_id = 0;
            while (cur_pos < s_e_lit.data() + 4102)
            {
                size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], s_e_lit.data() + 4102 - cur_pos);
                obj.put(cur_pos, dest_size);
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
            }
            auto dev = obj.detach();
    
            if (cur_pos != s_e_lit.data() + 4102) throw std::runtime_error("zlib_cvt::put response incorrect");
            compress_res = dev.str();
        }
        
        {
            T obj2{Comp::zlib_cvt{make_root_cvt<true>(mem_device(compress_res)), 0}};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            std::string out_buf; out_buf.resize(4200);
            auto s = obj2.get(out_buf.data(), 4200);
            if (s != 4102) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (out_buf.substr(0, 4102) != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
        }
    };

    Comp::zlib_cvt obj{make_root_cvt<true>(mem_device("")), 8};
    helper(obj);
    
    Comp::zlib_cvt tmp{make_root_cvt<true>(mem_device("")), 8};
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_io_2()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt io case 2...");

    auto helper = []<typename T>(T& obj)
    {
        size_t buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            char* cur_pos = s_e_lit.data();
            int buffer_id = 0;
            while (cur_pos < s_e_lit.data() + 4102)
            {
                size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], s_e_lit.data() + 4102 - cur_pos);
                obj.put(cur_pos, dest_size);
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
            }
            auto dev = obj.detach();
    
            if (cur_pos != s_e_lit.data() + 4102) throw std::runtime_error("zlib_cvt::put response incorrect");
            compress_res = dev.str();
        }
        
        {
            T obj2{Comp::zlib_cvt{make_root_cvt<true>(mem_device(compress_res)), 0}};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            std::string out_buf; out_buf.resize(4200);
            auto s = obj2.get(out_buf.data(), 4200);
            if (s != 4102) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (out_buf.substr(0, 4102) != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
        }
    };

    Comp::zlib_cvt obj{make_root_cvt<true>(mem_device("")), 0};    // no compression
    helper(obj);
    
    Comp::zlib_cvt tmp{make_root_cvt<true>(mem_device("")), 0};
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_io_3()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt io case 3...");

    auto helper = []<typename T>(T& obj)
    {
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 4102);
            auto dev = obj.detach();
    
            compress_res = dev.str();
        }
        
        {
            T obj2{Comp::zlib_cvt{make_root_cvt<true>(mem_device(compress_res)), 0}};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            std::string out_buf; out_buf.resize(4102);
            size_t total_count = 0;
            char* cur_pos = out_buf.data();
            int out_buffer_id = 0;
            while (true)
            {
                size_t dest_size = std::min<size_t>(4102 - total_count, out_buffer_size[out_buffer_id++]);
                if (dest_size == 0) break;
                auto s = obj2.get(cur_pos, dest_size);
                out_buffer_id %= std::size(out_buffer_size);
                cur_pos += s;
                total_count += s;
                if (s == 0) break;
            }
        
            if (cur_pos - out_buf.data() != 4102) throw std::runtime_error("code_cvt<memor y<char>>::get response incorrect");
            if (out_buf.substr(0, 4102) != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
        }
    };

    Comp::zlib_cvt_creator<char> creator{6};
    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj);
    
    auto tmp = creator.create(make_root_cvt<true>(mem_device("")));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_flush_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt::flush case 1...");

    auto helper = []<typename T>(T& obj)
    {
        size_t buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            char* cur_pos = s_e_lit.data();
            int buffer_id = 0;
            while (cur_pos < s_e_lit.data() + 4102)
            {
                size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], s_e_lit.data() + 4102 - cur_pos);
                obj.put(cur_pos, dest_size);
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
            }
            auto dev = obj.detach();
    
            if (cur_pos != s_e_lit.data() + 4102) throw std::runtime_error("zlib_cvt::put response incorrect");
            compress_res = dev.str();
        }
        
        {
            T obj{Comp::zlib_cvt{make_root_cvt<true>(mem_device("")), 8}};
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            char* cur_pos = s_e_lit.data();
            int buffer_id = 0;
            while (cur_pos < s_e_lit.data() + 4102)
            {
                size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], s_e_lit.data() + 4102 - cur_pos);
                obj.put(cur_pos, dest_size);
                obj.flush();
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
            }
            auto dev = obj.detach();
            if (compress_res != dev.str()) throw std::runtime_error("zlib_cvt::flush response incorrect");
        }
    };

    Comp::zlib_cvt_creator<char> creator{8};
    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj);
    
    auto tmp = creator.create(make_root_cvt<true>(mem_device("")));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_flush_2()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt::flush case 2...");

    auto helper = []<typename T>(T& obj)
    {
        size_t buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            char* cur_pos = s_e_lit.data();
            int buffer_id = 0;
            while (cur_pos < s_e_lit.data() + 4102)
            {
                size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], s_e_lit.data() + 4102 - cur_pos);
                obj.put(cur_pos, dest_size);
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
            }
            auto dev = obj.detach();
    
            if (cur_pos != s_e_lit.data() + 4102) throw std::runtime_error("zlib_cvt::put response incorrect");
            compress_res = dev.str();
        }
        
        {
            T obj{Comp::zlib_cvt{make_root_cvt<true>(mem_device("")), 8}};
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
    
            Comp::zlib_sync_flush acc(true);
            obj.adjust(acc);
            
            char* cur_pos = s_e_lit.data();
            int buffer_id = 0;
            while (cur_pos < s_e_lit.data() + 4102)
            {
                size_t ori_dev_size = obj.device().str().size();
                size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], s_e_lit.data() + 4102 - cur_pos);
                obj.put(cur_pos, dest_size);
                
                if (obj.device().str().size() != ori_dev_size) throw std::runtime_error("zlib_cvt::put response incorrect");
                obj.flush();
                if (obj.device().str().size() == ori_dev_size) throw std::runtime_error("zlib_cvt::flush response incorrect");
                
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
            }
            auto dev = obj.detach();
            if (compress_res == dev.str()) throw std::runtime_error("zlib_cvt::flush response incorrect");
            
            compress_res = dev.str();
        }
    
        {
            T obj{Comp::zlib_cvt{make_root_cvt<true>(mem_device(compress_res)), 0}};
            if (obj.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
    
            std::string out_buf; out_buf.resize(4200);
            auto s = obj.get(out_buf.data(), 4200);
            if (s != 4102) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (out_buf.substr(0, 4102) != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
        }
    };

    Comp::zlib_cvt_creator<char> creator{8};
    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj);
    
    auto tmp = creator.create(make_root_cvt<true>(mem_device("")));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_reset_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt::reset case 1...");

    auto helper = []<typename T>(T& obj)
    {
        size_t buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            char* cur_pos = s_e_lit.data();
            int buffer_id = 0;
            while (cur_pos < s_e_lit.data() + 4102)
            {
                size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], s_e_lit.data() + 4102 - cur_pos);
                obj.put(cur_pos, dest_size);
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
            }
            auto dev = obj.attach(mem_device(""));
    
            if (cur_pos != s_e_lit.data() + 4102) throw std::runtime_error("zlib_cvt::put response incorrect");
            compress_res = dev.str();
        }
        
        {
            T obj{Comp::zlib_cvt{make_root_cvt<true>(mem_device("")), 8}};
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            char* cur_pos = s_e_lit.data();
            int buffer_id = 0;
            while (cur_pos < s_e_lit.data() + 4102)
            {
                size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], s_e_lit.data() + 4102 - cur_pos);
                obj.put(cur_pos, dest_size);
                obj.flush();
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
            }
            auto dev = obj.detach();
            if (compress_res != dev.str()) throw std::runtime_error("zlib_cvt::flush response incorrect");
        }
    };

    Comp::zlib_cvt_creator<char> creator{8};
    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj);
    
    auto tmp = creator.create(make_root_cvt<true>(mem_device("")));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}