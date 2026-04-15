#include <cvt/root_cvt.h>
#include <cvt/code_cvt.h>
#include <cvt/comp/zlib_cvt.h>
#include <cvt/crypt/vigenere_cvt.h>
#include <cvt/crypt/hash_cvt.h>
#include <cvt/cvt_pipe_creator.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <common/dump_info.h>

void test_cvt_pipe_creator_put_1()
{
    // vigenere_cvt + code_cvt
    using namespace IOv2;
    dump_info("Test cvt pipe creator put 1...");

    std::string e_lit; e_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = '\xE6' + 'a';
        e_lit[i+1] = '\x9D' + 'b';
        e_lit[i+2] = '\x8E' + 'c';
        e_lit[i+3] = '\xE4' + 'd';
        e_lit[i+4] = '\xBC' + 'e';
        e_lit[i+5] = '\x9F' + 'f';
        e_lit[i+6] = (i / 7) % 127 + 1 + 'g';
    }
    std::u32string i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(U'李');
        i_lit.push_back(U'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    auto helper = [&i_lit, &e_lit](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("cvt_pipe::bos response incorrect");
        obj.main_cont_beg();

        size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        size_t total_count = 0;
        char32_t* cur_pos = i_lit.data();
        int buffer_id = 0;
        while (total_count < 4102 / 7 * 3)
        {
            size_t dest_size = std::min<size_t>(4102 / 7 * 3 - total_count, buffer_size[buffer_id++]);
            obj.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;
            total_count += dest_size;
            if (obj.tell() != total_count) throw std::runtime_error("cvt_pipe::tell response incorrect");
        }

        auto dev = obj.detach();
        if (dev.str() != e_lit) throw std::runtime_error("cvt_pipe::put response incorrect");
    };
    
    auto creator = Crypt::Classic::vigenere_cvt_creator("abcdefg") | 
                   code_cvt_creator<char, char32_t>("zh_CN.UTF-8");

    {
        auto obj = creator.create(make_root_cvt<true>(mem_device("")));
        helper(obj);
    }

    {
        auto tmp = creator.create(make_root_cvt<true>(mem_device("")));
        runtime_cvt obj(std::move(tmp));
        helper(obj);
    }
    dump_info("Done\n");
}

void test_cvt_pipe_creator_put_2()
{
    // hash_cvt + vigenere_cvt + zlib_cvt + code_cvt
    using namespace IOv2;
    dump_info("Test cvt pipe creator put 2...");

    std::u32string i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(U'李');
        i_lit.push_back(U'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    std::string hash_res;
    auto helper = [&i_lit](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("cvt_pipe::bos response incorrect");
        obj.main_cont_beg();

        size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        size_t total_count = 0;
        char32_t* cur_pos = i_lit.data();
        int buffer_id = 0;
        while (total_count < 4102 / 7 * 3)
        {
            size_t dest_size = std::min<size_t>(4102 / 7 * 3 - total_count, buffer_size[buffer_id++]);
            obj.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;
            total_count += dest_size;
        }

        auto dev = obj.detach();
        return dev.str();
    };

    {
        auto creator = (Crypt::Classic::vigenere_cvt_creator("abcdefg") | 
                        Crypt::hash_cvt_creator<char>(Crypt::hash_algo::MD5)) |
                       (Comp::zlib_cvt_creator<char>(6) | 
                        code_cvt_creator<char, char32_t>("zh_CN.UTF-8"));

        auto obj = creator.create(make_root_cvt<true>(mem_device("")));
        hash_res = helper(obj);
    }

    {
        auto creator = Crypt::Classic::vigenere_cvt_creator("abcdefg") | 
                       Crypt::hash_cvt_creator<char>(Crypt::hash_algo::MD5) |
                       Comp::zlib_cvt_creator<char>(6) | 
                       code_cvt_creator<char, char32_t>("zh_CN.UTF-8");

        auto tmp = creator.create(make_root_cvt<true>(mem_device("")));
        runtime_cvt obj{std::move(tmp)};
        if (hash_res != helper(obj)) throw std::runtime_error("cvt_pipe response incorrect");
    }

    dump_info("Done\n");
}

void test_cvt_pipe_creator_put_3()
{
    // hash_cvt + zlib_cvt + vigenere_cvt + code_cvt
    using namespace IOv2;
    dump_info("Test cvt pipe creator put 3...");

    std::u32string i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(U'李');
        i_lit.push_back(U'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    std::string hash_res;
    auto helper = [&i_lit](auto& obj)
    {
        if (obj.bos() != io_status::output) throw std::runtime_error("cvt_pipe::bos response incorrect");
        obj.main_cont_beg();

        size_t buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        size_t total_count = 0;
        char32_t* cur_pos = i_lit.data();
        int buffer_id = 0;
        while (total_count < 4102 / 7 * 3)
        {
            size_t dest_size = std::min<size_t>(4102 / 7 * 3 - total_count, buffer_size[buffer_id++]);
            obj.put(cur_pos, dest_size);
            buffer_id %= std::size(buffer_size);
            cur_pos += dest_size;
            total_count += dest_size;
        }

        auto dev = obj.detach();
        return dev.str();
    };

    {
        auto creator = (Crypt::Classic::vigenere_cvt_creator("abcdefg") | 
                        Comp::zlib_cvt_creator<char>(6)) | 
                       (Crypt::hash_cvt_creator<char>(Crypt::hash_algo::MD5) |
                        code_cvt_creator<char, char32_t>("zh_CN.UTF-8"));

        auto obj = creator.create(make_root_cvt<true>(mem_device("")));
        hash_res = helper(obj);
    }

    {
        auto creator = (Crypt::Classic::vigenere_cvt_creator("abcdefg") | 
                        Comp::zlib_cvt_creator<char>(6)) | 
                       (Crypt::hash_cvt_creator<char>(Crypt::hash_algo::MD5) |
                        code_cvt_creator<char, char32_t>("zh_CN.UTF-8"));

        auto tmp = creator.create(make_root_cvt<true>(mem_device("")));
        runtime_cvt obj{std::move(tmp)};
        if (hash_res != helper(obj)) throw std::runtime_error("cvt_pipe response incorrect");
    }
    
    dump_info("Done\n");
}

void test_cvt_pipe_creator_get_1()
{
    // vigenere_cvt + code_cvt
    using namespace IOv2;
    dump_info("Test cvt pipe creator get 1...");

    std::string e_lit; e_lit.resize(4102);
    for (int i = 0; i < 4102; i += 7)
    {
        e_lit[i+0] = '\xE6' + 'a';
        e_lit[i+1] = '\x9D' + 'b';
        e_lit[i+2] = '\x8E' + 'c';
        e_lit[i+3] = '\xE4' + 'd';
        e_lit[i+4] = '\xBC' + 'e';
        e_lit[i+5] = '\x9F' + 'f';
        e_lit[i+6] = (i / 7) % 127 + 1 + 'g';
    }

    auto helper = [](auto& obj)
    {
        if (obj.bos() != io_status::input) throw std::runtime_error("cvt_pipe::bos response incorrect");
        obj.main_cont_beg();
        size_t out_buffer_size[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    
        std::vector<char32_t> out_buf; out_buf.resize(4102);
        size_t total_count = 0;
        char32_t* cur_pos = out_buf.data();
        int out_buffer_id = 0;
        while (true)
        {
            size_t dest_size = std::min<size_t>(4102 - total_count, out_buffer_size[out_buffer_id++]);
            auto s = obj.get(cur_pos, dest_size);
            out_buffer_id %= std::size(out_buffer_size);
            cur_pos += s;
            total_count += s;
            if (obj.tell() != total_count) throw std::runtime_error("cvt_pipe::tell response incorrect");
            if (s == 0) break;
        }
    
        if (cur_pos - out_buf.data() != 4102 / 7 * 3) throw std::runtime_error("cvt_pipe::get response incorrect");
        out_buf.resize(4102 / 7 * 3);
            
        auto it = out_buf.begin();
        for (size_t i = 0; i < out_buf.size(); i += 3)
        {
            if (*it++ != U'李') throw std::runtime_error("cvt_pipe::get response incorrect");
            if (*it++ != U'伟') throw std::runtime_error("cvt_pipe::get response incorrect");
            if (*it++ != (i / 3) % 127 + 1) throw std::runtime_error("cvt_pipe::get response incorrect");
        }
    };
    
    auto creator = Crypt::Classic::vigenere_cvt_creator("abcdefg") | 
                   code_cvt_creator<char, char32_t>("zh_CN.UTF-8");

    {
        auto obj = creator.create(make_root_cvt<true>(mem_device(e_lit)));
        helper(obj);
    }
    
    {
        auto tmp = creator.create(make_root_cvt<true>(mem_device(e_lit)));
        runtime_cvt obj{std::move(tmp)};
        helper(obj);
    }
    dump_info("Done\n");
}

void test_cvt_pipe_creator_io_1()
{
    // vigenere_cvt + zlib_cvt + code_cvt
    using namespace IOv2;
    dump_info("Test cvt pipe creator IO case 1...");

    std::u32string i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(U'李');
        i_lit.push_back(U'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    auto creator = Crypt::Classic::vigenere_cvt_creator("abcdefg") | 
                   Comp::zlib_cvt_creator<char>(6) | 
                   code_cvt_creator<char, char32_t>("zh_CN.UTF-8");

    auto helper = [&i_lit]<typename T, typename U>(T& obj, const U& p_creator)
    {
        std::string e_lit;
        if (obj.bos() != io_status::output) throw std::runtime_error("cvt_pipe::bos response incorrect");
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());
    
        auto dev = obj.detach();
        e_lit = dev.str();
        
        T obj2 = p_creator.create(make_root_cvt<true>(mem_device(e_lit)));
        std::u32string ilit2; ilit2.resize(4102 * 2);
        if (obj2.bos() != io_status::input) throw std::runtime_error("cvt_pipe::bos response incorrect");
        obj2.main_cont_beg();
        
        if (obj2.get(ilit2.data(), 4102 * 2) != 4102 / 7 * 3) throw std::runtime_error("cvt_pipe::get response incorrect");
        if (ilit2.substr(0, 4102 / 7 * 3) != i_lit) throw std::runtime_error("cvt_pipe io response incorrect");
    };
    
    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj, creator);

    auto tmp = creator.create(make_root_cvt<true>(mem_device("")));
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2, creator);

    dump_info("Done\n");
}

void test_cvt_pipe_creator_io_2()
{
    // vigenere_cvt + zlib_cvt + code_cvt
    using namespace IOv2;
    dump_info("Test cvt pipe creator IO case 2...");

    std::u32string i_lit; i_lit.reserve(4102 / 7 * 3);
    for (int i = 0; i < 4102 / 7 * 3; i += 3)
    {
        i_lit.push_back(U'李');
        i_lit.push_back(U'伟');
        i_lit.push_back((i / 3) % 127 + 1);
    }

    auto creator = Crypt::Classic::vigenere_cvt_creator("abcdefg") | 
                   (Comp::zlib_cvt_creator<char>(6) | 
                    code_cvt_creator<char, char32_t>("zh_CN.UTF-8"));

    auto helper = [&i_lit]<typename T, typename U>(T& obj, const U& p_creator)
    {
        std::string e_lit;
        if (obj.bos() != io_status::output) throw std::runtime_error("cvt_pipe::bos response incorrect");
        obj.main_cont_beg();
        obj.put(i_lit.data(), i_lit.size());
    
        auto dev = obj.detach();
        e_lit = dev.str();
        
        T obj2 = p_creator.create(make_root_cvt<true>(mem_device(e_lit)));
        std::u32string ilit2; ilit2.resize(4102 * 2);
        if (obj2.bos() != io_status::input) throw std::runtime_error("cvt_pipe::bos response incorrect");
        obj2.main_cont_beg();
        
        if (obj2.get(ilit2.data(), 4102 * 2) != 4102 / 7 * 3) throw std::runtime_error("cvt_pipe::get response incorrect");
        if (ilit2.substr(0, 4102 / 7 * 3) != i_lit) throw std::runtime_error("cvt_pipe io response incorrect");
    };

    auto obj = creator.create(make_root_cvt<true>(mem_device("")));
    helper(obj, creator);

    auto tmp = creator.create(make_root_cvt<true>(mem_device("")));
    runtime_cvt obj2{std::move(tmp)};
    helper(obj2, creator);
    
    dump_info("Done\n");
}

void test_cvt_pipe_creator()
{
    test_cvt_pipe_creator_put_1();
    test_cvt_pipe_creator_put_2();
    test_cvt_pipe_creator_put_3();
    test_cvt_pipe_creator_get_1();
    test_cvt_pipe_creator_io_1();
    test_cvt_pipe_creator_io_2();
}