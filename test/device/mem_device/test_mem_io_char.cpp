// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/device/device_concepts.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

using namespace IOv2;

namespace
{
    using dev = mem_device<char>;

    static_assert(io_device<dev>);
    static_assert(std::is_same_v<dev::char_type, char>);
    static_assert(dev_cpt::support_positioning<dev>);
    static_assert(dev_cpt::support_put<dev>);
    static_assert(dev_cpt::support_get<dev>);
}

TEST(MemDeviceChar, Traits)
{
    // Every assertion above is a static_assert; compiling is the check.
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Construction and assignment.
// ---------------------------------------------------------------------------

TEST(MemDeviceChar, DefaultConstructionIsEmpty)
{
    dev obj;
    EXPECT_TRUE(obj.str().empty());
    EXPECT_EQ(obj.dtell(), 0u);
    EXPECT_EQ(obj.dsize(), 0u);
}

TEST(MemDeviceChar, ConstructionFromStringKeepsTheContents)
{
    const std::string text = "buffered characters";

    dev obj(text);
    EXPECT_EQ(obj.str(), text);

    // Both cursors start at the front, so the first read yields the first
    // character and the following write lands on the second.
    EXPECT_EQ(obj.dtell(), 0u);
    char ch = 0;
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, text[0]);

    obj.dput("Y", 1);
    EXPECT_EQ(obj.str(), "bYffered characters");
    EXPECT_EQ(obj.dtell(), 2u);
}

TEST(MemDeviceChar, ConstructionFromCharPointer)
{
    dev obj("Hello world");
    EXPECT_EQ(obj.str(), "Hello world");
    EXPECT_EQ(obj.dtell(), 0u);
}

TEST(MemDeviceChar, MoveAssignmentFromTemporaryResetsThePosition)
{
    dev obj;
    EXPECT_EQ(obj.str(), "");
    EXPECT_EQ(obj.dtell(), 0u);

    obj = dev{"Hello world"};
    EXPECT_EQ(obj.str(), "Hello world");
    EXPECT_EQ(obj.dtell(), 0u);
}

TEST(MemDeviceChar, ConstructionCopiesTheSourceString)
{
    std::string ref = "Hello world";

    dev obj(ref);
    EXPECT_EQ(obj.str(), ref);
    EXPECT_EQ(obj.dtell(), 0u);

    // The device holds a copy, so growing `ref` does not reach it until it is
    // rebuilt from the longer string.
    ref += "123";
    EXPECT_NE(obj.str(), ref);
    obj = dev{ref};
    EXPECT_EQ(obj.str(), ref);
    EXPECT_EQ(obj.dtell(), 0u);
}

// ---------------------------------------------------------------------------
// Positioning.  drseek() counts back from the end, so drseek(0) is the end of
// the buffer and drseek(size()) is the front; anything past size() is out of
// range and must leave the position where it was.
// ---------------------------------------------------------------------------

TEST(MemDeviceChar, DrseekCountsBackFromTheEnd)
{
    dev obj("12345");

    obj.drseek(0);
    EXPECT_EQ(obj.dtell(), 5u);
    EXPECT_TRUE(obj.deof());

    obj.drseek(5);
    EXPECT_EQ(obj.dtell(), 0u);
    EXPECT_FALSE(obj.deof());

    obj.drseek(3);
    EXPECT_EQ(obj.dtell(), 2u);
}

TEST(MemDeviceChar, DrseekPastTheFrontLeavesThePositionAlone)
{
    dev obj("12345");
    obj.drseek(3);
    ASSERT_EQ(obj.dtell(), 2u);

    // One past the front, then the two values that would underflow a subtraction
    // of the offset from the size rather than compare against it first.
    EXPECT_ANY_THROW(obj.drseek(6));
    EXPECT_EQ(obj.dtell(), 2u);

    EXPECT_ANY_THROW(obj.drseek(std::numeric_limits<std::size_t>::max()));
    EXPECT_EQ(obj.dtell(), 2u);

    EXPECT_ANY_THROW(obj.drseek(static_cast<std::size_t>(-1)));
    EXPECT_EQ(obj.dtell(), 2u);
}

// ---------------------------------------------------------------------------
// Input.
// ---------------------------------------------------------------------------

TEST(MemDeviceChar, GetOneCharacterAtATime)
{
    dev obj("12");
    EXPECT_FALSE(obj.deof());

    char ch = 0;
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, '1');
    EXPECT_EQ(obj.dtell(), 1u);
    EXPECT_FALSE(obj.deof());

    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, '2');
    EXPECT_EQ(obj.dtell(), 2u);
    EXPECT_TRUE(obj.deof());

    // Reading at the end yields nothing and moves nothing.
    EXPECT_EQ(obj.dget(&ch, 1), 0u);
    EXPECT_EQ(obj.dtell(), 2u);
    EXPECT_TRUE(obj.deof());
}

TEST(MemDeviceChar, GetFillsTheWholeRequest)
{
    dev obj("12345");
    char buf[5] = {};

    EXPECT_EQ(obj.dget(buf, 5), 5u);
    EXPECT_EQ(std::string(buf, 5), "12345");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceChar, SuccessiveGetsResumeWhereTheLastStopped)
{
    dev obj("12345");
    char buf[5] = {};

    EXPECT_EQ(obj.dget(buf, 3), 3u);
    EXPECT_EQ(std::string(buf, 3), "123");
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_EQ(obj.dget(buf, 2), 2u);
    EXPECT_EQ(std::string(buf, 2), "45");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceChar, GetPastTheEndReturnsWhatIsLeft)
{
    dev obj("12345");
    char buf[10] = {};

    EXPECT_EQ(obj.dget(buf, 10), 5u);
    EXPECT_EQ(std::string(buf, 5), "12345");
    EXPECT_EQ(obj.dtell(), 5u);

    EXPECT_EQ(obj.dget(buf, 10), 0u);
}

TEST(MemDeviceChar, ShortGetAfterAFullOneStopsAtTheEnd)
{
    dev obj("12345");
    char buf[5] = {};

    EXPECT_EQ(obj.dget(buf, 3), 3u);
    EXPECT_EQ(std::string(buf, 3), "123");
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_EQ(obj.dget(buf, 5), 2u);
    EXPECT_EQ(std::string(buf, 2), "45");
    EXPECT_EQ(obj.dtell(), 5u);

    EXPECT_EQ(obj.dget(buf, 10), 0u);
}

TEST(MemDeviceChar, GetAfterSeek)
{
    dev obj("12345");
    char buf[5] = {};

    obj.dseek(3);
    EXPECT_EQ(obj.dget(buf, 5), 2u);
    EXPECT_EQ(std::string(buf, 2), "45");

    obj.dseek(obj.dtell() - 1);
    EXPECT_EQ(obj.dget(buf, 5), 1u);
    EXPECT_EQ(buf[0], '5');
}

TEST(MemDeviceChar, SeekAndDrseekAddressTheSameCharacter)
{
    dev obj("12345");
    char ch = 0;

    obj.dseek(2);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, '3');

    obj.dseek(1);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, '2');

    obj.dseek(obj.dtell() + 2);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, '5');

    // Three back from the end is the same place as two from the front.
    obj.drseek(3);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, '3');
}

TEST(MemDeviceChar, FailedSeeksLeaveTheReadPositionAlone)
{
    dev obj("12345");

    EXPECT_ANY_THROW(obj.dseek(100));
    EXPECT_EQ(obj.dtell(), 0u);

    obj.dseek(3);

    EXPECT_ANY_THROW(obj.dseek(100));
    EXPECT_EQ(obj.dtell(), 3u);

    // Negative offsets reach dseek as huge size_t values; they must be rejected
    // rather than wrap into a valid position.
    EXPECT_ANY_THROW(obj.dseek(-100));
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_ANY_THROW(obj.dseek(obj.dtell() + 100));
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_ANY_THROW(obj.dseek(obj.dtell() - 100));
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_ANY_THROW(obj.drseek(-100));
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_ANY_THROW(obj.drseek(100));
    EXPECT_EQ(obj.dtell(), 3u);
}

// ---------------------------------------------------------------------------
// Output.  There is one cursor for both directions: dput() overwrites from the
// current position and only grows the buffer once it reaches the end.
// ---------------------------------------------------------------------------

TEST(MemDeviceChar, PutAtTheEndAppends)
{
    dev obj("12");
    obj.drseek(0);
    EXPECT_TRUE(obj.deof());

    obj.dput("x", 1);
    EXPECT_TRUE(obj.deof());
    EXPECT_EQ(obj.str(), "12x");
    EXPECT_EQ(obj.dtell(), 3u);
}

TEST(MemDeviceChar, PutInsideTheBufferOverwrites)
{
    dev obj("12");
    EXPECT_FALSE(obj.deof());

    obj.dput("x", 1);
    EXPECT_FALSE(obj.deof());
    EXPECT_EQ(obj.str(), "x2");
    EXPECT_EQ(obj.dtell(), 1u);
}

TEST(MemDeviceChar, PutIntoAnEmptyBuffer)
{
    dev obj("");
    EXPECT_TRUE(obj.deof());

    obj.dput("y", 1);
    EXPECT_TRUE(obj.deof());
    EXPECT_EQ(obj.str(), "y");
    EXPECT_EQ(obj.dtell(), 1u);
}

TEST(MemDeviceChar, PutGrowsAnEmptyDevice)
{
    dev obj;
    obj.dput("12345", 5);
    EXPECT_EQ(obj.str(), "12345");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceChar, ZeroLengthTransfersAcceptANullBuffer)
{
    dev obj;

    // Nothing is dereferenced when the count is zero, so a null pointer is not
    // an error; it becomes one as soon as a character is actually asked for.
    obj.dput(nullptr, 0);
    EXPECT_EQ(obj.str(), "");
    EXPECT_EQ(obj.dtell(), 0u);

    EXPECT_EQ(obj.dget(nullptr, 0), 0u);
    EXPECT_THROW(obj.dget(nullptr, 1), device_error);
}

TEST(MemDeviceChar, SuccessivePutsAppendInOrder)
{
    dev obj;

    obj.dput("123", 3);
    EXPECT_EQ(obj.str(), "123");
    EXPECT_EQ(obj.dtell(), 3u);

    obj.dput("45", 2);
    EXPECT_EQ(obj.str(), "12345");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceChar, SinglePutBetweenLongerOnes)
{
    dev obj;

    obj.dput("123", 3);
    EXPECT_EQ(obj.str(), "123");

    obj.dput("x", 1);
    EXPECT_EQ(obj.str(), "123x");

    obj.dput("45", 2);
    EXPECT_EQ(obj.str(), "123x45");
    EXPECT_EQ(obj.dtell(), 6u);
}

TEST(MemDeviceChar, PutAfterSeekOverwritesThenExtends)
{
    dev obj("12345");

    obj.dseek(3);
    obj.dput("ab", 2);
    EXPECT_EQ(obj.str(), "123ab");

    obj.dseek(obj.dtell() - 1);
    obj.dput("x", 1);
    EXPECT_EQ(obj.str(), "123ax");
}

TEST(MemDeviceChar, SeekAndDrseekSelectTheSameByteToOverwrite)
{
    dev obj("12345");

    obj.dseek(2);
    obj.dput("x", 1);
    EXPECT_EQ(obj.str(), "12x45");

    obj.dseek(1);
    obj.dput("y", 1);
    EXPECT_EQ(obj.str(), "1yx45");

    obj.dseek(obj.dtell() + 2);
    obj.dput("z", 1);
    EXPECT_EQ(obj.str(), "1yx4z");

    obj.drseek(3);
    obj.dput("a", 1);
    EXPECT_EQ(obj.str(), "1ya4z");
}

TEST(MemDeviceChar, FailedSeeksLeaveTheWritePositionAlone)
{
    dev obj("12345");
    obj.drseek(0);

    EXPECT_ANY_THROW(obj.dseek(100));
    EXPECT_EQ(obj.dtell(), 5u);

    obj.dseek(3);

    EXPECT_ANY_THROW(obj.dseek(100));
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_ANY_THROW(obj.dseek(-100));
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_ANY_THROW(obj.dseek(obj.dtell() + 100));
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_ANY_THROW(obj.dseek(obj.dtell() - 100));
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_ANY_THROW(obj.drseek(-100));
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_ANY_THROW(obj.drseek(100));
    EXPECT_EQ(obj.dtell(), 3u);
}

// ---------------------------------------------------------------------------
// Reading and writing through the same cursor.
// ---------------------------------------------------------------------------

TEST(MemDeviceChar, ReadThenWriteSharesOneCursor)
{
    dev obj;
    EXPECT_EQ(obj.dtell(), 0u);

    obj.dput("123", 3);
    EXPECT_EQ(obj.dtell(), 3u);

    obj.dseek(0);
    char ch = 0;
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, '1');
    EXPECT_EQ(obj.dtell(), 1u);

    obj.drseek(0);
    obj.dput("x", 1);
    EXPECT_EQ(obj.dtell(), 4u);
}

TEST(MemDeviceChar, AppendingAfterAReadExtendsTheBuffer)
{
    dev obj("12345");
    EXPECT_EQ(obj.dtell(), 0u);

    char buf[5] = {};
    EXPECT_EQ(obj.dget(buf, 4), 4u);
    EXPECT_EQ(obj.dtell(), 4u);

    obj.drseek(0);
    obj.dput("123", 3);
    EXPECT_EQ(obj.dtell(), 8u);

    obj.dseek(4);
    EXPECT_EQ(obj.dget(buf, 5), 4u);
    EXPECT_EQ(obj.dtell(), 8u);
}

TEST(MemDeviceChar, SeekToTheNewEndAfterGrowing)
{
    dev obj("12345");
    EXPECT_EQ(obj.dtell(), 0u);

    // 10 is past the end of a five-character buffer...
    EXPECT_ANY_THROW(obj.dseek(10));
    EXPECT_EQ(obj.dtell(), 0u);

    obj.drseek(0);
    obj.dput("abcde", 5);

    // ...and one past the last character of a ten-character one, which is where
    // the next write goes, so it is a legal position.
    obj.dseek(10);
    EXPECT_EQ(obj.dtell(), 10u);

    obj.dseek(3);
    EXPECT_EQ(obj.dtell(), 3u);

    obj.dput("xxxx", 4);
    EXPECT_EQ(obj.str(), "123xxxxcde");
}

TEST(MemDeviceChar, PutFromTheDevicesOwnBufferSurvivesReallocation)
{
    dev obj(std::string("OriginalData"));

    // The source overlaps the destination and the write grows the buffer, so a
    // naive implementation would copy from a pointer its own resize freed.
    obj.dseek(obj.dsize());
    const char* alias = obj.str().data();
    obj.dput(alias, 8);

    EXPECT_EQ(obj.str(), "OriginalDataOriginal");
    EXPECT_EQ(obj.dtell(), 20u);
}

// ---------------------------------------------------------------------------
// Error paths and the buffer-borrowing interface.
// ---------------------------------------------------------------------------

TEST(MemDeviceChar, ConstructionFromANullPointerThrows)
{
    EXPECT_THROW((void)dev(static_cast<const char*>(nullptr)), device_error);
}

TEST(MemDeviceChar, MoveConstructionTakesTheBufferAndThePosition)
{
    dev d1("abc");
    d1.dseek(1);

    dev d2(std::move(d1));
    EXPECT_EQ(d2.str(), "abc");
    EXPECT_EQ(d2.dtell(), 1u);
    EXPECT_EQ(d1.dtell(), 0u);
    EXPECT_TRUE(d1.str().empty());
}

TEST(MemDeviceChar, MoveAssignmentIncludingOntoItself)
{
    dev d1("abc");
    dev d2("xyz");

    d2 = std::move(d1);
    EXPECT_EQ(d2.str(), "abc");
    EXPECT_TRUE(d1.str().empty());

    // Outside any macro: the diagnostic fires at the assignment itself.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    d2 = std::move(d2);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    EXPECT_EQ(d2.str(), "abc");
}

TEST(MemDeviceChar, GetRejectsANullBufferOnlyWhenItWouldReadIt)
{
    dev obj("abc");
    char ch = 0;

    EXPECT_EQ(obj.dget(&ch, 0), 0u);
    EXPECT_THROW(obj.dget(nullptr, 1), device_error);
}

TEST(MemDeviceChar, PutRejectsANullBufferAndAnOverflowingLength)
{
    dev obj;

    obj.dput("a", 0);
    EXPECT_THROW(obj.dput(nullptr, 1), device_error);
    EXPECT_THROW(obj.dput("a", std::numeric_limits<std::size_t>::max()), device_error);
}

TEST(MemDeviceChar, GetBufSaturatesOrInsists)
{
    dev obj("abc");

    // <true> demands the exact length asked for; <false> hands back what is
    // there and reports how much that was.
    EXPECT_THROW((void)obj.get_buf<true>(4), device_error);

    auto [ptr, len] = obj.get_buf<false>(5);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(len, 3u);

    dev exact("abc");
    const char* exact_ptr = exact.get_buf<true>(2);
    EXPECT_EQ(std::string(exact_ptr, 2), "ab");
    EXPECT_EQ(exact.dtell(), 2u);
}

TEST(MemDeviceChar, PutBufReusesExistingStorageAndCanGrowBeyondCapacity)
{
    dev existing("abc");
    existing.dseek(1);
    char* reused = existing.put_buf(1);
    *reused = 'X';
    EXPECT_EQ(existing.str(), "aXc");
    EXPECT_EQ(existing.dtell(), 2u);

    dev growing;
    const std::size_t requested = growing.str().capacity() + 1;
    char* expanded = growing.put_buf(requested);
    expanded[0] = 'Y';
    EXPECT_EQ(growing.str()[0], 'Y');
    EXPECT_EQ(growing.dsize(), requested);
    EXPECT_EQ(growing.dtell(), requested);
}

TEST(MemDeviceChar, GetRollbackTakesBackExactlyWhatWasRead)
{
    dev obj("abc");
    obj.dseek(1);

    EXPECT_THROW(obj.get_rollback(0), device_error);
    EXPECT_THROW(obj.get_rollback(2), device_error);

    obj.get_rollback(1);
    EXPECT_EQ(obj.dtell(), 0u);
}

TEST(MemDeviceChar, PutRollbackNeedsAMatchingPutBuf)
{
    dev obj;

    // No checkpoint yet, so there is nothing to roll back to.
    EXPECT_THROW(obj.put_rollback(1), device_error);

    obj.put_buf(5);
    EXPECT_THROW(obj.put_rollback(0), device_error);
    EXPECT_THROW(obj.put_rollback(6), device_error);

    obj.put_rollback(2);
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_THROW((void)obj.put_buf(std::numeric_limits<std::size_t>::max()), device_error);
}

TEST(MemDeviceChar, FlushLeavesTheBufferUntouched)
{
    dev obj("abc");
    obj.dflush();
    EXPECT_EQ(obj.dsize(), 3u);
}
