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
    using dev = mem_device<wchar_t>;

    static_assert(io_device<dev>);
    static_assert(std::is_same_v<dev::char_type, wchar_t>);
    static_assert(dev_cpt::support_positioning<dev>);
    static_assert(dev_cpt::support_put<dev>);
    static_assert(dev_cpt::support_get<dev>);
}

TEST(MemDeviceWchar, Traits)
{
    // Every assertion above is a static_assert; compiling is the check.
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Construction and assignment.
// ---------------------------------------------------------------------------

TEST(MemDeviceWchar, DefaultConstructionIsEmpty)
{
    dev obj;
    EXPECT_TRUE(obj.str().empty());
    EXPECT_EQ(obj.dtell(), 0u);
}

TEST(MemDeviceWchar, ConstructionFromStringKeepsTheContents)
{
    const std::wstring text = L"buffered characters";

    dev obj(text);
    EXPECT_EQ(obj.str(), text);
    EXPECT_EQ(obj.dtell(), 0u);

    wchar_t ch = 0;
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, text[0]);

    obj.dput(L"Y", 1);
    EXPECT_EQ(obj.str(), L"bYffered characters");
    EXPECT_EQ(obj.dtell(), 2u);
}

TEST(MemDeviceWchar, MoveAssignmentFromTemporaryResetsThePosition)
{
    dev obj;
    EXPECT_EQ(obj.str(), L"");
    EXPECT_EQ(obj.dtell(), 0u);

    obj = dev{L"Hello world"};
    EXPECT_EQ(obj.str(), L"Hello world");
    EXPECT_EQ(obj.dtell(), 0u);
}

TEST(MemDeviceWchar, ConstructionCopiesTheSourceString)
{
    std::wstring ref = L"Hello world";

    dev obj(ref);
    EXPECT_EQ(obj.str(), ref);
    EXPECT_EQ(obj.dtell(), 0u);

    // The device holds a copy, so growing `ref` does not reach it.
    ref += L"123";
    EXPECT_NE(obj.str(), ref);
    obj = dev{ref};
    EXPECT_EQ(obj.str(), ref);
    EXPECT_EQ(obj.dtell(), 0u);
}

// ---------------------------------------------------------------------------
// Input.
// ---------------------------------------------------------------------------

TEST(MemDeviceWchar, GetOneCharacterAtATime)
{
    dev obj(L"12");
    wchar_t ch = 0;

    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, L'1');
    EXPECT_EQ(obj.dtell(), 1u);

    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, L'2');
    EXPECT_EQ(obj.dtell(), 2u);

    EXPECT_EQ(obj.dget(&ch, 1), 0u);
    EXPECT_EQ(obj.dtell(), 2u);
}

TEST(MemDeviceWchar, GetFillsTheWholeRequest)
{
    dev obj(L"12345");
    wchar_t buf[5] = {};

    EXPECT_EQ(obj.dget(buf, 5), 5u);
    EXPECT_EQ(std::wstring(buf, 5), L"12345");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceWchar, SuccessiveGetsResumeWhereTheLastStopped)
{
    dev obj(L"12345");
    wchar_t buf[5] = {};

    EXPECT_EQ(obj.dget(buf, 3), 3u);
    EXPECT_EQ(std::wstring(buf, 3), L"123");
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_EQ(obj.dget(buf, 2), 2u);
    EXPECT_EQ(std::wstring(buf, 2), L"45");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceWchar, GetPastTheEndReturnsWhatIsLeft)
{
    dev obj(L"12345");
    wchar_t buf[10] = {};

    EXPECT_EQ(obj.dget(buf, 10), 5u);
    EXPECT_EQ(std::wstring(buf, 5), L"12345");
    EXPECT_EQ(obj.dtell(), 5u);

    EXPECT_EQ(obj.dget(buf, 10), 0u);
}

TEST(MemDeviceWchar, ShortGetAfterAFullOneStopsAtTheEnd)
{
    dev obj(L"12345");
    wchar_t buf[5] = {};

    EXPECT_EQ(obj.dget(buf, 3), 3u);
    EXPECT_EQ(std::wstring(buf, 3), L"123");
    EXPECT_EQ(obj.dtell(), 3u);

    EXPECT_EQ(obj.dget(buf, 5), 2u);
    EXPECT_EQ(std::wstring(buf, 2), L"45");
    EXPECT_EQ(obj.dtell(), 5u);

    EXPECT_EQ(obj.dget(buf, 10), 0u);
}

TEST(MemDeviceWchar, GetAfterSeek)
{
    dev obj(L"12345");
    wchar_t buf[5] = {};

    obj.dseek(3);
    EXPECT_EQ(obj.dget(buf, 5), 2u);
    EXPECT_EQ(std::wstring(buf, 2), L"45");

    obj.dseek(obj.dtell() - 1);
    EXPECT_EQ(obj.dget(buf, 5), 1u);
    EXPECT_EQ(buf[0], L'5');
}

TEST(MemDeviceWchar, SeekAndDrseekAddressTheSameCharacter)
{
    dev obj(L"12345");
    wchar_t ch = 0;

    obj.dseek(2);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, L'3');

    obj.dseek(1);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, L'2');

    obj.dseek(obj.dtell() + 2);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, L'5');

    // Three back from the end is the same place as two from the front.
    obj.drseek(3);
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, L'3');
}

TEST(MemDeviceWchar, FailedSeeksLeaveTheReadPositionAlone)
{
    dev obj(L"12345");

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

TEST(MemDeviceWchar, PutAtTheEndAppends)
{
    dev obj(L"12");
    obj.drseek(0);

    obj.dput(L"x", 1);
    EXPECT_EQ(obj.str(), L"12x");
    EXPECT_EQ(obj.dtell(), 3u);
}

TEST(MemDeviceWchar, PutInsideTheBufferOverwrites)
{
    dev obj(L"12");

    obj.dput(L"x", 1);
    EXPECT_EQ(obj.str(), L"x2");
    EXPECT_EQ(obj.dtell(), 1u);
}

TEST(MemDeviceWchar, PutIntoADefaultConstructedDevice)
{
    dev obj;

    obj.dput(L"y", 1);
    EXPECT_EQ(obj.str(), L"y");
    EXPECT_EQ(obj.dtell(), 1u);
}

TEST(MemDeviceWchar, PutGrowsAnEmptyDevice)
{
    dev obj;
    obj.dput(L"12345", 5);
    EXPECT_EQ(obj.str(), L"12345");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceWchar, ZeroLengthPutAcceptsANullBuffer)
{
    dev obj;

    // Nothing is dereferenced when the count is zero.
    obj.dput(nullptr, 0);
    EXPECT_EQ(obj.str(), L"");
    EXPECT_EQ(obj.dtell(), 0u);
}

TEST(MemDeviceWchar, SuccessivePutsAppendInOrder)
{
    dev obj;

    obj.dput(L"123", 3);
    EXPECT_EQ(obj.str(), L"123");
    EXPECT_EQ(obj.dtell(), 3u);

    obj.dput(L"45", 2);
    EXPECT_EQ(obj.str(), L"12345");
    EXPECT_EQ(obj.dtell(), 5u);
}

TEST(MemDeviceWchar, SinglePutBetweenLongerOnes)
{
    dev obj;

    obj.dput(L"123", 3);
    EXPECT_EQ(obj.str(), L"123");

    obj.dput(L"x", 1);
    EXPECT_EQ(obj.str(), L"123x");

    obj.dput(L"45", 2);
    EXPECT_EQ(obj.str(), L"123x45");
    EXPECT_EQ(obj.dtell(), 6u);
}

TEST(MemDeviceWchar, PutAfterSeekOverwritesThenExtends)
{
    dev obj(L"12345");

    obj.dseek(3);
    obj.dput(L"ab", 2);
    EXPECT_EQ(obj.str(), L"123ab");

    obj.dseek(obj.dtell() - 1);
    obj.dput(L"x", 1);
    EXPECT_EQ(obj.str(), L"123ax");
}

TEST(MemDeviceWchar, SeekAndDrseekSelectTheSameByteToOverwrite)
{
    dev obj(L"12345");

    obj.dseek(2);
    obj.dput(L"x", 1);
    EXPECT_EQ(obj.str(), L"12x45");

    obj.dseek(1);
    obj.dput(L"y", 1);
    EXPECT_EQ(obj.str(), L"1yx45");

    obj.dseek(obj.dtell() + 2);
    obj.dput(L"z", 1);
    EXPECT_EQ(obj.str(), L"1yx4z");

    obj.drseek(3);
    obj.dput(L"a", 1);
    EXPECT_EQ(obj.str(), L"1ya4z");
}

TEST(MemDeviceWchar, FailedSeeksLeaveTheWritePositionAlone)
{
    dev obj(L"12345");
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

TEST(MemDeviceWchar, ReadThenWriteSharesOneCursor)
{
    dev obj;
    EXPECT_EQ(obj.dtell(), 0u);

    obj.dput(L"123", 3);
    EXPECT_EQ(obj.dtell(), 3u);

    obj.dseek(0);
    wchar_t ch = 0;
    EXPECT_EQ(obj.dget(&ch, 1), 1u);
    EXPECT_EQ(ch, L'1');
    EXPECT_EQ(obj.dtell(), 1u);

    obj.drseek(0);
    obj.dput(L"x", 1);
    EXPECT_EQ(obj.dtell(), 4u);
}

TEST(MemDeviceWchar, AppendingAfterAReadExtendsTheBuffer)
{
    dev obj(L"12345");
    EXPECT_EQ(obj.dtell(), 0u);

    wchar_t buf[5] = {};
    EXPECT_EQ(obj.dget(buf, 4), 4u);
    EXPECT_EQ(obj.dtell(), 4u);

    obj.drseek(0);
    obj.dput(L"123", 3);
    EXPECT_EQ(obj.dtell(), 8u);

    obj.dseek(4);
    EXPECT_EQ(obj.dget(buf, 5), 4u);
    EXPECT_EQ(obj.dtell(), 8u);
}

TEST(MemDeviceWchar, SeekToTheNewEndAfterGrowing)
{
    dev obj(L"12345");
    EXPECT_EQ(obj.dtell(), 0u);

    // 10 is past the end of a five-character buffer...
    EXPECT_ANY_THROW(obj.dseek(10));
    EXPECT_EQ(obj.dtell(), 0u);

    obj.drseek(0);
    obj.dput(L"abcde", 5);

    // ...and one past the last character of a ten-character one, which is where
    // the next write goes, so it is a legal position.
    obj.dseek(10);
    EXPECT_EQ(obj.dtell(), 10u);

    obj.dseek(3);
    EXPECT_EQ(obj.dtell(), 3u);

    obj.dput(L"xxxx", 4);
    EXPECT_EQ(obj.str(), L"123xxxxcde");
}
