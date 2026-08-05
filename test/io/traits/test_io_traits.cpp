// Compile-time regression coverage for the io_traits extension point.
//
// Everything here is a static_assert; the runtime function only reports that the translation
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

#include <ctime>
#include <string>

#include <device/mem_device.h>
#include <io/io_manip.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/traits/nullptr.h>
#include <io/traits/tm.h>
#include <support/dump_info.h>

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

// The same rule, asked of a type that is streamable in the other direction: io_traits<char, _Ws>
// is complete but has no swrite, so the failure is a missing member rather than an incomplete
// type. Both paths must yield `false` too.
static_assert( !insertable<os_c, IOv2::_Ws> );
static_assert( !extractable_rvalue<is_c, IOv2::_Endl> );

// ---------------------------------------------------------------------------------------------
// 2. Direction is decided by which member exists, not by the stream type.
//
// An iostream satisfies both istream_type and ostream_type, so it is the sharpest test that the
// direction really comes from io_traits and not from a constraint on the stream.
// ---------------------------------------------------------------------------------------------
static_assert(  insertable<ios_c, IOv2::_Endl>  && !extractable_rvalue<is_c, IOv2::_Endl>  );
static_assert(  insertable<ios_c, IOv2::_Ends>  && !extractable_rvalue<is_c, IOv2::_Ends>  );
static_assert(  insertable<ios_c, IOv2::_Flush> && !extractable_rvalue<is_c, IOv2::_Flush> );
static_assert(  extractable_rvalue<ios_c, IOv2::_Ws> && !insertable<os_c, IOv2::_Ws> );

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
// decays to one, both still reach it.
static_assert(  extractable_lvalue<is_c, void (*)(IOv2::ios_base<char>&)> );
static_assert(  extractable_lvalue<is_c, void (IOv2::ios_base<char>&)> );

// The two-way manipulators work in both directions.
static_assert(  insertable<os_c, IOv2::_Setw> && extractable_rvalue<is_c, IOv2::_Setw> );

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
static_assert( !insertable<os_c, IOv2::_Setfill<wchar_t>> );
static_assert(  insertable<os_c, IOv2::_Setfill<char>>    );
static_assert( !insertable<os_w, IOv2::_Setfill<char>>    );

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
static_assert(  extractable_rvalue<is_c, IOv2::_Setw> );

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
}

void test_io_traits()
{
    dump_info("Test io_traits detection (compile-time only)...");
    dump_info("Done\n");
}
