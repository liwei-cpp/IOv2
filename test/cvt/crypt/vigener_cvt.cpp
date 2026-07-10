#include <cvt/crypt/vigenere_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <support/dump_info.h>
#include <support/verify.h>

void test_vigenere_cvt_gen_1()
{
    using namespace IOv2;
    dump_info("Test vigenere_cvt general case 1...");
    
    {
        using CheckType = Crypt::Classic::vigenere_cvt<rb_root_cvt<mem_device<char>>>;
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
        using CheckType = Crypt::Classic::vigenere_cvt<no_rb_root_cvt<mem_device<char32_t>>>;
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
    
    using CheckType = Crypt::Classic::vigenere_cvt<rb_root_cvt<mem_device<char>>>;
    
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
        VERIFY(new_str[5] == static_cast<char>(' ' + 'a'));
        VERIFY(new_str[6] == static_cast<char>('w' + 'b'));
        VERIFY(new_str[7] == static_cast<char>('o' + 'c'));
        VERIFY(new_str[8] == static_cast<char>('r' + 'd'));
        VERIFY(new_str[9] == static_cast<char>('l' + 'e'));
        VERIFY(new_str[10] == static_cast<char>('d' + 'f'));
        
        VERIFY(obj2.device().str() == "hello");
    };
    
    {
        mem_device dev{"hello"}; dev.drseek(0);
        CheckType obj{rb_root_cvt{std::move(dev)}, "abcdef"};
        helper1(obj);

        mem_device dev2{"hello"}; dev2.drseek(0);
        CheckType tmp{rb_root_cvt{std::move(dev2)}, "abcdef"};
        runtime_cvt obj2(std::move(tmp));
        helper1(obj2);
    }
    
    auto helper2 = []<typename T>(T& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        T obj2{Crypt::Classic::vigenere_cvt{rb_root_cvt{mem_device("")}, "abcdef"}};
        obj2 = obj;
        VERIFY(obj2.device().str() == "hello");
        
        obj.put(" world", 6);
        obj.flush();
        auto new_str = obj.device().str();
        VERIFY(new_str.size() == 11);
        VERIFY(new_str.substr(0, 5) == "hello");
        VERIFY(new_str[5] == static_cast<char>(' ' + 'a'));
        VERIFY(new_str[6] == static_cast<char>('w' + 'b'));
        VERIFY(new_str[7] == static_cast<char>('o' + 'c'));
        VERIFY(new_str[8] == static_cast<char>('r' + 'd'));
        VERIFY(new_str[9] == static_cast<char>('l' + 'e'));
        VERIFY(new_str[10] == static_cast<char>('d' + 'f'));

        VERIFY(obj2.device().str() == "hello");
    };

    {
        mem_device dev{"hello"}; dev.drseek(0);
        CheckType obj{rb_root_cvt{std::move(dev)}, "abcdef"};
        helper2(obj);

        mem_device dev2{"hello"}; dev2.drseek(0);
        CheckType tmp{rb_root_cvt{std::move(dev2)}, "abcdef"};
        runtime_cvt obj2(std::move(tmp));
        helper2(obj2);
    }

    auto helper3 = [](auto& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        auto obj2(std::move(obj));
        VERIFY(obj2.device().str() == "hello");
    };
    {
        mem_device dev{"hello"}; dev.drseek(0);
        CheckType obj{rb_root_cvt{std::move(dev)}, "abcdef"};
        helper3(obj);

        mem_device dev2{"hello"}; dev2.drseek(0);
        CheckType tmp{rb_root_cvt{std::move(dev2)}, "abcdef"};
        runtime_cvt obj2(std::move(tmp));
        helper3(obj2);
    }

    auto helper4 = []<typename T>(T& obj)
    {
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        T obj2{Crypt::Classic::vigenere_cvt{rb_root_cvt{mem_device("")}, "abcdef"}};
        obj2 = std::move(obj);
        VERIFY(obj2.device().str() == "hello");
    };
    {
        mem_device dev{"hello"}; dev.drseek(0);
        CheckType obj{rb_root_cvt{std::move(dev)}, "abcdef"};
        helper4(obj);

        mem_device dev2{"hello"}; dev2.drseek(0);
        CheckType tmp{rb_root_cvt{std::move(dev2)}, "abcdef"};
        runtime_cvt obj2(std::move(tmp));
        helper4(obj2);
    }

    dump_info("Done\n");
}

void test_vigenere_cvt_get_1()
{
    using namespace IOv2;
    dump_info("Test vigenere_cvt::get case 1...");
    using CheckType = Crypt::Classic::vigenere_cvt<rb_root_cvt<mem_device<char>>>;

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
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

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
        VERIFY(total_count == 4102);
        VERIFY(cur_pos == out_buf + 4102);
        for (size_t i = 0; i < 4102; ++i)
            VERIFY(out_buf[i] == i_lit[i]);
    };

    CheckType obj{rb_root_cvt{mem_device(e_lit)}, "liweixy"};
    helper(obj);
    
    CheckType tmp{rb_root_cvt{mem_device(e_lit)}, "liweixy"};
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2);

    dump_info("Done\n");
}

void test_vigenere_cvt_get_nra_1()
{
    using namespace IOv2;
    dump_info("Test vigenere_cvt::get_nra case 1...");
    using CheckType = Crypt::Classic::vigenere_cvt<no_rb_root_cvt<mem_device<char>>>;

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

        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);
    
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
        VERIFY(total_count == 4102);
        VERIFY(cur_pos == out_buf + 4102);
        for (size_t i = 0; i < 4102; ++i)
            VERIFY(out_buf[i] == i_lit[i]);
    };

    CheckType obj{no_rb_root_cvt{mem_device(e_lit)}, "liweixy"};
    helper(obj);
    
    CheckType tmp{no_rb_root_cvt{mem_device(e_lit)}, "liweixy"};
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2);

    dump_info("Done\n");
}

void test_vigenere_cvt_put_1()
{
    using namespace IOv2;
    dump_info("Test vigenere_cvt::put case 1...");
    using CheckType = Crypt::Classic::vigenere_cvt<rb_root_cvt<mem_device<char>>>;

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

        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);
        
        char* cur_pos = e_lit.data();
        int buffer_id = 0;
        while (cur_pos < e_lit.data() + 4102)
        {
            size_t dest_size = std::min<size_t>(buffer_size[buffer_id++], e_lit.data() + 4102 - cur_pos);
            obj.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;
        }
    
        VERIFY(cur_pos == e_lit.data() + 4102);
        obj.flush();
        auto& dev = obj.device();
        for (size_t i = 0; i < 4102; ++i)
            VERIFY(dev.str()[i] == i_lit[i]);
    };

    CheckType obj{rb_root_cvt{mem_device("")}, "liweixy"};
    helper(obj);
    
    CheckType tmp{rb_root_cvt{mem_device("")}, "liweixy"};
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
        VERIFY(obj.bos() == io_status::input);
        obj.main_cont_beg();
        
        obj.seek(3);
        VERIFY(obj.tell() == 3);
        
        char ch = 0;
        VERIFY((obj.get(&ch, 1) == 1) && (ch == '4' - 'e'));
        
        obj.rseek(3);
        VERIFY(obj.tell() == 2);
        VERIFY((obj.get(&ch, 1) == 1) && (ch == '3' - 'w'));
    };
    
    using CheckType = Crypt::Classic::vigenere_cvt<rb_root_cvt<mem_device<char>>>;
    {
        mem_device dev("12345");
        CheckType obj(rb_root_cvt{dev}, "liwei");
        helper(obj);
    }
    {
        mem_device dev("12345");
        CheckType tmp(rb_root_cvt{dev}, "liwei");
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
        VERIFY(obj.bos() == io_status::input);
        char c = 0;
        VERIFY((obj.get(&c, 1) == 1) && (c == '1'));
        VERIFY((obj.get(&c, 1) == 1) && (c == '2'));
        VERIFY((obj.get(&c, 1) == 1) && (c == '3'));

        obj.main_cont_beg();
        VERIFY(obj.tell() == 0);

        obj.seek(3);
        VERIFY(obj.tell() == 3);
        
        char ch = 0;
        VERIFY((obj.get(&ch, 1) == 1) && (ch == 'd' - 'e'));
        
        obj.rseek(3);
        VERIFY(obj.tell() == 4);
        VERIFY((obj.get(&ch, 1) == 1) && (ch == 'e' - 'i'));

        FAIL_RSEEK(obj, 60);
        VERIFY(obj.tell() == 5);

        FAIL_RSEEK(obj, 9);
        VERIFY(obj.tell() == 5);

        // seek out-of-bounds → seek_impl catches, tell() still works, rethrows (line 419)
        FAIL_SEEK(obj, 100);
        VERIFY(obj.tell() == 5);
    };

    Crypt::Classic::vigenere_cvt_creator<char> creator("liwei");
    auto obj = creator.create(rb_root_cvt{mem_device("123abcdefg")});
    helper(obj);

    auto tmp = creator.create(rb_root_cvt{mem_device("123abcdefg")});
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_vigenere_cvt_error_1()
{
    using namespace IOv2;
    dump_info("Test vigenere_cvt error paths...");

    // vigenere_cvt_creator with empty key throws
    {
        bool threw = false;
        try { Crypt::Classic::vigenere_cvt_creator<char> bad_creator(""); }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // vigenere_cvt constructor with empty key throws
    {
        bool threw = false;
        try
        {
            using CheckType = Crypt::Classic::vigenere_cvt<rb_root_cvt<mem_device<char>>>;
            std::string_view empty_key{};
            CheckType obj(rb_root_cvt{mem_device("")}, empty_key);
        }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // attach_impl() empty key: move-from then call attach() — hits line 183
    {
        using CheckType = Crypt::Classic::vigenere_cvt<rb_root_cvt<mem_device<char>>>;
        CheckType obj(rb_root_cvt{mem_device("")}, "liwei");
        auto moved = std::move(obj);
        // obj is moved-from: m_key is empty
        bool threw = false;
        try { obj.attach(); }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // detach_impl() resets m_pos; attach_impl() validates key on re-use
    {
        using CheckType = Crypt::Classic::vigenere_cvt<rb_root_cvt<mem_device<char>>>;
        CheckType obj(rb_root_cvt{mem_device("")}, "liwei");
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("hello", 5);
        obj.seek(3);
        VERIFY(obj.tell() == 3);
        auto [dev2, err] = obj.detach();
        // detach_impl() was called: m_pos reset to 0
        // attach_impl() validates key is non-empty
        obj.attach();
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("world", 5);
        auto [dev3, err3] = obj.detach();
        // "world" encrypted from m_pos=0 (reset by detach_impl)
        const std::string expected = {
            static_cast<char>('w' + 'l'),
            static_cast<char>('o' + 'i'),
            static_cast<char>('r' + 'w'),
            static_cast<char>('l' + 'e'),
            static_cast<char>('d' + 'i'),
        };
        VERIFY(dev3.str() == expected);
    }

    dump_info("Done\n");
}
