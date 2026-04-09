#pragma once
#include <unistd.h>
#include <cstdio>
#include <cerrno>
#include <poll.h>
#include <stdexcept>
#include <common/defs.h>
#include <device/device_concepts.h>

namespace IOv2
{
template <int ID>
    requires ((ID == STDIN_FILENO) || (ID == STDOUT_FILENO) || (ID == STDERR_FILENO))
class std_device
{
public:
    using char_type = char;

    std_device() = default;

    ~std_device()
    {
        if constexpr ((ID == STDOUT_FILENO) || (ID == STDERR_FILENO))
        {
            try {
                dflush();
            } catch (...) {}
        }
    }

    bool deos() const
    {
        return (ID != STDIN_FILENO) || m_eof_hit;
    }

    size_t dget(char* s, size_t n)
        requires (ID == STDIN_FILENO)
    {
        if (n == 0 || m_eof_hit) return 0;
        ssize_t ret;
        while (true)
        {
            ret = read(ID, s, n);
            if (ret != -1) break;

            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                struct pollfd pfd{ .fd = ID, .events = POLLIN, .revents = 0 };
                if (poll(&pfd, 1, -1) == -1)
                {
                    if (errno == EINTR) continue;
                    throw device_error("std_device::dget fail: poll error");
                }
                continue;
            }
            throw device_error("std_device::dget fail: read error");
        }

        if (ret == 0)
            m_eof_hit = true;

        return static_cast<size_t>(ret);
    }

    void dput(const char* ch, size_t n)
        requires ((ID == STDOUT_FILENO) || (ID == STDERR_FILENO))
    {
        bool put_res = false;
        if constexpr (ID == STDOUT_FILENO)
            put_res = (std::fwrite(ch, sizeof(char), n, stdout) == n);
        else
            put_res = (std::fwrite(ch, sizeof(char), n, stderr) == n);

        if (!put_res)
            throw device_error("std_device::dput fail: partial success.");
    }
    
    void dflush()
        requires ((ID == STDOUT_FILENO) || (ID == STDERR_FILENO))
    {
        bool flush_res = false;
        if constexpr (ID == STDOUT_FILENO)
            flush_res = (std::fflush(stdout) != EOF);
        else
            flush_res = (std::fflush(stderr) != EOF);

        if (!flush_res)
            throw device_error("std_device::dflush fail: fflush failed.");
    }

private:
    bool m_eof_hit = false;
};
}