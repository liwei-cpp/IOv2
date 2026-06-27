#pragma once
#include <common/clocale_wrapper.h>
#include <common/sing_temp.h>
#include <facet/facet_common.h>
#include <facet/messages_details.h>

#include <clocale>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace IOv2
{
class ori_facet_buf : public sing_temp<ori_facet_buf>
{
    friend sing_temp<ori_facet_buf>;

public:
    template <typename TF>
    std::shared_ptr<abs_ft> try_get(const std::string& name)
    {
        const std::size_t id = TF::id();

        {
            std::lock_guard guard(m_mutex);
            auto& sub_cache = m_cache[id];
            auto it = sub_cache.find(name);
            if (it != sub_cache.end())
                return it->second;
        }

        // Construct the facet *outside* the lock: a facet constructor may be
        // expensive and -- crucially -- may itself call back into ori_facet_buf, so
        // building under m_mutex would both serialize all construction and risk a
        // self-deadlock on this non-recursive mutex. Re-lock only to insert, and
        // double-check via emplace: if another thread (or a re-entrant call)
        // inserted the same (id, name) meanwhile, emplace no-ops and returns the
        // existing entry, so the object built here is simply discarded.
        auto obj = std::make_shared<TF>(name);

        std::lock_guard guard(m_mutex);
        auto& sub_cache = m_cache[id];
        return sub_cache.emplace(name, std::move(obj)).first->second;
    }

    template <typename TChar>
    std::shared_ptr<messages_conf<TChar>> try_get_msg(const std::string& domain, const std::string& lang, const std::string& cvt = "")
    {
        const std::size_t id = messages_conf<TChar>::id();

        std::lock_guard guard(m_mutex);
        auto& sub_cache = m_msg_cache[id];

        auto it = sub_cache.find(msg_key{domain, lang, cvt});
        if (it == sub_cache.end())
            return nullptr;
        return std::static_pointer_cast<messages_conf<TChar>>(it->second);
    }

    template <typename TChar>
    std::shared_ptr<messages_conf<TChar>> put_msg(std::shared_ptr<messages_conf<TChar>> ptr, const std::string& domain, const std::string& lang, const std::string& cvt = "")
    {
        if (ptr)
        {
            const std::size_t id = messages_conf<TChar>::id();

            std::lock_guard guard(m_mutex);
            auto& sub_cache = m_msg_cache[id];

            auto it = sub_cache.find(msg_key{domain, lang, cvt});
            if (it != sub_cache.end())
                return std::static_pointer_cast<messages_conf<TChar>>(it->second);
            sub_cache.emplace(msg_key{domain, lang, cvt}, ptr);
        }
        return ptr;
    }

    /**
     * @lang{ZH}
     * 返回某个 `LC_*` 类别在**程序启动时由环境变量解析得到**的 locale 名称。
     *
     * 这些名称在单例构造（静态初始化期、`main` 之前、单线程）时由
     * `resolve_locale` 一次性解析并保存，此后只读、不再变化；因此该访问器返回的是
     * 不可变成员的常量引用，无需加锁，也不会与 `setlocale` 写者发生数据竞争——
     * 它根本不读取进程级全局 C locale。
     *
     * @note 这意味着本库的 locale 来源是**环境变量**，而**不**跟随运行期对
     * `std::setlocale` 的编程式修改。
     * @endif
     *
     * @lang{EN}
     * Return the locale name for an `LC_*` category as *resolved from the
     * environment at program startup*.
     *
     * The names are resolved once by `resolve_locale` during singleton
     * construction (static-init time, before `main`, single-threaded) and are
     * read-only thereafter; this accessor hands back a const reference to an
     * immutable member, so it needs no lock and cannot race a `setlocale` writer
     * -- it never reads the process-wide global C locale at all.
     *
     * @note Consequently the library's locale source is the *environment*; it does
     * *not* follow runtime programmatic changes via `std::setlocale`.
     * @endif
     */
    const std::string& locale_name(int category) const
    {
        switch (category)
        {
        case LC_COLLATE:
            return m_collate;
        case LC_CTYPE:
            return m_ctype;
        case LC_MONETARY:
            return m_monetary;
        case LC_NUMERIC:
            return m_numeric;
        case LC_TIME:
            return m_time;
        default:
            {
                static const std::string c_locale = "C";
                return c_locale;
            }
        }
    }

private:
    ori_facet_buf()
        : m_ctype(resolve_locale("LC_CTYPE")),
          m_collate(resolve_locale("LC_COLLATE")),
          m_monetary(resolve_locale("LC_MONETARY")),
          m_numeric(resolve_locale("LC_NUMERIC")),
          m_time(resolve_locale("LC_TIME"))
    {
    }
    ori_facet_buf(const ori_facet_buf&) = delete;
    const ori_facet_buf& operator=(const ori_facet_buf&) = delete;

    /**
     * @lang{ZH}
     * 按 POSIX/glibc 中 `setlocale(category, "")` 的解析顺序，从环境变量推导出某个
     * 类别应使用的 locale 名称：`LC_ALL` > `LC_<category>` > `LANG` > `"C"`，其中
     * 每一级都要求该变量**已设置且非空**才被采用。解析出的名称随后会经 C 库校验
     * （`newlocale`）：若无法实例化（如非法的 `LANG`），回退为 `"C"`。
     *
     * 仅在单例构造期（静态初始化、`main` 之前、单线程）被调用，此时不存在其它线程，
     * 也没有 `setenv` 写者，因此对 `getenv` 的读取 by-construction 无竞争。
     * @endif
     *
     * @lang{EN}
     * Derive the locale name for a category from the environment, following the
     * same precedence as POSIX/glibc `setlocale(category, "")`:
     * `LC_ALL` > `LC_<category>` > `LANG` > `"C"`, where each level is used only if
     * that variable is *set and non-empty*. The resolved name is then validated via
     * the C library (`newlocale`); if it cannot be instantiated (e.g. a bogus
     * `LANG`) it falls back to `"C"`.
     *
     * Called only during singleton construction (static-init, before `main`,
     * single-threaded); no other thread and no `setenv` writer exists yet, so these
     * `getenv` reads are race-free by construction.
     * @endif
     */
    static std::string resolve_locale(const char* category_var)
    {
        std::string name = "C";
        if (const char* all = std::getenv("LC_ALL"); all && *all)
            name = all;
        else if (const char* cat = std::getenv(category_var); cat && *cat)
            name = cat;
        else if (const char* lang = std::getenv("LANG"); lang && *lang)
            name = lang;

        // Validate the resolved name against the C library: clocale_wrapper builds a
        // locale_t via newlocale and throws if the name cannot be instantiated. An
        // unusable name (e.g. a bogus LANG) falls back to the neutral "C". Done here,
        // once per category at construction, so locale_name stays a plain O(1) read.
        try
        {
            clocale_wrapper probe(name.c_str());
            return name;
        }
        catch (...)
        {
            return "C";
        }
    }

private:
    // Per-category locale names resolved from the environment at construction time
    // (see resolve_locale / locale_name). Written once during static init, then
    // immutable -- safe to read concurrently without locking.
    std::string m_ctype;
    std::string m_collate;
    std::string m_monetary;
    std::string m_numeric;
    std::string m_time;

    // Key for the messages cache: (domain, lang, cvt). Using a tuple instead of a
    // '\n'-joined string removes the ambiguity collision that arises when a
    // component itself contains the separator (e.g. a domain with an embedded
    // '\n'): distinct (domain, lang, cvt) triples always map to distinct keys.
    using msg_key = std::tuple<std::string, std::string, std::string>;

    struct msg_key_hash
    {
        std::size_t operator()(const msg_key& k) const noexcept
        {
            const std::hash<std::string> h;
            std::size_t seed = h(std::get<0>(k));
            seed ^= h(std::get<1>(k)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h(std::get<2>(k)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    std::unordered_map<size_t, std::unordered_map<std::string, std::shared_ptr<abs_ft>>> m_cache;
    std::unordered_map<size_t, std::unordered_map<msg_key, std::shared_ptr<abs_ft>, msg_key_hash>> m_msg_cache;
    // Guards m_cache / m_msg_cache. Facet construction in try_get happens *outside*
    // this lock (it is taken only to look up and to insert), so the mutex is never
    // held across a facet constructor. That keeps construction unserialized and
    // makes a re-entrant facet ctor safe even though the mutex is non-recursive.
    std::mutex m_mutex;
};

static ori_facet_buf::init _ori_facet_buf_init;
static ori_facet_buf& s_ori_facet_buf = *ori_facet_buf::ptr();
}
