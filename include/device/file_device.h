/**
 * @file file_device.h
 * @lang{ZH}
 * 定义了 `basic_file_device` 类，这是一个 I/O 设备，它封装了 C 标准库的 `FILE` 操作。
 * 这个设备提供了对文件进行读、写和定位的功能。
 * @endif
 *
 * @lang{EN}
 * Defines the `basic_file_device` class, an I/O device that wraps the C standard library `FILE` operations.
 * This device provides functionality for reading, writing, and seeking within files.
 * @endif
 */
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
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief `fopen` 的文件打开模式标志。
 *
 * 这个枚举类提供了一组标志，用于以类型安全的方式指定文件打开模式，
 * 例如二进制模式、截断模式等。
 * @endif
 *
 * @lang{EN}
 * @brief File opening mode flags for `fopen`.
 *
 * This enum class provides a set of flags to specify file opening modes
 * in a type-safe manner, such as binary mode, truncation, etc.
 * @endif
 */
enum class file_open_flag : std::uint8_t
{
    none        = 0,                            ///< @lang{ZH} 无特殊标志。 @endif @lang{EN} No special flags. @endif
    binary      = (std::uint8_t)1 << 0,         ///< @lang{ZH} 以二进制模式打开文件。 @endif @lang{EN} Open the file in binary mode. @endif
    trunc       = (std::uint8_t)1 << 1,         ///< @lang{ZH} 如果文件已存在，则截断它。 @endif @lang{EN} Truncate the file if it already exists. @endif
    noreplace   = (std::uint8_t)1 << 2          ///< @lang{ZH} 如果文件已存在，则打开失败。 @endif @lang{EN} Fail to open if the file already exists. @endif
};

/// @cond
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
/// @endcond

template <bool IsIn, bool IsOut, typename CharType>
class basic_file_device;

/**
 * @lang{ZH}
 * @brief 基于 C `FILE*` 的文件设备模板。
 *
 * 封装了 C 标准 I/O 库，提供了一个 RAII 风格的接口来管理文件。
 * 它可以通过模板参数配置为只读、只写或读写模式。
 *
 * @note 目前仅支持 Linux 系统。
 * @note 此类不是线程安全的，多线程并发由更高层次的代码处理。
 *
 * @tparam IsIn 指定设备是否支持输入（读取）。
 * @tparam IsOut 指定设备是否支持输出（写入）。
 * @tparam CharType 字符类型，当前支持 `char` 和 `char8_t`。
 * @endif
 *
 * @lang{EN}
 * @brief A file device template based on the C `FILE*`.
 *
 * This class wraps the C standard I/O library, providing an RAII-style interface for managing files.
 * It can be configured as read-only, write-only, or read-write via template parameters.
 *
 * @note Currently, only Linux is supported.
 * @note This class is not thread-safe; multi-threading is handled at a higher level.
 *
 * @tparam IsIn Specifies if the device supports input (reading).
 * @tparam IsOut Specifies if the device supports output (writing).
 * @tparam CharType The character type, currently supports `char` and `char8_t`.
 * @endif
 */
template <bool IsIn, bool IsOut, typename CharType>
    requires ((IsIn || IsOut) && (std::is_same_v<CharType, char> || std::is_same_v<CharType, char8_t>))
class basic_file_device<IsIn, IsOut, CharType>
{
public:
    using char_type = CharType;

private:
    /**
     * @lang{ZH}
     * @brief 用于 `std::unique_ptr` 的 `FILE` 删除器。
     *
     * 当 `unique_ptr` 销毁时，这个删除器会调用 `std::fclose` 来确保文件被关闭。
     * @endif
     *
     * @lang{EN}
     * @brief A `FILE` deleter for `std::unique_ptr`.
     *
     * This deleter calls `std::fclose` to ensure the file is closed when the `unique_ptr` is destroyed.
     * @endif
     */
    struct file_deleter
    {
        void operator()(FILE* fp) const noexcept
        {
            if (fp) std::fclose(fp); // NOLINT(cppcoreguidelines-owning-memory)
        }
    };

public:
    /**
     * @lang{ZH}
     * @brief 默认构造函数，创建一个关闭状态的文件设备。
     * @endif
     *
     * @lang{EN}
     * @brief Default constructor, creates a file device in a closed state.
     * @endif
     */
    basic_file_device() = default;

    /**
     * @lang{ZH}
     * @brief 构造函数，打开一个文件并准备 I/O 操作。
     *
     * @param file_name 要打开的文件的路径。
     * @param flags 文件打开标志。
     * @throw device_error 如果文件打开、查询大小或定位失败。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that opens a file and prepares it for I/O operations.
     *
     * @param file_name The path to the file to open.
     * @param flags File opening flags.
     * @throw device_error If opening the file, querying its size, or seeking fails.
     * @endif
     */
    explicit basic_file_device(const std::string& file_name, file_open_flag flags = file_open_flag::none)
    {
        const char* mode = fopen_mode(flags);
        m_file.reset(std::fopen(file_name.c_str(), mode));
        if (!m_file)
            throw device_error("cannot open file \"" + file_name + "\" (errno=" + std::to_string(errno) + ")");

        if (fseek_64(m_file.get(), 0, SEEK_END) != 0)
            throw device_error("cannot get file length for \"" + file_name + "\"");

        int64_t len = ftell_64(m_file.get());
        if (len < 0)
            throw device_error("cannot get file length for \"" + file_name + "\"");
        else if (!std::in_range<size_t>(len))
            throw device_error("file too large for this platform");

        if (fseek_64(m_file.get(), 0, SEEK_SET) != 0)
            throw device_error("cannot reset the file for \"" + file_name + "\"");

        m_file_len = static_cast<size_t>(len);
    }

    /**
     * @lang{ZH}
     * @brief 尝试打开一个文件，并以 `std::expected` 返回结果。
     *
     * 这是一个不会抛出异常的工厂函数。如果成功，返回文件设备；如果失败，返回错误信息。
     * @param file_name 要打开的文件的路径。
     * @param flags 文件打开标志。
     * @return 包含 `basic_file_device` 或错误字符串的 `std::expected` 对象。
     * @endif
     *
     * @lang{EN}
     * @brief Tries to open a file and returns the result as a `std::expected`.
     *
     * This is a factory function that does not throw exceptions. It returns a file device on success
     * or an error message on failure.
     * @param file_name The path to the file to open.
     * @param flags File opening flags.
     * @return A `std::expected` object containing either a `basic_file_device` or an error string.
     * @endif
     */
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
    /**
     * @lang{ZH}
     * @brief 检查是否已到达文件末尾。
     *
     * 通过比较当前位置和文件大小来判断。如果文件未打开或发生错误，也返回 `true`。
     * @return 如果到达文件末尾，则为 `true`，否则为 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Checks if the end of the file has been reached.
     *
     * Determined by comparing the current position with the file size. Returns `true`
     * if the file is not open or an error occurs.
     * @return `true` if the end of the file is reached, otherwise `false`.
     * @endif
     */
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

    /**
     * @lang{ZH}
     * @brief 检查文件是否已成功打开。
     * @return 如果文件已打开，则为 `true`，否则为 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Checks if the file is successfully opened.
     * @return `true` if the file is open, otherwise `false`.
     * @endif
     */
    [[nodiscard]] bool is_open() const
    {
        return m_file != nullptr;
    }

    /**
     * @lang{ZH}
     * @brief 关闭文件。
     *
     * 该函数保证无论是否发生异常，文件句柄都会被释放。
     * 对于输出设备，会首先尝试刷新缓冲区（dflush）。如果刷新过程中抛出异常，
     * 会先确保文件句柄被正确关闭，然后再重新抛出捕获到的异常。
     * @endif
     *
     * @lang{EN}
     * @brief Closes the file.
     *
     * This function guarantees that the file handle is released regardless of whether an exception occurs.
     * For output devices, it first attempts to flush the buffer (dflush). If an exception occurs
     * during flushing, the file handle is still reset before the exception is re-thrown.
     * @endif
     */
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
    /**
     * @lang{ZH}
     * @brief 从设备读取数据。
     * @param s 指向存储读取数据的缓冲区的指针。
     * @param n 要读取的字符数。
     * @return 实际读取的字符数。
     * @throw device_error 如果缓冲区为空或发生读取错误。
     * @endif
     *
     * @lang{EN}
     * @brief Reads data from the device.
     * @param s Pointer to the buffer where the read data will be stored.
     * @param n The number of characters to read.
     * @return The number of characters actually read.
     * @throw device_error If the buffer is null or a read error occurs.
     * @endif
     */
    size_t dget(CharType* s, size_t n)
        requires (IsIn)
    {
        if (n == 0) return 0;
        if (s == nullptr && n > 0)
            throw device_error("file_device::dget fail: null buffer");

        if (!is_open()) return 0;
        size_t count = std::fread(s, sizeof(CharType), n, m_file.get());
        if (count < n && std::ferror(m_file.get()))
            throw device_error("file_device::dget fail: read error");
        return count;
    }

    /**
     * @lang{ZH}
     * @brief 获取当前读/写指针的位置。
     * @return 从文件开头算起的字节偏移量。
     * @throw device_error 如果文件未打开或获取位置失败。
     * @endif
     *
     * @lang{EN}
     * @brief Gets the current position of the read/write pointer.
     * @return The byte offset from the beginning of the file.
     * @throw device_error If the file is not open or getting the position fails.
     * @endif
     */
    [[nodiscard]] size_t dtell() const
    {
        if (!is_open())
            throw device_error("file_device::dtell fail: file is closed");

        auto res = ftell_64(m_file.get());
        if (res < 0)
            throw device_error("file_device::dtell fail: got invalid position.");
        return static_cast<size_t>(res);
    }

    /**
     * @lang{ZH}
     * @brief 获取文件的总大小。
     * @return 文件的字节大小。
     * @endif
     *
     * @lang{EN}
     * @brief Gets the total size of the file.
     * @return The size of the file in bytes.
     * @endif
     */
    [[nodiscard]] size_t dsize() const
    {
        return m_file_len;
    }

    /**
     * @lang{ZH}
     * @brief 将读/写指针移动到指定位置。
     * @param v 从文件开头算起的绝对位置。
     * @throw device_error 如果文件未打开或定位失败。
     * @endif
     *
     * @lang{EN}
     * @brief Moves the read/write pointer to a specified position.
     * @param v The absolute position from the beginning of the file.
     * @throw device_error If the file is not open or seeking fails.
     * @endif
     */
    void dseek(size_t v)
    {
        if (!is_open())
            throw device_error("file_device::dseek fail: file is closed");
        
        // In input-enabled modes, seeking is restricted to the current file length.
        // This ensures that any subsequent read operation (dget) starts from a valid 
        // position within the existing data bounds.
        if constexpr (IsIn)
        {
            if (v > m_file_len)
                throw device_error("file_device::dseek fail: invalid parameter");
        }

        if (v > std::numeric_limits<int64_t>::max())
            throw device_error("file_device::dseek fail: position exceeds maximum seekable range");

        if (fseek_64(m_file.get(), static_cast<int64_t>(v), SEEK_SET) != 0)
            throw device_error("file_device::dseek fail: fseek invalid response");
    }

    /**
     * @lang{ZH}
     * @brief 将读/写指针从文件末尾向前移动。
     * @param offset 从文件末尾向前的偏移量。
     * @throw device_error 如果文件未打开或定位失败。
     * @endif
     *
     * @lang{EN}
     * @brief Moves the read/write pointer forward from the end of the file.
     * @param offset The offset from the end of the file.
     * @throw device_error If the file is not open or seeking fails.
     * @endif
     */
    void drseek(size_t offset)
    {
        if (!is_open())
            throw device_error("file_device::drseek fail: file is closed");

        if ((offset > m_file_len) || (offset > static_cast<size_t>(std::numeric_limits<int64_t>::max())))
            throw device_error("file_device::drseek fail: invalid parameter");

        const int64_t l_off = -static_cast<int64_t>(offset);

        if (fseek_64(m_file.get(), l_off, SEEK_END) != 0)
            throw device_error("file_device::drseek fail: fseek invalid response");
    }

    /**
     * @lang{ZH}
     * @brief 将数据写入设备。
     * @param ch 指向要写入数据的缓冲区的指针。
     * @param n 要写入的字符数。
     * @throw device_error 如果文件未打开或写入失败。
     * @endif
     *
     * @lang{EN}
     * @brief Writes data to the device.
     * @param ch Pointer to the buffer containing the data to be written.
     * @param n The number of characters to write.
     * @throw device_error If the file is not open or writing fails.
     * @endif
     */
    void dput(const CharType* ch, size_t n)
        requires (IsOut)
    {
        if (n == 0) return;
        if (ch == nullptr && n > 0)
            throw device_error("file_device::dput fail: null buffer");
        if (!is_open())
            throw device_error("file_device::dput fail: file closed.");
        size_t written = std::fwrite(ch, sizeof(CharType), n, m_file.get());

        auto res = ftell_64(m_file.get());
        if (res >= 0)
        {
            if (!std::in_range<size_t>(res))
                throw device_error("file_device::dput fail: file size overflow");
            if (static_cast<size_t>(res) > m_file_len)
                m_file_len = static_cast<size_t>(res);
        }
        else
            throw device_error("file_device::dput fail: cannot determine file position");

        if (written != n)
            throw device_error("file_device::dput fail: partial success.");
    }

    /**
     * @lang{ZH}
     * @brief 刷新写入缓冲区。
     *
     * 强制将 C 标准库 `FILE` 的内部缓冲区内容写入操作系统。
     * @throw device_error 如果刷新失败。
     * @endif
     *
     * @lang{EN}
     * @brief Flushes the write buffer.
     *
     * Forces the contents of the C standard library `FILE`'s internal buffer to be written to the operating system.
     * @throw device_error If flushing fails.
     * @endif
     */
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

    // Cached file length, in CharType units.
    //
    // Type is `size_t` (not `uint64_t`) because the public API — `dsize()`,
    // `dseek(size_t)`, `drseek(size_t)` — is `size_t`-typed; using `size_t`
    // here keeps the value lossless across the boundary on every platform
    // (including 32-bit, where a `uint64_t` field could hold values that
    // `dsize()` cannot return without truncation).
    //
    // Invariant: `m_file_len <= INT64_MAX`. All writes to this field source
    // from a non-negative `int64_t` (the result of `ftell_64()`), narrowed
    // through `std::in_range<size_t>` in the constructor and `dput()`.
    // This invariant is what makes `-static_cast<int64_t>(offset)` in
    // `drseek()` safe: any `offset <= m_file_len` fits in `int64_t`'s
    // positive range, so the negation cannot overflow. Do not introduce
    // setters or alternate sources that bypass the `int64_t`-derived path.
    size_t m_file_len = 0;
};

/**
 * @lang{ZH}
 * @brief 只读文件设备的类型别名。
 * @tparam CharType 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Type alias for a read-only file device.
 * @tparam CharType The character type.
 * @endif
 */
template <typename CharType>
using ifile_device = basic_file_device<true, false, CharType>;

/**
 * @lang{ZH}
 * @brief 只写文件设备的类型别名。
 * @tparam CharType 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Type alias for a write-only file device.
 * @tparam CharType The character type.
 * @endif
 */
template <typename CharType>
using ofile_device = basic_file_device<false, true, CharType>;

/**
 * @lang{ZH}
 * @brief 读写文件设备的类型别名。
 * @tparam CharType 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Type alias for a read-write file device.
 * @tparam CharType The character type.
 * @endif
 */
template <typename CharType>
using file_device = basic_file_device<true, true, CharType>;
}
