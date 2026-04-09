#include <limits>
#include <stdexcept>
#include <system_error>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/char_and_str.h>
#include <io/fp_defs/arithmetic.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <common/dump_info.h>

namespace
{
    struct MyNP: IOv2::numeric_conf<char>
    {
        MyNP()
            : IOv2::numeric_conf<char>("C")
        {}

        const std::string& truename() const override { return m_tname; }
        const std::string& falsename() const override { return m_fname; }
    private:
        const std::string m_tname = "yea";
        const std::string m_fname = "nay";
    };
    
    struct WMyNP: IOv2::numeric_conf<wchar_t>
    {
        WMyNP()
            : IOv2::numeric_conf<wchar_t>("C")
        {}

        const std::wstring& truename() const override { return m_tname; }
        const std::wstring& falsename() const override { return m_fname; }
    private:
        const std::wstring m_tname = L"yea";
        const std::wstring m_fname = L"nay";
    };
}

void test_io_base_manipulators_adjustfield_char_1()
{
    dump_info("Test ios_base<char> adjustfield case 1...");
    const char lit[] = "1 0\n"
                        "true false\n"
                        ":  true:\n"
                        ":true  :\n"
                        ": false:\n"
                        ":  1:\n"
                        ":1  :\n"
                        ":  0:\n"
                        "yea nay\n"
                        ":   yea:\n"
                        ":yea   :\n"
                        ":   nay:\n";

    IOv2::ostream oss{IOv2::mem_device{""}};
    oss << true << " " << false << IOv2::endl;
    oss << IOv2::boolalpha;
    oss << true << " " << false << IOv2::endl;

    oss << ":" << IOv2::setw(6) << IOv2::internal << true << ":" << IOv2::endl;
    oss << ":" << IOv2::setw(6) << IOv2::left << true << ":" << IOv2::endl;
    oss << ":" << IOv2::setw(6) << IOv2::right << false << ":" << IOv2::endl;
    oss << IOv2::noboolalpha;
    oss << ":" << IOv2::setw(3) << IOv2::internal << true << ":" << IOv2::endl;
    oss << ":" << IOv2::setw(3) << IOv2::left << true << ":" << IOv2::endl;
    oss << ":" << IOv2::setw(3) << IOv2::right << false << ":" << IOv2::endl;

    IOv2::locale<char> loc = IOv2::locale<char>("C").involve(std::make_shared<MyNP>());
    oss.locale(loc);

    oss << IOv2::boolalpha;
    oss << true << " " << false << IOv2::endl;

    oss << ":" << IOv2::setw(6) << IOv2::internal << true << ":" << IOv2::endl;
    oss << ":" << IOv2::setw(6) << IOv2::left << true << ":" << IOv2::endl;
    oss << ":" << IOv2::setw(6) << IOv2::right << false << ":" << IOv2::endl;

    if (!oss.good()) throw std::runtime_error("ios_base<char> adjustfield check fail");
    
    if (oss.detach().str() != lit) throw std::runtime_error("ios_base<char> adjustfield check fail");
    dump_info("Done\n");
}

void test_io_base_manipulators_adjustfield_char_2()
{
    dump_info("Test ios_base<char> adjustfield case 2...");
    IOv2::ostream o{IOv2::mem_device{""}};
    o << IOv2::setw(6) << IOv2::right << "san";
    if (o.detach().str() != "   san") throw std::runtime_error("ios_base<char> adjustfield check fail");
    o.attach(IOv2::mem_device{""});

    o << IOv2::setw(6) << IOv2::internal << "fran";
    if (o.detach().str() != "  fran") throw std::runtime_error("ios_base<char> adjustfield check fail");
    o.attach(IOv2::mem_device{""});

    o << IOv2::setw(6) << IOv2::left << "cisco";
    if (o.detach().str() != "cisco ") throw std::runtime_error("ios_base<char> adjustfield check fail");
    
    dump_info("Done\n");
}

void test_io_base_manipulators_adjustfield_wchar_t_1()
{
    dump_info("Test ios_base<wchar_t> adjustfield case 1...");
    const wchar_t lit[] = L"1 0\n"
                          L"true false\n"
                          L":  true:\n"
                          L":true  :\n"
                          L": false:\n"
                          L":  1:\n"
                          L":1  :\n"
                          L":  0:\n"
                          L"yea nay\n"
                          L":   yea:\n"
                          L":yea   :\n"
                          L":   nay:\n";

    IOv2::ostream oss{IOv2::mem_device{L""}};
    oss << true << L" " << false << IOv2::endl;
    oss << IOv2::boolalpha;
    oss << true << L" " << false << IOv2::endl;

    oss << L":" << IOv2::setw(6) << IOv2::internal << true << L":" << IOv2::endl;
    oss << L":" << IOv2::setw(6) << IOv2::left << true << L":" << IOv2::endl;
    oss << L":" << IOv2::setw(6) << IOv2::right << false << L":" << IOv2::endl;
    oss << IOv2::noboolalpha;
    oss << L":" << IOv2::setw(3) << IOv2::internal << true << L":" << IOv2::endl;
    oss << L":" << IOv2::setw(3) << IOv2::left << true << L":" << IOv2::endl;
    oss << L":" << IOv2::setw(3) << IOv2::right << false << L":" << IOv2::endl;

    IOv2::locale<wchar_t> loc = IOv2::locale<wchar_t>("C").involve(std::make_shared<WMyNP>());
    oss.locale(loc);

    oss << IOv2::boolalpha;
    oss << true << L" " << false << IOv2::endl;

    oss << L":" << IOv2::setw(6) << IOv2::internal << true << L":" << IOv2::endl;
    oss << L":" << IOv2::setw(6) << IOv2::left << true << L":" << IOv2::endl;
    oss << L":" << IOv2::setw(6) << IOv2::right << false << L":" << IOv2::endl;

    if (!oss.good()) throw std::runtime_error("ios_base<wchar_t> adjustfield check fail");
    if (oss.detach().str() != lit) throw std::runtime_error("ios_base<wchar_t> adjustfield check fail");
    dump_info("Done\n");
}

void test_io_base_manipulators_adjustfield_wchar_t_2()
{
    dump_info("Test ios_base<wchar_t> adjustfield case 2...");
    IOv2::ostream o{IOv2::mem_device{L""}};
    o << IOv2::setw(6) << IOv2::right << L"san";
    if (o.detach().str() != L"   san") throw std::runtime_error("ios_base<wchar_t> adjustfield check fail");
    o.attach(IOv2::mem_device{L""});

    o << IOv2::setw(6) << IOv2::internal << L"fran";
    if (o.detach().str() != L"  fran") throw std::runtime_error("ios_base<wchar_t> adjustfield check fail");
    o.attach(IOv2::mem_device{L""});

    o << IOv2::setw(6) << IOv2::left << L"cisco";
    if (o.detach().str() != L"cisco ") throw std::runtime_error("ios_base<wchar_t> adjustfield check fail");
    
    dump_info("Done\n");
}