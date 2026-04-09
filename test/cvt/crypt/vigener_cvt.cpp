#include <cvt/crypt/vigenere_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <common/dump_info.h>
#include <common/verify.h>

void test_vigenere_cvt_gen_1()
{
    using namespace IOv2;
    dump_info("Test vigenere_cvt general case 1...");
    
    {
        using CheckType = Crypt::vigenere_cvt<root_cvt<mem_device<char>, true>>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, char>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(cvt_cpt::support_positioning<CheckType>);
        static_assert(cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = Crypt::vigenere_cvt<root_cvt<mem_device<char32_t>, false>>;
        static_assert(IOv2::io_converter<CheckType>);
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

void test_vigenere_cvt_gen_2()
{
    using namespace IOv2;
    dump_info("Test vigenere_cvt<mem_device> general case 2...");
    
    using CheckType = Crypt::vigenere_cvt<root_cvt<mem_device<char>, true>>;
    
    auto helper1 = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        auto obj2(obj);
        VERIFY(obj2.device().str() == "hello");
        
        obj.put(" world", 6);
        obj.flush();
        auto new_str = obj.device().str();
        VERIFY(new_str.size() == 11);
        VERIFY(new_str.substr(0, 5) == "hello");
        if (new_str[5] != static_cast<char>(' ' + 'a')) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        if (new_str[6] != static_cast<char>('w' + 'b')) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        if (new_str[7] != static_cast<char>('o' + 'c')) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        if (new_str[8] != static_cast<char>('r' + 'd')) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        if (new_str[9] != static_cast<char>('l' + 'e')) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        if (new_str[10] != static_cast<char>('d' + 'f')) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        
        if (obj2.device().str() != "hello") throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
    };
    
    {
        mem_device dev{"hello"}; dev.drseek(0);
        CheckType obj{make_root_cvt<true>(std::move(dev)), "abcdef"};
        helper1(obj);

        mem_device dev2{"hello"}; dev2.drseek(0);
        CheckType tmp{make_root_cvt<true>(std::move(dev2)), "abcdef"};
        runtime_cvt obj2(std::move(tmp));
        helper1(obj2);
    }
    
    auto helper2 = []<typename T>(T& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("vigenere_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        T obj2{Crypt::vigenere_cvt{make_root_cvt<true>(mem_device("")), "abcdef"}};
        obj2 = obj;
        if (obj2.device().str() != "hello") throw std::runtime_error("vigenere_cvt<mem_device> copy assignment response incorrect");
        
        obj.put(" world", 6);
        obj.flush();
        auto new_str = obj.device().str();
        if (new_str.size() != 11) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        if (new_str.substr(0, 5) != "hello") throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        if (new_str[5] != static_cast<char>(' ' + 'a')) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        if (new_str[6] != static_cast<char>('w' + 'b')) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        if (new_str[7] != static_cast<char>('o' + 'c')) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        if (new_str[8] != static_cast<char>('r' + 'd')) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        if (new_str[9] != static_cast<char>('l' + 'e')) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");
        if (new_str[10] != static_cast<char>('d' + 'f')) throw std::runtime_error("vigenere_cvt<mem_device> copy constructor response incorrect");

        if (obj2.device().str() != "hello") throw std::runtime_error("vigenere_cvt<mem_device> copy assignment response incorrect");
    };

    {
        mem_device dev{"hello"}; dev.drseek(0);
        CheckType obj{make_root_cvt<true>(std::move(dev)), "abcdef"};
        helper2(obj);

        mem_device dev2{"hello"}; dev2.drseek(0);
        CheckType tmp{make_root_cvt<true>(std::move(dev2)), "abcdef"};
        runtime_cvt obj2(std::move(tmp));
        helper2(obj2);
    }

    auto helper3 = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("vigenere_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        auto obj2(std::move(obj));
        if (obj2.device().str() != "hello") throw std::runtime_error("vigenere_cvt<mem_device> move constructor response incorrect");
    };
    {
        mem_device dev{"hello"}; dev.drseek(0);
        CheckType obj{make_root_cvt<true>(std::move(dev)), "abcdef"};
        helper3(obj);

        mem_device dev2{"hello"}; dev2.drseek(0);
        CheckType tmp{make_root_cvt<true>(std::move(dev2)), "abcdef"};
        runtime_cvt obj2(std::move(tmp));
        helper3(obj2);
    }

    auto helper4 = []<typename T>(T& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("vigenere_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        T obj2{Crypt::vigenere_cvt{make_root_cvt<true>(mem_device("")), "abcdef"}};
        obj2 = std::move(obj);
        if (obj2.device().str() != "hello") throw std::runtime_error("vigenere_cvt<mem_device> move assignment response incorrect");
    };
    {
        mem_device dev{"hello"}; dev.drseek(0);
        CheckType obj{make_root_cvt<true>(std::move(dev)), "abcdef"};
        helper4(obj);

        mem_device dev2{"hello"}; dev2.drseek(0);
        CheckType tmp{make_root_cvt<true>(std::move(dev2)), "abcdef"};
        runtime_cvt obj2(std::move(tmp));
        helper4(obj2);
    }

    dump_info("Done\n");
}

void test_vigenere_cvt_get_1()
{
    using namespace IOv2;
    dump_info("Test vigenere_cvt::get case 1...");
    using CheckType = Crypt::vigenere_cvt<root_cvt<mem_device<char>, true>>;

    std::string e_lit; e_lit.resize(4102);
    std::string i_lit; i_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = '\xE6';                i_lit[i+0] = e_lit[i+0] - 'l';
        e_lit[i+1] = '\x9D';                i_lit[i+1] = e_lit[i+1] - 'i';
        e_lit[i+2] = '\x8E';                i_lit[i+2] = e_lit[i+2] - 'w';
        e_lit[i+3] = '\xE4';                i_lit[i+3] = e_lit[i+3] - 'e';
        e_lit[i+4] = '\xBC';                i_lit[i+4] = e_lit[i+4] - 'i';
        e_lit[i+5] = '\x9F';                i_lit[i+5] = e_lit[i+5] - 'x';
        e_lit[i+6] = (i / 7) % 127 + 1;     i_lit[i+6] = e_lit[i+6] - 'y';
    }

    auto helper = [&i_lit](auto& obj)
    {
        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
        if (obj.bos() != io_status::input) throw std::runtime_error("vigenere_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("vigenere_cvt::tell fail");

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
        if (total_count != 4102) throw std::runtime_error("vigenere_cvt::get response incorrect");
        if (cur_pos != out_buf + 4102) throw std::runtime_error("vigenere_cvt::get response incorrect");
        for (size_t i = 0; i < 4102; ++i)
            if (out_buf[i] != i_lit[i]) throw std::runtime_error("vigenere_cvt::get response incorrect");
    };

    CheckType obj{make_root_cvt<true>(mem_device(e_lit)), "liweixy"};
    helper(obj);
    
    CheckType tmp{make_root_cvt<true>(mem_device(e_lit)), "liweixy"};
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2);

    dump_info("Done\n");
}

void test_vigenere_cvt_get_nra_1()
{
    using namespace IOv2;
    dump_info("Test vigenere_cvt::get_nra case 1...");
    using CheckType = Crypt::vigenere_cvt<root_cvt<mem_device<char>, false>>;

    std::string e_lit; e_lit.resize(4102);
    std::string i_lit; i_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = '\xE6';                i_lit[i+0] = e_lit[i+0] - 'l';
        e_lit[i+1] = '\x9D';                i_lit[i+1] = e_lit[i+1] - 'i';
        e_lit[i+2] = '\x8E';                i_lit[i+2] = e_lit[i+2] - 'w';
        e_lit[i+3] = '\xE4';                i_lit[i+3] = e_lit[i+3] - 'e';
        e_lit[i+4] = '\xBC';                i_lit[i+4] = e_lit[i+4] - 'i';
        e_lit[i+5] = '\x9F';                i_lit[i+5] = e_lit[i+5] - 'x';
        e_lit[i+6] = (i / 7) % 127 + 1;     i_lit[i+6] = e_lit[i+6] - 'y';
    }

    auto helper = [&i_lit](auto& obj)
    {
        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        if (obj.bos() != io_status::input) throw std::runtime_error("vigenere_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("vigenere_cvt::tell fail");
    
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
        if (total_count != 4102) throw std::runtime_error("vigenere_cvt::get response incorrect");
        if (cur_pos != out_buf + 4102) throw std::runtime_error("vigenere_cvt::get response incorrect");
        for (size_t i = 0; i < 4102; ++i)
            if (out_buf[i] != i_lit[i]) throw std::runtime_error("vigenere_cvt::get response incorrect");
    };

    CheckType obj{make_root_cvt<false>(mem_device(e_lit)), "liweixy"};
    helper(obj);
    
    CheckType tmp{make_root_cvt<false>(mem_device(e_lit)), "liweixy"};
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2);

    dump_info("Done\n");
}

void test_vigenere_cvt_put_1()
{
    using namespace IOv2;
    dump_info("Test vigenere_cvt::put case 1...");
    using CheckType = Crypt::vigenere_cvt<root_cvt<mem_device<char>, true>>;

    std::string e_lit; e_lit.resize(4102);
    std::string i_lit; i_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = '\xE6';                i_lit[i+0] = e_lit[i+0] + 'l';
        e_lit[i+1] = '\x9D';                i_lit[i+1] = e_lit[i+1] + 'i';
        e_lit[i+2] = '\x8E';                i_lit[i+2] = e_lit[i+2] + 'w';
        e_lit[i+3] = '\xE4';                i_lit[i+3] = e_lit[i+3] + 'e';
        e_lit[i+4] = '\xBC';                i_lit[i+4] = e_lit[i+4] + 'i';
        e_lit[i+5] = '\x9F';                i_lit[i+5] = e_lit[i+5] + 'x';
        e_lit[i+6] = (i / 7) % 127 + 1;     i_lit[i+6] = e_lit[i+6] + 'y';
    }

    auto helper = [&e_lit, &i_lit](auto& obj)
    {
        size_t buffer_size[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};

        if (obj.bos() != io_status::output) throw std::runtime_error("vigenere_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("vigenere_cvt::tell fail");
        
        char* cur_pos = e_lit.data();
        int buffer_id = 0;
        while (cur_pos < e_lit.data() + 4102)
        {
            size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], e_lit.data() + 4102 - cur_pos);
            obj.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;
        }
    
        if (cur_pos != e_lit.data() + 4102) throw std::runtime_error("vigenere_cvt::put response incorrect");
        obj.flush();
        auto& dev = obj.device();
        for (size_t i = 0; i < 4102; ++i)
            if (dev.str()[i] != i_lit[i]) throw std::runtime_error("vigenere_cvt::get response incorrect");
    };

    CheckType obj{make_root_cvt<true>(mem_device("")), "liweixy"};
    helper(obj);
    
    CheckType tmp{make_root_cvt<true>(mem_device("")), "liweixy"};
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2);

    dump_info("Done\n");
}

void test_vigenere_cvt_seek_1()
{
    using namespace IOv2;
    dump_info("Test vigenere_cvt::seek case 1...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("vigenere_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        
        obj.seek(3);
        if (obj.tell() != 3) throw std::runtime_error("vigenere_cvt::tell fail");
        
        char ch = 0;
        if ((obj.get(&ch, 1) != 1) || (ch != '4' - 'e')) throw std::runtime_error("vigenere_cvt::get fail");
        
        obj.rseek(3);
        if (obj.tell() != 2) throw std::runtime_error("vigenere_cvt::tell fail");
        if ((obj.get(&ch, 1) != 1) || (ch != '3' - 'w')) throw std::runtime_error("vigenere_cvt::get fail");
    };
    
    using CheckType = Crypt::vigenere_cvt<root_cvt<mem_device<char>, true>>;
    {
        mem_device dev("12345");
        CheckType obj(make_root_cvt<true>(dev), "liwei");
        helper(obj);
    }
    {
        mem_device dev("12345");
        CheckType tmp(make_root_cvt<true>(dev), "liwei");
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }

    dump_info("Done\n");
}

void test_vigenere_cvt_seek_2()
{
    using namespace IOv2;
    dump_info("Test vigenere_cvt::seek case 2...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("vigenere_cvt<mem_device>::bos response incorrect");
        char c = 0;
        if ((obj.get(&c, 1) != 1) || (c != '1')) throw std::runtime_error("vigenere_cvt::get fail");
        if ((obj.get(&c, 1) != 1) || (c != '2')) throw std::runtime_error("vigenere_cvt::get fail");
        if ((obj.get(&c, 1) != 1) || (c != '3')) throw std::runtime_error("vigenere_cvt::get fail");

        obj.main_cont_beg();
        if (obj.tell() != 0) throw std::runtime_error("vigenere_cvt::get_bos fail");

        obj.seek(3);
        if (obj.tell() != 3) throw std::runtime_error("vigenere_cvt::tell fail");
        
        char ch = 0;
        if ((obj.get(&ch, 1) != 1) || (ch != 'd' - 'e')) throw std::runtime_error("vigenere_cvt::get fail");
        
        obj.rseek(3);
        if (obj.tell() != 4) throw std::runtime_error("vigenere_cvt::tell fail");
        if ((obj.get(&ch, 1) != 1) || (ch != 'e' - 'i')) throw std::runtime_error("vigenere_cvt::get fail");

        FAIL_RSEEK(obj, 60);
        if (obj.tell() != 5) throw std::runtime_error("vigenere_cvt::tell fail");
        
        FAIL_RSEEK(obj, 9);
        if (obj.tell() != 5) throw std::runtime_error("vigenere_cvt::tell fail");
    };

    Crypt::vigenere_cvt_creator<char> creator("liwei");
    auto obj = creator.create(make_root_cvt<true>(mem_device("123abcdefg")));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device("123abcdefg")));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}
