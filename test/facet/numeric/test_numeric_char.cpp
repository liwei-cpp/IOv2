#include <deque>
#include <list>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <facet/numeric.h>
#include <io/streambuf_iterator.h>
#include <common/verify.h>

#include <common/dump_info.h>

namespace
{
    struct Punct: IOv2::numeric_conf<char>
    {
        Punct(const std::string& n)
            : IOv2::numeric_conf<char>(n)
            , m_grouping(IOv2::numeric_conf<char>::grouping())
            , m_truename(IOv2::numeric_conf<char>::truename())
            , m_falsename(IOv2::numeric_conf<char>::falsename())
            , m_thousands_sep(IOv2::numeric_conf<char>::thousands_sep())
            , m_decimal_point(IOv2::numeric_conf<char>::decimal_point())
        {}
        
        const std::vector<uint8_t>& grouping() const override { return m_grouping; };
        const std::string& truename() const override { return m_truename; }
        const std::string& falsename() const override { return m_falsename; }
        char thousands_sep() const override { return m_thousands_sep; }
        char decimal_point() const override { return m_decimal_point; }
        
        void set_grouping(std::vector<uint8_t> g)
        {
            m_grouping = std::move(g);
        }

        void set_truename(std::string n) { m_truename = std::move(n); }
        void set_falsename(std::string n)
        {
            m_falsename = std::move(n);
        }
        
        void set_thousands_sep(char c) { m_thousands_sep = c; }
        void set_decimal_point(char c) { m_decimal_point = c; }
    private:
        std::vector<uint8_t> m_grouping;
        std::string m_truename;
        std::string m_falsename;
        char m_thousands_sep;
        char m_decimal_point;
    };

    std::shared_ptr<IOv2::ctype<char>> s_ctype_c
        = std::make_shared<IOv2::ctype<char>>(std::make_shared<IOv2::ctype_conf<char>>("C"));

    std::shared_ptr<IOv2::ctype<char>> s_ctype_de_utf8
        = std::make_shared<IOv2::ctype<char>>(std::make_shared<IOv2::ctype_conf<char>>("de_DE.UTF-8"));

    std::shared_ptr<IOv2::ctype<char>> s_ctype_de_8859
        = std::make_shared<IOv2::ctype<char>>(std::make_shared<IOv2::ctype_conf<char>>("de_DE.ISO-8859-1"));

    std::shared_ptr<IOv2::ctype<char>> s_ctype_hk_utf8
        = std::make_shared<IOv2::ctype<char>>(std::make_shared<IOv2::ctype_conf<char>>("en_HK.UTF-8"));
}

void test_numeric_char_common_1()
{
    dump_info("Test numeric<char> common 1...");
    static_assert(std::is_same_v<IOv2::numeric<char>::char_type, char>);
    
    IOv2::numeric<char> nump_c(std::make_shared<IOv2::numeric_conf<char>>("C"),
                               s_ctype_c);
    IOv2::numeric<char> nump_de(std::make_shared<IOv2::numeric_conf<char>>("de_DE.UTF-8"),
                                s_ctype_de_utf8);

    if (nump_c.decimal_point() == nump_de.decimal_point())
        throw std::runtime_error("numeric<char>::decimal_point incorrect");
    if (nump_c.thousands_sep() == nump_de.thousands_sep())
        throw std::runtime_error("numeric<char>::thousands_sep incorrect");
    if (nump_c.grouping() == nump_de.grouping())
        throw std::runtime_error("numeric<char>::grouping incorrect");
        
    if (nump_c.truename().empty())
        throw std::runtime_error("numeric<char>::truename incorrect");
    if (nump_de.truename().empty())
        throw std::runtime_error("numeric<char>::truename incorrect");
    if (nump_c.truename() == nump_de.truename())
        throw std::runtime_error("numeric<char>::truename incorrect");

    if (nump_c.falsename().empty())
        throw std::runtime_error("numeric<char>::falsename incorrect");
    if (nump_de.falsename().empty())
        throw std::runtime_error("numeric<char>::falsename incorrect");
    if (nump_c.falsename() == nump_de.falsename())
        throw std::runtime_error("numeric<char>::falsename incorrect");
    dump_info("Done\n");
}

void test_numeric_char_put_1()
{
    dump_info("Test numeric<char>::put 1...");
    
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('+');
    
        bool b1 = true;
        bool b0 = false;
        unsigned long ul1 = 1294967294;
        double d1 =  1.7976931348623157e+308;
        double d2 = 2.2250738585072014e-308;
        long double ld1 = 1.7976931348623157e+308;
        long double ld2 = 2.2250738585072014e-308;
        const void* cv = &ld1;
        
        // cache the num_put facet
        std::string oss;
        
        // bool, simple
        obj.put(std::back_inserter(oss), ios, b1);
        if (oss != "1") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        obj.put(std::back_inserter(oss), ios, b0);
        if (oss != "0") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        // ... and one that does
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, ul1);
        if (oss != "1.294.967.294+++++++") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        // double
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, d1);
        if (oss != "1,79769e+308++++++++") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, d2);
        if (oss != "++++++++2,22507e-308") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
        obj.put(std::back_inserter(oss), ios, d2);
        if (oss != "+++++++2,225074e-308") throw std::runtime_error("IOv2::numeric<char>::put fails");
    
        oss.clear();
        ios.width(20);
        ios.precision(10);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
        ios.setf(IOv2::ios_defs::uppercase);
        obj.put(std::back_inserter(oss), ios, d2);
        if (oss != "+++2,2250738585E-308") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        // long double
        oss.clear();
        obj.put(std::back_inserter(oss), ios, ld1);
        if (oss != "1,7976931349E+308") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        ios.precision(0);
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        obj.put(std::back_inserter(oss), ios, ld2);
        if (oss != "0") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        // const void*
        oss.clear();
        obj.put(std::back_inserter(oss), ios, cv);
        if (oss.find(obj.decimal_point()) != std::string::npos) throw std::runtime_error("IOv2::numeric<char>::put fails");
        if (oss.find('x') != 1) throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        long long ll1 = 9223372036854775807LL;
        
        oss.clear();
        obj.put(std::back_inserter(oss), ios, ll1);
        if (oss != "9.223.372.036.854.775.807") throw std::runtime_error("IOv2::numeric<char>::put fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_put_2()
{
    dump_info("Test numeric<char>::put 2...");
    
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('+');
        
        bool b1 = true;
        bool b0 = false;
        unsigned long ul1 = 1294967294;
        unsigned long ul2 = 0;
    
        // cache the num_put facet
        std::string oss;
        
        // C
        // bool, more twisted examples
        ios.width(20);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, b0);
        if (oss != "+++++++++++++++++++0") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        ios.setf(IOv2::ios_defs::boolalpha);
        obj.put(std::back_inserter(oss), ios, b1);
        if (oss != "true++++++++++++++++") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        // unsigned long, in a locale that does not group
        oss.clear();
        obj.put(std::back_inserter(oss), ios, ul1);
        if (oss != "1294967294") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, ul2);
        if (oss != "0+++++++++++++++++++") throw std::runtime_error("IOv2::numeric<char>::put fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);
    dump_info("Done\n");
}

void test_numeric_char_put_3()
{
    dump_info("Test numeric<char>::put 3...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('+');

        long l1 = 2147483647;
        long l2 = -2147483647;

        // cache the num_put facet
        std::string oss;

        // HK
        // long, in a locale that expects grouping
        oss.clear();
        obj.put(std::back_inserter(oss), ios, l1);
        if (oss != "2,147,483,647") throw std::runtime_error("IOv2::numeric<char>::put fails");

        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, l2);
        if (oss != "-2,147,483,647++++++") throw std::runtime_error("IOv2::numeric<char>::put fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("en_HK.UTF-8"),
                            s_ctype_hk_utf8);
    helper(obj);
    dump_info("Done\n");
}

void test_numeric_char_put_4()
{
    dump_info("Test numeric<char>::put 4...");
    
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        
        const std::string x(18, 'x');
        std::string res;
        
        // 01 put(long)
        const long l = 1798;
        res = x;
        auto ret1 = obj.put(res.begin(), ios, l);
        std::string sanity1(res.begin(), ret1);
        if (res != "1798xxxxxxxxxxxxxx") throw std::runtime_error("IOv2::numeric<char>::put fails");
        if (sanity1 != "1798") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        // 02 put(long double)
        const long double ld = 1798.0;
        res = x;
        auto ret2 = obj.put(res.begin(), ios, ld);
        std::string sanity2(res.begin(), ret2);
        if (res != "1798xxxxxxxxxxxxxx") throw std::runtime_error("IOv2::numeric<char>::put fails");
        if (sanity2 != "1798") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        // 03 put(bool)
        bool b = 1;
        res = x;
        auto ret3 = obj.put(res.begin(), ios, b);
        std::string sanity3(res.begin(), ret3);
        if (res != "1xxxxxxxxxxxxxxxxx") throw std::runtime_error("IOv2::numeric<char>::put fails");
        if (sanity3 != "1") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        b = 0;
        res = x;
        ios.setf(IOv2::ios_defs::boolalpha);
        auto ret4 = obj.put(res.begin(), ios, b);
        std::string sanity4(res.begin(), ret4);
        if (res != "falsexxxxxxxxxxxxx") throw std::runtime_error("IOv2::numeric<char>::put fails");
        if (sanity4 != "false") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        // 04 put(void*)
        const void* cv = &ld;
        res = x;
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        auto ret5 = obj.put(res.begin(), ios, cv);
        std::string sanity5(res.begin(), ret5);
        if (sanity5.size() < 2) throw std::runtime_error("IOv2::numeric<char>::put fails");
        if (sanity5[1] != 'x') throw std::runtime_error("IOv2::numeric<char>::put fails");
    };
    
    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_put_5()
{
    dump_info("Test numeric<char>::put 5...");
    
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('+');
        
        std::string oss;
    
        long l = 0;
    
        ios.setf(IOv2::ios_defs::showbase);
        ios.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
        obj.put(std::back_inserter(oss), ios, l);
        if (oss != "0") throw std::runtime_error("IOv2::numeric<char>::put fails");
    
        oss.clear();
        ios.setf(IOv2::ios_defs::showbase);
        ios.setf(IOv2::ios_defs::oct, IOv2::ios_defs::basefield);
        obj.put(std::back_inserter(oss), ios, l);
        if (oss != "0") throw std::runtime_error("IOv2::numeric<char>::put fails");
    };
    
    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_put_6()
{
    dump_info("Test numeric<char>::put 6...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('+');
        
        std::string oss;
    
        ios.precision(-1);
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        obj.put(std::back_inserter(oss), ios, 30.5);
        if (oss != "30.500000") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        ios.precision(0);
        ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
        obj.put(std::back_inserter(oss), ios, 1.0);
        if (oss != "1e+00") throw std::runtime_error("IOv2::numeric<char>::put fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_put_7()
{
    dump_info("Test numeric<char>::put 7...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        std::string oss;

        obj.put(std::back_inserter(oss), ios, static_cast<long>(10));
        if (oss != "10") throw std::runtime_error("IOv2::numeric<char>::put fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_put_8()
{
    dump_info("Test numeric<char>::put 8...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
    
        std::string oss;
    
        bool b = true;
        obj.put(std::back_inserter(oss), ios, b);
        if (oss != "1") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, b);
        if (oss != "+1") throw std::runtime_error("IOv2::numeric<char>::put fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_put_9()
{
    dump_info("Test numeric<char>::put 9...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('+');
        ios.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
        
        std::string oss;
    
        {
            long l = -1;
            obj.put(std::back_inserter(oss), ios, l);
            if (oss == "1") throw std::runtime_error("IOv2::numeric<char>::put fails");
        }
    
        {
            long long ll = -1LL;
            oss.clear();
            obj.put(std::back_inserter(oss), ios, ll);
            if (oss == "1") throw std::runtime_error("IOv2::numeric<char>::put fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_put_10()
{
    dump_info("Test numeric<char>::put 10...");
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({3, 2, 1});
    auto p2 = std::make_shared<Punct>("C"); p2->set_grouping({1, 3});
    
    IOv2::numeric<char> ng1(p1, s_ctype_c);
    IOv2::numeric<char> ng2(p2, s_ctype_c);
    
    IOv2::ios_base<char> ios;

    std::string oss;
    
    long l1 = 12345678l;
    double d1 = 1234567.0;
    double d2 = 123456.0;
    
    {
        ng1.put(std::back_inserter(oss), ios, l1);
        if (oss != "1,2,3,45,678") throw std::runtime_error("IOv2::numeric<char>::put fails");
    }
    {
        ios.precision(1);
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        oss.clear();
        ng2.put(std::back_inserter(oss), ios, d1);
        if (oss != "123,456,7.0") throw std::runtime_error("IOv2::numeric<char>::put fails");
    }
    {
        oss.clear();
        ng2.put(std::back_inserter(oss), ios, d2);
        if (oss != "12,345,6.0") throw std::runtime_error("IOv2::numeric<char>::put fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_put_11()
{
    dump_info("Test numeric<char>::put 11...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('*');
        ios.setf(IOv2::ios_defs::showpos);
        
        std::string result1, result2, result3;
    
        long int li1 = 0;
        long int li2 = 5;
        double d1 = 0.0;
        
        {
            obj.put(std::back_inserter(result1), ios, li1);
            if (result1 != "+0") throw std::runtime_error("IOv2::numeric<char>::put fails");
        }
        {
            obj.put(std::back_inserter(result2), ios, li2);
            if (result2 != "+5") throw std::runtime_error("IOv2::numeric<char>::put fails");
        }
        {
            obj.put(std::back_inserter(result3), ios, d1);
            if (result3 != "+0") throw std::runtime_error("IOv2::numeric<char>::put fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_put_12()
{
    dump_info("Test numeric<char>::put 12...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('+');
    
        std::string oss;
    
        const int precision = 1000;
        
        ios.precision(precision);
        ios.setf(IOv2::ios_defs::fixed);
        obj.put(std::back_inserter(oss), ios, 1.0);
        if (oss.size() != precision + 2) throw std::runtime_error("IOv2::numeric<char>::put fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);
    dump_info("Done\n");
}

void test_numeric_char_put_13()
{
    dump_info("Test numeric<char>::put 13...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        std::string oss;
        
        unsigned long ul1 = 42UL;
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, ul1);
        if (oss != "42") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        unsigned long long ull1 = 31ULL;
        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, ull1);
        if (oss != "31") throw std::runtime_error("IOv2::numeric<char>::put fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);
    dump_info("Done\n");
}

void test_numeric_char_put_14()
{
    dump_info("Test numeric<char>::put 14...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('*');

        std::string oss;
        
        double d0 = 2e20;
        double d1 = -2e20;
        
        obj.put(std::back_inserter(oss), ios, d0);
        if (oss != "2e+20") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        obj.put(std::back_inserter(oss), ios, d1);
        if (oss != "-2e+20") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        ios.setf(IOv2::ios_defs::uppercase);
        obj.put(std::back_inserter(oss), ios, d0);
        if (oss != "2E+20") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, d0);
        if (oss != "+2E+20") throw std::runtime_error("IOv2::numeric<char>::put fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_put_15()
{
    dump_info("Test numeric<char>::put 15...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('*');
    
        std::string oss;
        
        long l0 = -300000;
        long l1 = 300;
        double d0 = -300000;
        double d1 = 300;
        
        obj.put(std::back_inserter(oss), ios, l0);
        if (oss != "-300.000") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        obj.put(std::back_inserter(oss), ios, d0);
        if (oss != "-300.000") throw std::runtime_error("IOv2::numeric<char>::put fails");
    
        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, l1);
        if (oss != "+300") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, d1);
        if (oss != "+300") throw std::runtime_error("IOv2::numeric<char>::put fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_put_16()
{
    dump_info("Test numeric<char>::put 16...");
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping(std::vector{(uint8_t)-1});
    auto p2 = std::make_shared<Punct>("C"); p2->set_grouping(std::vector{(uint8_t)2, (uint8_t)-1});
    auto p3 = std::make_shared<Punct>("C"); p3->set_grouping(std::vector{(uint8_t)1, (uint8_t)2, (uint8_t)-1});
    
    IOv2::numeric<char> ng1(p1, s_ctype_c);
    IOv2::numeric<char> ng2(p2, s_ctype_c);
    IOv2::numeric<char> ng3(p3, s_ctype_c);
    
    IOv2::ios_base<char> ios;
    ios.fill('+');

    std::string oss;

    long l1 = 12345l;
    long l2 = 12345678l;
    double d1 = 1234567.0;
    
    ng1.put(std::back_inserter(oss), ios, l1);
    if (oss != "12345") throw std::runtime_error("IOv2::numeric<char>::put fails");

    oss.clear();
    ng2.put(std::back_inserter(oss), ios, l2);
    if (oss != "123456,78") throw std::runtime_error("IOv2::numeric<char>::put fails");

    ios.precision(1);
    ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
    oss.clear();
    ng3.put(std::back_inserter(oss), ios, d1);
    if (oss != "1234,56,7.0") throw std::runtime_error("IOv2::numeric<char>::put fails");
  
    dump_info("Done\n");
}

void test_numeric_char_put_17()
{
    dump_info("Test numeric<char>::put 17...");
    auto p1 = std::make_shared<Punct>("C"); p1->set_falsename("-no-");
    IOv2::numeric<char> ng1(p1, s_ctype_c);
    
    IOv2::ios_base<char> ios;
    ios.fill('*');

    std::string oss;

    ios.width(6);
    ios.setf(IOv2::ios_defs::boolalpha);
    ng1.put(std::back_inserter(oss), ios, false);
    if (oss != "**-no-") throw std::runtime_error("IOv2::numeric<char>::put fails");

    oss.clear();
    ios.width(6);
    ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
    ios.setf(IOv2::ios_defs::boolalpha);
    ng1.put(std::back_inserter(oss), ios, false);
    if (oss != "**-no-") throw std::runtime_error("IOv2::numeric<char>::put fails");

    oss.clear();
    ios.width(6);
    ios.setf(IOv2::ios_defs::internal, IOv2::ios_defs::adjustfield);
    ios.setf(IOv2::ios_defs::boolalpha);
    ng1.put(std::back_inserter(oss), ios, false);
    if (oss != "**-no-") throw std::runtime_error("IOv2::numeric<char>::put fails");
    
    oss.clear();
    ios.width(6);
    ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
    ios.setf(IOv2::ios_defs::boolalpha);
    ng1.put(std::back_inserter(oss), ios, false);
    if (oss != "-no-**") throw std::runtime_error("IOv2::numeric<char>::put fails");
  
    dump_info("Done\n");
}

void test_numeric_char_put_18()
{
    dump_info("Test numeric<char>::put 18...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('*');
    
        std::string oss;
    
        void* p = (void*)0x1;
    
        ios.width(5);
        obj.put(std::back_inserter(oss), ios, p);
        if (oss != "**0x1") throw std::runtime_error("IOv2::numeric<char>::put fails");
    
        oss.clear();
        ios.width(5);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, p);
        if (oss != "**0x1") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        ios.width(5);
        ios.setf(IOv2::ios_defs::internal, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, p);
        if (oss != "0x**1") throw std::runtime_error("IOv2::numeric<char>::put fails");
        
        oss.clear();
        ios.width(5);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, p);
        if (oss != "0x1**") throw std::runtime_error("IOv2::numeric<char>::put fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_1()
{
    dump_info("Test numeric<char>::get 1...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('+');
    
        std::string iss;
    
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
            iss = "1";
            auto it = obj.get(iss.begin(), iss.end(), ios, b1);
            if (b1 != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "0";
            auto it = obj.get(iss.begin(), iss.end(), ios, b0);
            if (b0 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        
        // ... and one that does
        {
            iss = "1.294.967.294+-----";
            ios.width(20);
            ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != ul1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        {
            iss = "+1,02345e+308";
            ios.width(20);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        
        {
            iss = "3,15E-308 ";
            ios.width(20);
            ios.precision(10);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
            ios.setf(IOv2::ios_defs::uppercase);
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        
        // long double
        {
            iss = "6,630025e+4";
            auto it = obj.get(iss.begin(), iss.end(), ios, ld);
            if (ld != ld1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        
        {
            iss = "0 ";
            ios.precision(0);
            ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, ld);
            if (ld != 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        
        // void*
        {
            iss = "0xbffff74c,";
            auto it = obj.get(iss.begin(), iss.end(), ios, v);
            if (v == 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ',') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        {
            long long ll1 = 9223372036854775807LL;
            long long ll;
            
            iss = "9.223.372.036.854.775.807";
            auto it = obj.get(iss.begin(), iss.end(), ios, ll);
            if (ll != ll1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_2()
{
    dump_info("Test numeric<char>::get 2...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        ios.fill('+');
    
        std::string iss;
    
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
            iss = "true ";
            ios.setf(IOv2::ios_defs::boolalpha);
            auto it = obj.get(iss.begin(), iss.end(), ios, b0);
            if (b0 != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        {
            iss = "false ";
            ios.setf(IOv2::ios_defs::boolalpha);
            auto it = obj.get(iss.begin(), iss.end(), ios, b1);
            if (b1 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        // unsigned long
        {
            iss = "1294967294";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != ul1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "0+----------------------";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != ul2) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        // double
        {
            iss = "1.02345e+308+-------";
            ios.width(20);
            ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "+3.15e-308";
            ios.width(20);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_3()
{
    dump_info("Test numeric<char>::get 3...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
    
        std::string iss;
    
        long l1 = 2147483647;
        long l2 = -2147483647;
        long l;
    
        // HK
        // long, in a locale that expects grouping
        {
            iss = "2,147,483,647 ";
            auto it = obj.get(iss.begin(), iss.end(), ios, l);
            if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "-2,147,483,647+-----";
            auto it = obj.get(iss.begin(), iss.end(), ios, l);
            if (l != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("en_HK.UTF-8"),
                            s_ctype_hk_utf8);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_4()
{
    dump_info("Test numeric<char>::get 4...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
    
        const std::string str("20000106 Elizabeth Durack");
        const std::string str2("0 true 0xbffff74c Durack");
    
        {
            // 01 get(long)
            long i = 0;
            auto it = obj.get(str.begin(), str.end(), ios, i);
            if (i != 20000106) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == str.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (std::string(it, str.end()) != " Elizabeth Durack") throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            // 02 get(long double)
            long double ld = 0.0;
            auto it = obj.get(str.begin(), str.end(), ios, ld);
            if (ld != 20000106) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == str.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (std::string(it, str.end()) != " Elizabeth Durack") throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        {
            // 03 get(bool)
            bool b = 1;
            auto it = obj.get(str2.begin(), str2.end(), ios, b);
            if (b != 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == str2.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (std::string(it, str2.end()) != " true 0xbffff74c Durack") throw std::runtime_error("IOv2::numeric<char>::get fails");
    
            ios.setf(IOv2::ios_defs::boolalpha);
            it = obj.get(++it, str2.end(), ios, b);
            if (b != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == str2.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (std::string(it, str2.end()) != " 0xbffff74c Durack") throw std::runtime_error("IOv2::numeric<char>::get fails");
            
            // 04 get(void*)
            void* v;
            ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
            it = obj.get(++it, str2.end(), ios, v);
            if (v != (void*)0xbffff74c) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == str2.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (std::string(it, str2.end()) != " Durack") throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_5()
{
    dump_info("Test numeric<char>::get 5...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        
        unsigned long ul;
        std::string iss;
        
        {
            ios.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
            iss = "0xbf.fff.74c ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != 0xbffff74c) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "0Xf.fff ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != 0xffff) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "ffe ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != 0xffe) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ios.setf(IOv2::ios_defs::oct, IOv2::ios_defs::basefield);
            iss = "07.654.321 ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != 07654321) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "07.777 ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != 07777) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "776 ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            if (ul != 0776) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_6()
{
    dump_info("Test numeric<char>::get 6...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        
        double d = 0.0;
        std::string iss;
        
        iss = "1234,5 ";
        auto it = obj.get(iss.begin(), iss.end(), ios, d);
        if (d != 1234.5) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (it == iss.end()) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != ' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);
    dump_info("Done\n");
}

void test_numeric_char_get_7()
{
    dump_info("Test numeric<char>::get 7...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        
        double d = 0.0;
        std::string iss;
        
        {
            iss = "+e3";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, d);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = ".e+1";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, d);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != '.') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_8()
{
    dump_info("Test numeric<char>::get 8...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        
        bool b;
        std::string iss;
        
        {
            ios.setf(IOv2::ios_defs::boolalpha);
            iss = "faLse";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, b);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != 'f') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "falsr";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, b);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != 'f') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "trus";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, b);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != 't') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_9()
{
    dump_info("Test numeric<char>::get 9...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        std::string iss;

        double d = 0.0;
        double d1 = 1e1;
        double d2 = 3e1;
        {
            iss = "1e1,";
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ',') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "3e1.";
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != '.') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_10()
{
    dump_info("Test numeric<char>::get 10...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        std::string iss;

        float f = 1.0f;
        double d = 1.0;
        long double ld = 1.0l;

        {
            iss = "1e.";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, f);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != '1') throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (f != 0.0f) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "3e+";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, d);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != '3') throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (d != 0.0) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            iss = "6e ";
            auto it = iss.begin();
            try
            {
                it = obj.get(iss.begin(), iss.end(), ios, ld);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != '6') throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ld != 0.0l) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);
    dump_info("Done\n");
}

void test_numeric_char_get_11()
{
    dump_info("Test numeric<char>::get 11...");
    
    IOv2::ios_base<char> ios;
    
    auto p1 = std::make_shared<Punct>("C");
    p1->set_grouping(std::vector<uint8_t>{1}); p1->set_thousands_sep('2'); p1->set_decimal_point('4');
    const IOv2::numeric<char> ng1(p1, s_ctype_c);
    
    auto p2 = std::make_shared<Punct>("C");
    p2->set_grouping(std::vector<uint8_t>{1}); p2->set_thousands_sep('2'); p2->set_decimal_point('2');
    const IOv2::numeric<char> ng2(p2, s_ctype_c);

    double d = 0.0;
    double d1 = 13.0;
    double d2 = 1.0;
    double d3 = 30.0;
    long l = 0l;
    long l1 = 13l;
    long l2 = 10l;

    {
        std::string iss1 = "1234";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, d);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }

    {
        std::string iss1 = "142";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, d);
        if (it == iss1.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '2') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }

    {
        std::string iss1 = "3e14";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, d);
        if (it == iss1.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '4') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d3) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }

    {
        std::string iss1 = "1234";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        if (it == iss1.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '4') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }

    {
        std::string iss2 = "123";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios, d);
        if (it != iss2.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }

    {
        std::string iss2 = "120";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios, l);
        if (it != iss2.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_12()
{
    dump_info("Test numeric<char>::get 12...");
    
    IOv2::ios_base<char> ios, ios2;

    auto p1 = std::make_shared<Punct>("C");
    p1->set_grouping(std::vector<uint8_t>{1}); p1->set_thousands_sep('+'); p1->set_decimal_point('x');
    const IOv2::numeric<char> ng1(p1, s_ctype_c);
    
    auto p2 = std::make_shared<Punct>("C");
    p2->set_grouping(std::vector<uint8_t>{1}); p2->set_thousands_sep('X'); p2->set_decimal_point('-');
    const IOv2::numeric<char> ng2(p2, s_ctype_c);

    std::string iss1, iss2;
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
        iss1 = "+3";
        auto it = iss1.begin();
        try
        {
            it = ng1.get(iss1.begin(), iss1.end(), ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "0x1";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        if (it == iss1.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != 'x') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "0Xa";
        ios.unsetf(IOv2::ios_defs::basefield);
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "0xa";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        if (it == iss1.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != 'x') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "+5";
        auto it = iss1.begin();
        try
        {
            it = ng1.get(iss1.begin(), iss1.end(), ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "x4";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, d);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss2 = "0001-";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios2, l);
        if (it == iss2.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '-') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l3) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss2 = "-2";
        auto it = iss2.begin();
        try
        {
            it = ng2.get(iss2.begin(), iss2.end(), ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '-') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss2 = "0X1";
        ios2.unsetf(IOv2::ios_defs::basefield);

        auto it = iss2.begin();
        try
        {
            it = ng2.get(iss2.begin(), iss2.end(), ios2, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '0') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss2 = "000778";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios2, l);
        if (it == iss2.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '8') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l4) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss2 = "00X";
        auto it = iss2.begin();
        try
        {
            it = ng2.get(iss2.begin(), iss2.end(), ios2, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '0') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss2 = "-1";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios2, d);
        if (it != iss2.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d3) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_13()
{
    dump_info("Test numeric<char>::get 13...");
    
    IOv2::ios_base<char> ios;
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({3, 2, 1});
    const IOv2::numeric<char> ng1(p1, s_ctype_c);
    
    auto p2 = std::make_shared<Punct>("C"); p2->set_grouping({1, 3});
    const IOv2::numeric<char> ng2(p2, s_ctype_c);

    std::string iss1, iss2;
    long l = 0l;
    long l1 = 12345678l;
    double d = 0.0;
    double d1 = 1234567.0;
    double d2 = 123456.0;

    {
        iss1 = "1,2,3,45,678";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss2 = "123,456,7.0";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios, d);
        if (it != iss2.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss2 = "12,345,6.0";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios, d);
        if (it != iss2.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_14()
{
    dump_info("Test numeric<char>::get 14...");
    
    IOv2::ios_base<char> ios;
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({1});
    const IOv2::numeric<char> obj(p1, s_ctype_c);

    std::string iss;
    double d = 0.0;
    double d1 = 1000.0;
    {
        iss = "1,0e2";
        auto it = obj.get(iss.begin(), iss.end(), ios, d);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_15()
{
    dump_info("Test numeric<char>::get 15...");
    
    IOv2::ios_base<char> ios;
    auto p1 = std::make_shared<Punct>("C");
    p1->set_grouping({1}); p1->set_thousands_sep('+');
    const IOv2::numeric<char> ng1(p1, s_ctype_c);

    auto p2 = std::make_shared<Punct>("C");
    p2->set_decimal_point('-');
    const IOv2::numeric<char> ng2(p2, s_ctype_c);

    std::string iss1, iss2;
    double d = 1.0;
    {
        iss1 = "1e+2";
        auto it = iss1.begin();
        try
        {
            it = ng1.get(iss1.begin(), iss1.end(), ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (d != 0.0) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '1') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss2 = "3e-1";
        auto it = iss2.begin();
        try
        {
            it = ng2.get(iss2.begin(), iss2.end(), ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (d != 0.0) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '3') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_16()
{ 
    dump_info("Test numeric<char>::get 16...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        std::stringstream ss;

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
            ss << us1; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, us0);
            if (it != str.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (us0 != us1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            us0 = 0;
            ss.clear(); ss.str("");
            ss << us1 << '0'; std::string str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, us0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (us0 != std::numeric_limits<unsigned short>::max()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ui0 = 0U;
            ss.clear(); ss.str("");
            ss << ui1 << ' '; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ui0);
            if (it == str.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ui0 != ui1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ui0 = 0U;
            ss.clear(); ss.str("");
            ss << ui1 << '1'; std::string str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, ui0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (ui0 != std::numeric_limits<unsigned int>::max()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ul0 = 0UL;
            ss.clear(); ss.str("");
            ss << ul1; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ul0);
            if (it != str.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ul0 != ul1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ul0 = 0UL;
            ss.clear(); ss.str("");
            ss << ul1 << '2'; std::string str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, ul0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (ul0 != std::numeric_limits<unsigned long>::max()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            l01 = 0L;
            ss.clear(); ss.str("");
            ss << l1 << ' '; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, l01);
            if (it == str.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (l01 != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            l01 = 0L;
            ss.clear(); ss.str("");
            ss << l1 << '3'; std::string str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, l01);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (l01 != std::numeric_limits<long>::max()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            l02 = 0L;
            ss.clear(); ss.str("");
            ss << l2; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, l02);
            if (it != str.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (l02 != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            l02 = 0L;
            ss.clear(); ss.str("");
            ss << l2 << '4'; std::string str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, l02);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (l02 != std::numeric_limits<long>::min()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ull0 = 0ULL;
            ss.clear(); ss.str("");
            ss << ull1 << ' '; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ull0);
            if (it == str.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ull0 != ull1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ull0 = 0ULL;
            ss.clear(); ss.str("");
            ss << ull1 << '5'; std::string str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, ull0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (ull0 != std::numeric_limits<unsigned long long>::max()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ll01 = 0LL;
            ss.clear(); ss.str("");
            ss << ll1; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ll01);
            if (it != str.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ll01 != ll1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ll01 = 0LL;
            ss.clear(); ss.str("");
            ss << ll1 << '6'; std::string str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, ll01);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (ll01 != std::numeric_limits<long long>::max()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ll02 = 0LL;
            ss.clear(); ss.str("");
            ss << ll2 << ' '; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ll02);
            if (it == str.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ll02 != ll2) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ll02 = 0LL;
            ss.clear(); ss.str("");
            ss << ll2 << '7'; std::string str = ss.str();
            auto it = str.begin();
            try
            {
                it = obj.get(str.begin(), str.end(), ios, ll02);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (ll02 != std::numeric_limits<long long>::min()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_17()
{
    dump_info("Test numeric<char>::get 17...");
    
    IOv2::ios_base<char> ios;
    auto p1 = std::make_shared<Punct>("C");
    p1->set_grouping({1}); p1->set_thousands_sep('#');
    const IOv2::numeric<char> obj(p1, s_ctype_c);

    std::string iss1, iss2;
    long l = 0l;
    long l1 = 1l;
    long l2 = 2l;
    long l3 = 3l;
    double d = 0.0;
    double d1 = 1.0;
    double d2 = 2.0;
    
    {
        iss1 = "00#0#1";
        auto it = iss1.begin();
        try
        {
            it = obj.get(iss1.begin(), iss1.end(), ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "000##2";
        auto it = iss1.begin();
        try
        {
            it = obj.get(iss1.begin(), iss1.end(), ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (l != 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '0') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "0#0#0#2";
        auto it = obj.get(iss1.begin(), iss1.end(), ios, l);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "00#0#1";
        auto it = iss1.begin();
        try
        {
            it = obj.get(iss1.begin(), iss1.end(), ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "000##2";
        auto it = iss1.begin();
        try
        {
            it = obj.get(iss1.begin(), iss1.end(), ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '0') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != 0.0) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "0#0#0#2";
        auto it = obj.get(iss1.begin(), iss1.end(), ios, d);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "0#0";
        ios.unsetf(IOv2::ios_defs::basefield);
        auto it = iss1.begin();
        try
        {
            it = obj.get(iss1.begin(), iss1.end(), ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '0') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "00#0#3";
        auto it = obj.get(iss1.begin(), iss1.end(), ios, l);
        if (it != iss1.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l3) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss1 = "00#02";
        auto it = iss1.begin();
        try
        {
            it = obj.get(iss1.begin(), iss1.end(), ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (l != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_18()
{
    dump_info("Test numeric<char>::get 18...");
    
    IOv2::ios_base<char> ios, ios2;
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({uint8_t(0)});
    auto p2 = std::make_shared<Punct>("C"); p2->set_grouping({2, uint8_t(0)});
    auto p3 = std::make_shared<Punct>("C"); p3->set_grouping({1, 2, uint8_t(0)});

    const IOv2::numeric<char> ng1(p1, s_ctype_c);
    const IOv2::numeric<char> ng2(p2, s_ctype_c);
    const IOv2::numeric<char> ng3(p3, s_ctype_c);

    std::string iss;
    long l = 0l;
    long l1 = 12345l;
    long l2 = 12345678l;
    double d = 0.0;
    double d1 = 1234567.0;

    {
        iss = "12345";
        auto it = ng1.get(iss.begin(), iss.end(), ios, l);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "123456,78";
        auto it = ng2.get(iss.begin(), iss.end(), ios, l);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "1234,56,7.0";
        auto it = ng3.get(iss.begin(), iss.end(), ios, d);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_19()
{
    dump_info("Test numeric<char>::get 19...");
    IOv2::ios_base<char> ios;
    ios.setf(IOv2::ios_defs::boolalpha);
    
    auto p1 = std::make_shared<Punct>("C"); p1->set_truename("a"); p1->set_falsename("abb");
    auto p2 = std::make_shared<Punct>("C"); p2->set_truename("1"); p2->set_falsename("0");
    auto p3 = std::make_shared<Punct>("C"); p3->set_truename(""); p3->set_falsename("");
    auto p4 = std::make_shared<Punct>("C"); p4->set_truename("one"); p4->set_falsename("two");

    const IOv2::numeric<char> ng0(std::make_shared<IOv2::numeric_conf<char>>("C"), s_ctype_c);
    const IOv2::numeric<char> ng1(p1, s_ctype_c);
    const IOv2::numeric<char> ng2(p2, s_ctype_c);
    const IOv2::numeric<char> ng3(p3, s_ctype_c);
    const IOv2::numeric<char> ng4(p4, s_ctype_c);

    std::string iss;
    bool b0 = false;
    bool b1 = false;
    bool b2 = false;
    bool b3 = true;
    bool b4 = false;

    {
        iss = "true";
        auto it = ng0.get(iss.begin(), iss.end(), ios, b0);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b0 != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "false";
        auto it = ng0.get(iss.begin(), iss.end(), ios, b0);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b0 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "a";
        auto it = ng1.get(iss.begin(), iss.end(), ios, b1);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b1 != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "abb";
        auto it = ng1.get(iss.begin(), iss.end(), ios, b1);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b1 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "abc";
        auto it = iss.begin();
        try
        {
            it = ng1.get(iss.begin(), iss.end(), ios, b1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b1 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != 'a') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "ab";
        auto it = iss.begin();
        try
        {
            it = ng1.get(iss.begin(), iss.end(), ios, b1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b1 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "1";
        auto it = ng2.get(iss.begin(), iss.end(), ios, b2);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b2 != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "0";
        auto it = ng2.get(iss.begin(), iss.end(), ios, b2);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b2 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "2";
        auto it = iss.begin();
        try
        {
            it = ng2.get(iss.begin(), iss.end(), ios, b2);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b2 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '2') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "blah";
        auto it = iss.begin();
        try
        {
            it = ng3.get(iss.begin(), iss.end(), ios, b3);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b3 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != 'b') throw std::runtime_error("IOv2::numeric<char>::get fails");
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
        if (b3 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "one";
        auto it = ng4.get(iss.begin(), iss.end(), ios, b4);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b4 != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "two";
        auto it = ng4.get(iss.begin(), iss.end(), ios, b4);
        if (it != iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b4 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "three"; b4 = true;
        auto it = iss.begin();
        try
        {
            it = ng4.get(iss.begin(), iss.end(), ios, b4);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b4 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != 't') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        iss = "on"; b4 = true;
        auto it = iss.begin();
        try
        {
            it = ng4.get(iss.begin(), iss.end(), ios, b4);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b4 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_20()
{
    dump_info("Test numeric<char>::get 20...");
    IOv2::ios_base<char> ios;
    long double l = -1;
    
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({CHAR_MAX});
    const IOv2::numeric<char> obj(p1, s_ctype_c);
     
    std::string iss = "123,456";
    auto it = obj.get(iss.begin(), iss.end(), ios, l);
    if (it == iss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
    if (l != 123) throw std::runtime_error("IOv2::numeric<char>::get fails");
    if (*it != ',') throw std::runtime_error("IOv2::numeric<char>::get fails");
    
    dump_info("Done\n");
}

void test_numeric_char_get_21()
{
    dump_info("Test numeric<char>::get 21...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        std::string ss;

        unsigned long ul0 = 1;
        const unsigned long ul1 = std::numeric_limits<unsigned long>::max();

        {
            ss  = "-0";
            auto it = obj.get(ss.begin(), ss.end(), ios, ul0);
            if (it != ss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ul0 != 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ss = "-1";
            auto it = obj.get(ss.begin(), ss.end(), ios, ul0);
            if (it != ss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ul0 != ul1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            std::stringstream ss0;
            ss0 << '-' << ul1; ss = ss0.str();
            auto it = obj.get(ss.begin(), ss.end(), ios, ul0);
            if (it != ss.end()) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ul0 != 1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            std::stringstream ss0;
            ss0 << '-' << ul1 << '0'; ss = ss0.str();
            auto it = ss.begin();
            try
            {
                it = obj.get(ss.begin(), ss.end(), ios, ul0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (ul0 != ul1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_22()
{
    dump_info("Test numeric<char>::get 22...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        using namespace IOv2;
        IOv2::ios_base<char> ios;
        ios.fill('+');
    
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
            streambuf sb(mem_device{"1"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, b1);
            if (b1 != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"0"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, b0);
            if (b0 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        // ... and one that does
        {
            streambuf sb(mem_device{"1.294.967.294+-----"});
            auto beg = istreambuf_iterator(sb);
            ios.width(20);
            ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            if (ul != ul1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        {
            streambuf sb(mem_device{"+1,02345e+308"});
            auto beg = istreambuf_iterator(sb);
            ios.width(20);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
            auto it = obj.get(beg, std::default_sentinel, ios, d);
            if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        {
            streambuf sb(mem_device{"3,15E-308 "});
            auto beg = istreambuf_iterator(sb);
            ios.width(20);
            ios.precision(10);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
            ios.setf(IOv2::ios_defs::uppercase);
            auto it = obj.get(beg, std::default_sentinel, ios, d);
            if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        // long double
        {
            streambuf sb(mem_device{"6,630025e+4"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ld);
            if (ld != ld1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        {
            streambuf sb(mem_device{"0 "});
            auto beg = istreambuf_iterator(sb);
            ios.precision(0);
            ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
            auto it = obj.get(beg, std::default_sentinel, ios, ld);
            if (ld != 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        // void*
        {
            streambuf sb(mem_device{"0xbffff74c,"});
            auto beg = istreambuf_iterator(sb);

            auto it = obj.get(beg, std::default_sentinel, ios, v);
            if (v == 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ',') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        {
            long long ll1 = 9223372036854775807LL;
            long long ll;
            
            streambuf sb(mem_device{"9.223.372.036.854.775.807"});
            auto beg = istreambuf_iterator(sb);

            auto it = obj.get(beg, std::default_sentinel, ios, ll);
            if (ll != ll1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_23()
{
    dump_info("Test numeric<char>::get 23...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        using namespace IOv2;
        IOv2::ios_base<char> ios;
        ios.fill('+');
    
        std::string iss;
    
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
            streambuf sb(mem_device{"true "});
            auto beg = istreambuf_iterator(sb);
            ios.setf(IOv2::ios_defs::boolalpha);
            auto it = obj.get(beg, std::default_sentinel, ios, b0);
            if (b0 != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        {
            streambuf sb(mem_device{"false "});
            auto beg = istreambuf_iterator(sb);
            ios.setf(IOv2::ios_defs::boolalpha);
            auto it = obj.get(beg, std::default_sentinel, ios, b1);
            if (b1 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        // unsigned long
        {
            streambuf sb(mem_device{"1294967294"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            if (ul != ul1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"0+----------------------"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            if (ul != ul2) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        // double
        {
            streambuf sb(mem_device{"1.02345e+308+-------"});
            auto beg = istreambuf_iterator(sb);
            ios.width(20);
            ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
            auto it = obj.get(beg, std::default_sentinel, ios, d);
            if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"+3.15e-308"});
            auto beg = istreambuf_iterator(sb);
            ios.width(20);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            auto it = obj.get(beg, std::default_sentinel, ios, d);
            if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_24()
{
    dump_info("Test numeric<char>::get 24...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        using namespace IOv2;
        IOv2::ios_base<char> ios;

        std::string iss;
    
        long l1 = 2147483647;
        long l2 = -2147483647;
        long l;

        // HK
        // long, in a locale that expects grouping
        {
            streambuf sb(mem_device{"2,147,483,647 "});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, l);
            if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"-2,147,483,647+-----"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, l);
            if (l != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("en_HK.UTF-8"),
                            s_ctype_hk_utf8);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_25()
{
    dump_info("Test numeric<char>::get 25...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        using namespace IOv2;
        IOv2::ios_base<char> ios;

        {
            // 01 get(long)
            streambuf sb(mem_device{"20000106 Elizabeth Durack"});
            auto beg = istreambuf_iterator(sb);
            long i = 0;
            auto it = obj.get(beg, std::default_sentinel, ios, i);
            if (i != 20000106) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (std::string(it, decltype(it)()) != " Elizabeth Durack") throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            // 02 get(long double)
            streambuf sb(mem_device{"20000106 Elizabeth Durack"});
            auto beg = istreambuf_iterator(sb);
            long double ld = 0.0;
            auto it = obj.get(beg, std::default_sentinel, ios, ld);
            if (ld != 20000106) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (std::string(it, decltype(it)()) != " Elizabeth Durack") throw std::runtime_error("IOv2::numeric<char>::get fails");
        }

        {
            // 03 get(bool)
            streambuf sb(mem_device{"0 true 0xbffff74c Durack"});
            auto beg = istreambuf_iterator(sb);

            bool b = 1;
            auto it = obj.get(beg, std::default_sentinel, ios, b);
            if (b != 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");

            ios.setf(IOv2::ios_defs::boolalpha);
            it = obj.get(++it, std::default_sentinel, ios, b);
            if (b != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            
            // 04 get(void*)
            void* v;
            ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
            it = obj.get(++it, std::default_sentinel, ios, v);
            if (v != (void*)0xbffff74c) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (std::string(it, decltype(it)()) != " Durack") throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_26()
{
    dump_info("Test numeric<char>::get 26...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        using namespace IOv2;
        IOv2::ios_base<char> ios;
        
        unsigned long ul;
        std::string iss;
        
        {
            ios.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
            streambuf sb(mem_device{"0xbf.fff.74c "});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            if (ul != 0xbffff74c) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"0Xf.fff "});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            if (ul != 0xffff) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"ffe "});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            if (ul != 0xffe) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ios.setf(IOv2::ios_defs::oct, IOv2::ios_defs::basefield);
            streambuf sb(mem_device{"07.654.321 "});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            if (ul != 07654321) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"07.777 "});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            if (ul != 07777) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"776 "});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            if (ul != 0776) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_27()
{
    dump_info("Test numeric<char>::get 27...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        using namespace IOv2;
        IOv2::ios_base<char> ios;

        double d = 0.0;
        std::string iss;

        streambuf sb(mem_device{"1234,5 "});
        auto beg = istreambuf_iterator(sb);
        auto it = obj.get(beg, std::default_sentinel, ios, d);
        if (d != 1234.5) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
        if (*it != ' ') throw std::runtime_error("IOv2::numeric<wchar_t>::get fails");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);
    dump_info("Done\n");
}

// ori: get_7
void test_numeric_char_get_28()
{
    dump_info("Test numeric<char>::get 28...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        using namespace IOv2;
        IOv2::ios_base<char> ios;
        
        double d = 0.0;
        std::string iss;
        
        {
            streambuf sb(mem_device{"+e3"});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, d);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != 'e') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{".e+1"});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, d);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != 'e') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_29()
{
    dump_info("Test numeric<char>::get 29...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        using namespace IOv2;
        IOv2::ios_base<char> ios;

        bool b;
        
        {
            ios.setf(IOv2::ios_defs::boolalpha);
            streambuf sb(mem_device{"faLse"});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, b);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != 'L') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"falsr"});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, b);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != 'r') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"trus"});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, b);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != 's') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_30()
{
    dump_info("Test numeric<char>::get 30...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        using namespace IOv2;
        IOv2::ios_base<char> ios;

        double d = 0.0;
        double d1 = 1e1;
        double d2 = 3e1;
        {
            streambuf sb(mem_device{"1e1,"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, d);
            if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != ',') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"3e1."});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, d);
            if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (*it != '.') throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_31()
{
    dump_info("Test numeric<char>::get 31...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        using namespace IOv2;
        IOv2::ios_base<char> ios;

        float f = 1.0f;
        double d = 1.0;
        long double ld = 1.0l;

        {
            streambuf sb(mem_device{"1e."});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, f);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != '.') throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (f != 0.0f) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"3e+"});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, d);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (d != 0.0) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"6e "});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, ld);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (*it != ' ') throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ld != 0.0l) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);
    dump_info("Done\n");
}

void test_numeric_char_get_32()
{
    dump_info("Test numeric<char>::get 32...");
    
    IOv2::ios_base<char> ios;
    
    auto p1 = std::make_shared<Punct>("C");
    p1->set_grouping(std::vector<uint8_t>{1}); p1->set_thousands_sep('2'); p1->set_decimal_point('4');
    const IOv2::numeric<char> ng1(p1, s_ctype_c);
    
    auto p2 = std::make_shared<Punct>("C");
    p2->set_grouping(std::vector<uint8_t>{1}); p2->set_thousands_sep('2'); p2->set_decimal_point('2');
    const IOv2::numeric<char> ng2(p2, s_ctype_c);

    double d = 0.0;
    double d1 = 13.0;
    double d2 = 1.0;
    double d3 = 30.0;
    long l = 0l;
    long l1 = 13l;
    long l2 = 10l;

    using namespace IOv2;
    {
        streambuf sb(mem_device{"1234"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, d);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }

    {
        streambuf sb(mem_device{"142"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, d);
        if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '2') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }

    {
        streambuf sb(mem_device{"3e14"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, d);
        if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '4') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d3) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }

    {
        streambuf sb(mem_device{"1234"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, l);
        if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '4') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }

    {
        streambuf sb(mem_device{"123"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, d);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }

    {
        streambuf sb(mem_device{"120"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, l);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_33()
{
    dump_info("Test numeric<char>::get 33...");
    
    IOv2::ios_base<char> ios, ios2;

    auto p1 = std::make_shared<Punct>("C");
    p1->set_grouping(std::vector<uint8_t>{1}); p1->set_thousands_sep('+'); p1->set_decimal_point('x');
    const IOv2::numeric<char> ng1(p1, s_ctype_c);
    
    auto p2 = std::make_shared<Punct>("C");
    p2->set_grouping(std::vector<uint8_t>{1}); p2->set_thousands_sep('X'); p2->set_decimal_point('-');
    const IOv2::numeric<char> ng2(p2, s_ctype_c);

    std::string iss1, iss2;
    long l = 1l;
    long l1 = 0l;
    long l2 = 10l;
    long l3 = 1l;
    long l4 = 63l;
    double d = 0.0;
    double d1 = .4;
    double d2 = 0.0;
    double d3 = .1;

    using namespace IOv2;
    {
        streambuf sb(mem_device{"+3"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng1.get(beg, std::default_sentinel, ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"0x1"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, l);
        if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != 'x') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"0Xa"});
        auto beg = istreambuf_iterator(sb);
        ios.unsetf(IOv2::ios_defs::basefield);
        auto it = ng1.get(beg, std::default_sentinel, ios, l);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"0xa"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, l);
        if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != 'x') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"+5"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng1.get(beg, std::default_sentinel, ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"x4"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, d);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"0001-"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios2, l);
        if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '-') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l3) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"-2"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng2.get(beg, std::default_sentinel, ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '-') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        ios2.unsetf(IOv2::ios_defs::basefield);
        streambuf sb(mem_device{"0X1"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng2.get(beg, std::default_sentinel, ios2, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != 'X') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"000778"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios2, l);
        if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '8') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l4) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"00X"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng2.get(beg, std::default_sentinel, ios2, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"-1"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios2, d);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d3) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_34()
{
    dump_info("Test numeric<char>::get 34...");
    
    IOv2::ios_base<char> ios;
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({3, 2, 1});
    const IOv2::numeric<char> ng1(p1, s_ctype_c);
    
    auto p2 = std::make_shared<Punct>("C"); p2->set_grouping({1, 3});
    const IOv2::numeric<char> ng2(p2, s_ctype_c);

    std::string iss1, iss2;
    long l = 0l;
    long l1 = 12345678l;
    double d = 0.0;
    double d1 = 1234567.0;
    double d2 = 123456.0;

    using namespace IOv2;
    {
        streambuf sb(mem_device{"1,2,3,45,678"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, l);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"123,456,7.0"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, d);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"12,345,6.0"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, d);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_35()
{
    dump_info("Test numeric<char>::get 35...");
    
    IOv2::ios_base<char> ios;
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({1});
    const IOv2::numeric<char> obj(p1, s_ctype_c);

    std::string iss;
    double d = 0.0;
    double d1 = 1000.0;
    
    using namespace IOv2;
    {
        streambuf sb(mem_device{"1,0e2"});
        auto beg = istreambuf_iterator(sb);
        auto it = obj.get(beg, std::default_sentinel, ios, d);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_36()
{
    dump_info("Test numeric<char>::get 36...");
    
    IOv2::ios_base<char> ios;
    auto p1 = std::make_shared<Punct>("C");
    p1->set_grouping({1}); p1->set_thousands_sep('+');
    const IOv2::numeric<char> ng1(p1, s_ctype_c);

    auto p2 = std::make_shared<Punct>("C");
    p2->set_decimal_point('-');
    const IOv2::numeric<char> ng2(p2, s_ctype_c);

    double d = 1.0;
    using namespace IOv2;
    {
        streambuf sb(mem_device{"1e+2"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng1.get(beg, std::default_sentinel, ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (d != 0.0) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '+') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"3e-1"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng2.get(beg, std::default_sentinel, ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (d != 0.0) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '-') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_37()
{ 
    dump_info("Test numeric<char>::get 37...");
    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;
        std::stringstream ss;

        unsigned short us0, us1 = std::numeric_limits<unsigned short>::max();
        unsigned int ui0, ui1 = std::numeric_limits<unsigned int>::max();
        unsigned long ul0, ul1 = std::numeric_limits<unsigned long>::max();
        long l01, l1 = std::numeric_limits<long>::max();
        long l02, l2 = std::numeric_limits<long>::min();
        unsigned long long ull0, ull1 = std::numeric_limits<unsigned long long>::max();
        long long ll01, ll1 = std::numeric_limits<long long>::max();
        long long ll02, ll2 = std::numeric_limits<long long>::min();

        using namespace IOv2;
        {
            us0 = 0;
            ss << us1;
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, us0);
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (us0 != us1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            us0 = 0;
            ss.clear(); ss.str("");
            ss << us1 << '0';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, us0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (us0 != std::numeric_limits<unsigned short>::max()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ui0 = 0U;
            ss.clear(); ss.str("");
            ss << ui1 << ' '; 
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ui0);
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ui0 != ui1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ui0 = 0U;
            ss.clear(); ss.str("");
            ss << ui1 << '1';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, ui0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ui0 != std::numeric_limits<unsigned int>::max()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ul0 = 0UL;
            ss.clear(); ss.str("");
            ss << ul1;
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul0);
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ul0 != ul1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ul0 = 0UL;
            ss.clear(); ss.str("");
            ss << ul1 << '2';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, ul0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ul0 != std::numeric_limits<unsigned long>::max()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            l01 = 0L;
            ss.clear(); ss.str("");
            ss << l1 << ' ';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, l01);
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (l01 != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            l01 = 0L;
            ss.clear(); ss.str("");
            ss << l1 << '3';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, l01);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (l01 != std::numeric_limits<long>::max()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            l02 = 0L;
            ss.clear(); ss.str("");
            ss << l2;
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, l02);
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (l02 != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            l02 = 0L;
            ss.clear(); ss.str("");
            ss << l2 << '4';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, l02);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (l02 != std::numeric_limits<long>::min()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ull0 = 0ULL;
            ss.clear(); ss.str("");
            ss << ull1 << ' ';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ull0);
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ull0 != ull1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ull0 = 0ULL;
            ss.clear(); ss.str("");
            ss << ull1 << '5';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, ull0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ull0 != std::numeric_limits<unsigned long long>::max()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ll01 = 0LL;
            ss.clear(); ss.str("");
            ss << ll1;
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ll01);
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ll01 != ll1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ll01 = 0LL;
            ss.clear(); ss.str("");
            ss << ll1 << '6';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, ll01);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ll01 != std::numeric_limits<long long>::max()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ll02 = 0LL;
            ss.clear(); ss.str("");
            ss << ll2 << ' ';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ll02);
            if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ll02 != ll2) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            ll02 = 0LL;
            ss.clear(); ss.str("");
            ss << ll2 << '7';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, ll02);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ll02 != std::numeric_limits<long long>::min()) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_get_38()
{
    dump_info("Test numeric<char>::get 38...");
    
    IOv2::ios_base<char> ios;
    auto p1 = std::make_shared<Punct>("C");
    p1->set_grouping({1}); p1->set_thousands_sep('#');
    const IOv2::numeric<char> obj(p1, s_ctype_c);

    long l = 0l;
    long l1 = 1l;
    long l2 = 2l;
    long l3 = 3l;
    double d = 0.0;
    double d1 = 1.0;
    double d2 = 2.0;

    using namespace IOv2;
    {
        streambuf sb(mem_device{"00#0#1"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = obj.get(beg, std::default_sentinel, ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"000##2"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = obj.get(beg, std::default_sentinel, ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (l != 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '#') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"0#0#0#2"});
        auto beg = istreambuf_iterator(sb);
        auto it = obj.get(beg, std::default_sentinel, ios, l);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"00#0#1"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = obj.get(beg, std::default_sentinel, ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"000##2"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = obj.get(beg, std::default_sentinel, ios, d);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '#') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != 0.0) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"0#0#0#2"});
        auto beg = istreambuf_iterator(sb);
        auto it = obj.get(beg, std::default_sentinel, ios, d);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"0#0"});
        auto beg = istreambuf_iterator(sb);
        ios.unsetf(IOv2::ios_defs::basefield);
        auto it = beg;
        try
        {
            it = obj.get(beg, std::default_sentinel, ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (*it != '#') throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"00#0#3"});
        auto beg = istreambuf_iterator(sb);
        auto it = obj.get(beg, std::default_sentinel, ios, l);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l3) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"00#02"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = obj.get(beg, std::default_sentinel, ios, l);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_39()
{
    dump_info("Test numeric<char>::get 39...");
    
    IOv2::ios_base<char> ios, ios2;
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({uint8_t(0)});
    auto p2 = std::make_shared<Punct>("C"); p2->set_grouping({2, uint8_t(0)});
    auto p3 = std::make_shared<Punct>("C"); p3->set_grouping({1, 2, uint8_t(0)});

    const IOv2::numeric<char> ng1(p1, s_ctype_c);
    const IOv2::numeric<char> ng2(p2, s_ctype_c);
    const IOv2::numeric<char> ng3(p3, s_ctype_c);

    long l = 0l;
    long l1 = 12345l;
    long l2 = 12345678l;
    double d = 0.0;
    double d1 = 1234567.0;

    using namespace IOv2;
    {
        streambuf sb(mem_device{"12345"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, l);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"123456,78"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, l);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (l != l2) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"1234,56,7.0"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng3.get(beg, std::default_sentinel, ios, d);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (d != d1) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_40()
{
    dump_info("Test numeric<char>::get 40...");
    IOv2::ios_base<char> ios;
    ios.setf(IOv2::ios_defs::boolalpha);
    
    auto p1 = std::make_shared<Punct>("C"); p1->set_truename("a"); p1->set_falsename("abb");
    auto p2 = std::make_shared<Punct>("C"); p2->set_truename("1"); p2->set_falsename("0");
    auto p3 = std::make_shared<Punct>("C"); p3->set_truename(""); p3->set_falsename("");
    auto p4 = std::make_shared<Punct>("C"); p4->set_truename("one"); p4->set_falsename("two");

    const IOv2::numeric<char> ng0(std::make_shared<IOv2::numeric_conf<char>>("C"), s_ctype_c);
    const IOv2::numeric<char> ng1(p1, s_ctype_c);
    const IOv2::numeric<char> ng2(p2, s_ctype_c);
    const IOv2::numeric<char> ng3(p3, s_ctype_c);
    const IOv2::numeric<char> ng4(p4, s_ctype_c);

    bool b0 = false;
    bool b1 = false;
    bool b2 = false;
    bool b3 = true;
    bool b4 = false;

    using namespace IOv2;
    {
        streambuf sb(mem_device{"true"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng0.get(beg, std::default_sentinel, ios, b0);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b0 != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"false"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng0.get(beg, std::default_sentinel, ios, b0);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b0 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"a"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, b1);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b1 != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"abb"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, b1);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b1 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"abc"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng1.get(beg, std::default_sentinel, ios, b1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b1 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != 'c') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"ab"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng1.get(beg, std::default_sentinel, ios, b1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b1 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"1"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, b2);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b2 != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"0"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, b2);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b2 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"2"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng2.get(beg, std::default_sentinel, ios, b2);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b2 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != '2') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"blah"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng3.get(beg, std::default_sentinel, ios, b3);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b3 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != 'b') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        b3 = true;
        streambuf sb(mem_device{""});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng3.get(beg, std::default_sentinel, ios, b3);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b3 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"one"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng4.get(beg, std::default_sentinel, ios, b4);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b4 != true) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        streambuf sb(mem_device{"two"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng4.get(beg, std::default_sentinel, ios, b4);
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b4 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        b4 = true;
        streambuf sb(mem_device{"three"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng4.get(beg, std::default_sentinel, ios, b4);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (b4 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (*it != 'h') throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    {
        b4 = true;
        streambuf sb(mem_device{"on"});
        auto beg = istreambuf_iterator(sb);
        auto it = beg;
        try
        {
            it = ng4.get(beg, std::default_sentinel, ios, b4);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
        if (b4 != false) throw std::runtime_error("IOv2::numeric<char>::get fails");
    }
    dump_info("Done\n");
}

void test_numeric_char_get_41()
{
    dump_info("Test numeric<char>::get 41...");
    IOv2::ios_base<char> ios;
    long double l = -1;
    
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({CHAR_MAX});
    const IOv2::numeric<char> obj(p1, s_ctype_c);

    using namespace IOv2;
    streambuf sb(mem_device{"123,456"});
    auto beg = istreambuf_iterator(sb);
    auto it = obj.get(beg, std::default_sentinel, ios, l);
    if (it == std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
    if (l != 123) throw std::runtime_error("IOv2::numeric<char>::get fails");
    if (*it != ',') throw std::runtime_error("IOv2::numeric<char>::get fails");
    
    dump_info("Done\n");
}

void test_numeric_char_get_42()
{
    dump_info("Test numeric<char>::get 42...");

    auto helper = [](const IOv2::numeric<char>& obj)
    {
        IOv2::ios_base<char> ios;

        unsigned long ul0 = 1;
        const unsigned long ul1 = std::numeric_limits<unsigned long>::max();

        using namespace IOv2;
        {
            streambuf sb(mem_device{"-0"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul0);
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ul0 != 0) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            streambuf sb(mem_device{"-1"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul0);
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ul0 != ul1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            std::stringstream ss0;
            ss0 << '-' << ul1;
            streambuf sb(mem_device{ss0.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul0);
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ul0 != 1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
        {
            std::stringstream ss0;
            ss0 << '-' << ul1 << '0';
            streambuf sb(mem_device{ss0.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = beg;
            try
            {
                it = obj.get(beg, std::default_sentinel, ios, ul0);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (it != std::default_sentinel) throw std::runtime_error("IOv2::numeric<char>::get fails");
            if (ul0 != ul1) throw std::runtime_error("IOv2::numeric<char>::get fails");
        }
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"),
                            s_ctype_c);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_vulnerability_fix_char()
{
    dump_info("Test numeric<char> vulnerability fix...");

    IOv2::numeric<char> nump(std::make_shared<IOv2::numeric_conf<char>>("C"), s_ctype_c);
    IOv2::ios_base<char> ios;

    // 1. Test precision support (20 digits)
    {
        ios.precision(20);
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        std::string oss;
        nump.put(std::back_inserter(oss), ios, 1.12345678901234567890);
        
        size_t dot_pos = oss.find('.');
        VERIFY(dot_pos != std::string::npos);
        // Now it should support the requested 20 digits instead of capping at 16
        VERIFY(oss.length() - dot_pos - 1 == 20);
    }

    // 2. Test hexfloat fix
    {
        ios.precision(6);
        ios.setf(IOv2::ios_defs::fixed | IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
        std::string oss;
        // This should not crash and should produce a valid hexfloat
        nump.put(std::back_inserter(oss), ios, 1.2345);
        VERIFY(!oss.empty());
        VERIFY(oss.find("0x") == 0);
    }

    // 3. Test dynamic resizing (precision 500)
    {
        const int high_prec = 500;
        ios.precision(high_prec);
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        std::string oss;
        // This will trigger the two-pass logic as it exceeds the initial 128/2048 buffer
        nump.put(std::back_inserter(oss), ios, 1.0);
        size_t dot_pos = oss.find('.');
        VERIFY(dot_pos != std::string::npos);
        VERIFY(oss.length() - dot_pos - 1 == high_prec);
    }

    dump_info("Done\n");
}
