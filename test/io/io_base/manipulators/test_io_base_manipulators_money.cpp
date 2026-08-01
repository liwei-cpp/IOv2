#include <limits>
#include <stdexcept>
#include <system_error>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/char_and_str.h>
#include <io/fp_defs/arithmetic.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <support/dump_info.h>
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

template <typename T> concept money_readable =
    requires (MoneyIs& is, T& v) { is >> IOv2::get_money(v); };
template <typename T> concept money_writable =
    requires (MoneyOs& os, const T& v) { os << IOv2::put_money(v); };

// get_money deduces its target type from the argument, so a const lvalue deduces `const int`
// -- and std::integral<const int> is true, since std::is_integral_v ignores cv-qualification.
// Without the cv exclusion on the reader, such a target selected the reader and then failed
// deep inside the monetary facet on an assignment to a read-only reference, burying the real
// cause. With it there is no reader, the call lands on the fallback operator>>, and its
// static_assert says the target is not a modifiable lvalue.
//
// The check is on the reader's own constraint rather than on `is >> get_money(x)`, because the
// fallback operator>> remains viable at overload resolution -- its static_assert fires only when
// the body is instantiated -- so probing the expression reports true whatever the target is.
static_assert(  IOv2::is_reader_def<char, IOv2::_Get_money<int>> );
static_assert(  IOv2::is_reader_def<char, IOv2::_Get_money<std::string>> );
static_assert( !IOv2::is_reader_def<char, IOv2::_Get_money<const int>> );
static_assert( !IOv2::is_reader_def<char, IOv2::_Get_money<volatile int>> );
static_assert( !IOv2::is_reader_def<char, IOv2::_Get_money<const std::string>> );
static_assert( !IOv2::is_reader_def<char, IOv2::_Get_money<bool>> );
static_assert( !IOv2::is_reader_def<char, IOv2::_Get_money<double>> );
static_assert( !IOv2::is_reader_def<char, IOv2::_Get_money<std::wstring>> );

// put_money carries no such restriction: output does not write to the target and put_money takes
// const _MoneyT&, so inserting a const lvalue stays a legitimate use.
static_assert(  money_writable<int> );
static_assert(  money_readable<int> );

// The writer's bool exclusion normalizes cv, its string branch does not: put() takes the integral
// by value (cv dropped), but its string overload takes a plain const&, which volatile cannot bind.
static_assert(  IOv2::is_writer_def<char, IOv2::_Put_money<int>> );
static_assert(  IOv2::is_writer_def<char, IOv2::_Put_money<const int>> );
static_assert(  IOv2::is_writer_def<char, IOv2::_Put_money<volatile int>> );
static_assert(  IOv2::is_writer_def<char, IOv2::_Put_money<std::string>> );
static_assert( !IOv2::is_writer_def<char, IOv2::_Put_money<bool>> );
static_assert( !IOv2::is_writer_def<char, IOv2::_Put_money<const bool>> );
static_assert( !IOv2::is_writer_def<char, IOv2::_Put_money<volatile bool>> );
static_assert( !IOv2::is_writer_def<char, IOv2::_Put_money<double>> );
static_assert( !IOv2::is_writer_def<char, IOv2::_Put_money<volatile std::string>> );
static_assert( !IOv2::is_writer_def<char, IOv2::_Put_money<std::wstring>> );

// Direction: put_money inserts only, get_money extracts only, enforced by a deleted overload
// per carrier. Here the expression is the right thing to probe -- unlike the cv cases above,
// what is being checked is exactly that the catch-all fallback no longer swallows the call.
using MoneyIos = IOv2::iostream<IOv2::mem_device<char>, char>;

// The probes go through templates because a requires-expression whose requirements depend on no
// template parameter is checked where it is written, and there selecting the deleted overload is
// a plain error rather than a substitution failure.
template <typename TStream, typename T> concept inserts_put_money =
    requires (TStream& s, T& v) { s << IOv2::put_money(v); };
template <typename TStream, typename T> concept extracts_put_money =
    requires (TStream& s, T& v) { s >> IOv2::put_money(v); };
template <typename TStream, typename T> concept extracts_get_money =
    requires (TStream& s, T& v) { s >> IOv2::get_money(v); };
template <typename TStream, typename T> concept inserts_get_money =
    requires (TStream& s, T& v) { s << IOv2::get_money(v); };

static_assert(  inserts_put_money <MoneyOs, int> );
static_assert( !extracts_put_money<MoneyIs, int> );
static_assert(  extracts_get_money<MoneyIs, int> );
static_assert( !inserts_get_money <MoneyOs, int> );

static_assert(  inserts_put_money <MoneyOs, std::string> );
static_assert( !extracts_put_money<MoneyIs, std::string> );
static_assert(  extracts_get_money<MoneyIs, std::string> );
static_assert( !inserts_get_money <MoneyOs, std::string> );

// The named-lvalue form resolves through a different candidate than the prvalue form, so it
// needs its own check.
template <typename TStream, typename TManip> concept extracts_manip =
    requires (TStream& s, TManip m) { s >> m; };
template <typename TStream, typename TManip> concept inserts_manip =
    requires (TStream& s, TManip m) { s << m; };

static_assert( !extracts_manip<MoneyIs, IOv2::_Put_money<int>> );
static_assert( !inserts_manip <MoneyOs, IOv2::_Get_money<int>> );

// The bidirectional stream is the case the direction check exists for: it satisfies both
// istream_type and ostream_type, so nothing but the deletion rules the wrong direction out.
static_assert(  inserts_put_money <MoneyIos, int> );
static_assert( !extracts_put_money<MoneyIos, int> );
static_assert(  extracts_get_money<MoneyIos, int> );
static_assert( !inserts_get_money <MoneyIos, int> );
}
