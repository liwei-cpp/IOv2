// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/clocale_wrapper.h>
#include <IOv2/common/defs.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <stdlib.h>

using namespace IOv2;

namespace
{
    // LC_ALL outranks LC_CTYPE, so it has to be out of the way too.
    constexpr const char* kLocaleVars[] = {"LC_ALL", "LC_CTYPE"};

    class ClocaleWrapperMacosEnv : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            for (const char* name : kLocaleVars)
            {
                const char* value = std::getenv(name);
                m_saved.push_back(value ? std::optional<std::string>(value) : std::nullopt);
                ::unsetenv(name);
            }
        }

        void TearDown() override
        {
            for (std::size_t i = 0; i < std::size(kLocaleVars); ++i)
            {
                if (m_saved[i]) ::setenv(kLocaleVars[i], m_saved[i]->c_str(), 1);
                else            ::unsetenv(kLocaleVars[i]);
            }
        }

        std::vector<std::optional<std::string>> m_saved;
    };
}

// macOS reports the name as given: "POSIX" is not normalized to "C", and the name
// still builds the same locale again.
TEST(ClocaleWrapperMacos, PosixIsReportedAsPosix)
{
    const clocale_wrapper loc("POSIX");
    EXPECT_EQ(loc.name(), "POSIX");
    EXPECT_EQ(clocale_wrapper(loc).name(), "POSIX");
    EXPECT_EQ(clocale_wrapper(loc.name()).name(), "POSIX");
}

// UTF-8 is a locale with an LC_CTYPE category and nothing else. "" picks it up under
// LC_CTYPE=UTF-8, and the name reported then is one the constructor's
// newlocale(LC_ALL_MASK) rejects.
TEST_F(ClocaleWrapperMacosEnv, ACtypeOnlyLocaleNameBuildsNoWrapper)
{
    ::setenv("LC_CTYPE", "UTF-8", 1);
    const clocale_wrapper loc("");
    EXPECT_EQ(loc.name(), "UTF-8");
    EXPECT_THROW((void)clocale_wrapper(loc.name()), io_error);
}
