// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/cvt/root_cvt.h>

#include <support/injectable_device.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

using namespace IOv2;

namespace
{
    using Device = injectable_device<char>;
    using Cvt = rb_root_cvt<Device>;

    struct output_cvt
    {
        std::shared_ptr<Device::state> state;
        Cvt cvt;

        output_cvt()
            : output_cvt(Device{})
        {
        }

    private:
        explicit output_cvt(Device dev)
            : state(dev.shared_state())
            , cvt(std::move(dev))
        {
            EXPECT_EQ(cvt.bos(), io_status::output);
            cvt.main_cont_beg();
            cvt.put("pending", 7);
        }
    };
}

TEST(RootCvtDeviceFlush, AttachFlushesTheReplacedOutputDevice)
{
    output_cvt stream;

    EXPECT_EQ(stream.state->dput, 0u);
    EXPECT_EQ(stream.state->dflush, 0u);

    stream.cvt.attach(Device{});

    EXPECT_EQ(stream.state->dput, 1u);
    EXPECT_EQ(stream.state->dflush, 1u);
}

// Constructing a converter does not select a direction. Replacing that neutral
// device therefore has neither converter output nor a device buffer to flush.
TEST(RootCvtDeviceFlush, AttachDoesNotFlushADeviceBeforeBos)
{
    Device dev;
    auto state = dev.shared_state();
    Cvt cvt{std::move(dev)};

    cvt.attach(Device{});

    EXPECT_EQ(state->dput, 0u);
    EXPECT_EQ(state->dflush, 0u);
}

// A plain detach transfers the still-open device to the caller. It finalizes
// converter buffers, but device-level flushing remains the new owner's job.
TEST(RootCvtDeviceFlush, DetachDoesNotFlushTheReturnedDevice)
{
    output_cvt stream;

    auto [device, error] = stream.cvt.detach();

    EXPECT_FALSE(error);
    EXPECT_EQ(stream.state->dput, 1u);
    EXPECT_EQ(stream.state->dflush, 0u);

    device.dflush();
    EXPECT_EQ(stream.state->dflush, 1u);
}

// Converter cleanup happens before device flushing. Even when both fail,
// dflush must still be attempted and the earlier converter error must win.
TEST(RootCvtDeviceFlush, AttachPreservesTheEarlierErrorAndStillAttemptsDeviceFlush)
{
    output_cvt stream;
    stream.state->fail_dput = true;
    stream.state->fail_dflush = true;

    try
    {
        stream.cvt.attach(Device{});
        FAIL() << "attach should propagate the old converter flush error";
    }
    catch (const device_error& error)
    {
        EXPECT_STREQ(error.what(), "injectable_device::dput: forced failure");
    }

    EXPECT_EQ(stream.state->dput, 1u);
    EXPECT_EQ(stream.state->dflush, 1u);

    // root_cvt installs and resets the new device before rethrowing.
    EXPECT_EQ(stream.cvt.bos(), io_status::output);
}
