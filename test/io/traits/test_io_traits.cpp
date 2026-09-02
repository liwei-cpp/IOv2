// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

// Compile-time regression coverage for the io_traits extension point.
//
// Everything here is a static_assert; the single runtime case only reports that the translation
// unit compiled at all -- which is itself the point of the first block.
//
// The load-bearing assumption of the whole design is that naming a member of an *incomplete*
// io_traits inside a requires-expression is a SFINAE-able substitution failure yielding `false`,
// not a hard error. The primary io_traits template is deliberately left undefined (see
// io/traits/traits_base.h), so every "this type is not streamable" answer in the library flows
// through that rule. If a compiler ever declined to treat it as SFINAE-able, the whole chain
// would collapse into hard errors and this file would stop compiling.
//
// The rest pins the detection contract: operator<< / operator>> are constrained by
// detail::insertable / detail::extractable, and the dispatch inside their bodies reuses those
// same concepts, so `requires { os << x; }` is the supported public way to ask "can this be
// streamed". These assertions are what keeps that answer honest.

#include <cstddef>
#include <ctime>
#include <string>
#include <type_traits>

#include <IOv2/common/streambuf_defs.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/io/traits/nullptr.h>
#include <IOv2/io/traits/tm.h>
#include <IOv2/io/utilities/istream_operators.h>
#include <IOv2/io/utilities/ostream_operators.h>

#include <gtest/gtest.h>

namespace
{
using os_c = IOv2::ostream<IOv2::mem_device<char>, char>;
using is_c = IOv2::istream<IOv2::mem_device<char>, char>;
using ios_c = IOv2::iostream<IOv2::mem_device<char>, char>;
using os_w = IOv2::ostream<IOv2::mem_device<wchar_t>, wchar_t>;

// A type with no io_traits specialization anywhere: io_traits<char, no_traits_t> is an
// incomplete type at every point below.
struct no_traits_t {};

// The public way to ask. TValue is the type as deduced by the operator, so an lvalue target is
// spelled `V&` and an rvalue target `V` -- value category takes part in the answer on the
// extraction side.
template <typename S, typename V>
concept insertable = requires (S& s, const V& v) { s << v; };

template <typename S, typename V>
concept extractable_lvalue = requires (S& s, V& v) { s >> v; };

template <typename S, typename V>
concept extractable_const = requires (S& s, const V& v) { s >> v; };

template <typename S, typename V>
concept extractable_rvalue = requires (S& s, V (*make)()) { s >> make(); };

// ---------------------------------------------------------------------------------------------
// 1. The incomplete-type SFINAE rule the design rests on.
//
// These must be false rather than ill-formed. Both the concepts and the operator expressions are
// asserted: the concepts name io_traits<...>::swrite directly, the operator expressions reach it
// through the requires-clause, and both have to degrade to `false`.
// ---------------------------------------------------------------------------------------------
static_assert( !IOv2::detail::insertable<os_c, no_traits_t> );
static_assert( !IOv2::detail::extractable<is_c, no_traits_t&> );
static_assert( !insertable<os_c, no_traits_t> );
static_assert( !extractable_lvalue<is_c, no_traits_t> );

// The same rule, asked of a type that is streamable in the other direction: io_traits<char, ws_t>
// is complete but has no swrite, so the failure is a missing member rather than an incomplete
// type. Both paths must yield `false` too.
static_assert( !insertable<os_c, IOv2::ws_t> );
static_assert( !extractable_rvalue<is_c, IOv2::endl_t> );

// ---------------------------------------------------------------------------------------------
// 2. Direction is decided by which member exists, not by the stream type.
//
// An iostream satisfies both istream_type and ostream_type, so it is the sharpest test that the
// direction really comes from io_traits and not from a constraint on the stream.
// ---------------------------------------------------------------------------------------------
static_assert(  insertable<ios_c, IOv2::endl_t>  && !extractable_rvalue<is_c, IOv2::endl_t>  );
static_assert(  insertable<ios_c, IOv2::ends_t>  && !extractable_rvalue<is_c, IOv2::ends_t>  );
static_assert(  insertable<ios_c, IOv2::flush_t> && !extractable_rvalue<is_c, IOv2::flush_t> );
static_assert(  extractable_rvalue<ios_c, IOv2::ws_t> && !insertable<os_c, IOv2::ws_t> );

// nullptr is insertion-only, so the extraction concept rejects it.
static_assert(  insertable<os_c, std::nullptr_t> );
static_assert( !IOv2::detail::extractable<is_c, std::nullptr_t&> );

// ...and `is >> nullptr` does not compile either. A null pointer constant converts to the
// function-pointer manipulator overload, whose parameter has to stay a non-deduced context, so no
// constraint on it can exclude one. The deleted `operator>>(T&, std::nullptr_t)` is what closes
// that hole and keeps the diagnosis at compile time instead of at run time (strfailbit).
static_assert( !extractable_lvalue<is_c, std::nullptr_t> );
static_assert( !extractable_rvalue<is_c, std::nullptr_t> );

// The literal spellings have to be pinned as expressions rather than as types: only a literal is a
// null pointer constant, and a concept parameterized on the target type cannot carry one. They are
// asked through a concept on the *stream* type so the requires-expression stays dependent -- a
// non-dependent one is ill-formed for any invalid requirement ([expr.prim.req]) instead of `false`,
// which is why these cannot simply be inlined into the static_assert.
template <typename S> concept extracts_literal_zero    = requires (S& s) { s >> 0; };
template <typename S> concept extracts_literal_nullptr = requires (S& s) { s >> nullptr; };
static_assert( !extracts_literal_zero<is_c> );     // ambiguous: two equal-rank pointer conversions
static_assert( !extracts_literal_nullptr<is_c> );  // exact match on the deleted overload

// The manipulator overload itself is untouched: a function pointer, and a function lvalue that
// decays to one, both still reach it, in either direction.
static_assert(  extractable_lvalue<is_c, void (*)(IOv2::ios_base<char>&)> );
static_assert(  extractable_lvalue<is_c, void (IOv2::ios_base<char>&)> );
static_assert(  insertable<os_c, void (*)(IOv2::ios_base<char>&)> );
static_assert(  insertable<os_c, void (IOv2::ios_base<char>&)> );

// A function pointer with any other signature has nowhere to go: io_traits' pointer specialization
// excludes pointees that are functions, so a manipulator written with its argument list left off is
// a compile error rather than the `1` a boolean conversion would have printed. Spelled as an
// expression as well as a type, for the same reason as the null-pointer literals above.
template <typename S> concept inserts_bare_setw = requires (S& s) { s << IOv2::setw; };
static_assert( !inserts_bare_setw<os_c> );
static_assert( !insertable<os_c, decltype(IOv2::setw)>  );
static_assert( !insertable<os_c, decltype(&IOv2::setw)> );
static_assert( !insertable<os_c, int (*)(double)> );

// Only pointees that are functions were taken out; object pointers keep the address path.
static_assert(  insertable<os_c, const void*> );
static_assert(  insertable<os_c, int*> );

// The two-way manipulators work in both directions.
static_assert(  insertable<os_c, IOv2::setw_t> && extractable_rvalue<is_c, IOv2::setw_t> );

// ---------------------------------------------------------------------------------------------
// 3. Char-type mismatches are rejected, mirroring the overloads the standard deletes.
//
// The asymmetry is deliberate and matches the standard: a narrow character or string widens into
// a stream of any character type, but a wide one never narrows into a `char` stream.
// ---------------------------------------------------------------------------------------------
static_assert( !insertable<os_c, wchar_t>  && !extractable_lvalue<is_c, wchar_t>  );
static_assert( !insertable<os_c, char16_t> && !extractable_lvalue<is_c, char16_t> );
static_assert(  insertable<os_w, char>     && !extractable_lvalue<os_w, char>     );
static_assert(  insertable<os_w, const char*> );

// A fill character whose type differs from the stream's char_type. The old operator()-based
// manipulators could not express this at declaration level; the member constraint can.
static_assert( !insertable<os_c, IOv2::setfill_t<wchar_t>> );
static_assert(  insertable<os_c, IOv2::setfill_t<char>>    );
static_assert( !insertable<os_w, IOv2::setfill_t<char>>    );

// ---------------------------------------------------------------------------------------------
// 4. The insertion side decays once, the extraction side never does.
//
// Decay is what lets `os << "hello"` work. Extraction must not decay: it writes back through a
// reference, and decaying an array target to a pointer would throw away its length.
// ---------------------------------------------------------------------------------------------
static_assert(  insertable<os_c, char[6]>      );
static_assert(  insertable<os_c, const char*>  );
static_assert(  extractable_lvalue<is_c, char[8]> );

// ---------------------------------------------------------------------------------------------
// 5. Value category decides whether a target is writable.
//
// An rvalue target is probed -- and passed to sread -- as a const lvalue, so `is >> 5` and
// `is >> const_obj` are rejected, while a prvalue manipulator such as setw(5), whose sread takes
// a const reference and does not modify it, still goes through.
// ---------------------------------------------------------------------------------------------
static_assert(  extractable_lvalue<is_c, int> );
static_assert( !extractable_rvalue<is_c, int> );
static_assert( !extractable_const <is_c, int> );
static_assert(  extractable_rvalue<is_c, IOv2::setw_t> );

// ---------------------------------------------------------------------------------------------
// 6. The parse-context relay is transparent to detection.
//
// std::tm parses through a context type rather than into itself, which is a separate rung of the
// extraction concept; it has to report the same way as the direct rung.
// ---------------------------------------------------------------------------------------------
static_assert(  extractable_lvalue<is_c, std::tm> );
static_assert(  insertable<os_c, std::tm> );

// ---------------------------------------------------------------------------------------------
// 7. The iterator aliases the concepts probe with are the ones the operators really use.
//
// o_iter() / i_iter() have their return types spelled as these aliases, so a mismatch is already
// a compile error at their definition. This only pins the shape, so that a stream cannot satisfy
// ostream_type / istream_type with an alias that is not a buffer iterator at all.
// ---------------------------------------------------------------------------------------------
static_assert( IOv2::is_ostreambuf_iterator<typename os_c::out_iter_type> );
static_assert( IOv2::is_istreambuf_iterator<typename is_c::in_iter_type>  );
static_assert( IOv2::is_ostreambuf_iterator<typename ios_c::out_iter_type> );
static_assert( IOv2::is_istreambuf_iterator<typename ios_c::in_iter_type>  );
static_assert( std::is_same_v<typename os_c::out_iter_type::value_type, char> );
static_assert( std::is_same_v<typename is_c::in_iter_type::value_type, char>  );

// ---------------------------------------------------------------------------------------------
// 8. The stream form ties TChar to the stream's char_type.
//
// The iterator form has always tied TChar to the iterator's value_type (arithmetic.h). The stream
// form left the two unrelated, so an explicitly qualified call could pair a wide key with a narrow
// stream -- unreachable through the operators, which always instantiate with T::char_type, but
// reachable by hand and answered at run time (strfailbit) rather than at compile time.
// ---------------------------------------------------------------------------------------------
template <typename TChar, typename S, typename V>
concept swrites_as = requires (S& s, const V& v) { IOv2::io_traits<TChar, V>::swrite(s, v); };

template <typename TChar, typename S, typename V>
concept sreads_as = requires (S& s, const V& v) { IOv2::io_traits<TChar, V>::sread(s, v); };

// The two that read TChar: endl widens through ctype<TChar>, ends writes a TChar().
static_assert(  swrites_as<char,    os_c, IOv2::endl_t>  );
static_assert( !swrites_as<wchar_t, os_c, IOv2::endl_t>  );
static_assert(  swrites_as<wchar_t, os_w, IOv2::endl_t>  );
static_assert(  swrites_as<char,    os_c, IOv2::ends_t>  );
static_assert( !swrites_as<wchar_t, os_c, IOv2::ends_t>  );

// The rest never name TChar, so the mismatch was inert; they are constrained for symmetry.
static_assert(  swrites_as<char,    os_c, IOv2::flush_t> );
static_assert( !swrites_as<wchar_t, os_c, IOv2::flush_t> );
static_assert(  sreads_as <char,    is_c, IOv2::ws_t>    );
static_assert( !sreads_as <wchar_t, is_c, IOv2::ws_t>    );
static_assert(  swrites_as<char,    os_c, IOv2::setw_t>  );
static_assert( !swrites_as<wchar_t, os_c, IOv2::setw_t>  );
static_assert(  sreads_as <char,    is_c, IOv2::setw_t>  );
static_assert( !sreads_as <wchar_t, is_c, IOv2::setw_t>  );

// setfill already tied T::char_type to TFill; TChar is now tied too, so all three must agree.
static_assert(  swrites_as<char,    os_c, IOv2::setfill_t<char>> );
static_assert( !swrites_as<wchar_t, os_c, IOv2::setfill_t<char>> );

// ---------------------------------------------------------------------------------------------
// 9. A key carrying top-level cv routes exactly like its unqualified spelling.
//
// operator<< deduces TValue from `const TValue&`, so top-level const can never reach io_traits --
// only volatile can. The arithmetic specialization used to take it: is_arithmetic_v is
// cv-insensitive while its !is_same_v<TValue, char> exclusions are not, so a volatile character
// type slipped past every exclusion and came out as a number, and `volatile wchar_t` reached a
// narrow stream at all. That specialization now declines any key with top-level cv and lets the
// operator's decayed rung re-run the whole partial ordering, which is the only thing that can
// reproduce the answer given for the unqualified key -- an exclusion list cannot, because the
// explicit specializations in char_and_str.h win by partial ordering and are deliberately not on
// it (see the note at the top of arithmetic.h).
// ---------------------------------------------------------------------------------------------
// Class types are rejected outright, by a different mechanism than the scalars below and matching
// what std::ostream does: the decayed rung finds the right specialization, but binding a volatile
// class lvalue to the member's `const MyType&` would drop the volatile and is ill-formed. A scalar
// escapes that because lvalue-to-rvalue conversion reads it once into a temporary; a class object
// would have to be read member by member, which is the same multi-pass problem the pointer rows
// below describe. The `const volatile` spellings are not a separate path -- `operator<<` deduces
// from `const TValue&`, so they arrive as plain `volatile`.
static_assert( !insertable<os_c, volatile IOv2::setw_t> );
static_assert( !insertable<os_c, volatile IOv2::setfill_t<char>> );
static_assert( !insertable<os_c, const volatile IOv2::endl_t> );
static_assert( !insertable<os_c, volatile std::string> );
static_assert( !insertable<os_c, const volatile std::string> );
static_assert(  insertable<os_c, std::tm> );
static_assert( !insertable<os_c, volatile std::tm> );
static_assert( !insertable<os_c, const volatile std::tm> );

// The char-type isolation of section 3 holds under volatile too. It did not before: the wide
// character types were reaching the arithmetic specialization and being written as numbers.
static_assert( !insertable<os_c, volatile wchar_t>  );
static_assert( !insertable<os_c, volatile char16_t> );
static_assert(  insertable<os_w, volatile char>     );

// Scalars do go through, because lvalue-to-rvalue conversion strips the volatile and materializes
// a temporary for the member's `const TValue&`. Which specialization they land in has to be the
// one the unqualified key lands in -- char_and_str.h for the character types, arithmetic.h for the
// rest -- so `volatile char` writes a character and `volatile signed char` writes a number.
static_assert(  insertable<os_c, volatile char>          );
static_assert(  insertable<os_c, volatile signed char>   );
static_assert(  insertable<os_c, volatile unsigned char> );
static_assert(  insertable<os_c, volatile int>           );
static_assert(  insertable<os_c, volatile bool>          );
static_assert(  insertable<os_c, int* volatile>          );

// A volatile pointee is the one place cv on the *pointee* changes the answer, and it changes it
// the other way: the pointer specialization's exclusion list keys on remove_const_t, so a volatile
// character pointer is not treated as a string pointer and takes the address path. Printing the
// string would need two passes -- find the terminator, then copy -- over memory that may change
// between them, and setw needs the length before the first character is written, so the two cannot
// be folded into one. The pointer value is not itself volatile, so printing it reads nothing.
// Only these four rows moved; the unqualified and const-qualified spellings are untouched.
static_assert(  insertable<os_w, wchar_t*>                );  // string path, as before
static_assert(  insertable<os_w, const wchar_t*>          );  // string path, as before
static_assert( !insertable<os_c, const wchar_t*>          );  // still the deleted overload
static_assert( !insertable<os_w, char16_t*>               );  // still the deleted overload
static_assert(  insertable<os_w, volatile wchar_t*>       );  // address path -- was uninsertable
static_assert(  insertable<os_w, const volatile wchar_t*> );
static_assert(  insertable<os_c, volatile wchar_t*>       );
static_assert(  insertable<os_w, volatile char16_t*>      );

// Top-level cv on a pointer key is the other axis, and the pointer specialization declines it for
// the same reason the arithmetic one does: is_pointer_v ignores top-level cv while the string
// specializations in char_and_str.h do not, so `char* volatile` was answered with an address where
// the unqualified `char*` gives the contents. What that costs is invisible to `insertable` -- both
// spellings were and remain insertable, only the specialization answering them changes -- so the
// character rows are pinned at run time in test_ostream_inserters_arithmetic_char case 12. What is
// checkable here is that nothing lost its answer on the way, and that the deleted overloads still
// survive the decayed rung.
static_assert(  insertable<os_c, char* volatile>           );
static_assert(  insertable<os_c, const char* volatile>     );
static_assert(  insertable<os_c, signed char* volatile>    );
static_assert(  insertable<os_c, unsigned char* volatile>  );
static_assert(  insertable<os_c, void* volatile>           );
static_assert(  insertable<os_w, wchar_t* volatile>        );
static_assert( !insertable<os_c, wchar_t* volatile>        );
static_assert( !insertable<os_c, const wchar_t* volatile>  );
static_assert( !insertable<os_w, char16_t* volatile>       );

// The extraction side has never taken a volatile target, matching the standard, which provides no
// `operator>>` for one either.
static_assert( !extractable_lvalue<is_c, volatile int>   );
static_assert( !extractable_lvalue<is_c, volatile void*> );
}

// The static_asserts above are the test; compiling this file is what passes it. This case
// exists so the suite reports a result rather than an empty run.
TEST(IoTraits, EveryDetectionRuleHoldsAtCompileTime)
{
    SUCCEED() << "all io_traits detection checks are static_asserts in this file";
}
