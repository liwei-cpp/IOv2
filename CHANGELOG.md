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
- `std_input_device::stdio_mode()`：切换 stdin 的读法。缺省的 fd 模式照旧直接读 fd 0；
  stdio 模式在 `stdin` 的 `FILE` 锁内 `fread`，与 C stdio 共用缓冲，EOF 以 `FILE` 的粘性
  标志为准。
- `std_input_device::dunget()`：把字节退回给 C stdio，要么全部退回、返回 `true`，要么
  一个也不退、返回 `false`。只有 stdio 模式可能成功：POSIX 上经 `ungetc` 逐字节退回，
  Windows 上总是拒绝（UCRT 的 `ungetc` 空间不够时可能改写未读的字节而照样报告成功）。
  字节可以用指针加长度给出，也可以用一对双向迭代器给出。
- `runtime_cvt::target<T>()`：类型与管线最外层精确一致时返回指向该转换器的指针，否则返回
  空指针，类似 `std::function::target`；`T` 的设备类型或内部字符类型不对时编译报错。
  `ichannel` 等通道经 `cvt_target<T>()` 转发。
- 输入通道的 `hand_back(f)`：把读缓冲里偷看、压回而未消费的字符按流中顺序交给 `f`，
  不拷贝；`f` 返回 `true` 时清空读缓冲，返回 `false` 或抛出异常时读缓冲不变。
- `ios_base` 与 `ios_state` 的构造函数可以接受一把外部的对象锁（缺省为空，用流自带的锁），
  供共用底层资源的几个流共用一把 `io_mutex()`。拷贝或移动出的流用自己的锁，赋值不改变
  双方各自用的锁。

#### 变更

- 与 C stdio 同步的标准输入流（`cin`、`wcin` 的缺省模式）改经 `stdin` 的 `FILE` 读，与 C
  stdio 共用缓冲：`getchar()`、`scanf()` 等预读进 `FILE` 的字节本流接着读得到。此前设备直接
  读 fd 0，终端上一次 `getchar()` 读走的整行本流看不到。随之而来的两处行为变化：到达 EOF
  后 `FILE` 的 EOF 标志是粘性的，`clear()` 之后仍读不到后来的输入，`reset()` 现在为此调用
  `clearerr(stdin)`；同步时本流读剩的字节留在 `FILE` 缓冲里，而不是 fd 上。不同步模式不变。
- 同步的 `wcin` 回退尚未解码的字节时（如输入末尾只剩半个多字节字符），把它们经
  `dunget()` 还给 C stdio，`getchar()` 等随后读得到；此前它们留在本流的缓冲里，C stdio 看不到。
  随之 `reset()` 不再能丢掉末尾的半个字符：它已归 C stdio，须由 C stdio 读走。设备拒绝退回时
  （不同步，或 Windows）行为不变。
- `stdin_root_cvt` 不再是模板，也不再派生自 `root_cvt`：它是不存状态的句柄，构造时不带
  参数，设备、缓冲与同步标志都在唯一的共享对象里，有自己的 `cvt_reader` 特化。写作
  `stdin_root_cvt<std_input_device>{dev, synced}` 的代码改为 `stdin_root_cvt{}`，同步与否经
  `adjust(stdin_sync{...})` 设置。
- `cin` 与 `wcin` 共用一把对象锁：二者读同一个 fd 0，一个流的操作会等另一个流正在进行的
  操作结束，两个线程分别使用它们不再交错。锁是递归的，同一线程在一次操作里再用另一个流
  不会自锁；但线程 A 在 `cin` 的一次提取里写 `wcout`、同时线程 B 在 `wcout` 的一次插入里
  读 `wcin`，会互相等待而死锁。
- `cin` 与 `wcin` 共用输入缓冲与同步状态：不同步时一个流预读的字节另一个流接着读得到，
  此前同一段输入分到哪个流不确定；切换其中一个的 `sync_with_stdio()` 就切换了二者，
  `synced_with_stdio()` 总是一致。随之，`reset()` 与 `wcin` 转换器的自动恢复丢掉的已预读
  字节对二者都丢掉。
- `cin` 与 `wcin` 每次读操作结束时把偷看、压回的字符退回下层：`cin >> n` 探到的分隔符、
  `peek()`、`putback()` 的字符，`getchar()` 等与另一个流接着读得到；此前它们留在本流自己的读
  缓冲里，别人看不到。`wcin` 的字符重新编码成字节退回，同一字符有多种字节表示的编码可能换成
  另一种表示；之后 `switch_code()` 切换了编码，这些字节按新编码解码。随之 `reset()` 在同步时
  不再能丢掉偷看的字符，它已归 C stdio。`FILE` 不收时（不同步，Windows，或字节缓冲里还有
  字节）字节进共用的字节缓冲，C stdio 看不到。退回是尽力而为的，失败不报告，字符留在读缓冲
  里，本流照样先读到。自定义创建器构造的流不退回。
- `file_device` 的构造函数与 `try_open` 改为接受 `std::filesystem::path`，路径编码由传入的
  类型决定：`char` 字符串按平台的原生窄编码（Windows 上为 ANSI 代码页，与 `fopen` 一致），
  `char8_t` 字符串按 UTF-8，`wchar_t` 字符串在 Windows 上即 UTF-16。传入 `std::string` 与
  字符串字面量的代码不受影响；只能隐式转换为 `std::string` 的自定义类型不再能直接传入。
- `file_device` 在所有平台上都以二进制打开文件，Windows 上也不做 `\r\n` 与 `\n` 的转换。
  换行转换将由转换器层提供（#505）。
- `abs_cvt` 删除受保护的 `kernel_get()`，改由 `kernel_reader()` 取得读下层的 `cvt_reader`：
  前者跳过取数区里没消费的字节，只有 `assert` 把关。自定义转换器在 `get_main()` 之外读下层
  时改用 `kernel_reader().get_buf<true>(n)`，先以 `reset(n)` 保证缓冲够大。随之 `zlib_cvt`、
  `chacha20_cvt` 读流头时遇到短读的设备会读到够为止，流头不完整时抛 `cvt_error`。`mem_device`
  上的 `cvt_reader::get_buf<true>` 读不满时也改抛 `cvt_error`，与其他 reader 一致，此前是
  `device_error`。
- `abs_cvt` 在 BOS 阶段（`bos()` 之后、`main_cont_beg()` 之前）不再换向：显式调用
  `switch_to_get()` / `switch_to_put()`，或与 `bos()` 所定方向相反的 `get()` / `put()`，
  都抛出 `cvt_error`，且不污染转换器。此前反向的 `put()` 会把原始字节交给下层，由下层
  `root_cvt` 自行换向写到流首，两层的方向也随之不一致。`main_cont_beg()` 之后的换向不变；
  `root_cvt` 本身不读写流头，不受此限。

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
- `std_input_device::stdio_mode()`: switches how stdin is read. The default fd
  mode reads fd 0 directly as before; stdio mode `fread`s under the lock of
  `stdin`'s `FILE`, sharing its buffer with C stdio, and takes EOF from the
  `FILE`'s sticky flag.
- `std_input_device::dunget()`: hands bytes back to C stdio, either all of them,
  returning `true`, or none, returning `false`. Only stdio mode can succeed: on
  POSIX the bytes go back one by one through `ungetc`; on Windows it always
  refuses (the UCRT's `ungetc` may overwrite an unread byte and still report
  success when it has no room). The bytes come either as a pointer and a length
  or as a pair of bidirectional iterators.
- `runtime_cvt::target<T>()`: a pointer to the outermost converter of the
  pipeline when its type is exactly `T`, a null pointer otherwise, much like
  `std::function::target`; a `T` with the wrong device or internal character
  type is a compile error. Channels such as `ichannel` forward it as
  `cvt_target<T>()`.
- `hand_back(f)` on input channels: offers the characters peeked or put back but
  not yet consumed to `f`, in stream order and without copying; the read buffer
  is cleared when `f` returns `true` and left as it is when `f` returns `false`
  or throws.
- The constructors of `ios_base` and `ios_state` accept an external object lock
  (null by default, meaning the stream's own), so that streams over a shared
  resource can share one `io_mutex()`. A stream copied or moved from another uses
  its own lock, and assignment leaves each side with the lock it had.

#### Changed

- A standard input stream synchronized with C stdio (the default mode of `cin`
  and `wcin`) reads through `stdin`'s `FILE`, sharing its buffer with C stdio:
  bytes `getchar()`, `scanf()` and the like have read ahead into the `FILE` are
  there for the stream to read on. The device used to read fd 0 directly, so on a
  terminal the whole line one `getchar()` took was invisible to the stream. Two
  behaviour changes come with it: once EOF is reached the `FILE`'s EOF flag is
  sticky, so after `clear()` later input is still not read, and `reset()` now
  calls `clearerr(stdin)` for that; and bytes the stream leaves unread when
  synchronized wait in the `FILE` buffer rather than on the fd. Unsynchronized
  mode is unchanged.
- A synchronized `wcin` that rolls back bytes it has not decoded (half a
  multibyte character at the end of the input, say) hands them back to C stdio
  through `dunget()`, so `getchar()` and the like read them next; they used to
  stay in the stream's buffer, out of C stdio's sight. With that, `reset()` can
  no longer drop a half character at the end: it is C stdio's now, and C stdio
  has to take it. Where the device refuses (unsynchronized, or Windows) nothing
  changes.
- `stdin_root_cvt` is no longer a template and no longer derives from
  `root_cvt`: it is a stateless handle constructed without arguments, the device,
  the buffer and the synchronization flag living in one shared object, and it has
  its own `cvt_reader` specialization. Code that writes
  `stdin_root_cvt<std_input_device>{dev, synced}` writes `stdin_root_cvt{}`
  instead, and sets the synchronization through `adjust(stdin_sync{...})`.
- `cin` and `wcin` share one object lock: they read the same fd 0, so an
  operation on one waits for an operation under way on the other, and two threads
  using them no longer interleave. The lock is recursive, so one thread using the
  other stream inside an operation does not deadlock itself; but thread A writing
  `wcout` inside an extraction on `cin` while thread B reads `wcin` inside an
  insertion on `wcout` deadlocks, each waiting for the other.
- `cin` and `wcin` share their input buffer and synchronization: unsynchronized,
  bytes one stream read ahead are there for the other to read on, where it used
  to be unpredictable which stream a stretch of input went to; switching
  `sync_with_stdio()` on either switches both, and `synced_with_stdio()` always
  agrees. With that, bytes read ahead that `reset()` or the automatic recovery of
  `wcin`'s converter drops are dropped for both.
- When a read operation ends, `cin` and `wcin` hand the characters they peeked at
  or had put back to the layer below: the delimiter `cin >> n` looked at and the
  characters of `peek()` and `putback()` are there for `getchar()` and the like and
  for the other stream to read next; they used to stay in the stream's own read
  buffer, out of everyone else's sight. `wcin`'s characters go back encoded into
  bytes again, so an encoding with several byte forms for one character may come
  back in another form, and after a `switch_code()` the bytes decode by the new
  encoding. With that, a synchronized `reset()` can no longer drop a peeked
  character, as it is C stdio's now. Where the `FILE` does not take them
  (unsynchronized, on Windows, or with bytes still in the byte buffer) the bytes go
  into the shared byte buffer, out of C stdio's sight. Handing back is best effort:
  a failure is not reported, and the characters stay in the read buffer for this
  stream to read first. A stream built with a custom creator does not hand back.
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
- `abs_cvt` drops the protected `kernel_get()` for `kernel_reader()`, which gives a
  `cvt_reader` on the layer below: the former skips unconsumed bytes of the read
  area with only an `assert` to stop it. A custom converter reading the layer below
  outside `get_main()` uses `kernel_reader().get_buf<true>(n)`, after `reset(n)` to
  make the buffer large enough. With that, `zlib_cvt` and `chacha20_cvt` keep reading
  a stream header from a device that reads short, and an incomplete header throws
  `cvt_error`. `cvt_reader::get_buf<true>` on a `mem_device` now throws `cvt_error`
  too when input runs out, like every other reader, where it threw `device_error`.
- `abs_cvt` no longer switches direction in the BOS phase (after `bos()`, before
  `main_cont_beg()`): an explicit `switch_to_get()` / `switch_to_put()`, or a `get()` /
  `put()` against the direction `bos()` settled on, throws `cvt_error` and does not taint
  the converter. A reverse `put()` used to hand raw bytes to the layer below, where
  `root_cvt` switched on its own and wrote at the stream origin, leaving the two layers
  disagreeing on the direction. Switching after `main_cont_beg()` is unchanged; `root_cvt`
  itself reads and writes no header and is not restricted.

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
