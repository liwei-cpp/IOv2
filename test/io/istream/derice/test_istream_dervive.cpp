#include <limits>
#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/io_manip.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/file_guard.h>
#include <support/verify.h>

namespace
{
struct DevIstream : public IOv2::istream<IOv2::mem_device<char>, char>
{
    using IOv2::istream<IOv2::mem_device<char>, char>::istream;
    int x = 20;
};

struct DevIOstream : public IOv2::istream<IOv2::mem_device<char>, char>
{
    using IOv2::istream<IOv2::mem_device<char>, char>::istream;
    int x = 50;
};
}

void test_istream_derive_1()
{
    dump_info("Test ostream derive case 1...");

    DevIstream obj1(IOv2::mem_device("hello"));
    // make sure the >> operator should return DevIstream object
    std::string str;
    VERIFY((obj1 >> str).x == 20);

    DevIOstream obj2(IOv2::mem_device{"hello"});
    // make sure the >> operator should return DevIOstream object
    VERIFY((obj2 >> str).x == 50);

    dump_info("Done\n");
}

namespace
{
// istream_type requires ios_state for the same reason ostream_type does: code
// constrained by the concept calls handle_exception() and operator bool directly
// (ws_t::operator(), in_sentry's constructor). See the note on the concept.
struct StatelessIs : IOv2::ios_base<char>
                   , IOv2::stream_common_operators
                   , IOv2::istream_operators<char>
{
    using char_type = char;
    using in_sentry_type = IOv2::in_sentry<StatelessIs, false>;
    // istream_type also demands the iterator type: i_iter() is private, so the extraction
    // concepts have only this alias to probe with. Nothing here can actually do I/O, so any
    // well-formed istreambuf_iterator will do.
    using in_iter_type = IOv2::istreambuf_iterator<IOv2::istreambuf<IOv2::mem_device<char>, char>>;
    IOv2::locale<char> m_locale;
};

// ios_state<char> derives from ios_base<char>, so it replaces that base rather than
// joining it.
struct StatefulIs : IOv2::ios_state<char>
                  , IOv2::stream_common_operators
                  , IOv2::istream_operators<char>
{
    using char_type = char;
    using in_sentry_type = IOv2::in_sentry<StatefulIs, false>;
    using in_iter_type = IOv2::istreambuf_iterator<IOv2::istreambuf<IOv2::mem_device<char>, char>>;
    IOv2::locale<char> m_locale;
};

// Two ios_base subobjects: rejected by the concept's derived_from<T, ios_base<char_type>>
// clause through base ambiguity. See the matching case in test_ostream_derive.cpp.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winaccessible-base"
struct DoubleBaseIs : StatefulIs, IOv2::ios_base<char> {};
#pragma GCC diagnostic pop

static_assert( IOv2::istream_type<IOv2::istream<IOv2::mem_device<char>, char>> );
static_assert( IOv2::istream_type<IOv2::iostream<IOv2::mem_device<char>, char>> );
static_assert(!IOv2::istream_type<StatelessIs> );
static_assert( IOv2::istream_type<StatefulIs> );
static_assert(!IOv2::istream_type<DoubleBaseIs> );
}

void test_istream_derive()
{
    test_istream_derive_1();
}