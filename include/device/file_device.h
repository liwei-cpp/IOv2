#pragma once
#include <common/defs.h>
#include <common/metafunctions.h>
#include <device/device_concepts.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <expected>
#include <limits>
#include <memory>
#include <string>

namespace IOv2
{
enum class file_open_flag : std::uint8_t
{
    none        = 0,
    binary      = (std::uint8_t)1 << 0,
    trunc       = (std::uint8_t)1 << 1,
    noreplace   = (std::uint8_t)1 << 2
};

constexpr file_open_flag operator | (file_open_flag v1, file_open_flag v2)
{
    return (file_open_flag)((std::uint8_t) v1 | (std::uint8_t)v2);
}

constexpr file_open_flag operator & (file_open_flag v1, file_open_flag v2)
{
    return (file_open_flag)((std::uint8_t) v1 & (std::uint8_t)v2);
}

constexpr file_open_flag operator ~ (file_open_flag v1)
{
    return (file_open_flag)(~((std::uint8_t) v1));
}

template <bool IsIn, bool IsOut, typename CharType>
class basic_file_device;

template <bool IsIn, bool IsOut, typename CharType>
    requires ((IsIn || IsOut) && (std::is_same_v<CharType, char> || std::is_same_v<CharType, char8_t>))
class basic_file_device<IsIn, IsOut, CharType>
{
public:
    using char_type = CharType;
    
private:
    struct file_deleter
    {
        void operator()(FILE* fp) const noexcept
        {
            if (fp) std::fclose(fp); // NOLINT(cppcoreguidelines-owning-memory)
        }
    };

public:
    basic_file_device() = default;

    explicit basic_file_device(const std::string& file_name, file_open_flag flags = file_open_flag::none)
    {
        const char* mode = fopen_mode(flags);
        m_file.reset(std::fopen(file_name.c_str(), mode));
        if (!m_file)
            throw device_error("cannot open file \"" + file_name + "\": " + std::strerror(errno));

        if (fseek_64(m_file.get(), 0, SEEK_END) != 0)
            throw device_error("cannot get file length for \"" + file_name + "\"");
        
        int64_t len = ftell_64(m_file.get());
        if (len < 0)
            throw device_error("cannot get file length for \"" + file_name + "\"");
        else if (static_cast<uint64_t>(len) > std::numeric_limits<size_t>::max())
            throw device_error("file too large for this platform");

        if (fseek_64(m_file.get(), 0, SEEK_SET) != 0)
            throw device_error("cannot reset the file for \"" + file_name + "\"");
              
        m_file_len = static_cast<size_t>(len);
    }

    static std::expected<basic_file_device, std::string> try_open(const std::string& file_name, file_open_flag flags = file_open_flag::none) noexcept
    {
        try {
            return basic_file_device(file_name, flags);
        } catch (const std::exception& e) {
            return std::unexpected(e.what());
        } catch (...) {
            return std::unexpected("unknown error occurred during try_open");
        }
    }
    
    basic_file_device(const basic_file_device&) = delete;
    basic_file_device& operator=(const basic_file_device&) = delete;
    
    basic_file_device(basic_file_device&&) noexcept = default;
    basic_file_device& operator=(basic_file_device&&) noexcept = default;
    
    ~basic_file_device() = default;

public:
    [[nodiscard]] bool deof() const
    {
        if (!is_open())
            return true;
        try
        {
            return (dtell() >= m_file_len);
        }
        catch (...)
        {
            return true;
        }
    }

    [[nodiscard]] bool is_open() const
    {
        return m_file != nullptr;
    }
    
    void close()
    {
        if (is_open())
        {
            std::exception_ptr ptr = nullptr;
            if constexpr (IsOut)
            {
                try {
                    dflush();
                } catch (...) {
                    ptr = std::current_exception();
                }
            }
            m_file.reset();
            m_file_len = 0;
            if (ptr) std::rethrow_exception(ptr);
        }
    }
    
public:
    size_t dget(CharType* s, size_t n)
        requires (IsIn)
    {
        if (s == nullptr && n > 0)
            throw device_error("file_device::dget fail: null buffer");
        if (n == 0) return 0;

        if (!is_open()) return 0;
        size_t count = std::fread(s, sizeof(CharType), n, m_file.get());
        if (count < n && std::ferror(m_file.get()))
            throw device_error("file_device::dget fail: read error");
        return count;
    }

    [[nodiscard]] size_t dtell() const
    {
        if (!is_open())
            throw device_error("file_device::dtell fail: file is closed");
        
        auto res = ftell_64(m_file.get());
        if (res < 0)
            throw device_error("file_device::dtell fail: got invalid position.");
        return static_cast<size_t>(res);
    }

    [[nodiscard]] size_t dsize() const
    {
        return m_file_len;
    }
    
    void dseek(size_t v)
    {
        if (!is_open())
            throw device_error("file_device::dseek fail: file is closed");
        if constexpr (!IsOut)
        {
            if (v > m_file_len)
                throw device_error("file_device::dseek fail: invalid parameter");
        }

        if (v > std::numeric_limits<int64_t>::max())
            throw device_error("file_device::dseek fail: position exceeds maximum seekable range");

        if (fseek_64(m_file.get(), static_cast<int64_t>(v), SEEK_SET) != 0)
            throw device_error("file_device::dseek fail: fseek invalid response");
    }

    void drseek(size_t offset)
    {
        if (!is_open())
            throw device_error("file_device::drseek fail: file is closed");

        if (offset > m_file_len)
            throw device_error("file_device::drseek fail: invalid parameter");
        const int64_t l_off = -static_cast<int64_t>(offset);

        if (fseek_64(m_file.get(), l_off, SEEK_END) != 0)
            throw device_error("file_device::drseek fail: fseek invalid response");
    }
    
    void dput(const CharType* ch, size_t n)
        requires (IsOut)
    {
        if (ch == nullptr && n > 0)
            throw device_error("file_device::dput fail: null buffer");
        if (!is_open())
            throw device_error("file_device::dput fail: file closed.");
        if (n == 0) return;
        if (std::fwrite(ch, sizeof(CharType), n, m_file.get()) != n)
            throw device_error("file_device::dput fail: partial success.");
        
        auto cur_pos = dtell();
        if (cur_pos > m_file_len)
            m_file_len = cur_pos;
    }
    
    void dflush()
        requires (IsOut)
    {
        if (!is_open()) return;
        if (std::fflush(m_file.get()) == EOF)
            throw device_error("file_device::dflush fail: fflush fail.");
    }

private:
    static const char* fopen_mode(file_open_flag flags)
    {
        using enum file_open_flag;
        
        file_open_flag checkres = flags & (trunc | binary | noreplace);
        
        if constexpr (IsIn && IsOut)
        {
            if (checkres == none)
                return "r+";
            else if (checkres == trunc)
                return "w+";
            else if (checkres == binary)
                return "r+b";
            else if (checkres == (trunc | binary))
                return "w+b";
            // noreplace (O_EXCL) implies creating a new file, so it's always empty.
            // Therefore, trunc is implicit in this mode.
            else if (checkres == noreplace || checkres == (trunc | noreplace))
                return "w+x";
            else if (checkres == (binary | noreplace) || checkres == (binary | trunc | noreplace))
                return "w+bx";
            else
                throw device_error("Invalid open mode combination for read-write file");
        }
        else if constexpr (IsIn)
        {
           if (checkres == none)
                return "r";
            else if (checkres == binary)
                return "rb";
            else
                throw device_error("Invalid open mode combination for read-only file");
        }
        else if constexpr (IsOut)
        {
            // In write-only mode, "w" already truncates by default if noreplace is not present.
            if (checkres == none || checkres == trunc)
                return "w";
            else if (checkres == binary || checkres == (trunc | binary))
                return "wb";
            // noreplace implies creation of a new file.
            else if (checkres == noreplace || checkres == (trunc | noreplace))
                return "wx";
            else if (checkres == (binary | noreplace) || checkres == (binary | trunc | noreplace))
                return "wbx";
            else
                throw device_error("Invalid open mode combination for write-only file");
        }
        else
        {
            static_assert(dependent_false_nttp_v<IsIn, IsOut>, "file device cannot open with neither in nor out mode");
        }
    }
    
private:
    static int fseek_64(FILE* stream, int64_t offset, int origin)
    {
#if defined(_WIN32)
        return _fseeki64(stream, offset, origin);
#else
        return fseeko(stream, offset, origin);
#endif
    }

    static int64_t ftell_64(FILE* stream)
    {
#if defined(_WIN32)
        return _ftelli64(stream);
#else
        return ftello(stream);
#endif
    }

private:
    std::unique_ptr<FILE, file_deleter> m_file;
    size_t m_file_len = 0;
};

template <typename CharType>
using ifile_device = basic_file_device<true, false, CharType>;

template <typename CharType>
using ofile_device = basic_file_device<false, true, CharType>;

template <typename CharType>
using file_device = basic_file_device<true, true, CharType>;
}
