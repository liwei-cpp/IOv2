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
#include <limits>
#include <stdfloat>
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

// Fixtures for section 6. They specialize templates in namespace IOv2, so they sit outside the
// unnamed namespace below. Each target names a context obtainable a different way, or not at all.
namespace ctx_fixture
{
struct via_default {};
struct via_maker   {};
struct via_neither {};
struct via_convertible {};

struct ctx_default
{
    int n = 0;
    void convert_to(via_default& ) const {}
};
struct ctx_maker
{
    int n;
    explicit ctx_maker(int seed) : n(seed) {}
    ctx_maker() = delete;
    void convert_to(via_maker& ) const {}
};
struct ctx_neither
{
    int n;
    explicit ctx_neither(int seed) : n(seed) {}
    ctx_neither() = delete;
    void convert_to(via_neither& ) const {}
};

// A maker may build the context out of whatever it likes internally, but must hand back the
// context type itself.
struct ctx_seed
{
    int n;
};
struct ctx_convertible
{
    int n;
    ctx_convertible(ctx_seed s) : n(s.n) {}
    ctx_convertible() = delete;
    void convert_to(via_convertible& ) const {}
};
}

namespace IOv2
{
template <typename TChar>
struct parse_context_type<TChar, ctx_fixture::via_default>
{ using type = ctx_fixture::ctx_default; };

template <typename TChar>
struct parse_context_type<TChar, ctx_fixture::via_maker>
{
    using type = ctx_fixture::ctx_maker;
    static type make_parse_context(const ctx_fixture::via_maker&) { return type{0}; }
};

template <typename TChar>
struct parse_context_type<TChar, ctx_fixture::via_neither>
{ using type = ctx_fixture::ctx_neither; };

template <typename TChar>
struct parse_context_type<TChar, ctx_fixture::via_convertible>
{
    using type = ctx_fixture::ctx_convertible;
    static type make_parse_context(const ctx_fixture::via_convertible&)
    { return type{ctx_fixture::ctx_seed{0}}; }
};

template <typename TChar, typename TCtx>
    requires (std::is_same_v<TCtx, ctx_fixture::ctx_default>
              || std::is_same_v<TCtx, ctx_fixture::ctx_maker>
              || std::is_same_v<TCtx, ctx_fixture::ctx_neither>
              || std::is_same_v<TCtx, ctx_fixture::ctx_convertible>)
struct io_traits<TChar, TCtx>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter it, TSent end, ios_base<TChar>& io, const locale<TChar>& loc, TCtx& c)
    {
        return io_traits<TChar, int>::sread(it, end, io, loc, c.n);
    }
};
}

// Fixtures for section 10. Each names a differently shaped make_parse_context; the section only
// evaluates predicates on them, never an extraction, so shapes the operator would reject with a
// static_assert are safe to declare here.
namespace maker_shape
{
struct ctx  { int n = 0; void convert_to(struct target&) const; };
struct target { int n = 0; };
struct seed { int copy; seed(const target& t) : copy(t.n) {} };

struct ok        {};   // static type f(const T&)
struct ok_nx     {};   // ... noexcept -- part of the function type since C++17
struct nonconst  {};   // f(T&)        -- caught before this round too
struct rvalue    {};   // f(T&&)       -- used to be skipped silently
struct nonstatic {};   // member       -- used to be skipped silently
struct middleman {};   // f(const U&)  -- used to dangle
struct voidret   {};   // static void f(const T&)
struct absent    {};   // no such member at all

// The rest of the shapes the contract names. Detection has to see every one of them -- a shape
// it misses is silently default constructed, which is the failure the name/shape split removes --
// and the shape check has to reject all but the inherited one, which the contract allows.
struct other      {};  // a second key, only so `overloaded` has something to overload on
struct templated  {};  // template <class U> static type f(const U&)
struct overloaded {};  // two overloads
struct priv       {};  // private
struct deleted    {};  // = delete
struct byvalue    {};  // f(T)          -- the parameter must be const T&, not a copy
struct crvalue    {};  // f(const T&&)
struct datamember {};  // a data member wearing the name
struct nestedtype {};  // a nested type wearing the name
struct inherited  {};  // the exact shape, but reached through a base -- must pass

template <typename T>
struct maker_base { static ctx make_parse_context(const T&) { return {}; } };
}

namespace IOv2
{
template <typename TChar> struct parse_context_type<TChar, maker_shape::ok>
{ using type = maker_shape::ctx;
  static type make_parse_context(const maker_shape::ok&) { return {}; } };

template <typename TChar> struct parse_context_type<TChar, maker_shape::ok_nx>
{ using type = maker_shape::ctx;
  static type make_parse_context(const maker_shape::ok_nx&) noexcept { return {}; } };

template <typename TChar> struct parse_context_type<TChar, maker_shape::nonconst>
{ using type = maker_shape::ctx;
  static type make_parse_context(maker_shape::nonconst&) { return {}; } };

template <typename TChar> struct parse_context_type<TChar, maker_shape::rvalue>
{ using type = maker_shape::ctx;
  static type make_parse_context(maker_shape::rvalue&&) { return {}; } };

template <typename TChar> struct parse_context_type<TChar, maker_shape::nonstatic>
{ using type = maker_shape::ctx;
  type make_parse_context(const maker_shape::nonstatic&) const { return {}; } };

template <typename TChar> struct parse_context_type<TChar, maker_shape::middleman>
{ using type = maker_shape::ctx;
  static type make_parse_context(const maker_shape::seed&) { return {}; } };

template <typename TChar> struct parse_context_type<TChar, maker_shape::voidret>
{ using type = maker_shape::ctx;
  static void make_parse_context(const maker_shape::voidret&) {} };

template <typename TChar> struct parse_context_type<TChar, maker_shape::templated>
{ using type = maker_shape::ctx;
  template <typename U> static type make_parse_context(const U&) { return {}; } };

template <typename TChar> struct parse_context_type<TChar, maker_shape::overloaded>
{ using type = maker_shape::ctx;
  static type make_parse_context(const maker_shape::overloaded&) { return {}; }
  static type make_parse_context(const maker_shape::other&)      { return {}; } };

template <typename TChar> struct parse_context_type<TChar, maker_shape::priv>
{ using type = maker_shape::ctx;
private:
  static type make_parse_context(const maker_shape::priv&) { return {}; } };

template <typename TChar> struct parse_context_type<TChar, maker_shape::deleted>
{ using type = maker_shape::ctx;
  static type make_parse_context(const maker_shape::deleted&) = delete; };

template <typename TChar> struct parse_context_type<TChar, maker_shape::byvalue>
{ using type = maker_shape::ctx;
  static type make_parse_context(maker_shape::byvalue) { return {}; } };

template <typename TChar> struct parse_context_type<TChar, maker_shape::crvalue>
{ using type = maker_shape::ctx;
  static type make_parse_context(const maker_shape::crvalue&&) { return {}; } };

template <typename TChar> struct parse_context_type<TChar, maker_shape::datamember>
{ using type = maker_shape::ctx;
  int make_parse_context = 0; };

template <typename TChar> struct parse_context_type<TChar, maker_shape::nestedtype>
{ using type = maker_shape::ctx;
  struct make_parse_context {}; };

template <typename TChar>
struct parse_context_type<TChar, maker_shape::inherited> : maker_shape::maker_base<maker_shape::inherited>
{ using type = maker_shape::ctx; };

template <typename TChar> struct parse_context_type<TChar, maker_shape::absent>
{ using type = maker_shape::ctx; };
}

// Fixture for section 12: the one shape that reaches char_sink_for's is_void_v disjunct.
namespace sink_shape
{
struct traits_void_sink
{
    traits_void_sink& operator*()      { return *this; }
    traits_void_sink& operator++()     { return *this; }
    traits_void_sink  operator++(int)  { return *this; }
    traits_void_sink& operator=(char)  { return *this; }
};
}

template <>
struct std::iterator_traits<sink_shape::traits_void_sink>
{
    using iterator_category = std::output_iterator_tag;
    using value_type        = void;
    using difference_type   = std::ptrdiff_t;
    using pointer           = void;
    using reference         = void;
};

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

// The dispatch has to *obtain* a context before it can parse into one: make_parse_context if
// there is one, default construction otherwise. A type with neither used to satisfy the concept
// and then hard-error inside the operator's body, which made this answer a lie.
static_assert(  extractable_lvalue<is_c, ctx_fixture::via_default> );
static_assert(  extractable_lvalue<is_c, ctx_fixture::via_maker>   );
static_assert( !extractable_lvalue<is_c, ctx_fixture::via_neither> );

// make_parse_context must return the context type exactly, however it builds it internally. A
// wrong return type (a missing return statement makes it void) and a non-const parameter are both
// static_asserts in the operator, so neither is reachable from here.
static_assert(  extractable_lvalue<is_c, ctx_fixture::via_convertible> );
static_assert(  std::is_same_v<
                  decltype(IOv2::parse_context_type<char, ctx_fixture::via_convertible>
                               ::make_parse_context(std::declval<const ctx_fixture::via_convertible&>())),
                  IOv2::parse_context_type<char, ctx_fixture::via_convertible>::type> );

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

// 10. The C++23 extended floating-point types are admitted only through a standard one.
//
// They satisfy is_arithmetic_v but do not promote, so reaching snprintf's %g unchanged was UB.
// The constraint now takes one only when a standard type represents it exactly. This matrix had
// no entry for any of them, which is how that went unnoticed.
#if defined(__STDCPP_FLOAT16_T__)
static_assert(  insertable<os_c, std::float16_t>          );
static_assert(  extractable_lvalue<is_c, std::float16_t>  );
#endif
#if defined(__STDCPP_BFLOAT16_T__)
static_assert(  insertable<os_c, std::bfloat16_t>         );
static_assert(  extractable_lvalue<is_c, std::bfloat16_t> );
#endif
#if defined(__STDCPP_FLOAT32_T__)
static_assert(  insertable<os_c, std::float32_t>          );
static_assert(  extractable_lvalue<is_c, std::float32_t>  );
#endif
#if defined(__STDCPP_FLOAT64_T__)
static_assert(  insertable<os_c, std::float64_t>          );
static_assert(  extractable_lvalue<is_c, std::float64_t>  );
#endif

// float128_t needs 113 mantissa bits, which x87 long double does not have. Spelled as the
// criterion, not the platform, so the row stays true where long double is IEEE binary128.
#if defined(__STDCPP_FLOAT128_T__)
inline constexpr bool f128_has_carrier =
    std::numeric_limits<long double>::digits >= std::numeric_limits<std::float128_t>::digits;
static_assert( insertable<os_c, std::float128_t>         == f128_has_carrier );
static_assert( extractable_lvalue<is_c, std::float128_t> == f128_has_carrier );
#endif

// The standard three carry themselves, so the relay is an identity cast.
static_assert(  insertable<os_c, float>               );
static_assert(  insertable<os_c, double>              );
static_assert(  insertable<os_c, long double>         );
static_assert(  extractable_lvalue<is_c, float>       );
static_assert(  extractable_lvalue<is_c, double>      );
static_assert(  extractable_lvalue<is_c, long double> );

// ---------------------------------------------------------------------------------------------
// 11. make_parse_context is detected by name, and its shape is checked separately.
//
// Three rounds of review kept finding holes here, each time because the check asked whether a
// *call* succeeds: the set of declarations whose call happens to succeed is open-ended, so some
// new shape always slipped through. Detection is now shape-agnostic (inheritance ambiguity) and
// the shape is one comparison against the required function-pointer type. These assertions pin
// both halves so the split cannot quietly collapse back into one.
// ---------------------------------------------------------------------------------------------
template <typename T>
using pct = IOv2::parse_context_type<char, T>;

// Detection must see the name whatever shape it has -- fold shape in here and a mis-written
// specialization reads as "no such member" and is silently default constructed, which is exactly
// the failure this split removes.
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::ok>,        maker_shape::ok>        );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::ok_nx>,     maker_shape::ok_nx>     );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::nonconst>,  maker_shape::nonconst>  );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::rvalue>,    maker_shape::rvalue>    );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::nonstatic>, maker_shape::nonstatic> );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::middleman>, maker_shape::middleman> );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::voidret>,   maker_shape::voidret>   );
static_assert( !IOv2::detail::declares_maker<pct<maker_shape::absent>,    maker_shape::absent>    );

// Detection is shape-agnostic, so every one of these is seen -- including the ones that are not
// functions at all. Access is checked after lookup, so `private` does not hide the name either.
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::templated>,  maker_shape::templated>  );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::overloaded>, maker_shape::overloaded> );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::priv>,       maker_shape::priv>       );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::deleted>,    maker_shape::deleted>    );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::byvalue>,    maker_shape::byvalue>    );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::crvalue>,    maker_shape::crvalue>    );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::datamember>, maker_shape::datamember> );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::nestedtype>, maker_shape::nestedtype> );
static_assert(  IOv2::detail::declares_maker<pct<maker_shape::inherited>,  maker_shape::inherited>  );

template <typename T>
concept maker_shape_ok =
    requires { &pct<T>::make_parse_context; }
    && IOv2::detail::maker_signature_v<typename pct<T>::type, T, decltype(&pct<T>::make_parse_context)>;

// Only the exact declaration passes. noexcept has to be its own case in maker_signature_v: since
// C++17 it is part of the function type, so a plain type comparison would reject a correct member.
static_assert(  maker_shape_ok<maker_shape::ok>        );
static_assert(  maker_shape_ok<maker_shape::ok_nx>     );
static_assert( !maker_shape_ok<maker_shape::nonconst>  );
static_assert( !maker_shape_ok<maker_shape::rvalue>    );
static_assert( !maker_shape_ok<maker_shape::nonstatic> );
static_assert( !maker_shape_ok<maker_shape::middleman> );
static_assert( !maker_shape_ok<maker_shape::voidret>   );

static_assert( !maker_shape_ok<maker_shape::templated>  );
static_assert( !maker_shape_ok<maker_shape::overloaded> );
static_assert( !maker_shape_ok<maker_shape::priv>       );
static_assert( !maker_shape_ok<maker_shape::deleted>    );
static_assert( !maker_shape_ok<maker_shape::byvalue>    );
static_assert( !maker_shape_ok<maker_shape::crvalue>    );
static_assert( !maker_shape_ok<maker_shape::datamember> );
static_assert( !maker_shape_ok<maker_shape::nestedtype> );

// Inheritance is the one indirection the contract allows, so this is the section's second
// positive case: detected through the base and accepted on shape.
static_assert(  maker_shape_ok<maker_shape::inherited>  );

// ---------------------------------------------------------------------------------------------
// 12. The std::tm parse context is admitted for this platform's time-zone tier only.
//
// tm_stream_format builds its format from the platform rather than from TzLevel, so an
// explicitly named off-platform tier would get a format whose %z / %Z are matched as literals and
// could only fail. The tier is spelled relative to tm_parse_tz_level rather than hard-coded, so
// these hold on a platform whose std::tm carries neither field.
// ---------------------------------------------------------------------------------------------
template <IOv2::tz_level L>
concept tm_ctx_admitted =
    requires { sizeof(IOv2::io_traits<char, IOv2::time_parse_context<char, true, true, L>>); };

inline constexpr IOv2::tz_level tm_tier = IOv2::parse_context_type<char, std::tm>::tm_parse_tz_level;

static_assert(  tm_ctx_admitted<tm_tier> );
static_assert(  tm_ctx_admitted<IOv2::tz_level::zone>   == (tm_tier == IOv2::tz_level::zone)   );
static_assert(  tm_ctx_admitted<IOv2::tz_level::offset> == (tm_tier == IOv2::tz_level::offset) );
static_assert(  tm_ctx_admitted<IOv2::tz_level::none>   == (tm_tier == IOv2::tz_level::none)   );

// ---------------------------------------------------------------------------------------------
// 13. Which disjunct of char_sink_for catches what.
//
// Every standard output adaptor declares a member value_type of void, yet iter_value_t on it is
// ill-formed rather than void -- so the leading !requires absorbs all of them and the is_void_v
// disjunct reaches none. That is the opposite of what the shape suggests, and the note in
// traits_base.h said it the wrong way round until this was measured.
// ---------------------------------------------------------------------------------------------
template <typename I>
concept ivt_ill_formed = !requires { typename std::iter_value_t<I>; };

static_assert( ivt_ill_formed<std::back_insert_iterator<std::string>>  );
static_assert( ivt_ill_formed<std::front_insert_iterator<std::string>> );
static_assert( ivt_ill_formed<std::insert_iterator<std::string>>       );
static_assert( ivt_ill_formed<std::ostream_iterator<char>>             );
static_assert( ivt_ill_formed<std::ostreambuf_iterator<char>>          );

// The is_void_v disjunct is reached only by an explicit std::iterator_traits specialization whose
// value_type is void. That shape is reachable, so the disjunct is not dead code.
static_assert( std::is_void_v<std::iter_value_t<sink_shape::traits_void_sink>>   );
static_assert( IOv2::char_sink_for<sink_shape::traits_void_sink, char>           );

// The library's own sink is accepted, and only for its own character type.
static_assert(  IOv2::char_sink_for<typename os_c::out_iter_type, char>    );
static_assert( !IOv2::char_sink_for<typename os_c::out_iter_type, wchar_t> );
}

// The static_asserts above are the test; compiling this file is what passes it. This case
// exists so the suite reports a result rather than an empty run.
TEST(IoTraits, EveryDetectionRuleHoldsAtCompileTime)
{
    SUCCEED() << "all io_traits detection checks are static_asserts in this file";
}

// The static_assert above answers for the concept. Only a real extraction instantiates the
// operator's body, which is the part that has to seed the context from what make_parse_context
// handed back.
// The section-10 rows ask the concept, which never instantiates the operator's
// body -- so they cannot reach the numeric facet's own static_assert that a
// carrier exists. Running a real insertion and extraction does, which is what
// makes this the check that the two criteria agree rather than merely look
// alike: if the io-side numeric_limits clause ever admitted a type the facet's
// float_carrier_t rejects, this case would stop compiling.
TEST(IoTraits, EveryAdmittedFloatingTypeReallyReachesTheFacet)
{
    auto round_trip = [](auto value)
    {
        using T = decltype(value);

        os_c os{IOv2::mem_device<char>{}, IOv2::locale<char>("C")};
        os << value;
        ASSERT_FALSE(os.str_fail());
        ASSERT_FALSE(os.device().str().empty());

        is_c is{IOv2::mem_device<char>{os.device().str()}, IOv2::locale<char>("C")};
        T    back{};
        is >> back;
        EXPECT_FALSE(is.str_fail());
        EXPECT_EQ(back, value);
    };

    round_trip(1.5f);
    round_trip(1.5);
    round_trip(1.5L);
#if defined(__STDCPP_FLOAT16_T__)
    round_trip(std::float16_t{1.5f16});
#endif
#if defined(__STDCPP_BFLOAT16_T__)
    round_trip(std::bfloat16_t{1.5bf16});
#endif
#if defined(__STDCPP_FLOAT32_T__)
    round_trip(std::float32_t{1.5f32});
#endif
#if defined(__STDCPP_FLOAT64_T__)
    round_trip(std::float64_t{1.5f64});
#endif
}

// Out-of-range input saturates at the extremes of the *target* type, not of the
// carrier, and the decision is made before the narrowing rather than after --
// [conv.double] makes narrowing an out-of-range value undefined, so the order is
// load-bearing. float16_t is the telling case: its carrier is float, which holds
// 70000 comfortably, so a check made on the carrier would let it through.
#if defined(__STDCPP_FLOAT16_T__)
TEST(IoTraits, AnOutOfRangeFloatSaturatesAtTheTargetTypesExtreme)
{
    is_c          is{IOv2::mem_device<char>{std::string("70000")}, IOv2::locale<char>("C")};
    std::float16_t value{};

    is >> value;

    EXPECT_TRUE(is.str_fail()) << "LWG 23 wants the failure bit as well as the saturated value";
    EXPECT_EQ(value, std::numeric_limits<std::float16_t>::max());
}
#endif

TEST(IoTraits, AMakerBuildingTheContextFromASeedStillSeedsIt)
{
    is_c                          is{IOv2::mem_device<char>{std::string("42")},
                                     IOv2::locale<char>("C")};
    ctx_fixture::via_convertible  value{};

    is >> value;
    EXPECT_FALSE(is.str_fail());
}
