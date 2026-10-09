// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file iov2_objects.cpp
 * @lang{ZH}
 * 共享库（DSO/DLL）模式下，全部进程级单例的唯一定义点。
 *
 * 默认的 header-only 模式**不**需要本文件：那时每个单例都是头文件里的 inline
 * 变量。只有当客户希望把 IOv2 编译成独立的共享库、以保证“跨任意 DSO/DLL 单实例”
 * 时，才把本文件编进库。
 *
 * 本翻译单元同时定义 `IOV2_SHARED` 与 `IOV2_EXPORTS`，于是各头文件只暴露对单例的
 * `extern` 声明，真正的定义集中在此处。因为是单一 TU，初始化顺序即定义顺序
 * （[basic.start.dynamic]/3.1，ordered）：
 *   - `s_ori_facet_buf` 必须最先——每个流的 `locale` 成员都经它解析；
 *   - `cout` / `wcout` 必须先于绑定到它们的流（`cerr`/`cin` tie `cout`，
 *     `wcerr`/`wcin` tie `wcout`）。
 *
 * 构建示例（Linux，按需替换第三方依赖与路径）：
 * @code
 *   g++ -std=c++23 -fPIC -fvisibility=hidden -shared \
 *       -Iinclude -I/usr/include/botan-2 -D_POSIX_C_SOURCE=200809L \
 *       -DIOV2_SHARED -DIOV2_EXPORTS src/iov2_objects.cpp \
 *       -o libiov2.so -lz -lbotan-2
 *   # 或直接用根 Makefile / or just use the root Makefile:  make shared
 *   # 客户：编译时加 -DIOV2_SHARED，链接时加 -liov2
 * @endcode
 * @endif
 *
 * @lang{EN}
 * The single definition point for all process-wide singletons in shared-library
 * (DSO/DLL) mode.
 *
 * The default header-only mode does NOT need this file: there every singleton is
 * an inline variable in the headers. Compile this file into the library only when
 * a customer wants IOv2 as a standalone shared library to guarantee a single
 * instance across arbitrary DSOs/DLLs.
 *
 * This translation unit defines both `IOV2_SHARED` and `IOV2_EXPORTS`, so the
 * headers expose only `extern` declarations of the singletons and the real
 * definitions live here. Being a single TU, initialization order equals definition
 * order ([basic.start.dynamic]/3.1, ordered):
 *   - `s_ori_facet_buf` must come first -- every stream's `locale` member resolves
 *     through it;
 *   - `cout` / `wcout` must precede the streams that tie to them (`cerr`/`cin` tie
 *     `cout`, `wcerr`/`wcin` tie `wcout`).
 *
 * Build example (Linux; substitute third-party deps/paths as needed):
 * @code
 *   g++ -std=c++23 -fPIC -fvisibility=hidden -shared \
 *       -Iinclude -I/usr/include/botan-2 -D_POSIX_C_SOURCE=200809L \
 *       -DIOV2_SHARED -DIOV2_EXPORTS src/iov2_objects.cpp \
 *       -o libiov2.so -lz -lbotan-2
 *   # 或直接用根 Makefile / or just use the root Makefile:  make shared
 *   # Consumers: compile with -DIOV2_SHARED, link with -liov2
 * @endcode
 * @endif
 */

#define IOV2_SHARED
#define IOV2_EXPORTS

#include <IOv2/io/objects/in_impl.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/locale/ori_facet_buf.h>

#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif
#if defined(__GLIBC__)
#include <link.h>
#endif

namespace IOv2
{
/**
 * @lang{ZH}
 * 单例的生命周期与跨 DSO 顺序。
 *
 * 本 TU 内:定义顺序即初始化顺序(单 TU、ordered init,[basic.start.dynamic]/3.1)。
 * 因此 `s_ori_facet_buf` 必须最前——每个流的 `locale` 成员都经它解析;`cout`/`wcout`
 * 必须先于绑定到它们的流。各 `_*_init` guard 是 TU-local(内部链接),只有引用被导出。
 *
 * 为什么这套设计对共享库(DSO)是安全的:
 * 这些单例在 libiov2.so 内定义一次、以导出引用暴露,所有模块共享同一实例;跨模块的
 * 生命周期顺序**交给 ELF 动态加载器的依赖序**保证。任何用到这些符号的模块都对
 * libiov2.so 有链接依赖(`DT_NEEDED`),于是加载器保证:
 *   - **初始化**:libiov2.so 的 initializer(即此处这些单例的构造)先于依赖方模块的
 *     静态构造 → 消费者静态对象的**构造函数**里可安全使用它们;
 *   - **终止**:finalize 是 init 的逆序,libiov2.so 最后 finalize → 消费者静态对象的
 *     **析构函数**里也可安全使用它们。
 * 而且这些单例在 finalize 时**不析构**(与 libstdc++ 对 `std::cout` 的处理相同):各
 * `_*_init` 的析构只执行构造时登记的退出钩子——输出流 flush、其余空操作,见
 * `sing_temp::exit_hook`。对象存储是静态缓冲,连同其持有的堆内存保持可达、由 OS 回收,
 * 不构成泄漏;退出阶段仍在其它线程中使用这些流也不会成为释放后使用。
 *
 * @warning 消费者模块被钉住,libiov2.so 随之驻留:共享模式下每个包含 IOv2 头文件的模块
 * 在载入时经 `pin_module_of` 把自己标记为不可卸载(见 `common/iov2_export.h`),对它
 * `dlclose` 只减引用计数;它对 libiov2.so 的依赖也就一直存在,libiov2.so 不会在仍有
 * 引用存活时被 unmap。仍不支持的是消费者与本库仅在运行期耦合、无加载器可见依赖边的用法
 * (如 `dlopen(RTLD_GLOBAL)` 靠符号插桩):那时初始化顺序没有保证。
 * @endif
 *
 * @lang{EN}
 * Singleton lifetime and cross-DSO ordering.
 *
 * Within this TU, definition order is initialization order (single TU, ordered
 * init, [basic.start.dynamic]/3.1). Hence `s_ori_facet_buf` must come first --
 * every stream's `locale` member resolves through it -- and `cout`/`wcout` must
 * precede the streams that tie to them. The `_*_init` guards are TU-local (internal
 * linkage); only the references are exported.
 *
 * Why this is safe as a shared library (DSO): these singletons are defined once in
 * libiov2.so and exposed as exported references, so every module shares the one
 * instance, and cross-module lifetime ordering is delegated to the ELF dynamic
 * loader's dependency ordering. Any module that uses these symbols has a link
 * dependency (`DT_NEEDED`) on libiov2.so, so the loader guarantees:
 *   - Initialization: libiov2.so's initializers (the construction of these
 *     singletons) run before a dependent module's static constructors -- so a
 *     consumer static may use them in its *constructor*;
 *   - Termination: finalization is the reverse of initialization, so libiov2.so is
 *     finalized last -- so a consumer static may also use them in its *destructor*.
 * Moreover the singletons are **not destroyed** at finalization (as libstdc++ treats
 * `std::cout`): each `_*_init` destructor only runs the exit hook registered at
 * construction -- flush for the output streams, a no-op for the rest, see
 * `sing_temp::exit_hook`. The objects live in static buffers and, together with the
 * heap they own, stay reachable and are reclaimed by the OS, so this is not a leak;
 * nor does using the streams from another thread during exit become a use-after-free.
 *
 * @warning Consumer modules are pinned, so libiov2.so stays resident: in shared mode
 * every module that includes IOv2 headers marks itself non-unloadable at load time
 * through `pin_module_of` (see `common/iov2_export.h`), and a `dlclose` of it only drops
 * the reference count. Its dependency on libiov2.so therefore never goes away, and
 * libiov2.so is never unmapped while references to it are live. Still unsupported is a
 * consumer coupled to this library only at runtime with no loader-visible dependency
 * edge (e.g. `dlopen(RTLD_GLOBAL)` via symbol interposition): the initialization order is
 * then unguaranteed.
 * @endif
 */

static ori_facet_buf::init _ori_facet_buf_init;
IOV2_API ori_facet_buf&    s_ori_facet_buf = _ori_facet_buf_init.get();

static cout_t::init _cout_init;
IOV2_API cout_t&    cout = _cout_init.get();

static cerr_t::init _cerr_init;
IOV2_API cerr_t&    cerr = _cerr_init.get();

static clog_t::init _clog_init;
IOV2_API clog_t&    clog = _clog_init.get();

static wcout_t::init _wcout_init;
IOV2_API wcout_t&    wcout = _wcout_init.get();

static wcerr_t::init _wcerr_init;
IOV2_API wcerr_t&    wcerr = _wcerr_init.get();

static wclog_t::init _wclog_init;
IOV2_API wclog_t&    wclog = _wclog_init.get();

static cin_t::init _cin_init;
IOV2_API cin_t&    cin = _cin_init.get();

static wcin_t::init _wcin_init;
IOV2_API wcin_t&    wcin = _wcin_init.get();

/**
 * @lang{ZH}
 * tie 图的进程级全局锁：shared 模式下的唯一定义点。函数被 `IOV2_API` 导出（默认可见），
 * 故全进程只有本 TU 里这一个定义、其函数内静态量也只有一份——这正是并发 `tie()` 检测成环
 * 所依赖的“单一实例”。懒构造，无静态初始化顺序依赖。详见 stream_common_operators.h。
 * @endif
 * @lang{EN}
 * The process-wide tie-graph lock: its single definition point in shared mode. The function
 * is exported via `IOV2_API` (default visibility), so the whole process sees exactly this
 * one definition and thus one function-local static -- the single instance that concurrent
 * `tie()` cycle detection relies on. Lazily constructed, no static-init-order dependency.
 * See stream_common_operators.h.
 * @endif
 */
IOV2_API copyable_mutex<std::mutex>& tie_graph_mutex()
{
    static copyable_mutex<std::mutex> m;
    return m;
}

/**
 * @lang{ZH}
 * `copyable_mutex` 的 fork 记录与回调登记：shared 模式下的唯一定义点，理由同上——全进程
 * 只有一份代数，`.so` 与可执行文件里内联的加锁代码比较的是同一个值，回调也只登记一次。
 * @endif
 * @lang{EN}
 * The fork record of `copyable_mutex` and its handler registration: their single definition
 * point in shared mode, for the reason above -- one generation in the whole process, so the
 * lock code inlined into the `.so` and into the executable compares against the same value,
 * and the handler is registered once.
 * @endif
 */
namespace detail
{
IOV2_API fork_state& copyable_mutex_fork_state() noexcept
{
    static constinit fork_state s;
    return s;
}

IOV2_API void copyable_mutex_watch_fork() noexcept
{
#if defined(__unix__) || defined(__APPLE__)
    [[maybe_unused]] static const int registered =
        ::pthread_atfork(nullptr, nullptr, &copyable_mutex_on_fork_child);
#endif
}

#if defined(__unix__) || defined(__APPLE__)
IOV2_API void pin_module_of(const void* addr) noexcept
{
    ::Dl_info info;
#if defined(__GLIBC__)
    // glibc's dladdr names the main program by argv[0], which the caller of exec chose:
    // a dlopen of it searches, opens and reads that path (a FIFO blocks before main).
    // The link_map has the name the loader matches; the main program's is empty, and it
    // cannot be unloaded anyway.
    ::link_map* map = nullptr;
    if (::dladdr1(addr, &info, reinterpret_cast<void**>(&map), RTLD_DL_LINKMAP) == 0
        || map == nullptr || map->l_name == nullptr || map->l_name[0] == '\0')
        return;
    const char* name = map->l_name;
#else
    if (::dladdr(addr, &info) == 0 || info.dli_fname == nullptr)
        return;
    const char* name = info.dli_fname;
#endif
    // A failure leaves an error pending for the program's next dlerror(); drop it.
    if (::dlopen(name, RTLD_LAZY | RTLD_NOLOAD | RTLD_NODELETE) == nullptr)
        ::dlerror();
}
#endif
}
}
