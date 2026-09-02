// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * put_money and get_money.
 *
 * The manipulators do nothing themselves: they hand the value to the monetary
 * facet, so the digits, the grouping and the decimal mark are the locale's and
 * the amount is always in the smallest unit. What the tests below own is the
 * round trip -- whatever put_money writes, get_money must read back as the same
 * amount -- and the state rule that a stream already failed does neither.
 *
 * The static_asserts at the bottom are the larger half of the file. They pin
 * down which target types each direction accepts, and the ADL trap that made an
 * unqualified put_money(str) ambiguous with std::put_money while put_money(n)
 * compiled fine.
 */
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <support/io_traits_probe.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

namespace
{
    // Every monetary expectation here depends on a real system locale. Skipping is
    // honest where it is missing; asserting would only report the machine.
    bool have_locale(const char* name)
    {
        try { (void)locale<char>(name); }
        catch (const cvt_error&) { return false; }
        return true;
    }

    constexpr const char* kGrouped = "de_DE.ISO-8859-1";
}

// The amount is in the smallest unit, so the facet is what decides where the
// decimal mark lands and how the integer part is grouped.
TEST(IoBaseManipMoney, PutMoneyWritesTheAmountTheWayTheFacetSpellsIt)
{
    if (!have_locale(kGrouped))
        GTEST_SKIP() << kGrouped << " is not installed here";

    ostream oss{mem_device{""}, locale<char>(kGrouped)};
    oss << put_money(std::string("123456789"));

    EXPECT_TRUE(oss.good());
    auto [dev, err] = oss.detach();
    EXPECT_EQ(dev.str(), "1.234.567,89 ");
}

// Whatever put_money wrote, get_money reads back as the same amount. The rvalue
// form here needs the by-value operator>> overload.
TEST(IoBaseManipMoney, TheAmountSurvivesTheRoundTrip)
{
    if (!have_locale(kGrouped))
        GTEST_SKIP() << kGrouped << " is not installed here";

    ostream oss{mem_device{""}, locale<char>(kGrouped)};
    oss << put_money(std::string("123456789"));
    auto [written, werr] = oss.detach();

    istream iss{mem_device{written.str()}, locale<char>(kGrouped)};
    std::string back;
    iss >> get_money(back);

    EXPECT_TRUE(static_cast<bool>(iss));
    EXPECT_FALSE(iss.str_fail());
    EXPECT_EQ(back, "123456789");
}

// Integral target, and the named-lvalue form `auto g = get_money(v); is >> g;`
// which resolves to the generic extraction operator instead.
TEST(IoBaseManipMoney, GetMoneyAlsoReadsIntoAnIntegralTarget)
{
    if (!have_locale(kGrouped))
        GTEST_SKIP() << kGrouped << " is not installed here";

    istream iss{mem_device{std::string("1.234,56 ")}, locale<char>(kGrouped)};

    long long units = -1;
    auto      manip = get_money(units);
    iss >> manip;

    EXPECT_TRUE(static_cast<bool>(iss));
    EXPECT_EQ(units, 123456);
}

// A stream already in a failed state does neither direction, and leaves the
// target and the device exactly as they were.
TEST(IoBaseManipMoney, AFailedStreamDoesNothingInEitherDirection)
{
    if (!have_locale(kGrouped))
        GTEST_SKIP() << kGrouped << " is not installed here";

    {
        ostream oss{mem_device{""}, locale<char>(kGrouped)};
        oss.setstate(ios_defs::cvtfailbit);
        oss << put_money(std::string("123"));

        EXPECT_TRUE(oss.cvt_fail());
        auto [dev, err] = oss.detach();
        EXPECT_TRUE(dev.str().empty());
    }
    {
        istream iss{mem_device{std::string("1.234,56 ")}, locale<char>(kGrouped)};
        long long untouched = -1;
        iss.setstate(ios_defs::cvtfailbit);
        iss >> get_money(untouched);

        EXPECT_TRUE(iss.cvt_fail());
        EXPECT_EQ(untouched, -1);
    }
}

// A volatile integral formats exactly as the plain one: monetary::put takes it by value,
// so deduction drops the cv-qualifier.
TEST(IoBaseManipMoney, AVolatileAmountFormatsLikeAPlainOne)
{
    if (!have_locale(kGrouped))
        GTEST_SKIP() << kGrouped << " is not installed here";

    const long long    plain = 123456;
    volatile long long vol   = 123456;

    ostream oss1{mem_device{""}, locale<char>(kGrouped)};
    oss1 << put_money(plain);
    EXPECT_TRUE(oss1.good());

    ostream oss2{mem_device{""}, locale<char>(kGrouped)};
    oss2 << put_money(vol);
    EXPECT_TRUE(oss2.good());

    auto [dev1, err1] = oss1.detach();
    auto [dev2, err2] = oss2.detach();
    EXPECT_FALSE(dev1.str().empty());
    EXPECT_EQ(dev1.str(), dev2.str());
}

// The same round trip on a wide stream: the separators have to survive widening.
TEST(IoBaseManipMoney, TheRoundTripHoldsOnAWideStream)
{
    if (!have_locale(kGrouped))
        GTEST_SKIP() << kGrouped << " is not installed here";

    ostream oss{mem_device{L""}, locale<wchar_t>(kGrouped)};
    oss << put_money(std::wstring(L"123456789"));
    EXPECT_TRUE(oss.good());
    auto [written, werr] = oss.detach();
    EXPECT_EQ(written.str(), L"1.234.567,89 ");

    istream iss{mem_device{written.str()}, locale<wchar_t>(kGrouped)};
    std::wstring back;
    iss >> get_money(back);
    EXPECT_TRUE(static_cast<bool>(iss));
    EXPECT_EQ(back, L"123456789");
}

namespace
{
using MoneyIs = istream<mem_device<char>, char>;
using MoneyOs = ostream<mem_device<char>, char>;

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
static_assert(  extractable<char, get_money_t<int>> );
static_assert(  extractable<char, get_money_t<std::string>> );
static_assert( !extractable<char, get_money_t<const int>> );
static_assert( !extractable<char, get_money_t<volatile int>> );
static_assert( !extractable<char, get_money_t<const std::string>> );
static_assert( !extractable<char, get_money_t<bool>> );
static_assert( !extractable<char, get_money_t<double>> );
static_assert( !extractable<char, get_money_t<std::wstring>> );

// put_money carries no such restriction: output does not write to the target and put_money takes
// const _MoneyT&, so inserting a const lvalue stays a legitimate use.

// The io_traits bool exclusion normalizes cv, its string branch does not: put() takes the integral
// by value (cv dropped), but its string overload takes a plain const&, which volatile cannot bind.
static_assert(  insertable<char, put_money_t<int>> );
static_assert(  insertable<char, put_money_t<const int>> );
static_assert(  insertable<char, put_money_t<volatile int>> );
static_assert(  insertable<char, put_money_t<std::string>> );
static_assert( !insertable<char, put_money_t<bool>> );
static_assert( !insertable<char, put_money_t<const bool>> );
static_assert( !insertable<char, put_money_t<volatile bool>> );
static_assert( !insertable<char, put_money_t<double>> );
static_assert( !insertable<char, put_money_t<volatile std::string>> );
static_assert( !insertable<char, put_money_t<std::wstring>> );

// Direction: put_money inserts only, get_money extracts only. It is expressed by which member
// io_traits provides, so the stream type drops out of the probe -- and it always had to: an
// iostream satisfies istream_type and ostream_type alike, so no constraint on the stream could
// ever have carried the direction.
static_assert(  insertable <char, put_money_t<int>> );
static_assert( !extractable<char, put_money_t<int>> );
static_assert(  extractable<char, get_money_t<int>> );
static_assert( !insertable <char, get_money_t<int>> );

static_assert(  insertable <char, put_money_t<std::string>> );
static_assert( !extractable<char, put_money_t<std::string>> );
static_assert(  extractable<char, get_money_t<std::string>> );
static_assert( !insertable <char, get_money_t<std::string>> );

// The char_type has to match too: a char-stream money manipulator is not usable on a wide stream.
static_assert( !insertable <wchar_t, put_money_t<std::string>> );
static_assert( !extractable<wchar_t, get_money_t<std::string>> );
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
// spelling that ever broke. Every other test in this file writes put_money, which is why
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
