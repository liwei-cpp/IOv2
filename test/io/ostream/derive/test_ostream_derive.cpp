#include <stdexcept>
#include <string>

#include <cvt/code_cvt.h>
#include <cvt/comp/zlib_cvt.h>
#include <cvt/crypt/vigenere_cvt.h>
#include <cvt/crypt/hash_cvt.h>
#include <cvt/cvt_pipe_creator.h>
#include <device/mem_device.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/io_base.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <locale/locale.h>

#include <support/dump_info.h>
#include <support/verify.h>

namespace
{
struct DevOstream : public IOv2::ostream<IOv2::mem_device<char>, char>
{
    int x = 20;
};

struct DevIOstream : public IOv2::ostream<IOv2::mem_device<char>, char>
{
    int x = 50;
};
}

void test_ostream_derive_1()
{
    dump_info("Test ostream derive case 1...");

    DevOstream obj1;
    // make sure the << operator should return DevOstream object
    VERIFY((obj1 << "hello").x == 20);

    DevIOstream obj2;
    // make sure the << operator should return DevIOstream object
    VERIFY((obj2 << "hello").x == 50);

    dump_info("Done\n");
}

namespace
{
struct Level {
    std::string val;
};

class MyLogger : public IOv2::ostream<IOv2::mem_device<char>, char>
{
public:
    using IOv2::ostream<IOv2::mem_device<char>, char>::ostream;

    MyLogger& operator<<(const Level& l) {
        *this << "[" << l.val << "] ";
        return *this;
    }
};
}

void test_ostream_derive_2()
{
    MyLogger logger;
    logger << Level{"DEBUG"} << "User login\n"
           << Level{"WARN"} << "something happened";
    auto [dev, err] = logger.detach();
    auto str = dev.str();
    VERIFY(str == "[DEBUG] User login\n[WARN] something happened");
}

namespace
{
// A tie target that is a bare abs_flusher, not a stream_common_operators. Used to drive two
// otherwise-hard-to-reach branches:
//   * ThrowingFlusher::flush() throws, so the sentry's pre-lock "flush the tied stream" step
//     hits its catch(...) swallow (out_sentry / in_sentry).
//   * a bare abs_flusher makes tie()'s cycle-detection walk dynamic_cast to
//     stream_common_operators* -> null -> break (the non-stream node case).
struct ThrowingFlusher : public IOv2::abs_flusher
{
    int flushed = 0;
    void try_flush() override { ++flushed; throw IOv2::stream_error("tied flush boom"); }
};

struct QuietFlusher : public IOv2::abs_flusher
{
    int flushed = 0;
    void try_flush() override { ++flushed; }
};
}

// Tie an ostream to a bare abs_flusher and drive output. Two effects are checked:
//   * the sentry flushes the tied target before locking; when that flush throws, the
//     swallowing catch(...) keeps the insertion succeeding (ThrowingFlusher case).
//   * tie() accepts a non-stream flusher node: its cycle-detection walk dynamic_casts the
//     target to stream_common_operators*, gets null, and breaks (both cases reach it).
void test_ostream_derive_3()
{
    dump_info("Test ostream derive case 3 (tied bare flusher)...");

    auto helper = []<template<typename, typename> class T>()
    {
        {
            ThrowingFlusher tf;
            T oss{IOv2::mem_device{std::string("")}};
            oss.tie(&tf);                 // non-stream node -> cycle walk breaks
            oss << "x";                   // sentry flushes tf -> throws -> swallowed
            VERIFY( tf.flushed >= 1 );
            VERIFY( oss.good() );         // swallowed flush must not fail the stream
            oss.tie(nullptr);
            auto [dev, err] = oss.detach();
            VERIFY( dev.str() == "x" );
        }
        {
            QuietFlusher qf;
            T oss{IOv2::mem_device{std::string("")}};
            oss.tie(&qf);
            oss << "y";                   // tied flush succeeds
            VERIFY( qf.flushed >= 1 );
            VERIFY( oss.good() );
            oss.tie(nullptr);
        }
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}

namespace
{
// ostream_type requires io_state_and_exp, because code constrained by the concept calls
// handle_exception() and operator bool directly (out_sentry's destructor). Without the clause
// those calls only fail when the template body is instantiated, which bypasses the fallback
// overload's short diagnostic -- the operators that consume manipulators only check callability
// with std::invocable, a declaration-level check.
struct StatelessOs : IOv2::ios_base<char>
                   , IOv2::stream_common_operators
                   , IOv2::ostream_operators<char>
{
    using char_type = char;
    using out_sentry_type = IOv2::out_sentry<StatelessOs, false>;
    IOv2::locale<char> m_locale;
};

// The same thing with the state component: everything else about it is unchanged, so this
// pair isolates the new clause.
struct StatefulOs : StatelessOs, IOv2::io_state_and_exp {};

static_assert( IOv2::ostream_type<IOv2::ostream<IOv2::mem_device<char>, char>> );
static_assert( IOv2::ostream_type<IOv2::iostream<IOv2::mem_device<char>, char>> );
static_assert(!IOv2::ostream_type<StatelessOs> );
static_assert( IOv2::ostream_type<StatefulOs> );
}

void test_ostream_derive()
{
    test_ostream_derive_1();
    test_ostream_derive_2();
    test_ostream_derive_3();
}