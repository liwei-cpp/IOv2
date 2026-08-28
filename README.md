# IOv2

[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue)](https://liwei-cpp.github.io/IOv2/)
[![C++ CI](https://github.com/liwei-cpp/IOv2/actions/workflows/ci.yml/badge.svg)](https://github.com/liwei-cpp/IOv2/actions/workflows/ci.yml)
[![Generate Doxygen Documentation](https://github.com/liwei-cpp/IOv2/actions/workflows/docs.yml/badge.svg)](https://github.com/liwei-cpp/IOv2/actions/workflows/docs.yml)
[![codecov](https://codecov.io/gh/liwei-cpp/IOv2/graph/badge.svg)](https://codecov.io/gh/liwei-cpp/IOv2)

**A Modern & Composable C++ I/O Framework**

[中文](#中文) | [English](#english)

---

## 中文

### 项目简介
**IOv2** 是一个基于现代 C++ (C++20/23) 重新构建的 **Header-Only (仅头文件)** 输入输出系统框架。它的核心代码完全位于 `include` 目录中，旨在解决传统标准库 `iostream` 在扩展性、可组合性和性能抽象上的局限性。`test` 目录则包含了所有用于验证该库正确性的测试程序。

通过将 I/O 操作解耦为**物理访问 (Device)**、**表示变换 (Converter)** 以及**语义解释 (Facet/Locale)** 三个独立维度，IOv2 允许开发者通过简单的链式组合（Pipeline）来实现复杂的流处理逻辑。

### 核心特性
- **Header-Only 库**：直接引入 `include` 目录即可使用，无需复杂的编译链接过程。
- **高度可组合**：使用 `operator|` 像搭积木一样自由串联转换器。
- **基于 C++20 Concepts 的类型安全**：采用 Concepts 形式化描述原子能力（如读、写、定位），提供清晰的编译期约束与错误提示。
- **零开销抽象**：大量使用模板元编程与静态多态，确保架构灵活性不以牺牲性能为代价。
- **流级线程安全**：单次 I/O 操作在流这一层被整体串行化，详见下文“线程安全”。

### 架构与核心组件
IOv2 的系统架构分为以下几个正交的维度：

1. **设备层 (Device) - 物理访问**
   负责原始数据的物理传输。内置的设备包括：
   - `file_device`: 基于文件的输入输出。
   - `mem_device`: 基于内存缓冲区的输入输出。
   - `std_device`: 针对标准流 (stdin, stdout, stderr) 的包装。

2. **转换器层 (Converter) - 表示变换**
   负责数据的表示变换，支持无限链式串联。内置功能包括：
   - **编码转换**：如 `code_cvt`、`code_cvt_stdio`，支持多字节与宽字符等转换。
   - **压缩解压**：`zlib_cvt`（依赖 zlib）提供数据压缩与解压能力。
   - **密码学与哈希**：
     - `chacha20_cvt`: 提供现代、安全的流加密支持。
     - `hash_cvt`: 进行哈希计算（依赖 Botan）。
     - `vigenere_cvt`: **[仅限教育用途]** 位于 `IOv2::Crypt::Classic` 命名空间。维吉尼亚密码在现代密码学中已被完全破解，**禁止用于保护任何敏感数据**。
   - **运行时转换**：`runtime_cvt` 提供基于类型擦除的运行时的转换器多态支持。

3. **语义层 (Semantic) - 语义解释**
   重构了传统 C++ 中的 `facet` 与 `locale` 体系，为结构化数据提供文化规则支持：
   - 支持 `collate`, `ctype`, `messages`, `monetary`, `numeric`, `timeio` 等多维度的格式化和本地化处理。

4. **流对象与缓冲区 (Stream & Buffer)**
   提供最上层符合直觉的 `>>` 与 `<<` 操作符以及高效的流缓冲区 (Stream Buffer) 管理：
   - `istream`, `ostream`, `iostream` 提供了与标准库相似的块式 I/O 与格式化接口。
   - 预定义的标准流对象：`cin`, `cout`, `cerr`, `clog` 及其宽字符版本 (`wcin`, `wcout` 等)。

### 快速开始

```cpp
#include <io/iostream.h>
#include <device/file_device.h>
#include <io/traits/char_and_str.h>

using namespace IOv2;

int main() {
    // 绑定到文件设备并创建流
    iostream stream(file_device<char>("hello.txt", file_open_flag::trunc));

    // 写入字符串
    stream << "Hello, IOv2 World!\n";
    return 0;
}
```

### 扩展点：让自定义类型支持 `<<` / `>>`

全库只有**一个** I/O 扩展点：`IOv2::io_traits<TChar, T>`（声明在 `io/traits/traits_base.h`）。它的主模板**故意不定义**——"某个类型不支持某个方向"就表达为"该特化不存在"。内置类型的特化按类别分放在 `io/traits/` 下（`char_and_str.h`、`arithmetic.h`、`tm.h`、`nullptr.h` 等），它们**不会**被流头文件自动带上，用到哪个就 include 哪个。

给自己的类型接上 `os << t` / `is >> t`，只需在命名空间 `IOv2` 里特化它，并按需要提供下列成员。名字里的 `s` 是 **static** 的意思，与 sentry 无关。

```cpp
#include <io/traits/traits_base.h>

namespace IOv2
{
template <typename TChar>
struct io_traits<TChar, my_point>
{
    // 插入：加锁与哨兵由 operator<< 负责，这里只管把字符写进迭代器。
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter it, ios_base<TChar>& io, const locale<TChar>& loc,
                        const my_point& v);

    // 提取：同理，只管从 [it, end) 里解析。
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter it, TSent end, ios_base<TChar>& io, const locale<TChar>& loc,
                       my_point& v);
};
}
```

四条要点：

- **方向由"哪个成员存在"决定**：只有 `swrite` 即只能插入，只有 `sread` 即只能提取，两个都有即两个方向都行。这一点不能改用约束来表达——`iostream` 同时满足 `istream_type` 与 `ostream_type`。用反了，那一侧的运算符不满足约束、没有可行重载，因此编译不过。
- **出错就直接抛**，不要自己去动流的状态位：运算符会接住并交给 `handle_exception`，转成相应的状态位、并遵守流的异常掩码。本库自己抛的一律是 `stream_error`。
- 上面是**迭代器形式**，格式化 I/O 用这一档。操纵符用的是**流形式**——`static void swrite(T& s, const my_point& v)` / `sread`，直接拿到流本身，**不加锁、不建哨兵**，需要就自己来。两种形式靠参数个数区分，一个特化**只能提供其中一种**：两种都提供是编译错误。库内置的操纵符没有 `operator()`，逻辑全在扩展点里，`os << m` / `is >> m` 是仅有的入口；标准的 `std::ws(is)` / `std::endl(os)` 直接调用形式在本库不存在。
- **扩展只能走特化 `io_traits` 这一条路，不要在自己的命名空间里写 `operator<<` / `operator>>`。** 加锁、建哨兵、消费 `width` / `fill`、把异常转交 `handle_exception`——这些都是运算符的职责，手写一条就全部绕开了，而且在多线程下不再与其它插入互斥。特化扩展点则完全没有这个问题：它不参与重载决议，运算符照常负责上述全部工作。真要给某个类型两条路都留（既特化了 `io_traits`，又自己写了一条运算符），两条候选同样好，调用点就是二义性错误。

细节（含提取端可选的 `parse_context_type` 中转）见 `io/traits/traits_base.h` 的文件头注释。

### 从 `writer` / `reader` 迁移

上一版有三套并行的"把 I/O 逻辑挂到类型上"的机制：类模板 `writer` / `reader`、方向标签基类 `in_manip` / `out_manip`（配一对 `= delete` 重载）、以及每侧六条运算符重载。现在只剩一个扩展点 `io_traits`，每侧两条运算符（泛型的那条，加一条收 `ios_base` 函数指针操纵符的）。破坏面如下：

| 旧写法 | 新写法 |
| --- | --- |
| `#include <io/fp_defs/xxx.h>` | `#include <io/traits/xxx.h>`；`base_fp.h` → `traits_base.h` |
| `struct writer<TChar, T> { swrite(...); };` | `struct io_traits<TChar, T> { swrite(...); };`，`swrite` 签名不变 |
| `struct reader<TChar, T> { sread(...); };` | 同一个 `io_traits<TChar, T>` 里的 `sread`，签名不变 |
| 分别特化 `writer` 和 `reader` | 在**同一个** `io_traits` 特化里同时给出 `swrite` 与 `sread` |
| `is_writer_def<TChar, T>` / `is_reader_def<TChar, T>` | 已删除。问"能不能流"直接写 `requires { os << v; }` / `requires { is >> v; }` |
| 操纵符派生 `in_manip` / `out_manip` 并写 `operator()` | 特化 `io_traits`，写**流形式**的 `swrite(T& s, const M& m)` / `sread(T& s, const M& m)`；方向由哪个成员存在决定，两个标签基类和那对 `= delete` 重载一并消失 |
| `setw(5)(os)`、`ws(is)`、`endl(os)` 直接调用 | 没有了。只能写 `os << setw(5)`、`is >> ws`、`os << endl` |
| `os << std::function<void(ios_base<TChar>&)>{f}` | 没有了。操纵符只认**函数指针**一种形状；无捕获 lambda 写 `+lambda` 退化，带状态的操纵符请走流形式 |
| `parse_context_type` | 保留，从 `io/fp_defs/base_fp.h` 挪到 `io/traits/traits_base.h`，用法不变 |

另有两处行为变化：

- **异常只在运算符里接一次。** 操纵符不再有 `operator()`，也就不再各自 try/catch；`os << m` / `is >> m` 里抛出的一切统一由运算符转交 `handle_exception`。
- **用错类型时的报错措辞变了。** 旧的泛型运算符无约束，用错会撞上函数体里的 `static_assert`，能区分"根本没有 `io_traits`"和"有 `io_traits` 但这个方向没成员"。现在运算符带约束，用错类型就是**没有可行重载**，编译器给的是通用的 `no match for 'operator<<'`。这是换取可探测性必须付的代价：定制诊断需要无约束的运算符，而无约束的运算符没法被 `requires` 探测，二者不可兼得。**忘了 include 对应的 `io/traits/*.h` 现在也是这一条报错，遇到它先检查 include。**

### 使用方式：Header-Only 与共享库（DSO/DLL）

IOv2 提供两种发布模式，默认即开箱即用的 header-only：

- **Header-Only（默认）**：像快速开始那样直接 `#include` 即可，无需任何宏或链接库。预定义的标准流对象（`cin/cout/cerr/clog` 及宽字符版本）与本地化缓存等进程级单例，以 C++17 `inline` 变量的形式直接定义在头文件中，效果类似 `std::cout`——包含头文件就能用。

- **共享库（可选）**：当你把 IOv2 编译成独立的共享库（`.so`/`.dylib`/`.dll`），并希望上述进程级单例在“主程序 + 多个共享库”之间**保持唯一一份实例**时使用。此模式把这些单例的唯一定义集中到 `src/iov2_objects.cpp`（等价于 `std::cout` 定义在 libstdc++ 中的做法）。

**安装（根目录 `Makefile`）**

```bash
# header-only：仅安装头文件 + iov2.pc
make install PREFIX=/usr/local

# 共享库：构建 libiov2.so，安装头文件 + libiov2.so + iov2-shared.pc
make install-shared PREFIX=/opt/iov2
```

`install-shared` 会自动打开**已安装那份**头文件里的发行模式开关，因此**消费方无需手动传 `-DIOV2_SHARED`**——模式信息随头文件一起交付。底层构建命令见 `src/iov2_objects.cpp`。卸载用 `make uninstall PREFIX=...`。

**消费（推荐 pkg-config）**

安装会附带两份 pkg-config 文件：`iov2.pc`（header-only）与 `iov2-shared.pc`（共享库）。客户 Makefile 用包名切换模式，本身无需改动：

```make
IOV2_PKG ?= iov2          # 默认 header-only；改成 iov2-shared 即共享库

CXXFLAGS += -std=c++23 $(shell pkg-config --cflags $(IOV2_PKG))
LDLIBS   += $(shell pkg-config --libs   $(IOV2_PKG))

app: app.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)
```
```bash
make                        # header-only
make IOV2_PKG=iov2-shared   # 共享 .so（无需 -DIOV2_SHARED）
```

- **zlib / Botan 为可选依赖**：仅当使用压缩（`zlib_cvt`）或哈希（`hash_cvt`）转换器时才需要。两份 `.pc` 都**不**强制链接它们；需要时自行追加 `$(shell pkg-config --cflags --libs zlib botan-2)`。
- **每种模式安装到各自的 PREFIX**：模式开关焙在单个头文件里，一棵头树只能对应一种模式。安装到非标准前缀时 `export PKG_CONFIG_PATH=<prefix>/lib/pkgconfig`，可用 `pkg-config --modversion iov2` 验证。

**编译宏一览**

| 宏 | 何时定义 | 作用 |
|---|---|---|
| （无） | 默认 | header-only；单例为 `inline` 变量 |
| `IOV2_SHARED` | 构建库时；消费方由**已安装头文件自动带上**（通常无需手动定义） | 启用共享库模式：头文件只对单例做 `extern` 声明 |
| `IOV2_EXPORTS` | **仅** `iov2_objects.cpp` | 标记“正在构建库本体”；仅 Windows 用于区分 `dllexport`/`dllimport` |

> **注意**：`IOV2_SHARED` 必须在同一次链接的所有翻译单元中保持一致——把开关焙进已安装头文件正是为此提供保证。
>
> 在 ELF/macOS 且默认可见性下，header-only 模式跨多个共享库通常也会被动态链接器合并为单实例；只有在 `-fvisibility=hidden` 的多 `.so`，或 **Windows 多 DLL** 场景下才会每个模块各一份——此时改用共享库模式即可获得严格的单实例保证。

### 线程安全

线程安全在**流对象这一层**提供：每个流各自持有一把递归互斥量 `io_mutex()`，所有会触碰缓冲区或流状态的操作都在其保护下进行。

**库提供的保证**

- **单次操作是原子的**：`operator<<` / `operator>>` / `put` / `write` / `get` 等在其 sentry 的整个生命周期内持有 `io_mutex()`。多个线程同时对同一流做 I/O 时逐个进入，不会交错，也不会并发操作底层缓冲区。
- **不建 sentry 的操作同样加锁**：`tell` / `seek` / `rseek` / `detach` / `attach` / `adjust` / `retrieve` 以及 `locale(loc)` setter 都在流级锁下执行。
- **写与刷互斥、并发刷新被串行化**：`flush()` 亦经 sentry 加锁，且每个调用者都真正完成一次刷新，而非“先到者刷、其余跳过”。
- **tie 恒无环**：`tie()` setter 在进程级全局锁 `tie_graph_mutex()` 下把“成环检测”与“提交”合成一个原子步骤，因此即便两个线程并发 `A.tie(B)` 与 `B.tie(A)` 也无法成环；成环请求抛 `stream_error` 并保持原绑定不变。这比标准库把成环留作未定义行为更强。
- **状态位与其异常指针一致**：`clear`/`setstate`/`exceptions`/`handle_exception` 对「状态位 + 各失败类别所保存的 `exception_ptr`」的更新是一次完整事务，不会被并发撕开。读取（`rdstate`/`good`/`eof`/`operator bool`）为无锁原子读，无额外开销。标准库在这一层不提供任何保证——`std::basic_ios` 的状态字是裸 `int`，且 `[iostream.objects]` 的豁免只覆盖 formatted/unformatted I/O 函数，不含状态位访问器。

**不提供的保证（调用方责任）**

- **多次操作的组合不是原子的**：`os << a << b` 是两次各自加锁的操作，另一线程可能插入其间。同理 `while (is >> x)` 这类「先查状态再动作」：单次 `>>` 与单次状态读各自原子，但两者之间仍有窗口。需要整体原子时，用 `IOv2::sync`（RAII 锁住该流的 `io_mutex()`）把它们圈进同一个临界区。
- **返回内部引用的 getter**：`locale()` 与 `device()` 返回的是内部状态的引用，不要保存到临界区之外；跨多次操作使用时同样以 `IOv2::sync` 保护。
- **tie 目标的生命周期**：`tie()` 只保存裸指针，不做生命周期管理，也不会自动解绑（与 `std::basic_ios::tie` 一致）。
- **下层组件本身不是线程安全的**：device 与 converter 均按“并发由更高层处理”设计——并发保护正是由流层的 `io_mutex()` 统一提供（facet 各有各的契约，以其自身文档为准，例如 `ctype`、`messages` 明确声明线程安全）。因此两个**各自独立**的流对象若指向同一底层资源（如同一个 fd），彼此之间并不同步。

### 开发环境与测试
- **标准**：C++23 及以上。
- **编译器**：GCC 15+ 或 Clang 18+ (建议配合 libstdc++)。
- **依赖**：核心头文件库本身无需构建，但部分特性依赖 `zlib` 和 `Botan`。

**运行测试**
`test` 目录包含了所有的单元测试与验证程序，由 CMake/CTest 构建，共 57 个套件。运行测试前，请确保系统中已安装必要的依赖（如 `botan-2` 和 `zlib` 开发包）。

```bash
cmake --preset gcc-release                        # 配置
cmake --build --preset gcc-release --parallel     # 编译全部
ctest --preset gcc-release --parallel             # 运行全部 57 个套件
ctest --preset gcc-release -L io                  # 只跑 IO 模块
```
构建产物位于 `build/<preset>/`，不再污染源码树。更多用法（sanitizer、Valgrind、覆盖率、如何新增测试文件）参见 `test/README.md`，可用配置见 `cmake --list-presets`。

---

## English

### Introduction
**IOv2** is a **Header-Only** reimagined Input/Output framework built from the ground up using Modern C++ (C++20/23). Its core library code resides entirely within the `include` directory, aiming to address the extensibility, composability, and performance abstraction limits of the traditional `iostream` library. The `test` directory contains programs dedicated to testing and validating the library.

By decoupling I/O into three orthogonal dimensions—**Physical Access (Device)**, **Representation Transformation (Converter)**, and **Semantic Interpretation (Facet/Locale)**—IOv2 enables developers to build complex processing pipelines through simple composition.

### Key Features
- **Header-Only Library**: Simply include the files from the `include` directory. No complex build or linking phase for the core library.
- **Highly Composable**: Use `operator|` to chain converters like building blocks.
- **Type-Safe with C++20 Concepts**: Formalizes atomic capabilities (e.g., Read, Write, Seek) using Concepts for robust compile-time validation.
- **Zero-Overhead Abstraction**: Powered by template metaprogramming and static polymorphism to ensure maximum flexibility without runtime penalties.
- **Stream-Level Thread Safety**: A single I/O operation is serialized as a whole at the stream layer — see "Thread Safety" below.

### Architecture & Core Components
The architecture is divided into the following orthogonal dimensions:

1. **Device Layer (Physical Access)**
   Handles raw data transmission. Built-in devices include:
   - `file_device`: File-based I/O.
   - `mem_device`: Memory buffer-based I/O.
   - `std_device`: Wrappers for standard streams (stdin, stdout, stderr).

2. **Converter Layer (Representation Transformation)**
   Manages intermediate transformations with support for infinite chaining. Built-in converters include:
   - **Encodings**: e.g., `code_cvt`, `code_cvt_stdio` for multibyte/wide-character conversions.
   - **Compression**: `zlib_cvt` (requires zlib) for data compression and decompression.
   - **Cryptography & Hashing**: `chacha20_cvt` and `vigenere_cvt` for encryption/decryption, and `hash_cvt` for hashing (requires Botan).
   - **Runtime Conversion**: `runtime_cvt` provides polymorphic converter dispatch at runtime via type erasure.

3. **Semantic Layer (Semantic Interpretation)**
   A reconstructed `facet` and `locale` system that makes cultural rule handling flexible:
   - Supports `collate`, `ctype`, `messages`, `monetary`, `numeric`, and `timeio` formatting and localization.

4. **Stream Objects & Buffers**
   High-level interfaces providing intuitive `>>` and `<<` operators and efficient stream buffer management:
   - `istream`, `ostream`, and `iostream` for block I/O and formatted interactions.
   - Pre-defined standard stream objects like `cin`, `cout`, `cerr`, `clog` and their wide-character counterparts.

### Quick Start

```cpp
#include <io/iostream.h>
#include <device/file_device.h>
#include <io/traits/char_and_str.h>

using namespace IOv2;

int main() {
    // Bind to a file device and create a stream
    iostream stream(file_device<char>("hello.txt", file_open_flag::trunc));

    // Write a string
    stream << "Hello, IOv2 World!\n";
    return 0;
}
```

### Extension Point: Making Your Own Types Work with `<<` / `>>`

The whole library has exactly **one** I/O extension point: `IOv2::io_traits<TChar, T>` (declared in `io/traits/traits_base.h`). Its primary template is **deliberately left undefined** -- "this type does not support this direction" is expressed as "the specialization does not exist". Specializations for the built-in types live under `io/traits/` grouped by category (`char_and_str.h`, `arithmetic.h`, `tm.h`, `nullptr.h`, ...); the stream headers do **not** pull them in, so include the ones you use.

To make `os << t` / `is >> t` work for your own type, specialize it in namespace `IOv2` and provide whichever members you need. The `s` in the names means **static** and has nothing to do with the sentry.

```cpp
#include <io/traits/traits_base.h>

namespace IOv2
{
template <typename TChar>
struct io_traits<TChar, my_point>
{
    // Insertion: operator<< owns the lock and the sentry; this only writes characters
    // through the iterator.
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter it, ios_base<TChar>& io, const locale<TChar>& loc,
                        const my_point& v);

    // Extraction: likewise, this only parses out of [it, end).
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter it, TSent end, ios_base<TChar>& io, const locale<TChar>& loc,
                       my_point& v);
};
}
```

Four things to know:

- **The direction is decided by which member exists**: `swrite` only means insertion only, `sread` only means extraction only, and both means both. Constraints cannot express this -- an `iostream` satisfies `istream_type` and `ostream_type` alike. Using one backwards leaves the operator on that side unsatisfied, so there is no viable overload and it does not compile.
- **Just throw on error**; do not touch the stream's state bits yourself. The operator catches and hands the exception to `handle_exception`, which turns it into the matching state bit and honours the stream's exception mask. Everything this library throws itself is a `stream_error`.
- The above is the **iterator form**, used by formatted I/O. Manipulators use the **stream form** instead -- `static void swrite(T& s, const my_point& v)` / `sread`, which get the stream itself with **no lock and no sentry**; do it yourself if you need one. The two forms are told apart by arity, and a specialization may provide **only one of them**: providing both is a compile error. The library's own manipulators have no `operator()`: all their logic lives in the extension point and `os << m` / `is >> m` is the only entry, so the standard's direct-call forms `std::ws(is)` and `std::endl(os)` do not exist here.
- **Specializing `io_traits` is the only supported way to extend; do not write your own `operator<<` / `operator>>` in your namespace.** Taking the lock, building the sentry, consuming `width` / `fill`, and routing exceptions to `handle_exception` are the operator's job, and a hand-written one bypasses all of it -- including mutual exclusion with other insertions under threads. The extension point has none of that risk: it does not take part in overload resolution, and the operator still does all of the above. And if you do both for one type -- specialize `io_traits` *and* write your own operator -- the two candidates are equally good and the call site is an ambiguity error.

For the details -- including the optional `parse_context_type` relay on the extraction side -- see the file-level comment in `io/traits/traits_base.h`.

### Migrating from `writer` / `reader`

The previous version had three parallel mechanisms for attaching I/O logic to a type: the `writer` / `reader` class templates, the `in_manip` / `out_manip` direction tag bases (with a matching pair of `= delete` overloads), and six operator overloads per side. All of it is now one extension point, `io_traits`, and two operators per side (the generic one, plus one that takes an `ios_base` function-pointer manipulator). The break surface:

| Old | New |
| --- | --- |
| `#include <io/fp_defs/xxx.h>` | `#include <io/traits/xxx.h>`; `base_fp.h` → `traits_base.h` |
| `struct writer<TChar, T> { swrite(...); };` | `struct io_traits<TChar, T> { swrite(...); };` -- `swrite`'s signature is unchanged |
| `struct reader<TChar, T> { sread(...); };` | `sread` in that same `io_traits<TChar, T>`, signature unchanged |
| Specializing `writer` and `reader` separately | Give `swrite` and `sread` in **one** `io_traits` specialization |
| `is_writer_def<TChar, T>` / `is_reader_def<TChar, T>` | Removed. To ask whether a type is streamable, write `requires { os << v; }` / `requires { is >> v; }` |
| A manipulator deriving from `in_manip` / `out_manip` with an `operator()` | Specialize `io_traits` with the **stream form** `swrite(T& s, const M& m)` / `sread(T& s, const M& m)`; the direction comes from which member exists, and both tag bases and the `= delete` pair are gone with them |
| Direct calls `setw(5)(os)`, `ws(is)`, `endl(os)` | Gone. Write `os << setw(5)`, `is >> ws`, `os << endl` |
| `os << std::function<void(ios_base<TChar>&)>{f}` | Gone. A function pointer is the only manipulator shape accepted; a capture-less lambda can be written `+lambda` to decay, and a stateful manipulator belongs in the stream form |
| `parse_context_type` | Kept, moved from `io/fp_defs/base_fp.h` to `io/traits/traits_base.h`; usage unchanged |

Two behavioural changes come with it:

- **Exceptions are caught once, in the operator.** Manipulators no longer have an `operator()` and so no longer try/catch individually; everything thrown out of `os << m` / `is >> m` is handed to `handle_exception` by the operator.
- **The wording you get for a wrong type changed.** The old generic operators were unconstrained, so a wrong type hit a `static_assert` in the body that distinguished "no `io_traits` at all" from "an `io_traits` exists but has no member for this direction". The operators are constrained now, so a wrong type simply leaves **no viable overload** and the compiler emits its generic `no match for 'operator<<'`. That is the price of detectability: a tailored diagnostic needs an unconstrained operator, and an unconstrained operator cannot be probed with `requires` -- you cannot have both. **A forgotten `io/traits/*.h` include now produces this same error, so check your includes first when you see it.**

### Usage Modes: Header-Only vs Shared Library (DSO/DLL)

IOv2 ships in two modes; the default is header-only and works out of the box:

- **Header-Only (default)**: just `#include` as shown in Quick Start — no macros, no library to link. The pre-defined standard streams (`cin/cout/cerr/clog` and their wide counterparts) and the localization cache — all process-wide singletons — are defined directly in the headers as C++17 `inline` variables, much like `std::cout`: include the header and use them.

- **Shared Library (optional)**: use this when you compile IOv2 into a standalone shared library (`.so`/`.dylib`/`.dll`) and want those process-wide singletons to remain a **single instance** shared across the main program and multiple shared libraries. This mode concentrates the one definition of each singleton in `src/iov2_objects.cpp` (the same approach `std::cout` uses inside libstdc++).

**Install (root `Makefile`)**

```bash
# header-only: install headers + iov2.pc only
make install PREFIX=/usr/local

# shared library: build libiov2.so, install headers + libiov2.so + iov2-shared.pc
make install-shared PREFIX=/opt/iov2
```

`install-shared` turns on the distribution switch in the *installed* copy of the headers, so **consumers do not pass `-DIOV2_SHARED` by hand** — the mode ships with the headers. The underlying build command lives in `src/iov2_objects.cpp`. Uninstall with `make uninstall PREFIX=...`.

**Consume (pkg-config recommended)**

The install ships two pkg-config files: `iov2.pc` (header-only) and `iov2-shared.pc` (shared). A consumer Makefile switches modes by package name, with no other change:

```make
IOV2_PKG ?= iov2          # header-only by default; set to iov2-shared for the DSO

CXXFLAGS += -std=c++23 $(shell pkg-config --cflags $(IOV2_PKG))
LDLIBS   += $(shell pkg-config --libs   $(IOV2_PKG))

app: app.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)
```
```bash
make                        # header-only
make IOV2_PKG=iov2-shared   # shared .so (no -DIOV2_SHARED needed)
```

- **zlib / Botan are optional**: needed only if you use the compression (`zlib_cvt`) or hashing (`hash_cvt`) converters. Neither `.pc` links them; add `$(shell pkg-config --cflags --libs zlib botan-2)` yourself when you do.
- **Install each mode into its own PREFIX**: the mode switch is baked into a single header, so one header tree corresponds to exactly one mode. For a non-standard prefix, `export PKG_CONFIG_PATH=<prefix>/lib/pkgconfig` and verify with `pkg-config --modversion iov2`.

**Compile-time macros**

| Macro | When defined | Effect |
|---|---|---|
| (none) | default | header-only; singletons are `inline` variables |
| `IOV2_SHARED` | when building the library; consumers get it **automatically from the installed header** (rarely set by hand) | enables shared mode: headers only `extern`-declare the singletons |
| `IOV2_EXPORTS` | **only** in `iov2_objects.cpp` | marks "building the library itself"; used on Windows to pick `dllexport`/`dllimport` |

> **Note**: `IOV2_SHARED` must be consistent across every translation unit in a single link — baking the switch into the installed header is exactly what guarantees that.
>
> On ELF/macOS with default visibility, header-only mode is usually merged into a single instance across multiple shared libraries by the dynamic linker anyway; only under `-fvisibility=hidden` with multiple `.so`s, or **multiple Windows DLLs**, do you get one instance per module — switch to shared-library mode there for a strict single-instance guarantee.

### Thread Safety

Thread safety is provided **at the stream-object layer**: every stream owns a recursive mutex, `io_mutex()`, and every operation that touches the buffer or the stream state runs under it.

**What the library guarantees**

- **A single operation is atomic**: `operator<<` / `operator>>` / `put` / `write` / `get` hold `io_mutex()` for their sentry's entire lifetime. Threads doing I/O on the same stream enter one at a time — operations never interleave and the underlying buffer is never operated on concurrently.
- **Sentry-less operations lock too**: `tell` / `seek` / `rseek` / `detach` / `attach` / `adjust` / `retrieve` and the `locale(loc)` setter all run under the stream lock.
- **Write and flush are mutually excluded; concurrent flushes are serialized**: `flush()` also locks via the sentry, and every caller actually completes a flush rather than the weak "first caller flushes, the rest skip" semantics.
- **The tie graph is always acyclic**: the `tie()` setter fuses cycle detection and commit into one atomic step under the process-wide `tie_graph_mutex()`, so no cycle can form even under concurrent `A.tie(B)` / `B.tie(A)`. A cycling request throws `stream_error` and leaves the existing tie unchanged — stronger than the standard, which leaves cycles undefined.
- **State bits stay consistent with their exception pointers**: `clear`/`setstate`/`exceptions`/`handle_exception` update the state bits together with the `exception_ptr` saved per failure category as one complete transaction, never torn apart by concurrency. Reads (`rdstate`/`good`/`eof`/`operator bool`) are lock-free atomic loads and cost nothing extra. The standard library guarantees nothing here — `std::basic_ios` keeps its state in a plain `int`, and the `[iostream.objects]` exemption covers only formatted/unformatted I/O functions, not the state accessors.

**What it does not guarantee (caller's responsibility)**

- **A sequence of operations is not atomic**: `os << a << b` is two separately-locked operations and another thread may interleave between them. The same applies to check-then-act patterns like `while (is >> x)`: the `>>` and the state read are each atomic, but there is still a window between them. To make a group atomic, wrap it in one critical section with `IOv2::sync` (an RAII lock on that stream's `io_mutex()`).
- **Getters returning internal references**: `locale()` and `device()` hand back references to internal state — do not keep them past the critical section; guard cross-operation use with `IOv2::sync` as well.
- **Tie-target lifetime**: `tie()` stores a raw pointer only; it performs no lifetime management and never auto-unties (matching `std::basic_ios::tie`).
- **Lower-layer components are not thread-safe themselves**: devices and converters are designed as "concurrency is handled at a higher level" — that protection is exactly what the stream layer's `io_mutex()` provides. (Facets each state their own contract; `ctype` and `messages`, for instance, are explicitly thread-safe.) Consequently two **separate** stream objects over the same underlying resource (e.g. the same fd) are not synchronized with each other.

### Development Environment & Tests
- **Standard**: C++23 or later.
- **Compiler**: GCC 15+ or Clang 18+ (with libstdc++ recommended).
- **Dependencies**: The core library requires no build, but specific converters depend on `zlib` and `Botan`.

**Running Tests**
The `test` directory contains all unit tests, built by CMake/CTest as 57 suites. Ensure you have the necessary dependencies installed (e.g., `botan-2` and `zlib` development packages).

```bash
cmake --preset gcc-release                        # configure
cmake --build --preset gcc-release --parallel     # build everything
ctest --preset gcc-release --parallel             # run all 57 suites
ctest --preset gcc-release -L io                  # run the IO module only
```
Build output goes to `build/<preset>/` rather than into the source tree. For sanitizers, Valgrind, coverage and how to add a test file, see `test/README.md`; `cmake --list-presets` lists the available configurations.
