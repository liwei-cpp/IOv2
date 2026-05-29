# GCC libstdc++ — `num_put::do_put(const void*)` / `num_get::do_get(void*&)` leak fmtflags when inner iterator throws

- **Upstream tracker**: <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125500>
- **Upstream project / component**: GCC / `libstdc++`
- **Bug ID**: PR 125500
- **Reported**: 2026-05-29 17:04 UTC
- **Last modified upstream**: 2026-05-29 17:04 UTC
- **Current status**: `UNCONFIRMED` — submitted, awaiting maintainer triage; not yet assigned, no comments from GCC maintainers.

[中文](#中文) | [English](#english)

---

## 中文

### 创建时间

2026-05-29 17:04 UTC(本地复现器与最小化案例完成后,于同日提交至 GCC Bugzilla,附带 attachment 64582 — `repro_gcc_void_ptr_flags_leak.cpp`)。

### 当前状态

`UNCONFIRMED`(已提交,尚未被 GCC 维护者确认)。Assignee 字段仍为 *Not yet assigned to anyone*,Importance 为 `P3 / normal`,目前没有任何来自上游开发者的评论。后续状态变更请以上游链接为准:<https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125500>。

### Bug 描述

`libstdc++-v3/include/bits/locale_facets.tcc` 中的 `std::num_put<CharT>::do_put(..., const void*)` 与 `std::num_get<CharT>::do_get(..., void*&)`(`do_get` 大约在 L1131,`do_put` 是对称写法)使用了"手动 save/restore"的模式:

1. 把 `io.flags()` 存到一个 const 局部变量;
2. 调用 `io.flags(...)` 把 ios_base 改成 `hex | showbase`(`do_put`)或者把 basefield 设为 `hex`(`do_get`);
3. 通过 iterator 调 `_M_insert_int` / `_M_extract_int`;
4. 在末尾再 `io.flags(__saved)` 把保存的标志位还原回去。

第 (4) 步**只在正常返回路径上执行**。当 iterator 在第 (3) 步抛异常时 —— 这是完全合法的:streambuf 的 `overflow` / `underflow` 在报 I/O 错误时本来就应该抛,内部分配也可能抛 `bad_alloc` —— 还原步骤被直接跳过,调用者的 `ios_base` 被永久留在实现的中间状态:

- `do_put` 抛出时:`hex | showbase` 保留下来,`dec` 位作为 `setf(hex, basefield)` 的副作用被清掉。
- `do_get` 抛出时:`hex` 保留下来,`dec` 同样被清掉。

更阴险的是:`basic_ostream::operator<<(const void*)` 与对称的 `>>` 都用 sentry 包装,sentry 会 catch 异常、把 `badbit` 设上、然后**把异常吞掉**。所以调用者看不到任何异常 —— 只看到 `badbit` 以及一个 `flags()` 被永久污染的 stream。这是基本异常保证的**静默违反**:`do_put` / `do_get` 的合同不包含修改 `flags()`,所以调用结束后外部能观察到的 `flags()` 变化就是 bug。

下游可观察后果:同一个 stream 上,接下来的 `<< 255` 会输出 `"0xff"`,因为 `hex | showbase` 永久留在 stream 上,后续的整数格式化全部走 hex + showbase 路径。

复现器 `repro_gcc_void_ptr_flags_leak.cpp`(仓库根目录)用两个自定义 streambuf —— `throwing_obuf::overflow` 与 `throwing_ibuf::underflow` 在 budget 耗尽时抛 —— 来触发 (3) 步的异常路径。在 Homebrew GCC 15.1.0(`aarch64-apple-darwin24`)的 `-O0` 与 `-O2` 下都稳定复现,且两个优化级别的输出逐字节一致,排除"优化器吃掉异常路径"的可能性。同时,**非抛异常**(budget 足够大)的 control 路径上 `flags()` 完整还原,确认差异只发生在异常路径上,不是 sentry 自身的副作用。

上游建议的修复:在 `locale_facets.tcc` 里把 `do_put(const void*)` 与 `do_get(void*&)` 各自的"手动 save/restore"换成一个小的 RAII guard(析构里调 `io.flags(saved)`),让正常返回路径和异常路径都会还原。

### IOv2 的处理

IOv2 自己的 `numeric` facet 在 `put(const void*)` / `get(void*&)` 中历史上写法与 libstdc++ 同源(手动 save/restore),因此同样存在这个缺陷。我们在向上游提交 bug 的同时,**已经在 IOv2 内部进行了修复**:

- **文件**:`include/facet/numeric.h`。
- **改动**:在私有段引入嵌套结构体 `fmtflags_guard`,构造时记录 `m_saved = io.flags()`,析构时无条件 `m_io.flags(m_saved)`;拷贝与移动操作显式 `= delete`,避免作用域逃逸。`put(const void*)` 与 `get(void*&)` 进入时各自实例化一个 guard 再去改写 flags,无论后续 `insert_int` / `extract_int` 正常返回还是抛异常,析构都会把原 flags 还原回去。
- **取舍**:**这是一处刻意偏离 libstdc++ 行为的修复**。在 libstdc++ 下,异常路径会留下被污染的 flags;在 IOv2 下,异常路径上的 sentry 仍然会把异常吞掉并把 `badbit` 设上,但 `flags()` 会保留为调用前的值。考虑到 `flags()` 不在 `do_put` / `do_get` 的可观察合同里,把它还原回原值是更安全、更符合直觉的选择,也更符合基本异常保证。
- **复现器**:`repro_gcc_void_ptr_flags_leak.cpp`(仓库根目录),原生 GCC 工具链上可直接编译运行,演示 libstdc++ 在异常路径上泄漏 flags 的行为,以及"下游 `<< 255` 输出 `0xff`"的可观察后果。

因此,即使上游短期内不修复,IOv2 用户在使用本库的 `numeric` facet 时,`flags()` 始终保持调用前后一致(只在正常成功路径上由用户显式修改),不会受到此 bug 影响。

---

## English

### Created at

2026-05-29 17:04 UTC. Submitted to GCC Bugzilla the same day the local reproducer was finalized, with attachment 64582 — `repro_gcc_void_ptr_flags_leak.cpp` — attached.

### Current status

`UNCONFIRMED` — submitted, awaiting upstream triage. The `Assignee` field still reads *Not yet assigned to anyone*; `Importance` is `P3 / normal`; there are no maintainer comments yet. For up-to-date status, see <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125500>.

### Bug description

`std::num_put<CharT>::do_put(..., const void*)` and `std::num_get<CharT>::do_get(..., void*&)` in `libstdc++-v3/include/bits/locale_facets.tcc` (`do_get` around L1131; `do_put` is the symmetric counterpart) use a "manual save/restore" pattern:

1. save `io.flags()` into a const local;
2. call `io.flags(...)` to mutate ios_base flags to `hex | showbase` (`do_put`) or set basefield to `hex` (`do_get`);
3. call `_M_insert_int` / `_M_extract_int` through an iterator;
4. restore the saved flags by calling `io.flags(__saved)` at the end.

Step (4) **runs only on the normal-return path.** When the iterator throws in step (3) — which is well-formed: a streambuf's `overflow` / `underflow` is *supposed* to throw when reporting I/O errors, and the inner machinery may itself throw `bad_alloc` — the restore is skipped and the caller's `ios_base` is left in the implementation's intermediate state:

- When `do_put` throws: `hex | showbase` remains set; `dec` is cleared as a side effect of `setf(hex, basefield)`.
- When `do_get` throws: `hex` remains set; `dec` is similarly cleared.

The corruption is silent: `basic_ostream::operator<<(const void*)` and the matching `>>` wrap the call in a sentry that catches the exception, sets `badbit`, and **swallows the exception**. The caller therefore sees no exception at all — only `badbit` and a stream whose `flags()` is permanently polluted. This is a silent violation of the basic exception guarantee: `do_put` / `do_get`'s contract does not include mutating `flags()`, so any observable change to `flags()` after the call is a bug.

Downstream observable effect: on the same stream, the next `<< 255` prints `"0xff"`, because `hex | showbase` remain permanently set and every subsequent integer format on this stream now goes through the hex+showbase path.

The reproducer `repro_gcc_void_ptr_flags_leak.cpp` (at the repo root) uses two custom streambufs — `throwing_obuf::overflow` and `throwing_ibuf::underflow` throw when their budget is exhausted — to trigger the exception path in step (3). It reproduces identically on Homebrew GCC 15.1.0 (`aarch64-apple-darwin24`) at both `-O0` and `-O2`; the two optimization levels produce byte-identical output, ruling out "the optimizer ate the exception path" as a possible explanation. The non-throwing control path (with a large enough budget) restores `flags()` fully, confirming the divergence is exception-path-only and not a sentry-side artifact.

The suggested upstream fix is to replace the manual save/restore in both `do_put(const void*)` and `do_get(void*&)` with a small RAII guard whose destructor calls `io.flags(saved)`, so the restore runs on both the normal-return path and the exception path.

### What IOv2 does

IOv2's own `numeric` facet's `put(const void*)` / `get(void*&)` historically share the manual save/restore shape with libstdc++, so they inherit the same defect. We have **fixed the issue inside IOv2** at the same time as filing the upstream bug:

- **File**: `include/facet/numeric.h`.
- **Change**: a private nested struct `fmtflags_guard` records `m_saved = io.flags()` at construction and unconditionally calls `m_io.flags(m_saved)` in its destructor; copy and move are explicitly `= delete`. Both `put(const void*)` and `get(void*&)` instantiate this guard before mutating flags, so whether the inner `insert_int` / `extract_int` returns normally or throws, the destructor restores the original flags.
- **Trade-off**: **this is a deliberate divergence from libstdc++ behavior.** Under libstdc++, the exception path leaves `flags()` polluted; under IOv2, the sentry still swallows the exception and sets `badbit` on the exception path, but `flags()` is restored to its pre-call value. Given that `flags()` is not part of the observable contract of `do_put` / `do_get`, restoring it is the safer, more intuitive choice and more in line with the basic exception guarantee.
- **Reproducer**: `repro_gcc_void_ptr_flags_leak.cpp` (at the repository root) is a self-contained program that compiles on a native GCC toolchain and demonstrates both the libstdc++ leak on the exception path and the downstream `<< 255` → `"0xff"` corruption.

As a result, even if the upstream bug is not fixed in the short term, IOv2 users observe `flags()` that is consistent across the call (only modified explicitly by the user on the successful path), unaffected by this libstdc++ defect.
