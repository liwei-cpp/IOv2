// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/code_cvt_stdio.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/stdin_root_cvt.h>
#include <IOv2/device/std_device.h>

#include <support/platform.h>
#include <support/platform_macros.h>
#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

#if IOV2_TEST_LINUX_GLIBC
#include <stdio_ext.h>
#endif

using namespace IOv2;

namespace
{
    namespace sys = iov2_test::sys;

    // The converter behind wcin / wcout: code_cvt_stdio accepts standard devices only.
    using StdioIn  = code_cvt_stdio<rb_root_cvt<std_device<sys::stdin_fd>>>;
    using StdioOut = code_cvt_stdio<rb_root_cvt<std_device<sys::stdout_fd>>>;
}

TEST(CodeCvtStdio, TraitsOverStderrWithWcharAsTheInternalType)
{
    using CheckType = code_cvt<rb_root_cvt<std_device<sys::stderr_fd>>, wchar_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, std_device<sys::stderr_fd>>);
    static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
    static_assert(std::is_same_v<CheckType::external_type, char>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(!cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

// code_cvt_stdio is the converter behind the standard streams: unlike code_cvt it
// can be told to change encoding at run time, which is what a locale change on
// IOv2::cout has to do.
TEST(CodeCvtStdio, TheEncodingCanBeSwitchedAtRunTime)
{
    StdioOut obj{rb_root_cvt{std_device<sys::stdout_fd>{}}, "C"};

    obj.adjust(code_cvt_switch{"zh_CN.UTF-8"});

    code_cvt_access status;
    obj.retrieve(status);
    EXPECT_EQ(status.code, "zh_CN.UTF-8");
}

// 0xE6 opens a three-byte UTF-8 sequence, so a stream that ends there is cut off
// mid-character: the read that reaches the end fails. The byte is not consumed -- it
// stays in the buffer -- so switching the encoding is not refused: from the switch on,
// what is left decodes by the new encoding.
TEST(CodeCvtStdio, TheEncodingCanBeSwitchedAfterAHalfCharacter)
{
    std::string partial;
    partial += '\xE6';
    iguard g(partial);

    StdioIn obj{rb_root_cvt{std_device<sys::stdin_fd>{}}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    wchar_t buf[4];
    EXPECT_THROW(obj.get(buf, 4), cvt_error);
    EXPECT_FALSE(obj.is_eof());

    EXPECT_NO_THROW(obj.adjust(code_cvt_switch{"C"}));
    code_cvt_access acc;
    obj.retrieve(acc);
    EXPECT_EQ(acc.code, "C");
    EXPECT_FALSE(obj.is_eof());
}

// The standard streams cannot be given another device, so code_cvt_stdio recovers
// from a taint by itself: the next put/get/flush/adjust reattaches the same fd and
// carries on. A character no encoding holds is the everyday way to get tainted --
// wcout << L'\xD800' -- and the caller's next insertion after clear() must work.
TEST(CodeCvtStdio, PutRecoversFromATaintOnItsOwn)
{
    oguard<true> g;
    StdioOut     obj{rb_root_cvt{std_device<sys::stdout_fd>{}}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    const wchar_t a[] = {L'a'};
    const wchar_t lone_surrogate[] = {L'\xD800'};
    const wchar_t b[] = {L'b'};

    obj.put(a, 1);
    EXPECT_THROW(obj.put(lone_surrogate, 1), cvt_error);
    EXPECT_TRUE(obj.is_tainted());

    EXPECT_NO_THROW(obj.put(b, 1));
    EXPECT_FALSE(obj.is_tainted());
    obj.flush();
    EXPECT_EQ(g.contents(), "ab"); // what was committed before the taint survives it
}

TEST(CodeCvtStdio, FlushRecoversFromATaintOnItsOwn)
{
    oguard<true> g;
    StdioOut     obj{rb_root_cvt{std_device<sys::stdout_fd>{}}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();

    const wchar_t lone_surrogate[] = {L'\xD800'};
    EXPECT_THROW(obj.put(lone_surrogate, 1), cvt_error);
    EXPECT_TRUE(obj.is_tainted());

    EXPECT_NO_THROW(obj.flush());
    EXPECT_FALSE(obj.is_tainted());
}

TEST(CodeCvtStdio, RetrieveReportsTheCurrentEncoding)
{
    StdioOut obj{rb_root_cvt{std_device<sys::stdout_fd>{}}, "zh_CN.UTF-8"};

    code_cvt_access status;
    obj.retrieve(status);
    EXPECT_EQ(status.code, "zh_CN.UTF-8");
}

// A status object this converter does not recognise is not an error: retrieve()
// passes it down so another stage of a pipe can answer it.
TEST(CodeCvtStdio, RetrieveWithAnUnknownStatusFallsThroughToTheBase)
{
    StdioOut obj{rb_root_cvt{std_device<sys::stdout_fd>{}}, "C"};

    cvt_status s;
    EXPECT_NO_THROW(obj.retrieve(s));
}

TEST(CodeCvtStdio, TheCreatorCarriesTheEncodingIntoTheConverter)
{
    static_assert(cvt_creator<code_cvt_stdio_creator>);

    code_cvt_stdio_creator creator{"zh_CN.UTF-8"};
    auto                   obj = creator.create(rb_root_cvt{std_device<sys::stdout_fd>{}});

    code_cvt_access status;
    obj.retrieve(status);
    EXPECT_EQ(status.code, "zh_CN.UTF-8");
}

// What wcin.sync_with_stdio() does now: it flips a flag on the root and rebuilds
// nothing, so what the unsynchronized root had read ahead is still handed out after a
// switch to synchronized.
TEST(CodeCvtStdio, ReadAheadBytesSurviveASwitchToSynchronized)
{
    using Root = stdin_root_cvt;

    iguard g("ab\xe4\xb8\xad" "cd");
    code_cvt_stdio<Root> obj{Root{}, "zh_CN.UTF-8"};
    obj.adjust(stdin_sync{false});              // the state is shared: switched back below
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();
    wchar_t c = 0;
    EXPECT_EQ(obj.get(&c, 1), 1u);
    EXPECT_EQ(c, L'a');

    obj.adjust(stdin_sync{true});
    std::wstring got;
    while (obj.get(&c, 1) == 1)
        got += c;
    EXPECT_EQ(got, L"b中cd");
}

// Half a character left at the end of the input is rolled back, and synchronized the
// root hands it back to C stdio: the read still reports it, and getchar() then gets the
// two bytes before the end.
TEST(CodeCvtStdio, AHalfCharacterAtTheEndGoesBackToCStdioWhenSynchronized)
{
#if !IOV2_TEST_WINDOWS
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
#else
    GTEST_SKIP() << "the CRT does not take bytes back into stdin (dunget)";
#endif
}

// A move hands the whole pipeline over -- the converter, its root buffer, and the
// device's duty to flush the FILE on destruction -- so the source must die without
// flushing. This is the layer wcout sits on; code_cvt's own move assignment adds
// a close_stream() that the root-level tests do not reach. Full buffering keeps
// the bytes in stdio, so __fpending() says whether the dying source flushed them.
TEST(CodeCvtStdio, AMovedFromConverterFlushesNothingWhenItDies)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true>       g;
    stdout_full_buffer buffered;

    auto moved = [&]
    {
        StdioOut src{rb_root_cvt{std_device<sys::stdout_fd>{}}, "zh_CN.UTF-8"};
        EXPECT_EQ(src.bos(), io_status::output);
        src.main_cont_beg();
        const wchar_t wen[] = {L'文'};
        src.put(wen, 1);
        src.flush();                                 // 3 bytes into stdio's buffer
        EXPECT_EQ(__fpending(stdout), 3u);
        return StdioOut{std::move(src)};
    }();                                             // src dies here
    EXPECT_EQ(__fpending(stdout), 3u);               // it neither flushed nor wrote

    const wchar_t bang[] = {L'!'};
    moved.put(bang, 1);
    moved.detach();
    std::fflush(stdout);
    EXPECT_EQ(g.contents(), "文!");
#else
    GTEST_SKIP() << "needs __fpending() from glibc's <stdio_ext.h>";
#endif
}

TEST(CodeCvtStdio, AMovedFromConverterFlushesNothingWhenItDiesAfterMoveAssignment)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true>       g;
    stdout_full_buffer buffered;

    StdioOut dst{rb_root_cvt{std_device<sys::stdout_fd>{}}, "zh_CN.UTF-8"};
    EXPECT_EQ(dst.bos(), io_status::output);
    dst.main_cont_beg();
    const wchar_t zhong[] = {L'中'};
    dst.put(zhong, 1);                               // 3 bytes in dst's root buffer
    {
        StdioOut src{rb_root_cvt{std_device<sys::stdout_fd>{}}, "zh_CN.UTF-8"};
        EXPECT_EQ(src.bos(), io_status::output);
        src.main_cont_beg();
        const wchar_t wen[] = {L'文'};
        src.put(wen, 1);
        src.flush();
        EXPECT_EQ(__fpending(stdout), 3u);

        dst = std::move(src);
        EXPECT_EQ(__fpending(stdout), 6u);           // dst's own bytes handed to stdio
    }                                                // src dies here
    EXPECT_EQ(__fpending(stdout), 6u);

    const wchar_t bang[] = {L'!'};
    dst.put(bang, 1);
    dst.detach();
    std::fflush(stdout);
    EXPECT_EQ(g.contents(), "文中!");
#else
    GTEST_SKIP() << "needs __fpending() from glibc's <stdio_ext.h>";
#endif
}

TEST(CodeCvtStdio, ASelfMoveChangesNothing)
{
#if IOV2_TEST_LINUX_GLIBC
    oguard<true>       g;
    stdout_full_buffer buffered;

    StdioOut obj{rb_root_cvt{std_device<sys::stdout_fd>{}}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    const wchar_t wen[] = {L'文'};
    obj.put(wen, 1);
    obj.flush();

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    obj = std::move(obj);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    EXPECT_EQ(__fpending(stdout), 3u);

    const wchar_t bang[] = {L'!'};
    obj.put(bang, 1);
    obj.detach();
    std::fflush(stdout);
    EXPECT_EQ(g.contents(), "文!");
#else
    GTEST_SKIP() << "needs __fpending() from glibc's <stdio_ext.h>";
#endif
}
