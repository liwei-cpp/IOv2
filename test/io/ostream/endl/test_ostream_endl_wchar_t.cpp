// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The endl manipulator on an ostream<wchar_t>, and the manipulator dispatch
 * around it.
 *
 * Same contract as the narrow case -- insert widen('\n'), then flush -- with
 * one wide-specific claim: what endl inserts is one wchar_t, so multi-byte text
 * on either side of it must not change how much endl contributes.
 *
 * The ctype-facet failure path is checked once, in the char file; what is
 * character-type-specific here is dispatch, because the function-pointer
 * overload has to name ios_base<wchar_t> to be found at all.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

TEST(OstreamEndlWchar, EndlWritesANewline)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{std::wstring(L"")});

        os << endl;
        EXPECT_EQ(os.device().str(), L"\n");
        EXPECT_TRUE(os.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// The newline is one character, not one byte, whatever sits next to it.
TEST(OstreamEndlWchar, EachEndlEndsTheLineItWasWrittenOn)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{std::wstring(L"")});

        os << L"中é" << endl << L"漢字ξ" << endl << L"z";
        EXPECT_EQ(os.device().str(), std::wstring(L"中é\n漢字ξ\nz"));
        EXPECT_EQ(os.device().str().size(), 8u);
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamEndlWchar, EndsAndFlushDispatchThroughIoTraitsToo)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{std::wstring(L"")});

        os << endl;
        EXPECT_EQ(os.device().str().size(), 1u);
        EXPECT_TRUE(os.good());

        os << ends;
        EXPECT_EQ(os.device().str().size(), 2u);
        EXPECT_TRUE(os.good());

        os << flush;
        EXPECT_TRUE(os.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// A function-pointer manipulator must dispatch to
// operator<<(T&, void(*)(ios_base<wchar_t>&)) rather than to the generic operator<<.
TEST(OstreamEndlWchar, AFunctionPointerManipulatorBeatsTheGenericInserter)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto       os = T(mem_device{std::wstring(L"")});
        static int calls;
        calls = 0;
        void (*manip)(ios_base<wchar_t>&) = [](ios_base<wchar_t>&) { ++calls; };

        os << manip;
        EXPECT_EQ(calls, 1);

        os << manip << manip;        // operator<< returns the stream, so manipulators chain
        EXPECT_EQ(calls, 3);

        // a capture-less lambda reaches the same overload once decayed with unary +
        os << +[](ios_base<wchar_t>&) { ++calls; };
        EXPECT_EQ(calls, 4);
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// A null manipulator must be rejected, leaving strfailbit set (no mask -> no throw).
TEST(OstreamEndlWchar, ANullFunctionPointerManipulatorIsRejected)
{
    auto helper = []<template <typename, typename> class T>()
    {
        auto os = T(mem_device{std::wstring(L"")});

        os << static_cast<void (*)(ios_base<wchar_t>&)>(nullptr);
        EXPECT_TRUE(os.rdstate() & ios_defs::strfailbit);
        os.clear();

        static int base_calls;
        base_calls = 0;
        os << +[](ios_base<wchar_t>&) { ++base_calls; };
        EXPECT_EQ(base_calls, 1);
        EXPECT_FALSE(os.rdstate() & ios_defs::strfailbit);

        os << L"ok";
        auto [dev, err] = os.detach();
        EXPECT_EQ(dev.str(), L"ok");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}
