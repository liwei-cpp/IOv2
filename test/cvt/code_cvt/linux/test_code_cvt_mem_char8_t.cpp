// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The cvt_facilities case of ../test_code_cvt_mem_char8_t.cpp that converts through a
 * named locale into char32_t, which needs wchar_t_is_utf32 (glibc only).
 */
#include <IOv2/cvt/cvt_facilities.h>

#include <gtest/gtest.h>

using namespace IOv2;

TEST(CvtFacilities, NarrowToUtf32ThroughANamedLocale)
{
    EXPECT_EQ(detail::to_u32string("hello", "C"), U"hello");
    EXPECT_TRUE(detail::to_u32string("", "C").empty());
}
