// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/cvt/comp/zlib_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/cvt/runtime_cvt.h>
#include <IOv2/device/mem_device.h>

#include <support/injectable_device.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

using namespace IOv2;

namespace
{
    constexpr std::size_t kSize = 4102;

    // 586 repetitions of the UTF-8 for U'李' U'伟' plus one byte cycling 1..127.
    // The repeating six bytes give the compressor something to find, the cycling
    // byte stops it from collapsing the whole stream into one match, and 4102 is a
    // multiple of neither the chunk sizes below nor zlib's internal buffer.
    std::string s_e_lit = []
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
    }();

    // Sizes the put/get loops rotate through. 90 is in there so at least one chunk
    // is larger than zlib's own scratch buffer.
    constexpr std::size_t kChunks[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};

    using CharCvt = Comp::zlib_cvt<rb_root_cvt<mem_device<char>>>;

    // Every case below is run twice: once on the statically typed zlib_cvt and
    // once on the same converter behind a runtime_cvt, which reaches it through a
    // virtual interface. The two have to agree byte for byte.
    CharCvt     static_cvt(unsigned level = 8) { return CharCvt{rb_root_cvt{mem_device("")}, level}; }
    auto        runtime_of(CharCvt&& obj) { return runtime_cvt{std::move(obj)}; }

    // Writes the whole sample through obj in rotating chunks and returns what the
    // device ended up holding.
    template <typename T>
    std::string put_in_chunks(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        char* cur = s_e_lit.data();
        int   id  = 0;
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

    // Decompresses `compressed` in one get() and checks it against the sample.
    template <typename T>
    void expect_inflates_to_sample(const std::string& compressed)
    {
        T obj{Comp::zlib_cvt{rb_root_cvt{mem_device(compressed)}, 0}};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::string out_buf(4200, '\0');
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

        T reader{Comp::zlib_cvt{rb_root_cvt{mem_device(compressed)}, 0}};
        EXPECT_EQ(reader.bos(), io_status::input);
        reader.main_cont_beg();

        std::string out_buf1(kSize, '\0');
        std::string out_buf2(kSize - 1026, '\0');

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

        T reader{Comp::zlib_cvt{rb_root_cvt{mem_device(compressed)}, 0}};
        EXPECT_EQ(reader.bos(), io_status::input);
        reader.main_cont_beg();

        std::string out_buf(kSize, '\0');
        EXPECT_EQ(reader.get(out_buf.data(), 1026), 1026u);
        T moved = transfer(reader);

        EXPECT_EQ(moved.get(out_buf.data() + 1026, kSize - 1026), kSize - 1026);
        EXPECT_EQ(out_buf, s_e_lit);
    }
}

TEST(ZlibCvt, TraitsOverARbRootCvtOfChar)
{
    using CheckType = Comp::zlib_cvt<rb_root_cvt<mem_device<char>>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    static_assert(std::is_same_v<CheckType::internal_type, char>);
    static_assert(std::is_same_v<CheckType::external_type, char>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    // A deflate stream has no addressable positions, and switching direction
    // mid-stream would need a second zlib state.
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(ZlibCvt, TraitsOverANoRbRootCvtOfChar8)
{
    using CheckType = Comp::zlib_cvt<no_rb_root_cvt<mem_device<char8_t>>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char8_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, char8_t>);
    static_assert(std::is_same_v<CheckType::external_type, char8_t>);

    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(!cvt_cpt::support_positioning<CheckType>);
    static_assert(!cvt_cpt::support_io_switch<CheckType>);
}

TEST(ZlibCvt, ACopyConstructedForkMatchesTheOriginal)
{
    auto obj = static_cvt();
    expect_fork_matches_original(obj, [](auto& src) { return CharCvt{src}; });
}

TEST(ZlibCvt, ACopyConstructedForkMatchesTheOriginalThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt());
    expect_fork_matches_original(obj, [](auto& src) { return runtime_cvt{src}; });
}

TEST(ZlibCvt, ACopyAssignedForkMatchesTheOriginal)
{
    auto obj = static_cvt();
    expect_fork_matches_original(obj, [](auto& src)
    {
        auto dst = static_cvt();
        dst = src;
        return dst;
    });
}

TEST(ZlibCvt, ACopyAssignedForkMatchesTheOriginalThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt());
    expect_fork_matches_original(obj, [](auto& src)
    {
        auto dst = runtime_of(static_cvt());
        dst = src;
        return dst;
    });
}

TEST(ZlibCvt, MoveConstructionCarriesTheStream)
{
    auto obj = static_cvt();
    expect_move_carries_the_stream(obj, [](auto& src) { return CharCvt{std::move(src)}; });
}

TEST(ZlibCvt, MoveConstructionCarriesTheStreamThroughARuntimeCvt)
{
    auto obj = runtime_of(static_cvt());
    expect_move_carries_the_stream(obj, [](auto& src) { return runtime_cvt{std::move(src)}; });
}

TEST(ZlibCvt, MoveAssignmentCarriesTheStream)
{
    auto obj = static_cvt();
    expect_move_carries_the_stream(obj, [](auto& src)
    {
        auto dst = static_cvt();
        dst = std::move(src);
        return dst;
    });
}

TEST(ZlibCvt, MoveAssignmentCarriesTheStreamThroughARuntimeCvt)
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
TEST(ZlibCvt, ALevelAboveNineIsClamped)
{
    Comp::zlib_cvt obj{rb_root_cvt{mem_device("")}, 15};
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    char data[] = "test data";
    obj.put(data, 9);
    EXPECT_NO_THROW(obj.detach());
}

// adjust() dynamic_casts its argument to the behaviours zlib_cvt understands; a
// plain cvt_behavior matches none of them and must fall through to the base.
TEST(ZlibCvt, AdjustWithABaseBehaviourFallsThroughToTheBase)
{
    Comp::zlib_cvt obj{rb_root_cvt{mem_device("")}, 6};
    obj.bos();
    obj.main_cont_beg();
    cvt_behavior base_acc;
    EXPECT_NO_THROW(obj.adjust(base_acc));
    obj.detach();
}

TEST(ZlibCvt, SelfAssignmentLeavesTheStreamIntact)
{
    Comp::zlib_cvt obj{rb_root_cvt{mem_device("")}, 6};
    obj.bos();
    obj.main_cont_beg();
    char data[] = "abc";
    obj.put(data, 3);

    const auto& const_obj = obj;
    obj = const_obj;
    EXPECT_NO_THROW(obj.detach());
}

// bos() only decides the direction; the 2-byte zlib header reaches the device
// when main_cont_beg() flushes it.
TEST(ZlibCvt, MainContBegEmitsTheZlibHeader)
{
    CharCvt     obj = static_cvt(6);
    const auto& dev = obj.device();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    EXPECT_EQ(dev.str(), "\x78\x9c");
}

TEST(ZlibCvt, MainContBegEmitsTheZlibHeaderThroughARuntimeCvt)
{
    runtime_cvt obj = runtime_of(static_cvt(6));
    const auto& dev = obj.device();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    EXPECT_EQ(dev.str(), "\x78\x9c");
}

// Data written between bos() and main_cont_beg() still lands after the header:
// the header is buffered ahead of it, not written when bos() returns.
TEST(ZlibCvt, PutBeforeMainContBegLandsAfterTheHeader)
{
    CharCvt     obj = static_cvt(6);
    const auto& dev = obj.device();
    EXPECT_EQ(obj.bos(), io_status::output);

    char buf[] = "123";
    obj.put(buf, 3);

    obj.main_cont_beg();
    EXPECT_EQ(dev.str(), "\x78\x9c""123");
}

TEST(ZlibCvt, PutBeforeMainContBegLandsAfterTheHeaderThroughARuntimeCvt)
{
    runtime_cvt obj = runtime_of(static_cvt(6));
    const auto& dev = obj.device();
    EXPECT_EQ(obj.bos(), io_status::output);

    char buf[] = "123";
    obj.put(buf, 3);

    obj.main_cont_beg();
    EXPECT_EQ(dev.str(), "\x78\x9c""123");
}

// 0xFF 0xFF: invalid zlib header (CM bits = 15; only 8 / deflate is legal).
// inflate returns Z_DATA_ERROR before producing output, so bos() unwinds while
// m_strm.next_in still points into the dying header_buf stack frame.
// inflate_guard's destructor must scrub that pointer; this test exercises the
// failure path so any future regression that reads stale m_strm fields during
// teardown is caught here. The destructor at the end of the test is part of what
// is under test -- it must complete cleanly.
TEST(ZlibCvt, BosThrowsOnAMalformedHeader)
{
    std::string    malformed("\xFF\xFF", 2);
    Comp::zlib_cvt obj{rb_root_cvt{mem_device(malformed)}, 6};
    EXPECT_ANY_THROW(obj.bos());
}

TEST(ZlibCvt, BosThrowsOnAMalformedHeaderThroughARuntimeCvt)
{
    std::string    malformed("\xFF\xFF", 2);
    Comp::zlib_cvt tmp{rb_root_cvt{mem_device(malformed)}, 6};
    runtime_cvt    obj{std::move(tmp)};
    EXPECT_ANY_THROW(obj.bos());
}

TEST(ZlibCvt, ChunkedPutRoundTrips)
{
    CharCvt obj = static_cvt(8);
    expect_inflates_to_sample<CharCvt>(put_in_chunks(obj));
}

TEST(ZlibCvt, ChunkedPutRoundTripsThroughARuntimeCvt)
{
    runtime_cvt obj = runtime_of(static_cvt(8));
    expect_inflates_to_sample<runtime_cvt<mem_device<char>, char>>(put_in_chunks(obj));
}

// Level 0 stores rather than compresses, so the deflate path never emits a match
// and the reader has to cope with a stream that is longer than its input.
TEST(ZlibCvt, ChunkedPutRoundTripsAtLevelZero)
{
    CharCvt obj = static_cvt(0);
    expect_inflates_to_sample<CharCvt>(put_in_chunks(obj));
}

TEST(ZlibCvt, ChunkedPutRoundTripsAtLevelZeroThroughARuntimeCvt)
{
    runtime_cvt obj = runtime_of(static_cvt(0));
    expect_inflates_to_sample<runtime_cvt<mem_device<char>, char>>(put_in_chunks(obj));
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

        T reader{Comp::zlib_cvt{rb_root_cvt{mem_device(compressed)}, 0}};
        EXPECT_EQ(reader.bos(), io_status::input);
        reader.main_cont_beg();

        constexpr std::size_t get_chunks[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};

        std::string out_buf(kSize, '\0');
        std::size_t total = 0;
        char*       cur   = out_buf.data();
        int         id    = 0;
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

TEST(ZlibCvt, ChunkedGetRoundTrips)
{
    Comp::zlib_cvt_creator<char> creator{6};
    auto                         obj = creator.create(rb_root_cvt{mem_device("")});
    expect_chunked_get_round_trip(obj);
}

TEST(ZlibCvt, ChunkedGetRoundTripsThroughARuntimeCvt)
{
    Comp::zlib_cvt_creator<char> creator{6};
    auto                         tmp = creator.create(rb_root_cvt{mem_device("")});
    runtime_cvt                  obj{std::move(tmp)};
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

        T           local{Comp::zlib_cvt{rb_root_cvt{mem_device("")}, 8}};
        EXPECT_EQ(local.bos(), io_status::output);
        local.main_cont_beg();

        char* cur = s_e_lit.data();
        int   id  = 0;
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

TEST(ZlibCvt, FlushWithoutSyncFlushDoesNotChangeTheOutput)
{
    Comp::zlib_cvt_creator<char> creator{8};
    auto                         obj = creator.create(rb_root_cvt{mem_device("")});
    expect_flush_is_transparent(obj);
}

TEST(ZlibCvt, FlushWithoutSyncFlushDoesNotChangeTheOutputThroughARuntimeCvt)
{
    Comp::zlib_cvt_creator<char> creator{8};
    auto                         tmp = creator.create(rb_root_cvt{mem_device("")});
    runtime_cvt                  obj{std::move(tmp)};
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

        T local{Comp::zlib_cvt{rb_root_cvt{mem_device("")}, 8}};
        EXPECT_EQ(local.bos(), io_status::output);
        local.main_cont_beg();

        Comp::zlib_sync_flush acc(true);
        local.adjust(acc);

        char* cur = s_e_lit.data();
        int   id  = 0;
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

TEST(ZlibCvt, SyncFlushEmitsAfterEveryFlush)
{
    Comp::zlib_cvt_creator<char> creator{8};
    auto                         obj = creator.create(rb_root_cvt{mem_device("")});
    expect_sync_flush_emits_eagerly(obj);
}

TEST(ZlibCvt, SyncFlushEmitsAfterEveryFlushThroughARuntimeCvt)
{
    Comp::zlib_cvt_creator<char> creator{8};
    auto                         tmp = creator.create(rb_root_cvt{mem_device("")});
    runtime_cvt                  obj{std::move(tmp)};
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

        char* cur = s_e_lit.data();
        int   id  = 0;
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

TEST(ZlibCvt, AttachAfterDetachRestartsTheStream)
{
    Comp::zlib_cvt_creator<char> creator{8};
    auto                         obj = creator.create(rb_root_cvt{mem_device("")});
    expect_reattach_restarts_the_stream(obj);
}

TEST(ZlibCvt, AttachAfterDetachRestartsTheStreamThroughARuntimeCvt)
{
    Comp::zlib_cvt_creator<char> creator{8};
    auto                         tmp = creator.create(rb_root_cvt{mem_device("")});
    runtime_cvt                  obj{std::move(tmp)};
    expect_reattach_restarts_the_stream(obj);
}

// A stream that is nothing but the 2-byte header: bos() consumes it happily, and
// get() then finds no payload. inflate has neither reached Z_STREAM_END nor
// filled the output buffer, which is what "compressed stream truncated" means.
TEST(ZlibCvt, GetThrowsOnATruncatedStream)
{
    std::string    just_header("\x78\x9c", 2);
    Comp::zlib_cvt obj{rb_root_cvt{mem_device(just_header)}, 6};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    char buf[16] = {};
    EXPECT_THROW((void)obj.get(buf, 16), cvt_error);
}

// m_stream_ended latches at Z_STREAM_END: is_eof() reports it, and a further
// get() takes the early return rather than asking zlib again.
TEST(ZlibCvt, IsEofLatchesAtStreamEnd)
{
    std::string compressed;
    {
        Comp::zlib_cvt comp{rb_root_cvt{mem_device("")}, 6};
        comp.bos();
        comp.main_cont_beg();
        char data[] = "hello";
        comp.put(data, 5);
        auto [dev, err] = comp.detach();
        ASSERT_FALSE(err);
        compressed = dev.str();
    }

    Comp::zlib_cvt decomp{rb_root_cvt{mem_device(compressed)}, 6};
    EXPECT_EQ(decomp.bos(), io_status::input);
    decomp.main_cont_beg();

    char buf[64] = {};
    EXPECT_EQ(decomp.get(buf, 64), 5u);
    EXPECT_EQ(buf[0], 'h');
    EXPECT_EQ(buf[4], 'o');

    EXPECT_TRUE(decomp.is_eof());
    EXPECT_EQ(decomp.get(buf, 64), 0u);
}

// bos() hands zlib the direction and bos_impl() allocates the matching state --
// deflateInit for output, inflateInit for input -- while main_cont_beg() resets
// m_io_status to neutral when it fails. Releasing that state needs the direction
// too, so abs_cvt must call detach_impl() BEFORE the reset; otherwise
// close_stream() matches neither branch and zlib's internal state (350 KB
// compressing, 41 KB decompressing) is never freed. bos_impl()'s own guards do
// not cover this: they are released by the time bos_impl returns.
//
// Like BosThrowsOnAMalformedHeader, a plain run cannot see the leak -- the
// sanitizer preset is what judges these. What a plain run checks is that bos()
// really succeeded and main_cont_beg() really failed, i.e. that the case still
// reaches the window at all.
namespace
{
    template <typename T>
    void expect_bos_succeeds_then_main_cont_beg_throws(T& obj, io_status dir)
    {
        EXPECT_EQ(obj.bos(), dir);
        EXPECT_ANY_THROW(obj.main_cont_beg());
    }

    std::string compressed_hello()
    {
        Comp::zlib_cvt comp{rb_root_cvt{mem_device("")}, 6};
        EXPECT_EQ(comp.bos(), io_status::output);
        comp.main_cont_beg();
        comp.put("hello", 5);
        auto [dev, err] = comp.detach();
        return dev.str();
    }
}

// Output: bos_impl() writes the 2-byte header into the root converter's buffer
// and main_cont_beg() flushes it out. A failing dput makes that flush throw on
// the write itself.
TEST(ZlibCvt, DeflateStateIsReleasedWhenTheHeaderWriteFails)
{
    injectable_device<char> dev{std::string("")};
    dev.shared_state()->fail_dput = true;

    Comp::zlib_cvt obj{no_rb_root_cvt{std::move(dev)}, 6};
    expect_bos_succeeds_then_main_cont_beg_throws(obj, io_status::output);
}

// The same window reached one step later: the write succeeds and the reposition
// query that follows it throws instead.
TEST(ZlibCvt, DeflateStateIsReleasedWhenThePostWriteTellFails)
{
    injectable_device<char> dev{std::string("")};
    dev.shared_state()->fail_dtell = true;

    Comp::zlib_cvt obj{no_rb_root_cvt{std::move(dev)}, 6};
    expect_bos_succeeds_then_main_cont_beg_throws(obj, io_status::output);
}

// Input: the header reads fine, so inflateInit's state is live when the query
// fails.
TEST(ZlibCvt, InflateStateIsReleasedWhenTheTellAfterTheHeaderFails)
{
    injectable_device<char> dev{compressed_hello()};
    dev.shared_state()->fail_dtell = true;

    Comp::zlib_cvt obj{rb_root_cvt{std::move(dev)}, 6};
    expect_bos_succeeds_then_main_cont_beg_throws(obj, io_status::input);
}

// Negative control for the three above: on the same payload, failing dget
// instead makes bos() itself throw. That is the path bos_impl()'s inflate_guard
// already covered and which never leaked. If it ever stops throwing, the three
// cases above have lost their window.
TEST(ZlibCvt, BosItselfThrowsWhenTheHeaderReadFails)
{
    injectable_device<char> dev{compressed_hello()};
    dev.shared_state()->fail_dget = true;

    Comp::zlib_cvt obj{rb_root_cvt{std::move(dev)}, 6};
    EXPECT_ANY_THROW((void)obj.bos());
}
