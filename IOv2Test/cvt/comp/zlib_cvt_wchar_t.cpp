#include <cvt/comp/zlib_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <common/dump_info.h>

namespace
{
    std::wstring s_e_lit = []()
    {
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
        return e_lit;
    }();
}

void test_zlib_cvt_wchar_t_gen_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t> general case 1...");
    
    {
        using CheckType = Comp::zlib_cvt<root_cvt<mem_device<char>, true>, wchar_t>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
        static_assert(std::is_same_v<CheckType::external_type, char>);

        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = Comp::zlib_cvt<root_cvt<mem_device<char8_t>, false>, wchar_t>;
        static_assert(io_converter<CheckType>);
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

void test_zlib_cvt_wchar_t_gen_2()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t> general case 2...");
    
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
            Comp::zlib_cvt_creator<wchar_t> creator{8};
            T obj2{creator.create(make_root_cvt<true>(mem_device(compress_res)))};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            std::wstring out_buf1; out_buf1.resize(4102);
            std::wstring out_buf2; out_buf2.resize(4102 - 1026);
    
            if (obj2.get(out_buf1.data(), 1026) != 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
            auto obj3(obj2);
            
            if (obj2.get(out_buf1.data() + 1026, 4102 - 1026) != 4102 - 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (obj3.get(out_buf2.data(), 4102 - 1026) != 4102 - 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
    
            if (out_buf1 != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (out_buf1.substr(1026) != out_buf2) throw std::runtime_error("zlib_cvt::copy constructor incorrect");
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj);

    runtime_cvt obj2{creator.create(make_root_cvt<true>(mem_device("")))};
    helper(obj2);
    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_gen_3()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t> general case 3...");

    auto helper = []<typename T>(T& obj)
    {
        Comp::zlib_cvt_creator<wchar_t> creator{8};
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);
            
            T obj2 = creator.create(make_root_cvt<true>(mem_device("")));
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
            T obj2{creator.create(make_root_cvt<true>(mem_device(compress_res)))};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            std::wstring out_buf1; out_buf1.resize(4102);
            std::wstring out_buf2; out_buf2.resize(4102 - 1026);
    
            if (obj2.get(out_buf1.data(), 1026) != 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
    
            T obj3{creator.create(make_root_cvt<true>(mem_device("")))};
            obj3 = obj2;
            
            if (obj2.get(out_buf1.data() + 1026, 4102 - 1026) != 4102 - 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (obj3.get(out_buf2.data(), 4102 - 1026) != 4102 - 1026) throw std::runtime_error("zlib_cvt::get response incorrect");

            if (out_buf1 != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (out_buf1.substr(1026) != out_buf2) throw std::runtime_error("zlib_cvt::copy assignment incorrect");
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj);
    
    runtime_cvt obj2{creator.create(make_root_cvt<true>(mem_device("")))};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_gen_4()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t> general case 4...");

    auto helper = []<typename T>(T& obj)
    {
        Comp::zlib_cvt_creator<wchar_t> creator{8};
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
            T obj2{creator.create(make_root_cvt<true>(mem_device(compress_res)))};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            std::wstring out_buf; out_buf.resize(4102);
    
            if (obj2.get(out_buf.data(), 1026) != 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
            auto obj3(std::move(obj2));
            
            if (obj3.get(out_buf.data() + 1026, 4102 - 1026) != 4102 - 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
    
            if (out_buf != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj);
    
    runtime_cvt obj2{creator.create(make_root_cvt<true>(mem_device("")))};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_gen_5()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t> general case 5...");

    auto helper = []<typename T>(T& obj)
    {
        Comp::zlib_cvt_creator<wchar_t> creator{8};
        std::string compress_res;    
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);

            T obj2{creator.create(make_root_cvt<true>(mem_device("")))};
            obj2 = std::move(obj);
            obj2.put(s_e_lit.data() + 1024, 4102 - 1024);
            auto dev = obj2.detach();
            
            compress_res = dev.str();
        }
    
        {
            T obj2{creator.create(make_root_cvt<true>(mem_device(compress_res)))};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            std::wstring out_buf; out_buf.resize(4102);
    
            if (obj2.get(out_buf.data(), 1026) != 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
            T obj3{creator.create(make_root_cvt<true>(mem_device("")))};
            obj3 = std::move(obj2);
            
            if (obj3.get(out_buf.data() + 1026, 4102 - 1026) != 4102 - 1026) throw std::runtime_error("zlib_cvt::get response incorrect");
    
            if (out_buf != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj);
    
    runtime_cvt obj2{creator.create(make_root_cvt<true>(mem_device("")))};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_bos_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t>::bos case 1...");

    auto helper = [](auto& obj)
    {
        const auto& dev = obj.device();
        if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
        obj.main_cont_beg();
        if (dev.str() != "\x78\x9c") throw std::runtime_error("zlib_cvt::put_bos fail");
    };

    Comp::zlib_cvt_creator<wchar_t> creator{6};
    auto obj(creator.create(make_root_cvt<true>(mem_device(""))));
    helper(obj);
    
    runtime_cvt obj2{creator.create(make_root_cvt<true>(mem_device("")))};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_bos_2()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t>::bos case 2...");

    auto helper = [](auto& obj)
    {
        const auto& dev = obj.device();
        if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
        
        wchar_t buf[] = L"uvw";
        obj.put(buf, 3);

        obj.main_cont_beg();
        if (dev.str() != std::string("\x78\x9c""u\x00\x00\x00v\x00\x00\x00w\x00\x00\x00", 14)) throw std::runtime_error("zlib_cvt::put_bos fail");
    };

    Comp::zlib_cvt_creator<wchar_t> creator{6};
    auto obj(creator.create(make_root_cvt<true>(mem_device(""))));
    helper(obj);

    runtime_cvt obj2{creator.create(make_root_cvt<true>(mem_device("")))};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_io_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t> io case 1...");

    auto helper = []<typename T>(T& obj)
    {
        Comp::zlib_cvt_creator<wchar_t> creator{8};
        size_t buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            wchar_t* cur_pos = s_e_lit.data();
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
            T obj2{creator.create(make_root_cvt<true>(mem_device(compress_res)))};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            std::wstring out_buf; out_buf.resize(4200);
            auto s = obj2.get(out_buf.data(), 4200);
            if (s != 4102) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (out_buf.substr(0, 4102) != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj(creator.create(make_root_cvt<true>(mem_device(""))));
    helper(obj);
    
    runtime_cvt obj2{creator.create(make_root_cvt<true>(mem_device("")))};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_io_2()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t> io case 2...");

    auto helper = []<typename T>(T& obj)
    {
        Comp::zlib_cvt_creator<wchar_t> creator{0};
        size_t buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            wchar_t* cur_pos = s_e_lit.data();
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
            T obj2{creator.create(make_root_cvt<true>(mem_device(compress_res)))};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            std::wstring out_buf; out_buf.resize(4200);
            auto s = obj2.get(out_buf.data(), 4200);
            if (s != 4102) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (out_buf.substr(0, 4102) != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{0};         // no compression
    auto obj(creator.create(make_root_cvt<true>(mem_device(""))));
    helper(obj);
    
    runtime_cvt obj2{creator.create(make_root_cvt<true>(mem_device("")))};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_io_3()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t> io case 3...");

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
            Comp::zlib_cvt_creator<wchar_t> creator{0};
            T obj2{creator.create(make_root_cvt<true>(mem_device(compress_res)))};
            if (obj2.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj2.main_cont_beg();
    
            size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            std::wstring out_buf; out_buf.resize(4102);
            size_t total_count = 0;
            wchar_t* cur_pos = out_buf.data();
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

    Comp::zlib_cvt_creator<wchar_t> creator{6};
    auto obj(creator.create(make_root_cvt<true>(mem_device(""))));
    helper(obj);
    
    runtime_cvt obj2{creator.create(make_root_cvt<true>(mem_device("")))};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_flush_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t>::flush case 1...");

    auto helper = []<typename T>(T& obj)
    {
        size_t buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            wchar_t* cur_pos = s_e_lit.data();
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
            Comp::zlib_cvt_creator<wchar_t> creator{8};
            T obj(creator.create(make_root_cvt<true>(mem_device(""))));
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            wchar_t* cur_pos = s_e_lit.data();
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

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj(creator.create(make_root_cvt<true>(mem_device(""))));
    helper(obj);

    runtime_cvt obj2{creator.create(make_root_cvt<true>(mem_device("")))};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_flush_2()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t>::flush case 2...");

    auto helper = []<typename T>(T& obj)
    {
        size_t buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};
        std::string compress_res;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            wchar_t* cur_pos = s_e_lit.data();
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
            Comp::zlib_cvt_creator<wchar_t> creator{8};
            T obj(creator.create(make_root_cvt<true>(mem_device(""))));
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
    
            Comp::zlib_sync_flush acc(true);
            obj.adjust(acc);
            
            wchar_t* cur_pos = s_e_lit.data();
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
            Comp::zlib_cvt_creator<wchar_t> creator{0};
            T obj(creator.create(make_root_cvt<true>(mem_device(compress_res))));
            if (obj.bos() != io_status::input) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
    
            std::wstring out_buf; out_buf.resize(4200);
            auto s = obj.get(out_buf.data(), 4200);
            if (s != 4102) throw std::runtime_error("zlib_cvt::get response incorrect");
            if (out_buf.substr(0, 4102) != s_e_lit) throw std::runtime_error("zlib_cvt::get response incorrect");
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj(creator.create(make_root_cvt<true>(mem_device(""))));
    helper(obj);
    
    runtime_cvt obj2{creator.create(make_root_cvt<true>(mem_device("")))};
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_reset_1()
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
            
            wchar_t* cur_pos = s_e_lit.data();
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
            Comp::zlib_cvt_creator<wchar_t> creator{8};
            T obj(creator.create(make_root_cvt<true>(mem_device(""))));
            if (obj.bos() != io_status::output) throw std::runtime_error("zlib_cvt::bos response incorrect");
            obj.main_cont_beg();
            
            wchar_t* cur_pos = s_e_lit.data();
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

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj);
    
    auto tmp = creator.create(make_root_cvt<true>(mem_device("")));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}