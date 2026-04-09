#include <deque>
#include <list>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <facet/numeric.h>

#include <common/dump_info.h>

namespace
{
    struct Punct: IOv2::numeric_conf<wchar_t>
    {
        Punct(const std::string& n)
            : IOv2::numeric_conf<wchar_t>(n)
            , m_grouping(IOv2::numeric_conf<wchar_t>::grouping())
            , m_truename(IOv2::numeric_conf<wchar_t>::truename())
            , m_falsename(IOv2::numeric_conf<wchar_t>::falsename())
            , m_thousands_sep(IOv2::numeric_conf<wchar_t>::thousands_sep())
            , m_decimal_point(IOv2::numeric_conf<wchar_t>::decimal_point())
        {}
        
        const std::vector<uint8_t>& grouping() const override { return m_grouping; };
        const std::wstring& truename() const override { return m_truename; }
        const std::wstring& falsename() const override { return m_falsename; }
        wchar_t thousands_sep() const override { return m_thousands_sep; }
        wchar_t decimal_point() const override { return m_decimal_point; }
        
        void set_grouping(std::vector<uint8_t> g)
        {
            m_grouping = std::move(g);
        }

        void set_truename(std::wstring n) { m_truename = std::move(n); }
        void set_falsename(std::wstring n)
        {
            m_falsename = std::move(n);
        }
        
        void set_thousands_sep(wchar_t c) { m_thousands_sep = c; }
        void set_decimal_point(wchar_t c) { m_decimal_point = c; }
    private:
        std::vector<uint8_t> m_grouping;
        std::wstring m_truename;
        std::wstring m_falsename;
        wchar_t m_thousands_sep;
        wchar_t m_decimal_point;
    };

    std::shared_ptr<IOv2::ctype<wchar_t>> s_ctype_c
        = std::make_shared<IOv2::ctype<wchar_t>>(std::make_shared<IOv2::ctype_conf<wchar_t>>("C"));

    std::shared_ptr<IOv2::ctype<wchar_t>> s_ctype_de_utf8
        = std::make_shared<IOv2::ctype<wchar_t>>(std::make_shared<IOv2::ctype_conf<wchar_t>>("de_DE.UTF-8"));

    std::shared_ptr<IOv2::ctype<wchar_t>> s_ctype_de_8859
        = std::make_shared<IOv2::ctype<wchar_t>>(std::make_shared<IOv2::ctype_conf<wchar_t>>("de_DE.ISO-8859-1"));

    std::shared_ptr<IOv2::ctype<wchar_t>> s_ctype_hk_utf8
        = std::make_shared<IOv2::ctype<wchar_t>>(std::make_shared<IOv2::ctype_conf<wchar_t>>("en_HK.UTF-8"));
}

void test_numeric_wchar_t_common_1()
{
    dump_info("Test numeric<wchar_t> common 1...");
    static_assert(std::is_same_v<IOv2::numeric<wchar_t>::char_type, wchar_t>);
    
    IOv2::numeric<wchar_t> nump_c(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    IOv2::numeric<wchar_t> nump_de(std::make_shared<IOv2::numeric_conf<wchar_t>>("de_DE.UTF-8"), s_ctype_de_utf8);

    if (nump_c.decimal_point() == nump_de.decimal_point())
        throw std::runtime_error("numeric<wchar_t>::decimal_point incorrect");
    if (nump_c.thousands_sep() == nump_de.thousands_sep())
        throw std::runtime_error("numeric<wchar_t>::thousands_sep incorrect");
    if (nump_c.grouping() == nump_de.grouping())
        throw std::runtime_error("numeric<wchar_t>::grouping incorrect");

    if (nump_c.truename().empty())
        throw std::runtime_error("numeric<wchar_t>::truename incorrect");
    if (nump_de.truename().empty())
        throw std::runtime_error("numeric<wchar_t>::truename incorrect");
    if (nump_c.truename() == nump_de.truename())
        throw std::runtime_error("numeric<wchar_t>::truename incorrect");

    if (nump_c.falsename().empty())
        throw std::runtime_error("numeric<wchar_t>::falsename incorrect");
    if (nump_de.falsename().empty())
        throw std::runtime_error("numeric<wchar_t>::falsename incorrect");
    if (nump_c.falsename() == nump_de.falsename())
        throw std::runtime_error("numeric<wchar_t>::falsename incorrect");

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_1()
{
    dump_info("Test numeric<wchar_t>::put 1...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill(u8'+');

        bool b1 = true;
        bool b0 = false;
        unsigned long ul1 = 1294967294;
        double d1 =  1.7976931348623157e+308;
        double d2 = 2.2250738585072014e-308;
        long double ld1 = 1.7976931348623157e+308;
        long double ld2 = 2.2250738585072014e-308;
        const void* cv = &ld1;
        
        // cache the num_put facet
        std::wstring oss;
        
        // bool, simple
        obj.put(std::back_inserter(oss), ios, b1);
        if (oss != L"1") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        oss.clear();
        obj.put(std::back_inserter(oss), ios, b0);
        if (oss != L"0") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        // ... and one that does
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, ul1);
        if (oss != L"1.294.967.294+++++++") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        // double
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, d1);
        if (oss != L"1,79769e+308++++++++") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, d2);
        if (oss != L"++++++++2,22507e-308") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
        obj.put(std::back_inserter(oss), ios, d2);
        if (oss != L"+++++++2,225074e-308") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    
        oss.clear();
        ios.width(20);
        ios.precision(10);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
        ios.setf(IOv2::ios_defs::uppercase);
        obj.put(std::back_inserter(oss), ios, d2);
        if (oss != L"+++2,2250738585E-308") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        // long double
        oss.clear();
        obj.put(std::back_inserter(oss), ios, ld1);
        if (oss != L"1,7976931349E+308") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        oss.clear();
        ios.precision(0);
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        obj.put(std::back_inserter(oss), ios, ld2);
        if (oss != L"0") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        // const void*
        oss.clear();
        obj.put(std::back_inserter(oss), ios, cv);
        if (oss.find(obj.decimal_point()) != std::wstring::npos) throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        if (oss.find(u8'x') != 1) throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        long long ll1 = 9223372036854775807LL;
        
        oss.clear();
        obj.put(std::back_inserter(oss), ios, ll1);
        if (oss != L"9.223.372.036.854.775.807") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");        
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("de_DE.ISO-8859-1"), s_ctype_de_8859);
    helper(obj);
    dump_info("Done\n");
}

void test_numeric_wchar_t_put_2()
{
    dump_info("Test numeric<wchar_t>::put 2...");

    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill('+');

        bool b1 = true;
        bool b0 = false;
        unsigned long ul1 = 1294967294;
        unsigned long ul2 = 0;
    
        // cache the num_put facet
        std::wstring oss;
        
        // C
        // bool, more twisted examples
        ios.width(20);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, b0);
        if (oss != L"+++++++++++++++++++0") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        ios.setf(IOv2::ios_defs::boolalpha);
        obj.put(std::back_inserter(oss), ios, b1);
        if (oss != L"true++++++++++++++++") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        // unsigned long, in a locale that does not group
        oss.clear();
        obj.put(std::back_inserter(oss), ios, ul1);
        if (oss != L"1294967294") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, ul2);
        if (oss != L"0+++++++++++++++++++") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");     
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_3()
{
    dump_info("Test numeric<wchar_t>::put 3...");

    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill('+');
    
        long l1 = 2147483647;
        long l2 = -2147483647;
    
        // cache the num_put facet
        std::wstring oss;
    
        // HK
        // long, in a locale that expects grouping
        oss.clear();
        obj.put(std::back_inserter(oss), ios, l1);
        if (oss != L"2,147,483,647") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, l2);
        if (oss != L"-2,147,483,647++++++") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("en_HK.UTF-8"), s_ctype_hk_utf8);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_4()
{
    dump_info("Test numeric<wchar_t>::put 4...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        const std::wstring x(18, u8'x');
        std::wstring res;

        // 01 put(long)
        const long l = 1798;
        res = x;
        auto ret1 = obj.put(res.begin(), ios, l);
        std::wstring sanity1(res.begin(), ret1);
        if (res != L"1798xxxxxxxxxxxxxx") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        if (sanity1 != L"1798") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        // 02 put(long double)
        const long double ld = 1798.0;
        res = x;
        auto ret2 = obj.put(res.begin(), ios, ld);
        std::wstring sanity2(res.begin(), ret2);
        if (res != L"1798xxxxxxxxxxxxxx") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        if (sanity2 != L"1798") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        // 03 put(bool)
        bool b = 1;
        res = x;
        auto ret3 = obj.put(res.begin(), ios, b);
        std::wstring sanity3(res.begin(), ret3);
        if (res != L"1xxxxxxxxxxxxxxxxx") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        if (sanity3 != L"1") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        b = 0;
        res = x;
        ios.setf(IOv2::ios_defs::boolalpha);
        auto ret4 = obj.put(res.begin(), ios, b);
        std::wstring sanity4(res.begin(), ret4);
        if (res != L"falsexxxxxxxxxxxxx") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        if (sanity4 != L"false") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        // 04 put(void*)
        const void* cv = &ld;
        res = x;
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        auto ret5 = obj.put(res.begin(), ios, cv);
        std::wstring sanity5(res.begin(), ret5);
        if (sanity5.size() < 2) throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        if (sanity5[1] != u8'x') throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_5()
{
    dump_info("Test numeric<wchar_t>::put 5...");

    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill('+');

        std::wstring oss;

        long l = 0;

        ios.setf(IOv2::ios_defs::showbase);
        ios.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
        obj.put(std::back_inserter(oss), ios, l);
        if (oss != L"0") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        oss.clear();
        ios.setf(IOv2::ios_defs::showbase);
        ios.setf(IOv2::ios_defs::oct, IOv2::ios_defs::basefield);
        obj.put(std::back_inserter(oss), ios, l);
        if (oss != L"0") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("de_DE.ISO-8859-1"), s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_6()
{
    dump_info("Test numeric<wchar_t>::put 6...");
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill('+');

        std::wstring oss;

        ios.precision(-1);
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        obj.put(std::back_inserter(oss), ios, 30.5);
        if (oss != L"30.500000") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        oss.clear();
        ios.precision(0);
        ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
        obj.put(std::back_inserter(oss), ios, 1.0);
        if (oss != L"1e+00") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_7()
{
    dump_info("Test numeric<wchar_t>::put 7...");

    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        std::wstring oss;

        obj.put(std::back_inserter(oss), ios, static_cast<long>(10));
        if (oss != L"10") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_8()
{
    dump_info("Test numeric<wchar_t>::put 8...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        std::wstring oss;

        bool b = true;
        obj.put(std::back_inserter(oss), ios, b);
        if (oss != L"1") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, b);
        if (oss != L"+1") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_9()
{
    dump_info("Test numeric<wchar_t>::put 9...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill('+');
        ios.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
        
        std::wstring oss;

        {
            long l = -1;
            obj.put(std::back_inserter(oss), ios, l);
            if (oss == L"1") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        }

        {
            long long ll = -1LL;
            oss.clear();
            obj.put(std::back_inserter(oss), ios, ll);
            if (oss == L"1") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_10()
{
    dump_info("Test numeric<wchar_t>::put 10...");
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({3, 2, 1});
    auto p2 = std::make_shared<Punct>("C"); p2->set_grouping({1, 3});
    
    IOv2::numeric<wchar_t> ng1(p1, s_ctype_c);
    IOv2::numeric<wchar_t> ng2(p2, s_ctype_c);
    
    IOv2::ios_base<wchar_t> ios;

    std::wstring oss;
    
    long l1 = 12345678l;
    double d1 = 1234567.0;
    double d2 = 123456.0;
    
    {
        ng1.put(std::back_inserter(oss), ios, l1);
        if (oss != L"1,2,3,45,678") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    }
    {
        ios.precision(1);
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        oss.clear();
        ng2.put(std::back_inserter(oss), ios, d1);
        if (oss != L"123,456,7.0") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    }
    {
        oss.clear();
        ng2.put(std::back_inserter(oss), ios, d2);
        if (oss != L"12,345,6.0") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    }
    dump_info("Done\n");
}

void test_numeric_wchar_t_put_11()
{
    dump_info("Test numeric<wchar_t>::put 11...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill('*');
        ios.setf(IOv2::ios_defs::showpos);

        std::wstring result1, result2, result3;

        long int li1 = 0;
        long int li2 = 5;
        double d1 = 0.0;

        {
            obj.put(std::back_inserter(result1), ios, li1);
            if (result1 != L"+0") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        }
        {
            obj.put(std::back_inserter(result2), ios, li2);
            if (result2 != L"+5") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        }
        {
            obj.put(std::back_inserter(result3), ios, d1);
            if (result3 != L"+0") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_12()
{
    dump_info("Test numeric<wchar_t>::put 12...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill('+');

        std::wstring oss;

        const int precision = 1000;

        ios.precision(precision);
        ios.setf(IOv2::ios_defs::fixed);
        obj.put(std::back_inserter(oss), ios, 1.0);
        if (oss.size() != precision + 2) throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_13()
{
    dump_info("Test numeric<wchar_t>::put 13...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        std::wstring oss;

        unsigned long ul1 = 42UL;
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, ul1);
        if (oss != L"42") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        unsigned long long ull1 = 31ULL;
        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, ull1);
        if (oss != L"31") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_14()
{
    dump_info("Test numeric<wchar_t>::put 14...");

    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill('*');

        std::wstring oss;

        double d0 = 2e20;
        double d1 = -2e20;

        obj.put(std::back_inserter(oss), ios, d0);
        if (oss != L"2e+20") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        oss.clear();
        obj.put(std::back_inserter(oss), ios, d1);
        if (oss != L"-2e+20") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        oss.clear();
        ios.setf(IOv2::ios_defs::uppercase);
        obj.put(std::back_inserter(oss), ios, d0);
        if (oss != L"2E+20") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, d0);
        if (oss != L"+2E+20") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("de_DE.ISO-8859-1"), s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_15()
{
    dump_info("Test numeric<wchar_t>::put 15...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill('*');

        std::wstring oss;

        long l0 = -300000;
        long l1 = 300;
        double d0 = -300000;
        double d1 = 300;

        obj.put(std::back_inserter(oss), ios, l0);
        if (oss != L"-300.000") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        oss.clear();
        obj.put(std::back_inserter(oss), ios, d0);
        if (oss != L"-300.000") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, l1);
        if (oss != L"+300") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, d1);
        if (oss != L"+300") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("de_DE.ISO-8859-1"), s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_put_16()
{
    dump_info("Test numeric<wchar_t>::put 16...");
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping(std::vector{(uint8_t)-1});
    auto p2 = std::make_shared<Punct>("C"); p2->set_grouping(std::vector{(uint8_t)2, (uint8_t)-1});
    auto p3 = std::make_shared<Punct>("C"); p3->set_grouping(std::vector{(uint8_t)1, (uint8_t)2, (uint8_t)-1});
    
    IOv2::numeric<wchar_t> ng1(p1, s_ctype_c);
    IOv2::numeric<wchar_t> ng2(p2, s_ctype_c);
    IOv2::numeric<wchar_t> ng3(p3, s_ctype_c);
    
    IOv2::ios_base<wchar_t> ios;
    ios.fill('+');

    std::wstring oss;

    long l1 = 12345l;
    long l2 = 12345678l;
    double d1 = 1234567.0;
    
    ng1.put(std::back_inserter(oss), ios, l1);
    if (oss != L"12345") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

    oss.clear();
    ng2.put(std::back_inserter(oss), ios, l2);
    if (oss != L"123456,78") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

    ios.precision(1);
    ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
    oss.clear();
    ng3.put(std::back_inserter(oss), ios, d1);
    if (oss != L"1234,56,7.0") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
  
    dump_info("Done\n");
}

void test_numeric_wchar_t_put_17()
{
    dump_info("Test numeric<wchar_t>::put 17...");
    auto p1 = std::make_shared<Punct>("C"); p1->set_falsename(L"-no-");
    IOv2::numeric<wchar_t> ng1(p1, s_ctype_c);
    
    IOv2::ios_base<wchar_t> ios;
    ios.fill('*');

    std::wstring oss;
    ios.width(6);
    ios.setf(IOv2::ios_defs::boolalpha);
    ng1.put(std::back_inserter(oss), ios, false);
    if (oss != L"**-no-") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

    oss.clear();
    ios.width(6);
    ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
    ios.setf(IOv2::ios_defs::boolalpha);
    ng1.put(std::back_inserter(oss), ios, false);
    if (oss != L"**-no-") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

    oss.clear();
    ios.width(6);
    ios.setf(IOv2::ios_defs::internal, IOv2::ios_defs::adjustfield);
    ios.setf(IOv2::ios_defs::boolalpha);
    ng1.put(std::back_inserter(oss), ios, false);
    if (oss != L"**-no-") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    
    oss.clear();
    ios.width(6);
    ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
    ios.setf(IOv2::ios_defs::boolalpha);
    ng1.put(std::back_inserter(oss), ios, false);
    if (oss != L"-no-**") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
  
    dump_info("Done\n");
}

void test_numeric_wchar_t_put_18()
{
    dump_info("Test numeric<wchar_t>::put 18...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill('*');

        std::wstring oss;
        void* p = (void*)0x1;

        ios.width(5);
        obj.put(std::back_inserter(oss), ios, p);
        if (oss != L"**0x1") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        oss.clear();
        ios.width(5);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, p);
        if (oss != L"**0x1") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        oss.clear();
        ios.width(5);
        ios.setf(IOv2::ios_defs::internal, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, p);
        if (oss != L"0x**1") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");

        oss.clear();
        ios.width(5);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, p);
        if (oss != L"0x1**") throw std::runtime_error("IOv2::numeric<wchar_t>::put fails");
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_get_1()
{
    dump_info("Test numeric<wchar_t>::get 1...");

    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill('+');

        std::wstring iss;

        bool b1 = true;
        bool b0 = false;
        unsigned long ul1 = 1294967294;
        unsigned long ul;
        double d1 =  1.02345e+308;
        double d2 = 3.15e-308;
        double d;
        long double ld1 = 6.630025e+4;
        long double ld;
        void* v = 0;

        // bool, simple
        {
            iss = L"1";
            auto it = obj.get(iss.begin(), iss.end(), ios, b1);
            if (b1 != true) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            iss = L"0";
            auto it = obj.get(iss.begin(), iss.end(), ios, b0);
            if (b0 != false) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }

        // ... and one that does
        {
            iss = L"1.294.967.294+------";
            ios.width(20);
            ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != ul1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != '+') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }

        {
            iss = L"+1,02345e+308";
            ios.width(20);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            if (d != d1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }

        {
            iss = L"3,15E-308 ";
            ios.width(20);
            ios.precision(10);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
            ios.setf(IOv2::ios_defs::uppercase);
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            if (d != d2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }

        // long double
        {
            iss = L"6,630025e+4";
            auto it = obj.get(iss.begin(), iss.end(), ios, ld);
            if (ld != ld1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        
        {
            iss = L"0 ";
            ios.precision(0);
            ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, ld);
            if (ld != 0) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }

        // void*
        {
            iss = L"0xbffff74c,";
            auto it = obj.get(iss.begin(), iss.end(), ios, v);
            if (v == 0) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L',') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }

        {
            long long ll1 = 9223372036854775807LL;
            long long ll;

            iss = L"9.223.372.036.854.775.807";
            auto it = obj.get(iss.begin(), iss.end(), ios, ll);
            if (ll != ll1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("de_DE.ISO-8859-1"), s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_get_2()
{
    dump_info("Test numeric<wchar_t>::get 2...");

    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        ios.fill('+');
    
        std::wstring iss;
    
        bool b1 = true;
        bool b0 = false;
        unsigned long ul1 = 1294967294;
        unsigned long ul2 = 0;
        unsigned long ul;
        double d1 =  1.02345e+308;
        double d2 = 3.15e-308;
        double d;
    
        // C
        // bool, more twisted examples
        {
            iss = L"true ";
            ios.setf(IOv2::ios_defs::boolalpha);
            auto it = obj.get(iss.begin(), iss.end(), ios, b0);
            if (b0 != true) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }

        {
            iss = L"false ";
            ios.setf(IOv2::ios_defs::boolalpha);
            auto it = obj.get(iss.begin(), iss.end(), ios, b1);
            if (b1 != false) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }

        // unsigned long
        {
            iss = L"1294967294";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != ul1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            iss = L"0+------------------";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != ul2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L'+') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }

        // double
        {
            iss = L"1.02345e+308+-------";
            ios.width(20);
            ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            if (d != d1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L'+') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            iss = L"+3.15e-308";
            ios.width(20);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            if (d != d2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_get_3()
{
    dump_info("Test numeric<wchar_t>::get 3...");

    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        std::wstring iss;
    
        long l1 = 2147483647;
        long l2 = -2147483647;
        long l;
    
        // HK
        // long, in a locale that expects grouping
        {
            iss = L"2,147,483,647 ";
            auto it = obj.get(iss.begin(), iss.end(), ios, l);
            if (l != l1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            iss = L"-2,147,483,647+-----";
            auto it = obj.get(iss.begin(), iss.end(), ios, l);
            if (l != l2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L'+') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("en_HK.UTF-8"), s_ctype_hk_utf8);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_get_4()
{
    dump_info("Test numeric<wchar_t>::get 4...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        const std::wstring str(L"20000106 Elizabeth Durack");
        const std::wstring str2(L"0 true 0xbffff74c Durack");
    
        {
            // 01 get(long)
            long i = 0;
            auto it = obj.get(str.begin(), str.end(), ios, i);
            if (i != 20000106) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == str.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (std::wstring(it, str.end()) != L" Elizabeth Durack") throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            // 02 get(long double)
            long double ld = 0.0;
            auto it = obj.get(str.begin(), str.end(), ios, ld);
            if (ld != 20000106) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == str.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (std::wstring(it, str.end()) != L" Elizabeth Durack") throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
    
        {
            // 03 get(bool)
            bool b = 1;
            auto it = obj.get(str2.begin(), str2.end(), ios, b);
            if (b != 0) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == str2.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (std::wstring(it, str2.end()) != L" true 0xbffff74c Durack") throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    
            ios.setf(IOv2::ios_defs::boolalpha);
            it = obj.get(++it, str2.end(), ios, b);
            if (b != true) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == str2.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (std::wstring(it, str2.end()) != L" 0xbffff74c Durack") throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            
            // 04 get(void*)
            void* v;
            ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
            it = obj.get(++it, str2.end(), ios, v);
            if (v != (void*)0xbffff74c) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == str2.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (std::wstring(it, str2.end()) != L" Durack") throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_get_5()
{
    dump_info("Test numeric<wchar_t>::get 5...");
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        unsigned long ul;
        std::wstring iss;

        {
            ios.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
            iss = L"0xbf.fff.74c ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != 0xbffff74c) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            iss = L"0Xf.fff ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != 0xffff) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            iss = L"ffe ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != 0xffe) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            ios.setf(IOv2::ios_defs::oct, IOv2::ios_defs::basefield);
            iss = L"07.654.321 ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != 07654321) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            iss = L"07.777 ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != 07777) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            iss = L"776 ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != 0776) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("de_DE.ISO-8859-1"), s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_get_6()
{
    dump_info("Test numeric<wchar_t>::get 6...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        double d = 0.0;
        std::wstring iss;

        iss = L"1234,5 ";
        auto it = obj.get(iss.begin(), iss.end(), ios, d);
        if (d != 1234.5) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("de_DE.ISO-8859-1"), s_ctype_de_8859);
    helper(obj);
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_7()
{
    dump_info("Test numeric<wchar_t>::get 7...");
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        double d = 0.0;
        std::wstring iss;

        {
            iss = L"+e3";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, d);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != L'+') throw std::runtime_error("IOv2::numeric<char32_t>::get fails");
        }
        {
            iss = L".e+1";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, d);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != L'.') throw std::runtime_error("IOv2::numeric<char32_t>::get fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_8()
{
    dump_info("Test numeric<wchar_t>::get 8...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        bool b;
        std::wstring iss;
        {
            ios.setf(IOv2::ios_defs::boolalpha);
            iss = L"faLse";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, b);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != L'f') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = L"falsr";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, b);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != L'f') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = L"trus";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, b);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != L't') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_9()
{
    dump_info("Test numeric<wchar_t>::get 9...");

    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        std::wstring iss;

        double d = 0.0;
        double d1 = 1e1;
        double d2 = 3e1;
        {
            iss = L"1e1,";
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            if (d != d1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L',') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            iss = L"3e1.";
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            if (d != d2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (*it != L'.') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("de_DE.ISO-8859-1"), s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_get_10()
{
    dump_info("Test numeric<wchar_t>::get 10...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        std::wstring iss;

        float f = 1.0f;
        double d = 1.0;
        long double ld = 1.0l;

        {
            iss = L"1e.";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, f);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != L'1') throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (f != 0.0f) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            iss = L"3e+";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, d);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != L'3') throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (d != 0.0) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            iss = L"6e ";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, ld);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != L'6') throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ld != 0.0l) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_11()
{
    dump_info("Test numeric<wchar_t>::get 11...");
    
    IOv2::ios_base<wchar_t> ios;
    
    auto p1 = std::make_shared<Punct>("C");
    p1->set_grouping(std::vector<uint8_t>{1}); p1->set_thousands_sep('2'); p1->set_decimal_point('4');
    const IOv2::numeric<wchar_t> ng1(p1, s_ctype_c);
    
    auto p2 = std::make_shared<Punct>("C");
    p2->set_grouping(std::vector<uint8_t>{1}); p2->set_thousands_sep('2'); p2->set_decimal_point('2');
    const IOv2::numeric<wchar_t> ng2(p2, s_ctype_c);

    double d = 0.0;
    double d1 = 13.0;
    double d2 = 1.0;
    double d3 = 30.0;
    long l = 0l;
    long l1 = 13l;
    long l2 = 10l;
  
    {
        std::wstring iss1 = L"1234";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, d);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    
    {
        std::wstring iss1 = L"142";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, d);
        if (it == iss1.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L'2') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != d2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    
    {
        std::wstring iss1 = L"3e14";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, d);
        if (it == iss1.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L'4') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != d3) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    
    {
        std::wstring iss1 = L"1234";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        if (it == iss1.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L'4') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    
    {
        std::wstring iss2 = L"123";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios, d);
        if (it != iss2.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    
    {
        std::wstring iss2 = L"120";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios, l);
        if (it != iss2.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_12()
{
    dump_info("Test numeric<wchar_t>::get 12...");
    
    IOv2::ios_base<wchar_t> ios, ios2;

    auto p1 = std::make_shared<Punct>("C");
    p1->set_grouping(std::vector<uint8_t>{1}); p1->set_thousands_sep('+'); p1->set_decimal_point('x');
    const IOv2::numeric<wchar_t> ng1(p1, s_ctype_c);
    
    auto p2 = std::make_shared<Punct>("C");
    p2->set_grouping(std::vector<uint8_t>{1}); p2->set_thousands_sep('X'); p2->set_decimal_point('-');
    const IOv2::numeric<wchar_t> ng2(p2, s_ctype_c);

    std::wstring iss1, iss2;
    long l = 1l;
    long l1 = 0l;
    long l2 = 10l;
    long l3 = 1l;
    long l4 = 63l;
    double d = 0.0;
    double d1 = .4;
    double d2 = 0.0;
    double d3 = .1;

    {
        iss1 = L"+3";
        auto it = iss1.begin();
        try
        {
            it = ng1.get(iss1.begin(), iss1.end(), ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != L'+') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"0x1";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        if (it == iss1.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L'x') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"0Xa";
        ios.unsetf(IOv2::ios_defs::basefield);
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"0xa";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        if (it == iss1.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L'x') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"+5";
        auto it = iss1.begin();
        try
        {
            it = ng1.get(iss1.begin(), iss1.end(), ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != L'+') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"x4";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, d);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss2 = L"0001-";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios2, l);
        if (it == iss2.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L'-') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != l3) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss2 = L"-2";
        auto it = iss2.begin();
        try
        {
            it = ng2.get(iss2.begin(), iss2.end(), ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != L'-') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss2 = L"0X1";
        ios2.unsetf(IOv2::ios_defs::basefield);
        auto it = iss2.begin();
        try
        {
            it = ng2.get(iss2.begin(), iss2.end(), ios2, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != L'0') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != 0) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss2 = L"000778";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios2, l);
        if (it == iss2.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L'8') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != l4) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss2 = L"00X";
        auto it = iss2.begin();
        try
        {
            it = ng2.get(iss2.begin(), iss2.end(), ios2, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != L'0') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != d2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss2 = L"-1";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios2, d);
        if (it != iss2.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != d3) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_13()
{
    dump_info("Test numeric<wchar_t>::get 13...");
    
    IOv2::ios_base<wchar_t> ios;
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({3, 2, 1});
    const IOv2::numeric<wchar_t> ng1(p1, s_ctype_c);
    
    auto p2 = std::make_shared<Punct>("C"); p2->set_grouping({1, 3});
    const IOv2::numeric<wchar_t> ng2(p2, s_ctype_c);

    std::wstring iss1, iss2;
    long l = 0l;
    long l1 = 12345678l;
    double d = 0.0;
    double d1 = 1234567.0;
    double d2 = 123456.0;

    {
        iss1 = L"1,2,3,45,678";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss2 = L"123,456,7.0";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios, d);
        if (it != iss2.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss2 = L"12,345,6.0";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios, d);
        if (it != iss2.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != d2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_14()
{
    dump_info("Test numeric<wchar_t>::get 14...");
    
    IOv2::ios_base<wchar_t> ios;
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({1});
    const IOv2::numeric<wchar_t> obj(p1, s_ctype_c);

    std::wstring iss;
    double d = 0.0;
    double d1 = 1000.0;
    {
        iss = L"1,0e2";
        auto it = obj.get(iss.begin(), iss.end(), ios, d);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_15()
{
    dump_info("Test numeric<wchar_t>::get 15...");
    
    IOv2::ios_base<wchar_t> ios;
    auto p1 = std::make_shared<Punct>("C");
    p1->set_grouping({1}); p1->set_thousands_sep('+');
    const IOv2::numeric<wchar_t> ng1(p1, s_ctype_c);

    auto p2 = std::make_shared<Punct>("C");
    p2->set_decimal_point('-');
    const IOv2::numeric<wchar_t> ng2(p2, s_ctype_c);

    std::wstring iss1, iss2;
    double d = 1.0;
    {
        iss1 = L"1e+2";
        auto it = iss1.begin();
        try
        {
            it = ng1.get(iss1.begin(), iss1.end(), ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (d != 0.0) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L'1') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss2 = L"3e-1";
        auto it = iss2.begin();
        try
        {
            it = ng2.get(iss2.begin(), iss2.end(), ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (d != 0.0) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L'3') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_16()
{
    dump_info("Test numeric<wchar_t>::get 16...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        std::wstringstream ss;

        unsigned short us0, us1 = std::numeric_limits<unsigned short>::max();
        unsigned int ui0, ui1 = std::numeric_limits<unsigned int>::max();
        unsigned long ul0, ul1 = std::numeric_limits<unsigned long>::max();
        long l01, l1 = std::numeric_limits<long>::max();
        long l02, l2 = std::numeric_limits<long>::min();
        unsigned long long ull0, ull1 = std::numeric_limits<unsigned long long>::max();
        long long ll01, ll1 = std::numeric_limits<long long>::max();
        long long ll02, ll2 = std::numeric_limits<long long>::min();

        {
            us0 = 0;
            ss << us1; std::wstring str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, us0);
            if (it != str.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (us0 != us1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            us0 = 0;
            ss.clear(); ss.str(L"");
            ss << us1 << '0'; std::wstring str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, us0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (us0 != std::numeric_limits<unsigned short>::max()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            ui0 = 0U;
            ss.clear(); ss.str(L"");
            ss << ui1 << ' '; std::wstring str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ui0);
            if (it == str.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (ui0 != ui1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            ui0 = 0U;
            ss.clear(); ss.str(L"");
            ss << ui1 << '1'; std::wstring str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, ui0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (ui0 != std::numeric_limits<unsigned int>::max()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            ul0 = 0UL;
            ss.clear(); ss.str(L"");
            ss << ul1; std::wstring str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ul0);
            if (it != str.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (ul0 != ul1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            ul0 = 0UL;
            ss.clear(); ss.str(L"");
            ss << ul1 << '2'; std::wstring str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, ul0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (ul0 != std::numeric_limits<unsigned long>::max()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            l01 = 0L;
            ss.clear(); ss.str(L"");
            ss << l1 << ' '; std::wstring str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, l01);
            if (it == str.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (l01 != l1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            l01 = 0L;
            ss.clear(); ss.str(L"");
            ss << l1 << '3'; std::wstring str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, l01);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (l01 != std::numeric_limits<long>::max()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            l02 = 0L;
            ss.clear(); ss.str(L"");
            ss << l2; std::wstring str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, l02);
            if (it != str.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (l02 != l2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            l02 = 0L;
            ss.clear(); ss.str(L"");
            ss << l2 << '4'; std::wstring str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, l02);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (l02 != std::numeric_limits<long>::min()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            ull0 = 0ULL;
            ss.clear(); ss.str(L"");
            ss << ull1 << ' '; std::wstring str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ull0);
            if (it == str.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (ull0 != ull1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            ull0 = 0ULL;
            ss.clear(); ss.str(L"");
            ss << ull1 << '5'; std::wstring str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, ull0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (ull0 != std::numeric_limits<unsigned long long>::max()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            ll01 = 0LL;
            ss.clear(); ss.str(L"");
            ss << ll1; std::wstring str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ll01);
            if (it != str.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (ll01 != ll1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            ll01 = 0LL;
            ss.clear(); ss.str(L"");
            ss << ll1 << '6'; std::wstring str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, ll01);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (ll01 != std::numeric_limits<long long>::max()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            ll02 = 0LL;
            ss.clear(); ss.str(L"");
            ss << ll2 << ' '; std::wstring str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ll02);
            if (it == str.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (ll02 != ll2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            ll02 = 0LL;
            ss.clear(); ss.str(L"");
            ss << ll2 << '7'; std::wstring str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, ll02);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (ll02 != std::numeric_limits<long long>::min()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_wchar_t_get_17()
{
    dump_info("Test numeric<wchar_t>::get 17...");
    
    IOv2::ios_base<wchar_t> ios;
    auto p1 = std::make_shared<Punct>("C");
    p1->set_grouping({1}); p1->set_thousands_sep('#');
    const IOv2::numeric<wchar_t> obj(p1, s_ctype_c);

    std::wstring iss1, iss2;
    long l = 0l;
    long l1 = 1l;
    long l2 = 2l;
    long l3 = 3l;
    double d = 0.0;
    double d1 = 1.0;
    double d2 = 2.0;
    
    {
        iss1 = L"00#0#1";
        auto it = iss1.begin();
        try
        {
            it = obj.get(iss1.begin(), iss1.end(), ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (l != l1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"000##2";
        auto it = iss1.begin();
        try
        {
            it = obj.get(iss1.begin(), iss1.end(), ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (l != 0) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L'0') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"0#0#0#2";
        auto it = obj.get(iss1.begin(), iss1.end(), ios, l);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"00#0#1";
        auto it = iss1.begin();
        try
        {
            it = obj.get(iss1.begin(), iss1.end(), ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (d != d1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"000##2";
        auto it = iss1.begin();
        try
        {
            it = obj.get(iss1.begin(), iss1.end(), ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != L'0') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != 0.0) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"0#0#0#2";
        auto it = obj.get(iss1.begin(), iss1.end(), ios, d);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != d2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"0#0";
        ios.unsetf(IOv2::ios_defs::basefield);
        auto it = iss1.begin();
        try
        {
            it = obj.get(iss1.begin(), iss1.end(), ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != L'0') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != 0) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"00#0#3";
        auto it = obj.get(iss1.begin(), iss1.end(), ios, l);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != l3) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss1 = L"00#02";
        auto it = iss1.begin();
        try
        {
            it = obj.get(iss1.begin(), iss1.end(), ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (l != l2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_18()
{
    dump_info("Test numeric<wchar_t>::get 18...");
    
    IOv2::ios_base<wchar_t> ios, ios2;
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({uint8_t(0)});
    auto p2 = std::make_shared<Punct>("C"); p2->set_grouping({2, uint8_t(0)});
    auto p3 = std::make_shared<Punct>("C"); p3->set_grouping({1, 2, uint8_t(0)});

    const IOv2::numeric<wchar_t> ng1(p1, s_ctype_c);
    const IOv2::numeric<wchar_t> ng2(p2, s_ctype_c);
    const IOv2::numeric<wchar_t> ng3(p3, s_ctype_c);

    std::wstring iss;
    long l = 0l;
    long l1 = 12345l;
    long l2 = 12345678l;
    double d = 0.0;
    double d1 = 1234567.0;

    {
        iss = L"12345";
        auto it = ng1.get(iss.begin(), iss.end(), ios, l);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"123456,78";
        auto it = ng2.get(iss.begin(), iss.end(), ios, l);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"1234,56,7.0";
        auto it = ng3.get(iss.begin(), iss.end(), ios, d);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_19()
{
    dump_info("Test numeric<wchar_t>::get 19...");
    IOv2::ios_base<wchar_t> ios;
    ios.setf(IOv2::ios_defs::boolalpha);
    
    auto p1 = std::make_shared<Punct>("C"); p1->set_truename(L"a"); p1->set_falsename(L"abb");
    auto p2 = std::make_shared<Punct>("C"); p2->set_truename(L"1"); p2->set_falsename(L"0");
    auto p3 = std::make_shared<Punct>("C"); p3->set_truename(L""); p3->set_falsename(L"");
    auto p4 = std::make_shared<Punct>("C"); p4->set_truename(L"one"); p4->set_falsename(L"two");

    const IOv2::numeric<wchar_t> ng0(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    const IOv2::numeric<wchar_t> ng1(p1, s_ctype_c);
    const IOv2::numeric<wchar_t> ng2(p2, s_ctype_c);
    const IOv2::numeric<wchar_t> ng3(p3, s_ctype_c);
    const IOv2::numeric<wchar_t> ng4(p4, s_ctype_c);

    std::wstring iss;
    bool b0 = false;
    bool b1 = false;
    bool b2 = false;
    bool b3 = true;
    bool b4 = false;

    {
        iss = L"true";
        auto it = ng0.get(iss.begin(), iss.end(), ios, b0);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (b0 != true) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"false";
        auto it = ng0.get(iss.begin(), iss.end(), ios, b0);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (b0 != false) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"a";
        auto it = ng1.get(iss.begin(), iss.end(), ios, b1);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (b1 != true) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"abb";
        auto it = ng1.get(iss.begin(), iss.end(), ios, b1);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (b1 != false) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"abc";
        auto it = iss.begin();
        try
        {
            it = ng1.get(iss.begin(), iss.end(), ios, b1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b1 != false) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L'a') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"ab";
        auto it = iss.begin();
        try
        {
            it = ng1.get(iss.begin(), iss.end(), ios, b1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b1 != false) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"1";
        auto it = ng2.get(iss.begin(), iss.end(), ios, b2);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (b2 != true) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"0";
        auto it = ng2.get(iss.begin(), iss.end(), ios, b2);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (b2 != false) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"2";
        auto it = iss.begin();
        try
        {
            it = ng2.get(iss.begin(), iss.end(), ios, b2);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '2') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"blah";
        auto it = iss.begin();
        try
        {
            it = ng3.get(iss.begin(), iss.end(), ios, b3);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b3 != false) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != L'b') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss.clear(); b3 = true;
        auto it = iss.begin();
        try
        {
            it = ng3.get(iss.begin(), iss.end(), ios, b3);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b3 != false) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"one";
        auto it = ng4.get(iss.begin(), iss.end(), ios, b4);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (b4 != true) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"two";
        auto it = ng4.get(iss.begin(), iss.end(), ios, b4);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (b4 != false) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"three"; b4 = true;
        auto it = iss.begin();
        try
        {
            it = ng4.get(iss.begin(), iss.end(), ios, b4);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != L't') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    {
        iss = L"on"; b4 = true;
        auto it = iss.begin();
        try
        {
            it = ng4.get(iss.begin(), iss.end(), ios, b4);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b4 != false) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_20()
{
    dump_info("Test numeric<wchar_t>::get 20...");
    IOv2::ios_base<wchar_t> ios;
    long double l = -1;
    
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({CHAR_MAX});
    const IOv2::numeric<wchar_t> obj(p1, s_ctype_c);
     
    std::wstring iss = L"123,456";
    auto it = obj.get(iss.begin(), iss.end(), ios, l);
    if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    if (l != 123) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    if (*it != L',') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    
    dump_info("Done\n");
}

void test_numeric_wchar_t_get_21()
{
    dump_info("Test numeric<wchar_t>::get 21...");
    
    auto helper = [](const IOv2::numeric<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        std::wstring ss;

        unsigned long ul0 = 1;
        const unsigned long ul1 = std::numeric_limits<unsigned long>::max();

        {
            ss  = L"-0";
            auto it = obj.get(ss.begin(), ss.end(), ios, ul0);
            if (it != ss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (ul0 != 0) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            ss = L"-1";
            auto it = obj.get(ss.begin(), ss.end(), ios, ul0);
            if (it != ss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (ul0 != ul1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            std::wstringstream ss0;
            ss0 << '-' << ul1; ss = ss0.str();
            auto it = obj.get(ss.begin(), ss.end(), ios, ul0);
            if (it != ss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
            if (ul0 != 1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
        {
            std::wstringstream ss0;
            ss0 << '-' << ul1 << '0'; ss = ss0.str();
            auto it = ss.begin();
            try
            {
                it = obj.get(ss.begin(), ss.end(), ios, ul0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (ul0 != ul1) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        }
    };
    
    IOv2::numeric<wchar_t> obj(std::make_shared<IOv2::numeric_conf<wchar_t>>("C"), s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}