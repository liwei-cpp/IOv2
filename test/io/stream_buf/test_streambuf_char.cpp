#include <cvt/root_cvt.h>
#include <device/mem_device.h>
#include <io/streambuf.h>
#include <common/dump_info.h>

void test_streambuf_char_gen_1()
{
    dump_info("Test streambuf<char> general case 1...");
    using namespace IOv2;

    {
        using CheckType = streambuf<mem_device<char>, char>;
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::char_type, char>);
    }

    {
        using CheckType = istreambuf<mem_device<char>, char>;
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::char_type, char>);
    }

    {
        using CheckType = ostreambuf<mem_device<char>, char>;
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::char_type, char>);
    }

    dump_info("Done\n");
}

void test_streambuf_char_gen_2()
{
    using namespace IOv2;
    dump_info("Test streambuf<char> general case 2...");
    
    auto helper = []<typename T>(const T& ori_obj)
    {
        {
            T obj = ori_obj;
            if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::tell fail");
            T obj2(obj);
            if (obj2.tell() != 0) throw std::runtime_error("streambuf<char>::tell fail");
            if (obj2.device().str() != "hello") throw std::runtime_error("root_cvt<mem_device> copy constructor response incorrect");

            obj.sputn(" world", 6);
            if (obj.tell() != 6) throw std::runtime_error("streambuf<char>::tell fail");
            if (obj2.tell() != 0) throw std::runtime_error("streambuf<char>::tell fail");
            obj.flush();
            if (obj.device().str() != "hello world") throw std::runtime_error("root_cvt<mem_device> copy constructor response incorrect");
            if (obj2.device().str() != "hello") throw std::runtime_error("root_cvt<mem_device> copy constructor response incorrect");
        }

        {
            auto obj = ori_obj;
            decltype(obj) obj2{mem_device("")};
            obj2 = obj;
            if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::tell fail");
            if (obj2.tell() != 0) throw std::runtime_error("streambuf<char>::tell fail");
            if (obj2.device().str() != "hello") throw std::runtime_error("root_cvt<mem_device> copy assignment response incorrect");
            
            obj.sputn(" world", 6);
            obj.flush();
            if (obj.tell() != 6) throw std::runtime_error("streambuf<char>::tell fail");
            if (obj2.tell() != 0) throw std::runtime_error("streambuf<char>::tell fail");
            if (obj.device().str() != "hello world") throw std::runtime_error("root_cvt<mem_device> copy assignment response incorrect");
            if (obj2.device().str() != "hello") throw std::runtime_error("root_cvt<mem_device> copy assignment response incorrect");
        }

        {
            auto obj = ori_obj;
            auto obj2(std::move(obj));
            if (obj2.tell() != 0) throw std::runtime_error("streambuf<char>::tell fail");
            if (obj2.device().str() != "hello") throw std::runtime_error("root_cvt<mem_device> move constructor response incorrect");
        }

        {
            auto obj = ori_obj;
            T obj2{mem_device("")};
            obj2 = std::move(obj);
            if (obj2.tell() != 0) throw std::runtime_error("streambuf<char>::tell fail");
            if (obj2.device().str() != "hello") throw std::runtime_error("root_cvt<mem_device> move assignment response incorrect");
        }
    };

    mem_device dev("hello"); dev.drseek(0);
    helper(streambuf{dev});
    helper(ostreambuf{dev});

    dump_info("Done\n");
}

void test_streambuf_char_gen_3()
{
    using namespace IOv2;
    dump_info("Test streambuf<char> general case 3...");
    
    auto helper = [](const auto& ori_obj)
    {
        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            if (obj.sgetn(str.data(), 5) != 5) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != "hello") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (obj.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            
            auto obj2(obj);
            if (obj2.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            str.resize(6);
            if (obj2.sgetn(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != " world") throw std::runtime_error("root_cvt<mem_device> copy constructor response incorrect");
            if (obj2.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            
            str = "xxxxxx";
            if (obj.sgetn(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != " world") throw std::runtime_error("root_cvt<mem_device> copy constructor response incorrect");
            if (obj.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
        }
        
        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            if (obj.sgetn(str.data(), 5) != 5) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != "hello") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (obj.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            
            decltype(obj) obj2{mem_device("")};
            obj2 = obj;
            if (obj2.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            str.resize(6);
            if (obj2.sgetn(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != " world") throw std::runtime_error("root_cvt<mem_device> copy assignment response incorrect");
            if (obj2.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            
            str = "xxxxxx";
            if (obj.sgetn(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != " world") throw std::runtime_error("root_cvt<mem_device> copy assignment response incorrect");
            if (obj.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
        }
    
        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            if (obj.sgetn(str.data(), 5) != 5) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != "hello") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (obj.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            
            auto obj2(std::move(obj));
            if (obj2.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            str.resize(6);
            if (obj2.sgetn(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != " world") throw std::runtime_error("root_cvt<mem_device> move constructor response incorrect");
            if (obj2.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
        }
    
        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            if (obj.sgetn(str.data(), 5) != 5) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != "hello") throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (obj.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            
            decltype(obj) obj2{mem_device("")};
            obj2 = std::move(obj);
            if (obj2.tell() != 5) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
            str.resize(6);
            if (obj2.sgetn(str.data(), 6) != 6) throw std::runtime_error("root_cvt<std_device>::get response incorrect");
            if (str != " world") throw std::runtime_error("root_cvt<mem_device> move assignment response incorrect");
            if (obj2.tell() != 11) throw std::runtime_error("root_cvt<std_device>::tell response incorrect");
        }
    };

    helper(streambuf{mem_device("hello world")});
    helper(istreambuf{mem_device("hello world")});

    dump_info("Done\n");
}

void test_streambuf_char_get1()
{
    dump_info("Test streambuf<char>::get 1...");
    using namespace IOv2;

    auto helper = [](auto& obj)
    {
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
    };

    streambuf obj1{mem_device{"abc"}};
    helper(obj1);

    istreambuf obj2{mem_device{"abc"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_streambuf_char_get2()
{
    dump_info("Test streambuf<char>::get 2...");
    using namespace IOv2;

    auto helper = [](auto& obj)
    {
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
    };

    streambuf obj1{mem_device{"abc"}};
    helper(obj1);

    istreambuf obj2{mem_device{"abc"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_streambuf_char_get3()
{
    dump_info("Test streambuf<char>::get 3...");
    using namespace IOv2;

    auto helper = [](auto& obj)
    {
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
    };

    streambuf obj1{mem_device{"abc"}};
    helper(obj1);

    istreambuf obj2{mem_device{"abc"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_streambuf_char_get4()
{
    dump_info("Test streambuf<char>::get 4...");
    using namespace IOv2;

    auto helper = [](auto& obj)
    {
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
    };

    streambuf obj1{mem_device{"abc"}};
    helper(obj1);

    istreambuf obj2{mem_device{"abc"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_streambuf_char_get5()
{
    dump_info("Test streambuf<char>::get 5...");
    using namespace IOv2;

    std::string info = "chicago underground trio/possible cube on delmark";
    
    auto helper = [&info](auto& obj)
    {
        std::string str(info.size(), '\0');
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetn(str.data(), 0) != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");

        if (obj.sgetn(str.data(), 1) != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (str[0] != 'c') throw std::runtime_error("streambuf<char>::get fail");

        if (obj.sgetn(str.data(), str.size()) != str.size() - 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != str.size()) throw std::runtime_error("streambuf<char>::get fail");
        if (str.substr(0, str.size() - 1) != info.substr(1)) throw std::runtime_error("streambuf<char>::get fail");
    };

    streambuf obj1{mem_device{"chicago underground trio/possible cube on delmark"}};
    helper(obj1);

    istreambuf obj2{mem_device{"chicago underground trio/possible cube on delmark"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_streambuf_char_get6()
{
    dump_info("Test streambuf<char>::get 6...");
    using namespace IOv2;

    std::string info = "chicago underground trio/possible cube on delmark";
    
    auto helper = [&info](auto& obj)
    {
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");

        std::string str(info.size(), '\0');
        if (obj.sgetn(str.data(), 0) != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");

        if (obj.sgetn(str.data(), 1) != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (str[0] != 'c') throw std::runtime_error("streambuf<char>::get fail");

        if (obj.sgetc() != 'h') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");

        if (obj.sgetn(str.data(), str.size()) != str.size() - 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != str.size()) throw std::runtime_error("streambuf<char>::get fail");
        if (str.substr(0, str.size() - 1) != info.substr(1)) throw std::runtime_error("streambuf<char>::get fail");
    };

    streambuf obj1{mem_device{"chicago underground trio/possible cube on delmark"}};
    helper(obj1);

    istreambuf obj2{mem_device{"chicago underground trio/possible cube on delmark"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_streambuf_char_get7()
{
    dump_info("Test streambuf<char>::get 7...");
    using namespace IOv2;

    std::string info = "chicago underground trio/possible cube on delmark";
    
    auto helper = [&info](auto& obj)
    {
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");

        std::string str(info.size(), '\0');
        if (obj.sgetn(str.data(), 0) != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");

        if (obj.sgetn(str.data(), 1) != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (str[0] != 'h') throw std::runtime_error("streambuf<char>::get fail");

        if (obj.sbumpc() != 'i') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");

        if (obj.sgetn(str.data(), str.size()) != str.size() - 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != str.size()) throw std::runtime_error("streambuf<char>::get fail");
        if (str.substr(0, str.size() - 3) != info.substr(3)) throw std::runtime_error("streambuf<char>::get fail");
    };

    streambuf obj1{mem_device{"chicago underground trio/possible cube on delmark"}};
    helper(obj1);

    istreambuf obj2{mem_device{"chicago underground trio/possible cube on delmark"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_streambuf_char_get8()
{
    dump_info("Test streambuf<char>::get 8...");
    using namespace IOv2;

    std::string info = "chicago underground trio/possible cube on delmark";
    
    auto helper = [&info](auto& obj)
    {
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc() != 'h') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");

        std::string str(info.size(), '\0');
        if (obj.sgetn(str.data(), 0) != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");

        if (obj.sgetn(str.data(), 1) != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (str[0] != 'h') throw std::runtime_error("streambuf<char>::get fail");
        
        if (obj.snextc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");

        if (obj.sgetn(str.data(), str.size()) != str.size() - 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != str.size()) throw std::runtime_error("streambuf<char>::get fail");
        if (str.substr(0, str.size() - 3) != info.substr(3)) throw std::runtime_error("streambuf<char>::get fail");
    };

    streambuf obj1{mem_device{"chicago underground trio/possible cube on delmark"}};
    helper(obj1);

    istreambuf obj2{mem_device{"chicago underground trio/possible cube on delmark"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_streambuf_char_putback_1()
{
    dump_info("Test streambuf<char>::putback 1...");
    using namespace IOv2;
    
    auto helper = [](auto& obj)
    {
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        obj.sputbackc('x');
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        obj.sputbackc('y');
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'y') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'y') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'x') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        obj.sputbackc('1');
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != '1') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        obj.sputbackc('?');
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        obj.sputbackc('c');
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        obj.sputbackc('b');
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        obj.sputbackc('a');
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 2) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.snextc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sbumpc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.sgetc().has_value()) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::get fail");
    };

    streambuf obj1{mem_device{"abc"}};
    helper(obj1);

    istreambuf obj2{mem_device{"abc"}};
    helper(obj2);

    dump_info("Done\n");
}

void test_streambuf_char_put1()
{
    dump_info("Test streambuf<char>::put 1...");
    using namespace IOv2;

    auto helper1 = [](auto& obj)
    {
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        obj.sputc('x');
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        obj.flush();
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.device().str() != "x") throw std::runtime_error("streambuf<char>::put fail");
        
        obj.sputn("12345", 5);
        if (obj.tell() != 6) throw std::runtime_error("streambuf<char>::get fail");
        obj.flush();
        if (obj.tell() != 6) throw std::runtime_error("streambuf<char>::get fail");
        if (obj.device().str() != "x12345") throw std::runtime_error("streambuf<char>::put fail");
    };
    {
        streambuf obj1{mem_device{""}};
        helper1(obj1);

        ostreambuf obj2{mem_device{""}};
        helper1(obj2);
    }
    
    auto helper2 = [](auto& obj)
    {
        if (obj.tell() != 0) throw std::runtime_error("streambuf<char>::get fail");
        obj.sputc('x');
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        obj.flush();
        if (obj.device().str() != "liwei: x") throw std::runtime_error("streambuf<char>::put fail");
        
        if (obj.tell() != 1) throw std::runtime_error("streambuf<char>::get fail");
        obj.sputn("12345", 5);
        if (obj.tell() != 6) throw std::runtime_error("streambuf<char>::get fail");
        obj.flush();
        if (obj.device().str() != "liwei: x12345") throw std::runtime_error("streambuf<char>::put fail");
    };
    {
        mem_device dev{"liwei: "}; dev.drseek(0);
        streambuf obj1{std::move(dev)};
        helper2(obj1);

        mem_device dev2{"liwei: "}; dev2.drseek(0);
        ostreambuf obj2{std::move(dev2)};
        helper2(obj2);
    }

    dump_info("Done\n");
}

void test_streambuf_char_seek_1()
{
    dump_info("Test streambuf<char>::seek 1...");
    using namespace IOv2;
    
    streambuf obj{mem_device{"abcde"}};

    if (obj.sbumpc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
    obj.sputbackc('x');
    obj.seek(0);
    if (obj.sbumpc() != 'a') throw std::runtime_error("streambuf<char>::get fail");

    dump_info("Done\n");
}

void test_streambuf_char_io_switch_1()
{
    dump_info("Test streambuf<char> IO switch 1...");
    using namespace IOv2;
    
    streambuf obj{mem_device{"abcde"}};

    if (obj.sbumpc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
    obj.sputbackc('x');
    obj.switch_to_put();
    obj.switch_to_get();
    if (obj.sbumpc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
    if (obj.sbumpc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
    
    obj.sputbackc('x');
    obj.switch_to_put();
    obj.sputc('B');
    obj.flush();
    
    if (obj.device().str() != "aBcde") throw std::runtime_error("streambuf<char> IO switch fail");

    dump_info("Done\n");
}

void test_streambuf_char_io_switch_2()
{
    dump_info("Test streambuf<char> IO switch 2...");
    using namespace IOv2;
    
    streambuf obj{mem_device{"abcde"}};

    if (obj.sbumpc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
    obj.sputbackc('x');
    obj.switch_to_put();
    obj.switch_to_get();
    if (obj.sbumpc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
    if (obj.sbumpc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
    
    obj.sputbackc('x');
    obj.sputc('B');
    obj.flush();
    
    if (obj.device().str() != "aBcde") throw std::runtime_error("streambuf<char> IO switch fail");

    dump_info("Done\n");
}

void test_streambuf_char_io_switch_3()
{
    dump_info("Test streambuf<char> IO switch 3...");
    using namespace IOv2;
    
    streambuf obj{mem_device{"abcde"}};

    if (obj.sbumpc() != 'a') throw std::runtime_error("streambuf<char>::get fail");
    if (obj.sbumpc() != 'b') throw std::runtime_error("streambuf<char>::get fail");
    obj.sputbackc('x');
    obj.sputc('B');
    if (obj.sbumpc() != 'c') throw std::runtime_error("streambuf<char>::get fail");
    if (obj.tell() != 3) throw std::runtime_error("streambuf<char>::tell fail");
    obj.flush();
    
    if (obj.device().str() != "aBcde") throw std::runtime_error("streambuf<char> IO switch fail");

    dump_info("Done\n");
}