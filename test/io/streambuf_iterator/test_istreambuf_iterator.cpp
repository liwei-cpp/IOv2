#include <cvt/root_cvt.h>
#include <device/mem_device.h>
#include <io/streambuf_iterator.h>
#include <support/dump_info.h>
#include <support/verify.h>

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
        VERIFY(istrb_it01 != istrb_eos);

        std::string tmp(istrb_it01, istrb_eos);
        VERIFY(tmp == str01);
        VERIFY(istrb_it01 == istrb_eos);

        // equality
        decltype(istrb_it01) istrb_it07;
        decltype(istrb_it01) istrb_it08;
        VERIFY(istrb_it07 == istrb_it08);

        TStreamBuf sb2(sb_ori);
        decltype(istrb_it01) istrb_it21(sb2);
        decltype(istrb_it01) istrb_it22(sb2);
        VERIFY(istrb_it21 == istrb_it22);

        VERIFY(istrb_it07 != istrb_it22);
    
        // charT operator*() const
        // istreambuf_iterator& operator++();
        // istreambuf_iterator& operator++(int);
        istreambuf_iterator istrb_it27(sb2);
        char c;
        for (std::size_t i = 0; i < str01.size() - 2; ++i)
        {
            c = *istrb_it27++;
            VERIFY(c == str01[i]);
        }

        VERIFY(sb2.sbumpc() == str01[str01.size() - 2]);
        VERIFY(sb2.sbumpc() == str01[str01.size() - 1]);

        TStreamBuf sb3(sb_ori);
        istreambuf_iterator istrb_it28(sb3);
        for (std::size_t i = 0; i < str01.size() - 2;)
        {
            c = *++istrb_it28;
            VERIFY(c == str01[++i]);
        }

        VERIFY(sb3.sbumpc() == str01[str01.size() - 2]);
        VERIFY(sb3.sbumpc() == str01[str01.size() - 1]);
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
    
    VERIFY(res_postfix == res_prefix);
    VERIFY(res_mixed == res_prefix);

    dump_info("Done\n");
}

void test_istreambuf_iterator_sentinel_1()
{
    dump_info("Test istreambuf_iterator with sentinel case 1...");
    using namespace IOv2;
    
    static_assert(std::sentinel_for<std::default_sentinel_t,
                                    istreambuf_iterator<streambuf<mem_device<char>, char>>>);

    istreambuf_iterator<streambuf<mem_device<char>, char>> i = std::default_sentinel;
    VERIFY(i == std::default_sentinel);
    VERIFY(std::default_sentinel == i);

    dump_info("Done\n");
}

void test_istreambuf_iterator_sentinel_2()
{
    dump_info("Test istreambuf_iterator with sentinel case 2...");
    using namespace IOv2;

    {
        streambuf in(mem_device{"abc"});
        istreambuf_iterator iter(in);
        VERIFY(iter != std::default_sentinel);
        VERIFY(std::default_sentinel != iter);

        (void)std::next(iter, 3);
        VERIFY(iter == std::default_sentinel);
        VERIFY(std::default_sentinel == iter);
    }

    {
        istreambuf in(mem_device{"abc"});
        istreambuf_iterator iter(in);
        VERIFY(iter != std::default_sentinel);
        VERIFY(std::default_sentinel != iter);

        (void)std::next(iter, 3);
        VERIFY(iter == std::default_sentinel);
        VERIFY(std::default_sentinel == iter);
    }
    dump_info("Done\n");
}

void test_istreambuf_iterator_chain_increment_1()
{
    dump_info("Test istreambuf_iterator chained increment on a cached copy case 1...");
    using namespace IOv2;

    // Regression test: once operator++(int) returns a copy that caches an
    // already-consumed character (m_c), incrementing that copy again must
    // not pull yet another character from the shared streambuf. Before the
    // fix, operator++ / operator++(int) called sbumpc() unconditionally,
    // silently discarding the cached character and consuming/skipping one
    // extra character from the stream.
    auto helper = []<typename TStreamBuf>(TStreamBuf& sb)
    {
        // prefix increment on a cached copy
        {
            istreambuf_iterator it(sb);
            auto old1 = it++;
            VERIFY(*old1 == 'a');

            ++old1;
            VERIFY(*it == 'b');
            VERIFY(*old1 == 'b');
        }
    };

    auto helper_postfix = []<typename TStreamBuf>(TStreamBuf& sb)
    {
        // postfix increment on a cached copy
        {
            istreambuf_iterator it(sb);
            auto old1 = it++;
            VERIFY(*old1 == 'a');

            auto old2 = old1++;
            VERIFY(*old2 == 'a');
            VERIFY(*old1 == 'b');
            VERIFY(*it == 'b');
        }
    };

    {
        streambuf sb(mem_device{"abc"});
        helper(sb);
    }
    {
        istreambuf sb(mem_device{"abc"});
        helper(sb);
    }
    {
        streambuf sb(mem_device{"abc"});
        helper_postfix(sb);
    }
    {
        istreambuf sb(mem_device{"abc"});
        helper_postfix(sb);
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
        VERIFY(*iter == 'b');
        iter.sputbackc('x');
        VERIFY(*iter++ == 'x');
        VERIFY(*iter++ == 'b');
    }

    {
        istreambuf in(mem_device{"abc"});
        istreambuf_iterator iter(in);
        ++iter;
        VERIFY(*iter == 'b');
        iter.sputbackc('x');
        VERIFY(*iter++ == 'x');
        VERIFY(*iter++ == 'b');
    }
    dump_info("Done\n");
}
