// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same attach() contract as test_istream_attach_char.cpp for wchar_t: the
 * state bits belong to the device that set them, and replacing the device
 * clears them.  The failure path needs a file device and stays in the narrow
 * file; what this instantiation adds is that the clearing is the stream's
 * behaviour rather than a property of the character type.
 */
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/char_and_str.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

TEST(IstreamAttachWchar, AttachClearsEndOfFile)
{
    auto expect_cleared = []<template <typename, typename> class T>()
    {
        T is{mem_device{std::wstring(L"ab")}};

        std::wstring tok;
        is >> tok;
        EXPECT_EQ(tok, L"ab");
        EXPECT_TRUE(is.eof());

        is.attach(mem_device{std::wstring(L"cd")});
        EXPECT_EQ(is.rdstate(), ios_defs::goodbit);

        std::wstring tok2;
        is >> tok2;
        EXPECT_EQ(tok2, L"cd");
    };

    expect_cleared.operator()<istream>();
    expect_cleared.operator()<iostream>();
}
