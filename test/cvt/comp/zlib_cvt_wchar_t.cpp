// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <common/defs.h>
#include <cvt/comp/zlib_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

using namespace IOv2;

// The char counterpart of these cases lives in zlib_cvt_char.cpp. Here the
// converter's internal type is wchar_t while the device still holds char, so
// every put() of n characters reaches zlib as 4n bytes and the round trips below
// are also checking that the width change survives compression.
namespace
{
    constexpr std::size_t kSize = 4102;

    // The same pattern as the char sample, one code unit per byte value: six
    // repeating units the compressor can match, plus one cycling 1..127 so the
    // whole stream cannot collapse into a single match.
    std::wstring s_e_lit = []
    {
        std::wstring out;
        out.resize(kSize);
        for (std::size_t i = 0; i < kSize; i += 7)
        {
            out[i + 0] = L'\xE6';
            out[i + 1] = L'\x9D';
            out[i + 2] = L'\x8E';
            out[i + 3] = L'\xE4';
            out[i + 4] = L'\xBC';
            out[i + 5] = L'\x9F';
            out[i + 6] = (i / 7) % 127 + 1;
        }
        return out;
    }();

    // Sizes the put/get loops rotate through. 90 is in there so at least one chunk
    // is larger than zlib's own scratch buffer.
    constexpr std::size_t kChunks[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};

    using WcharCvt = Comp::zlib_cvt<rb_root_cvt<mem_device<char>>, wchar_t>;

    // Every case below is run twice: once on the statically typed zlib_cvt and
    // once on the same converter behind a runtime_cvt, which reaches it through a
    // virtual interface. The two have to agree byte for byte.
    WcharCvt static_cvt(unsigned level = 8)
    {
        return Comp::zlib_cvt_creator<wchar_t>{level}.create(rb_root_cvt{mem_device("")});
    }

    WcharCvt reader_over(const std::string& compressed)
    {
        return Comp::zlib_cvt_creator<wchar_t>{8}.create(rb_root_cvt{mem_device(compressed)});
    }

    auto runtime_of(WcharCvt&& obj) { return runtime_cvt{std::move(obj)}; }

    template <typename T>
    std::string put_in_chunks(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        wchar_t* cur = s_e_lit.data();
        int      id  = 0;
        while (cur < s_e_lit.data() + kSize)
        {
            std::size_t n = std::min<std::size_t>(kChunks[id++], s_e_lit.data() + kSize - cur);
            obj.put(cur, n);
            id %= std::size(kChunks);
            cur += n;
        }
        auto [dev, err] = obj.detach();

        EXPECT_EQ(cur, s_e_lit.data() + kSize);
        return dev.str();
    }

    template <typename T>
    void expect_inflates_to_sample(const std::string& compressed)
    {
        T obj{reader_over(compressed)};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::wstring out_buf(4200, L'\0');
        EXPECT_EQ(obj.get(out_buf.data(), 4200), kSize);
        EXPECT_EQ(out_buf.substr(0, kSize), s_e_lit);
    }

    // A copy taken mid-stream has to carry the compressor state with it: from the
    // fork on, the original and the copy see the same remaining input and must
    // produce the same output. `fork` is what makes the second converter -- copy
    // construction in one case, copy assignment in the other.
    template <typename T, typename Fork>
    void expect_fork_matches_original(T& obj, Fork fork)
    {
        std::string compressed;
        {
            EXPECT_EQ(obj.bos(), io_status::output);
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);

            T forked = fork(obj);
            obj.put(s_e_lit.data() + 1024, kSize - 1024);
            forked.put(s_e_lit.data() + 1024, kSize - 1024);

            auto [dev1, err1] = obj.detach();
            auto [dev2, err2] = forked.detach();

            EXPECT_EQ(dev1.str(), dev2.str());
            compressed = dev1.str();
        }

        T reader{reader_over(compressed)};
        EXPECT_EQ(reader.bos(), io_status::input);
        reader.main_cont_beg();

        std::wstring out_buf1(kSize, L'\0');
        std::wstring out_buf2(kSize - 1026, L'\0');

        EXPECT_EQ(reader.get(out_buf1.data(), 1026), 1026u);
        T forked = fork(reader);

        EXPECT_EQ(reader.get(out_buf1.data() + 1026, kSize - 1026), kSize - 1026);
        EXPECT_EQ(forked.get(out_buf2.data(), kSize - 1026), kSize - 1026);

        EXPECT_EQ(out_buf1, s_e_lit);
        EXPECT_EQ(out_buf1.substr(1026), out_buf2);
    }

    // Moving mid-stream leaves the source empty and the target holding the whole
    // stream, so the transfer is checked by finishing the stream through the
    // target alone. `transfer` is move construction or move assignment.
    template <typename T, typename Transfer>
    void expect_move_carries_the_stream(T& obj, Transfer transfer)
    {
        std::string compressed;
        {
            EXPECT_EQ(obj.bos(), io_status::output);
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), 1024);

            T moved = transfer(obj);
            moved.put(s_e_lit.data() + 1024, kSize - 1024);
            auto [dev, err] = moved.detach();
            compressed = dev.str();
        }

        T reader{reader_over(compressed)};
        EXPECT_EQ(reader.bos(), io_status::input);
        reader.main_cont_beg();

        std::wstring out_buf(kSize, L'\0');
        EXPECT_EQ(reader.get(out_buf.data(), 1026), 1026u);
        T moved = transfer(reader);

        EXPECT_EQ(moved.get(out_buf.data() + 1026, kSize - 1026), kSize - 1026);
        EXPECT_EQ(out_buf, s_e_lit);
    }
}

TEST(ZlibCvtWchar, TraitsOverARbRootCvtOfChar)
{
    using CheckType = Comp::zlib_cvt<root_cvt<mem_device<char>, true>, wchar_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
    static_assert(std::is_same_v<CheckType::external_type, char>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    // A deflate stream has no addressable positions, and switching direction
    // mid-stream would need a second zlib state.
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(ZlibCvtWchar, TraitsOverANoRbRootCvtOfChar8)
{
    using CheckType = Comp::zlib_cvt<root_cvt<mem_device<char8_t>, false>, wchar_t>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, wchar_t>);
    static_assert(std::is_same_v<CheckType::external_type, char8_t>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(ZlibCvtWchar, ACopyConstructedForkMatchesTheOriginal)
{
    auto obj = static_cvt();
    expect_fork_matches_original(obj, [](auto& src) { return WcharCvt{src}; });
}

TEST(ZlibCvtWchar, ACopyConstructedForkMatchesTheOriginalThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt());
    expect_fork_matches_original(obj, [](auto& src) { return runtime_cvt{src}; });
}

TEST(ZlibCvtWchar, ACopyAssignedForkMatchesTheOriginal)
{
    auto obj = static_cvt();
    expect_fork_matches_original(obj, [](auto& src)
    {
        auto dst = static_cvt();
        dst = src;
        return dst;
    });
}

TEST(ZlibCvtWchar, ACopyAssignedForkMatchesTheOriginalThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt());
    expect_fork_matches_original(obj, [](auto& src)
    {
        auto dst = runtime_of(static_cvt());
        dst = src;
        return dst;
    });
}

TEST(ZlibCvtWchar, MoveConstructionCarriesTheStream)
{
    auto obj = static_cvt();
    expect_move_carries_the_stream(obj, [](auto& src) { return WcharCvt{std::move(src)}; });
}

TEST(ZlibCvtWchar, MoveConstructionCarriesTheStreamThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt());
    expect_move_carries_the_stream(obj, [](auto& src) { return runtime_cvt{std::move(src)}; });
}

TEST(ZlibCvtWchar, MoveAssignmentCarriesTheStream)
{
    auto obj = static_cvt();
    expect_move_carries_the_stream(obj, [](auto& src)
    {
        auto dst = static_cvt();
        dst = std::move(src);
        return dst;
    });
}

TEST(ZlibCvtWchar, MoveAssignmentCarriesTheStreamThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt());
    expect_move_carries_the_stream(obj, [](auto& src)
    {
        auto dst = runtime_of(static_cvt());
        dst = std::move(src);
        return dst;
    });
}

// zlib takes 0..9; anything above is clamped rather than rejected, so a level of
// 15 still has to produce a usable stream.
TEST(ZlibCvtWchar, ALevelAboveNineIsClamped)
{
    WcharCvt obj{rb_root_cvt{mem_device("")}, 15};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    wchar_t data[] = L"hi";
    obj.put(data, 2);
    EXPECT_NO_THROW(obj.detach());
}

// adjust() dynamic_casts its argument to the behaviours zlib_cvt understands; a
// plain cvt_behavior matches none of them and must fall through to the base.
TEST(ZlibCvtWchar, AdjustWithABaseBehaviourFallsThroughToTheBase)
{
    WcharCvt obj{rb_root_cvt{mem_device("")}, 6};
    obj.bos();
    obj.main_cont_beg();
    cvt_behavior base_acc;
    EXPECT_NO_THROW(obj.adjust(base_acc));
    obj.detach();
}

TEST(ZlibCvtWchar, SelfAssignmentLeavesTheStreamIntact)
{
    WcharCvt obj{rb_root_cvt{mem_device("")}, 6};
    obj.bos();
    obj.main_cont_beg();
    wchar_t data[] = L"abc";
    obj.put(data, 3);

    const auto& const_obj = obj;
    obj = const_obj;
    EXPECT_NO_THROW(obj.detach());
}

// bos() only decides the direction; the 2-byte zlib header reaches the device
// when main_cont_beg() flushes it.
TEST(ZlibCvtWchar, MainContBegEmitsTheZlibHeader)
{
    WcharCvt    obj = static_cvt(6);
    const auto& dev = obj.device();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    EXPECT_EQ(dev.str(), "\x78\x9c");
}

TEST(ZlibCvtWchar, MainContBegEmitsTheZlibHeaderThroughARuntimeCvt)
{
    auto        obj = runtime_of(static_cvt(6));
    const auto& dev = obj.device();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    EXPECT_EQ(dev.str(), "\x78\x9c");
}

// Data written between bos() and main_cont_beg() reaches the device uncompressed,
// after the header -- and as raw wchar_t code units, four little-endian bytes
// each, which is what makes the width change visible at the byte level.
TEST(ZlibCvtWchar, PutBeforeMainContBegLandsAfterTheHeaderAsRawCodeUnits)
{
    WcharCvt    obj = static_cvt(6);
    const auto& dev = obj.device();
    EXPECT_EQ(obj.bos(), io_status::output);

    wchar_t buf[] = L"uvw";
    obj.put(buf, 3);

    obj.main_cont_beg();
    EXPECT_EQ(dev.str(), std::string("\x78\x9c""u\x00\x00\x00v\x00\x00\x00w\x00\x00\x00", 14));
}

TEST(ZlibCvtWchar, PutBeforeMainContBegLandsAfterTheHeaderThroughARuntimeCvt)
{
    auto        obj = runtime_of(static_cvt(6));
    const auto& dev = obj.device();
    EXPECT_EQ(obj.bos(), io_status::output);

    wchar_t buf[] = L"uvw";
    obj.put(buf, 3);

    obj.main_cont_beg();
    EXPECT_EQ(dev.str(), std::string("\x78\x9c""u\x00\x00\x00v\x00\x00\x00w\x00\x00\x00", 14));
}

TEST(ZlibCvtWchar, ChunkedPutRoundTrips)
{
    WcharCvt obj = static_cvt(8);
    expect_inflates_to_sample<WcharCvt>(put_in_chunks(obj));
}

TEST(ZlibCvtWchar, ChunkedPutRoundTripsThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt(8));
    expect_inflates_to_sample<runtime_cvt<mem_device<char>, wchar_t>>(put_in_chunks(obj));
}

// Level 0 stores rather than compresses, so the deflate path never emits a match
// and the reader has to cope with a stream longer than its input.
TEST(ZlibCvtWchar, ChunkedPutRoundTripsAtLevelZero)
{
    WcharCvt obj = static_cvt(0);
    expect_inflates_to_sample<WcharCvt>(put_in_chunks(obj));
}

TEST(ZlibCvtWchar, ChunkedPutRoundTripsAtLevelZeroThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt(0));
    expect_inflates_to_sample<runtime_cvt<mem_device<char>, wchar_t>>(put_in_chunks(obj));
}

namespace
{
    // The mirror of put_in_chunks: one put() of the whole sample, then a get()
    // loop in rotating chunks that stops when inflate reports nothing left.
    template <typename T>
    void expect_chunked_get_round_trip(T& obj)
    {
        std::string compressed;
        {
            EXPECT_EQ(obj.bos(), io_status::output);
            obj.main_cont_beg();
            obj.put(s_e_lit.data(), kSize);
            auto [dev, err] = obj.detach();
            compressed = dev.str();
        }

        T reader{reader_over(compressed)};
        EXPECT_EQ(reader.bos(), io_status::input);
        reader.main_cont_beg();

        constexpr std::size_t get_chunks[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        std::wstring out_buf(kSize, L'\0');
        std::size_t  total = 0;
        wchar_t*     cur   = out_buf.data();
        int          id    = 0;
        while (true)
        {
            std::size_t n = std::min<std::size_t>(kSize - total, get_chunks[id++]);
            if (n == 0) break;
            auto s = reader.get(cur, n);
            id %= std::size(get_chunks);
            cur   += s;
            total += s;
            if (s == 0) break;
        }

        EXPECT_EQ(cur - out_buf.data(), static_cast<std::ptrdiff_t>(kSize));
        EXPECT_EQ(out_buf, s_e_lit);
    }
}

TEST(ZlibCvtWchar, ChunkedGetRoundTrips)
{
    WcharCvt obj = static_cvt(6);
    expect_chunked_get_round_trip(obj);
}

TEST(ZlibCvtWchar, ChunkedGetRoundTripsThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt(6));
    expect_chunked_get_round_trip(obj);
}

namespace
{
    // Without zlib_sync_flush, flush() is advisory: deflate keeps buffering, so
    // flushing after every chunk has to produce exactly the same bytes as not
    // flushing at all.
    template <typename T>
    void expect_flush_is_transparent(T& obj)
    {
        const std::string unflushed = put_in_chunks(obj);

        T local{static_cvt(8)};
        EXPECT_EQ(local.bos(), io_status::output);
        local.main_cont_beg();

        wchar_t* cur = s_e_lit.data();
        int      id  = 0;
        while (cur < s_e_lit.data() + kSize)
        {
            std::size_t n = std::min<std::size_t>(kChunks[id++], s_e_lit.data() + kSize - cur);
            local.put(cur, n);
            local.flush();
            id %= std::size(kChunks);
            cur += n;
        }
        auto [dev, err] = local.detach();
        EXPECT_EQ(unflushed, dev.str());
    }
}

TEST(ZlibCvtWchar, FlushWithoutSyncFlushDoesNotChangeTheOutput)
{
    WcharCvt obj = static_cvt(8);
    expect_flush_is_transparent(obj);
}

TEST(ZlibCvtWchar, FlushWithoutSyncFlushDoesNotChangeTheOutputThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt(8));
    expect_flush_is_transparent(obj);
}

namespace
{
    // With zlib_sync_flush on, each flush() has to push a sync point out to the
    // device: the device must not grow on put() alone and must grow on flush().
    // The result is a longer stream than the unflushed one, and it still has to
    // inflate back to the sample.
    template <typename T>
    void expect_sync_flush_emits_eagerly(T& obj)
    {
        const std::string unflushed = put_in_chunks(obj);

        T local{static_cvt(8)};
        EXPECT_EQ(local.bos(), io_status::output);
        local.main_cont_beg();

        Comp::zlib_sync_flush acc(true);
        local.adjust(acc);

        wchar_t* cur = s_e_lit.data();
        int      id  = 0;
        while (cur < s_e_lit.data() + kSize)
        {
            std::size_t before = local.device().str().size();
            std::size_t n = std::min<std::size_t>(kChunks[id++], s_e_lit.data() + kSize - cur);
            local.put(cur, n);

            EXPECT_EQ(local.device().str().size(), before);
            local.flush();
            EXPECT_NE(local.device().str().size(), before);

            id %= std::size(kChunks);
            cur += n;
        }
        auto [dev, err] = local.detach();
        EXPECT_NE(unflushed, dev.str());

        expect_inflates_to_sample<T>(dev.str());
    }
}

TEST(ZlibCvtWchar, SyncFlushEmitsAfterEveryFlush)
{
    WcharCvt obj = static_cvt(8);
    expect_sync_flush_emits_eagerly(obj);
}

TEST(ZlibCvtWchar, SyncFlushEmitsAfterEveryFlushThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt(8));
    expect_sync_flush_emits_eagerly(obj);
}

namespace
{
    // detach() hands the device back and attach() gives the converter a fresh one.
    // A converter that survived that pair has to be usable again from bos()
    // onwards, and produce what a converter that was never detached produces.
    template <typename T>
    void expect_reattach_restarts_the_stream(T& obj)
    {
        const std::string first = put_in_chunks(obj);
        obj.attach(mem_device(""));

        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        wchar_t* cur = s_e_lit.data();
        int      id  = 0;
        while (cur < s_e_lit.data() + kSize)
        {
            std::size_t n = std::min<std::size_t>(kChunks[id++], s_e_lit.data() + kSize - cur);
            obj.put(cur, n);
            id %= std::size(kChunks);
            cur += n;
        }
        auto [dev, err] = obj.detach();
        EXPECT_EQ(first, dev.str());
    }
}

TEST(ZlibCvtWchar, AttachAfterDetachRestartsTheStream)
{
    WcharCvt obj = static_cvt(8);
    expect_reattach_restarts_the_stream(obj);
}

TEST(ZlibCvtWchar, AttachAfterDetachRestartsTheStreamThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt(8));
    expect_reattach_restarts_the_stream(obj);
}

// A stream that is nothing but the 2-byte header: bos() consumes it happily, and
// get() then finds no payload. inflate has neither reached Z_STREAM_END nor
// filled the output buffer, which is what "compressed stream truncated" means.
TEST(ZlibCvtWchar, GetThrowsOnATruncatedStream)
{
    std::string just_header("\x78\x9c", 2);
    WcharCvt    obj{rb_root_cvt{mem_device(just_header)}, 6};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    wchar_t buf[16] = {};
    EXPECT_THROW((void)obj.get(buf, 16), cvt_error);
}

// m_stream_ended latches at Z_STREAM_END: is_eof() reports it, and a further
// get() takes the early return rather than asking zlib again.
TEST(ZlibCvtWchar, IsEofLatchesAtStreamEnd)
{
    std::string compressed;
    {
        WcharCvt comp{rb_root_cvt{mem_device("")}, 6};
        comp.bos();
        comp.main_cont_beg();
        wchar_t data[] = L"hi";
        comp.put(data, 2);
        auto [dev, err] = comp.detach();
        ASSERT_FALSE(err);
        compressed = dev.str();
    }

    WcharCvt decomp{rb_root_cvt{mem_device(compressed)}, 6};
    EXPECT_EQ(decomp.bos(), io_status::input);
    decomp.main_cont_beg();

    wchar_t buf[64] = {};
    EXPECT_EQ(decomp.get(buf, 64), 2u);
    EXPECT_EQ(buf[0], L'h');
    EXPECT_EQ(buf[1], L'i');

    EXPECT_TRUE(decomp.is_eof());
    EXPECT_EQ(decomp.get(buf, 64), 0u);
}
