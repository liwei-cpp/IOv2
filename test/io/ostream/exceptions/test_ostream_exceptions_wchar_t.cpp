// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * How an ostream<wchar_t> reports failures.
 *
 * The categories, the mask and the rethrow rule are the character type's
 * business only in that the failing inserter has to be found for wchar_t at
 * all; everything structural -- the copy guarantees, the unwinding rule -- is
 * checked once, in the char file. What is here is the same category-by-category
 * walk over a wide stream, so that a mask honoured only for char would show up.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/locale/locale.h>

#include <support/failing_device.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace
{
    class insertion_failure final : public std::runtime_error
    {
    public:
        insertion_failure() : std::runtime_error("injected wide inserter failure") { }
    };
    struct dummy_type {};
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

TEST(OstreamExceptionsWchar, AMaskedInserterFailureReachesTheCallerAndFailsTheStream)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(IOv2::mem_device{L""});
        os.exceptions(IOv2::ios_defs::otherfailbit);

        EXPECT_THROW(os << dummy_type{}, insertion_failure);
        EXPECT_FALSE(static_cast<bool>(os));
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
}

TEST(OstreamExceptionsWchar, AFailureOutsideTheMaskOnlySetsTheState)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(IOv2::mem_device{L""});
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
TEST(OstreamExceptionsWchar, AMaskedFlushFailureIsRethrownFromTheSentryDestructor)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(failing_device<wchar_t>{std::wstring(L""), true}, IOv2::locale<wchar_t>("C"));
        os.setf(IOv2::ios_defs::unitbuf);
        os.exceptions(IOv2::ios_defs::devfailbit);

        EXPECT_THROW(os.put(L'x'), IOv2::device_error);
        EXPECT_TRUE(os.rdstate() & IOv2::ios_defs::devfailbit);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
}

// no mask: the same flush failure only sets devfailbit and does not throw.
TEST(OstreamExceptionsWchar, AnUnmaskedFlushFailureOnlySetsDevfailbit)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(failing_device<wchar_t>{std::wstring(L""), true}, IOv2::locale<wchar_t>("C"));
        os.setf(IOv2::ios_defs::unitbuf);

        EXPECT_NO_THROW(os.put(L'x'));
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
TEST(OstreamExceptionsWchar, TheSentryDestructorSwallowsAFlushFailureWhileUnwinding)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(failing_device<wchar_t>{std::wstring(L""), true}, IOv2::locale<wchar_t>("C"));
        os.setf(IOv2::ios_defs::unitbuf);

        os << dummy_type{};
        EXPECT_FALSE(static_cast<bool>(os));
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
}
