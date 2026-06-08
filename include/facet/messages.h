/**
 * @file messages.h
 * @lang{ZH}
 * 定义了 `messages<CharT>` 类，这是 gettext 风格消息翻译的用户端 facet。
 * 它封装了一个 `messages_conf<CharT>` 实例，并提供三个 `translate` 重载，
 * 通过值类别（lvalue / rvalue / const rvalue）区分，在避免悬空引用的同时
 * 将不必要的拷贝降至最低。
 * @endif
 *
 * @lang{EN}
 * Defines the `messages<CharT>` class, the user-facing facet for gettext-style
 * message translation. It wraps a `messages_conf<CharT>` instance and provides
 * three `translate` overloads distinguished by value category
 * (lvalue / rvalue / const rvalue), minimizing unnecessary copies while
 * preventing dangling references.
 * @endif
 */
#pragma once
#include <common/metafunctions.h>
#include <facet/facet_common.h>
#include <facet/messages_details.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief gettext 风格消息翻译的用户端 facet。
 *
 * `messages<CharT>` 持有一个指向 `messages_conf<CharT>` 的共享指针，
 * 后者负责从 `.mo` 文件加载翻译字典。`translate` 提供三个重载，
 * 分别处理 lvalue、rvalue 和 const rvalue 输入：
 * - **lvalue**：借用语义（零拷贝），返回对字典或原始参数的引用，
 *   调用者需保证参数在结果的生命周期内有效。
 * - **rvalue** 与 **const rvalue**：按值返回，结果始终持有独立数据，不会悬空。
 *
 * @tparam CharT 字符类型，由所用的 `messages_conf` 特化决定。
 * @endif
 *
 * @lang{EN}
 * @brief User-facing facet for gettext-style message translation.
 *
 * `messages<CharT>` holds a shared pointer to a `messages_conf<CharT>` that
 * loads the translation dictionary from a `.mo` file. Three `translate`
 * overloads cover lvalue, rvalue, and const rvalue inputs:
 * - **lvalue**: borrowing semantics (zero copy) — returns a reference into
 *   the dictionary or to the original argument; the caller must keep the
 *   argument alive for as long as the result is used.
 * - **rvalue** and **const rvalue**: return by value so the result always
 *   owns its data and can never dangle.
 *
 * @tparam CharT The character type, determined by the `messages_conf` specialization used.
 * @endif
 */
template <typename CharT>
class messages
{
public:
    /// @cond
    using create_rules = facet_create_rule<messages_conf<CharT>>;
    /// @endcond

    using char_type = CharT; ///< @lang{ZH} 此 facet 使用的字符类型。 @endif @lang{EN} The character type used by this facet. @endif

    /**
     * @lang{ZH}
     * @brief 构造函数，从指向 `messages_conf<CharT>` 的共享指针创建 facet。
     *
     * @tparam TConfPtr 满足 `shared_ptr_to<messages_conf<CharT>>` 约束的指针类型。
     * @param p_obj 指向已初始化的 `messages_conf<CharT>` 的非空共享指针。
     * @throw std::runtime_error 如果 `p_obj` 为空。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that creates the facet from a shared pointer to `messages_conf<CharT>`.
     *
     * @tparam TConfPtr A pointer type satisfying `shared_ptr_to<messages_conf<CharT>>`.
     * @param p_obj A non-null shared pointer to an initialized `messages_conf<CharT>`.
     * @throw std::runtime_error If `p_obj` is empty.
     * @endif
     */
    template <shared_ptr_to<messages_conf<CharT>> TConfPtr>
    messages(TConfPtr p_obj)
        : m_obj(p_obj)
    { if (!m_obj) throw std::runtime_error("shared_ptr is empty"); }

    /**
     * @lang{ZH}
     * @brief 翻译 lvalue 字符串（借用语义，零拷贝）。
     *
     * gettext 传递语义：命中时返回翻译，未命中时返回原始 `ori`。
     * 返回值是对字典条目（在 facet 生命周期内稳定）或 `ori` 本身的引用，
     * 因此调用者的 lvalue 必须在返回值的生命周期内保持有效。
     * 只有 lvalue 会选择此重载；所有 rvalue（包括 `const` rvalue）
     * 均路由到下方的按值返回重载，从而保证临时参数不会悬空。
     *
     * @param ori 原始字符串（`msgid`）的 lvalue 引用。
     * @return 对翻译字符串或 `ori` 的 const 引用。
     * @endif
     *
     * @lang{EN}
     * @brief Translates an lvalue string with borrowing semantics (zero copy).
     *
     * Gettext pass-through: returns the translation on a hit, otherwise the
     * original `ori`. The result aliases either the dictionary (stable for the
     * facet's lifetime) or `ori` itself on a miss, so the caller's lvalue must
     * outlive the result. Only an lvalue selects this overload; every rvalue —
     * including a `const` one — routes to a by-value overload below, so a
     * temporary argument can never dangle.
     *
     * @param ori An lvalue reference to the original string (`msgid`).
     * @return A const reference to the translated string or to `ori`.
     * @endif
     */
    const std::basic_string<char_type>& translate(const std::basic_string<char_type>& ori) const
    {
        const static std::basic_string<char_type> empty_res;
        if (ori.empty()) return empty_res;

        const auto* p = m_obj->translate(ori);
        return p ? *p : ori;
    }

    /**
     * @lang{ZH}
     * @brief 翻译 rvalue 字符串，按值返回以确保结果不悬空。
     *
     * 按值返回，结果始终持有独立数据：未命中时将调用者的临时对象移出（无拷贝），
     * 命中时从字典条目拷贝。这使得以字面量或临时对象调用此函数是安全的，
     * 而不需要禁止这种用法。
     *
     * @param ori 原始字符串（`msgid`）的 rvalue 引用。
     * @return 翻译字符串或原始字符串（按值）。
     * @endif
     *
     * @lang{EN}
     * @brief Translates an rvalue string, returning by value so the result cannot dangle.
     *
     * Returns by value so the result always owns its data: a miss moves the
     * caller's temporary out (no copy), a hit copies the dictionary entry out.
     * This makes the common literal/temporary call shape safe without forbidding it.
     *
     * @param ori An rvalue reference to the original string (`msgid`).
     * @return The translated string or the original string (by value).
     * @endif
     */
    std::basic_string<char_type> translate(std::basic_string<char_type>&& ori) const
    {
        if (ori.empty()) return {};

        const auto* p = m_obj->translate(ori);
        return p ? *p : std::move(ori);
    }

    /**
     * @lang{ZH}
     * @brief 翻译 const rvalue 字符串，按值返回以确保结果不悬空。
     *
     * `const` rvalue 无法绑定到上方的非 `const` `&&` 重载；若没有此重载，
     * 它将回退到借用语义的 lvalue 重载，并在未命中时悬空
     * （返回值将引用调用者即将销毁的临时对象）。
     * 此处也按值返回：由于是 `const` 无法移出，未命中时拷贝 `ori`，
     * 命中时拷贝字典条目——无论哪种情况结果都持有独立数据。
     *
     * @param ori 原始字符串（`msgid`）的 const rvalue 引用。
     * @return 翻译字符串或原始字符串（按值）。
     * @endif
     *
     * @lang{EN}
     * @brief Translates a const rvalue string, returning by value so the result cannot dangle.
     *
     * A `const` rvalue cannot bind to the non-const `&&` overload above; without
     * this overload it would fall back to the borrowing lvalue overload and, on a
     * miss, dangle (the result would reference the caller's expiring temporary).
     * Return by value here too: being `const` it cannot be moved from, so a miss
     * copies `ori` out and a hit copies the dictionary entry — either way the
     * result owns its data.
     *
     * @param ori A const rvalue reference to the original string (`msgid`).
     * @return The translated string or the original string (by value).
     * @endif
     */
    std::basic_string<char_type> translate(const std::basic_string<char_type>&& ori) const
    {
        if (ori.empty()) return {};

        const auto* p = m_obj->translate(ori);
        return p ? *p : ori;
    }

    /**
     * @lang{ZH}
     * @brief 返回构造时确定的有效语言标识符。
     * @return 过滤后的语言字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the effective language identifier determined at construction.
     * @return The filtered language string.
     * @endif
     */
    [[nodiscard]] const std::string& filtered_lang() const
    {
        return m_obj->filtered_lang();
    }

    /**
     * @lang{ZH}
     * @brief 返回描述该 facet 实例的域信息字符串。
     *
     * 格式为 `[domain] [lang(filtered_lang)] [dirname]`，可用于调试和日志记录。
     *
     * @return 包含域、语言和目录信息的字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns a string describing this facet instance's domain information.
     *
     * The format is `[domain] [lang(filtered_lang)] [dirname]`, useful for
     * debugging and logging.
     *
     * @return A string containing domain, language, and directory information.
     * @endif
     */
    [[nodiscard]] const std::string& domain_info() const
    {
        return m_obj->domain_info();
    }

    /**
     * @lang{ZH}
     * @brief 返回 `.mo` 文件的头部条目。
     *
     * 在 GNU gettext 的 `.mo` 格式中，`msgid` 为空字符串的条目存储文件头，
     * 其 `msgstr` 包含版本、字符集、复数规则等元数据。若头部条目不存在，
     * 则返回空字符串。
     *
     * @return 对头部条目（`msgid` 为空字符串对应的 `msgstr`）的 const 引用，
     *         若不存在则为空字符串。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the header entry of the `.mo` file.
     *
     * In the GNU gettext `.mo` format, the entry whose `msgid` is the empty
     * string stores the file header, whose `msgstr` contains metadata such as
     * version, charset, and plural-form rules. Returns an empty string if no
     * header entry is present.
     *
     * @return A const reference to the header entry (`msgstr` for the empty-string `msgid`),
     *         or an empty string if absent.
     * @endif
     */
    const std::basic_string<char_type>& head_entry() const
    {
        const static std::basic_string<char_type> empty_msgid;
        const auto* p = m_obj->translate(empty_msgid);
        return p ? *p : empty_msgid;
    }

private:
    std::shared_ptr<const messages_conf<CharT>> m_obj;
};

/// @cond
template<typename TConfPtr>
messages(TConfPtr) -> messages<typename TConfPtr::element_type::char_type>;
/// @endcond
}
