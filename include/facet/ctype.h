#pragma once

#include <common/metafunctions.h>
#include <facet/ctype_details.h>
#include <facet/facet_common.h>

#include <array>
#include <concepts>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>

namespace IOv2
{
template <typename CharT>
class ctype;

namespace detail
{
// CRTP mixin supplying sequence operations and the narrow(c, default) overload
// in terms of the Derived class's five single-character primitives: is(),
// toupper(), tolower(), widen(), and narrow(). Both ctype<CharT> specialisations
// inherit from this to avoid duplicating these definitions.
//
// Methods whose parameter types depend on Derived::char_type or Derived::mask
// are declared as constrained function templates: the requires clause defers
// evaluation of those dependent names to call time (when Derived is complete),
// avoiding the incomplete-type error that class-scope type aliases would cause.
//
// Range precondition (applies to every method taking iterator pairs --
// is_seq, scan_is_any, scan_not_any, toupper_seq, tolower_seq, widen_seq,
// narrow_seq): [beg, end) must be a valid range, i.e. `end` is reachable
// from `beg` via repeated `++`. The loops increment `beg` until it compares
// equal to `end`; passing iterators that do not satisfy this (swapped
// arguments on a random-access range, mismatched iterator pairs, an `end`
// past a valid range, etc.) is undefined behaviour. Matches the equivalent
// std::ctype contract; not asserted on the hot path.
template <typename Derived>
class ctype_ops
{
    const Derived& self() const { return static_cast<const Derived&>(*this); }

public:
    template <typename TM, typename TC>
        requires std::convertible_to<TM, typename Derived::mask> &&
                 std::convertible_to<TC, typename Derived::char_type>
    bool is_any(TM m, TC c) const
    {
        return self().is(c) & m;
    }

    template <typename InIt, typename OutIt>
    OutIt is_seq(InIt low, InIt high, OutIt vec) const
    {
        while (low != high)
            *vec++ = self().is(*low++);
        return vec;
    }

    template <typename TM, typename InIt>
        requires std::convertible_to<TM, typename Derived::mask>
    InIt scan_is_any(TM m, InIt beg, InIt end) const
    {
        while ((beg != end) && (!(self().is(*beg) & m)))
            ++beg;
        return beg;
    }

    template <typename TM, typename InIt>
        requires std::convertible_to<TM, typename Derived::mask>
    InIt scan_not_any(TM m, InIt beg, InIt end) const
    {
        while ((beg != end) && (self().is(*beg) & m))
            ++beg;
        return beg;
    }

    template <typename InIt, typename OutIt>
    OutIt toupper_seq(InIt beg, InIt end, OutIt dst) const
    {
        while (beg != end)
            *dst++ = self().toupper(*beg++);
        return dst;
    }

    template <typename InIt, typename OutIt>
    OutIt tolower_seq(InIt beg, InIt end, OutIt dst) const
    {
        while (beg != end)
            *dst++ = self().tolower(*beg++);
        return dst;
    }

    template <typename InIt, typename OutIt>
    OutIt widen_seq(InIt beg, InIt end, OutIt dst) const
    {
        while (beg != end)
            *dst++ = self().widen(*beg++);
        return dst;
    }

    // narrow(c) with a caller-supplied fallback for characters that have no
    // single-byte representation in the target locale.
    //
    // @pre `def` is returned verbatim when `self().narrow(c)` yields no value;
    //      this function does NOT validate that `def` is itself representable
    //      in the target locale. By convention (matching std::ctype::narrow)
    //      callers should pass a member of the basic source character set --
    //      typically a printable ASCII byte such as '?' -- so the fallback is
    //      meaningful in any encoding. Passing an arbitrary byte (e.g. '\xFF')
    //      is accepted silently and may yield output that is not a valid
    //      standalone character in the locale's encoding.
    template <typename TC>
        requires std::convertible_to<TC, typename Derived::char_type>
    char narrow(TC c, char def) const
    {
        auto res = self().narrow(c);
        return res ? *res : def;
    }

    template <typename InIt, typename OutIt>
    OutIt narrow_seq(InIt beg, InIt end, char dflt, OutIt dst) const
    {
        while (beg != end)
            *dst++ = self().narrow(*beg++, dflt);
        return dst;
    }
};
} // namespace detail

// Specialisation for single-byte character types (char, char8_t). The full
// [0, s_len) input space is enumerable at construction, so this specialisation
// materialises every primitive (is/toupper/tolower/widen/narrow) into a
// snapshot table and discards the source ctype_conf<CharT> pointer afterwards.
// All later lookups are pure array reads with no virtual dispatch and no
// shared_ptr indirection.
//
// Asymmetry with the sizeof(CharT) > 1 specialisation: that one retains m_obj
// because its input space is too large to enumerate, and forwards out-of-table
// values to the conf at call time. As a consequence, runtime mutations to a
// user-derived ctype_conf<CharT>'s is/toupper/tolower/widen/narrow take effect
// for the size > 1 path (on the out-of-table branch) but NOT for the size == 1
// path: this specialisation captures a one-shot snapshot at construction and
// never re-queries the conf. Callers who need dynamically-changing single-byte
// behaviour should either rebuild a new ctype<CharT> after each change, or
// supply a custom ctype<CharT> that does not snapshot.
template <typename CharT>
    requires (sizeof(CharT) == 1)
class ctype<CharT> : public detail::ctype_ops<ctype<CharT>>
{
    // Note: we have to use unsigned char here to avoid negative value.
    // Sizing is in units of "values representable by unsigned char", not a
    // hard-coded 256. The CHAR_BIT == 8 assumption is enforced by a
    // static_assert in ctype_details.h, so on every supported platform
    // s_len == 256.
    constexpr static unsigned s_len = std::numeric_limits<unsigned char>::max() + 1;

public:
    using create_rules = facet_create_rule<ctype_conf<CharT>>;

    using char_type = CharT;
    using mask = typename ctype_conf<CharT>::mask;

    template <shared_ptr_to<ctype_conf<CharT>> TConfPtr>
    ctype(TConfPtr p_obj)
    {
        if (!p_obj) throw std::runtime_error("shared_ptr is empty");
        for (unsigned i = 0; i < s_len; ++i)
        {
            m_table[i] = p_obj->is(static_cast<CharT>(i));
            m_toupper[i] = p_obj->toupper(static_cast<CharT>(i));
            m_tolower[i] = p_obj->tolower(static_cast<CharT>(i));
            m_widen[i] = p_obj->widen(static_cast<CharT>(i));
            m_narrow[i] = p_obj->narrow(static_cast<CharT>(i));
        }
    }

    ~ctype() = default;
    ctype(const ctype&) = delete;
    ctype& operator=(const ctype&) = delete;
    ctype(ctype&&) = delete;
    ctype& operator=(ctype&&) = delete;

public:
    mask is(CharT c) const
    {
        return m_table[static_cast<unsigned char>(c)];
    }

    CharT toupper(CharT c) const
    {
        return m_toupper[static_cast<unsigned char>(c)];
    }

    CharT tolower(CharT c) const
    {
        return m_tolower[static_cast<unsigned char>(c)];
    }

    CharT widen(char c) const
    {
        return m_widen[static_cast<unsigned char>(c)];
    }

    std::optional<char> narrow(CharT c) const
    {
        return m_narrow[static_cast<unsigned char>(c)];
    }

    using detail::ctype_ops<ctype<CharT>>::narrow;

private:
    std::array<mask, s_len>                m_table;
    std::array<CharT, s_len>               m_toupper;
    std::array<CharT, s_len>               m_tolower;
    std::array<CharT, s_len>               m_widen;
    std::array<std::optional<char>, s_len> m_narrow;
};

// Specialisation for multi-byte character types (wchar_t, char32_t). Values in
// [0, s_len) are served from precomputed lock-free tables; values beyond that
// are forwarded directly to the underlying ctype_conf<CharT> on each call, with
// no cache and no lock (see do_is() for the rationale).
//
// Thread-safety contract: because the out-of-table path calls the conf
// concurrently, ctype_conf<CharT>'s const methods must be safe for concurrent
// invocation. The default implementation is: its members are immutable after
// construction, is/toupper/tolower use the *_l locale functions (which take an
// explicit, immutable locale_t), and narrow() switches only the calling
// thread's locale via a per-thread uselocale guard. A user-supplied override of
// ctype_conf<CharT> must preserve this property.
//
// Self-consistency contract for user overrides: values inside [0, s_len) are
// served from the snapshot tables built at construction, while values outside
// that range go through virtual dispatch to the conf on every call. A
// user-derived ctype_conf<CharT> whose is(), toupper(), and tolower() are not
// internally consistent -- for example, `is(c) & upper` returning true while
// `toupper(c) == c`, or returning a mask that disagrees with what the
// case-conversion methods imply -- will exhibit "torn" behaviour: the
// inconsistency is frozen into the in-range tables at construction but is
// re-observed live for every out-of-range character. Overrides must be
// self-consistent across is/toupper/tolower (and across is/widen/narrow where
// the semantics demand it) so that the same character produces the same
// answer regardless of which branch served it.
template <typename CharT>
    requires (sizeof(CharT) > 1)
class ctype<CharT> : public detail::ctype_ops<ctype<CharT>>
{
    // See the matching s_len note in the sizeof(CharT)==1 specialization
    // above. Tables span [0, unsigned_char::max()+1) per-byte; CHAR_BIT == 8
    // is enforced by static_assert in ctype_details.h.
    constexpr static unsigned s_len = std::numeric_limits<unsigned char>::max() + 1;

    // Unsigned counterpart of CharT, used to fold a (possibly signed) CharT
    // into [0, 2^N) for the table-range check and indexing without sign
    // extension. make_unsigned_t guarantees the same width as CharT, so no
    // value is truncated regardless of how wide plain `unsigned` happens to be.
    using uchar_type = std::make_unsigned_t<CharT>;

public:
    using create_rules = facet_create_rule<ctype_conf<CharT>>;

    using char_type = CharT;
    using mask = typename ctype_conf<CharT>::mask;

    template <shared_ptr_to<ctype_conf<CharT>> TConfPtr>
    ctype(TConfPtr p_obj)
        : m_obj(p_obj)
    {
        if (!m_obj) throw std::runtime_error("shared_ptr is empty");
        for (unsigned i = 0; i < s_len; ++i)
        {
            m_toupper[i] = m_obj->toupper(static_cast<CharT>(i));
            m_tolower[i] = m_obj->tolower(static_cast<CharT>(i));
            m_widen[i] = m_obj->widen(static_cast<CharT>(i));
            m_narrow[i] = m_obj->narrow(static_cast<CharT>(i));
            m_table[i] = m_obj->is(static_cast<CharT>(i));
        }
    }

    ~ctype() = default;
    ctype(const ctype&) = delete;
    ctype& operator=(const ctype&) = delete;
    ctype(ctype&&) = delete;
    ctype& operator=(ctype&&) = delete;

public:
    mask is(CharT c) const
    {
        return do_is(c);
    }

    CharT toupper(CharT c) const
    {
        return do_toupper(c);
    }

    CharT tolower(CharT c) const
    {
        return do_tolower(c);
    }

    // Default behaviour (inherited from ctype_conf<CharT>::widen at
    // construction time): matches std::ctype<wchar_t>::widen() semantics
    // -- only guaranteed to be correct for the basic source character
    // set. For multi-byte locales (e.g. UTF-8), high bytes are not
    // standalone characters and btowc() returns WEOF for them at
    // table-build time; the corresponding entries of m_widen hold WEOF
    // cast to CharT (a sentinel-looking but unspecified value), and
    // callers must not widen() such bytes under this default. This caveat
    // also applies to widen_seq() (inherited from detail::ctype_ops).
    //
    // This is only the default. A user may derive from ctype_conf<CharT>
    // and override widen() to supply a different mapping; the lookup
    // table held here is then rebuilt from that override when the
    // owning ctype<CharT> is constructed, and the caveat above no
    // longer applies under that derived behaviour.
    CharT widen(char c) const
    {
        return m_widen[static_cast<unsigned char>(c)];
    }

    std::optional<char> narrow(CharT c) const
    {
        return do_narrow(c);
    }

    using detail::ctype_ops<ctype<CharT>>::narrow;

private:
    // For values inside the precomputed [0, s_len) range, answer from the
    // lock-free table; otherwise defer straight to the (immutable) conf.
    //
    // There used to be a per-character locked LRU cache on this slow path; it
    // was removed. Benchmarks showed it to be a net loss for this facet's
    // real workload: large alphabets (e.g. CJK) overflow any bounded cache and
    // thrash, so the lookup recomputes anyway while also paying the cache and
    // mutex overhead; and a single mutex serialises the threads that may share
    // one facet. Calling the conf directly is faster there and lock-free, so
    // it scales with thread count. (It loses to a cache only for a tiny,
    // highly repetitive working set -- not the case here.)
    mask do_is(CharT c) const
    {
        if (static_cast<uchar_type>(c) < s_len)
            return m_table[static_cast<uchar_type>(c)];
        return m_obj->is(c);
    }

    CharT do_toupper(CharT c) const
    {
        if (static_cast<uchar_type>(c) < s_len)
            return m_toupper[static_cast<uchar_type>(c)];
        return m_obj->toupper(c);
    }

    CharT do_tolower(CharT c) const
    {
        if (static_cast<uchar_type>(c) < s_len)
            return m_tolower[static_cast<uchar_type>(c)];
        return m_obj->tolower(c);
    }

    std::optional<char> do_narrow(CharT c) const
    {
        if (static_cast<uchar_type>(c) < s_len)
            return m_narrow[static_cast<uchar_type>(c)];
        return m_obj->narrow(c);
    }

private:
    std::shared_ptr<const ctype_conf<CharT>> m_obj;

    std::array<CharT, s_len>               m_toupper;
    std::array<CharT, s_len>               m_tolower;
    std::array<CharT, s_len>               m_widen;
    std::array<std::optional<char>, s_len> m_narrow;
    std::array<mask, s_len>                m_table;
};

template<typename TConfPtr>
ctype(TConfPtr) -> ctype<typename TConfPtr::element_type::char_type>;
}
