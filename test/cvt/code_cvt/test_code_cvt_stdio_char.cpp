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
#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>

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
