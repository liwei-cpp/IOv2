#include <cvt/crypt/hash_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <common/dump_info.h>
#include <common/verify.h>

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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("he", 2);
        auto obj2(obj);
        obj2.put("llo", 3);
        auto [dev, err] = obj2.detach();
        VERIFY(dev.str() == hello_hex_low);
        auto [dev2, err2] = obj.detach();
        VERIFY(dev2.str() != hello_hex_low);
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(rb_root_cvt{mem_device{""}});
    helper(obj);

    auto tmp = creator.create(rb_root_cvt{mem_device{""}});
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("he", 2);
        T obj2(creator.create(rb_root_cvt{mem_device{""}}));
        obj2 = obj;
        obj2.put("llo", 3);
        auto [dev, err] = obj2.detach();
        VERIFY(dev.str() == hello_hex_low);
        auto [dev2, err2] = obj.detach();
        VERIFY(dev2.str() != hello_hex_low);
    };

    auto obj = creator.create(rb_root_cvt{mem_device{""}});
    helper(obj);

    auto tmp = creator.create(rb_root_cvt{mem_device{""}});
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("he", 2);
        auto obj2(std::move(obj));
        obj2.put("llo", 3);
        auto [dev, err] = obj2.detach();
        VERIFY(dev.str() == hello_hex_low);
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(rb_root_cvt{mem_device{""}});
    helper(obj);

    auto tmp = creator.create(rb_root_cvt{mem_device{""}});
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("he", 2);
        T obj2(creator.create(rb_root_cvt{mem_device{""}}));
        obj2 = std::move(obj);
        obj2.put("llo", 3);
        auto [dev, err] = obj2.detach();
        VERIFY(dev.str() == hello_hex_low);
    };

    auto obj = creator.create(rb_root_cvt{mem_device{""}});
    helper(obj);

    auto tmp = creator.create(rb_root_cvt{mem_device{""}});
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();

        auto [dev, err] = obj.detach();
        VERIFY(dev.str().empty());
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(rb_root_cvt{mem_device{""}});
    helper(obj);

    auto tmp = creator.create(rb_root_cvt{mem_device{""}});
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
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put("hello", 5);
            auto [dev, err] = obj.detach();
            obj.attach();
            VERIFY(dev.str() == hello_hex_low);
        }

        {
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::lower_hex));
            obj.put("hello", 5);
            auto [dev, err] = obj.detach();
            obj.attach();
            VERIFY(dev.str() == hello_hex_low);
        }
        
        {
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::upper_hex));
            obj.put("hello", 5);
            auto [dev, err] = obj.detach();
            obj.attach();
            VERIFY(dev.str() == hello_hex_up);
        }
        
        {
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::binary));
            obj.put("hello", 5);
            auto [dev, err] = obj.detach();
            obj.attach();
            VERIFY(dev.str() == hello_binary);
        }
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(rb_root_cvt{mem_device{""}});
    helper(obj);

    auto tmp = creator.create(rb_root_cvt{mem_device{""}});
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
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put("hello", 5);
            obj.adjust(Crypt::dump_hash('\n'));
            obj.put("hello", 5);
            auto [dev, err] = obj.detach();
            obj.attach();
            VERIFY(dev.str() == hello_hex_low + '\n' + hello_hex_low);
        }

        {
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put("hello", 5);
            obj.adjust(Crypt::dump_hash('*'));
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::upper_hex));
            obj.put("hello", 5);
            auto [dev, err] = obj.detach();
            obj.attach();
            VERIFY(dev.str() == hello_hex_low + '*' + hello_hex_up);
        }
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(rb_root_cvt{mem_device{""}});
    helper(obj);

    auto tmp = creator.create(rb_root_cvt{mem_device{""}});
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
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("he", 2);
        obj.put("llo", 3);
        auto [dev, err] = obj.detach();
        VERIFY(dev.str() == hello_hex_low);
    };

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(rb_root_cvt{mem_device{""}});
    helper(obj);

    auto tmp = creator.create(rb_root_cvt{mem_device{""}});
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
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.put(u8"李伟", 6);
            auto [dev, err] = obj.detach();
            obj.attach();
            VERIFY(dev.str() == liwei_hex_low);
        }

        {
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::lower_hex));
            obj.put(u8"李伟", 6);
            auto [dev, err] = obj.detach();
            obj.attach();
            VERIFY(dev.str() == liwei_hex_low);
        }
    
        {
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::upper_hex));
            obj.put(u8"李伟", 6);
            auto [dev, err] = obj.detach();
            obj.attach();
            VERIFY(dev.str() == liwei_hex_up);
        }
    
        {
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
            obj.adjust(Crypt::set_hash_fmt(Crypt::hash_fmt::binary));
            obj.put(u8"李伟", 6);
            auto [dev, err] = obj.detach();
            obj.attach();
            VERIFY(dev.str() == liwei_binary);
        }
    };

    Crypt::hash_cvt_creator<char8_t> creator(Crypt::hash_algo::SHA256);
    auto obj = creator.create(rb_root_cvt{mem_device{u8""}});
    helper(obj);

    auto tmp = creator.create(rb_root_cvt{mem_device{u8""}});
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_sha256_cvt_adjust_1()
{
    using namespace IOv2;
    dump_info("Test sha256_cvt adjust edge cases...");

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);

    // adjust(dump_hash) when no main content yet → no-op (m_has_main_cont == false)
    {
        auto obj = creator.create(rb_root_cvt{mem_device{""}});
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        // No put() before dump_hash → m_has_main_cont is false → adjust is a no-op
        obj.adjust(Crypt::dump_hash{});
        auto [dev, err] = obj.detach();
        VERIFY(dev.str().empty());
    }

    // adjust(dump_hash) without delimiter when there IS main content
    {
        auto obj = creator.create(rb_root_cvt{mem_device{""}});
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("hello", 5);
        obj.adjust(Crypt::dump_hash{});      // no delimiter
        obj.put("hello", 5);
        auto [dev, err] = obj.detach();
        // Two SHA-256 digests of "hello" concatenated, lower-hex format
        VERIFY(dev.str() == hello_hex_low + hello_hex_low);
    }

    // adjust(unrecognised cvt_behavior) → silently ignored
    {
        struct unknown_behavior : cvt_behavior {};
        auto obj = creator.create(rb_root_cvt{mem_device{""}});
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("hello", 5);
        obj.adjust(unknown_behavior{});
        auto [dev, err] = obj.detach();
        VERIFY(dev.str() == hello_hex_low);
    }

    // bos_impl() in input mode → hash_cvt only supports output mode, must throw
    {
        // Device with content → bos() detects input mode → bos_impl() throws
        auto obj = creator.create(rb_root_cvt{mem_device{hello_hex_low}});
        bool threw = false;
        try { obj.bos(); }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // attach_impl() with null m_hash (moved-from object) → throws
    {
        auto src = creator.create(rb_root_cvt{mem_device{""}});
        auto moved = std::move(src);
        // src is now moved-from: m_hash is null
        bool threw = false;
        try { src.attach(); }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}

void test_sha256_cvt_assign_1()
{
    using namespace IOv2;
    dump_info("Test sha256_cvt assignment/destructor edge cases...");

    Crypt::hash_cvt_creator<char> creator(Crypt::hash_algo::SHA256);

    // Copy-assign to object with m_has_main_cont == true (hits line 265: dump_stream in copy-assign)
    {
        auto obj1 = creator.create(rb_root_cvt{mem_device{""}});
        auto obj2 = creator.create(rb_root_cvt{mem_device{""}});
        obj1.bos(); obj1.main_cont_beg(); obj1.put("hello", 5);
        obj2.bos(); obj2.main_cont_beg(); obj2.put("hello", 5);
        // Copy-assign: obj2.m_has_main_cont == true → dump_stream() flushed first
        obj2 = obj1;
        auto [dev, err] = obj2.detach();
        VERIFY(!err);
        VERIFY(dev.str() == hello_hex_low);
    }

    // Move-assign to object with m_has_main_cont == true (hits line 282: try{dump_stream} in move-assign)
    {
        auto obj1 = creator.create(rb_root_cvt{mem_device{""}});
        auto obj2 = creator.create(rb_root_cvt{mem_device{""}});
        obj1.bos(); obj1.main_cont_beg(); obj1.put("hello", 5);
        obj2.bos(); obj2.main_cont_beg(); obj2.put("hello", 5);
        // Move-assign: obj2.m_has_main_cont == true → dump_stream() called first
        obj2 = std::move(obj1);
        auto [dev, err] = obj2.detach();
        VERIFY(!err);
        VERIFY(dev.str() == hello_hex_low);
    }

    // Destructor with m_has_main_cont == true (hits line 299: try{dump_stream} in destructor)
    {
        auto obj = creator.create(rb_root_cvt{mem_device{""}});
        obj.bos(); obj.main_cont_beg(); obj.put("hello", 5);
        // obj goes out of scope here; destructor calls dump_stream
    }

    // bos_impl() with null m_hash (moved-from object) — hits line 412
    {
        auto src = creator.create(rb_root_cvt{mem_device{""}});
        auto moved = std::move(src);
        bool threw = false;
        try { src.bos(); }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // algo_to_str default case (invalid hash_algo) — hits lines 550-551
    {
        bool threw = false;
        try
        {
            using KernelType = rb_root_cvt<mem_device<char>>;
            Crypt::hash_cvt<KernelType> bad(rb_root_cvt{mem_device{""}},
                                            static_cast<Crypt::hash_algo>(255));
        }
        catch (...) { threw = true; }
        VERIFY(threw);
    }

    // detach_impl error path via dump_stream with invalid fmt — hits lines 342-354 and 635-636
    {
        auto obj = creator.create(rb_root_cvt{mem_device{""}});
        VERIFY(obj.bos() == io_status::output);
        obj.main_cont_beg();
        obj.put("hello", 5);
        // Set an invalid output format so dump_stream throws inside detach_impl
        obj.adjust(Crypt::set_hash_fmt{static_cast<Crypt::hash_fmt>(255)});
        auto [dev, err] = obj.detach();
        VERIFY(err != nullptr);
    }

    dump_info("Done\n");
}