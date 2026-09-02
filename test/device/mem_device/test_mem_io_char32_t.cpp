// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/device/device_concepts.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>

using namespace IOv2;

namespace
{
    using dev = mem_device<char32_t>;

    static_assert(io_device<dev>);
    static_assert(std::is_same_v<dev::char_type, char32_t>);
    static_assert(dev_cpt::support_positioning<dev>);
    static_assert(dev_cpt::support_put<dev>);
    static_assert(dev_cpt::support_get<dev>);
}

TEST(MemDeviceChar32, Traits)
{
    // Every assertion above is a static_assert; compiling is the check.
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Construction and assignment.
// ---------------------------------------------------------------------------

TEST(MemDeviceChar32, DefaultConstructionIsEmpty)
{
    dev obj;
    EXPECT_TRUE(obj.str().empty());
    EXPECT_EQ(obj.dtell(), 0u);
}

TEST(MemDeviceChar32, ConstructionFromStringKeepsTheContents)
{
    const std::u32string text = U"buffered characters";

    dev obj(text);
    EXPECT_EQ(obj.str(), text);
    EXPECT_EQ(obj.dtell(), 0u);

    char32_t ch = 0;
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, text[0]);

    obj.dput(U"Y", 1);
    EXPECT_EQ(obj.str(), U"bYffered characters");
    EXPECT_EQ(obj.dtell(), 2u);
}

TEST(MemDeviceChar32, MoveAssignmentFromTemporaryResetsThePosition)
{
    dev obj;
    EXPECT_EQ(obj.str(), U"");
    EXPECT_EQ(obj.dtell(), 0u);

    obj = dev{U"Hello world"};
    EXPECT_EQ(obj.str(), U"Hello world");
    EXPECT_EQ(obj.dtell(), 0u);
}

TEST(MemDeviceChar32, ConstructionCopiesTheSourceString)
{
    std::u32string ref = U"Hello world";

    dev obj(ref);
    EXPECT_EQ(obj.str(), ref);
    EXPECT_EQ(obj.dtell(), 0u);

    // The device holds a copy, so growing `ref` does not reach it.
    ref += U"123";
    EXPECT_NE(obj.str(), ref);
    obj = dev{ref};
    EXPECT_EQ(obj.str(), ref);
    EXPECT_EQ(obj.dtell(), 0u);
}

// ---------------------------------------------------------------------------
// Input.
// ---------------------------------------------------------------------------

TEST(MemDeviceChar32, GetOneCharacterAtATime)
{
    dev obj(U"12");
    char32_t ch = 0;

    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, U'1');
    EXPECT_EQ(obj.dtell(), 1u);

    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, U'2');
    EXPECT_EQ(obj.dtell(), 2u);

    EXPECT_EQ(obj.dget(&ch, 1), 0u);
    EXPECT_EQ(obj.dtell(), 2u);
}

TEST(MemDeviceChar32, GetFillsTheWholeRequest)
{
    dev obj(U"12345");
    char32_t buf[5] = {};

    EXPECT_EQ(obj.dget(buf, 5), 5u);
    EXPECT_EQ(std::u32string(buf, 5), U"12345");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceChar32, SuccessiveGetsResumeWhereTheLastStopped)
{
    dev obj(U"12345");
    char32_t buf[5] = {};

    EXPECT_EQ(obj.dget(buf, 3), 3u);
    EXPECT_EQ(std::u32string(buf, 3), U"123");
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_EQ(obj.dget(buf, 2), 2u);
    EXPECT_EQ(std::u32string(buf, 2), U"45");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceChar32, GetPastTheEndReturnsWhatIsLeft)
{
    dev obj(U"12345");
    char32_t buf[10] = {};

    EXPECT_EQ(obj.dget(buf, 10), 5u);
    EXPECT_EQ(std::u32string(buf, 5), U"12345");
    EXPECT_EQ(obj.dtell(), 5u);

    EXPECT_EQ(obj.dget(buf, 10), 0u);
}

TEST(MemDeviceChar32, ShortGetAfterAFullOneStopsAtTheEnd)
{
    dev obj(U"12345");
    char32_t buf[5] = {};

    EXPECT_EQ(obj.dget(buf, 3), 3u);
    EXPECT_EQ(std::u32string(buf, 3), U"123");
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_EQ(obj.dget(buf, 5), 2u);
    EXPECT_EQ(std::u32string(buf, 2), U"45");
    EXPECT_EQ(obj.dtell(), 5u);

    EXPECT_EQ(obj.dget(buf, 10), 0u);
}

TEST(MemDeviceChar32, GetAfterSeek)
{
    dev obj(U"12345");
    char32_t buf[5] = {};

    obj.dseek(3);
    EXPECT_EQ(obj.dget(buf, 5), 2u);
    EXPECT_EQ(std::u32string(buf, 2), U"45");

    obj.dseek(obj.dtell() - 1);
    EXPECT_EQ(obj.dget(buf, 5), 1u);
    EXPECT_EQ(buf[0], U'5');
}

TEST(MemDeviceChar32, SeekAndDrseekAddressTheSameCharacter)
{
    dev obj(U"12345");
    char32_t ch = 0;

    obj.dseek(2);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, U'3');

    obj.dseek(1);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, U'2');

    obj.dseek(obj.dtell() + 2);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, U'5');

    // Three back from the end is the same place as two from the front.
    obj.drseek(3);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, U'3');
}

TEST(MemDeviceChar32, FailedSeeksLeaveTheReadPositionAlone)
{
    dev obj(U"12345");

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

TEST(MemDeviceChar32, PutAtTheEndAppends)
{
    dev obj(U"12");
    obj.drseek(0);

    obj.dput(U"x", 1);
    EXPECT_EQ(obj.str(), U"12x");
    EXPECT_EQ(obj.dtell(), 3u);
}

TEST(MemDeviceChar32, PutInsideTheBufferOverwrites)
{
    dev obj(U"12");

    obj.dput(U"x", 1);
    EXPECT_EQ(obj.str(), U"x2");
    EXPECT_EQ(obj.dtell(), 1u);
}

TEST(MemDeviceChar32, PutIntoADefaultConstructedDevice)
{
    dev obj;

    obj.dput(U"y", 1);
    EXPECT_EQ(obj.str(), U"y");
    EXPECT_EQ(obj.dtell(), 1u);
}

TEST(MemDeviceChar32, PutGrowsAnEmptyDevice)
{
    dev obj;
    obj.dput(U"12345", 5);
    EXPECT_EQ(obj.str(), U"12345");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceChar32, ZeroLengthPutAcceptsANullBuffer)
{
    dev obj;

    // Nothing is dereferenced when the count is zero.
    obj.dput(nullptr, 0);
    EXPECT_EQ(obj.str(), U"");
    EXPECT_EQ(obj.dtell(), 0u);
}

TEST(MemDeviceChar32, SuccessivePutsAppendInOrder)
{
    dev obj;

    obj.dput(U"123", 3);
    EXPECT_EQ(obj.str(), U"123");
    EXPECT_EQ(obj.dtell(), 3u);

    obj.dput(U"45", 2);
    EXPECT_EQ(obj.str(), U"12345");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceChar32, SinglePutBetweenLongerOnes)
{
    dev obj;

    obj.dput(U"123", 3);
    EXPECT_EQ(obj.str(), U"123");

    obj.dput(U"x", 1);
    EXPECT_EQ(obj.str(), U"123x");

    obj.dput(U"45", 2);
    EXPECT_EQ(obj.str(), U"123x45");
    EXPECT_EQ(obj.dtell(), 6u);
}

TEST(MemDeviceChar32, PutAfterSeekOverwritesThenExtends)
{
    dev obj(U"12345");

    obj.dseek(3);
    obj.dput(U"ab", 2);
    EXPECT_EQ(obj.str(), U"123ab");

    obj.dseek(obj.dtell() - 1);
    obj.dput(U"x", 1);
    EXPECT_EQ(obj.str(), U"123ax");
}

TEST(MemDeviceChar32, SeekAndDrseekSelectTheSameByteToOverwrite)
{
    dev obj(U"12345");

    obj.dseek(2);
    obj.dput(U"x", 1);
    EXPECT_EQ(obj.str(), U"12x45");

    obj.dseek(1);
    obj.dput(U"y", 1);
    EXPECT_EQ(obj.str(), U"1yx45");

    obj.dseek(obj.dtell() + 2);
    obj.dput(U"z", 1);
    EXPECT_EQ(obj.str(), U"1yx4z");

    obj.drseek(3);
    obj.dput(U"a", 1);
    EXPECT_EQ(obj.str(), U"1ya4z");
}

TEST(MemDeviceChar32, FailedSeeksLeaveTheWritePositionAlone)
{
    dev obj(U"12345");
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

TEST(MemDeviceChar32, ReadThenWriteSharesOneCursor)
{
    dev obj;
    EXPECT_EQ(obj.dtell(), 0u);

    obj.dput(U"123", 3);
    EXPECT_EQ(obj.dtell(), 3u);

    obj.dseek(0);
    char32_t ch = 0;
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, U'1');
    EXPECT_EQ(obj.dtell(), 1u);

    obj.drseek(0);
    obj.dput(U"x", 1);
    EXPECT_EQ(obj.dtell(), 4u);
}

TEST(MemDeviceChar32, AppendingAfterAReadExtendsTheBuffer)
{
    dev obj(U"12345");
    EXPECT_EQ(obj.dtell(), 0u);

    char32_t buf[5] = {};
    EXPECT_EQ(obj.dget(buf, 4), 4u);
    EXPECT_EQ(obj.dtell(), 4u);

    obj.drseek(0);
    obj.dput(U"123", 3);
    EXPECT_EQ(obj.dtell(), 8u);

    obj.dseek(4);
    EXPECT_EQ(obj.dget(buf, 5), 4u);
    EXPECT_EQ(obj.dtell(), 8u);
}

TEST(MemDeviceChar32, SeekToTheNewEndAfterGrowing)
{
    dev obj(U"12345");
    EXPECT_EQ(obj.dtell(), 0u);

    // 10 is past the end of a five-character buffer...
    EXPECT_ANY_THROW(obj.dseek(10));
    EXPECT_EQ(obj.dtell(), 0u);

    obj.drseek(0);
    obj.dput(U"abcde", 5);

    // ...and one past the last character of a ten-character one, which is where
    // the next write goes, so it is a legal position.
    obj.dseek(10);
    EXPECT_EQ(obj.dtell(), 10u);

    obj.dseek(3);
    EXPECT_EQ(obj.dtell(), 3u);

    obj.dput(U"xxxx", 4);
    EXPECT_EQ(obj.str(), U"123xxxxcde");
}
