# Apple libc — `fread()` on an unbuffered stream skips the bytes `ungetc()` pushed back

- **Upstream tracker**: Apple Feedback Assistant (no public tracker). Feedback ID: `FB25523856`.
- **Upstream project / component**: Apple libc (macOS), `stdio`
- **Bug ID**: FB25523856
- **Reported**: 2026-11-10 (macOS 27.2, arm64)
- **Last modified upstream**: n/a. Feedback reports are private; the status is only visible to the submitter.
- **Current status**: `Reported`. In the public source mirror, `fread.c` was last changed in 2024-09 and still has the behavior below as of 2026-04-17. No public report found.

[中文](#中文) | [English](#english)

---

## 中文

### Bug 描述

`stdin` 等流设为 `_IONBF` 后，先用 `ungetc()` 回退 N（≥2）个字节，再用 `fread()` 读 k 个字节（2 ≤ k ≤ N）：`fread` 返回 0 并置 EOF，回退的字节读不出来。`ungetc()` 全部返回成功，`getc()` 能按序读出这些字节；缓冲模式不受影响。

回退 5 字节、fd 已到 EOF 时，`fread(k)` 的结果：

| k | 返回 |
|---|---|
| 1 | 1（正确） |
| 2 到 5 | **0**，置 EOF |
| 6、7 | 5（正确） |

原因（Apple `Libc` 提交 [`71bbe350`](https://github.com/apple-oss-distributions/Libc/tree/71bbe350ab79eef58113991d817ccc6165061a64)）：

- [`stdio/FreeBSD/fread.c:82`](https://github.com/apple-oss-distributions/Libc/blob/71bbe350ab79eef58113991d817ccc6165061a64/stdio/FreeBSD/fread.c#L82)：先排空已有字节（含 `ungetc` 缓冲）的循环，只在请求量大于 `_r` 时才进入。
- [`fread.c:101`](https://github.com/apple-oss-distributions/Libc/blob/71bbe350ab79eef58113991d817ccc6165061a64/stdio/FreeBSD/fread.c#L101)：无缓冲流的"直接读到用户内存"路径，只要请求量大于 `_bf._size`（`_IONBF` 下为 1）就走，不检查 `_r`。
- 所以 2 ≤ k ≤ N 时，待取字节被跳过，[`__srefill1`](https://github.com/apple-oss-distributions/Libc/blob/71bbe350ab79eef58113991d817ccc6165061a64/stdio/FreeBSD/refill.c#L136) 直接读 fd，读到 0 即置 `__SEOF`。

C 标准只保证 1 字节回退，更多字节允许 `ungetc` 返回 `EOF`；但返回成功之后，字节必须仍可读。

复现：一个只用 C stdio 的小程序；复现程序未纳入本仓库。

### IOv2 的处理

**库没有改，测试做了规避。**

- 发现经过：`CodeCvtStdio.ReadAheadBytesSurviveASwitchToSynchronized` 在 macOS 上只读到 `b`。同步模式下解码到半个字符，`rollback` 经 `std_stdin_file_unget` 把根缓冲里剩下的 5 字节整体 `ungetc` 回去，随后的 `fread(2)` 恰好落在 2 ≤ k ≤ N。
- `test/support/stdio_guard.h` 的 `iguard` 在 macOS 上不再把 `stdin` 设为 `_IONBF`，其他平台不变。
- 库侧：`std_stdin_file_unget` 假定多字节 `ungetc` 失败时会返回 `EOF`，走回读兜底；macOS 的 `_IONBF` 流上它返回成功、之后 `fread` 读不出。这需要程序自己对 `stdin` 调 `setvbuf(..., _IONBF, 0)`，且在同步模式下遇到跨边界的半个字符，才会丢字节。IOv2 尚未对此做规避。

---

## English

### Bug description

On a stream set to `_IONBF`, push back N (>= 2) bytes with `ungetc()` and then `fread()` k bytes with 2 <= k <= N: `fread` returns 0 and sets EOF, and the pushed-back bytes cannot be read. Every `ungetc()` succeeds and `getc()` returns the bytes in order; buffered streams are not affected.

With 5 bytes pushed back and the fd at EOF, `fread(k)` returns:

| k | returns |
|---|---|
| 1 | 1 (correct) |
| 2 to 5 | **0**, EOF set |
| 6, 7 | 5 (correct) |

Cause (Apple `Libc` commit [`71bbe350`](https://github.com/apple-oss-distributions/Libc/tree/71bbe350ab79eef58113991d817ccc6165061a64)):

- [`stdio/FreeBSD/fread.c:82`](https://github.com/apple-oss-distributions/Libc/blob/71bbe350ab79eef58113991d817ccc6165061a64/stdio/FreeBSD/fread.c#L82): the loop that first drains the bytes already held (the `ungetc` buffer included) is entered only when the request is larger than `_r`.
- [`fread.c:101`](https://github.com/apple-oss-distributions/Libc/blob/71bbe350ab79eef58113991d817ccc6165061a64/stdio/FreeBSD/fread.c#L101): the unbuffered "read straight into the caller's memory" path is taken whenever the request is larger than `_bf._size` (1 under `_IONBF`), without looking at `_r`.
- So for 2 <= k <= N the pending bytes are skipped, [`__srefill1`](https://github.com/apple-oss-distributions/Libc/blob/71bbe350ab79eef58113991d817ccc6165061a64/stdio/FreeBSD/refill.c#L136) reads the fd directly, gets 0, and sets `__SEOF`.

The C standard guarantees one byte of pushback and lets `ungetc` fail with `EOF` beyond that; but once it has returned success the byte has to stay readable.

Reproducer: a small program using plain C stdio; it is not part of this repository.

### What IOv2 does

**The library is unchanged; the tests work around it.**

- How it was found: `CodeCvtStdio.ReadAheadBytesSurviveASwitchToSynchronized` read only `b` on macOS. In synchronized mode the decoder met half a character, `rollback` pushed the 5 bytes left in the root buffer back through `std_stdin_file_unget` as a whole, and the following `fread(2)` falls in 2 <= k <= N.
- `iguard` in `test/support/stdio_guard.h` no longer sets `stdin` to `_IONBF` on macOS; other platforms are unchanged.
- Library side: `std_stdin_file_unget` assumes a multi-byte `ungetc` that cannot work returns `EOF`, so the read-back fallback runs; on a macOS `_IONBF` stream it returns success and `fread` then cannot read the bytes. It takes a program that calls `setvbuf(..., _IONBF, 0)` on `stdin` itself and, in synchronized mode, meets a character split at a read boundary. IOv2 does not work around this yet.
