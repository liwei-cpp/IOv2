/**
 * @file mem_device.h
 * @lang{ZH}
 * 定义了 `mem_device` 类，这是一个基于内存的 I/O 设备。
 * 它使用 `std::basic_string` 作为底层存储，提供了在内存字符串上进行
 * 读、写和定位操作的接口。
 * @endif
 *
 * @lang{EN}
 * Defines the `mem_device` class, a memory-based I/O device.
 * It uses `std::basic_string` as its underlying storage, providing an interface
 * for reading, writing, and seeking on an in-memory string.
 * @endif
 */
#pragma once
#include <common/defs.h>
#include <device/device_concepts.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 一个在内存中进行 I/O 操作的设备。
 *
 * `mem_device` 将 `std::basic_string` 封装成一个满足 `io_device` 概念的设备。
 * 它支持对内部字符串缓冲区的读、写和寻址操作，是实现 `stringstream` 功能的基础。
 *
 * @note 此类不是线程安全的，多线程并发由更高层次的代码处理。
 *
 * @tparam CharT 字符类型。
 * @tparam Traits 字符类型的特性，默认为 `std::char_traits<CharT>`。
 * @tparam Allocator 内存分配器，默认为 `std::allocator<CharT>`。
 * @endif
 *
 * @lang{EN}
 * @brief A device that performs I/O operations in memory.
 *
 * `mem_device` wraps a `std::basic_string` into a device that satisfies the `io_device` concept.
 * It supports reading, writing, and seeking within its internal string buffer, forming the
 * basis for implementing features like `stringstream`.
 *
 * @note This class is not thread-safe; multi-threading is handled at a higher level.
 *
 * @tparam CharT The character type.
 * @tparam Traits The character traits, defaulting to `std::char_traits<CharT>`.
 * @tparam Allocator The memory allocator, defaulting to `std::allocator<CharT>`.
 * @endif
 */
template <class CharT,
          class Traits = std::char_traits<CharT>,
          class Allocator = std::allocator<CharT>>
    requires std::is_trivially_copyable_v<CharT>
class mem_device
{
public:
    using char_type = CharT;

public:
    /**
     * @lang{ZH}
     * @brief 构造函数，可以从一个现有的 `basic_string` 初始化设备。
     * @param info 用于初始化内部缓冲区的字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor, optionally initializes the device from an existing `basic_string`.
     * @param info The string used to initialize the internal buffer.
     * @endif
     */
    mem_device(std::basic_string<CharT, Traits, Allocator> info = std::basic_string<CharT, Traits, Allocator>{})
        : m_str(std::move(info))
    {}

    /**
     * @lang{ZH}
     * @brief 从 C 风格字符串构造设备。
     * @param str 指向以 null 结尾的字符串的指针。
     * @throw device_error 如果 str 为 nullptr。
     * @endif
     *
     * @lang{EN}
     * @brief Constructs a device from a C-style string.
     * @param str Pointer to a null-terminated string.
     * @throw device_error If str is nullptr.
     * @endif
     */
    explicit mem_device(const CharT* str)
        : m_str(str ? str : throw device_error("mem_device: null string pointer"))
    {}

    mem_device(const mem_device&) = default;
    mem_device& operator=(const mem_device&) = default;
    mem_device(mem_device&& other) noexcept
        : m_str(std::move(other.m_str))
        , m_next_pos(other.m_next_pos)
    {
        other.m_next_pos = 0;
    }

    mem_device& operator=(mem_device&& other)
        noexcept(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
                 std::allocator_traits<Allocator>::is_always_equal::value)
    {
        if (this != &other)
        {
            m_str = std::move(other.m_str);
            m_next_pos = other.m_next_pos;
            other.m_next_pos = 0;
        }
        return *this;
    }

    ~mem_device() = default;

    /**
     * @lang{ZH}
     * @brief 获取对内部字符串的常量引用。
     * @return 内部 `std::basic_string` 的常量引用。
     * @endif
     *
     * @lang{EN}
     * @brief Gets a constant reference to the internal string.
     * @return A constant reference to the internal `std::basic_string`.
     * @endif
     */
    [[nodiscard]] const std::basic_string<CharT, Traits, Allocator>& str() const { return m_str; }

public:
    /**
     * @lang{ZH}
     * @brief 检查是否已到达内存缓冲区的末尾。
     * @return 如果当前位置大于或等于缓冲区大小，则为 `true`。
     * @endif
     *
     * @lang{EN}
     * @brief Checks if the end of the memory buffer has been reached.
     * @return `true` if the current position is at or beyond the end of the buffer.
     * @endif
     */
    [[nodiscard]] bool deof() const
    {
        return (m_next_pos >= m_str.size());
    }

    /**
     * @lang{ZH}
     * @brief 从当前位置读取数据。
     * @param s 存储读取数据的缓冲区。
     * @param n 要读取的字符数。
     * @return 实际读取的字符数。
     * @endif
     *
     * @lang{EN}
     * @brief Reads data from the current position.
     * @param s The buffer to store the read data.
     * @param n The number of characters to read.
     * @return The number of characters actually read.
     * @endif
     */
    size_t dget(char_type* s, size_t n)
    {
        if (n == 0) return 0;
        if (s == nullptr)
            throw device_error("mem_device::dget fail: null buffer");
        assert(m_next_pos <= m_str.size());

        size_t res = std::min(m_str.size() - m_next_pos, n);
        std::memmove(s, m_str.data() + m_next_pos, res * sizeof(char_type));
        m_next_pos += res;
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 获取当前读/写位置。
     * @return 当前在缓冲区中的位置。
     * @endif
     *
     * @lang{EN}
     * @brief Gets the current read/write position.
     * @return The current position within the buffer.
     * @endif
     */
    [[nodiscard]] size_t dtell() const
    {
        return m_next_pos;
    }

    /**
     * @lang{ZH}
     * @brief 获取内部缓冲区的总大小。
     * @return 字符串的大小。
     * @endif
     *
     * @lang{EN}
     * @brief Gets the total size of the internal buffer.
     * @return The size of the string.
     * @endif
     */
    [[nodiscard]] size_t dsize() const
    {
        return m_str.size();
    }

    /**
     * @lang{ZH}
     * @brief 将读/写位置移动到绝对位置。
     * @param v 要移动到的新位置。
     * @throw device_error 如果位置超出范围。
     * @endif
     *
     * @lang{EN}
     * @brief Moves the read/write pointer to an absolute position.
     * @param v The new position to move to.
     * @throw device_error If the position is out of bounds.
     * @endif
     */
    void dseek(size_t v)
    {
        if (v > m_str.size())
            throw device_error("mem_device::dseek fail: out of bounds");
        m_next_pos = v;
    }

    /**
     * @lang{ZH}
     * @brief 将读/写位置从末尾反向移动。
     * @param offset 从末尾算起的偏移量。
     * @throw device_error 如果位置超出范围。
     * @endif
     *
     * @lang{EN}
     * @brief Moves the read/write pointer backwards from the end.
     * @param offset The offset from the end.
     * @throw device_error If the position is out of bounds.
     * @endif
     */
    void drseek(size_t offset)
    {
        if (offset > m_str.size())
            throw device_error("mem_device::drseek fail: out of bounds");
        m_next_pos = m_str.size() - offset;
    }

    /**
     * @lang{ZH}
     * @brief 向当前位置写入数据。
     *
     * 如果写入操作超出当前字符串大小，字符串会自动增长。
     *
     * @param ch 要写入的数据。
     * @param n 要写入的字符数。
     * @throw device_error 当 `ch` 为 `nullptr` 且 `n > 0` 或大小溢出时抛出。
     * @endif
     *
     * @lang{EN}
     * @brief Writes data at the current position.
     *
     * The string grows automatically if the write operation exceeds the current string size.
     *
     * @param ch The data to write.
     * @param n The number of characters to write.
     * @throw device_error When `ch` is `nullptr` and `n > 0`, or when the size overflows.
     * @endif
     */
    void dput(const char_type* ch, size_t n)
    {
        if (n == 0) return;
        if (ch == nullptr)
            throw device_error("mem_device::dput fail: null buffer");

        if (n > m_str.max_size() - m_next_pos)
            throw device_error("mem_device::dput fail: size overflow");

        m_str.replace(m_next_pos, n, ch, n);
        m_next_pos += n;
    }

    /**
     * @lang{ZH}
     * @brief 刷新设备。对于内存设备，此操作为空。
     * @endif
     *
     * @lang{EN}
     * @brief Flushes the device. This is a no-op for a memory device.
     * @endif
     */
    void dflush() {}

    /**
     * @lang{ZH}
     * @brief 获取输入缓冲区中的一段连续内存。
     * @tparam Saturate 如果为 true，则要求必须读取到请求的长度，否则抛出异常。
     * @param to_max 请求获取的最大长度。
     * @return 如果 Saturate 为 true，返回指向数据的指针；否则返回 std::pair，包含数据指针和实际获取的长度。
     * @throw device_error 如果 Saturate 为 true 且剩余数据不足。
     * @endif
     *
     * @lang{EN}
     * @brief Retrieves a contiguous block of memory from the input buffer.
     * @tparam Saturate If true, requires the exact requested length; otherwise, throws an exception.
     * @param to_max The maximum requested length.
     * @return If Saturate is true, returns a pointer to the data; otherwise, returns a std::pair containing the data pointer and actual length.
     * @throw device_error If Saturate is true and there is insufficient data remaining.
     * @endif
     */
    template <bool Saturate = false>
    auto get_buf(size_t to_max)
    {
        assert(m_str.size() >= m_next_pos);
        const size_t remain = m_str.size() - m_next_pos;
        if constexpr (Saturate)
        {
            if (to_max > remain)
                throw device_error("mem_device::get_but fail: not enough input");
            auto res = const_cast<const CharT*>(m_str.c_str() + m_next_pos);
            m_next_pos += to_max;
            return res;
        }
        else
        {
            const size_t res_len = std::min(to_max, remain);
            auto res = std::pair<const CharT*, size_t>{m_str.c_str() + m_next_pos, res_len};
            m_next_pos += res_len;
            return res;
        }
    }

    /**
     * @lang{ZH}
     * @brief 回退输入流的读取位置。
     * @param len 要回退的长度。
     * @throw device_error 如果回退长度为零或超过当前已读取的位置。
     * @endif
     *
     * @lang{EN}
     * @brief Rolls back the read position of the input stream.
     * @param len The length to roll back.
     * @throw device_error If the rollback length is zero or exceeds the current read position.
     * @endif
     */
    void get_rollback(size_t len)
    {
        if (len == 0)
            throw device_error("mem_device::get_rollback fail, length cannot be zero");
        if (m_next_pos < len)
            throw device_error("mem_device::get_rollback fail, rollback length too large");
        m_next_pos -= len;
    }

    /**
     * @lang{ZH}
     * @brief 获取输出缓冲区中的一段可写入的连续内存。
     * @param len 要请求的可写入长度。
     * @return 指向可写入区域起始位置的指针。
     * @endif
     *
     * @lang{EN}
     * @brief Retrieves a contiguous block of writable memory from the output buffer.
     * @param len The requested length to write.
     * @return A pointer to the start of the writable area.
     * @endif
     */
    CharT* put_buf(size_t len)
    {
        m_ori_size = m_str.size();
        const size_t needed = m_next_pos + len;
        if (needed > m_str.size())
        {
            if (needed > m_str.capacity())
            {
                const size_t cap = m_str.capacity();
                m_str.reserve(std::max(needed, cap + cap / 2));
            }
            m_str.resize(needed);
        }
        CharT* res = m_str.data() + m_next_pos;
        m_next_pos = needed;
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 回退输出流的写入位置，并根据需要调整底层缓冲区大小。
     * @param len 要回退的长度。
     * @throw device_error 如果回退长度为零或超过当前写入的位置。
     * @endif
     *
     * @lang{EN}
     * @brief Rolls back the write position of the output stream and adjusts the internal buffer size if necessary.
     * @param len The length to roll back.
     * @throw device_error If the rollback length is zero or exceeds the current write position.
     * @endif
     */
    void put_rollback(size_t len)
    {
        if (len == 0)
            throw device_error("mem_device::put_rollback fail, length cannot be zero");
        if (m_next_pos < len)
            throw device_error("mem_device::put_rollback fail, rollback length too large");
        m_next_pos -= len;
        m_str.resize(std::max(m_next_pos, m_ori_size));
    }

private:
    std::basic_string<CharT, Traits, Allocator> m_str;
    size_t m_next_pos = 0;
    size_t m_ori_size = 0;
};

/// @cond
template <typename TChar>
mem_device(const TChar*) -> mem_device<TChar>;
/// @endcond

/**
 * @lang{ZH}
 * @brief 判断一个类型是否为 `mem_device` 的辅助模板。
 * @tparam T 要检查的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Helper template to determine if a type is `mem_device`.
 * @tparam T The type to check.
 * @endif
 */
template <typename T>
struct is_mem_device_impl
{
    static constexpr bool value = false;
};

/**
 * @lang{ZH}
 * @brief `is_mem_device_impl` 的 `mem_device` 特化版本。
 * @endif
 *
 * @lang{EN}
 * @brief Specialization of `is_mem_device_impl` for `mem_device`.
 * @endif
 */
template <typename CharT, typename Traits, typename Allocator>
struct is_mem_device_impl<mem_device<CharT, Traits, Allocator>>
{
    static constexpr bool value = true;
};

/**
 * @lang{ZH}
 * @brief 判断一个类型是否为 `mem_device` 的概念（Concept）。
 * @tparam T 要检查的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Concept to determine if a type is `mem_device`.
 * @tparam T The type to check.
 * @endif
 */
template <typename T>
concept is_mem_device = is_mem_device_impl<T>::value;
}
