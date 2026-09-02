// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * How an ostream<char> reports failures, and what it guarantees while doing so.
 *
 * Every failure in this library lands in one of the four categories, is
 * recorded in the state, and is rethrown only if that category is in the
 * exception mask. What the tests here pin down is that the rule holds no matter
 * where the failure arises -- an inserter, an explicit flush, a manipulator, or
 * the sentry's own destructor -- and that it keeps holding while the stack is
 * unwinding, where rethrowing would call std::terminate and lose the original
 * exception.
 *
 * The second half is about the guarantees around copying: a copy that throws
 * must leave both sides exactly as they were, including the source's lock.
 */
#include <device/file_device.h>
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/traits/char_and_str.h>

#include <support/failing_device.h>
#include <support/file_guard.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <thread>

namespace
{
    class insertion_failure final : public std::runtime_error
    {
    public:
        insertion_failure() : std::runtime_error("injected inserter failure") { }
    };
    struct dummy_type {};

    // Runs fn(os) from a destructor, so that the I/O happens while an exception is unwinding.
    // Deliberately at namespace scope rather than local to the lambda that uses it: some clang
    // versions give the destructor of a class defined inside a (generic) lambda internal linkage
    // and then never emit it, which fails the build under -Werror=undefined-internal.
    template <typename TStream, typename TFn>
    struct unwind_probe
    {
        TStream& os;
        TFn&     fn;
        ~unwind_probe() { fn(os); }
    };
}

namespace IOv2
{
    template <typename TChar>
    struct io_traits<TChar, dummy_type>
    {
        template <typename TIter>
            requires (std::is_same_v<TChar, typename TIter::value_type>)
        static TIter swrite(TIter, ios_base<TChar>&, const locale<TChar>&, dummy_type)
        {
            throw insertion_failure();
        }
    };
}

// An inserter that throws is categorized as otherfailbit; with that bit masked the
// exception reaches the caller unchanged, and the stream is failed by the time it does.
TEST(OstreamExceptionsChar, AMaskedInserterFailureReachesTheCallerAndFailsTheStream)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(IOv2::mem_device{""});
        os.exceptions(IOv2::ios_defs::otherfailbit);

        EXPECT_THROW(os << dummy_type{}, insertion_failure);
        EXPECT_FALSE(static_cast<bool>(os));
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
}

// The mask is per category: masking a different one does not make this failure throw,
// and the stream can be cleared and fail again the same way.
TEST(OstreamExceptionsChar, AFailureOutsideTheMaskOnlySetsTheState)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(IOv2::mem_device{""});
        os.setf(IOv2::ios_defs::unitbuf);
        os.exceptions(IOv2::ios_defs::cvtfailbit);

        EXPECT_NO_THROW(os << dummy_type{});
        EXPECT_FALSE(static_cast<bool>(os));

        os.clear();
        ASSERT_TRUE(static_cast<bool>(os));
        os.exceptions(IOv2::ios_defs::cvtfailbit);

        EXPECT_NO_THROW(os << dummy_type{});
        EXPECT_FALSE(static_cast<bool>(os));
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
}

// devfailbit masked: a unitbuf stream whose device fails its flush reports the
// failure through the out_sentry destructor. Because the destructor runs on a
// normal scope exit (no unwinding), it routes the device_error through
// handle_exception, which rethrows the original device_error to the caller and
// leaves devfailbit set.
TEST(OstreamExceptionsChar, AMaskedFlushFailureIsRethrownFromTheSentryDestructor)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(failing_device<char>{std::string(""), true});
        os.setf(IOv2::ios_defs::unitbuf);
        os.exceptions(IOv2::ios_defs::devfailbit);

        EXPECT_THROW(os.put('x'), IOv2::device_error);
        EXPECT_TRUE(os.rdstate() & IOv2::ios_defs::devfailbit);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
}

// no mask: the same flush failure only sets devfailbit and does not throw.
TEST(OstreamExceptionsChar, AnUnmaskedFlushFailureOnlySetsDevfailbit)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(failing_device<char>{std::string(""), true});
        os.setf(IOv2::ios_defs::unitbuf);

        EXPECT_NO_THROW(os.put('x'));
        EXPECT_TRUE(os.rdstate() & IOv2::ios_defs::devfailbit);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
}

// unwinding branch: the operation body itself throws while
// unitbuf is set, so the out_sentry destructor runs during stack unwinding and the
// device flush also fails. The destructor must swallow that failure and never throw
// during unwinding (no std::terminate). operator<< then catches that failure
// and, with otherfailbit unmasked, only sets state, so control returns normally;
// reaching the assertion proves there was no terminate and the stream failed.
TEST(OstreamExceptionsChar, TheSentryDestructorSwallowsAFlushFailureWhileUnwinding)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(failing_device<char>{std::string(""), true});
        os.setf(IOv2::ios_defs::unitbuf);

        os << dummy_type{};
        EXPECT_FALSE(static_cast<bool>(os));
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
}

// Copy assignment gives the strong exception guarantee. The copy is made into a temporary
// first, so a throw leaves the destination exactly as it was; only a move assignment, which
// is noexcept throughout, commits it. A file_device is move-only, so copying its converter
// kernel always throws -- which is what makes this observable at all.
TEST(OstreamExceptionsChar, AFailedCopyAssignmentLeavesTheDestinationUntouched)
{
    const std::string f1 = "test_ostream_exceptions_char_4_1.txt";
    const std::string f2 = "test_ostream_exceptions_char_4_2.txt";

    file_guard g1(f1);
    file_guard g2(f2);

    // Output direction: a failed assignment must not leak the source's format state or
    // status bits into the destination, nor swap out its device.
    auto helper = [&]<template <typename, typename> class T>()
    {
        T<IOv2::ofile_device<char>, char> src(IOv2::ofile_device<char>{f1});
        T<IOv2::ofile_device<char>, char> dst(IOv2::ofile_device<char>{f2});

        src.width(42);
        src.precision(9);
        src.fill('#');
        src.setf(IOv2::ios_defs::hex, IOv2::ios_defs::basefield);
        src.setstate(IOv2::ios_defs::strfailbit);

        const auto w  = dst.width();
        const auto p  = dst.precision();
        const auto fl = dst.fill();
        const auto fg = dst.flags();
        const auto st = dst.rdstate();

        EXPECT_THROW(dst = src, IOv2::cvt_error);

        EXPECT_EQ(dst.width(), w);
        EXPECT_EQ(dst.precision(), p);
        EXPECT_EQ(dst.fill(), fl);
        EXPECT_EQ(dst.flags(), fg);
        EXPECT_EQ(dst.rdstate(), st);
        EXPECT_TRUE(static_cast<bool>(dst));

        // Still bound to its own device, and unpadded: a leaked width would show up here.
        dst << "abc";
        dst.flush();
    };

    helper.operator()<IOv2::ostream>();
    EXPECT_EQ(g2.contents(), "abc");
}

// Input direction, and a self-assignment: without the self-check the temporary copy
// would throw on `s = s` even though nothing needs to happen.
TEST(OstreamExceptionsChar, SelfAssignmentNeverMakesTheTemporaryCopy)
{
    const std::string f1 = "test_ostream_exceptions_char_4_3.txt";
    file_guard        g1(f1, std::string("hello world"));

    IOv2::istream<IOv2::ifile_device<char>, char> s(IOv2::ifile_device<char>{f1});
    s.width(11);

    // Through a pointer, so that the self-assignment survives to run time instead of
    // being rejected by -Wself-assign-overloaded.
    auto* self = &s;

    EXPECT_NO_THROW(s = *self);
    EXPECT_EQ(s.width(), 11u);
    EXPECT_TRUE(static_cast<bool>(s));

    std::string got;
    s >> got;
    EXPECT_EQ(got, "hello");
}

// The happy path is unchanged: with a copyable device the assignment still copies.
TEST(OstreamExceptionsChar, AssignmentStillCopiesWhenTheDeviceIsCopyable)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T src(IOv2::mem_device{""});
        T dst(IOv2::mem_device{""});

        src.width(7);
        src.precision(3);
        src.fill('*');

        dst = src;

        EXPECT_EQ(dst.width(), 7u);
        EXPECT_EQ(dst.precision(), 3);
        EXPECT_EQ(dst.fill(), '*');

        auto* self = &dst;
        dst = *self;
        EXPECT_EQ(dst.width(), 7u);
        EXPECT_EQ(dst.precision(), 3);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
    helper.operator()<IOv2::istream>();
}

namespace
{
    // io_mutex() is recursive, so a try_lock() on the thread that leaked it would succeed and
    // prove nothing; the probe has to run from another thread.
    bool unlocked(auto& s)
    {
        bool        res = false;
        std::thread t([&s, &res] {
            if (s.io_mutex().try_lock())
            {
                res = true;
                s.io_mutex().unlock();
            }
        });
        t.join();
        return res;
    }
}

// Copy construction reads the source under the source's io_mutex(), taken in a mem-initializer
// that delegates to a private constructor -- one full-expression, so the lock spans every
// subobject's initialization instead of being released after the first one. The other half of
// that idiom is unwinding: when copying a move-only converter kernel throws, the lock temporary
// must be destroyed too, or the source stays locked for good and every later tie flush on it
// silently skips.
TEST(OstreamExceptionsChar, AFailedCopyConstructionDoesNotLeaveTheSourceLocked)
{
    const std::string f1 = "test_ostream_exceptions_char_5_1.txt";
    file_guard        g1(f1);

    // trunc is spelled out so that both devices create the file: ofile_device opens "w"
    // either way, but file_device without it opens "r+" and would fail on a missing file.
    auto helper = [&]<template <typename, typename> class T, typename TDevice>()
    {
        T<TDevice, char> src(TDevice{f1, IOv2::file_open_flag::trunc});
        ASSERT_TRUE(unlocked(src));

        EXPECT_THROW({ auto copy = src; (void)copy; }, IOv2::cvt_error);
        EXPECT_TRUE(unlocked(src));
    };

    helper.operator()<IOv2::ostream, IOv2::ofile_device<char>>();
    helper.operator()<IOv2::iostream, IOv2::file_device<char>>();
}

TEST(OstreamExceptionsChar, AFailedCopyLeavesAnInputStreamUsable)
{
    const std::string f2 = "test_ostream_exceptions_char_5_2.txt";
    file_guard        g2(f2, std::string("hello world"));

    IOv2::istream<IOv2::ifile_device<char>, char> src(IOv2::ifile_device<char>{f2});
    ASSERT_TRUE(unlocked(src));

    EXPECT_THROW({ auto copy = src; (void)copy; }, IOv2::cvt_error);
    EXPECT_TRUE(unlocked(src));

    // The source is still usable: a failed copy must not disturb it either.
    std::string got;
    src >> got;
    EXPECT_EQ(got, "hello");
}

// flush() must report a failed stream the same way endl/ends do. It used to return early,
// before its try block, so on a failed stream it flushed nothing, set no bit and threw
// nothing -- bypassing the exception mask entirely and turning `os << flush` into a silent
// no-op exactly when the caller had asked to be told about failures.
TEST(OstreamExceptionsChar, FlushOnAFailedStreamRecordsWithoutThrowingWhenUnmasked)
{
    IOv2::ostream<IOv2::mem_device<char>, char> os(IOv2::mem_device<char>{});
    os.put('a');
    os.setstate(IOv2::ios_defs::strfailbit);

    EXPECT_NO_THROW(os.flush());
    EXPECT_EQ(os.rdstate(), IOv2::ios_defs::strfailbit);
}

// With the bit masked in, all four flush spellings must throw.
TEST(OstreamExceptionsChar, AllFourFlushSpellingsThrowWhenTheBitIsMasked)
{
    IOv2::ostream<IOv2::mem_device<char>, char> os(IOv2::mem_device<char>{});
    os.put('a');
    os.exceptions(IOv2::ios_defs::strfailbit);
    try { os.setstate(IOv2::ios_defs::strfailbit); }
    catch (const IOv2::stream_error&) {}
    ASSERT_FALSE(static_cast<bool>(os));

    EXPECT_THROW(os.flush(), IOv2::stream_error);
    EXPECT_THROW(os << IOv2::flush, IOv2::stream_error);
    EXPECT_THROW(os << IOv2::endl, IOv2::stream_error);
    EXPECT_THROW(os << IOv2::ends, IOv2::stream_error);
}

// A good stream is untouched by all of this.
TEST(OstreamExceptionsChar, AGoodStreamIsUntouchedByTheMask)
{
    IOv2::ostream<IOv2::mem_device<char>, char> os(IOv2::mem_device<char>{});
    os.exceptions(IOv2::ios_defs::strfailbit);
    os.put('a');
    os.flush();
    os << IOv2::flush;

    EXPECT_TRUE(static_cast<bool>(os));
    EXPECT_EQ(os.rdstate(), IOv2::ios_defs::goodbit);
    EXPECT_EQ(os.detach().first.str(), "a");
}

// Whether a masked failure survives stack unwinding must not depend on *where* it lands.
// out_sentry's destructor was the only place that checked; every other handler --
// ostream::flush()'s own try, operator<<'s, io_traits<endl_t>::swrite's -- would rethrow per
// the mask, so a user destructor doing I/O while unwinding hit std::terminate and lost the
// original exception. The check now lives in handle_exception, which covers all of them.
TEST(OstreamExceptionsChar, MaskedFailuresNeverThrowWhileUnwinding)
{
    // Each op below fails on a stream whose failure bit IS masked, and is run from a
    // destructor during unwinding. Reaching the catch proves there was no terminate; the
    // state check proves the failure was still recorded.
    auto unwind_with = [](auto op, IOv2::ios_defs::iostate mask, IOv2::ios_defs::iostate expected)
    {
        auto helper = [&]<template <typename, typename> class T>()
        {
            T out(failing_device<char>{std::string(""), true});
            out.exceptions(mask);

            bool caught_original = false;
            try
            {
                unwind_probe<decltype(out), decltype(op)> p{out, op};
                throw insertion_failure();
            }
            catch (const insertion_failure&) { caught_original = true; }
            catch (...)           { ADD_FAILURE() << "the original exception was replaced"; }

            EXPECT_TRUE(caught_original);            // the original exception survives
            EXPECT_TRUE(out.rdstate() & expected);   // the I/O failure was still recorded
        };
        helper.template operator()<IOv2::ostream>();
        helper.template operator()<IOv2::iostream>();
    };

    // ostream::flush() -- handled by its own try, no sentry involved.
    unwind_with([](auto& s) { s.put('x'); s.flush(); },
                IOv2::ios_defs::devfailbit, IOv2::ios_defs::devfailbit);

    // the endl manipulator -- handled by io_traits<endl_t>::swrite's try.
    unwind_with([](auto& s) { s << IOv2::endl; },
                IOv2::ios_defs::devfailbit, IOv2::ios_defs::devfailbit);

    // generic operator<< -- handled by the inserter's own try.
    unwind_with([](auto& s) { s << dummy_type{}; },
                IOv2::ios_defs::otherfailbit, IOv2::ios_defs::otherfailbit);

    // out_sentry's destructor -- the path that always worked; it must keep working.
    unwind_with([](auto& s) { s.setf(IOv2::ios_defs::unitbuf); s.put('x'); },
                IOv2::ios_defs::devfailbit, IOv2::ios_defs::devfailbit);
}

// Control: outside any unwinding the mask is honored exactly as before.
TEST(OstreamExceptionsChar, OutsideUnwindingAMaskedFailureStillThrows)
{
    IOv2::ostream<failing_device<char>, char> os(failing_device<char>{std::string(""), true});
    os.exceptions(IOv2::ios_defs::devfailbit);
    os.put('x');

    EXPECT_THROW(os.flush(), IOv2::device_error);
    EXPECT_TRUE(os.rdstate() & IOv2::ios_defs::devfailbit);
}
