/**
 * ios_base's per-object pword storage and the xalloc indices that address it.
 *
 * [ios.base.storage] says only that xalloc returns a value not returned by any
 * previous call, and that a slot starts out empty. Everything a caller can get
 * wrong follows from those two: indices must not be reused, a slot nobody has
 * written must read as empty rather than as anything else, and the counter is
 * shared per character type, so ios_base<char> and ios_base<wchar_t> hand out
 * indices independently.
 *
 * IOv2 stores shared_ptr<void> rather than void*, which makes set_pword answer
 * with what it displaced and makes a null store an erase; that three-way
 * behaviour is what the last test pins down.
 */
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/ostream.h>

#include <gtest/gtest.h>

#include <limits>
#include <memory>

using namespace IOv2;

namespace
{
    class derived : public ios_base<char>
    {
    public:
        derived() {}
    };
}

TEST(IosBaseStorage, XallocNeverHandsOutTheSameIndexTwice)
{
    const int first  = ios_base<char>::xalloc();
    const int second = ios_base<char>::xalloc();
    EXPECT_NE(first, second);

    // The counters are per character type, so a wide allocation cannot collide
    // with a narrow one that is already in use.
    const int wide = ios_base<wchar_t>::xalloc();
    const int again = ios_base<wchar_t>::xalloc();
    EXPECT_NE(wide, again);
}

// A slot nobody has written to is empty, whether or not the index came from
// xalloc at all. Reading one must answer, not misbehave.
TEST(IosBaseStorage, AnIndexNobodyHasWrittenToReadsAsEmpty)
{
    ostream out(mem_device{""});

    EXPECT_EQ(out.get_pword(ios_base<char>::xalloc()), nullptr);
    EXPECT_EQ(out.get_pword(std::numeric_limits<int>::max() - 1), nullptr);
}

TEST(IosBaseStorage, StoringNullIntoAnEmptySlotLeavesItEmpty)
{
    ios_base<char> io;

    io.set_pword(1, nullptr);
    EXPECT_EQ(io.get_pword(1), nullptr);
}

TEST(IosBaseStorage, WhatIsStoredComesBackUnchanged)
{
    ios_base<char> io;

    auto d = std::make_shared<derived>();
    io.set_pword(1, d);
    EXPECT_EQ(io.get_pword(1), d);
}

// Exercise set_pword() on an id that already has an entry (the "else"
// branch): replacing returns the previous value; setting nullptr erases the
// entry and likewise returns the previous value.
TEST(IosBaseStorage, SetPwordAnswersWithWhatItDisplaced)
{
    ios_base<char> io;

    auto d1 = std::make_shared<derived>();
    auto d2 = std::make_shared<derived>();

    // First insert (id absent): returns nullptr.
    EXPECT_EQ(io.set_pword(1, d1), nullptr);
    EXPECT_EQ(io.get_pword(1), d1);

    // Replace an existing entry: returns the previous value, stores the new one.
    EXPECT_EQ(io.set_pword(1, d2), d1);
    EXPECT_EQ(io.get_pword(1), d2);

    // Erase an existing entry via nullptr: returns the previous value, removes it.
    EXPECT_EQ(io.set_pword(1, nullptr), d2);
    EXPECT_EQ(io.get_pword(1), nullptr);
}
