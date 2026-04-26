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

#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 一个在内存中进行 I/O 操作的设备。
 *
 * `mem_device` 将 `std::basic_string` 封装成一个满足 `io_device` 概念的设备。
 * 它支持对内部字符串缓冲区的读、写和寻址操作，是实现 `stringstream` 功能的基础。
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
 * @tparam CharT The character type.
 * @tparam Traits The character traits, defaulting to `std::char_traits<CharT>`.
 * @tparam Allocator The memory allocator, defaulting to `std::allocator<CharT>`.
 * @endif
 */
template <class CharT,
          class Traits = std::char_traits<CharT>,
          class Allocator = std::allocator<CharT>>
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

    mem_device(const mem_device&) = default;
    mem_device(mem_device&&) = default;
    mem_device& operator=(const mem_device&) = default;
    mem_device& operator=(mem_device&&) = default;
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
        if (s == nullptr && n > 0)
            throw device_error("mem_device::dget fail: null buffer");
        if (n == 0) return 0;

        std::size_t res = std::min(m_str.size() - m_next_pos, n);
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
            throw device_error("mem_device::seek fail: out of boundary");
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
            throw device_error("mem_device::rseek fail: out of boundary");
        m_next_pos = m_str.size() - offset;
    }

    /**
     * @lang{ZH}
     * @brief 向当前位置写入数据。
     *
     * 如果写入操作超出当前字符串大小，字符串会自动增长。
     *
     * @warning `ch` 不能指向设备的内部缓冲区（即 `str()` 返回的存储区域）。
     * 在需要扩容的写入路径上，扩容可能使指向内部缓冲区的指针失效，因此一旦
     * 检测到 `[ch, ch + n)` 与内部缓冲区重叠，就会抛出 `device_error`。
     * 如果调用者需要从设备自身的数据进行写入，请先复制到独立缓冲区。
     *
     * @param ch 要写入的数据。
     * @param n 要写入的字符数。
     * @throw device_error 当 `ch` 为 `nullptr` 且 `n > 0`、大小溢出，
     * 或在扩容路径上 `ch` 与内部缓冲区别名时抛出。
     * @endif
     *
     * @lang{EN}
     * @brief Writes data at the current position.
     *
     * The string grows automatically if the write operation exceeds the current string size.
     *
     * @warning `ch` must not point into the device's internal buffer (the storage
     * exposed via `str()`). On the grow path, growth may invalidate pointers into
     * the internal buffer, so if `[ch, ch + n)` is detected to overlap the internal
     * buffer the function throws `device_error`. Callers that need to write from
     * the device's own data must copy it into an independent buffer first.
     *
     * @param ch The data to write.
     * @param n The number of characters to write.
     * @throw device_error When `ch` is `nullptr` and `n > 0`, when the size overflows,
     * or when `ch` aliases the internal buffer on the grow path.
     * @endif
     */
    void dput(const char_type* ch, size_t n)
    {
        if (ch == nullptr && n > 0)
            throw device_error("mem_device::dput fail: null buffer");

        if (n > m_str.max_size() - m_next_pos)
            throw device_error("mem_device::dput fail: size overflow");

        if (m_next_pos + n > m_str.size())
        {
            // Reserve below may reallocate m_str's storage and invalidate any
            // pointer that lives inside it. Reject aliased input up-front rather
            // than risk a use-after-free. std::less is used because comparing
            // unrelated pointers with raw `<` is unspecified.
            const std::less<const char_type*> ptr_less;
            const char_type* buf_beg = m_str.data();
            const char_type* buf_end = buf_beg + m_str.capacity();
            if (ptr_less(ch, buf_end) && ptr_less(buf_beg, ch + n))
                throw device_error("mem_device::dput fail: ch aliases internal buffer");

            m_str.erase(m_str.begin() + m_next_pos, m_str.end());
            m_str.reserve(m_next_pos + n);
            m_str.append(ch, n);
        }
        else
            std::memmove(m_str.data() + m_next_pos, ch, n * sizeof(char_type));
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

private:
    std::basic_string<CharT, Traits, Allocator> m_str;
    size_t m_next_pos = 0;
};

/// @cond
template <typename TChar>
mem_device(const TChar*) -> mem_device<TChar>;
/// @endcond
}
