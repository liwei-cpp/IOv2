#include <sstream>
#include <facet/monetary.h>
#include <common/dump_info.h>

namespace
{
    class MoneyIO : public IOv2::monetary_conf<wchar_t>
    {
    public:
        MoneyIO(const std::string& n)
            : IOv2::monetary_conf<wchar_t>(n)
            , m_decimal_point(IOv2::monetary_conf<wchar_t>::decimal_point())
            , m_thousands_sep(IOv2::monetary_conf<wchar_t>::thousands_sep())
            , m_grouping(IOv2::monetary_conf<wchar_t>::grouping())
            , m_negative_sign_nat(IOv2::monetary_conf<wchar_t>::negative_sign_nat())
            , m_frac_digits_nat(IOv2::monetary_conf<wchar_t>::frac_digits_nat())
            , m_neg_format_nat(IOv2::monetary_conf<wchar_t>::neg_format_nat())
            , m_curr_sym_nat(IOv2::monetary_conf<wchar_t>::curr_symbol_nat())
            , m_pos_sign_nat(IOv2::monetary_conf<wchar_t>::positive_sign_nat())
        {}
        
        virtual wchar_t decimal_point() const override { return m_decimal_point; }
        void set_decimal_point(wchar_t ch) { m_decimal_point = ch; }
        
        virtual wchar_t thousands_sep() const override { return m_thousands_sep; }
        void set_thousands_sep(wchar_t ch) { m_thousands_sep = ch; }

        virtual const std::vector<uint8_t>& grouping() const override { return m_grouping; }
        void set_grouping(const std::vector<uint8_t>& g) { m_grouping = g; }

        virtual const std::wstring& negative_sign_nat() const override { return m_negative_sign_nat; }
        void set_negative_sign_nat(const std::wstring& i) { m_negative_sign_nat = i; }

        virtual int frac_digits_nat() const override { return m_frac_digits_nat; }
        void set_frac_digits_nat(int v) { m_frac_digits_nat = v; }
        
        virtual const IOv2::base_ft<IOv2::monetary>::pattern& neg_format_nat() const override { return m_neg_format_nat; }
        void set_neg_format_nat(const IOv2::base_ft<IOv2::monetary>::pattern& p) { m_neg_format_nat = p; }
        
        virtual const std::wstring& curr_symbol_nat() const override { return m_curr_sym_nat; }
        void set_curr_symbol_nat(const std::wstring& s) { m_curr_sym_nat = s; }

        virtual const std::basic_string<wchar_t>& positive_sign_nat() const override { return m_pos_sign_nat; }
        void set_positive_sign_nat(const std::wstring& s) { m_pos_sign_nat = s; }
        
    private:
        wchar_t m_decimal_point;
        wchar_t m_thousands_sep;
        std::vector<uint8_t> m_grouping;
        std::wstring m_negative_sign_nat;
        int m_frac_digits_nat;
        IOv2::base_ft<IOv2::monetary>::pattern m_neg_format_nat;
        std::wstring m_curr_sym_nat;
        std::wstring m_pos_sign_nat;
    };
}

void test_monetary_wchar_t_common_1()
{
    dump_info("Test monetary<wchar_t> common 1...");
    static_assert(std::is_same_v<IOv2::monetary<wchar_t>::char_type, wchar_t>);

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("C"));
    
    if (obj.decimal_point() != '.') throw std::runtime_error("monetary<wchar_t>::decimal_point fail");
    if (obj.thousands_sep() != ',') throw std::runtime_error("monetary<wchar_t>::thousands_sep fail");
    if (!obj.grouping().empty()) throw std::runtime_error("monetary<wchar_t>::grouping fail");
    if (!obj.curr_symbol_nat().empty()) throw std::runtime_error("monetary<wchar_t>::curr_symbol_nat fail");
    if (!obj.curr_symbol_int().empty()) throw std::runtime_error("monetary<wchar_t>::curr_symbol_int fail");
    if (!obj.positive_sign_nat().empty()) throw std::runtime_error("monetary<wchar_t>::positive_sign_nat fail");
    if (!obj.positive_sign_int().empty()) throw std::runtime_error("monetary<wchar_t>::positive_sign_int fail");
    if (obj.negative_sign_nat().empty()) throw std::runtime_error("monetary<wchar_t>::negative_sign_nat fail");
    if (obj.negative_sign_int().empty()) throw std::runtime_error("monetary<wchar_t>::negative_sign_int fail");
    if (obj.frac_digits_int() != 0) throw std::runtime_error("monetary<wchar_t>::frac_digits_int fail");
    if (obj.frac_digits_nat() != 0) throw std::runtime_error("monetary<wchar_t>::frac_digits_nat fail");
    if (obj.pos_format_int() != obj.pos_format_nat()) throw std::runtime_error("monetary<wchar_t>::pos_format fail");
    if (obj.neg_format_int() != obj.neg_format_nat()) throw std::runtime_error("monetary<wchar_t>::neg_format fail");

    dump_info("Done\n");
}

void test_monetary_wchar_t_common_2()
{
    dump_info("Test monetary<wchar_t> common 2...");
    IOv2::monetary obj_c(std::make_shared<IOv2::monetary_conf<wchar_t>>("C"));
    IOv2::monetary obj_de(std::make_shared<IOv2::monetary_conf<wchar_t>>("de_DE.ISO-8859-1"));
    
    if (obj_c.decimal_point() == char()) throw std::runtime_error("monetary<wchar_t>::decimal_point fail");
    if (obj_c.thousands_sep() == char()) throw std::runtime_error("monetary<wchar_t>::thousands_sep fail");
    if (obj_c.decimal_point() == obj_de.decimal_point()) throw std::runtime_error("monetary<wchar_t>::decimal_point fail");
    if (obj_c.thousands_sep() == obj_de.thousands_sep()) throw std::runtime_error("monetary<wchar_t>::thousands_sep fail");
    if (obj_c.grouping() == obj_de.grouping()) throw std::runtime_error("monetary<wchar_t>::grouping fail");
    if (obj_c.curr_symbol_int() == obj_de.curr_symbol_int()) throw std::runtime_error("monetary<wchar_t>::curr_symbol_int fail");
    if (obj_c.negative_sign_int() != obj_de.negative_sign_int()) throw std::runtime_error("monetary<wchar_t>::negative_sign_int fail");
    if (obj_c.frac_digits_int() == obj_de.frac_digits_int()) throw std::runtime_error("monetary<wchar_t>::frac_digits_int fail");
    if (obj_c.pos_format_int() == obj_de.pos_format_int()) throw std::runtime_error("monetary<wchar_t>::pos_format_int fail");

    dump_info("Done\n");
}

void test_monetary_wchar_t_put_1()
{
    dump_info("Test monetary<wchar_t>::put 1...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        // total EPA budget FY 2002
        const std::wstring digits1(L"720000000000");
        // input less than frac_digits
        const std::wstring digits2(L"-1");
        std::wstring oss;

        obj.put(std::back_inserter(oss), true, ios, digits1);
        if (oss != L"7.200.000.000,00 ") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");

        oss.clear();
        obj.put(std::back_inserter(oss), false, ios, digits1);
        if (oss != L"7.200.000.000,00 ") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
    
        // now try with showbase, to get currency symbol in format
        ios.setf(IOv2::ios_defs::showbase);
        
        oss.clear();
        obj.put(std::back_inserter(oss), true, ios, digits1);
        if (oss != L"7.200.000.000,00 EUR ") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
        
        oss.clear();
        obj.put(std::back_inserter(oss), false, ios, digits1);
        if (oss != L"7.200.000.000,00 €") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");

        ios.unsetf(IOv2::ios_defs::showbase);
    
        // test io.width() > length
        // test various fill strategies
        ios.width(20); ios.fill('*');
        oss.clear();
        obj.put(std::back_inserter(oss), true, ios, digits2);
        if (oss != L"***************-,01*") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
        
        ios.width(20); ios.setf(IOv2::ios_defs::internal);
        oss.clear();
        obj.put(std::back_inserter(oss), true, ios, digits2);
        if (oss != L"-,01****************") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_put_2()
{
    dump_info("Test monetary<wchar_t>::put 2...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj, const IOv2::monetary<wchar_t>& obj_c)
    {
        IOv2::ios_base<wchar_t> ios;

        // total EPA budget FY 2002
        const std::wstring digits1(L"720000000000");
        
        // est. cost, national missile "defense", expressed as a loss in USD 2001
        const std::wstring digits2(L"-10000000000000");  
        
        // not valid input
        const std::wstring digits3(L"-A"); 
        
        // input less than frac_digits
        const std::wstring digits4(L"-1");
    
        // cache the money_put facet
        std::wstring oss;
    
        // now try with showbase, to get currency symbol in format
        ios.setf(IOv2::ios_defs::showbase);
        
        // test sign of more than one digit, say hong kong.
        oss.clear();
        obj.put(std::back_inserter(oss), false, ios, digits1);
        if (oss != L"HK$7,200,000,000.00") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
        
        oss.clear();
        obj.put(std::back_inserter(oss), true, ios, digits2);
        if (oss != L"(HKD 100,000,000,000.00)") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
        
        
        // test one-digit formats without zero padding
        // note: the result is different with libstdc++'s test case (libstdc%2B%2B-v3/testsuite/22_locale/money_put/put/char/2.cc)
        // since IOv2 set '-' as the negative sign of C locale.
        oss.clear();
        obj_c.put(std::back_inserter(oss), true, ios, digits4);
        if (oss != L"-1") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
    
        // test one-digit formats with zero padding, zero frac widths
        oss.clear();
        obj.put(std::back_inserter(oss), true, ios, digits4);
        if (oss != L"(HKD .01)") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
        
        ios.unsetf(IOv2::ios_defs::showbase);
    
        // test bunk input
        oss.clear();
        obj.put(std::back_inserter(oss), true, ios, digits3);
        if (oss != L"") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("en_HK.UTF-8"));
    IOv2::monetary obj_c(std::make_shared<IOv2::monetary_conf<wchar_t>>("C"));
    helper(obj, obj_c);

    dump_info("Done\n");
}

void test_monetary_wchar_t_put_3()
{
    dump_info("Test monetary<wchar_t>::put 3...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        // woman, art, thief (stole the blues)
        const std::wstring str(L"1943 Janis Joplin");
        const int64_t ld = 1943;
        const std::wstring x(str.size(), L'x'); // have to have allocated string!
        std::wstring res;

        std::wstring oss;
    
        // 01 string
        res = x;
        auto ret1 = obj.put(res.begin(), false, ios, str);
        std::wstring sanity1(res.begin(), ret1);
        if (res != L"1943xxxxxxxxxxxxx") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
        if (sanity1 != L"1943") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");

        // 02 int64_t
        res = x;
        auto ret2 = obj.put(res.begin(), false, ios, ld);
        std::wstring sanity2(res.begin(), ret2);
        if (res != L"1943xxxxxxxxxxxxx") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
        if (sanity2 != L"1943") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("C"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_put_4()
{
    dump_info("Test monetary<wchar_t>::put 4...");
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_decimal_point(L'.');
    tmp_io->set_thousands_sep(L',');
    tmp_io->set_grouping({3});
    tmp_io->set_negative_sign_nat(L"()");
    tmp_io->set_frac_digits_nat(2);
    tmp_io->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::symbol,
                                IOv2::base_ft<IOv2::monetary>::space,
                                IOv2::base_ft<IOv2::monetary>::sign,
                                IOv2::base_ft<IOv2::monetary>::value});

    IOv2::monetary<wchar_t> obj(tmp_io);
    IOv2::ios_base<wchar_t> ios;
    ios.fill(L'*');

    std::wstring val(L"-123456");

    std::wstring fmt;
    obj.put(std::back_inserter(fmt), false, ios, val);
    if (fmt != L"*(1,234.56)") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
  
    dump_info("Done\n");
}

void test_monetary_wchar_t_put_5()
{
    dump_info("Test monetary<wchar_t>::put 5...");
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_thousands_sep(',');
    tmp_io->set_grouping({1});

    IOv2::monetary<wchar_t> obj(tmp_io);
    IOv2::ios_base<wchar_t> ios;
    ios.fill(L'*');

    long double val = 1.0e50L;

    std::wostringstream fmt;
    std::ostreambuf_iterator<wchar_t> out(fmt);
    obj.put(out, false, ios, (int64_t)val);
    if (!fmt.good()) throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
  
    dump_info("Done\n");
}

void test_monetary_wchar_t_put_6()
{
    dump_info("Test monetary<wchar_t>::put 6...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        int64_t amount = 11;
        
        // cache the money_put facet
        std::wstring oss;
        obj.put(std::back_inserter(oss), true, ios, amount);
        if (oss != L"11") throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("C"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_put_7()
{
    dump_info("Test monetary<wchar_t>::put 7...");
    
    IOv2::ios_base<wchar_t> ios;
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_grouping({CHAR_MAX});
    IOv2::monetary<wchar_t> obj(tmp_io);
    
    std::wstring digits(300, L'1');
    
    std::wstring oss;
    obj.put(std::back_inserter(oss), false, ios, digits);
    if (oss != digits) throw std::runtime_error("IOv2::monetary<wchar_t>::put fails");
    
    dump_info("Done\n");
}

void test_monetary_wchar_t_get_1()
{
    dump_info("Test monetary<wchar_t>::get 1...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        // total EPA budget FY 2002
        const std::wstring digits1(L"720000000000");

        std::wstring iss;
    
        {
            iss = L"7.200.000.000,00 ";
            std::wstring result1;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result1);
            if (result1 != digits1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }

        {
            iss = L"7.200.000.000,00  ";
            std::wstring result2;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result2);
            if (result2 != digits1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }

        {
            iss = L"7.200.000.000,00  a";
            std::wstring result3;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result3);
            if (result3 != digits1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (*it != 'a') throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }

        {
            iss = L"";
            std::wstring result4;
            try
            {
                obj.get(iss.begin(), iss.end(), true, ios, result4);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (result4 != L"") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
    
        {
            iss = L"working for enlightenment and peace in a mad world";
            std::wstring result5;
            try
            {
                obj.get(iss.begin(), iss.end(), true, ios, result5);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (result5 != L"") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }

        // now try with showbase, to get currency symbol in format
        ios.setf(IOv2::ios_defs::showbase);
    
        {
            iss = L"7.200.000.000,00 EUR ";
            std::wstring result6;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result6);
            if (result6 != digits1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }

        {
            iss = L"7.200.000.000,00 EUR  ";
            std::wstring result7;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result7);
            if (result7 != digits1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }

        {
            iss = L"7.200.000.000,00 \x20ac";
            std::wstring result8;
            auto it = obj.get(iss.begin(), iss.end(), false, ios, result8);
            if (result8 != digits1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_2()
{
    dump_info("Test monetary<wchar_t>::get 2...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        // total EPA budget FY 2002
        const std::wstring digits1(L"720000000000");

        // est. cost, national missile "defense", expressed as a loss in USD 2001
        const std::wstring digits2(L"-10000000000000");  

        // input less than frac_digits
        const std::wstring digits4(L"-1");
    
        std::wstring iss;
        
        // now try with showbase, to get currency symbol in format
        ios.setf(IOv2::ios_defs::showbase);
        
        {
            iss = L"HK$7,200,000,000.00";
            std::wstring result9;
            auto it = obj.get(iss.begin(), iss.end(), false, ios, result9);
            if (result9 != digits1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
        {
            iss = L"(HKD 100,000,000,000.00)";
            std::wstring result10;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result10);
            if (result10 != digits2) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
        {
            iss = L"(HKD .01)";
            std::wstring result11;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result11);
            if (result11 != digits4) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
        
        // for the "en_HK.ISO8859-1" locale the parsing of the very same input streams must
        // be successful without showbase too, since the symbol field appears in
        // the first positions in the format and the symbol, when present, must be
        // consumed.
        ios.unsetf(IOv2::ios_defs::showbase);
        {
            iss = L"HK$7,200,000,000.00";
            std::wstring result12;
            auto it = obj.get(iss.begin(), iss.end(), false, ios, result12);
            if (result12 != digits1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
        {
            iss = L"(HKD 100,000,000,000.00)";
            std::wstring result13;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result13);
            if (result13 != digits2) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
        {
            iss = L"(HKD .01)";
            std::wstring result14;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result14);
            if (result14 != digits4) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("en_HK.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_3()
{
    dump_info("Test monetary<wchar_t>::get 3...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        // total EPA budget FY 2002
        const long double  digits1 = 720000000000.0;

        std::wstring iss;
        {
            iss = L"7.200.000.000,00 ";
            int64_t result1;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result1);
            if (result1 != digits1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
        {
            iss = L"7.200.000.000,00 ";
            int64_t result2;
            auto it = obj.get(iss.begin(), iss.end(), false, ios, result2);
            if (result2 != digits1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_4()
{
    dump_info("Test monetary<wchar_t>::get 4...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        // input less than frac_digits
        const long double digits4 = -1.0;
        std::wstring iss;

        // now try with showbase, to get currency symbol in format
        ios.setf(IOv2::ios_defs::showbase);
        
        iss = L"(HKD .01)";
        int64_t result3;
        auto it = obj.get(iss.begin(), iss.end(), true, ios, result3);
        if (result3 != digits4) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("en_HK.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_5()
{
    dump_info("Test monetary<wchar_t>::get 5...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        const std::wstring str = L"1Eleanor Roosevelt";
        
        {
            // 01 string
            std::wstring res1;
            auto it = obj.get(str.begin(), str.end(), false, ios, res1);
            if (it == str.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            std::wstring rem1(it, str.end());
            if (res1 != L"1") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (rem1 != L"Eleanor Roosevelt") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
    
        {
            // 02 int64_t
            int64_t res2;
            auto it = obj.get(str.begin(), str.end(), false, ios, res2);
            if (it == str.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            std::wstring rem2(it, str.end());
            if (res2 != 1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (rem2 != L"Eleanor Roosevelt") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("C"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_6()
{
    dump_info("Test monetary<wchar_t>::get 6...");
    IOv2::ios_base<wchar_t> ios;
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_decimal_point(L'.');
    tmp_io->set_grouping({4});
    tmp_io->set_curr_symbol_nat(L"$");
    tmp_io->set_positive_sign_nat(L"");
    tmp_io->set_negative_sign_nat(L"-");
    tmp_io->set_frac_digits_nat(2);
    tmp_io->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::symbol,
                                IOv2::base_ft<IOv2::monetary>::none,
                                IOv2::base_ft<IOv2::monetary>::sign,
                                IOv2::base_ft<IOv2::monetary>::value});

    IOv2::monetary<wchar_t> obj(tmp_io);
    
    std::wstring bufferp(L"$1234.56");
    std::wstring buffern(L"$-1234.56");
    std::wstring bufferp_ns(L"1234.56");
    std::wstring buffern_ns(L"-1234.56");
    
    {
        std::wstring valp;
        obj.get(bufferp.begin(), bufferp.end(), false, ios, valp);
        if (valp != L"123456") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }
    {
        std::wstring valn;
        obj.get(buffern.begin(), buffern.end(), false, ios, valn);
        if (valn != L"-123456") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }
    {
        std::wstring valp_ns;
        obj.get(bufferp_ns.begin(), bufferp_ns.end(), false, ios, valp_ns);
        if (valp_ns != L"123456") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }
    {
        std::wstring valn_ns;
        obj.get(buffern_ns.begin(), buffern_ns.end(), false, ios, valn_ns);
        if (valn_ns != L"-123456") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }
  
    dump_info("Done\n");
}

void test_monetary_wchar_t_get_7()
{
    dump_info("Test monetary<wchar_t>::get 7...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        std::wstring buffer1(L"123");
        std::wstring buffer2(L"456");
        std::wstring buffer3(L"Golgafrincham"); // From Nathan's original idea.

        std::wstring val;

        {
            obj.get(buffer1.begin(), buffer1.end(), false, ios, val);
            if (val != buffer1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
        {
            obj.get(buffer2.begin(), buffer2.end(), false, ios, val);
            if (val != buffer2) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
        {
            val = buffer3;
            try
            {
                obj.get(buffer3.begin(), buffer3.end(), false, ios, val);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (val != buffer3) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("C"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_8()
{
    dump_info("Test monetary<wchar_t>::get 8...");
    
    IOv2::ios_base<wchar_t> ios;
    
    auto tmp_io_a = std::make_shared<MoneyIO>("C");
    tmp_io_a->set_decimal_point(L'.');
    tmp_io_a->set_grouping({4});
    tmp_io_a->set_curr_symbol_nat(L"$");
    tmp_io_a->set_positive_sign_nat(L"()");
    tmp_io_a->set_frac_digits_nat(2);
    tmp_io_a->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::sign,
                                  IOv2::base_ft<IOv2::monetary>::value,
                                  IOv2::base_ft<IOv2::monetary>::space,
                                  IOv2::base_ft<IOv2::monetary>::symbol});

    auto tmp_io_b = std::make_shared<MoneyIO>("C");
    tmp_io_b->set_decimal_point(L'.');
    tmp_io_b->set_grouping({4});
    tmp_io_b->set_curr_symbol_nat(L"$");
    tmp_io_b->set_positive_sign_nat(L"()");
    tmp_io_b->set_frac_digits_nat(2);
    tmp_io_b->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::sign,
                                  IOv2::base_ft<IOv2::monetary>::value,
                                  IOv2::base_ft<IOv2::monetary>::symbol,
                                  IOv2::base_ft<IOv2::monetary>::none});
                                
    IOv2::monetary<wchar_t> obj_a(tmp_io_a);
    IOv2::monetary<wchar_t> obj_b(tmp_io_b);
    
    std::wstring buffer_a(L"(1234.56 $)");
    std::wstring buffer_a_ns(L"(1234.56 )");

    std::wstring val_a, val_a_ns;
    {
        obj_a.get(buffer_a.begin(), buffer_a.end(), false, ios, val_a);
        if (val_a != L"123456") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }
    {
        obj_a.get(buffer_a_ns.begin(), buffer_a_ns.end(), false, ios, val_a_ns);
        if (val_a_ns != L"123456") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }

    std::wstring buffer_b(L"(1234.56$)");
    std::wstring buffer_b_ns(L"(1234.56)");

    std::wstring val_b, val_b_ns;
    {
        obj_b.get(buffer_b.begin(), buffer_b.end(), false, ios, val_b);
        if (val_b != L"123456") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }
    {
        obj_b.get(buffer_b_ns.begin(), buffer_b_ns.end(), false, ios, val_b_ns);
        if (val_b_ns != L"123456") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_9()
{
    dump_info("Test monetary<wchar_t>::get 9...");
    
    IOv2::ios_base<wchar_t> ios;
    
    auto dublin = std::make_shared<MoneyIO>("C");
    dublin->set_frac_digits_nat(3);

    IOv2::monetary<wchar_t> obj(dublin);
    std::wstring liffey;
    std::wstring coins;

    {
        // Feed it 1 digit too many, which should fail.
        liffey = L"12.3456";
        try
        {
            obj.get(liffey.begin(), liffey.end(), false, ios, coins);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }
    {
        // Feed it exactly what it wants, which should succeed.
        liffey = L"12.345";
        obj.get(liffey.begin(), liffey.end(), false, ios, coins);
    }
    {
        // Feed it 1 digit too few, which should fail.
        liffey = L"12.34";
        try
        {
            obj.get(liffey.begin(), liffey.end(), false, ios, coins);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }
    {
        // Feed it only a decimal-point, which should fail.
        liffey = L"12.";
        try
        {
            obj.get(liffey.begin(), liffey.end(), false, ios, coins);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }
    {
        // Feed it no decimal-point at all, which should succeed.
        liffey = L"12";
        obj.get(liffey.begin(), liffey.end(), false, ios, coins);
    }
    dump_info("Done\n");
}

void test_monetary_wchar_t_get_10()
{
    dump_info("Test monetary<wchar_t>::get 10...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;
        std::wstring iss;
        std::wstring extracted_amount;
        {
            iss = L"-$0 ";
            auto it = obj.get(iss.begin(), iss.end(), false, ios, extracted_amount);
            if (it == iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (extracted_amount != L"0") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            
        }
        {
            extracted_amount.clear();
            iss = L"-$ ";
            try
            {
                obj.get(iss.begin(), iss.end(), false, ios, extracted_amount);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (!extracted_amount.empty()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("en_US.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_11()
{
    dump_info("Test monetary<wchar_t>::get 11...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        // A _very_ big amount.
        std::wstring str = L"1";
        for (int i = 0; i < 2 * std::numeric_limits<int64_t>::digits10; ++i)
            str += L".000";
        str += L",00 ";

        try
        {
            int64_t result1;
            obj.get(str.begin(), str.end(), true, ios, result1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_12()
{
    dump_info("Test monetary<wchar_t>::get 12...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        // total EPA budget FY 2002
        const long double  digits1 = 720000000000.0;
        
        std::wstring iss;
        
        {
            iss = L"7200000000,00 ";
            int64_t result1;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result1);
            if (result1 != digits1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
        {
            iss = L"7200000000,00 ";
            int64_t result2;
            auto it = obj.get(iss.begin(), iss.end(), false, ios, result2);
            if (result2 != digits1) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_13()
{
    dump_info("Test monetary<wchar_t>::get 13...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        std::wstring iss;
        {
            iss = L"500,1.0 ";
            int64_t result1;
            try
            {
                obj.get(iss.begin(), iss.end(), true, ios, result1);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
        }
        {
            iss = L"500,1.0 ";
            int64_t result2;
            try
            {
                obj.get(iss.begin(), iss.end(), false, ios, result2);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_14()
{
    dump_info("Test monetary<wchar_t>::get 14...");
    IOv2::ios_base<wchar_t> ios;
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_positive_sign_nat(L"+");
    tmp_io->set_negative_sign_nat(L"");
    
    IOv2::monetary<wchar_t> obj(tmp_io);

    std::wstring buffer(L"69");
    std::wstring val;
    
    obj.get(buffer.begin(), buffer.end(), false, ios, val);
    if (val != L"-69") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_15()
{
    dump_info("Test monetary<wchar_t>::get 15...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        std::wstring iss;
        {
            iss = L".100";
            int64_t result1;
            try
            {
                obj.get(iss.begin(), iss.end(), true, ios, result1);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
        }
        {
            iss = L"30..0";
            int64_t result1;
            try
            {
                obj.get(iss.begin(), iss.end(), false, ios, result1);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_16()
{
    dump_info("Test monetary<wchar_t>::get 16...");
    IOv2::ios_base<wchar_t> ios1, ios2;
    
    IOv2::monetary obj_de(std::make_shared<IOv2::monetary_conf<wchar_t>>("de_DE.UTF-8"));
    IOv2::monetary obj_hk(std::make_shared<IOv2::monetary_conf<wchar_t>>("en_HK.UTF-8"));

    {
        ios1.setf(IOv2::ios_defs::showbase);
        std::wstring iss01 = L"EUR ";
        int64_t result1;
        try
        {
            obj_de.get(iss01.begin(), iss01.end(), true, ios1, result1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }
    {
        std::wstring iss02 = L"(HKD )";
        int64_t result2;
        try
        {
            obj_hk.get(iss02.begin(), iss02.end(), true, ios2, result2);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    }

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_17()
{
    dump_info("Test monetary<wchar_t>::get 17...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        std::wstring iss;
        {
            iss = L"7.200.000.000,00";
            std::wstring result1;
            try
            {
                obj.get(iss.begin(), iss.end(), true, ios, result1);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
        }
        
        // now try with showbase, to get currency symbol in format
        {
            ios.setf(IOv2::ios_defs::showbase);
            iss = L"7.200.000.000,00EUR ";
            std::wstring result2;
            try
            {
                obj.get(iss.begin(), iss.end(), true, ios, result2);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_18()
{
    dump_info("Test monetary<wchar_t>::get 18...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        std::wstring iss;
        {
            iss = L"HK7,200,000,000.00";
            std::wstring result1;
            try
            {
                obj.get(iss.begin(), iss.end(), false, ios, result1);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
        }
        {
            iss = L"(HK100,000,000,000.00)";
            std::wstring result1;
            try
            {
                obj.get(iss.begin(), iss.end(), false, ios, result1);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("en_HK.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_19()
{
    dump_info("Test monetary<wchar_t>::get 19...");
    
    IOv2::ios_base<wchar_t> ios;
    
    auto tmp_io_a = std::make_shared<MoneyIO>("C");
    tmp_io_a->set_curr_symbol_nat(L"$");
    tmp_io_a->set_positive_sign_nat(L"");
    tmp_io_a->set_negative_sign_nat(L"");
    tmp_io_a->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::value,
                                  IOv2::base_ft<IOv2::monetary>::symbol,
                                  IOv2::base_ft<IOv2::monetary>::none,
                                  IOv2::base_ft<IOv2::monetary>::sign});

    auto tmp_io_b = std::make_shared<MoneyIO>("C");
    tmp_io_b->set_curr_symbol_nat(L"%");
    tmp_io_b->set_positive_sign_nat(L"");
    tmp_io_b->set_negative_sign_nat(L"-");
    tmp_io_b->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::value,
                                  IOv2::base_ft<IOv2::monetary>::symbol,
                                  IOv2::base_ft<IOv2::monetary>::sign,
                                  IOv2::base_ft<IOv2::monetary>::none});

    auto tmp_io_c = std::make_shared<MoneyIO>("C");
    tmp_io_c->set_curr_symbol_nat(L"&");
    tmp_io_c->set_positive_sign_nat(L"");
    tmp_io_c->set_negative_sign_nat(L"");
    tmp_io_c->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::value,
                                  IOv2::base_ft<IOv2::monetary>::space,
                                  IOv2::base_ft<IOv2::monetary>::symbol,
                                  IOv2::base_ft<IOv2::monetary>::sign});
                                
    IOv2::monetary<wchar_t> obj_a(tmp_io_a);
    IOv2::monetary<wchar_t> obj_b(tmp_io_b);
    IOv2::monetary<wchar_t> obj_c(tmp_io_c);
    
    {
        std::wstring iss_01 = L"10$";
        std::wstring result01;
        auto it = obj_a.get(iss_01.begin(), iss_01.end(), false, ios, result01);
        if (it == iss_01.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        if (*it != '$') throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }
    {
        std::wstring iss_02 = L"50%";
        std::wstring result02;
        auto it = obj_a.get(iss_02.begin(), iss_02.end(), false, ios, result02);
        if (it == iss_02.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        if (*it != '%') throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }
    {
        std::wstring iss_03 = L"7 &";
        std::wstring result03;
        auto it = obj_a.get(iss_03.begin(), iss_03.end(), false, ios, result03);
        if (it == iss_03.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        if (*it != '&') throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_20()
{
    dump_info("Test monetary<wchar_t>::get 20...");

    auto helper = [](const IOv2::monetary<wchar_t>& obj)
    {
        IOv2::ios_base<wchar_t> ios;

        std::wstring iss = L"$.00 ";
        std::wstring extracted_amount;
        auto it = obj.get(iss.begin(), iss.end(), false, ios, extracted_amount);
        if (it == iss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        if (*it != ' ') throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
        if (extracted_amount != L"0") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<wchar_t>>("en_US.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_21()
{
    dump_info("Test monetary<wchar_t>::get 21...");
    IOv2::ios_base<wchar_t> ios;
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_grouping({1});
    tmp_io->set_thousands_sep(L'#');
    tmp_io->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::symbol,
                                IOv2::base_ft<IOv2::monetary>::none,
                                IOv2::base_ft<IOv2::monetary>::sign,
                                IOv2::base_ft<IOv2::monetary>::value});
                                  
    IOv2::monetary<wchar_t> obj(tmp_io);
    std::wstring buffer1(L"00#0#1");
    std::wstring buffer2(L"000##1");
    std::wstring val1, val2;
    
    {
        try
        {
            obj.get(buffer1.begin(), buffer1.end(), false, ios, val1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (val1 != L"1") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }
    {
        try
        {
            obj.get(buffer2.begin(), buffer2.end(), false, ios, val2);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (val2 != L"") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    }

    dump_info("Done\n");
}

void test_monetary_wchar_t_get_22()
{
    dump_info("Test monetary<wchar_t>::get 22...");
    IOv2::ios_base<wchar_t> ios;
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_frac_digits_nat(0);
    IOv2::monetary<wchar_t> obj(tmp_io);
    
    std::wstring ss = L"123.455";
    std::wstring digits;
    
    auto it = obj.get(ss.begin(), ss.end(), false, ios, digits);
    std::wstring rest = std::wstring(it, ss.end());
    if (digits != L"123") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    if (rest != L".455") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
  
    dump_info("Done\n");
}

void test_monetary_wchar_t_get_23()
{
    dump_info("Test monetary<wchar_t>::get 23...");
    IOv2::ios_base<wchar_t> ios;
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_grouping({CHAR_MAX});
    IOv2::monetary<wchar_t> obj(tmp_io);
    
    std::wstring ss = L"123,456";
    std::wstring digits;
    
    auto it = obj.get(ss.begin(), ss.end(), false, ios, digits);
    if (it == ss.end()) throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    if (digits != L"123") throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");
    if (*it != L',') throw std::runtime_error("IOv2::monetary<wchar_t>::get fails");

    dump_info("Done\n");
}