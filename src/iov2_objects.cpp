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

#include <io/objects/in_impl.h>
#include <io/objects/out_impl.h>
#include <locale/ori_facet_buf.h>

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
 *     **析构函数**里也可安全使用它们(此刻单例仍存活)。
 * 因此当前"load 时构造 / fini 时析构"对正常链接的消费者两端都正确,无需额外处理。
 *
 * @warning 不支持 dlopen/dlclose 语义。上述保证依赖一条**静态链接依赖边**。若 libiov2.so
 * 在仍有引用存活时被显式 `dlclose` 卸载,这些单例会在该点被析构,之后再经导出引用
 * (或任何持有它们的对象)访问即为悬空引用 / UB;若消费者与本库仅在运行期耦合而无加载器
 * 可见的依赖边(如 `dlopen(RTLD_GLOBAL)` 靠符号插桩),顺序同样无保证。libiov2.so 被设计为
 * **常规链接期依赖的基础库**(全进程保持加载),而非被 dlopen/dlclose 的插件。若将来确需
 * 对 dlopen/dlclose 鲁棒,有两种办法:(a) 加载器层面——把 libiov2.so 链接时标记为不可卸载
 * (`-Wl,-z,nodelete`,或调用方 `dlopen(..., RTLD_NODELETE)`),这样 `dlclose` 只减引用计数、
 * **不 unmap**,单例活到进程真正退出,上述悬空即消除(析构仍在最终退出时运行);(b) 代码
 * 层面——改用 libstdc++ 的做法:共享库模式下**不析构**这些单例(存储为静态缓冲,退出由 OS 回收,
 * 仍可达故非泄漏),仅经钩子执行退出副作用(如流 flush)。基础库最省事的是 (a)。
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
 *     finalized last -- so a consumer static may also use them in its *destructor*
 *     (the singletons are still alive at that point).
 * The current "construct at load / destroy at fini" scheme is therefore correct on
 * both ends for a normally-linked consumer, with nothing extra required.
 *
 * @warning dlopen/dlclose is NOT supported. The guarantee above relies on a static
 * link-dependency edge. If libiov2.so is explicitly `dlclose`d (unloaded) while
 * references are still live, these singletons are destroyed at that point, and any
 * later use of the exported references (or of anything holding them) is a dangling
 * reference / UB. Likewise, if a consumer is coupled to this library only at runtime
 * with no loader-visible dependency edge (e.g. `dlopen(RTLD_GLOBAL)` via symbol
 * interposition), the ordering is unguaranteed. libiov2.so is designed to be a normal
 * link-time dependency -- a base library that stays loaded for the whole process --
 * not a dlopen/dlclose'd plugin. If dlopen/dlclose robustness is ever required, adopt
 * one of two things: (a) at the loader level, mark libiov2.so non-unloadable at link
 * time (`-Wl,-z,nodelete`, or have callers pass `dlopen(..., RTLD_NODELETE)`), so that
 * `dlclose` only drops the reference count and never unmaps -- the singletons then live
 * until real process exit and the dangling reference above disappears (destructors still
 * run, but only at final exit); or (b) at the code level, adopt the libstdc++ approach:
 * in shared-library mode do NOT destroy these singletons (their storage is a static
 * buffer, reclaimed by the OS at exit and still reachable, hence not a leak) and run
 * exit side effects (e.g. stream flush) via a hook instead. For a base library, (a) is
 * the simplest.
 * @endif
 */

static ori_facet_buf::init _ori_facet_buf_init;
IOV2_API ori_facet_buf&    s_ori_facet_buf = *ori_facet_buf::ptr();

static __cout::init _cout_init;
IOV2_API __cout&    cout = *__cout::ptr();

static __cerr::init _cerr_init;
IOV2_API __cerr&    cerr = *__cerr::ptr();

static __clog::init _clog_init;
IOV2_API __clog&    clog = *__clog::ptr();

static __wcout::init _wcout_init;
IOV2_API __wcout&    wcout = *__wcout::ptr();

static __wcerr::init _wcerr_init;
IOV2_API __wcerr&    wcerr = *__wcerr::ptr();

static __wclog::init _wclog_init;
IOV2_API __wclog&    wclog = *__wclog::ptr();

static __cin::init _cin_init;
IOV2_API __cin&    cin = *__cin::ptr();

static __wcin::init _wcin_init;
IOV2_API __wcin&    wcin = *__wcin::ptr();

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
IOV2_API std::mutex& tie_graph_mutex()
{
    static std::mutex m;
    return m;
}
}
