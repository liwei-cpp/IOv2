#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>

void test_istream_extractors_arithmetic_char_1()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 1...");

    auto helper = []<template <typename, typename > class T>()
    {
        std::string str_02("true false 0 1 110001");
        std::string str_03("-19999999 777777 -234234 233 -234 33 1 66300.25 .315 1.5");
        std::string str_04("0123");

        T is_02{IOv2::mem_device{str_02}};
        T is_03{IOv2::mem_device{str_03}};
        T is_04{IOv2::mem_device{str_04}};

        // Integral Types:
        bool            b1  = false;
        short           s1  = 0;
        int             i1  = 0;
        long            l1  = 0;
        unsigned short  us1 = 0;
        unsigned int    ui1 = 0;
        unsigned long   ul1 = 0;

        // Floating-point Types:
        float       f1  = 0;
        double      d1  = 0;
        long double ld1 = 0;

        // process alphanumeric versions of bool values
        is_02.setf(IOv2::ios_defs::boolalpha);
        is_02 >> b1;
        VERIFY(b1 == 1);
        is_02 >> b1;
        VERIFY(b1 == 0);
        b1 = 1;

        // process numeric versions of of bool values
        is_02.unsetf(IOv2::ios_defs::boolalpha);
        is_02 >> b1;
        VERIFY(b1 == 0);
        is_02 >> b1;
        VERIFY(b1 == 1);

        // is_03 == "-19999999 777777 -234234 233 -234 33 1 66300.25 .315 1.5"
        is_03 >> l1;
        VERIFY(l1 == -19999999);
        is_03 >> ul1;
        VERIFY(ul1 == 777777);
        is_03 >> i1;
        VERIFY(i1 == -234234);
        is_03 >> ui1;
        VERIFY(ui1 == 233);
        is_03 >> s1;
        VERIFY(s1 == -234);
        is_03 >> us1;
        VERIFY(us1 == 33);
        is_03 >> b1;
        VERIFY(b1 == 1);
        is_03 >> ld1;
        VERIFY(ld1 == 66300.25);
        is_03 >> d1;
        VERIFY(d1 == (double) .315);
        is_03 >> f1;
        VERIFY(f1 == 1.5);

        is_04 >> IOv2::hex >> i1;
        VERIFY(i1 == 0x123);

        // test void pointers
// TODO: comment out since this one does not work for the current gcc compiler, should fix then un-comment
//        int i = 55;
//        int j = 3;
//        int* po = &i;
//        int* pi = &j;
//
//        IOv2::ostream ss_01{IOv2::mem_device{""}};
//        ss_01 << po;
//        auto dev = ss_01.detach();
//        dev.dseek(0);
//        T is_05{std::move(dev)};
//        is_05 >> pi;
//        VERIFY(pi == po);
    };
    
    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_arithmetic_char_2()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 2...");

    auto helper = []<template<typename, typename> class T>()
    {
        const std::string str_01("20000AB");
        T is(IOv2::mem_device{str_01});

        int n = 15;
        is >> n;
        VERIFY(n == 20000);
        auto c = is.peek();
        VERIFY(c == 65);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_arithmetic_char_3()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 3...");

    auto helper = []<template<typename, typename> class T>()
    {
        IOv2::ostream ostr(IOv2::mem_device{""});
        ostr << "12220101";
        auto dev = ostr.detach();
        dev.dseek(0);

        T istr(std::move(dev));
        long l01;
        istr >> l01;
        VERIFY(l01 == 12220101);
        VERIFY(istr.rdstate() == IOv2::ios_defs::eofbit);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_arithmetic_char_4()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 4...");

    auto helper = []<template<typename, typename> class T>()
    {
        // default locale, grouping is turned off
        unsigned int h4;
        char c;
        T is{IOv2::mem_device{"205,199,144"}};

        is >> h4; // 205
        VERIFY(h4 == 205);
        is >> c; // ','
        VERIFY(c == ',');

        is >> h4; // 199
        VERIFY(h4 == 199);
        is >> c; // ','
        VERIFY(c == ',');

        is >> h4; // 144
        VERIFY(h4 == 144);
        VERIFY(is.rdstate() == IOv2::ios_defs::eofbit);

        is >> c; // EOF
        VERIFY(c == ',');
        VERIFY(is.rdstate() & IOv2::ios_defs::eofbit);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

namespace
{
    class test_numpunct1 : public IOv2::numeric_conf<char>
    {
    public:
        test_numpunct1()
            : IOv2::numeric_conf<char>("C")
            , m_group{3}
        { }

        virtual const std::vector<uint8_t>& grouping() const override { return m_group; }
    private:
        std::vector<uint8_t> m_group;
    };
}

void test_istream_extractors_arithmetic_char_5()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 5...");
    auto helper = []<template<typename, typename> class T>()
    {
        // manufactured locale, grouping is turned on
        unsigned int h4 = 0, h3 = 0, h2 = 0;
        float f1 = 0.0;
        const std::string s1("205,199 23,445.25 1,024,365 123,22,24");
        T is(IOv2::mem_device{s1});
        is.locale(IOv2::locale<char>("C").involve(std::make_shared<test_numpunct1>()));

        // Basic operation.
        is >> h4;
        VERIFY(h4 == 205199);
        VERIFY(is.good());

        is.clear();
        is >> f1;
        VERIFY(f1 == 23445.25);
        VERIFY(is.good());

        is.clear();
        is >> h3;
        VERIFY(h3 == 1024365);
        VERIFY(is.good());

        is.clear();
        is >> h2;
        VERIFY(h2 == 1232224);
        VERIFY(is.rdstate() & IOv2::ios_defs::strfailbit);
        VERIFY(is.rdstate() & IOv2::ios_defs::eofbit);

        const std::string s2(",111 4,,4 0.25,345 5..25 156,, 1,000000 1000000 1234,567");
        h3 = h4 = h2 = 0;
        f1 = 0.0;
        const char c_control = '?';
        char c = c_control;
        is.clear();
        is.attach(IOv2::mem_device{s2});

        is >> h4;
        VERIFY(h4 == 0);
        VERIFY(is.rdstate() & IOv2::ios_defs::strfailbit);

        is.clear();
        is >> c;
        VERIFY(c == ',');
        VERIFY(is.good());
    
        is.ignore(3);
        is >> f1;
        VERIFY(f1 == 0.0);
        VERIFY(is.rdstate() & IOv2::ios_defs::strfailbit);
        is.clear();
        is >> c;
        VERIFY(c == ',');
        is >> c;
        VERIFY(c == '4');
        VERIFY(is.good());

        is >> f1; 
        VERIFY(f1 == 0.25);
        VERIFY(is.good());
        is >> c;
        VERIFY(c == ',');
        is >> h2;
        VERIFY(h2 == 345);
        VERIFY(is.good());

        f1 = 0.0;
        h2 = 0;
        is >> f1;
        VERIFY(f1 == 5.0);
        VERIFY(is.good());
        is >> f1;
        VERIFY(f1 == 0.25);
        VERIFY(is.good());

        is >> h3;
        VERIFY(h3 == 0);
        VERIFY(is.rdstate() & IOv2::ios_defs::strfailbit);
        is.clear();
        is >> c;
        VERIFY(c == ',');
        VERIFY(is.good());

        is >> h2;
        VERIFY(h2 == 1000000);
        VERIFY(is.rdstate() & IOv2::ios_defs::strfailbit);

        h2 = 0;
        is.clear();
        is >> h2;
        VERIFY(h2 == 1000000);
        VERIFY(is.good());
        h2 = 0;

        is >> h2; 
        VERIFY(h2 == 1234567);
        VERIFY(is.rdstate() & IOv2::ios_defs::strfailbit);
        VERIFY(is.rdstate() & IOv2::ios_defs::eofbit);
        is.clear();
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

namespace
{
    class test_numpunct2 : public IOv2::numeric_conf<char>
    {
    public:
        test_numpunct2()
            : IOv2::numeric_conf<char>("C")
            , m_group{2, 3}
        { }

        virtual const std::vector<uint8_t>& grouping() const override { return m_group; }
    private:
        std::vector<uint8_t> m_group;
    };
}

void test_istream_extractors_arithmetic_char_6()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 6...");
    auto helper = []<template<typename, typename> class T>()
    {
        // manufactured locale, grouping is turned on
        unsigned int h4 = 0, h3 = 0, h2 = 0;
        const std::string s1("1,22 205,19 22,123,22");
    
        T is(IOv2::mem_device{s1});
        is.locale(IOv2::locale<char>("C").involve(std::make_shared<test_numpunct2>()));

        // Basic operation.
        is >> h4;
        VERIFY(h4 == 122);
        VERIFY(is.good());

        is.clear();
        is >> h3;
        VERIFY(h3 == 20519);
        VERIFY(is.good());

        is.clear();
        is >> h2;
        VERIFY(h2 == 2212322);
        VERIFY(is.rdstate() & IOv2::ios_defs::eofbit);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_arithmetic_char_7()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 7...");
    auto helper = []<template<typename, typename> class T>()
    {
        std::string st("2.456e3-+0.567e-2");
        T is(IOv2::mem_device{st});

        double f1 = 0, f2 = 0;
        char c;

        (is >> IOv2::ws) >> f1;
        (is >> IOv2::ws) >> c;
        (is >> IOv2::ws) >> f2;

        VERIFY(f1 == 2456);
        // N.B. cast removes excess precision
        VERIFY(f2 == (double) 0.00567);
        VERIFY(c == '-');
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_arithmetic_char_8()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 8...");
    auto helper = []<template<typename, typename> class T>()
    {
        std::string str_01("0 00 000 +0 +0 -0");
        T is_01{IOv2::mem_device{str_01}};
        int n = 365;
        is_01 >> n;
        VERIFY(n == 0);
        n = 364;
        is_01 >> n;
        VERIFY(n == 0);
        n = 363;
        is_01 >> n;
        VERIFY(n == 0);
        n = 362;
        is_01 >> n;
        VERIFY(n == 0);
        n = 361;
        is_01 >> n;
        VERIFY(n == 0);
        n = 360;
        is_01 >> n;
        VERIFY(n == 0);
        VERIFY(is_01.rdstate() == IOv2::ios_defs::eofbit);

        std::string str_02("0x32 0X33 033 33");
        T is_02{IOv2::mem_device{str_02}};
        is_02.unsetf(IOv2::ios_defs::basefield);
        is_02 >> n;
        VERIFY(n == 50);
        is_02 >> n;
        VERIFY(n == 51);
        is_02 >> n;
        VERIFY(n == 27);
        is_02 >> n;
        VERIFY(n == 33);
        VERIFY(is_02.rdstate() == IOv2::ios_defs::eofbit);

        T is_03{IOv2::mem_device{str_02}};
        char c;
        int m;

        is_03 >> IOv2::dec >> n >> c >> m;
        VERIFY(n == 0);
        VERIFY(c == 'x');
        VERIFY(m == 32);

        is_03 >> IOv2::oct >> m >> c >> n;
        VERIFY(m == 0);
        VERIFY(c == 'X');
        VERIFY(n == 27);

        is_03 >> IOv2::dec >> m >> n;
        VERIFY(m == 33);
        VERIFY(n == 33);
        VERIFY(is_03.rdstate() == IOv2::ios_defs::eofbit);

        std::string str_04("3. 4.5E+2a5E-3 .6E1");
        T is_04{IOv2::mem_device{str_04}};
        double f;
        is_04 >> f;
        VERIFY(f == 3.0);
        is_04 >> f;
        VERIFY(f == 450.0);
        is_04.ignore();
        is_04 >> f;
        VERIFY(f == (double) 0.005);
        is_04 >> f;
        VERIFY(f == 6);
        VERIFY(is_04.rdstate() == IOv2::ios_defs::eofbit);

        std::string str_05("0E20 5Ea E16");
        T is_05{IOv2::mem_device{str_05}};
        is_05 >> f;
        VERIFY(f == 0);
        f = 1;
        is_05 >> f;
        VERIFY(f == 0);
        VERIFY(is_05.rdstate() == IOv2::ios_defs::strfailbit);
        is_05.clear();
        is_05 >> c;
        VERIFY(c == 'a');
        f = 1;
        is_05 >> f;
        VERIFY(f == 0);
        VERIFY(is_05.rdstate() == IOv2::ios_defs::strfailbit);
        is_05.clear();
        is_05.ignore();
        is_05 >> n;
        VERIFY(n == 16);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_arithmetic_char_9()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 9...");
    auto helper = []<template<typename, typename> class T>()
    {
        const char* cstrlit = "0x2a";
        // sanity check via 'C' library call
        char* err;
        long l = std::strtol(cstrlit, &err, 0);

        T iss(IOv2::mem_device{cstrlit});
        iss.setf(IOv2::ios_defs::fmtflags(0), IOv2::ios_defs::basefield);
        int i;
        iss >> i;
        VERIFY((bool)iss);
        VERIFY(l == i);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_arithmetic_char_10()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 10...");
    auto helper = []<template<typename, typename> class T, typename TV>(bool integer_type)
    {
        int digits_overflow;
        if (integer_type)
            // This many digits will overflow integer types in base 10.
            digits_overflow = std::numeric_limits<TV>::digits10 + 2;
        else
            // This might do it, unsure.
            digits_overflow = std::numeric_limits<TV>::max_exponent10 + 1;
  
        std::string st;
        std::string part = "1234567890123456789012345678901234567890";
        for (std::size_t i = 0; i < digits_overflow / part.size() + 1; ++i)
            st += part;
        T is(IOv2::mem_device{st});
        TV t;
        is >> t;
        VERIFY(is.str_fail());
    };

    helper.operator()<IOv2::istream, short>(true);
    helper.operator()<IOv2::istream, int>(true);
    helper.operator()<IOv2::istream, long>(true);
    helper.operator()<IOv2::istream, float>(false);
    helper.operator()<IOv2::istream, double>(false);
// TODO: comment out since this one shows different behavior with valgrind & release build, need to figure out the root cause
//    helper.operator()<IOv2::istream, long double>(false);

    helper.operator()<IOv2::iostream, short>(true);
    helper.operator()<IOv2::iostream, int>(true);
    helper.operator()<IOv2::iostream, long>(true);
    helper.operator()<IOv2::iostream, float>(false);
    helper.operator()<IOv2::iostream, double>(false);
// TODO: comment out since this one shows different behavior with valgrind & release build, need to figure out the root cause
//    helper.operator()<IOv2::iostream, long double>(false);

    dump_info("Done\n");
}

void test_istream_extractors_arithmetic_char_11()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 11...");
    auto helper = []<template<typename, typename> class T>()
    {
        const char* l2 = "1.2345678901234567890123456789012345678901234567890123456"
                    "  "
                    "1246.9";
        // 1 
        // used to core.
        double d;
        T iss1(IOv2::mem_device{l2});
        iss1 >> d;
        iss1 >> d;
        VERIFY(d > 1246 && d < 1247);

        // 2
        // quick test for failbit on maximum length extraction.
        int i;
        int max_digits = std::numeric_limits<int>::digits10 + 1;
        std::string digits;
        for (int j = 0; j < max_digits; ++j)
            digits += '1';
        T iss2(IOv2::mem_device{digits});
        iss2 >> i;
        VERIFY((bool)iss2);

        digits += '1';
        i = 0;
        iss2.attach(IOv2::mem_device{digits});
        iss2.clear();
        iss2 >> i;
        VERIFY(i == std::numeric_limits<int>::max());
        VERIFY(iss2.str_fail());
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

namespace
{
    struct dum_str{};
}
namespace IOv2
{
template <typename TChar>
struct reader<TChar, dum_str>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, dum_str& str)
    {
        throw 0;
    }
};
}

void test_istream_extractors_arithmetic_char_12()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 12...");
    auto helper = []<template<typename, typename> class T>()
    {
        T is{IOv2::mem_device{"hello"}};
        is.exceptions(IOv2::ios_defs::otherfailbit);

        try
        {
            dum_str arg;
            is >> arg;
            dump_info("unreachable code");
            std::abort();
        }
        catch(int)
        {
            VERIFY(!is);
        }
        catch(...)
        {
            VERIFY(false);
        }
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_arithmetic_char_13()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 13...");

    auto helper = []<template<typename, typename> class T>()
    {
        short s1 = 0;
        IOv2::ostream oss1{IOv2::mem_device{""}};
        oss1 << std::numeric_limits<short>::max();
        auto dev = oss1.detach(); dev.dseek(0);
        T iss1{std::move(dev)};
        iss1 >> s1;
        VERIFY(s1 == std::numeric_limits<short>::max());
        VERIFY((bool)iss1);
        VERIFY(iss1.eof());

        short s2 = 0;
        IOv2::ostream oss2{IOv2::mem_device{""}};
        oss2 << static_cast<long long>(std::numeric_limits<short>::max()) + 1;
        auto dev2 = oss2.detach(); dev2.dseek(0);
        T iss2{std::move(dev2)};
        iss2 >> s2;
        VERIFY(s2 == std::numeric_limits<short>::max());
        VERIFY(iss2.str_fail());
        VERIFY(iss2.eof());

        short s3 = 0;
        IOv2::ostream oss3{IOv2::mem_device{""}};
        oss3 << std::numeric_limits<short>::min();
        auto dev3 = oss3.detach(); dev3.dseek(0);
        T iss3{std::move(dev3)};
        iss3 >> s3;
        VERIFY(s3 == std::numeric_limits<short>::min());
        VERIFY((bool)iss3);
        VERIFY(iss3.eof());

        short s4 = 0;
        IOv2::ostream oss4{IOv2::mem_device{""}};
        oss4 << static_cast<long long>(std::numeric_limits<short>::min()) - 1;
        auto dev4 = oss4.detach(); dev4.dseek(0);
        T iss4{std::move(dev4)};
        iss4 >> s4;
        VERIFY(s4 == std::numeric_limits<short>::min());
        VERIFY(iss4.str_fail());
        VERIFY(iss4.eof());

        int i1 = 0;
        IOv2::ostream oss5{IOv2::mem_device{""}};
        oss5 << std::numeric_limits<int>::max();
        auto dev5 = oss5.detach(); dev5.dseek(0);
        T iss5{std::move(dev5)};
        iss5 >> i1;
        VERIFY(i1 == std::numeric_limits<int>::max());
        VERIFY((bool)iss5);
        VERIFY(iss5.eof());

        int i2 = 0;
        IOv2::ostream oss6{IOv2::mem_device{""}};
        oss6 << static_cast<long long>(std::numeric_limits<int>::max()) + 1;
        auto dev6 = oss6.detach(); dev6.dseek(0);
        T iss6{std::move(dev6)};
        iss6 >> i2;
        VERIFY(i2 == std::numeric_limits<int>::max());
        VERIFY(iss6.str_fail());
        VERIFY(iss6.eof());

        int i3 = 0;
        IOv2::ostream oss7{IOv2::mem_device{""}};
        oss7 << std::numeric_limits<int>::min();
        auto dev7 = oss7.detach(); dev7.dseek(0);
        T iss7{std::move(dev7)};
        iss7 >> i3;
        VERIFY(i3 == std::numeric_limits<int>::min());
        VERIFY((bool)iss7);
        VERIFY(iss7.eof());

        int i4 = 0;
        IOv2::ostream oss8{IOv2::mem_device{""}};
        oss8 << static_cast<long long>(std::numeric_limits<int>::min()) - 1;
        auto dev8 = oss8.detach(); dev8.dseek(0);
        T iss8{std::move(dev8)};
        iss8 >> i4;
        VERIFY(i4 == std::numeric_limits<int>::min());
        VERIFY(iss8.str_fail());
        VERIFY(iss8.eof());
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_istream_extractors_arithmetic_char_14()
{
    dump_info("Test istream<char> operator>> (arithmetic) case 13...");

    auto helper = []<template<typename, typename> class T>()
    {
        int i = 0;
        T iss{IOv2::mem_device{" 43"}};
        iss >> IOv2::noskipws >> i;
        VERIFY(!iss);
    };

    helper.operator()<IOv2::istream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

