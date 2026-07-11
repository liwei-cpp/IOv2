#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <locale/locale.h>
#include <support/dump_info.h>
#include <support/verify.h>

void test_ostream_callbacks_1()
{
    dump_info("Test ostream callback case 1...");
    using namespace IOv2;

    auto helper = []<typename T>()
    {
        const std::string str01("the nubians of plutonia");
        std::string str02;
    
        std::function<std::shared_ptr<void>(const locale<char>&, std::shared_ptr<void>)> callb01
            = [&str02](const locale<char>&, std::shared_ptr<void> res)
            {
                str02 += "the nubians";
                return res;
            };
    
        std::function<std::shared_ptr<void>(const locale<char>&, std::shared_ptr<void>)> callb02
            = [&str02](const locale<char>&, std::shared_ptr<void> res)
            {
                str02 += " of ";
                return res;
            };
    
        std::function<std::shared_ptr<void>(const locale<char>&, std::shared_ptr<void>)> callb03
            = [&str02](const locale<char>&, std::shared_ptr<void> res)
            {
                str02 += "plutonia";
                return res;
            };
    
        using namespace IOv2;
        T ios01;
        ios01.register_callback(callb03, 1);
        ios01.register_callback(callb02, 1);
        ios01.register_callback(callb01, 1);
        IOv2::locale<char> loc("C");
        ios01.locale(loc);
        VERIFY(str01 == str02);
    };
    
    helper.operator()<ostream<mem_device<char>, char>>();
    helper.operator()<iostream<mem_device<char>, char>>();

    dump_info("Done\n");
}

void test_ostream_callbacks_sync_1()
{
    dump_info("Test ostream callback (with sync) case 1...");
    using namespace IOv2;

    auto helper = []<typename T>()
    {
        const std::string str01("the nubians of plutonia");
        std::string str02;
    
        std::function<std::shared_ptr<void>(const locale<char>&, std::shared_ptr<void>)> callb01
            = [&str02](const locale<char>&, std::shared_ptr<void> res)
            {
                str02 += "the nubians";
                return res;
            };
    
        std::function<std::shared_ptr<void>(const locale<char>&, std::shared_ptr<void>)> callb02
            = [&str02](const locale<char>&, std::shared_ptr<void> res)
            {
                str02 += " of ";
                return res;
            };
    
        std::function<std::shared_ptr<void>(const locale<char>&, std::shared_ptr<void>)> callb03
            = [&str02](const locale<char>&, std::shared_ptr<void> res)
            {
                str02 += "plutonia";
                return res;
            };
    
        using namespace IOv2;
        T ios01;
        IOv2::sync(ios01).stream.register_callback(callb03, 1);
        IOv2::sync(ios01).stream.register_callback(callb02, 1);
        IOv2::sync(ios01).stream.register_callback(callb01, 1);
        IOv2::locale<char> loc("C");
        IOv2::sync(ios01).stream.locale(loc);
        VERIFY(str01 == str02);
    };
    
    helper.operator()<ostream<mem_device<char>, char>>();
    helper.operator()<iostream<mem_device<char>, char>>();

    dump_info("Done\n");
}

void test_ostream_callbacks_pword_1()
{
    dump_info("Test ostream callback pword mutation case 1...");
    using namespace IOv2;

    using cb_t = std::function<std::shared_ptr<void>(const locale<char>&, std::shared_ptr<void>)>;
    locale<char> loc("C");

    // insert path: id has no pword yet, callback returns new data -> inserted.
    {
        ostream<mem_device<char>, char> ios{mem_device<char>{""}};
        auto data = std::make_shared<int>(42);
        ios.register_callback(cb_t{[data](const locale<char>&, std::shared_ptr<void>)
                                   { return data; }},
                              5);
        ios.locale(loc);
        VERIFY(ios.get_pword(5) == data);
    }

    // replace path: id already has a pword, callback returns different data.
    {
        ostream<mem_device<char>, char> ios{mem_device<char>{""}};
        auto old_data = std::make_shared<int>(1);
        auto new_data = std::make_shared<int>(2);
        ios.set_pword(5, old_data);
        ios.register_callback(cb_t{[new_data](const locale<char>&, std::shared_ptr<void>)
                                   { return new_data; }},
                              5);
        ios.locale(loc);
        VERIFY(ios.get_pword(5) == new_data);
    }

    // erase path: id already has a pword, callback returns nullptr -> erased.
    {
        ostream<mem_device<char>, char> ios{mem_device<char>{""}};
        auto old_data = std::make_shared<int>(1);
        ios.set_pword(5, old_data);
        ios.register_callback(cb_t{[](const locale<char>&, std::shared_ptr<void>)
                                   { return std::shared_ptr<void>{}; }},
                              5);
        ios.locale(loc);
        VERIFY(ios.get_pword(5) == nullptr);
    }

    // throwing callbacks: access_callbacks() captures the first exception and
    // rethrows it after all callbacks have run; a second throwing callback
    // exercises the already-have-an-exception branch. locale() routes the
    // rethrown exception through handle_exception(), leaving the stream failed.
    {
        ostream<mem_device<char>, char> ios{mem_device<char>{""}};
        ios.register_callback(cb_t{[](const locale<char>&, std::shared_ptr<void>) -> std::shared_ptr<void>
                                   { throw stream_error("cb boom 1"); }},
                              5);
        ios.register_callback(cb_t{[](const locale<char>&, std::shared_ptr<void>) -> std::shared_ptr<void>
                                   { throw stream_error("cb boom 2"); }},
                              5);
        ios.locale(loc);
        VERIFY(!static_cast<bool>(ios));
        VERIFY(ios.str_fail());
    }

    dump_info("Done\n");
}

void test_ostream_callbacks()
{
    test_ostream_callbacks_1();
    test_ostream_callbacks_sync_1();
    test_ostream_callbacks_pword_1();
}