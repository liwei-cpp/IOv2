// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Callbacks registered on an output stream and run when its locale changes.
 *
 * [ios.base.callback] fixes two things: every registered callback is called,
 * and they are called in the opposite order of registration. IOv2 hands each
 * one the new locale and the pword currently stored under the id it was
 * registered with, and takes what it returns as the new pword -- so the same
 * mechanism inserts, replaces and erases pword entries depending on what comes
 * back, and each of those three answers is checked here.
 *
 * A callback that throws must not be able to stop the others: the first
 * exception is held, the remaining callbacks still run, and the stream is left
 * failed rather than half-imbued.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace IOv2;

namespace
{
    using callback_t =
        std::function<std::shared_ptr<void>(const locale<char>&, std::shared_ptr<void>)>;

    // A callback that records that it ran and leaves the pword as it found it.
    callback_t recorder(std::vector<char>& log, char tag)
    {
        return callback_t{[&log, tag](const locale<char>&, std::shared_ptr<void> pword) {
            log.push_back(tag);
            return pword;
        }};
    }
}

TEST(OstreamCallbacks, CallbacksRunInTheOppositeOrderOfRegistration)
{
    auto helper = []<typename T>()
    {
        std::vector<char> log;

        T os;
        os.register_callback(recorder(log, 'a'), 1);
        os.register_callback(recorder(log, 'b'), 1);
        os.register_callback(recorder(log, 'c'), 1);

        os.locale(locale<char>("C"));

        EXPECT_EQ(log, (std::vector<char>{'c', 'b', 'a'}));
    };

    helper.operator()<ostream<mem_device<char>, char>>();
    helper.operator()<iostream<mem_device<char>, char>>();
}

// The synchronized view reaches the same registry, so registering through it
// and imbuing through it behave exactly as above.
TEST(OstreamCallbacks, TheSyncViewReachesTheSameRegistry)
{
    auto helper = []<typename T>()
    {
        std::vector<char> log;

        T os;
        IOv2::sync(os).stream.register_callback(recorder(log, 'a'), 1);
        IOv2::sync(os).stream.register_callback(recorder(log, 'b'), 1);
        IOv2::sync(os).stream.register_callback(recorder(log, 'c'), 1);

        IOv2::sync(os).stream.locale(locale<char>("C"));

        EXPECT_EQ(log, (std::vector<char>{'c', 'b', 'a'}));
    };

    helper.operator()<ostream<mem_device<char>, char>>();
    helper.operator()<iostream<mem_device<char>, char>>();
}

// insert path: the id has no pword yet, the callback returns new data.
TEST(OstreamCallbacks, WhatACallbackReturnsBecomesThePwordForItsId)
{
    ostream<mem_device<char>, char> os{mem_device<char>{""}};

    auto data = std::make_shared<int>(42);
    os.register_callback(callback_t{[data](const locale<char>&, std::shared_ptr<void>)
                                    { return data; }},
                         5);

    os.locale(locale<char>("C"));
    EXPECT_EQ(os.get_pword(5), data);
}

// replace path: the id already has a pword, the callback returns different data.
TEST(OstreamCallbacks, ACallbackCanReplaceThePwordItWasHanded)
{
    ostream<mem_device<char>, char> os{mem_device<char>{""}};

    auto old_data = std::make_shared<int>(1);
    auto new_data = std::make_shared<int>(2);
    os.set_pword(5, old_data);
    os.register_callback(callback_t{[new_data](const locale<char>&, std::shared_ptr<void>)
                                    { return new_data; }},
                         5);

    os.locale(locale<char>("C"));
    EXPECT_EQ(os.get_pword(5), new_data);
}

// erase path: the id already has a pword, the callback returns nullptr.
TEST(OstreamCallbacks, ACallbackThatReturnsNothingErasesThePword)
{
    ostream<mem_device<char>, char> os{mem_device<char>{""}};

    os.set_pword(5, std::make_shared<int>(1));
    os.register_callback(callback_t{[](const locale<char>&, std::shared_ptr<void>)
                                    { return std::shared_ptr<void>{}; }},
                         5);

    os.locale(locale<char>("C"));
    EXPECT_EQ(os.get_pword(5), nullptr);
}

// access_callbacks() captures the first exception and rethrows it after all
// callbacks have run; the second throwing callback exercises the
// already-have-an-exception branch. locale() routes the rethrown exception
// through handle_exception(), leaving the stream failed.
TEST(OstreamCallbacks, AThrowingCallbackDoesNotStopTheOthersAndLeavesTheStreamFailed)
{
    ostream<mem_device<char>, char> os{mem_device<char>{""}};

    std::vector<char> log;
    os.register_callback(callback_t{[&log](const locale<char>&, std::shared_ptr<void>)
                                        -> std::shared_ptr<void> {
                             log.push_back('a');
                             throw stream_error("cb boom 1");
                         }},
                         5);
    os.register_callback(callback_t{[&log](const locale<char>&, std::shared_ptr<void>)
                                        -> std::shared_ptr<void> {
                             log.push_back('b');
                             throw stream_error("cb boom 2");
                         }},
                         5);

    EXPECT_NO_THROW(os.locale(locale<char>("C")));

    EXPECT_EQ(log, (std::vector<char>{'b', 'a'}));
    EXPECT_FALSE(static_cast<bool>(os));
    EXPECT_TRUE(os.str_fail());
}
