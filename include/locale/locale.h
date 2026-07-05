/**
 * @file locale.h
 * @lang{ZH}
 * 定义了 `locale` 类模板——管理一组 facet 的本地化对象。
 * `locale` 是值语义、写时拷贝类型，以各 `LC_*` 类别的基础 facet conf 为权威状态，
 * 并按需惰性构建复合 facet（如 `numeric`、`timeio`）。它还负责消息翻译
 * （`involve_msg`），以及查询程序启动时由环境解析得到的初始 locale 名称。
 * @endif
 *
 * @lang{EN}
 * Defines the `locale` class template -- a localization object that manages a set
 * of facets. `locale` is a value-semantic, copy-on-write type whose authoritative
 * state is the per-`LC_*` base facet confs; it lazily builds composite facets
 * (e.g. `numeric`, `timeio`) on demand. It also handles message translation
 * (`involve_msg`) and lookup of the initial locale names resolved from the
 * environment at program startup.
 * @endif
 */
#pragma once
#include <common/defs.h>
#include <common/metafunctions.h>
#include <facet/collate.h>
#include <facet/ctype.h>
#include <facet/facet_common.h>
#include <facet/messages.h>
#include <facet/monetary.h>
#include <facet/numeric.h>
#include <facet/timeio.h>
#include <locale/ori_facet_buf.h>

#include <clocale>
#include <concepts>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace IOv2
{
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
 * 的旧状态"构建而来，导致缓存与当前 conf 不一致。注意对加锁的拷贝赋值而言，这
 * **不是**内存安全 / UB 问题（所有 `shared_ptr` 访问都在锁内，不存在撕裂读或
 * 悬空），仅是逻辑状态不一致；它必须靠上述契约（赋值期间独占访问）来避免，而非
 * 靠在 `get` 内加更细的
 * 锁——复合操作的原子性无法由内部 per-operation 加锁提供。
 *
 * @note `involve` / `involve_msg` / `remove` 均为 const：它们基于 `*this` 的一份
 * 一致快照构造并返回**新的** `locale`，从不原地修改 `*this`（写时拷贝）。因此对一个
 * **已存在**的 `locale` 实例而言，会改变其状态的操作只有两类：赋值（改变被赋值的实例），
 * 以及移动构造 / 移动赋值（**额外会清空被移动的源对象**）。上面按实例独占的同步契约适用于
 * 每一个被如此改变的实例——**包括移动的源对象**；拷贝构造是 const，不在此列。
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
 * inconsistent with the current confs. For the locking copy-assignment this is
 * *not* a memory-safety / UB problem (every `shared_ptr` access is locked -- no
 * torn reads, no dangling), only a logical-consistency one; it must be avoided
 * via the contract above (exclusive access during assignment) rather than by
 * finer-grained locking
 * inside `get` -- compound-operation atomicity cannot be provided by internal
 * per-operation locks.
 *
 * @note `involve` / `involve_msg` / `remove` are const: they build and return a
 * *new* `locale` from a single consistent snapshot of `*this` and never mutate
 * `*this` in place (copy-on-write). For an *existing* `locale` instance, then, the
 * only state-changing operations are assignment (which mutates the assigned-to
 * instance) and move construction / move assignment (which *additionally empties the
 * moved-from source*). The per-instance synchronization contract above applies to
 * every instance so mutated -- including the moved-from source; copy construction, by
 * contrast, is const and is not among them.
 * @endif
 *
 * @tparam TChar
 * @lang{ZH} 该 locale 各 facet 所用的字符类型。 @endif
 * @lang{EN} The character type used by this locale's facets. @endif
 */
template <typename TChar>
class locale
{
    /**
     * @lang{ZH}
     * @brief 复合 facet 的按规则构建器（内部辅助）。
     *
     * 依据某复合 facet 类型的 `create_rules`（`facet_create_pack` /
     * `facet_create_rule`），从一个 `locale` 中递归地检测（`has`）或构建（`get`）
     * 所需的依赖 facet。主模板仅作声明，具体逻辑见对 `facet_create_pack` 与
     * `facet_create_rule` 的特化。
     * @tparam T 描述构建规则的类型（`facet_create_pack<...>` 或
     *         `facet_create_rule<...>`）。
     * @endif
     *
     * @lang{EN}
     * @brief Rule-driven builder for composite facets (internal helper).
     *
     * Given a composite facet type's `create_rules` (`facet_create_pack` /
     * `facet_create_rule`), it recursively checks (`has`) or builds (`get`) the
     * required dependency facets out of a `locale`. The primary template is only
     * declared; the actual logic lives in the specializations for
     * `facet_create_pack` and `facet_create_rule`.
     * @tparam T The type describing the build rule (`facet_create_pack<...>` or
     *         `facet_create_rule<...>`).
     * @endif
     */
    template <typename T>
    struct ft_wrapper;

    /**
     * @lang{ZH}
     * @brief `ft_wrapper` 对 `facet_create_pack` 的特化：**全部齐备**（and-of）规则。
     *
     * `has()` 要求包中每一个依赖 facet 均存在（逻辑与）；`get<TF>()` 依次取得每个
     * 依赖 facet，任一缺失即返回 `nullptr`，全部就绪时用它们构造 `TF`。
     * @endif
     *
     * @lang{EN}
     * @brief `ft_wrapper` specialization for `facet_create_pack`: an "all-of" rule.
     *
     * `has()` requires every dependency facet in the pack to be present (logical
     * AND); `get<TF>()` fetches each dependency in turn, returning `nullptr` if any
     * is missing, and constructs `TF` from them once all are available.
     * @endif
     */
    template <typename... T>
    struct ft_wrapper<facet_create_pack<T...>>
    {
        explicit ft_wrapper(const locale& l)
            : m_ref(l) {}

        [[nodiscard]] bool has() const
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

        const locale& m_ref; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    };

    /**
     * @lang{ZH}
     * @brief `ft_wrapper` 对 `facet_create_rule` 的特化：**多选一**（any-of）候选规则。
     *
     * 规则由若干候选按序尝试组成；每个候选可以是单个依赖 facet，也可以是嵌套的
     * `facet_create_pack`。`has()` 只要有一个候选成立即为真（逻辑或）；`get<TF>()`
     * 返回第一个能成功构建出的候选结果，全部失败则返回 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * @brief `ft_wrapper` specialization for `facet_create_rule`: an "any-of" rule.
     *
     * The rule is a list of candidates tried in order; a candidate may be a single
     * dependency facet or a nested `facet_create_pack`. `has()` is true if any one
     * candidate holds (logical OR); `get<TF>()` returns the first candidate that can
     * be built successfully, or `nullptr` if all fail.
     * @endif
     */
    template <typename... T>
    struct ft_wrapper<facet_create_rule<T...>>
    {
        explicit ft_wrapper(const locale& l)
            : m_ref(l) {}

        [[nodiscard]] bool has() const
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
        [[nodiscard]] bool has_helper() const
        {
            return false;
        }

        template <typename TC, typename... TRem>
        [[nodiscard]] bool has_helper() const
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

        const locale& m_ref; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    };

public:
    /**
     * @lang{ZH}
     * @brief 构造 locale；`name` 为空串（默认）表示使用**程序启动时由环境变量解析得到**
     * 的各 `LC_*` locale，否则其全部 facet 均使用调用方显式给定的 `name`。
     *
     * - **`name` 为空串 `""`（含默认实参）**：等价于"环境 / 默认 locale"。每个 facet 的
     *   locale 名称逐类别取自 `ori_facet_buf::locale_name(LC_*)`；这些名称在单例构造期已由
     *   `resolve_locale` 校验过（无法实例化的名称已回退为 `"C"`）。因此这种情形**不会**因
     *   "locale 名称非法"而抛异常——其名称来源始终可用。注意：空串是表示"环境 / 默认"的
     *   **显式哨兵**，**不会**被原样传给 `newlocale`。
     * - **`name` 非空**：全部 facet 均使用该名称，且**不**做"回退到 `"C"`"处理。既然调用方
     *   显式要求某个特定 locale，则当该名称无法被 C 库实例化时（如拼写错误、宿主未安装该
     *   locale），应当**显式失败**而非静默降级。校验在底层 facet 构造中完成
     *   （`clocale_wrapper` → `newlocale`）；首个失败的 facet 即抛出异常，其余 facet 不再
     *   构造，半构造的 `locale` 随构造函数栈展开而销毁（不泄漏、不留下污染缓存）。
     *
     * @param name 一个在宿主上可被实例化的 locale 名称（语义同 `newlocale`）；为空串
     *        （默认）时表示使用环境 / 默认 locale。
     * @throws cvt_error 当**非空** `name` 无法被 C 库实例化时抛出（`cvt_error` 派生自
     *         `io_error`，并最终派生自 `std::runtime_error`）。空 `name` 不会因此抛出。
     * @endif
     *
     * @lang{EN}
     * @brief Construct a locale; an empty `name` (the default) selects the per-`LC_*`
     * locales resolved from the environment at program startup, otherwise every facet
     * uses the caller-supplied `name`.
     *
     * - **`name` is the empty string `""` (including the default argument)**: equivalent
     *   to the "environment / default locale". Each facet's locale name is taken
     *   per-category from `ori_facet_buf::locale_name(LC_*)`; those names were already
     *   validated by `resolve_locale` at singleton construction (a name that could not
     *   be instantiated has already fallen back to `"C"`). This case therefore never
     *   throws because of an *invalid locale name* -- its name source is always usable.
     *   Note the empty string is an explicit sentinel for "environment / default"; it is
     *   never passed through to `newlocale`.
     * - **`name` is non-empty**: every facet uses that name and does *not* fall back to
     *   `"C"`. Since the caller explicitly asked for a specific locale, an unknown or
     *   uninstantiable name (a typo, or a locale not installed on the host) fails loudly
     *   rather than silently degrading. Validation happens in the underlying facet
     *   construction (`clocale_wrapper` -> `newlocale`); the first failing facet throws,
     *   the remaining facets are not constructed, and the partially-built `locale` is
     *   destroyed as the constructor unwinds (no leak, no poisoned cache entry).
     *
     * @param name A locale name instantiable on the host (as by `newlocale`); empty (the
     *        default) selects the environment / default locale.
     * @throws cvt_error if a *non-empty* `name` cannot be instantiated by the C library
     *         (`cvt_error` derives from `io_error`, and ultimately from
     *         `std::runtime_error`). An empty `name` never throws for this reason.
     * @endif
     */
    explicit locale(const std::string& name = "")
    {
        if (name.empty())
        {
            init<ctype_conf>(s_ori_facet_buf.locale_name(LC_CTYPE));
            init<collate_conf>(s_ori_facet_buf.locale_name(LC_COLLATE));
            init<monetary_conf>(s_ori_facet_buf.locale_name(LC_MONETARY));
            init<numeric_conf>(s_ori_facet_buf.locale_name(LC_NUMERIC));
            init<timeio_conf>(s_ori_facet_buf.locale_name(LC_TIME));
        }
        else
        {
            init<ctype_conf>(name);
            init<collate_conf>(name);
            init<monetary_conf>(name);
            init<numeric_conf>(name);
            init<timeio_conf>(name);
        }
    }

    /**
     * @lang{ZH}
     * @brief 拷贝构造：在共享锁下对源实例做一致快照。
     *
     * 在 `val` 的共享锁保护下拷贝其 facet conf 表与派生 facet 缓存，因此可与源实例
     * 上的并发只读操作安全共存（见类级线程契约）。
     * @param val 被拷贝的源 locale。
     * @endif
     *
     * @lang{EN}
     * @brief Copy constructor: takes a consistent snapshot of the source under a shared lock.
     *
     * Copies the source's facet-conf map and derived-facet cache while holding
     * `val`'s shared lock, so it coexists safely with concurrent read-only
     * operations on the source (see the class-level threading contract).
     * @param val The source locale to copy from.
     * @endif
     */
    locale(const locale& val)
    {
        std::shared_lock g(val.m_facet_mutex);
        m_facet_confs = val.m_facet_confs; // NOLINT(cppcoreguidelines-prefer-member-initializer)
        m_facets = val.m_facets; // NOLINT(cppcoreguidelines-prefer-member-initializer)
    }

    /**
     * @lang{ZH}
     * @brief 移动构造：接管源的内部状态并清空源。
     *
     * 移动后源对象处于有效但已清空的状态。按类级契约，移动会改变源实例，故调用方须
     * 保证移动期间无其它线程访问该源实例。
     * @param val 被移动的源 locale（移动后被清空）。
     * @endif
     *
     * @lang{EN}
     * @brief Move constructor: steals the source's internal state and leaves it empty.
     *
     * After the move the source is in a valid, emptied state. Per the class contract
     * a move mutates the source instance, so the caller must ensure no other thread
     * accesses that source during the move.
     * @param val The source locale to move from (emptied afterwards).
     * @endif
     */
    locale(locale&& val) noexcept
        : m_facet_confs(std::move(val.m_facet_confs)),
          m_facets(std::move(val.m_facets))
    {
    }

    /**
     * @lang{ZH}
     * @brief 拷贝赋值。赋值是 locale 实例上唯一的原地修改操作。
     *
     * 采用两阶段加锁：先在共享锁下对源做快照（不阻塞源上的并发只读读者），再在独占锁
     * 下发布进 `*this`。两把锁从不同时持有，故按构造即无死锁。加锁保证源的一致性、并
     * 使各 map 不出现撕裂读。
     *
     * @warning 按类级线程契约（见类文档），赋值期间调用方必须保证没有其它线程访问
     * **同一个** `*this` 实例（无论读写）。上述加锁**并不能**使被赋值对象上并发的
     * `get<TF>()` 相对本次赋值保持原子。
     * @param val 源 locale。
     * @return 对 `*this` 的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Copy assignment. Assignment is the only in-place mutating operation on a locale instance.
     *
     * Uses two-phase locking: first snapshot the source under a shared lock
     * (concurrent const readers of the source are not blocked), then publish into
     * `*this` under an exclusive lock. The two locks are never held at once, so this
     * is deadlock-free by construction. The locks keep the source consistent and the
     * maps free of torn reads.
     *
     * @warning Per the class threading contract (see the class doc), the caller must
     * ensure no other thread accesses *this same instance* (read or write) during an
     * assignment. The locking does *not* make a concurrent `get<TF>()` on the
     * assigned-into object atomic with respect to the assignment.
     * @param val The source locale.
     * @return A reference to `*this`.
     * @endif
     */
    locale& operator=(const locale& val)
    {
        if (this == &val) return *this;
        decltype(m_facet_confs) confs;
        decltype(m_facets)      facets;
        {
            std::shared_lock src(val.m_facet_mutex);
            confs  = val.m_facet_confs;
            facets = val.m_facets;
        }
        {
            std::unique_lock dst(m_facet_mutex);
            m_facet_confs = std::move(confs);
            m_facets      = std::move(facets);
        }
        return *this;
    }

    /**
     * @lang{ZH}
     * @brief 移动赋值：接管源状态并清空源。
     *
     * 与移动构造一样，本操作同时改变 `*this` 与源实例；按类级契约，二者在此期间都须
     * 独占访问。含自赋值保护。
     * @param val 源 locale（移动后被清空）。
     * @return 对 `*this` 的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Move assignment: steals the source's state and empties it.
     *
     * Like move construction, this mutates both `*this` and the source instance; per
     * the class contract both require exclusive access for the duration.
     * Self-assignment safe.
     * @param val The source locale (emptied afterwards).
     * @return A reference to `*this`.
     * @endif
     */
    locale& operator=(locale&& val) noexcept
    {
        if (this == &val) return *this;
        m_facet_confs = std::move(val.m_facet_confs);
        m_facets = std::move(val.m_facets);
        return *this;
    }

    ~locale() = default;

    /**
     * @lang{ZH}
     * @brief 返回一个在 `*this` 基础上绑定（或替换）给定 facet 的**新** locale
     * （写时拷贝；`*this` 不变）。
     *
     * 新 locale 以 `ft` 的类型 id 为键写入 facet conf 表：若 `*this` 已有同 id 的
     * facet，则在返回的副本中将其**替换**，否则新增。由于底层 conf 发生变化，返回副本
     * 的派生 facet 缓存会被清空，以便后续按需重建。`*this` 本身不被修改。
     *
     * @param ft 要绑定的 facet（非空）。
     * @throws stream_error 当 `ft` 为空指针时抛出。
     * @return 绑定了该 facet 的新 `locale`。
     * @endif
     *
     * @lang{EN}
     * @brief Return a *new* locale that binds (or replaces) the given facet on top of
     * `*this` (copy-on-write; `*this` is unchanged).
     *
     * The new locale writes `ft` into the facet-conf map keyed by its type id: if
     * `*this` already has a facet with the same id, the returned copy **replaces** it,
     * otherwise it is added. Because the underlying confs change, the returned copy's
     * derived-facet cache is cleared so it is rebuilt on demand. `*this` itself is
     * unchanged.
     *
     * @param ft The facet to bind (must be non-null).
     * @throws stream_error if `ft` is a null pointer.
     * @return A new `locale` carrying that facet.
     * @endif
     */
    locale involve(std::shared_ptr<abs_ft> ft) const
    {
        if (!ft) throw stream_error("cannot add empty facet pointer into locale.");

        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 返回一个在 `*this` 基础上、额外绑定了指定文本域消息翻译的**新** locale
     * （写时拷贝；`*this` 不变）。
     *
     * 一个 `locale` 实例**最多只能持有一个** messages facet（`m_facet_confs` 以单一的
     * `messages_conf<TChar>` 类型 id 为键，与 domain 无关）。因此若 `*this` 已含 messages
     * facet，返回的新 locale 会以本次绑定的 facet **替换**掉它——即便 domain 相同亦然；
     * 这与 `involve` 对同 id facet 的替换语义一致。`*this` 本身不被修改（写时拷贝）。
     *
     * 对不支持的字符类型（无 `messages_conf<TChar>` 特化，如 `char16_t` 或非 UTF-32 的
     * `wchar_t`），本函数是**编译期**错误（见内部 `static_assert`）。
     *
     * @param domain 文本域名称（`.mo` 文件基名）。
     * @param lang 候选语言字符串；为空（默认）表示按环境变量决定。
     * @param throw_if_fail 翻译字典**加载失败**（`filter_lang` 选中的 `.mo` 文件存在但损坏 /
     *        截断，或无可用语言）时是否抛出。为 `false`（默认）静默降级为空（穿透）facet；为
     *        `true` 抛 `stream_error`。**降级 facet 绝不写入缓存**，因此该参数不受此前调用影响：
     *        无论之前是否有 `false` 调用发生过降级，后续 `true` 调用都会重新尝试加载并如实抛出。
     * @throws stream_error 当 `throw_if_fail` 为 `true` 且翻译字典加载失败时抛出；失败结果不会
     *         被写入缓存。
     * @throws std::filesystem::filesystem_error 解析翻译文件可用性（`filter_lang` →
     *         `available` → `std::filesystem::exists`）时，若 domain / 派生路径触发
     *         "文件不存在"以外的文件系统错误（如 ENAMETOOLONG、ELOOP、EACCES、ENOTDIR）
     *         则抛出。此情形发生在任何 `throw_if_fail` 保护之外，与之无关。
     * @throws std::bad_alloc 构造 messages facet 期间内存分配失败时抛出。
     * @return 绑定了该 messages facet 的新 `locale`。
     * @endif
     *
     * @lang{EN}
     * @brief Return a *new* locale that, on top of `*this`, additionally binds the
     * message translations for the given text domain (copy-on-write; `*this` is
     * unchanged).
     *
     * A `locale` instance holds **at most one** messages facet (`m_facet_confs` keys it
     * by the single `messages_conf<TChar>` type id, independent of domain). If `*this`
     * already has a messages facet, the returned locale **replaces** it with the one
     * bound here -- even when the domain is the same -- mirroring `involve`'s same-id
     * replace semantics. `*this` itself is unchanged (copy-on-write).
     *
     * For an unsupported character type (no `messages_conf<TChar>` specialization,
     * e.g. `char16_t` or a non-UTF-32 `wchar_t`) this is a *compile-time* error (see
     * the internal `static_assert`).
     *
     * @param domain The text domain name (the `.mo` file's basename).
     * @param lang The candidate language string; empty (the default) defers to the
     *        environment.
     * @param throw_if_fail Whether to throw when the translation dictionary *fails to
     *        load* (the `.mo` chosen by `filter_lang` exists but is corrupt / truncated,
     *        or no language is available). `false` (the default) silently degrades to an
     *        empty (passthrough) facet; `true` throws `stream_error`. A degraded facet is
     *        *never* cached, so this flag is independent of earlier calls: regardless of
     *        any prior `false` call that degraded, a later `true` call re-attempts the
     *        load and throws as expected.
     * @throws stream_error if `throw_if_fail` is `true` and the translation dictionary
     *         fails to load; the failed result is not cached.
     * @throws std::filesystem::filesystem_error if resolving translation-file
     *         availability (`filter_lang` -> `available` -> `std::filesystem::exists`)
     *         hits a filesystem error other than "not found" (e.g. ENAMETOOLONG,
     *         ELOOP, EACCES, ENOTDIR) for the domain / derived path. This happens
     *         outside any `throw_if_fail` guard and is independent of it.
     * @throws std::bad_alloc on allocation failure while building the messages facet.
     * @return A new `locale` carrying that messages facet.
     * @endif
     */
    locale involve_msg(const std::string& domain, const std::string& lang = "",
                       bool throw_if_fail = false) const requires (!std::is_same_v<TChar, char>)
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
        const std::string dirname = base_ft<messages>::get_dirname(domain);
        auto ft = s_ori_facet_buf.try_get_msg<TChar>(domain, filtered_lang, dirname);
        if (!ft)
        {
            // Build strictly so a load failure surfaces as an exception, and intern only
            // on success: a degraded (load-failed) facet is never cached, so a later
            // throw_if_fail=true call re-attempts the load and can still throw.
            try
            {
                ft = std::make_shared<messages_conf<TChar>>(domain, filtered_lang, dirname, true);
                // Caching is separate from the load: a cache-insert failure (e.g. an
                // allocation failure while interning) must never degrade or drop a
                // successfully-loaded facet. On such a failure keep it, just uncached.
                try { ft = s_ori_facet_buf.put_msg<TChar>(ft, domain, filtered_lang, dirname); }
                catch (...) {} // NOLINT(bugprone-empty-catch)
            }
            catch (...)
            {
                if (throw_if_fail) throw;
                ft = std::make_shared<messages_conf<TChar>>(domain, filtered_lang, dirname, false);
            }
        }

        // At most one messages facet per locale (keyed by the single messages_conf
        // type id). If *this already carries one, this replaces it in the returned
        // copy -- matching involve()'s same-id replace semantics; *this is unchanged.
        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    /**
     * @lang{ZH}
     * @brief `char` 版本：返回一个在 `*this` 基础上绑定了指定文本域消息翻译的**新** locale
     * （写时拷贝）；额外接受目标编码名 `cvt`。
     *
     * 同非 `char` 重载：一个 `locale` **最多一个** messages facet；若 `*this` 已含 messages
     * facet，返回的新 locale 会以本次绑定的 facet **替换**之（同 domain 亦然），与 `involve`
     * 的同 id 替换语义一致；`*this` 本身不被修改（写时拷贝）。
     *
     * @param domain 文本域名称（`.mo` 文件基名）。
     * @param lang 候选语言字符串；为空（默认）表示按环境变量决定。
     * @param cvt 目标 `char` 编码名；为空（默认）时取程序启动时解析得到的 `LC_CTYPE` 编码，
     *        并以该有效值同时作为缓存键与构造参数。
     * @param throw_if_fail 翻译字典**加载失败**（`filter_lang` 选中的 `.mo` 文件存在但损坏 /
     *        截断，无可用语言，或 `cvt` 编码转换器无法构造 / 转换）时是否抛出。为 `false`（默认）
     *        静默降级为空（穿透）facet；为 `true` 抛 `stream_error`。**降级 facet 绝不写入缓存**，
     *        因此该参数不受此前调用影响：无论之前是否有 `false` 调用发生过降级，后续 `true` 调用
     *        都会重新尝试加载并如实抛出。
     * @throws stream_error 当 `throw_if_fail` 为 `true` 且翻译字典加载失败时抛出；失败结果不会
     *         被写入缓存。
     * @throws std::filesystem::filesystem_error 解析翻译文件可用性（`filter_lang` →
     *         `available` → `std::filesystem::exists`）时，若 domain / 派生路径触发
     *         "文件不存在"以外的文件系统错误（如 ENAMETOOLONG、ELOOP、EACCES、ENOTDIR）
     *         则抛出。此情形发生在任何 `throw_if_fail` 保护之外，与之无关。
     * @throws std::bad_alloc 构造 messages facet 期间内存分配失败时抛出。
     * @return 绑定了该 messages facet 的新 `locale`。
     * @endif
     *
     * @lang{EN}
     * @brief `char` overload: return a *new* locale that, on top of `*this`, binds the
     * message translations for the given text domain (copy-on-write); additionally
     * takes a target encoding name `cvt`.
     *
     * As with the non-`char` overload, a `locale` holds **at most one** messages facet;
     * if `*this` already has one, the returned locale **replaces** it with the one bound
     * here -- even when the domain is the same -- mirroring `involve`'s same-id replace
     * semantics. `*this` itself is unchanged (copy-on-write).
     *
     * @param domain The text domain name (the `.mo` file's basename).
     * @param lang The candidate language string; empty (the default) defers to the
     *        environment.
     * @param cvt The target `char` encoding name; empty (the default) maps to the
     *        `LC_CTYPE` encoding resolved at program startup, and that effective value
     *        is used as both the cache key and the construction argument.
     * @param throw_if_fail Whether to throw when the translation dictionary *fails to
     *        load* (the `.mo` chosen by `filter_lang` exists but is corrupt / truncated,
     *        no language is available, or the `cvt` encoding converter cannot be
     *        constructed / applied). `false` (the default) silently degrades to an empty
     *        (passthrough) facet; `true` throws `stream_error`. A degraded facet is
     *        *never* cached, so this flag is independent of earlier calls: regardless of
     *        any prior `false` call that degraded, a later `true` call re-attempts the
     *        load and throws as expected.
     * @throws stream_error if `throw_if_fail` is `true` and the translation dictionary
     *         fails to load; the failed result is not cached.
     * @throws std::filesystem::filesystem_error if resolving translation-file
     *         availability (`filter_lang` -> `available` -> `std::filesystem::exists`)
     *         hits a filesystem error other than "not found" (e.g. ENAMETOOLONG,
     *         ELOOP, EACCES, ENOTDIR) for the domain / derived path. This happens
     *         outside any `throw_if_fail` guard and is independent of it.
     * @throws std::bad_alloc on allocation failure while building the messages facet.
     * @return A new `locale` carrying that messages facet.
     * @endif
     */
    locale involve_msg(const std::string& domain, const std::string& lang = "", // NOLINT(bugprone-easily-swappable-parameters)
                       const std::string& cvt = "", bool throw_if_fail = false) const requires (std::is_same_v<TChar, char>)
    {
        const std::string filtered_lang = base_ft<messages>::filter_lang(domain, lang);
        // Resolve the encoding up front and use it consistently as both the cache
        // key and the construction argument. An empty cvt maps to the (stable,
        // env-derived) CTYPE name; keying by this effective value -- rather than by
        // the raw cvt -- ensures the cached facet is always looked up under the
        // exact encoding it was built with, and lets an explicit cvt equal to the
        // CTYPE name share the same cache entry as the empty-cvt case.
        const std::string effective_cvt = cvt.empty() ? s_ori_facet_buf.locale_name(LC_CTYPE) : cvt;
        const std::string dirname = base_ft<messages>::get_dirname(domain);
        auto ft = s_ori_facet_buf.try_get_msg<char>(domain, filtered_lang, dirname, effective_cvt);
        if (!ft)
        {
            // Build strictly so a load failure surfaces as an exception, and intern only
            // on success: a degraded (load-failed) facet is never cached, so a later
            // throw_if_fail=true call re-attempts the load and can still throw.
            try
            {
                ft = std::make_shared<messages_conf<char>>(domain, filtered_lang, effective_cvt, dirname, true);
                // Caching is separate from the load: a cache-insert failure (e.g. an
                // allocation failure while interning) must never degrade or drop a
                // successfully-loaded facet. On such a failure keep it, just uncached.
                try { ft = s_ori_facet_buf.put_msg<char>(ft, domain, filtered_lang, dirname, effective_cvt); }
                catch (...) {} // NOLINT(bugprone-empty-catch)
            }
            catch (...)
            {
                if (throw_if_fail) throw;
                ft = std::make_shared<messages_conf<char>>(domain, filtered_lang, effective_cvt, dirname, false);
            }
        }

        // At most one messages facet per locale (keyed by the single messages_conf
        // type id). If *this already carries one, this replaces it in the returned
        // copy -- matching involve()'s same-id replace semantics; *this is unchanged.
        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 返回一个移除了指定 facet 类型的**新** locale（写时拷贝；`*this` 不变）。
     *
     * 若 `*this` 含有类型 `TF` 的 facet，则在返回的副本中将其从 conf 表移除并清空派生
     * facet 缓存；若不含该 facet，则返回 `*this` 的等价副本。`*this` 本身不被修改。
     *
     * @tparam TF 要移除的 facet conf 类型（派生自 `abs_ft`）。
     * @return 移除该 facet 后的新 `locale`。
     * @endif
     *
     * @lang{EN}
     * @brief Return a *new* locale with the given facet type removed (copy-on-write;
     * `*this` is unchanged).
     *
     * If `*this` carries a facet of type `TF`, the returned copy erases it from the
     * conf map and clears the derived-facet cache; if not, an equivalent copy of
     * `*this` is returned. `*this` itself is unchanged.
     *
     * @tparam TF The facet conf type to remove (derived from `abs_ft`).
     * @return A new `locale` with that facet removed.
     * @endif
     */
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

    /**
     * @lang{ZH}
     * @brief 查询本 locale 是否直接持有指定的 facet conf 类型 `TF`。
     *
     * 在共享锁下按 `TF::id()` 查 conf 表，并校验存放的指针确实可转换为 `TF`。
     * @tparam TF 要查询的 facet conf 类型（派生自 `abs_ft`）。
     * @return 若持有该 facet 则为 `true`，否则为 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Query whether this locale directly holds the given facet conf type `TF`.
     *
     * Looks up the conf map by `TF::id()` under a shared lock and verifies the stored
     * pointer is indeed convertible to `TF`.
     * @tparam TF The facet conf type to query (derived from `abs_ft`).
     * @return `true` if the facet is present, `false` otherwise.
     * @endif
     */
    template <std::derived_from<abs_ft> TF>
    bool has() const
    {
        std::shared_lock g(m_facet_mutex);
        facet_id_t id = TF::id();
        auto it = m_facet_confs.find(id);
        if (it == m_facet_confs.end()) return false;

        return std::dynamic_pointer_cast<TF>(it->second) != nullptr;
    }

    /**
     * @lang{ZH}
     * @brief 查询本 locale 能否构建出指定的**复合** facet `TF`。
     *
     * 先在共享锁下查派生 facet 缓存；未命中则依据 `TF::create_rules` 用 `ft_wrapper`
     * 递归检测所需依赖是否齐备（不实际构建、不写缓存）。
     *
     * @note 约束中的 `!std::derived_from` 合取项用于排除"既是 conf 又是复合 facet"
     * 这一病态类型，使这类类型只匹配下方的守卫重载（否则会与本重载并列而造成调用
     * 二义）；`std::derived_from` 是概念、可参与包含（subsumption），而在两处分别书写
     * 的裸变量模板 `is_nonempty_...` 形成互不包含的原子约束。
     * @tparam TF 要查询的复合 facet 类型（带非空 `create_rules`，且非 `abs_ft` 派生）。
     * @return 若可构建该复合 facet 则为 `true`，否则为 `false`。
     * @endif
     *
     * @lang{EN}
     * @brief Query whether this locale can build the given *composite* facet `TF`.
     *
     * First checks the derived-facet cache under a shared lock; on a miss it uses
     * `ft_wrapper` to recursively test whether the dependencies required by
     * `TF::create_rules` are all available (without actually building or caching).
     *
     * @note The `!std::derived_from` conjunct excludes the pathological "both a conf
     * and a composite facet" type, so that for such a type only the guard overload
     * below is viable (it would otherwise tie with this one and make the call
     * ambiguous -- `std::derived_from` is a concept and subsumes, but the bare
     * `is_nonempty_...` variable template re-typed in two places forms distinct,
     * non-subsuming atomic constraints).
     * @tparam TF The composite facet type to query (with a non-empty `create_rules`,
     *         and not derived from `abs_ft`).
     * @return `true` if the composite facet can be built, `false` otherwise.
     * @endif
     */
    template <typename TF>
        requires (is_nonempty_facet_create_rule<typename TF::create_rules>
                  && !std::derived_from<TF, abs_ft>)
    bool has() const
    {
        {
            std::shared_lock g(m_facet_mutex);
            if (auto it = m_facets.find(type_id_v<TF>()); it != m_facets.end())
                return true;
        }

        ft_wrapper<typename TF::create_rules> obj(*this);
        return obj.has();
    }

    /**
     * @lang{ZH}
     * @brief 守卫重载：把"既是 facet conf、又是复合 facet"的病态类型转为清晰的编译期
     * 诊断。
     *
     * 某类型若**同时**派生自 `abs_ft` 且带有非空 `create_rules`，会同时满足上面两个
     * `has()` 重载的约束而造成调用二义。按设计这不应发生——conf 派生自 `abs_ft`，
     * facet 携带 `create_rules`，二者是不同的类型。本重载的约束是另外两者约束的合取，
     * 因而包含（subsume）二者、对这类病态类型被无歧义地选中，从而把晦涩的"调用二义"
     * 变为下方精确的 `static_assert`。
     * @tparam TF 病态类型（既派生自 `abs_ft` 又带非空 `create_rules`）。
     * @endif
     *
     * @lang{EN}
     * @brief Guard overload: turns the pathological "both a facet conf and a composite
     * facet" type into a clear compile-time diagnostic.
     *
     * A type that is *both* derived from `abs_ft` and carries a non-empty
     * `create_rules` would satisfy the constraints of both `has()` overloads above and
     * make the call ambiguous. By design that never happens -- confs derive from
     * `abs_ft` while facets carry `create_rules`, and they are distinct types. This
     * overload's constraint is the conjunction of the other two's, so it subsumes both
     * and is selected unambiguously for such a pathological type, turning an opaque
     * "ambiguous call" into the precise `static_assert` below.
     * @tparam TF The pathological type (both derived from `abs_ft` and carrying a
     *         non-empty `create_rules`).
     * @endif
     */
    template <typename TF>
        requires (std::derived_from<TF, abs_ft>
                  && is_nonempty_facet_create_rule<typename TF::create_rules>)
    bool has() const
    {
        static_assert(dependent_false_v<TF>,
                      "has<TF>(): TF is both a facet conf (derived from abs_ft) and a "
                      "composite facet (with a non-empty create_rules). These roles are "
                      "mutually exclusive by design -- confs derive from abs_ft, facets "
                      "carry create_rules -- so such a type is a definition error.");
        return false;
    }

    /**
     * @lang{ZH}
     * @brief 取得本 locale 直接持有的指定 facet conf `TF` 的共享指针。
     *
     * 在共享锁下按 `TF::id()` 查 conf 表；未找到或类型不匹配时返回 `nullptr`。
     * @tparam TF 要获取的 facet conf 类型（派生自 `abs_ft`）。
     * @return 指向该 facet 的 `std::shared_ptr<TF>`；不存在则为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * @brief Get a shared pointer to the given facet conf `TF` directly held by this locale.
     *
     * Looks up the conf map by `TF::id()` under a shared lock; returns `nullptr` if
     * not found or the type does not match.
     * @tparam TF The facet conf type to get (derived from `abs_ft`).
     * @return A `std::shared_ptr<TF>` to the facet, or `nullptr` if absent.
     * @endif
     */
    template <std::derived_from<abs_ft> TF>
    std::shared_ptr<TF> get() const
    {
        std::shared_lock g(m_facet_mutex);
        facet_id_t id = TF::id();
        auto it = m_facet_confs.find(id);
        if (it == m_facet_confs.end()) return nullptr;

        return std::dynamic_pointer_cast<TF>(it->second);
    }

    /**
     * @lang{ZH}
     * @brief 取得（必要时惰性构建并缓存）指定的**复合** facet `TF`。
     *
     * 先在共享锁下查派生 facet 缓存；命中直接返回。未命中则依 `TF::create_rules` 用
     * `ft_wrapper` 构建；构建成功后在独占锁下写入缓存——若期间已被其它线程抢先插入，
     * 则返回已缓存者，以保证同一 `TF` 全程唯一。
     *
     * @note 约束中的 `!std::derived_from` 合取项作用同上方 `has()` 复合重载：排除
     * "既是 conf 又是复合 facet"的病态类型以避免调用二义。
     * @tparam TF 要获取的复合 facet 类型（带非空 `create_rules`，且非 `abs_ft` 派生）。
     * @return 指向该复合 facet 的 `std::shared_ptr<TF>`；无法构建则为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * @brief Get (lazily building and caching if needed) the given *composite* facet `TF`.
     *
     * First checks the derived-facet cache under a shared lock and returns a hit
     * directly. On a miss it builds `TF` via `ft_wrapper` per `TF::create_rules`; on
     * success it inserts into the cache under an exclusive lock -- if another thread
     * inserted first in the meantime, the already-cached instance is returned so that
     * a given `TF` stays unique.
     *
     * @note The `!std::derived_from` conjunct plays the same role as in the `has()`
     * composite overload above: it excludes the "both a conf and a composite facet"
     * pathological type to avoid an ambiguous call.
     * @tparam TF The composite facet type to get (with a non-empty `create_rules`, and
     *         not derived from `abs_ft`).
     * @return A `std::shared_ptr<TF>` to the composite facet, or `nullptr` if it
     *         cannot be built.
     * @endif
     */
    template <typename TF>
        requires (is_nonempty_facet_create_rule<typename TF::create_rules>
                  && !std::derived_from<TF, abs_ft>)
    std::shared_ptr<TF> get() const
    {
        {
            std::shared_lock g(m_facet_mutex);
            if (auto it = m_facets.find(type_id_v<TF>()); it != m_facets.end())
                return std::static_pointer_cast<TF>(it->second);
        }

        ft_wrapper<typename TF::create_rules> obj(*this);
        auto res = obj.template get<TF>();
        if (res)
        {
            std::scoped_lock g(m_facet_mutex);
            auto [it, inserted] = m_facets.insert({type_id_v<TF>(), res});
            if (!inserted)
                return std::static_pointer_cast<TF>(it->second);
        }
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 守卫重载：为"既是 facet conf、又是复合 facet"的病态类型给出精确诊断。
     *
     * 理由同上方 `has()` 的守卫重载：其约束为另外两个 `get()` 重载约束的合取，因而
     * 无歧义地选中这类病态类型，用下方的 `static_assert` 给出明确报错，而非晦涩的
     * "调用二义"。
     * @tparam TF 病态类型（既派生自 `abs_ft` 又带非空 `create_rules`）。
     * @endif
     *
     * @lang{EN}
     * @brief Guard overload: gives a precise diagnostic for the "both a facet conf and
     * a composite facet" pathological type.
     *
     * Same rationale as the `has()` guard overload above: its constraint is the
     * conjunction of the other two `get()` overloads' constraints, so it is selected
     * unambiguously for such a pathological type and reports a clear error via the
     * `static_assert` below instead of an opaque "ambiguous call".
     * @tparam TF The pathological type (both derived from `abs_ft` and carrying a
     *         non-empty `create_rules`).
     * @endif
     */
    template <typename TF>
        requires (std::derived_from<TF, abs_ft>
                  && is_nonempty_facet_create_rule<typename TF::create_rules>)
    std::shared_ptr<TF> get() const
    {
        static_assert(dependent_false_v<TF>,
                      "get<TF>(): TF is both a facet conf (derived from abs_ft) and a "
                      "composite facet (with a non-empty create_rules). These roles are "
                      "mutually exclusive by design -- confs derive from abs_ft, facets "
                      "carry create_rules -- so such a type is a definition error.");
        return nullptr;
    }

    /**
     * @lang{ZH}
     * 返回某个 `LC_*` 类别在**程序启动时由环境变量解析得到**的初始 locale 名称。
     *
     * 这是对 `ori_facet_buf::locale_name` 的薄转发，使得仅依赖 `<locale/locale.h>`
     * 的代码（如 stdio 流对象）无需直接看到 `ori_facet_buf` 即可获取该初始名称。
     * 返回值与字符类型无关；之所以作为 `locale` 的静态成员暴露，是为了让调用方只
     * 看见 `locale`。
     *
     * @throws stream_error 由底层 `ori_facet_buf::locale_name` 透出：当 `category`
     *         不是已解析的五个类别之一（`LC_CTYPE` / `LC_COLLATE` / `LC_MONETARY` /
     *         `LC_NUMERIC` / `LC_TIME`）时抛出，例如传入 `LC_ALL` 或 `LC_MESSAGES`。
     * @endif
     *
     * @lang{EN}
     * Return the *initial* locale name for an `LC_*` category, as resolved from the
     * environment at program startup.
     *
     * A thin forwarder over `ori_facet_buf::locale_name` so that code depending
     * only on `<locale/locale.h>` (e.g. the stdio stream objects) can obtain this
     * initial name without seeing `ori_facet_buf` directly. The result is
     * independent of character type; it is exposed as a static member of `locale`
     * so that callers only need to see `locale`.
     *
     * @throws stream_error propagated from the underlying `ori_facet_buf::locale_name`
     *         if `category` is not one of the five resolved categories (`LC_CTYPE` /
     *         `LC_COLLATE` / `LC_MONETARY` / `LC_NUMERIC` / `LC_TIME`), e.g. when
     *         passed `LC_ALL` or `LC_MESSAGES`.
     * @endif
     */
    static const std::string& initial_locale_name(int category)
    {
        return s_ori_facet_buf.locale_name(category);
    }

private:
    /**
     * @lang{ZH}
     * @brief 用给定名称构建单个 facet conf 并存入 conf 表（构造期辅助）。
     *
     * 经 `ori_facet_buf::try_get` 获取（可能复用缓存的）`T<TChar>` conf，以其类型 id
     * 为键插入 `m_facet_confs`。
     * @tparam T facet conf 的类模板（如 `ctype_conf`），以 `TChar` 实例化。
     * @param ft_name 该 facet 使用的 locale 名称。
     * @endif
     *
     * @lang{EN}
     * @brief Build a single facet conf by name and store it in the conf map (construction helper).
     *
     * Obtains a (possibly cache-shared) `T<TChar>` conf via `ori_facet_buf::try_get`
     * and inserts it into `m_facet_confs` keyed by its type id.
     * @tparam T The facet conf class template (e.g. `ctype_conf`), instantiated with `TChar`.
     * @param ft_name The locale name to use for this facet.
     * @endif
     */
    template <template <typename> class T>
    void init(const std::string& ft_name)
    {
        facet_id_t k = T<TChar>::id();
        auto v = s_ori_facet_buf.try_get<T<TChar>>(ft_name);
        m_facet_confs.insert({k, std::move(v)});
    }

private:
    /**
     * @lang{ZH} facet 配置（conf）表：以 facet 类型 id 为键，保存本 locale 直接持有的
     * 各基础 facet conf。这是 locale 的权威状态，拷贝 / 赋值即拷贝此表。 @endif
     * @lang{EN} The facet-conf map: keyed by facet type id, holding the base facet
     * confs this locale directly owns. This is the locale's authoritative state;
     * copy / assignment copies this map. @endif
     */
    std::unordered_map<facet_id_t, std::shared_ptr<abs_ft>> m_facet_confs;

    /**
     * @lang{ZH} 派生（复合）facet 的惰性缓存：以复合 facet 的类型 id 为键。由 `get`
     * 按需填充，`involve` / `involve_msg` / `remove` 会将其清空。`mutable`，故 const
     * 访问下也可更新。 @endif
     * @lang{EN} Lazy cache of derived (composite) facets, keyed by the composite
     * facet's type id. Populated on demand by `get` and cleared by `involve` /
     * `involve_msg` / `remove`. `mutable` so it can be updated under const access. @endif
     */
    mutable std::unordered_map<facet_id_t, std::shared_ptr<void>> m_facets;

    /**
     * @lang{ZH} 保护 `m_facet_confs` 与 `m_facets` 的读写锁：只读操作取共享锁，发布新
     * 状态（赋值、缓存写入）取独占锁。`mutable`，以便 const 操作也能加锁。 @endif
     * @lang{EN} Reader-writer lock guarding `m_facet_confs` and `m_facets`: read-only
     * operations take a shared lock, publishing new state (assignment, cache
     * insertion) takes an exclusive lock. `mutable` so const operations can lock. @endif
     */
    mutable std::shared_mutex m_facet_mutex;
};
}
