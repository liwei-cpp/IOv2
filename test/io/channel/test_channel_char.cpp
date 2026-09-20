// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * iochannel, the layer between a stream and its converter pipeline.
 *
 * The public surface is the getc / bumpc / nextc / getn / putc / putn
 * family, and what the tests pin down is the position each of them leaves
 * behind: getc looks without advancing, bumpc takes and advances, nextc
 * advances and then looks. Getting those three confused is the classic
 * iochannel bug, so every case checks tell() between calls rather than only the
 * character returned.
 *
 * The rest is about the direction machinery: which operations a get-only or
 * put-only buffer offers at all, what switching between them costs, and what
 * detach leaves behind.
 */
#include <IOv2/cvt/comp/zlib_cvt.h>
#include <IOv2/cvt/crypt/hash_cvt.h>
#include <IOv2/cvt/crypt/vigenere_cvt.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/device/std_device.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/iochannel.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <stdio_ext.h>

#include <cstddef>
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

TEST(Channel, AChannelReportsWhichDirectionsItSupports)
{
    using namespace IOv2;

    {
        using CheckType = iochannel<mem_device<char>, char>;
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::char_type, char>);
    }

    {
        using CheckType = ichannel<mem_device<char>, char>;
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::char_type, char>);
    }

    {
        using CheckType = ochannel<mem_device<char>, char>;
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::char_type, char>);
    }
}

TEST(Channel, WritingThroughPutcAndPutnLandsInOrder)
{
    using namespace IOv2;
    
    auto helper = []<typename T>(const T& ori_obj)
    {
        {
            T obj = ori_obj;
            EXPECT_EQ(obj.tell(), 0);
            T obj2(obj);
            EXPECT_EQ(obj2.tell(), 0);
            EXPECT_EQ(obj2.device().str(), "hello");

            obj.putn(" world", 6);
            EXPECT_EQ(obj.tell(), 6);
            EXPECT_EQ(obj2.tell(), 0);
            obj.flush();
            EXPECT_EQ(obj.device().str(), "hello world");
            EXPECT_EQ(obj2.device().str(), "hello");
        }

        {
            auto obj = ori_obj;
            decltype(obj) obj2{mem_device("")};
            obj2 = obj;
            EXPECT_EQ(obj.tell(), 0);
            EXPECT_EQ(obj2.tell(), 0);
            EXPECT_EQ(obj2.device().str(), "hello");

            obj.putn(" world", 6);
            obj.flush();
            EXPECT_EQ(obj.tell(), 6);
            EXPECT_EQ(obj2.tell(), 0);
            EXPECT_EQ(obj.device().str(), "hello world");
            EXPECT_EQ(obj2.device().str(), "hello");
        }

        {
            auto obj = ori_obj;
            auto obj2(std::move(obj));
            EXPECT_EQ(obj2.tell(), 0);
            EXPECT_EQ(obj2.device().str(), "hello");
        }

        {
            auto obj = ori_obj;
            T obj2{mem_device("")};
            obj2 = std::move(obj);
            EXPECT_EQ(obj2.tell(), 0);
            EXPECT_EQ(obj2.device().str(), "hello");
        }
    };

    mem_device dev("hello"); dev.drseek(0);
    helper(iochannel{dev});
    helper(ochannel{dev});
}

TEST(Channel, ReadingBackWhatWasWritten)
{
    using namespace IOv2;
    
    auto helper = [](const auto& ori_obj)
    {
        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            EXPECT_EQ(obj.getn(str.data(), 5), 5);
            EXPECT_EQ(str, "hello");
            EXPECT_EQ(obj.tell(), 5);

            auto obj2(obj);
            EXPECT_EQ(obj2.tell(), 5);
            str.resize(6);
            EXPECT_EQ(obj2.getn(str.data(), 6), 6);
            EXPECT_EQ(str, " world");
            EXPECT_EQ(obj2.tell(), 11);

            str = "xxxxxx";
            EXPECT_EQ(obj.getn(str.data(), 6), 6);
            EXPECT_EQ(str, " world");
            EXPECT_EQ(obj.tell(), 11);
        }

        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            EXPECT_EQ(obj.getn(str.data(), 5), 5);
            EXPECT_EQ(str, "hello");
            EXPECT_EQ(obj.tell(), 5);

            decltype(obj) obj2{mem_device("")};
            obj2 = obj;
            EXPECT_EQ(obj2.tell(), 5);
            str.resize(6);
            EXPECT_EQ(obj2.getn(str.data(), 6), 6);
            EXPECT_EQ(str, " world");
            EXPECT_EQ(obj2.tell(), 11);

            str = "xxxxxx";
            EXPECT_EQ(obj.getn(str.data(), 6), 6);
            EXPECT_EQ(str, " world");
            EXPECT_EQ(obj.tell(), 11);
        }

        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            EXPECT_EQ(obj.getn(str.data(), 5), 5);
            EXPECT_EQ(str, "hello");
            EXPECT_EQ(obj.tell(), 5);

            auto obj2(std::move(obj));
            EXPECT_EQ(obj2.tell(), 5);
            str.resize(6);
            EXPECT_EQ(obj2.getn(str.data(), 6), 6);
            EXPECT_EQ(str, " world");
            EXPECT_EQ(obj2.tell(), 11);
        }

        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            EXPECT_EQ(obj.getn(str.data(), 5), 5);
            EXPECT_EQ(str, "hello");
            EXPECT_EQ(obj.tell(), 5);

            decltype(obj) obj2{mem_device("")};
            obj2 = std::move(obj);
            EXPECT_EQ(obj2.tell(), 5);
            str.resize(6);
            EXPECT_EQ(obj2.getn(str.data(), 6), 6);
            EXPECT_EQ(str, " world");
            EXPECT_EQ(obj2.tell(), 11);
        }
    };

    helper(iochannel{mem_device("hello world")});
    helper(ichannel{mem_device("hello world")});
}

TEST(Channel, GetcLooksWhileBumpcTakes)
{
    using namespace IOv2;

    auto helper = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.getc(), 'a');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.getc(), 'a');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.bumpc(), 'a');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.getc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.getc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.bumpc(), 'b');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.getc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.getc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.bumpc(), 'c');
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.getc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.bumpc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
    };

    iochannel obj1{mem_device{"abc"}};
    helper(obj1);

    ichannel obj2{mem_device{"abc"}};
    helper(obj2);
}

TEST(Channel, TheSameHoldsOnAGetOnlyChannel)
{
    using namespace IOv2;

    auto helper = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.bumpc(), 'a');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.getc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.getc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.bumpc(), 'b');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.getc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.getc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.bumpc(), 'c');
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.getc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.bumpc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
    };

    iochannel obj1{mem_device{"abc"}};
    helper(obj1);

    ichannel obj2{mem_device{"abc"}};
    helper(obj2);
}

TEST(Channel, NextcAdvancesBeforeItLooks)
{
    using namespace IOv2;

    auto helper = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.getc(), 'a');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.getc(), 'a');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.nextc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.getc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.nextc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.getc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.bumpc(), 'c');
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.nextc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.bumpc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.getc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
    };

    iochannel obj1{mem_device{"abc"}};
    helper(obj1);

    ichannel obj2{mem_device{"abc"}};
    helper(obj2);
}

TEST(Channel, NextcOnAGetOnlyChannel)
{
    using namespace IOv2;

    auto helper = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.nextc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.getc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.nextc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.getc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.bumpc(), 'c');
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.nextc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.bumpc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.getc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
    };

    iochannel obj1{mem_device{"abc"}};
    helper(obj1);

    ichannel obj2{mem_device{"abc"}};
    helper(obj2);
}

TEST(Channel, GetnTakesExactlyTheCountAsked)
{
    using namespace IOv2;

    std::string info = "clear morning, a kettle on, and a page of notes";
    
    auto helper = [&info](auto& obj)
    {
        std::string str(info.size(), '\0');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.getn(str.data(), 0), 0);
        EXPECT_EQ(obj.tell(), 0);

        EXPECT_EQ(obj.getn(str.data(), 1), 1);
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(str[0], info[0]);

        EXPECT_EQ(obj.getn(str.data(), str.size()), str.size() - 1);
        EXPECT_EQ(obj.tell(), str.size());
        EXPECT_EQ(str.substr(0, str.size() - 1), info.substr(1));
    };

    iochannel obj1{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj1);

    ichannel obj2{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj2);
}

TEST(Channel, GetnAfterALookAheadStartsWhereTheLookLeftOff)
{
    using namespace IOv2;

    std::string info = "clear morning, a kettle on, and a page of notes";
    
    auto helper = [&info](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.getc(), info[0]);
        EXPECT_EQ(obj.tell(), 0);

        std::string str(info.size(), '\0');
        EXPECT_EQ(obj.getn(str.data(), 0), 0);
        EXPECT_EQ(obj.tell(), 0);

        EXPECT_EQ(obj.getn(str.data(), 1), 1);
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(str[0], info[0]);

        EXPECT_EQ(obj.getc(), info[1]);
        EXPECT_EQ(obj.tell(), 1);

        EXPECT_EQ(obj.getn(str.data(), str.size()), str.size() - 1);
        EXPECT_EQ(obj.tell(), str.size());
        EXPECT_EQ(str.substr(0, str.size() - 1), info.substr(1));
    };

    iochannel obj1{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj1);

    ichannel obj2{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj2);
}

TEST(Channel, GetnAfterATakeStartsAfterIt)
{
    using namespace IOv2;

    std::string info = "clear morning, a kettle on, and a page of notes";
    
    auto helper = [&info](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.bumpc(), info[0]);
        EXPECT_EQ(obj.tell(), 1);

        std::string str(info.size(), '\0');
        EXPECT_EQ(obj.getn(str.data(), 0), 0);
        EXPECT_EQ(obj.tell(), 1);

        EXPECT_EQ(obj.getn(str.data(), 1), 1);
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(str[0], info[1]);

        EXPECT_EQ(obj.bumpc(), info[2]);
        EXPECT_EQ(obj.tell(), 3);

        EXPECT_EQ(obj.getn(str.data(), str.size()), str.size() - 3);
        EXPECT_EQ(obj.tell(), str.size());
        EXPECT_EQ(str.substr(0, str.size() - 3), info.substr(3));
    };

    iochannel obj1{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj1);

    ichannel obj2{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj2);
}

TEST(Channel, GetnAfterAnAdvancingLook)
{
    using namespace IOv2;

    std::string info = "clear morning, a kettle on, and a page of notes";
    
    auto helper = [&info](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.nextc(), info[1]);
        EXPECT_EQ(obj.tell(), 1);

        std::string str(info.size(), '\0');
        EXPECT_EQ(obj.getn(str.data(), 0), 0);
        EXPECT_EQ(obj.tell(), 1);

        EXPECT_EQ(obj.getn(str.data(), 1), 1);
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(str[0], info[1]);

        EXPECT_EQ(obj.nextc(), info[3]);
        EXPECT_EQ(obj.tell(), 3);

        EXPECT_EQ(obj.getn(str.data(), str.size()), str.size() - 3);
        EXPECT_EQ(obj.tell(), str.size());
        EXPECT_EQ(str.substr(0, str.size() - 3), info.substr(3));
    };

    iochannel obj1{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj1);

    ichannel obj2{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj2);
}

TEST(Channel, PutbackReplacesTheCharacterTheNextReadWillSee)
{
    using namespace IOv2;
    
    auto helper = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        obj.putbackc('x');
        EXPECT_EQ(obj.tell(), 0);
        obj.putbackc('y');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.getc(), 'y');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.bumpc(), 'y');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.getc(), 'x');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.nextc(), 'a');
        EXPECT_EQ(obj.tell(), 0);
        obj.putbackc('1');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.bumpc(), '1');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.bumpc(), 'a');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.bumpc(), 'b');
        EXPECT_EQ(obj.tell(), 2);
        obj.putbackc('?');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.nextc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_FALSE((obj.nextc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        obj.putbackc('c');
        EXPECT_EQ(obj.tell(), 2);
        obj.putbackc('b');
        EXPECT_EQ(obj.tell(), 1);
        obj.putbackc('a');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.nextc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.getc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.nextc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.getc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.bumpc(), 'c');
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.nextc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.bumpc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.getc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
    };

    iochannel obj1{mem_device{"abc"}};
    helper(obj1);

    ichannel obj2{mem_device{"abc"}};
    helper(obj2);
}

TEST(Channel, PutcAndPutnAdvanceThePutPosition)
{
    using namespace IOv2;

    auto helper1 = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        obj.putc('x');
        EXPECT_EQ(obj.tell(), 1);
        obj.flush();
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.device().str(), "x");

        obj.putn("12345", 5);
        EXPECT_EQ(obj.tell(), 6);
        obj.flush();
        EXPECT_EQ(obj.tell(), 6);
        EXPECT_EQ(obj.device().str(), "x12345");
    };
    {
        iochannel obj1{mem_device{""}};
        helper1(obj1);

        ochannel obj2{mem_device{""}};
        helper1(obj2);
    }
    
    auto helper2 = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        obj.putc('x');
        EXPECT_EQ(obj.tell(), 1);
        obj.flush();
        EXPECT_EQ(obj.device().str(), "liwei: x");

        EXPECT_EQ(obj.tell(), 1);
        obj.putn("12345", 5);
        EXPECT_EQ(obj.tell(), 6);
        obj.flush();
        EXPECT_EQ(obj.device().str(), "liwei: x12345");
    };
    {
        mem_device dev{"liwei: "}; dev.drseek(0);
        iochannel obj1{std::move(dev)};
        helper2(obj1);

        mem_device dev2{"liwei: "}; dev2.drseek(0);
        ochannel obj2{std::move(dev2)};
        helper2(obj2);
    }
}

TEST(Channel, SeekingMovesBothDirections)
{
    using namespace IOv2;
    
    iochannel obj{mem_device{"abcde"}};

    EXPECT_EQ(obj.bumpc(), 'a');
    obj.putbackc('x');
    obj.seek(0);
    EXPECT_EQ(obj.bumpc(), 'a');
}

TEST(Channel, SwitchingToPutAfterAReadRepositions)
{
    using namespace IOv2;

    iochannel obj{mem_device{"abcde"}};

    EXPECT_EQ(obj.bumpc(), 'a');
    obj.putbackc('x');
    obj.switch_to_put();
    obj.switch_to_get();
    EXPECT_EQ(obj.bumpc(), 'a');
    EXPECT_EQ(obj.bumpc(), 'b');

    obj.putbackc('x');
    obj.switch_to_put();
    obj.putc('B');
    obj.flush();

    EXPECT_EQ(obj.device().str(), "aBcde");
}

TEST(Channel, SwitchingToGetAfterAWriteRepositions)
{
    using namespace IOv2;

    iochannel obj{mem_device{"abcde"}};

    EXPECT_EQ(obj.bumpc(), 'a');
    obj.putbackc('x');
    obj.switch_to_put();
    obj.switch_to_get();
    EXPECT_EQ(obj.bumpc(), 'a');
    EXPECT_EQ(obj.bumpc(), 'b');

    obj.putbackc('x');
    obj.putc('B');
    obj.flush();

    EXPECT_EQ(obj.device().str(), "aBcde");
}

TEST(Channel, SwitchingWithNothingBufferedCostsNothing)
{
    using namespace IOv2;

    iochannel obj{mem_device{"abcde"}};

    EXPECT_EQ(obj.bumpc(), 'a');
    EXPECT_EQ(obj.bumpc(), 'b');
    obj.putbackc('x');
    obj.putc('B');
    EXPECT_EQ(obj.bumpc(), 'c');
    EXPECT_EQ(obj.tell(), 3);
    obj.flush();

    EXPECT_EQ(obj.device().str(), "aBcde");
}

// A converter pipeline must be capable enough for the direction the channel has:
// bidirectional needs support_io_switch, input-only needs support_get, output-only needs
// support_put (io_concepts.h: cvt_fits_direction, enforced on base_channel's two
// creator-taking constructors). zlib can read and write but cannot switch direction; a hash
// can only write. Before the constraint existed all of these compiled, and the failure
// arrived far from the mistake: the bidirectional case set cvtfailbit on the first direction
// change, while the input-only hash case threw "only output mode is supported" from the
// constructor -- and a stream constructor runs outside any try block.
//
// The predicate must be applied to the type the creator produces, never to the runtime_cvt
// the buffer stores: runtime_cvt is a type-erasing wrapper that implements every interface
// and reports every capability as present, deferring the failure to a run-time throw.
TEST(Channel, SwitchingCarriesTheBufferedCharactersWithIt)
{
    using namespace IOv2;

    using Dev  = mem_device<char>;
    using Zlib = Comp::zlib_cvt_creator<char>;                  // get + put, no io_switch
    using Hash = Crypt::hash_cvt_creator<char>;                 // put only
    using Vig  = Crypt::Classic::vigenere_cvt_creator<char>;    // get + put + io_switch

    // Bidirectional: only a pipeline that can change direction is accepted.
    static_assert(!std::is_constructible_v<iochannel<Dev, char>, Dev, Zlib>);
    static_assert(!std::is_constructible_v<iochannel<Dev, char>, Dev, Hash>);
    static_assert( std::is_constructible_v<iochannel<Dev, char>, Dev, Vig>);

    // Input-only: needs support_get, which a hash pipeline does not have.
    static_assert(!std::is_constructible_v<ichannel<Dev, char>, Dev, Hash>);
    static_assert( std::is_constructible_v<ichannel<Dev, char>, Dev, Zlib>);
    static_assert( std::is_constructible_v<ichannel<Dev, char>, Dev, Vig>);

    // Output-only: needs support_put, which all three have.
    static_assert( std::is_constructible_v<ochannel<Dev, char>, Dev, Zlib>);
    static_assert( std::is_constructible_v<ochannel<Dev, char>, Dev, Hash>);
    static_assert( std::is_constructible_v<ochannel<Dev, char>, Dev, Vig>);

    // The constraint sits on the channel, and the stream layer inherits it: the stream
    // constructors mention decltype(iochannel{...}) in their own constraints, so a rejected
    // buffer removes the corresponding stream constructor instead of producing a hard error.
    static_assert(!std::is_constructible_v<iostream<Dev, char>, Dev, Zlib>);
    static_assert(!std::is_constructible_v<iostream<Dev, char>, Dev, Hash>);
    static_assert(!std::is_constructible_v<istream<Dev, char>,  Dev, Hash>);
    static_assert( std::is_constructible_v<ostream<Dev, char>,  Dev, Hash>);
    static_assert( std::is_constructible_v<istream<Dev, char>,  Dev, Zlib>);

    // An accepted bidirectional pipeline really does switch direction at run time.
    {
        iochannel<Dev, char> obj{Dev{""}, Vig{std::string("key")}};
        obj.putc('a');
        obj.putc('b');
        obj.flush();
        obj.switch_to_get();
        obj.seek(0);
        EXPECT_EQ(obj.bumpc(), 'a');
        EXPECT_EQ(obj.bumpc(), 'b');
    }
}


// The device-direction counterpart of the case above: what a device can do, with no converter in
// between, checked at both layers so the two must agree cell for cell. The channel half is
// the one worth having -- a channel is public API, so a caller using it directly never
// passes a stream's class constraint, and without it the mismatch reaches run time as the
// cvt_error runtime_cvt throws when init_cvt() asks a one-directional pipeline to switch.
namespace
{
// Neither device has any members, so both are nothrow-movable and satisfy io_device.
struct put_only_device
{
    using char_type = char;
    void dput(const char*, std::size_t) {}
    void dflush() {}
};

struct get_only_device
{
    using char_type = char;
    std::size_t dget(char*, std::size_t) { return 0; }
    bool deof() { return true; }
};

// std::is_constructible_v, which the case above uses, is not enough here: the streams carry
// their constraint on the *class*, so istream<put_only_device, char> is not a type at all and
// naming it inside is_constructible_v is a hard error rather than a false. The device has to
// stay a template parameter so the check happens under substitution.
template <template <typename, typename> class TStream, typename TDevice>
concept buildable_over = requires(TDevice dev) { TStream<TDevice, char>{std::move(dev)}; };
}

TEST(Channel, ADirectionalChannelOffersOnlyItsOwnOperations)
{
    using Get  = get_only_device;
    using Put  = put_only_device;
    using Both = IOv2::mem_device<char>;

    // input wants a readable device, at both layers
    static_assert( buildable_over<IOv2::ichannel, Get>);
    static_assert(!buildable_over<IOv2::ichannel, Put>);
    static_assert( buildable_over<IOv2::istream,    Get>);
    static_assert(!buildable_over<IOv2::istream,    Put>);

    // output wants a writable device
    static_assert( buildable_over<IOv2::ochannel, Put>);
    static_assert(!buildable_over<IOv2::ochannel, Get>);
    static_assert( buildable_over<IOv2::ostream,    Put>);
    static_assert(!buildable_over<IOv2::ostream,    Get>);

    // bidirectional wants both; one direction alone is not enough
    static_assert(!buildable_over<IOv2::iochannel,  Get>);
    static_assert(!buildable_over<IOv2::iochannel,  Put>);
    static_assert(!buildable_over<IOv2::iostream,   Get>);
    static_assert(!buildable_over<IOv2::iostream,   Put>);

    // positive control: a device that does both is accepted everywhere
    static_assert( buildable_over<IOv2::ichannel, Both>);
    static_assert( buildable_over<IOv2::ochannel, Both>);
    static_assert( buildable_over<IOv2::iochannel,  Both>);
    static_assert( buildable_over<IOv2::istream,    Both>);
    static_assert( buildable_over<IOv2::ostream,    Both>);
    static_assert( buildable_over<IOv2::iostream,   Both>);
}

TEST(Channel, DetachHandsBackTheDeviceAndLeavesTheBufferEmpty)
{
    using namespace IOv2;

    // detach() with a non-empty read buffer over a positionable converter:
    // the buffered/pushed-back lookahead is rewound (tell()+seek()) before the
    // device is handed back, so the returned device is positioned at the logical
    // read cursor.
    {
        iochannel obj{mem_device{"abcde"}};
        EXPECT_EQ(obj.getc(), 'a');   // fills the read buffer with a lookahead 'a'
        auto [dev, err] = obj.detach();
        EXPECT_FALSE(err);
        EXPECT_EQ(dev.str(), "abcde");

        // Re-reading from the returned device begins at the logical read cursor.
        ichannel again{std::move(dev)};
        EXPECT_EQ(again.bumpc(), 'a');
    }

    // Same, but on an ichannel and after consuming a couple of characters so
    // the physical cursor is genuinely ahead of the logical one.
    {
        ichannel obj{mem_device{"abcde"}};
        EXPECT_EQ(obj.bumpc(), 'a');
        EXPECT_EQ(obj.getc(), 'b');   // buffered lookahead 'b'
        EXPECT_EQ(obj.tell(), 1);
        auto [dev, err] = obj.detach();
        EXPECT_FALSE(err);
        ichannel again{std::move(dev)};
        EXPECT_EQ(again.bumpc(), 'b');
    }
}

TEST(Channel, DetachAfterAFailureStillHandsBackTheDevice)
{
    using namespace IOv2;

    // Build a zlib-compressed payload.
    std::string payload = "the quick brown fox jumps over the lazy dog";
    std::string comp;
    {
        ochannel ost{mem_device{""}, Comp::zlib_cvt_creator<char>{6}};
        ost.putn(payload.data(), payload.size());
        ost.flush();
        auto [dev, err] = ost.detach();
        EXPECT_FALSE(err);
        comp = dev.str();
        EXPECT_FALSE(comp.empty());
    }

    // detach() with a non-empty read buffer over a converter that does NOT
    // support positioning (zlib): the attempted rewind fails and is swallowed
    // on purpose (see base_channel::detach), so detach() still succeeds and
    // reports no error - the lookahead character is the accepted, unavoidable
    // loss for a non-positionable stream.
    {
        ichannel ichan{mem_device{comp}, Comp::zlib_cvt_creator<char>{6}};
        EXPECT_EQ(ichan.getc(), payload.front());  // buffers a lookahead char
        auto [dev, err] = ichan.detach();
        EXPECT_FALSE(err);
    }
}

// Moving an ochannel hands the whole converter pipeline over as one
// unique_ptr, so the source is left holding nothing at all -- no buffer, no
// converter, no device, and therefore no second destructor that could write.
// That is a different mechanism from the one a move of the converter itself
// goes through (where the moved-from device has to be told to stay quiet; see
// RootCvtStd.AMovedFromOutputRootFlushesNothingWhenItDies), and this pins that
// the channel layer really does hand over as a whole. Full buffering is
// what makes a stray write visible: __fpending() would move if the dying source
// flushed what had been handed to stdio.
TEST(ChannelChar, AMovedFromOchannelOverAStdDeviceWritesNothingWhenItDies)
{
    oguard<true>       g;
    stdout_full_buffer buffered;

    auto moved = []
    {
        using Buf = IOv2::ochannel<IOv2::std_device<STDOUT_FILENO>, char>;
        Buf src{IOv2::std_device<STDOUT_FILENO>{}};
        src.putn("hello", 5);
        src.flush();                                // into stdio's buffer, not to the fd
        EXPECT_EQ(__fpending(stdout), 5u);
        return Buf{std::move(src)};
    }();                                            // src dies here

    EXPECT_EQ(__fpending(stdout), 5u);              // nothing followed it out

    moved.putn(" world", 6);
    moved.flush();
    std::fflush(stdout);
    EXPECT_EQ(g.contents(), "hello world");         // the target owns all of it, once
}
