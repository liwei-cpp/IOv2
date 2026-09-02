// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <common/defs.h>
#include <device/device_concepts.h>
#include <device/file_device.h>

#include <support/file_guard.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

using namespace IOv2;

namespace
{
    static_assert(io_device<ifile_device<char8_t>>);
    static_assert(io_device<ofile_device<char8_t>>);
    static_assert(io_device<file_device<char8_t>>);

    static_assert(std::is_same_v<ifile_device<char8_t>::char_type, char8_t>);
    static_assert(std::is_same_v<ofile_device<char8_t>::char_type, char8_t>);
    static_assert(std::is_same_v<file_device<char8_t>::char_type, char8_t>);

    // All three seek; which direction each supports follows its template
    // arguments, and only those.
    static_assert(dev_cpt::support_positioning<ifile_device<char8_t>>);
    static_assert(dev_cpt::support_get<ifile_device<char8_t>>);
    static_assert(!dev_cpt::support_put<ifile_device<char8_t>>);

    static_assert(dev_cpt::support_positioning<ofile_device<char8_t>>);
    static_assert(!dev_cpt::support_get<ofile_device<char8_t>>);
    static_assert(dev_cpt::support_put<ofile_device<char8_t>>);

    static_assert(dev_cpt::support_positioning<file_device<char8_t>>);
    static_assert(dev_cpt::support_get<file_device<char8_t>>);
    static_assert(dev_cpt::support_put<file_device<char8_t>>);

    // Byte n of this string is n rendered in base 36, so a seek to n and the
    // character read there check each other without a lookup table.
    const std::u8string data = u8"0123456789abcdefghijklmnopqrstuvwxyz";
    constexpr std::size_t data_len = 36;

    char8_t at(std::size_t n) { return data[n]; }
}

TEST(FileDeviceChar8, Traits)
{
    // Every assertion above is a static_assert; compiling is the check.
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Opening and closing.
// ---------------------------------------------------------------------------

TEST(FileDeviceChar8, ADefaultConstructedDeviceIsNotOpen)
{
    ifile_device<char8_t> in;
    ofile_device<char8_t> out;
    file_device<char8_t>  io;

    EXPECT_FALSE(in.is_open());
    EXPECT_FALSE(out.is_open());
    EXPECT_FALSE(io.is_open());
}

TEST(FileDeviceChar8, EachModeOpensAndCloses)
{
    const char* readable = "fd_char8_open_in.tst";
    const char* writable = "fd_char8_open_out.tst";
    const char* both     = "fd_char8_open_io.tst";

    file_guard g1(readable, data);
    file_guard g2(writable, data);
    file_guard g3(both, data);

    ifile_device<char8_t> in(readable);
    ofile_device<char8_t> out(writable, file_open_flag::trunc);
    file_device<char8_t>  io(both, file_open_flag::trunc);

    EXPECT_TRUE(in.is_open());
    EXPECT_TRUE(out.is_open());
    EXPECT_TRUE(io.is_open());

    in.close();
    out.close();
    io.close();

    EXPECT_FALSE(in.is_open());
    EXPECT_FALSE(out.is_open());
    EXPECT_FALSE(io.is_open());
}

TEST(FileDeviceChar8, ClosingTwiceIsHarmless)
{
    const char* name = "fd_char8_close_twice.tst";
    file_guard g(name, data);

    file_device<char8_t> dev(name);
    ASSERT_TRUE(dev.is_open());

    dev.close();
    EXPECT_FALSE(dev.is_open());

    // close() is guarded by is_open(), so a second call does nothing at all.
    dev.close();
    EXPECT_FALSE(dev.is_open());
}

TEST(FileDeviceChar8, ReadingAfterCloseThrows)
{
    const char* name = "fd_char8_read_after_close.tst";
    file_guard g(name, data);

    ifile_device<char8_t> dev(name);
    char8_t ch = 0;
    char8_t buf[8] = {};
    ASSERT_EQ(dev.dget(&ch, 1), 1u);

    dev.close();
    EXPECT_ANY_THROW(dev.dget(&ch, 1));
    EXPECT_ANY_THROW(dev.dget(buf, sizeof(buf)));
}

TEST(FileDeviceChar8, WritingAfterCloseThrows)
{
    const char* name = "fd_char8_write_after_close.tst";
    file_guard g(name);

    ofile_device<char8_t> dev(name, file_open_flag::trunc);
    dev.dput(u8"T", 1);

    dev.close();
    EXPECT_ANY_THROW(dev.dput(u8"T", 1));
    EXPECT_ANY_THROW(dev.dput(data.data(), data.size()));
}

TEST(FileDeviceChar8, AFileWrittenAndClosedCanBeOpenedForReading)
{
    const char* name = "fd_char8_reopen.tst";
    file_guard g(name);

    ofile_device<char8_t> writer(name, file_open_flag::trunc);
    writer.close();

    ifile_device<char8_t> reader(name);
    EXPECT_TRUE(reader.is_open());
}

// ---------------------------------------------------------------------------
// Reading.
// ---------------------------------------------------------------------------

TEST(FileDeviceChar8, GetReadsBytesInOrderAndAdvancesTheposition)
{
    const char* name = "fd_char8_get_order.tst";
    file_guard g(name, data);

    ifile_device<char8_t> dev(name);
    EXPECT_FALSE(dev.deof());

    char8_t ch = 0;
    for (std::size_t i = 0; i < 6; ++i)
    {
        EXPECT_EQ(dev.dget(&ch, 1), 1u) << "at byte " << i;
        EXPECT_EQ(ch, at(i)) << "at byte " << i;
    }
    EXPECT_EQ(dev.dtell(), 6u);
}

TEST(FileDeviceChar8, GetOnAnEmptyFileReturnsNothing)
{
    const char* name = "fd_char8_get_empty.tst";
    file_guard g(name);

    file_device<char8_t> dev(name, file_open_flag::trunc);
    EXPECT_EQ(dev.dtell(), 0u);

    char8_t ch = 0;
    EXPECT_EQ(dev.dget(&ch, 1), 0u);
    EXPECT_EQ(dev.dtell(), 0u);
}

TEST(FileDeviceChar8, AReadWriteDeviceReadsFromTheFront)
{
    const char* name = "fd_char8_get_io.tst";
    file_guard g(name, data);

    file_device<char8_t> dev(name);
    EXPECT_EQ(dev.dtell(), 0u);

    char8_t ch = 0;
    for (std::size_t i = 0; i < 6; ++i)
    {
        EXPECT_EQ(dev.dget(&ch, 1), 1u) << "at byte " << i;
        EXPECT_EQ(ch, at(i)) << "at byte " << i;
    }
    EXPECT_EQ(dev.dtell(), 6u);
}

TEST(FileDeviceChar8, ReadBackWhatWasJustWritten)
{
    const char* name = "fd_char8_write_read_back.tst";
    file_guard g(name);

    file_device<char8_t> dev(name, file_open_flag::trunc);
    const std::u8string payload = u8"red fox\n";
    dev.dput(payload.data(), payload.size());
    ASSERT_EQ(dev.dsize(), payload.size());

    dev.dseek(0);
    char8_t first[3] = {};
    EXPECT_EQ(dev.dget(first, sizeof(first)), sizeof(first));
    EXPECT_EQ(std::u8string(first, sizeof(first)), payload.substr(0, sizeof(first)));
    EXPECT_EQ(dev.dtell(), sizeof(first));
}

// ---------------------------------------------------------------------------
// Positioning.  dseek() takes an absolute offset and drseek() counts back from
// the end, so drseek(0) is one past the last byte and drseek(1) is the last one.
// A relative move is dseek() applied to dtell().
// ---------------------------------------------------------------------------

TEST(FileDeviceChar8, SeekFormsOnAReadOnlyDevice)
{
    const char* name = "fd_char8_seek_in.tst";
    file_guard g(name, data);

    ifile_device<char8_t> dev(name);
    char8_t ch = 0;

    // Absolute.
    dev.dseek(2);
    EXPECT_EQ(dev.dtell(), 2u);
    EXPECT_EQ(dev.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, at(2));
    EXPECT_EQ(dev.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, at(3));

    dev.dseek(4);
    EXPECT_EQ(dev.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, at(4));

    // Relative, expressed through dtell().
    dev.dseek(dev.dtell() + 2);
    EXPECT_EQ(dev.dtell(), 7u);
    EXPECT_EQ(dev.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, at(7));

    // Seeking to where it already is changes nothing.
    dev.dseek(dev.dtell());
    EXPECT_EQ(dev.dtell(), 8u);
    EXPECT_EQ(dev.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, at(8));

    // From the end.
    dev.drseek(0);
    EXPECT_TRUE(dev.deof());
    EXPECT_EQ(dev.dget(&ch, 1), 0u);

    dev.drseek(1);
    EXPECT_EQ(dev.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, at(data_len - 1));
}

TEST(FileDeviceChar8, SeekFormsOnAReadWriteDevice)
{
    const char* name = "fd_char8_seek_io.tst";
    file_guard g(name, data);

    file_device<char8_t> dev(name);
    EXPECT_EQ(dev.dtell(), 0u);

    char8_t ch = 0;

    // Absolute: seek, read, then overwrite the byte just read and read it back.
    dev.dseek(3);
    EXPECT_EQ(dev.dtell(), 3u);
    EXPECT_EQ(dev.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, at(3));

    dev.dseek(3);
    dev.dput(u8"\n", 1);
    dev.dseek(3);
    EXPECT_EQ(dev.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, u8'\n');

    // Relative.
    dev.dseek(dev.dtell() + 2);
    EXPECT_EQ(dev.dtell(), 6u);
    EXPECT_EQ(dev.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, at(6));

    // From the end: appending moves the end, so drseek(1) then reads the last
    // byte written rather than the last byte of the original file.
    dev.drseek(0);
    dev.dput(u8"tail.", 5);
    dev.drseek(1);
    EXPECT_EQ(dev.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, u8'.');
}

TEST(FileDeviceChar8, SeekFormsOnAWriteOnlyDevice)
{
    const char* name = "fd_char8_seek_out.tst";
    file_guard g(name, data);

    // A write-only open is fopen "w", which truncates whether or not trunc was
    // asked for: the bytes the guard wrote are gone before the first seek.
    // Seeking past the end is still legal -- the gap becomes a hole.
    ofile_device<char8_t> dev(name);
    EXPECT_EQ(dev.dtell(), 0u);
    EXPECT_EQ(dev.dsize(), 0u);

    dev.dseek(2);
    EXPECT_EQ(dev.dtell(), 2u);

    dev.dseek(dev.dtell() + 2);
    EXPECT_EQ(dev.dtell(), 4u);

    dev.dseek(dev.dtell());
    dev.dput(u8"x", 1);
    EXPECT_EQ(dev.dtell(), 5u);

    // drseek(0) is the end of what has been written, which is 5, not 36.
    dev.drseek(0);
    dev.dput(u8"tail.", 5);
    EXPECT_EQ(dev.dtell(), 10u);
}

TEST(FileDeviceChar8, SeekingAnUnopenedDeviceThrows)
{
    // Every mode combination: the position lives in the file handle, and a
    // default-constructed device has none.
    {
        ifile_device<char8_t> dev;
        EXPECT_ANY_THROW(dev.dseek(0));
        EXPECT_ANY_THROW(dev.dseek(0));
    }
    {
        ofile_device<char8_t> dev;
        EXPECT_ANY_THROW(dev.dseek(0));
        EXPECT_ANY_THROW(dev.dseek(0));
    }
    {
        file_device<char8_t> dev;
        EXPECT_ANY_THROW(dev.dseek(0));
        EXPECT_ANY_THROW(dev.dseek(0));
    }
}

TEST(FileDeviceChar8, SeekingToTheStartOfAnOpenFileAlwaysWorks)
{
    const char* name = "fd_char8_seek_zero.tst";

    {
        file_guard g(name, data);
        ifile_device<char8_t> dev(name);
        dev.dseek(0);
        dev.dseek(0);
        EXPECT_EQ(dev.dtell(), 0u);
    }
    {
        file_guard g(name, data);
        file_device<char8_t> dev(name);
        dev.dseek(0);
        dev.dseek(0);
        EXPECT_EQ(dev.dtell(), 0u);
    }
    {
        file_guard g(name, data);
        ofile_device<char8_t> dev(name);
        dev.dseek(0);
        dev.dseek(0);
        EXPECT_EQ(dev.dtell(), 0u);
    }
}

TEST(FileDeviceChar8, ReadAndWriteAlternateWithoutASeekBetweenThem)
{
    const char* name = "fd_char8_alternate.tst";
    file_guard g(name);

    file_device<char8_t> dev(name, file_open_flag::trunc);
    char8_t buf[12] = {};

    // An empty file is already at its end.
    EXPECT_TRUE(dev.deof());
    dev.dput(u8"1234", 4);
    EXPECT_TRUE(dev.deof());

    dev.dseek(0);
    EXPECT_FALSE(dev.deof());
    EXPECT_EQ(dev.dget(buf, 3), 3u);
    EXPECT_EQ(std::memcmp(buf, u8"123", 3), 0);
    EXPECT_FALSE(dev.deof());

    // Read then write, with nothing in between: the write continues from where
    // the read stopped, overwriting byte 3 and appending byte 4.
    dev.dput(u8"AB", 2);
    EXPECT_TRUE(dev.deof());
    dev.dseek(0);
    EXPECT_EQ(dev.dget(buf, 5), 5u);
    EXPECT_EQ(std::memcmp(buf, u8"123AB", 5), 0);
    EXPECT_TRUE(dev.deof());

    // Write then read, again with nothing in between.
    dev.dseek(0);
    dev.dput(u8"XY", 2);
    EXPECT_FALSE(dev.deof());
    EXPECT_EQ(dev.dget(buf, 3), 3u);
    EXPECT_EQ(std::memcmp(buf, u8"3AB", 3), 0);
    EXPECT_TRUE(dev.deof());

    dev.dput(u8"CDEF", 4);
    EXPECT_TRUE(dev.deof());

    dev.dseek(0);
    EXPECT_EQ(dev.dget(buf, 2), 2u);
    EXPECT_EQ(std::memcmp(buf, u8"XY", 2), 0);
    EXPECT_FALSE(dev.deof());

    dev.drseek(0);
    EXPECT_TRUE(dev.deof());
    dev.dput(u8"GHI", 3);
    EXPECT_TRUE(dev.deof());

    dev.dseek(0);
    EXPECT_FALSE(dev.deof());
    EXPECT_EQ(dev.dget(buf, 12), 12u);
    EXPECT_EQ(std::memcmp(buf, u8"XY3ABCDEFGHI", 12), 0);
    EXPECT_TRUE(dev.deof());
}

// ---------------------------------------------------------------------------
// Failing to open.
// ---------------------------------------------------------------------------

TEST(FileDeviceChar8, OpeningAMissingFileForReadingThrows)
{
    EXPECT_THROW((void)ifile_device<char8_t>("non_existent_file.txt"), device_error);
    EXPECT_THROW((void)file_device<char8_t>("non_existent_file.txt"), device_error);
}

TEST(FileDeviceChar8, TryOpenReportsTheFailureInsteadOfThrowing)
{
    auto res = file_device<char8_t>::try_open("non_existent_file.txt");
    EXPECT_FALSE(res.has_value());
    EXPECT_FALSE(res.error().empty());
}

TEST(FileDeviceChar8, NoreplaceRefusesAnExistingFile)
{
    file_guard g("existing_file_u8.txt", std::u8string(u8"content"));

    EXPECT_THROW((void)ofile_device<char8_t>("existing_file_u8.txt", file_open_flag::noreplace),
                 device_error);

    auto res = ofile_device<char8_t>::try_open("existing_file_u8.txt", file_open_flag::noreplace);
    EXPECT_FALSE(res.has_value());
}

TEST(FileDeviceChar8, TryOpenReportsARejectedFlagCombination)
{
    const char* name = "fd_char8_try_open_flags.tst";
    file_guard g(name, data);

    // trunc is meaningless for a read-only device, and the constructor says so;
    // try_open turns that into an error value.
    auto res = ifile_device<char8_t>::try_open(name, file_open_flag::trunc);
    EXPECT_FALSE(res.has_value());
    EXPECT_FALSE(res.error().empty());
}

// ---------------------------------------------------------------------------
// Move semantics.  The device owns a FILE*, so it is move-only.
// ---------------------------------------------------------------------------

TEST(FileDeviceChar8, MoveTransfersTheOpenFile)
{
    const char* name = "fd_char8_move.tst";
    file_guard g(name, std::u8string(u8"move content"));

    file_device<char8_t> dev1(name);
    ASSERT_TRUE(dev1.is_open());
    const std::size_t len = dev1.dsize();

    file_device<char8_t> dev2(std::move(dev1));
    EXPECT_TRUE(dev2.is_open());
    EXPECT_EQ(dev2.dsize(), len);
    EXPECT_FALSE(dev1.is_open());

    file_device<char8_t> dev3;
    dev3 = std::move(dev2);
    EXPECT_TRUE(dev3.is_open());
    EXPECT_EQ(dev3.dsize(), len);
    EXPECT_FALSE(dev2.is_open());

    // Outside any macro: the diagnostic fires at the assignment itself.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    dev3 = std::move(dev3);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    EXPECT_TRUE(dev3.is_open());
    EXPECT_EQ(dev3.dsize(), len);
}

TEST(FileDeviceChar8, MoveAssignmentWorksForTheSingleDirectionDevices)
{
    // The read-only and write-only instantiations have their own operator=,
    // and only the read-write one is covered by the case above.
    const char* in_name  = "fd_char8_move_in.tst";
    const char* out_name = "fd_char8_move_out.tst";
    file_guard g1(in_name, data);
    file_guard g2(out_name);

    ifile_device<char8_t> reader(in_name);
    ifile_device<char8_t> reader_dst;
    reader_dst = std::move(reader);
    EXPECT_TRUE(reader_dst.is_open());
    EXPECT_FALSE(reader.is_open());
    EXPECT_EQ(reader_dst.dsize(), data_len);

    ofile_device<char8_t> writer(out_name, file_open_flag::trunc);
    ofile_device<char8_t> writer_dst;
    writer_dst = std::move(writer);
    EXPECT_TRUE(writer_dst.is_open());
    EXPECT_FALSE(writer.is_open());
    writer_dst.dput(u8"ok", 2);
}

// ---------------------------------------------------------------------------
// Error paths.
// ---------------------------------------------------------------------------

TEST(FileDeviceChar8, SeekingPastTheEndOfAReadOnlyFileThrows)
{
    file_guard g("seek_err_u8.txt", std::u8string(u8"123"));
    ifile_device<char8_t> dev("seek_err_u8.txt");
    EXPECT_THROW(dev.dseek(10), device_error);
}

TEST(FileDeviceChar8, PositionQueriesOnAClosedDeviceThrow)
{
    ifile_device<char8_t> dev;

    EXPECT_THROW((void)dev.dtell(), device_error);
    EXPECT_THROW((void)dev.dsize(), device_error);
    EXPECT_THROW(dev.dseek(0), device_error);
    EXPECT_THROW(dev.drseek(0), device_error);

    // deof() is the exception: it answers "yes" for a closed device rather than
    // throwing, so a read loop over one terminates instead of blowing up.
    EXPECT_NO_THROW((void)dev.deof());
    EXPECT_TRUE(dev.deof());
}

TEST(FileDeviceChar8, PutOnAClosedDeviceOrANullBufferThrows)
{
    {
        ofile_device<char8_t> dev;
        EXPECT_THROW(dev.dput(u8"a", 1), device_error);
    }
    {
        file_guard g("put_err_u8.txt");
        ofile_device<char8_t> dev("put_err_u8.txt");
        EXPECT_THROW(dev.dput(nullptr, 1), device_error);
    }
}

TEST(FileDeviceChar8, TruncIsRejectedForAReadOnlyDevice)
{
    EXPECT_THROW((void)ifile_device<char8_t>("test_u8.txt", file_open_flag::trunc), device_error);
}

TEST(FileDeviceChar8, FlushOnAClosedDeviceIsANoOp)
{
    ofile_device<char8_t> dev;
    EXPECT_NO_THROW(dev.dflush());
}

TEST(FileDeviceChar8, DrseekPastTheStartThrows)
{
    file_guard g("drseek_err_u8.txt", std::u8string(u8"hello"));
    ifile_device<char8_t> dev("drseek_err_u8.txt");
    EXPECT_THROW(dev.drseek(1000), device_error);
}

TEST(FileDeviceChar8, DseekRejectsAnOffsetThatWouldOverflowTheFileOffsetType)
{
    file_guard g("dseek_overflow_u8.txt", std::u8string(u8"hi"));
    ofile_device<char8_t> dev("dseek_overflow_u8.txt");
    EXPECT_THROW(dev.dseek(std::numeric_limits<std::size_t>::max()), device_error);
}

// ---------------------------------------------------------------------------
// Open flags.  Each combination selects a different fopen() mode string, and
// the table that does it is the point of these cases.
// ---------------------------------------------------------------------------

TEST(FileDeviceChar8, WriteOnlyOpenFlagCombinations)
{
    const char* name = "modes_test_u8.txt";

    {
        file_guard g(name);
        ofile_device<char8_t> dev(name, file_open_flag::noreplace);
        EXPECT_TRUE(dev.is_open());
    }
    {
        file_guard g(name);
        ofile_device<char8_t> dev(name, file_open_flag::binary);
        EXPECT_TRUE(dev.is_open());
    }
    {
        file_guard g(name);
        ofile_device<char8_t> dev(name, file_open_flag::trunc | file_open_flag::binary);
        EXPECT_TRUE(dev.is_open());
    }
}

TEST(FileDeviceChar8, ReadWriteOpenFlagCombinations)
{
    const char* name = "modes_test_u8.txt";

    {
        file_guard g(name);
        file_device<char8_t> dev(name, file_open_flag::noreplace);
        EXPECT_TRUE(dev.is_open());
    }
    {
        file_guard g(name);
        file_device<char8_t> dev(name, file_open_flag::trunc | file_open_flag::noreplace);
        EXPECT_TRUE(dev.is_open());
    }
}

TEST(FileDeviceChar8, ReadOnlyBinaryOpen)
{
    const char* name = "modes_test_u8.txt";
    file_guard g(name, std::u8string(u8"existing"));

    ifile_device<char8_t> dev(name, file_open_flag::binary);
    EXPECT_TRUE(dev.is_open());
}
