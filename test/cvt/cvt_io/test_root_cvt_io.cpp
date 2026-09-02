// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/cvt/abs_cvt.h>
#include <IOv2/cvt/crypt/vigenere_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>

#include <support/snatchy_device.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

using namespace IOv2;

namespace
{
    // Converter whose put_main always throws -- used to exercise abs_cvt's catch
    // block (m_is_tainted = true) and the assert_not_tainted() throw that follows.
    template <io_converter KernelType>
    struct failing_put_cvt
        : public abs_cvt<failing_put_cvt<KernelType>, KernelType,
                         typename KernelType::internal_type>
    {
        using IT = typename KernelType::internal_type;
        using BT = abs_cvt<failing_put_cvt<KernelType>, KernelType, IT>;
        friend BT;

    public:
        using device_type   = typename KernelType::device_type;
        using internal_type = IT;
        using external_type = IT;
        explicit failing_put_cvt(KernelType k) : BT(std::move(k)) {}

        // get_main is only invoked after assert_not_tainted() passes; the stub
        // returning 0 is never actually reached in the tainted-state test.
        std::size_t get_main(cvt_reader<KernelType>&, IT*, std::size_t) { return 0; }
        void        put_main(cvt_writer<KernelType>&, const IT*, std::size_t)
        {
            throw cvt_error("failing_put_cvt: simulated failure");
        }
    };

    // Exposes force_neutral_status() so a test can set m_io_status back to neutral
    // after main_cont_beg(), triggering the second guard in bos().
    template <io_converter KernelType>
    struct bos_guard_hack_cvt
        : public abs_cvt<bos_guard_hack_cvt<KernelType>, KernelType,
                         typename KernelType::internal_type>
    {
        using IT = typename KernelType::internal_type;
        using BT = abs_cvt<bos_guard_hack_cvt<KernelType>, KernelType, IT>;
        friend BT;

    public:
        using device_type   = typename KernelType::device_type;
        using internal_type = IT;
        using external_type = IT;
        explicit bos_guard_hack_cvt(KernelType k) : BT(std::move(k)) {}

        void        force_neutral_status() { this->m_io_status = io_status::neutral; }
        std::size_t get_main(cvt_reader<KernelType>&, IT*, std::size_t) { return 0; }
        void        put_main(cvt_writer<KernelType>&, const IT*, std::size_t) {}
    };

    // Minimal CRTP cvt: exposes `char` as internal_type while the kernel uses
    // `wchar_t` units. Lets us exercise the BOS partial-unit code path in
    // abs_cvt::get / abs_cvt::put (triggered when to_max * sizeof(char) is not a
    // multiple of sizeof(wchar_t)).
    template <io_converter KernelType>
        requires std::is_same_v<typename KernelType::internal_type, wchar_t>
    struct wext_char_cvt
        : public abs_cvt<wext_char_cvt<KernelType>, KernelType, char,
                         /*position=*/false, /*io_switch=*/false>
    {
        using BT = abs_cvt<wext_char_cvt<KernelType>, KernelType, char, false, false>;
        friend BT;

    public:
        using device_type   = typename KernelType::device_type;
        using internal_type = char;
        using external_type = wchar_t;
        explicit wext_char_cvt(KernelType k) : BT(std::move(k)) {}

        std::size_t get_main(cvt_reader<KernelType>&, char*, std::size_t) { return 0; }
        void        put_main(cvt_writer<KernelType>&, const char*, std::size_t) {}
    };

    // Minimal CRTP cvt with io-direction switching disabled
    // (default_io_switch=false). Used to exercise the "cannot switch to
    // input/output mode" error paths in abs_cvt::get / abs_cvt::put.
    template <io_converter KernelType>
    struct no_switch_cvt
        : public abs_cvt<no_switch_cvt<KernelType>, KernelType,
                         typename KernelType::internal_type,
                         /*position=*/false, /*io_switch=*/false>
    {
        using IT = typename KernelType::internal_type;
        using BT = abs_cvt<no_switch_cvt<KernelType>, KernelType, IT, false, false>;
        friend BT;

    public:
        using device_type   = typename KernelType::device_type;
        using internal_type = IT;
        using external_type = IT;
        explicit no_switch_cvt(KernelType k) : BT(std::move(k)) {}

        std::size_t get_main(cvt_reader<KernelType>& r, IT* to, std::size_t n)
        {
            auto [ptr, len] = r.get_buf(n);
            std::copy(ptr, ptr + len, to);
            return len;
        }
        void put_main(cvt_writer<KernelType>& w, const IT* from, std::size_t n)
        {
            auto* ptr = w.put_buf(n);
            std::copy(from, from + n, ptr);
            w.commit();
        }
    };

    // Minimal write-only device for the cvt_writer specialization that does not
    // reserve inside the device.
    struct str_write_device
    {
        using char_type = char;
        std::string result;
        void        dput(const char_type* s, std::size_t n) { result.append(s, n); }
        void        dflush() {}
    };

    std::string alphabet_1k()
    {
        std::string ref;
        ref.reserve(1024);
        for (std::size_t i = 0; i < 1024; ++i) ref += 'a' + (i % 26);
        return ref;
    }

    // Every case in this half is run against both root converters: rb_root_cvt
    // keeps a read-back buffer of its own, no_rb_root_cvt does not, and the
    // staging layer has to look the same either way.
    template <typename T>
    void expect_reader_serves_and_rolls_back(T& obj)
    {
        obj.bos();
        obj.main_cont_beg();

        std::vector<typename T::internal_type> buf;
        cvt_reader<T>                          reader(obj, buf);
        reader.reset(1024);

        auto [ptr, len] = reader.get_buf(3);
        EXPECT_EQ(len, 3u);
        EXPECT_EQ(std::string(ptr, ptr + len), "123");

        std::tie(ptr, len) = reader.get_buf(2);
        EXPECT_EQ(len, 2u);
        EXPECT_EQ(std::string(ptr, ptr + len), "45");

        // Rolling back two puts "45" back on offer, so the next request for five
        // can only be served the three characters that are left.
        reader.rollback(2);
        std::tie(ptr, len) = reader.get_buf(5);
        EXPECT_EQ(len, 3u);
        EXPECT_EQ(std::string(ptr, ptr + len), "456");
    }

    // With a staging buffer of exactly five, a rollback has to be honoured out of
    // what is still buffered and the refill that follows has to slide the window
    // rather than restart it.
    template <typename T>
    void expect_reader_refills_after_rollback(T& obj)
    {
        obj.bos();
        obj.main_cont_beg();

        std::vector<typename T::internal_type> buf;
        cvt_reader<T>                          reader(obj, buf);
        reader.reset(5);

        auto [ptr, len] = reader.get_buf(5);
        EXPECT_EQ(len, 5u);
        EXPECT_EQ(std::string(ptr, ptr + len), "01234");

        reader.rollback(1);
        std::tie(ptr, len) = reader.get_buf(2);
        EXPECT_EQ(len, 2u);
        EXPECT_EQ(std::string(ptr, ptr + len), "45");

        reader.rollback(2);
        std::tie(ptr, len) = reader.get_buf(5);
        EXPECT_EQ(len, 5u);
        EXPECT_EQ(std::string(ptr, ptr + len), "45678");
    }

    // A staging buffer of seven against requests cycling 1..7: every combination
    // of request size and buffer offset is hit at least once over 1024 bytes.
    template <typename T>
    void expect_reader_serves_every_chunk_size(T& obj, const std::string& ref)
    {
        obj.bos();
        obj.main_cont_beg();

        std::string res;
        {
            std::vector<typename T::internal_type> buf;
            cvt_reader<T>                          reader(obj, buf);
            reader.reset(7);

            std::size_t cur     = 0;
            std::size_t get_len = 1;
            while (cur < 1024)
            {
                auto [ptr, len] = reader.get_buf(std::min<std::size_t>(1024 - cur, get_len));
                std::copy(ref.data() + cur, ref.data() + cur + len, std::back_inserter(res));
                cur += len;
                get_len = (get_len % 7) + 1;
            }
            EXPECT_EQ(cur, 1024u);
        }
        EXPECT_EQ(res, ref);
    }

    // The saturating overload must return exactly what was asked for, so it has to
    // keep pulling from a device that never hands over more than three at a time.
    template <typename T>
    void expect_saturating_reader_fills(T& obj, const std::string& ref)
    {
        obj.bos();
        obj.main_cont_beg();

        std::string res;
        {
            std::vector<typename T::internal_type> buf;
            cvt_reader<T>                          reader(obj, buf);
            reader.reset(7);

            std::size_t cur     = 0;
            std::size_t get_len = 1;
            while (cur < 1024)
            {
                const std::size_t aim_len = std::min<std::size_t>(1024 - cur, get_len);
                auto              ptr     = reader.template get_buf<true>(aim_len);
                std::copy(ptr, ptr + aim_len, std::back_inserter(res));
                cur += aim_len;
                get_len = (get_len % 7) + 1;
            }
            EXPECT_EQ(cur, 1024u);
        }
        EXPECT_EQ(res, ref);
    }

    template <typename T>
    void expect_writer_concatenates_reservations(T& obj)
    {
        obj.bos();
        obj.main_cont_beg();
        {
            std::vector<typename T::internal_type> buf;
            cvt_writer<T>                          writer(obj, buf);
            writer.reset(1024);

            auto        ptr = writer.put_buf(3);
            std::string str = "123";
            std::copy(str.begin(), str.end(), ptr);

            ptr = writer.put_buf(2);
            str = "45";
            std::copy(str.begin(), str.end(), ptr);

            ptr = writer.put_buf(2);
            str = "67";
            std::copy(str.begin(), str.end(), ptr);
            writer.commit();
        }

        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.str(), "1234567");
    }

    // Each reservation here is larger than what gets written into it, and the
    // difference is handed back: only what was actually filled may reach the
    // device.
    template <typename T>
    void expect_writer_rollback_trims_the_reservation(T& obj)
    {
        obj.bos();
        obj.main_cont_beg();
        {
            std::vector<typename T::internal_type> buf;
            cvt_writer<T>                          writer(obj, buf);
            writer.reset(1024);

            auto        ptr = writer.put_buf(5);
            std::string str = "123";
            std::copy(str.begin(), str.end(), ptr);
            writer.rollback(2);

            ptr = writer.put_buf(3);
            str = "45";
            std::copy(str.begin(), str.end(), ptr);
            writer.rollback(1);

            ptr = writer.put_buf(3);
            str = "67";
            std::copy(str.begin(), str.end(), ptr);
            writer.rollback(1);
            writer.commit();
        }

        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.str(), "1234567");
    }

    // A staging buffer of ten against reservations cycling 1..10, so the buffer
    // has to be flushed at every offset within it.
    template <typename T>
    void expect_writer_serves_every_chunk_size(T& obj, const std::string& ref)
    {
        obj.bos();
        obj.main_cont_beg();
        {
            std::vector<typename T::internal_type> buf;
            cvt_writer<T>                          writer(obj, buf);
            writer.reset(10);

            std::size_t cur     = 0;
            std::size_t get_len = 1;
            while (cur < 1024)
            {
                auto aim_len = std::min<std::size_t>(1024 - cur, get_len);
                auto ptr     = writer.put_buf(aim_len);
                std::copy(ref.data() + cur, ref.data() + cur + aim_len, ptr);
                cur += aim_len;
                get_len = (get_len % 10) + 1;
            }
            EXPECT_EQ(cur, 1024u);
            writer.commit();
        }

        auto [dev, err] = obj.detach();
        EXPECT_EQ(dev.str(), ref);
    }
}

TEST(CvtIo, ReaderServesAndRollsBackWithAReadBuffer)
{
    auto obj = rb_root_cvt{mem_device{"123456"}};
    expect_reader_serves_and_rolls_back(obj);
}

TEST(CvtIo, ReaderServesAndRollsBackWithoutAReadBuffer)
{
    auto obj = no_rb_root_cvt{mem_device{"123456"}};
    expect_reader_serves_and_rolls_back(obj);
}

TEST(CvtIo, ReaderRefillsAfterRollbackWithAReadBuffer)
{
    auto obj = rb_root_cvt{mem_device{"0123456789"}};
    expect_reader_refills_after_rollback(obj);
}

TEST(CvtIo, ReaderRefillsAfterRollbackWithoutAReadBuffer)
{
    auto obj = no_rb_root_cvt{mem_device{"0123456789"}};
    expect_reader_refills_after_rollback(obj);
}

TEST(CvtIo, ReaderServesEveryChunkSizeWithAReadBuffer)
{
    const std::string ref = alphabet_1k();
    auto              obj = rb_root_cvt{mem_device{ref}};
    expect_reader_serves_every_chunk_size(obj, ref);
}

TEST(CvtIo, ReaderServesEveryChunkSizeWithoutAReadBuffer)
{
    const std::string ref = alphabet_1k();
    auto              obj = no_rb_root_cvt{mem_device{ref}};
    expect_reader_serves_every_chunk_size(obj, ref);
}

TEST(CvtIo, SaturatingReaderFillsAcrossShortDeviceReadsWithAReadBuffer)
{
    const std::string ref = alphabet_1k();
    auto              obj = rb_root_cvt{snatchy_device<char, 3>{ref}};
    expect_saturating_reader_fills(obj, ref);
}

TEST(CvtIo, SaturatingReaderFillsAcrossShortDeviceReadsWithoutAReadBuffer)
{
    const std::string ref = alphabet_1k();
    auto              obj = no_rb_root_cvt{snatchy_device<char, 3>{ref}};
    expect_saturating_reader_fills(obj, ref);
}

TEST(CvtIo, WriterConcatenatesReservationsWithAReadBuffer)
{
    auto obj = rb_root_cvt{mem_device{""}};
    expect_writer_concatenates_reservations(obj);
}

TEST(CvtIo, WriterConcatenatesReservationsWithoutAReadBuffer)
{
    auto obj = no_rb_root_cvt{mem_device{""}};
    expect_writer_concatenates_reservations(obj);
}

TEST(CvtIo, WriterRollbackTrimsTheReservationWithAReadBuffer)
{
    auto obj = rb_root_cvt{mem_device{""}};
    expect_writer_rollback_trims_the_reservation(obj);
}

TEST(CvtIo, WriterRollbackTrimsTheReservationWithoutAReadBuffer)
{
    auto obj = no_rb_root_cvt{mem_device{""}};
    expect_writer_rollback_trims_the_reservation(obj);
}

TEST(CvtIo, WriterServesEveryChunkSizeWithAReadBuffer)
{
    const std::string ref = alphabet_1k();
    auto              obj = rb_root_cvt{mem_device{""}};
    expect_writer_serves_every_chunk_size(obj, ref);
}

TEST(CvtIo, WriterServesEveryChunkSizeWithoutAReadBuffer)
{
    const std::string ref = alphabet_1k();
    auto              obj = no_rb_root_cvt{mem_device{""}};
    expect_writer_serves_every_chunk_size(obj, ref);
}

// The rest of the reader/writer cases use a device that is not a mem_device, so
// they run against the general root_cvt specializations rather than the
// mem_device ones exercised above.
namespace
{
    using SnatchyCvt = rb_root_cvt<snatchy_device<char, 3>>;
    using StrWrtCvt  = rb_root_cvt<str_write_device>;

    SnatchyCvt opened_reader(const char* data)
    {
        SnatchyCvt obj{snatchy_device<char, 3>{data}};
        obj.bos();
        obj.main_cont_beg();
        return obj;
    }

    StrWrtCvt opened_writer()
    {
        StrWrtCvt obj{str_write_device{}};
        obj.bos();
        obj.main_cont_beg();
        return obj;
    }
}

// A zero-length request has no meaning and a request larger than the root
// converter's own buffer can never be served, so both are rejected rather than
// clamped.
TEST(CvtIo, ReaderRejectsAZeroLengthRequest)
{
    auto              obj = opened_reader("hello");
    std::vector<char> buf;
    cvt_reader<SnatchyCvt> reader(obj, buf);
    reader.reset(10);
    EXPECT_THROW((void)reader.get_buf(0), cvt_error);
}

TEST(CvtIo, ReaderRejectsARequestLargerThanTheRootBuffer)
{
    auto              obj = opened_reader("hello");
    std::vector<char> buf;
    cvt_reader<SnatchyCvt> reader(obj, buf);
    reader.reset(10);
    EXPECT_THROW((void)reader.get_buf(SnatchyCvt::s_buffer_length + 1), cvt_error);
}

// The saturating overload promises the full length, so running out of stream
// before that is an error rather than a short read.
TEST(CvtIo, SaturatingReaderThrowsAtEndOfStream)
{
    auto              obj = opened_reader("ab"); // only two bytes available
    std::vector<char> buf;
    cvt_reader<SnatchyCvt> reader(obj, buf);
    reader.reset(10);
    EXPECT_THROW((void)reader.get_buf<true>(5), cvt_error);
}

// snatchy_device hands over at most three characters per read, so this also
// checks that a rollback is measured against what was served, not what was asked.
TEST(CvtIo, ReaderRollbackReExposesTheRolledBackCharacters)
{
    auto              obj = opened_reader("hello");
    std::vector<char> buf;
    cvt_reader<SnatchyCvt> reader(obj, buf);
    reader.reset(5);

    auto [ptr, len] = reader.get_buf(3);
    EXPECT_EQ(len, 3u);

    reader.rollback(2);
    auto [ptr2, len2] = reader.get_buf(3);
    EXPECT_EQ(len2, 3u);
    EXPECT_EQ(std::string(ptr2, len2), "ell");
}

TEST(CvtIo, ReaderRejectsAZeroLengthRollback)
{
    auto              obj = opened_reader("hello");
    std::vector<char> buf;
    cvt_reader<SnatchyCvt> reader(obj, buf);
    reader.reset(5);
    reader.get_buf(3);
    EXPECT_THROW(reader.rollback(0), cvt_error);
}

TEST(CvtIo, ReaderRejectsARollbackPastWhatWasServed)
{
    auto              obj = opened_reader("hello");
    std::vector<char> buf;
    cvt_reader<SnatchyCvt> reader(obj, buf);
    reader.reset(5);
    reader.get_buf(3);
    EXPECT_THROW(reader.rollback(100), cvt_error);
}

TEST(CvtIo, WriterRejectsAResetLargerThanTheRootBuffer)
{
    auto              obj = opened_writer();
    std::vector<char> buf;
    cvt_writer<StrWrtCvt> writer(obj, buf);
    EXPECT_THROW(writer.reset(StrWrtCvt::s_buffer_length + 1), cvt_error);
}

TEST(CvtIo, WriterRejectsAZeroLengthReservation)
{
    auto              obj = opened_writer();
    std::vector<char> buf;
    cvt_writer<StrWrtCvt> writer(obj, buf);
    writer.reset(10);
    EXPECT_THROW((void)writer.put_buf(0), cvt_error);
}

TEST(CvtIo, WriterRejectsAReservationLargerThanItsStagingBuffer)
{
    auto              obj = opened_writer();
    std::vector<char> buf;
    cvt_writer<StrWrtCvt> writer(obj, buf);
    writer.reset(5);
    EXPECT_THROW((void)writer.put_buf(6), cvt_error);
}

TEST(CvtIo, WriterRejectsAZeroLengthRollback)
{
    auto              obj = opened_writer();
    std::vector<char> buf;
    cvt_writer<StrWrtCvt> writer(obj, buf);
    writer.reset(10);
    auto ptr = writer.put_buf(5);
    std::copy_n("hello", 5, ptr);
    EXPECT_THROW(writer.rollback(0), cvt_error);
}

TEST(CvtIo, WriterRejectsARollbackPastWhatWasReserved)
{
    auto              obj = opened_writer();
    std::vector<char> buf;
    cvt_writer<StrWrtCvt> writer(obj, buf);
    writer.reset(10);
    auto ptr = writer.put_buf(5);
    std::copy_n("hello", 5, ptr);
    EXPECT_THROW(writer.rollback(100), cvt_error);
}

// put_buf_guard: on the exception path the whole reserved slot goes back, on the
// normal path only what used() did not account for, and a throwing rollback in
// the destructor must not escape. The two cvt_writer specializations that reserve
// outside their own staging buffer are the ones where a missing rollback leaks
// bytes, and each implements rollback differently, so both are covered.
//
// mem_device specialization: put_buf reserves inside the device itself and
// commit() is a no-op, so an un-rolled-back slot is visible as filler bytes.
TEST(CvtIo, PutBufGuardRollsBackTheUnusedTailOnAMemDevice)
{
    auto obj = no_rb_root_cvt{mem_device{""}};
    obj.bos();
    obj.main_cont_beg();
    {
        std::vector<char>         buf;
        cvt_writer<decltype(obj)> writer(obj, buf);
        writer.reset(1024);

        auto ptr = writer.put_buf(3);
        std::copy_n("abc", 3, ptr);

        // Nothing used: the exception path gives the whole slot back.
        auto abandon_a_reservation = [&]
        {
            auto          p = writer.put_buf(6);
            put_buf_guard guard{writer, static_cast<std::size_t>(6)};
            std::copy_n("XXXXXX", 6, p);
            throw cvt_error("PutBufGuard: forced throw");
        };
        EXPECT_THROW(abandon_a_reservation(), cvt_error);

        // Partially used: only the unused tail goes back.
        {
            auto          p = writer.put_buf(6);
            put_buf_guard guard{writer, static_cast<std::size_t>(6)};
            std::copy_n("de", 2, p);
            guard.used(2);
        }

        // Fully used: nothing goes back.
        {
            auto          p = writer.put_buf(2);
            put_buf_guard guard{writer, static_cast<std::size_t>(2)};
            std::copy_n("fg", 2, p);
            guard.used(2);
        }
        writer.commit();
    }
    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.str(), "abcdefg");
}

// non-mem-device specialization: rollback moves root_cvt's internal buffer
// cursor, so an un-rolled-back slot would reach the device on the next flush.
TEST(CvtIo, PutBufGuardRollsBackTheUnusedTailOnAPlainDevice)
{
    StrWrtCvt obj{str_write_device{}};
    obj.bos();
    obj.main_cont_beg();
    {
        std::vector<char>     buf;
        cvt_writer<StrWrtCvt> writer(obj, buf);
        writer.reset(64);

        auto ptr = writer.put_buf(3);
        std::copy_n("abc", 3, ptr);

        auto abandon_a_reservation = [&]
        {
            auto          p = writer.put_buf(6);
            put_buf_guard guard{writer, static_cast<std::size_t>(6)};
            std::copy_n("XXXXXX", 6, p);
            throw cvt_error("PutBufGuard: forced throw");
        };
        EXPECT_THROW(abandon_a_reservation(), cvt_error);

        {
            auto          p = writer.put_buf(6);
            put_buf_guard guard{writer, static_cast<std::size_t>(6)};
            std::copy_n("de", 2, p);
            guard.used(2);
        }
        writer.commit();
    }
    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.result, "abcde");
}

// A guard handed a length larger than what was reserved makes rollback throw from
// the destructor, where it has to be swallowed rather than terminate.
TEST(CvtIo, PutBufGuardSwallowsAThrowingRollback)
{
    auto obj = no_rb_root_cvt{mem_device{""}};
    obj.bos();
    obj.main_cont_beg();
    {
        std::vector<char>         buf;
        cvt_writer<decltype(obj)> writer(obj, buf);
        writer.reset(1024);
        auto ptr = writer.put_buf(2);
        std::copy_n("hi", 2, ptr);
        {
            put_buf_guard guard{writer, static_cast<std::size_t>(1000)};
        }
        writer.commit();
    }
    auto [dev, err] = obj.detach();
    EXPECT_EQ(dev.str(), "hi");
}

// The cases below drive the base cvt_reader<> / cvt_writer<> templates rather
// than the root_cvt specializations: vigenere_cvt is an io_converter but not a
// root_cvt, so a reader over it instantiates the generic template in abs_cvt.h.
namespace
{
    using VigKernelT = rb_root_cvt<mem_device<char>>;
    using VigCvtT    = Crypt::Classic::vigenere_cvt<VigKernelT>;

    VigCvtT opened_vigenere_reader(const char* data)
    {
        VigCvtT k{VigKernelT{mem_device<char>{data}}, "key"};
        k.bos();
        k.main_cont_beg();
        return k;
    }

    VigCvtT opened_vigenere_writer()
    {
        VigCvtT k{VigKernelT{mem_device<char>{""}}, "key"};
        k.bos();
        k.main_cont_beg();
        return k;
    }
}

TEST(CvtIo, BaseReaderRejectsAZeroLengthRequest)
{
    auto                 k = opened_vigenere_reader("hello world");
    std::vector<char>    buf;
    cvt_reader<VigCvtT>  reader(k, buf);
    reader.reset(10);
    EXPECT_THROW((void)reader.get_buf(0), cvt_error);
}

TEST(CvtIo, BaseReaderRejectsARequestLargerThanItsStagingBuffer)
{
    auto                 k = opened_vigenere_reader("hello world");
    std::vector<char>    buf;
    cvt_reader<VigCvtT>  reader(k, buf);
    reader.reset(5);
    EXPECT_THROW((void)reader.get_buf(6), cvt_error);
}

// After a rollback the buffered characters are enough to serve the next request
// on their own, so it must be answered without touching the kernel again.
TEST(CvtIo, BaseReaderServesASmallRequestOutOfWhatWasRolledBack)
{
    auto                 k = opened_vigenere_reader("hello world");
    std::vector<char>    buf;
    cvt_reader<VigCvtT>  reader(k, buf);
    reader.reset(10);

    auto [ptr, len] = reader.get_buf(5);
    EXPECT_EQ(len, 5u);
    reader.rollback(3);

    auto [ptr2, len2] = reader.get_buf(2);
    EXPECT_EQ(len2, 2u);
}

TEST(CvtIo, BaseSaturatingReaderServesASmallRequestOutOfWhatWasRolledBack)
{
    auto                 k = opened_vigenere_reader("hello world");
    std::vector<char>    buf;
    cvt_reader<VigCvtT>  reader(k, buf);
    reader.reset(10);
    reader.get_buf(5);
    reader.rollback(3);

    EXPECT_NE(reader.get_buf<true>(2), nullptr);
}

TEST(CvtIo, BaseReaderRejectsAZeroLengthRollback)
{
    auto                 k = opened_vigenere_reader("hello world");
    std::vector<char>    buf;
    cvt_reader<VigCvtT>  reader(k, buf);
    reader.reset(10);
    reader.get_buf(5);
    EXPECT_THROW(reader.rollback(0), cvt_error);
}

TEST(CvtIo, BaseReaderRejectsARollbackPastWhatWasServed)
{
    auto                 k = opened_vigenere_reader("hello world");
    std::vector<char>    buf;
    cvt_reader<VigCvtT>  reader(k, buf);
    reader.reset(10);
    reader.get_buf(5);
    EXPECT_THROW(reader.rollback(100), cvt_error);
}

TEST(CvtIo, BaseSaturatingReaderThrowsAtEndOfStream)
{
    auto                 k = opened_vigenere_reader("ab"); // only two bytes available
    std::vector<char>    buf;
    cvt_reader<VigCvtT>  reader(k, buf);
    reader.reset(10);
    EXPECT_THROW((void)reader.get_buf<true>(5), cvt_error);
}

TEST(CvtIo, BaseWriterRejectsAZeroLengthReservation)
{
    auto                 k = opened_vigenere_writer();
    std::vector<char>    buf;
    cvt_writer<VigCvtT>  writer(k, buf);
    writer.reset(10);
    EXPECT_THROW((void)writer.put_buf(0), cvt_error);
}

TEST(CvtIo, BaseWriterRejectsAReservationLargerThanItsStagingBuffer)
{
    auto                 k = opened_vigenere_writer();
    std::vector<char>    buf;
    cvt_writer<VigCvtT>  writer(k, buf);
    writer.reset(5);
    EXPECT_THROW((void)writer.put_buf(6), cvt_error);
}

TEST(CvtIo, BaseWriterRejectsAZeroLengthRollback)
{
    auto                 k = opened_vigenere_writer();
    std::vector<char>    buf;
    cvt_writer<VigCvtT>  writer(k, buf);
    writer.reset(10);
    writer.put_buf(5);
    EXPECT_THROW(writer.rollback(0), cvt_error);
}

TEST(CvtIo, BaseWriterRejectsARollbackPastWhatWasReserved)
{
    auto                 k = opened_vigenere_writer();
    std::vector<char>    buf;
    cvt_writer<VigCvtT>  writer(k, buf);
    writer.reset(10);
    writer.put_buf(5);
    EXPECT_THROW(writer.rollback(100), cvt_error);
}

TEST(CvtIo, RetrieveOnAFreshConverterSucceeds)
{
    VigCvtT    k{VigKernelT{mem_device<char>{"hello"}}, "key"};
    cvt_status s;
    EXPECT_NO_THROW(k.retrieve(s));
}

// bos() settles the direction once. A second call would have to undo a decision
// the converter has already acted on, so it is refused.
TEST(CvtIo, ASecondBosIsRejected)
{
    VigCvtT k{VigKernelT{mem_device<char>{"hello"}}, "key"};
    k.bos();
    EXPECT_THROW((void)k.bos(), cvt_error);
}

TEST(CvtIo, ASwitchableConverterTurnsAroundInBothDirections)
{
    VigCvtT k{VigKernelT{mem_device<char>{"hello"}}, "key"};
    EXPECT_EQ(k.bos(), io_status::input);
    k.main_cont_beg();
    EXPECT_NO_THROW(k.switch_to_put());
    EXPECT_NO_THROW(k.switch_to_get());
}

// A converter built with io_switch disabled cannot turn around, so a get() while
// it is writing -- and a put() while it is reading -- has to fail rather than
// silently do the wrong thing.
TEST(CvtIo, AConverterThatCannotSwitchRejectsGetWhileWriting)
{
    using NSKernel = rb_root_cvt<mem_device<char>>;
    no_switch_cvt<NSKernel> cvt{NSKernel{mem_device<char>{""}}}; // empty -> output
    cvt.bos();
    cvt.main_cont_beg();

    // Oversized on purpose: the request is still for one character, but with a
    // one-byte destination the optimizer inlines get_main() far enough to warn
    // about a copy this test never reaches.
    char buf[8] = {};
    EXPECT_THROW((void)cvt.get(buf, 1), cvt_error);
}

TEST(CvtIo, AConverterThatCannotSwitchRejectsPutWhileReading)
{
    using NSKernel = rb_root_cvt<mem_device<char>>;
    no_switch_cvt<NSKernel> cvt{NSKernel{mem_device<char>{"hello"}}}; // non-empty -> input
    cvt.bos();
    cvt.main_cont_beg();

    const char src[1] = {'x'};
    EXPECT_THROW(cvt.put(src, 1), cvt_error);
}

// The BOS phase reads and writes in whole external units. wext_char_cvt has a
// one-byte internal type over a four-byte external one, so a request for three
// characters ends part-way through a unit -- the partial-unit path in
// abs_cvt::get / abs_cvt::put.
TEST(CvtIo, GetDuringBosHandlesAPartialExternalUnit)
{
    using WKernelT = rb_root_cvt<mem_device<wchar_t>>;
    wext_char_cvt<WKernelT> cvt{WKernelT{mem_device<wchar_t>{L"\x01020304"}}};
    EXPECT_EQ(cvt.bos(), io_status::input);

    char buf[4] = {};
    EXPECT_EQ(cvt.get(buf, 3), 3u);
}

TEST(CvtIo, PutDuringBosHandlesAPartialExternalUnit)
{
    using WKernelT = rb_root_cvt<mem_device<wchar_t>>;
    wext_char_cvt<WKernelT> cvt{WKernelT{mem_device<wchar_t>{L""}}};
    EXPECT_EQ(cvt.bos(), io_status::output);

    const char src[3] = {'a', 'b', 'c'};
    EXPECT_NO_THROW(cvt.put(src, 3));
}

// A converter whose put_main throws is left tainted: the stream is in an unknown
// state, so every later operation has to refuse rather than guess.
TEST(CvtIo, AThrowingPutTaintsTheConverter)
{
    using KernelT = rb_root_cvt<mem_device<char>>;
    failing_put_cvt<KernelT> cvt{KernelT{mem_device<char>{""}}}; // empty -> output
    cvt.bos();
    cvt.main_cont_beg();

    const char data[] = {'x'};
    EXPECT_THROW(cvt.put(data, 1), cvt_error);

    char buf[1];
    EXPECT_THROW((void)cvt.get(buf, 1), cvt_error);
}

// bos() has two guards: the direction is already settled, or the BOS phase is
// already over. The second one is unreachable through the public API alone, so
// the converter here exposes the protected member needed to reach it.
TEST(CvtIo, BosIsRejectedOnceTheBosPhaseIsOver)
{
    using KernelT = rb_root_cvt<mem_device<char>>;
    bos_guard_hack_cvt<KernelT> cvt{KernelT{mem_device<char>{""}}};
    cvt.bos();           // m_io_status = output (empty device)
    cvt.main_cont_beg(); // m_is_bos_done = true

    cvt.force_neutral_status();
    EXPECT_THROW((void)cvt.bos(), cvt_error);
}
