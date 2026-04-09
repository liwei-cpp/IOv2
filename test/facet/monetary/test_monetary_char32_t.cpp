#include <sstream>
#include <facet/monetary.h>
#include <common/dump_info.h>

namespace
{
    class MoneyIO : public IOv2::monetary_conf<char32_t>
    {
    public:
        MoneyIO(const std::string& n)
            : IOv2::monetary_conf<char32_t>(n)
            , m_decimal_point(IOv2::monetary_conf<char32_t>::decimal_point())
            , m_thousands_sep(IOv2::monetary_conf<char32_t>::thousands_sep())
            , m_grouping(IOv2::monetary_conf<char32_t>::grouping())
            , m_negative_sign_nat(IOv2::monetary_conf<char32_t>::negative_sign_nat())
            , m_frac_digits_nat(IOv2::monetary_conf<char32_t>::frac_digits_nat())
            , m_neg_format_nat(IOv2::monetary_conf<char32_t>::neg_format_nat())
            , m_curr_sym_nat(IOv2::monetary_conf<char32_t>::curr_symbol_nat())
            , m_pos_sign_nat(IOv2::monetary_conf<char32_t>::positive_sign_nat())
        {}
        
        virtual char32_t decimal_point() const override { return m_decimal_point; }
        void set_decimal_point(char32_t ch) { m_decimal_point = ch; }
        
        virtual char32_t thousands_sep() const override { return m_thousands_sep; }
        void set_thousands_sep(char32_t ch) { m_thousands_sep = ch; }

        virtual const std::vector<uint8_t>& grouping() const override { return m_grouping; }
        void set_grouping(const std::vector<uint8_t>& g) { m_grouping = g; }

        virtual const std::u32string & negative_sign_nat() const override { return m_negative_sign_nat; }
        void set_negative_sign_nat(const std::u32string & i) { m_negative_sign_nat = i; }

        virtual int frac_digits_nat() const override { return m_frac_digits_nat; }
        void set_frac_digits_nat(int v) { m_frac_digits_nat = v; }
        
        virtual const IOv2::base_ft<IOv2::monetary>::pattern& neg_format_nat() const override { return m_neg_format_nat; }
        void set_neg_format_nat(const IOv2::base_ft<IOv2::monetary>::pattern& p) { m_neg_format_nat = p; }
        
        virtual const std::u32string & curr_symbol_nat() const override { return m_curr_sym_nat; }
        void set_curr_symbol_nat(const std::u32string & s) { m_curr_sym_nat = s; }

        virtual const std::basic_string<char32_t>& positive_sign_nat() const override { return m_pos_sign_nat; }
        void set_positive_sign_nat(const std::u32string & s) { m_pos_sign_nat = s; }
        
    private:
        char32_t m_decimal_point;
        char32_t m_thousands_sep;
        std::vector<uint8_t> m_grouping;
        std::u32string  m_negative_sign_nat;
        int m_frac_digits_nat;
        IOv2::base_ft<IOv2::monetary>::pattern m_neg_format_nat;
        std::u32string  m_curr_sym_nat;
        std::u32string  m_pos_sign_nat;
    };
}

void test_monetary_char32_t_common_1()
{
    dump_info("Test monetary<char32_t> common 1...");
    static_assert(std::is_same_v<IOv2::monetary<char32_t>::char_type, char32_t>);

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("C"));
    if (obj.decimal_point() != U'.') throw std::runtime_error("monetary<char32_t>::decimal_point fail");
    if (obj.thousands_sep() != U',') throw std::runtime_error("monetary<char32_t>::thousands_sep fail");
    if (!obj.grouping().empty()) throw std::runtime_error("monetary<char32_t>::grouping fail");
    if (!obj.curr_symbol_nat().empty()) throw std::runtime_error("monetary<char32_t>::curr_symbol_nat fail");
    if (!obj.curr_symbol_int().empty()) throw std::runtime_error("monetary<char32_t>::curr_symbol_int fail");
    if (!obj.positive_sign_nat().empty()) throw std::runtime_error("monetary<char32_t>::positive_sign_nat fail");
    if (!obj.positive_sign_int().empty()) throw std::runtime_error("monetary<char32_t>::positive_sign_int fail");
    if (obj.negative_sign_nat().empty()) throw std::runtime_error("monetary<char32_t>::negative_sign_nat fail");
    if (obj.negative_sign_int().empty()) throw std::runtime_error("monetary<char32_t>::negative_sign_int fail");
    if (obj.frac_digits_int() != 0) throw std::runtime_error("monetary<char32_t>::frac_digits_int fail");
    if (obj.frac_digits_nat() != 0) throw std::runtime_error("monetary<char32_t>::frac_digits_nat fail");
    if (obj.pos_format_int() != obj.pos_format_nat()) throw std::runtime_error("monetary<char32_t>::pos_format fail");
    if (obj.neg_format_int() != obj.neg_format_nat()) throw std::runtime_error("monetary<char32_t>::neg_format fail");

    dump_info("Done\n");
}

void test_monetary_char32_t_common_2()
{
    dump_info("Test monetary<char32_t> common 2...");
    IOv2::monetary obj_c(std::make_shared<IOv2::monetary_conf<char32_t>>("C"));
    IOv2::monetary obj_de(std::make_shared<IOv2::monetary_conf<char32_t>>("de_DE.ISO-8859-1"));

    if (obj_c.decimal_point() == char()) throw std::runtime_error("monetary<char32_t>::decimal_point fail");
    if (obj_c.thousands_sep() == char()) throw std::runtime_error("monetary<char32_t>::thousands_sep fail");
    if (obj_c.decimal_point() == obj_de.decimal_point()) throw std::runtime_error("monetary<char32_t>::decimal_point fail");
    if (obj_c.thousands_sep() == obj_de.thousands_sep()) throw std::runtime_error("monetary<char32_t>::thousands_sep fail");
    if (obj_c.grouping() == obj_de.grouping()) throw std::runtime_error("monetary<char32_t>::grouping fail");
    if (obj_c.curr_symbol_int() == obj_de.curr_symbol_int()) throw std::runtime_error("monetary<char32_t>::curr_symbol_int fail");
    if (obj_c.negative_sign_int() != obj_de.negative_sign_int()) throw std::runtime_error("monetary<char32_t>::negative_sign_int fail");
    if (obj_c.frac_digits_int() == obj_de.frac_digits_int()) throw std::runtime_error("monetary<char32_t>::frac_digits_int fail");
    if (obj_c.pos_format_int() == obj_de.pos_format_int()) throw std::runtime_error("monetary<char32_t>::pos_format_int fail");

    dump_info("Done\n");
}

void test_monetary_char32_t_put_1()
{
    dump_info("Test monetary<char32_t>::put 1...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        // total EPA budget FY 2002
        const std::u32string  digits1(U"720000000000");
        // input less than frac_digits
        const std::u32string  digits2(U"-1");
        std::u32string  oss;
        
        obj.put(std::back_inserter(oss), true, ios, digits1);
        if (oss != U"7.200.000.000,00 ") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
        
        oss.clear();
        obj.put(std::back_inserter(oss), false, ios, digits1);
        if (oss != U"7.200.000.000,00 ") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
    
        // now try with showbase, to get currency symbol in format
        ios.setf(IOv2::ios_defs::showbase);
        
        oss.clear();
        obj.put(std::back_inserter(oss), true, ios, digits1);
        if (oss != U"7.200.000.000,00 EUR ") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
        
        oss.clear();
        obj.put(std::back_inserter(oss), false, ios, digits1);
        if (oss != U"7.200.000.000,00 €") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
    
    
        ios.unsetf(IOv2::ios_defs::showbase);
    
        // test io.width() > length
        // test various fill strategies
        ios.width(20); ios.fill('*');
        oss.clear();
        obj.put(std::back_inserter(oss), true, ios, digits2);
        if (oss != U"***************-,01*") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
        
        ios.width(20); ios.setf(IOv2::ios_defs::internal);
        oss.clear();
        obj.put(std::back_inserter(oss), true, ios, digits2);
        if (oss != U"-,01****************") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_put_2()
{
    dump_info("Test monetary<char32_t>::put 2...");

    auto helper = [](const IOv2::monetary<char32_t>& obj, const IOv2::monetary<char32_t>& obj_c)
    {
        IOv2::ios_base<char32_t> ios;

        // total EPA budget FY 2002
        const std::u32string  digits1(U"720000000000");

        // est. cost, national missile "defense", expressed as a loss in USD 2001
        const std::u32string  digits2(U"-10000000000000");  

        // not valid input
        const std::u32string  digits3(U"-A"); 
        
        // input less than frac_digits
        const std::u32string  digits4(U"-1");
    
        // cache the money_put facet
        std::u32string  oss;
    
        // now try with showbase, to get currency symbol in format
        ios.setf(IOv2::ios_defs::showbase);
        
        // test sign of more than one digit, say hong kong.
        oss.clear();
        obj.put(std::back_inserter(oss), false, ios, digits1);
        if (oss != U"HK$7,200,000,000.00") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
        
        oss.clear();
        obj.put(std::back_inserter(oss), true, ios, digits2);
        if (oss != U"(HKD 100,000,000,000.00)") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
        
        
        // test one-digit formats without zero padding
        // note: the result is different with libstdc++'s test case (libstdc%2B%2B-v3/testsuite/22_locale/money_put/put/char/2.cc)
        // since IOv2 set '-' as the negative sign of C locale.
        oss.clear();
        obj_c.put(std::back_inserter(oss), true, ios, digits4);
        if (oss != U"-1") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
    
        // test one-digit formats with zero padding, zero frac widths
        oss.clear();
        obj.put(std::back_inserter(oss), true, ios, digits4);
        if (oss != U"(HKD .01)") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
        
        ios.unsetf(IOv2::ios_defs::showbase);
    
        // test bunk input
        oss.clear();
        obj.put(std::back_inserter(oss), true, ios, digits3);
        if (oss != U"") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("en_HK.UTF-8"));
    IOv2::monetary obj_c(std::make_shared<IOv2::monetary_conf<char32_t>>("C"));
    helper(obj, obj_c);

    dump_info("Done\n");
}

void test_monetary_char32_t_put_3()
{
    dump_info("Test monetary<char32_t>::put 3...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        // woman, art, thief (stole the blues)
        const std::u32string  str(U"1943 Janis Joplin");
        const int64_t ld = 1943;
        const std::u32string  x(str.size(), L'x'); // have to have allocated string!
        std::u32string  res;
    
        std::u32string  oss;
    
        // 01 string
        res = x;
        auto ret1 = obj.put(res.begin(), false, ios, str);
        std::u32string  sanity1(res.begin(), ret1);
        if (res != U"1943xxxxxxxxxxxxx") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
        if (sanity1 != U"1943") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
    
        // 02 int64_t
        res = x;
        auto ret2 = obj.put(res.begin(), false, ios, ld);
        std::u32string  sanity2(res.begin(), ret2);
        if (res != U"1943xxxxxxxxxxxxx") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
        if (sanity2 != U"1943") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("C"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_put_4()
{
    dump_info("Test monetary<char32_t>::put 4...");
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_decimal_point(L'.');
    tmp_io->set_thousands_sep(L',');
    tmp_io->set_grouping({3});
    tmp_io->set_negative_sign_nat(U"()");
    tmp_io->set_frac_digits_nat(2);
    tmp_io->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::symbol,
                                IOv2::base_ft<IOv2::monetary>::space,
                                IOv2::base_ft<IOv2::monetary>::sign,
                                IOv2::base_ft<IOv2::monetary>::value});

    IOv2::monetary<char32_t> obj(tmp_io);
    IOv2::ios_base<char32_t> ios;
    ios.fill(L'*');

    std::u32string  val(U"-123456");

    std::u32string  fmt;
    obj.put(std::back_inserter(fmt), false, ios, val);
    if (fmt != U"*(1,234.56)") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
  
    dump_info("Done\n");
}

void test_monetary_char32_t_put_5()
{
    dump_info("Test monetary<char32_t>::put 5...");
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_thousands_sep(',');
    tmp_io->set_grouping({1});

    IOv2::monetary<char32_t> obj(tmp_io);
    IOv2::ios_base<char32_t> ios;
    ios.fill(L'*');

    long double val = 1.0e50L;

    std::basic_ostringstream<char32_t> fmt;
    std::ostreambuf_iterator<char32_t> out(fmt);
    obj.put(out, false, ios, (int64_t)val);
    if (!fmt.good()) throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
  
    dump_info("Done\n");
}

void test_monetary_char32_t_put_6()
{
    dump_info("Test monetary<char32_t>::put 6...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        int64_t amount = 11;

        // cache the money_put facet
        std::u32string  oss;
        obj.put(std::back_inserter(oss), true, ios, amount);
        if (oss != U"11") throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("C"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_put_7()
{
    dump_info("Test monetary<char32_t>::put 7...");
    
    IOv2::ios_base<char32_t> ios;
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_grouping({CHAR_MAX});
    IOv2::monetary<char32_t> obj(tmp_io);
    
    std::u32string  digits(300, L'1');
    
    std::u32string  oss;
    obj.put(std::back_inserter(oss), false, ios, digits);
    if (oss != digits) throw std::runtime_error("IOv2::monetary<char32_t>::put fails");
    
    dump_info("Done\n");
}

void test_monetary_char32_t_get_1()
{
    dump_info("Test monetary<char32_t>::get 1...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        // total EPA budget FY 2002
        const std::u32string  digits1(U"720000000000");

        std::u32string  iss;

        {
            iss = U"7.200.000.000,00 ";
            std::u32string  result1;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result1);
            if (result1 != digits1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }

        {
            iss = U"7.200.000.000,00  ";
            std::u32string  result2;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result2);
            if (result2 != digits1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }

        {
            iss = U"7.200.000.000,00  a";
            std::u32string  result3;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result3);
            if (result3 != digits1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (*it != 'a') throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }

        {
            iss = U"";
            std::u32string  result4;
            try
            {
                obj.get(iss.begin(), iss.end(), true, ios, result4);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (result4 != U"") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }

        {
            iss = U"working for enlightenment and peace in a mad world";
            std::u32string  result5;
            try
            {
                obj.get(iss.begin(), iss.end(), true, ios, result5);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (result5 != U"") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }

        // now try with showbase, to get currency symbol in format
        ios.setf(IOv2::ios_defs::showbase);

        {
            iss = U"7.200.000.000,00 EUR ";
            std::u32string  result6;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result6);
            if (result6 != digits1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }

        {
            iss = U"7.200.000.000,00 EUR  ";
            std::u32string  result7;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result7);
            if (result7 != digits1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it == iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (*it != U' ') throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }

        {
            iss = U"7.200.000.000,00 \x20ac";
            std::u32string  result8;
            auto it = obj.get(iss.begin(), iss.end(), false, ios, result8);
            if (result8 != digits1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_2()
{
    dump_info("Test monetary<char32_t>::get 2...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        // total EPA budget FY 2002
        const std::u32string  digits1(U"720000000000");

        // est. cost, national missile "defense", expressed as a loss in USD 2001
        const std::u32string  digits2(U"-10000000000000");  

        // input less than frac_digits
        const std::u32string  digits4(U"-1");

        std::u32string  iss;

        // now try with showbase, to get currency symbol in format
        ios.setf(IOv2::ios_defs::showbase);
        
        {
            iss = U"HK$7,200,000,000.00";
            std::u32string  result9;
            auto it = obj.get(iss.begin(), iss.end(), false, ios, result9);
            if (result9 != digits1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
        {
            iss = U"(HKD 100,000,000,000.00)";
            std::u32string  result10;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result10);
            if (result10 != digits2) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
        {
            iss = U"(HKD .01)";
            std::u32string  result11;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result11);
            if (result11 != digits4) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
        
        // for the "en_HK.ISO8859-1" locale the parsing of the very same input streams must
        // be successful without showbase too, since the symbol field appears in
        // the first positions in the format and the symbol, when present, must be
        // consumed.
        ios.unsetf(IOv2::ios_defs::showbase);
        {
            iss = U"HK$7,200,000,000.00";
            std::u32string  result12;
            auto it = obj.get(iss.begin(), iss.end(), false, ios, result12);
            if (result12 != digits1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
        {
            iss = U"(HKD 100,000,000,000.00)";
            std::u32string  result13;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result13);
            if (result13 != digits2) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
        {
            iss = U"(HKD .01)";
            std::u32string  result14;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result14);
            if (result14 != digits4) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("en_HK.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_3()
{
    dump_info("Test monetary<char32_t>::get 3...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        // total EPA budget FY 2002
        const long double  digits1 = 720000000000.0;
        
        std::u32string  iss;
        {
            iss = U"7.200.000.000,00 ";
            int64_t result1;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result1);
            if (result1 != digits1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
        
        {
            iss = U"7.200.000.000,00 ";
            int64_t result2;
            auto it = obj.get(iss.begin(), iss.end(), false, ios, result2);
            if (result2 != digits1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_4()
{
    dump_info("Test monetary<char32_t>::get 4...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        // input less than frac_digits
        const long double digits4 = -1.0;
        std::u32string  iss;

        // now try with showbase, to get currency symbol in format
        ios.setf(IOv2::ios_defs::showbase);
        
        iss = U"(HKD .01)";
        int64_t result3;
        auto it = obj.get(iss.begin(), iss.end(), true, ios, result3);
        if (result3 != digits4) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("en_HK.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_5()
{
    dump_info("Test monetary<char32_t>::get 5...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        const std::u32string  str = U"1Eleanor Roosevelt";
        
        {
            // 01 string
            std::u32string  res1;
            auto it = obj.get(str.begin(), str.end(), false, ios, res1);
            if (it == str.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            std::u32string  rem1(it, str.end());
            if (res1 != U"1") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (rem1 != U"Eleanor Roosevelt") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }

        {
            // 02 int64_t
            int64_t res2;
            auto it = obj.get(str.begin(), str.end(), false, ios, res2);
            if (it == str.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            std::u32string  rem2(it, str.end());
            if (res2 != 1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (rem2 != U"Eleanor Roosevelt") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("C"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_6()
{
    dump_info("Test monetary<char32_t>::get 6...");
    IOv2::ios_base<char32_t> ios;
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_decimal_point(L'.');
    tmp_io->set_grouping({4});
    tmp_io->set_curr_symbol_nat(U"$");
    tmp_io->set_positive_sign_nat(U"");
    tmp_io->set_negative_sign_nat(U"-");
    tmp_io->set_frac_digits_nat(2);
    tmp_io->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::symbol,
                                IOv2::base_ft<IOv2::monetary>::none,
                                IOv2::base_ft<IOv2::monetary>::sign,
                                IOv2::base_ft<IOv2::monetary>::value});

    IOv2::monetary<char32_t> obj(tmp_io);
    
    std::u32string  bufferp(U"$1234.56");
    std::u32string  buffern(U"$-1234.56");
    std::u32string  bufferp_ns(U"1234.56");
    std::u32string  buffern_ns(U"-1234.56");
    
    {
        std::u32string  valp;
        obj.get(bufferp.begin(), bufferp.end(), false, ios, valp);
        if (valp != U"123456") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }
    {
        std::u32string  valn;
        obj.get(buffern.begin(), buffern.end(), false, ios, valn);
        if (valn != U"-123456") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }
    {
        std::u32string  valp_ns;
        obj.get(bufferp_ns.begin(), bufferp_ns.end(), false, ios, valp_ns);
        if (valp_ns != U"123456") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }
    {
        std::u32string  valn_ns;
        obj.get(buffern_ns.begin(), buffern_ns.end(), false, ios, valn_ns);
        if (valn_ns != U"-123456") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }
  
    dump_info("Done\n");
}

void test_monetary_char32_t_get_7()
{
    dump_info("Test monetary<char32_t>::get 7...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        std::u32string  buffer1(U"123");
        std::u32string  buffer2(U"456");
        std::u32string  buffer3(U"Golgafrincham"); // From Nathan's original idea.

        std::u32string  val;

        {
            obj.get(buffer1.begin(), buffer1.end(), false, ios, val);
            if (val != buffer1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
        {
            obj.get(buffer2.begin(), buffer2.end(), false, ios, val);
            if (val != buffer2) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
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
            if (val != buffer3) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("C"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_8()
{
    dump_info("Test monetary<char32_t>::get 8...");
    
    IOv2::ios_base<char32_t> ios;
    
    auto tmp_io_a = std::make_shared<MoneyIO>("C");
    tmp_io_a->set_decimal_point(L'.');
    tmp_io_a->set_grouping({4});
    tmp_io_a->set_curr_symbol_nat(U"$");
    tmp_io_a->set_positive_sign_nat(U"()");
    tmp_io_a->set_frac_digits_nat(2);
    tmp_io_a->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::sign,
                                  IOv2::base_ft<IOv2::monetary>::value,
                                  IOv2::base_ft<IOv2::monetary>::space,
                                  IOv2::base_ft<IOv2::monetary>::symbol});

    auto tmp_io_b = std::make_shared<MoneyIO>("C");
    tmp_io_b->set_decimal_point(L'.');
    tmp_io_b->set_grouping({4});
    tmp_io_b->set_curr_symbol_nat(U"$");
    tmp_io_b->set_positive_sign_nat(U"()");
    tmp_io_b->set_frac_digits_nat(2);
    tmp_io_b->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::sign,
                                  IOv2::base_ft<IOv2::monetary>::value,
                                  IOv2::base_ft<IOv2::monetary>::symbol,
                                  IOv2::base_ft<IOv2::monetary>::none});
                                
    IOv2::monetary<char32_t> obj_a(tmp_io_a);
    IOv2::monetary<char32_t> obj_b(tmp_io_b);
    
    std::u32string  buffer_a(U"(1234.56 $)");
    std::u32string  buffer_a_ns(U"(1234.56 )");

    std::u32string  val_a, val_a_ns;

    {
        obj_a.get(buffer_a.begin(), buffer_a.end(), false, ios, val_a);
        if (val_a != U"123456") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }
    {
        obj_a.get(buffer_a_ns.begin(), buffer_a_ns.end(), false, ios, val_a_ns);
        if (val_a_ns != U"123456") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }

    std::u32string  buffer_b(U"(1234.56$)");
    std::u32string  buffer_b_ns(U"(1234.56)");

    std::u32string  val_b, val_b_ns;
    {
        obj_b.get(buffer_b.begin(), buffer_b.end(), false, ios, val_b);
        if (val_b != U"123456") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }
    {
        obj_b.get(buffer_b_ns.begin(), buffer_b_ns.end(), false, ios, val_b_ns);
        if (val_b_ns != U"123456") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }

    dump_info("Done\n");
}

void test_monetary_char32_t_get_9()
{
    dump_info("Test monetary<char32_t>::get 9...");
    
    IOv2::ios_base<char32_t> ios;
    
    auto dublin = std::make_shared<MoneyIO>("C");
    dublin->set_frac_digits_nat(3);
                                
    IOv2::monetary<char32_t> obj(dublin);
    std::u32string  liffey;
    std::u32string  coins;
    
    {
        // Feed it 1 digit too many, which should fail.
        liffey = U"12.3456";
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
        liffey = U"12.345";
        obj.get(liffey.begin(), liffey.end(), false, ios, coins);
    }
    {
        // Feed it 1 digit too few, which should fail.
        liffey = U"12.34";
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
        liffey = U"12.";
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
        liffey = U"12";
        obj.get(liffey.begin(), liffey.end(), false, ios, coins);
    }
    dump_info("Done\n");
}

void test_monetary_char32_t_get_10()
{
    dump_info("Test monetary<char32_t>::get 10...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;
        std::u32string  iss;
        std::u32string  extracted_amount;
        {
            iss = U"-$0 ";
            auto it = obj.get(iss.begin(), iss.end(), false, ios, extracted_amount);
            if (it == iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (*it != ' ') throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (extracted_amount != U"0") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
        {
            extracted_amount.clear();
            iss = U"-$ ";
            try
            {
                obj.get(iss.begin(), iss.end(), false, ios, extracted_amount);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
            if (!extracted_amount.empty()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("en_US.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_11()
{
    dump_info("Test monetary<char32_t>::get 11...");
    
    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        // A _very_ big amount.
        std::u32string  str = U"1";
        for (int i = 0; i < 2 * std::numeric_limits<int64_t>::digits10; ++i)
            str += U".000";
        str += U",00 ";

        try
        {
            int64_t result1;
            obj.get(str.begin(), str.end(), true, ios, result1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_12()
{
    dump_info("Test monetary<char32_t>::get 12...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        // total EPA budget FY 2002
        const long double  digits1 = 720000000000.0;

        std::u32string  iss;

        {
            iss = U"7200000000,00 ";
            int64_t result1;
            auto it = obj.get(iss.begin(), iss.end(), true, ios, result1);
            if (result1 != digits1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
        {
            iss = U"7200000000,00 ";
            int64_t result2;
            auto it = obj.get(iss.begin(), iss.end(), false, ios, result2);
            if (result2 != digits1) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
            if (it != iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_13()
{
    dump_info("Test monetary<char32_t>::get 13...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        std::u32string  iss;
        {
            iss = U"500,1.0 ";
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
            iss = U"500,1.0 ";
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

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_14()
{
    dump_info("Test monetary<char32_t>::get 14...");
    IOv2::ios_base<char32_t> ios;
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_positive_sign_nat(U"+");
    tmp_io->set_negative_sign_nat(U"");
    
    IOv2::monetary<char32_t> obj(tmp_io);

    std::u32string  buffer(U"69");
    std::u32string  val;
    
    obj.get(buffer.begin(), buffer.end(), false, ios, val);
    if (val != U"-69") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");

    dump_info("Done\n");
}

void test_monetary_char32_t_get_15()
{
    dump_info("Test monetary<char32_t>::get 15...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        std::u32string  iss;
        {
            iss = U".100";
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
            iss = U"30..0";
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

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_16()
{
    dump_info("Test monetary<char32_t>::get 16...");
    IOv2::ios_base<char32_t> ios1, ios2;
    
    IOv2::monetary<char32_t> obj_de(std::make_shared<IOv2::monetary_conf<char32_t>>("de_DE.UTF-8"));
    IOv2::monetary<char32_t> obj_hk(std::make_shared<IOv2::monetary_conf<char32_t>>("en_HK.UTF-8"));

    {
        ios1.setf(IOv2::ios_defs::showbase);
        std::u32string  iss01 = U"EUR ";
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
        std::u32string  iss02 = U"(HKD )";
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

void test_monetary_char32_t_get_17()
{
    dump_info("Test monetary<char32_t>::get 17...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        std::u32string  iss;
        {
            iss = U"7.200.000.000,00";
            std::u32string result1;
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
            iss = U"7.200.000.000,00EUR ";
            std::u32string  result2;
            try
            {
                obj.get(iss.begin(), iss.end(), true, ios, result2);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("de_DE.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_18()
{
    dump_info("Test monetary<char32_t>::get 18...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        std::u32string  iss;
        {
            iss = U"HK7,200,000,000.00";
            std::u32string  result1;
            try
            {
                obj.get(iss.begin(), iss.end(), false, ios, result1);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
        }
        {
            iss = U"(HK100,000,000,000.00)";
            std::u32string  result1;
            try
            {
                obj.get(iss.begin(), iss.end(), false, ios, result1);
                dump_info("unreachable code");
                std::abort();
            }
            catch (IOv2::stream_error&) {}
        }
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("en_HK.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_19()
{
    dump_info("Test monetary<char32_t>::get 19...");
    
    IOv2::ios_base<char32_t> ios;
    
    auto tmp_io_a = std::make_shared<MoneyIO>("C");
    tmp_io_a->set_curr_symbol_nat(U"$");
    tmp_io_a->set_positive_sign_nat(U"");
    tmp_io_a->set_negative_sign_nat(U"");
    tmp_io_a->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::value,
                                  IOv2::base_ft<IOv2::monetary>::symbol,
                                  IOv2::base_ft<IOv2::monetary>::none,
                                  IOv2::base_ft<IOv2::monetary>::sign});

    auto tmp_io_b = std::make_shared<MoneyIO>("C");
    tmp_io_b->set_curr_symbol_nat(U"%");
    tmp_io_b->set_positive_sign_nat(U"");
    tmp_io_b->set_negative_sign_nat(U"-");
    tmp_io_b->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::value,
                                  IOv2::base_ft<IOv2::monetary>::symbol,
                                  IOv2::base_ft<IOv2::monetary>::sign,
                                  IOv2::base_ft<IOv2::monetary>::none});

    auto tmp_io_c = std::make_shared<MoneyIO>("C");
    tmp_io_c->set_curr_symbol_nat(U"&");
    tmp_io_c->set_positive_sign_nat(U"");
    tmp_io_c->set_negative_sign_nat(U"");
    tmp_io_c->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::value,
                                  IOv2::base_ft<IOv2::monetary>::space,
                                  IOv2::base_ft<IOv2::monetary>::symbol,
                                  IOv2::base_ft<IOv2::monetary>::sign});
                                
    IOv2::monetary<char32_t> obj_a(tmp_io_a);
    IOv2::monetary<char32_t> obj_b(tmp_io_b);
    IOv2::monetary<char32_t> obj_c(tmp_io_c);
    
    {
        std::u32string  iss_01 = U"10$";
        std::u32string  result01;
        auto it = obj_a.get(iss_01.begin(), iss_01.end(), false, ios, result01);
        if (it == iss_01.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        if (*it != '$') throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }
    {
        std::u32string  iss_02 = U"50%";
        std::u32string  result02;
        auto it = obj_a.get(iss_02.begin(), iss_02.end(), false, ios, result02);
        if (it == iss_02.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        if (*it != '%') throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }
    {
        std::u32string  iss_03 = U"7 &";
        std::u32string  result03;
        auto it = obj_a.get(iss_03.begin(), iss_03.end(), false, ios, result03);
        if (it == iss_03.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        if (*it != '&') throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }

    dump_info("Done\n");
}

void test_monetary_char32_t_get_20()
{
    dump_info("Test monetary<char32_t>::get 20...");

    auto helper = [](const IOv2::monetary<char32_t>& obj)
    {
        IOv2::ios_base<char32_t> ios;

        std::u32string  iss = U"$.00 ";
        std::u32string  extracted_amount;
        auto it = obj.get(iss.begin(), iss.end(), false, ios, extracted_amount);
        if (it == iss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        if (*it != ' ') throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
        if (extracted_amount != U"0") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    };

    IOv2::monetary obj(std::make_shared<IOv2::monetary_conf<char32_t>>("en_US.UTF-8"));
    helper(obj);

    dump_info("Done\n");
}

void test_monetary_char32_t_get_21()
{
    dump_info("Test monetary<char32_t>::get 21...");
    IOv2::ios_base<char32_t> ios;
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_grouping({1});
    tmp_io->set_thousands_sep(L'#');
    tmp_io->set_neg_format_nat({IOv2::base_ft<IOv2::monetary>::symbol,
                                IOv2::base_ft<IOv2::monetary>::none,
                                IOv2::base_ft<IOv2::monetary>::sign,
                                IOv2::base_ft<IOv2::monetary>::value});
                                  
    IOv2::monetary<char32_t> obj(tmp_io);
    std::u32string  buffer1(U"00#0#1");
    std::u32string  buffer2(U"000##1");
    std::u32string  val1, val2;
    
    {
        try
        {
            obj.get(buffer1.begin(), buffer1.end(), false, ios, val1);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (val1 != U"1") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }
    {
        try
        {
            obj.get(buffer2.begin(), buffer2.end(), false, ios, val2);
            dump_info("unreachable code");
            std::abort();
        }
        catch (IOv2::stream_error&) {}
        if (val2 != U"") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    }

    dump_info("Done\n");
}

void test_monetary_char32_t_get_22()
{
    dump_info("Test monetary<char32_t>::get 22...");
    IOv2::ios_base<char32_t> ios;
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_frac_digits_nat(0);
    IOv2::monetary<char32_t> obj(tmp_io);
    
    std::u32string  ss = U"123.455";
    std::u32string  digits;
    
    auto it = obj.get(ss.begin(), ss.end(), false, ios, digits);
    std::u32string  rest = std::u32string (it, ss.end());
    if (digits != U"123") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    if (rest != U".455") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");

    dump_info("Done\n");
}

void test_monetary_char32_t_get_23()
{
    dump_info("Test monetary<char32_t>::get 23...");
    IOv2::ios_base<char32_t> ios;
    
    auto tmp_io = std::make_shared<MoneyIO>("C");
    tmp_io->set_grouping({CHAR_MAX});
    IOv2::monetary<char32_t> obj(tmp_io);
    
    std::u32string  ss = U"123,456";
    std::u32string  digits;
    
    auto it = obj.get(ss.begin(), ss.end(), false, ios, digits);
    if (it == ss.end()) throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    if (digits != U"123") throw std::runtime_error("IOv2::monetary<char32_t>::get fails");
    if (*it != L',') throw std::runtime_error("IOv2::monetary<char32_t>::get fails");

    dump_info("Done\n");
}