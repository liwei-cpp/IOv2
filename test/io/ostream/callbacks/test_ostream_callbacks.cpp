#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <locale/locale.h>
#include <common/dump_info.h>
#include <common/verify.h>

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

void test_ostream_callbacks()
{
    test_ostream_callbacks_1();
    test_ostream_callbacks_sync_1();
}