// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The same mixed read/write script driven through mem_device and through a real file_device,
 * compared byte for byte.
 *
 * Every other iostream case runs on mem_device, which keeps one contiguous buffer and can answer
 * any read from it. file_device sits on a real FILE* whose read and write positions are the same
 * cursor, so a direction change has to be flushed or repositioned to stay correct. That is the
 * one place the two can disagree, and nothing else in the suite would notice: a bug there shows
 * up only as different bytes on disk, with both streams reporting good().
 *
 * The scripts are the three from the round-12 probe -- read 4 then write 2, write 3 then read 2,
 * and an r,r,w,w,r,r,w,r alternation -- plus a large write whose landing offset is checked.
 * tell() is compared against the model position after every step, so a divergence is pinned to
 * the operation that caused it rather than to the final contents.
 */
#include <IOv2/device/file_device.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <support/file_guard.h>

#include <gtest/gtest.h>

using namespace IOv2;

namespace
{
// One step of a script: read n characters, or write this text.
struct step
{
    enum kind { read, write } what;
    std::size_t n = 0;
    std::string text;

    static step reading(std::size_t count) { return {read, count, {}}; }
    static step writing(std::string s) { return {write, 0, std::move(s)}; }
};

// What a script produced: the characters it read back, and the stream's tell() after each step.
struct trace
{
    std::string                             got;
    std::vector<std::optional<std::size_t>> positions;
    bool                                    good = false;
};

// The scripts below start from this, so a read has something to return and a write has something
// to overwrite. Position-bearing so a wrong offset shows as wrong content, not just wrong length.
std::string seed(std::size_t n)
{
    std::string s;
    s.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        s.push_back(static_cast<char>('a' + i % 26));
    return s;
}

template <typename TStream>
trace run(TStream& io, const std::vector<step>& script)
{
    trace t;
    for (const step& s : script)
    {
        if (s.what == step::read)
        {
            std::string buf(s.n, '\0');
            io.read(buf.data(), s.n);
            t.got += buf;
        }
        else
        {
            io.write(s.text.data(), s.text.size());
        }
        t.positions.push_back(io.tell());
    }
    io.flush();
    t.good = io.good();
    return t;
}

void expect_same_trace(const trace& mem, const trace& file, const char* tag)
{
    EXPECT_EQ(mem.good, file.good) << tag;
    EXPECT_EQ(mem.got, file.got) << tag;
    ASSERT_EQ(mem.positions.size(), file.positions.size()) << tag;
    for (std::size_t i = 0; i < mem.positions.size(); ++i)
        EXPECT_EQ(mem.positions[i], file.positions[i]) << tag << " after step " << i;
}
}

TEST(IostreamDeviceParity, TheThreeMixedScriptsLandTheSameBytes)
{
    const std::string start = seed(64);

    const std::vector<std::pair<const char*, std::vector<step>>> scripts = {
        {"read4-write2", {step::reading(4), step::writing("XY")}},
        {"write3-read2", {step::writing("XYZ"), step::reading(2)}},
        {"alternating",  {step::reading(1), step::reading(1), step::writing("W"),
                          step::writing("V"), step::reading(1), step::reading(1),
                          step::writing("U"), step::reading(1)}},
    };

    for (const auto& [tag, script] : scripts)
    {
        std::string mem_bytes;
        trace       mem_trace;
        {
            iostream io{mem_device{start}};
            ASSERT_TRUE(static_cast<bool>(io)) << tag;
            mem_trace = run(io, script);
            auto [dev, err] = io.detach();
            mem_bytes = dev.str();
        }

        const std::string path = std::string("iostream_parity_") + tag + ".bin";
        file_guard        guard(path, start);
        trace             file_trace;
        {
            iostream io{file_device<char>{path}};
            ASSERT_TRUE(static_cast<bool>(io)) << tag;
            file_trace = run(io, script);
        }

        expect_same_trace(mem_trace, file_trace, tag);
        EXPECT_EQ(mem_bytes, guard.contents()) << tag;

        // Non-vacuity: two devices that both read nothing and wrote nothing would agree too.
        EXPECT_FALSE(mem_trace.got.empty()) << tag;
        EXPECT_EQ(mem_bytes.size(), start.size()) << tag;
        EXPECT_NE(mem_bytes, start) << tag;
    }
}

TEST(IostreamDeviceParity, ALargeWriteLandsAtTheSameOffsetOnBothDevices)
{
    // Past any internal buffer, so the write is split across refills on at least one of the two.
    constexpr std::size_t skip = 5;
    const std::string     start = seed(64);
    const std::string     payload(100000, 'Z');

    const std::vector<step> script = {step::reading(skip), step::writing(payload)};

    std::string mem_bytes;
    trace       mem_trace;
    {
        iostream io{mem_device{start}};
        mem_trace = run(io, script);
        auto [dev, err] = io.detach();
        mem_bytes = dev.str();
    }

    const std::string path = "iostream_parity_large.bin";
    file_guard        guard(path, start);
    trace             file_trace;
    {
        iostream io{file_device<char>{path}};
        file_trace = run(io, script);
    }

    expect_same_trace(mem_trace, file_trace, "large");
    EXPECT_EQ(mem_bytes, guard.contents());

    // And the payload really did land after the five characters that were read, rather than at
    // the start or appended at the end.
    ASSERT_GE(mem_bytes.size(), skip + payload.size());
    EXPECT_EQ(mem_bytes.compare(0, skip, start, 0, skip), 0);
    EXPECT_EQ(mem_bytes.compare(skip, payload.size(), payload), 0);
}
