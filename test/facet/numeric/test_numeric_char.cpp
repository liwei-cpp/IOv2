#include <deque>
#include <list>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <facet/numeric.h>
#include <io/streambuf_iterator.h>
#include <support/verify.h>

#include <support/dump_info.h>

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

    VERIFY(nump_c.decimal_point() != nump_de.decimal_point());
    VERIFY(nump_c.thousands_sep() != nump_de.thousands_sep());
    VERIFY(nump_c.grouping() != nump_de.grouping());

    VERIFY(!(nump_c.truename().empty()));
    VERIFY(!(nump_de.truename().empty()));
    VERIFY(nump_c.truename() != nump_de.truename());

    VERIFY(!(nump_c.falsename().empty()));
    VERIFY(!(nump_de.falsename().empty()));
    VERIFY(nump_c.falsename() != nump_de.falsename());
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
        VERIFY(oss == "1");
        
        oss.clear();
        obj.put(std::back_inserter(oss), ios, b0);
        VERIFY(oss == "0");
        
        // ... and one that does
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, ul1);
        VERIFY(oss == "1.294.967.294+++++++");
        
        // double
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, d1);
        VERIFY(oss == "1,79769e+308++++++++");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, d2);
        VERIFY(oss == "++++++++2,22507e-308");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
        obj.put(std::back_inserter(oss), ios, d2);
        VERIFY(oss == "+++++++2,225074e-308");
    
        oss.clear();
        ios.width(20);
        ios.precision(10);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
        ios.setf(IOv2::ios_defs::uppercase);
        obj.put(std::back_inserter(oss), ios, d2);
        VERIFY(oss == "+++2,2250738585E-308");
        
        // long double
        oss.clear();
        obj.put(std::back_inserter(oss), ios, ld1);
        VERIFY(oss == "1,7976931349E+308");
        
        oss.clear();
        ios.precision(0);
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        obj.put(std::back_inserter(oss), ios, ld2);
        VERIFY(oss == "0");
        
        // const void*
        oss.clear();
        obj.put(std::back_inserter(oss), ios, cv);
        VERIFY(oss.find(obj.decimal_point()) == std::string::npos);
        VERIFY(oss.find('x') == 1);
        
        long long ll1 = 9223372036854775807LL;
        
        oss.clear();
        obj.put(std::back_inserter(oss), ios, ll1);
        VERIFY(oss == "9.223.372.036.854.775.807");
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
        VERIFY(oss == "+++++++++++++++++++0");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        ios.setf(IOv2::ios_defs::boolalpha);
        obj.put(std::back_inserter(oss), ios, b1);
        VERIFY(oss == "true++++++++++++++++");
        
        // unsigned long, in a locale that does not group
        oss.clear();
        obj.put(std::back_inserter(oss), ios, ul1);
        VERIFY(oss == "1294967294");
        
        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, ul2);
        VERIFY(oss == "0+++++++++++++++++++");
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
        VERIFY(oss == "2,147,483,647");

        oss.clear();
        ios.width(20);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, l2);
        VERIFY(oss == "-2,147,483,647++++++");
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
        VERIFY(res == "1798xxxxxxxxxxxxxx");
        VERIFY(sanity1 == "1798");
        
        // 02 put(long double)
        const long double ld = 1798.0;
        res = x;
        auto ret2 = obj.put(res.begin(), ios, ld);
        std::string sanity2(res.begin(), ret2);
        VERIFY(res == "1798xxxxxxxxxxxxxx");
        VERIFY(sanity2 == "1798");
        
        // 03 put(bool)
        bool b = 1;
        res = x;
        auto ret3 = obj.put(res.begin(), ios, b);
        std::string sanity3(res.begin(), ret3);
        VERIFY(res == "1xxxxxxxxxxxxxxxxx");
        VERIFY(sanity3 == "1");
        
        b = 0;
        res = x;
        ios.setf(IOv2::ios_defs::boolalpha);
        auto ret4 = obj.put(res.begin(), ios, b);
        std::string sanity4(res.begin(), ret4);
        VERIFY(res == "falsexxxxxxxxxxxxx");
        VERIFY(sanity4 == "false");
        
        // 04 put(void*)
        const void* cv = &ld;
        res = x;
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        auto ret5 = obj.put(res.begin(), ios, cv);
        std::string sanity5(res.begin(), ret5);
        VERIFY(sanity5.size() >= 2);
        VERIFY(sanity5[1] == 'x');
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
        VERIFY(oss == "0");
    
        oss.clear();
        ios.setf(IOv2::ios_defs::showbase);
        ios.setf(IOv2::ios_defs::oct, IOv2::ios_defs::basefield);
        obj.put(std::back_inserter(oss), ios, l);
        VERIFY(oss == "0");
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
    
        ios.precision(6);
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        obj.put(std::back_inserter(oss), ios, 30.5);
        VERIFY(oss == "30.500000");
        
        oss.clear();
        ios.precision(0);
        ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
        obj.put(std::back_inserter(oss), ios, 1.0);
        VERIFY(oss == "1e+00");
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
        VERIFY(oss == "10");
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
        VERIFY(oss == "1");
        
        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, b);
        VERIFY(oss == "+1");
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
            VERIFY(oss != "1");
        }
    
        {
            long long ll = -1LL;
            oss.clear();
            obj.put(std::back_inserter(oss), ios, ll);
            VERIFY(oss != "1");
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
        VERIFY(oss == "1,2,3,45,678");
    }
    {
        ios.precision(1);
        ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
        oss.clear();
        ng2.put(std::back_inserter(oss), ios, d1);
        VERIFY(oss == "123,456,7.0");
    }
    {
        oss.clear();
        ng2.put(std::back_inserter(oss), ios, d2);
        VERIFY(oss == "12,345,6.0");
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
            VERIFY(result1 == "+0");
        }
        {
            obj.put(std::back_inserter(result2), ios, li2);
            VERIFY(result2 == "+5");
        }
        {
            obj.put(std::back_inserter(result3), ios, d1);
            VERIFY(result3 == "+0");
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
    
        const std::uint8_t precision = 200;

        ios.precision(precision);
        ios.setf(IOv2::ios_defs::fixed);
        obj.put(std::back_inserter(oss), ios, 1.0);
        VERIFY(!(oss.size() != static_cast<size_t>(precision) + 2));
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
        VERIFY(oss == "42");
        
        unsigned long long ull1 = 31ULL;
        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, ull1);
        VERIFY(oss == "31");
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
        VERIFY(oss == "2e+20");
        
        oss.clear();
        obj.put(std::back_inserter(oss), ios, d1);
        VERIFY(oss == "-2e+20");
        
        oss.clear();
        ios.setf(IOv2::ios_defs::uppercase);
        obj.put(std::back_inserter(oss), ios, d0);
        VERIFY(oss == "2E+20");
        
        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, d0);
        VERIFY(oss == "+2E+20");
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
        VERIFY(oss == "-300.000");
        
        oss.clear();
        obj.put(std::back_inserter(oss), ios, d0);
        VERIFY(oss == "-300.000");
    
        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, l1);
        VERIFY(oss == "+300");
        
        oss.clear();
        ios.setf(IOv2::ios_defs::showpos);
        obj.put(std::back_inserter(oss), ios, d1);
        VERIFY(oss == "+300");
    };

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("de_DE.ISO-8859-1"),
                            s_ctype_de_8859);
    helper(obj);

    dump_info("Done\n");
}

void test_numeric_char_put_16()
{
    dump_info("Test numeric<char>::put 16...");
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping(std::vector<uint8_t>{});
    auto p2 = std::make_shared<Punct>("C"); p2->set_grouping(std::vector<uint8_t>{(uint8_t)2, (uint8_t)0});
    auto p3 = std::make_shared<Punct>("C"); p3->set_grouping(std::vector<uint8_t>{(uint8_t)1, (uint8_t)2, (uint8_t)0});
    
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
    VERIFY(oss == "12345");

    oss.clear();
    ng2.put(std::back_inserter(oss), ios, l2);
    VERIFY(oss == "123456,78");

    ios.precision(1);
    ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
    oss.clear();
    ng3.put(std::back_inserter(oss), ios, d1);
    VERIFY(oss == "1234,56,7.0");
  
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
    VERIFY(oss == "**-no-");

    oss.clear();
    ios.width(6);
    ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
    ios.setf(IOv2::ios_defs::boolalpha);
    ng1.put(std::back_inserter(oss), ios, false);
    VERIFY(oss == "**-no-");

    oss.clear();
    ios.width(6);
    ios.setf(IOv2::ios_defs::internal, IOv2::ios_defs::adjustfield);
    ios.setf(IOv2::ios_defs::boolalpha);
    ng1.put(std::back_inserter(oss), ios, false);
    VERIFY(oss == "**-no-");
    
    oss.clear();
    ios.width(6);
    ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
    ios.setf(IOv2::ios_defs::boolalpha);
    ng1.put(std::back_inserter(oss), ios, false);
    VERIFY(oss == "-no-**");
  
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
        VERIFY(oss == "**0x1");
    
        oss.clear();
        ios.width(5);
        ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, p);
        VERIFY(oss == "**0x1");
        
        oss.clear();
        ios.width(5);
        ios.setf(IOv2::ios_defs::internal, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, p);
        VERIFY(oss == "0x**1");
        
        oss.clear();
        ios.width(5);
        ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
        obj.put(std::back_inserter(oss), ios, p);
        VERIFY(oss == "0x1**");
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
            VERIFY(b1 == true);
            VERIFY(it == iss.end());
        }
        {
            iss = "0";
            auto it = obj.get(iss.begin(), iss.end(), ios, b0);
            VERIFY(b0 == false);
            VERIFY(it == iss.end());
        }
        
        // ... and one that does
        {
            iss = "1.294.967.294+-----";
            ios.width(20);
            ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            VERIFY(ul == ul1);
            VERIFY(it != iss.end());
            VERIFY(*it == '+');
        }

        {
            iss = "+1,02345e+308";
            ios.width(20);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            VERIFY(d == d1);
            VERIFY(it == iss.end());
        }
        
        {
            iss = "3,15E-308 ";
            ios.width(20);
            ios.precision(10);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
            ios.setf(IOv2::ios_defs::uppercase);
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            VERIFY(d == d2);
            VERIFY(it != iss.end());
            VERIFY(*it == ' ');
        }
        
        // long double
        {
            iss = "6,630025e+4";
            auto it = obj.get(iss.begin(), iss.end(), ios, ld);
            VERIFY(ld == ld1);
            VERIFY(it == iss.end());
        }
        
        {
            iss = "0 ";
            ios.precision(0);
            ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, ld);
            VERIFY(ld == 0);
            VERIFY(it != iss.end());
            VERIFY(*it == ' ');
        }
        
        // void*
        {
            iss = "0xbffff74c,";
            auto it = obj.get(iss.begin(), iss.end(), ios, v);
            VERIFY(v != 0);
            VERIFY(it != iss.end());
            VERIFY(*it == ',');
        }

        {
            long long ll1 = 9223372036854775807LL;
            long long ll;
            
            iss = "9.223.372.036.854.775.807";
            auto it = obj.get(iss.begin(), iss.end(), ios, ll);
            VERIFY(ll == ll1);
            VERIFY(it == iss.end());
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
            VERIFY(b0 == true);
            VERIFY(it != iss.end());
            VERIFY(*it == ' ');
        }

        {
            iss = "false ";
            ios.setf(IOv2::ios_defs::boolalpha);
            auto it = obj.get(iss.begin(), iss.end(), ios, b1);
            VERIFY(b1 == false);
            VERIFY(it != iss.end());
            VERIFY(*it == ' ');
        }

        // unsigned long
        {
            iss = "1294967294";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            VERIFY(ul == ul1);
            VERIFY(it == iss.end());
        }
        {
            iss = "0+----------------------";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            VERIFY(ul == ul2);
            VERIFY(it != iss.end());
            VERIFY(*it == '+');
        }

        // double
        {
            iss = "1.02345e+308+-------";
            ios.width(20);
            ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            VERIFY(d == d1);
            VERIFY(it != iss.end());
            VERIFY(*it == '+');
        }
        {
            iss = "+3.15e-308";
            ios.width(20);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            VERIFY(d == d2);
            VERIFY(it == iss.end());
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
            VERIFY(l == l1);
            VERIFY(it != iss.end());
            VERIFY(*it == ' ');
        }
        {
            iss = "-2,147,483,647+-----";
            auto it = obj.get(iss.begin(), iss.end(), ios, l);
            VERIFY(l == l2);
            VERIFY(it != iss.end());
            VERIFY(*it == '+');
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
            VERIFY(i == 20000106);
            VERIFY(it != str.end());
            VERIFY(std::string(it, str.end()) == " Elizabeth Durack");
        }
        {
            // 02 get(long double)
            long double ld = 0.0;
            auto it = obj.get(str.begin(), str.end(), ios, ld);
            VERIFY(ld == 20000106);
            VERIFY(it != str.end());
            VERIFY(std::string(it, str.end()) == " Elizabeth Durack");
        }

        {
            // 03 get(bool)
            bool b = 1;
            auto it = obj.get(str2.begin(), str2.end(), ios, b);
            VERIFY(b == 0);
            VERIFY(it != str2.end());
            VERIFY(std::string(it, str2.end()) == " true 0xbffff74c Durack");
    
            ios.setf(IOv2::ios_defs::boolalpha);
            it = obj.get(++it, str2.end(), ios, b);
            VERIFY(b == true);
            VERIFY(it != str2.end());
            VERIFY(std::string(it, str2.end()) == " 0xbffff74c Durack");
            
            // 04 get(void*)
            void* v;
            ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
            it = obj.get(++it, str2.end(), ios, v);
            VERIFY(v == (void*)0xbffff74c);
            VERIFY(it != str2.end());
            VERIFY(std::string(it, str2.end()) == " Durack");
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
            VERIFY(ul == 0xbffff74c);
            VERIFY(it != iss.end());
            VERIFY(*it == ' ');
        }
        {
            iss = "0Xf.fff ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            VERIFY(ul == 0xffff);
            VERIFY(it != iss.end());
            VERIFY(*it == ' ');
        }
        {
            iss = "ffe ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            VERIFY(ul == 0xffe);
            VERIFY(it != iss.end());
            VERIFY(*it == ' ');
        }
        {
            ios.setf(IOv2::ios_defs::oct, IOv2::ios_defs::basefield);
            iss = "07.654.321 ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            VERIFY(ul == 07654321);
            VERIFY(it != iss.end());
            VERIFY(*it == ' ');
        }
        {
            iss = "07.777 ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            VERIFY(ul == 07777);
            VERIFY(it != iss.end());
            VERIFY(*it == ' ');
        }
        {
            iss = "776 ";
            auto it = obj.get(iss.begin(), iss.end(), ios, ul);
            VERIFY(ul == 0776);
            VERIFY(it != iss.end());
            VERIFY(*it == ' ');
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
        VERIFY(d == 1234.5);
        VERIFY(it != iss.end());
        VERIFY(*it == ' ');
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
            VERIFY(*it == '+');
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
            VERIFY(*it == '.');
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
            VERIFY(*it == 'f');
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
            VERIFY(*it == 'f');
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
            VERIFY(*it == 't');
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
            VERIFY(d == d1);
            VERIFY(it != iss.end());
            VERIFY(*it == ',');
        }
        {
            iss = "3e1.";
            auto it = obj.get(iss.begin(), iss.end(), ios, d);
            VERIFY(d == d2);
            VERIFY(it != iss.end());
            VERIFY(*it == '.');
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
            VERIFY(*it == '1');
            VERIFY(f == 0.0f);
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
            VERIFY(*it == '3');
            VERIFY(d == 0.0);
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
            VERIFY(*it == '6');
            VERIFY(ld == 0.0l);
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
        VERIFY(it == iss1.end());
        VERIFY(d == d1);
    }

    {
        std::string iss1 = "142";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, d);
        VERIFY(it != iss1.end());
        VERIFY(*it == '2');
        VERIFY(d == d2);
    }

    {
        std::string iss1 = "3e14";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, d);
        VERIFY(it != iss1.end());
        VERIFY(*it == '4');
        VERIFY(d == d3);
    }

    {
        std::string iss1 = "1234";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        VERIFY(it != iss1.end());
        VERIFY(*it == '4');
        VERIFY(l == l1);
    }

    {
        std::string iss2 = "123";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios, d);
        VERIFY(it == iss2.end());
        VERIFY(d == d1);
    }

    {
        std::string iss2 = "120";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios, l);
        VERIFY(it == iss2.end());
        VERIFY(l == l2);
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
        VERIFY(*it == '+');
    }
    {
        iss1 = "0x1";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        VERIFY(it != iss1.end());
        VERIFY(*it == 'x');
        VERIFY(l == l1);
    }
    {
        iss1 = "0Xa";
        ios.unsetf(IOv2::ios_defs::basefield);
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        VERIFY(it == iss1.end());
        VERIFY(l == l2);
    }
    {
        iss1 = "0xa";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, l);
        VERIFY(it != iss1.end());
        VERIFY(*it == 'x');
        VERIFY(l == l1);
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
        VERIFY(*it == '+');
    }
    {
        iss1 = "x4";
        auto it = ng1.get(iss1.begin(), iss1.end(), ios, d);
        VERIFY(it == iss1.end());
        VERIFY(d == d1);
    }
    {
        iss2 = "0001-";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios2, l);
        VERIFY(it != iss2.end());
        VERIFY(*it == '-');
        VERIFY(l == l3);
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
        VERIFY(*it == '-');
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
        VERIFY(*it == '0');
        VERIFY(l == 0);
    }
    {
        iss2 = "000778";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios2, l);
        VERIFY(it != iss2.end());
        VERIFY(*it == '8');
        VERIFY(l == l4);
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
        VERIFY(*it == '0');
        VERIFY(d == d2);
    }
    {
        iss2 = "-1";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios2, d);
        VERIFY(it == iss2.end());
        VERIFY(d == d3);
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
        VERIFY(it == iss1.end());
        VERIFY(l == l1);
    }
    {
        iss2 = "123,456,7.0";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios, d);
        VERIFY(it == iss2.end());
        VERIFY(d == d1);
    }
    {
        iss2 = "12,345,6.0";
        auto it = ng2.get(iss2.begin(), iss2.end(), ios, d);
        VERIFY(it == iss2.end());
        VERIFY(d == d2);
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
        VERIFY(it == iss.end());
        VERIFY(d == d1);
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
        VERIFY(d == 0.0);
        VERIFY(*it == '1');
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
        VERIFY(d == 0.0);
        VERIFY(*it == '3');
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
            VERIFY(it == str.end());
            VERIFY(us0 == us1);
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
            VERIFY(!(us0 != std::numeric_limits<unsigned short>::max()));
        }
        {
            ui0 = 0U;
            ss.clear(); ss.str("");
            ss << ui1 << ' '; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ui0);
            VERIFY(it != str.end());
            VERIFY(ui0 == ui1);
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
            VERIFY(!(ui0 != std::numeric_limits<unsigned int>::max()));
        }
        {
            ul0 = 0UL;
            ss.clear(); ss.str("");
            ss << ul1; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ul0);
            VERIFY(it == str.end());
            VERIFY(ul0 == ul1);
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
            VERIFY(!(ul0 != std::numeric_limits<unsigned long>::max()));
        }
        {
            l01 = 0L;
            ss.clear(); ss.str("");
            ss << l1 << ' '; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, l01);
            VERIFY(it != str.end());
            VERIFY(l01 == l1);
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
            VERIFY(!(l01 != std::numeric_limits<long>::max()));
        }
        {
            l02 = 0L;
            ss.clear(); ss.str("");
            ss << l2; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, l02);
            VERIFY(it == str.end());
            VERIFY(l02 == l2);
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
            VERIFY(!(l02 != std::numeric_limits<long>::min()));
        }
        {
            ull0 = 0ULL;
            ss.clear(); ss.str("");
            ss << ull1 << ' '; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ull0);
            VERIFY(it != str.end());
            VERIFY(ull0 == ull1);
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
            VERIFY(!(ull0 != std::numeric_limits<unsigned long long>::max()));
        }
        {
            ll01 = 0LL;
            ss.clear(); ss.str("");
            ss << ll1; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ll01);
            VERIFY(it == str.end());
            VERIFY(ll01 == ll1);
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
            VERIFY(!(ll01 != std::numeric_limits<long long>::max()));
        }
        {
            ll02 = 0LL;
            ss.clear(); ss.str("");
            ss << ll2 << ' '; std::string str = ss.str();
            auto it = obj.get(str.begin(), str.end(), ios, ll02);
            VERIFY(it != str.end());
            VERIFY(ll02 == ll2);
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
            VERIFY(!(ll02 != std::numeric_limits<long long>::min()));
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
        VERIFY(l == l1);
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
        VERIFY(l == 0);
        VERIFY(*it == '0');
    }
    {
        iss1 = "0#0#0#2";
        auto it = obj.get(iss1.begin(), iss1.end(), ios, l);
        VERIFY(it == iss1.end());
        VERIFY(l == l2);
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
        VERIFY(d == d1);
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
        VERIFY(*it == '0');
        VERIFY(d == 0.0);
    }
    {
        iss1 = "0#0#0#2";
        auto it = obj.get(iss1.begin(), iss1.end(), ios, d);
        VERIFY(it == iss1.end());
        VERIFY(d == d2);
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
        VERIFY(*it == '0');
        VERIFY(l == 0);
    }
    {
        iss1 = "00#0#3";
        auto it = obj.get(iss1.begin(), iss1.end(), ios, l);
        VERIFY(it == iss1.end());
        VERIFY(l == l3);
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
        VERIFY(l == l2);
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
        VERIFY(it == iss.end());
        VERIFY(l == l1);
    }
    {
        iss = "123456,78";
        auto it = ng2.get(iss.begin(), iss.end(), ios, l);
        VERIFY(it == iss.end());
        VERIFY(l == l2);
    }
    {
        iss = "1234,56,7.0";
        auto it = ng3.get(iss.begin(), iss.end(), ios, d);
        VERIFY(it == iss.end());
        VERIFY(d == d1);
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
        VERIFY(it == iss.end());
        VERIFY(b0 == true);
    }
    {
        iss = "false";
        auto it = ng0.get(iss.begin(), iss.end(), ios, b0);
        VERIFY(it == iss.end());
        VERIFY(b0 == false);
    }
    {
        iss = "a";
        auto it = ng1.get(iss.begin(), iss.end(), ios, b1);
        VERIFY(it == iss.end());
        VERIFY(b1 == true);
    }
    {
        iss = "abb";
        auto it = ng1.get(iss.begin(), iss.end(), ios, b1);
        VERIFY(it == iss.end());
        VERIFY(b1 == false);
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
        VERIFY(b1 == false);
        VERIFY(*it == 'a');
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
        VERIFY(b1 == false);
    }
    {
        iss = "1";
        auto it = ng2.get(iss.begin(), iss.end(), ios, b2);
        VERIFY(it == iss.end());
        VERIFY(b2 == true);
    }
    {
        iss = "0";
        auto it = ng2.get(iss.begin(), iss.end(), ios, b2);
        VERIFY(it == iss.end());
        VERIFY(b2 == false);
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
        VERIFY(b2 == false);
        VERIFY(*it == '2');
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
        VERIFY(b3 == false);
        VERIFY(*it == 'b');
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
        VERIFY(b3 == false);
    }
    {
        iss = "one";
        auto it = ng4.get(iss.begin(), iss.end(), ios, b4);
        VERIFY(it == iss.end());
        VERIFY(b4 == true);
    }
    {
        iss = "two";
        auto it = ng4.get(iss.begin(), iss.end(), ios, b4);
        VERIFY(it == iss.end());
        VERIFY(b4 == false);
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
        VERIFY(b4 == false);
        VERIFY(*it == 't');
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
        VERIFY(b4 == false);
    }
    dump_info("Done\n");
}

void test_numeric_char_get_20()
{
    dump_info("Test numeric<char>::get 20...");
    IOv2::ios_base<char> ios;
    long double l = -1;
    
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({});
    const IOv2::numeric<char> obj(p1, s_ctype_c);
     
    std::string iss = "123,456";
    auto it = obj.get(iss.begin(), iss.end(), ios, l);
    VERIFY(it != iss.end());
    VERIFY(l == 123);
    VERIFY(*it == ',');
    
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
            VERIFY(it == ss.end());
            VERIFY(ul0 == 0);
        }
        {
            ss = "-1";
            auto it = obj.get(ss.begin(), ss.end(), ios, ul0);
            VERIFY(it == ss.end());
            VERIFY(ul0 == ul1);
        }
        {
            std::stringstream ss0;
            ss0 << '-' << ul1; ss = ss0.str();
            auto it = obj.get(ss.begin(), ss.end(), ios, ul0);
            VERIFY(it == ss.end());
            VERIFY(ul0 == 1);
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
            VERIFY(ul0 == ul1);
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
            VERIFY(b1 == true);
            VERIFY(it == std::default_sentinel);
        }
        {
            streambuf sb(mem_device{"0"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, b0);
            VERIFY(b0 == false);
            VERIFY(it == std::default_sentinel);
        }

        // ... and one that does
        {
            streambuf sb(mem_device{"1.294.967.294+-----"});
            auto beg = istreambuf_iterator(sb);
            ios.width(20);
            ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            VERIFY(ul == ul1);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == '+');
        }

        {
            streambuf sb(mem_device{"+1,02345e+308"});
            auto beg = istreambuf_iterator(sb);
            ios.width(20);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
            auto it = obj.get(beg, std::default_sentinel, ios, d);
            VERIFY(d == d1);
            VERIFY(it == std::default_sentinel);
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
            VERIFY(d == d2);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ' ');
        }

        // long double
        {
            streambuf sb(mem_device{"6,630025e+4"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ld);
            VERIFY(ld == ld1);
            VERIFY(it == std::default_sentinel);
        }

        {
            streambuf sb(mem_device{"0 "});
            auto beg = istreambuf_iterator(sb);
            ios.precision(0);
            ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
            auto it = obj.get(beg, std::default_sentinel, ios, ld);
            VERIFY(ld == 0);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ' ');
        }

        // void*
        {
            streambuf sb(mem_device{"0xbffff74c,"});
            auto beg = istreambuf_iterator(sb);

            auto it = obj.get(beg, std::default_sentinel, ios, v);
            VERIFY(v != 0);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ',');
        }

        {
            long long ll1 = 9223372036854775807LL;
            long long ll;
            
            streambuf sb(mem_device{"9.223.372.036.854.775.807"});
            auto beg = istreambuf_iterator(sb);

            auto it = obj.get(beg, std::default_sentinel, ios, ll);
            VERIFY(ll == ll1);
            VERIFY(it == std::default_sentinel);
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
            VERIFY(b0 == true);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ' ');
        }

        {
            streambuf sb(mem_device{"false "});
            auto beg = istreambuf_iterator(sb);
            ios.setf(IOv2::ios_defs::boolalpha);
            auto it = obj.get(beg, std::default_sentinel, ios, b1);
            VERIFY(b1 == false);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ' ');
        }

        // unsigned long
        {
            streambuf sb(mem_device{"1294967294"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            VERIFY(ul == ul1);
            VERIFY(it == std::default_sentinel);
        }
        {
            streambuf sb(mem_device{"0+----------------------"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            VERIFY(ul == ul2);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == '+');
        }

        // double
        {
            streambuf sb(mem_device{"1.02345e+308+-------"});
            auto beg = istreambuf_iterator(sb);
            ios.width(20);
            ios.setf(IOv2::ios_defs::left, IOv2::ios_defs::adjustfield);
            auto it = obj.get(beg, std::default_sentinel, ios, d);
            VERIFY(d == d1);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == '+');
        }
        {
            streambuf sb(mem_device{"+3.15e-308"});
            auto beg = istreambuf_iterator(sb);
            ios.width(20);
            ios.setf(IOv2::ios_defs::right, IOv2::ios_defs::adjustfield);
            auto it = obj.get(beg, std::default_sentinel, ios, d);
            VERIFY(d == d2);
            VERIFY(it == std::default_sentinel);
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
            VERIFY(l == l1);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ' ');
        }
        {
            streambuf sb(mem_device{"-2,147,483,647+-----"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, l);
            VERIFY(l == l2);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == '+');
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
            VERIFY(i == 20000106);
            VERIFY(it != std::default_sentinel);
            VERIFY(std::string(it, decltype(it)()) == " Elizabeth Durack");
        }
        {
            // 02 get(long double)
            streambuf sb(mem_device{"20000106 Elizabeth Durack"});
            auto beg = istreambuf_iterator(sb);
            long double ld = 0.0;
            auto it = obj.get(beg, std::default_sentinel, ios, ld);
            VERIFY(ld == 20000106);
            VERIFY(it != std::default_sentinel);
            VERIFY(std::string(it, decltype(it)()) == " Elizabeth Durack");
        }

        {
            // 03 get(bool)
            streambuf sb(mem_device{"0 true 0xbffff74c Durack"});
            auto beg = istreambuf_iterator(sb);

            bool b = 1;
            auto it = obj.get(beg, std::default_sentinel, ios, b);
            VERIFY(b == 0);
            VERIFY(it != std::default_sentinel);

            ios.setf(IOv2::ios_defs::boolalpha);
            it = obj.get(++it, std::default_sentinel, ios, b);
            VERIFY(b == true);
            VERIFY(it != std::default_sentinel);
            
            // 04 get(void*)
            void* v;
            ios.setf(IOv2::ios_defs::fixed, IOv2::ios_defs::floatfield);
            it = obj.get(++it, std::default_sentinel, ios, v);
            VERIFY(v == (void*)0xbffff74c);
            VERIFY(it != std::default_sentinel);
            VERIFY(std::string(it, decltype(it)()) == " Durack");
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
            VERIFY(ul == 0xbffff74c);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ' ');
        }
        {
            streambuf sb(mem_device{"0Xf.fff "});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            VERIFY(ul == 0xffff);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ' ');
        }
        {
            streambuf sb(mem_device{"ffe "});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            VERIFY(ul == 0xffe);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ' ');
        }
        {
            ios.setf(IOv2::ios_defs::oct, IOv2::ios_defs::basefield);
            streambuf sb(mem_device{"07.654.321 "});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            VERIFY(ul == 07654321);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ' ');
        }
        {
            streambuf sb(mem_device{"07.777 "});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            VERIFY(ul == 07777);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ' ');
        }
        {
            streambuf sb(mem_device{"776 "});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul);
            VERIFY(ul == 0776);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ' ');
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
        VERIFY(d == 1234.5);
        VERIFY(it != std::default_sentinel);
        VERIFY(*it == ' ');
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
            VERIFY(*it == 'e');
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
            VERIFY(*it == 'e');
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
            VERIFY(*it == 'L');
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
            VERIFY(*it == 'r');
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
            VERIFY(*it == 's');
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
            VERIFY(d == d1);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == ',');
        }
        {
            streambuf sb(mem_device{"3e1."});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, d);
            VERIFY(d == d2);
            VERIFY(it != std::default_sentinel);
            VERIFY(*it == '.');
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
            VERIFY(*it == '.');
            VERIFY(f == 0.0f);
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
            VERIFY(it == std::default_sentinel);
            VERIFY(d == 0.0);
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
            VERIFY(*it == ' ');
            VERIFY(ld == 0.0l);
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
        VERIFY(it == std::default_sentinel);
        VERIFY(d == d1);
    }

    {
        streambuf sb(mem_device{"142"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, d);
        VERIFY(it != std::default_sentinel);
        VERIFY(*it == '2');
        VERIFY(d == d2);
    }

    {
        streambuf sb(mem_device{"3e14"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, d);
        VERIFY(it != std::default_sentinel);
        VERIFY(*it == '4');
        VERIFY(d == d3);
    }

    {
        streambuf sb(mem_device{"1234"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, l);
        VERIFY(it != std::default_sentinel);
        VERIFY(*it == '4');
        VERIFY(l == l1);
    }

    {
        streambuf sb(mem_device{"123"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, d);
        VERIFY(it == std::default_sentinel);
        VERIFY(d == d1);
    }

    {
        streambuf sb(mem_device{"120"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, l);
        VERIFY(it == std::default_sentinel);
        VERIFY(l == l2);
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
        VERIFY(*it == '+');
    }
    {
        streambuf sb(mem_device{"0x1"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, l);
        VERIFY(it != std::default_sentinel);
        VERIFY(*it == 'x');
        VERIFY(l == l1);
    }
    {
        streambuf sb(mem_device{"0Xa"});
        auto beg = istreambuf_iterator(sb);
        ios.unsetf(IOv2::ios_defs::basefield);
        auto it = ng1.get(beg, std::default_sentinel, ios, l);
        VERIFY(it == std::default_sentinel);
        VERIFY(l == l2);
    }
    {
        streambuf sb(mem_device{"0xa"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, l);
        VERIFY(it != std::default_sentinel);
        VERIFY(*it == 'x');
        VERIFY(l == l1);
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
        VERIFY(*it == '+');
    }
    {
        streambuf sb(mem_device{"x4"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, d);
        VERIFY(it == std::default_sentinel);
        VERIFY(d == d1);
    }
    {
        streambuf sb(mem_device{"0001-"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios2, l);
        VERIFY(it != std::default_sentinel);
        VERIFY(*it == '-');
        VERIFY(l == l3);
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
        VERIFY(*it == '-');
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
        VERIFY(*it == 'X');
        VERIFY(l == 0);
    }
    {
        streambuf sb(mem_device{"000778"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios2, l);
        VERIFY(it != std::default_sentinel);
        VERIFY(*it == '8');
        VERIFY(l == l4);
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
        VERIFY(d == d2);
    }
    {
        streambuf sb(mem_device{"-1"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios2, d);
        VERIFY(it == std::default_sentinel);
        VERIFY(d == d3);
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
        VERIFY(it == std::default_sentinel);
        VERIFY(l == l1);
    }
    {
        streambuf sb(mem_device{"123,456,7.0"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, d);
        VERIFY(it == std::default_sentinel);
        VERIFY(d == d1);
    }
    {
        streambuf sb(mem_device{"12,345,6.0"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, d);
        VERIFY(it == std::default_sentinel);
        VERIFY(d == d2);
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
        VERIFY(it == std::default_sentinel);
        VERIFY(d == d1);
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
        VERIFY(d == 0.0);
        VERIFY(*it == '+');
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
        VERIFY(d == 0.0);
        VERIFY(*it == '-');
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
            VERIFY(it == std::default_sentinel);
            VERIFY(us0 == us1);
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
            VERIFY(it == std::default_sentinel);
            VERIFY(!(us0 != std::numeric_limits<unsigned short>::max()));
        }
        {
            ui0 = 0U;
            ss.clear(); ss.str("");
            ss << ui1 << ' '; 
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ui0);
            VERIFY(it != std::default_sentinel);
            VERIFY(ui0 == ui1);
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
            VERIFY(it == std::default_sentinel);
            VERIFY(!(ui0 != std::numeric_limits<unsigned int>::max()));
        }
        {
            ul0 = 0UL;
            ss.clear(); ss.str("");
            ss << ul1;
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul0);
            VERIFY(it == std::default_sentinel);
            VERIFY(ul0 == ul1);
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
            VERIFY(it == std::default_sentinel);
            VERIFY(!(ul0 != std::numeric_limits<unsigned long>::max()));
        }
        {
            l01 = 0L;
            ss.clear(); ss.str("");
            ss << l1 << ' ';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, l01);
            VERIFY(it != std::default_sentinel);
            VERIFY(l01 == l1);
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
            VERIFY(it == std::default_sentinel);
            VERIFY(!(l01 != std::numeric_limits<long>::max()));
        }
        {
            l02 = 0L;
            ss.clear(); ss.str("");
            ss << l2;
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, l02);
            VERIFY(it == std::default_sentinel);
            VERIFY(l02 == l2);
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
            VERIFY(it == std::default_sentinel);
            VERIFY(!(l02 != std::numeric_limits<long>::min()));
        }
        {
            ull0 = 0ULL;
            ss.clear(); ss.str("");
            ss << ull1 << ' ';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ull0);
            VERIFY(it != std::default_sentinel);
            VERIFY(ull0 == ull1);
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
            VERIFY(it == std::default_sentinel);
            VERIFY(!(ull0 != std::numeric_limits<unsigned long long>::max()));
        }
        {
            ll01 = 0LL;
            ss.clear(); ss.str("");
            ss << ll1;
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ll01);
            VERIFY(it == std::default_sentinel);
            VERIFY(ll01 == ll1);
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
            VERIFY(it == std::default_sentinel);
            VERIFY(!(ll01 != std::numeric_limits<long long>::max()));
        }
        {
            ll02 = 0LL;
            ss.clear(); ss.str("");
            ss << ll2 << ' ';
            streambuf sb(mem_device{ss.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ll02);
            VERIFY(it != std::default_sentinel);
            VERIFY(ll02 == ll2);
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
            VERIFY(it == std::default_sentinel);
            VERIFY(!(ll02 != std::numeric_limits<long long>::min()));
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
        VERIFY(it == std::default_sentinel);
        VERIFY(l == l1);
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
        VERIFY(l == 0);
        VERIFY(*it == '#');
    }
    {
        streambuf sb(mem_device{"0#0#0#2"});
        auto beg = istreambuf_iterator(sb);
        auto it = obj.get(beg, std::default_sentinel, ios, l);
        VERIFY(it == std::default_sentinel);
        VERIFY(l == l2);
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
        VERIFY(it == std::default_sentinel);
        VERIFY(d == d1);
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
        VERIFY(*it == '#');
        VERIFY(d == 0.0);
    }
    {
        streambuf sb(mem_device{"0#0#0#2"});
        auto beg = istreambuf_iterator(sb);
        auto it = obj.get(beg, std::default_sentinel, ios, d);
        VERIFY(it == std::default_sentinel);
        VERIFY(d == d2);
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
        VERIFY(*it == '#');
        VERIFY(l == 0);
    }
    {
        streambuf sb(mem_device{"00#0#3"});
        auto beg = istreambuf_iterator(sb);
        auto it = obj.get(beg, std::default_sentinel, ios, l);
        VERIFY(it == std::default_sentinel);
        VERIFY(l == l3);
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
        VERIFY(it == std::default_sentinel);
        VERIFY(l == l2);
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
        VERIFY(it == std::default_sentinel);
        VERIFY(l == l1);
    }
    {
        streambuf sb(mem_device{"123456,78"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, l);
        VERIFY(it == std::default_sentinel);
        VERIFY(l == l2);
    }
    {
        streambuf sb(mem_device{"1234,56,7.0"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng3.get(beg, std::default_sentinel, ios, d);
        VERIFY(it == std::default_sentinel);
        VERIFY(d == d1);
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
        VERIFY(it == std::default_sentinel);
        VERIFY(b0 == true);
    }
    {
        streambuf sb(mem_device{"false"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng0.get(beg, std::default_sentinel, ios, b0);
        VERIFY(it == std::default_sentinel);
        VERIFY(b0 == false);
    }
    {
        streambuf sb(mem_device{"a"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, b1);
        VERIFY(it == std::default_sentinel);
        VERIFY(b1 == true);
    }
    {
        streambuf sb(mem_device{"abb"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng1.get(beg, std::default_sentinel, ios, b1);
        VERIFY(it == std::default_sentinel);
        VERIFY(b1 == false);
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
        VERIFY(b1 == false);
        VERIFY(*it == 'c');
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
        VERIFY(it == std::default_sentinel);
        VERIFY(b1 == false);
    }
    {
        streambuf sb(mem_device{"1"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, b2);
        VERIFY(it == std::default_sentinel);
        VERIFY(b2 == true);
    }
    {
        streambuf sb(mem_device{"0"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng2.get(beg, std::default_sentinel, ios, b2);
        VERIFY(it == std::default_sentinel);
        VERIFY(b2 == false);
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
        VERIFY(b2 == false);
        VERIFY(*it == '2');
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
        VERIFY(b3 == false);
        VERIFY(*it == 'b');
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
        VERIFY(b3 == false);
    }
    {
        streambuf sb(mem_device{"one"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng4.get(beg, std::default_sentinel, ios, b4);
        VERIFY(it == std::default_sentinel);
        VERIFY(b4 == true);
    }
    {
        streambuf sb(mem_device{"two"});
        auto beg = istreambuf_iterator(sb);
        auto it = ng4.get(beg, std::default_sentinel, ios, b4);
        VERIFY(it == std::default_sentinel);
        VERIFY(b4 == false);
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
        VERIFY(b4 == false);
        VERIFY(*it == 'h');
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
        VERIFY(it == std::default_sentinel);
        VERIFY(b4 == false);
    }
    dump_info("Done\n");
}

void test_numeric_char_get_41()
{
    dump_info("Test numeric<char>::get 41...");
    IOv2::ios_base<char> ios;
    long double l = -1;
    
    auto p1 = std::make_shared<Punct>("C"); p1->set_grouping({});
    const IOv2::numeric<char> obj(p1, s_ctype_c);

    using namespace IOv2;
    streambuf sb(mem_device{"123,456"});
    auto beg = istreambuf_iterator(sb);
    auto it = obj.get(beg, std::default_sentinel, ios, l);
    VERIFY(it != std::default_sentinel);
    VERIFY(l == 123);
    VERIFY(*it == ',');
    
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
            VERIFY(it == std::default_sentinel);
            VERIFY(ul0 == 0);
        }
        {
            streambuf sb(mem_device{"-1"});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul0);
            VERIFY(it == std::default_sentinel);
            VERIFY(ul0 == ul1);
        }
        {
            std::stringstream ss0;
            ss0 << '-' << ul1;
            streambuf sb(mem_device{ss0.str()});
            auto beg = istreambuf_iterator(sb);
            auto it = obj.get(beg, std::default_sentinel, ios, ul0);
            VERIFY(it == std::default_sentinel);
            VERIFY(ul0 == 1);
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
            VERIFY(it == std::default_sentinel);
            VERIFY(ul0 == ul1);
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

    // 3. Test dynamic resizing (scientific, precision 200 triggers two-pass)
    {
        const std::uint8_t high_prec = 200;
        ios.precision(high_prec);
        ios.setf(IOv2::ios_defs::scientific, IOv2::ios_defs::floatfield);
        std::string oss;
        // scientific + prec 200: initial cs_size=77 < output ~206 chars -> two-pass triggered
        nump.put(std::back_inserter(oss), ios, 1.0);
        size_t dot_pos = oss.find('.');
        VERIFY(dot_pos != std::string::npos);
        size_t e_pos = oss.find('e', dot_pos);
        VERIFY(e_pos != std::string::npos);
        VERIFY(e_pos - dot_pos - 1 == static_cast<size_t>(high_prec));
    }

    dump_info("Done\n");
}

void test_numeric_char_conf_bool_fallback()
{
    dump_info("Test numeric_conf<char> bool-name fallback...");

    // "C.UTF-8" is NOT the literal "C"/"POSIX" fast path, so the constructor
    // runs the full locale-snapshot branch. glibc's C.UTF-8 exposes empty
    // YESSTR/NOSTR, which the constructor treats as missing keys and replaces
    // with the ASCII defaults "true"/"false".
    IOv2::numeric_conf<char> conf("C.UTF-8");

    VERIFY(conf.truename() == "true");
    VERIFY(conf.falsename() == "false");
    // C.UTF-8 has no thousands separator, so grouping stays empty and
    // decimal_point is the plain ASCII dot.
    VERIFY(conf.decimal_point() == '.');
    VERIFY(conf.thousands_sep() == '\0');
    VERIFY(conf.grouping().empty());

    dump_info("Done\n");
}

void test_numeric_char_get_bool_edge()
{
    dump_info("Test numeric<char>::get bool edge cases...");

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"), s_ctype_c);

    // Without boolalpha, the field is parsed as a long; any value other than
    // 0/1 is out of range. Per LWG 23 the result is set to true and the parse
    // fails (throws).
    {
        IOv2::ios_base<char> ios;
        bool v = false;
        std::string in = "2";
        bool threw = false;
        try { obj.get(in.begin(), in.end(), ios, v); }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
        VERIFY(v == true);
    }

    // With boolalpha, identical truename()/falsename() make a matching input
    // ambiguous: it satisfies both names at the same length, so the parse
    // fails and the result is set to false (LWG 23).
    {
        auto punct = std::make_shared<Punct>("C");
        punct->set_truename("same");
        punct->set_falsename("same");
        IOv2::numeric<char> ambiguous(punct, s_ctype_c);

        IOv2::ios_base<char> ios;
        ios.setf(IOv2::ios_defs::boolalpha);
        bool v = true;
        std::string in = "same";
        bool threw = false;
        try { ambiguous.get(in.begin(), in.end(), ios, v); }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
        VERIFY(v == false);
    }

    dump_info("Done\n");
}

void test_numeric_char_put_edge()
{
    dump_info("Test numeric<char>::put edge cases...");

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"), s_ctype_c);

    // Octal base with showbase prepends a single '0' for non-zero values.
    {
        IOv2::ios_base<char> ios;
        ios.setf(IOv2::ios_defs::oct, IOv2::ios_defs::basefield);
        ios.setf(IOv2::ios_defs::showbase);
        std::string out;
        obj.put(std::back_inserter(out), ios, 64L);
        VERIFY(out == "0100");
    }

    // showpoint sets the '#' printf flag, forcing a decimal point.
    {
        IOv2::ios_base<char> ios;
        ios.setf(IOv2::ios_defs::showpoint);
        std::string out;
        obj.put(std::back_inserter(out), ios, 1.0);
        VERIFY(out.find('.') != std::string::npos);
        VERIFY(out.size() > 1);
    }

    // A float formatted as "0.5", right-padded into a wider field, drives the
    // pad path where the leading '0' triggers the 0x-prefix probe.
    {
        IOv2::ios_base<char> ios;
        ios.width(10);
        std::string out;
        obj.put(std::back_inserter(out), ios, 0.5);
        VERIFY(out.size() == 10);
        VERIFY(out.back() == '5');
    }

    // Internal adjustment on a negative value keeps the sign anchored to the
    // left and inserts fill between the sign and the digits.
    {
        IOv2::ios_base<char> ios;
        ios.width(8);
        ios.setf(IOv2::ios_defs::internal, IOv2::ios_defs::adjustfield);
        std::string out;
        obj.put(std::back_inserter(out), ios, -42L);
        VERIFY(out.size() == 8);
        VERIFY(out.front() == '-');
        VERIFY(out.substr(out.size() - 2) == "42");
    }

    dump_info("Done\n");
}

void test_numeric_char_get_int_edge()
{
    dump_info("Test numeric<char>::get integer edge cases...");

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"), s_ctype_c);

    // A lone sign with no following digit is a parse failure.
    {
        IOv2::ios_base<char> ios;
        long v = 123;
        std::string in = "-";
        bool threw = false;
        try { obj.get(in.begin(), in.end(), ios, v); }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    // Under an explicit octal base, "0x5" stops at 'x' (the "0x" prefix is only
    // honored for hex/auto base) and yields just the leading zero.
    {
        IOv2::ios_base<char> ios;
        ios.setf(IOv2::ios_defs::oct, IOv2::ios_defs::basefield);
        long v = -1;
        std::string in = "0x5";
        auto it = obj.get(in.begin(), in.end(), ios, v);
        VERIFY(v == 0);
        VERIFY(it == in.begin() + 1);
    }

    // A single digit group exceeding 255 digits cannot be stored in the
    // uint8_t group counter and is rejected. Leading zeros under an explicit
    // decimal base accumulate the group length without overflowing the value.
    auto punct = std::make_shared<Punct>("C");
    punct->set_grouping({3});
    punct->set_thousands_sep(',');
    IOv2::numeric<char> grp(punct, s_ctype_c);

    {
        IOv2::ios_base<char> ios;
        ios.setf(IOv2::ios_defs::dec, IOv2::ios_defs::basefield);
        long v = 1;
        std::string in = std::string(256, '0') + ",5";  // oversized leading group, then a separator
        bool threw = false;
        try { grp.get(in.begin(), in.end(), ios, v); }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }
    {
        IOv2::ios_base<char> ios;
        ios.setf(IOv2::ios_defs::dec, IOv2::ios_defs::basefield);
        long v = 1;
        // A leading '0' keeps the running value at zero so it never overflows;
        // the digit count then reaches the >255 grouping limit at end of input.
        std::string in = "0," + std::string(256, '0');  // oversized final group at end of input
        bool threw = false;
        try { grp.get(in.begin(), in.end(), ios, v); }
        catch (const IOv2::stream_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}

void test_numeric_char_get_float_edge()
{
    dump_info("Test numeric<char>::get floating-point edge cases...");

    IOv2::numeric<char> obj(std::make_shared<IOv2::numeric_conf<char>>("C"), s_ctype_c);

    auto throws = [&](const std::string& in)
    {
        IOv2::ios_base<char> ios;
        double v = -1.0;
        try { obj.get(in.begin(), in.end(), ios, v); return false; }
        catch (const IOv2::stream_error&) { return true; }
    };

    // A lone sign fails.
    VERIFY(throws("+"));
    // An exponent marker with no exponent digits fails.
    VERIFY(throws("1e"));
    // Magnitudes that strtod rounds to +/-infinity map to +/-max() and fail
    // per LWG 23.
    VERIFY(throws("1e400"));
    VERIFY(throws("-1e400"));

    // A bare "0" parses successfully to 0.0 (exercises the leading-zero / EOF path).
    {
        IOv2::ios_base<char> ios;
        double v = -1.0;
        std::string in = "0";
        obj.get(in.begin(), in.end(), ios, v);
        VERIFY(v == 0.0);
    }

    // Oversized digit groups (>255 digits) are rejected at each structural
    // boundary: before a thousands separator, before the decimal point, before
    // the exponent marker, and at end of input.
    auto punct = std::make_shared<Punct>("C");
    punct->set_grouping({3});
    punct->set_thousands_sep(',');
    punct->set_decimal_point('.');
    IOv2::numeric<char> grp(punct, s_ctype_c);

    auto grp_throws = [&](const std::string& in)
    {
        IOv2::ios_base<char> ios;
        double v = -1.0;
        try { grp.get(in.begin(), in.end(), ios, v); return false; }
        catch (const IOv2::stream_error&) { return true; }
    };

    const std::string big(256, '1');
    VERIFY(grp_throws(big + ",5"));        // oversized group before a separator
    VERIFY(grp_throws("1," + big + ".5")); // oversized group before the decimal point
    VERIFY(grp_throws("1," + big + "e5")); // oversized group before the exponent
    VERIFY(grp_throws("1," + big));        // oversized final group at end of input

    dump_info("Done\n");
}
