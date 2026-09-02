// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <common/defs.h>
#include <cvt/cvt_concepts.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

using namespace IOv2;

namespace
{
    using MemCvt = rb_root_cvt<mem_device<char>>;

    constexpr std::size_t kSize = 4102;

    // 586 repetitions of the UTF-8 for U'李' U'伟' plus one byte cycling 1..127.
    // What matters here is the length: 4102 is not a multiple of any chunk size
    // below, so the loops cross the root converter's buffer boundary at every
    // offset.
    std::string sample()
    {
        std::string out;
        out.resize(kSize);
        for (std::size_t i = 0; i < kSize; i += 7)
        {
            out[i + 0] = '\xE6';
            out[i + 1] = '\x9D';
            out[i + 2] = '\x8E';
            out[i + 3] = '\xE4';
            out[i + 4] = '\xBC';
            out[i + 5] = '\x9F';
            out[i + 6] = (i / 7) % 127 + 1;
        }
        return out;
    }

    constexpr std::size_t kGetChunks[]  = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    constexpr std::size_t kWideChunks[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};

    // A device holding "hello" with the read cursor pushed to the end, so the
    // converter opens for output and appends rather than overwriting.
    MemCvt cvt_over_hello()
    {
        mem_device dev{"hello"};
        dev.drseek(0);
        return rb_root_cvt{std::move(dev)};
    }

    // A converter forked before any write shares nothing with the original: the
    // fork keeps the device as it was, the original goes on to grow it.
    template <typename T, typename Fork>
    void expect_a_fork_does_not_see_later_writes(const T& ori, Fork fork)
    {
        T obj = ori;
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        T forked = fork(obj);
        EXPECT_EQ(forked.device().str(), "hello");

        obj.put(" world", 6);
        obj.flush();
        EXPECT_EQ(obj.device().str(), "hello world");
        EXPECT_EQ(forked.device().str(), "hello");
    }

    template <typename T, typename Transfer>
    void expect_a_move_carries_the_device(const T& ori, Transfer transfer)
    {
        T obj = ori;
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        T moved = transfer(obj);
        EXPECT_EQ(moved.device().str(), "hello");
    }

    // A copy taken mid-read carries the read position, and the two then advance
    // independently: both must be able to read the same remaining six characters.
    template <typename T, typename Fork>
    void expect_a_fork_reads_on_independently(const T& ori, Fork fork)
    {
        T obj = ori;
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::string str(5, '\0');
        EXPECT_EQ(obj.get(str.data(), 5), 5u);
        EXPECT_EQ(str, "hello");
        EXPECT_EQ(obj.tell(), 5u);

        T forked = fork(obj);
        EXPECT_EQ(forked.tell(), 5u);
        str.resize(6);
        EXPECT_EQ(forked.get(str.data(), 6), 6u);
        EXPECT_EQ(str, " world");
        EXPECT_EQ(forked.tell(), 11u);

        str = "xxxxxx";
        EXPECT_EQ(obj.get(str.data(), 6), 6u);
        EXPECT_EQ(str, " world");
        EXPECT_EQ(obj.tell(), 11u);
    }

    // A move carries the read position too, but leaves nothing behind, so the
    // remaining characters have to come out of the target alone.
    template <typename T, typename Transfer>
    void expect_a_move_carries_the_read_position(const T& ori, Transfer transfer)
    {
        T obj = ori;
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::string str(5, '\0');
        EXPECT_EQ(obj.get(str.data(), 5), 5u);
        EXPECT_EQ(str, "hello");
        EXPECT_EQ(obj.tell(), 5u);

        T moved = transfer(obj);
        EXPECT_EQ(moved.tell(), 5u);
        str.resize(6);
        EXPECT_EQ(moved.get(str.data(), 6), 6u);
        EXPECT_EQ(str, " world");
        EXPECT_EQ(moved.tell(), 11u);
    }

    template <typename T, std::size_t N>
    void expect_chunked_get_reads_the_sample(T& obj, const std::string& expected,
                                             const std::size_t (&chunks)[N])
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::string out_buf(kSize, '\0');
        std::size_t total = 0;
        char*       cur   = out_buf.data();
        int         id    = 0;
        while (true)
        {
            std::size_t n = std::min<std::size_t>(kSize - total, chunks[id++]);
            auto        s = obj.get(cur, n);
            id %= N;
            cur   += s;
            total += s;
            if (s == 0) break;
        }

        EXPECT_EQ(total, kSize);
        EXPECT_EQ(cur, out_buf.data() + kSize);
        EXPECT_EQ(out_buf, expected);
    }

    template <typename T>
    void expect_chunked_put_writes_the_sample(T& obj, const std::string& plain)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        const char* cur = plain.data();
        int         id  = 0;
        while (cur < plain.data() + kSize)
        {
            std::size_t n = std::min<std::size_t>(kWideChunks[id++], plain.data() + kSize - cur);
            obj.put(cur, n);
            id %= std::size(kWideChunks);
            cur += n;
        }
        EXPECT_EQ(cur, plain.data() + kSize);

        obj.flush();
        const mem_device<char>& dev = obj.device();
        EXPECT_EQ(dev.str(), plain);
    }
}

TEST(RootCvtMem, TraitsWithoutAReadBuffer)
{
    using CheckType = no_rb_root_cvt<mem_device<char>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    static_assert(std::is_same_v<CheckType::internal_type, char>);
    static_assert(std::is_same_v<CheckType::external_type, char>);
    // A memory device is addressable and bidirectional, so the root converter
    // over it offers everything.
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(cvt_cpt::support_io_switch<CheckType>);
}

TEST(RootCvtMem, TraitsOverAChar32Device)
{
    using CheckType = rb_root_cvt<mem_device<char32_t>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char32_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
    static_assert(std::is_same_v<CheckType::external_type, char32_t>);
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(cvt_cpt::support_io_switch<CheckType>);
}

TEST(RootCvtMem, ACopyConstructedForkDoesNotSeeLaterWrites)
{
    expect_a_fork_does_not_see_later_writes(cvt_over_hello(),
                                            [](auto& src) { return MemCvt{src}; });
}

TEST(RootCvtMem, ACopyConstructedForkDoesNotSeeLaterWritesThroughARuntimeCvt)
{
    runtime_cvt ori{cvt_over_hello()};
    expect_a_fork_does_not_see_later_writes(ori, [](auto& src) { return runtime_cvt{src}; });
}

TEST(RootCvtMem, ACopyAssignedForkDoesNotSeeLaterWrites)
{
    expect_a_fork_does_not_see_later_writes(cvt_over_hello(), [](auto& src)
    {
        MemCvt dst{rb_root_cvt{mem_device("")}};
        dst = src;
        return dst;
    });
}

TEST(RootCvtMem, ACopyAssignedForkDoesNotSeeLaterWritesThroughARuntimeCvt)
{
    runtime_cvt ori{cvt_over_hello()};
    expect_a_fork_does_not_see_later_writes(ori, [](auto& src)
    {
        runtime_cvt dst{rb_root_cvt{mem_device("")}};
        dst = src;
        return dst;
    });
}

TEST(RootCvtMem, MoveConstructionCarriesTheDevice)
{
    expect_a_move_carries_the_device(cvt_over_hello(),
                                     [](auto& src) { return MemCvt{std::move(src)}; });
}

TEST(RootCvtMem, MoveConstructionCarriesTheDeviceThroughARuntimeCvt)
{
    runtime_cvt ori{cvt_over_hello()};
    expect_a_move_carries_the_device(ori, [](auto& src) { return runtime_cvt{std::move(src)}; });
}

TEST(RootCvtMem, MoveAssignmentCarriesTheDevice)
{
    expect_a_move_carries_the_device(cvt_over_hello(), [](auto& src)
    {
        MemCvt dst{rb_root_cvt{mem_device("")}};
        dst = std::move(src);
        return dst;
    });
}

TEST(RootCvtMem, MoveAssignmentCarriesTheDeviceThroughARuntimeCvt)
{
    runtime_cvt ori{cvt_over_hello()};
    expect_a_move_carries_the_device(ori, [](auto& src)
    {
        runtime_cvt dst{rb_root_cvt{mem_device("")}};
        dst = std::move(src);
        return dst;
    });
}

TEST(RootCvtMem, ACopyConstructedForkReadsOnIndependently)
{
    MemCvt ori{mem_device("hello world")};
    expect_a_fork_reads_on_independently(ori, [](auto& src) { return MemCvt{src}; });
}

TEST(RootCvtMem, ACopyConstructedForkReadsOnIndependentlyThroughARuntimeCvt)
{
    runtime_cvt ori{rb_root_cvt{mem_device("hello world")}};
    expect_a_fork_reads_on_independently(ori, [](auto& src) { return runtime_cvt{src}; });
}

TEST(RootCvtMem, ACopyAssignedForkReadsOnIndependently)
{
    MemCvt ori{mem_device("hello world")};
    expect_a_fork_reads_on_independently(ori, [](auto& src)
    {
        MemCvt dst{rb_root_cvt{mem_device("")}};
        dst = src;
        return dst;
    });
}

TEST(RootCvtMem, ACopyAssignedForkReadsOnIndependentlyThroughARuntimeCvt)
{
    runtime_cvt ori{rb_root_cvt{mem_device("hello world")}};
    expect_a_fork_reads_on_independently(ori, [](auto& src)
    {
        runtime_cvt dst{rb_root_cvt{mem_device("")}};
        dst = src;
        return dst;
    });
}

TEST(RootCvtMem, MoveConstructionCarriesTheReadPosition)
{
    MemCvt ori{mem_device("hello world")};
    expect_a_move_carries_the_read_position(ori, [](auto& src) { return MemCvt{std::move(src)}; });
}

TEST(RootCvtMem, MoveConstructionCarriesTheReadPositionThroughARuntimeCvt)
{
    runtime_cvt ori{rb_root_cvt{mem_device("hello world")}};
    expect_a_move_carries_the_read_position(ori,
                                            [](auto& src) { return runtime_cvt{std::move(src)}; });
}

TEST(RootCvtMem, MoveAssignmentCarriesTheReadPosition)
{
    MemCvt ori{mem_device("hello world")};
    expect_a_move_carries_the_read_position(ori, [](auto& src)
    {
        MemCvt dst{rb_root_cvt{mem_device("")}};
        dst = std::move(src);
        return dst;
    });
}

TEST(RootCvtMem, MoveAssignmentCarriesTheReadPositionThroughARuntimeCvt)
{
    runtime_cvt ori{rb_root_cvt{mem_device("hello world")}};
    expect_a_move_carries_the_read_position(ori, [](auto& src)
    {
        runtime_cvt dst{rb_root_cvt{mem_device("")}};
        dst = std::move(src);
        return dst;
    });
}

TEST(RootCvtMem, ChunkedGetReadsTheWholeSample)
{
    const std::string expected = sample();
    MemCvt            obj{mem_device(expected)};
    expect_chunked_get_reads_the_sample(obj, expected, kGetChunks);
}

TEST(RootCvtMem, ChunkedGetReadsTheWholeSampleThroughARuntimeCvt)
{
    const std::string expected = sample();
    runtime_cvt       obj{rb_root_cvt{mem_device(expected)}};
    expect_chunked_get_reads_the_sample(obj, expected, kGetChunks);
}

// The same read without a read-back buffer, and with one chunk (90) larger than
// the others so the no-buffer path has to serve an oversized request too.
TEST(RootCvtMem, ChunkedGetReadsTheWholeSampleWithoutAReadBuffer)
{
    const std::string expected = sample();
    auto              obj = no_rb_root_cvt{mem_device(expected)};
    expect_chunked_get_reads_the_sample(obj, expected, kWideChunks);
}

TEST(RootCvtMem, ChunkedGetReadsTheWholeSampleWithoutAReadBufferThroughARuntimeCvt)
{
    const std::string expected = sample();
    runtime_cvt       obj{no_rb_root_cvt{mem_device(expected)}};
    expect_chunked_get_reads_the_sample(obj, expected, kWideChunks);
}

TEST(RootCvtMem, ChunkedPutWritesTheWholeSample)
{
    const std::string plain = sample();
    MemCvt            obj{mem_device("")};
    expect_chunked_put_writes_the_sample(obj, plain);
}

TEST(RootCvtMem, ChunkedPutWritesTheWholeSampleThroughARuntimeCvt)
{
    const std::string plain = sample();
    runtime_cvt       obj{rb_root_cvt{mem_device("")}};
    expect_chunked_put_writes_the_sample(obj, plain);
}

namespace
{
    // seek() takes an absolute position, rseek() counts back from the end of the
    // five-character device, so rseek(3) lands on index 2.
    template <typename T>
    void expect_seek_and_rseek_land_where_asked(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        obj.seek(3);
        EXPECT_EQ(obj.tell(), 3u);

        char ch = 0;
        EXPECT_EQ(obj.get(&ch, 1), 1u);
        EXPECT_EQ(ch, '4');

        obj.rseek(3);
        EXPECT_EQ(obj.tell(), 2u);
        EXPECT_EQ(obj.get(&ch, 1), 1u);
        EXPECT_EQ(ch, '3');
    }

    // main_cont_beg() marks where the main content starts: reads taken before it
    // are a prologue, so tell() restarts at 0 and positions are counted from
    // there. An rseek that would land before that point must be refused and must
    // not move the position it failed to reach.
    template <typename T>
    void expect_positions_are_relative_to_the_main_content(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);

        char c = 0;
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, '1');
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, '2');
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, '3');

        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);

        obj.seek(3);
        EXPECT_EQ(obj.tell(), 3u);

        char ch = 0;
        EXPECT_EQ(obj.get(&ch, 1), 1u);
        EXPECT_EQ(ch, 'd');

        obj.rseek(3);
        EXPECT_EQ(obj.tell(), 4u);
        EXPECT_EQ(obj.get(&ch, 1), 1u);
        EXPECT_EQ(ch, 'e');

        EXPECT_ANY_THROW(obj.rseek(60));
        EXPECT_EQ(obj.tell(), 5u);

        EXPECT_ANY_THROW(obj.rseek(9));
        EXPECT_EQ(obj.tell(), 5u);
    }

    // detach() hands the device back and attach() gives a fresh one; the
    // converter has to come back to a state where bos() starts a new stream with
    // its own prologue and its own position origin.
    template <typename T>
    void expect_reattach_restarts_the_stream(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.put("hello", 5);
        obj.main_cont_beg();
        obj.put(" world", 6);
        EXPECT_EQ(obj.tell(), 6u);

        auto [ori, ori_err] = obj.detach();
        obj.attach(mem_device(""));
        EXPECT_EQ(ori.str(), "hello world");

        obj.bos();
        EXPECT_EQ(obj.tell(), 0u);
        obj.put("liwei", 5);
        obj.main_cont_beg();
        obj.put(" cpp", 4);
        EXPECT_EQ(obj.tell(), 4u);

        obj.flush();
        const mem_device<char>& new_dev = obj.device();
        EXPECT_EQ(new_dev.str(), "liwei cpp");
    }

    // Three sessions on one converter, with the first device detached, parked, and
    // attached again: the writes have to resume on the device they belong to, not
    // spill into whichever one happens to be attached.
    template <typename T>
    void expect_devices_can_be_swapped_in_and_out(T& obj)
    {
        using device_type = typename T::device_type;

        device_type f1("");
        obj.attach(std::move(f1));
        obj.bos();
        obj.main_cont_beg();
        obj.put("abc", 3);

        auto [detach1_dev, detach1_err] = obj.detach();
        f1 = std::move(detach1_dev);
        obj.attach(device_type(""));
        obj.bos();
        obj.main_cont_beg();
        obj.put("123", 3);

        auto [f2, f2_err] = obj.detach();
        obj.attach(std::move(f1));
        obj.bos();
        obj.main_cont_beg();
        obj.put("def", 3);

        auto [detach2_dev, detach2_err] = obj.detach();
        f1 = std::move(detach2_dev);
        EXPECT_EQ(f1.str(), "abcdef");
        EXPECT_EQ(f2.str(), "123");
    }
}

TEST(RootCvtMem, SeekAndRseekLandWhereAsked)
{
    MemCvt obj{mem_device("12345")};
    expect_seek_and_rseek_land_where_asked(obj);
}

TEST(RootCvtMem, SeekAndRseekLandWhereAskedThroughARuntimeCvt)
{
    runtime_cvt obj{rb_root_cvt{mem_device("12345")}};
    expect_seek_and_rseek_land_where_asked(obj);
}

TEST(RootCvtMem, PositionsAreRelativeToTheMainContent)
{
    MemCvt obj{mem_device("123abcdefg")};
    expect_positions_are_relative_to_the_main_content(obj);
}

TEST(RootCvtMem, PositionsAreRelativeToTheMainContentThroughARuntimeCvt)
{
    runtime_cvt obj{rb_root_cvt{mem_device("123abcdefg")}};
    expect_positions_are_relative_to_the_main_content(obj);
}

TEST(RootCvtMem, AttachAfterDetachRestartsTheStream)
{
    MemCvt obj{mem_device("")};
    expect_reattach_restarts_the_stream(obj);
}

TEST(RootCvtMem, AttachAfterDetachRestartsTheStreamThroughARuntimeCvt)
{
    runtime_cvt obj{rb_root_cvt{mem_device("")}};
    expect_reattach_restarts_the_stream(obj);
}

TEST(RootCvtMem, DevicesCanBeSwappedInAndOut)
{
    MemCvt obj{mem_device("")};
    expect_devices_can_be_swapped_in_and_out(obj);
}

TEST(RootCvtMem, DevicesCanBeSwappedInAndOutThroughARuntimeCvt)
{
    runtime_cvt obj{rb_root_cvt{mem_device("")}};
    expect_devices_can_be_swapped_in_and_out(obj);
}

// What the converter consumed from the device has to be reflected in the device
// it hands back: a detached device whose cursor was rewound would re-serve
// characters the caller already has.
TEST(RootCvtMem, DetachHandsBackTheDeviceAtTheReadPosition)
{
    MemCvt obj{mem_device("12345678")};
    char   ch = 0;
    obj.get(&ch, 1);
    EXPECT_EQ(ch, '1');
    EXPECT_EQ(obj.device().dtell(), 1u);

    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.dtell(), 1u);
}

TEST(RootCvtMem, DetachHandsBackTheDeviceAtTheReadPositionThroughARuntimeCvt)
{
    runtime_cvt obj{rb_root_cvt{mem_device("12345678")}};
    char        ch = 0;
    obj.get(&ch, 1);
    EXPECT_EQ(ch, '1');
    EXPECT_EQ(obj.device().dtell(), 1u);

    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.dtell(), 1u);
}

TEST(RootCvtMem, DetachHandsBackTheDeviceAtTheWritePosition)
{
    MemCvt obj{mem_device("")};
    obj.put("123", 3);
    EXPECT_EQ(obj.device().dtell(), 3u);

    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.dtell(), 3u);
}

TEST(RootCvtMem, DetachHandsBackTheDeviceAtTheWritePositionThroughARuntimeCvt)
{
    runtime_cvt obj{rb_root_cvt{mem_device("")}};
    obj.put("123", 3);
    EXPECT_EQ(obj.device().dtell(), 3u);

    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.dtell(), 3u);
}

// Attaching a new device afterwards must not reach back into the one already
// handed out.
TEST(RootCvtMem, AttachAfterDetachLeavesTheOldReadDeviceAlone)
{
    MemCvt obj{mem_device("12345678")};
    char   ch = 0;
    obj.get(&ch, 1);
    EXPECT_EQ(ch, '1');
    EXPECT_EQ(obj.device().dtell(), 1u);

    auto [dev, err] = obj.detach();
    obj.attach(mem_device{""});
    EXPECT_EQ(dev.dtell(), 1u);
}

TEST(RootCvtMem, AttachAfterDetachLeavesTheOldReadDeviceAloneThroughARuntimeCvt)
{
    runtime_cvt obj{rb_root_cvt{mem_device("12345678")}};
    char        ch = 0;
    obj.get(&ch, 1);
    EXPECT_EQ(ch, '1');
    EXPECT_EQ(obj.device().dtell(), 1u);

    auto [dev, err] = obj.detach();
    obj.attach(mem_device{""});
    EXPECT_EQ(dev.dtell(), 1u);
}

TEST(RootCvtMem, AttachAfterDetachLeavesTheOldWriteDeviceAlone)
{
    MemCvt obj{mem_device("")};
    obj.put("123", 3);
    EXPECT_EQ(obj.device().dtell(), 3u);

    auto [dev, err] = obj.detach();
    obj.attach(mem_device{""});
    EXPECT_EQ(dev.dtell(), 3u);
}

TEST(RootCvtMem, AttachAfterDetachLeavesTheOldWriteDeviceAloneThroughARuntimeCvt)
{
    runtime_cvt obj{rb_root_cvt{mem_device("")}};
    obj.put("123", 3);
    EXPECT_EQ(obj.device().dtell(), 3u);

    auto [dev, err] = obj.detach();
    obj.attach(mem_device{""});
    EXPECT_EQ(dev.dtell(), 3u);
}

// Both assignment operators guard against the source and the target being the
// same object; without the guard the device would be released before it is read.
TEST(RootCvtMem, SelfAssignmentLeavesTheStreamIntact)
{
    auto obj = cvt_over_hello();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.put(" world", 6);

    const auto& const_obj = obj;
    obj = const_obj;
    obj.flush();
    EXPECT_EQ(obj.device().str(), "hello world");

    auto* p_obj = &obj;
    obj = std::move(*p_obj);
    EXPECT_EQ(obj.device().str(), "hello world");
}

TEST(RootCvtMem, SelfAssignmentLeavesTheStreamIntactThroughARuntimeCvt)
{
    runtime_cvt obj{cvt_over_hello()};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.put(" world", 6);

    const auto& const_obj = obj;
    obj = const_obj;
    obj.flush();
    EXPECT_EQ(obj.device().str(), "hello world");

    auto* p_obj = &obj;
    obj = std::move(*p_obj);
    EXPECT_EQ(obj.device().str(), "hello world");
}

TEST(RootCvtMem, IsEofIsFalseOnANonEmptyStream)
{
    MemCvt obj{mem_device("hello")};
    obj.bos();
    obj.main_cont_beg();
    EXPECT_FALSE(obj.is_eof());
}

TEST(RootCvtMem, IsEofIsTrueOnAnEmptyStream)
{
    MemCvt obj{mem_device("")};
    obj.bos();
    obj.main_cont_beg();
    EXPECT_TRUE(obj.is_eof());
}

TEST(RootCvtMem, IsEofBecomesTrueOnceEverythingIsRead)
{
    MemCvt obj{mem_device("ab")};
    obj.bos();
    obj.main_cont_beg();

    char buf[2];
    obj.get(buf, 2);
    EXPECT_TRUE(obj.is_eof());
}

namespace
{
    // seek() adds the position to the length of the prologue before handing it to
    // the device. With a one-character prologue, SIZE_MAX would wrap, so the
    // overflow has to be caught before the addition rather than after.
    template <typename T>
    void expect_seek_rejects_an_overflowing_position(T& obj)
    {
        obj.bos();
        char ch = 0;
        obj.get(&ch, 1);     // the device cursor advances to 1
        obj.main_cont_beg(); // so the prologue is one character long

        EXPECT_THROW(obj.seek(std::numeric_limits<std::size_t>::max()), cvt_error);
    }
}

TEST(RootCvtMem, SeekRejectsAnOverflowingPosition)
{
    MemCvt obj{mem_device("hello")};
    expect_seek_rejects_an_overflowing_position(obj);
}

TEST(RootCvtMem, SeekRejectsAnOverflowingPositionThroughARuntimeCvt)
{
    runtime_cvt obj{rb_root_cvt{mem_device("hello")}};
    expect_seek_rejects_an_overflowing_position(obj);
}

// retrieve() has nothing to report for a memory device, but it is part of the
// converter interface and has to be callable.
TEST(RootCvtMem, RetrieveIsANoop)
{
    MemCvt obj{mem_device("hello")};
    obj.bos();
    obj.main_cont_beg();

    cvt_status stat;
    EXPECT_NO_THROW(obj.retrieve(stat));
}

// rseek() counts back from the end of the main content, which is the device size
// minus the prologue. Swapping in a device shorter than the prologue makes that
// subtraction impossible, and it has to be refused rather than wrapped.
TEST(RootCvtMem, RseekIsRejectedWhenTheDeviceIsShorterThanThePrologue)
{
    MemCvt obj{mem_device("hello world")};
    obj.bos();

    char buf[5];
    obj.get(buf, 5);     // the device cursor advances to 5
    obj.main_cont_beg(); // so the prologue is five characters long

    obj.device() = mem_device{""}; // now the whole device is shorter than that
    EXPECT_THROW(obj.rseek(0), cvt_error);
}
