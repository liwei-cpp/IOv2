#include <cvt/crypt/hash_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <common/dump_info.h>

namespace
{
    std::string hello_md5_hex_low = "5d41402abc4b2a76b9719d911017c592";
    std::string hello_md5_hex_up  = "5D41402ABC4B2A76B9719D911017C592";
    std::string hello_md5_binary  = "\x5D\x41\x40\x2A\xBC\x4B\x2A\x76\xB9\x71\x9D\x91\x10\x17\xC5\x92";

    std::u8string liwei_md5_hex_low = u8"ffb031550e9681adbe2223cc408d48fc";
    std::u8string liwei_md5_hex_up  = u8"FFB031550E9681ADBE2223CC408D48FC";
    std::u8string liwei_md5_binary  = u8"\xFF\xB0\x31\x55\x0E\x96\x81\xAD\xBE\x22\x23\xCC\x40\x8D\x48\xFC";
}

void test_md5_cvt_gen_1()
{
    using namespace IOv2;
    dump_info("Test md5_cvt general case 1...");
    
    {
        using CheckType = Crypt::hash_cvt<root_cvt<mem_device<char>, true>>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, char>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(!cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = Crypt::hash_cvt<root_cvt<mem_device<char8_t>, false>>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
        static_assert(std::is_same_v<CheckType::internal_type, char8_t>);
        static_assert(std::is_same_v<CheckType::external_type, char8_t>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(!cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }

    dump_info("Done\n");
}

void test_md5_cvt_gen_2()
{
    using namespace IOv2;
    dump_info("Test md5_cvt general case 2...");
    
    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put("he", 2);
        auto obj2(obj);
        obj2.put("llo", 3);
        auto dev = obj2.detach();
        if (dev.str() != hello_md5_hex_low) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
        auto dev2 = obj.detach();
        if (dev2.str() == hello_md5_hex_low) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::MD5);
    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_md5_cvt_gen_3()
{
    using namespace IOv2;
    dump_info("Test md5_cvt general case 3...");

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::MD5);
    auto helper = [&creator]<typename T>(T& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put("he", 2);
        T obj2(creator.create(make_root_cvt<true>(mem_device{""})));
        obj2 = obj;
        obj2.put("llo", 3);
        auto dev = obj2.detach();
        if (dev.str() != hello_md5_hex_low) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
        auto dev2 = obj.detach();
        if (dev2.str() == hello_md5_hex_low) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
    };

    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_md5_cvt_gen_4()
{
    using namespace IOv2;
    dump_info("Test md5_cvt general case 4...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put("he", 2);
        auto obj2(std::move(obj));
        obj2.put("llo", 3);
        auto dev = obj2.detach();
        if (dev.str() != hello_md5_hex_low) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::MD5);
    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_md5_cvt_gen_5()
{
    using namespace IOv2;
    dump_info("Test md5_cvt general case 5...");
    
    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::MD5);
    auto helper = [&creator]<typename T>(T& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put("he", 2);
        T obj2(creator.create(make_root_cvt<true>(mem_device{""})));
        obj2 = std::move(obj);
        obj2.put("llo", 3);
        auto dev = obj2.detach();
        if (dev.str() != hello_md5_hex_low) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
    };

    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_md5_cvt_put_1()
{
    using namespace IOv2;
    dump_info("Test md5_cvt::put case 1...");
    
    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();

        auto dev = obj.detach();
        if (!dev.str().empty()) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::MD5);
    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);
    
    dump_info("Done\n");
}

void test_md5_cvt_put_2()
{
    using namespace IOv2;
    dump_info("Test md5_cvt::put case 2...");

    auto helper = [](auto& obj)
    {
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.put("hello", 5);
            auto dev = obj.attach();
            if (dev.str() != hello_md5_hex_low) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
        }
        
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::lower_hex));
            obj.put("hello", 5);
            auto dev = obj.attach();
            if (dev.str() != hello_md5_hex_low) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
        }
        
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::upper_hex));
            obj.put("hello", 5);
            auto dev = obj.attach();
            if (dev.str() != hello_md5_hex_up) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
        }
        
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::binary));
            obj.put("hello", 5);
            auto dev = obj.attach();
            if (dev.str() != hello_md5_binary) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
        }
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::MD5);
    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_md5_cvt_put_3()
{
    using namespace IOv2;
    dump_info("Test md5_cvt::put case 3...");

    auto helper = [](auto& obj)
    {
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.put("hello", 5);
            obj.adjust(Crypt::dump_hash('\n'));
            obj.put("hello", 5);
            auto dev = obj.attach();
            if (dev.str() != hello_md5_hex_low + '\n' + hello_md5_hex_low) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
        }

        {
            if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.put("hello", 5);
            obj.adjust(Crypt::dump_hash('*'));
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::upper_hex));
            obj.put("hello", 5);
            auto dev = obj.detach();
            if (dev.str() != hello_md5_hex_low + '*' + hello_md5_hex_up) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
        }
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::MD5);
    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_md5_cvt_put_4()
{
    using namespace IOv2;
    dump_info("Test md5_cvt::put case 4...");

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
        obj.main_cont_beg();
        obj.put("he", 2);
        obj.put("llo", 3);
        auto dev = obj.detach();
        if (dev.str() != hello_md5_hex_low) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::MD5);
    auto obj = creator.create(make_root_cvt<true>(mem_device{""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_md5_cvt_put_5()
{
    using namespace IOv2;
    dump_info("Test md5_cvt::put case 5...");

    auto helper = [](auto& obj)
    {
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.put(u8"李伟", 6);
            auto dev = obj.attach();
            if (dev.str() != liwei_md5_hex_low) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
        }

        {
            if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::lower_hex));
            obj.put(u8"李伟", 6);
            auto dev = obj.attach();
            if (dev.str() != liwei_md5_hex_low) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
        }

        {
            if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::upper_hex));
            obj.put(u8"李伟", 6);
            auto dev = obj.attach();
            if (dev.str() != liwei_md5_hex_up) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
        }
    
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("md5_cvt<mem_device>::bos response incorrect");
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::binary));
            obj.put(u8"李伟", 6);
            auto dev = obj.attach();
            if (dev.str() != liwei_md5_binary) throw std::runtime_error("md5_cvt<mem_device>::put response incorrect");
        }
    };
    
    Crypt::hash_cvt_creator<char8_t> creator(Crypt::hash_algo::MD5);
    auto obj = creator.create(make_root_cvt<true>(mem_device{u8""}));
    helper(obj);

    auto tmp = creator.create(make_root_cvt<true>(mem_device{u8""}));
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}