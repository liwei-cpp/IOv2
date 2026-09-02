// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * streambuf, the layer between a stream and its converter pipeline.
 *
 * The public surface is the sgetc / sbumpc / snextc / sgetn / sputc / sputn
 * family, and what the tests pin down is the position each of them leaves
 * behind: sgetc looks without advancing, sbumpc takes and advances, snextc
 * advances and then looks. Getting those three confused is the classic
 * streambuf bug, so every case checks tell() between calls rather than only the
 * character returned.
 *
 * The rest is about the direction machinery: which operations a get-only or
 * put-only buffer offers at all, what switching between them costs, and what
 * detach leaves behind.
 */
#include <cvt/comp/zlib_cvt.h>
#include <cvt/crypt/hash_cvt.h>
#include <cvt/crypt/vigenere_cvt.h>
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/streambuf.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

TEST(Streambuf, ABufferReportsWhichDirectionsItSupports)
{
    using namespace IOv2;

    {
        using CheckType = streambuf<mem_device<char>, char>;
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::char_type, char>);
    }

    {
        using CheckType = istreambuf<mem_device<char>, char>;
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::char_type, char>);
    }

    {
        using CheckType = ostreambuf<mem_device<char>, char>;
        static_assert(std::is_same_v<CheckType::device_type, mem_device<char>>);
        static_assert(std::is_same_v<CheckType::char_type, char>);
    }
}

TEST(Streambuf, WritingThroughSputcAndSputnLandsInOrder)
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

            obj.sputn(" world", 6);
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

            obj.sputn(" world", 6);
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
    helper(streambuf{dev});
    helper(ostreambuf{dev});
}

TEST(Streambuf, ReadingBackWhatWasWritten)
{
    using namespace IOv2;
    
    auto helper = [](const auto& ori_obj)
    {
        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            EXPECT_EQ(obj.sgetn(str.data(), 5), 5);
            EXPECT_EQ(str, "hello");
            EXPECT_EQ(obj.tell(), 5);

            auto obj2(obj);
            EXPECT_EQ(obj2.tell(), 5);
            str.resize(6);
            EXPECT_EQ(obj2.sgetn(str.data(), 6), 6);
            EXPECT_EQ(str, " world");
            EXPECT_EQ(obj2.tell(), 11);

            str = "xxxxxx";
            EXPECT_EQ(obj.sgetn(str.data(), 6), 6);
            EXPECT_EQ(str, " world");
            EXPECT_EQ(obj.tell(), 11);
        }

        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            EXPECT_EQ(obj.sgetn(str.data(), 5), 5);
            EXPECT_EQ(str, "hello");
            EXPECT_EQ(obj.tell(), 5);

            decltype(obj) obj2{mem_device("")};
            obj2 = obj;
            EXPECT_EQ(obj2.tell(), 5);
            str.resize(6);
            EXPECT_EQ(obj2.sgetn(str.data(), 6), 6);
            EXPECT_EQ(str, " world");
            EXPECT_EQ(obj2.tell(), 11);

            str = "xxxxxx";
            EXPECT_EQ(obj.sgetn(str.data(), 6), 6);
            EXPECT_EQ(str, " world");
            EXPECT_EQ(obj.tell(), 11);
        }

        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            EXPECT_EQ(obj.sgetn(str.data(), 5), 5);
            EXPECT_EQ(str, "hello");
            EXPECT_EQ(obj.tell(), 5);

            auto obj2(std::move(obj));
            EXPECT_EQ(obj2.tell(), 5);
            str.resize(6);
            EXPECT_EQ(obj2.sgetn(str.data(), 6), 6);
            EXPECT_EQ(str, " world");
            EXPECT_EQ(obj2.tell(), 11);
        }

        {
            auto obj = ori_obj;
            std::string str; str.resize(5);
            EXPECT_EQ(obj.sgetn(str.data(), 5), 5);
            EXPECT_EQ(str, "hello");
            EXPECT_EQ(obj.tell(), 5);

            decltype(obj) obj2{mem_device("")};
            obj2 = std::move(obj);
            EXPECT_EQ(obj2.tell(), 5);
            str.resize(6);
            EXPECT_EQ(obj2.sgetn(str.data(), 6), 6);
            EXPECT_EQ(str, " world");
            EXPECT_EQ(obj2.tell(), 11);
        }
    };

    helper(streambuf{mem_device("hello world")});
    helper(istreambuf{mem_device("hello world")});
}

TEST(Streambuf, SgetcLooksWhileSbumpcTakes)
{
    using namespace IOv2;

    auto helper = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sgetc(), 'a');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sgetc(), 'a');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sbumpc(), 'a');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.sgetc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.sgetc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.sbumpc(), 'b');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.sgetc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.sgetc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.sbumpc(), 'c');
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.sgetc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.sbumpc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
    };

    streambuf obj1{mem_device{"abc"}};
    helper(obj1);

    istreambuf obj2{mem_device{"abc"}};
    helper(obj2);
}

TEST(Streambuf, TheSameHoldsOnAGetOnlyBuffer)
{
    using namespace IOv2;

    auto helper = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sbumpc(), 'a');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.sgetc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.sgetc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.sbumpc(), 'b');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.sgetc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.sgetc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.sbumpc(), 'c');
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.sgetc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.sbumpc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
    };

    streambuf obj1{mem_device{"abc"}};
    helper(obj1);

    istreambuf obj2{mem_device{"abc"}};
    helper(obj2);
}

TEST(Streambuf, SnextcAdvancesBeforeItLooks)
{
    using namespace IOv2;

    auto helper = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sgetc(), 'a');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sgetc(), 'a');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.snextc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.sgetc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.snextc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.sgetc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.sbumpc(), 'c');
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.snextc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.sbumpc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.sgetc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
    };

    streambuf obj1{mem_device{"abc"}};
    helper(obj1);

    istreambuf obj2{mem_device{"abc"}};
    helper(obj2);
}

TEST(Streambuf, SnextcOnAGetOnlyBuffer)
{
    using namespace IOv2;

    auto helper = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.snextc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.sgetc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.snextc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.sgetc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.sbumpc(), 'c');
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.snextc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.sbumpc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.sgetc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
    };

    streambuf obj1{mem_device{"abc"}};
    helper(obj1);

    istreambuf obj2{mem_device{"abc"}};
    helper(obj2);
}

TEST(Streambuf, SgetnTakesExactlyTheCountAsked)
{
    using namespace IOv2;

    std::string info = "clear morning, a kettle on, and a page of notes";
    
    auto helper = [&info](auto& obj)
    {
        std::string str(info.size(), '\0');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sgetn(str.data(), 0), 0);
        EXPECT_EQ(obj.tell(), 0);

        EXPECT_EQ(obj.sgetn(str.data(), 1), 1);
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(str[0], info[0]);

        EXPECT_EQ(obj.sgetn(str.data(), str.size()), str.size() - 1);
        EXPECT_EQ(obj.tell(), str.size());
        EXPECT_EQ(str.substr(0, str.size() - 1), info.substr(1));
    };

    streambuf obj1{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj1);

    istreambuf obj2{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj2);
}

TEST(Streambuf, SgetnAfterALookAheadStartsWhereTheLookLeftOff)
{
    using namespace IOv2;

    std::string info = "clear morning, a kettle on, and a page of notes";
    
    auto helper = [&info](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sgetc(), info[0]);
        EXPECT_EQ(obj.tell(), 0);

        std::string str(info.size(), '\0');
        EXPECT_EQ(obj.sgetn(str.data(), 0), 0);
        EXPECT_EQ(obj.tell(), 0);

        EXPECT_EQ(obj.sgetn(str.data(), 1), 1);
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(str[0], info[0]);

        EXPECT_EQ(obj.sgetc(), info[1]);
        EXPECT_EQ(obj.tell(), 1);

        EXPECT_EQ(obj.sgetn(str.data(), str.size()), str.size() - 1);
        EXPECT_EQ(obj.tell(), str.size());
        EXPECT_EQ(str.substr(0, str.size() - 1), info.substr(1));
    };

    streambuf obj1{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj1);

    istreambuf obj2{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj2);
}

TEST(Streambuf, SgetnAfterATakeStartsAfterIt)
{
    using namespace IOv2;

    std::string info = "clear morning, a kettle on, and a page of notes";
    
    auto helper = [&info](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sbumpc(), info[0]);
        EXPECT_EQ(obj.tell(), 1);

        std::string str(info.size(), '\0');
        EXPECT_EQ(obj.sgetn(str.data(), 0), 0);
        EXPECT_EQ(obj.tell(), 1);

        EXPECT_EQ(obj.sgetn(str.data(), 1), 1);
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(str[0], info[1]);

        EXPECT_EQ(obj.sbumpc(), info[2]);
        EXPECT_EQ(obj.tell(), 3);

        EXPECT_EQ(obj.sgetn(str.data(), str.size()), str.size() - 3);
        EXPECT_EQ(obj.tell(), str.size());
        EXPECT_EQ(str.substr(0, str.size() - 3), info.substr(3));
    };

    streambuf obj1{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj1);

    istreambuf obj2{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj2);
}

TEST(Streambuf, SgetnAfterAnAdvancingLook)
{
    using namespace IOv2;

    std::string info = "clear morning, a kettle on, and a page of notes";
    
    auto helper = [&info](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.snextc(), info[1]);
        EXPECT_EQ(obj.tell(), 1);

        std::string str(info.size(), '\0');
        EXPECT_EQ(obj.sgetn(str.data(), 0), 0);
        EXPECT_EQ(obj.tell(), 1);

        EXPECT_EQ(obj.sgetn(str.data(), 1), 1);
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(str[0], info[1]);

        EXPECT_EQ(obj.snextc(), info[3]);
        EXPECT_EQ(obj.tell(), 3);

        EXPECT_EQ(obj.sgetn(str.data(), str.size()), str.size() - 3);
        EXPECT_EQ(obj.tell(), str.size());
        EXPECT_EQ(str.substr(0, str.size() - 3), info.substr(3));
    };

    streambuf obj1{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj1);

    istreambuf obj2{mem_device{"clear morning, a kettle on, and a page of notes"}};
    helper(obj2);
}

TEST(Streambuf, PutbackReplacesTheCharacterTheNextReadWillSee)
{
    using namespace IOv2;
    
    auto helper = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        obj.sputbackc('x');
        EXPECT_EQ(obj.tell(), 0);
        obj.sputbackc('y');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sgetc(), 'y');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sbumpc(), 'y');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sgetc(), 'x');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.snextc(), 'a');
        EXPECT_EQ(obj.tell(), 0);
        obj.sputbackc('1');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sbumpc(), '1');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.sbumpc(), 'a');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.sbumpc(), 'b');
        EXPECT_EQ(obj.tell(), 2);
        obj.sputbackc('?');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.snextc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_FALSE((obj.snextc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        obj.sputbackc('c');
        EXPECT_EQ(obj.tell(), 2);
        obj.sputbackc('b');
        EXPECT_EQ(obj.tell(), 1);
        obj.sputbackc('a');
        EXPECT_EQ(obj.tell(), 0);
        EXPECT_EQ(obj.snextc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.sgetc(), 'b');
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.snextc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.sgetc(), 'c');
        EXPECT_EQ(obj.tell(), 2);
        EXPECT_EQ(obj.sbumpc(), 'c');
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.snextc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.sbumpc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
        EXPECT_FALSE((obj.sgetc().has_value()));
        EXPECT_EQ(obj.tell(), 3);
    };

    streambuf obj1{mem_device{"abc"}};
    helper(obj1);

    istreambuf obj2{mem_device{"abc"}};
    helper(obj2);
}

TEST(Streambuf, SputcAndSputnAdvanceThePutPosition)
{
    using namespace IOv2;

    auto helper1 = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        obj.sputc('x');
        EXPECT_EQ(obj.tell(), 1);
        obj.flush();
        EXPECT_EQ(obj.tell(), 1);
        EXPECT_EQ(obj.device().str(), "x");

        obj.sputn("12345", 5);
        EXPECT_EQ(obj.tell(), 6);
        obj.flush();
        EXPECT_EQ(obj.tell(), 6);
        EXPECT_EQ(obj.device().str(), "x12345");
    };
    {
        streambuf obj1{mem_device{""}};
        helper1(obj1);

        ostreambuf obj2{mem_device{""}};
        helper1(obj2);
    }
    
    auto helper2 = [](auto& obj)
    {
        EXPECT_EQ(obj.tell(), 0);
        obj.sputc('x');
        EXPECT_EQ(obj.tell(), 1);
        obj.flush();
        EXPECT_EQ(obj.device().str(), "liwei: x");

        EXPECT_EQ(obj.tell(), 1);
        obj.sputn("12345", 5);
        EXPECT_EQ(obj.tell(), 6);
        obj.flush();
        EXPECT_EQ(obj.device().str(), "liwei: x12345");
    };
    {
        mem_device dev{"liwei: "}; dev.drseek(0);
        streambuf obj1{std::move(dev)};
        helper2(obj1);

        mem_device dev2{"liwei: "}; dev2.drseek(0);
        ostreambuf obj2{std::move(dev2)};
        helper2(obj2);
    }
}

TEST(Streambuf, SeekingMovesBothDirections)
{
    using namespace IOv2;
    
    streambuf obj{mem_device{"abcde"}};

    EXPECT_EQ(obj.sbumpc(), 'a');
    obj.sputbackc('x');
    obj.seek(0);
    EXPECT_EQ(obj.sbumpc(), 'a');
}

TEST(Streambuf, SwitchingToPutAfterAReadRepositions)
{
    using namespace IOv2;

    streambuf obj{mem_device{"abcde"}};

    EXPECT_EQ(obj.sbumpc(), 'a');
    obj.sputbackc('x');
    obj.switch_to_put();
    obj.switch_to_get();
    EXPECT_EQ(obj.sbumpc(), 'a');
    EXPECT_EQ(obj.sbumpc(), 'b');

    obj.sputbackc('x');
    obj.switch_to_put();
    obj.sputc('B');
    obj.flush();

    EXPECT_EQ(obj.device().str(), "aBcde");
}

TEST(Streambuf, SwitchingToGetAfterAWriteRepositions)
{
    using namespace IOv2;

    streambuf obj{mem_device{"abcde"}};

    EXPECT_EQ(obj.sbumpc(), 'a');
    obj.sputbackc('x');
    obj.switch_to_put();
    obj.switch_to_get();
    EXPECT_EQ(obj.sbumpc(), 'a');
    EXPECT_EQ(obj.sbumpc(), 'b');

    obj.sputbackc('x');
    obj.sputc('B');
    obj.flush();

    EXPECT_EQ(obj.device().str(), "aBcde");
}

TEST(Streambuf, SwitchingWithNothingBufferedCostsNothing)
{
    using namespace IOv2;

    streambuf obj{mem_device{"abcde"}};

    EXPECT_EQ(obj.sbumpc(), 'a');
    EXPECT_EQ(obj.sbumpc(), 'b');
    obj.sputbackc('x');
    obj.sputc('B');
    EXPECT_EQ(obj.sbumpc(), 'c');
    EXPECT_EQ(obj.tell(), 3);
    obj.flush();

    EXPECT_EQ(obj.device().str(), "aBcde");
}

// A converter pipeline must be capable enough for the direction the stream buffer has:
// bidirectional needs support_io_switch, input-only needs support_get, output-only needs
// support_put (io_concepts.h: cvt_fits_direction, enforced on base_streambuf's two
// creator-taking constructors). zlib can read and write but cannot switch direction; a hash
// can only write. Before the constraint existed all of these compiled, and the failure
// arrived far from the mistake: the bidirectional case set cvtfailbit on the first direction
// change, while the input-only hash case threw "only output mode is supported" from the
// constructor -- and a stream constructor runs outside any try block.
//
// The predicate must be applied to the type the creator produces, never to the runtime_cvt
// the buffer stores: runtime_cvt is a type-erasing wrapper that implements every interface
// and reports every capability as present, deferring the failure to a run-time throw.
TEST(Streambuf, SwitchingCarriesTheBufferedCharactersWithIt)
{
    using namespace IOv2;

    using Dev  = mem_device<char>;
    using Zlib = Comp::zlib_cvt_creator<char>;                  // get + put, no io_switch
    using Hash = Crypt::hash_cvt_creator<char>;                 // put only
    using Vig  = Crypt::Classic::vigenere_cvt_creator<char>;    // get + put + io_switch

    // Bidirectional: only a pipeline that can change direction is accepted.
    static_assert(!std::is_constructible_v<streambuf<Dev, char>, Dev, Zlib>);
    static_assert(!std::is_constructible_v<streambuf<Dev, char>, Dev, Hash>);
    static_assert( std::is_constructible_v<streambuf<Dev, char>, Dev, Vig>);

    // Input-only: needs support_get, which a hash pipeline does not have.
    static_assert(!std::is_constructible_v<istreambuf<Dev, char>, Dev, Hash>);
    static_assert( std::is_constructible_v<istreambuf<Dev, char>, Dev, Zlib>);
    static_assert( std::is_constructible_v<istreambuf<Dev, char>, Dev, Vig>);

    // Output-only: needs support_put, which all three have.
    static_assert( std::is_constructible_v<ostreambuf<Dev, char>, Dev, Zlib>);
    static_assert( std::is_constructible_v<ostreambuf<Dev, char>, Dev, Hash>);
    static_assert( std::is_constructible_v<ostreambuf<Dev, char>, Dev, Vig>);

    // The constraint sits on the stream buffer, and the stream layer inherits it: the stream
    // constructors mention decltype(streambuf{...}) in their own constraints, so a rejected
    // buffer removes the corresponding stream constructor instead of producing a hard error.
    static_assert(!std::is_constructible_v<iostream<Dev, char>, Dev, Zlib>);
    static_assert(!std::is_constructible_v<iostream<Dev, char>, Dev, Hash>);
    static_assert(!std::is_constructible_v<istream<Dev, char>,  Dev, Hash>);
    static_assert( std::is_constructible_v<ostream<Dev, char>,  Dev, Hash>);
    static_assert( std::is_constructible_v<istream<Dev, char>,  Dev, Zlib>);

    // An accepted bidirectional pipeline really does switch direction at run time.
    {
        streambuf<Dev, char> obj{Dev{""}, Vig{std::string("key")}};
        obj.sputc('a');
        obj.sputc('b');
        obj.flush();
        obj.switch_to_get();
        obj.seek(0);
        EXPECT_EQ(obj.sbumpc(), 'a');
        EXPECT_EQ(obj.sbumpc(), 'b');
    }
}


// The device-direction counterpart of the case above: what a device can do, with no converter in
// between, checked at both layers so the two must agree cell for cell. The stream-buffer half is
// the one worth having -- a stream buffer is public API, so a caller using it directly never
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

TEST(Streambuf, ADirectionalBufferOffersOnlyItsOwnOperations)
{
    using Get  = get_only_device;
    using Put  = put_only_device;
    using Both = IOv2::mem_device<char>;

    // input wants a readable device, at both layers
    static_assert( buildable_over<IOv2::istreambuf, Get>);
    static_assert(!buildable_over<IOv2::istreambuf, Put>);
    static_assert( buildable_over<IOv2::istream,    Get>);
    static_assert(!buildable_over<IOv2::istream,    Put>);

    // output wants a writable device
    static_assert( buildable_over<IOv2::ostreambuf, Put>);
    static_assert(!buildable_over<IOv2::ostreambuf, Get>);
    static_assert( buildable_over<IOv2::ostream,    Put>);
    static_assert(!buildable_over<IOv2::ostream,    Get>);

    // bidirectional wants both; one direction alone is not enough
    static_assert(!buildable_over<IOv2::streambuf,  Get>);
    static_assert(!buildable_over<IOv2::streambuf,  Put>);
    static_assert(!buildable_over<IOv2::iostream,   Get>);
    static_assert(!buildable_over<IOv2::iostream,   Put>);

    // positive control: a device that does both is accepted everywhere
    static_assert( buildable_over<IOv2::istreambuf, Both>);
    static_assert( buildable_over<IOv2::ostreambuf, Both>);
    static_assert( buildable_over<IOv2::streambuf,  Both>);
    static_assert( buildable_over<IOv2::istream,    Both>);
    static_assert( buildable_over<IOv2::ostream,    Both>);
    static_assert( buildable_over<IOv2::iostream,   Both>);
}

TEST(Streambuf, DetachHandsBackTheDeviceAndLeavesTheBufferEmpty)
{
    using namespace IOv2;

    // detach() with a non-empty read buffer over a positionable converter:
    // the buffered/pushed-back lookahead is rewound (tell()+seek()) before the
    // device is handed back, so the returned device is positioned at the logical
    // read cursor.
    {
        streambuf obj{mem_device{"abcde"}};
        EXPECT_EQ(obj.sgetc(), 'a');   // fills the read buffer with a lookahead 'a'
        auto [dev, err] = obj.detach();
        EXPECT_FALSE(err);
        EXPECT_EQ(dev.str(), "abcde");

        // Re-reading from the returned device begins at the logical read cursor.
        istreambuf again{std::move(dev)};
        EXPECT_EQ(again.sbumpc(), 'a');
    }

    // Same, but on an istreambuf and after consuming a couple of characters so
    // the physical cursor is genuinely ahead of the logical one.
    {
        istreambuf obj{mem_device{"abcde"}};
        EXPECT_EQ(obj.sbumpc(), 'a');
        EXPECT_EQ(obj.sgetc(), 'b');   // buffered lookahead 'b'
        EXPECT_EQ(obj.tell(), 1);
        auto [dev, err] = obj.detach();
        EXPECT_FALSE(err);
        istreambuf again{std::move(dev)};
        EXPECT_EQ(again.sbumpc(), 'b');
    }
}

TEST(Streambuf, DetachAfterAFailureStillHandsBackTheDevice)
{
    using namespace IOv2;

    // Build a zlib-compressed payload.
    std::string payload = "the quick brown fox jumps over the lazy dog";
    std::string comp;
    {
        ostreambuf ost{mem_device{""}, Comp::zlib_cvt_creator<char>{6}};
        ost.sputn(payload.data(), payload.size());
        ost.flush();
        auto [dev, err] = ost.detach();
        EXPECT_FALSE(err);
        comp = dev.str();
        EXPECT_FALSE(comp.empty());
    }

    // detach() with a non-empty read buffer over a converter that does NOT
    // support positioning (zlib): the attempted rewind fails and is swallowed
    // on purpose (see base_streambuf::detach), so detach() still succeeds and
    // reports no error - the lookahead character is the accepted, unavoidable
    // loss for a non-positionable stream.
    {
        istreambuf isb{mem_device{comp}, Comp::zlib_cvt_creator<char>{6}};
        EXPECT_EQ(isb.sgetc(), payload.front());  // buffers a lookahead char
        auto [dev, err] = isb.detach();
        EXPECT_FALSE(err);
    }
}
