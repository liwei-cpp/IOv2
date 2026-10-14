// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <support/platform.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    namespace sys = iov2_test::sys;

    // Redirects stdin from a pipe whose write end stays open until close_write().
    // Unlike iguard (a regular file, where poll() always reports ready and read()
    // never waits), this reproduces a stream that has data but has not ended --
    // the only way to catch an operation that waits on input it was not asked for.
    struct pipe_iguard
    {
        explicit pipe_iguard(const std::string& buf)
        {
            if (sys::pipe(m_fds) != 0)
                throw std::runtime_error("Cannot create pipe");

            m_old_stdin = sys::dup(sys::stdin_fd);
            if (m_old_stdin == -1 || sys::dup2(m_fds[0], sys::stdin_fd) == -1)
                throw std::runtime_error("Cannot re-direct stdin");

            feed(buf);
        }

        // Appends more input. Blocks while the pipe is full, i.e. until the reader
        // has consumed enough of what was fed before.
        void feed(const std::string& buf)
        {
            if (!buf.empty() && sys::write(m_fds[1], buf.c_str(), buf.size())
                                    != static_cast<long long>(buf.size()))
                throw std::runtime_error("Cannot fill pipe");
        }

        void close_write()
        {
            if (m_fds[1] != -1) { sys::close(m_fds[1]); m_fds[1] = -1; }
        }

        ~pipe_iguard()
        {
            close_write();
            sys::dup2(m_old_stdin, sys::stdin_fd);
            sys::close(m_old_stdin);
            sys::close(m_fds[0]);
        }

        pipe_iguard(const pipe_iguard&) = delete;
        pipe_iguard& operator=(const pipe_iguard&) = delete;

    private:
        int m_fds[2] = {-1, -1};
        int m_old_stdin = -1;
    };

    struct iguard
    {
        iguard(const std::string& buf)
        {
            std::filesystem::remove("stdio_input_guard");
            {
                std::ofstream t("stdio_input_guard");
                t.write(buf.c_str(), buf.size());
            }
            
            m_old_stdin = sys::dup(sys::stdin_fd);
            if (!sys::reopen("stdio_input_guard", "r", stdin))
                throw std::runtime_error("Cannot re-direct stdin");
            setvbuf(stdin, nullptr, _IONBF, 0);
        }

        ~iguard()
        {
            fflush(stdin);
            sys::dup2(m_old_stdin, sys::stdin_fd);
            sys::close(m_old_stdin);
            setvbuf(stdin, nullptr, _IONBF, 0);
            std::filesystem::remove("stdio_input_guard");
        }

    private:
        int         m_old_stdin;
    };
    
    template <bool isOut>
    struct oguard
    {
        oguard()
        {
            m_dir_file_name = isOut ? "stdio_output_guard" : "stdio_outerr_guard";
            std::filesystem::remove(m_dir_file_name);
            
            if constexpr (isOut)
            {
                m_old_std = sys::dup(sys::stdout_fd);
                if (!sys::reopen(m_dir_file_name, "w", stdout))
                    throw std::runtime_error("Cannot re-direct stdout");
                setvbuf(stdout, nullptr, _IONBF, 0);
            }
            else
            {
                m_old_std = sys::dup(sys::stderr_fd);
                if (!sys::reopen(m_dir_file_name, "w", stderr))
                    throw std::runtime_error("Cannot re-direct stderr");
                setvbuf(stderr, nullptr, _IONBF, 0);
            }
        }
        
        ~oguard()
        {
            if constexpr (isOut)
            {
                fflush(stdout);
                sys::dup2(m_old_std, sys::stdout_fd);
                sys::close(m_old_std);
                setvbuf(stdout, nullptr, _IONBF, 0);
            }
            else
            {
                fflush(stderr);
                sys::dup2(m_old_std, sys::stderr_fd);
                sys::close(m_old_std);
                setvbuf(stderr, nullptr, _IONBF, 0);
            }
            std::filesystem::remove(m_dir_file_name);
        }

        std::string contents() const
        {
            std::ifstream t(m_dir_file_name);
            std::stringstream buffer;
            buffer << t.rdbuf();
            return buffer.str();
        }
    private:
        int         m_old_std;
        const char* m_dir_file_name;
    };

    // Puts stdout (isOut) or stderr into full buffering on a buffer this object
    // owns, the way glibc buffers a stdout that is not a tty -- stderr is never
    // buffered by default, so for it this is the shape a caller's own setvbuf
    // creates. The destructor hands the stream back to unbuffered mode (what
    // oguard and the surrounding suites expect) before the buffer dies, so an
    // early return from a failed ASSERT cannot leave the stream pointing at freed
    // storage.
    template <bool isOut>
    struct full_buffer
    {
        full_buffer()
        {
            if (std::setvbuf(stream(), m_buffer.data(), _IOFBF, m_buffer.size()) != 0)
                throw std::runtime_error("Cannot make the standard stream fully buffered");
        }

        ~full_buffer()
        {
            std::clearerr(stream());
            std::setvbuf(stream(), nullptr, _IONBF, 0);
            std::clearerr(stream());
        }

        full_buffer(const full_buffer&) = delete;
        full_buffer& operator=(const full_buffer&) = delete;

    private:
        static FILE* stream() { return isOut ? stdout : stderr; }

        std::array<char, BUFSIZ> m_buffer{};
    };

    using stdout_full_buffer = full_buffer<true>;
    using stderr_full_buffer = full_buffer<false>;
}