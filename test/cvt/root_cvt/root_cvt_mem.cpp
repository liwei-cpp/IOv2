#include <typeinfo>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <common/dump_info.h>
#include <common/verify.h>

void test_root_cvt_mem_gen_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device> general case 1...");
    
    {
        using CheckType = root_cvt<mem_device<char>, false>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, char>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = root_cvt<mem_device<char32_t>, true>;
        static_assert(io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char32_t>>);
        static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
        static_assert(std::is_same_v<CheckType::external_type, char32_t>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(cvt_cpt::support_io_switch<CheckType>);
    }

    dump_info("Done\n");
}

void test_root_cvt_mem_gen_2()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device> general case 2...");
    
    auto helper = [](const auto& ori_obj)
    {
        {
            auto obj = ori_obj;
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            auto obj2(obj);
            VERIFY(obj2.device().str() == "hello");
            
            obj.put(" world", 6);
            obj.flush();
            VERIFY(obj.device().str() == "hello world");
            VERIFY(obj2.device().str() == "hello");
        }

        {
            auto obj = ori_obj;
            if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            decltype(obj) obj2{make_root_cvt<true>(mem_device(""))};
            obj2 = obj;
            if (obj2.device().str() != "hello") throw std::runtime_error("root_cvt<mem_device> copy assignment response incorrect");
            
            obj.put(" world", 6);
            obj.flush();
            if (obj.device().str() != "hello world") throw std::runtime_error("root_cvt<mem_device> copy assignment response incorrect");
            if (obj2.device().str() != "hello") throw std::runtime_error("root_cvt<mem_device> copy assignment response incorrect");
        }

        {
            auto obj = ori_obj;
            if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            auto obj2(std::move(obj));
            if (obj2.device().str() != "hello") throw std::runtime_error("root_cvt<mem_device> move constructor response incorrect");
        }

        {
            auto obj = ori_obj;
            if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            decltype(obj) obj2{make_root_cvt<true>(mem_device(""))};
            obj2 = std::move(obj);
            if (obj2.device().str() != "hello") throw std::runtime_error("root_cvt<mem_device> move assignment response incorrect");
        }
    };

    mem_device dev{"hello"}; dev.drseek(0);
    auto obj1 = make_root_cvt<true>(std::move(dev));
    helper(obj1);
    
    runtime_cvt obj2(std::move(obj1));
    helper(obj2);
    dump_info("Done\n");
}

void test_root_cvt_mem_gen_3()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device> general case 3...");
    
    auto helper = [](const auto& ori_obj)
    {
        {
            auto obj = ori_obj;
            if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            std::string str; str.resize(5);
            if (obj.get(str.data(), 5) != 5) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != "hello") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (obj.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            
            auto obj2(obj);
            if (obj2.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            str.resize(6);
            if (obj2.get(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != " world") throw std::runtime_error("root_cvt<mem_device> copy constructor response incorrect");
            if (obj2.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            
            str = "xxxxxx";
            if (obj.get(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != " world") throw std::runtime_error("root_cvt<mem_device> copy constructor response incorrect");
            if (obj.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
        }
        
        {
            auto obj = ori_obj;
            if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            std::string str; str.resize(5);
            if (obj.get(str.data(), 5) != 5) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != "hello") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (obj.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            
            decltype(obj) obj2{make_root_cvt<true>(mem_device(""))};
            obj2 = obj;
            if (obj2.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            str.resize(6);
            if (obj2.get(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != " world") throw std::runtime_error("root_cvt<mem_device> copy assignment response incorrect");
            if (obj2.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            
            str = "xxxxxx";
            if (obj.get(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != " world") throw std::runtime_error("root_cvt<mem_device> copy assignment response incorrect");
            if (obj.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
        }
    
        {
            auto obj = ori_obj;
            if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            std::string str; str.resize(5);
            if (obj.get(str.data(), 5) != 5) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != "hello") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (obj.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            
            auto obj2(std::move(obj));
            if (obj2.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            str.resize(6);
            if (obj2.get(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != " world") throw std::runtime_error("root_cvt<mem_device> move constructor response incorrect");
            if (obj2.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
        }
    
        {
            auto obj = ori_obj;
            if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            std::string str; str.resize(5);
            if (obj.get(str.data(), 5) != 5) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != "hello") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (obj.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            
            decltype(obj) obj2{make_root_cvt<true>(mem_device(""))};
            obj2 = std::move(obj);
            if (obj2.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            str.resize(6);
            if (obj2.get(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != " world") throw std::runtime_error("root_cvt<mem_device> move assignment response incorrect");
            if (obj2.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
        }
    };

    auto obj1 = make_root_cvt<true>(mem_device("hello world"));
    helper(obj1);

    runtime_cvt obj2(std::move(obj1));
    helper(obj2);
    dump_info("Done\n");
}

void test_root_cvt_mem_get_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device>::get case 1...");

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

    auto helper = [&e_lit](auto& obj)
    {
        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        char out_buf[4102];
        size_t total_count = 0;
        char* cur_pos = out_buf;
        int out_buffer_id = 0;
        while (true)
        {
            size_t dest_size = std::min<size_t>(4102 - total_count, out_buffer_size[out_buffer_id++]);
            auto s = obj.get(cur_pos, dest_size);
            out_buffer_id %= std::size(out_buffer_size);
            cur_pos += s;
            total_count += s;
            if (s == 0) break;
        }
        if (total_count != 4102) throw std::runtime_error("root_cvt<mem_device>::get response incorrect");
        if (cur_pos != out_buf + 4102) throw std::runtime_error("root_cvt<mem_device>::get response incorrect");
        for (size_t i = 0; i < 4102; ++i)
            if (out_buf[i] != e_lit[i]) throw std::runtime_error("root_cvt<mem_device>::get response incorrect");
    };

    auto obj1 = make_root_cvt<true>(mem_device(e_lit));
    helper(obj1);
    
    runtime_cvt obj2(make_root_cvt<true>(mem_device(e_lit)));
    helper(obj2);

    dump_info("Done\n");
}

void test_root_cvt_mem_get_nra_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device>::get_nra case 1...");

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

    auto helper = [&e_lit](auto& obj)
    {
        size_t out_buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};

        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        char out_buf[4102];
        size_t total_count = 0;
        char* cur_pos = out_buf;
        int out_buffer_id = 0;
        while (true)
        {
            size_t dest_size = std::min<size_t>(4102 - total_count, out_buffer_size[out_buffer_id++]);
            auto s = obj.get(cur_pos, dest_size);
            out_buffer_id %= std::size(out_buffer_size);
            cur_pos += s;
            total_count += s;
            if (s == 0) break;
        }
        if (total_count != 4102) throw std::runtime_error("root_cvt<mem_device>::get response incorrect");
        if (cur_pos != out_buf + 4102) throw std::runtime_error("root_cvt<mem_device>::get response incorrect");
        for (size_t i = 0; i < 4102; ++i)
            if (out_buf[i] != e_lit[i]) throw std::runtime_error("root_cvt<mem_device>::get response incorrect");
    };

    auto obj1 = make_root_cvt<false>(mem_device(e_lit));
    helper(obj1);
    
    runtime_cvt obj2(make_root_cvt<false>(mem_device(e_lit)));
    helper(obj2);

    dump_info("Done\n");
}

void test_root_cvt_mem_put_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device>::put case 1...");

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

    auto helper = [&e_lit = std::as_const(e_lit)] (auto& obj)
    {
        size_t buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};

        if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        const char* cur_pos = e_lit.data();
        int buffer_id = 0;
        while (cur_pos < e_lit.data() + 4102)
        {
            size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], e_lit.data() + 4102 - cur_pos);
            obj.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;
        }
    
        if (cur_pos != e_lit.data() + 4102) throw std::runtime_error("root_cvt<mem_device>::put response incorrect");
    
        obj.flush();
        const mem_device<char>& dev = obj.device();
        if (dev.str() != e_lit) throw std::runtime_error("root_cvt<mem_device>::put response incorrect");
    };

    auto obj1 = make_root_cvt<true>(mem_device(""));
    helper(obj1);
    
    runtime_cvt obj2(make_root_cvt<true>(mem_device("")));
    helper(obj2);

    dump_info("Done\n");
}

void test_root_cvt_mem_seek_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device>::seek case 1...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.seek(3);
        if (obj.tell() != 3) throw std::runtime_error("root_cvt<mem_device>::tell fail");
        
        char ch = 0;
        if ((obj.get(&ch, 1) != 1) || (ch != '4')) throw std::runtime_error("root_cvt<mem_device>::get fail");
        
        obj.rseek(3);
        if (obj.tell() != 2) throw std::runtime_error("root_cvt<mem_device>::tell fail");
        if ((obj.get(&ch, 1) != 1) || (ch != '3')) throw std::runtime_error("root_cvt<mem_device>::get fail");
    };

    auto obj1 = make_root_cvt<true>(mem_device("12345"));
    helper(obj1);

    runtime_cvt obj2(make_root_cvt<true>(mem_device("12345")));
    helper(obj2);

    dump_info("Done\n");
}

void test_root_cvt_mem_seek_2()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device>::seek case 2...");

    auto helper = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::input);
        char c = 0;
        if ((obj.get(&c, 1) != 1) || (c != '1')) throw std::runtime_error("root_cvt<mem_device>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != '2')) throw std::runtime_error("root_cvt<mem_device>::get_nra fail");
        if ((obj.get(&c, 1) != 1) || (c != '3')) throw std::runtime_error("root_cvt<mem_device>::get_nra fail");

        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        obj.seek(3);
        VERIFY(obj.tell() == 3);
        
        char ch = 0;
        if ((obj.get(&ch, 1) != 1) || (ch != 'd')) throw std::runtime_error("root_cvt<mem_device>::get fail");
        
        obj.rseek(3);
        if (obj.tell() != 4) throw std::runtime_error("root_cvt<mem_device>::tell fail");
        if ((obj.get(&ch, 1) != 1) || (ch != 'e')) throw std::runtime_error("root_cvt<mem_device>::get fail");

        FAIL_RSEEK(obj, 60);
        VERIFY(obj.tell() == 5);

        FAIL_RSEEK(obj, 9);
        VERIFY(obj.tell() == 5);
    };

    auto obj1 = make_root_cvt<true>(mem_device("123abcdefg"));
    helper(obj1);

    runtime_cvt obj2(make_root_cvt<true>(mem_device("123abcdefg")));
    helper(obj2);

    dump_info("Done\n");
}

void test_root_cvt_mem_reset_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device>::reset case 1...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("root_cvt<mem_device>::bos response incorrect");

        obj.put("hello", 5);
        obj.main_cont_beg();
        obj.put(" world", 6);
        if (obj.tell() != 6) throw std::runtime_error("root_cvt<mem_device>::tell incorrect");

        mem_device<char> ori = obj.attach(mem_device(""));
        if (ori.str() != "hello world") throw std::runtime_error("root_cvt<mem_device>::reset incorrect");
        
        obj.bos();
        if (obj.tell() != 0) throw std::runtime_error("root_cvt<mem_device>::tell incorrect");
        obj.put("liwei", 5);
        obj.main_cont_beg();
        obj.put(" cpp", 4);
        if (obj.tell() != 4) throw std::runtime_error("root_cvt<mem_device>::tell incorrect");
    
        obj.flush();
        const mem_device<char>& new_dev = obj.device();
        if (new_dev.str() != "liwei cpp") throw std::runtime_error("root_cvt<mem_device>::put incorrect");
    };

    auto obj1 = make_root_cvt<true>(mem_device(""));
    helper(obj1);

    runtime_cvt obj2(make_root_cvt<true>(mem_device("")));
    helper(obj2);
    dump_info("Done\n");
}

void test_root_cvt_mem_device_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device>::device case 1...");

    auto helper = []<typename T>(T& obj)
    {
        using device_type = typename T::device_type;

        device_type f1("");
        obj.attach(std::move(f1));
        obj.bos(); obj.main_cont_beg();
        obj.put("abc", 3);

        f1 = obj.detach();
        obj.attach(device_type(""));
        obj.bos(); obj.main_cont_beg();
        obj.put("123", 3);

        device_type f2 = obj.attach(std::move(f1));
        obj.bos(); obj.main_cont_beg();
        obj.put("def", 3);
        
        f1 = obj.detach();
        if (f1.str() != "abcdef") throw std::runtime_error("root_cvt<file_device> output incorrect");
        if (f2.str() != "123") throw std::runtime_error("root_cvt<file_device> output incorrect");
    };
    {
        auto obj = make_root_cvt<true>(mem_device(""));
        helper(obj);
    }
    
    {
        auto tmp = make_root_cvt<true>(mem_device(""));
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }
    dump_info("Done\n");
}

void test_root_cvt_mem_detach_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device>::detach case 1...");

    auto helper = []<typename T>(T& obj)
    {
        char ch = 0;
        obj.get(&ch, 1);
        VERIFY(ch == '1');
        VERIFY(obj.device().dtell() == 8);
        
        auto dev = obj.detach();
        VERIFY(dev.dtell() == 1);
    };
    {
        auto obj = make_root_cvt<true>(mem_device("12345678"));
        helper(obj);
    }
    
    {
        runtime_cvt obj(make_root_cvt<true>(mem_device("12345678")));
        helper(obj);
    }
    dump_info("Done\n");
}

void test_root_cvt_mem_detach_2()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device>::detach case 2...");

    auto helper = []<typename T>(T& obj)
    {
        obj.put("123", 3);
        VERIFY(obj.device().dtell() == 0);
        
        auto dev = obj.detach();
        VERIFY(dev.dtell() == 3);
    };
    {
        auto obj = make_root_cvt<true>(mem_device(""));
        helper(obj);
    }
    
    {
        runtime_cvt obj(make_root_cvt<true>(mem_device("")));
        helper(obj);
    }
    dump_info("Done\n");
}

void test_root_cvt_mem_attach_1()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device>::attach case 1...");

    auto helper = []<typename T>(T& obj)
    {
        char ch = 0;
        obj.get(&ch, 1);
        VERIFY(ch == '1');
        VERIFY(obj.device().dtell() == 8);
        
        auto dev = obj.attach(mem_device{""});
        VERIFY(dev.dtell() == 1);
    };
    {
        auto obj = make_root_cvt<true>(mem_device("12345678"));
        helper(obj);
    }
    
    {
        runtime_cvt obj(make_root_cvt<true>(mem_device("12345678")));
        helper(obj);
    }
    dump_info("Done\n");
}

void test_root_cvt_mem_attach_2()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device>::attach case 2...");

    auto helper = []<typename T>(T& obj)
    {
        obj.put("123", 3);
        VERIFY(obj.device().dtell() == 0);
        
        auto dev = obj.attach(mem_device{""});
        VERIFY(dev.dtell() == 3);
    };
    {
        auto obj = make_root_cvt<true>(mem_device(""));
        helper(obj);
    }
    
    {
        runtime_cvt obj(make_root_cvt<true>(mem_device("")));
        helper(obj);
    }
    dump_info("Done\n");
}

void test_root_cvt_mem_self_assignment()
{
    using namespace IOv2;
    dump_info("Test root_cvt<mem_device> self-assignment...");
    
    {
        mem_device dev{"hello"}; dev.drseek(0);
        auto obj = make_root_cvt<true>(std::move(dev));
        VERIFY(obj.bos() == io_status::output); 
        obj.main_cont_beg();
        obj.put(" world", 6);
        
        // Self copy assignment
        const auto& const_obj = obj;
        obj = const_obj;
        
        obj.flush();
        VERIFY(obj.device().str() == "hello world");
        
        // Self move assignment
        auto* pObj = &obj;
        obj = std::move(*pObj);
        VERIFY(obj.device().str() == "hello world");
    }
    
    {
        mem_device dev{"hello"}; dev.drseek(0);
        runtime_cvt obj(make_root_cvt<true>(std::move(dev)));
        VERIFY(obj.bos() == io_status::output); 
        obj.main_cont_beg();
        obj.put(" world", 6);
        
        // Self copy assignment
        const auto& const_obj = obj;
        obj = const_obj;
        obj.flush();
        VERIFY(obj.device().str() == "hello world");
        
        // Self move assignment
        auto* pObj = &obj;
        obj = std::move(*pObj);
        VERIFY(obj.device().str() == "hello world");
    }

    dump_info("Done\n");
}
