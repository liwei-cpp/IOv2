/**
 * @file istream_operators.h
 * @lang{ZH}
 * 定义了输入流的格式化与非格式化提取设施。
 * 包含输入哨兵 `in_sentry`、输入流概念 `istream_type`、承载各类提取操作
 * （`get`/`peek`/`read`/`ignore`/`putback`）的 `istream_operators` 混入基类，
 * 以及提取运算符 `operator>>`（含操纵符重载）。
 * @endif
 *
 * @lang{EN}
 * Defines the formatted and unformatted extraction facilities for input streams.
 * Includes the input sentry `in_sentry`, the input-stream concept `istream_type`,
 * the `istream_operators` mix-in base that carries the various extraction operations
 * (`get`/`peek`/`read`/`ignore`/`putback`), and the extraction `operator>>`
 * (including manipulator overloads).
 * @endif
 */
#pragma once

#include <common/defs.h>
#include <common/metafunctions.h>
#include <facet/ctype.h>
#include <io/fp_defs/base_fp.h>
#include <io/io_base.h>
#include <io/streambuf_iterator.h>
#include <locale/locale.h>

#include <concepts>
#include <cstddef>
#include <exception>
#include <functional>
#include <iterator>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 输入操作的 RAII 哨兵：在每次提取操作的入口统一完成前置准备。
 *
 * 哨兵负责校验流的有效性、刷新关联（tie）流、获取流锁，并在需要时跳过前导空白，
 * 从而让每个提取操作以一致的前置条件开始。哨兵不可拷贝、不可移动。
 * @tparam TStream 关联的输入流类型。
 * @tparam involve_output 若为 `true`，表示该流同时支持输出，构造时会将底层缓冲区
 *                        切换到读取模式（`switch_to_get`）。
 * @endif
 *
 * @lang{EN}
 * @brief RAII sentry for input operations: performs the shared setup at the entry of
 *        every extraction operation.
 *
 * The sentry validates the stream, flushes the tied stream, acquires the stream lock,
 * and optionally skips leading whitespace, so that each extraction begins with a
 * consistent set of preconditions. The sentry is neither copyable nor movable.
 * @tparam TStream The associated input stream type.
 * @tparam involve_output If `true`, the stream also supports output, and construction
 *                        switches the underlying buffer to get mode (`switch_to_get`).
 * @endif
 */
template <typename TStream, bool involve_output>
struct in_sentry
{
    /**
     * @lang{ZH}
     * @brief 哨兵所用锁的类型，即针对流 `io_mutex()` 返回的互斥量的 `std::unique_lock`。
     * @endif
     *
     * @lang{EN}
     * @brief The lock type used by the sentry: a `std::unique_lock` over the mutex
     *        returned by the stream's `io_mutex()`.
     * @endif
     */
    using lock_type =
        std::unique_lock<std::remove_reference_t<decltype(std::declval<TStream&>().io_mutex())>>;

    /**
     * @lang{ZH}
     * @brief 构造输入哨兵：校验流、（在加锁前）刷新关联流、加锁，并按需跳过前导空白。
     *
     * 锁不由哨兵自身持有，而是从调用方借入（`lock`，须处于 `defer_lock` 状态）：哨兵在构造
     * 中对其加锁，但其生命周期由调用方的局部变量掌握。因此当哨兵在 `try` 块末尾析构后，锁
     * 仍被调用方持有，`catch` 中的 `handle_exception` 得以在持锁状态下更新流状态，使成功路径
     * 与失败路径对同一把 `io_mutex()` 的可见性保持一致。
     *
     * 关联流的刷新在加锁之前完成，保证任一时刻本线程至多持有一把流锁，维持不死锁保证。
     * @param is 要操作的输入流。
     * @param noskip 若为 `true`，则不跳过前导空白；若为 `false`，按当前 locale 的
     *               空白定义跳过前导空白。
     * @param lock 从调用方借入的锁，必须处于 `defer_lock` 状态；哨兵在构造时对其加锁。
     * @throw stream_error 若流无效，或缺少 ctype facet。
     * @throw eof_error 若在跳过空白的过程中到达输入结尾。
     * @endif
     * @lang{EN}
     * @brief Constructs the input sentry: validates the stream, flushes the tied stream
     * (before locking), acquires the lock, and skips leading whitespace if requested.
     *
     * The lock is not owned by the sentry but borrowed from the caller (`lock`, which must be
     * in `defer_lock` state): the sentry locks it during construction, yet its lifetime is
     * owned by the caller's local variable. Thus, after the sentry is destroyed at the end of
     * the enclosing `try`, the lock is still held by the caller, so `handle_exception` in the
     * `catch` can update the stream state while holding the lock, keeping the success and
     * failure paths consistent with respect to the same `io_mutex()`.
     *
     * The tied stream is flushed before locking, so at most one stream lock is held by this
     * thread at any time, preserving the no-deadlock guarantee.
     * @param is The input stream to operate on.
     * @param noskip If `true`, leading whitespace is not skipped; if `false`, leading
     *               whitespace is skipped according to the current locale's definition.
     * @param lock The lock borrowed from the caller; must be in `defer_lock` state. The
     *             sentry locks it during construction.
     * @throw stream_error If the stream is invalid or the ctype facet is missing.
     * @throw eof_error If end of input is reached while skipping whitespace.
     * @endif
     */
    in_sentry(TStream& is, bool noskip, lock_type& lock)
        : m_is(is)
        , m_lock(lock)
    {
        if (!m_is)
            throw stream_error("istream_sentry create fail: Invalid istream");

        if (auto* tied = is.tie())
        {
            try { tied->flush(); }
            catch (...) {} // NOLINT(bugprone-empty-catch)
        }

        m_lock.lock();

        if constexpr (involve_output)
            is.m_streambuf.switch_to_get();

        if (!noskip)
        {
            try
            {
                auto ct = is.m_locale.template get<IOv2::ctype<typename TStream::char_type>>();
                if (!ct)
                    throw stream_error{"istream ignore_ws fail: no ctype facet"};
                auto c = is.m_streambuf.sgetc();
                while (c.has_value() &&
                        ct->is_any(base_ft<ctype>::space, c.value()))
                {
                    c = is.m_streambuf.snextc();
                }

                if (!c.has_value())
                    throw eof_error{};
            }
            catch(...)
            {
                is.handle_exception(std::current_exception());
            }
        }

        if (!m_is)
            throw stream_error("istream_sentry create fail: Invalid istream");
    }

    ~in_sentry() = default;

    in_sentry(const in_sentry&) = delete;
    in_sentry& operator=(const in_sentry&) = delete;
    in_sentry(in_sentry&&) = delete;
    in_sentry& operator=(in_sentry&&) = delete;

private:
    TStream&   m_is;
    lock_type& m_lock;
};

/**
 * @lang{ZH}
 * @brief 类型特征的主模板：默认判定任意类型都不是 `in_sentry`。
 * @tparam T 待检测的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Primary template of the type trait: by default any type is not an `in_sentry`.
 * @tparam T The type under inspection.
 * @endif
 */
template <typename>
struct is_in_sentry_impl
{
    constexpr static bool value = false;
};

/**
 * @lang{ZH}
 * @brief `is_in_sentry_impl` 的偏特化：对 `in_sentry` 实例判定为真。
 * @endif
 *
 * @lang{EN}
 * @brief Partial specialization of `is_in_sentry_impl`: true for instances of `in_sentry`.
 * @endif
 */
template <typename TStream, bool involve_output>
struct is_in_sentry_impl<in_sentry<TStream, involve_output>>
{
    constexpr static bool value = true;
};

/**
 * @lang{ZH}
 * @brief 判定某类型是否为 `in_sentry` 实例的概念。
 * @tparam T 待检测的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Concept that checks whether a type is an instance of `in_sentry`.
 * @tparam T The type under inspection.
 * @endif
 */
template <typename T>
concept is_in_sentry = is_in_sentry_impl<T>::value;

/**
 * @lang{ZH}
 * @brief 分隔符处理策略标签：**消费**分隔符（提取后从流中移除，getline 语义）。
 * @endif
 *
 * @lang{EN}
 * @brief Delimiter-policy tag: **consume** the delimiter (removed from the stream after
 *        extraction, getline semantics).
 * @endif
 */
struct cons_sep;

/**
 * @lang{ZH}
 * @brief 分隔符处理策略标签：**保留**分隔符（留在流中，get 语义）。
 * @endif
 *
 * @lang{EN}
 * @brief Delimiter-policy tag: **keep** the delimiter (left in the stream, get semantics).
 * @endif
 */
struct keep_sep;

/**
 * @lang{ZH}
 * @brief C 字符串处理策略标签：在输出末尾**追加**空字符结尾（`'\0'`）。
 * @endif
 *
 * @lang{EN}
 * @brief C-string-policy tag: **append** a null terminator (`'\0'`) at the end of the output.
 * @endif
 */
struct app_zt;

/**
 * @lang{ZH}
 * @brief C 字符串处理策略标签：**不**追加空字符结尾。
 * @endif
 *
 * @lang{EN}
 * @brief C-string-policy tag: do **not** append a null terminator.
 * @endif
 */
struct no_zt;

template <typename TChar>
struct istream_operators;

/**
 * @lang{ZH}
 * @brief 输入流类型的概念。
 *
 * 一个类型要成为输入流，必须提供 `in_sentry_type` 与 `char_type` 类型、可返回其
 * locale，且其 `in_sentry_type` 满足 `is_in_sentry`；同时它必须同时派生自
 * `ios_base<char_type>` 与 `istream_operators<char_type>`。
 * @tparam T 待检测的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Concept for an input stream type.
 *
 * To qualify as an input stream, a type must expose the `in_sentry_type` and `char_type`
 * types, be able to return its locale, and have an `in_sentry_type` that satisfies
 * `is_in_sentry`; it must also derive from both `ios_base<char_type>` and
 * `istream_operators<char_type>`.
 * @tparam T The type under inspection.
 * @endif
 */
template <typename T>
concept istream_type =
    requires (T a)
    {
        typename T::in_sentry_type;
        typename T::char_type;
        { a.locale() } -> std::same_as<const locale<typename T::char_type>&>;
    } &&
    is_in_sentry<typename T::in_sentry_type> &&
    std::derived_from<T, ios_base<typename T::char_type>> &&
    std::derived_from<T, istream_operators<typename T::char_type>>;

/**
 * @lang{ZH}
 * @brief 为输入流提供各类提取操作的混入（mix-in）基类。
 *
 * 本模板集中承载 `get`、`peek`、`read`、`ignore`、`putback` 等成员，供具体输入流类型
 * 派生使用；这些成员通过 deducing-this（`this TSelf& self`）以派生类的具体类型执行操作。
 * 每个操作都在其内部构造输入哨兵以完成加锁与前置准备，并将异常统一交由流的
 * `handle_exception` 处理，从而按异常掩码更新流状态。
 * @tparam TChar 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Mix-in base that provides the various extraction operations for input streams.
 *
 * This template centralizes members such as `get`, `peek`, `read`, `ignore`, and `putback`
 * for concrete input stream types to derive from; these members use deducing-this
 * (`this TSelf& self`) to operate on the concrete derived type. Each operation constructs
 * an input sentry internally to handle locking and setup, and routes exceptions through the
 * stream's `handle_exception`, which updates the stream state according to the exception mask.
 * @tparam TChar The character type.
 * @endif
 */
template <typename TChar>
struct istream_operators
{
    /**
     * @lang{ZH}
     * @brief 从流中提取单个字符并以 `std::optional` 返回。
     *
     * 这是非格式化提取，不跳过前导空白。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 提取到的字符；若无字符可提取（到达 EOF），则返回空的 `std::optional`。
     * @note 若无字符可提取，会置位 `eofbit`/`failbit`（按流的异常掩码，可能改为抛出）。
     * @endif
     *
     * @lang{EN}
     * @brief Extracts a single character from the stream and returns it as a `std::optional`.
     *
     * This is an unformatted extraction and does not skip leading whitespace.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return The extracted character; an empty `std::optional` if no character could be
     *         extracted (EOF reached).
     * @note If no character can be extracted, `eofbit`/`failbit` are set (or, subject to the
     *       stream's exception mask, an exception is thrown instead).
     * @endif
     */
    template <typename TSelf>
    std::optional<TChar> get(this TSelf& self)
    {
        std::optional<TChar> c;
        bool at_eof = false;
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
            c = self.m_streambuf.sbumpc();
            if (!c.has_value())
            {
                at_eof = true;
                throw stream_error{"istream get fail: no character extracted"};
            }
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }
        return c;
    }

    /**
     * @lang{ZH}
     * @brief 从流中提取单个字符并存入 `c`。
     *
     * 这是非格式化提取，不跳过前导空白。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param c 用于接收提取字符的引用；仅在成功提取时被写入。
     * @return 流自身的引用。
     * @note 若无字符可提取，会置位 `eofbit`/`failbit`，且 `c` 保持不变。
     * @endif
     *
     * @lang{EN}
     * @brief Extracts a single character from the stream and stores it in `c`.
     *
     * This is an unformatted extraction and does not skip leading whitespace.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param c Reference receiving the extracted character; written only on success.
     * @return A reference to the stream itself.
     * @note If no character can be extracted, `eofbit`/`failbit` are set and `c` is left
     *       unchanged.
     * @endif
     */
    template <typename TSelf>
    TSelf& get(this TSelf& self, TChar& c)
    {
        bool at_eof = false;
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
            auto tmp = self.m_streambuf.sbumpc();
            if (tmp.has_value()) c = tmp.value();
            else
            {
                at_eof = true;
                throw stream_error{"istream get fail: no character extracted"};
            }
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }

        return self;
    }

    /**
     * @lang{ZH}
     * @brief 将字符提取到输出序列，直到遇到分隔符、缓冲区满或到达 EOF。
     *
     * 这是 `get`/`getline` 系列的策略化底层实现，行为由两个策略标签控制：
     * - `DelimPolicy`：`cons_sep` 表示消费分隔符（getline 语义，分隔符被读走但不写入
     *   输出）；`keep_sep` 表示保留分隔符（get 语义，分隔符留在流中）。
     * - `CStrPolicy`：`app_zt` 表示在末尾追加空字符结尾（此时最多写入 `n-1` 个字符）；
     *   `no_zt` 表示不追加结尾。
     * @tparam DelimPolicy 分隔符处理策略：`cons_sep` 或 `keep_sep`。
     * @tparam CStrPolicy C 字符串处理策略：`app_zt` 或 `no_zt`。
     * @tparam TOut 输出目标类型：可为指针或输出迭代器。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param s 输出目标（指针或输出迭代器）。
     * @param n 缓冲区容量；当 `CStrPolicy` 为 `app_zt` 时，最多写入 `n-1` 个字符。
     * @param delim 分隔符。
     * @return 指向最后一个写入位置之后的输出迭代器/指针。
     * @throw stream_error 若 `s` 为空指针、`n` 为 0、未提取到任何字符，或
     *        （`cons_sep` 下）在缓冲区容量内未找到分隔符。
     * @note 到达 EOF 时置位 `eofbit`。在异常路径上，若为 `app_zt`，仍会尽力写入空字符结尾。
     * @endif
     *
     * @lang{EN}
     * @brief Extracts characters into an output sequence until a delimiter, a full buffer,
     *        or EOF is reached.
     *
     * This is the policy-based low-level implementation behind the `get`/`getline` family;
     * its behavior is controlled by two policy tags:
     * - `DelimPolicy`: `cons_sep` consumes the delimiter (getline semantics; the delimiter is
     *   read but not written to the output); `keep_sep` keeps it (get semantics; the delimiter
     *   stays in the stream).
     * - `CStrPolicy`: `app_zt` appends a null terminator (at most `n-1` characters are then
     *   written); `no_zt` appends no terminator.
     * @tparam DelimPolicy Delimiter policy: `cons_sep` or `keep_sep`.
     * @tparam CStrPolicy C-string policy: `app_zt` or `no_zt`.
     * @tparam TOut The output target type: either a pointer or an output iterator.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param s The output target (pointer or output iterator).
     * @param n The buffer capacity; when `CStrPolicy` is `app_zt`, at most `n-1` characters
     *          are written.
     * @param delim The delimiter.
     * @return An output iterator/pointer past the last written position.
     * @throw stream_error If `s` is a null pointer, `n` is 0, no character was extracted, or
     *        (under `cons_sep`) the delimiter was not found within the buffer capacity.
     * @note Sets `eofbit` on reaching EOF. On the exception path, a null terminator is still
     *       best-effort written when `app_zt` is in effect.
     * @endif
     */
    template <typename DelimPolicy, typename CStrPolicy, typename TOut, typename TSelf>
        requires ((std::is_same_v<DelimPolicy, cons_sep> || std::is_same_v<DelimPolicy, keep_sep>) &&
                  (std::is_same_v<CStrPolicy, app_zt> || std::is_same_v<CStrPolicy, no_zt>))
    TOut get(this TSelf& self, TOut s, size_t n, TChar delim)
    {
        constexpr bool is_cstr = std::is_same_v<CStrPolicy, app_zt>;

        size_t gcount = 0;
        bool at_eof = false;
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
            if constexpr (std::is_pointer_v<TOut>)
            {
                if (s == nullptr)
                    throw stream_error("istream get fail: null character sequence");
            }
            if (n == 0)
                throw stream_error("istream get fail: zero buffer size");
            auto c = self.m_streambuf.sgetc();
            while ((gcount + is_cstr < n) &&
                   (c.has_value()) &&
                   (c.value() != delim))
            {
                *s++ = c.value();
                ++gcount;
                c = self.m_streambuf.snextc();
            }

            at_eof = (gcount + is_cstr < n) && (!c.has_value());

            if constexpr (std::is_same_v<DelimPolicy, cons_sep>)
            {
                if (c.has_value())
                {
                    if (c.value() == delim)
                    {
                        self.m_streambuf.sbumpc();
                        ++gcount;
                    }
                    else
                        throw stream_error("istream getline fail: delimiter not found within buffer capacity");
                }
            }

            if (gcount == 0)
                throw stream_error{"istream get fail: no character extracted"};

            if (at_eof)
                self.setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            if constexpr (is_cstr)
            {
                if constexpr (std::is_pointer_v<TOut>)
                {
                    if (s != nullptr && n != 0)
                        *s++ = TChar{};
                }
                else if (n != 0)
                    *s++ = TChar{};
            }
            self.handle_exception(std::current_exception(), at_eof);
            return s;
        }

        if constexpr (is_cstr)
            *s++ = TChar{};
        return s;
    }

    /**
     * @lang{ZH}
     * @brief 将字符提取到输出序列，直到遇到换行符、缓冲区满或到达 EOF。
     *
     * 等价于以当前 locale 下 `'\n'` 的宽化结果作为分隔符调用三参数版本
     * `get<DelimPolicy, CStrPolicy>(s, n, delim)`。
     * @tparam DelimPolicy 分隔符处理策略：`cons_sep` 或 `keep_sep`。
     * @tparam CStrPolicy C 字符串处理策略：`app_zt` 或 `no_zt`。
     * @tparam TOut 输出目标类型：可为指针或输出迭代器。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param s 输出目标（指针或输出迭代器）。
     * @param n 缓冲区容量；当 `CStrPolicy` 为 `app_zt` 时，最多写入 `n-1` 个字符。
     * @return 指向最后一个写入位置之后的输出迭代器/指针。
     * @throw stream_error 若缺少 ctype facet（用于宽化 `'\n'`）；其余异常与三参数版本一致。
     * @endif
     *
     * @lang{EN}
     * @brief Extracts characters into an output sequence until a newline, a full buffer,
     *        or EOF is reached.
     *
     * Equivalent to calling the three-argument overload
     * `get<DelimPolicy, CStrPolicy>(s, n, delim)` with the delimiter set to the widened
     * `'\n'` under the current locale.
     * @tparam DelimPolicy Delimiter policy: `cons_sep` or `keep_sep`.
     * @tparam CStrPolicy C-string policy: `app_zt` or `no_zt`.
     * @tparam TOut The output target type: either a pointer or an output iterator.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param s The output target (pointer or output iterator).
     * @param n The buffer capacity; when `CStrPolicy` is `app_zt`, at most `n-1` characters
     *          are written.
     * @return An output iterator/pointer past the last written position.
     * @throw stream_error If the ctype facet (used to widen `'\n'`) is missing; other
     *        exceptions match the three-argument overload.
     * @endif
     */
    template <typename DelimPolicy, typename CStrPolicy, typename TOut, typename TSelf>
        requires ((std::is_same_v<DelimPolicy, cons_sep> || std::is_same_v<DelimPolicy, keep_sep>) &&
                  (std::is_same_v<CStrPolicy, app_zt> || std::is_same_v<CStrPolicy, no_zt>))
    TOut get(this TSelf& self, TOut s, size_t n)
    {
        TChar delim;
        {
            std::lock_guard guard(self.io_mutex());
            try
            {
                auto ct = self.m_locale.template get<IOv2::ctype<TChar>>();
                if (!ct)
                    throw stream_error{"istream get fail: no ctype facet"};
                delim = ct->widen('\n');
            }
            catch(...)
            {
                if constexpr (std::is_same_v<CStrPolicy, app_zt>)
                {
                    if constexpr (std::is_pointer_v<TOut>)
                    {
                        if (s != nullptr && n != 0)
                            *s++ = TChar{};
                    }
                    else if (n != 0)
                        *s++ = TChar{};
                }
                self.handle_exception(std::current_exception());
                return s;
            }
        }

        return self.template get<DelimPolicy, CStrPolicy, TOut>(s, n, delim);
    }

    /**
     * @lang{ZH}
     * @brief 查看流中的下一个字符但不提取它。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 下一个字符；若已到达 EOF，则返回空的 `std::optional`。
     * @throw eof_error 若已到达 EOF（按流的异常掩码，可能改为置位状态位）。
     * @endif
     *
     * @lang{EN}
     * @brief Reads the next character in the stream without extracting it.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return The next character; an empty `std::optional` if EOF has been reached.
     * @throw eof_error If EOF has been reached (or, subject to the stream's exception mask,
     *        a status bit is set instead).
     * @endif
     */
    template <typename TSelf>
    std::optional<TChar> peek(this TSelf& self)
    {
        std::optional<TChar> c;
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
            c = self.m_streambuf.sgetc();
            if (!c.has_value()) throw eof_error{};
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }
        return c;
    }

    /**
     * @lang{ZH}
     * @brief 从流中读取恰好 `n` 个字符到 `s`。
     *
     * 这是非格式化提取。若在读满 `n` 个字符前到达 EOF，则视为失败。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param s 目标缓冲区。当 `n != 0` 时不得为空指针。
     * @param n 要读取的字符数。
     * @return 指向最后一个写入位置之后的指针（即 `s + 实际读取数`）。
     * @throw stream_error 若 `s` 为空指针而 `n != 0`，或无法读满 `n` 个字符。
     * @note 未能读满时置位 `eofbit`。
     * @endif
     *
     * @lang{EN}
     * @brief Reads exactly `n` characters from the stream into `s`.
     *
     * This is an unformatted extraction. Reaching EOF before `n` characters are read is
     * treated as a failure.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param s The destination buffer. Must not be a null pointer when `n != 0`.
     * @param n The number of characters to read.
     * @return A pointer past the last written position (i.e. `s + characters actually read`).
     * @throw stream_error If `s` is a null pointer while `n != 0`, or if `n` characters could
     *        not be read.
     * @note Sets `eofbit` when fewer than `n` characters could be read.
     * @endif
     */
    template <typename TSelf>
    TChar* read(this TSelf& self, TChar* s, size_t n)
    {
        size_t gcount = 0;
        bool at_eof = false;
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
            if (s == nullptr && n != 0)
                throw stream_error{"istream read fail: null character sequence"};
            self.m_streambuf.sgetn(s, n, &gcount);
            if (gcount != n)
            {
                at_eof = true;
                throw stream_error{"istream read fail: cannot read enough characters"};
            }
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }
        return s + gcount;
    }

    /**
     * @lang{ZH}
     * @brief 丢弃流中最多 `n` 个字符。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param n 要丢弃的字符数，默认为 1。
     * @return 流自身的引用。
     * @note 若在丢弃 `n` 个字符前到达 EOF，则置位 `eofbit`。
     * @endif
     *
     * @lang{EN}
     * @brief Discards up to `n` characters from the stream.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param n The number of characters to discard; defaults to 1.
     * @return A reference to the stream itself.
     * @note Sets `eofbit` if EOF is reached before `n` characters are discarded.
     * @endif
     */
    template <typename TSelf>
    TSelf& ignore(this TSelf& self, size_t n = 1)
    {
        bool at_eof = false;
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);

            for (size_t gcount = 0; gcount < n; ++gcount)
            {
                if (self.m_streambuf.is_eof())
                {
                    at_eof = true;
                    break;
                }
                self.m_streambuf.sbumpc();
            }

            if (at_eof)
                self.setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }

        return self;
    }

    /**
     * @lang{ZH}
     * @brief 丢弃流中最多 `n` 个字符，直到遇到并丢弃分隔符为止。
     *
     * 若在丢弃 `n` 个字符之内遇到 `delim`，则该分隔符也会被丢弃（计入丢弃计数）。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param n 最多丢弃的字符数。为 0 时直接返回。
     * @param delim 分隔符。
     * @return 流自身的引用。
     * @note 若在遇到分隔符或丢弃满 `n` 个字符前到达 EOF，则置位 `eofbit`。
     * @endif
     *
     * @lang{EN}
     * @brief Discards up to `n` characters from the stream, up to and including a delimiter.
     *
     * If `delim` is encountered within `n` characters, that delimiter is discarded as well
     * (counted toward the discard count).
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param n The maximum number of characters to discard. Returns immediately if 0.
     * @param delim The delimiter.
     * @return A reference to the stream itself.
     * @note Sets `eofbit` if EOF is reached before the delimiter is found or `n` characters
     *       are discarded.
     * @endif
     */
    template <typename TSelf>
    TSelf& ignore(this TSelf& self, size_t n, TChar delim)
    {
        size_t gcount = 0;
        bool at_eof = false;
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
            if (n == 0) return self;

            auto c = self.m_streambuf.sgetc();
            while (gcount < n
                    && c.has_value()
                    && (c.value() != delim))
            {
                ++gcount;
                c = self.m_streambuf.snextc();
            }

            at_eof = (gcount < n) && (!c.has_value());

            if (gcount < n)
            {
                if (c.has_value())
                {
                    ++gcount;
                    self.m_streambuf.sbumpc();
                }
            }

            if (at_eof)
                self.setstate(ios_defs::eofbit);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception(), at_eof);
        }

        return self;
    }

    /**
     * @lang{ZH}
     * @brief 将字符 `c` 放回流中，使其成为下一个被读取的字符。
     *
     * 放回前会清除 `eofbit`，因此可在到达 EOF 后重新放回字符继续读取。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param c 要放回的字符。
     * @return 流自身的引用。
     * @note 若底层缓冲区不支持放回该字符，会置位失败状态（按流的异常掩码可能改为抛出）。
     * @endif
     *
     * @lang{EN}
     * @brief Puts the character `c` back into the stream so it becomes the next character read.
     *
     * `eofbit` is cleared before the put-back, so a character can be put back and read again
     * after EOF was reached.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param c The character to put back.
     * @return A reference to the stream itself.
     * @note If the underlying buffer cannot put the character back, a failure state is set
     *       (or, subject to the stream's exception mask, an exception is thrown).
     * @endif
     */
    template <typename TSelf>
    TSelf& putback(this TSelf& self, TChar c)
    {
        std::unique_lock lk(self.io_mutex(), std::defer_lock);
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true, lk);
            self.unset_state(IOv2::ios_defs::eofbit);
            self.m_streambuf.sputbackc(c);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    /**
     * @lang{ZH}
     * @brief 取输入迭代器；可选地附加一个“已观察到输入结束”的报告位。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param saw_eof 可选的报告位；生存期必须覆盖迭代器及其所有副本，`nullptr` 表示不
     *                上报。详见 istreambuf_iterator。
     * @return 绑定到本流缓冲区的 `istreambuf_iterator`。
     * @endif
     * @lang{EN}
     * @brief Gets an input iterator; optionally attaches an "observed end of input" flag.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param saw_eof Optional report flag; its lifetime must cover the iterator and all
     *                copies, `nullptr` means do not report. See istreambuf_iterator.
     * @return An `istreambuf_iterator` bound to this stream's buffer.
     * @endif
     */
private:
    template <typename TSelf>
    auto i_iter(this TSelf& self, bool* saw_eof = nullptr)
    {
        return istreambuf_iterator(self.m_streambuf, saw_eof);
    }

    /**
     * @lang{ZH}
     * @brief 声明格式化提取运算符为友元，使其可访问私有的 `i_iter`。
     * @endif
     *
     * @lang{EN}
     * @brief Befriends the formatted extraction operator so it can access the private `i_iter`.
     * @endif
     */
    template <istream_type U, typename TValue>
        requires is_reader_def<typename U::char_type,
                               typename parse_context_type<typename U::char_type, TValue>::type>
    friend U& operator>>(U& obj, TValue& value);
};

/**
 * @lang{ZH}
 * @brief 提取操纵符：应用一个作用于 `ios_base<char_type>&` 的函数指针操纵符。
 * @tparam T 输入流类型。
 * @param obj 输入流。
 * @param pf 操纵符函数指针。
 * @return 流自身的引用。
 * @throw stream_error 若 `pf` 为空。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction manipulator: applies a function-pointer manipulator taking
 *        `ios_base<char_type>&`.
 * @tparam T The input stream type.
 * @param obj The input stream.
 * @param pf The manipulator function pointer.
 * @return A reference to the stream itself.
 * @throw stream_error If `pf` is null.
 * @endif
 */
template <istream_type T>
T& operator >> (T& obj, void(*pf)(ios_base<typename T::char_type>&))
{
    try
    {
        if (!pf)
            throw stream_error("istream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 提取操纵符：应用一个作用于 `ios_base<char_type>&` 的 `std::function` 操纵符。
 * @tparam T 输入流类型。
 * @param obj 输入流。
 * @param pf 操纵符可调用对象。
 * @return 流自身的引用。
 * @throw stream_error 若 `pf` 为空。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction manipulator: applies a `std::function` manipulator taking
 *        `ios_base<char_type>&`.
 * @tparam T The input stream type.
 * @param obj The input stream.
 * @param pf The manipulator callable.
 * @return A reference to the stream itself.
 * @throw stream_error If `pf` is empty.
 * @endif
 */
template <istream_type T>
T& operator >> (T& obj, const std::function<void(ios_base<typename T::char_type>&)>& pf)
{
    try
    {
        if (!pf)
            throw stream_error("istream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 提取操纵符：应用一个作用于具体流类型 `T&` 的函数指针操纵符。
 * @tparam T 输入流类型。
 * @param obj 输入流。
 * @param pf 操纵符函数指针。
 * @return 流自身的引用。
 * @throw stream_error 若 `pf` 为空。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction manipulator: applies a function-pointer manipulator taking the concrete
 *        stream type `T&`.
 * @tparam T The input stream type.
 * @param obj The input stream.
 * @param pf The manipulator function pointer.
 * @return A reference to the stream itself.
 * @throw stream_error If `pf` is null.
 * @endif
 */
template <istream_type T>
T& operator >> (T& obj, void(*pf)(T&))
{
    try
    {
        if (!pf)
            throw stream_error("istream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 提取操纵符：应用一个作用于具体流类型 `T&` 的 `std::function` 操纵符。
 * @tparam T 输入流类型。
 * @param obj 输入流。
 * @param pf 操纵符可调用对象。
 * @return 流自身的引用。
 * @throw stream_error 若 `pf` 为空。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction manipulator: applies a `std::function` manipulator taking the concrete
 *        stream type `T&`.
 * @tparam T The input stream type.
 * @param obj The input stream.
 * @param pf The manipulator callable.
 * @return A reference to the stream itself.
 * @throw stream_error If `pf` is empty.
 * @endif
 */
template <istream_type T>
T& operator >> (T& obj, const std::function<void(T&)>& pf)
{
    try
    {
        if (!pf)
            throw stream_error("istream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 提取操纵符：应用一个作用于 `T` 某个基类 `U&` 的函数指针操纵符。
 * @tparam T 输入流类型。
 * @tparam U `T` 的基类类型。
 * @param obj 输入流。
 * @param pf 操纵符函数指针。
 * @return 流自身的引用。
 * @throw stream_error 若 `pf` 为空。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction manipulator: applies a function-pointer manipulator taking a base class
 *        `U&` of `T`.
 * @tparam T The input stream type.
 * @tparam U A base class type of `T`.
 * @param obj The input stream.
 * @param pf The manipulator function pointer.
 * @return A reference to the stream itself.
 * @throw stream_error If `pf` is null.
 * @endif
 */
template <istream_type T, typename U>
    requires std::derived_from<T, U>
T& operator >> (T& obj, void(*pf)(U&))
{
    try
    {
        if (!pf)
            throw stream_error("istream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 提取操纵符：应用一个作用于 `T` 某个基类 `U&` 的 `std::function` 操纵符。
 * @tparam T 输入流类型。
 * @tparam U `T` 的基类类型。
 * @param obj 输入流。
 * @param pf 操纵符可调用对象。
 * @return 流自身的引用。
 * @throw stream_error 若 `pf` 为空。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction manipulator: applies a `std::function` manipulator taking a base class
 *        `U&` of `T`.
 * @tparam T The input stream type.
 * @tparam U A base class type of `T`.
 * @param obj The input stream.
 * @param pf The manipulator callable.
 * @return A reference to the stream itself.
 * @throw stream_error If `pf` is empty.
 * @endif
 */
template <istream_type T, typename U>
    requires std::derived_from<T, U>
T& operator >> (T& obj, const std::function<void(U&)>& pf)
{
    try
    {
        if (!pf)
            throw stream_error("istream manipulator fail: null or empty manipulator");
        pf(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 格式化提取运算符：按当前 locale 从流中解析出一个 `TValue` 类型的值。
 *
 * 依据 `ios_defs::skipws` 标志决定是否跳过前导空白，随后借助为该值类型（或其解析上下文
 * 类型 `TCtx`）注册的 `reader` 完成解析。当解析上下文类型与目标类型不同时，先解析到临时
 * 上下文对象，再显式转换为 `TValue`。
 * @tparam T 输入流类型。
 * @tparam TValue 目标值类型；必须存在对应的 `reader` 定义。
 * @param obj 输入流。
 * @param value 用于接收解析结果的引用。
 * @return 流自身的引用。
 * @throw stream_error 若在未提取到任何值时已处于 EOF。
 * @note 解析过程中观察到输入结束时置位 `eofbit`；其他异常统一交由 `handle_exception` 处理。
 * @endif
 *
 * @lang{EN}
 * @brief Formatted extraction operator: parses a value of type `TValue` from the stream
 *        under the current locale.
 *
 * Whether leading whitespace is skipped is governed by the `ios_defs::skipws` flag; parsing
 * is then delegated to the `reader` registered for the value type (or its parse-context type
 * `TCtx`). When the parse-context type differs from the target type, the value is first parsed
 * into a temporary context object and then explicitly converted to `TValue`.
 * @tparam T The input stream type.
 * @tparam TValue The target value type; a corresponding `reader` definition must exist.
 * @param obj The input stream.
 * @param value Reference receiving the parsed result.
 * @return A reference to the stream itself.
 * @throw stream_error If EOF is already reached with no value extracted.
 * @note Sets `eofbit` when end of input is observed during parsing; other exceptions are
 *       routed through `handle_exception`.
 * @endif
 */
template <istream_type T, typename TValue>
    requires is_reader_def<typename T::char_type,
                           typename parse_context_type<typename T::char_type, TValue>::type>
T& operator>>(T& obj, TValue& value)
{
    using TChar = typename T::char_type;
    using TCtx = typename parse_context_type<TChar, TValue>::type;
    using sentry_type = typename T::in_sentry_type;

    bool saw_eof = false;
    std::unique_lock lk(obj.io_mutex(), std::defer_lock);
    try
    {
        auto iter = obj.i_iter(&saw_eof);
        bool skip = bool(obj.flags() & ios_defs::skipws);
        sentry_type cerb(obj, !skip, lk);

        if constexpr (is_reader_def<TChar, TCtx>)
        {
            if (obj.eof())
                throw stream_error("istream extraction fail: reached EOF with no value extracted");

            if constexpr (std::is_same_v<TCtx, TValue>)
                reader<TChar, TValue>::sread(iter, std::default_sentinel, obj, obj.locale(), value);
            else
            {
                TCtx tmp;
                reader<TChar, TCtx>::sread(iter, std::default_sentinel, obj, obj.locale(), tmp);
                value = static_cast<TValue>(tmp);
            }

            if (saw_eof) obj.setstate(ios_defs::eofbit);
        }
        else
            static_assert(dependent_false_v<TValue>, "No parse method provided");
    }
    catch(...)
    {
        obj.handle_exception(std::current_exception(), saw_eof);
    }

    return obj;
}
}
