/**
 * @file stream_common_operators.h
 * @lang{ZH}
 * 定义了输入流与输出流共用的公共操作。
 * 包含保护整张 tie 图的进程级全局锁 `tie_graph_mutex()`，以及承载定位（`tell`/`seek`/
 * `rseek`）、底层设备管理（`device`/`detach`/`attach`）、转换行为调整（`adjust`/`retrieve`）、
 * locale 存取与流绑定（`tie`）等操作的 `stream_common_operators` 混入基类。
 * @endif
 *
 * @lang{EN}
 * Defines the common operations shared by input and output streams.
 * Includes the process-wide lock `tie_graph_mutex()` that guards the entire tie graph, and the
 * `stream_common_operators` mix-in base carrying positioning (`tell`/`seek`/`rseek`), underlying
 * device management (`device`/`detach`/`attach`), conversion-behavior adjustment
 * (`adjust`/`retrieve`), locale access, and stream tying (`tie`).
 * @endif
 */
#pragma once

#include <common/copyable_atomic.h>
#include <common/defs.h>
#include <common/iov2_export.h>
#include <cvt/cvt_concepts.h>
#include <io/io_base.h>
#include <io/utilities/ostream_operators.h>
#include <locale/locale.h>

#include <concepts>
#include <cstddef>
#include <exception>
#include <mutex>
#include <optional>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 返回保护整张 tie 图的进程级全局锁。
 *
 * `tie()` setter 会以本锁把“沿目标链检测是否成环”与“提交新的 tie 指针”合成一个原子
 * 步骤。若无此锁，两个线程并发 `A.tie(B)` 与 `B.tie(A)` 可能各自读到对方尚未指回的旧
 * 状态、双双通过检测，从而形成环（详见 `tie()`）。有了本锁，所有 setter 串行化，任一
 * 次检测期间图都被冻结，故并发也无法成环。
 *
 * 本锁是**普通**（非递归）互斥量即可：检测遍历调用的是 tie() 的 getter（一次原子读，
 * 不取本锁），持锁期间不会重入 setter。它与各流的 `io_mutex()` 互不嵌套——setter 只取
 * 本锁、sentry 只取 `io_mutex()`——故不引入新的加锁顺序约束。
 *
 * 采用按需构造的函数内静态量（Meyers 单例）：`tie()` 在静态初始化期间即被调用
 * （如 `__cerr` 构造时 `tie(&cout)`），懒构造保证“首次使用前必已构造”，天然无静态
 * 初始化顺序问题。为在共享库（DSO）模式下仍是**全进程唯一**一份，本函数在 `IOV2_SHARED`
 * 下只声明、定义集中于 `iov2_objects.cpp` 并经 `IOV2_API` 导出；header-only 模式下则为
 * inline 定义。
 * @return 保护整张 tie 图的进程级全局互斥量的引用。
 * @endif
 *
 * @lang{EN}
 * @brief Returns the process-wide lock that guards the entire tie graph.
 *
 * The `tie()` setter uses this lock to fuse "walk the target chain to detect a cycle"
 * and "commit the new tie pointer" into one atomic step. Without it, two threads doing
 * `A.tie(B)` and `B.tie(A)` concurrently could each read the other's not-yet-updated
 * state, both pass the check, and form a cycle (see `tie()`). With it, all setters are
 * serialized and the graph is frozen for the duration of any walk, so no cycle can form
 * even under concurrency.
 *
 * A **plain** (non-recursive) mutex suffices: the detection walk calls tie()'s getter (a
 * single atomic load that does not take this lock), so a setter never re-enters while
 * holding it. This lock never nests with a stream's `io_mutex()` -- the setter takes only
 * this lock, a sentry takes only `io_mutex()` -- so it adds no new lock-ordering
 * constraint.
 *
 * It is a lazily-constructed function-local static (Meyers singleton): `tie()` runs during
 * static initialization (e.g. `__cerr`'s ctor does `tie(&cout)`), and lazy construction
 * guarantees "constructed before first use", so there is no static-init-order problem. To
 * stay a single process-wide instance under shared-library (DSO) mode, this function is
 * only declared under `IOV2_SHARED` with its one definition living in `iov2_objects.cpp`
 * and exported via `IOV2_API`; in header-only mode it is defined inline here.
 * @return A reference to the process-wide mutex that guards the entire tie graph.
 * @endif
 */
#if defined(IOV2_SHARED)
IOV2_API std::mutex& tie_graph_mutex();     // defined in iov2_objects.cpp
#else
inline std::mutex& tie_graph_mutex()
{
    static std::mutex m;
    return m;
}
#endif

/**
 * @lang{ZH}
 * @brief 为输入流与输出流提供公共操作的混入（mix-in）基类。
 *
 * 本结构集中承载定位、底层设备管理、转换行为调整、locale 存取与流绑定
 * 等成员，供具体流类型派生使用；这些成员通过 deducing-this（`this TSelf& self`）以派生类
 * 的具体类型执行操作。除显式标注为**不做线程同步**者（`device`/`detach`/`attach`）外，
 * 其余操作均持有本流的 `io_mutex()`，并将异常统一交由 `handle_exception` 处理。
 * 本结构还持有本流所绑定（tie）的目标指针。
 * @endif
 *
 * @lang{EN}
 * @brief Mix-in base that provides operations common to input and output streams.
 *
 * This struct centralizes members for positioning, underlying device management,
 * conversion-behavior adjustment, locale access, and stream tying, for
 * concrete stream types to derive from; these members use deducing-this (`this TSelf& self`)
 * to operate on the concrete derived type. Except for those explicitly documented as **not
 * synchronized** (`device`/`detach`/`attach`), all operations hold the stream's `io_mutex()`
 * and route exceptions through `handle_exception`. This struct also holds the pointer to the
 * stream this one is tied to.
 * @endif
 */
struct stream_common_operators
{
    /**
     * @lang{ZH}
     * @brief 拷贝/移动语义：**tie 边不参与拷贝与移动**。
     *
     * 四个特殊成员都把目标的 tie 目标置为 `nullptr`，移动还会额外清空源。
     *
     * @note **为什么不搬运。** tie 边不是一个"值"，而是本节点在 tie 图中的一条出边，绑定于
     *       对象身份。这与 `copyable_mutex` 的取舍完全一致——它的拷贝/移动同样是 no-op，
     *       产生一把全新的未锁互斥量，理由是"锁标识的是本对象的临界区，而不是需要被拷贝的
     *       值"。
     * @note **不搬运也是正确性所必需的。** 若赋值把源的 tie 边搬进目标，就等于绕过 `tie()`
     *       写入了 `m_tie_stream`，而 `tie()` 的环检测（`ensure_no_cycle`）是"图无环"这一
     *       不变式的唯一把关处。例如 `b.tie(&c); c.tie(&a); a = b;` 会闭合出环 `a→c→a`，
     *       且全程没有任何一次 `tie()` 调用；此后任意一次 `tie()` 都会在持有进程级全局锁的
     *       情况下永久自旋。置空使 `tie()` 重新成为唯一写入者。
     * @note **移动为何还要清空源。** 移动后的流，其 `m_streambuf` 与 `m_locale` 都已被掏空
     *       （而格式标志、状态位等是按值搬运、源仍保留）。tie 边属于前一类——它驱动 flush，
     *       是功能性状态。若不清空，一个被移走、随后又经 `attach()` 复活的流会继续静默地
     *       刷新一个调用方以为早已解除的目标。
     * @note **移后窗口内不得调用 `device()` 与 `detach()`。** 被移走的流可经 `attach()`（或被
     *       赋值）复活，但在复活之前它不持有设备，而这两个函数正以"持有设备"为前置条件，其间
     *       调用即为未定义行为；详见各自的文档。其余操作不受影响，照常把错误转为状态位。
     * @warning 这意味着拷贝或移动一个流之后，其 tie 关系**不会**保留，需要重新调用 `tie()`。
     *          另需注意：若某个流是**别人的** tie 目标，移动它并不会更新那些指向它的流——
     *          它们仍指向这个已被移空的对象，其后的 `tie()->flush()` 会静默失败（置上该目标
     *          自己的失败位，并被 sentry 吞掉）。这一点与 tie 目标的生命周期约束同属调用方
     *          责任，详见 `tie()`。
     * @endif
     *
     * @lang{EN}
     * @brief Copy/move semantics: **the tie edge takes no part in copying or moving**.
     *
     * All four special members set the destination's tie target to `nullptr`; a move
     * additionally clears the source.
     *
     * @note **Why it is not carried over.** A tie edge is not a "value" but this node's outgoing
     *       edge in the tie graph, bound to object identity. This matches the choice made for
     *       `copyable_mutex`, whose copy/move are likewise no-ops yielding a fresh unlocked
     *       mutex, on the grounds that "a lock denotes this object's critical section, not a
     *       value to be copied".
     * @note **Not carrying it over is also required for correctness.** If assignment moved the
     *       source's tie edge into the destination, that would write `m_tie_stream` while
     *       bypassing `tie()`, whose cycle check (`ensure_no_cycle`) is the only place the
     *       "graph is acyclic" invariant is enforced. For instance
     *       `b.tie(&c); c.tie(&a); a = b;` closes the cycle `a→c→a` without a single `tie()`
     *       call, after which any `tie()` spins forever while holding a process-wide lock.
     *       Resetting restores `tie()` as the sole writer.
     * @note **Why a move also clears the source.** After a move the stream's `m_streambuf` and
     *       `m_locale` have both been emptied (whereas the format flags, state bits and so on
     *       are carried by value and retained by the source). The tie edge belongs with the
     *       former: it drives flushing and is functional state. Without clearing, a stream that
     *       was moved from and then revived via `attach()` would keep silently flushing a target
     *       the caller believed it had long since given up.
     * @note **`device()` and `detach()` must not be called inside the moved-from window.** A
     *       moved-from stream can be revived by `attach()` (or by assignment), but until then it
     *       holds no device -- which is precisely what those two functions require -- so calling
     *       them meanwhile is undefined behavior; see their own documentation. The other
     *       operations are unaffected and keep turning errors into state bits as usual.
     * @warning This means a stream's tie relationship is **not** preserved across a copy or a
     *          move; call `tie()` again. Note also that moving a stream which is **someone
     *          else's** tie target does not update the streams pointing at it -- they still
     *          point at the emptied object, and their subsequent `tie()->flush()` fails silently
     *          (setting a failure bit on that target, which the sentry swallows). Like the
     *          lifetime constraint on tie targets, this is the caller's responsibility; see
     *          `tie()`.
     * @endif
     */
    stream_common_operators() = default;
    stream_common_operators(const stream_common_operators&) noexcept {}
    stream_common_operators(stream_common_operators&& other) noexcept
    { other.m_tie_stream.store(nullptr); }
    stream_common_operators& operator=(const stream_common_operators&) noexcept
    {
        m_tie_stream.store(nullptr);
        return *this;
    }
    stream_common_operators& operator=(stream_common_operators&& other) noexcept
    {
        // No self-assignment guard on purpose: for `a = std::move(a)` both stores hit the
        // same object and the result is still nullptr, which is what the rule prescribes.
        m_tie_stream.store(nullptr);
        other.m_tie_stream.store(nullptr);
        return *this;
    }
    ~stream_common_operators() = default;

    /**
     * @lang{ZH}
     * @brief 返回底层流的当前位置。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 当前位置；若流处于失败状态或发生异常，则返回空的 optional。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the current position of the underlying stream.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return The current position; an empty optional if the stream is in a failed
     *         state or an exception occurs.
     * @endif
     */
    template <typename TSelf>
    std::optional<size_t> tell(this TSelf& self)
    {
        std::lock_guard guard(self.io_mutex());
        if (!static_cast<bool>(self))
            return std::nullopt;
        try
        {
            return self.m_streambuf.tell();
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return std::nullopt;
    }

    /**
     * @lang{ZH}
     * @brief 从头（起始处）定位到绝对位置 `pos`。
     *
     * 定位前会清除 `eofbit`。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param pos 相对起始处的目标位置。
     * @return 流自身的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Seeks to the absolute position `pos` measured from the beginning.
     *
     * `eofbit` is cleared before seeking.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param pos The target position relative to the beginning.
     * @return A reference to the stream itself.
     * @endif
     */
    template <typename TSelf>
    TSelf& seek(this TSelf& self, size_t pos)
    {
        std::lock_guard guard(self.io_mutex());
        try
        {
            self.unset_state(ios_defs::eofbit);
            self.m_streambuf.seek(pos);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    /**
     * @lang{ZH}
     * @brief 从尾（末尾处）定位到位置 `pos`。
     *
     * 定位前会清除 `eofbit`。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param pos 相对末尾处的目标位置。
     * @return 流自身的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Seeks to the position `pos` measured from the end.
     *
     * `eofbit` is cleared before seeking.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param pos The target position relative to the end.
     * @return A reference to the stream itself.
     * @endif
     */
    template <typename TSelf>
    TSelf& rseek(this TSelf& self, size_t pos)
    {
        std::lock_guard guard(self.io_mutex());
        try
        {
            self.unset_state(ios_defs::eofbit);
            self.m_streambuf.rseek(pos);
        }
        catch(...)
        {
            self.handle_exception(std::current_exception());
        }

        return self;
    }

    /**
     * @lang{ZH}
     * @brief 返回底层设备的引用。
     *
     * @warning 本操作**不做线程同步**，不获取 `io_mutex()`。返回的是绑定到底层设备的引用，
     *          **不要把它保存到临界区之外**再使用：在并发变更（如 `attach`/`detach`）下，
     *          离开锁的保护后读取该引用即为数据竞争与未定义行为。
     * @warning **本函数要求流当前持有设备。** 移动之后（转换器已被掏空）与 `detach()` 之后
     *          （设备已交还调用方）都不满足这个前置条件，其间调用即为未定义行为。当前实现在
     *          这两种情形下的表现并不相同——移后会抛出 `cvt_error`（来自 `runtime_cvt::device()`），
     *          `detach()` 之后则返回一个指向已被移走的设备对象的引用——但两者都只是实现细节、
     *          不构成契约，尤其**不得**以抛不抛异常来判断流是否持有设备。经 `attach()`（或被
     *          赋值）重新装上设备之后，本函数即恢复正常。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 底层设备的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Returns a reference to the underlying device.
     *
     * @warning This operation is **not synchronized** and takes no `io_mutex()`. It returns a
     *          reference bound to the underlying device; **do not keep it past the critical
     *          section**: under concurrent mutation (e.g. `attach`/`detach`), reading the
     *          reference outside the lock is a data race and undefined behavior.
     * @warning **This function requires the stream to currently hold a device.** Neither a
     *          moved-from stream (its converter has been emptied) nor one after `detach()` (its
     *          device has been handed back to the caller) meets that precondition, and calling
     *          it meanwhile is undefined behavior. What the current implementation does differs
     *          between the two -- a moved-from stream throws a `cvt_error` (from
     *          `runtime_cvt::device()`), while after `detach()` it returns a reference to a
     *          moved-from device object -- but both are implementation details rather than a
     *          contract, and in particular whether it throws must **not** be used to test
     *          whether the stream holds a device. Installing a device again with `attach()` (or
     *          by assignment) restores normal use.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return A reference to the underlying device.
     * @endif
     */
    template <typename TSelf>
    auto& device(this TSelf& self)
    {
        return self.m_streambuf.device();
    }

    /**
     * @lang{ZH}
     * @brief 分离并取回底层设备（连同分离期间捕获的错误）。
     *
     * @warning 本操作**不做线程同步**：与本流的其它操作（`tell`/`seek`/格式化 I/O 等均持有
     *          `io_mutex()`）不同，`detach()` 不获取任何锁。它是一个类似构造/析构的生命周期
     *          操作——分离底层设备本就意味着流不再处于可用于读写的稳定状态。调用方必须保证在
     *          `detach()` 执行期间没有任何其它线程对本流进行操作（读、写，或再次 attach/detach），
     *          否则行为未定义；正如不应在对象构造/析构过程中使用该对象一样。
     * @warning 与 `device()` 相同，**流处于移后（moved-from）状态时不得调用本函数**：此时转换器
     *          已被掏空，没有可分离的设备，调用即为未定义行为。本函数是 `noexcept`，底层
     *          （`runtime_cvt::detach()`）当前在这种情形下直接 `std::terminate()`；这同样只是
     *          实现细节，不构成契约。经 `attach()`（或被赋值）重新装上设备之后即恢复正常。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 取回的设备，以及一个 `exception_ptr`（分离时若发生 flush 等错误则非空）。
     * @endif
     *
     * @lang{EN}
     * @brief Detaches and retrieves the underlying device (along with any error captured
     *        during detach).
     *
     * @warning This operation is **not synchronized**: unlike the stream's other operations
     *          (`tell`/`seek`/formatted I/O, which all hold `io_mutex()`), `detach()` takes no
     *          lock. It is a lifecycle operation akin to construction/destruction -- detaching
     *          the underlying device inherently means the stream is no longer in a stable state
     *          usable for I/O. The caller must ensure that no other thread operates on this
     *          stream (reading, writing, or another attach/detach) while `detach()` runs;
     *          otherwise the behavior is undefined, just as one must not use an object while it
     *          is being constructed or destroyed.
     * @warning As with `device()`, **this function must not be called on a moved-from stream**:
     *          its converter has been emptied, so there is no device to detach, and calling it
     *          then is undefined behavior. This function is `noexcept`, and the underlying
     *          `runtime_cvt::detach()` currently calls `std::terminate()` in that case; that too
     *          is an implementation detail rather than a contract. Installing a device again
     *          with `attach()` (or by assignment) restores normal use.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return The retrieved device and an `exception_ptr` (non-null if e.g. a flush error
     *         occurred during detach).
     * @endif
     */
    template <typename TSelf>
    auto detach(this TSelf& self) noexcept
    {
        return self.m_streambuf.detach();
    }

    /**
     * @lang{ZH}
     * @brief 安装（替换）底层设备。
     *
     * 状态在换设备**之前**就被整体清回 `goodbit`，成功时的效果与 `std::basic_ifstream::open`
     * 自 C++11 起的做法一致。已有的状态位描述的是**旧设备**上发生过什么——EOF 读到过尾、上一次
     * 解析失败、上一次 `attach()` 装了个不可用的设备——设备既已换掉，这些结论便不再成立。若不清，
     * 一次失败的 `attach()` 会把流永久毒化：即便随后装上完全正常的设备，流仍报告失败，调用方
     * 必须自己记得 `clear()`，"换个设备重试"这条本该走通的恢复路径就断了。
     *
     * @note 清状态必须排在换设备**之前**，否则失败路径上留下的是新旧混合的状态。底层的
     *       `streambuf::attach()` 先装入新设备、再初始化转换器，而抛异常的是后一步——异常抛出时
     *       旧设备已经不复存在、新设备已经就位，没有回滚。若把 `clear()` 放在后面，它在失败时
     *       根本不会执行，`handle_exception` 置上的新失败位便与旧设备遗留的位叠在一起，得到一个
     *       描述两个不同设备的状态。清在前面，失败后的状态就只描述这一次 `attach()`。
     *
     * @note 安装设备时抛出的异常（如新设备无法确定流起点）交由 `handle_exception` 处理：置相应
     *       失败位，并按流的异常掩码决定是否重新抛出。这一点与**构造函数**不同——构造函数的成员
     *       初始化列表若抛出异常，C++ 规定它必然向外传播（构造函数 function-try-block 的处理器
     *       执行到末尾时会自动重抛，且其中不允许 `return`），流对象根本没有诞生，也就无处安放
     *       状态位。因此"默认构造后 `attach()`"这条生命周期上，构造那步可能抛，`attach()` 这步
     *       不会。`detach()` 同样不抛，它把错误作为 `exception_ptr` 返回。
     * @warning 与 `detach()` 相同，本操作**不做线程同步**、不获取 `io_mutex()`。它是类似构造的
     *          生命周期操作：替换底层设备期间，任何并发读写本身都是不稳定且无意义的。调用方必须
     *          保证在 `attach()` 执行期间没有任何其它线程对本流进行操作，否则行为未定义。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param dev 要安装的设备；默认为默认构造的设备。
     * @endif
     *
     * @lang{EN}
     * @brief Installs (replaces) the underlying device.
     *
     * The state is cleared back to `goodbit` **before** the device is replaced, which on success
     * amounts to what `std::basic_ifstream::open` has done since C++11. Whatever bits are set
     * describe what happened on the **old** device -- input was read to the end, the last parse
     * failed, the last `attach()` installed an unusable device -- and none of those conclusions
     * survive replacing it. Leaving them would let one failed `attach()` poison the stream for
     * good: even after a perfectly good device is installed the stream would keep reporting
     * failure until the caller remembered to `clear()`, which breaks the "install another device
     * and retry" recovery path.
     *
     * @note Clearing has to come **before** the replacement, or the failure path is left holding
     *       a mixture of old and new. The underlying `streambuf::attach()` installs the new
     *       device first and initializes the converter second, and it is the second step that
     *       throws -- by then the old device is gone and the new one is in place, with no
     *       rollback. A `clear()` placed afterwards simply would not run on failure, so the bit
     *       `handle_exception` sets would sit alongside the old device's leftovers and describe
     *       two different devices at once. Clearing first leaves a state that describes only
     *       this `attach()`.
     *
     * @note An exception thrown while installing the device (a new device whose stream origin
     *       cannot be determined, say) goes to `handle_exception`: the matching failure bit is
     *       set, and whether it is rethrown follows the stream's exception mask. This is unlike
     *       a **constructor**, where C++ requires an exception from the member initializer list
     *       to propagate (the handler of a constructor function-try-block rethrows when control
     *       reaches its end, and a `return` is not allowed there), because the stream object
     *       never came into existence and there is nowhere to put a state bit. So along the
     *       "default-construct, then `attach()`" lifecycle the construction step may throw and
     *       the `attach()` step will not. `detach()` likewise does not throw; it returns the
     *       error as an `exception_ptr`.
     * @warning Like `detach()`, this operation is **not synchronized** and takes no
     *          `io_mutex()`. It is a construction-like lifecycle operation: any concurrent
     *          read/write while the underlying device is being replaced is itself unstable and
     *          meaningless. The caller must ensure that no other thread operates on this stream
     *          while `attach()` runs; otherwise the behavior is undefined.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param dev The device to install; defaults to a default-constructed device.
     * @endif
     */
    template <typename TSelf>
    void attach(this TSelf& self, typename TSelf::device_type&& dev = typename TSelf::device_type{})
    {
        try
        {
            self.clear();
            self.m_streambuf.attach(std::move(dev));
        }
        catch (...)
        {
            self.handle_exception(std::current_exception());
        }
    }

    /**
     * @lang{ZH}
     * @brief 调整底层编码转换的行为。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param acc 要应用的转换行为设置。
     * @endif
     *
     * @lang{EN}
     * @brief Adjusts the behavior of the underlying encoding conversion.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param acc The conversion-behavior settings to apply.
     * @endif
     */
    template <typename TSelf>
    void adjust(this TSelf& self, const cvt_behavior& acc)
    {
        std::lock_guard guard(self.io_mutex());
        return self.m_streambuf.adjust(acc);
    }

    /**
     * @lang{ZH}
     * @brief 取回底层编码转换的当前状态。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param acc 用于接收转换状态的输出参数。
     * @endif
     *
     * @lang{EN}
     * @brief Retrieves the current state of the underlying encoding conversion.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param acc Output parameter receiving the conversion status.
     * @endif
     */
    template <typename TSelf>
    void retrieve(this const TSelf& self, cvt_status& acc)
    {
        std::lock_guard guard(self.io_mutex());
        return self.m_streambuf.retrieve(acc);
    }

    /**
     * @lang{ZH}
     * @brief 返回本流当前使用的 locale（getter）。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 绑定到本流 `m_locale` 的常量引用。
     * @warning 返回的是引用，**不要把它保存到临界区之外**再使用：在并发调用 `locale(loc)`
     *          setter 时，离开锁的保护后读取该引用即为数据竞争与未定义行为。详见 locale setter。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the locale currently used by this stream (getter).
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return A const reference bound to this stream's `m_locale`.
     * @warning This returns a reference; **do not keep it past the critical section**: while a
     *          `locale(loc)` setter runs concurrently, reading the reference outside the lock is
     *          a data race and undefined behavior. See the locale setter for details.
     * @endif
     */
    template <typename TSelf>
    const auto& locale(this const TSelf& self)
    {
        return self.m_locale;
    }

    /**
     * @lang{ZH}
     * @brief 设置本流使用的 locale，并返回先前的 locale。
     *
     * 新 locale 生效后会立即以其调用 `access_callbacks`，使已注册的 facet 相关回调据以
     * 刷新缓存。
     *
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @tparam TChar locale 的字符类型。
     * @param loc 要设置的新 locale（按值接收，内部移动入 `m_locale`）。
     * @return 先前的 locale（已被移出并通过返回值转交调用方）。
     *
     * @note 回调失败时的行为：新 locale 的安装（`m_locale` 的 move-assign）在调用回调**之前**
     *       就已完成，且 `access_callbacks` 会先把所有回调**全部执行完毕**、再重抛其中第一个
     *       捕获的异常（见 `access_callbacks`）。因此回调抛出并不意味着本次设置被中断或只完成了
     *       一半：新 locale 已生效，全部回调也都已执行，异常只是“其中至少有一个失败了”的事后
     *       报告。本 setter **不做任何回滚**——回滚会让已按新 locale 重建完毕的 pword 缓存与被
     *       还原的旧 locale 相互矛盾，且为修复它而重跑一遍回调同样可能失败。
     * @note 该异常随后交由 `handle_exception` 归类：置上相应失败位（`stream_error`→`strfailbit`、
     *       `cvt_error`→`cvtfailbit` 等），并**仅当该位处于异常掩码中时**才向调用方传播。
     *       故默认（掩码为空）情况下本函数不抛出，正常返回旧 locale，仅留下一个失败位供检查；
     *       若该位在掩码中则本函数抛出，此时旧 locale 随返回值一同丢失，无法取回。
     * @note 本 setter 自身持有本流的 `io_mutex()`。由于 `operator>>`/`operator<<`/`get`/`put`
     *       等格式化 I/O 在其 sentry 生命周期内持有同一把锁，本次 move-assign `m_locale` 绝不会
     *       落在某次格式化操作的中途——格式化过程内部持有的 `locale()` 引用因此始终有效。
     * @warning 仍属调用方责任的是：`locale()` getter 返回的是绑定到 `m_locale` 的引用，**不要
     *          把它保存到临界区之外**再使用；一旦离开锁的保护，另一线程的 `locale(loc)` 会把它
     *          指向的对象移走，读取即为数据竞争与未定义行为。若需跨多次操作稳定持有，请以
     *          `IOv2::sync` 把这些操作圈进同一个临界区。
     * @note 此约束并非 locale 专有：凡返回内部状态引用的 getter（如 `device()`）在并发变更下都
     *       有同样要求。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the locale used by this stream and returns the previous one.
     *
     * Once the new locale takes effect, `access_callbacks` is invoked with it immediately so
     * that registered facet-related callbacks refresh their caches accordingly.
     *
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @tparam TChar The character type of the locale.
     * @param loc The new locale to install (taken by value, moved into `m_locale`).
     * @return The previous locale (moved out and handed back to the caller).
     *
     * @note Behavior when a callback fails: the new locale is installed (the move-assignment
     *       of `m_locale`) *before* the callbacks are invoked, and `access_callbacks` runs
     *       *every* callback to completion before rethrowing the first exception it captured
     *       (see `access_callbacks`). A throwing callback therefore does not mean the set was
     *       interrupted or half-applied: the new locale is in effect and all callbacks have
     *       run; the exception is merely an after-the-fact report that at least one of them
     *       failed. This setter performs **no rollback** -- restoring the old locale would
     *       contradict the pword caches already rebuilt for the new one, and re-running the
     *       callbacks to repair that could fail just the same.
     * @note The exception is then categorized by `handle_exception`: it sets the matching
     *       failure bit (`stream_error`→`strfailbit`, `cvt_error`→`cvtfailbit`, etc.) and
     *       propagates to the caller **only if that bit is in the exception mask**. So by
     *       default (empty mask) this function does not throw: it returns the previous locale
     *       normally and merely leaves a failure bit to be inspected. If the bit is in the
     *       mask this function throws, and the previous locale is lost along with the return
     *       value -- it cannot be recovered.
     * @note This setter itself holds the stream's `io_mutex()`. Since formatted I/O
     *       (`operator>>`/`operator<<`/`get`/`put`) holds that same lock for its sentry's
     *       lifetime, this move-assignment of `m_locale` can never land in the middle of a
     *       formatting operation, so the `locale()` reference that formatting holds internally
     *       stays valid throughout.
     * @warning What remains the caller's responsibility: the `locale()` getter returns a
     *          reference bound to `m_locale` — **do not keep it past the critical section**.
     *          Outside the lock, another thread's `locale(loc)` can move the referenced object
     *          out from under you, and reading it is a data race and undefined behavior. To
     *          hold it stably across several operations, group them into one critical section
     *          with `IOv2::sync`.
     * @note This is not specific to locale: every getter returning a reference to internal
     *       state (e.g. `device()`) carries the same requirement under concurrent mutation.
     * @endif
     */
    template <typename TSelf, typename TChar>
    auto locale(this TSelf& self, IOv2::locale<TChar> loc)
    {
        std::lock_guard guard(self.io_mutex());
        auto res = std::move(self.m_locale);
        self.m_locale = std::move(loc);
        try
        {
            self.access_callbacks(self.m_locale);
        }
        catch (...)
        {
            self.handle_exception(std::current_exception());
        }
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 设置本流所绑定（tie）的输出流，并返回先前绑定的流。
     *
     * 绑定后，本流在每次 I/O 前都会（经 sentry）调用 `tie()->flush()`，以保证例如提示符
     * 先于其对应的输入被刷出。
     *
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @param str 要绑定的输出流；传 `nullptr` 解除绑定。
     * @return 先前绑定的输出流（可能为 `nullptr`）。
     *
     * @warning 生命周期由调用方负责：`str` 仅以裸指针保存，本类不做任何生命周期管理，
     *          也不会在析构时自动解绑。最简单且始终安全的规则是：让 `str` 所指的流存活于
     *          所有绑定到它的流之后（这也是标准用法，如全局 `cout` 活得比 `cin` 久）。否则
     *          任何 I/O 都会经由 `tie()->flush()` 解引用悬空指针，导致未定义行为。此契约与
     *          标准库 `std::basic_ios::tie` 一致。
     * @warning **销毁之外，"被移动"同样会让一个 tie 目标失效。** 移动一个流会掏空它的
     *          `m_streambuf` 与 `m_locale`，但指向它的那些流并不会被更新——它们仍指向这个
     *          已被移空的对象。这不会导致未定义行为（`tie()->flush()` 会在该目标上置一个失败
     *          位，随后被 sentry 吞掉），但 tie 关系从此**静默失效**：绑定方仍报告 `good()`，
     *          而刷新实际上什么也没做。因此上面那条规则应读作：让 tie 目标既不被销毁、也不被
     *          移动，直到所有绑定到它的流都已解绑或销毁。
     * @note 本流自身的 tie 边不随拷贝或移动传递——四个特殊成员都会把它重置为 `nullptr`
     *       （移动还会清空源）。拷贝或移动一个流之后需要重新调用本函数。详见本类的拷贝/移动
     *       语义说明。
     * @warning **并发下的加强约束**：单独调用 `tie(nullptr)` 并**不足以**使随后销毁 `str`
     *          变得安全。sentry 会在获取本流 `io_mutex()` **之前**就（无锁）原子读出 tie 指针
     *          并 `flush()` 该目标（见 in_sentry/out_sentry 构造函数），因此另一线程在途的
     *          I/O 可能在你的 `tie(nullptr)` 写入变得可见之前，就已读到旧指针并正准备对目标
     *          `flush()`。故若要在生命周期中途改绑或销毁某个 tie 目标 P，调用方必须：先保证
     *          没有任何线程正在、也不会即将对任何 `tie() == P` 的流发起 I/O；再对这些流调用
     *          `tie(nullptr)`（或确保其已析构）；最后才销毁 P。这一点无法用锁代劳——目标的
     *          生命周期不受本流 `io_mutex()` 保护，且没有任何锁能保护一个正被其自身析构函数
     *          销毁的对象。
     * @note 设置前会沿目标流 `str` 的 tie 链向前遍历：若该链会回到本流（即形成环，自绑定是
     *       长度为 1 的环），则抛出 `stream_error` 并**保持本流原绑定不变**，从而在设置时杜绝
     *       环。这满足了 `std::basic_ios::tie` “不得成环”的前置条件，而非像标准那样把成环留作
     *       未定义行为。仅当本流本身可作为 tie 目标（即派生自 `abs_flusher`）时才会遍历；纯输入
     *       流不可能被 tie，也就不可能出现在环中。
     * @note 上述“检测 + 提交”由进程级全局锁 `tie_graph_mutex()` 合成一个原子步骤，因此即便
     *       两个线程并发 `tie()`（如 `A.tie(B)` 与 `B.tie(A)`）也无法成环：所有 setter 串行化，
     *       任一次检测遍历期间整张 tie 图都被冻结，绝不会出现“各自读到对方旧状态、双双通过检测”
     *       的窗口。该锁为**普通**互斥量即可——遍历调用的是 tie() 的 getter（一次原子读、不取该
     *       锁），持锁期间不会重入 setter；它也不与各流的 `io_mutex()` 嵌套，故不引入新的加锁
     *       顺序约束。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the output stream this stream is tied to and returns the previous one.
     *
     * Once tied, before every I/O operation this stream calls `tie()->flush()` (via the
     * sentry), so that e.g. a prompt is flushed before its matching input is read.
     *
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @param str The stream to tie to; pass `nullptr` to untie.
     * @return The previously tied stream (may be `nullptr`).
     *
     * @warning Lifetime is the caller's responsibility: `str` is stored as a raw pointer;
     *          this class performs no lifetime management and does not auto-untie on
     *          destruction. The simplest always-safe rule is to let `str` outlive every
     *          stream tied to it (this is also the standard usage, e.g. a global `cout`
     *          outlives `cin`); otherwise any I/O dereferences a dangling pointer through
     *          `tie()->flush()` — undefined behavior. This matches the
     *          `std::basic_ios::tie` contract.
     * @warning **Beyond destruction, *being moved from* also invalidates a tie target.**
     *          Moving a stream empties its `m_streambuf` and `m_locale`, but the streams
     *          pointing at it are not updated -- they still point at the emptied object. This
     *          is not undefined behavior (`tie()->flush()` sets a failure bit on that target,
     *          which the sentry then swallows), but the tie relationship **fails silently**:
     *          the tied stream still reports `good()` while the flush does nothing. So read the
     *          rule above as: let a tie target be neither destroyed nor moved from until every
     *          stream tied to it has been untied or destroyed.
     * @note This stream's own tie edge is not carried across a copy or a move -- all four
     *       special members reset it to `nullptr` (a move also clears the source). Call this
     *       function again after copying or moving a stream. See this class's copy/move
     *       semantics for details.
     * @warning **Concurrency addendum:** calling `tie(nullptr)` alone is **not** sufficient
     *          to make a subsequent destruction of `str` safe. The sentry reads the tie
     *          pointer (a lock-free atomic load) and `flush()`es the target *before* it
     *          acquires this stream's `io_mutex()` (see the in_sentry/out_sentry
     *          constructors), so another thread's in-flight I/O may have already latched the
     *          old pointer -- and be about to `flush()` the target -- before your
     *          `tie(nullptr)` store becomes visible. Therefore, to retarget or destroy a tie
     *          target P during its lifetime, the caller must: first ensure no thread is
     *          performing, or about to perform, I/O on any stream whose `tie() == P`; then
     *          `tie(nullptr)` those streams (or ensure they are already destroyed); and only
     *          then destroy P. No lock can do this for you -- the target's lifetime is not
     *          guarded by this stream's `io_mutex()`, and no lock can protect an object while
     *          it is being torn down by its own destructor.
     * @note Before setting, the tie chain reachable from the target `str` is walked
     *       forward: if that chain leads back to this stream (i.e. it would form a cycle;
     *       a self-tie is the length-1 cycle), a `stream_error` is thrown and **this
     *       stream's existing tie is left unchanged**, so cycles are rejected at set time.
     *       This satisfies the no-cycle precondition of `std::basic_ios::tie` rather than
     *       leaving a cycle as undefined behavior as the standard does. The walk runs only
     *       when this stream can itself be a tie target (i.e. derives from `abs_flusher`);
     *       a pure input stream can never be tied to, so it can never appear in a cycle.
     * @note This "detect + commit" is fused into one atomic step by the process-wide lock
     *       `tie_graph_mutex()`, so no cycle can form even under concurrent `tie()` (e.g.
     *       `A.tie(B)` and `B.tie(A)`): all setters are serialized and the whole tie graph
     *       is frozen for the duration of any detection walk, so the "each reads the other's
     *       stale state and both pass the check" window never exists. A **plain** mutex
     *       suffices -- the walk calls tie()'s getter (a single atomic load that does not
     *       take this lock), so a setter never re-enters while holding it -- and it never
     *       nests with a stream's `io_mutex()`, so it adds no new lock-ordering constraint.
     * @endif
     */
    template <typename TSelf>
    abs_flusher* tie(this TSelf& self, abs_flusher* str)
    {
        std::lock_guard graph_lock(tie_graph_mutex());
        auto res = self.m_tie_stream.load();
        if constexpr (std::derived_from<TSelf, abs_flusher>)
            self.ensure_no_cycle(str);
        self.m_tie_stream.store(str);
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 返回本流当前所绑定（tie）的输出流（getter）。
     *
     * 通过一次无锁的原子读取完成，不获取任何锁。
     * @tparam TSelf 派生的具体流类型（由 deducing-this 推导）。
     * @return 当前绑定的输出流；未绑定时为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * @brief Returns the output stream this stream is currently tied to (getter).
     *
     * Performed by a single lock-free atomic load; takes no lock.
     * @tparam TSelf The concrete derived stream type (deduced via deducing-this).
     * @return The currently tied output stream; `nullptr` if none.
     * @endif
     */
    template <typename TSelf>
    abs_flusher* tie(this const TSelf& self)
    {
        return self.m_tie_stream.load();
    }

private:
    /**
     * @lang{ZH}
     * @brief 沿 `str` 的 tie 链向前遍历，若该链会回到 `*this` 则抛出。
     *
     * 遍历用 Floyd 判圈（龟兔赛跑）：慢指针每次一步、快指针每次两步。朴素的单指针写法只有
     * 两个出口——走到 `nullptr`，或撞上 `*this`；一旦链上已经存在一个**不含** `*this` 的环，
     * 两个出口都到不了，循环永不终止。而本函数是在 `tie()` 持有进程级全局锁
     * `tie_graph_mutex()` 的情况下运行的，所以那不是一个线程卡住，而是**整个进程的 `tie()`
     * 永久瘫痪**。快指针追上慢指针即说明有环，于是降级为一个可诊断的异常。
     *
     * @note 只要 `tie()` 保持为 `m_tie_stream` 的唯一写入者，第二种异常就不会发生——它是纵深
     *       防御：若将来又出现绕过 `tie()` 的写入路径（本类的拷贝/移动赋值曾经就是这样一条），
     *       结果是一个能定位的错误，而不是静默死锁。
     * @note 用 Floyd 而不是"记录已访问节点"，是因为后者要分配内存，而这里持有全局锁；也不用
     *       "迭代次数上限"，因为那需要一个既可能误伤长链、又可能失效的魔法常数。
     * @param str 起点，即待设置的 tie 目标；为 `nullptr`、或指向一个不带出边的裸
     *            `abs_flusher` 时，链长为零，本函数直接返回。
     * @throw stream_error 若链会回到 `*this`（本次设置会成环），或链上已存在其它环。
     * @endif
     *
     * @lang{EN}
     * @brief Walks forward along `str`'s tie chain and throws if it leads back to `*this`.
     *
     * The walk uses Floyd's cycle detection (tortoise and hare): the slow pointer advances one
     * step per round, the fast pointer two. The naive single-pointer formulation has only two
     * exits -- reaching `nullptr`, or hitting `*this`; once the chain already contains a cycle
     * that does **not** include `*this`, neither is reachable and the loop never terminates.
     * And this runs while `tie()` holds the process-wide `tie_graph_mutex()`, so that is not one
     * thread stuck but **every `tie()` in the process wedged forever**. The fast pointer catching
     * the slow one means a cycle exists, which is then reported as a diagnosable exception.
     *
     * @note As long as `tie()` remains the sole writer of `m_tie_stream`, the second exception
     *       cannot happen -- it is defense in depth: should another write path bypass `tie()`
     *       again (this class's copy/move assignment once was exactly such a path), the result
     *       is a locatable error rather than a silent deadlock.
     * @note Floyd is used rather than "remember every visited node" because the latter
     *       allocates and a process-wide lock is held here; and rather than "cap the iteration
     *       count" because that needs a magic constant that can both falsely reject a long chain
     *       and fail to catch a real cycle.
     * @param str The starting node, i.e. the tie target being set. When it is `nullptr`, or a
     *            bare `abs_flusher` that carries no outgoing edge, the chain has length zero and
     *            this function returns immediately.
     * @throw stream_error If the chain leads back to `*this` (this set would form a cycle), or
     *        if the chain already contains another cycle.
     * @endif
     */
    void ensure_no_cycle(abs_flusher* str) const
    {
        // Edges are stored as abs_flusher*, but only a stream_common_operators carries an
        // outgoing edge, so each step has to cross-cast. A node that is a bare abs_flusher
        // yields nullptr and ends the chain -- which is what keeps a plain flusher usable as a
        // tie target.
        auto next = [](stream_common_operators* p)
        { return dynamic_cast<stream_common_operators*>(p->tie()); };

        stream_common_operators* slow = dynamic_cast<stream_common_operators*>(str);
        stream_common_operators* fast = slow;

        while (slow != nullptr)
        {
            if (slow == this)
                throw stream_error("stream tie fail: the requested tie would form a cycle in the tie chain");

            slow = next(slow);
            fast = (fast != nullptr) ? next(fast) : nullptr;
            fast = (fast != nullptr) ? next(fast) : nullptr;

            if (fast != nullptr && fast == slow)
                throw stream_error("stream tie fail: the tie chain already contains a cycle; "
                                   "something other than tie() has written a tie pointer");
        }
    }

    /**
     * @lang{ZH}
     * @brief 本流所绑定（tie）目标的原子指针；`nullptr` 表示未绑定。
     *
     * @warning **`tie()` 必须保持为本成员的唯一写入者**（拷贝/移动特殊成员除外，它们只把它
     *          重置为 `nullptr`，见上）。整张 tie 图"无环"这一不变式完全建立在这一点上：
     *          `tie()` 在提交前会做环检测，而任何绕过它的写入都能悄无声息地制造出环，进而让
     *          `ensure_no_cycle` 的遍历在持有进程级全局锁时不终止。此前编译器隐式生成的
     *          拷贝/移动赋值就是这样一条路径——`m_tie_stream` 是 `copyable_atomic`，赋值会
     *          直接搬运指针值，于是 `a = b` 等同于一次未经检查的 `tie()`。
     * @endif
     *
     * @lang{EN}
     * @brief Atomic pointer to this stream's tie target; `nullptr` means untied.
     *
     * @warning **`tie()` must remain the sole writer of this member** (apart from the copy/move
     *          special members, which only reset it to `nullptr`; see above). The whole
     *          "the tie graph is acyclic" invariant rests on that: `tie()` runs a cycle check
     *          before committing, and any write that bypasses it can silently create a cycle,
     *          which then makes `ensure_no_cycle`'s walk non-terminating while a process-wide
     *          lock is held. The compiler-generated copy/move assignment used to be exactly such
     *          a path -- `m_tie_stream` is a `copyable_atomic`, so assignment carried the
     *          pointer value straight over, making `a = b` an unchecked `tie()`.
     * @endif
     */
    copyable_atomic<abs_flusher*> m_tie_stream{nullptr};
};
}
