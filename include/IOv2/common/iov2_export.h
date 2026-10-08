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
//  共享库发行：安装步骤会把它解开注释（将来迁到 CMake 后，改由
//  generate_export_header() 自动产出本文件并写好这行）。这样消费者只要
//  #include 就自动进入 shared 模式，无需在命令行上自己加 -DIOV2_SHARED，
//  从根上消除“漏宏 → 静默双实例”（方向 A）。构建 .so 时 iov2_objects.cpp 仍
//  自行 #define IOV2_SHARED/IOV2_EXPORTS；下面的 #ifndef 守卫保证两边不冲突。
// @lang{EN}
//  Header-only distribution (default): keep the line below commented out.
//  Shared-library distribution: the install step uncomments it (later, under
//  CMake, generate_export_header() emits this file with it enabled). Consumers
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
 *          用安装版头文件（`make install-shared` 会打开上面那个开关）就不会遇到；
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
 * @warning **共享库模式会钉住每个消费者模块**：在 ELF / Mach-O 上，共享模式下包含本头文件
 *          会给每个模块加一个 hidden 可见性的 inline 变量，其初始化调用 `detail::pin_module_of`，
 *          使该模块不可卸载：`dlclose` 只减引用计数。模块建出的 facet 会进入进程级缓存（标准流
 *          的 locale、`s_ori_facet_buf`），其 vtable、`type_info` 键与控制块都在该模块的映像里，
 *          模块一卸载，这些缓存就指向已 unmap 的内存。变量必须是 hidden：默认可见性的会跨模块
 *          合并，只钉住第一个模块。
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
 *          this problem (`make install-shared` turns the switch above on); when consuming
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
 * @warning **Shared-library mode pins every consumer module**: on ELF / Mach-O, including
 *          this header in shared mode adds one hidden-visibility inline variable per module,
 *          whose initializer calls `detail::pin_module_of`. That makes the module
 *          non-unloadable: `dlclose` only drops the reference count. Facets a module builds
 *          end up in process-wide caches (the standard streams' locales, `s_ori_facet_buf`)
 *          with their vtables, `type_info` keys and control blocks in that module's image;
 *          unloading it would leave those caches pointing into unmapped memory. The variable
 *          has to be hidden: a default-visibility one would be merged across modules and pin
 *          only the first.
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

#if defined(IOV2_SHARED) && !defined(IOV2_EXPORTS) && (defined(__unix__) || defined(__APPLE__))
namespace IOv2::detail
{
IOV2_API void pin_module_of(const void* addr) noexcept;   // defined in iov2_objects.cpp

struct __attribute__((visibility("hidden"))) module_pin
{
    module_pin() noexcept { pin_module_of(this); }
};

__attribute__((visibility("hidden"))) inline module_pin s_module_pin;
}
#endif
