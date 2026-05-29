# GCC — `-O2/-O3` miscompiles pointer→string→pointer round-trip (IPA escape analysis loses the equality)

- **Upstream tracker**: <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=121814>
- **Upstream project / component**: GCC / optimizer (IPA modref + points-to / tree-optimization). The `Blocks:` field is set to `modref`, and maintainer analysis (comments 13–14) places the root cause in IPA escape tracking.
- **Bug ID**: PR 121814
- **Reported**: 2025-09-05 20:20 UTC
- **Last modified upstream**: 2026-02-06 11:26 UTC
- **Current status**: `NEW` — confirmed by GCC maintainers (Richard Biener in comment #13, Jan Hubicka in comment #14). No fix yet; not yet assigned to anyone. Importance `P3 / normal`.

[中文](#中文) | [English](#english)

---

## 中文

### 创建时间

2025-09-05 20:20 UTC(由 IOv2 作者在最小化复现并准备好两个 attachment 文件后,提交至 GCC Bugzilla)。

### 当前状态

`NEW` —— 上游已确认。Richard Biener(comment #13)和 Jan Hubicka(comment #14)分别介入分析过,基本结论是:这是 IPA(过程间分析)中 escape / points-to 信息丢失的问题。目前尚未分配 owner,也尚无补丁。最新进展请参考上游链接:<https://gcc.gnu.org/bugzilla/show_bug.cgi?id=121814>。

主要时间线:

- 2025-09-05:Jonathan Wakely 先指出复现样例本身有越界(`__ilen` 应为 `__len`、`__s += c` 应为 `__s += *c`),提交者在 comment #8 给出修正后的版本,**bug 在修正后仍然以异常形式复现**。
- 2025-09-08:Richard Biener 给出 GIMPLE dump 分析,指出 `if (p == q) ...` 在 IPA 层缺少 unification 约束,导致指针等价比较之后,原指针在数据流上"看起来没有 escape"。
- 2026-02-06:Jan Hubicka 复测,**确认即使加上 `-fno-ipa-modref` 同样会失败**,并定位到根因:`_2 = ss_out_atoms[_1]` 这种"以待分析 SSA 值作为数组下标的载入"没有被当作"可能的 escape"来处理,所以分析器认为指针整数表示始终被困在函数内部,从而把后续的 `if (pi != po)` 误判为恒真。

### Bug 描述

复现路径(简化版):

1. 一个 `put(std::string& s, const void* vv)` 把指针 `vv` 转成 `uintptr_t`,再按十六进制逐位查表(`__u & 0xf` 作为常量数组 `ss_out_atoms[16]` 的下标),最终把 `"0x...."` 形式的串拼到 `s` 后。
2. 一个 `get(...)` 反向操作:从字符串里逐字符在 `s_in_atoms[16]` 中 `std::find`,把每个 hex 字符还原成 4 bit,组装回 `uintptr_t`,再 `reinterpret_cast<void*>` 拿回指针。
3. `main()` 对同一个 `int i` 做一轮 `po → put → get → pi`,然后 `if (pi != po) throw std::runtime_error("check fail");`。

在 `g++ -O0` 下程序正常退出;在 `g++ -O2` 或 `-O3` 下程序总是抛出 `check fail`,**即使 `pi` 与 `po` 在位级别上是同一个值**。增加一行 `std::cout << pi << ' ' << po`(强制让指针的字节真正离开函数)就足以让异常消失,这是一个典型的优化器 escape 信息错误的征兆。

Richard Biener(comment #13)指出关键点:

> 我们按字节存储指针,然后再按字节抽取回来;但抽取过程里 `if (p == q) ... use q ...` 这种"通过等值比较得到的别名"在 IPA 约束系统里没有 unification,我们把这层关联丢掉了。modref 也是同样的问题。

Jan Hubicka(comment #14)进一步定位:

> 分析没有把 `_2 = ss_out_atoms[_1]` 这种"用待分析 SSA 名作为地址的一部分的载入"当作可能的 escape。正确的修法应该是:让带有该 SSA 名作为下标的载入,被视为可能把该值传到 RHS。

由此带来的实际影响是:在 IPA 看来,`__vv` 这个参数从未 escape,所以"`pi` 与外面任何东西都无关",`if (pi != po)` 这种比较里 `pi` 的取值域被错误地收窄到"`{nonlocal, escaped, null}` 的一个子集且不包含 `&i`",于是比较结果在编译期就被定死。

### 关键链接

- 标准角度的"指针 round-trip 是否合法"讨论(Andrew Pinski 在 comment #6、#11 中给出):
  - <https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3292r0.html>
  - <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2219.htm>
- 与之相关的 libstdc++ 测试,本复现样例正是其精简化:
  - <https://github.com/gcc-mirror/gcc/blob/41ea9305466ce54027324258aeae9893101941db/libstdc%2B%2B-v3/testsuite/27_io/basic_istream/extractors_arithmetic/char/01.cc#L104>

### IOv2 的处理

**这是一个编译器优化器(IPA escape / points-to)的缺陷,不是库代码的缺陷。IOv2 暂时没有可靠的本地规避手段,只能跟踪上游修复。**

理由:

- 该 bug 的根因位于 GCC 的 IPA 分析,而非 libstdc++。把对应代码放到任何库里都会以同样方式被错误优化。
- 用户层最常被尝试的"开关"`-fno-ipa-modref` 已经被 Jan Hubicka 在 comment #14 当场否决 —— `-fno-ipa-modref` 下仍然复现。
- 加 `volatile`、`asm volatile` barrier、`std::launder` 之类的"挡板"都属于黑魔法,既不便携、也无法在通用 header-only 库里默认开启。
- 把内部使用的指针打印/重建逻辑改成"避免 round-trip"虽然可行,但 IOv2 对外的 `numeric` facet 必须支持 `void* / const void*` 的格式化与读回(这是标准 `num_get` / `num_put` 的合同),无法在不破坏接口语义的前提下绕开该模式。

因此,目前 IOv2 在该问题上的策略是:**追踪上游进展、不在自身代码里引入"看起来在规避问题、实则不可靠"的临时改写**。等待 GCC 修复后,使用 IOv2 + 修复后的 GCC 版本,问题自然消失。

---

## English

### Created at

2025-09-05 20:20 UTC. Filed with GCC Bugzilla by the IOv2 author after producing a minimal reproducer (the two attached `.cpp` files on the Bugzilla page).

### Current status

`NEW` — confirmed by GCC maintainers. Richard Biener (comment #13) and Jan Hubicka (comment #14) both engaged and located the issue inside the IPA escape / points-to analysis. Not yet assigned to an owner; no patch yet. Importance `P3 / normal`. For up-to-date status, see <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=121814>.

Timeline highlights:

- 2025-09-05: Jonathan Wakely first pointed out unrelated bugs in the reproducer itself (`__ilen` should be `__len`, `__s += c` should be `__s += *c`, plus reserved-identifier usage). The reporter posted a corrected attachment in comment #8 — **the bug still reproduces with that fixed version**, so the issue is real and not an artifact of the original buggy test.
- 2025-09-08: Richard Biener (comment #13) gave a GIMPLE-level analysis: equality compares (`if (p == q) ... use q ...`) lack a unification constraint in IPA, so the connection between the round-tripped representation and the original pointer is lost. Same root cause for modref.
- 2026-02-06: Jan Hubicka (comment #14) re-tested and **confirmed the failure reproduces even under `-fno-ipa-modref`**, then narrowed the root cause: a load whose address contains the analyzed SSA name as a subscript (`_2 = ss_out_atoms[_1]`) is not currently treated as a possible escape, so the analyzer believes the pointer's integer encoding never leaves the function.

### Bug description

The reduced reproducer (simplified shape):

1. `put(std::string& s, const void* vv)` converts the pointer to `uintptr_t` and emits its hex representation by indexing a 16-character lookup table (`__u & 0xf` → `ss_out_atoms[...]`), producing `"0x...."` and appending it to `s`.
2. `get(...)` reverses the operation: for each character it uses `std::find` against `s_in_atoms[16]` to recover a nibble, assembles back into `uintptr_t`, and casts to `void*`.
3. `main()` round-trips a `int i` through `po → put → get → pi`, then `if (pi != po) throw std::runtime_error("check fail");`.

Under `g++ -O0` the program exits cleanly. Under `g++ -O2` or `-O3` the program always throws `check fail`, **even though `pi` and `po` are bitwise identical**. Adding a `std::cout << pi << ' ' << po` line (which forces the bytes of the pointer to actually leave the function) is enough to make the exception vanish — a classic signature of escape-information loss in the optimizer.

Richard Biener (comment #13), key sentences:

> We track pointers as you store them piecewise, but when you then extract them by `if (p == q) ... use q ...`, we lose the association. … In principle for the constraint-based points-to any equality compare would need a unification constraint. Likewise modref would need to handle things this way.

Jan Hubicka (comment #14), narrowing the fix direction:

> The analysis does not consider `_2 = ss_out_atoms[_1]` as a possible escape. So the right fix should be to handle loads with an address containing a given SSA name as possibly passing the value to RHS.

The practical effect: from IPA's point of view, the function parameter `__vv` never escapes, so `pi`'s value-set is incorrectly narrowed to something that cannot contain `&i`. The comparison `if (pi != po)` is then folded as if it were unconditionally true.

### Related references

- Standards-side discussion of whether the pointer-round-trip pattern is well-defined (Andrew Pinski raised these in comments #6 and #11):
  - <https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3292r0.html>
  - <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2219.htm>
- A libstdc++ regression test that exercises the same pattern (the original reproducer was reduced from this):
  - <https://github.com/gcc-mirror/gcc/blob/41ea9305466ce54027324258aeae9893101941db/libstdc%2B%2B-v3/testsuite/27_io/basic_istream/extractors_arithmetic/char/01.cc#L104>

### What IOv2 does

**This is a compiler-optimizer defect (IPA escape / points-to), not a library defect. IOv2 has no reliable local workaround and is simply waiting for upstream to fix it.**

Reasons:

- The root cause lives in GCC's IPA analysis, not in libstdc++. Putting the same code into any library reproduces the same miscompile.
- The most obvious user-facing knob, `-fno-ipa-modref`, has been explicitly disqualified by Jan Hubicka in comment #14 — the failure reproduces even with that flag.
- Local workarounds (`volatile`, `asm volatile` barriers, `std::launder`, output-side prints to force escape) are all fragile and unportable, and none of them is appropriate as a default inside a general header-only library.
- Reshaping IOv2's internal logic to "avoid the pointer round-trip" is not an option for the affected surface: IOv2's `numeric` facet must support formatting and reading back `void*` / `const void*` to fulfill the standard `num_get` / `num_put` contract; that *is* a round-trip by design.

The current IOv2 stance on this bug is therefore: **track upstream progress; do not bake "looks-like-a-workaround, actually-unreliable" ad-hoc rewrites into the codebase.** Once GCC's IPA escape tracking is fixed, IOv2 built with a fixed GCC will simply stop miscompiling — no library change required.
