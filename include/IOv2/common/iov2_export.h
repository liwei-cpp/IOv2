// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file iov2_export.h
 * @lang{ZH}
 * 单例符号的链接/可见性宏 `IOV2_API`，用于在两种发布模式之间切换：
 * header-only（默认）与单独编译的共享库（DSO/DLL）。
 * @endif
 * @lang{EN}
 * Linkage/visibility macro `IOV2_API` for singleton symbols. Selects between the
 * two release modes: header-only (the default) and a separately-compiled shared
 * library (DSO/DLL).
 * @endif
 */

#pragma once

// ─── 发行模式开关 / Distribution-mode switch ─────────────────────────────────
// @lang{ZH}
//  header-only 发行（默认）：保持下面这行注释。
//  共享库发行：安装步骤会把它解开注释（根目录 CMake 工程以 IOV2_BUILD_SHARED=ON
//  配置时，安装的是解开这行的副本）。这样消费者只要
//  #include 就自动进入 shared 模式，无需在命令行上自己加 -DIOV2_SHARED，
//  从根上消除“漏宏 → 静默双实例”（方向 A）。构建 .so 时 iov2_objects.cpp 仍
//  自行 #define IOV2_SHARED/IOV2_EXPORTS；下面的 #ifndef 守卫保证两边不冲突。
// @lang{EN}
//  Header-only distribution (default): keep the line below commented out.
//  Shared-library distribution: the install step uncomments it (the root CMake
//  project configured with IOV2_BUILD_SHARED=ON installs a copy with this line
//  enabled). Consumers
//  then get shared mode just by #include -- no need to pass -DIOV2_SHARED, which
//  removes the "forgot the macro -> silent duplicate instance" footgun. When
//  building the .so, iov2_objects.cpp still defines IOV2_SHARED/IOV2_EXPORTS
//  itself; the #ifndef guard below keeps the two from clashing.
#ifndef IOV2_SHARED
// #define IOV2_SHARED 1
#endif

/**
 * @lang{ZH}
 * 用法：
 *  - 默认（IOV2_SHARED、IOV2_EXPORTS 都不定义）：header-only。单例以 inline 变量
 *    在头文件中定义，`IOV2_API` 展开为空。
 *  - 消费共享库：定义 `IOV2_SHARED`。头文件只对单例做 `extern` 声明，定义来自库；
 *    Windows 上 `IOV2_API` 展开为 `dllimport`，ELF/Mach-O 上为 default 可见性。
 *  - 构建共享库：同时定义 `IOV2_SHARED` 与 `IOV2_EXPORTS`（仅在 iov2_objects.cpp）。
 *    Windows 上 `IOV2_API` 展开为 `dllexport`。
 *
 * `IOV2_EXPORTS` 仅在 Windows 上有意义——它区分“我正在构建这个库”（导出）与
 * “我在使用这个库”（导入）。ELF/Mach-O 两侧都是 default 可见性，与它无关。
 *
 * @warning `IOV2_SHARED` 必须在同一次链接的所有翻译单元中“要么都定义、要么都不
 *          定义”。混用会导致一边 inline 定义、一边 extern 声明，属于 ODR 违规。
 *
 * @warning **漏掉 `IOV2_SHARED` 却链接了 `libiov2.so`：能编译、能链接、通常也能跑，
 *          但进程里有两套单例**——头文件在可执行文件里定义了那些 inline 单例，.so
 *          对它们的引用被绑到那一份，随后可执行文件自己的初始化再构造一套覆盖掉。
 *          后果是两张 tie 图、两份 facet 缓存、退出时重复刷出，而链接期没有任何诊断。
 *          用安装版头文件（以 `IOV2_BUILD_SHARED=ON` 安装时会打开上面那个开关）就不会遇到；
 *          直接用仓库内的头文件时请自行核对：
 *          @code
 *            # 链接了 libiov2.so 的二进制里不应有自己定义的 IOv2 单例符号
 *            readelf -d prog | grep -q libiov2.so && comm -23 \
 *              <(nm -D --defined-only prog | awk '$3 ~ /^_ZN4IOv2/ && $2 ~ /^[BDuV]$/ {print $3}' | sort) \
 *              <(readelf -rW prog | awk '$3 ~ /_COPY$/ {print $5}' | sort)
 *          @endcode
 *          有输出即漏了宏：正确的消费者不定义单例符号，或只把它带成 COPY 重定位；漏宏的
 *          消费者自己定义它们，且没有 COPY 重定位。只看 `nm` 的字母不够：gcc 编出的显示
 *          `u`（STB_GNU_UNIQUE），clang 编出的显示 `V`（弱对象），g++ `-flto` 编出的却显示
 *          `B`，与 COPY 重定位那一份同字母。配方用到进程替换，需在 bash 下运行。
 *          以 `-fvisibility=hidden` 编译的漏宏程序不导出单例符号，配方没有输出，但进程里
 *          同样是两套单例（只是互不插桩），同属不支持的用法。
 *
 * @warning **不支持 header-only 与共享库模式混用**：进程里有多个模块用到 IOv2 时，每个模块
 *          都必须使用共享库模式（见 README「使用方式」）。「header-only 可执行文件 + 链接
 *          `libiov2.so` 的插件」也在此列：进程里会有两套单例，各管各的；宿主若再以
 *          `-rdynamic` / `--export-dynamic` 导出 IOv2 符号，`libiov2.so` 对单例的引用会被
 *          宿主那一份插桩，.so 的初始化写进宿主的槽位，实测在 .so 初始化期间即崩溃。
 *
 * @warning **Windows 上每个模块都要用动态 CRT，且配置与库一致**：库与所有消费者都用 `/MD`，
 *          Debug 配置用 `/MDd`；Debug 的消费者链接 Debug 版的 `iov2d.lib`（运行时载入 `iov2d.dll`），
 *          其余配置链接 `iov2.lib`（`iov2.dll`）。静态 CRT 下每个模块各有一份堆与 `stdout`，跨模块
 *          释放内存会崩，`sync_with_stdio` 也失去意义；Debug 与 Release 的堆不同，STL 的布局也不同。
 *          两种错误都在构建时报出：以 `/MT` 或 `/MTd` 在共享模式下包含本头文件，编译报错；配置与
 *          导入库不一致，链接时找不到 `detail::pin_module_of_md`（或 `_mdd`）——钉住函数按 CRT 命名，
 *          每种配置的库只导出自己那个。
 *
 * @warning **想隐藏符号的消费者模块，请用 `-fvisibility-ms-compat`，不要用 `-fvisibility=hidden`**
 *          （Windows 以外）：前者隐藏函数与变量，类型保持可见，库本身也是这样编译的。facet 以
 *          `type_info` 为键，转换器的调整参数经 `dynamic_cast` 识别；libc++ 对它认为只属于一个
 *          模块的类型（clang 在 macOS arm64 上把非默认可见性的类型都算在内）只按地址比较
 *          `type_info`，而每个模块各有一份，于是库与该模块之间的 facet 查找、`dynamic_cast`
 *          全部失败。macOS 上这是必需的；libstdc++ 按名字比较，Linux 上不受影响。
 *
 * @warning **共享库模式会钉住每个消费者模块**：共享模式下包含本头文件会给每个模块加一个 inline
 *          变量，其初始化把该模块钉住，使它不可卸载：ELF / Mach-O 上经
 *          `dlopen(RTLD_NOLOAD | RTLD_NODELETE)`，之后 `dlclose` 只减引用计数；Windows 上经
 *          `GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN)`，之后 `FreeLibrary` 不会卸载它。模块
 *          建出的 facet 会进入进程级缓存（标准流的 locale、`s_ori_facet_buf`），其 vtable、`type_info`
 *          键与控制块都在该模块的映像里，模块一卸载，这些缓存就指向已 unmap 的内存。ELF / Mach-O
 *          上变量必须是 hidden：默认可见性的会跨模块合并，只钉住第一个模块；Windows 上 DLL 之间本来
 *          就不合并。
 * @endif
 *
 * @lang{EN}
 * Usage:
 *  - Default (neither IOV2_SHARED nor IOV2_EXPORTS): header-only. Singletons are
 *    inline variables defined in the headers; `IOV2_API` expands to nothing.
 *  - Consuming the shared lib: define `IOV2_SHARED`. Headers only `extern`-declare
 *    the singletons; definitions come from the library. On Windows `IOV2_API`
 *    expands to `dllimport`, on ELF/Mach-O to default visibility.
 *  - Building the shared lib: define `IOV2_SHARED` and `IOV2_EXPORTS` (only in
 *    iov2_objects.cpp). On Windows `IOV2_API` expands to `dllexport`.
 *
 * `IOV2_EXPORTS` matters only on Windows: it distinguishes "I am building the
 * library" (export) from "I am using the library" (import). On ELF/Mach-O both
 * sides use default visibility, so it has no effect there.
 *
 * @warning `IOV2_SHARED` must be defined (or left undefined) consistently across
 *          every translation unit in a single link. Mixing the two would pair an
 *          inline definition with an extern declaration -- an ODR violation.
 *
 * @warning **Forgetting `IOV2_SHARED` while linking `libiov2.so` compiles, links and
 *          usually runs, but leaves two sets of singletons in the process**: the headers
 *          define the inline singletons in the executable, the .so's references bind to
 *          those, and the executable's own initialization then constructs a second set
 *          over the top. The result is two tie graphs, two facet caches and a duplicated
 *          flush at exit, with no diagnostic at link time. Installed headers do not have
 *          this problem (installing with `IOV2_BUILD_SHARED=ON` turns the switch above on); when consuming
 *          the headers straight from the source tree, check it yourself:
 *          @code
 *            # a binary linking libiov2.so must not define IOv2 singletons of its own
 *            readelf -d prog | grep -q libiov2.so && comm -23 \
 *              <(nm -D --defined-only prog | awk '$3 ~ /^_ZN4IOv2/ && $2 ~ /^[BDuV]$/ {print $3}' | sort) \
 *              <(readelf -rW prog | awk '$3 ~ /_COPY$/ {print $5}' | sort)
 *          @endcode
 *          Any output means the macro was missed: a correct consumer defines no singleton
 *          symbol, or carries it only as a COPY relocation, while one that missed the macro
 *          defines it itself with no COPY relocation. The `nm` letter alone is not enough:
 *          gcc shows `u` (STB_GNU_UNIQUE), clang `V` (a weak object), but g++ with `-flto`
 *          shows `B`, the same letter as a COPY relocation. The recipe uses process
 *          substitution, so run it under bash. A consumer built with `-fvisibility=hidden`
 *          that missed the macro exports no singleton symbol, so the recipe prints nothing,
 *          yet the process still has two sets of singletons (they just do not interpose
 *          each other) -- equally unsupported.
 *
 * @warning **Header-only and shared-library mode cannot be mixed**: when more than one
 *          module in the process uses IOv2, every module must use shared-library mode (see
 *          the README's usage modes). A header-only executable with plugins that link
 *          `libiov2.so` is no exception: the process has two sets of singletons, each
 *          minding its own; and if the host also exports IOv2 symbols (`-rdynamic` /
 *          `--export-dynamic`), `libiov2.so`'s references to the singletons are interposed
 *          by the host's copies, the .so's initialization writes into the host's slots,
 *          and it was measured to crash during that initialization.
 *
 * @warning **On Windows every module needs the dynamic CRT, in the library's configuration**:
 *          the library and all its consumers use `/MD`, or `/MDd` in the Debug configuration; a
 *          Debug consumer links the Debug `iov2d.lib` (and loads `iov2d.dll`), any other
 *          configuration links `iov2.lib` (`iov2.dll`). With the static CRT each module has a heap
 *          and a `stdout` of its own, freeing memory across modules crashes, and
 *          `sync_with_stdio` means nothing; Debug and Release differ in their heaps and in the
 *          STL's layout. Both mistakes are caught at build time: including this header in shared
 *          mode under `/MT` or `/MTd` is a compile error, and a configuration that does not match
 *          the import library fails to link on `detail::pin_module_of_md` (or `_mdd`) -- the pin
 *          function is named after the CRT, and each configuration of the library exports only
 *          its own.
 *
 * @warning **A consumer module that hides its symbols should use `-fvisibility-ms-compat`, not
 *          `-fvisibility=hidden`** (outside Windows): the former hides functions and variables but
 *          leaves types visible, and the library itself is built that way. Facets are keyed by
 *          `type_info`, and converters recognize their adjustments through `dynamic_cast`; libc++
 *          compares by address alone the `type_info` of a type it deems unique to one module
 *          (clang on macOS arm64 counts every type not of default visibility), each module has a
 *          copy of its own, and every facet lookup and `dynamic_cast` between the library and that
 *          module then fails. On macOS this is required; libstdc++ compares names, so Linux is
 *          unaffected.
 *
 * @warning **Shared-library mode pins every consumer module**: including this header in shared
 *          mode adds one inline variable per module, whose initializer pins the module, making
 *          it non-unloadable: on ELF / Mach-O through `dlopen(RTLD_NOLOAD | RTLD_NODELETE)`,
 *          after which `dlclose` only drops the reference count; on Windows through
 *          `GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN)`, after which `FreeLibrary` does
 *          not unload it. Facets a module builds end up in process-wide caches (the standard
 *          streams' locales, `s_ori_facet_buf`) with their vtables, `type_info` keys and control
 *          blocks in that module's image; unloading it would leave those caches pointing into
 *          unmapped memory. On ELF / Mach-O the variable has to be hidden: a default-visibility
 *          one would be merged across modules and pin only the first. Windows merges nothing
 *          across DLLs anyway.
 * @endif
 */
#if defined(IOV2_SHARED)
#  if defined(_WIN32)
#    if defined(IOV2_EXPORTS)
#      define IOV2_API __declspec(dllexport)
#    else
#      define IOV2_API __declspec(dllimport)
#    endif
#  else
#    define IOV2_API __attribute__((visibility("default")))
#  endif
#else
#  define IOV2_API
#endif

#if defined(IOV2_SHARED) && defined(_WIN32) && !defined(_DLL)
#  error "IOv2's shared-library mode needs the dynamic CRT in every module: /MD, or /MDd for Debug"
#endif

#if defined(IOV2_SHARED) && !defined(IOV2_EXPORTS)
namespace IOv2::detail
{
#if defined(_WIN32)
// Named after the CRT: a Debug module linked against the Release iov2.lib, or the other way
// round, fails to link on this name instead of crashing at run time.
#  if defined(_DEBUG)
IOV2_API void pin_module_of_mdd(const void* addr) noexcept;   // defined in iov2_objects.cpp, iov2d.dll only
#  else
IOV2_API void pin_module_of_md(const void* addr) noexcept;    // defined in iov2_objects.cpp, iov2.dll only
#  endif

// Not hidden as below: no inline variable is merged across DLLs, so each module has its own.
struct module_pin
{
#  if defined(_DEBUG)
    module_pin() noexcept { pin_module_of_mdd(this); }
#  else
    module_pin() noexcept { pin_module_of_md(this); }
#  endif
};

inline module_pin s_module_pin;
#elif defined(__unix__) || defined(__APPLE__)
IOV2_API void pin_module_of(const void* addr) noexcept;   // defined in iov2_objects.cpp

struct __attribute__((visibility("hidden"))) module_pin
{
    module_pin() noexcept { pin_module_of(this); }
};

__attribute__((visibility("hidden"))) inline module_pin s_module_pin;
#endif
}
#endif
