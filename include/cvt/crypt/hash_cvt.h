/**
 * @file hash_cvt.h
 * @lang{ZH}
 * 哈希摘要转换器。
 *
 * 本文件提供以下组件：
 * - `hash_algo`：支持的哈希算法枚举（MD5、SHA-256）。
 * - `hash_fmt`：哈希摘要输出格式枚举（二进制、大写十六进制、小写十六进制）。
 * - `set_hash_fmt`：用于通过 `adjust()` 动态切换输出格式的行为对象。
 * - `dump_hash`：用于在主内容阶段结束前手动触发哈希输出的行为对象。
 * - `hash_cvt`：只写哈希摘要转换器，在 `detach()` 或 `dump_hash` 行为触发时
 *   将当前哈希结果写入底层 kernel。
 * - `hash_cvt_creator`：工厂类，用于在管道中统一构造 `hash_cvt` 实例。
 * @endif
 *
 * @lang{EN}
 * Hash-digest converter.
 *
 * This file provides the following components:
 * - `hash_algo`: Enumeration of supported hash algorithms (MD5, SHA-256).
 * - `hash_fmt`: Enumeration of hash-digest output formats (binary, upper-hex, lower-hex).
 * - `set_hash_fmt`: Behavior object for dynamically switching the output format via `adjust()`.
 * - `dump_hash`: Behavior object for manually flushing the hash output before the
 *   main-content phase ends.
 * - `hash_cvt`: A write-only hash-digest converter that writes the current hash result
 *   to the underlying kernel on `detach()` or in response to a `dump_hash` behavior.
 * - `hash_cvt_creator`: A factory class for constructing `hash_cvt` instances within
 *   a pipeline.
 * @endif
 */

#pragma once
#include <cvt/abs_cvt.h>
#include <cvt/cvt_concepts.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <botan/hash.h>
#include <botan/hex.h>

namespace IOv2::Crypt
{
/**
 * @lang{ZH}
 * `hash_cvt` 支持的哈希算法。
 * @endif
 *
 * @lang{EN}
 * Hash algorithms supported by `hash_cvt`.
 * @endif
 */
enum class hash_algo : uint8_t
{
    MD5,    ///< @lang{ZH} MD5 算法（128 位）。仅用于兼容性目的；不提供安全性保证。 @endif @lang{EN} MD5 algorithm (128-bit). For compatibility only; provides no security guarantees. @endif
    SHA256, ///< @lang{ZH} SHA-256 算法（256 位）。 @endif @lang{EN} SHA-256 algorithm (256-bit). @endif
};

/**
 * @lang{ZH}
 * 哈希摘要的输出格式。
 * @endif
 *
 * @lang{EN}
 * Output format for the hash digest.
 * @endif
 */
enum class hash_fmt : unsigned char
{
    binary,    ///< @lang{ZH} 原始二进制字节。 @endif @lang{EN} Raw binary bytes. @endif
    upper_hex, ///< @lang{ZH} 大写十六进制字符串（如 `"A1B2C3..."`）。 @endif @lang{EN} Uppercase hexadecimal string (e.g. `"A1B2C3..."`). @endif
    lower_hex  ///< @lang{ZH} 小写十六进制字符串（如 `"a1b2c3..."`），为默认格式。 @endif @lang{EN} Lowercase hexadecimal string (e.g. `"a1b2c3..."`); the default format. @endif
};

/**
 * @lang{ZH}
 * 用于在 `hash_cvt` 上动态切换哈希输出格式的行为对象。
 *
 * 通过 `cvt.adjust(set_hash_fmt{hash_fmt::upper_hex})` 传入，
 * 影响下次 `dump_hash` 或 `detach()` 时输出摘要的格式。
 * @endif
 *
 * @lang{EN}
 * Behavior object for dynamically switching the hash output format on `hash_cvt`.
 *
 * Pass via `cvt.adjust(set_hash_fmt{hash_fmt::upper_hex})` to change the format
 * used when the digest is next written out (on `dump_hash` or `detach()`).
 * @endif
 */
struct set_hash_fmt : cvt_behavior
{
    explicit set_hash_fmt(hash_fmt val)
        : m_val(val) {}

    hash_fmt m_val;
};

/**
 * @lang{ZH}
 * 触发 `hash_cvt` 在主内容阶段中途手动输出当前哈希摘要的行为对象。
 *
 * 通过 `cvt.adjust(dump_hash{})` 触发，输出后哈希状态被清零，
 * 后续写入的数据将参与新一轮哈希计算。可携带一个可选的分隔符字节，
 * 在摘要写出之后立即将其写入 kernel。
 * @endif
 *
 * @lang{EN}
 * Behavior object that causes `hash_cvt` to flush the current hash digest
 * mid-stream without waiting for `detach()`.
 *
 * Triggered via `cvt.adjust(dump_hash{})`. After flushing, the hash state is
 * cleared and subsequent writes contribute to a fresh digest. An optional
 * delimiter byte can be supplied; if present, it is written to the kernel
 * immediately after the digest.
 * @endif
 */
struct dump_hash : cvt_behavior
{
    explicit dump_hash(std::optional<uint8_t> delim = std::nullopt)
        : m_delim(delim) {}

    std::optional<uint8_t> m_delim;
};

/**
 * @lang{ZH}
 * 只写哈希摘要转换器。
 *
 * 接收用户写入的数据并持续更新内部哈希状态（通过 `put_main()`），
 * 在以下时机将摘要写入底层 kernel：
 * - 调用 `detach()`（或对象析构）时，若存在未输出的主内容。
 * - 通过 `adjust(dump_hash{})` 手动触发时。
 *
 * 默认输出格式为小写十六进制（`hash_fmt::lower_hex`），
 * 可通过 `adjust(set_hash_fmt{...})` 切换。
 * 仅支持输出模式（`put`），不支持 `get`。
 * @endif
 *
 * @lang{EN}
 * Write-only hash-digest converter.
 *
 * Accepts data written by the user and continuously updates the internal hash
 * state via `put_main()`. The digest is written to the underlying kernel at the
 * following points:
 * - On `detach()` (or object destruction) if there is unwritten main content.
 * - When manually triggered via `adjust(dump_hash{})`.
 *
 * The default output format is lowercase hexadecimal (`hash_fmt::lower_hex`),
 * switchable via `adjust(set_hash_fmt{...})`.
 * Only write (output) mode is supported; `get` is not available.
 * @endif
 *
 * @tparam KernelType
 * @lang{ZH} 底层 IO 转换核心类型，须满足 `io_converter` concept 且支持 `put`。 @endif
 * @lang{EN} The underlying IO converter kernel type; must satisfy `io_converter` and
 * support `put`. @endif
 *
 * @tparam TInt
 * @lang{ZH}
 * 用户侧元素类型（`internal_type`），默认与 kernel 的 `internal_type` 相同。
 * 须为单字节平凡可复制类型，且具有唯一对象表示。
 * @endif
 * @lang{EN}
 * The user-facing element type (`internal_type`), defaulting to the kernel's
 * `internal_type`. Must be a single-byte, trivially copyable type with unique
 * object representations.
 * @endif
 */
template <io_converter KernelType, typename TInt = typename KernelType::internal_type>
    requires (std::is_integral_v<typename KernelType::internal_type> &&
              sizeof(typename KernelType::internal_type) == sizeof(uint8_t) &&
              std::is_trivially_copyable_v<TInt> &&
              std::has_unique_object_representations_v<TInt> &&
              cvt_cpt::support_put<KernelType>)
class hash_cvt : public abs_cvt<hash_cvt<KernelType, TInt>, KernelType, TInt, false, false>
{
    using BT = abs_cvt<hash_cvt<KernelType, TInt>, KernelType, TInt, false, false>;
    friend BT;  // for put_main

public:
    using device_type = typename KernelType::device_type;
    using internal_type = TInt;
    using external_type = typename KernelType::internal_type;

public:
    /**
     * @lang{ZH}
     * 构造 `hash_cvt`，指定底层 kernel 与哈希算法。
     * @endif
     *
     * @lang{EN}
     * Constructs a `hash_cvt` with the given underlying kernel and hash algorithm.
     * @endif
     *
     * @param kernel
     * @lang{ZH} 底层 IO 转换核心，以移动语义传入。 @endif
     * @lang{EN} The underlying IO converter kernel, transferred by move. @endif
     *
     * @param algo
     * @lang{ZH} 要使用的哈希算法。 @endif
     * @lang{EN} The hash algorithm to use. @endif
     *
     * @throws Botan::Lookup_Error
     * @lang{ZH} 若无法创建对应的哈希函数对象。 @endif
     * @lang{EN} If the hash function object cannot be created. @endif
     */
    hash_cvt(KernelType kernel, hash_algo algo)
        : BT(std::move(kernel))
        , m_hash(Botan::HashFunction::create_or_throw(algo_to_str(algo)))
    {}

    /**
     * @lang{ZH}
     * 拷贝构造函数。复制 kernel 与哈希状态（通过 `copy_state()`），不会触发摘要输出。
     *
     * 若 `copy_state()` 返回 `nullptr`，`m_has_main_cont` 被强制置为 `false`，
     * 防止析构时对无效哈希对象调用 `dump_stream()`。
     * @endif
     *
     * @lang{EN}
     * Copy constructor. Copies the kernel and hash state (via `copy_state()`),
     * without triggering any digest output.
     *
     * If `copy_state()` returns `nullptr`, `m_has_main_cont` is forced to `false`
     * to prevent `dump_stream()` being called on a null hash object at destruction.
     * @endif
     */
    hash_cvt(const hash_cvt& val)
        requires (std::copy_constructible<KernelType>)
        : BT(val)
        , m_hash(val.m_hash ? val.m_hash->copy_state() : nullptr)
        // Gate on `m_hash` (this object's, already initialized due to member
        // declaration order) rather than `val.m_hash`: if copy_state() returned
        // nullptr without throwing, we have no hash to dump, so the invariant
        // "m_has_main_cont == true ⟹ m_hash != nullptr" must be preserved.
        , m_has_main_cont(m_hash ? val.m_has_main_cont : false)
        , m_out_fmt(val.m_out_fmt)
    {}

    hash_cvt(hash_cvt&& val) noexcept
        : BT(std::move(val))
        , m_hash(std::move(val.m_hash))
        , m_has_main_cont(val.m_has_main_cont)
        , m_out_fmt(val.m_out_fmt)
    {
        val.m_has_main_cont = false;
    }

    hash_cvt& operator=(const hash_cvt& val)
        requires (std::copy_constructible<KernelType>)
    {
        if (this != &val)
        {
            // 1. Flush current data first to avoid double-dump if temp's
            //    destructor runs due to exception during copy construction
            if (m_has_main_cont)
                dump_stream();

            // 2. Construct full copy (may throw, but avoids wrong-data-dump bug)
            hash_cvt temp(val);

            // 3. Move-assign from temp (noexcept, per io_converter concept)
            *this = std::move(temp);
        }
        return *this;
    }

    hash_cvt& operator=(hash_cvt&& val) noexcept
    {
        if (this == &val) return *this;

        if (m_has_main_cont)
        {
            try { dump_stream(); }
            catch (...) {} // NOLINT(bugprone-empty-catch)
        }

        BT::operator=(std::move(val));
        m_hash = std::move(val.m_hash);
        m_has_main_cont = val.m_has_main_cont;
        m_out_fmt = val.m_out_fmt;
        val.m_has_main_cont = false;

        return *this;
    }

    ~hash_cvt()
    {
        if (m_has_main_cont)
        {
            try { dump_stream(); }
            catch (...) {} // NOLINT(bugprone-empty-catch)
        }
    }

private:
    /**
     * @lang{ZH}
     * `abs_cvt::detach()` 的 CRTP 钩子，在 kernel 层 `detach()` 之前调用。
     *
     * 若当前处于主内容阶段（`m_has_main_cont`），尝试调用 `dump_stream()` 将哈希结果写出；
     * 若 `dump_stream()` 抛出异常，则手动将 `m_has_main_cont` 置为 `false` 并尝试
     * `m_hash->clear()` 清除残余状态（`clear()` 的异常被吞掉，避免掩盖首要错误）。
     * 成功路径下 `dump_stream()` 自行置 `m_has_main_cont = false`。
     * 最后将 `m_out_fmt` 重置为默认值。
     *
     * 异常以 `exception_ptr` 形式返回，由调用方（`abs_cvt::detach()`）按 first-failure-wins
     * 与 kernel 层异常合并。本函数为 `noexcept`，此约束由 `abs_cvt` 的 `static_assert` 强制。
     *
     * @return 捕获到的首个清理异常；无异常时为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::detach()`, called before the kernel-level `detach()`.
     *
     * If currently in the main-content phase (`m_has_main_cont`), attempts to call
     * `dump_stream()` to write out the hash result. If `dump_stream()` throws, resets
     * `m_has_main_cont` to `false` and attempts `m_hash->clear()` to wipe residual
     * state (the `clear()` exception is swallowed to avoid obscuring the primary error).
     * On the success path, `dump_stream()` itself resets `m_has_main_cont`.
     * Finally, `m_out_fmt` is reset to its default value.
     *
     * Any captured exception is returned as an `exception_ptr`; the caller
     * (`abs_cvt::detach()`) merges it with the kernel-layer exception under
     * first-failure-wins. Must be `noexcept` — enforced by a `static_assert` in `abs_cvt`.
     *
     * @return The first captured cleanup exception, or `nullptr` if none.
     * @endif
     */
    std::exception_ptr detach_impl() noexcept
    {
        std::exception_ptr local_err = nullptr;
        try { if (m_has_main_cont) dump_stream(); }
        catch (...)
        {
            local_err = std::current_exception();
            m_has_main_cont = false;
            // Clear any residual hash state so a subsequent attach()+put()
            // does not accumulate on top of half-finished data.  If clear()
            // itself fails, swallow the exception — we are already in error
            // recovery from a failed dump_stream(), and surfacing a secondary
            // failure here would only obscure the primary error.
            if (m_hash)
            {
                try { m_hash->clear(); }
                catch (...) {} // NOLINT(bugprone-empty-catch)
            }
        }
        m_out_fmt = hash_fmt::lower_hex;
        return local_err;
    }

    /**
     * @lang{ZH}
     * `abs_cvt::attach()` 的 CRTP 钩子，在 kernel 层 `attach()` 之后调用。
     *
     * 调用 `m_hash->clear()` 重置哈希状态，确保新会话从空消息状态开始。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::attach()`, called after the kernel-level `attach()`.
     *
     * Calls `m_hash->clear()` to reset the hash state, ensuring a new session
     * starts from an empty-message state.
     * @endif
     *
     * @throws cvt_error
     * @lang{ZH} 若 `m_hash` 为空（已移动走的对象）。 @endif
     * @lang{EN} If `m_hash` is null (moved-from object). @endif
     */
    void attach_impl()
    {
        if (!m_hash)
            throw cvt_error("hash_cvt::attach fail: hash not initialized (moved-from object?)");
        m_hash->clear();
    }

    /**
     * @lang{ZH}
     * `abs_cvt::bos()` 的 CRTP 钩子，处理流起始（BOS）。
     *
     * `hash_cvt` 仅支持输出模式；若 `m_io_status` 不为 `output` 则抛出异常。
     * 调用 `m_hash->clear()` 为新流准备干净的哈希状态。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::bos()`, invoked at the beginning of stream (BOS).
     *
     * `hash_cvt` supports output mode only; throws if `m_io_status` is not `output`.
     * Calls `m_hash->clear()` to prepare a clean hash state for the new stream.
     * @endif
     *
     * @throws cvt_error
     * @lang{ZH} 若当前不处于输出模式；或 `m_hash` 为空（已移动走的对象）。 @endif
     * @lang{EN} If the current mode is not output, or if `m_hash` is null
     * (moved-from object). @endif
     */
    void bos_impl()
    {
        if (BT::m_io_status != io_status::output)
            throw cvt_error("hash_cvt::bos fail: only output mode is supported");

        if (!m_hash)
            throw cvt_error("hash_cvt::bos fail: hash not initialized (moved-from object?)");
        m_hash->clear();
    }

    /**
     * @lang{ZH}
     * 将用户数据送入哈希函数，不向 kernel 写出任何内容。
     *
     * 以字节为单位将 `to_size * sizeof(internal_type)` 字节传递给
     * `m_hash->update()`。本函数不产生任何输出；摘要在 `detach()` 或
     * `dump_hash` 行为触发时才写出。调用后 `m_has_main_cont` 被置为 `true`，
     * 标记存在未输出的哈希内容。
     * @endif
     *
     * @lang{EN}
     * Feeds user data into the hash function without writing anything to the kernel.
     *
     * Passes `to_size * sizeof(internal_type)` bytes to `m_hash->update()`.
     * No output is produced here; the digest is written only on `detach()` or in
     * response to a `dump_hash` behavior. Sets `m_has_main_cont` to `true` to mark
     * that there is unwritten hash content.
     * @endif
     *
     * @param writer
     * @lang{ZH} 内部写入辅助器，本函数中未实际使用。 @endif
     * @lang{EN} The internal write helper; unused in this function. @endif
     *
     * @param to
     * @lang{ZH} 用户输入数据缓冲区。 @endif
     * @lang{EN} The user input data buffer. @endif
     *
     * @param to_size
     * @lang{ZH} 输入的 `internal_type` 元素数量。 @endif
     * @lang{EN} Number of `internal_type` elements in the input. @endif
     *
     * @throws cvt_error
     * @lang{ZH} 若 `m_hash` 为空；或 `to_size * sizeof(internal_type)` 发生溢出。 @endif
     * @lang{EN} If `m_hash` is null, or if `to_size * sizeof(internal_type)` overflows. @endif
     */
    void put_main(cvt_writer<KernelType>& /*writer*/, const internal_type* to, size_t to_size)
    {
        if (to_size == 0) return;
        if (!m_hash)
            throw cvt_error("hash_cvt::put_main fail: hash not initialized (moved-from object?)");

        // Overflow check: ensure to_size * sizeof(internal_type) won't wrap
        constexpr size_t max_safe = std::numeric_limits<size_t>::max() / sizeof(internal_type);
        if (to_size > max_safe)
            throw cvt_error("hash_cvt::put_main fail: size overflow");

        m_hash->update(reinterpret_cast<const uint8_t*>(to), to_size * sizeof(internal_type)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        m_has_main_cont = true;
    }

    /**
     * @lang{ZH}
     * `abs_cvt::adjust()` 的 CRTP 钩子，处理 `hash_cvt` 专属行为对象。
     *
     * - `set_hash_fmt`：更新下次摘要输出时使用的格式（`m_out_fmt`）。
     * - `dump_hash`：若有未输出的主内容，立即调用 `dump_stream()` 输出摘要；
     *   若携带分隔符字节，则在摘要之后将其写入 kernel。若分隔符写入失败，
     *   调用 `set_tainted()` 防止后续产生无边界的串联摘要。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::adjust()`, handling `hash_cvt`-specific behavior objects.
     *
     * - `set_hash_fmt`: Updates the format used for the next digest output (`m_out_fmt`).
     * - `dump_hash`: If there is unwritten main content, immediately calls `dump_stream()`
     *   to flush the digest. If a delimiter byte is provided, it is written to the kernel
     *   after the digest. If the delimiter write fails, `set_tainted()` is called to
     *   prevent subsequent dumps from producing boundary-less concatenated digests.
     * @endif
     *
     * @param acc
     * @lang{ZH} 行为对象，须为 `set_hash_fmt` 或 `dump_hash`；否则忽略。 @endif
     * @lang{EN} The behavior object; must be `set_hash_fmt` or `dump_hash`, otherwise
     * ignored. @endif
     */
    void adjust_impl(const cvt_behavior& acc)
    {
        if (const auto* shf_ptr = dynamic_cast<const set_hash_fmt*>(&acc); shf_ptr)
            m_out_fmt = shf_ptr->m_val;
        else if (const auto* dh_ptr = dynamic_cast<const dump_hash*>(&acc); dh_ptr)
        {
            if (m_has_main_cont)
            {
                dump_stream();
                if (dh_ptr->m_delim)
                {
                    const uint8_t delim = *dh_ptr->m_delim;
                    // If the delimiter write fails, the digest is already on
                    // the kernel without its separator; subsequent dumps would
                    // produce concatenated digests with no boundary. Taint so
                    // the user is forced to detach/attach instead of silently
                    // emitting corrupted output.
                    try
                    {
                        BT::m_kernel.put(reinterpret_cast<const external_type*>(&delim), 1); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                    }
                    catch (...)
                    {
                        BT::set_tainted();
                        throw;
                    }
                }
            }
        }
    }

private:
    /**
     * @lang{ZH}
     * 将 `hash_algo` 枚举值转换为 Botan 可识别的算法名称字符串。
     * @endif
     *
     * @lang{EN}
     * Converts a `hash_algo` enumerator to the algorithm name string recognized by Botan.
     * @endif
     *
     * @param algo
     * @lang{ZH} 要转换的算法枚举值。 @endif
     * @lang{EN} The algorithm enumerator to convert. @endif
     *
     * @return
     * @lang{ZH} 对应的 Botan 算法名称字符串（如 `"MD5"`、`"SHA-256"`）。 @endif
     * @lang{EN} The corresponding Botan algorithm name string (e.g. `"MD5"`, `"SHA-256"`). @endif
     *
     * @throws cvt_error
     * @lang{ZH} 若 `algo` 不是已知枚举值。 @endif
     * @lang{EN} If `algo` is not a recognized enumerator. @endif
     */
    static const char* algo_to_str(hash_algo algo)
    {
        switch (algo)
        {
        case hash_algo::MD5:    return "MD5";
        case hash_algo::SHA256: return "SHA-256";
        default:
            throw cvt_error("algo_to_str: invalid algorithm");
        }
    }

    /**
     * @lang{ZH}
     * 将当前哈希摘要写入 kernel，并重置哈希状态。
     *
     * 使用 `copy_state()->final()` 计算摘要以提供强异常安全性：若 kernel
     * 写入失败，原始哈希状态保持不变，调用方可重试。写出格式由 `m_out_fmt` 决定。
     * 成功后将 `m_has_main_cont` 置为 `false`，并调用 `m_hash->clear()`
     * 重置内部状态；若 `clear()` 抛出，则 taint。
     *
     * kernel 层写入失败不会导致 `hash_cvt` 本身 taint：`hash_cvt` 的不变量
     * 在 kernel 侧局部写入失败时仍保持完整（`m_has_main_cont` 保持 `true`，
     * `m_hash` 经由 `copy_state()` 未被修改），允许调用方恢复 kernel 后重试。
     * @endif
     *
     * @lang{EN}
     * Writes the current hash digest to the kernel and resets the hash state.
     *
     * Uses `copy_state()->final()` to compute the digest, providing strong exception
     * safety: if the kernel write fails, the original hash state is unchanged and
     * the caller may retry. Output format is determined by `m_out_fmt`. On success,
     * `m_has_main_cont` is set to `false` and `m_hash->clear()` is called to reset
     * internal state; if `clear()` throws, the cvt is tainted.
     *
     * A failure in the kernel write does NOT taint `hash_cvt` itself because
     * `hash_cvt`'s own invariants remain intact on a partial kernel write
     * (`m_has_main_cont` stays `true`, `m_hash` is untouched via `copy_state()`),
     * allowing the caller to recover the kernel and retry.
     * @endif
     *
     * @throws cvt_error
     * @lang{ZH}
     * 若无法复制哈希状态；若摘要为空；若 `m_out_fmt` 为无效值；
     * 或若 kernel 写入抛出异常（从 kernel 层传播）。
     * @endif
     * @lang{EN}
     * If the hash state cannot be copied; if the digest is empty; if `m_out_fmt`
     * is invalid; or if the kernel write throws (propagated from the kernel layer).
     * @endif
     */
    void dump_stream()
    {
        if (!m_has_main_cont) return;
        if (!m_hash) return;

        // Use copy_state()->final() to preserve original state until put() succeeds.
        // This provides strong exception safety: if put() fails, state is unchanged
        // and the operation can be retried.
        //
        // Intentional: a failure inside `BT::m_kernel.put(...)` is the kernel's
        // own concern — the kernel layer must taint itself per the abs_cvt
        // contract. hash_cvt deliberately does NOT mirror that taint upward,
        // because (a) hash_cvt's own invariants remain intact on a kernel-side
        // partial write (`m_has_main_cont` stays true, `m_hash` is untouched
        // via copy_state), and (b) leaving hash_cvt clean allows the caller to
        // recover the kernel transiently and retry `dump_stream()` without
        // losing the accumulated `update()` history.
        auto cp_state = m_hash->copy_state();
        if (!cp_state)
            throw cvt_error("hash_cvt::dump_stream fail: cannot copy hash state");
        auto digest = cp_state->final();
        if (digest.empty())
            throw cvt_error("hash_cvt::dump_stream fail: cannot get hash result");

        switch (m_out_fmt)
        {
        case hash_fmt::binary:
            BT::m_kernel.put(reinterpret_cast<const external_type*>(digest.data()), digest.size()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            break;
        case hash_fmt::upper_hex:
            {
                std::string hex_string = Botan::hex_encode(digest);
                BT::m_kernel.put(reinterpret_cast<const external_type*>(hex_string.data()), hex_string.size()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            }
            break;
        case hash_fmt::lower_hex:
            {
                std::string hex_string = Botan::hex_encode(digest, false);
                BT::m_kernel.put(reinterpret_cast<const external_type*>(hex_string.data()), hex_string.size()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            }
            break;
        default:
            throw cvt_error("hash_cvt::dump_stream fail: invalid output format");
        }

        // Only clear state after successful output
        m_has_main_cont = false;
        try
        {
            m_hash->clear();
        }
        catch (...)
        {
            BT::set_tainted();
            throw;
        }
    }
private:
    std::unique_ptr<Botan::HashFunction> m_hash;
    bool         m_has_main_cont{false};
    hash_fmt     m_out_fmt{hash_fmt::lower_hex};
};

/**
 * @lang{ZH}
 * `hash_cvt` 的工厂类，用于在转换器管道中统一构造哈希摘要转换器实例。
 * @endif
 *
 * @lang{EN}
 * Factory class for `hash_cvt`, for uniform construction of hash-digest converter
 * instances within a pipeline.
 * @endif
 *
 * @tparam TInt
 * @lang{ZH} 用户侧元素类型，透传给 `hash_cvt`。 @endif
 * @lang{EN} The user-facing element type, forwarded to `hash_cvt`. @endif
 */
template <typename TInt>
struct hash_cvt_creator
{
public:
    using category = CvtCreatorCategory;
    explicit hash_cvt_creator(hash_algo algo)
        : m_algo(algo)
    {}

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        return hash_cvt<TKernel, TInt>{std::forward<TKernel>(kernel), m_algo};
    }

private:
    hash_algo m_algo;
};
}
