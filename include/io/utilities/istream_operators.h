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
 * 哨兵负责校验流的有效性、刷新关联（tie）流，并在需要时跳过前导空白，
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
 * The sentry validates the stream, flushes the tied stream,
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
     * @brief 构造输入哨兵：校验流、刷新关联流，并按需跳过前导空白。
     *
     * @warning **调用方必须已经持有 `is.io_mutex()`，且要持到自己的 `catch` 之后。** 哨兵自己
     *          不加锁：它在 `try` 块末尾就析构了，而 `catch` 里的 `handle_exception` 需要在锁内
     *          更新流状态，才能让成功路径与失败路径对同一把 `io_mutex()` 的可见性保持一致。锁因此
     *          必须是调用方的局部变量，而不是哨兵的成员。该前置条件无法在运行期校验——
     *          `copyable_mutex` 不记录属主，递归锁的 `try_lock()` 也分不清"我已持有"与"无人持有"。
     *
     * 关联流的刷新走 `abs_flusher::try_flush()`，取不到对方的锁就跳过，绝不阻塞。本线程因此可以
     * 安全地在持有本流锁的状态下发起它：tie 这条用户看不见的加锁边永远不会成为等待边，死锁只可能
     * 由用户自己能定序的锁构成。
     * @param is 要操作的输入流。
     * @param noskip 若为 `true`，则不跳过前导空白；若为 `false`，按当前 locale 的
     *               空白定义跳过前导空白。
     * @throw stream_error 若流无效，或缺少 ctype facet。
     * @throw eof_error 若在跳过空白的过程中到达输入结尾。
     * @endif
     * @lang{EN}
     * @brief Constructs the input sentry: validates the stream, flushes the tied stream, and
     * skips leading whitespace if requested.
     *
     * @warning **The caller must already hold `is.io_mutex()`, and must keep holding it past its
     *          own `catch`.** The sentry does not lock: it is destroyed at the end of the
     *          enclosing `try`, while `handle_exception` in the `catch` needs the lock to update
     *          the stream state, so that the success and failure paths stay consistent with
     *          respect to the same `io_mutex()`. The lock therefore has to be a local of the
     *          caller rather than a member of the sentry. The precondition cannot be checked at
     *          run time -- `copyable_mutex` tracks no owner, and a recursive mutex's `try_lock()`
     *          cannot tell "this thread already holds it" from "nobody holds it".
     *
     * The tied stream is flushed through `abs_flusher::try_flush()`, which skips the flush rather
     * than wait when the target's lock cannot be taken. This thread can therefore start it safely
     * while holding its own stream's lock: the tie edge -- the one lock edge the user cannot see
     * -- never becomes a waiting edge, so any deadlock can only be built from locks the user is
     * able to order.
     * @param is The input stream to operate on.
     * @param noskip If `true`, leading whitespace is not skipped; if `false`, leading
     *               whitespace is skipped according to the current locale's definition.
     * @throw stream_error If the stream is invalid or the ctype facet is missing.
     * @throw eof_error If end of input is reached while skipping whitespace.
     * @endif
     */
    in_sentry(TStream& is, bool noskip)
        : m_is(is)
    {
        if (!m_is)
            throw stream_error("istream_sentry create fail: Invalid istream");

        if (auto* tied = is.tie())
        {
            try { tied->try_flush(); }
            catch (...) {} // NOLINT(bugprone-empty-catch)
        }

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
    TStream& m_is;
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
 * locale，且其 `in_sentry_type` 满足 `is_in_sentry`；同时它必须派生自
 * `ios_base<char_type>`、`io_state_and_exp` 与 `istream_operators<char_type>`。
 * @note `io_state_and_exp` 这一条是必需的，不只是描述性的：本概念约束下的代码会直接调用
 *       `handle_exception()`（`_Ws::operator()`、`in_sentry` 的构造）与 `operator bool`。
 *       缺了它，这些调用要到模板**体**实例化时才报错，而消费操纵符的运算符只用
 *       `std::invocable` 检查声明层面的可调用性，兜底重载那条简短诊断路径就被绕开了。
 * @tparam T 待检测的类型。
 * @endif
 *
 * @lang{EN}
 * @brief Concept for an input stream type.
 *
 * To qualify as an input stream, a type must expose the `in_sentry_type` and `char_type`
 * types, be able to return its locale, and have an `in_sentry_type` that satisfies
 * `is_in_sentry`; it must also derive from `ios_base<char_type>`, `io_state_and_exp` and
 * `istream_operators<char_type>`.
 * @note The `io_state_and_exp` clause is a requirement, not just a description: code
 *       constrained by this concept calls `handle_exception()` (in `_Ws::operator()` and in
 *       `in_sentry`'s constructor) and `operator bool` directly. Without it those calls only
 *       fail once the template **body** is instantiated, and since the operators that consume
 *       manipulators check callability with `std::invocable` -- a declaration-level check --
 *       the short diagnostic the fallback overload exists to produce is bypassed.
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
    std::derived_from<T, io_state_and_exp> &&
    std::derived_from<T, istream_operators<typename T::char_type>>;

/**
 * @lang{ZH}
 * @brief 为输入流提供各类提取操作的混入（mix-in）基类。
 *
 * 本模板集中承载 `get`、`peek`、`read`、`ignore`、`putback` 等成员，供具体输入流类型
 * 派生使用；这些成员通过 deducing-this（`this TSelf& self`）以派生类的具体类型执行操作。
 * 每个操作先取本流的 `io_mutex()`，再在锁内构造输入哨兵以完成前置准备，并将异常统一交由流的
 * `handle_exception` 处理，从而按异常掩码更新流状态。
 * @tparam TChar 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Mix-in base that provides the various extraction operations for input streams.
 *
 * This template centralizes members such as `get`, `peek`, `read`, `ignore`, and `putback`
 * for concrete input stream types to derive from; these members use deducing-this
 * (`this TSelf& self`) to operate on the concrete derived type. Each operation takes the
 * stream's `io_mutex()` and then constructs an input sentry under it to handle the setup, and
 * routes exceptions through the stream's `handle_exception`, which updates the stream state
 * according to the exception mask.
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
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
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
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
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
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
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
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
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
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
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
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);

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
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
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
        std::lock_guard guard(self.io_mutex());
        try
        {
            using sentry_type = typename TSelf::in_sentry_type;
            sentry_type cerb(self, true);
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
 * @brief 提取操纵符：应用一个可用于输入方向的操纵符对象。
 *
 * 本重载把流交给操纵符自己的 `operator()`：**加锁由操纵符负责**。各操纵符所需的锁作用域并
 * 不相同（`ws` 要在构造 `in_sentry` 之前先加锁，并在锁内调用 `handle_exception`；`endl` 要把
 * 读 locale 与 `put()` 一起罩在锁内；`ends`/`flush` 完全不需要显式加锁），无法上提到本函数。
 *
 * 这里的 `catch` 是**兜底**而非主机制：库内置的操纵符都在自己的临界区里就地处理了异常，走
 * 不到这里；它保证的是自定义操纵符即使漏了异常处理，也仍然按本库的错误模型置位并遵守异常
 * 掩码，而不是把异常抛给调用方。
 * @note 约束里的 `std::invocable` 不是多余的：只看基类的话，`is >> setfill(L'*')` 这种字符
 *       类型不匹配的调用仍然可行，错误要到 `f(obj)` 实例化时才在模板体内爆出来。加上它，
 *       本重载直接不可行，调用落到兜底重载，报出那条指明原因的 `static_assert`。
 * @tparam T 输入流类型。
 * @tparam TManip 操纵符类型，须派生自 `in_manip` 且能以本流调用。
 * @param obj 输入流。
 * @param f 操纵符对象。
 * @return 流自身的引用。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction manipulator: applies a manipulator object usable in the input direction.
 *
 * This overload hands the stream to the manipulator's own `operator()`: **locking is the
 * manipulator's job**. The required lock scope differs per manipulator (`ws` must take the lock
 * before constructing `in_sentry` and calls `handle_exception` under it; `endl` must cover both
 * the locale read and the `put()`; `ends`/`flush` need no explicit lock at all), so it cannot be
 * hoisted here.
 *
 * The `catch` here is a **backstop**, not the primary mechanism: the library's own manipulators
 * handle their exceptions inside their own critical sections and never reach it. What it
 * guarantees is that a user-defined manipulator which forgets to handle its exceptions still
 * sets state through this library's error model and honours the exception mask, rather than
 * throwing at the caller.
 * @note The `std::invocable` in the constraint is not redundant: on the base classes alone a
 *       call whose character type does not match, such as `is >> setfill(L'*')`, would still be
 *       viable and would only blow up inside the template body when `f(obj)` is instantiated.
 *       With it, this overload is simply not viable, the call lands on the fallback, and the
 *       `static_assert` there says why.
 * @tparam T The input stream type.
 * @tparam TManip The manipulator type, which must derive from `in_manip` and be callable with
 *         this stream.
 * @param obj The input stream.
 * @param f The manipulator object.
 * @return A reference to the stream itself.
 * @endif
 */
template <istream_type T, std::derived_from<in_manip> TManip>
    requires std::invocable<const TManip&, T&>
T& operator >> (T& obj, const TManip& f)
{
    try
    {
        f(obj);
    }
    catch (...)
    {
        obj.handle_exception(std::current_exception());
    }
    return obj;
}

/**
 * @lang{ZH}
 * @brief 已删除：仅用于输出方向的操纵符不能被提取。
 *
 * 在双向流上，若无本重载，`io >> endl` 会静默地写出一个换行并刷新，而不读入任何东西——写法
 * 与实际方向相反，且不置任何状态位。删除本重载使其成为编译错误，与标准库一致。正确写法是
 * `os << endl`。
 * @note 约束里必须排除同时派生自 `in_manip` 的类型。两个方向都合法的操纵符（如 `setw`）会
 *       同时派生两个基类，若不排除，本重载与上面那个真实重载会同时可行且互不包含，导致该
 *       操纵符在**任何**方向上都因二义性而不可用。
 * @tparam T 输入流类型。
 * @tparam TManip 操纵符类型，派生自 `out_manip` 且不派生自 `in_manip`。
 * @endif
 *
 * @lang{EN}
 * @brief Deleted: a manipulator meant for the output direction only cannot be extracted.
 *
 * Without this overload, `io >> endl` on a bidirectional stream would silently write a newline
 * and flush, reading nothing at all -- the opposite of what the expression reads like, and with
 * no state bit set. Deleting it makes that a compile error, as in the standard library. Write
 * `os << endl` instead.
 * @note The constraint must exclude types that also derive from `in_manip`. A manipulator legal
 *       in both directions (`setw`, say) derives from both bases; without the exclusion this
 *       overload and the real one above would both be viable and neither would subsume the
 *       other, making that manipulator ambiguous -- and therefore unusable -- in **either**
 *       direction.
 * @tparam T The input stream type.
 * @tparam TManip The manipulator type, deriving from `out_manip` but not from `in_manip`.
 * @endif
 */
template <istream_type T, typename TManip>
    requires std::derived_from<TManip, out_manip> && (!std::derived_from<TManip, in_manip>)
T& operator >> (T& obj, const TManip& f) = delete;

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
    std::lock_guard guard(obj.io_mutex());
    try
    {
        auto iter = obj.i_iter(&saw_eof);
        bool skip = bool(obj.flags() & ios_defs::skipws);
        sentry_type cerb(obj, !skip);

        if (obj.eof())
            throw stream_error("istream extraction fail: reached EOF with no value extracted");

        if constexpr (std::is_same_v<TCtx, TValue>)
            reader<TChar, TValue>::sread(iter, std::default_sentinel, obj, obj.locale(), value);
        else
        {
            TCtx tmp = [&value]() -> TCtx {
                if constexpr (requires (const TValue& v)
                              { parse_context_type<TChar, TValue>::make_parse_context(v); })
                    return parse_context_type<TChar, TValue>::make_parse_context(value);
                else
                    return TCtx{};
            }();
            reader<TChar, TCtx>::sread(iter, std::default_sentinel, obj, obj.locale(), tmp);
            value = static_cast<TValue>(tmp);
        }

        if (saw_eof) obj.setstate(ios_defs::eofbit);
    }
    catch(...)
    {
        obj.handle_exception(std::current_exception(), saw_eof);
    }

    return obj;
}

/**
 * @lang{ZH}
 * @brief 兜底重载：当提取无法成立时，给出一条简短的编译期错误。
 *
 * 覆盖两类失败：目标类型没有对应的 `reader` 特化；或目标不是可修改左值（临时量、`const`
 * 对象）。若没有本重载，前者只会得到一句 `no match for 'operator>>'` 外加三十余个候选的
 * 转储，后者更会在 facet 内部炸出几十行"向只读变量赋值"——真正的原因都埋在里面。
 *
 * @note 本重载**不加约束**是刻意的：它要能接住任意类型。它不会抢走本该成功的调用，因为
 *       所有正常路径在重载决议中都严格优于它——
 *       - 上面的格式化提取运算符收 `TValue&`，对非常量左值是非常量引用绑定，优于本重载的
 *         常量引用绑定；两者签名不等价时由此决出，等价时则由"约束更多者胜"决出。
 *       - 取 `ios_base<char_type>&` 的两个操纵符重载与本重载转换序列打平，转由模板偏序裁决，
 *         而它们的形参类型更特化；取 `const TManip&` 的方向标签重载与本重载签名等价，靠
 *         "约束更多者胜"取胜。
 * @note 因此落到本重载的操纵符只有两类：没有派生方向标签基类的（用户自己写的函数或
 *       lambda），以及派生了但 `operator()` 不接受本流的（如在 `char` 流上用
 *       `setfill(L'*')`）。下面的错误消息把这两类也一并点出来。
 * @warning 形参必须是 `const TValue&` 而非 `TValue&`。写成后者会以非常量引用绑定**压过**
 *          所有操纵符重载的常量引用绑定，把 `is >> manip` 之类的调用全部劫持到这里、变成
 *          编译错误——这正是当初给上面那个运算符加 `requires` 所要修复的缺陷，
 *          见 `test_istream_ws_char.cpp` 中的回归用例。
 * @tparam TValue 被提取的目标类型。
 * @endif
 *
 * @lang{EN}
 * @brief Fallback overload: emits one short compile-time error when an extraction cannot work.
 *
 * It covers two kinds of failure: the target type has no `reader` specialization, or the target
 * is not a modifiable lvalue (a temporary, or a `const` object). Without this overload the
 * former yields only `no match for 'operator>>'` plus a dump of thirty-odd candidates, and the
 * latter explodes into dozens of "assignment of read-only variable" lines inside the facets --
 * in both cases burying the actual cause.
 *
 * @note Leaving this overload **unconstrained** is deliberate: it has to catch any type. It
 *       cannot steal a call that ought to succeed, because every working path outranks it in
 *       overload resolution:
 *       - The formatted extraction operator above takes `TValue&`, a non-const reference
 *         binding for a non-const lvalue, which beats this overload's const reference binding;
 *         where the signatures are equivalent instead, the more-constrained one wins.
 *       - The two manipulator overloads taking `ios_base<char_type>&` tie with this one on
 *         conversion sequence, so partial ordering decides and their more specialized parameter
 *         types win; the direction-tag overload taking `const TManip&` has an equivalent
 *         signature and wins on "more constrained".
 * @note Only two kinds of manipulator therefore reach this overload: one that does not derive
 *       from a direction tag base (a user's own function or lambda), and one that does but whose
 *       `operator()` will not take this stream (`setfill(L'*')` on a `char` stream, say). The
 *       message below names both.
 * @warning The parameter must be `const TValue&`, not `TValue&`. The latter would bind a
 *          non-const reference and thereby **outrank** every manipulator overload's const
 *          binding, hijacking calls such as `is >> manip` into this overload and turning them
 *          into compile errors -- precisely the defect that constraining the operator above was
 *          meant to fix; see the regression case in `test_istream_ws_char.cpp`.
 * @tparam TValue The type being extracted into.
 * @endif
 */
template <istream_type T, typename TValue>
T& operator>>(T& obj, const TValue&)
{
    static_assert(dependent_false_v<TValue>,
        "IOv2: cannot extract into this type. Either no reader<char_type, TValue> is defined "
        "for it (define one -- see io/fp_defs/base_fp.h -- or extract into a supported type "
        "such as an arithmetic type, CharT[N], or std::basic_string), or the target is not a "
        "modifiable lvalue (extraction cannot write into a temporary or a const object), or it "
        "was meant to be a manipulator, in which case it must derive from IOv2::in_manip (see "
        "io/io_base.h) and its operator() must accept this stream -- a fill character whose "
        "type differs from the stream's char_type is the usual cause. A manipulator used in "
        "the wrong direction is reported separately, as a deleted operator.");
    return obj;
}
}
