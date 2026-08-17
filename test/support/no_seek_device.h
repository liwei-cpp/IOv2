#pragma once
#include <cstddef>
#include <string>
#include <utility>

#include <common/defs.h>
#include <device/mem_device.h>

// A bidirectional device that deliberately provides no positioning primitives, yielding a
// pipeline that can change direction yet cannot reposition -- the state switch_to_put() has to
// reject while the read buffer is non-empty. No fake converter kernel is needed: root_cvt gates
// tell()/seek()/rseek() behind dev_cpt::support_positioning but does not gate
// switch_to_get()/switch_to_put().
template <class CharT>
class no_seek_device
{
public:
    using char_type = CharT;

public:
    explicit no_seek_device(std::basic_string<CharT> info = std::basic_string<CharT>{})
        : m_dev(std::move(info))
    {}

    no_seek_device(const no_seek_device&) = default;
    no_seek_device(no_seek_device&&) noexcept = default;
    no_seek_device& operator=(const no_seek_device&) = default;
    no_seek_device& operator=(no_seek_device&&) noexcept = default;

    const std::basic_string<CharT>& str() const { return m_dev.str(); }

public:
    // input side
    bool deof() const { return m_dev.deof(); }
    std::size_t dget(char_type* s, std::size_t n) { return m_dev.dget(s, n); }

    // output side
    void dput(const char_type* ch, std::size_t n) { m_dev.dput(ch, n); }
    void dflush() { m_dev.dflush(); }

    // No dtell / dsize / dseek / drseek -- see the comment above.

private:
    IOv2::mem_device<CharT> m_dev;
};
