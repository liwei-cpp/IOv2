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

void test_io_base_manipulators_put_money_char_1()
{
    dump_info("Test ios_base<char> put_money case 1...");

    IOv2::ostream oss{IOv2::mem_device{""}, IOv2::locale<char>("de_DE.ISO-8859-1")};
    
    const std::string str("720000000000");
    oss << IOv2::put_money(str);
    if (!oss.good()) throw std::runtime_error("ios_base<char> put_money check fail");
    if (oss.detach().str() != "7.200.000.000,00 ") throw std::runtime_error("ios_base<char> put_money check fail");

    dump_info("Done\n");
}

void test_io_base_manipulators_put_money_char_2()
{
    dump_info("Test ios_base<char> put_money case 2...");

    IOv2::ostream oss{IOv2::mem_device{""}, IOv2::locale<char>("en_US.UTF-8")};
    
    const std::string str("123");
    oss.setstate(IOv2::ios_defs::cvtfailbit);

    oss << IOv2::put_money(str);
    if (!oss.cvt_fail()) throw std::runtime_error("ios_base<char> put_money check fail");
    if (!oss.detach().str().empty()) throw std::runtime_error("ios_base<char> put_money check fail");

    dump_info("Done\n");
}

void test_io_base_manipulators_put_money_wchar_t_1()
{
    dump_info("Test ios_base<wchar_t> put_money case 1...");

    IOv2::ostream oss{IOv2::mem_device{L""}, IOv2::locale<wchar_t>("de_DE.ISO-8859-1")};
    
    const std::wstring str(L"720000000000");
    oss << IOv2::put_money(str);
    if (!oss.good()) throw std::runtime_error("ios_base<wchar_t> put_money check fail");
    if (oss.detach().str() != L"7.200.000.000,00 ") throw std::runtime_error("ios_base<wchar_t> put_money check fail");

    dump_info("Done\n");
}

void test_io_base_manipulators_put_money_wchar_t_2()
{
    dump_info("Test ios_base<wchar_t> put_money case 2...");

    IOv2::ostream oss{IOv2::mem_device{L""}, IOv2::locale<wchar_t>("en_US.UTF-8")};
    
    const std::wstring str(L"123");
    oss.setstate(IOv2::ios_defs::cvtfailbit);

    oss << IOv2::put_money(str);
    if (!oss.cvt_fail()) throw std::runtime_error("ios_base<wchar_t> put_money check fail");
    if (!oss.detach().str().empty()) throw std::runtime_error("ios_base<wchar_t> put_money check fail");

    dump_info("Done\n");
}