// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/clocale_wrapper.h>
#include <IOv2/common/defs.h>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <utility>

using namespace IOv2;

TEST(ClocaleWrapper, NothrowTraits)
{
    static_assert(std::is_nothrow_destructible_v<clocale_wrapper>);
    static_assert(std::is_nothrow_move_constructible_v<clocale_wrapper>);
    static_assert(std::is_nothrow_move_assignable_v<clocale_wrapper>);
}

// name() says which locale a wrapper holds but not whether it holds exactly one, so
// what the move and copy paths are checked for here is that they neither throw nor
// double-free. The freelocale() side is what the sanitizer and valgrind jobs are
// watching; NameSurvivesCopyAndMove covers the identity of what was handed over.
TEST(ClocaleWrapper, MoveConstructAndAssign)
{
    EXPECT_NO_THROW({
        clocale_wrapper loc1("C");
        clocale_wrapper loc2(std::move(loc1));

        clocale_wrapper loc3("C");
        loc3 = std::move(loc2);

        // loc1 is moved-from, so this also exercises assignment into an empty
        // target rather than only replacing a live locale.
        loc1 = std::move(loc3);
    });
}

TEST(ClocaleWrapper, CopyConstructAndAssign)
{
    EXPECT_NO_THROW({
        clocale_wrapper loc1("C");
        clocale_wrapper loc2(loc1);

        clocale_wrapper loc3("C");
        loc3 = loc2;

        // Move loc3's locale away, then copy back into the moved-from target.
        clocale_wrapper loc4(std::move(loc3));
        loc3 = loc4;
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

// name() reports the locale's own resolved LC_CTYPE name, which is what lets it stand
// in for the string the wrapper was built from: "" is a lookup rather than a name and
// comes back concrete, an alias comes back normalized, and whatever comes back builds
// the same locale again.
TEST(ClocaleWrapper, NameReportsTheResolvedLocale)
{
    EXPECT_EQ(clocale_wrapper("C").name(), "C");
    EXPECT_EQ(clocale_wrapper("POSIX").name(), "C");

    const std::string resolved = clocale_wrapper("").name();
    EXPECT_FALSE(resolved.empty());
    EXPECT_EQ(clocale_wrapper(resolved.c_str()).name(), resolved);
}

// duplocale() hands out a locale of its own, and it has to be the same locale; the
// source of a move keeps nothing and can no longer be asked.
TEST(ClocaleWrapper, NameSurvivesCopyAndMove)
{
    clocale_wrapper loc1("POSIX");
    clocale_wrapper loc2(loc1);
    EXPECT_EQ(loc2.name(), "C");

    clocale_wrapper loc3(std::move(loc1));
    EXPECT_EQ(loc3.name(), "C");
    EXPECT_THROW((void)loc1.name(), cvt_error);
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

TEST(CommonDefs, DefaultEofErrorMessage)
{
    const eof_error error;
    EXPECT_STREQ(error.what(), "end of file");
}
