#include <cvt/root_cvt.h>
#include <cvt/comp/zlib_cvt.h>
#include <device/mem_device.h>
#include <io/streambuf.h>
#include <string>
#include <support/dump_info.h>
#include <support/verify.h>

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
            VERIFY(obj.tell() == 0);
            T obj2(obj);
            VERIFY(obj2.tell() == 0);
            VERIFY(obj2.device().str() == "hello");

            obj.sputn(" world", 6);
            VERIFY(obj.tell() == 6);
            VERIFY(obj2.tell() == 0);
            obj.flush();
            VERIFY(obj.device().str() == "hello world");
            VERIFY(obj2.device().str() == "hello");
        }

        {
            auto obj = ori_obj;
            decltype(obj) obj2{mem_device("")};
            obj2 = obj;
            VERIFY(obj.tell() == 0);
            VERIFY(obj2.tell() == 0);
            VERIFY(obj2.device().str() == "hello");

            obj.sputn(" world", 6);
            obj.flush();
            VERIFY(obj.tell() == 6);
            VERIFY(obj2.tell() == 0);
            VERIFY(obj.device().str() == "hello world");
            VERIFY(obj2.device().str() == "hello");
        }

        {
            auto obj = ori_obj;
            auto obj2(std::move(obj));
            VERIFY(obj2.tell() == 0);
            VERIFY(obj2.device().str() == "hello");
        }

        {
            auto obj = ori_obj;
            T obj2{mem_device("")};
            obj2 = std::move(obj);
            VERIFY(obj2.tell() == 0);
            VERIFY(obj2.device().str() == "hello");
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
            VERIFY(obj.sgetn(str.data(), 5) == 5);
            VERIFY(str == "hello");
            VERIFY(obj.tell() == 5);

            auto obj2(obj);
            VERIFY(obj2.tell() == 5);
            str.resize(6);
            VERIFY(obj2.sgetn(str.data(), 6) == 6);
            VERIFY(str == " world");
            VERIFY(obj2.tell() == 11);

            str = "xxxxxx";
            VERIFY(obj.sgetn(str.data(), 6) == 6);
            VERIFY(str == " world");
            VERIFY(obj.tell() == 11);
        }

        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            VERIFY(obj.sgetn(str.data(), 5) == 5);
            VERIFY(str == "hello");
            VERIFY(obj.tell() == 5);

            decltype(obj) obj2{mem_device("")};
            obj2 = obj;
            VERIFY(obj2.tell() == 5);
            str.resize(6);
            VERIFY(obj2.sgetn(str.data(), 6) == 6);
            VERIFY(str == " world");
            VERIFY(obj2.tell() == 11);

            str = "xxxxxx";
            VERIFY(obj.sgetn(str.data(), 6) == 6);
            VERIFY(str == " world");
            VERIFY(obj.tell() == 11);
        }

        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            VERIFY(obj.sgetn(str.data(), 5) == 5);
            VERIFY(str == "hello");
            VERIFY(obj.tell() == 5);

            auto obj2(std::move(obj));
            VERIFY(obj2.tell() == 5);
            str.resize(6);
            VERIFY(obj2.sgetn(str.data(), 6) == 6);
            VERIFY(str == " world");
            VERIFY(obj2.tell() == 11);
        }

        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            VERIFY(obj.sgetn(str.data(), 5) == 5);
            VERIFY(str == "hello");
            VERIFY(obj.tell() == 5);

            decltype(obj) obj2{mem_device("")};
            obj2 = std::move(obj);
            VERIFY(obj2.tell() == 5);
            str.resize(6);
            VERIFY(obj2.sgetn(str.data(), 6) == 6);
            VERIFY(str == " world");
            VERIFY(obj2.tell() == 11);
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
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sgetc() == 'a');
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sgetc() == 'a');
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sbumpc() == 'a');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.sgetc() == 'b');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.sgetc() == 'b');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.sbumpc() == 'b');
        VERIFY(obj.tell() == 2);
        VERIFY(obj.sgetc() == 'c');
        VERIFY(obj.tell() == 2);
        VERIFY(obj.sgetc() == 'c');
        VERIFY(obj.tell() == 2);
        VERIFY(obj.sbumpc() == 'c');
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.sgetc().has_value()));
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.sbumpc().has_value()));
        VERIFY(obj.tell() == 3);
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
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sbumpc() == 'a');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.sgetc() == 'b');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.sgetc() == 'b');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.sbumpc() == 'b');
        VERIFY(obj.tell() == 2);
        VERIFY(obj.sgetc() == 'c');
        VERIFY(obj.tell() == 2);
        VERIFY(obj.sgetc() == 'c');
        VERIFY(obj.tell() == 2);
        VERIFY(obj.sbumpc() == 'c');
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.sgetc().has_value()));
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.sbumpc().has_value()));
        VERIFY(obj.tell() == 3);
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
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sgetc() == 'a');
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sgetc() == 'a');
        VERIFY(obj.tell() == 0);
        VERIFY(obj.snextc() == 'b');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.sgetc() == 'b');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.snextc() == 'c');
        VERIFY(obj.tell() == 2);
        VERIFY(obj.sgetc() == 'c');
        VERIFY(obj.tell() == 2);
        VERIFY(obj.sbumpc() == 'c');
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.snextc().has_value()));
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.sbumpc().has_value()));
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.sgetc().has_value()));
        VERIFY(obj.tell() == 3);
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
        VERIFY(obj.tell() == 0);
        VERIFY(obj.snextc() == 'b');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.sgetc() == 'b');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.snextc() == 'c');
        VERIFY(obj.tell() == 2);
        VERIFY(obj.sgetc() == 'c');
        VERIFY(obj.tell() == 2);
        VERIFY(obj.sbumpc() == 'c');
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.snextc().has_value()));
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.sbumpc().has_value()));
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.sgetc().has_value()));
        VERIFY(obj.tell() == 3);
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
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sgetn(str.data(), 0) == 0);
        VERIFY(obj.tell() == 0);

        VERIFY(obj.sgetn(str.data(), 1) == 1);
        VERIFY(obj.tell() == 1);
        VERIFY(str[0] == 'c');

        VERIFY(obj.sgetn(str.data(), str.size()) == str.size() - 1);
        VERIFY(obj.tell() == str.size());
        VERIFY(str.substr(0, str.size() - 1) == info.substr(1));
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
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sgetc() == 'c');
        VERIFY(obj.tell() == 0);

        std::string str(info.size(), '\0');
        VERIFY(obj.sgetn(str.data(), 0) == 0);
        VERIFY(obj.tell() == 0);

        VERIFY(obj.sgetn(str.data(), 1) == 1);
        VERIFY(obj.tell() == 1);
        VERIFY(str[0] == 'c');

        VERIFY(obj.sgetc() == 'h');
        VERIFY(obj.tell() == 1);

        VERIFY(obj.sgetn(str.data(), str.size()) == str.size() - 1);
        VERIFY(obj.tell() == str.size());
        VERIFY(str.substr(0, str.size() - 1) == info.substr(1));
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
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sbumpc() == 'c');
        VERIFY(obj.tell() == 1);

        std::string str(info.size(), '\0');
        VERIFY(obj.sgetn(str.data(), 0) == 0);
        VERIFY(obj.tell() == 1);

        VERIFY(obj.sgetn(str.data(), 1) == 1);
        VERIFY(obj.tell() == 2);
        VERIFY(str[0] == 'h');

        VERIFY(obj.sbumpc() == 'i');
        VERIFY(obj.tell() == 3);

        VERIFY(obj.sgetn(str.data(), str.size()) == str.size() - 3);
        VERIFY(obj.tell() == str.size());
        VERIFY(str.substr(0, str.size() - 3) == info.substr(3));
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
        VERIFY(obj.tell() == 0);
        VERIFY(obj.snextc() == 'h');
        VERIFY(obj.tell() == 1);

        std::string str(info.size(), '\0');
        VERIFY(obj.sgetn(str.data(), 0) == 0);
        VERIFY(obj.tell() == 1);

        VERIFY(obj.sgetn(str.data(), 1) == 1);
        VERIFY(obj.tell() == 2);
        VERIFY(str[0] == 'h');

        VERIFY(obj.snextc() == 'c');
        VERIFY(obj.tell() == 3);

        VERIFY(obj.sgetn(str.data(), str.size()) == str.size() - 3);
        VERIFY(obj.tell() == str.size());
        VERIFY(str.substr(0, str.size() - 3) == info.substr(3));
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
        VERIFY(obj.tell() == 0);
        obj.sputbackc('x');
        VERIFY(obj.tell() == 0);
        obj.sputbackc('y');
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sgetc() == 'y');
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sbumpc() == 'y');
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sgetc() == 'x');
        VERIFY(obj.tell() == 0);
        VERIFY(obj.snextc() == 'a');
        VERIFY(obj.tell() == 0);
        obj.sputbackc('1');
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sbumpc() == '1');
        VERIFY(obj.tell() == 0);
        VERIFY(obj.sbumpc() == 'a');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.sbumpc() == 'b');
        VERIFY(obj.tell() == 2);
        obj.sputbackc('?');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.snextc() == 'c');
        VERIFY(obj.tell() == 2);
        VERIFY(!(obj.snextc().has_value()));
        VERIFY(obj.tell() == 3);
        obj.sputbackc('c');
        VERIFY(obj.tell() == 2);
        obj.sputbackc('b');
        VERIFY(obj.tell() == 1);
        obj.sputbackc('a');
        VERIFY(obj.tell() == 0);
        VERIFY(obj.snextc() == 'b');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.sgetc() == 'b');
        VERIFY(obj.tell() == 1);
        VERIFY(obj.snextc() == 'c');
        VERIFY(obj.tell() == 2);
        VERIFY(obj.sgetc() == 'c');
        VERIFY(obj.tell() == 2);
        VERIFY(obj.sbumpc() == 'c');
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.snextc().has_value()));
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.sbumpc().has_value()));
        VERIFY(obj.tell() == 3);
        VERIFY(!(obj.sgetc().has_value()));
        VERIFY(obj.tell() == 3);
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
        VERIFY(obj.tell() == 0);
        obj.sputc('x');
        VERIFY(obj.tell() == 1);
        obj.flush();
        VERIFY(obj.tell() == 1);
        VERIFY(obj.device().str() == "x");

        obj.sputn("12345", 5);
        VERIFY(obj.tell() == 6);
        obj.flush();
        VERIFY(obj.tell() == 6);
        VERIFY(obj.device().str() == "x12345");
    };
    {
        streambuf obj1{mem_device{""}};
        helper1(obj1);

        ostreambuf obj2{mem_device{""}};
        helper1(obj2);
    }
    
    auto helper2 = [](auto& obj)
    {
        VERIFY(obj.tell() == 0);
        obj.sputc('x');
        VERIFY(obj.tell() == 1);
        obj.flush();
        VERIFY(obj.device().str() == "liwei: x");

        VERIFY(obj.tell() == 1);
        obj.sputn("12345", 5);
        VERIFY(obj.tell() == 6);
        obj.flush();
        VERIFY(obj.device().str() == "liwei: x12345");
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

    VERIFY(obj.sbumpc() == 'a');
    obj.sputbackc('x');
    obj.seek(0);
    VERIFY(obj.sbumpc() == 'a');

    dump_info("Done\n");
}

void test_streambuf_char_io_switch_1()
{
    dump_info("Test streambuf<char> IO switch 1...");
    using namespace IOv2;

    streambuf obj{mem_device{"abcde"}};

    VERIFY(obj.sbumpc() == 'a');
    obj.sputbackc('x');
    obj.switch_to_put();
    obj.switch_to_get();
    VERIFY(obj.sbumpc() == 'a');
    VERIFY(obj.sbumpc() == 'b');

    obj.sputbackc('x');
    obj.switch_to_put();
    obj.sputc('B');
    obj.flush();

    VERIFY(obj.device().str() == "aBcde");

    dump_info("Done\n");
}

void test_streambuf_char_io_switch_2()
{
    dump_info("Test streambuf<char> IO switch 2...");
    using namespace IOv2;

    streambuf obj{mem_device{"abcde"}};

    VERIFY(obj.sbumpc() == 'a');
    obj.sputbackc('x');
    obj.switch_to_put();
    obj.switch_to_get();
    VERIFY(obj.sbumpc() == 'a');
    VERIFY(obj.sbumpc() == 'b');

    obj.sputbackc('x');
    obj.sputc('B');
    obj.flush();

    VERIFY(obj.device().str() == "aBcde");

    dump_info("Done\n");
}

void test_streambuf_char_io_switch_3()
{
    dump_info("Test streambuf<char> IO switch 3...");
    using namespace IOv2;

    streambuf obj{mem_device{"abcde"}};

    VERIFY(obj.sbumpc() == 'a');
    VERIFY(obj.sbumpc() == 'b');
    obj.sputbackc('x');
    obj.sputc('B');
    VERIFY(obj.sbumpc() == 'c');
    VERIFY(obj.tell() == 3);
    obj.flush();

    VERIFY(obj.device().str() == "aBcde");

    dump_info("Done\n");
}

void test_streambuf_char_detach_1()
{
    dump_info("Test streambuf<char>::detach with buffered read...");
    using namespace IOv2;

    // detach() with a non-empty read buffer over a positionable converter:
    // the buffered/pushed-back lookahead is rewound (tell()+seek()) before the
    // device is handed back, so the returned device is positioned at the logical
    // read cursor.
    {
        streambuf obj{mem_device{"abcde"}};
        VERIFY(obj.sgetc() == 'a');   // fills the read buffer with a lookahead 'a'
        auto [dev, err] = obj.detach();
        VERIFY(!err);
        VERIFY(dev.str() == "abcde");

        // Re-reading from the returned device begins at the logical read cursor.
        istreambuf again{std::move(dev)};
        VERIFY(again.sbumpc() == 'a');
    }

    // Same, but on an istreambuf and after consuming a couple of characters so
    // the physical cursor is genuinely ahead of the logical one.
    {
        istreambuf obj{mem_device{"abcde"}};
        VERIFY(obj.sbumpc() == 'a');
        VERIFY(obj.sgetc() == 'b');   // buffered lookahead 'b'
        VERIFY(obj.tell() == 1);
        auto [dev, err] = obj.detach();
        VERIFY(!err);
        istreambuf again{std::move(dev)};
        VERIFY(again.sbumpc() == 'b');
    }

    dump_info("Done\n");
}

void test_streambuf_char_detach_2()
{
    dump_info("Test streambuf<char>::detach with buffered read over non-positionable cvt...");
    using namespace IOv2;

    // Build a zlib-compressed payload.
    std::string payload = "the quick brown fox jumps over the lazy dog";
    std::string comp;
    {
        ostreambuf ost{mem_device{""}, Comp::zlib_cvt_creator<char>{6}};
        ost.sputn(payload.data(), payload.size());
        ost.flush();
        auto [dev, err] = ost.detach();
        VERIFY(!err);
        comp = dev.str();
        VERIFY(!comp.empty());
    }

    // detach() with a non-empty read buffer over a converter that does NOT
    // support positioning (zlib): the attempted rewind fails and is swallowed
    // on purpose (see base_streambuf::detach), so detach() still succeeds and
    // reports no error - the lookahead character is the accepted, unavoidable
    // loss for a non-positionable stream.
    {
        istreambuf isb{mem_device{comp}, Comp::zlib_cvt_creator<char>{6}};
        VERIFY(isb.sgetc() == payload.front());  // buffers a lookahead char
        auto [dev, err] = isb.detach();
        VERIFY(!err);
    }

    dump_info("Done\n");
}