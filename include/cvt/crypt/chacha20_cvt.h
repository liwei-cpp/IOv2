/**
 * @file chacha20_cvt.h
 * @lang{ZH}
 * ChaCha20 流加密转换器。
 *
 * 本文件提供两个组件：
 * - `chacha20_cvt`：基于 Botan ChaCha20 的流加密/解密转换器。写入（加密）模式下
 *   随机生成 IV 并将其前置于密文流；读取（解密）模式下从流头部读取 IV 进行初始化。
 * - `chacha20_cvt_creator`：工厂类，用于在管道中统一构造 `chacha20_cvt` 实例。
 * @endif
 *
 * @lang{EN}
 * ChaCha20 stream-cipher converter.
 *
 * This file provides two components:
 * - `chacha20_cvt`: A stream encryption/decryption converter backed by Botan's
 *   ChaCha20 implementation. In write (encryption) mode the IV is randomly generated
 *   and prepended to the ciphertext stream; in read (decryption) mode the IV is read
 *   from the stream head.
 * - `chacha20_cvt_creator`: A factory class for constructing `chacha20_cvt` instances
 *   uniformly within a pipeline.
 * @endif
 */

#pragma once
#include <cvt/abs_cvt.h>
#include <cvt/cvt_concepts.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <botan/auto_rng.h>
#include <botan/hash.h>
#include <botan/secmem.h>
#include <botan/stream_cipher.h>

namespace IOv2::Crypt
{
/**
 * @lang{ZH}
 * `chacha20_cvt` 的内部辅助命名空间，不作为公开 API 使用。
 * @endif
 *
 * @lang{EN}
 * Internal helper namespace for `chacha20_cvt`; not part of the public API.
 * @endif
 */
namespace chacha20_cvt_helpers
{
/**
 * @lang{ZH}
 * 将任意字符串密码通过 SHA-256 哈希派生为 ChaCha20 密钥。
 *
 * 拒绝空字符串：对空串哈希会得到公开常量 `SHA-256("")`，
 * 将其用作流密码密钥意味着零保密性，因此直接抛出异常。
 * @endif
 *
 * @lang{EN}
 * Derives a ChaCha20 key from an arbitrary passphrase string by hashing it with SHA-256.
 *
 * Empty input is rejected: hashing an empty string yields the publicly known constant
 * `SHA-256("")`, which provides zero confidentiality when used as a stream-cipher key.
 * @endif
 *
 * @param key
 * @lang{ZH} 原始密码字符串，不得为空。 @endif
 * @lang{EN} The raw passphrase string; must not be empty. @endif
 *
 * @return
 * @lang{ZH} 长度为 32 字节的 SHA-256 摘要，可直接用作 ChaCha20 密钥。 @endif
 * @lang{EN} A 32-byte SHA-256 digest suitable for direct use as a ChaCha20 key. @endif
 *
 * @throws cvt_error
 * @lang{ZH} 若 `key` 为空；或无法创建 SHA-256 哈希对象。 @endif
 * @lang{EN} If `key` is empty, or if the SHA-256 hash object cannot be created. @endif
 */
inline Botan::secure_vector<uint8_t> key_gen(std::string_view key)
{
    // Reject empty input: hashing an empty string would silently yield the
    // publicly known constant SHA-256("") and use it as the ChaCha20 key,
    // providing zero confidentiality. The secure_vector-based constructor
    // already rejects invalid key lengths; mirror that contract here.
    if (key.empty())
        throw cvt_error("chacha20 key generation fail: key must not be empty");

    auto hash = Botan::HashFunction::create("SHA-256");
    if (!hash)
        throw cvt_error("chacha20 key generation fail: cannot create SHA-256 hash");

    hash->update(reinterpret_cast<const uint8_t*>(key.data()), key.size()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    return hash->final();
}
}

/**
 * @lang{ZH}
 * 基于 ChaCha20 的流加密/解密转换器。
 *
 * 将上游 kernel 的字节流透明地进行 ChaCha20 加解密：
 * - **写入（加密）模式**：`bos_impl()` 随机生成 IV，将 IV 明文写入 kernel，
 *   随后 `put_main()` 对用户数据加密后写出。
 * - **读取（解密）模式**：`bos_impl()` 从 kernel 读取 IV，随后 `get_main()`
 *   从 kernel 读取密文并解密后交付给用户。
 *
 * `internal_type` 须为单字节整数类型，以保证每个用户元素在字节流中的偏移
 * 与密钥流偏移精确对齐。跨调用的非对齐残留字节由 `m_leftover` 缓冲区管理。
 * @endif
 *
 * @lang{EN}
 * ChaCha20-based stream encryption/decryption converter.
 *
 * Transparently applies ChaCha20 to the byte stream of the upstream kernel:
 * - **Write (encryption) mode**: `bos_impl()` generates a random IV, writes it
 *   verbatim to the kernel, and subsequent `put_main()` calls encrypt user data
 *   before forwarding it.
 * - **Read (decryption) mode**: `bos_impl()` reads the IV from the kernel, and
 *   subsequent `get_main()` calls decrypt ciphertext read from the kernel before
 *   delivering it to the user.
 *
 * `internal_type` must be a single-byte integral type so that every user element
 * maps exactly onto one byte in the keystream. Sub-element ciphertext fragments
 * carried across calls are managed by the `m_leftover` buffer.
 * @endif
 *
 * @tparam KernelType
 * @lang{ZH} 底层 IO 转换核心类型，须满足 `io_converter` concept。 @endif
 * @lang{EN} The underlying IO converter kernel type, which must satisfy the `io_converter` concept. @endif
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
              std::has_unique_object_representations_v<TInt>)
class chacha20_cvt : public abs_cvt<chacha20_cvt<KernelType, TInt>, KernelType, TInt, false, false>
{
    using BT = abs_cvt<chacha20_cvt<KernelType, TInt>, KernelType, TInt, false, false>;
    friend BT; // for put_main and get_main
public:
    using device_type = typename KernelType::device_type;
    using internal_type = TInt;
    using external_type = typename KernelType::internal_type;

    static_assert(sizeof(external_type) == 1,
        "chacha20_cvt requires external_type (kernel's internal_type) to be 1 byte");

    static constexpr size_t block_size = 64;

    static_assert(sizeof(TInt) <= block_size,
        "internal_type must fit within a single chacha20 block");

public:
    /**
     * @lang{ZH}
     * 从字符串密码构造 ChaCha20 转换器。
     *
     * 内部调用 `chacha20_cvt_helpers::key_gen(key)` 将 `key` 通过 SHA-256
     * 派生为实际密钥材料。
     * @endif
     *
     * @lang{EN}
     * Constructs a ChaCha20 converter from a passphrase string.
     *
     * Internally calls `chacha20_cvt_helpers::key_gen(key)` to derive actual
     * key material from `key` via SHA-256.
     * @endif
     *
     * @param kernel
     * @lang{ZH} 底层 IO 转换核心，以移动语义传入。 @endif
     * @lang{EN} The underlying IO converter kernel, transferred by move. @endif
     *
     * @param key
     * @lang{ZH} 原始密码字符串，不得为空。 @endif
     * @lang{EN} The raw passphrase string; must not be empty. @endif
     *
     * @throws cvt_error
     * @lang{ZH} 若无法创建 ChaCha20 流密码对象；或 `key` 为空；或 SHA-256 不可用。 @endif
     * @lang{EN} If the ChaCha20 stream cipher cannot be created, if `key` is empty,
     * or if SHA-256 is unavailable. @endif
     */
    chacha20_cvt(KernelType kernel, std::string_view key)
        : BT(std::move(kernel))
        , m_cipher(Botan::StreamCipher::create("ChaCha20"))
    {
        if (!m_cipher)
            throw cvt_error("chacha20_cvt constructor fail: cannot create the stream cipher");

        m_key = chacha20_cvt_helpers::key_gen(key);
    }

    /**
     * @lang{ZH}
     * 从原始密钥字节构造 ChaCha20 转换器。
     *
     * 直接使用 `key` 作为 ChaCha20 密钥，不再进行哈希派生。
     * 调用方须确保 `key` 的长度对 ChaCha20 有效（通常为 16 或 32 字节）。
     * @endif
     *
     * @lang{EN}
     * Constructs a ChaCha20 converter from raw key bytes.
     *
     * Uses `key` directly as the ChaCha20 key without any additional hashing.
     * The caller must ensure that `key` has a valid length for ChaCha20
     * (typically 16 or 32 bytes).
     * @endif
     *
     * @param kernel
     * @lang{ZH} 底层 IO 转换核心，以移动语义传入。 @endif
     * @lang{EN} The underlying IO converter kernel, transferred by move. @endif
     *
     * @param key
     * @lang{ZH} 原始 ChaCha20 密钥字节序列；长度须为 ChaCha20 所支持的合法值。 @endif
     * @lang{EN} Raw ChaCha20 key bytes; length must be valid for ChaCha20. @endif
     *
     * @throws cvt_error
     * @lang{ZH} 若无法创建 ChaCha20 流密码对象；或 `key` 长度不合法。 @endif
     * @lang{EN} If the ChaCha20 stream cipher cannot be created, or if `key` has an
     * invalid length. @endif
     */
    chacha20_cvt(KernelType kernel, const Botan::secure_vector<uint8_t>& key)
        : BT(std::move(kernel))
        , m_cipher(Botan::StreamCipher::create("ChaCha20"))
        , m_key(key)
    {
        if (!m_cipher)
            throw cvt_error("chacha20_cvt constructor fail: cannot create the stream cipher");
        if (!m_cipher->valid_keylength(key.size()))
            throw cvt_error("chacha20_cvt constructor fail: invalid key length");
    }

    chacha20_cvt(const chacha20_cvt&) = delete;
    chacha20_cvt& operator=(const chacha20_cvt&) = delete;

    // Move ops are written by hand (not = default) so the source's m_leftover
    // ciphertext fragment is zeroed after transfer. std::array's "move" is a
    // per-element copy for trivially-copyable element types — without explicit
    // zeroing the source would keep a stale copy alive until its own dtor.
    chacha20_cvt(chacha20_cvt&& val) noexcept
        : BT(std::move(val))
        , m_cipher(std::move(val.m_cipher))
        , m_key(std::move(val.m_key))
        , m_leftover(val.m_leftover)
        , m_leftover_len(val.m_leftover_len)
    {
        val.m_leftover.fill(0);
        val.m_leftover_len = 0;
    }

    chacha20_cvt& operator=(chacha20_cvt&& val) noexcept
    {
        if (this != &val)
        {
            BT::operator=(std::move(val));
            m_cipher = std::move(val.m_cipher);
            m_key = std::move(val.m_key);
            m_leftover = val.m_leftover;
            m_leftover_len = val.m_leftover_len;
            val.m_leftover.fill(0);
            val.m_leftover_len = 0;
        }
        return *this;
    }

    ~chacha20_cvt() = default;

// optional methods
private:
    /**
     * @lang{ZH}
     * `abs_cvt::detach()` 的 CRTP 钩子，在 kernel 层 `detach()` 之前调用。
     *
     * 若 `m_cipher` 有效（非 moved-from 状态），调用 `m_cipher->clear()` 清除密钥和 IV 状态；
     * 同时清零跨调用残留的非对齐密文缓冲区（`m_leftover` / `m_leftover_len`），
     * 防止下次 `attach()`+`get()` 时与新流的数据拼接而污染解密结果。
     * 异常以 `exception_ptr` 形式返回，由调用方（`abs_cvt::detach()`）按 first-failure-wins
     * 与 kernel 层异常合并。本函数为 `noexcept`，此约束由 `abs_cvt` 的 `static_assert` 强制。
     *
     * @return 捕获到的首个清理异常；无异常时为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::detach()`, called before the kernel-level `detach()`.
     *
     * If `m_cipher` is valid (not a moved-from object), calls `m_cipher->clear()` to
     * wipe the key and IV state. Also zeroes the carry-over ciphertext buffer
     * (`m_leftover` / `m_leftover_len`) so that a subsequent `attach()`+`get()` cannot
     * splice stale bytes from a previous stream onto the new one and corrupt the
     * decrypted output. Any captured exception is returned as an `exception_ptr`;
     * the caller (`abs_cvt::detach()`) merges it with the kernel-layer exception under
     * first-failure-wins. Must be `noexcept` — enforced by a `static_assert` in `abs_cvt`.
     *
     * @return The first captured cleanup exception, or `nullptr` if none.
     * @endif
     */
    std::exception_ptr detach_impl() noexcept
    {
        std::exception_ptr local_err = nullptr;
        if (m_cipher)
        {
            try { m_cipher->clear(); }
            catch (...) { local_err = std::current_exception(); }
        }
        m_leftover.fill(0);
        m_leftover_len = 0;
        return local_err;
    }

    /**
     * @lang{ZH}
     * `abs_cvt::attach()` 的 CRTP 钩子，在 kernel 层 `attach()` 之后调用。
     *
     * 调用 `m_cipher->clear()` 重置密码器状态，并清零跨调用残留缓冲区，
     * 确保新会话从干净状态开始，不受前次会话的密钥流影响。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::attach()`, called after the kernel-level `attach()`.
     *
     * Calls `m_cipher->clear()` to reset cipher state and zeroes the cross-call
     * leftover buffer. Ensures a new session starts from a clean state, independent
     * of any previous session's keystream.
     * @endif
     *
     * @throws cvt_error
     * @lang{ZH} 若 `m_cipher` 为空或密钥缺失（已移动走的对象）。 @endif
     * @lang{EN} If `m_cipher` is null or the key is missing (moved-from object). @endif
     */
    void attach_impl()
    {
        if (!m_cipher || m_key.empty())
            throw cvt_error("chacha20_cvt::attach fail: cipher or key missing (moved-from object?)");
        m_cipher->clear();
        // Defensive: detach_impl already cleared these on the previous detach.
        // Reset again in case the object was attached fresh (no prior detach)
        // or detach_impl was bypassed due to a noexcept escape route.
        m_leftover.fill(0);
        m_leftover_len = 0;
    }

    /**
     * @lang{ZH}
     * `abs_cvt::bos()` 的 CRTP 钩子，处理流起始（BOS）的 IV 协商。
     *
     * - **输出模式**：随机生成 IV，设置密码器密钥与 IV，然后将 IV 明文写入 kernel。
     * - **输入模式**：从 kernel 读取 IV，设置密码器密钥与 IV。
     *
     * 调用前会清零残留缓冲区，确保新流的解密不受前次流尾部残留的影响。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::bos()`, handling IV negotiation at the beginning
     * of stream (BOS).
     *
     * - **Output mode**: generates a random IV, initializes the cipher with the key
     *   and IV, then writes the IV in plaintext to the kernel.
     * - **Input mode**: reads the IV from the kernel and initializes the cipher.
     *
     * The leftover buffer is zeroed before processing to ensure decryption of the
     * new stream is not contaminated by residual bytes from a previous stream.
     * @endif
     *
     * @throws cvt_error
     * @lang{ZH}
     * 若 `m_cipher` 为空或密钥缺失；若密码器报告 IV 长度为零；
     * 若 IV 读取不完整；或 IO 方向既非输入也非输出。
     * @endif
     * @lang{EN}
     * If `m_cipher` is null or the key is missing; if the cipher reports a zero IV
     * length; if the IV read is incomplete; or if the IO direction is neither input
     * nor output.
     * @endif
     */
    void bos_impl()
    {
        if (!m_cipher || m_key.empty())
            throw cvt_error("chacha20_cvt::bos fail: cipher or key missing (moved-from object?)");

        // Reset any carry-over from a previous stream. bos() establishes a
        // fresh stream, so the leftover buffer from a prior main-content phase
        // must not bleed across the boundary.
        m_leftover.fill(0);
        m_leftover_len = 0;

        const size_t iv_len = m_cipher->default_iv_length();
        if (iv_len == 0)
            throw cvt_error("chacha20_cvt::bos fail: cipher reports zero IV length");
        std::vector<external_type> iv_buf(iv_len);

        if (BT::m_io_status == io_status::input)
        {
            if constexpr (cvt_cpt::support_get<KernelType>)
            {
                // Contract: kernel.get() on BOS path guarantees to return exactly the
                // requested byte count, or throw an exception if insufficient data is
                // available. The check below is purely defensive, verifying this
                // invariant at runtime.
                const size_t n = BT::m_kernel.get(iv_buf.data(), iv_len);
                if (n != iv_len)
                    throw cvt_error("chacha20_cvt::bos fail: incomplete IV read");

                m_cipher->set_key(m_key);
                m_cipher->set_iv(reinterpret_cast<const uint8_t*>(iv_buf.data()), iv_len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            }
            else
                throw cvt_error("chacha20_cvt::bos fail: input mode but kernel does not support get");
        }
        else if (BT::m_io_status == io_status::output)
        {
            if constexpr (cvt_cpt::support_put<KernelType>)
            {
                Botan::AutoSeeded_RNG rng;
                rng.randomize(reinterpret_cast<uint8_t*>(iv_buf.data()), iv_len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

                m_cipher->set_key(m_key);
                m_cipher->set_iv(reinterpret_cast<const uint8_t*>(iv_buf.data()), iv_len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

                BT::m_kernel.put(iv_buf.data(), iv_len);
            }
            else
                throw cvt_error("chacha20_cvt::bos fail: output mode but kernel does not support put");
        }
        else
            throw cvt_error("chacha20_cvt::bos fail: neither in input nor output mode");
    }

    /**
     * @lang{ZH}
     * 解密主内容流并以 `internal_type` 元素为单位输出。
     *
     * 维护一个跨调用的非对齐密文残留缓冲区 `m_leftover`：
     * - **splice 分支**：若上次调用结束时残留了不足一个元素的密文（`m_leftover_len > 0`），
     *   先从 reader 取 `isize - m_leftover_len` 字节补齐到一个完整元素，再 cipher 到用户缓冲。
     * - **bulk 分支**：无残留时按 `bulk_chunk = floor(block_size / isize) * isize` 字节
     *   批量读取并 cipher 对齐前缀；尾部不足一个元素的部分 raw（未 cipher）存入 `m_leftover`，
     *   留待下次调用拼接。这样保证 cipher 的字节流是连续的，密钥流偏移与字节流偏移一一对应。
     *
     * 短读语义：若 `reader.get_buf()` 返回 0 字节，本次循环立即退出；只要
     * `BT::is_eof()` 为 false，残留状态视为合法（下次调用通过 splice 分支补齐），
     * 不抛出、不污染。仅当流真正到达 EOF 且仍有残留时，对齐错误不可恢复，taint+throw。
     *
     * Cipher 故障语义：尽管 Botan 的 ChaCha20 实现在实践中不抛异常，
     * `m_cipher->cipher()` 的 API 并未声明 `noexcept`。一旦抛出，cipher 内部
     * counter 已部分推进且不可回退（`clear()` + `set_key()` 重置会换成完全
     * 不同的 keystream），状态不可恢复 —— 必须 taint。因此 get_main 内两处
     * cipher 调用都本地 try/catch + `set_tainted()`。put_main 不需要本地
     * taint，因为 `abs_cvt::put` 已经把整个 put_main 包在 taint-on-throw 的
     * catch-all 中（见 `abs_cvt.h` 输出流异常安全说明）。
     *
     * @param[in,out] reader 内部缓冲读取器。
     * @param[out]    _to    用户输出缓冲区。
     * @param[in]     to_max 期望输出的元素数量上限。
     * @return 实际解密并写入用户缓冲的元素数量。
     * @endif
     *
     * @lang{EN}
     * Decrypts the main-content stream and delivers it in `internal_type`-aligned units.
     *
     * Maintains a cross-call leftover buffer `m_leftover` for sub-element ciphertext:
     * - **Splice path**: if a previous call ended with fewer than `sizeof(internal_type)`
     *   bytes pending (`m_leftover_len > 0`), pull `isize - m_leftover_len` bytes from
     *   the reader to complete one element, then cipher it into the user buffer.
     * - **Bulk path**: when no leftover is held, fetch up to
     *   `bulk_chunk = floor(block_size / isize) * isize` bytes and cipher the aligned
     *   prefix into the user buffer; stash any sub-element tail as **raw, un-ciphered**
     *   ciphertext in `m_leftover` for the next call to splice. This keeps the byte
     *   stream fed to `cipher()` contiguous, preserving the keystream-to-byte mapping.
     *
     * Short-read semantics: a `get_buf()` returning 0 bytes exits the loop immediately.
     * If `BT::is_eof()` is false, the leftover state is a legitimate in-flight
     * condition — the next call splices it through — so no exception and no taint.
     * Only when the stream is genuinely at EOF and a leftover still persists is the
     * misalignment unrecoverable: taint and throw.
     *
     * Cipher failure: although Botan's ChaCha20 implementation is effectively
     * noexcept in practice, `m_cipher->cipher()` is not declared `noexcept` by the
     * API. If it ever did throw, the cipher's internal counter would have advanced
     * partially and cannot be rewound (calling `clear()` + `set_key()` resets to an
     * entirely different keystream), so the cvt is unrecoverable and must taint.
     * Both cipher() calls in get_main therefore wrap in try/catch + `set_tainted()`.
     * put_main does not need local tainting because `abs_cvt::put` already wraps
     * the entire put_main body in a taint-on-throw catch-all (see the output
     * exception-safety notes in `abs_cvt.h`).
     *
     * @param[in,out] reader The buffered reader.
     * @param[out]    _to    The user output buffer.
     * @param[in]     to_max The maximum number of elements to deliver.
     * @return The number of elements actually decrypted into the user buffer.
     * @endif
     */
    size_t get_main(cvt_reader<KernelType>& reader, internal_type* _to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        if (to_max == 0) return 0;
        if (!m_cipher)
            throw cvt_error("chacha20_cvt::get_main fail: cipher not initialized (moved-from object?)");

        constexpr size_t isize = sizeof(internal_type);
        constexpr size_t bulk_chunk = (block_size / isize) * isize;
        static_assert(bulk_chunk >= isize,
            "block_size must accommodate at least one internal_type");

        auto* to = reinterpret_cast<uint8_t*>(_to); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        size_t elements_delivered = 0;

        reader.reset(block_size);

        while (elements_delivered < to_max)
        {
            if (m_leftover_len > 0)
            {
                // Splice path: finish the partial element carried over from a
                // previous short read before consuming any new bulk data.
                const size_t want = isize - m_leftover_len;
                auto [ptr, cur_len] = reader.get_buf(want);
                if (cur_len == 0) break;
                std::memcpy(m_leftover.data() + m_leftover_len, ptr, cur_len);
                m_leftover_len += cur_len;
                if (m_leftover_len == isize)
                {
                    try { m_cipher->cipher(m_leftover.data(), to, isize); }
                    catch (...) { BT::set_tainted(); throw; }
                    to += isize;
                    elements_delivered += 1;
                    m_leftover_len = 0;
                }
            }
            else
            {
                // Bulk path: cipher only the aligned prefix into the user buffer;
                // stash any trailing sub-element ciphertext for the next call.
                const size_t elements_remaining = to_max - elements_delivered;
                const size_t want = (elements_remaining > bulk_chunk / isize)
                                  ? bulk_chunk
                                  : elements_remaining * isize;
                auto [ptr, cur_len] = reader.get_buf(want);
                if (cur_len == 0) break;

                const size_t complete_bytes = (cur_len / isize) * isize;
                const size_t tail           = cur_len - complete_bytes;

                if (complete_bytes > 0)
                {
                    try
                    {
                        m_cipher->cipher(reinterpret_cast<const uint8_t*>(ptr), to, complete_bytes); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                    }
                    catch (...) { BT::set_tainted(); throw; }
                    to += complete_bytes;
                    elements_delivered += complete_bytes / isize;
                }
                if (tail > 0)
                {
                    std::memcpy(m_leftover.data(), ptr + complete_bytes, tail);
                    m_leftover_len = tail;
                }
            }
        }

        // A short read with no real EOF is a recoverable state: m_leftover is
        // preserved and the next get_main call will complete the partial
        // element via the splice path above. Only flag taint when we could not
        // satisfy the request, the stream is genuinely at EOF, and we still
        // hold a partial element — that misalignment can never be resolved.
        //
        // Gating on `elements_delivered < to_max` is essential: when the loop
        // exits because the request was fully satisfied, the leftover is an
        // in-flight artifact of the final bulk read, not a failure indicator —
        // throwing here would discard elements we just successfully decrypted.
        if (elements_delivered < to_max && m_leftover_len > 0 && BT::is_eof())
        {
            BT::set_tainted();
            throw cvt_error("chacha20_cvt::get_main fail: stream ended on a non-aligned boundary");
        }

        return elements_delivered;
    }

    /**
     * @lang{ZH}
     * 加密主内容流并以 `block_size` 为单位批量写入 kernel。
     *
     * 以 `block_size`（64 字节）为块大小，逐块从用户缓冲区读取数据，
     * 调用 `m_cipher->cipher()` 原地加密后通过 `writer` 提交到 kernel。
     * 若 `to_size` 超过 `size_t` 所能安全表示的最大字节数，则分批处理。
     *
     * 本函数无需本地 taint 处理：`abs_cvt::put` 已将整个 `put_main`
     * 包裹在 taint-on-throw 的 catch-all 中。详见 `get_main` 的文档。
     * @endif
     *
     * @lang{EN}
     * Encrypts the main-content stream and submits it to the kernel in
     * `block_size`-aligned chunks.
     *
     * Reads from the user buffer in blocks of `block_size` (64 bytes), encrypts
     * each block in-place via `m_cipher->cipher()`, and commits it to the kernel
     * through `writer`. If `to_size` would overflow the safe byte-count range of
     * `size_t`, it is processed in multiple passes.
     *
     * No local taint handling is needed: `abs_cvt::put` already wraps the entire
     * `put_main` in a taint-on-throw catch-all. See `get_main` for the full rationale.
     * @endif
     *
     * @param[in,out] writer  The buffered writer.
     * @param[in]     _to     The user input buffer to encrypt.
     * @param[in]     to_size The number of `internal_type` elements to encrypt.
     *
     * @throws cvt_error
     * @lang{ZH} 若 `m_cipher` 为空（已移动走的对象）。 @endif
     * @lang{EN} If `m_cipher` is null (moved-from object). @endif
     */
    void put_main(cvt_writer<KernelType>& writer, const internal_type* _to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        if (to_size == 0) return;
        if (!m_cipher)
            throw cvt_error("chacha20_cvt::put_main fail: cipher not initialized (moved-from object?)");

        constexpr size_t max_type_limit = std::numeric_limits<size_t>::max();
        constexpr size_t max_chunk = max_type_limit - (max_type_limit % sizeof(internal_type));

        const auto* to = reinterpret_cast<const uint8_t*>(_to); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

        writer.reset(block_size);
        while (to_size > 0)
        {
            size_t aim_output = (max_chunk / sizeof(internal_type) > to_size)
                              ? (to_size * sizeof(internal_type))
                              : max_chunk;
            size_t to_i = 0;
            while (to_i != aim_output)
            {
                size_t dest_size = std::min<size_t>(block_size, aim_output - to_i);
                auto ptr = writer.put_buf(dest_size);
                // No local try/catch here: abs_cvt::put already wraps put_main in
                // a catch-all that sets the taint flag on any exception. If cipher
                // ever throws, that outer guard correctly marks the cvt unusable.
                // See the get_main doxygen for the full design rationale.
                m_cipher->cipher(to + to_i, reinterpret_cast<uint8_t*>(ptr), dest_size); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                to_i += dest_size;
            }
            to += aim_output;
            to_size -= aim_output / sizeof(internal_type);
        }
        // commit() is the responsibility of abs_cvt::put.
    }
private:
    std::unique_ptr<Botan::StreamCipher>        m_cipher;
    Botan::secure_vector<uint8_t>               m_key;
    // Cross-call carry-over for sub-element ciphertext. Holds raw (un-ciphered)
    // bytes from a short read that did not complete one internal_type; the next
    // get_main call splices them with fresh bytes before feeding cipher() so the
    // keystream stays aligned with the byte stream.
    std::array<uint8_t, sizeof(internal_type)>  m_leftover{};
    size_t                                      m_leftover_len{0};
};

/**
 * @lang{ZH}
 * `chacha20_cvt` 的工厂类，用于在转换器管道中统一构造加密转换器实例。
 *
 * 在构造时通过 `chacha20_cvt_helpers::key_gen()` 将字符串密码预先派生为密钥，
 * 之后每次调用 `create()` 均复用该密钥，无需重复哈希。
 * @endif
 *
 * @lang{EN}
 * Factory class for `chacha20_cvt`, for uniform construction of encryption
 * converter instances within a pipeline.
 *
 * Derives key material from a passphrase via `chacha20_cvt_helpers::key_gen()`
 * at construction time; subsequent `create()` calls reuse the derived key without
 * re-hashing.
 * @endif
 *
 * @tparam TInt
 * @lang{ZH} 用户侧元素类型，透传给 `chacha20_cvt`。 @endif
 * @lang{EN} The user-facing element type, forwarded to `chacha20_cvt`. @endif
 */
template <typename TInt>
struct chacha20_cvt_creator
{
public:
    using category = CvtCreatorCategory;
    explicit chacha20_cvt_creator(std::string_view key)
        : m_key(chacha20_cvt_helpers::key_gen(key)) {}

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        return chacha20_cvt<TKernel, TInt>{std::forward<TKernel>(kernel), m_key};
    }

private:
    Botan::secure_vector<uint8_t> m_key;
};
}
