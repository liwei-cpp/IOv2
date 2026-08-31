#include <common/defs.h>
#include <cvt/crypt/vigenere_cvt.h>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace IOv2;

namespace
{
    using CharCvt   = Crypt::Classic::vigenere_cvt<rb_root_cvt<mem_device<char>>>;
    using CharCvtNr = Crypt::Classic::vigenere_cvt<no_rb_root_cvt<mem_device<char>>>;

    constexpr std::size_t kSize = 4102;

    // The external sample: 586 repetitions of the UTF-8 for U'李' U'伟' plus one
    // byte cycling 1..127. The seven-byte period matches the seven-byte key, so
    // every position's shift is fixed and the expected plaintext can be written
    // out in the same loop.
    void build_sample(std::string& external, std::string& internal, int sign)
    {
        external.resize(kSize);
        internal.resize(kSize);
        const char key[] = "liweixy";
        for (std::size_t i = 0; i < kSize; i += 7)
        {
            external[i + 0] = '\xE6';
            external[i + 1] = '\x9D';
            external[i + 2] = '\x8E';
            external[i + 3] = '\xE4';
            external[i + 4] = '\xBC';
            external[i + 5] = '\x9F';
            external[i + 6] = (i / 7) % 127 + 1;
            for (int k = 0; k < 7; ++k)
                internal[i + k] = static_cast<char>(external[i + k] + sign * key[k]);
        }
    }

    // Sizes the loops rotate through, so no call is aligned with the key period.
    constexpr std::size_t kGetChunks[] = {2, 41, 3, 5, 7, 11, 13, 17, 19};
    constexpr std::size_t kPutChunks[] = {2, 41, 3, 90, 7, 11, 13, 17, 19};

    // "hello" is already in the device and the converter opens for output at the
    // end of it, so the first six characters written are the ones the key shifts.
    CharCvt cvt_over_hello()
    {
        mem_device dev{"hello"};
        dev.drseek(0);
        return CharCvt{rb_root_cvt{std::move(dev)}, "abcdef"};
    }

    // Appending " world" to a stream whose key starts at 'a' gives a shift that
    // walks the key one letter per character.
    template <typename T>
    void expect_world_is_shifted_by_the_key(T& obj)
    {
        obj.put(" world", 6);
        obj.flush();

        const std::string s = obj.device().str();
        ASSERT_EQ(s.size(), 11u);
        EXPECT_EQ(s.substr(0, 5), "hello");
        EXPECT_EQ(s[5],  static_cast<char>(' ' + 'a'));
        EXPECT_EQ(s[6],  static_cast<char>('w' + 'b'));
        EXPECT_EQ(s[7],  static_cast<char>('o' + 'c'));
        EXPECT_EQ(s[8],  static_cast<char>('r' + 'd'));
        EXPECT_EQ(s[9],  static_cast<char>('l' + 'e'));
        EXPECT_EQ(s[10], static_cast<char>('d' + 'f'));
    }

    // A converter forked before any write shares the device contents but not the
    // key position, so writing through one must leave the other's view untouched.
    template <typename T, typename Fork>
    void expect_a_fork_does_not_see_later_writes(T& obj, Fork fork)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        T forked = fork(obj);
        EXPECT_EQ(forked.device().str(), "hello");

        expect_world_is_shifted_by_the_key(obj);
        EXPECT_EQ(forked.device().str(), "hello");
    }

    // Moving hands the device over wholesale: the target sees exactly what the
    // source had.
    template <typename T, typename Transfer>
    void expect_a_move_carries_the_device(T& obj, Transfer transfer)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        T moved = transfer(obj);
        EXPECT_EQ(moved.device().str(), "hello");
    }

    // Reads the whole sample back in rotating chunks and compares it character by
    // character with the expected plaintext.
    template <typename T>
    void expect_decrypts_to(T& obj, const std::string& expected)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);

        std::string out_buf(kSize, '\0');
        std::size_t total = 0;
        char*       cur   = out_buf.data();
        int         id    = 0;
        while (true)
        {
            std::size_t n = std::min<std::size_t>(kSize - total, kGetChunks[id++]);
            auto        s = obj.get(cur, n);
            id %= std::size(kGetChunks);
            cur   += s;
            total += s;
            if (s == 0) break;
        }

        EXPECT_EQ(total, kSize);
        EXPECT_EQ(cur, out_buf.data() + kSize);
        EXPECT_EQ(out_buf, expected);
    }
}

TEST(VigenereCvt, TraitsOverARbRootCvtOfChar)
{
    using CheckType = Crypt::Classic::vigenere_cvt<rb_root_cvt<mem_device<char>>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
    static_assert(std::is_same_v<CheckType::internal_type, char>);
    static_assert(std::is_same_v<CheckType::external_type, char>);
    // A per-character shift is position-addressable and direction-agnostic, so
    // unlike a compressor this converter keeps all four capabilities.
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(cvt_cpt::support_io_switch<CheckType>);
}

TEST(VigenereCvt, TraitsOverANoRbRootCvtOfChar32)
{
    using CheckType = Crypt::Classic::vigenere_cvt<no_rb_root_cvt<mem_device<char32_t>>>;
    static_assert(io_converter<CheckType>);
    static_assert(std::is_same_v<CheckType::device_type, mem_device<char32_t>>);
    static_assert(std::is_same_v<CheckType::internal_type, char32_t>);
    static_assert(std::is_same_v<CheckType::external_type, char32_t>);
    static_assert(cvt_cpt::support_put<CheckType>);
    static_assert(cvt_cpt::support_get<CheckType>);
    static_assert(cvt_cpt::support_positioning<CheckType>);
    static_assert(cvt_cpt::support_io_switch<CheckType>);
}

TEST(VigenereCvt, ACopyConstructedForkDoesNotSeeLaterWrites)
{
    auto obj = cvt_over_hello();
    expect_a_fork_does_not_see_later_writes(obj, [](auto& src) { return CharCvt{src}; });
}

TEST(VigenereCvt, ACopyConstructedForkDoesNotSeeLaterWritesThroughARuntimeCvt)
{
    runtime_cvt obj{cvt_over_hello()};
    expect_a_fork_does_not_see_later_writes(obj, [](auto& src) { return runtime_cvt{src}; });
}

TEST(VigenereCvt, ACopyAssignedForkDoesNotSeeLaterWrites)
{
    auto obj = cvt_over_hello();
    expect_a_fork_does_not_see_later_writes(obj, [](auto& src)
    {
        CharCvt dst{rb_root_cvt{mem_device("")}, "abcdef"};
        dst = src;
        return dst;
    });
}

TEST(VigenereCvt, ACopyAssignedForkDoesNotSeeLaterWritesThroughARuntimeCvt)
{
    runtime_cvt obj{cvt_over_hello()};
    expect_a_fork_does_not_see_later_writes(obj, [](auto& src)
    {
        runtime_cvt dst{CharCvt{rb_root_cvt{mem_device("")}, "abcdef"}};
        dst = src;
        return dst;
    });
}

TEST(VigenereCvt, MoveConstructionCarriesTheDevice)
{
    auto obj = cvt_over_hello();
    expect_a_move_carries_the_device(obj, [](auto& src) { return CharCvt{std::move(src)}; });
}

TEST(VigenereCvt, MoveConstructionCarriesTheDeviceThroughARuntimeCvt)
{
    runtime_cvt obj{cvt_over_hello()};
    expect_a_move_carries_the_device(obj, [](auto& src) { return runtime_cvt{std::move(src)}; });
}

TEST(VigenereCvt, MoveAssignmentCarriesTheDevice)
{
    auto obj = cvt_over_hello();
    expect_a_move_carries_the_device(obj, [](auto& src)
    {
        CharCvt dst{rb_root_cvt{mem_device("")}, "abcdef"};
        dst = std::move(src);
        return dst;
    });
}

TEST(VigenereCvt, MoveAssignmentCarriesTheDeviceThroughARuntimeCvt)
{
    runtime_cvt obj{cvt_over_hello()};
    expect_a_move_carries_the_device(obj, [](auto& src)
    {
        runtime_cvt dst{CharCvt{rb_root_cvt{mem_device("")}, "abcdef"}};
        dst = std::move(src);
        return dst;
    });
}

TEST(VigenereCvt, ChunkedGetDecryptsTheWholeStream)
{
    std::string external, internal;
    build_sample(external, internal, -1);

    CharCvt obj{rb_root_cvt{mem_device(external)}, "liweixy"};
    expect_decrypts_to(obj, internal);
}

TEST(VigenereCvt, ChunkedGetDecryptsTheWholeStreamThroughARuntimeCvt)
{
    std::string external, internal;
    build_sample(external, internal, -1);

    runtime_cvt obj{CharCvt{rb_root_cvt{mem_device(external)}, "liweixy"}};
    expect_decrypts_to(obj, internal);
}

// The same read through a root converter without a read-back buffer: the
// converter has to reassemble the stream from the device's own reads.
TEST(VigenereCvt, ChunkedGetDecryptsTheWholeStreamWithoutAReadBuffer)
{
    std::string external, internal;
    build_sample(external, internal, -1);

    CharCvtNr obj{no_rb_root_cvt{mem_device(external)}, "liweixy"};
    expect_decrypts_to(obj, internal);
}

TEST(VigenereCvt, ChunkedGetDecryptsTheWholeStreamWithoutAReadBufferThroughARuntimeCvt)
{
    std::string external, internal;
    build_sample(external, internal, -1);

    runtime_cvt obj{CharCvtNr{no_rb_root_cvt{mem_device(external)}, "liweixy"}};
    expect_decrypts_to(obj, internal);
}

namespace
{
    // The put mirror of expect_decrypts_to: writing the sample in rotating chunks
    // has to leave the device holding the key-shifted text.
    template <typename T>
    void expect_encrypts_to(T& obj, std::string& plain, const std::string& expected)
    {
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();
        EXPECT_EQ(obj.tell(), 0u);

        char* cur = plain.data();
        int   id  = 0;
        while (cur < plain.data() + kSize)
        {
            std::size_t n = std::min<std::size_t>(kPutChunks[id++], plain.data() + kSize - cur);
            obj.put(cur, n);
            id %= std::size(kPutChunks);
            cur += n;
        }
        EXPECT_EQ(cur, plain.data() + kSize);

        obj.flush();
        EXPECT_EQ(obj.device().str(), expected);
    }
}

TEST(VigenereCvt, ChunkedPutEncryptsTheWholeStream)
{
    std::string plain, expected;
    build_sample(plain, expected, +1);

    CharCvt obj{rb_root_cvt{mem_device("")}, "liweixy"};
    expect_encrypts_to(obj, plain, expected);
}

TEST(VigenereCvt, ChunkedPutEncryptsTheWholeStreamThroughARuntimeCvt)
{
    std::string plain, expected;
    build_sample(plain, expected, +1);

    runtime_cvt obj{CharCvt{rb_root_cvt{mem_device("")}, "liweixy"}};
    expect_encrypts_to(obj, plain, expected);
}

namespace
{
    // seek() and rseek() move the key position along with the read position: the
    // character that comes out depends on where in the key the converter thinks it
    // is, so a wrong position shows up as a wrong shift rather than a wrong byte.
    template <typename T>
    void expect_seek_moves_the_key_position(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        obj.seek(3);
        EXPECT_EQ(obj.tell(), 3u);

        char ch = 0;
        EXPECT_EQ(obj.get(&ch, 1), 1u);
        EXPECT_EQ(ch, static_cast<char>('4' - 'e'));

        // rseek counts from the end of the five-character device, so rseek(3)
        // lands on index 2.
        obj.rseek(3);
        EXPECT_EQ(obj.tell(), 2u);
        EXPECT_EQ(obj.get(&ch, 1), 1u);
        EXPECT_EQ(ch, static_cast<char>('3' - 'w'));
    }
}

TEST(VigenereCvt, SeekMovesTheKeyPosition)
{
    mem_device dev("12345");
    CharCvt    obj(rb_root_cvt{dev}, "liwei");
    expect_seek_moves_the_key_position(obj);
}

TEST(VigenereCvt, SeekMovesTheKeyPositionThroughARuntimeCvt)
{
    mem_device  dev("12345");
    runtime_cvt obj{CharCvt{rb_root_cvt{dev}, "liwei"}};
    expect_seek_moves_the_key_position(obj);
}

namespace
{
    // main_cont_beg() marks where the main content starts: reads taken before it
    // are a prologue, so tell() restarts at 0 and positions are counted from
    // there. Seeks past either end must throw and leave the position alone.
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
        EXPECT_EQ(ch, static_cast<char>('d' - 'e'));

        obj.rseek(3);
        EXPECT_EQ(obj.tell(), 4u);
        EXPECT_EQ(obj.get(&ch, 1), 1u);
        EXPECT_EQ(ch, static_cast<char>('e' - 'i'));

        // Past the start of the main content, and past its end: both are refused,
        // and neither is allowed to move the position it failed to reach.
        EXPECT_ANY_THROW(obj.rseek(60));
        EXPECT_EQ(obj.tell(), 5u);

        EXPECT_ANY_THROW(obj.rseek(9));
        EXPECT_EQ(obj.tell(), 5u);

        EXPECT_ANY_THROW(obj.seek(100));
        EXPECT_EQ(obj.tell(), 5u);
    }
}

TEST(VigenereCvt, PositionsAreRelativeToTheMainContent)
{
    Crypt::Classic::vigenere_cvt_creator<char> creator("liwei");
    auto obj = creator.create(rb_root_cvt{mem_device("123abcdefg")});
    expect_positions_are_relative_to_the_main_content(obj);
}

TEST(VigenereCvt, PositionsAreRelativeToTheMainContentThroughARuntimeCvt)
{
    Crypt::Classic::vigenere_cvt_creator<char> creator("liwei");
    runtime_cvt obj{creator.create(rb_root_cvt{mem_device("123abcdefg")})};
    expect_positions_are_relative_to_the_main_content(obj);
}

// An empty key would make the cipher the identity and hide the mistake, so it is
// rejected wherever a key enters: at the creator, at the constructor, and at
// attach() on a converter whose key was moved away.
TEST(VigenereCvt, AnEmptyKeyIsRejectedByTheCreator)
{
    EXPECT_THROW(Crypt::Classic::vigenere_cvt_creator<char>(""), cvt_error);
}

TEST(VigenereCvt, AnEmptyKeyIsRejectedByTheConstructor)
{
    std::string_view empty_key{};
    EXPECT_THROW(CharCvt(rb_root_cvt{mem_device("")}, empty_key), cvt_error);
}

TEST(VigenereCvt, AttachIsRejectedOnAMovedFromConverter)
{
    CharCvt obj(rb_root_cvt{mem_device("")}, "liwei");
    auto    moved = std::move(obj);

    EXPECT_THROW(obj.attach(), cvt_error);
}

// detach() resets the key position, so the stream written after a re-attach is
// shifted from the start of the key again rather than from wherever the previous
// stream left off.
TEST(VigenereCvt, DetachResetsTheKeyPosition)
{
    CharCvt obj(rb_root_cvt{mem_device("")}, "liwei");
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.put("hello", 5);
    obj.seek(3);
    EXPECT_EQ(obj.tell(), 3u);
    obj.detach();

    obj.attach();
    EXPECT_EQ(obj.bos(), io_status::output);
    obj.main_cont_beg();
    obj.put("world", 5);
    auto [dev, err] = obj.detach();

    const std::string expected = {
        static_cast<char>('w' + 'l'),
        static_cast<char>('o' + 'i'),
        static_cast<char>('r' + 'w'),
        static_cast<char>('l' + 'e'),
        static_cast<char>('d' + 'i'),
    };
    EXPECT_EQ(dev.str(), expected);
}
