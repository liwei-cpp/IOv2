# GCC — false `-Warray-bounds` when speculative devirtualization inlines the wrong target of a two-target polymorphic call

- **Upstream tracker**: <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=126408>, closed as a duplicate of <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=122197>
- **Upstream project / component**: GCC / filed as `c++`; the bug it duplicates, PR 122197 "[16/17 Regression] predictively devirtualization vs middle-end warnings since r16-4000", is `tree-optimization`
- **Bug ID**: PR 126408 (duplicate of PR 122197)
- **Reported**: 2026-07-26 00:22 UTC by liwei (Version: 16.1.0)
- **Last modified upstream**: 2026-07-26 00:43 UTC (PR 126408)
- **Current status**: `RESOLVED DUPLICATE` of PR 122197 — Drea Pinski, comment #1 ("Dup."), 43 minutes after submission. The reporter was added to PR 122197's CC list. PR 122197 itself is still open (`NEW`, Target Milestone 16.3, per the gcc-bugs archive on 2026-11-12); the commits on it so far only suppress the warning inside libstdc++'s `shared_ptr`, and GCC 16.2.1 still warns on the reproducer below.

[中文](#中文) | [English](#english)

---

## 中文

### 创建时间

2026-07-26 00:22 UTC 提交到 GCC Bugzilla,同日 00:43 UTC 被标为 PR 122197 的重复。当时因为"上游已有同类 bug 在跟踪"而没有收进本目录;2026-11-12 在新机器(Fedora 45,GCC 16.2.1)上排查 g++ 16 编译问题时重新找回,补录于此。

### 当前状态

`RESOLVED DUPLICATE`。Drea Pinski 在 comment #1 写 "Dup." 并标记为 PR 122197 的重复;PR 122197 的 comment #22 记录了这次合并,提交者被加入其 CC 列表。

PR 122197 标题为 "[16/17 Regression] predictively devirtualization vs middle-end warnings since r16-4000",即从 r16-4000 起,推测性去虚化与中端警告(`-Warray-bounds` 等)相互作用产生误报,是 GCC 16 的回归。据 2026-11-12 的 gcc-bugs 邮件存档,PR 122197 仍为 `NEW`,Target Milestone 16.3(2026-08-07 由 16.2 推迟)。它上面的提交都是 libstdc++ 侧在 `shared_ptr` 内部压制警告,并未修正去虚化本身:r16-7106(2026-01)与 r17-2781 / 回移 gcc-16 的 r16-9434(2026-07-28,"libstdc++: Disable more false positives in shared_ptr [PR122197]")。2026-08-05 有人在 comment #25 附了一个让 array-bounds 检查忽略对象末尾 clobber 的补丁,尚未合入。GCC 16.2.1(20261029 快照,已含 r16-9434)对下面的复现器仍然报警。后续请以上游链接为准:<https://gcc.gnu.org/bugzilla/show_bug.cgi?id=122197>。

### Bug 描述

一个虚函数调用在整个翻译单元里恰好有两个可能目标时,ipa-devirt 会把它改写成推测性的两路分发:按 vtable 比较选择分支,两个目标的函数体都内联进来。这个合成的函数体再被内联进调用者后,调用者里对象的真实大小已知,而那条 vtable 比较永远选不中的分支访问了超出该对象的成员,`-Warray-bounds` 对它报警——警告分析没有考虑到 vtable 守卫使该分支不可达。

触发特征(据原报告与本机核验):

- `-O2` 及以上;`-O0`/`-O1` 不报。
- 恰好两个目标;`-fno-devirtualize-speculatively` 或 `-fno-devirtualize` 可消除警告。
- 同一翻译单元里加上 `main()` 后不再报。
- C++11 到 C++23 均复现。
- GCC 15.3.1 不报,GCC 16.1.1 与 16.2.1 报——回归。

### 最小复现

复现器为本项目自写,随上游 bug 一并提交,全文见英文部分。从接口命名看(`is_eof`/`bos`/`to_get` 的虚接口、`Wrap` 中的 `m_zbuf`/`m_ended`),它应是从 `runtime_cvt` 类型擦除包装 `zlib_cvt` 的组合缩减而来;这一点是根据命名的推断,当时的缩减过程没有留下记录。

`g++ -O2 -Wall -std=c++17 -c bugreport.cpp` 在 GCC 16 下对 `small_ops()` 报两处 `array subscript 'Wrap<Small>[0]' is partly outside array bounds of 'unsigned char [96]'`:`small_ops()` 里的对象是 96 字节的 `Imp<Small>`,而被内联进来的却是 `Imp<Wrap<Small>>::to_get()` 那一路。

2026-11-12 在 GCC 16.2.1(Fedora 45,`16.2.1 20261029 (Red Hat 16.2.1-3)`)上核验:默认报 2 处,加 `-fno-devirtualize-speculatively` 后为 0。

### IOv2 的处理

仓库历史里没有针对这个 bug 的规避提交。2026-11-12 用 GCC 16.2.1 完整构建 `gcc-release`(197 个源文件),没有出现这个诊断——IOv2 当前代码不触发它,推测是之后 cvt 的结构变化使其不再满足"恰好两个目标"的条件。

同一次构建中出现的唯一错误是另一个 GCC 16 误报:`std::vector<char>::resize(1)` 在 `-O3` 下的 `-Warray-bounds`。它没有虚调用,`-fno-devirtualize-speculatively` 也消除不了,与本 bug 无关,另行上报。

---

## English

### Creation time

Submitted to GCC Bugzilla on 2026-07-26 00:22 UTC and marked a duplicate of PR 122197 at 00:43 UTC the same day. It was left out of this directory at the time because upstream was already tracking the same bug; it was found again on 2026-11-12 while investigating g++ 16 build failures on a new machine (Fedora 45, GCC 16.2.1), and recorded here.

### Current status

`RESOLVED DUPLICATE`. Drea Pinski wrote "Dup." in comment #1 and marked it a duplicate of PR 122197; PR 122197's comment #22 records the merge, and the reporter was added to its CC list.

PR 122197 is titled "[16/17 Regression] predictively devirtualization vs middle-end warnings since r16-4000": since r16-4000, speculative devirtualization combines with middle-end warnings such as `-Warray-bounds` to produce false positives, a GCC 16 regression. According to the gcc-bugs archive on 2026-11-12, PR 122197 is still `NEW`, Target Milestone 16.3 (moved from 16.2 on 2026-08-07). The commits on it all suppress the warning inside libstdc++'s `shared_ptr` rather than fix the devirtualization: r16-7106 (2026-01), and r17-2781 with its gcc-16 backport r16-9434 (2026-07-28, "libstdc++: Disable more false positives in shared_ptr [PR122197]"). A patch making the array-bounds check ignore end-of-object clobbers was attached in comment #25 on 2026-08-05 and has not been committed. GCC 16.2.1 (a 20261029 snapshot, which includes r16-9434) still warns on the reproducer below. See <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=122197> for later changes.

### Description

When a virtual call has exactly two possible targets in the translation unit, ipa-devirt rewrites it into a speculative two-way dispatch: a vtable comparison picks a branch, and both targets' bodies are inlined. Once that combined body is inlined into a caller where the object's real size is known, the branch the vtable comparison can never select accesses members past the end of that object, and `-Warray-bounds` reports it: the warning analysis does not take into account that the vtable guard makes the branch unreachable.

Characteristics (from the original report and checked locally):

- `-O2` and above; `-O0`/`-O1` are clean.
- Exactly two targets; `-fno-devirtualize-speculatively` or `-fno-devirtualize` silences it.
- Adding `main()` to the same translation unit makes it go away.
- Reproduces from C++11 through C++23.
- GCC 15.3.1 is silent, GCC 16.1.1 and 16.2.1 warn: a regression.

### Minimal reproducer

Written for this report and submitted with it. Judging by its names (the `is_eof`/`bos`/`to_get` virtual interface, `m_zbuf`/`m_ended` in `Wrap`), it was reduced from `runtime_cvt` type-erasing a `zlib_cvt`; that is an inference from the names, as the reduction itself was not recorded.

```cpp
// g++ -O2 -Wall -std=c++17 -c bugreport.cpp
#include <memory>
#include <utility>
#include <exception>

struct AbsImp {
    virtual ~AbsImp() = default;
    virtual bool is_eof() = 0;
    virtual int  bos()    = 0;
    virtual void to_get() = 0;
};

struct Small {
    char m_buf[64]{};
    bool m_eof = false;
    long m_pos = 0;
    bool is_eof() { return m_eof; }
    int  bos()    { return 1; }
    void to_get() { m_buf[2]++; }
};

template <typename K>
struct Wrap : K {
    char m_zbuf[256]{};
    bool m_ended = false;
    bool is_eof() { return m_ended || K::is_eof(); }
    int  bos()    { m_ended = false; return K::bos(); }
    void to_get() { m_zbuf[5]++; K::to_get(); }
};

template <typename K>
struct Imp : AbsImp {
    int m_state = 0;
    K   m_kernel;
    explicit Imp(K k) : m_kernel(std::move(k)) {}
    bool is_eof() override { return m_kernel.is_eof(); }
    int  bos()    override { m_state = m_kernel.bos(); return m_state; }
    void to_get() override { m_state = 2; m_kernel.to_get(); }
};

struct Erased {
    std::unique_ptr<AbsImp> m_ptr;
    template <typename K>
    explicit Erased(K k) : m_ptr(new Imp<K>(std::move(k))) {}

    bool is_eof() { if (!m_ptr) std::rethrow_exception(std::exception_ptr{}); return m_ptr->is_eof(); }
    int  bos()    { if (!m_ptr) std::rethrow_exception(std::exception_ptr{}); return m_ptr->bos(); }
    void to_get() { if (!m_ptr) std::rethrow_exception(std::exception_ptr{}); m_ptr->to_get(); }
};

extern void sink(long);

void small_ops() {
    Erased e{Small{}};
    e.bos();
    sink(e.is_eof());
    e.to_get();
}

void big_ops() {
    Erased e{Wrap<Small>{}};
    e.bos();
    e.to_get();
}
```

With GCC 16 it reports, for `small_ops()`, two instances of `array subscript 'Wrap<Small>[0]' is partly outside array bounds of 'unsigned char [96]'`: the object in `small_ops()` is a 96-byte `Imp<Small>`, yet the `Imp<Wrap<Small>>::to_get()` branch is what got inlined.

Checked on 2026-11-12 with GCC 16.2.1 (Fedora 45, `16.2.1 20261029 (Red Hat 16.2.1-3)`): two warnings by default, none with `-fno-devirtualize-speculatively`.

### What IOv2 does

The repository history has no workaround commit for this bug. A full `gcc-release` build (197 sources) with GCC 16.2.1 on 2026-11-12 did not produce this diagnostic: IOv2's current code does not trigger it, presumably because later changes to cvt mean the call no longer has exactly two targets.

The only error in that build was a different GCC 16 false positive, `-Warray-bounds` from `std::vector<char>::resize(1)` at `-O3`. It involves no virtual call and `-fno-devirtualize-speculatively` does not silence it, so it is unrelated to this bug and is being reported separately.
