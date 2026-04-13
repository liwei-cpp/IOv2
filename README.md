# IOv2: A Modern & Composable C++ I/O Framework

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
#include <io/fp_defs/char_and_str.h>

using namespace IOv2;

int main() {
    // 绑定到文件设备并创建流
    iostream stream(file_device<char>("hello.txt", file_open_flag::trunc));

    // 写入字符串
    stream << "Hello, IOv2 World!\n";
    return 0;
}
```

### 开发环境与测试
- **标准**：C++23 及以上。
- **编译器**：GCC 15+ 或 Clang 18+ (建议配合 libstdc++)。
- **依赖**：核心头文件库本身无需构建，但部分特性依赖 `zlib` 和 `Botan`。

**运行测试**
`test` 目录包含了所有的单元测试与验证程序。运行测试前，请确保系统中已安装必要的依赖（如 `botan-2` 和 `zlib` 开发包）。

```bash
cd test
make test        # 编译并运行所有测试
make -j8 test-io # 并行编译并运行 IO 模块测试
```
更多测试命令请参考 `test/README.md` 或在 `test` 目录下运行 `make help`。

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
#include <io/fp_defs/char_and_str.h>

using namespace IOv2;

int main() {
    // Bind to a file device and create a stream
    iostream stream(file_device<char>("hello.txt", file_open_flag::trunc));

    // Write a string
    stream << "Hello, IOv2 World!\n";
    return 0;
}
```

### Development Environment & Tests
- **Standard**: C++23 or later.
- **Compiler**: GCC 15+ or Clang 18+ (with libstdc++ recommended).
- **Dependencies**: The core library requires no build, but specific converters depend on `zlib` and `Botan`.

**Running Tests**
The `test` directory contains all unit tests. Ensure you have the necessary dependencies installed (e.g., `botan-2` and `zlib` development packages).

```bash
cd test
make test        # Build and run all tests
make -j8 test-io # Build in parallel and run the IO module tests
```
For more testing commands, refer to `test/README.md` or run `make help` in the `test` directory.
