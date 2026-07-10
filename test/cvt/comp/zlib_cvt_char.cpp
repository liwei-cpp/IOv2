#include <cvt/comp/zlib_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <support/dump_info.h>
#include <support/verify.h>

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
        using CheckType = Comp::zlib_cvt<rb_root_cvt<mem_device<char>>>;
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
        using CheckType = Comp::zlib_cvt<no_rb_root_cvt<mem_device<char8_t>>>;
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
            T obj2{Comp::zlib_cvt{rb_root_cvt{mem_device(compress_res)}, 0}};
            VERIFY(obj2.bos() == io_status::input);
            obj2.main_cont_beg();
    
            std::string out_buf1; out_buf1.resize(4102);
            std::string out_buf2; out_buf2.resize(4102 - 1026);
    
            VERIFY(obj2.get(out_buf1.data(), 1026) == 1026);
            auto obj3(obj2);
            
            VERIFY(obj2.get(out_buf1.data() + 1026, 4102 - 1026) == 4102 - 1026);
            VERIFY(obj3.get(out_buf2.data(), 4102 - 1026) == 4102 - 1026);
    
            VERIFY(out_buf1 == s_e_lit);
            VERIFY(out_buf1.substr(1026) == out_buf2);
        }
    };

    Comp::zlib_cvt obj{rb_root_cvt{mem_device("")}, 8};
    helper(obj);
    
    Comp::zlib_cvt tmp{rb_root_cvt{mem_device("")}, 8};
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
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);
            
            T obj2(Comp::zlib_cvt{rb_root_cvt{mem_device("")}, 8});
            obj2 = obj;
            obj.put(s_e_lit.data() + 1024, 4102 - 1024);
            obj2.put(s_e_lit.data() + 1024, 4102 - 1024);
            
            auto [dev1, err1] = obj.detach();
            auto [dev2, err2] = obj2.detach();
            
            VERIFY(dev1.str() == dev2.str());
            compress_res = dev1.str();
        }
    
        {
            T obj2{Comp::zlib_cvt{rb_root_cvt{mem_device(compress_res)}, 0}};
            VERIFY(obj2.bos() == io_status::input);
            obj2.main_cont_beg();
    
            std::string out_buf1; out_buf1.resize(4102);
            std::string out_buf2; out_buf2.resize(4102 - 1026);
    
            VERIFY(obj2.get(out_buf1.data(), 1026) == 1026);
    
            T obj3(Comp::zlib_cvt{rb_root_cvt{mem_device("")}, 8});
            obj3 = obj2;
            
            VERIFY(obj2.get(out_buf1.data() + 1026, 4102 - 1026) == 4102 - 1026);
            VERIFY(obj3.get(out_buf2.data(), 4102 - 1026) == 4102 - 1026);
    
            VERIFY(out_buf1 == s_e_lit);
            VERIFY(out_buf1.substr(1026) == out_buf2);
        }
    };

    Comp::zlib_cvt obj{rb_root_cvt{mem_device("")}, 8};
    helper(obj);
    
    Comp::zlib_cvt tmp{rb_root_cvt{mem_device("")}, 8};
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
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);
    
            T obj2(std::move(obj));
            obj2.put(s_e_lit.data() + 1024, 4102 - 1024);
            auto [dev, err] = obj2.detach();

            compress_res = dev.str();
        }
    
        {
            T obj2{Comp::zlib_cvt{rb_root_cvt{mem_device(compress_res)}, 0}};
            VERIFY(obj2.bos() == io_status::input);
            obj2.main_cont_beg();
    
            std::string out_buf; out_buf.resize(4102);
    
            VERIFY(obj2.get(out_buf.data(), 1026) == 1026);
            auto obj3(std::move(obj2));
            
            VERIFY(obj3.get(out_buf.data() + 1026, 4102 - 1026) == 4102 - 1026);
    
            VERIFY(out_buf == s_e_lit);
        }
    };

    Comp::zlib_cvt obj{rb_root_cvt{mem_device("")}, 8};
    helper(obj);
    
    Comp::zlib_cvt tmp{rb_root_cvt{mem_device("")}, 8};
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
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);
    
            T obj2(Comp::zlib_cvt{rb_root_cvt{mem_device("")}, 8});
            obj2 = std::move(obj);
            obj2.put(s_e_lit.data() + 1024, 4102 - 1024);
            auto [dev, err] = obj2.detach();

            compress_res = dev.str();
        }
    
        {
            T obj2(Comp::zlib_cvt{rb_root_cvt{mem_device(compress_res)}, 0});
            VERIFY(obj2.bos() == io_status::input);
            obj2.main_cont_beg();
    
            std::string out_buf; out_buf.resize(4102);
    
            VERIFY(obj2.get(out_buf.data(), 1026) == 1026);
            T obj3(Comp::zlib_cvt{rb_root_cvt{mem_device("")}, 8});
            obj3 = std::move(obj2);
            
            VERIFY(obj3.get(out_buf.data() + 1026, 4102 - 1026) == 4102 - 1026);
    
            VERIFY(out_buf == s_e_lit);
        }
    };

    Comp::zlib_cvt obj{rb_root_cvt{mem_device("")}, 8};
    helper(obj);
    
    Comp::zlib_cvt tmp{rb_root_cvt{mem_device("")}, 8};
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        VERIFY(dev.str() == "\x78\x9c");
    };

    using CheckType = Comp::zlib_cvt<rb_root_cvt<mem_device<char>>>;
    CheckType obj(rb_root_cvt{mem_device("")}, 6);
    helper(obj);
    
    CheckType tmp(rb_root_cvt{mem_device("")}, 6);
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
        VERIFY(obj.bos() == io_status::output);

        char buf[] = "123";
        obj.put(buf, 3);

        obj.main_cont_beg();
        VERIFY(dev.str() == "\x78\x9c""123");
    };

    using CheckType = Comp::zlib_cvt<rb_root_cvt<mem_device<char>>>;
    CheckType obj(rb_root_cvt{mem_device("")}, 6);
    helper(obj);

    CheckType tmp(rb_root_cvt{mem_device("")}, 6);
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_bos_3()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt::bos failure on malformed input header...");

    auto helper = [](auto& obj)
    {
        bool got_exception = false;
        try { obj.bos(); }
        catch (...) { got_exception = true; }
        VERIFY(got_exception);
    };

    // 0xFF 0xFF: invalid zlib header (CM bits = 15; only 8 / deflate is legal).
    // inflate returns Z_DATA_ERROR before producing output, so bos() unwinds while
    // m_strm.next_in still points into the dying header_buf stack frame.
    // inflate_guard's destructor must scrub that pointer; this test exercises the
    // failure path so any future regression that reads stale m_strm fields during
    // teardown is caught here.
    std::string malformed("\xFF\xFF", 2);

    {
        Comp::zlib_cvt obj{rb_root_cvt{mem_device(malformed)}, 6};
        helper(obj);
    }   // dtor runs here; must complete cleanly with no read of stale m_strm.next_in.

    {
        Comp::zlib_cvt tmp{rb_root_cvt{mem_device(malformed)}, 6};
        runtime_cvt obj2(std::move(tmp));
        helper(obj2);
    }

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
            VERIFY(obj.bos() == io_status::output);
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
            auto [dev, err] = obj.detach();

            VERIFY(cur_pos == s_e_lit.data() + 4102);
            compress_res = dev.str();
        }
        
        {
            T obj2{Comp::zlib_cvt{rb_root_cvt{mem_device(compress_res)}, 0}};
            VERIFY(obj2.bos() == io_status::input);
            obj2.main_cont_beg();
    
            std::string out_buf; out_buf.resize(4200);
            auto s = obj2.get(out_buf.data(), 4200);
            VERIFY(s == 4102);
            VERIFY(out_buf.substr(0, 4102) == s_e_lit);
        }
    };

    Comp::zlib_cvt obj{rb_root_cvt{mem_device("")}, 8};
    helper(obj);
    
    Comp::zlib_cvt tmp{rb_root_cvt{mem_device("")}, 8};
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
            VERIFY(obj.bos() == io_status::output);
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
            auto [dev, err] = obj.detach();

            VERIFY(cur_pos == s_e_lit.data() + 4102);
            compress_res = dev.str();
        }
        
        {
            T obj2{Comp::zlib_cvt{rb_root_cvt{mem_device(compress_res)}, 0}};
            VERIFY(obj2.bos() == io_status::input);
            obj2.main_cont_beg();
    
            std::string out_buf; out_buf.resize(4200);
            auto s = obj2.get(out_buf.data(), 4200);
            VERIFY(s == 4102);
            VERIFY(out_buf.substr(0, 4102) == s_e_lit);
        }
    };

    Comp::zlib_cvt obj{rb_root_cvt{mem_device("")}, 0};    // no compression
    helper(obj);
    
    Comp::zlib_cvt tmp{rb_root_cvt{mem_device("")}, 0};
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
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 4102);
            auto [dev, err] = obj.detach();

            compress_res = dev.str();
        }
        
        {
            T obj2{Comp::zlib_cvt{rb_root_cvt{mem_device(compress_res)}, 0}};
            VERIFY(obj2.bos() == io_status::input);
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
        
            VERIFY(cur_pos - out_buf.data() == 4102);
            VERIFY(out_buf.substr(0, 4102) == s_e_lit);
        }
    };

    Comp::zlib_cvt_creator<char> creator{6};
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    helper(obj);
    
    auto tmp = creator.create(rb_root_cvt{mem_device("")});
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
            VERIFY(obj.bos() == io_status::output);
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
            auto [dev, err] = obj.detach();

            VERIFY(cur_pos == s_e_lit.data() + 4102);
            compress_res = dev.str();
        }
        
        {
            T local_obj{Comp::zlib_cvt{rb_root_cvt{mem_device("")}, 8}};
            VERIFY(local_obj.bos() == io_status::output);
            local_obj.main_cont_beg();
            
            char* cur_pos = s_e_lit.data();
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

    Comp::zlib_cvt_creator<char> creator{8};
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    helper(obj);
    
    auto tmp = creator.create(rb_root_cvt{mem_device("")});
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
            VERIFY(obj.bos() == io_status::output);
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
            auto [dev, err] = obj.detach();

            VERIFY(cur_pos == s_e_lit.data() + 4102);
            compress_res = dev.str();
        }
        
        {
            T local_obj{Comp::zlib_cvt{rb_root_cvt{mem_device("")}, 8}};
            VERIFY(local_obj.bos() == io_status::output);
            local_obj.main_cont_beg();
    
            Comp::zlib_sync_flush acc(true);
            local_obj.adjust(acc);
            
            char* cur_pos = s_e_lit.data();
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
            T local_obj{Comp::zlib_cvt{rb_root_cvt{mem_device(compress_res)}, 0}};
            VERIFY(local_obj.bos() == io_status::input);
            local_obj.main_cont_beg();
    
            std::string out_buf; out_buf.resize(4200);
            auto s = local_obj.get(out_buf.data(), 4200);
            VERIFY(s == 4102);
            VERIFY(out_buf.substr(0, 4102) == s_e_lit);
        }
    };

    Comp::zlib_cvt_creator<char> creator{8};
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    helper(obj);
    
    auto tmp = creator.create(rb_root_cvt{mem_device("")});
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
            VERIFY(obj.bos() == io_status::output);
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
            auto [dev, err] = obj.detach();
            obj.attach(mem_device(""));

            VERIFY(cur_pos == s_e_lit.data() + 4102);
            compress_res = dev.str();
        }

        {
            T local_obj{Comp::zlib_cvt{rb_root_cvt{mem_device("")}, 8}};
            VERIFY(local_obj.bos() == io_status::output);
            local_obj.main_cont_beg();

            char* cur_pos = s_e_lit.data();
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

    Comp::zlib_cvt_creator<char> creator{8};
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    helper(obj);

    auto tmp = creator.create(rb_root_cvt{mem_device("")});
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_zlib_cvt_gen_6()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt gen 6: level clamp, adjust base behavior, self-assignment...");

    // compression level > 9 is silently clamped to 9
    {
        Comp::zlib_cvt obj{rb_root_cvt{mem_device("")}, 15};
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        char data[] = "test data";
        obj.put(data, 9);
        obj.detach();
    }

    // adjust() with base cvt_behavior: dynamic_cast returns nullptr, only BT::adjust() is called
    {
        Comp::zlib_cvt obj{rb_root_cvt{mem_device("")}, 6};
        obj.bos();
        obj.main_cont_beg();
        cvt_behavior base_acc;
        obj.adjust(base_acc);
        obj.detach();
    }

    // self-assignment in copy-assignment operator: 'this == &val' guard → return *this immediately
    {
        Comp::zlib_cvt obj{rb_root_cvt{mem_device("")}, 6};
        obj.bos();
        obj.main_cont_beg();
        char data[] = "abc";
        obj.put(data, 3);
        const auto& const_obj = obj;
        obj = const_obj;
        obj.detach();
    }

    dump_info("Done\n");
}

void test_zlib_cvt_error_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt error paths: truncated stream...");

    // Truncated stream: only the 2-byte zlib header "\x78\x9c", no compressed payload.
    // bos() consumes the header successfully; get() finds no more input and throws
    // "compressed stream truncated" (ret != Z_STREAM_END && avail_out != 0 after reader
    // returns len == 0).
    {
        std::string just_header("\x78\x9c", 2);
        Comp::zlib_cvt obj{rb_root_cvt{mem_device(just_header)}, 6};
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        char buf[16] = {};
        bool threw = false;
        try { obj.get(buf, 16); }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}

void test_zlib_cvt_eof_1()
{
    using namespace IOv2;
    dump_info("Test zlib_cvt is_eof and m_stream_ended early-return...");

    // compress a small string
    std::string compressed;
    {
        Comp::zlib_cvt comp{rb_root_cvt{mem_device("")}, 6};
        comp.bos();
        comp.main_cont_beg();
        char data[] = "hello";
        comp.put(data, 5);
        auto [dev, err] = comp.detach();
        if (err) std::rethrow_exception(err);
        compressed = dev.str();
    }

    // decompress with a buffer larger than the payload
    {
        Comp::zlib_cvt decomp{rb_root_cvt{mem_device(compressed)}, 6};
        VERIFY(decomp.bos() == io_status::input);
        decomp.main_cont_beg();

        char buf[64] = {};
        // request 64 bytes: inflate will produce 5 then hit Z_STREAM_END → m_stream_ended = true
        auto n = decomp.get(buf, 64);
        VERIFY(n == 5);
        VERIFY(buf[0] == 'h' && buf[4] == 'o');

        // is_eof() should return true now (m_stream_ended is latched)
        VERIFY(decomp.is_eof());

        // calling get() again should hit the 'm_stream_ended' early-return and produce 0
        auto n2 = decomp.get(buf, 64);
        VERIFY(n2 == 0);
    }

    dump_info("Done\n");
}