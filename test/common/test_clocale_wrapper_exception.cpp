// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <clocale>

#include <IOv2/common/defs.h>

#include <gtest/gtest.h>

namespace
{
// Mocking system calls for testing exception paths
static bool mock_newlocale_fail = false;
static bool mock_duplocale_fail = false;

inline locale_t mock_newlocale(int category_mask, const char* locale, locale_t base)
{
    if (mock_newlocale_fail) return (locale_t)0;
    return newlocale(category_mask, locale, base);
}

inline locale_t mock_duplocale(locale_t loc)
{
    if (mock_duplocale_fail) return (locale_t)0;
    return duplocale(loc);
}
}

// Macro Interception:
// 1. Redirect system calls to our mocks
// 2. Rename the class to avoid ODR conflicts with the original version in the same binary
#define newlocale mock_newlocale
#define duplocale mock_duplocale
#define clocale_wrapper clocale_wrapper_mock
#define clocale_user clocale_user_mock

#include <IOv2/common/clocale_wrapper.h>

#undef newlocale
#undef duplocale
// We keep the class renames active for the rest of this file so we can use the names naturally

using namespace IOv2;

// The two mock switches are file-scope globals, so a case that left one armed
// would derail whichever case ran next. The fixture clears both on entry and
// exit rather than trusting each case to reset what it set.
class ClocaleWrapperMock : public ::testing::Test
{
protected:
    void SetUp() override { reset(); }
    void TearDown() override { reset(); }

    static void reset()
    {
        mock_newlocale_fail = false;
        mock_duplocale_fail = false;
    }
};

TEST_F(ClocaleWrapperMock, NewlocaleFailureThrows)
{
    mock_newlocale_fail = true;
    EXPECT_THROW((void)clocale_wrapper_mock("C"), cvt_error);
}

TEST_F(ClocaleWrapperMock, DuplocaleFailureInCopyConstructorThrows)
{
    clocale_wrapper_mock loc1("C");

    mock_duplocale_fail = true;
    EXPECT_THROW((void)clocale_wrapper_mock(loc1), cvt_error);
}

TEST_F(ClocaleWrapperMock, DuplocaleFailureInCopyAssignmentThrows)
{
    clocale_wrapper_mock loc1("C");
    clocale_wrapper_mock loc2("C");

    mock_duplocale_fail = true;
    EXPECT_THROW(loc2 = loc1, cvt_error);
}
