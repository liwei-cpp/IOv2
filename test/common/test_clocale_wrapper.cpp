// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <common/clocale_wrapper.h>
#include <common/defs.h>

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

using namespace IOv2;

TEST(ClocaleWrapper, NothrowTraits)
{
    static_assert(std::is_nothrow_destructible_v<clocale_wrapper>);
    static_assert(std::is_nothrow_move_constructible_v<clocale_wrapper>);
    static_assert(std::is_nothrow_move_assignable_v<clocale_wrapper>);
}

// c_locale is private and clocale_wrapper exposes no accessor, so the only
// thing the move and copy paths can be checked for here is that they neither
// throw nor double-free. The freelocale() side is what the sanitizer and
// valgrind jobs are watching.
TEST(ClocaleWrapper, MoveConstructAndAssign)
{
    EXPECT_NO_THROW({
        clocale_wrapper loc1("C");
        clocale_wrapper loc2(std::move(loc1));

        clocale_wrapper loc3("C");
        loc3 = std::move(loc2);
    });
}

TEST(ClocaleWrapper, CopyConstructAndAssign)
{
    EXPECT_NO_THROW({
        clocale_wrapper loc1("C");
        clocale_wrapper loc2(loc1);

        clocale_wrapper loc3("C");
        loc3 = loc2;
    });
}

TEST(ClocaleWrapper, SelfAssignment)
{
    clocale_wrapper loc1("C");

    // Through a pointer, otherwise -Wself-assign-overloaded rejects it.
    EXPECT_NO_THROW([&loc1](clocale_wrapper* p) { loc1 = *p; }(&loc1));

    // Outside the macro: a #pragma inside a macro argument is not honoured, and
    // self-move-assignment is noexcept anyway, so there is nothing to wrap.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    loc1 = std::move(loc1);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

// A moved-from wrapper holds a null locale_t, and both copy paths have to take
// the null branch rather than hand it to duplocale().
TEST(ClocaleWrapper, CopyFromMovedFrom)
{
    EXPECT_NO_THROW({
        clocale_wrapper loc1("C");
        clocale_wrapper loc2(std::move(loc1));

        clocale_wrapper loc3(loc1);
        clocale_wrapper loc4("C");
        loc4 = loc1;
    });
}

TEST(ClocaleWrapper, NullNameThrows)
{
    EXPECT_THROW((void)clocale_wrapper(nullptr), cvt_error);
}

TEST(ClocaleWrapper, ClocaleUserRejectsMovedFrom)
{
    clocale_wrapper loc1("C");
    clocale_wrapper loc2(std::move(loc1));

    EXPECT_THROW((void)clocale_user{loc1}, cvt_error);
}

TEST(ClocaleWrapper, ClocaleUserAcceptsLiveWrapper)
{
    EXPECT_NO_THROW({
        clocale_wrapper loc3("C");
        clocale_user user(loc3);
    });
}
