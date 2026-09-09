// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Shared body for the include-combination suite. Each translation unit in this
 * directory defines a different set of IOV2_COMB_* macros, includes this
 * header, and gets one test case named after its combination.
 *
 * The invariant is one sentence: for every combination of traits headers, an
 * expression either does not compile or produces the same text it would with
 * all of them included. Never a third outcome.
 *
 * That third outcome is what this suite exists to prevent. arithmetic.h used to
 * claim `const char*`, `signed char` and `unsigned char` on a char stream and
 * hand them to the numeric and pointer paths, on the assumption that
 * char_and_str.h's explicit specializations would outrank it -- an assumption
 * that only holds when char_and_str.h is also included. A translation unit that
 * included only arithmetic.h therefore compiled `os << "hello"` and printed an
 * address, with the stream still good(). Those combinations are now excluded by
 * constraint rather than by specialization ranking, so the same spelling is a
 * compile error instead.
 *
 * Single-TU tests cannot see this class of defect: they are self-consistent by
 * construction. Only varying the include set across TUs exposes it, which is
 * why the checks live here rather than in test_io_traits.cpp.
 *
 * The set of TUs is deliberately smaller than the 31 non-empty subsets. What an
 * expression resolves to depends on the *effective* set after transitive
 * includes, and nullptr.h and tm.h both pull in char_and_str.h, so the distinct
 * effective sets are far fewer than the subsets that name them. The TUs here
 * cover every distinct one.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/ostream.h>

#ifdef IOV2_COMB_TRAITS_BASE
#include <IOv2/io/traits/traits_base.h>
#endif
#ifdef IOV2_COMB_CHAR_AND_STR
#include <IOv2/io/traits/char_and_str.h>
#endif
#ifdef IOV2_COMB_ARITHMETIC
#include <IOv2/io/traits/arithmetic.h>
#endif
#ifdef IOV2_COMB_NULLPTR
#include <IOv2/io/traits/nullptr.h>
#endif
#ifdef IOV2_COMB_TM
#include <IOv2/io/traits/tm.h>
#endif

#include <string>

#include <gtest/gtest.h>

#ifndef IOV2_COMB_NAME
#error "define IOV2_COMB_NAME before including this header"
#endif

namespace
{
using os_c = IOv2::ostream<IOv2::mem_device<char>, char>;
using os_w = IOv2::ostream<IOv2::mem_device<wchar_t>, wchar_t>;

template <typename S, typename V>
concept insertable = requires (S& s, const V& v) { s << v; };

// Three outcomes are acceptable and one is not. An expression may be rejected
// at compile time, or fail at run time and say so, or produce the reference
// text on a stream that stays good(). What must never happen is the fourth:
// the wrong text on a stream that still reports success.
template <typename S, typename V>
std::string emit(const V& value)
{
    if constexpr (insertable<S, V>)
    {
        S os{typename S::device_type{}, IOv2::locale<typename S::char_type>("C")};
        os << value;
        if (!os.good())
            return "<failed>";

        const auto written = os.device().str();
        std::string out;
        for (auto c : written)
            out += (c >= 0 && c < 128) ? static_cast<char>(c) : '?';
        return out;
    }
    else
    {
        return "<rejected>";
    }
}

// Single-argument wrappers: a bare emit<os_c, V>(...) inside a gtest macro
// would have its template argument list split at the comma.
template <typename V> std::string emit_c(const V& v) { return emit<os_c, V>(v); }
template <typename V> std::string emit_w(const V& v) { return emit<os_w, V>(v); }
}

// The invariant: the text this combination produces is either the reference
// text or the rejection sentinel. Anything else -- an address where a string
// belongs, a number where a character belongs -- is the silent third outcome
// this suite exists to catch.
namespace
{
void expect_reference_or_refused(const std::string& actual,
                                 const char*        reference,
                                 const char*        cell)
{
    EXPECT_TRUE(actual == reference || actual == "<rejected>" || actual == "<failed>")
        << "cell " << cell << " produced \"" << actual
        << "\" on a good stream, which is neither the reference \"" << reference
        << "\" nor a refusal";
}
}

TEST(IoTraitsIncludeCombinations, IOV2_COMB_NAME)
{
    const signed char sbytes[] = {'h', 'i', '\0'};

    expect_reference_or_refused(emit<os_c, const char*>("hello"), "hello", "char stream <- const char*");
    expect_reference_or_refused(emit<os_c, signed char>(static_cast<signed char>('A')), "A", "char stream <- signed char");
    expect_reference_or_refused(emit<os_c, unsigned char>(static_cast<unsigned char>('B')), "B", "char stream <- unsigned char");
    expect_reference_or_refused(emit<os_c, const signed char*>(sbytes), "hi", "char stream <- const signed char*");
    expect_reference_or_refused(emit<os_w, const char*>("hello"), "hello", "wide stream <- const char*");
    expect_reference_or_refused(emit<os_c, int>(42), "42", "char stream <- int");
    // Without nullptr.h this binds to the function-pointer manipulator overload
    // instead -- nullptr converts to that pointer type -- where apply_ios_manip's
    // null check throws and the stream reports strfailbit. A refusal, not a
    // wrong answer, so the invariant holds.
    expect_reference_or_refused(emit<os_c, std::nullptr_t>(nullptr), "nullptr", "char stream <- nullptr");

#ifdef IOV2_COMB_EXPECT_CHARACTER_CELLS_REJECTED
    // Stronger than the invariant, and the actual regression lock: without
    // char_and_str.h these four must not compile at all. They used to, and
    // printed an address or a number.
    EXPECT_EQ(emit_c<const char*>("hello"), "<rejected>");
    EXPECT_EQ(emit_c<signed char>(static_cast<signed char>('A')), "<rejected>");
    EXPECT_EQ(emit_c<unsigned char>(static_cast<unsigned char>('B')), "<rejected>");
    EXPECT_EQ(emit_c<const signed char*>(sbytes), "<rejected>");
    EXPECT_EQ(emit_w<const char*>("hello"), "<rejected>");
    // nullptr is refused a different way: see the note above.
    EXPECT_EQ(emit_c<std::nullptr_t>(nullptr), "<failed>");
#endif
}
