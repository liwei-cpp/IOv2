#include <cvt/root_cvt.h>
#include <device/mem_device.h>
#include <io/streambuf_iterator.h>
#include <common/dump_info.h>

void test_ostreambuf_iterator_gen_1()
{
    dump_info("Test ostreambuf_iterator general case 1...");
    using namespace IOv2;

    {
        using CheckType = ostreambuf_iterator<streambuf<mem_device<char>, char>>;
        static_assert(std::output_iterator<CheckType, char>);
        static_assert(std::is_same_v<CheckType::value_type, char>);
    }

    {
        using CheckType = ostreambuf_iterator<streambuf<mem_device<char>, char>>;
        static_assert(std::output_iterator<CheckType, char>);
        static_assert(std::is_same_v<CheckType::value_type, char>);
    }

    {
        using CheckType = ostreambuf_iterator<streambuf<mem_device<char>, char32_t>>;
        static_assert(std::output_iterator<CheckType, char32_t>);
        static_assert(std::is_same_v<CheckType::value_type, char32_t>);
    }

    {
        using CheckType = ostreambuf_iterator<streambuf<mem_device<char>, char32_t>>;
        static_assert(std::output_iterator<CheckType, char32_t>);
        static_assert(std::is_same_v<CheckType::value_type, char32_t>);
    }

    dump_info("Done\n");
}

// https://github.com/gcc-mirror/gcc/blob/d4a777d098d524a3f26c3db28e50d064a7a4407e/libstdc%2B%2B-v3/testsuite/24_iterators/ostreambuf_iterator/2.cc
void test_ostreambuf_iterator_put_1()
{
    dump_info("Test ostreambuf_iterator put case 1...");
    using namespace IOv2;

    auto helper = []<typename T>(const T& sb_ori)
    {
        std::string str01 = "playa hermosa, liberia, guanacaste";
        std::string str02 = "bodega bay, lost coast, california";
        T strbuf01 = sb_ori;

        // ctor sanity checks
        ostreambuf_iterator ostrb_it01(strbuf01);
        ostrb_it01++;
        ++ostrb_it01;
        ostrb_it01 = 'a';
        (void)*ostrb_it01;
    
        // charT operator*() const
        // ostreambuf_iterator& operator++();
        // ostreambuf_iterator& operator++(int);
        ostreambuf_iterator ostrb_it27(strbuf01);
        int j = str02.size();   // same as str01.size
        for (int i = 0; i < j; ++i)
            ostrb_it27 = str02[i];
        auto tmp = strbuf01.detach().str();
        if (tmp == str01) throw std::runtime_error("ostreambuf_iterator put check fail");
        if (tmp != 'a' + str02) throw std::runtime_error("ostreambuf_iterator put check fail");
    
        T strbuf02 = sb_ori;
        ostreambuf_iterator ostrb_it28(strbuf02);
        j = str01.size();
        for (int i = 0; i < j + 2; ++i)
            ostrb_it28 = 'b';
        tmp = strbuf01.detach().str();
        if (tmp == str01) throw std::runtime_error("ostreambuf_iterator put check fail");
        if (tmp == str02) throw std::runtime_error("ostreambuf_iterator put check fail");
    };
    
    streambuf sb{mem_device{""}};
    helper(sb);

    ostreambuf sb2{mem_device{""}};
    helper(sb2);
    dump_info("Done\n");
}