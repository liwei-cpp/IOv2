// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

// Class-template-argument-deduction probes for istream / ostream / iostream.
//
// The locale-taking constructors are constrained, and that constraint is what carries the
// "the locale's character type must match the stream's" rule. It deliberately lives on the
// constructor rather than on a deduction guide: the implicit guide a constructor generates
// inherits its constraints, so one `requires` covers CTAD *and* explicitly-written template
// arguments. A guide alone covers only the former -- and a guide alone is exactly what this
// used to be, which made the constraint dead code: the implicit guide from the unconstrained
// constructor deduced TChar from the locale and accepted a mismatched pair, and the failure
// then surfaced as a unique_ptr error deep inside runtime_cvt rather than at the call.
//
// These are compile-time only: every check below is a static_assert, so the suite passes by
// compiling. The gtest header is included so the suite is built and registered like the rest,
// and one runtime case is there to say out loud that the file ran.
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

namespace
{
using MD = IOv2::mem_device<char>;
// A char device plus a code converter yields a *wide* stream: the stream's character type comes
// from the converter pipeline, not from the device. That is why the three-argument constraint
// asks the pipeline rather than the device.
using CC = IOv2::code_cvt_creator<char, wchar_t>;

template <typename L> concept ctad_i2 = requires (MD d, L l)       { IOv2::istream {d, l};    };
template <typename L> concept ctad_o2 = requires (MD d, L l)       { IOv2::ostream {d, l};    };
template <typename L> concept ctad_s2 = requires (MD d, L l)       { IOv2::iostream{d, l};    };
template <typename L> concept ctad_i3 = requires (MD d, CC c, L l) { IOv2::istream {d, c, l}; };
template <typename L> concept ctad_o3 = requires (MD d, CC c, L l) { IOv2::ostream {d, c, l}; };
template <typename L> concept ctad_s3 = requires (MD d, CC c, L l) { IOv2::iostream{d, c, l}; };

// (device, locale): the locale must agree with the device's char_type.
static_assert(  ctad_i2<IOv2::locale<char>>    );
static_assert(  ctad_o2<IOv2::locale<char>>    );
static_assert(  ctad_s2<IOv2::locale<char>>    );
static_assert( !ctad_i2<IOv2::locale<wchar_t>> );
static_assert( !ctad_o2<IOv2::locale<wchar_t>> );
static_assert( !ctad_s2<IOv2::locale<wchar_t>> );

// (device, creator, locale): the locale must agree with what the pipeline produces, which here
// is wchar_t even though the device is char. Constraining against the device would reject this
// legitimate combination and accept the bogus one, so the two cases are both pinned.
static_assert(  ctad_i3<IOv2::locale<wchar_t>> );
static_assert(  ctad_o3<IOv2::locale<wchar_t>> );
static_assert(  ctad_s3<IOv2::locale<wchar_t>> );
static_assert( !ctad_i3<IOv2::locale<char>>    );
static_assert( !ctad_o3<IOv2::locale<char>>    );
static_assert( !ctad_s3<IOv2::locale<char>>    );

// All four forms still deduce, and deduce the right thing. The first two have no locale to
// deduce TChar from, so they are the two that still need an explicit deduction guide.
static_assert( std::is_same_v<decltype(IOv2::istream{std::declval<MD>()}),
                              IOv2::istream<MD, char>> );
static_assert( std::is_same_v<decltype(IOv2::istream{std::declval<MD>(), std::declval<CC>()}),
                              IOv2::istream<MD, wchar_t>> );
static_assert( std::is_same_v<decltype(IOv2::istream{std::declval<MD>(),
                                                     std::declval<IOv2::locale<char>>()}),
                              IOv2::istream<MD, char>> );
static_assert( std::is_same_v<decltype(IOv2::istream{std::declval<MD>(), std::declval<CC>(),
                                                     std::declval<IOv2::locale<wchar_t>>()}),
                              IOv2::istream<MD, wchar_t>> );

// The constraint also covers the path CTAD never sees: spelling the template arguments out.
// Without it this compiled here and failed inside runtime_cvt instead.
static_assert( !std::is_constructible_v<IOv2::istream<MD, wchar_t>, MD, IOv2::locale<wchar_t>> );
static_assert( !std::is_constructible_v<IOv2::ostream<MD, wchar_t>, MD, IOv2::locale<wchar_t>> );
static_assert(  std::is_constructible_v<IOv2::istream<MD, char>,    MD, IOv2::locale<char>>    );
static_assert(  std::is_constructible_v<IOv2::ostream<MD, char>,    MD, IOv2::locale<char>>    );
}

// The static_asserts above are the test. This case exists so the suite reports a result
// rather than an empty run.
TEST(StreamCtad, EveryDeductionRuleHoldsAtCompileTime)
{
    SUCCEED() << "all deduction and constraint checks are static_asserts in this file";
}
