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
 *   - 文本域表最先——它不依赖任何东西，之后谁在初始化时经 `get_dirname` 读它都已就绪；
 *   - `s_ori_facet_buf` 必须先于各流——每个流的 `locale` 成员都经它解析；
 *   - `cout` / `wcout` 必须先于绑定到它们的流（`cerr`/`cin` tie `cout`，
 *     `wcerr`/`wcin` tie `wcout`）；
 *   - `detail::stdin_shared_imp` 必须先于 `cin` / `wcin`——它们的根转换器都经它读；
 *   - 时区树最后——它只依赖标准库的 tzdb，前面的单例初始化时也不用它。
 *
 * 构建示例（Linux，按需替换第三方依赖与路径；`-fvisibility-ms-compat` 隐藏函数与变量、
 * 类型保持可见，原因见 `common/iov2_export.h`）：
 * @code
 *   g++ -std=c++23 -fPIC -fvisibility-ms-compat -shared \
 *       -Iinclude -I/usr/include/botan-3 -D_POSIX_C_SOURCE=200809L \
 *       -DIOV2_SHARED -DIOV2_EXPORTS src/iov2_objects.cpp \
 *       -o libiov2.so -lz -lbotan-3
 *   # 或用根目录 CMake 工程 / or use the root CMake project:
 *   #   cmake -S . -B build/pkg-shared -DIOV2_BUILD_SHARED=ON && cmake --build build/pkg-shared
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
 *   - the text-domain table comes first -- it depends on nothing, and is then ready for
 *     any initialization that reads it through `get_dirname`;
 *   - `s_ori_facet_buf` must precede the streams -- every stream's `locale` member
 *     resolves through it;
 *   - `cout` / `wcout` must precede the streams that tie to them (`cerr`/`cin` tie
 *     `cout`, `wcerr`/`wcin` tie `wcout`);
 *   - `detail::stdin_shared_imp` must precede `cin` / `wcin` -- their root converters
 *     all read through it;
 *   - the time-zone trie comes last -- it depends only on the standard library's tzdb,
 *     and none of the singletons before it uses it while being initialized.
 *
 * Build example (Linux; substitute third-party deps/paths as needed;
 * `-fvisibility-ms-compat` hides functions and variables but not types, see
 * `common/iov2_export.h`):
 * @code
 *   g++ -std=c++23 -fPIC -fvisibility-ms-compat -shared \
 *       -Iinclude -I/usr/include/botan-3 -D_POSIX_C_SOURCE=200809L \
 *       -DIOV2_SHARED -DIOV2_EXPORTS src/iov2_objects.cpp \
 *       -o libiov2.so -lz -lbotan-3
 *   # 或用根目录 CMake 工程 / or use the root CMake project:
 *   #   cmake -S . -B build/pkg-shared -DIOV2_BUILD_SHARED=ON && cmake --build build/pkg-shared
 *   # Consumers: compile with -DIOV2_SHARED, link with -liov2
 * @endcode
 * @endif
 */

#define IOV2_SHARED
#define IOV2_EXPORTS
// dladdr, Dl_info, RTLD_NOLOAD and RTLD_NODELETE, which pin_module_of uses, are Darwin
// extensions that the _POSIX_C_SOURCE this library is built with hides. No IOv2 header
// depends on it. First, as <sys/cdefs.h> reads it once.
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE   // NOLINT(bugprone-reserved-identifier)
#endif

#include <IOv2/facet/messages.h>
#include <IOv2/facet/timeio.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/objects/in_impl.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/locale/ori_facet_buf.h>

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif
#if defined(__GLIBC__)
#include <link.h>
#endif

#if defined(_WIN32)
// Declared rather than taken from <windows.h>. HMODULE is spelled as <windows.h> spells it
// (STRICT), so the two declarations agree should they ever meet.
struct HINSTANCE__;   // NOLINT(bugprone-reserved-identifier): <windef.h>'s own tag
extern "C" __declspec(dllimport) int __stdcall
GetModuleHandleExW(unsigned long dwFlags, const wchar_t* lpModuleName, HINSTANCE__** phModule);
#endif

namespace IOv2
{
/**
 * @lang{ZH}
 * 文本域表与它的锁：shared 模式下的唯一定义点。二者以引用导出，所有模块——Windows 上的每个
 * DLL、ELF/Mach-O 上以 hidden 可见性编译的模块——内联的 `bind_text_domain` / `get_dirname`
 * 都经它们访问这一张表。放在本 TU 最前：它们不依赖任何东西，之后的初始化若读表，表已就绪。
 * 永不析构，理由见 messages_details.h。
 * @endif
 * @lang{EN}
 * The text-domain table and its lock: their single definition point in shared mode. Both are
 * exported as references, and the `bind_text_domain` / `get_dirname` inlined into every
 * module -- any DLL on Windows, a module built with hidden visibility on ELF/Mach-O -- reach
 * this one table through them. First in this TU: they depend on nothing, and are ready for
 * any initialization after them that reads the table. Never destroyed, see
 * messages_details.h.
 * @endif
 */
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
IOV2_API std::unordered_map<std::string, std::filesystem::path>& base_ft<messages>::s_domain_dirs =
    *new std::unordered_map<std::string, std::filesystem::path>;
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
IOV2_API copyable_mutex<std::mutex>& base_ft<messages>::s_domain_mutex =
    *new copyable_mutex<std::mutex>;

/**
 * @lang{ZH}
 * `xalloc()` 的计数器：shared 模式下的唯一定义点，理由同上——各模块内联的 `xalloc()` 改的都是
 * 这一个，发放的索引不会重复。常量初始化，与定义顺序无关。
 * @endif
 * @lang{EN}
 * The counter behind `xalloc()`: its single definition point in shared mode, for the reason
 * above -- the `xalloc()` inlined into every module bumps this one, so no index is handed out
 * twice. Constant-initialized, so its place in the definition order does not matter.
 * @endif
 */
IOV2_API constinit std::atomic<std::size_t> ios_base<void>::s_top{0};

/**
 * @lang{ZH}
 * 单例的生命周期与跨 DSO 顺序。
 *
 * 本 TU 内:定义顺序即初始化顺序(单 TU、ordered init,[basic.start.dynamic]/3.1)。
 * 因此 `s_ori_facet_buf` 必须先于各流——每个流的 `locale` 成员都经它解析;`cout`/`wcout`
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
 * Windows 上同理:DLL 的静态初始化在 `DllMain` 里进行,被依赖方先于依赖方;EXE 的静态初始化
 * 晚于它链接的 DLL。退出时 `exit()` 先执行 EXE 的静态析构,再由 `ExitProcess` 终止其余线程、
 * 按逆序给各 DLL 发 `DLL_PROCESS_DETACH`,iov2.dll 的退出钩子仍在消费者的静态析构之后。被终止
 * 的线程可能死在持锁状态,所以钩子只尽力刷新(`try_flush`)。
 * 而且这些单例在 finalize 时**不析构**(与 libstdc++ 对 `std::cout` 的处理相同):各
 * `_*_init` 的析构只执行构造时登记的退出钩子——输出流 flush、其余空操作,见
 * `sing_temp::exit_hook`。对象存储是静态缓冲,连同其持有的堆内存保持可达、由 OS 回收,
 * 不构成泄漏;退出阶段仍在其它线程中使用这些流也不会成为释放后使用。
 *
 * @warning 消费者模块被钉住,libiov2.so 随之驻留:共享模式下每个包含 IOv2 头文件的模块
 * 在载入时经 `pin_module_of` 把自己标记为不可卸载(见 `common/iov2_export.h`),对它
 * `dlclose` 只减引用计数(Windows 上 `FreeLibrary` 不会卸载它);它对 libiov2.so 的依赖也就一直存在,libiov2.so 不会在仍有
 * 引用存活时被 unmap。仍不支持的是消费者与本库仅在运行期耦合、无加载器可见依赖边的用法
 * (如 `dlopen(RTLD_GLOBAL)` 靠符号插桩):那时初始化顺序没有保证。
 * @endif
 *
 * @lang{EN}
 * Singleton lifetime and cross-DSO ordering.
 *
 * Within this TU, definition order is initialization order (single TU, ordered
 * init, [basic.start.dynamic]/3.1). Hence `s_ori_facet_buf` must precede the streams
 * -- every stream's `locale` member resolves through it -- and `cout`/`wcout` must
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
 * Windows behaves alike: a DLL's static initialization runs in `DllMain`, a dependency
 * before its dependents, and an EXE's after the DLLs it links. At exit, `exit()` runs the
 * EXE's static destructors first; `ExitProcess` then terminates the other threads and sends
 * `DLL_PROCESS_DETACH` to the DLLs in reverse order, so iov2.dll's exit hooks still run
 * after the consumers' static destructors. A terminated thread may have died holding a lock,
 * which is why the hooks only try to flush (`try_flush`).
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
 * the reference count (on Windows, `FreeLibrary` does not unload it). Its dependency on libiov2.so therefore never goes away, and
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

static detail::stdin_root_cvt_imp::init _stdin_imp_init;
IOV2_API detail::stdin_root_cvt_imp& detail::stdin_shared_imp = _stdin_imp_init.get();

static cin_t::init _cin_init;
IOV2_API cin_t&    cin = _cin_init.get();

static wcin_t::init _wcin_init;
IOV2_API wcin_t&    wcin = _wcin_init.get();

/**
 * @lang{ZH}
 * 时区树：shared 模式下的唯一定义点。在库载入时建一次——ELF 的 init 阶段、Windows 的
 * `DllMain`——而不是每个包含 timeio 的模块载入时各建一棵；消费者模块的静态构造都在它之后。
 * 仍在启动时建、不改成首次使用才建，理由见 timeio_details.h 的 `s_timezone_tree`。
 * @endif
 * @lang{EN}
 * The time-zone trie: its single definition point in shared mode. Built once, when the
 * library loads -- ELF's init phase, `DllMain` on Windows -- instead of once per module that
 * includes timeio; every consumer module's static constructors run after it. Still built at
 * startup rather than on first use, see `s_timezone_tree` in timeio_details.h.
 * @endif
 */
// Only bad_alloc can escape the build, at load time, where nothing could handle it anyway.
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
IOV2_API const prefix_tree<char, zone_ref>& base_ft<timeio>::s_timezone_tree =
    *new prefix_tree<char, zone_ref>(build_timezone_tree());

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
    // Never deleted on purpose: see stream_common_operators.h.
    static auto* const m = new copyable_mutex<std::mutex>(); // NOLINT(cppcoreguidelines-owning-memory)
    return *m;
}

/**
 * @lang{ZH}
 * 标准输入流共用的对象锁：shared 模式下的唯一定义点，理由同上——`cin` 与 `wcin` 无论在哪个
 * 模块里加锁，取到的都是这一把。永不析构，与 `cin`、`wcin` 一样。详见 in_impl.h。
 * @endif
 * @lang{EN}
 * The object lock the standard input streams share: its single definition point in shared
 * mode, for the reason above -- whichever module locks `cin` or `wcin` gets this one lock.
 * Never destroyed, like `cin` and `wcin`. See in_impl.h.
 * @endif
 */
IOV2_API copyable_mutex<std::recursive_mutex>& detail::stdin_io_mutex()
{
    // Never deleted on purpose: see above.
    static auto* const m = new copyable_mutex<std::recursive_mutex>(); // NOLINT(cppcoreguidelines-owning-memory)
    return *m;
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
#elif defined(_WIN32)
// Named after the CRT this library is built with, see common/iov2_export.h.
#if defined(_DEBUG)
IOV2_API void pin_module_of_mdd(const void* addr) noexcept
#else
IOV2_API void pin_module_of_md(const void* addr) noexcept
#endif
{
    constexpr unsigned long pin = 0x1;            // GET_MODULE_HANDLE_EX_FLAG_PIN
    constexpr unsigned long from_address = 0x4;   // GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
    // With FROM_ADDRESS the name is an address inside the module. A pinned module stays
    // loaded until the process ends, whatever FreeLibrary is called on it.
    ::HINSTANCE__* module = nullptr;
    (void)::GetModuleHandleExW(pin | from_address, static_cast<const wchar_t*>(addr), &module);
}
#endif
}
}
