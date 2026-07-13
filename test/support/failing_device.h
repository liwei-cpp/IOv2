#pragma once
#include <cstddef>
#include <string>
#include <utility>

#include <common/defs.h>
#include <device/mem_device.h>

// A minimal I/O device for tests that wraps a mem_device and can be told to
// make its dflush() throw device_error on demand. Used to exercise the
// flush-failure paths of out_sentry / ostream::flush().
template <class CharT>
class failing_device
{
public:
    using char_type = CharT;

public:
    failing_device(std::basic_string<CharT> info = std::basic_string<CharT>{},
                   bool fail_flush = false)
        : m_dev(std::move(info))
        , m_fail_flush(fail_flush)
    {}

    failing_device(const failing_device&) = default;
    failing_device(failing_device&&) noexcept = default;
    failing_device& operator=(const failing_device&) = default;
    failing_device& operator=(failing_device&&) noexcept = default;

    void set_fail_flush(bool f) { m_fail_flush = f; }
    const std::basic_string<CharT>& str() const { return m_dev.str(); }

public:
    // input side (needed by iostream)
    bool deof() const { return m_dev.deof(); }
    size_t dget(char_type* s, size_t n) { return m_dev.dget(s, n); }
    template <bool Saturate = false>
    auto get_buf(size_t to_max) { return m_dev.template get_buf<Saturate>(to_max); }
    void get_rollback(size_t len) { m_dev.get_rollback(len); }

    // positioning
    size_t dtell() const { return m_dev.dtell(); }
    size_t dsize() const { return m_dev.dsize(); }
    void dseek(size_t v) { m_dev.dseek(v); }
    void drseek(size_t offset) { m_dev.drseek(offset); }

    // output side
    void dput(const char_type* ch, size_t n) { m_dev.dput(ch, n); }
    CharT* put_buf(size_t len) { return m_dev.put_buf(len); }
    void put_rollback(size_t len) { m_dev.put_rollback(len); }

    void dflush()
    {
        if (m_fail_flush)
            throw IOv2::device_error("failing_device::dflush: forced failure");
    }

private:
    IOv2::mem_device<CharT> m_dev;
    bool m_fail_flush = false;
};
