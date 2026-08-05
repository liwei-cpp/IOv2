#include <limits>
#include <stdexcept>
#include <system_error>
#include <string>
#include <device/mem_device.h>
#include <io/traits/char_and_str.h>
#include <io/traits/arithmetic.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <support/dump_info.h>
#include <support/io_traits_probe.h>
#include <support/verify.h>

void test_io_base_manipulators_put_money_char_1()
{
    dump_info("Test ios_base<char> put_money case 1...");

    IOv2::ostream oss{IOv2::mem_device{""}, IOv2::locale<char>("de_DE.ISO-8859-1")};
    
    const std::string str("720000000000");
    oss << IOv2::put_money(str);
    VERIFY(oss.good());
    auto [dev1, err1] = oss.detach();
    VERIFY(dev1.str() == "7.200.000.000,00 ");

    dump_info("Done\n");
}

void test_io_base_manipulators_put_money_char_2()
{
    dump_info("Test ios_base<char> put_money case 2...");

    IOv2::ostream oss{IOv2::mem_device{""}, IOv2::locale<char>("en_US.UTF-8")};
    
    const std::string str("123");
    oss.setstate(IOv2::ios_defs::cvtfailbit);

    oss << IOv2::put_money(str);
    VERIFY(oss.cvt_fail());
    auto [dev2, err2] = oss.detach();
    VERIFY(dev2.str().empty());

    dump_info("Done\n");
}

void test_io_base_manipulators_put_money_char_3()
{
    dump_info("Test ios_base<char> put_money case 3...");

    // A volatile integral formats exactly as the plain one: monetary::put takes it by value,
    // so deduction drops the cv-qualifier.
    const long long plain = 123456;
    volatile long long vol = 123456;

    IOv2::ostream oss1{IOv2::mem_device{""}, IOv2::locale<char>("de_DE.ISO-8859-1")};
    oss1 << IOv2::put_money(plain);
    VERIFY(oss1.good());

    IOv2::ostream oss2{IOv2::mem_device{""}, IOv2::locale<char>("de_DE.ISO-8859-1")};
    oss2 << IOv2::put_money(vol);
    VERIFY(oss2.good());

    auto [dev5, err5] = oss1.detach();
    auto [dev6, err6] = oss2.detach();
    VERIFY(!dev5.str().empty());
    VERIFY(dev5.str() == dev6.str());

    dump_info("Done\n");
}

void test_io_base_manipulators_put_money_wchar_t_1()
{
    dump_info("Test ios_base<wchar_t> put_money case 1...");

    IOv2::ostream oss{IOv2::mem_device{L""}, IOv2::locale<wchar_t>("de_DE.ISO-8859-1")};
    
    const std::wstring str(L"720000000000");
    oss << IOv2::put_money(str);
    VERIFY(oss.good());
    auto [dev3, err3] = oss.detach();
    VERIFY(dev3.str() == L"7.200.000.000,00 ");

    dump_info("Done\n");
}

void test_io_base_manipulators_put_money_wchar_t_2()
{
    dump_info("Test ios_base<wchar_t> put_money case 2...");

    IOv2::ostream oss{IOv2::mem_device{L""}, IOv2::locale<wchar_t>("en_US.UTF-8")};
    
    const std::wstring str(L"123");
    oss.setstate(IOv2::ios_defs::cvtfailbit);

    oss << IOv2::put_money(str);
    VERIFY(oss.cvt_fail());
    auto [dev4, err4] = oss.detach();
    VERIFY(dev4.str().empty());

    dump_info("Done\n");
}
void test_io_base_manipulators_get_money_char_1()
{
    dump_info("Test ios_base<char> get_money case 1...");

    // Round-trips the put_money case-1 output. Exercises the idiomatic rvalue form
    // `is >> get_money(x)`, which needs the by-value operator>> overload.
    IOv2::istream iss{IOv2::mem_device{std::string("7.200.000.000,00 ")},
                      IOv2::locale<char>("de_DE.ISO-8859-1")};

    std::string str;
    iss >> IOv2::get_money(str);
    VERIFY(static_cast<bool>(iss));
    VERIFY(!iss.str_fail());
    VERIFY(str == "720000000000");

    dump_info("Done\n");
}

void test_io_base_manipulators_get_money_char_2()
{
    dump_info("Test ios_base<char> get_money case 2...");

    // Integral target, and the named-lvalue form `auto g = get_money(v); is >> g;`
    // which resolves to the generic extraction operator instead.
    IOv2::istream iss{IOv2::mem_device{std::string("1.234,56 ")},
                      IOv2::locale<char>("de_DE.ISO-8859-1")};

    long long units = -1;
    auto manip = IOv2::get_money(units);
    iss >> manip;
    VERIFY(static_cast<bool>(iss));
    VERIFY(units == 123456);

    // A stream already in a failed state extracts nothing and leaves the target alone.
    IOv2::istream iss2{IOv2::mem_device{std::string("1.234,56 ")},
                       IOv2::locale<char>("de_DE.ISO-8859-1")};
    long long untouched = -1;
    iss2.setstate(IOv2::ios_defs::cvtfailbit);
    iss2 >> IOv2::get_money(untouched);
    VERIFY(iss2.cvt_fail());
    VERIFY(untouched == -1);

    dump_info("Done\n");
}

void test_io_base_manipulators_get_money_wchar_t_1()
{
    dump_info("Test ios_base<wchar_t> get_money case 1...");

    IOv2::istream iss{IOv2::mem_device{std::wstring(L"7.200.000.000,00 ")},
                      IOv2::locale<wchar_t>("de_DE.ISO-8859-1")};

    std::wstring str;
    iss >> IOv2::get_money(str);
    VERIFY(static_cast<bool>(iss));
    VERIFY(!iss.str_fail());
    VERIFY(str == L"720000000000");

    dump_info("Done\n");
}

namespace
{
using MoneyIs = IOv2::istream<IOv2::mem_device<char>, char>;
using MoneyOs = IOv2::ostream<IOv2::mem_device<char>, char>;

// get_money deduces its target type from the argument, so a const lvalue deduces `const int`
// -- and std::integral<const int> is true, since std::is_integral_v ignores cv-qualification.
// Without the cv exclusion on io_traits, such a target selected sread and then failed
// deep inside the monetary facet on an assignment to a read-only reference, burying the real
// cause. With it there is no sread and the operator's static_assert says so instead.
//
// Every probe here goes through io_traits rather than through `is >> get_money(x)`: the operators
// are unconstrained on the value type and reject in the body with a static_assert, which no
// requires-expression can see, so probing the expression reports true whatever the target is.
static_assert(  extractable<char, IOv2::_Get_money<int>> );
static_assert(  extractable<char, IOv2::_Get_money<std::string>> );
static_assert( !extractable<char, IOv2::_Get_money<const int>> );
static_assert( !extractable<char, IOv2::_Get_money<volatile int>> );
static_assert( !extractable<char, IOv2::_Get_money<const std::string>> );
static_assert( !extractable<char, IOv2::_Get_money<bool>> );
static_assert( !extractable<char, IOv2::_Get_money<double>> );
static_assert( !extractable<char, IOv2::_Get_money<std::wstring>> );

// put_money carries no such restriction: output does not write to the target and put_money takes
// const _MoneyT&, so inserting a const lvalue stays a legitimate use.

// The io_traits bool exclusion normalizes cv, its string branch does not: put() takes the integral
// by value (cv dropped), but its string overload takes a plain const&, which volatile cannot bind.
static_assert(  insertable<char, IOv2::_Put_money<int>> );
static_assert(  insertable<char, IOv2::_Put_money<const int>> );
static_assert(  insertable<char, IOv2::_Put_money<volatile int>> );
static_assert(  insertable<char, IOv2::_Put_money<std::string>> );
static_assert( !insertable<char, IOv2::_Put_money<bool>> );
static_assert( !insertable<char, IOv2::_Put_money<const bool>> );
static_assert( !insertable<char, IOv2::_Put_money<volatile bool>> );
static_assert( !insertable<char, IOv2::_Put_money<double>> );
static_assert( !insertable<char, IOv2::_Put_money<volatile std::string>> );
static_assert( !insertable<char, IOv2::_Put_money<std::wstring>> );

// Direction: put_money inserts only, get_money extracts only. It is expressed by which member
// io_traits provides, so the stream type drops out of the probe -- and it always had to: an
// iostream satisfies istream_type and ostream_type alike, so no constraint on the stream could
// ever have carried the direction.
static_assert(  insertable <char, IOv2::_Put_money<int>> );
static_assert( !extractable<char, IOv2::_Put_money<int>> );
static_assert(  extractable<char, IOv2::_Get_money<int>> );
static_assert( !insertable <char, IOv2::_Get_money<int>> );

static_assert(  insertable <char, IOv2::_Put_money<std::string>> );
static_assert( !extractable<char, IOv2::_Put_money<std::string>> );
static_assert(  extractable<char, IOv2::_Get_money<std::string>> );
static_assert( !insertable <char, IOv2::_Get_money<std::string>> );

// The char_type has to match too: a char-stream money manipulator is not usable on a wide stream.
static_assert( !insertable <wchar_t, IOv2::_Put_money<std::string>> );
static_assert( !extractable<wchar_t, IOv2::_Get_money<std::string>> );
}
