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

        const std::vector<uint8_t>& grouping() const override { return m_grouping; };
        char thousands_sep() const override { return m_thousands_sep; }
    private:
        std::vector<uint8_t> m_grouping = {3};
        char m_thousands_sep = ' ';
    };
    
    struct WMyNP: IOv2::numeric_conf<wchar_t>
    {
        WMyNP()
            : IOv2::numeric_conf<wchar_t>("C")
        {}

        const std::vector<uint8_t>& grouping() const override { return m_grouping; };
        wchar_t thousands_sep() const override { return m_thousands_sep; }
    private:
        std::vector<uint8_t> m_grouping = {3};
        wchar_t m_thousands_sep = L' ';
    };
}

void test_io_base_manipulators_basefield_char_1()
{
    dump_info("Test ios_base<char> basefield case 1...");
    const char lit[] = "0123 456\n"
                     ": 01 234 567:\n"
                     ":0123 456   :\n"
                     ":    012 345:\n"
                     ":     01 234:\n"
                     ":0726 746 425:\n"
                     ":04 553 207 :\n"
                     ":   0361 100:\n"
                     ":       0173:\n"
                     "0x12 345 678\n"
                     "|0x000012 345 678|\n"
                     "|0x12 345 6780000|\n"
                     "|00000x12 345 678|\n"
                     "|0x000012 345 678|\n";

    IOv2::ostream oss{IOv2::mem_device{""}};
    oss.locale(IOv2::locale<char>("C").involve(std::make_shared<MyNP>()));
    
    // Octals
    oss << IOv2::oct << IOv2::showbase;
    oss << 0123456l << IOv2::endl;
    
    oss << ":" << IOv2::setw(11);
    oss << 01234567l << ":" << IOv2::endl;
    
    oss << ":" << IOv2::setw(11) << IOv2::left;
    oss << 0123456l << ":" << IOv2::endl;
    
    oss << ":" << IOv2::setw(11) << IOv2::right;
    oss << 012345l << ":" << IOv2::endl;
    
    oss << ":" << IOv2::setw(11) << IOv2::internal;
    oss << 01234l << ":" << IOv2::endl;
    
    oss << ":" << IOv2::setw(11);
    oss << 123456789l << ":" << IOv2::endl;
    
    oss << ":" << IOv2::setw(11) << IOv2::left;
    oss << 1234567l << ":" << IOv2::endl;
    
    oss << ":" << IOv2::setw(11) << IOv2::right;
    oss << 123456l << ":" << IOv2::endl;
    
    oss << ":" << IOv2::setw(11) << IOv2::internal;
    oss << 123l << ":" << IOv2::endl;

    // Hexadecimals
    oss << IOv2::hex << IOv2::setfill('0');
    oss << 0x12345678l << IOv2::endl;
    
    oss << "|" << IOv2::setw(16);
    oss << 0x12345678l << "|" << IOv2::endl;
    
    oss << "|" << IOv2::setw(16) << IOv2::left;
    oss << 0x12345678l << "|" << IOv2::endl;
    
    oss << "|" << IOv2::setw(16) << IOv2::right;
    oss << 0x12345678l << "|" << IOv2::endl;
    
    oss << "|" << IOv2::setw(16) << IOv2::internal;
    oss << 0x12345678l << "|" << IOv2::endl;
    
    if (!oss.good()) throw std::runtime_error("ios_base<char> adjustfield check fail");
    if (oss.detach().str() != lit) throw std::runtime_error("ios_base<char> adjustfield check fail");
    dump_info("Done\n");
}

void test_io_base_manipulators_basefield_wchar_t_1()
{
    dump_info("Test ios_base<wchar_t> basefield case 1...");
    const wchar_t lit[] = L"0123 456\n"
                          L": 01 234 567:\n"
                          L":0123 456   :\n"
                          L":    012 345:\n"
                          L":     01 234:\n"
                          L":0726 746 425:\n"
                          L":04 553 207 :\n"
                          L":   0361 100:\n"
                          L":       0173:\n"
                          L"0x12 345 678\n"
                          L"|0x000012 345 678|\n"
                          L"|0x12 345 6780000|\n"
                          L"|00000x12 345 678|\n"
                          L"|0x000012 345 678|\n";

    IOv2::ostream oss{IOv2::mem_device{L""}};
    oss.locale(IOv2::locale<wchar_t>("C").involve(std::make_shared<WMyNP>()));
    
    // Octals
    oss << IOv2::oct << IOv2::showbase;
    oss << 0123456l << IOv2::endl;
    
    oss << L":" << IOv2::setw(11);
    oss << 01234567l << L":" << IOv2::endl;
    
    oss << L":" << IOv2::setw(11) << IOv2::left;
    oss << 0123456l << L":" << IOv2::endl;
    
    oss << L":" << IOv2::setw(11) << IOv2::right;
    oss << 012345l << L":" << IOv2::endl;
    
    oss << L":" << IOv2::setw(11) << IOv2::internal;
    oss << 01234l << L":" << IOv2::endl;
    
    oss << L":" << IOv2::setw(11);
    oss << 123456789l << L":" << IOv2::endl;
    
    oss << L":" << IOv2::setw(11) << IOv2::left;
    oss << 1234567l << L":" << IOv2::endl;
    
    oss << L":" << IOv2::setw(11) << IOv2::right;
    oss << 123456l << L":" << IOv2::endl;
    
    oss << L":" << IOv2::setw(11) << IOv2::internal;
    oss << 123l << L":" << IOv2::endl;

    // Hexadecimals
    oss << IOv2::hex << IOv2::setfill(L'0');
    oss << 0x12345678l << IOv2::endl;
    
    oss << L"|" << IOv2::setw(16);
    oss << 0x12345678l << L"|" << IOv2::endl;
    
    oss << L"|" << IOv2::setw(16) << IOv2::left;
    oss << 0x12345678l << L"|" << IOv2::endl;
    
    oss << L"|" << IOv2::setw(16) << IOv2::right;
    oss << 0x12345678l << L"|" << IOv2::endl;
    
    oss << L"|" << IOv2::setw(16) << IOv2::internal;
    oss << 0x12345678l << L"|" << IOv2::endl;
    
    if (!oss.good()) throw std::runtime_error("ios_base<char> adjustfield check fail");
    if (oss.detach().str() != lit) throw std::runtime_error("ios_base<char> adjustfield check fail");
    dump_info("Done\n");
}