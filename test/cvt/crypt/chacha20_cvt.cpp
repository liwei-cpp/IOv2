#include <cvt/crypt/chacha20_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <common/dump_info.h>
#include <common/verify.h>

#include <botan/secmem.h>

void test_chacha20_cvt_gen_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt general case 1...");
    
    {
        using CheckType = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, char>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = Crypt::chacha20_cvt<no_rb_root_cvt<mem_device<char8_t>>>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
        static_assert(std::is_same_v<CheckType::internal_type, char8_t>);
        static_assert(std::is_same_v<CheckType::external_type, char8_t>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }

    dump_info("Done\n");
}

void test_chacha20_cvt_gen_2()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt general case 2...");

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

    auto helper = [&e_lit]<typename T>(T& obj)
    {
        std::string enc_msg;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("chacha20::bos response incorrect");
            obj.main_cont_beg();
    
            size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            size_t total_count = 0;
            char* cur_pos = e_lit.data();
            int buffer_id = 0;
            while (total_count < 4102)
            {
                auto obj2(std::move(obj));
                size_t dest_size = std::min<size_t>(4102 - total_count, buffer_size[buffer_id++]);
                obj2.put(cur_pos, dest_size);
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
                total_count += dest_size;
                obj = std::move(obj2);
            }
    
            auto [dev, err] = obj.detach();
            enc_msg = dev.str();
        }
    
        {
            T local_obj(Crypt::chacha20_cvt{rb_root_cvt{mem_device(enc_msg)}, "liwei"});
            if (local_obj.bos() != io_status::input) throw std::runtime_error("chacha20::bos response incorrect");
            local_obj.main_cont_beg();
            
            size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
        
            std::string buf; buf.resize(4102 * 2);
            size_t total_count = 0;
            char* cur_pos = buf.data();
            int out_buffer_id = 0;
            while (true)
            {
                auto obj2(std::move(local_obj));
                size_t dest_size = std::min<size_t>(4102 * 2 - total_count, out_buffer_size[out_buffer_id++]);
                auto s = obj2.get(cur_pos, dest_size);
                out_buffer_id %= std::size(out_buffer_size);
                cur_pos += s;
                total_count += s;
                if (s == 0) break;
                local_obj = std::move(obj2);
            }
        
            if (cur_pos - buf.data() != 4102) throw std::runtime_error("code_cvt<memory<char>>::get response incorrect");
            buf.resize(4102);
            if (buf != e_lit) throw std::runtime_error("chacha20 io response incorrect");
        }
    };

    using CheckType = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>>;
    CheckType obj(rb_root_cvt{mem_device("")}, "liwei");
    helper(obj);

    CheckType tmp(rb_root_cvt{mem_device("")}, "liwei");
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);

    dump_info("Done\n");
}

void test_chacha20_cvt_put_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt::put case 1...");

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

    using CheckType = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>>;

    auto out_helper = [&e_lit](auto& obj1, auto& obj2)
    {
        auto helper = [&e_lit](auto& obj)
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("chacha20::bos response incorrect");
            obj.main_cont_beg();
    
            size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            size_t total_count = 0;
            char* cur_pos = e_lit.data();
            int buffer_id = 0;
            while (total_count < 4102)
            {
                size_t dest_size = std::min<size_t>(4102 - total_count, buffer_size[buffer_id++]);
                obj.put(cur_pos, dest_size);
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
                total_count += dest_size;
            }
    
            auto [dev, err] = obj.detach();
            return dev.str();
        };
        
        std::string r1 = helper(obj1);
        std::string r2 = helper(obj2);
        if (r1 == r2) throw std::runtime_error("chacha20::put response incorrect");
    };

    using CheckType = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>>;
    {
        CheckType obj1(rb_root_cvt{mem_device("")}, "liwei");
        CheckType obj2(rb_root_cvt{mem_device("")}, "liwei");
        out_helper(obj1, obj2);
    }
    {
        CheckType tmp1(rb_root_cvt{mem_device("")}, "liwei");
        CheckType tmp2(rb_root_cvt{mem_device("")}, "liwei");
        runtime_cvt obj1(std::move(tmp1));
        runtime_cvt obj2(std::move(tmp2));
        out_helper(obj1, obj2);
    }
    dump_info("Done\n");
}

void test_chacha20_cvt_io_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt io case 1...");

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

    auto helper = [&e_lit]<typename T>(T& obj)
    {
        std::string enc_msg;
        {
            if (obj.bos() != io_status::output) throw std::runtime_error("chacha20_cvt::bos response incorrect");
            obj.main_cont_beg();
    
            size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            size_t total_count = 0;
            char* cur_pos = e_lit.data();
            int buffer_id = 0;
            while (total_count < 4102)
            {
                size_t dest_size = std::min<size_t>(4102 - total_count, buffer_size[buffer_id++]);
                obj.put(cur_pos, dest_size);
                buffer_id %= std::size(buffer_size);
                cur_pos += dest_size;
                total_count += dest_size;
            }
    
            auto [dev, err] = obj.detach();
            enc_msg = dev.str();
        }
    
        {
            T local_obj(Crypt::chacha20_cvt{rb_root_cvt{mem_device(enc_msg)}, "liwei"});
            if (local_obj.bos() != io_status::input) throw std::runtime_error("chacha20_cvt::bos response incorrect");
            local_obj.main_cont_beg();
            
            std::string buf;
            buf.resize(4102 * 2);
            if (local_obj.get(buf.data(), buf.size()) != 4102) throw std::runtime_error("chacha20_cvt::get response incorrect");
            buf.resize(4102);
            if (buf != e_lit) throw std::runtime_error("chacha20_cvt io response incorrect");
        }
    };

    using CheckType = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>>;
    CheckType obj(rb_root_cvt{mem_device("")}, "liwei");
    helper(obj);

    CheckType tmp(rb_root_cvt{mem_device("")}, "liwei");
    runtime_cvt obj2(std::move(tmp));
    helper(obj2);
    
    dump_info("Done\n");
}

void test_chacha20_cvt_key_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt key difference...");

    std::string msg = "Hello, world! This is a test message for ChaCha20.";
    std::string enc_msg;

    using CheckType = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>>;

    {
        CheckType obj(rb_root_cvt{mem_device("")}, "key1");
        obj.bos();
        obj.main_cont_beg();
        obj.put(msg.data(), msg.size());
        auto [dev, err] = obj.detach();
        enc_msg = dev.str();
    }

    {
        CheckType obj(rb_root_cvt{mem_device(enc_msg)}, "key1");
        obj.bos();
        obj.main_cont_beg();
        std::string dec_msg;
        dec_msg.resize(msg.size());
        obj.get(dec_msg.data(), dec_msg.size());
        if (dec_msg != msg) throw std::runtime_error("chacha20 decryption with correct key failed");
    }

    {
        CheckType obj(rb_root_cvt{mem_device(enc_msg)}, "key2");
        obj.bos();
        obj.main_cont_beg();
        std::string dec_msg;
        dec_msg.resize(msg.size());
        obj.get(dec_msg.data(), dec_msg.size());
        if (dec_msg == msg) throw std::runtime_error("chacha20 decryption with WRONG key succeeded (unexpectedly)");
    }

    dump_info("Done\n");
}

void test_chacha20_cvt_raw_key_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt raw-key constructor and error paths...");

    // key_gen with empty string throws
    {
        bool threw = false;
        try { Crypt::chacha20_cvt_helpers::key_gen(""); }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // Raw key constructor with valid 32-byte key: encrypt then decrypt
    {
        Botan::secure_vector<uint8_t> key(32, 0xAB);
        std::string msg = "raw-key test message";
        std::string enc_msg;

        using CheckType = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>>;
        {
            CheckType obj(rb_root_cvt{mem_device("")}, key);
            obj.bos();
            obj.main_cont_beg();
            obj.put(msg.data(), msg.size());
            auto [dev, err] = obj.detach();
            VERIFY(!err);
            enc_msg = dev.str();
        }
        {
            CheckType obj(rb_root_cvt{mem_device(enc_msg)}, key);
            obj.bos();
            obj.main_cont_beg();
            std::string dec;
            dec.resize(msg.size() * 2);
            auto n = obj.get(dec.data(), dec.size());
            dec.resize(n);
            VERIFY(dec == msg);
        }
    }

    // Raw key constructor with invalid key length throws
    {
        using CheckType = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>>;
        Botan::secure_vector<uint8_t> bad_key(5, 0x00);
        bool threw = false;
        try { CheckType obj(rb_root_cvt{mem_device("")}, bad_key); }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // Move assignment operator
    {
        using CheckType = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>>;
        std::string msg = "move assign test";
        CheckType obj1(rb_root_cvt{mem_device("")}, "movekey");
        CheckType obj2(rb_root_cvt{mem_device("")}, "movekey");

        obj1.bos();
        obj1.main_cont_beg();
        obj2 = std::move(obj1);
        obj2.put(msg.data(), msg.size());
        auto [dev, err] = obj2.detach();
        VERIFY(!err);
        VERIFY(!dev.str().empty());
    }

    dump_info("Done\n");
}

void test_chacha20_cvt_attach_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt attach/detach cycle...");

    std::string msg = "attach-cycle test message for chacha20";
    std::string enc1, enc2;

    using CheckType = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>>;
    CheckType obj(rb_root_cvt{mem_device("")}, "attachkey");

    // First session
    obj.bos();
    obj.main_cont_beg();
    obj.put(msg.data(), msg.size());
    {
        auto [dev, err] = obj.detach();
        VERIFY(!err);
        enc1 = dev.str();
    }

    // attach() exercises attach_impl(): resets cipher state
    obj.attach();
    obj.bos();
    obj.main_cont_beg();
    obj.put(msg.data(), msg.size());
    {
        auto [dev, err] = obj.detach();
        VERIFY(!err);
        enc2 = dev.str();
    }

    // Both sessions produce different ciphertext (different random IVs)
    VERIFY(enc1 != enc2);

    // Both decrypt correctly
    auto decrypt = [&msg](const std::string& enc)
    {
        CheckType dec_obj(rb_root_cvt{mem_device(enc)}, "attachkey");
        dec_obj.bos();
        dec_obj.main_cont_beg();
        std::string out;
        out.resize(msg.size() * 2);
        auto n = dec_obj.get(out.data(), out.size());
        out.resize(n);
        VERIFY(out == msg);
    };
    decrypt(enc1);
    decrypt(enc2);

    // bos_impl() on moved-from (null m_cipher) — hits line 396
    {
        CheckType src(rb_root_cvt{mem_device("")}, "attachkey");
        auto moved = std::move(src);
        bool threw = false;
        try { src.bos(); }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // attach_impl() on moved-from (null m_cipher) — hits line 351
    {
        CheckType src(rb_root_cvt{mem_device("")}, "attachkey");
        auto moved = std::move(src);
        bool threw = false;
        try { src.attach(); }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // bos_impl() input mode with too-short device data — incomplete IV read (hits line 419)
    {
        // ChaCha20 IV is 12 bytes; a 5-byte device has less than that
        CheckType dec_obj(rb_root_cvt{mem_device("hello")}, "attachkey");
        bool threw = false;
        try { dec_obj.bos(); }
        catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}