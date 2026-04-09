#include <cvt/root_cvt.h>
#include <device/mem_device.h>
#include <io/streambuf_iterator.h>
#include <common/dump_info.h>

void test_istreambuf_iterator_gen_1()
{
    dump_info("Test istreambuf_iterator general case 1...");
    using namespace IOv2;

    {
        using CheckType = istreambuf_iterator<streambuf<mem_device<char>, char>>;
        static_assert(std::input_iterator<CheckType>);
        static_assert(std::is_same_v<CheckType::value_type, char>);
    }

    {
        using CheckType = istreambuf_iterator<streambuf<mem_device<char>, char>>;
        static_assert(std::input_iterator<CheckType>);
        static_assert(std::is_same_v<CheckType::value_type, char>);
    }

    {
        using CheckType = istreambuf_iterator<streambuf<mem_device<char>, char32_t>>;
        static_assert(std::input_iterator<CheckType>);
        static_assert(std::is_same_v<CheckType::value_type, char32_t>);
    }

    {
        using CheckType = istreambuf_iterator<streambuf<mem_device<char>, char32_t>>;
        static_assert(std::input_iterator<CheckType>);
        static_assert(std::is_same_v<CheckType::value_type, char32_t>);
    }

    dump_info("Done\n");
}

void test_istreambuf_iterator_gen_2()
{
    dump_info("Test istreambuf_iterator general case 2...");
    using namespace IOv2;

    std::string str01 = "playa hermosa, liberia, guanacaste";
    
    auto helper = [&str01]<typename TStreamBuf>(const TStreamBuf& sb_ori)
    {
        TStreamBuf sb(sb_ori);
        istreambuf_iterator istrb_it01(sb);
        decltype(istrb_it01) istrb_eos;
        if (istrb_it01 == istrb_eos) throw std::runtime_error("istreambuf_iterator ctor sanity check fail");
    
        std::string tmp(istrb_it01, istrb_eos);
        if (tmp != str01) throw std::runtime_error("istreambuf_iterator ctor sanity check fail");
        if (istrb_it01 != istrb_eos) throw std::runtime_error("istreambuf_iterator ctor sanity check fail");
    
        // equality
        decltype(istrb_it01) istrb_it07;
        decltype(istrb_it01) istrb_it08;
        if (istrb_it07 != istrb_it08) throw std::runtime_error("istreambuf_iterator equality check fail");

        TStreamBuf sb2(sb_ori);
        decltype(istrb_it01) istrb_it21(sb2);
        decltype(istrb_it01) istrb_it22(sb2);
        if (istrb_it21 != istrb_it22) throw std::runtime_error("istreambuf_iterator equality check fail");
    
        if (istrb_it07 == istrb_it22) throw std::runtime_error("istreambuf_iterator equality check fail");
    
        // charT operator*() const
        // istreambuf_iterator& operator++();
        // istreambuf_iterator& operator++(int);
        istreambuf_iterator istrb_it27(sb2);
        char c;
        for (std::size_t i = 0; i < str01.size() - 2; ++i)
        {
            c = *istrb_it27++;
            if (c != str01[i]) throw std::runtime_error("istreambuf_iterator operator* check fail");
        }
        
        if (sb2.sbumpc() != str01[str01.size() - 2]) throw std::runtime_error("istreambuf_iterator operator* check fail");
        if (sb2.sbumpc() != str01[str01.size() - 1]) throw std::runtime_error("istreambuf_iterator operator* check fail");

        TStreamBuf sb3(sb_ori);
        istreambuf_iterator istrb_it28(sb3);
        for (std::size_t i = 0; i < str01.size() - 2;)
        {
            c = *++istrb_it28;
            if (c != str01[++i]) throw std::runtime_error("istreambuf_iterator operator* check fail");
        }
    
        if (sb3.sbumpc() != str01[str01.size() - 2]) throw std::runtime_error("istreambuf_iterator operator* check fail");
        if (sb3.sbumpc() != str01[str01.size() - 1]) throw std::runtime_error("istreambuf_iterator operator* check fail");
    };

    streambuf sb(mem_device{str01});
    helper(sb);
    
    istreambuf sb2(mem_device{str01});
    helper(sb2);

    dump_info("Done\n");
}

void test_istreambuf_iterator_get_1()
{
    dump_info("Test istreambuf_iterator get case 1...");
    using namespace IOv2;
    
    const std::string s("free the vieques");
    streambuf sb_ori(mem_device{s});

    // 1
    std::string res_postfix;
    streambuf sb1(sb_ori);
    istreambuf_iterator isbufit01(sb1);
    for (std::size_t j = 0; j < s.size(); ++j, isbufit01++)
        res_postfix += *isbufit01;

    // 2
    std::string res_prefix;
    streambuf sb2(sb_ori);
    istreambuf_iterator isbufit02(sb2);
    for (std::size_t j = 0; j < s.size(); ++j, ++isbufit02)
        res_prefix += *isbufit02;

    // 3 mixed
    std::string res_mixed;
    streambuf sb3(sb_ori);
    istreambuf_iterator isbufit03(sb3);
    for (std::size_t j = 0; j < (s.size() / 2); ++j)
    {
        res_mixed += *isbufit03;
        ++isbufit03;
        res_mixed += *isbufit03;
        isbufit03++;
    }
    
    if (res_postfix != res_prefix) throw std::runtime_error("istreambuf_iterator get check fail");
    if (res_mixed != res_prefix)   throw std::runtime_error("istreambuf_iterator get check fail");

    dump_info("Done\n");
}

void test_istreambuf_iterator_sentinel_1()
{
    dump_info("Test istreambuf_iterator with sentinel case 1...");
    using namespace IOv2;
    
    static_assert(std::sentinel_for<std::default_sentinel_t,
                                    istreambuf_iterator<streambuf<mem_device<char>, char>>>);

    istreambuf_iterator<streambuf<mem_device<char>, char>> i = std::default_sentinel;
    if (i != std::default_sentinel) throw std::runtime_error("istreambuf_iterator sentinel check fail");
    if (std::default_sentinel != i) throw std::runtime_error("istreambuf_iterator sentinel check fail");

    dump_info("Done\n");
}

void test_istreambuf_iterator_sentinel_2()
{
    dump_info("Test istreambuf_iterator with sentinel case 2...");
    using namespace IOv2;

    {
        streambuf in(mem_device{"abc"});
        istreambuf_iterator iter(in);
        if (iter == std::default_sentinel) throw std::runtime_error("istreambuf_iterator sentinel check fail");
        if (std::default_sentinel == iter) throw std::runtime_error("istreambuf_iterator sentinel check fail");

        (void)std::next(iter, 3);
        if (iter != std::default_sentinel) throw std::runtime_error("istreambuf_iterator sentinel check fail");
        if (std::default_sentinel != iter) throw std::runtime_error("istreambuf_iterator sentinel check fail");
    }

    {
        istreambuf in(mem_device{"abc"});
        istreambuf_iterator iter(in);
        if (iter == std::default_sentinel) throw std::runtime_error("istreambuf_iterator sentinel check fail");
        if (std::default_sentinel == iter) throw std::runtime_error("istreambuf_iterator sentinel check fail");

        (void)std::next(iter, 3);
        if (iter != std::default_sentinel) throw std::runtime_error("istreambuf_iterator sentinel check fail");
        if (std::default_sentinel != iter) throw std::runtime_error("istreambuf_iterator sentinel check fail");
    }
    dump_info("Done\n");
}

void test_istreambuf_iterator_putback_1()
{
    dump_info("Test istreambuf_iterator::sputbackc case 1...");
    using namespace IOv2;

    {
        streambuf in(mem_device{"abc"});
        istreambuf_iterator iter(in);
        ++iter;
        if (*iter != 'b') throw std::runtime_error("istreambuf_iterator output check fail");
        iter.sputbackc('x');
        if (*iter++ != 'x') throw std::runtime_error("istreambuf_iterator output check fail");
        if (*iter++ != 'b') throw std::runtime_error("istreambuf_iterator output check fail");
    }

    {
        istreambuf in(mem_device{"abc"});
        istreambuf_iterator iter(in);
        ++iter;
        if (*iter != 'b') throw std::runtime_error("istreambuf_iterator output check fail");
        iter.sputbackc('x');
        if (*iter++ != 'x') throw std::runtime_error("istreambuf_iterator output check fail");
        if (*iter++ != 'b') throw std::runtime_error("istreambuf_iterator output check fail");
    }
    dump_info("Done\n");
}
