// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * ios_state: the state bits, the exception mask, and the rule that ties them.
 *
 * Setting the mask is not a passive act. exceptions(mask) re-applies the state
 * through clear(rdstate()), so arming a category whose bit is already set
 * raises immediately -- and so does a later setstate(goodbit), which re-clears
 * the same state and raises again. That is the whole of the first two tests,
 * and it is the part callers are surprised by.
 *
 * The static_asserts at the top are a separate matter: fmtflags and iostate are
 * two bitmask types, not two spellings of one integer, and keeping them apart
 * is what stops a state bit being handed to a formatting interface.
 */
#include <IOv2/common/defs.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/io_manip.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <exception>
#include <string>
#include <type_traits>

namespace
{
// ---------------------------------------------------------------------------------------------
// fmtflags and iostate are two bitmask types, not two spellings of one integer.
//
// As typedefs for uint16_t/uint8_t they converted freely into each other and accepted any integer,
// so setiosflags(strfailbit) -- a state bit handed to a formatting interface -- compiled and set
// boolalpha, the two constants sharing bit 0. They are unscoped enums now, closed under the bitwise
// operators; being unscoped, the conversion out to bool that `if (state & eofbit)` relies on is
// unchanged.
// ---------------------------------------------------------------------------------------------
using fmt = IOv2::ios_defs::fmtflags;
using ist = IOv2::ios_defs::iostate;

template <typename T> concept setf_takes        = requires (IOv2::ios_state<char>& s, T v) { s.setf(v); };
template <typename T> concept clear_takes       = requires (IOv2::ios_state<char>& s, T v) { s.clear(v); };
template <typename T> concept setiosflags_takes = requires (T v) { IOv2::setiosflags(v); };

static_assert(std::is_enum_v<fmt> && std::is_same_v<std::underlying_type_t<fmt>, std::uint16_t>);
static_assert(std::is_enum_v<ist> && std::is_same_v<std::underlying_type_t<ist>, std::uint8_t>);

// Unscoped, so the outward conversions the old typedefs allowed still hold.
static_assert(std::is_convertible_v<fmt, bool>);
static_assert(std::is_convertible_v<ist, unsigned>);

// Nothing converts inward, and neither converts to the other.
static_assert(!std::is_convertible_v<int, fmt> && !std::is_convertible_v<int, ist>);
static_assert(!std::is_convertible_v<ist, fmt> && !std::is_convertible_v<fmt, ist>);

static_assert( setf_takes<fmt>         && !setf_takes<ist>         && !setf_takes<int>);
static_assert( clear_takes<ist>        && !clear_takes<fmt>        && !clear_takes<int>);
static_assert( setiosflags_takes<fmt>  && !setiosflags_takes<ist>  && !setiosflags_takes<int>);

// The operators keep a result in its own type, with the values the masks always had.
static_assert(std::is_same_v<decltype(IOv2::ios_defs::left | IOv2::ios_defs::right), fmt>);
static_assert(std::is_same_v<decltype(~IOv2::ios_defs::eofbit), ist>);
static_assert((IOv2::ios_defs::left | IOv2::ios_defs::right | IOv2::ios_defs::internal)
              == IOv2::ios_defs::adjustfield);
static_assert((IOv2::ios_defs::adjustfield & ~IOv2::ios_defs::left)
              == (IOv2::ios_defs::right | IOv2::ios_defs::internal));
static_assert((IOv2::ios_defs::skipws | IOv2::ios_defs::dec) == fmt{0x1002});
}

// Arming a category on a stream that has not failed changes nothing else.
TEST(IosState, ArmingACategoryOnACleanStreamIsQuiet)
{
    IOv2::ios_state<char> s;
    EXPECT_EQ(s.exceptions(), IOv2::ios_defs::goodbit);

    EXPECT_NO_THROW(s.exceptions(IOv2::ios_defs::cvtfailbit));
    EXPECT_EQ(s.exceptions(), IOv2::ios_defs::cvtfailbit);
}

// exceptions(mask) re-applies the current state through clear(rdstate()), so
// arming a category whose bit is already set raises there and then. The mask is
// stored first, so it is in force by the time the exception comes out.
TEST(IosState, ArmingACategoryThatHasAlreadyFailedRaisesImmediately)
{
    IOv2::ios_state<char> s;
    s.clear(IOv2::ios_defs::cvtfailbit);

    EXPECT_THROW(s.exceptions(IOv2::ios_defs::cvtfailbit), IOv2::cvt_error);
    EXPECT_EQ(s.exceptions(), IOv2::ios_defs::cvtfailbit);
}

// setstate(goodbit) adds no bit, but it still goes through clear(rdstate()), so
// on an armed and already-failed stream it raises just as the arming did. A
// caller reaching for it to "do nothing" gets an exception instead.
TEST(IosState, SetstateOfGoodbitStillReappliesTheStateAndSoStillRaises)
{
    IOv2::ios_state<char> s;
    s.setstate(IOv2::ios_defs::cvtfailbit);

    EXPECT_THROW(s.exceptions(IOv2::ios_defs::cvtfailbit), IOv2::cvt_error);
    EXPECT_THROW(s.setstate(IOv2::ios_defs::goodbit), IOv2::cvt_error);
}

TEST(IosState, AnEofErrorRaisesLikeEveryOtherCategory)
{
    // Regression test: handle_exception() routes a caught eof_error through
    // setstate(eofbit) -> clear(). When exceptions(eofbit) is enabled, this
    // must actually raise a notification exception, exactly like the
    // devfailbit/cvtfailbit/strfailbit/otherfailbit categories already do.
    // Previously, clear()'s eofbit branch was guarded by
    // `!std::current_exception()`, which is always false while inside
    // handle_exception's own catch block, so the exception was silently
    // swallowed.
    {
        IOv2::ios_state<char> stream;
        stream.exceptions(IOv2::ios_defs::eofbit);

        bool threw = false;
        try
        {
            stream.handle_exception(std::make_exception_ptr(IOv2::eof_error{}));
        }
        catch (IOv2::eof_error&)
        {
            threw = true;
        }
        catch (...)
        {
            ADD_FAILURE() << "the eof_error was replaced by something else";
        }

        EXPECT_TRUE(threw);
        EXPECT_TRUE(stream.eof());
    }

    // Without exceptions(eofbit) enabled, the state bit is still set but no
    // exception should be raised.
    {
        IOv2::ios_state<char> stream;

        EXPECT_NO_THROW(stream.handle_exception(std::make_exception_ptr(IOv2::eof_error{})));
        EXPECT_TRUE(stream.eof());
    }
}

TEST(IosState, ClearRethrowsTheStashedExceptionOrThrowsAFreshOne)
{
    using namespace IOv2;

    // For each failure category, clear() must, when the corresponding exception
    // mask bit is enabled, either re-throw the exception previously stashed by
    // handle_exception(), or, when none was stashed, throw a fresh category
    // exception.

    // --- devfailbit: stashed exception is re-thrown ---
    {
        ios_state<char> s;
        s.exceptions(ios_defs::devfailbit);
        bool threw = false;
        try
        {
            s.handle_exception(std::make_exception_ptr(device_error("stashed dev")));
        }
        catch (const device_error& e)
        {
            threw = (std::string(e.what()) == "stashed dev");
        }
        EXPECT_TRUE(threw);
        EXPECT_TRUE(s.dev_fail());
    }

    // --- devfailbit: no stashed exception -> fresh device_error ---
    {
        ios_state<char> s;
        s.exceptions(ios_defs::devfailbit);
        bool threw = false;
        try
        {
            s.setstate(ios_defs::devfailbit);
        }
        catch (const device_error&)
        {
            threw = true;
        }
        EXPECT_TRUE(threw);
    }

    // --- cvtfailbit: stashed exception is re-thrown ---
    {
        ios_state<char> s;
        s.exceptions(ios_defs::cvtfailbit);
        bool threw = false;
        try
        {
            s.handle_exception(std::make_exception_ptr(cvt_error("stashed cvt")));
        }
        catch (const cvt_error& e)
        {
            threw = (std::string(e.what()) == "stashed cvt");
        }
        EXPECT_TRUE(threw);
        EXPECT_TRUE(s.cvt_fail());
    }

    // --- strfailbit: stashed exception is re-thrown ---
    {
        ios_state<char> s;
        s.exceptions(ios_defs::strfailbit);
        bool threw = false;
        try
        {
            s.handle_exception(std::make_exception_ptr(stream_error("stashed str")));
        }
        catch (const stream_error& e)
        {
            threw = (std::string(e.what()) == "stashed str");
        }
        EXPECT_TRUE(threw);
        EXPECT_TRUE(s.str_fail());
    }

    // --- strfailbit: no stashed exception -> fresh stream_error ---
    {
        ios_state<char> s;
        s.exceptions(ios_defs::strfailbit);
        bool threw = false;
        try
        {
            s.setstate(ios_defs::strfailbit);
        }
        catch (const stream_error&)
        {
            threw = true;
        }
        EXPECT_TRUE(threw);
    }

    // --- otherfailbit: no stashed exception -> fresh stream_error ---
    {
        ios_state<char> s;
        s.exceptions(ios_defs::otherfailbit);
        bool threw = false;
        try
        {
            s.setstate(ios_defs::otherfailbit);
        }
        catch (const stream_error&)
        {
            threw = true;
        }
        EXPECT_TRUE(threw);
        EXPECT_TRUE(s.other_fail());
    }
}

// handle_exception must only set bits -- never throw -- while an exception is unwinding, even
// when the mask says the bit should throw. Otherwise a destructor that reports a failure through
// it would call std::terminate and lose the original exception.
TEST(IosState, AFailureReportedWhileUnwindingSetsTheBitWithoutThrowing)
{
    // A destructor reporting a failure while another exception is in flight: bit set, no throw.
    {
        IOv2::ios_state<char> s;
        s.exceptions(IOv2::ios_defs::strfailbit);

        struct probe
        {
            IOv2::ios_state<char>& st;
            ~probe()
            {
                st.handle_exception(std::make_exception_ptr(IOv2::stream_error("unwind")));
            }
        };

        bool caught_original = false;
        try
        {
            probe p{s};
            throw 42;
        }
        catch (int)
        {
            caught_original = true;   // the original exception survives
        }
        EXPECT_TRUE(caught_original);
        EXPECT_TRUE(s.rdstate() & IOv2::ios_defs::strfailbit);
    }

    // The check is exact inside a catch handler: the exception being handled no longer counts,
    // so a mask-driven throw still happens there.
    {
        IOv2::ios_state<char> s;
        s.exceptions(IOv2::ios_defs::strfailbit);

        bool threw = false;
        try
        {
            try { throw 42; }
            catch (int)
            {
                s.handle_exception(std::make_exception_ptr(IOv2::stream_error("in handler")));
            }
        }
        catch (const IOv2::stream_error&)
        {
            threw = true;
        }
        EXPECT_TRUE(threw);
        EXPECT_TRUE(s.rdstate() & IOv2::ios_defs::strfailbit);
    }

    // Control: outside any unwinding the mask is honored as before.
    {
        IOv2::ios_state<char> s;
        s.exceptions(IOv2::ios_defs::strfailbit);

        bool threw = false;
        try { s.handle_exception(std::make_exception_ptr(IOv2::stream_error("plain"))); }
        catch (const IOv2::stream_error&) { threw = true; }
        EXPECT_TRUE(threw);
        EXPECT_TRUE(s.rdstate() & IOv2::ios_defs::strfailbit);
    }
}

TEST(IosState, TheFlagAccessorsKeepTheirOwnBitmaskType)
{
    IOv2::ios_state<char> s;
    EXPECT_EQ(s.flags(), (IOv2::ios_defs::skipws | IOv2::ios_defs::dec));
    EXPECT_EQ(s.setf(IOv2::ios_defs::boolalpha), (IOv2::ios_defs::skipws | IOv2::ios_defs::dec));
    EXPECT_EQ(s.flags(), (IOv2::ios_defs::skipws | IOv2::ios_defs::dec | IOv2::ios_defs::boolalpha));

    s.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
    EXPECT_EQ((s.flags() & IOv2::ios_defs::basefield), IOv2::ios_defs::hex);
    s.unsetf(IOv2::ios_defs::boolalpha);
    EXPECT_FALSE((s.flags() & IOv2::ios_defs::boolalpha));
    EXPECT_EQ(s.flags(IOv2::ios_defs::skipws), (IOv2::ios_defs::skipws | IOv2::ios_defs::hex));
    EXPECT_EQ(s.flags(), IOv2::ios_defs::skipws);

    EXPECT_TRUE(s.good());
    s.setstate(IOv2::ios_defs::strfailbit | IOv2::ios_defs::eofbit);
    EXPECT_EQ(s.rdstate(), (IOv2::ios_defs::strfailbit | IOv2::ios_defs::eofbit));
    EXPECT_TRUE(s.str_fail() && s.eof() && !s.good() && !static_cast<bool>(s));
    s.unset_state(IOv2::ios_defs::strfailbit);
    EXPECT_EQ(s.rdstate(), IOv2::ios_defs::eofbit);
    EXPECT_TRUE(static_cast<bool>(s));
    s.clear();
    EXPECT_TRUE(s.good());
    EXPECT_EQ(s.rdstate(), IOv2::ios_defs::goodbit);
}
