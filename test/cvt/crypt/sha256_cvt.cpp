#include <cvt/crypt/hash_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <common/dump_info.h>

namespace
{
    std::string hello_hex_low = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
    std::string hello_hex_up  = "2CF24DBA5FB0A30E26E83B2AC5B9E29E1B161E5C1FA7425E73043362938B9824";
    std::string hello_binary  = "\x2C\xF2\x4D\xBA\x5F\xB0\xA3\x0E\x26\xE8\x3B\x2A\xC5\xB9\xE2\x9E\x1B\x16\x1E\x5C\x1F\xA7\x42\x5E\x73\x04\x33\x62\x93\x8B\x98\x24";

    std::u8string liwei_hex_low = u8"5ed4d58e5d8948c2aa2f9ba57667cbb6c63c679c045b0d6009bac9060e66ec45";
    std::u8string liwei_hex_up  = u8"5ED4D58E5D8948C2AA2F9BA57667CBB6C63C679C045B0D6009BAC9060E66EC45";
    std::u8string liwei_binary  = u8"\x5E\xD4\xD5\x8E\x5D\x89\x48\xC2\xAA\x2F\x9B\xA5\x76\x67\xCB\xB6\xC6\x3C\x67\x9C\x04\x5B\x0D\x60\x09\xBA\xC9\x06\x0E\x66\xEC\x45";
    
}

void test_sha256_cvt_gen_1()
{
    using namespace IOv2;
    dump_info("Test sha256_cvt general case 1...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put("he", 2);
        auto obj2(obj);
        obj2.put("llo", 3);
        auto dev = obj2.detach();
        if (dev.str() != hello_hex_low) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
        auto dev2 = obj.detach();
        if (dev2.str() == hello_hex_low) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_sha256_cvt_gen_2()
{
    using namespace IOv2;
    dump_info("Test sha256_cvt general case 2...");
    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);

    auto helper = [&creator]<typename T>(T& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put("he", 2);
        T obj2(creator.create(make_root_cvt<true>(mem_device{""})));
        obj2 = obj;
        obj2.put("llo", 3);
        auto dev = obj2.detach();
        if (dev.str() != hello_hex_low) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
        auto dev2 = obj.detach();
        if (dev2.str() == hello_hex_low) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
    };

    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_sha256_cvt_gen_3()
{
    using namespace IOv2;
    dump_info("Test sha256_cvt general case 3...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put("he", 2);
        auto obj2(std::move(obj));
        obj2.put("llo", 3);
        auto dev = obj2.detach();
        if (dev.str() != hello_hex_low) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_sha256_cvt_gen_4()
{
    using namespace IOv2;
    dump_info("Test sha256_cvt general case 4...");
    
    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto helper = [&creator]<typename T>(T& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put("he", 2);
        T obj2(creator.create(make_root_cvt<true>(mem_device{""})));
        obj2 = std::move(obj);
        obj2.put("llo", 3);
        auto dev = obj2.detach();
        if (dev.str() != hello_hex_low) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
    };

    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_sha256_cvt_put_1()
{
    using namespace IOv2;
    dump_info("Test sha256_cvt::put case 1...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        auto dev = obj.detach();
        if (!dev.str().empty()) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_sha256_cvt_put_2()
{
    using namespace IOv2;
    dump_info("Test sha256_cvt::put case 2...");

    auto helper = [](auto& obj)
    {
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.put("hello", 5);
            auto dev = obj.attach();
            if (dev.str() != hello_hex_low) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
        }
        
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::lower_hex));
            obj.put("hello", 5);
            auto dev = obj.attach();
            if (dev.str() != hello_hex_low) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
        }
        
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::upper_hex));
            obj.put("hello", 5);
            auto dev = obj.attach();
            if (dev.str() != hello_hex_up) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
        }
        
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::binary));
            obj.put("hello", 5);
            auto dev = obj.attach();
            if (dev.str() != hello_binary) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
        }
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_sha256_cvt_put_3()
{
    using namespace IOv2;
    dump_info("Test sha256_cvt::put case 3...");

    auto helper = [](auto& obj)
    {
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.put("hello", 5);
            obj.adjust(Crypt::dump_hash('\n'));
            obj.put("hello", 5);
            auto dev = obj.attach();
            if (dev.str() != hello_hex_low + '\n' + hello_hex_low) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
        }

        {
            if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.put("hello", 5);
            obj.adjust(Crypt::dump_hash('*'));
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::upper_hex));
            obj.put("hello", 5);
            auto dev = obj.attach();
            if (dev.str() != hello_hex_low + '*' + hello_hex_up) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
        }
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_sha256_cvt_put_4()
{
    using namespace IOv2;
    dump_info("Test sha256_cvt::put case 4...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put("he", 2);
        obj.put("llo", 3);
        auto dev = obj.detach();
        if (dev.str() != hello_hex_low) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_sha256_cvt_put_5()
{
    using namespace IOv2;
    dump_info("Test sha256_cvt::put case 5...");

    auto helper = [](auto& obj)
    {
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.put(u8"李伟", 6);
            auto dev = obj.attach();
            if (dev.str() != liwei_hex_low) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
        }

        {
            if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::lower_hex));
            obj.put(u8"李伟", 6);
            auto dev = obj.attach();
            if (dev.str() != liwei_hex_low) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
        }
    
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::upper_hex));
            obj.put(u8"李伟", 6);
            auto dev = obj.attach();
            if (dev.str() != liwei_hex_up) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
        }
    
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("sha256_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::binary));
            obj.put(u8"李伟", 6);
            auto dev = obj.attach();
            if (dev.str() != liwei_binary) throw std::runtime_error("sha256_cvt<mem_device>::put response incorrect");
        }
    };

    Crypt::hash_cvt_creator<char8_t> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(make_root_cvt<true>(mem_device{u8""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{u8""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);
    
    dump_info("Done\n");
}