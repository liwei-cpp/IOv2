#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include <common/defs.h>
#include <device/mem_device.h>

// A bidirectional device wrapping a mem_device that can be told, at run time, to make any one of
// its primitives throw device_error, and that counts every call.
//
// The counts are what make ordering testable: injecting a dseek failure alone cannot be told
// apart from failing before dseek was ever reached. They share a block with the switches because
// a stream takes its device by value, so the test needs a handle on the stream's copy.
template <class CharT>
class injectable_device
{
public:
    using char_type = CharT;

    struct state
    {
        // call counts
        std::size_t dget = 0;
        std::size_t dput = 0;
        std::size_t dflush = 0;
        std::size_t dtell = 0;
        std::size_t dseek = 0;
        std::size_t drseek = 0;

        // injection switches
        bool fail_dget = false;
        bool fail_dput = false;
        bool fail_dflush = false;
        bool fail_dtell = false;
        bool fail_dseek = false;
        bool fail_drseek = false;
    };

public:
    explicit injectable_device(std::basic_string<CharT> info = std::basic_string<CharT>{})
        : m_dev(std::move(info))
        , m_state(std::make_shared<state>())
    {}

    injectable_device(const injectable_device&) = default;
    injectable_device(injectable_device&&) noexcept = default;
    injectable_device& operator=(const injectable_device&) = default;
    injectable_device& operator=(injectable_device&&) noexcept = default;

    // Shared with every copy, so it stays reachable after the device is moved into a stream.
    const std::shared_ptr<state>& shared_state() const { return m_state; }
    const std::basic_string<CharT>& str() const { return m_dev.str(); }

public:
    // input side
    bool deof() const { return m_dev.deof(); }

    std::size_t dget(char_type* s, std::size_t n)
    {
        ++m_state->dget;
        if (m_state->fail_dget)
            throw IOv2::device_error("injectable_device::dget: forced failure");
        return m_dev.dget(s, n);
    }

    // output side
    void dput(const char_type* ch, std::size_t n)
    {
        ++m_state->dput;
        if (m_state->fail_dput)
            throw IOv2::device_error("injectable_device::dput: forced failure");
        m_dev.dput(ch, n);
    }

    void dflush()
    {
        ++m_state->dflush;
        if (m_state->fail_dflush)
            throw IOv2::device_error("injectable_device::dflush: forced failure");
        m_dev.dflush();
    }

    // positioning
    std::size_t dtell() const
    {
        ++m_state->dtell;
        if (m_state->fail_dtell)
            throw IOv2::device_error("injectable_device::dtell: forced failure");
        return m_dev.dtell();
    }

    std::size_t dsize() const { return m_dev.dsize(); }

    void dseek(std::size_t v)
    {
        ++m_state->dseek;
        if (m_state->fail_dseek)
            throw IOv2::device_error("injectable_device::dseek: forced failure");
        m_dev.dseek(v);
    }

    void drseek(std::size_t offset)
    {
        ++m_state->drseek;
        if (m_state->fail_drseek)
            throw IOv2::device_error("injectable_device::drseek: forced failure");
        m_dev.drseek(offset);
    }

private:
    IOv2::mem_device<CharT> m_dev;
    std::shared_ptr<state> m_state;
};
