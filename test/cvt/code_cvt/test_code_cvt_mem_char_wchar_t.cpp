// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The one case of test_code_cvt_mem_char_char32_t.cpp that puts wchar_t, not
 * char32_t, inside: char <-> wchar_t is available wherever wchar_t is, which
 * char <-> char32_t is not (it needs wchar_t_is_utf32).
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <type_traits>

using namespace IOv2;

TEST(CodeCvtMemChar32, TraitsWithWcharAsTheInternalType)
{
    using CheckType = code_cvt<rb_root_cvt<mem_device<char>>, wchar_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
    static_assert(std::is_same_v<CheckType::external_type, char>);
}
