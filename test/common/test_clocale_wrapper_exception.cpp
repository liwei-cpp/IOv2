// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>

// Macro interception, at the backend rather than the C library so that it is the same on
// every platform. Every name whose body differs here from the rest of the binary is
// renamed, so the mocked copies never meet the real ones (no ODR clash):
// 1. The backend comes in first, its create/duplicate renamed *_real; #pragma once keeps
//    clocale_wrapper.h from including it again.
#define clocale_create clocale_create_real
#define clocale_duplicate clocale_duplicate_real
#include <IOv2/common/platform/clocale_backend.h>
#undef clocale_create
#undef clocale_duplicate

#include <gtest/gtest.h>

namespace
{
bool mock_create_fail = false;
bool mock_duplicate_fail = false;
}

namespace IOv2::detail
{
static clocale_handle clocale_create_mock(const char* name)
{
    if (mock_create_fail) return nullptr;
    return clocale_create_real(name);
}

static clocale_handle clocale_duplicate_mock(clocale_handle loc)
{
    if (mock_duplicate_fail) return nullptr;
    return clocale_duplicate_real(loc);
}
}

// 2. The wrapper calls the mocks, and its classes are renamed too.
#define clocale_create clocale_create_mock
#define clocale_duplicate clocale_duplicate_mock
#define clocale_wrapper clocale_wrapper_mock
#define clocale_user clocale_user_mock

#include <IOv2/common/clocale_wrapper.h>

#undef clocale_create
#undef clocale_duplicate
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
        mock_create_fail = false;
        mock_duplicate_fail = false;
    }
};

TEST_F(ClocaleWrapperMock, CreateFailureThrows)
{
    mock_create_fail = true;
    EXPECT_THROW((void)clocale_wrapper_mock("C"), io_error);
}

TEST_F(ClocaleWrapperMock, DuplicateFailureInCopyConstructorThrows)
{
    clocale_wrapper_mock loc1("C");

    mock_duplicate_fail = true;
    EXPECT_THROW((void)clocale_wrapper_mock(loc1), io_error);
}

TEST_F(ClocaleWrapperMock, DuplicateFailureInCopyAssignmentThrows)
{
    clocale_wrapper_mock loc1("C");
    clocale_wrapper_mock loc2("C");

    mock_duplicate_fail = true;
    EXPECT_THROW(loc2 = loc1, io_error);
}
