/**
 * @file root_cvt.h
 * @lang{ZH}
 * 根转换器（root_cvt）定义文件。root_cvt 是转换器链的底层，直接管理设备与内部缓冲区之间的数据流。
 * @endif
 *
 * @lang{EN}
 * Root converter (root_cvt) definition file. root_cvt sits at the bottom of the
 * converter chain and manages data flow directly between a device and an internal buffer.
 * @endif
 */
#pragma once
#include <common/defs.h>
#include <common/metafunctions.h>
#include <cvt/abs_cvt.h>
#include <cvt/cvt_concepts.h>
#include <device/device_concepts.h>
#include <device/mem_device.h>

#include <cassert>
#include <exception>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace IOv2
{
/**
 * @lang{ZH}
 * 根转换器：I/O 操作的底层转换器，负责管理设备与内部缓冲区之间的数据传输。
 *
 * root_cvt 直接持有一个 I/O 设备（DeviceType），并可选地维护一个内部读写缓冲区。
 * 上层转换器通过 cvt_reader / cvt_writer 与 root_cvt 交互，无需直接操作设备。
 *
 * @note 移后对象（Moved-from）：移动构造或移动赋值之后，源对象处于有效但未指定的状态。
 *       对移后对象，仅以下操作是安全的：
 *       - 析构
 *       - 赋值（拷贝或移动）
 *       调用任何其他方法均为未定义行为（undefined behavior）。
 *
 * @note 调用顺序：成员函数必须按以下顺序调用：
 *       1. **bos()**：必须首先调用，以建立初始 IO 状态。
 *       2. **BOS 阶段**：可多次调用 `get()` 或多次调用 `put()`，但不可混合调用。
 *          此阶段用于读写流起始数据（如文件头）。
 *       3. **main_cont_beg()**：标记 BOS 阶段结束，进入主内容阶段。
 *       4. **主内容阶段**：可调用所有其他成员函数（`get`、`put`、`flush`、
 *          `tell`、`seek`、`rseek`、`switch_to_get`、`switch_to_put` 等）。
 *       违反此调用顺序将导致未定义行为。
 * @endif
 *
 * @lang{EN}
 * Root converter: the bottom-level converter for I/O operations, responsible for
 * transferring data between a device and an optional internal buffer.
 *
 * root_cvt owns an I/O device (DeviceType) and optionally maintains an internal
 * read/write buffer. Upper-level converters interact with root_cvt through
 * cvt_reader / cvt_writer without touching the device directly.
 *
 * @note Moved-from objects: After a move operation (move construction or move
 *       assignment), the source object is left in a valid but unspecified state.
 *       The only safe operations on a moved-from object are:
 *       - Destruction
 *       - Assignment (copy or move)
 *       Calling any other method on a moved-from object results in undefined behavior.
 *       This follows standard C++ conventions for moved-from objects.
 *
 * @note Calling sequence: Member functions must be called in the following order:
 *       1. **bos()**: Must be called first to establish the initial IO status.
 *       2. **BOS phase**: May call `get()` multiple times or `put()` multiple times,
 *          but must not mix get and put calls. This phase is for reading/writing
 *          stream-leading data (e.g., file headers).
 *       3. **main_cont_beg()**: Marks the end of the BOS phase and enters the
 *          main-content phase.
 *       4. **Main-content phase**: All other member functions may be called
 *          (`get`, `put`, `flush`, `tell`, `seek`, `rseek`, `switch_to_get`,
 *          `switch_to_put`, etc.).
 *       Violating this calling sequence results in undefined behavior.
 * @endif
 *
 * @tparam DeviceType
 * @lang{ZH} 底层 I/O 设备类型，须满足 io_device 概念。 @endif
 * @lang{EN} The underlying I/O device type, which must satisfy the io_device concept. @endif
 *
 * @tparam HasInBuffer
 * @lang{ZH}
 * 是否启用内部读取缓冲区。为 true 时，get() 通过内部缓冲区批量读取以减少设备调用次数；
 * 为 false 时，每次 get() 直接转发给设备。写入路径始终使用缓冲区。
 * @endif
 * @lang{EN}
 * Whether to enable an internal read buffer. When true, get() reads in bulk through
 * an internal buffer to reduce device call frequency; when false, each get() is
 * forwarded directly to the device. The write path always uses a buffer regardless.
 * @endif
 */
template <io_device DeviceType, bool HasInBuffer>
class root_cvt
{
    template <io_converter KernelType>
    friend class cvt_reader;

    template <io_converter KernelType>
    friend class cvt_writer;

public:
    /**
     * @lang{ZH}
     * 内部缓冲区的字符容量。读取和写入均使用此大小的缓冲区。
     * @endif
     *
     * @lang{EN}
     * Character capacity of the internal buffer. Both read and write paths use
     * a buffer of this size.
     * @endif
     */
    constexpr static size_t s_buffer_length = 2048;

    /**
     * @lang{ZH}
     * 指示是否启用内部读取缓冲区，与模板参数 HasInBuffer 相同。
     * @endif
     *
     * @lang{EN}
     * Indicates whether the internal read buffer is enabled; mirrors the
     * HasInBuffer template parameter.
     * @endif
     */
    constexpr static bool s_has_buffer = HasInBuffer;

    /**
     * @lang{ZH} 底层设备类型。 @endif
     * @lang{EN} The underlying device type. @endif
     */
    using device_type = DeviceType;

    /**
     * @lang{ZH} 上层转换器所使用的字符类型（与 device_type::char_type 相同）。 @endif
     * @lang{EN} Character type used by upper-level converters (same as device_type::char_type). @endif
     */
    using internal_type = typename device_type::char_type;

    /**
     * @lang{ZH} 设备侧的字符类型（与 device_type::char_type 相同）。 @endif
     * @lang{EN} Character type on the device side (same as device_type::char_type). @endif
     */
    using external_type = typename device_type::char_type;

public:
    /**
     * @lang{ZH}
     * 构造函数：接管给定设备，初始化内部缓冲区。
     * @endif
     *
     * @lang{EN}
     * Constructor: takes ownership of the given device and initializes the internal buffer.
     * @endif
     *
     * @param device
     * @lang{ZH} 要接管的 I/O 设备（按值传入，触发移动语义）。 @endif
     * @lang{EN} The I/O device to take ownership of (passed by value, triggering move semantics). @endif
     */
    explicit root_cvt(device_type device)
        : m_device(std::move(device))
        , m_buffer(s_buffer_length)
        , m_buf_cur(m_buffer.data())
        , m_buf_end(m_buffer.data()) {}

    /**
     * @lang{ZH}
     * 拷贝构造函数：深拷贝设备状态和缓冲区，并正确重建指针偏移。
     * 要求 DeviceType 满足 std::copy_constructible。
     * @endif
     *
     * @lang{EN}
     * Copy constructor: deep-copies device state and buffer, correctly rebuilding
     * pointer offsets into the new buffer allocation.
     * Requires DeviceType to satisfy std::copy_constructible.
     * @endif
     *
     * @param val
     * @lang{ZH} 源对象。若 val 是移后对象（m_buf_cur 为 nullptr），则拷贝后缓冲区重置为空。 @endif
     * @lang{EN} Source object. If val is a moved-from object (m_buf_cur is nullptr),
     *           the copy resets its buffer to empty rather than computing invalid offsets. @endif
     */
    root_cvt(const root_cvt& val)
        requires (std::copy_constructible<device_type>)
        : m_device(val.m_device)
        , m_bos_len(val.m_bos_len)
        , m_buffer(val.m_buffer)
        , m_io_status(val.m_io_status)
    {
        // Defensive: handle moved-from source gracefully to avoid UB from
        // nullptr arithmetic. Moved-from objects have m_buf_cur == nullptr.
        if (val.m_buf_cur == nullptr)
        {
            m_buf_cur = m_buffer.data();
            m_buf_end = m_buffer.data();
        }
        else
        {
            size_t cp = val.m_buf_cur - val.m_buffer.data();
            m_buf_cur = m_buffer.data() + cp;

            size_t ep = val.m_buf_end - val.m_buffer.data();
            m_buf_end = m_buffer.data() + ep;
        }
    }

    /**
     * @lang{ZH}
     * 移动构造函数：O(1) 转移设备与缓冲区所有权。移后源对象缓冲区指针置为 nullptr，
     * io_status 重置为 neutral，对源对象的任何非析构、非赋值操作均为未定义行为。
     * @endif
     *
     * @lang{EN}
     * Move constructor: transfers ownership of the device and buffer in O(1).
     * The source object's buffer pointers are set to nullptr and io_status is reset to
     * neutral; any operation other than destruction or assignment on the moved-from object
     * is undefined behavior.
     * @endif
     *
     * @param val
     * @lang{ZH} 源对象，移动后处于有效但未指定的状态。 @endif
     * @lang{EN} Source object, left in a valid but unspecified state after the move. @endif
     */
    root_cvt(root_cvt&& val) noexcept
        : m_device(std::move(val.m_device))
        , m_bos_len(val.m_bos_len)
        , m_io_status(val.m_io_status)
    {
        size_t cp = 0;
        size_t ep = 0;

        if (val.m_buf_cur)
        {
            cp = val.m_buf_cur - val.m_buffer.data();
            ep = val.m_buf_end - val.m_buffer.data();
        }

        m_buffer = std::move(val.m_buffer);
        m_buf_cur = m_buffer.data() + cp;
        m_buf_end = m_buffer.data() + ep;

        val.m_buf_cur = val.m_buf_end = nullptr;
        val.m_io_status = io_status::neutral;
    }

    /**
     * @lang{ZH}
     * 拷贝赋值运算符：先将当前缓冲区刷入设备（如设备支持写入），再以拷贝构造 + 移动赋值实现。
     * 要求 DeviceType 满足 std::is_copy_assignable。
     * @endif
     *
     * @lang{EN}
     * Copy assignment operator: flushes the current buffer to the device (if the device
     * supports writing) and then delegates to copy construction followed by move assignment.
     * Requires DeviceType to satisfy std::is_copy_assignable.
     * @endif
     *
     * @param val
     * @lang{ZH} 源对象。 @endif
     * @lang{EN} Source object. @endif
     * @return
     * @lang{ZH} *this 的引用。 @endif
     * @lang{EN} Reference to *this. @endif
     */
    root_cvt& operator=(const root_cvt& val)
        requires (std::is_copy_assignable_v<device_type>)
    {
        if (this == &val) return *this;

        if constexpr (dev_cpt::support_put<device_type>)
            flush();

        root_cvt tmp(val);
        *this = std::move(tmp);
        return *this;
    }

    /**
     * @lang{ZH}
     * 移动赋值运算符：先尝试刷出当前缓冲区（忽略异常），再以 O(1) 转移设备与缓冲区所有权。
     * @endif
     *
     * @lang{EN}
     * Move assignment operator: attempts to flush the current buffer first (exceptions
     * are swallowed), then transfers ownership of the device and buffer in O(1).
     * @endif
     *
     * @param val
     * @lang{ZH} 源对象，赋值后处于有效但未指定的状态。 @endif
     * @lang{EN} Source object, left in a valid but unspecified state after the assignment. @endif
     * @return
     * @lang{ZH} *this 的引用。 @endif
     * @lang{EN} Reference to *this. @endif
     */
    root_cvt& operator=(root_cvt&& val) noexcept
    {
        if (this == &val) return *this;

        if constexpr (dev_cpt::support_put<device_type>)
        {
            try { flush(); }
            catch (...) {} // NOLINT(bugprone-empty-catch)
        }

        m_device = std::move(val.m_device);
        m_bos_len = val.m_bos_len;

        size_t cp = 0;
        size_t ep = 0;

        if (val.m_buf_cur)
        {
            cp = val.m_buf_cur - val.m_buffer.data();
            ep = val.m_buf_end - val.m_buffer.data();
        }

        m_buffer = std::move(val.m_buffer);
        m_buf_cur = m_buffer.data() + cp;
        m_buf_end = m_buffer.data() + ep;
        val.m_buf_cur = val.m_buf_end = nullptr;

        m_io_status = val.m_io_status;
        val.m_io_status = io_status::neutral;
        return *this;
    }

    /**
     * @lang{ZH}
     * 析构函数：若设备支持写入，则尝试将缓冲区数据刷入设备；异常被静默忽略以防止 std::terminate。
     * @endif
     *
     * @lang{EN}
     * Destructor: attempts to flush buffered data to the device if the device supports
     * writing; exceptions are silently ignored to prevent std::terminate.
     * @endif
     */
    ~root_cvt()
    {
        if constexpr (dev_cpt::support_put<device_type>)
        {
            try
            {
                flush();
            }
            catch (...) {// NOLINT(bugprone-empty-catch)
                // Ignore exceptions in destructor to prevent std::terminate
            }
        }
    }

// mandatory methods
public:
    /**
     * @lang{ZH}
     * 返回底层设备的引用，以便调用方直接访问设备接口。
     * @endif
     *
     * @lang{EN}
     * Returns a reference to the underlying device, allowing the caller to access
     * device-specific interfaces directly.
     * @endif
     *
     * @return
     * @lang{ZH} 底层 DeviceType 的可变引用。 @endif
     * @lang{EN} Mutable reference to the underlying DeviceType. @endif
     */
    device_type& device() { return m_device; }

    /**
     * @lang{ZH}
     * 将底层设备从 root_cvt 中分离并返回。
     * - 若当前处于输出状态，先尝试将缓冲区刷入设备。
     * - 若当前处于输入状态且缓冲区中仍有未消费数据，则将设备位置倒回到未消费数据的起始处（需设备支持定位）。
     *
     * 本函数为 `noexcept`：清理阶段的异常会被捕获并存入返回值的 `second`（`exception_ptr`），
     * 设备始终通过返回值的 `first` 无条件交还给调用方，即便清理失败。
     * 调用后，root_cvt 内部状态被重置为初始状态，不再持有有效设备。
     * @endif
     *
     * @lang{EN}
     * Detaches the underlying device from root_cvt and returns it.
     * - If currently in output mode, attempts to flush buffered data to the device.
     * - If currently in input mode with unconsumed buffered data, seeks the device
     *   back to the start of unconsumed data (requires the device to support positioning).
     *
     * This function is `noexcept`: any exception thrown during cleanup is captured
     * into the returned pair's `second` (`exception_ptr`); the device is always
     * handed back unconditionally via `first` even if cleanup failed.
     * After this call, root_cvt's internal state is reset and it no longer holds a valid device.
     * @endif
     *
     * @return
     * @lang{ZH} pair：`first` 为已分离的设备对象（通过移动语义返回），`second` 为清理阶段捕获的首个异常（`nullptr` 表示无异常）。 @endif
     * @lang{EN} A pair: `first` is the detached device (returned via move); `second` is the first exception captured during cleanup (`nullptr` if none). @endif
     */
    std::pair<device_type, std::exception_ptr> detach() noexcept
    {
        std::exception_ptr err;
        try
        {
            if (m_io_status == io_status::output)
            {
                if constexpr (dev_cpt::support_put<device_type>)
                    flush();
            }
            else if ((m_io_status == io_status::input) &&
                     (m_buf_cur != m_buf_end))
            {
                if constexpr (dev_cpt::support_positioning<device_type>)
                    seek(tell());
            }
        }
        catch (...)
        {
            err = std::current_exception();
        }

        m_bos_len = 0;
        m_buf_cur = m_buffer.data();
        m_buf_end = m_buffer.data();
        m_io_status = io_status::neutral;

        return {std::move(m_device), err};
    }

    /**
     * @lang{ZH}
     * 分离当前设备，将新设备附加到 root_cvt，并将内部状态重置为初始状态。
     * 等价于先调用 `detach()` 清理原设备，再接管新设备。
     *
     * 与 `detach()` 不同的是，本函数**不**将原设备归还给调用方——原设备在
     * 函数内部被静默析构。如需保留原设备，调用方应先单独调用 `detach()` 取得
     * 设备，再调用本函数装入新设备。
     *
     * 异常：若 `detach()` 在清理原设备时捕获到异常（详见 `detach()` 文档），
     * 本函数会在新设备装入并完成所有状态重置之后将该异常重新抛出。换言之，
     * 即使抛出异常，root_cvt 也已切换到新设备并处于可用的 `neutral` 状态——
     * 不提供强异常安全保证（不会回滚到调用前状态）。
     * @endif
     *
     * @lang{EN}
     * Detaches the current device, attaches a new device to root_cvt, and resets
     * internal state to its initial values. Equivalent to calling `detach()` to
     * clean up the old device, followed by taking ownership of the new device.
     *
     * Unlike `detach()`, this function does NOT hand the old device back to the
     * caller — the old device is silently destroyed inside this function. Callers
     * who need to preserve the old device must call `detach()` first to retrieve
     * it, then call this function to install the new device.
     *
     * Exceptions: if `detach()` captures an exception while cleaning up the old
     * device (see `detach()` for details), this function rethrows that exception
     * after the new device has been installed and all state has been reset. In
     * other words, even when the function throws, root_cvt has already switched
     * to the new device and is in a usable `neutral` state — this provides no
     * strong exception safety guarantee (no rollback to pre-call state).
     * @endif
     *
     * @param dev
     * @lang{ZH} 要附加的新设备，默认为默认构造的空设备。 @endif
     * @lang{EN} The new device to attach; defaults to a default-constructed empty device. @endif
     */
    void attach(device_type&& dev = device_type{})
    {
        auto detach_res = detach();
        m_device = std::move(dev);
        m_bos_len = 0;

        m_buffer.resize(s_buffer_length);
        m_buf_cur = m_buffer.data();
        m_buf_end = m_buffer.data();
        m_io_status = io_status::neutral;
        if (detach_res.second) std::rethrow_exception(detach_res.second);
    }

    /**
     * @lang{ZH}
     * 调整转换器行为参数。对于 root_cvt，该方法为空操作，因为根层无可配置的转换行为。
     * @endif
     *
     * @lang{EN}
     * Adjusts converter behavior parameters. For root_cvt this is a no-op because the
     * root layer has no configurable conversion behavior.
     * @endif
     *
     * @param
     * @lang{ZH} 忽略的行为配置对象。 @endif
     * @lang{EN} Ignored behavior configuration object. @endif
     */
    void adjust(const cvt_behavior&) {}

    /**
     * @lang{ZH}
     * 检索转换器状态。对于 root_cvt，该方法为空操作，根层不产生需要向上传递的状态。
     * @endif
     *
     * @lang{EN}
     * Retrieves converter status. For root_cvt this is a no-op because the root layer
     * produces no status that needs to be propagated upward.
     * @endif
     *
     * @param
     * @lang{ZH} 忽略的状态输出对象。 @endif
     * @lang{EN} Ignored status output object. @endif
     */
    void retrieve(cvt_status&) const {}

    /**
     * @lang{ZH}
     * 标记一个连续数据块的开始，记录设备当前位置作为流起点（m_bos_len）。
     * 对支持定位的设备，此调用会建立"流内偏移 = 设备偏移 - m_bos_len"的不变量。
     * 在调用此方法之前，必须先调用 bos() 以建立 io_status。
     * @endif
     *
     * @lang{EN}
     * Marks the beginning of a contiguous data block, recording the current device
     * position as the stream origin (m_bos_len). For positionable devices this call
     * establishes the invariant: stream_offset = device_offset - m_bos_len.
     * bos() must be called before main_cont_beg() to establish io_status.
     * @endif
     */
    void main_cont_beg()
    {
        // bos() must be called before main_cont_beg() to establish io_status
        assert(m_io_status != io_status::neutral);

        if constexpr (dev_cpt::support_put<device_type>)
        {
            if (m_io_status == io_status::output)
                flush();
        }

        if constexpr (dev_cpt::support_positioning<device_type>)
        {
            // Invariant: dtell() >= buffered data. The buffer is filled via dget(),
            // which advances the device position by the amount read. Consuming data
            // from the buffer only moves m_buf_cur forward, never increasing the
            // buffered amount beyond what was read from the device.
            const auto buffered = static_cast<size_t>(m_buf_end - m_buf_cur);
            assert(m_device.dtell() >= buffered);
            m_bos_len = m_device.dtell() - buffered;
        }
    }

    /**
     * @lang{ZH}
     * 确定并设置初始 I/O 方向（beginning of stream）。
     * - 若设备同时支持读写：当 deof() 为 true 时进入输出模式，否则进入输入模式。
     * - 若设备仅支持读：进入输入模式。
     * - 若设备仅支持写：进入输出模式。
     *
     * 通常在打开或重新附加设备后立即调用，之后调用 main_cont_beg() 记录流起点。
     *
     * @par 设计说明
     * 对于读写设备，初始方向基于流是否包含数据来确定，这是为了保护数据完整性：
     * - **流有内容（deof() 为 false）→ 输入模式**：避免意外覆盖现有数据。例如，
     *   覆盖 UTF-8 文件的头部可能导致整个文件无法正确解析。
     * - **流无内容（deof() 为 true）→ 输出模式**：空流没有可读内容，只能写入。
     *
     * 若需覆盖非空文件，应在打开前先清空（truncate）文件，而非强制进入输出模式。
     * @endif
     *
     * @lang{EN}
     * Determines and sets the initial I/O direction (beginning of stream).
     * - For read-write devices: enters output mode when deof() is true,
     *   otherwise enters input mode.
     * - For read-only devices: enters input mode.
     * - For write-only devices: enters output mode.
     *
     * Typically called immediately after opening or re-attaching a device; call
     * main_cont_beg() afterward to record the stream origin.
     *
     * @par Design Rationale
     * For read-write devices, the initial direction is determined by whether the
     * stream contains data. This design protects data integrity:
     * - **Stream has content (deof() is false) → input mode**: Prevents accidental
     *   overwriting of existing data. For example, overwriting the header of a
     *   UTF-8 file could corrupt the entire file and make it unparseable.
     * - **Stream is empty (deof() is true) → output mode**: An empty stream has
     *   nothing to read, so writing is the only meaningful operation.
     *
     * To overwrite a non-empty file, truncate it before opening rather than
     * forcing output mode.
     * @endif
     *
     * @return
     * @lang{ZH} 确定后的 io_status（input 或 output）。 @endif
     * @lang{EN} The resolved io_status (input or output). @endif
     */
    io_status bos()
    {
        if constexpr (dev_cpt::support_get<device_type> &&
                      dev_cpt::support_put<device_type>)
        {
            if (m_device.deof())
                m_io_status = io_status::output;
            else
                m_io_status = io_status::input;
        }
        else if constexpr (dev_cpt::support_get<device_type>)
            m_io_status = io_status::input;
        else if constexpr (dev_cpt::support_put<device_type>)
            m_io_status = io_status::output;
        else
            static_assert(dependent_false_v<device_type>, "device does not support get nor put");

        return m_io_status;
    }

    /**
     * @lang{ZH}
     * 判断当前流是否已到达末尾（EOF）。
     * - 输出状态：若写缓冲区非空则先刷出，再查询设备 deof()。
     * - 输入状态：若缓冲区仍有未消费数据则返回 false，否则查询设备 deof()。
     * - neutral 状态：抛出 cvt_error。
     * @endif
     *
     * @lang{EN}
     * Tests whether the current stream has reached end of file (EOF).
     * - Output mode: flushes the write buffer if non-empty, then queries device deof().
     * - Input mode: returns false if the buffer still has unconsumed data, otherwise
     *   queries device deof().
     * - Neutral state: throws cvt_error.
     * @endif
     *
     * @return
     * @lang{ZH} 若已到达 EOF 则返回 true。 @endif
     * @lang{EN} true if end of file has been reached. @endif
     * @throws cvt_error
     * @lang{ZH} io_status 为 neutral 时抛出。 @endif
     * @lang{EN} Thrown when io_status is neutral. @endif
     */
    bool is_eof()
        requires (dev_cpt::support_get<device_type>)
    {
        switch(m_io_status)
        {
        case io_status::output:
            if constexpr (dev_cpt::support_put<device_type>)
            {
                if (m_buf_cur != m_buffer.data())
                    flush();
            }
            return m_device.deof();
        case io_status::input:
            if (m_buf_cur != m_buf_end) return false;
            return m_device.deof();
        default:
            throw cvt_error("root_cvt::is_eof fails: invalid io status");
        }
    }

// optional methods
public:
    /**
     * @lang{ZH}
     * 从设备读取最多 to_max 个字符到 to 指向的缓冲区。
     * - 若 HasInBuffer 为 true，通过内部缓冲区批量读取；缓冲区耗尽时再向设备请求下一批次。
     * - 若 HasInBuffer 为 false，直接转发给设备的 dget()。
     * - 若当前处于输出状态，会先切换到输入状态。
     * @endif
     *
     * @lang{EN}
     * Reads up to to_max characters from the device into the buffer pointed to by to.
     * - When HasInBuffer is true, reads are served from the internal buffer; an exhausted
     *   buffer triggers a bulk read from the device.
     * - When HasInBuffer is false, the call is forwarded directly to the device's dget().
     * - If currently in output mode, switches to input mode first.
     * @endif
     *
     * @param to
     * @lang{ZH} 接收数据的目标缓冲区指针。 @endif
     * @lang{EN} Pointer to the destination buffer that receives the data. @endif
     * @param to_max
     * @lang{ZH} 最多读取的字符数。 @endif
     * @lang{EN} Maximum number of characters to read. @endif
     * @return
     * @lang{ZH} 实际读取的字符数（可能小于 to_max）。 @endif
     * @lang{EN} The actual number of characters read (may be less than to_max). @endif
     * @throws cvt_error
     * @lang{ZH} io_status 不为 input 且无法切换时抛出。 @endif
     * @lang{EN} Thrown when io_status is not input and cannot be switched. @endif
     */
    size_t get(internal_type* to, size_t to_max)
        requires (dev_cpt::support_get<device_type>)
    {
        if constexpr (dev_cpt::support_put<device_type>)
        {
            if (m_io_status != io_status::input)
                switch_to_get();
        }
        if (m_io_status != io_status::input)
            throw cvt_error("root_cvt::get fails: invalid io status");

        if constexpr (!HasInBuffer)
            return m_device.dget(to, to_max);
        else
        {
            if (m_buf_cur == m_buf_end)
            {
                const size_t act_len = m_device.dget(m_buffer.data(), s_buffer_length);
                m_buf_cur = m_buffer.data();
                m_buf_end = m_buf_cur + act_len;
            }

            const size_t res_len = std::min<size_t>(m_buf_end - m_buf_cur, to_max);
            std::copy(m_buf_cur, m_buf_cur + res_len, to);
            m_buf_cur += res_len;
            return res_len;
        }
    }

    /**
     * @lang{ZH}
     * 将 to_size 个字符写入设备（经由内部缓冲区）。
     * 写入策略：
     * - 若写入数据量小于缓冲区剩余空间，直接追加到缓冲区。
     * - 若写入数据量恰好填满缓冲区，将完整缓冲区刷入设备。
     * - 若写入数据量超过缓冲区，先刷出当前缓冲区内容，再根据数据大小决定
     *   是写入缓冲区还是直接绕过缓冲区写入设备。
     * - 若当前处于输入状态，会先切换到输出状态。
     * @endif
     *
     * @lang{EN}
     * Writes to_size characters to the device via the internal buffer.
     * Write strategy:
     * - If the data fits in the remaining buffer space, appends to the buffer.
     * - If the data exactly fills the buffer, flushes the full buffer to the device.
     * - If the data exceeds the buffer, flushes the current buffer content first, then
     *   either buffers or bypasses the buffer depending on data size.
     * - If currently in input mode, switches to output mode first.
     * @endif
     *
     * @param to
     * @lang{ZH} 要写入的数据来源指针。 @endif
     * @lang{EN} Pointer to the source data to write. @endif
     * @param to_size
     * @lang{ZH} 要写入的字符数。 @endif
     * @lang{EN} Number of characters to write. @endif
     * @throws cvt_error
     * @lang{ZH} io_status 不为 output 且无法切换时抛出。 @endif
     * @lang{EN} Thrown when io_status is not output and cannot be switched. @endif
     */
    void put(const internal_type* to, size_t to_size)
        requires (dev_cpt::support_put<device_type>)
    {
        if constexpr (dev_cpt::support_get<device_type>)
        {
            if (m_io_status != io_status::output)
                switch_to_put();
        }
        if (m_io_status != io_status::output)
            throw cvt_error("root_cvt::put fails: invalid io status");

        const size_t buf_used = m_buf_cur - m_buffer.data();
        const size_t remain = s_buffer_length - buf_used;
        if (to_size < remain)
        {
            m_buf_cur = std::copy(to, to + to_size, m_buf_cur);
            return;
        }

        if (to_size == remain)
        {
            std::copy(to, to + to_size, m_buf_cur);
            m_device.dput(m_buffer.data(), s_buffer_length);
            m_buf_cur = m_buffer.data();
            return;
        }

        if (buf_used > 0)
            m_device.dput(m_buffer.data(), buf_used);
        if (to_size < s_buffer_length / 2)
            m_buf_cur = std::copy(to, to + to_size, m_buffer.data());
        else
        {
            m_buf_cur = m_buffer.data();
            m_device.dput(to, to_size);
        }
    }

    /**
     * @lang{ZH}
     * 将内部写缓冲区中未提交的数据刷入设备。
     * 若当前不处于输出状态，或缓冲区为空（m_buf_cur 与缓冲区首地址相同），则为空操作。
     * @endif
     *
     * @lang{EN}
     * Flushes any uncommitted data in the internal write buffer to the device.
     * This is a no-op when not in output mode or when the buffer is already empty
     * (m_buf_cur equals the buffer start).
     * @endif
     */
    void flush()
        requires (dev_cpt::support_put<device_type>)
    {
        if (m_io_status != io_status::output) return;

        if ((m_buf_cur != nullptr) && (m_buf_cur != m_buffer.data()))
        {
            m_device.dput(m_buffer.data(), m_buf_cur - m_buffer.data());
            m_buf_cur = m_buffer.data();
        }
    }

    /**
     * @lang{ZH}
     * 返回当前流位置（相对于 m_bos_len 的偏移量）。
     * - 输出状态：设备位置加上缓冲区中已写但未刷出的字节数。
     * - 输入状态：设备位置减去缓冲区中已读入但未被消费的字节数。
     * - neutral 状态：抛出 cvt_error。
     * @endif
     *
     * @lang{EN}
     * Returns the current stream position (offset relative to m_bos_len).
     * - Output mode: device position plus unflushed bytes in the write buffer.
     * - Input mode: device position minus unconsumed bytes in the read buffer.
     * - Neutral state: throws cvt_error.
     * @endif
     *
     * @return
     * @lang{ZH} 相对于流起点的当前位置偏移（字符数）。 @endif
     * @lang{EN} Current position offset from the stream origin in characters. @endif
     * @throws cvt_error
     * @lang{ZH} io_status 为 neutral 时抛出。 @endif
     * @lang{EN} Thrown when io_status is neutral. @endif
     */
    [[nodiscard]] size_t tell() const
        requires (dev_cpt::support_positioning<device_type>)
    {
        const size_t device_tell = m_device.dtell();

        switch(m_io_status)
        {
        case io_status::output:
        {
            // Invariant: device_tell >= m_bos_len. The device position only advances
            // via flush operations, never moving backward from the stream start.
            assert(device_tell >= m_bos_len);
            const auto buf_used = static_cast<size_t>(m_buf_cur - m_buffer.data());
            return device_tell - m_bos_len + buf_used;
        }
        case io_status::input:
        {
            // Invariant: device_tell - m_bos_len >= buffered. Same invariant as
            // main_cont_beg(): buffer data comes from dget(), which advances device
            // position by the amount read.
            const auto buffered = static_cast<size_t>(m_buf_end - m_buf_cur);
            assert(device_tell >= m_bos_len);
            assert(device_tell - m_bos_len >= buffered);
            return device_tell - m_bos_len - buffered;
        }
        default:
            throw cvt_error("root_cvt::tell fails: invalid io status");
        }
    }

    /**
     * @lang{ZH}
     * 绝对定位：将流位置移动到距流起点 pos 个字符处。
     * 先刷出写缓冲区（如处于输出状态），再对底层设备调用 dseek(pos + m_bos_len)。
     * 若处于输入状态，读缓冲区将被标记为空（强制下次 get() 从设备重新填充）。
     * @endif
     *
     * @lang{EN}
     * Absolute seek: moves the stream position to pos characters from the stream origin.
     * Flushes the write buffer first (when in output mode), then calls
     * dseek(pos + m_bos_len) on the underlying device. If in input mode, the read
     * buffer is marked empty so the next get() refills from the device.
     * @endif
     *
     * @param pos
     * @lang{ZH} 目标位置，相对于流起点（m_bos_len）的字符偏移量。 @endif
     * @lang{EN} Target position as a character offset from the stream origin (m_bos_len). @endif
     */
    void seek(size_t pos)
        requires (dev_cpt::support_positioning<device_type>)
    {
        if constexpr (dev_cpt::support_put<device_type>)
        {
            if (m_io_status == io_status::output)
                flush();
        }

        if (pos > std::numeric_limits<size_t>::max() - m_bos_len)
            throw cvt_error("root_cvt::seek fails: position overflow");

        m_device.dseek(pos + m_bos_len);
        if (m_io_status == io_status::input)
            m_buf_cur = m_buf_end;
    }

    /**
     * @lang{ZH}
     * 反向定位：将流位置移动到距流末尾 pos 个字符处。
     * 先刷出写缓冲区（如处于输出状态），再验证 pos 未超出流范围，
     * 然后对底层设备调用 drseek(pos)。
     * @endif
     *
     * @lang{EN}
     * Reverse seek: moves the stream position to pos characters before the end of stream.
     * Flushes the write buffer first (when in output mode), validates that pos is within
     * bounds, then calls drseek(pos) on the underlying device.
     * @endif
     *
     * @param pos
     * @lang{ZH} 目标位置，相对于流末尾的字符偏移量（0 表示末尾）。 @endif
     * @lang{EN} Target position as a character offset from the end of stream (0 = end). @endif
     * @throws cvt_error
     * @lang{ZH} pos 超出流边界，或 m_bos_len 大于设备总大小时抛出。 @endif
     * @lang{EN} Thrown when pos exceeds stream bounds or m_bos_len exceeds device size. @endif
     */
    void rseek(size_t pos)
        requires (dev_cpt::support_positioning<device_type>)
    {
        if constexpr (dev_cpt::support_put<device_type>)
        {
            if (m_io_status == io_status::output)
                flush();
        }

        const size_t size_from_device = m_device.dsize();
        if (m_bos_len > size_from_device)
            throw cvt_error("root_cvt::rseek fails: bos_len exceeds device size");
        if (size_from_device - m_bos_len < pos)
            throw cvt_error("root_cvt::rseek fails: out of boundary");

        m_device.drseek(pos);
        if (m_io_status == io_status::input)
            m_buf_cur = m_buf_end;
    }

    /**
     * @lang{ZH}
     * 切换到读取模式。若已处于输入状态则为空操作；若处于输出状态则先刷出缓冲区。
     * 切换后读缓冲区被清空，下次 get() 将从设备重新填充。
     * 仅当设备同时支持读写时可用。
     * @endif
     *
     * @lang{EN}
     * Switches to read (input) mode. No-op if already in input mode; if in output mode,
     * flushes the write buffer first. After switching, the read buffer is cleared so
     * the next get() refills from the device.
     * Only available when the device supports both reading and writing.
     * @endif
     */
    void switch_to_get()
        requires (dev_cpt::support_get<device_type> && dev_cpt::support_put<device_type>)
    {
        switch(m_io_status)
        {
        case io_status::input:
            return;
        case io_status::output:
            flush();
            [[fallthrough]];
        default:
            m_io_status = io_status::input;
            m_buf_cur = m_buf_end = m_buffer.data();
        }
    }

    /**
     * @lang{ZH}
     * 切换到写入模式。若已处于输出状态则为空操作。
     * 若处于输入状态且缓冲区中存在未消费数据，则先将设备位置同步到当前逻辑读取位置
     * （需要设备支持定位；不支持则抛出异常）。
     * 切换后写缓冲区被清空。
     * 仅当设备同时支持读写时可用。
     * @endif
     *
     * @lang{EN}
     * Switches to write (output) mode. No-op if already in output mode.
     * If in input mode with unconsumed buffered data, seeks the device to the current
     * logical read position first (requires the device to support positioning; throws
     * if not supported). After switching, the write buffer is cleared.
     * Only available when the device supports both reading and writing.
     * @endif
     *
     * @throws cvt_error
     * @lang{ZH} 处于输入状态且缓冲区有未消费数据，但设备不支持定位时抛出。 @endif
     * @lang{EN} Thrown when in input mode with unconsumed data and the device does not
     *           support positioning. @endif
     */
    void switch_to_put()
        requires (dev_cpt::support_get<device_type> && dev_cpt::support_put<device_type>)
    {
        switch(m_io_status)
        {
        case io_status::output:
            return;
        case io_status::input:
            if (m_buf_cur != m_buf_end)
            {
                if constexpr (!dev_cpt::support_positioning<device_type>)
                    throw cvt_error("root_cvt::switch_to_put fails: device does not support positioning");
                seek(tell());
            }
            [[fallthrough]];
        default:
            m_io_status = io_status::output;
            m_buf_cur = m_buf_end = m_buffer.data();
        }
    }

private:
    /** @lang{ZH} 底层 I/O 设备实例。 @endif @lang{EN} The underlying I/O device instance. @endif */
    device_type                 m_device;
    /** @lang{ZH} 流起点的设备偏移量，由 main_cont_beg() 记录。tell()/seek() 用此值将逻辑偏移转换为设备偏移。 @endif
     *  @lang{EN} Device offset of the stream origin, recorded by main_cont_beg(). Used by tell()/seek() to translate between logical and device offsets. @endif */
    size_t                      m_bos_len = 0;
    // std::vector is intentionally used over std::array: move operations are O(1)
    // (pointer swap), whereas std::array move degrades to a full element-wise copy.
    /** @lang{ZH} 读写公用的内部缓冲区。使用 std::vector 而非 std::array，以确保移动操作为 O(1)（指针交换）；std::array 的移动会退化为逐元素拷贝。 @endif
     *  @lang{EN} Internal buffer shared by read and write paths. std::vector is used instead of std::array so that move operations are O(1) (pointer swap); std::array move degrades to element-wise copy. @endif */
    std::vector<external_type>  m_buffer;
    /** @lang{ZH} 读模式下：下一个可读字符的指针；写模式下：下一个可写位置的指针。移后对象此指针为 nullptr。 @endif
     *  @lang{EN} In read mode: pointer to the next character to consume; in write mode: pointer to the next write position. Set to nullptr in moved-from objects. @endif */
    external_type*              m_buf_cur;
    /** @lang{ZH} 读模式下：缓冲区有效数据的末尾（past-the-end）指针；写模式下：与 m_buf_cur 共同维护写区域，通常等于 m_buffer.data()。 @endif
     *  @lang{EN} In read mode: past-the-end pointer of valid buffered data; in write mode: maintained alongside m_buf_cur to track the write region. @endif */
    external_type*              m_buf_end;
    /** @lang{ZH} 当前 I/O 方向（neutral / input / output）。 @endif
     *  @lang{EN} Current I/O direction (neutral / input / output). @endif */
    io_status                   m_io_status = io_status::neutral;
};

/**
 * @lang{ZH}
 * root_cvt 针对 mem_device 的偏特化。
 *
 * mem_device 直接暴露内部缓冲区指针，因此不需要额外的内部缓冲区。
 * HasInBuffer 参数被忽略：rb_root_cvt 和 no_rb_root_cvt 均可作为包装器，两者行为完全相同。
 *
 * 由于 mem_device 始终支持随机定位，且无需缓冲层，大多数方法直接转发给设备。
 *
 * @note 调用顺序：成员函数必须按以下顺序调用：
 *       1. **bos()**：必须首先调用，以建立初始 IO 状态。
 *       2. **BOS 阶段**：可多次调用 `get()` 或多次调用 `put()`，但不可混合调用。
 *          此阶段用于读写流起始数据（如文件头）。
 *       3. **main_cont_beg()**：标记 BOS 阶段结束，进入主内容阶段。
 *       4. **主内容阶段**：可调用所有其他成员函数（`get`、`put`、`flush`、
 *          `tell`、`seek`、`rseek`、`switch_to_get`、`switch_to_put` 等）。
 *       违反此调用顺序将导致未定义行为。
 * @endif
 *
 * @lang{EN}
 * Partial specialization of root_cvt for mem_device.
 *
 * mem_device directly exposes its internal buffer pointers and therefore never needs
 * an additional internal buffer. The HasInBuffer parameter is ignored: both rb_root_cvt
 * and no_rb_root_cvt are valid wrappers and behave identically here.
 *
 * Because mem_device always supports random positioning and requires no buffering layer,
 * most methods forward directly to the device.
 *
 * @note Calling sequence: Member functions must be called in the following order:
 *       1. **bos()**: Must be called first to establish the initial IO status.
 *       2. **BOS phase**: May call `get()` multiple times or `put()` multiple times,
 *          but must not mix get and put calls. This phase is for reading/writing
 *          stream-leading data (e.g., file headers).
 *       3. **main_cont_beg()**: Marks the end of the BOS phase and enters the
 *          main-content phase.
 *       4. **Main-content phase**: All other member functions may be called
 *          (`get`, `put`, `flush`, `tell`, `seek`, `rseek`, `switch_to_get`,
 *          `switch_to_put`, etc.).
 *       Violating this calling sequence results in undefined behavior.
 * @endif
 *
 * @tparam CharT
 * @lang{ZH} 字符类型，与 mem_device 的模板参数一致。 @endif
 * @lang{EN} Character type, matching the mem_device template parameter. @endif
 *
 * @tparam Traits
 * @lang{ZH} 字符 traits 类型，与 mem_device 的模板参数一致。 @endif
 * @lang{EN} Character traits type, matching the mem_device template parameter. @endif
 *
 * @tparam Allocator
 * @lang{ZH} 分配器类型，与 mem_device 的模板参数一致。 @endif
 * @lang{EN} Allocator type, matching the mem_device template parameter. @endif
 *
 * @tparam HasInBuffer
 * @lang{ZH} 忽略。mem_device 直接暴露缓冲区指针，无需内部缓冲层。 @endif
 * @lang{EN} Ignored. mem_device directly exposes buffer pointers; no internal buffer layer is needed. @endif
 */
// HasInBuffer is ignored for mem_device specialization:
// mem_device directly exposes buffer pointers and never needs an internal buffer.
// Both rb_root_cvt and no_rb_root_cvt are valid wrappers and behave identically here.
template <typename CharT,
          typename Traits,
          typename Allocator,
          bool HasInBuffer>
class root_cvt<mem_device<CharT, Traits, Allocator>, HasInBuffer>
{
public:
    /**
     * @lang{ZH} 底层设备类型（mem_device 特化）。 @endif
     * @lang{EN} Underlying device type (mem_device specialization). @endif
     */
    using device_type = mem_device<CharT, Traits, Allocator>;

    /**
     * @lang{ZH} 上层转换器所使用的字符类型。 @endif
     * @lang{EN} Character type used by upper-level converters. @endif
     */
    using internal_type = typename device_type::char_type;

    /**
     * @lang{ZH} 设备侧的字符类型（与 internal_type 相同）。 @endif
     * @lang{EN} Character type on the device side (same as internal_type). @endif
     */
    using external_type = typename device_type::char_type;

public:
    /**
     * @lang{ZH}
     * 构造函数：接管给定的 mem_device。
     * @endif
     *
     * @lang{EN}
     * Constructor: takes ownership of the given mem_device.
     * @endif
     *
     * @param device
     * @lang{ZH} 要接管的内存设备（按值传入，触发移动语义）。 @endif
     * @lang{EN} The memory device to take ownership of (passed by value, triggering move semantics). @endif
     */
    explicit root_cvt(device_type device)
        : m_device(std::move(device)) {}

    /**
     * @lang{ZH}
     * 拷贝构造函数：拷贝设备状态、流起点偏移量和 I/O 方向。
     * @endif
     *
     * @lang{EN}
     * Copy constructor: copies device state, stream origin offset, and I/O direction.
     * @endif
     *
     * @param val
     * @lang{ZH} 源对象。 @endif
     * @lang{EN} Source object. @endif
     */
    root_cvt(const root_cvt& val)
        : m_device(val.m_device)
        , m_bos_len(val.m_bos_len)
        , m_io_status(val.m_io_status)
    {}

    /**
     * @lang{ZH}
     * 移动构造函数：转移设备所有权、流起点偏移量和 I/O 方向。
     * @endif
     *
     * @lang{EN}
     * Move constructor: transfers device ownership, stream origin offset, and I/O direction.
     * @endif
     *
     * @param val
     * @lang{ZH} 源对象，移动后处于有效但未指定的状态。 @endif
     * @lang{EN} Source object, left in a valid but unspecified state after the move. @endif
     */
    root_cvt(root_cvt&& val) noexcept
        : m_device(std::move(val.m_device))
        , m_bos_len(val.m_bos_len)
        , m_io_status(val.m_io_status)
    {}

    /**
     * @lang{ZH}
     * 拷贝赋值运算符。
     * @endif
     *
     * @lang{EN}
     * Copy assignment operator.
     * @endif
     *
     * @param val
     * @lang{ZH} 源对象。 @endif
     * @lang{EN} Source object. @endif
     * @return
     * @lang{ZH} *this 的引用。 @endif
     * @lang{EN} Reference to *this. @endif
     */
    root_cvt& operator=(const root_cvt& val)
    {
        if (this == &val) return *this;
        m_device = val.m_device;
        m_bos_len = val.m_bos_len;
        m_io_status = val.m_io_status;
        return *this;
    }

    /**
     * @lang{ZH}
     * 移动赋值运算符：转移设备所有权，源对象 io_status 重置为 neutral。
     * @endif
     *
     * @lang{EN}
     * Move assignment operator: transfers device ownership; the source object's
     * io_status is reset to neutral.
     * @endif
     *
     * @param val
     * @lang{ZH} 源对象，赋值后处于有效但未指定的状态。 @endif
     * @lang{EN} Source object, left in a valid but unspecified state after the assignment. @endif
     * @return
     * @lang{ZH} *this 的引用。 @endif
     * @lang{EN} Reference to *this. @endif
     */
    root_cvt& operator=(root_cvt&& val) noexcept
    {
        if (this == &val) return *this;
        m_device = std::move(val.m_device);
        m_bos_len = val.m_bos_len;
        m_io_status = val.m_io_status;
        val.m_io_status = io_status::neutral;
        return *this;
    }

    /**
     * @lang{ZH}
     * 析构函数：mem_device 不需要刷新操作，因此为默认析构。
     * @endif
     *
     * @lang{EN}
     * Destructor: mem_device requires no flush, so this is defaulted.
     * @endif
     */
    ~root_cvt() = default;

public:
    /**
     * @lang{ZH} 返回底层 mem_device 的引用。 @endif
     * @lang{EN} Returns a reference to the underlying mem_device. @endif
     *
     * @return
     * @lang{ZH} 底层 device_type 的可变引用。 @endif
     * @lang{EN} Mutable reference to the underlying device_type. @endif
     */
    device_type& device() { return m_device; }

    /**
     * @lang{ZH}
     * 将底层 mem_device 从 root_cvt 中分离并返回。
     * mem_device 无需刷新，无清理动作可抛出异常；返回 pair 的 `second` 始终为 `nullptr`。
     * 调用后，root_cvt 内部状态被重置为初始状态。本函数为 `noexcept`。
     * @endif
     *
     * @lang{EN}
     * Detaches the underlying mem_device from root_cvt and returns it.
     * No flush is needed for mem_device and no cleanup step can throw; the returned
     * pair's `second` is always `nullptr`. After this call, root_cvt's internal state
     * is reset to initial values. This function is `noexcept`.
     * @endif
     *
     * @return
     * @lang{ZH} pair：`first` 为已分离的设备对象（通过移动语义返回），`second` 始终为 `nullptr`。 @endif
     * @lang{EN} A pair: `first` is the detached device (returned via move); `second` is always `nullptr`. @endif
     */
    std::pair<device_type, std::exception_ptr> detach() noexcept
    {
        m_bos_len = 0;
        m_io_status = io_status::neutral;
        return {std::move(m_device), nullptr};
    }

    /**
     * @lang{ZH}
     * 分离当前 mem_device，附加新设备，并将内部状态重置为初始值。
     *
     * 与 `detach()` 不同的是，本函数**不**将原 mem_device 归还给调用方——
     * 原 mem_device 通过移动赋值被新设备覆盖（即静默析构）。如需保留原
     * mem_device（例如取出其缓冲区内容），调用方应先单独调用 `detach()`
     * 取得设备，再调用本函数装入新设备。
     *
     * mem_device 无清理动作，本函数不会抛出异常。
     * @endif
     *
     * @lang{EN}
     * Detaches the current mem_device, attaches a new device, and resets
     * internal state to initial values.
     *
     * Unlike `detach()`, this function does NOT hand the old mem_device back
     * to the caller — the old mem_device is overwritten by move-assignment
     * with the new device (i.e. silently destroyed). Callers who need to
     * preserve the old mem_device (e.g. to extract its buffered contents) must
     * call `detach()` first to retrieve it, then call this function to install
     * the new device.
     *
     * mem_device has no cleanup step, so this function never throws.
     * @endif
     *
     * @param dev
     * @lang{ZH} 要附加的新设备，默认为默认构造的空设备。 @endif
     * @lang{EN} The new device to attach; defaults to a default-constructed empty device. @endif
     */
    void attach(device_type&& dev = device_type{})
    {
        m_device = std::move(dev);
        m_bos_len = 0;
        m_io_status = io_status::neutral;
    }

    /**
     * @lang{ZH} 调整转换器行为参数，对此特化为空操作。 @endif
     * @lang{EN} Adjusts converter behavior parameters; no-op for this specialization. @endif
     *
     * @param
     * @lang{ZH} 忽略的行为配置对象。 @endif
     * @lang{EN} Ignored behavior configuration object. @endif
     */
    void adjust(const cvt_behavior&) {}

    /**
     * @lang{ZH} 检索转换器状态，对此特化为空操作。 @endif
     * @lang{EN} Retrieves converter status; no-op for this specialization. @endif
     *
     * @param
     * @lang{ZH} 忽略的状态输出对象。 @endif
     * @lang{EN} Ignored status output object. @endif
     */
    void retrieve(cvt_status&) const {}

    /**
     * @lang{ZH}
     * 标记连续数据块的开始，将当前设备位置记录为流起点（m_bos_len）。
     * 必须先调用 bos() 建立 io_status。
     * @endif
     *
     * @lang{EN}
     * Marks the beginning of a contiguous data block by recording the current device
     * position as the stream origin (m_bos_len).
     * bos() must be called before this to establish io_status.
     * @endif
     */
    void main_cont_beg()
    {
        // bos() must be called before main_cont_beg() to establish io_status
        assert(m_io_status != io_status::neutral);
        m_bos_len = m_device.dtell();
    }

    /**
     * @lang{ZH}
     * 确定并设置初始 I/O 方向。当 deof() 为 true 时进入输出模式，否则进入输入模式。
     *
     * @par 设计说明
     * 初始方向基于流是否包含数据来确定，这是为了保护数据完整性：
     * - **流有内容（deof() 为 false）→ 输入模式**：避免意外覆盖现有数据。
     * - **流无内容（deof() 为 true）→ 输出模式**：空流没有可读内容，只能写入。
     *
     * 若需覆盖非空流，应先清空内容，而非强制进入输出模式。
     * @endif
     *
     * @lang{EN}
     * Determines and sets the initial I/O direction. Enters output mode when deof() is
     * true, otherwise enters input mode.
     *
     * @par Design Rationale
     * The initial direction is determined by whether the stream contains data.
     * This design protects data integrity:
     * - **Stream has content (deof() is false) → input mode**: Prevents accidental
     *   overwriting of existing data.
     * - **Stream is empty (deof() is true) → output mode**: An empty stream has
     *   nothing to read, so writing is the only meaningful operation.
     *
     * To overwrite a non-empty stream, clear its content first rather than
     * forcing output mode.
     * @endif
     *
     * @return
     * @lang{ZH} 确定后的 io_status（input 或 output）。 @endif
     * @lang{EN} The resolved io_status (input or output). @endif
     */
    io_status bos()
    {
        m_io_status = (m_device.deof()) ? io_status::output : io_status::input;
        return m_io_status;
    }

    /**
     * @lang{ZH}
     * 判断当前流是否已到达末尾（EOF），直接转发给 mem_device::deof()。
     * @endif
     *
     * @lang{EN}
     * Tests whether the current stream has reached end of file (EOF),
     * forwarding directly to mem_device::deof().
     * @endif
     *
     * @return
     * @lang{ZH} 若已到达 EOF 则返回 true。 @endif
     * @lang{EN} true if end of file has been reached. @endif
     */
    bool is_eof()
    {
        return m_device.deof();
    }

    /**
     * @lang{ZH}
     * 从 mem_device 读取最多 to_max 个字符到 to 指向的缓冲区。
     * 先切换到输入模式，再直接转发给设备的 dget()。
     * @endif
     *
     * @lang{EN}
     * Reads up to to_max characters from mem_device into the buffer pointed to by to.
     * Switches to input mode first, then forwards directly to the device's dget().
     * @endif
     *
     * @param to
     * @lang{ZH} 接收数据的目标缓冲区指针。 @endif
     * @lang{EN} Pointer to the destination buffer that receives the data. @endif
     * @param to_max
     * @lang{ZH} 最多读取的字符数。 @endif
     * @lang{EN} Maximum number of characters to read. @endif
     * @return
     * @lang{ZH} 实际读取的字符数。 @endif
     * @lang{EN} The actual number of characters read. @endif
     */
    size_t get(internal_type* to, size_t to_max)
    {
        switch_to_get();
        return m_device.dget(to, to_max);
    }

    /**
     * @lang{ZH}
     * 将 to_size 个字符写入 mem_device。
     * 先切换到输出模式，再直接转发给设备的 dput()。
     * @endif
     *
     * @lang{EN}
     * Writes to_size characters to mem_device.
     * Switches to output mode first, then forwards directly to the device's dput().
     * @endif
     *
     * @param to
     * @lang{ZH} 要写入的数据来源指针。 @endif
     * @lang{EN} Pointer to the source data to write. @endif
     * @param to_size
     * @lang{ZH} 要写入的字符数。 @endif
     * @lang{EN} Number of characters to write. @endif
     */
    void put(const internal_type* to, size_t to_size)
    {
        switch_to_put();
        m_device.dput(to, to_size);
    }

    /**
     * @lang{ZH}
     * 刷新操作，对此特化为空操作。mem_device 是纯内存设备，写入即生效，无需刷新。
     * @endif
     *
     * @lang{EN}
     * Flush operation; no-op for this specialization. mem_device is a pure in-memory
     * device where writes take effect immediately, so no flush is needed.
     * @endif
     */
    void flush() {}

    /**
     * @lang{ZH}
     * 返回当前流位置（相对于 m_bos_len 的偏移量）。
     * 直接转发给 mem_device::dtell()，减去流起点偏移量。
     * @endif
     *
     * @lang{EN}
     * Returns the current stream position (offset relative to m_bos_len).
     * Forwards directly to mem_device::dtell() minus the stream origin offset.
     * @endif
     *
     * @return
     * @lang{ZH} 相对于流起点的当前位置偏移（字符数）。 @endif
     * @lang{EN} Current position offset from the stream origin in characters. @endif
     */
    [[nodiscard]] size_t tell() const
    {
        const size_t device_tell = m_device.dtell();
        // Invariant: device_tell >= m_bos_len. The device position should never
        // move before the stream origin established by main_cont_beg().
        assert(device_tell >= m_bos_len);
        return device_tell - m_bos_len;
    }

    /**
     * @lang{ZH}
     * 绝对定位：将流位置移动到距流起点 pos 个字符处。
     * 直接对底层设备调用 dseek(pos + m_bos_len)，无需刷新（mem_device 无写缓冲区）。
     * @endif
     *
     * @lang{EN}
     * Absolute seek: moves the stream position to pos characters from the stream origin.
     * Calls dseek(pos + m_bos_len) on the underlying device directly; no flush is
     * needed since mem_device has no write buffer.
     * @endif
     *
     * @param pos
     * @lang{ZH} 目标位置，相对于流起点的字符偏移量。 @endif
     * @lang{EN} Target position as a character offset from the stream origin. @endif
     */
    void seek(size_t pos)
    {
        if (pos > std::numeric_limits<size_t>::max() - m_bos_len)
            throw cvt_error("root_cvt::seek fails: position overflow");

        m_device.dseek(pos + m_bos_len);
    }

    /**
     * @lang{ZH}
     * 反向定位：将流位置移动到距流末尾 pos 个字符处。
     * 验证 pos 未超出流范围，再对底层设备调用 drseek(pos)。
     * @endif
     *
     * @lang{EN}
     * Reverse seek: moves the stream position to pos characters before the end of stream.
     * Validates that pos is within bounds, then calls drseek(pos) on the underlying device.
     * @endif
     *
     * @param pos
     * @lang{ZH} 目标位置，相对于流末尾的字符偏移量（0 表示末尾）。 @endif
     * @lang{EN} Target position as a character offset from the end of stream (0 = end). @endif
     * @throws cvt_error
     * @lang{ZH} pos 超出流边界，或 m_bos_len 大于设备总大小时抛出。 @endif
     * @lang{EN} Thrown when pos exceeds stream bounds or m_bos_len exceeds device size. @endif
     */
    void rseek(size_t pos)
    {
        const size_t size_from_device = m_device.dsize();
        if (m_bos_len > size_from_device)
            throw cvt_error("root_cvt::rseek fails: bos_len exceeds device size");
        if (size_from_device - m_bos_len < pos)
            throw cvt_error("root_cvt::rseek fails: out of boundary");

        m_device.drseek(pos);
    }

    /**
     * @lang{ZH}
     * 切换到读取模式，直接将 io_status 设置为 input。
     * mem_device 支持随机访问，切换无需任何额外操作。
     * @endif
     *
     * @lang{EN}
     * Switches to read (input) mode by setting io_status to input directly.
     * mem_device supports random access, so the switch requires no additional operations.
     * @endif
     */
    void switch_to_get()
    {
        m_io_status = io_status::input;
    }

    /**
     * @lang{ZH}
     * 切换到写入模式，直接将 io_status 设置为 output。
     * mem_device 支持随机访问，切换无需任何额外操作。
     * @endif
     *
     * @lang{EN}
     * Switches to write (output) mode by setting io_status to output directly.
     * mem_device supports random access, so the switch requires no additional operations.
     * @endif
     */
    void switch_to_put()
    {
        m_io_status = io_status::output;
    }

private:
    /** @lang{ZH} 底层 mem_device 实例。 @endif @lang{EN} The underlying mem_device instance. @endif */
    device_type                 m_device;
    /** @lang{ZH} 流起点的设备偏移量，由 main_cont_beg() 记录。 @endif
     *  @lang{EN} Device offset of the stream origin, recorded by main_cont_beg(). @endif */
    size_t                      m_bos_len = 0;
    /** @lang{ZH} 当前 I/O 方向（neutral / input / output）。 @endif
     *  @lang{EN} Current I/O direction (neutral / input / output). @endif */
    io_status                   m_io_status = io_status::neutral;
};

/**
 * @lang{ZH}
 * root_cvt 的便捷别名：启用内部读取缓冲区（HasInBuffer = true）。
 * 适用于需要回滚（rollback）能力的场景，因为缓冲区中保留了已读但未消费的数据。
 * @endif
 *
 * @lang{EN}
 * Convenience alias for root_cvt with the internal read buffer enabled (HasInBuffer = true).
 * Use this when rollback capability is needed, since the buffer retains already-read but
 * unconsumed data.
 * @endif
 *
 * @tparam DeviceType
 * @lang{ZH} 底层 I/O 设备类型，须满足 io_device 概念。 @endif
 * @lang{EN} The underlying I/O device type, which must satisfy the io_device concept. @endif
 */
template <io_device DeviceType>
class rb_root_cvt : public root_cvt<DeviceType, true>
{
    using root_cvt<DeviceType, true>::root_cvt;
};

template <io_device DeviceType>
rb_root_cvt(DeviceType) -> rb_root_cvt<DeviceType>;

/**
 * @lang{ZH}
 * root_cvt 的便捷别名：禁用内部读取缓冲区（HasInBuffer = false）。
 * 每次 get() 调用直接转发给设备，不在转换层维护读缓冲区。
 * 适用于不需要回滚、或设备本身已有缓冲的场景。
 * @endif
 *
 * @lang{EN}
 * Convenience alias for root_cvt with the internal read buffer disabled (HasInBuffer = false).
 * Each get() call is forwarded directly to the device without maintaining a read buffer
 * in the converter layer. Use this when rollback is not needed or the device already buffers.
 * @endif
 *
 * @tparam DeviceType
 * @lang{ZH} 底层 I/O 设备类型，须满足 io_device 概念。 @endif
 * @lang{EN} The underlying I/O device type, which must satisfy the io_device concept. @endif
 */
template <io_device DeviceType>
class no_rb_root_cvt : public root_cvt<DeviceType, false>
{
    using root_cvt<DeviceType, false>::root_cvt;
};

template <io_device DeviceType>
no_rb_root_cvt(DeviceType) -> no_rb_root_cvt<DeviceType>;

/**
 * @lang{ZH}
 * cvt_reader 针对具有内部缓冲区的 root_cvt（HasInBuffer = true，非 mem_device）的偏特化。
 * 直接复用 root_cvt 内部缓冲区，避免额外的数据拷贝。
 * @endif
 *
 * @lang{EN}
 * cvt_reader specialization for root_cvt with an internal buffer (HasInBuffer = true,
 * non-mem_device). Directly reuses root_cvt's internal buffer to avoid extra data copies.
 * @endif
 *
 * @tparam KernelType
 * @lang{ZH} 须派生自 root_cvt&lt;DeviceType, true&gt; 且 DeviceType 非 mem_device。 @endif
 * @lang{EN} Must derive from root_cvt&lt;DeviceType, true&gt; with a non-mem_device DeviceType. @endif
 */
// cvt_reader specialization for root_cvt with buffer: directly use root_cvt's internal buffer
template <io_converter KernelType>
    requires (std::is_base_of_v<root_cvt<typename KernelType::device_type, true>, KernelType> &&
              !is_mem_device<typename KernelType::device_type>)
class cvt_reader<KernelType>
{
    using device_type = typename KernelType::device_type;
    using char_type = typename KernelType::internal_type;

public:
    /**
     * @lang{ZH}
     * 构造函数：绑定到指定的 root_cvt 内核。外部缓冲区参数被忽略，直接使用内核的内部缓冲区。
     * @endif
     *
     * @lang{EN}
     * Constructor: binds to the specified root_cvt kernel. The external buffer argument
     * is ignored; the kernel's internal buffer is used directly.
     * @endif
     *
     * @param kernel
     * @lang{ZH} 要绑定的 root_cvt 内核引用。 @endif
     * @lang{EN} Reference to the root_cvt kernel to bind to. @endif
     */
    explicit cvt_reader(KernelType& kernel, std::vector<char_type>&)
        : m_kernel(kernel) {}

    cvt_reader(const cvt_reader&) = delete;
    cvt_reader& operator=(const cvt_reader&) = delete;
    cvt_reader(cvt_reader&&) = delete;
    cvt_reader& operator=(cvt_reader&&) = delete;
    ~cvt_reader() = default;

    /**
     * @lang{ZH} 重置读取状态，对此特化为空操作（缓冲区由内核管理）。 @endif
     * @lang{EN} Resets read state; no-op for this specialization as the buffer is managed by the kernel. @endif
     *
     * @param
     * @lang{ZH} 忽略的缓冲区大小参数。 @endif
     * @lang{EN} Ignored buffer size parameter. @endif
     */
    void reset(size_t) {}

    /**
     * @lang{ZH}
     * 从 root_cvt 的内部缓冲区获取数据，缓冲区不足时自动从设备读取补充。
     * @endif
     *
     * @lang{EN}
     * Retrieves data from root_cvt's internal buffer, reading from device if needed.
     * @endif
     *
     * @tparam Saturate
     * @lang{ZH}
     * 控制数据不足时的行为：
     * - false（默认）：返回当前可用数据（可能少于请求量），返回类型为 std::pair&lt;const char_type*, size_t&gt;。
     * - true：要求恰好 to_max 个字符，不足则抛异常，返回类型为 const char_type*。
     * @endif
     * @lang{EN}
     * Controls behavior when insufficient data is available:
     * - false (default): Returns available data (may be less than requested).
     *                    Return type: std::pair&lt;const char_type*, size_t&gt;.
     * - true: Requires exactly to_max chars; throws if not enough data.
     *         Return type: const char_type*.
     * @endif
     *
     * @param to_max
     * @lang{ZH} 要读取的字符数，不得超过 s_buffer_length。 @endif
     * @lang{EN} Number of characters to retrieve; must not exceed s_buffer_length. @endif
     * @return
     * @lang{ZH} Saturate=false 时返回 (指针, 实际数量) 对；Saturate=true 时返回恰好 to_max 个字符的起始指针。 @endif
     * @lang{EN} When Saturate=false: pair of (pointer, actual_count).
     *           When Saturate=true: pointer to exactly to_max chars. @endif
     * @throws cvt_error
     * @lang{ZH}
     * - to_max 为 0 时。
     * - to_max 超过 s_buffer_length 时。
     * - io_status 不为 input 且无法切换时。
     * - Saturate=true 时在读满前遇到流末尾。
     * @endif
     * @lang{EN}
     * - When to_max is 0.
     * - When to_max exceeds s_buffer_length.
     * - When io_status is not input and cannot be switched.
     * - When Saturate=true and end-of-stream is reached before reading to_max characters.
     * @endif
     */
    template <bool Saturate = false>
    auto get_buf(size_t to_max)
    {
        if (to_max == 0)
            throw cvt_error("cvt_reader::get_buf fail, read size cannot be zero");
        if (to_max > KernelType::s_buffer_length)
            throw cvt_error("cvt_reader::get_buf fail, read size too large");

        if constexpr (dev_cpt::support_put<device_type>)
        {
            if (m_kernel.m_io_status != io_status::input)
                m_kernel.switch_to_get();
        }
        if (m_kernel.m_io_status != io_status::input)
            throw cvt_error("cvt_reader::get_buf fail, invalid io status");

        const size_t remain = m_kernel.m_buf_end - m_kernel.m_buf_cur;
        // need to read, then read at most as it can
        if (remain < to_max)
        {
            if (remain != 0)
                m_kernel.m_buf_end = std::copy(m_kernel.m_buf_cur, m_kernel.m_buf_end, m_kernel.m_buffer.data());
            else
                m_kernel.m_buf_end = m_kernel.m_buffer.data();

            m_kernel.m_buf_cur = m_kernel.m_buffer.data();
            m_kernel.m_buf_end += m_kernel.m_device.dget(m_kernel.m_buf_end, KernelType::s_buffer_length - remain);
        }

        if constexpr (Saturate)
        {
            const auto aim_ptr = to_max + m_kernel.m_buf_cur;
            while (m_kernel.m_buf_end < aim_ptr)
            {
                auto new_size = m_kernel.m_device.dget(m_kernel.m_buf_end, aim_ptr - m_kernel.m_buf_end);
                if (new_size == 0)
                    throw cvt_error("get_buf<Saturate> fail: meet eos");
                m_kernel.m_buf_end += new_size;
            }
        }

        if constexpr (!Saturate)
        {
            auto res_len = std::min<size_t>(m_kernel.m_buf_end - m_kernel.m_buf_cur, to_max);
            std::pair<const char_type*, size_t> res{m_kernel.m_buf_cur, res_len};
            m_kernel.m_buf_cur += res_len;
            return res;
        }
        else
        {
            const char_type* res = m_kernel.m_buf_cur;
            m_kernel.m_buf_cur += to_max;
            return res;
        }
    }

    /**
     * @lang{ZH}
     * 将读取游标向后退 len 个字符，使已被 get_buf() 消费的数据重新可读。
     * 用于解析失败时的回溯。
     * @endif
     *
     * @lang{EN}
     * Moves the read cursor back by len characters, making data previously consumed
     * by get_buf() available to read again. Used for backtracking after a failed parse.
     * @endif
     *
     * @param len
     * @lang{ZH} 要回退的字符数，不得超过当前缓冲区游标距缓冲区起始的距离。 @endif
     * @lang{EN} Number of characters to roll back; must not exceed the current cursor
     *           offset from the buffer start. @endif
     * @throws cvt_error
     * @lang{ZH} len 为 0、len 超出可回退范围、或缓冲区指针为 nullptr 时抛出。 @endif
     * @lang{EN} Thrown when len is 0, len exceeds the rollback range, or the buffer
     *           pointer is nullptr. @endif
     */
    void rollback(size_t len)
    {
        if (len == 0)
            throw cvt_error("cvt_reader::rollback fail, length cannot be zero");
        if ((m_kernel.m_buf_cur == nullptr) || (len > static_cast<size_t>(m_kernel.m_buf_cur - m_kernel.m_buffer.data())))
            throw cvt_error("cvt_reader::rollback fail, rollback length too large");
        m_kernel.m_buf_cur -= len;
    }

private:
    KernelType& m_kernel;
};

/**
 * @lang{ZH}
 * cvt_writer 针对 root_cvt（任意 HasInBuffer，非 mem_device）的偏特化。
 * 直接复用 root_cvt 内部缓冲区，无需额外分配，与 cvt_reader 对称。
 * @endif
 *
 * @lang{EN}
 * cvt_writer specialization for root_cvt (any HasInBuffer, non-mem_device).
 * Directly reuses root_cvt's internal buffer without additional allocation,
 * symmetric with the cvt_reader specialization.
 * @endif
 *
 * @tparam KernelType
 * @lang{ZH} 须派生自 root_cvt&lt;DeviceType, s_has_buffer&gt; 且 DeviceType 非 mem_device。 @endif
 * @lang{EN} Must derive from root_cvt&lt;DeviceType, s_has_buffer&gt; with a non-mem_device DeviceType. @endif
 */
// cvt_writer specialization for root_cvt: directly use root_cvt's internal buffer
template <io_converter KernelType>
    requires (std::is_base_of_v<root_cvt<typename KernelType::device_type, KernelType::s_has_buffer>, KernelType> &&
              !is_mem_device<typename KernelType::device_type>)
class cvt_writer<KernelType>
{
    using device_type = typename KernelType::device_type;
    using char_type = typename KernelType::internal_type;

public:
    /**
     * @lang{ZH}
     * 构造函数：绑定到指定的 root_cvt 内核。外部缓冲区参数被忽略，直接使用内核的内部缓冲区。
     * @endif
     *
     * @lang{EN}
     * Constructor: binds to the specified root_cvt kernel. The external buffer argument
     * is ignored; the kernel's internal buffer is used directly.
     * @endif
     *
     * @param kernel
     * @lang{ZH} 要绑定的 root_cvt 内核引用。 @endif
     * @lang{EN} Reference to the root_cvt kernel to bind to. @endif
     */
    explicit cvt_writer(KernelType& kernel, std::vector<char_type>&)
        : m_kernel(kernel) {}

    cvt_writer(const cvt_writer&) = delete;
    cvt_writer& operator=(const cvt_writer&) = delete;
    cvt_writer(cvt_writer&&) = delete;
    cvt_writer& operator=(cvt_writer&&) = delete;
    ~cvt_writer() = default;

    /**
     * @lang{ZH}
     * 设置每次 put_buf() 调用允许写入的最大字符数。
     * 必须在首次调用 put_buf() 之前调用，以通知写入器本次转换步骤的最大输出量。
     * @endif
     *
     * @lang{EN}
     * Sets the maximum number of characters that each put_buf() call may write.
     * Must be called before the first put_buf() to inform the writer of the maximum
     * output size for this conversion step.
     * @endif
     *
     * @param buf_size
     * @lang{ZH} 允许的最大写入字符数，不得超过 s_buffer_length。 @endif
     * @lang{EN} Maximum number of characters allowed per write; must not exceed s_buffer_length. @endif
     * @throws cvt_error
     * @lang{ZH} buf_size 超过 s_buffer_length 时抛出。 @endif
     * @lang{EN} Thrown when buf_size exceeds s_buffer_length. @endif
     */
    void reset(size_t buf_size)
    {
        m_buf_size = buf_size;
        if (m_buf_size > KernelType::s_buffer_length)
            throw cvt_error("cvt_writer::reset fail: buf_size exceeds buffer capacity");
    }

    /**
     * @lang{ZH}
     * 在 root_cvt 的内部缓冲区中为 len 个字符预留空间，并返回写入起始指针。
     * 若缓冲区剩余空间不足，先将已有缓冲内容刷入设备，再返回缓冲区起始地址。
     * 调用方将数据写入返回的指针后，调用 commit() 提交（本特化中 commit() 为空操作）。
     * @endif
     *
     * @lang{EN}
     * Reserves space for len characters in root_cvt's internal buffer and returns a
     * pointer to the write start. If insufficient space remains, the existing buffer
     * content is flushed to the device first, then the buffer start is returned.
     * The caller writes data into the returned pointer, then calls commit() to
     * finalize (commit() is a no-op in this specialization).
     * @endif
     *
     * @param len
     * @lang{ZH} 要写入的字符数，不得超过 reset() 设置的 buf_size。 @endif
     * @lang{EN} Number of characters to write; must not exceed the buf_size set by reset(). @endif
     * @return
     * @lang{ZH} 可写入 len 个字符的缓冲区指针。 @endif
     * @lang{EN} Pointer into the buffer where len characters may be written. @endif
     * @throws cvt_error
     * @lang{ZH} len 为 0、len 超过 buf_size、或 io_status 不为 output 且无法切换时抛出。 @endif
     * @lang{EN} Thrown when len is 0, len exceeds buf_size, or io_status is not output
     *           and cannot be switched. @endif
     */
    char_type* put_buf(size_t len)
    {
        if (len == 0)
            throw cvt_error("cvt_writer::put_buf fail, write size cannot be zero");
        if (len > m_buf_size)
            throw cvt_error("cvt_writer::put_buf fail, write size too large");

        if constexpr (dev_cpt::support_get<device_type>)
        {
            if (m_kernel.m_io_status != io_status::output)
                m_kernel.switch_to_put();
        }
        if (m_kernel.m_io_status != io_status::output)
            throw cvt_error("cvt_writer::put_buf fail, invalid io status");

        const size_t buf_used = m_kernel.m_buf_cur - m_kernel.m_buffer.data();
        const size_t remain = KernelType::s_buffer_length - buf_used;
        if (len < remain)
        {
            auto res = m_kernel.m_buf_cur;
            m_kernel.m_buf_cur += len;
            return res;
        }

        m_kernel.m_device.dput(m_kernel.m_buffer.data(), buf_used);
        m_kernel.m_buf_cur = m_kernel.m_buffer.data() + len;
        return m_kernel.m_buffer.data();
    }

    /**
     * @lang{ZH}
     * 将写入游标向后退 len 个字符，撤销最近一次 put_buf() 写入的内容。
     * 用于转换失败时丢弃已预留但未实际写入有效数据的空间。
     * @endif
     *
     * @lang{EN}
     * Moves the write cursor back by len characters, undoing the most recent put_buf()
     * reservation. Used to discard reserved but unused space after a conversion failure.
     * @endif
     *
     * @param len
     * @lang{ZH} 要回退的字符数，不得超过当前缓冲区游标距缓冲区起始的距离。 @endif
     * @lang{EN} Number of characters to roll back; must not exceed the current cursor
     *           offset from the buffer start. @endif
     * @throws cvt_error
     * @lang{ZH} len 为 0、len 超出可回退范围、或缓冲区指针为 nullptr 时抛出。 @endif
     * @lang{EN} Thrown when len is 0, len exceeds the rollback range, or the buffer
     *           pointer is nullptr. @endif
     */
    void rollback(size_t len)
    {
        if (len == 0)
            throw cvt_error("cvt_writer::rollback fail, length cannot be zero");
        if ((m_kernel.m_buf_cur == nullptr) || (len > static_cast<size_t>(m_kernel.m_buf_cur - m_kernel.m_buffer.data())))
            throw cvt_error("cvt_writer::rollback fail, rollback length too large");
        m_kernel.m_buf_cur -= len;
    }

    /**
     * @lang{ZH}
     * 提交写入操作，对此特化为空操作。
     * put_buf() 返回的指针直接指向内部缓冲区，写入立即生效，无需额外的提交步骤。
     * @endif
     *
     * @lang{EN}
     * Commits the write operation; no-op for this specialization.
     * The pointer returned by put_buf() points directly into the internal buffer, so
     * writes take effect immediately with no additional commit step required.
     * @endif
     */
    void commit() {}

private:
    KernelType& m_kernel;
    /** @lang{ZH} reset() 设置的每次 put_buf() 最大写入字符数。 @endif
     *  @lang{EN} Maximum characters per put_buf() call, set by reset(). @endif */
    size_t m_buf_size = 0;
};

/**
 * @lang{ZH}
 * cvt_reader 针对 mem_device 的偏特化（适用于 rb_root_cvt 和 no_rb_root_cvt 两种内核）。
 * mem_device 直接暴露内部缓冲区指针，因此所有操作直接转发给设备，无需额外的缓冲层。
 * @endif
 *
 * @lang{EN}
 * cvt_reader specialization for mem_device (applies to both rb_root_cvt and
 * no_rb_root_cvt kernels). mem_device directly exposes its internal buffer pointers,
 * so all operations forward directly to the device without an additional buffering layer.
 * @endif
 *
 * @tparam KernelType
 * @lang{ZH} 须派生自 root_cvt&lt;DeviceType, true|false&gt; 且 DeviceType 须为 mem_device。 @endif
 * @lang{EN} Must derive from root_cvt&lt;DeviceType, true|false&gt; with a mem_device DeviceType. @endif
 */
// cvt_reader specialization for mem_device: directly access device buffer
template <io_converter KernelType>
    requires ((std::is_base_of_v<root_cvt<typename KernelType::device_type, true>, KernelType> ||
               std::is_base_of_v<root_cvt<typename KernelType::device_type, false>, KernelType>) &&
              is_mem_device<typename KernelType::device_type>)
class cvt_reader<KernelType>
{
    using device_type = typename KernelType::device_type;
    using char_type = typename KernelType::internal_type;

public:
    /**
     * @lang{ZH}
     * 构造函数：绑定到指定的 root_cvt 内核（mem_device 特化）。外部缓冲区参数被忽略。
     * @endif
     *
     * @lang{EN}
     * Constructor: binds to the specified root_cvt kernel (mem_device specialization).
     * The external buffer argument is ignored.
     * @endif
     *
     * @param kernel
     * @lang{ZH} 要绑定的 root_cvt 内核引用。 @endif
     * @lang{EN} Reference to the root_cvt kernel to bind to. @endif
     */
    explicit cvt_reader(KernelType& kernel, std::vector<char_type>&)
        : m_kernel(kernel) {}

    cvt_reader(const cvt_reader&) = delete;
    cvt_reader& operator=(const cvt_reader&) = delete;
    cvt_reader(cvt_reader&&) = delete;
    cvt_reader& operator=(cvt_reader&&) = delete;
    ~cvt_reader() = default;

    /**
     * @lang{ZH} 重置读取状态，对此特化为空操作（mem_device 自管理缓冲区）。 @endif
     * @lang{EN} Resets read state; no-op for this specialization as mem_device manages its own buffer. @endif
     *
     * @param
     * @lang{ZH} 忽略的缓冲区大小参数。 @endif
     * @lang{EN} Ignored buffer size parameter. @endif
     */
    void reset(size_t) {}

    /**
     * @lang{ZH}
     * 直接转发给 mem_device::get_buf()，零拷贝返回设备缓冲区内的指针。
     * @endif
     *
     * @lang{EN}
     * Forwards directly to mem_device::get_buf(), returning a zero-copy pointer
     * into the device's own buffer.
     * @endif
     *
     * @tparam Saturate
     * @lang{ZH} 为 true 时要求恰好 to_max 个字符并返回 const char_type*；
     *           为 false（默认）时返回 std::pair&lt;const char_type*, size_t&gt;。 @endif
     * @lang{EN} When true, requires exact count and returns const char_type*;
     *           when false (default), returns std::pair&lt;const char_type*, size_t&gt;. @endif
     *
     * @param to_max
     * @lang{ZH} 要读取的字符数。 @endif
     * @lang{EN} Number of characters to retrieve. @endif
     * @return
     * @lang{ZH} 见 mem_device::get_buf() 的返回值说明。 @endif
     * @lang{EN} See mem_device::get_buf() for return value details. @endif
     */
    template <bool Saturate = false>
    auto get_buf(size_t to_max)
    {
        return m_kernel.device().template get_buf<Saturate>(to_max);
    }

    /**
     * @lang{ZH}
     * 将读取游标向后退 len 个字符，直接转发给 mem_device::get_rollback()。
     * @endif
     *
     * @lang{EN}
     * Moves the read cursor back by len characters, forwarding directly to
     * mem_device::get_rollback().
     * @endif
     *
     * @param len
     * @lang{ZH} 要回退的字符数。 @endif
     * @lang{EN} Number of characters to roll back. @endif
     */
    void rollback(size_t len)
    {
        m_kernel.device().get_rollback(len);
    }

private:
    KernelType& m_kernel;
};

/**
 * @lang{ZH}
 * cvt_writer 针对 mem_device 的偏特化（适用于 rb_root_cvt 和 no_rb_root_cvt 两种内核）。
 * 所有操作直接转发给 mem_device，零拷贝写入设备缓冲区。
 * @endif
 *
 * @lang{EN}
 * cvt_writer specialization for mem_device (applies to both rb_root_cvt and
 * no_rb_root_cvt kernels). All operations forward directly to mem_device,
 * writing zero-copy into the device's own buffer.
 * @endif
 *
 * @tparam KernelType
 * @lang{ZH} 须派生自 root_cvt&lt;DeviceType, true|false&gt; 且 DeviceType 须为 mem_device。 @endif
 * @lang{EN} Must derive from root_cvt&lt;DeviceType, true|false&gt; with a mem_device DeviceType. @endif
 */
// cvt_writer specialization for mem_device: directly access device buffer
template <io_converter KernelType>
    requires ((std::is_base_of_v<root_cvt<typename KernelType::device_type, true>, KernelType> ||
               std::is_base_of_v<root_cvt<typename KernelType::device_type, false>, KernelType>) &&
              is_mem_device<typename KernelType::device_type>)
class cvt_writer<KernelType>
{
    using device_type = typename KernelType::device_type;
    using char_type = typename KernelType::internal_type;

public:
    /**
     * @lang{ZH}
     * 构造函数：绑定到指定的 root_cvt 内核（mem_device 特化）。外部缓冲区参数被忽略。
     * @endif
     *
     * @lang{EN}
     * Constructor: binds to the specified root_cvt kernel (mem_device specialization).
     * The external buffer argument is ignored.
     * @endif
     *
     * @param kernel
     * @lang{ZH} 要绑定的 root_cvt 内核引用。 @endif
     * @lang{EN} Reference to the root_cvt kernel to bind to. @endif
     */
    explicit cvt_writer(KernelType& kernel, std::vector<char_type>&)
        : m_kernel(kernel) {}

    cvt_writer(const cvt_writer&) = delete;
    cvt_writer& operator=(const cvt_writer&) = delete;
    cvt_writer(cvt_writer&&) = delete;
    cvt_writer& operator=(cvt_writer&&) = delete;
    ~cvt_writer() = default;

    /**
     * @lang{ZH} 重置写入状态，对此特化为空操作（mem_device 自管理缓冲区大小）。 @endif
     * @lang{EN} Resets write state; no-op for this specialization as mem_device manages its own buffer size. @endif
     *
     * @param
     * @lang{ZH} 忽略的缓冲区大小参数。 @endif
     * @lang{EN} Ignored buffer size parameter. @endif
     */
    void reset(size_t) {}

    /**
     * @lang{ZH}
     * 在 mem_device 缓冲区中为 len 个字符预留空间，零拷贝返回写入起始指针。
     * 直接转发给 mem_device::put_buf()。
     * @endif
     *
     * @lang{EN}
     * Reserves space for len characters in mem_device's buffer and returns a zero-copy
     * pointer to the write start. Forwards directly to mem_device::put_buf().
     * @endif
     *
     * @param len
     * @lang{ZH} 要写入的字符数。 @endif
     * @lang{EN} Number of characters to write. @endif
     * @return
     * @lang{ZH} 可写入 len 个字符的设备缓冲区指针。 @endif
     * @lang{EN} Pointer into the device buffer where len characters may be written. @endif
     */
    char_type* put_buf(size_t len)
    {
        return m_kernel.device().put_buf(len);
    }

    /**
     * @lang{ZH}
     * 将写入游标向后退 len 个字符，直接转发给 mem_device::put_rollback()。
     * @endif
     *
     * @lang{EN}
     * Moves the write cursor back by len characters, forwarding directly to
     * mem_device::put_rollback().
     * @endif
     *
     * @param len
     * @lang{ZH} 要回退的字符数。 @endif
     * @lang{EN} Number of characters to roll back. @endif
     */
    void rollback(size_t len)
    {
        m_kernel.device().put_rollback(len);
    }

    /**
     * @lang{ZH}
     * 提交写入操作，对此特化为空操作。
     * mem_device 的写入通过 put_buf() 直接修改设备内部状态，无需显式提交。
     * @endif
     *
     * @lang{EN}
     * Commits the write operation; no-op for this specialization.
     * mem_device writes via put_buf() modify the device's internal state directly,
     * so no explicit commit step is required.
     * @endif
     */
    void commit() {}

private:
    KernelType& m_kernel;
};
}
