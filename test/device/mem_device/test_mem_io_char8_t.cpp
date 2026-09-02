// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <device/device_concepts.h>
#include <device/mem_device.h>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>

using namespace IOv2;

namespace
{
    using dev = mem_device<char8_t>;

    static_assert(io_device<dev>);
    static_assert(std::is_same_v<dev::char_type, char8_t>);
    static_assert(dev_cpt::support_positioning<dev>);
    static_assert(dev_cpt::support_put<dev>);
    static_assert(dev_cpt::support_get<dev>);
}

TEST(MemDeviceChar8, Traits)
{
    // Every assertion above is a static_assert; compiling is the check.
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Construction and assignment.
// ---------------------------------------------------------------------------

TEST(MemDeviceChar8, DefaultConstructionIsEmpty)
{
    dev obj;
    EXPECT_TRUE(obj.str().empty());
    EXPECT_EQ(obj.dtell(), 0u);
}

TEST(MemDeviceChar8, ConstructionFromStringKeepsTheContents)
{
    const std::u8string text = u8"buffered characters";

    dev obj(text);
    EXPECT_EQ(obj.str(), text);
    EXPECT_EQ(obj.dtell(), 0u);

    char8_t ch = 0;
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, text[0]);

    obj.dput(u8"Y", 1);
    EXPECT_EQ(obj.str(), u8"bYffered characters");
    EXPECT_EQ(obj.dtell(), 2u);
}

TEST(MemDeviceChar8, MoveAssignmentFromTemporaryResetsThePosition)
{
    dev obj;
    EXPECT_EQ(obj.str(), u8"");
    EXPECT_EQ(obj.dtell(), 0u);

    obj = dev{u8"Hello world"};
    EXPECT_EQ(obj.str(), u8"Hello world");
    EXPECT_EQ(obj.dtell(), 0u);
}

TEST(MemDeviceChar8, ConstructionCopiesTheSourceString)
{
    std::u8string ref = u8"Hello world";

    dev obj(ref);
    EXPECT_EQ(obj.str(), ref);
    EXPECT_EQ(obj.dtell(), 0u);

    // The device holds a copy, so growing `ref` does not reach it.
    ref += u8"123";
    EXPECT_NE(obj.str(), ref);
    obj = dev{ref};
    EXPECT_EQ(obj.str(), ref);
    EXPECT_EQ(obj.dtell(), 0u);
}

// ---------------------------------------------------------------------------
// Input.
// ---------------------------------------------------------------------------

TEST(MemDeviceChar8, GetOneCharacterAtATime)
{
    dev obj(u8"12");
    char8_t ch = 0;

    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, u8'1');
    EXPECT_EQ(obj.dtell(), 1u);

    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, u8'2');
    EXPECT_EQ(obj.dtell(), 2u);

    EXPECT_EQ(obj.dget(&ch, 1), 0u);
    EXPECT_EQ(obj.dtell(), 2u);
}

TEST(MemDeviceChar8, GetFillsTheWholeRequest)
{
    dev obj(u8"12345");
    char8_t buf[5] = {};

    EXPECT_EQ(obj.dget(buf, 5), 5u);
    EXPECT_EQ(std::u8string(buf, 5), u8"12345");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceChar8, SuccessiveGetsResumeWhereTheLastStopped)
{
    dev obj(u8"12345");
    char8_t buf[5] = {};

    EXPECT_EQ(obj.dget(buf, 3), 3u);
    EXPECT_EQ(std::u8string(buf, 3), u8"123");
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_EQ(obj.dget(buf, 2), 2u);
    EXPECT_EQ(std::u8string(buf, 2), u8"45");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceChar8, GetPastTheEndReturnsWhatIsLeft)
{
    dev obj(u8"12345");
    char8_t buf[10] = {};

    EXPECT_EQ(obj.dget(buf, 10), 5u);
    EXPECT_EQ(std::u8string(buf, 5), u8"12345");
    EXPECT_EQ(obj.dtell(), 5u);

    EXPECT_EQ(obj.dget(buf, 10), 0u);
}

TEST(MemDeviceChar8, ShortGetAfterAFullOneStopsAtTheEnd)
{
    dev obj(u8"12345");
    char8_t buf[5] = {};

    EXPECT_EQ(obj.dget(buf, 3), 3u);
    EXPECT_EQ(std::u8string(buf, 3), u8"123");
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_EQ(obj.dget(buf, 5), 2u);
    EXPECT_EQ(std::u8string(buf, 2), u8"45");
    EXPECT_EQ(obj.dtell(), 5u);

    EXPECT_EQ(obj.dget(buf, 10), 0u);
}

TEST(MemDeviceChar8, GetAfterSeek)
{
    dev obj(u8"12345");
    char8_t buf[5] = {};

    obj.dseek(3);
    EXPECT_EQ(obj.dget(buf, 5), 2u);
    EXPECT_EQ(std::u8string(buf, 2), u8"45");

    obj.dseek(obj.dtell() - 1);
    EXPECT_EQ(obj.dget(buf, 5), 1u);
    EXPECT_EQ(buf[0], u8'5');
}

TEST(MemDeviceChar8, SeekAndDrseekAddressTheSameCharacter)
{
    dev obj(u8"12345");
    char8_t ch = 0;

    obj.dseek(2);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, u8'3');

    obj.dseek(1);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, u8'2');

    obj.dseek(obj.dtell() + 2);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, u8'5');

    // Three back from the end is the same place as two from the front.
    obj.drseek(3);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, u8'3');
}

TEST(MemDeviceChar8, FailedSeeksLeaveTheReadPositionAlone)
{
    dev obj(u8"12345");

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
// Output.
// ---------------------------------------------------------------------------

TEST(MemDeviceChar8, PutAtTheEndAppends)
{
    dev obj(u8"12");
    obj.drseek(0);

    obj.dput(u8"x", 1);
    EXPECT_EQ(obj.str(), u8"12x");
    EXPECT_EQ(obj.dtell(), 3u);
}

TEST(MemDeviceChar8, PutInsideTheBufferOverwrites)
{
    dev obj(u8"12");

    obj.dput(u8"x", 1);
    EXPECT_EQ(obj.str(), u8"x2");
    EXPECT_EQ(obj.dtell(), 1u);
}

TEST(MemDeviceChar8, PutIntoADefaultConstructedDevice)
{
    dev obj;

    obj.dput(u8"y", 1);
    EXPECT_EQ(obj.str(), u8"y");
    EXPECT_EQ(obj.dtell(), 1u);
}

TEST(MemDeviceChar8, PutGrowsAnEmptyDevice)
{
    dev obj;
    obj.dput(u8"12345", 5);
    EXPECT_EQ(obj.str(), u8"12345");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceChar8, ZeroLengthPutAcceptsANullBuffer)
{
    dev obj;

    // Nothing is dereferenced when the count is zero.
    obj.dput(nullptr, 0);
    EXPECT_EQ(obj.str(), u8"");
    EXPECT_EQ(obj.dtell(), 0u);
}

TEST(MemDeviceChar8, SuccessivePutsAppendInOrder)
{
    dev obj;

    obj.dput(u8"123", 3);
    EXPECT_EQ(obj.str(), u8"123");
    EXPECT_EQ(obj.dtell(), 3u);

    obj.dput(u8"45", 2);
    EXPECT_EQ(obj.str(), u8"12345");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceChar8, SinglePutBetweenLongerOnes)
{
    dev obj;

    obj.dput(u8"123", 3);
    EXPECT_EQ(obj.str(), u8"123");

    obj.dput(u8"x", 1);
    EXPECT_EQ(obj.str(), u8"123x");

    obj.dput(u8"45", 2);
    EXPECT_EQ(obj.str(), u8"123x45");
    EXPECT_EQ(obj.dtell(), 6u);
}

TEST(MemDeviceChar8, PutAfterSeekOverwritesThenExtends)
{
    dev obj(u8"12345");

    obj.dseek(3);
    obj.dput(u8"ab", 2);
    EXPECT_EQ(obj.str(), u8"123ab");

    obj.dseek(obj.dtell() - 1);
    obj.dput(u8"x", 1);
    EXPECT_EQ(obj.str(), u8"123ax");
}

TEST(MemDeviceChar8, SeekAndDrseekSelectTheSameByteToOverwrite)
{
    dev obj(u8"12345");

    obj.dseek(2);
    obj.dput(u8"x", 1);
    EXPECT_EQ(obj.str(), u8"12x45");

    obj.dseek(1);
    obj.dput(u8"y", 1);
    EXPECT_EQ(obj.str(), u8"1yx45");

    obj.dseek(obj.dtell() + 2);
    obj.dput(u8"z", 1);
    EXPECT_EQ(obj.str(), u8"1yx4z");

    obj.drseek(3);
    obj.dput(u8"a", 1);
    EXPECT_EQ(obj.str(), u8"1ya4z");
}

TEST(MemDeviceChar8, FailedSeeksLeaveTheWritePositionAlone)
{
    dev obj(u8"12345");
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

TEST(MemDeviceChar8, ReadThenWriteSharesOneCursor)
{
    dev obj;
    EXPECT_EQ(obj.dtell(), 0u);

    obj.dput(u8"123", 3);
    EXPECT_EQ(obj.dtell(), 3u);

    obj.dseek(0);
    char8_t ch = 0;
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, u8'1');
    EXPECT_EQ(obj.dtell(), 1u);

    obj.drseek(0);
    obj.dput(u8"x", 1);
    EXPECT_EQ(obj.dtell(), 4u);
}

TEST(MemDeviceChar8, AppendingAfterAReadExtendsTheBuffer)
{
    dev obj(u8"12345");
    EXPECT_EQ(obj.dtell(), 0u);

    char8_t buf[5] = {};
    EXPECT_EQ(obj.dget(buf, 4), 4u);
    EXPECT_EQ(obj.dtell(), 4u);

    obj.drseek(0);
    obj.dput(u8"123", 3);
    EXPECT_EQ(obj.dtell(), 8u);

    obj.dseek(4);
    EXPECT_EQ(obj.dget(buf, 5), 4u);
    EXPECT_EQ(obj.dtell(), 8u);
}

TEST(MemDeviceChar8, SeekToTheNewEndAfterGrowing)
{
    dev obj(u8"12345");
    EXPECT_EQ(obj.dtell(), 0u);

    // 10 is past the end of a five-character buffer...
    EXPECT_ANY_THROW(obj.dseek(10));
    EXPECT_EQ(obj.dtell(), 0u);

    obj.drseek(0);
    obj.dput(u8"abcde", 5);

    // ...and one past the last character of a ten-character one, which is where
    // the next write goes, so it is a legal position.
    obj.dseek(10);
    EXPECT_EQ(obj.dtell(), 10u);

    obj.dseek(3);
    EXPECT_EQ(obj.dtell(), 3u);

    obj.dput(u8"xxxx", 4);
    EXPECT_EQ(obj.str(), u8"123xxxxcde");
}
