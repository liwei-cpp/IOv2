#pragma once
#include <facet/collate.h>
#include <facet/ctype.h>
#include <facet/messages.h>
#include <facet/monetary.h>
#include <facet/numeric.h>
#include <facet/timeio.h>
#include <locale/ori_facet_buf.h>

#include <bit>
#include <clocale>
#include <concepts>
#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace IOv2
{
template <typename T>
struct type_id
{
    inline static const void* s_id = nullptr;
};

template <typename T>
static const size_t type_id_v = std::bit_cast<size_t>(&(type_id<T>::s_id));

// type_id_v keys m_facets by the address of type_id<T>::s_id. This mirrors
// ft_basic::id() (facet_common.h), which guards the same idiom. bit_cast already
// requires the pointer and size_t to have equal size; the assert documents the
// invariant and yields a clear diagnostic so a platform with
// sizeof(void*) > sizeof(size_t) fails to compile instead of silently aliasing
// distinct facet types in m_facets.
static_assert(sizeof(void*) <= sizeof(size_t),
              "type_id_v relies on a pointer fitting losslessly into size_t");

/**
 * @lang{ZH}
 * @brief 管理一组 facet 的本地化对象。
 *
 * `locale` 是值语义类型，遵循与标准库容器 / `std::shared_ptr` **实例**相同的
 * 线程契约（即 "thread-compatible" / 基本保证）：
 *
 * - **并发只读是安全的**：多个线程可以同时对同一个 `const locale` 实例调用
 *   只读 / const 操作（`get`、`has`、`involve`、`involve_msg`、`remove` 以及
 *   拷贝构造）。这些操作不改变 locale 的逻辑状态；`get` / `has` 虽然会惰性填充
 *   内部派生 facet 缓存（`m_facets`），但该缓存改动由 `m_facet_mutex` 内部同步，
 *   因此并发 const 调用之间不会产生数据竞争。
 * - **赋值是写操作，需要调用方外部同步**：对某个 `locale` 实例执行 `operator=`
 *   或移动赋值时，调用方必须保证此期间没有其它线程访问**同一个实例**（无论读
 *   还是写）。这与 `std::string`、标准容器、单个 `std::shared_ptr` 实例的契约
 *   完全一致。
 *
 * @warning 不要在一个线程对某 `locale` 实例赋值的同时，让另一线程对**同一实例**
 * 调用 `get` / `has`。对复合 facet（如 `numeric`、`timeio`）而言，`get<TF>()` 是
 * 一次 "查缓存 → 构建 → 写回缓存" 的复合操作，期间会多次、分别地加锁读取各
 * 依赖 facet 的 conf。即便每次单独访问都在锁内，整个复合操作也**无法**相对于
 * 一次整体赋值保持原子：并发赋值可能使最终写入 `m_facets` 的对象由"已被替换掉
 * 的旧状态"构建而来，导致缓存与当前 conf 不一致。注意这**不是**内存安全 / UB
 * 问题（所有 `shared_ptr` 访问都在锁内，不存在撕裂读或悬空），仅是逻辑状态不
 * 一致；它必须靠上述契约（赋值期间独占访问）来避免，而非靠在 `get` 内加更细的
 * 锁——复合操作的原子性无法由内部 per-operation 加锁提供。
 *
 * @note `involve` / `involve_msg` / `remove` 均为 const：它们基于 `*this` 的一份
 * 一致快照构造并返回**新的** `locale`，从不原地修改 `*this`（写时拷贝）。因此一个
 * `locale` 实例唯一的"变异面"就是赋值；按上述契约同步赋值即可。
 * @endif
 *
 * @lang{EN}
 * @brief Localization object managing a set of facets.
 *
 * `locale` is a value type that follows the same threading contract as the
 * standard-library containers and `std::shared_ptr` *instances* (the
 * "thread-compatible" / basic guarantee):
 *
 * - **Concurrent reads are safe**: multiple threads may call read-only / const
 *   operations (`get`, `has`, `involve`, `involve_msg`, `remove`, and copy
 *   construction) on the same `const locale` instance simultaneously. These
 *   operations do not change the locale's logical state; although `get` / `has`
 *   lazily populate the internal derived-facet cache (`m_facets`), that cache
 *   mutation is synchronized internally by `m_facet_mutex`, so concurrent const
 *   calls do not race.
 * - **Assignment is a write and requires external synchronization**: when
 *   `operator=` or move-assignment is applied to a `locale` instance, the caller
 *   must ensure no other thread accesses that *same instance* (read or write)
 *   for the duration. This is exactly the contract of `std::string`, the
 *   standard containers, and a single `std::shared_ptr` instance.
 *
 * @warning Do not assign to a `locale` instance in one thread while another
 * thread calls `get` / `has` on the *same* instance. For a composite facet (e.g.
 * `numeric`, `timeio`), `get<TF>()` is a compound "check cache -> build ->
 * insert" operation that locks and reads each dependency facet's conf
 * separately, several times. Even though each individual access is locked, the
 * whole compound operation *cannot* be atomic with respect to a whole-object
 * assignment: a concurrent assignment can cause the object finally inserted into
 * `m_facets` to have been built from a now-replaced state, leaving the cache
 * inconsistent with the current confs. This is *not* a memory-safety / UB
 * problem (every `shared_ptr` access is locked -- no torn reads, no dangling);
 * it is a logical-consistency one, and it must be avoided via the contract above
 * (exclusive access during assignment) rather than by finer-grained locking
 * inside `get` -- compound-operation atomicity cannot be provided by internal
 * per-operation locks.
 *
 * @note `involve` / `involve_msg` / `remove` are const: they build and return a
 * *new* `locale` from a single consistent snapshot of `*this` and never mutate
 * `*this` in place (copy-on-write). The only mutating surface of a `locale`
 * instance is therefore assignment; synchronizing assignment per the contract
 * above is sufficient.
 * @endif
 *
 * @tparam TChar
 * @lang{ZH} 该 locale 各 facet 所用的字符类型。 @endif
 * @lang{EN} The character type used by this locale's facets. @endif
 */
template <typename TChar>
class locale
{
    template <typename T>
    struct ft_wrapper;

    template <typename... T>
    struct ft_wrapper<facet_create_pack<T...>>
    {
        ft_wrapper(const locale& l)
            : m_ref(l) {}

        bool has() const
        {
            return (m_ref.has<T>() && ...);
        }

        template<typename TF>
        std::shared_ptr<TF> get()
        {
            return get_helper<TF, facet_create_pack<T...>>();
        }

    private:
        template<typename TF, typename TRemArray, typename... TProcessed>
        std::shared_ptr<TF> get_helper(TProcessed&&... params)
        {
            if constexpr(facet_create_pack_size<TRemArray> > 0)
            {
                using head = typename facet_create_pack_head<TRemArray>::type;
                using tail = typename facet_create_pack_tail<TRemArray>::type;

                std::shared_ptr<head> cur = m_ref.get<head>();
                if (cur)
                    return get_helper<TF, tail>(std::forward<TProcessed>(params)..., std::move(cur));
                else
                    return nullptr;
            }
            else
            {
                return std::make_shared<TF>(std::forward<TProcessed>(params)...);
            }
        }

        const locale& m_ref;
    };

    template <typename... T>
    struct ft_wrapper<facet_create_rule<T...>>
    {
        ft_wrapper(const locale& l)
            : m_ref(l) {}

        bool has() const
        {
            return has_helper<T...>();
        }

        template<typename TF>
        std::shared_ptr<TF> get()
        {
            return get_helper<TF, T...>();
        }

    private:
        template <typename... TRem>
            requires (sizeof...(TRem) == 0)
        bool has_helper() const
        {
            return false;
        }

        template <typename TC, typename... TRem>
        bool has_helper() const
        {
            if constexpr(is_nonempty_facet_create_pack<TC>)
            {
                auto checked = ft_wrapper<TC>(m_ref);
                if (checked.has()) return true;
                else return has_helper<TRem...>();
            }
            else
            {
                if (m_ref.has<TC>()) return true;
                else return has_helper<TRem...>();
            }
        }


        template <typename TF>
        std::shared_ptr<TF> get_helper()
        {
            return nullptr;
        }

        template <typename TF, typename TC, typename... TSub>
        std::shared_ptr<TF> get_helper()
        {
            if constexpr(is_nonempty_facet_create_pack<TC>)
            {
                auto creator = ft_wrapper<TC>(m_ref);

                std::shared_ptr<TF> obj = creator.template get<TF>();
                if (obj) return obj;
                return get_helper<TF, TSub...>();
            }
            else
            {

                std::shared_ptr<TC> obj = m_ref.get<TC>();
                if (obj)
                    return std::make_shared<TF>(std::move(obj));
                else
                    return get_helper<TF, TSub...>();
            }
        }

        const locale& m_ref;
    };

public:
    locale()
    {
        init<ctype_conf>(safe_setlocale(LC_CTYPE));
        init<collate_conf>(safe_setlocale(LC_COLLATE));
        init<monetary_conf>(safe_setlocale(LC_MONETARY));
        init<numeric_conf>(safe_setlocale(LC_NUMERIC));
        init<timeio_conf>(safe_setlocale(LC_TIME));
    }

    explicit locale(const std::string& name)
    {
        init<ctype_conf>(name);
        init<collate_conf>(name);
        init<monetary_conf>(name);
        init<numeric_conf>(name);
        init<timeio_conf>(name);
    }

    locale(const locale& val)
    {
        std::shared_lock g(val.m_facet_mutex);
        m_facet_confs = val.m_facet_confs;
        m_facets = val.m_facets;
    }

    locale(locale&& val)
    {
        std::lock_guard g(val.m_facet_mutex);
        m_facet_confs = std::move(val.m_facet_confs);
        m_facets = std::move(val.m_facets);
    }

    // Assignment is the only mutating operation on a locale instance. Per the
    // class threading contract (see the class doc), the caller must ensure no
    // other thread accesses *this same instance* (read or write) during an
    // assignment. The locks below keep the source consistent and the maps free
    // of torn reads, but they do NOT make a concurrent get<TF>() on the
    // assigned-into object atomic against the assignment.
    locale& operator=(const locale& val)
    {
        if (this == &val) return *this;
        std::scoped_lock g(val.m_facet_mutex, m_facet_mutex);
        m_facet_confs = val.m_facet_confs;
        m_facets = val.m_facets;
        return *this;
    }

    locale& operator=(locale&& val)
    {
        if (this == &val) return *this;
        std::scoped_lock g(val.m_facet_mutex, m_facet_mutex);
        m_facet_confs = std::move(val.m_facet_confs);
        m_facets = std::move(val.m_facets);
        return *this;
    }

    locale involve(std::shared_ptr<abs_ft> ft) const
    {
        if (!ft) throw std::runtime_error("cannot add empty facet pointer into locale.");

        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    locale involve_msg(const std::string& domain, const std::string& lang = "") const requires (!std::is_same_v<TChar, char>)
    {
        // messages_conf<TChar> is only specialized for char8_t, char32_t and
        // UTF-32 wchar_t (plus the char overload above); for char16_t or a
        // non-UTF-32 wchar_t it stays incomplete. Reject those here with a clear
        // message instead of an opaque "incomplete type" error from make_shared
        // below. Safe as a completeness check because messages_conf<TChar>'s
        // completeness is fixed program-wide (a type either has a specialization
        // everywhere or nowhere).
        static_assert(requires { sizeof(messages_conf<TChar>); },
                      "involve_msg: messages translation is unsupported for this character "
                      "type -- no messages_conf<TChar> specialization exists (e.g. char16_t, "
                      "or wchar_t on a non-UTF-32 platform).");
        const std::string filtered_lang = base_ft<messages>::filter_lang(domain, lang);
        auto ft = s_ori_facet_buf.try_get_msg<TChar>(domain, filtered_lang);
        if (!ft)
        {
            ft = std::make_shared<messages_conf<TChar>>(domain, filtered_lang, false);
            ft = s_ori_facet_buf.put_msg<TChar>(ft, domain, filtered_lang);
        }

        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    locale involve_msg(const std::string& domain, const std::string& lang = "",
                       const std::string& cvt = safe_setlocale(LC_CTYPE)) const requires (std::is_same_v<TChar, char>)
    {
        const std::string filtered_lang = base_ft<messages>::filter_lang(domain, lang);
        auto ft = s_ori_facet_buf.try_get_msg<char>(domain, filtered_lang, cvt);
        if (!ft)
        {
            ft = std::make_shared<messages_conf<char>>(domain, filtered_lang, cvt, false);
            ft = s_ori_facet_buf.put_msg<char>(ft, domain, filtered_lang, cvt);
        }

        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    template <std::derived_from<abs_ft> TF>
    locale remove() const
    {
        locale res(*this);
        if (res.has<TF>())
        {
            res.m_facet_confs.erase(TF::id());
            res.m_facets.clear();
        }
        return res;
    }

    template <std::derived_from<abs_ft> TF>
    bool has() const
    {
        std::shared_lock g(m_facet_mutex);
        std::size_t id = TF::id();
        auto it = m_facet_confs.find(id);
        if (it == m_facet_confs.end()) return false;

        return std::dynamic_pointer_cast<TF>(it->second) != nullptr;
    }

    template <typename TF>
        requires (is_nonempty_facet_create_rule<typename TF::create_rules>)
    bool has() const
    {
        {
            std::shared_lock g(m_facet_mutex);
            if (auto it = m_facets.find(type_id_v<TF>); it != m_facets.end())
                return true;
        }

        ft_wrapper<typename TF::create_rules> obj(*this);
        return obj.has();
    }

    template <std::derived_from<abs_ft> TF>
    std::shared_ptr<TF> get() const
    {
        std::shared_lock g(m_facet_mutex);
        std::size_t id = TF::id();
        auto it = m_facet_confs.find(id);
        if (it == m_facet_confs.end()) return nullptr;

        return std::dynamic_pointer_cast<TF>(it->second);
    }

    template <typename TF>
        requires (is_nonempty_facet_create_rule<typename TF::create_rules>)
    std::shared_ptr<TF> get() const
    {
        {
            std::shared_lock g(m_facet_mutex);
            if (auto it = m_facets.find(type_id_v<TF>); it != m_facets.end())
                return std::static_pointer_cast<TF>(it->second);
        }

        ft_wrapper<typename TF::create_rules> obj(*this);
        auto res = obj.template get<TF>();
        if (res)
        {
            std::lock_guard g(m_facet_mutex);
            auto [it, inserted] = m_facets.insert({type_id_v<TF>, res});
            if (!inserted)
                return std::static_pointer_cast<TF>(it->second);
        }
        return res;
    }

private:
    /**
     * @lang{ZH}
     * 查询当前全局 C locale 的名称；不可用时回退为 "C"。
     *
     * @warning 本函数通过 `std::setlocale(category, nullptr)` 读取**进程级全局**
     * locale：返回的指针指向 C 库的共享缓冲区，随后被拷入 `std::string`。该读取
     * **假设没有其他线程并发以非空实参调用 `std::setlocale` 来修改全局 locale**——
     * 否则按 C 标准（`setlocale` 无需线程安全），该缓冲区可能在拷贝期间被改写或失效，
     * 从而构成数据竞争（UB）。注意：纯查询读取彼此之间并不冲突，只有"读 vs 并发写"
     * 才有竞态；因此在"启动期单线程配置一次 locale、之后不再修改"的常规模型下不存在
     * 该竞态。这一假设与 messages 对 `getenv` 所做的假设同源；库层面无法对此加以同步，
     * 因为外部的 `setlocale` 写者不会经过本库的任何锁。调用方应在程序启动期以单线程
     * 方式一次性配置 locale。
     *
     * 另外注意：`involve_msg(char 重载)` 的默认实参 `cvt = safe_setlocale(LC_CTYPE)`
     * 以及默认构造函数对各 `LC_*` 类别的调用，都会隐式触发此全局读取。
     * @endif
     *
     * @lang{EN}
     * Query the current global C locale name; fall back to "C" if unavailable.
     *
     * @warning This reads the *process-wide global* locale via
     * `std::setlocale(category, nullptr)`: the returned pointer aliases a shared
     * C-library buffer that is then copied into a `std::string`. The read *assumes
     * no other thread concurrently calls `std::setlocale` with a non-null argument*
     * to mutate the global locale -- otherwise, since `setlocale` need not be
     * thread-safe, the buffer may be rewritten/invalidated mid-copy, a data race
     * (UB). Note that pure query reads do not conflict with each other; only a
     * read racing a concurrent write is a problem, so under the usual model
     * (configure the locale once, single-threaded, at startup, then never modify
     * it) no such race exists. This assumption is the same one messages makes for
     * `getenv`; it cannot be synchronized at the library level because external
     * `setlocale` writers do not go through any lock this library controls.
     * Callers should configure the locale once, single-threaded, at startup.
     *
     * Also note that `involve_msg` (char overload)'s default argument
     * `cvt = safe_setlocale(LC_CTYPE)`, and the default constructor's calls for the
     * various `LC_*` categories, trigger this global read implicitly.
     * @endif
     */
    static std::string safe_setlocale(int category)
    {
        const char* p = std::setlocale(category, nullptr);
        return p ? p : "C";
    }

    template <template <typename> class T>
    void init(const std::string& ft_name)
    {
        std::size_t k = T<TChar>::id();
        auto v = s_ori_facet_buf.try_get<T<TChar>>(ft_name);
        m_facet_confs.insert({k, v});
    }

private:
    std::unordered_map<std::size_t, std::shared_ptr<abs_ft>> m_facet_confs;
    mutable std::unordered_map<std::size_t, std::shared_ptr<void>> m_facets;
    mutable std::shared_mutex m_facet_mutex;
};
}
