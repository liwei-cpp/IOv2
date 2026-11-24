// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * What it takes to be an input stream, from the outside.
 *
 * Two separate claims. The extractors return the stream by its own type rather
 * than by the base they were inherited from, so `derived >> x` chains into the
 * derived interface; a class deriving from istream to add members would lose
 * them at the first extraction otherwise.
 *
 * And istream_type, the concept the extractors are constrained on, is what says
 * a type can be one. It demands ios_state rather than ios_base because code
 * behind the concept calls handle_exception() and operator bool directly
 * (ws_t::operator(), in_sentry's constructor), and it demands the iterator
 * alias because i_iter() is private and the extraction concepts have nothing
 * else to probe with. The cases below pin both requirements by exhibiting types
 * that satisfy everything except one of them.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/iochannel.h>
#include <IOv2/io/iochannel_iterator.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/io/utilities/istream_operators.h>
#include <IOv2/io/utilities/stream_common_operators.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>

using namespace IOv2;

namespace
{
    // Two derived streams with a member of their own, so the type an extraction
    // returns is visible in the value it yields.
    struct marked_istream : istream<mem_device<char>, char>
    {
        using istream<mem_device<char>, char>::istream;
        int mark = 20;
    };

    struct marked_iostream : iostream<mem_device<char>, char>
    {
        using iostream<mem_device<char>, char>::iostream;
        int mark = 50;
    };

    using probe_iter = ichannel_iterator<ichannel<mem_device<char>, char>>;

    // Everything istream_type asks for except the state: ios_base alone.
    struct stateless_is : ios_base<char>
                        , stream_common_operators
                        , istream_operators<char>
    {
        using char_type      = char;
        using in_sentry_type = in_sentry<stateless_is, false>;
        using in_iter_type   = probe_iter;
        // Qualified: inside the struct, `locale` names ios_base's member function.
        IOv2::locale<char> m_locale;
    };

    // ios_state<char> derives from ios_base<char>, so it replaces that base
    // rather than joining it.
    struct stateful_is : ios_state<char>
                       , stream_common_operators
                       , istream_operators<char>
    {
        using char_type      = char;
        using in_sentry_type = in_sentry<stateful_is, false>;
        using in_iter_type   = probe_iter;
        // Qualified: inside the struct, `locale` names ios_base's member function.
        IOv2::locale<char> m_locale;
    };

    // Two ios_base subobjects: rejected through base ambiguity by the concept's
    // derived_from<T, ios_base<char_type>> clause -- where the compiler refuses a
    // conversion to an ambiguous base. clang in MSVC-compatible mode (clang-cl)
    // accepts it, so the clause cannot see the second base there; the probe says
    // which kind of compiler this is.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winaccessible-base"
#endif
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4584)   // a base class that is already a base of another
#endif
    struct double_base_is : stateful_is, ios_base<char> {};

    struct probe_base {};
    struct probe_left : probe_base {};
    struct probe_both : probe_left, probe_base {};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

    constexpr bool ambiguous_base_refused = !std::is_convertible_v<probe_both*, probe_base*>;

    static_assert(istream_type<istream<mem_device<char>, char>>);
    static_assert(istream_type<iostream<mem_device<char>, char>>);
    static_assert(istream_type<stateful_is>);
    static_assert(!istream_type<stateless_is>);
    static_assert(!ambiguous_base_refused || !istream_type<double_base_is>);
}

TEST(IstreamDerive, TheConceptDemandsStateAndOneUnambiguousBase)
{
    // Every claim above is a static_assert; compiling is the check.
    SUCCEED();
}

// The extractor has to hand back the stream's own type. If it returned the base,
// the member added by the derived class would not be reachable from the result.
TEST(IstreamDerive, AnExtractionReturnsTheDerivedStream)
{
    std::string str;

    marked_istream is(mem_device("hello"));
    EXPECT_EQ((is >> str).mark, 20);

    marked_iostream ios(mem_device{"hello"});
    EXPECT_EQ((ios >> str).mark, 50);
}
