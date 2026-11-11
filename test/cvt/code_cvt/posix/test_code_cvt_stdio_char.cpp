// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The code_cvt_stdio case of ../test_code_cvt_stdio_char.cpp that needs a C stdio
 * which takes bytes back (dunget): Windows refuses.
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt_stdio.h>
#include <IOv2/cvt/stdin_root_cvt.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <cstdio>

using namespace IOv2;

// Half a character left at the end of the input is rolled back, and synchronized the
// root hands it back to C stdio: the read still reports it, and getchar() then gets the
// two bytes before the end.
TEST(CodeCvtStdio, AHalfCharacterAtTheEndGoesBackToCStdioWhenSynchronized)
{
    using Root = stdin_root_cvt;

    iguard g("a\xe4\xb8");
    code_cvt_stdio<Root> obj{Root{}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();
    wchar_t c = 0;
    EXPECT_EQ(obj.get(&c, 1), 1u);
    EXPECT_EQ(c, L'a');
    EXPECT_THROW(obj.get(&c, 1), cvt_error);

    EXPECT_EQ(std::getchar(), 0xe4);
    EXPECT_EQ(std::getchar(), 0xb8);
    EXPECT_EQ(std::getchar(), EOF);

    code_cvt_access acc;
    obj.retrieve(acc);
    EXPECT_EQ(acc.code, "zh_CN.UTF-8");
}
