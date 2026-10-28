# Changelog

[中文](#中文) | [English](#english)

---

## 中文

本文件记录 IOv2 主线（`main` 分支，tag 形如 `vX.Y.Z`）的版本变化，版本号遵循
[语义化版本](https://semver.org/lang/zh-CN/)，格式参照
[Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

`book-v1.*` 是《重构C++ IO流》（李伟 著，中文版第 1 版，人民邮电出版社，2026 年 6 月）
配套代码的独立发布线（GPL-3.0-or-later），代码冻结在出版时的状态，版本号与本库无关，
不在本文件记录。此后的书籍直接基于本库主线 1.x，不另设发布线。

### [Unreleased]

#### 新增

- **设备层可在 Windows 与 macOS 上构建、运行**：Windows 上为 MSVC `cl` 与 clang-cl（x64、x86），
  macOS 上为 Apple clang。其余各层尚未移植。
- `IOV2_NO_UNIQUE_ADDRESS`：`[[no_unique_address]]` 的可移植写法。MSVC ABI 忽略标准写法、
  只认 `[[msvc::no_unique_address]]`。

#### 变更

- `file_device` 的构造函数与 `try_open` 改为接受 `std::filesystem::path`，路径编码由传入的
  类型决定：`char` 字符串按平台的原生窄编码（Windows 上为 ANSI 代码页，与 `fopen` 一致），
  `char8_t` 字符串按 UTF-8，`wchar_t` 字符串在 Windows 上即 UTF-16。传入 `std::string` 与
  字符串字面量的代码不受影响；只能隐式转换为 `std::string` 的自定义类型不再能直接传入。
- `file_device` 在所有平台上都以二进制打开文件，Windows 上也不做 `\r\n` 与 `\n` 的转换。
  换行转换将由转换器层提供（#505）。

#### 废弃

- `file_open_flag::binary`：已不起作用，计划在 2.0 中删除。

#### 修复

- 读写 `file_device` 在读与写之间切换时先定位一次，满足 C 标准对同一流读写切换的要求。
  此前依赖 glibc 的宽容，在 MSVC CRT 上写入会失败。
- `file_device::close()` 检查 `fclose()` 的返回值：冲刷成功之后关闭本身仍可能失败（如
  close(2) 报 EIO），此前这类失败被删除器静默丢弃，现在抛出 `device_error`。两者都失败时
  报告先发生的冲刷失败；无论哪种情况，文件都已关闭。析构函数的行为不变。
- `mem_device` 的约束补上 `Traits::char_type` 与分配器 `value_type` 必须是 `CharT` 两项
  （`std::basic_string` 的要求），错误的组合在用户代码处被拒绝，而不是在 `<string>` 内部
  报错。合法代码不受影响。
- `zlib_cvt` 可以叠在只读设备（如 `ifile_device`）上使用：此前两处写路径的调用未按设备
  能力分支，这样的组合编译不过。
- 注释更正：`locale` 拷贝赋值不是两阶段加锁；与 `cerr` 共用 stderr 的是
  `fprintf(stderr, ...)` 而非 `printf`；`tm_gmtoff` / `tm_zone` 的探测在缺少这两个成员的
  平台上是编译错误而非退化为字面量；`reset()` 不能用来丢掉本行剩余的输入，应改用
  `clear()` 加 `ignore()`；转换器 `put` 失败时，已交给下层的输出不会撤销，可能留下截断的
  前缀；`abs_cvt` 的钩子清单补上 `is_eof_impl()`，中文文档不再隐藏这份清单；`messages`
  按环境变量选语言时，名字原样当目录名，不做 gettext 那样的截短回退。

### [1.0.2] - 2026-10-26

#### 变更

- 标准流的公共操作基类 `std_stream_common_operators` 不再含受保护的 `detach` / `attach`
  重载（从未被调用）。标准流上的限定名调用（如 `cout.IOv2::std_stream_common_operators::detach()`）
  仍然编译不过，改由公开版本的 `requires (!is_std)` 拒绝；公开接口不变。

### [1.0.1] - 2026-10-23

#### 变更

- **tie 改为链式**：`a.tie(&b); b.tie(&c);` 之后，`a` 的一次 I/O 会依次刷新 `c`、`b`，
  此前只刷 `b`。每一级仍是尽力而为、不阻塞。标准输出流的退出钩子同样沿链刷新。
  相应地，tie 链上**每一个**流（不只是直接目标）都须活过所有经由它的流；标准流 tie 链上的
  流须活到退出钩子之后，否则先解开 tie。

### [1.0.0] - 2026-10-13

首个正式版本。自此公开接口遵循语义化版本：破坏性变更只在主版本号提升时发生。

#### 包含的组件

- **设备层**：`file_device`、`mem_device`、`std_device`，以 Concepts 描述读、写、定位等原子能力。
- **转换器层**：经 `operator|` 链式组合。
  - 编码转换：`code_cvt`、`code_cvt_stdio`。
  - 压缩：`zlib_cvt`（需 zlib）。
  - 密码学与哈希：`chacha20_cvt`、`hash_cvt`（需 Botan）、`vigenere_cvt`（仅限教学，位于 `IOv2::Crypt::Classic`）。
  - 运行时多态：`runtime_cvt`。
- **语义层**：重新设计的 `locale` 与 `collate`、`ctype`、`messages`、`monetary`、`numeric`、`timeio` 六类 facet。
- **流与通道**：`istream`、`ostream`、`iostream`、`iochannel`，操纵符，以及预定义标准流对象 `cin`/`cout`/`cerr`/`clog` 及其宽字符版本。
- **带填充的数值与货币可读回**：用相同的 locale、flags 与 `fill` 写出的字段都能原样读回；读不回的填充组合在写出侧被拒绝。
- **唯一的 I/O 扩展点** `io_traits<TChar, T>`，以及转发辅助 `compose_read`/`compose_write`。
- **流级线程安全**：单次 I/O 操作在流的 `io_mutex()` 下原子完成；`IOv2::sync` 使一组操作整体原子；`tie` 恒无环，且 tie 刷新不会死锁。
- **两种发布模式**：header-only（默认）与共享库 `libiov2.so`，经 `make install` / `make install-shared` 安装，附 pkg-config 文件。
#### 环境要求

- C++23；Linux + glibc + libstdc++。
- CI 验证的编译器：GCC 15.3、Clang 19.1（配 libstdc++）。
- 可选依赖：zlib（`zlib_cvt`）、Botan 2（`hash_cvt`）。

#### 许可证

MIT，适用于 `main` 分支 `b40a29d` 及之后的版本。更早的历史版本与 `book-v1` 线的许可
边界见 `README.md`、`NOTICE` 与 `PROVENANCE.md`。

#### 已知限制

- `chacha20_cvt` 尚未实现认证加密（AEAD），不应单独用于需要完整性保护的场景。
- 尚未适配 macOS 与 Windows。
- 读取返回 0 与 EOF 判定的解耦（#6）不在本版本范围内。

---

## English

This file records changes to the IOv2 mainline (the `main` branch, tagged
`vX.Y.Z`). Versions follow [Semantic Versioning](https://semver.org/), and the
format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

`book-v1.*` is a separate release line holding the companion code of
the book 《重构C++ IO流》 (in Chinese, 1st edition; Posts & Telecom Press,
June 2026), under GPL-3.0-or-later and frozen as
published. Its version numbers are unrelated to this library's and are not
recorded here. Later books build directly on this library's 1.x mainline and
have no release line of their own.

### [Unreleased]

#### Added

- **The device layer builds and runs on Windows and macOS**: MSVC `cl` and
  clang-cl (x64, x86) on Windows, Apple clang on macOS. The other layers are not
  ported yet.
- `IOV2_NO_UNIQUE_ADDRESS`: a portable spelling of `[[no_unique_address]]`. The
  MSVC ABI ignores the standard spelling and honours only
  `[[msvc::no_unique_address]]`.

#### Changed

- `file_device`'s constructor and `try_open` take a `std::filesystem::path`, so
  the path's encoding follows the type passed in: a `char` string is in the
  platform's native narrow encoding (the ANSI code page on Windows, as with
  `fopen`), a `char8_t` string is UTF-8, and a `wchar_t` string is UTF-16 on
  Windows. Code passing a `std::string` or a string literal is unaffected; a
  user type that only converts implicitly to `std::string` can no longer be
  passed as is.
- `file_device` opens files in binary on every platform; on Windows too there is
  no translation between `\r\n` and `\n`. Newline translation is to come from
  the converter layer (#505).

#### Deprecated

- `file_open_flag::binary`: it no longer has any effect, and is to be removed in
  2.0.

#### Fixed

- A read-write `file_device` seeks once when switching between reading and
  writing, as C requires of a stream used both ways. It used to rely on glibc
  tolerating the omission; on the MSVC CRT the write failed.
- `file_device::close()` checks `fclose()`'s result: a close can fail on its
  own after a successful flush (EIO from close(2), say), and that failure,
  which the deleter used to discard silently, now throws `device_error`. When
  both fail, the earlier flush failure is the one reported; either way the
  file is closed. The destructor is unchanged.
- `mem_device`'s constraint now also requires `Traits::char_type` and the
  allocator's `value_type` to be `CharT` (as `std::basic_string` does), so a
  wrong combination is rejected at the user's code rather than inside
  `<string>`. Valid code is unaffected.
- `zlib_cvt` can sit on a read-only device (such as `ifile_device`): two
  write-path calls were not branched on the device's capability, so that
  combination did not compile.
- Comment corrections: `locale` copy assignment is not two-phase locking; the
  writer sharing stderr with `cerr` is `fprintf(stderr, ...)`, not `printf`;
  the `tm_gmtoff` / `tm_zone` probes are a compile error, not a degradation to
  literals, on a platform lacking those members; `reset()` does not discard
  the rest of a line, use `clear()` followed by `ignore()`; a failed converter
  `put` does not undo output already handed to the layer below and may leave a
  truncated prefix; the `abs_cvt` hook list now includes `is_eof_impl()` and is
  no longer hidden in the Chinese documentation; when `messages` picks a
  language from the environment, names are used verbatim as directory names,
  without gettext's fallback to shorter names.

### [1.0.2] - 2026-10-26

#### Changed

- `std_stream_common_operators`, the common-operations base of the standard streams, no
  longer has protected `detach` / `attach` overloads (nothing ever called them). A qualified
  call on a standard stream, such as `cout.IOv2::std_stream_common_operators::detach()`, still
  does not compile; the public version's `requires (!is_std)` now refuses it. The public
  interface is unchanged.

### [1.0.1] - 2026-10-23

#### Changed

- **A tie is now chained**: after `a.tie(&b); b.tie(&c);`, an I/O on `a` flushes `c` and
  then `b`; before, it flushed `b` only. Each link is still best-effort and never blocks. The
  exit hook of the standard output streams flushes along the chain too. Accordingly, **every**
  stream on a tie chain, not just the direct target, must outlive every stream whose chain
  passes through it; a stream on a standard stream's tie chain must outlive the exit hook, or
  the tie must be undone first.

### [1.0.0] - 2026-10-13

First stable release. From here on the public interface follows Semantic
Versioning: breaking changes come only with a major version bump.

#### Components

- **Device layer**: `file_device`, `mem_device`, `std_device`, with reading, writing and seeking described as Concepts.
- **Converter layer**, composed with `operator|`:
  - Encodings: `code_cvt`, `code_cvt_stdio`.
  - Compression: `zlib_cvt` (requires zlib).
  - Cryptography and hashing: `chacha20_cvt`, `hash_cvt` (requires Botan), `vigenere_cvt` (educational only, in `IOv2::Crypt::Classic`).
  - Runtime polymorphism: `runtime_cvt`.
- **Semantic layer**: a redesigned `locale` and six facet families: `collate`, `ctype`, `messages`, `monetary`, `numeric`, `timeio`.
- **Streams and channels**: `istream`, `ostream`, `iostream`, `iochannel`, manipulators, and the predefined `cin`/`cout`/`cerr`/`clog` with their wide-character counterparts.
- **Padded numbers and monetary values read back**: a field written under a given locale, flags and `fill` reads back as the same value under the same settings; fills that would not are refused when writing.
- **A single I/O extension point**, `io_traits<TChar, T>`, plus the forwarding helpers `compose_read`/`compose_write`.
- **Stream-level thread safety**: each I/O operation completes atomically under the stream's `io_mutex()`; `IOv2::sync` makes a group of operations atomic; `tie` can never form a cycle, and the tie flush cannot deadlock.
- **Two distribution modes**: header-only (the default) and the shared library `libiov2.so`, installed with `make install` / `make install-shared`, with pkg-config files.
#### Requirements

- C++23; Linux + glibc + libstdc++.
- Compilers verified in CI: GCC 15.3 and Clang 19.1 (on libstdc++).
- Optional dependencies: zlib (`zlib_cvt`), Botan 2 (`hash_cvt`).

#### License

MIT, for the `main` branch at `b40a29d` and later. The licence boundary for
earlier history and for the `book-v1` line is set out in `README.md`, `NOTICE`
and `PROVENANCE.md`.

#### Known limitations

- `chacha20_cvt` is not yet authenticated encryption (AEAD); do not rely on it alone where integrity matters.
- macOS and Windows are not yet supported.
- Decoupling "read returned 0" from EOF detection (#6) is outside this release.

[Unreleased]: https://github.com/liwei-cpp/IOv2/compare/v1.0.0...HEAD
[1.0.2]: https://github.com/liwei-cpp/IOv2/releases/tag/v1.0.2
[1.0.1]: https://github.com/liwei-cpp/IOv2/releases/tag/v1.0.1
[1.0.0]: https://github.com/liwei-cpp/IOv2/releases/tag/v1.0.0
