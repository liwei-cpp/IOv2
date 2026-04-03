#include <cfloat>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>

namespace
{
    struct _TestCase
    {
      double val;
        
      int precision;
      int width;
      char decimal;
      char fill;

      bool fixed;
      bool scientific;
      bool showpos;
      bool showpoint;
      bool uppercase;
      bool internal;
      bool left;
      bool right;

      const wchar_t* result;
    };

    static bool T=true;
    static bool F=false;

    static _TestCase testcases[] =
    {
      // standard output (no formatting applied)
      { 1.2, 6,0,'.',' ', F,F,F,F,F,F,F,F, L"1.2" },
      { 54, 6,0,'.',' ', F,F,F,F,F,F,F,F, L"54" },
      { -.012, 6,0,'.',' ', F,F,F,F,F,F,F,F, L"-0.012" },
      { -.00000012, 6,0,'.',' ', F,F,F,F,F,F,F,F, L"-1.2e-07" },
        
      // fixed formatting
      { 10.2345, 0,0,'.',' ', T,F,F,F,F,F,F,F, L"10" },
      { 10.2345, 0,0,'.',' ', T,F,F,T,F,F,F,F, L"10." },
      { 10.2345, 1,0,'.',' ', T,F,F,F,F,F,F,F, L"10.2" },
      { 10.2345, 4,0,'.',' ', T,F,F,F,F,F,F,F, L"10.2345" },
      { 10.2345, 6,0,'.',' ', T,F,T,F,F,F,F,F, L"+10.234500" },
      { -10.2345, 6,0,'.',' ', T,F,F,F,F,F,F,F, L"-10.234500" },
      { -10.2345, 6,0,',',' ', T,F,F,F,F,F,F,F, L"-10,234500" },

      // fixed formatting with width
      { 10.2345, 4,5,'.',' ', T,F,F,F,F,F,F,F, L"10.2345" },
      { 10.2345, 4,6,'.',' ', T,F,F,F,F,F,F,F, L"10.2345" },
      { 10.2345, 4,7,'.',' ', T,F,F,F,F,F,F,F, L"10.2345" },
      { 10.2345, 4,8,'.',' ', T,F,F,F,F,F,F,F, L" 10.2345" },
      { 10.2345, 4,10,'.',' ', T,F,F,F,F,F,F,F, L"   10.2345" },
      { 10.2345, 4,10,'.',' ', T,F,F,F,F,F,T,F, L"10.2345   " },
      { 10.2345, 4,10,'.',' ', T,F,F,F,F,F,F,T, L"   10.2345" },
      { 10.2345, 4,10,'.',' ', T,F,F,F,F,T,F,F, L"   10.2345" },
      { -10.2345, 4,10,'.',' ', T,F,F,F,F,T,F,F, L"-  10.2345" },
      { -10.2345, 4,10,'.','A', T,F,F,F,F,T,F,F, L"-AA10.2345" },
      { 10.2345, 4,10,'.','#', T,F,T,F,F,T,F,F, L"+##10.2345" },

      // scientific formatting
      { 1.23e+12, 1,0,'.',' ', F,T,F,F,F,F,F,F, L"1.2e+12" },
      { 1.23e+12, 1,0,'.',' ', F,T,F,F,T,F,F,F, L"1.2E+12" },
      { 1.23e+12, 2,0,'.',' ', F,T,F,F,F,F,F,F, L"1.23e+12" },
      { 1.23e+12, 3,0,'.',' ', F,T,F,F,F,F,F,F, L"1.230e+12" },
      { 1.23e+12, 3,0,'.',' ', F,T,T,F,F,F,F,F, L"+1.230e+12" },
      { -1.23e-12, 3,0,'.',' ', F,T,F,F,F,F,F,F, L"-1.230e-12" },
      { 1.23e+12, 3,0,',',' ', F,T,F,F,F,F,F,F, L"1,230e+12" },
    };

    template<typename _CharT>
    class testpunct : public IOv2::numeric_conf<_CharT>
    {
    public:
        testpunct(_CharT decimal_char)
            : IOv2::numeric_conf<_CharT>("C")
            , dchar(decimal_char)
        { }

        virtual _CharT decimal_point() const override { return dchar; }           
        virtual _CharT thousands_sep() const override { return ','; }
        virtual const std::vector<uint8_t>& grouping() const override { return m_group; }
    private:
        std::vector<uint8_t> m_group;
        _CharT dchar;
    };
    
    template <typename _CharT, typename TOstream>  
    void apply_formatting(const _TestCase & tc, TOstream & os)
    {
        os.precision(tc.precision);
        os.width(tc.width);
        os.fill(static_cast<_CharT>(tc.fill));
        if (tc.fixed)
            os.setf(IOv2::ios_defs::fixed);
        if (tc.scientific)
            os.setf(IOv2::ios_defs::scientific);
        if (tc.showpos)
            os.setf(IOv2::ios_defs::showpos);
        if (tc.showpoint)
            os.setf(IOv2::ios_defs::showpoint);
        if (tc.uppercase)
            os.setf(IOv2::ios_defs::uppercase);
        if (tc.internal)
            os.setf(IOv2::ios_defs::internal);
        if (tc.left)
            os.setf(IOv2::ios_defs::left);
        if (tc.right)
            os.setf(IOv2::ios_defs::right);
    }
}

void test_ostream_inserters_arithmetic_wchar_t_1()
{
    dump_info("Test ostream<wchar_t> operator<< (arithmetic) case 1...");
    auto helper = []<template<typename, typename> class T>()
    {
        for (std::size_t j = 0; j<sizeof(testcases)/sizeof(testcases[0]); j++)
        {
            _TestCase & tc = testcases[j];
            IOv2::locale<wchar_t> loc = IOv2::locale<wchar_t>("C").involve(std::make_shared<testpunct<wchar_t>>(tc.decimal));
            // test double with wchar_t type
            {
                T os(IOv2::mem_device{L""}, loc);
                apply_formatting<wchar_t>(tc, os);
                os << tc.val;
                VERIFY(os.detach().str() == tc.result);
            }
            // test long double with wchar_t type
            {
                T os(IOv2::mem_device{L""}, loc);
                apply_formatting<wchar_t>(tc, os);
                os << (long double)tc.val;
                VERIFY(os.detach().str() == tc.result);
            }
        }
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_arithmetic_wchar_t_2()
{
    dump_info("Test ostream<wchar_t> operator<< (arithmetic) case 2...");

    auto helper = []<template<typename, typename> class T>()
    {
        double val2 = 3.5e230;
        T os2{IOv2::mem_device{L""}};
        os2.precision(3);
        os2.setf(IOv2::ios_defs::fixed);

        // Check it can be done in a locale with grouping on.
        IOv2::locale<wchar_t> loc2("de_DE.ISO-8859-1");
        os2.locale(loc2);
        os2 << IOv2::fixed << IOv2::setprecision(3) << val2 << IOv2::endl;
        os2 << IOv2::endl;
        os2 << IOv2::fixed << IOv2::setprecision(1) << val2 << IOv2::endl;
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_arithmetic_wchar_t_3()
{
    dump_info("Test ostream<wchar_t> operator<< (arithmetic) case 3...");
    auto helper = []<template <typename, typename> class TO, typename T>(T n)
    {
        TO o(IOv2::mem_device{L""});
        const wchar_t *expect;
        if (std::numeric_limits<T>::digits + 1 == 16)
            expect = L"177777 ffff";
        else if (std::numeric_limits<T>::digits + 1 == 32)
            expect = L"37777777777 ffffffff";
        else if (std::numeric_limits<T>::digits + 1 == 64)
            expect = L"1777777777777777777777 ffffffffffffffff";
        else
            expect = L"wow, you've got some big numbers here";

        o << IOv2::oct << n << ' ' << IOv2::hex << n;
        
        auto str = o.detach().str();
        VERIFY(str == expect);
    };

    helper.operator()<IOv2::ostream>(static_cast<short>(-1));
    helper.operator()<IOv2::ostream>(static_cast<int>(-1));
    helper.operator()<IOv2::ostream>(static_cast<long>(-1));

    helper.operator()<IOv2::iostream>(static_cast<short>(-1));
    helper.operator()<IOv2::iostream>(static_cast<int>(-1));
    helper.operator()<IOv2::iostream>(static_cast<long>(-1));

    dump_info("Done\n");
}

void test_ostream_inserters_arithmetic_wchar_t_4()
{
    dump_info("Test ostream<wchar_t> operator<< (arithmetic) case 4...");

    auto helper = []<template <typename, typename> class T>()
    {
        T o1(IOv2::mem_device{L""});
        T o2(IOv2::mem_device{L""});

        o1 << IOv2::hex << IOv2::showbase << IOv2::setw(6) << IOv2::internal << 0xff;
        VERIFY(o1.detach().str() == L"0x  ff");

        o2 << IOv2::hex << IOv2::showbase << IOv2::setw(6) << IOv2::internal << L"0xff";
        VERIFY(o2.detach().str() == L"  0xff");
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_arithmetic_wchar_t_5()
{
    dump_info("Test ostream<wchar_t> operator<< (arithmetic) case 5...");

    auto helper = []<template <typename, typename> class T>()
    {
        double pi = 3.14159265358979323846;
        T ostr(IOv2::mem_device{L""});
        ostr.precision(20);
        ostr << pi;
        std::wstring sval = ostr.detach().str();
        IOv2::istream istr(IOv2::mem_device{sval});
        double d;
        istr >> d;
        VERIFY( std::abs(pi-d)/pi < DBL_EPSILON );
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_arithmetic_wchar_t_6()
{
    dump_info("Test ostream<wchar_t> operator<< (arithmetic) case 6...");

    auto helper = []<template <typename, typename> class T>()
    {
        int prec = std::numeric_limits<double>::digits10 + 2;
        double oval = std::numeric_limits<double>::min();

        T ostr(IOv2::mem_device{L""});
        ostr.precision(prec);
        ostr << oval;
        auto sval = ostr.detach().str();
        IOv2::istream istr{IOv2::mem_device{sval}};
        double ival;
        istr >> ival;
        VERIFY( std::abs(oval-ival)/oval < DBL_EPSILON ); 
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_arithmetic_wchar_t_7()
{
    dump_info("Test ostream<wchar_t> operator<< (arithmetic) case 7...");

    auto helper = []<template <typename, typename> class T>()
    {
        T ostr1(IOv2::mem_device{L""});
        T ostr2(IOv2::mem_device{L""});
        T ostr3(IOv2::mem_device{L""});
        T ostr4(IOv2::mem_device{L""});

        ostr1.setf(IOv2::ios_defs::oct);
        ostr1.setf(IOv2::ios_defs::hex);
        short s = -1;
        ostr1 << s;
        VERIFY(ostr1.detach().str() == L"-1");

        ostr2.setf(IOv2::ios_defs::oct);
        ostr2.setf(IOv2::ios_defs::hex);
    
        int i = -1;
        ostr2 << i;
        VERIFY(ostr2.detach().str() == L"-1");

        ostr3.setf(IOv2::ios_defs::oct);
        ostr3.setf(IOv2::ios_defs::hex);

        long l = -1;
        ostr3 << l;
        VERIFY(ostr3.detach().str() == L"-1");

        ostr4.setf(IOv2::ios_defs::oct);
        ostr4.setf(IOv2::ios_defs::hex);

        long long ll = -1LL;
        ostr4 << ll;
        VERIFY(ostr4.detach().str() == L"-1");
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

namespace
{
class MyClass
{
    double x;

public:
    MyClass(double X) : x(X) {}
    friend bool operator&&(int i, const MyClass& Z);
};

inline bool operator&&(int i, const MyClass& Z)
{ return int(Z.x) == i; }
}

void test_ostream_inserters_arithmetic_wchar_t_8()
{
    dump_info("Test ostream<wchar_t> operator<< (arithmetic) case 8...");

    auto helper = []<template<typename, typename> class T>
    {
        int k =3;
        MyClass X(3.1);
        T oss(IOv2::mem_device{L""});
        oss << (k && X);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_arithmetic_wchar_t_9()
{
    dump_info("Test ostream<wchar_t> operator<< (arithmetic) case 9...");

    auto helper = []<template <typename, typename> class T>
    {
        // make sure we can output a very long float
        long double val = std::numeric_limits<long double>::max();
        int prec = std::numeric_limits<long double>::digits10;
    
        T os(IOv2::mem_device{L""});
        os.precision(prec);
        os.setf(IOv2::ios_defs::scientific);
        os << val;
    
        wchar_t largebuf[512];
        swprintf(largebuf, 512, L"%.*Le", prec, val);
        VERIFY(static_cast<bool>(os));
        VERIFY(os.detach().str() == largebuf);

        // Make sure we can output a long float in fixed format
        // without seg-faulting (libstdc++/4402)
        double val2 = std::numeric_limits<double>::max();
    
        T os2(IOv2::mem_device{L""});
        os2.precision(3);
        os2.setf(IOv2::ios_defs::fixed);
        os2 << val2;
    
        swprintf(largebuf, 512, L"%.*f", 3, val2);
        VERIFY(static_cast<bool>(os2));
        VERIFY(os2.detach().str() == largebuf);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_inserters_arithmetic_wchar_t_10()
{
    dump_info("Test ostream<wchar_t> operator<< (arithmetic) case 10...");

    auto helper = []<template<typename, typename> class TO, typename T>()
    {
        TO os{IOv2::mem_device{L""}};
        
        T d = 272.; // 0x1.1p+8;
        os << IOv2::hexfloat << IOv2::setprecision(1);
        os << d;

        VERIFY(static_cast<bool>(os));
        auto str = os.detach().str();
        VERIFY(std::stod(str) == d);
        VERIFY(str.substr(0, 2) == L"0x");
        VERIFY(str.find('p') != std::string::npos);

        os.attach(IOv2::mem_device{L""});
        os << IOv2::uppercase << d;
        os.flush();
        VERIFY(static_cast<bool>(os));
        VERIFY(std::stod(os.device().str()) == d);
        VERIFY(os.device().str().substr(0, 2) == L"0X");
        VERIFY(os.device().str().find('P') != std::string::npos);
    
        os << IOv2::nouppercase;
        os.attach(IOv2::mem_device{L""});
        os << IOv2::defaultfloat << IOv2::setprecision(6);
        os << d;
        os.flush();
        VERIFY(static_cast<bool>(os));
        VERIFY(os.device().str() == L"272");
    
        os.attach(IOv2::mem_device{L""});
        d = 15.; //0x1.ep+3;
        os << IOv2::hexfloat << IOv2::setprecision(1);
        os << d;
        VERIFY(static_cast<bool>(os));
        VERIFY(std::stod(os.detach().str()) == d);
    
        os.attach(IOv2::mem_device{L""});
        os << IOv2::uppercase << IOv2::setprecision(1);
        os << d;
        VERIFY(static_cast<bool>(os));
        VERIFY(std::stod(os.detach().str()) == d);
    
        os << IOv2::nouppercase;
        os.attach(IOv2::mem_device{L""});
        os << IOv2::defaultfloat << IOv2::setprecision(6);
        os << d;
        VERIFY(static_cast<bool>(os));
        VERIFY(os.detach().str() == L"15");
    };
    
    helper.template operator()<IOv2::ostream, double>();
    helper.template operator()<IOv2::ostream, long double>();
    helper.template operator()<IOv2::iostream, double>();
    helper.template operator()<IOv2::iostream, long double>();

    dump_info("Done\n");
}

void test_ostream_inserters_arithmetic_wchar_t_11()
{
    dump_info("Test ostream<wchar_t> operator<< (arithmetic) case 11...");

    auto helper = []<template<typename, typename> class T>()
    {
        float nan = std::numeric_limits<float>::quiet_NaN();
        T os(IOv2::mem_device{L""});
        os << -nan;
        VERIFY(os.detach().str()[0] == L'-');
    };

    helper.template operator()<IOv2::ostream>();
    helper.template operator()<IOv2::iostream>();

    dump_info("Done\n");
}