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

#include <locale/ori_facet_buf.h>
#include <io/objects/out_impl.h>
#include <io/objects/in_impl.h>

namespace IOv2
{
// NOTE: definition order is initialization order here (single TU, ordered init).
// Keep s_ori_facet_buf first, and cout/wcout ahead of the streams that tie to them.
// The `_*_init` guards are TU-local (internal linkage); only the references are exported.

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
}
