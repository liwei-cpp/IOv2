#include <cvt/crypt/chacha20_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>
#include <support/dump_info.h>
#include <support/verify.h>

void test_chacha20_cvt_wchar_t_gen_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt<wchar_t> general case 1...");
    
    {
        using CheckType = Crypt::chacha20_cvt<root_cvt<mem_device<char>, true>, wchar_t>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
        static_assert(std::is_same_v<CheckType::external_type, char>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }
    
    {
        using CheckType = Crypt::chacha20_cvt<root_cvt<mem_device<char8_t>, false>, wchar_t>;
        static_assert(IOv2::io_converter<CheckType>);
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
        static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
        static_assert(std::is_same_v<CheckType::external_type, char8_t>);
        static_assert(cvt_cpt::support_put<CheckType>);
        static_assert(cvt_cpt::support_get<CheckType>);
        static_assert(!cvt_cpt::support_positioning<CheckType>);
        static_assert(!cvt_cpt::support_io_switch<CheckType>);
    }

    dump_info("Done\n");
}

void test_chacha20_cvt_wchar_t_gen_2()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt<wchar_t> general case 2...");

    std::wstring e_lit; e_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = L'\xE6';
        e_lit[i+1] = L'\x9D';
        e_lit[i+2] = L'\x8E';
        e_lit[i+3] = L'\xE4';
        e_lit[i+4] = L'\xBC';
        e_lit[i+5] = L'\x9F';
        e_lit[i+6] = (i / 7) % 127 + 1;
    }

    auto helper = [&e_lit]<typename T>(T& obj)
    {
        Crypt::chacha20_cvt_creator<wchar_t> creator("liwei");
        std::string enc_msg;
        {
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
    
            size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            size_t total_count = 0;
            wchar_t* cur_pos = e_lit.data();
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
            T local_obj(creator.create(rb_root_cvt{mem_device(enc_msg)}));
            VERIFY(local_obj.bos() == io_status::input);
            local_obj.main_cont_beg();
            
            size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
        
            std::wstring buf; buf.resize(4102 * 2);
            size_t total_count = 0;
            wchar_t* cur_pos = buf.data();
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
        
            VERIFY(cur_pos - buf.data() == 4102);
            buf.resize(4102);
            VERIFY(buf == e_lit);
        }
    };

    Crypt::chacha20_cvt_creator<wchar_t> creator("liwei");
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    helper(obj);

    runtime_cvt obj2(creator.create(rb_root_cvt{mem_device("")}));
    helper(obj2);

    dump_info("Done\n");
}

void test_chacha20_cvt_wchar_t_put_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt<wchar_t>::put case 1...");

    std::wstring e_lit; e_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = L'\xE6';
        e_lit[i+1] = L'\x9D';
        e_lit[i+2] = L'\x8E';
        e_lit[i+3] = L'\xE4';
        e_lit[i+4] = L'\xBC';
        e_lit[i+5] = L'\x9F';
        e_lit[i+6] = (i / 7) % 127 + 1;
    }

    auto out_helper = [&e_lit](auto& obj1, auto& obj2)
    {
        auto helper = [&e_lit](auto& obj)
        {
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
    
            size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            size_t total_count = 0;
            auto cur_pos = e_lit.data();
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
        VERIFY(r1 != r2);
    };

    Crypt::chacha20_cvt_creator<wchar_t> creator("liwei");
    {
        auto obj1 = creator.create(rb_root_cvt{mem_device("")});
        auto obj2 = creator.create(rb_root_cvt{mem_device("")});
        out_helper(obj1, obj2);
    }
    {
        runtime_cvt obj1(creator.create(rb_root_cvt{mem_device("")}));
        runtime_cvt obj2(creator.create(rb_root_cvt{mem_device("")}));
        out_helper(obj1, obj2);
    }
    dump_info("Done\n");
}

void test_chacha20_cvt_wchar_t_io_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt<wchar_t> io case 1...");

    std::wstring e_lit; e_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = L'\xE6';
        e_lit[i+1] = L'\x9D';
        e_lit[i+2] = L'\x8E';
        e_lit[i+3] = L'\xE4';
        e_lit[i+4] = L'\xBC';
        e_lit[i+5] = L'\x9F';
        e_lit[i+6] = (i / 7) % 127 + 1;
    }

    auto helper = [&e_lit]<typename T>(T& obj)
    {
        Crypt::chacha20_cvt_creator<wchar_t> creator("liwei");
        std::string enc_msg;
        {
            VERIFY(obj.bos() == io_status::output);
            obj.main_cont_beg();
    
            size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
            size_t total_count = 0;
            wchar_t* cur_pos = e_lit.data();
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
            T local_obj(creator.create(rb_root_cvt{mem_device(enc_msg)}));
            VERIFY(local_obj.bos() == io_status::input);
            local_obj.main_cont_beg();
            
            std::wstring buf;
            buf.resize(4102 * 2);
            VERIFY(local_obj.get(buf.data(), buf.size()) == 4102);
            buf.resize(4102);
            VERIFY(buf == e_lit);
        }
    };

    Crypt::chacha20_cvt_creator<wchar_t> creator("liwei");
    auto obj = creator.create(rb_root_cvt{mem_device("")});
    helper(obj);

    runtime_cvt obj2(creator.create(rb_root_cvt{mem_device("")}));
    helper(obj2);
    
    dump_info("Done\n");
}
void test_chacha20_cvt_wchar_t_err_1()
{
    using namespace IOv2;
    dump_info("Test chacha20_cvt<wchar_t> error paths...");

    // Encrypt 1 wchar_t element, then truncate ciphertext by 1 byte so it is not
    // wchar_t-aligned. Decrypting triggers the EOF-on-non-aligned-boundary path
    // (lines 533-535 splice entry + line 567 taint-and-throw).
    {
        Crypt::chacha20_cvt_creator<wchar_t> creator("errkey");
        std::string enc_msg;
        {
            auto enc_obj = creator.create(rb_root_cvt{mem_device("")});
            enc_obj.bos();
            enc_obj.main_cont_beg();
            wchar_t ch = L'A';
            enc_obj.put(&ch, 1);
            auto [dev, err] = enc_obj.detach();
            enc_msg = dev.str(); // IV_len bytes + 4 bytes ciphertext
        }

        // Truncate last byte: IV_len + 3 bytes = non-aligned for wchar_t
        if (enc_msg.size() > 4)
        {
            enc_msg.resize(enc_msg.size() - 1);

            using WcharType = Crypt::chacha20_cvt<rb_root_cvt<mem_device<char>>, wchar_t>;
            WcharType dec_obj(rb_root_cvt{mem_device(enc_msg)}, "errkey");
            dec_obj.bos();          // reads IV successfully
            dec_obj.main_cont_beg();
            wchar_t buf[2];
            bool threw = false;
            try { dec_obj.get(buf, 1); } // 3 bytes available, needs 4 → EOF mid-element
            catch (const cvt_error&) { threw = true; }
            VERIFY(threw);
        }
    }

    dump_info("Done\n");
}
