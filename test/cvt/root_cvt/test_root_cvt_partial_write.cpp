// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * A device whose dput() fails partway reports how much it accepted through
 * dput_error; root_cvt keeps only the rest, so the flush after recovery writes
 * every character exactly once. A plain device_error means nothing was accepted.
 *
 * Before, root_cvt kept the whole batch and the recovery wrote the accepted
 * prefix a second time: `0123456789` under a 5-byte file limit came out as
 * `012340123456789`.
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/file_device.h>

#include <support/file_guard.h>
#include <support/test_child.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <csignal>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>

#include <sys/resource.h>

using namespace IOv2;

namespace
{
    std::string g_out;
    std::optional<std::size_t> g_accept;
    std::optional<std::size_t> g_report;
    bool g_plain = false;

    // Takes everything, except that one dput armed through g_accept takes that many
    // characters and throws dput_error, and one armed through g_plain takes none and
    // throws a plain device_error. g_report, if set, is what that dput_error claims was
    // taken, in place of the truth.
    struct partial_write_device
    {
        using char_type = char;

        void dput(const char* s, std::size_t n)
        {
            if (g_plain)
            {
                g_plain = false;
                throw device_error("partial_write_device: nothing accepted");
            }
            if (g_accept)
            {
                const std::size_t k = std::min(*g_accept, n);
                const std::size_t reported = g_report.value_or(k);
                g_accept.reset();
                g_report.reset();
                g_out.append(s, k);
                throw dput_error("partial_write_device: partial write", reported);
            }
            g_out.append(s, n);
        }
        void dflush() {}
    };

    using Root = rb_root_cvt<partial_write_device>;
    constexpr std::size_t kLen = Root::s_buffer_length;

    Root opened_root()
    {
        g_out.clear();
        g_accept.reset();
        g_report.reset();
        g_plain = false;
        Root obj{partial_write_device{}};
        obj.bos();
        obj.main_cont_beg();
        return obj;
    }
}

TEST(RootCvtPartialWrite, AFlushKeepsOnlyWhatTheDeviceDidNotAccept)
{
    auto obj = opened_root();
    obj.put("0123456789", 10);

    g_accept = 5;
    EXPECT_THROW(obj.flush(), dput_error);
    EXPECT_EQ(g_out, "01234");

    obj.flush();
    EXPECT_EQ(g_out, "0123456789");
}

// A device that claims more than it was given is taken at its word only up to what it
// was given: nothing is read past the buffer, nothing is kept, nothing goes out twice.
// The claim is clamped before it is used, so SIZE_MAX costs no more than one past the end.
TEST(RootCvtPartialWrite, AClaimBeyondTheBatchCountsAsTheWholeBatch)
{
    for (const std::size_t claim : {std::size_t{11}, std::numeric_limits<std::size_t>::max()})
    {
        SCOPED_TRACE(claim);
        auto obj = opened_root();
        obj.put("0123456789", 10);

        g_accept = 10;
        g_report = claim;
        EXPECT_THROW(obj.flush(), dput_error);
        EXPECT_EQ(g_out, "0123456789");

        obj.flush();
        EXPECT_EQ(g_out, "0123456789");
    }
}

TEST(RootCvtPartialWrite, APlainDeviceErrorKeepsTheWholeBatch)
{
    auto obj = opened_root();
    obj.put("0123456789", 10);

    g_plain = true;
    EXPECT_THROW(obj.flush(), device_error);
    EXPECT_EQ(g_out, "");

    obj.flush();
    EXPECT_EQ(g_out, "0123456789");
}

// A put that fills the buffer exactly writes it whole: its own characters count as
// accepted only once the write got past the earlier ones.
TEST(RootCvtPartialWrite, APutThatFillsTheBufferKeepsItsOwnCharactersOnlyOnceSomeLanded)
{
    const std::string earlier(kLen - 3, 'a');
    {
        auto obj = opened_root();
        obj.put(earlier.data(), earlier.size());
        g_accept = 5;
        EXPECT_THROW(obj.put("xyz", 3), dput_error);
        obj.flush();
        EXPECT_EQ(g_out, earlier) << "the failed put's characters were not accepted";
    }
    {
        auto obj = opened_root();
        obj.put(earlier.data(), earlier.size());
        g_accept = kLen - 1;
        EXPECT_THROW(obj.put("xyz", 3), dput_error);
        obj.flush();
        EXPECT_EQ(g_out, earlier + "xyz") << "part of the failed put landed, so the rest goes out";
    }
}

// A put that does not fit writes the earlier characters first; failing there, its own
// were not accepted.
TEST(RootCvtPartialWrite, APutThatOverflowsKeepsTheEarlierCharactersOnly)
{
    const std::string earlier(kLen - 3, 'a');
    auto obj = opened_root();
    obj.put(earlier.data(), earlier.size());

    g_accept = 5;
    EXPECT_THROW(obj.put("vwxyz", 5), dput_error);
    EXPECT_EQ(g_out, std::string(5, 'a'));

    obj.flush();
    EXPECT_EQ(g_out, earlier);
}

// A layer above writes through the root's own buffer (cvt_writer's root specialization);
// its put_buf makes room the same way once the buffer has a single slot left. The detach
// after the failure sends the rest.
TEST(RootCvtPartialWrite, ALayerAboveLosesNothingTheRootAccepted)
{
    g_out.clear();
    g_accept.reset();
    g_plain = false;
    code_cvt<Root, char32_t> obj{Root{partial_write_device{}}, "C"};
    obj.bos();
    obj.main_cont_beg();

    const std::u32string earlier(kLen - 1, U'a');
    obj.put(earlier.data(), earlier.size());
    EXPECT_EQ(g_out, "") << "the buffer was to hold all of it";

    g_accept = 7;
    const char32_t one_more = U'b';
    EXPECT_THROW(obj.put(&one_more, 1), dput_error);
    EXPECT_EQ(g_out, std::string(7, 'a'));

    auto [dev, err] = obj.detach();
    EXPECT_FALSE(err);
    EXPECT_EQ(g_out, std::string(kLen - 1, 'a')) << "the failed put's character was not accepted";
}

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
