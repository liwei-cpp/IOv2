// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The partial write of cvt/root_cvt/test_root_cvt_partial_write.cpp made real: a
 * file size limit (setrlimit, SIGXFSZ) stops fwrite partway.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/file_device.h>

#include <support/file_guard.h>
#include <support/test_child.h>

#include <gtest/gtest.h>

#include <csignal>
#include <fstream>
#include <iterator>
#include <string>

#include <sys/resource.h>

using namespace IOv2;

// The real thing: file_device's fwrite stops at a 5-byte file size limit and reports how
// much it took. The write is larger than stdio's own buffer, so fwrite goes to the file
// at once instead of buffering. The limit applies to the whole process, so the write runs
// in a child (support/test_child.h).
TEST(RootCvtPartialWrite, AFileDeviceReportsWhatFwriteAccepted)
{
    using WODev = basic_file_device<false, true, char>;
    const char* const file = "root_cvt_partial_write";

    if (in_test_child())
    {
        std::signal(SIGXFSZ, SIG_IGN);
        rlimit old{};
        ::getrlimit(RLIMIT_FSIZE, &old);
        rlimit small = old;
        small.rlim_cur = 5;

        WODev dev(file);
        const std::string big(10000, 'x');
        ::setrlimit(RLIMIT_FSIZE, &small);
        try
        {
            dev.dput(big.data(), big.size());
            ADD_FAILURE() << "the write did not fail";
        }
        catch (const dput_error& e)
        {
            EXPECT_EQ(e.written(), 5u);
        }
        ::setrlimit(RLIMIT_FSIZE, &old);
        return;
    }

    file_guard g(file, "");
    EXPECT_EQ(run_test_child("RootCvtPartialWrite.AFileDeviceReportsWhatFwriteAccepted"), 0)
        << "the checks in the child failed; see its output above";

    std::ifstream in(file, std::ios::binary);
    const std::string got{std::istreambuf_iterator<char>(in), {}};
    EXPECT_EQ(got, "xxxxx");
}
