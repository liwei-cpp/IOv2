#include <cvt/comp/zlib_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <support/dump_info.h>
#include <support/verify.h>

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
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);
            
            auto obj2(obj);
            obj.put(s_e_lit.data() + 1024, 4102 - 1024);
            obj2.put(s_e_lit.data() + 1024, 4102 - 1024);
            
            auto [dev1, err1] = obj.detach();
            auto [dev2, err2] = obj2.detach();

            VERIFY(dev1.str() == dev2.str());
            compress_res = dev1.str();
        }
    
        {
            Comp::zlib_cvt_creator<wchar_t> creator{8};
            T obj2{creator.create(rb_root_cvt{mem_device(compress_res)})};
            VERIFY(obj2.bos() == io_status::input);
            obj2.main_cont_beg();
    
            std::wstring out_buf1; out_buf1.resize(4102);
            std::wstring out_buf2; out_buf2.resize(4102 - 1026);
    
            VERIFY(obj2.get(out_buf1.data(), 1026) == 1026);
            auto obj3(obj2);
            
            VERIFY(obj2.get(out_buf1.data() + 1026, 4102 - 1026) == 4102 - 1026);
            VERIFY(obj3.get(out_buf2.data(), 4102 - 1026) == 4102 - 1026);
    
            VERIFY(out_buf1 == s_e_lit);
            VERIFY(out_buf1.substr(1026) == out_buf2);
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    helper(obj);

    runtime_cvt obj2{creator.create(rb_root_cvt{mem_device("")})};
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
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);
            
            T obj2 = creator.create(rb_root_cvt{mem_device("")});
            obj2 = obj;
            obj.put(s_e_lit.data() + 1024, 4102 - 1024);
            obj2.put(s_e_lit.data() + 1024, 4102 - 1024);
            
            auto [dev1, err1] = obj.detach();
            auto [dev2, err2] = obj2.detach();

            VERIFY(dev1.str() == dev2.str());
            compress_res = dev1.str();
        }
    
        {
            T obj2{creator.create(rb_root_cvt{mem_device(compress_res)})};
            VERIFY(obj2.bos() == io_status::input);
            obj2.main_cont_beg();
    
            std::wstring out_buf1; out_buf1.resize(4102);
            std::wstring out_buf2; out_buf2.resize(4102 - 1026);
    
            VERIFY(obj2.get(out_buf1.data(), 1026) == 1026);
    
            T obj3{creator.create(rb_root_cvt{mem_device("")})};
            obj3 = obj2;
            
            VERIFY(obj2.get(out_buf1.data() + 1026, 4102 - 1026) == 4102 - 1026);
            VERIFY(obj3.get(out_buf2.data(), 4102 - 1026) == 4102 - 1026);

            VERIFY(out_buf1 == s_e_lit);
            VERIFY(out_buf1.substr(1026) == out_buf2);
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    helper(obj);
    
    runtime_cvt obj2{creator.create(rb_root_cvt{mem_device("")})};
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
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);
    
            T obj2(std::move(obj));
            obj2.put(s_e_lit.data() + 1024, 4102 - 1024);
            auto [dev, err] = obj2.detach();

            compress_res = dev.str();
        }
    
        {
            T obj2{creator.create(rb_root_cvt{mem_device(compress_res)})};
            VERIFY(obj2.bos() == io_status::input);
            obj2.main_cont_beg();
    
            std::wstring out_buf; out_buf.resize(4102);
    
            VERIFY(obj2.get(out_buf.data(), 1026) == 1026);
            auto obj3(std::move(obj2));
            
            VERIFY(obj3.get(out_buf.data() + 1026, 4102 - 1026) == 4102 - 1026);
    
            VERIFY(out_buf == s_e_lit);
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    helper(obj);
    
    runtime_cvt obj2{creator.create(rb_root_cvt{mem_device("")})};
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
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);

            T obj2{creator.create(rb_root_cvt{mem_device("")})};
            obj2 = std::move(obj);
            obj2.put(s_e_lit.data() + 1024, 4102 - 1024);
            auto [dev, err] = obj2.detach();

            compress_res = dev.str();
        }
    
        {
            T obj2{creator.create(rb_root_cvt{mem_device(compress_res)})};
            VERIFY(obj2.bos() == io_status::input);
            obj2.main_cont_beg();
    
            std::wstring out_buf; out_buf.resize(4102);
    
            VERIFY(obj2.get(out_buf.data(), 1026) == 1026);
            T obj3{creator.create(rb_root_cvt{mem_device("")})};
            obj3 = std::move(obj2);
            
            VERIFY(obj3.get(out_buf.data() + 1026, 4102 - 1026) == 4102 - 1026);
    
            VERIFY(out_buf == s_e_lit);
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    helper(obj);
    
    runtime_cvt obj2{creator.create(rb_root_cvt{mem_device("")})};
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        VERIFY(dev.str() == "\x78\x9c");
    };

    Comp::zlib_cvt_creator<wchar_t> creator{6};
    auto obj(creator.create(rb_root_cvt{mem_device("")}));
    helper(obj);
    
    runtime_cvt obj2{creator.create(rb_root_cvt{mem_device("")})};
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
        VERIFY(obj.bos() == io_status::output);

        wchar_t buf[] = L"uvw";
        obj.put(buf, 3);

        obj.main_cont_beg();
        VERIFY(dev.str() == std::string("\x78\x9c""u\x00\x00\x00v\x00\x00\x00w\x00\x00\x00", 14));
    };

    Comp::zlib_cvt_creator<wchar_t> creator{6};
    auto obj(creator.create(rb_root_cvt{mem_device("")}));
    helper(obj);

    runtime_cvt obj2{creator.create(rb_root_cvt{mem_device("")})};
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
            VERIFY(obj.bos() == io_status::output);
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
            auto [dev, err] = obj.detach();

            VERIFY(cur_pos == s_e_lit.data() + 4102);
            compress_res = dev.str();
        }
        
        {
            T obj2{creator.create(rb_root_cvt{mem_device(compress_res)})};
            VERIFY(obj2.bos() == io_status::input);
            obj2.main_cont_beg();
    
            std::wstring out_buf; out_buf.resize(4200);
            auto s = obj2.get(out_buf.data(), 4200);
            VERIFY(s == 4102);
            VERIFY(out_buf.substr(0, 4102) == s_e_lit);
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj(creator.create(rb_root_cvt{mem_device("")}));
    helper(obj);
    
    runtime_cvt obj2{creator.create(rb_root_cvt{mem_device("")})};
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
            VERIFY(obj.bos() == io_status::output);
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
            auto [dev, err] = obj.detach();

            VERIFY(cur_pos == s_e_lit.data() + 4102);
            compress_res = dev.str();
        }
        
        {
            T obj2{creator.create(rb_root_cvt{mem_device(compress_res)})};
            VERIFY(obj2.bos() == io_status::input);
            obj2.main_cont_beg();
    
            std::wstring out_buf; out_buf.resize(4200);
            auto s = obj2.get(out_buf.data(), 4200);
            VERIFY(s == 4102);
            VERIFY(out_buf.substr(0, 4102) == s_e_lit);
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{0};         // no compression
    auto obj(creator.create(rb_root_cvt{mem_device("")}));
    helper(obj);
    
    runtime_cvt obj2{creator.create(rb_root_cvt{mem_device("")})};
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
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 4102);
            auto [dev, err] = obj.detach();

            compress_res = dev.str();
        }
        
        {
            Comp::zlib_cvt_creator<wchar_t> creator{0};
            T obj2{creator.create(rb_root_cvt{mem_device(compress_res)})};
            VERIFY(obj2.bos() == io_status::input);
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
        
            VERIFY(cur_pos - out_buf.data() == 4102);
            VERIFY(out_buf.substr(0, 4102) == s_e_lit);
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{6};
    auto obj(creator.create(rb_root_cvt{mem_device("")}));
    helper(obj);
    
    runtime_cvt obj2{creator.create(rb_root_cvt{mem_device("")})};
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
            VERIFY(obj.bos() == io_status::output);
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
            auto [dev, err] = obj.detach();

            VERIFY(cur_pos == s_e_lit.data() + 4102);
            compress_res = dev.str();
        }
        
        {
            Comp::zlib_cvt_creator<wchar_t> creator{8};
            T local_obj(creator.create(rb_root_cvt{mem_device("")}));
            VERIFY(local_obj.bos() == io_status::output);
            local_obj.main_cont_beg();
            
            wchar_t* cur_pos = s_e_lit.data();
            int buffer_id = 0;
            while (cur_pos < s_e_lit.data() + 4102)
            {
                size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], s_e_lit.data() + 4102 - cur_pos);
                local_obj.put(cur_pos, dest_size);
                local_obj.flush();
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
            }
            auto [dev, err] = local_obj.detach();
            VERIFY(compress_res == dev.str());
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj(creator.create(rb_root_cvt{mem_device("")}));
    helper(obj);

    runtime_cvt obj2{creator.create(rb_root_cvt{mem_device("")})};
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
            VERIFY(obj.bos() == io_status::output);
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
            auto [dev, err] = obj.detach();

            VERIFY(cur_pos == s_e_lit.data() + 4102);
            compress_res = dev.str();
        }
        
        {
            Comp::zlib_cvt_creator<wchar_t> creator{8};
            T local_obj(creator.create(rb_root_cvt{mem_device("")}));
            VERIFY(local_obj.bos() == io_status::output);
            local_obj.main_cont_beg();
    
            Comp::zlib_sync_flush acc(true);
            local_obj.adjust(acc);
            
            wchar_t* cur_pos = s_e_lit.data();
            int buffer_id = 0;
            while (cur_pos < s_e_lit.data() + 4102)
            {
                size_t ori_dev_size = local_obj.device().str().size();
                size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], s_e_lit.data() + 4102 - cur_pos);
                local_obj.put(cur_pos, dest_size);
                
                VERIFY(local_obj.device().str().size() == ori_dev_size);
                local_obj.flush();
                VERIFY(local_obj.device().str().size() != ori_dev_size);
                
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
            }
            auto [dev, err] = local_obj.detach();
            VERIFY(compress_res != dev.str());
            
            compress_res = dev.str();
        }
    
        {
            Comp::zlib_cvt_creator<wchar_t> creator{0};
            T local_obj(creator.create(rb_root_cvt{mem_device(compress_res)}));
            VERIFY(local_obj.bos() == io_status::input);
            local_obj.main_cont_beg();
    
            std::wstring out_buf; out_buf.resize(4200);
            auto s = local_obj.get(out_buf.data(), 4200);
            VERIFY(s == 4102);
            VERIFY(out_buf.substr(0, 4102) == s_e_lit);
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj(creator.create(rb_root_cvt{mem_device("")}));
    helper(obj);
    
    runtime_cvt obj2{creator.create(rb_root_cvt{mem_device("")})};
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
            VERIFY(obj.bos() == io_status::output);
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
            auto [dev, err] = obj.detach();
            obj.attach(mem_device(""));

            VERIFY(cur_pos == s_e_lit.data() + 4102);
            compress_res = dev.str();
        }
        
        {
            Comp::zlib_cvt_creator<wchar_t> creator{8};
            T local_obj(creator.create(rb_root_cvt{mem_device("")}));
            VERIFY(local_obj.bos() == io_status::output);
            local_obj.main_cont_beg();
            
            wchar_t* cur_pos = s_e_lit.data();
            int buffer_id = 0;
            while (cur_pos < s_e_lit.data() + 4102)
            {
                size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], s_e_lit.data() + 4102 - cur_pos);
                local_obj.put(cur_pos, dest_size);
                local_obj.flush();
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
            }
            auto [dev, err] = local_obj.detach();
            VERIFY(compress_res == dev.str());
        }
    };

    Comp::zlib_cvt_creator<wchar_t> creator{8};
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    helper(obj);

    auto tmp = creator.create(rb_root_cvt{mem_device("")});
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_gen_6()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t> gen 6: level clamp, adjust base behavior, self-assignment...");

    // compression level > 9 is silently clamped to 9
    {
        Comp::zlib_cvt<rb_root_cvt<mem_device<char>>, wchar_t> obj{rb_root_cvt{mem_device("")}, 15};
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        wchar_t data[] = L"hi";
        obj.put(data, 2);
        obj.detach();
    }

    // adjust() with base cvt_behavior: dynamic_cast returns nullptr, only BT::adjust() is called
    {
        Comp::zlib_cvt<rb_root_cvt<mem_device<char>>, wchar_t> obj{rb_root_cvt{mem_device("")}, 6};
        obj.bos();
        obj.main_cont_beg();
        cvt_behavior base_acc;
        obj.adjust(base_acc);
        obj.detach();
    }

    // self-assignment in copy-assignment operator: 'this == &val' guard
    {
        Comp::zlib_cvt<rb_root_cvt<mem_device<char>>, wchar_t> obj{rb_root_cvt{mem_device("")}, 6};
        obj.bos();
        obj.main_cont_beg();
        wchar_t data[] = L"abc";
        obj.put(data, 3);
        const auto& const_obj = obj;
        obj = const_obj;
        obj.detach();
    }

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_error_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t> error paths: truncated stream...");

    // Truncated stream: only the 2-byte zlib header, no compressed payload.
    {
        std::string just_header("\x78\x9c", 2);
        Comp::zlib_cvt<rb_root_cvt<mem_device<char>>, wchar_t> obj{rb_root_cvt{mem_device(just_header)}, 6};
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        wchar_t buf[16] = {};
        bool threw = false;
        try { obj.get(buf, 16); }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}

void test_zlib_cvt_wchar_t_eof_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt<wchar_t> is_eof and m_stream_ended early-return...");

    // compress a small wstring
    std::string compressed;
    {
        Comp::zlib_cvt<rb_root_cvt<mem_device<char>>, wchar_t> comp{rb_root_cvt{mem_device("")}, 6};
        comp.bos();
        comp.main_cont_beg();
        wchar_t data[] = L"hi";
        comp.put(data, 2);
        auto [dev, err] = comp.detach();
        if (err) std::rethrow_exception(err);
        compressed = dev.str();
    }

    // decompress with a buffer larger than the payload
    {
        Comp::zlib_cvt<rb_root_cvt<mem_device<char>>, wchar_t> decomp{rb_root_cvt{mem_device(compressed)}, 6};
        VERIFY(decomp.bos() == io_status::input);
        decomp.main_cont_beg();

        wchar_t buf[64] = {};
        auto n = decomp.get(buf, 64);
        VERIFY(n == 2);

        VERIFY(decomp.is_eof());

        auto n2 = decomp.get(buf, 64);
        VERIFY(n2 == 0);
    }

    dump_info("Done\n");
}