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
// cause. With it there is no sread, so `detail::extractable` is false and the operator is simply
// not viable.
//
// Every probe here goes through io_traits rather than through `is >> get_money(x)`, so a failure
// points at the io_traits specialization itself rather than at the value-category and
// parse-context handling operator>> layers on top of it.
static_assert(  extractable<char, IOv2::get_money_t<int>> );
static_assert(  extractable<char, IOv2::get_money_t<std::string>> );
static_assert( !extractable<char, IOv2::get_money_t<const int>> );
static_assert( !extractable<char, IOv2::get_money_t<volatile int>> );
static_assert( !extractable<char, IOv2::get_money_t<const std::string>> );
static_assert( !extractable<char, IOv2::get_money_t<bool>> );
static_assert( !extractable<char, IOv2::get_money_t<double>> );
static_assert( !extractable<char, IOv2::get_money_t<std::wstring>> );

// put_money carries no such restriction: output does not write to the target and put_money takes
// const _MoneyT&, so inserting a const lvalue stays a legitimate use.

// The io_traits bool exclusion normalizes cv, its string branch does not: put() takes the integral
// by value (cv dropped), but its string overload takes a plain const&, which volatile cannot bind.
static_assert(  insertable<char, IOv2::put_money_t<int>> );
static_assert(  insertable<char, IOv2::put_money_t<const int>> );
static_assert(  insertable<char, IOv2::put_money_t<volatile int>> );
static_assert(  insertable<char, IOv2::put_money_t<std::string>> );
static_assert( !insertable<char, IOv2::put_money_t<bool>> );
static_assert( !insertable<char, IOv2::put_money_t<const bool>> );
static_assert( !insertable<char, IOv2::put_money_t<volatile bool>> );
static_assert( !insertable<char, IOv2::put_money_t<double>> );
static_assert( !insertable<char, IOv2::put_money_t<volatile std::string>> );
static_assert( !insertable<char, IOv2::put_money_t<std::wstring>> );

// Direction: put_money inserts only, get_money extracts only. It is expressed by which member
// io_traits provides, so the stream type drops out of the probe -- and it always had to: an
// iostream satisfies istream_type and ostream_type alike, so no constraint on the stream could
// ever have carried the direction.
static_assert(  insertable <char, IOv2::put_money_t<int>> );
static_assert( !extractable<char, IOv2::put_money_t<int>> );
static_assert(  extractable<char, IOv2::get_money_t<int>> );
static_assert( !insertable <char, IOv2::get_money_t<int>> );

static_assert(  insertable <char, IOv2::put_money_t<std::string>> );
static_assert( !extractable<char, IOv2::put_money_t<std::string>> );
static_assert(  extractable<char, IOv2::get_money_t<std::string>> );
static_assert( !insertable <char, IOv2::get_money_t<std::string>> );

// The char_type has to match too: a char-stream money manipulator is not usable on a wide stream.
static_assert( !insertable <wchar_t, IOv2::put_money_t<std::string>> );
static_assert( !extractable<wchar_t, IOv2::get_money_t<std::string>> );
}

// put_money / get_money are function objects rather than function templates, so that an
// unqualified call under `using namespace IOv2` stays unambiguous. With a function template the
// std overloads win a seat by ADL whenever the argument is a std::basic_string -- its associated
// namespace is std -- and neither candidate is more specialized, so the call is ambiguous. The
// user cannot dodge it either: <iomanip> arrives transitively through io/io_manip.h, so
// std::put_money is always declared. Integral arguments never had the problem (a fundamental
// type has no associated namespace), which is what made the failure so lopsided: put_money(n)
// compiled while put_money(str) did not, in the same translation unit.
//
// These probes must be unqualified and must sit under a using-directive, since that is the only
// spelling that ever broke. Every other test in this file writes IOv2::put_money, which is why
// the gap went unnoticed.
namespace adl_probe
{
using namespace IOv2;

template <typename T> concept unqualified_put = requires (T& v) { put_money(v); };
template <typename T> concept unqualified_get = requires (T& v) { get_money(v); };

static_assert( unqualified_put<std::string> );      // was ambiguous with std::put_money
static_assert( unqualified_get<std::string> );      // was ambiguous with std::get_money
static_assert( unqualified_put<std::wstring> );     // the factory is unconstrained; io_traits rejects it later
static_assert( unqualified_get<std::wstring> );
static_assert( unqualified_put<long> );             // never affected, kept as the control
static_assert( unqualified_get<long> );
}
