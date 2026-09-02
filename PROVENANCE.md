# Provenance

This file records how IOv2 was audited for third-party material, what was
found, what was replaced, what was deliberately kept, and what is known to
remain. It exists so that the MIT licence declared in `LICENSE` can be checked
rather than taken on trust.

It is a working record, not a legal opinion.

---

## 1. What was found, and when

The early history of this repository contains code, comments and identifier
naming taken or adapted from the **GNU ISO C++ Library (libstdc++)** and, in
the date/time facets, from the **GNU C Library (glibc)**. The root commit
`4c336f0` (2026-04-03) is a 244-file bulk import; the repository has no
pre-history, so this material is present from the first commit onwards.

The author disclosed this on 2026-08-24. The affected files were:

| File | Derived from |
| --- | --- |
| `facet/numeric.h` | `num_get` / `num_put`: the two-stage formatting structure, the `boolalpha` longest-match loop, integer and floating-point field extraction, decimal grouping, and the LWG defect-resolution comments |
| `facet/facet_helper.h` | `add_grouping` ← `__add_grouping`, `verify_grouping` ← `__verify_grouping`, including their explanatory comments |
| `facet/monetary.h` | `money_get` / `money_put` and its parse state (`testvalid`, `testdecfound`, `sign_size`, `last_pos`) |
| `facet/monetary_details.h` | `s_construct_pattern` ← `_S_construct_pattern` (`config/locale/gnu/monetary_members.cc`) |
| `facet/timeio.h` | `day_of_the_week` and `extract_num` ← `_M_extract_num`, plus roughly ten verbatim glibc comments |
| `facet/timeio_details.h` | `parse_glibc_era_entries` ← glibc `time/era.c` |
| `cvt/code_cvt.h` | UTF-8 decoding bias constants |
| `test/**` | Test fixtures and expected-value tables carried over from the libstdc++ testsuite |

Upstream terms: libstdc++ is `GPL-3.0-or-later WITH GCC-exception-3.1`; glibc
is `LGPL-2.1-or-later`. The GCC Runtime Library Exception covers Target Code
produced by an Eligible Compilation Process. It grants no permission to
redistribute libstdc++ *source* under more permissive terms, and none is
claimed here.

## 2. Comparison baseline

* **libstdc++ headers**: `/usr/include/c++/15`, from GCC 15.3.1 20260722
  (Red Hat 15.3.1-1). The `.tcc` files matter — the implementations of
  `num_get`, `num_put`, `money_get`, `money_put` and `time_get` live there,
  not in the `.h` files.
* **libstdc++ sources**: 31 files from `gcc-mirror/gcc`, branch
  `releases/gcc-15` — `src/c++98/locale_facets.cc`, `src/c++11/locale_init.cc`,
  `src/c++11/localename.cc`, and, importantly,
  `config/locale/gnu/*.cc` (`monetary_members.cc`, `time_members.cc`,
  `numeric_members.cc`, `c_locale.cc`, `ctype_members.cc`, …).
* **glibc sources**: 13 files from `bminor/glibc`, branch `release/2.42/master`
  — `time/strptime_l.c`, `time/strftime_l.c`, `time/era.c`, `time/mktime.c`,
  `locale/C-time.c`, and others.

Comparing against the installed headers alone is not sufficient: a large part
of the real implementation is not in the installed headers.

Two attributions raised on suspicion were checked and **excluded**: the Rust
`chrono` / `time` / `strftime-ruby` crates (hit counts indistinguishable from
the unrelated-file baseline; the one comment hit traces to the C standard and
cppreference, not to Rust — IOv2 uses cppreference's en dash and a 1-based
month convention that C does not), and Arnold Robbins' `strftime.c` (which
self-declares as public domain in any case; exact and fuzzy comment matching
both scored zero, and the two most characteristic functions use unrelated
algorithms).

## 3. Method

Reading files by eye misses things. The audit used three mechanical layers.

1. **Normalised token 8-gram containment.** Comments and string literals
   stripped; identifiers lowercased and stripped of underscores — without that
   normalisation, `__testvalid` does not match `testvalid` and a rename alone
   defeats the detector. Scanned as a sliding local window rather than
   whole-file, so a single transliterated function inside a large original file
   still shows up.
2. **Whole-line comment matching**, normalised, exact. This turned out to be
   the single most decisive layer.
3. **String-literal matching** for literals of eight characters or more.

A fuzzy variant of layer 2 — comment word 5-grams, stop-words removed, three or
more content words — was used as a backstop.

**Calibration.** Percentages mean nothing without a scale, and anything in the
10–20 % band is inconclusive on the number alone:

| Population | Median containment | Spread |
| --- | --- | --- |
| GCC's own `char` / `wchar_t` twin files | 72.9 % | p10 = 53.4 % |
| Unrelated file pairs | 1.6 % | p90 = 6.4 %, max = 26.6 % |

**A positive control is mandatory.** Every scan was run twice: once against the
current tree and once against the pre-rewrite files recovered with `git show`.
Without the control there is no way to distinguish "the tree is clean" from
"the detector is broken."

**What decides a finding.** Algorithmic similarity is weak evidence and is
usually merger — two implementations of one specification converge. What
actually settles a case is *meaningless residue at the expression layer*. Four
kinds were decisive here:

1. **A comment referring to a variable that does not exist.** `day_of_the_week`
   carried "the difference between this data in the one on TM" — there is no
   `TM` in the function. glibc's function of the same name takes
   `struct tm *tm`.
2. **A reproduced typo.** "this **data in** the one" is ungrammatical; it should
   read "this date and the one". The typo has been in glibc for years and
   arrived verbatim in the root commit. A later hygiene pass, `6a391ae`
   (2026-06-13), quietly fixed the grammar — which is itself evidence of
   copy-then-edit rather than independent authorship.
3. **Redundant parentheses from transliterating a macro.** `isleap` kept
   `(year) % 4 == 0` after being turned into a function, because glibc's
   original is `#define isleap(year) ((year) % 4 == 0 && ...)`.
4. **Types and magic numbers out of keeping with the file.**
   `unsigned short int` in a file that otherwise uses `uint16_t`; the `-473` and
   `(corr_year / 4) % 25 < 0` truncating-division correction terms.

**Renaming is not a fix.** Commit `e0c82f4` had already stripped `__` prefixes
once, and that is exactly the pattern the normalisation in layer 1 is designed
to see through. Where a construct had to go, it was re-derived from the
standard's wording with the upstream file closed, not renamed.

## 4. Results

Whole-tree rescan, 2026-08-27, repeated after the test-tree work on 2026-09-01:

| Measurement | Positive control (pre-rewrite) | Current tree |
| --- | --- | --- |
| 8-gram hits, `facet/monetary.h` | 222 | ≤ 3 |
| 8-gram hits, `facet/numeric.h` | 206 | ≤ 3 |
| Comment whole-line exact matches | 27 | **0** |
| Comment 5-gram fuzzy matches | 138 | 9 |
| glibc corpus, n = 8 | — | **0** |
| Worst containment, `test/io/ostream` | — | 1.8 % |
| Worst containment, rest of `test/io` | — | 2.6 % |

The nine remaining fuzzy comment matches are generic English or citations of
the standard (`LWG 23 (Num_get overflow result)`, `22.2.2.2.2`). Citing a
defect report by its number is the correct way to write such a comment and has
nothing to do with libstdc++'s `_GLIBCXX_RESOLVE_LIB_DEFECTS` marker style.

On the libstdc++ `.cc` side only `isleap` and
`m_widen[static_cast<unsigned char>(c)]` still match, both merger.

Four generic string literals remain in the test tree (`"abcdef"` and
similar). They are not distinctive.

Five directories — `common/`, `cvt/`, `device/`, `io/`, `locale/` — were found
clean on their own terms: `locale.h`, `streambuf.h`, `io_base.h` and
`streambuf_iterator.h` are independent designs with no relation to libstdc++'s
`_Impl` / `_M_word` / `_M_sbuf`, and the `.mo` reader in `messages_details.h`
is original.

## 5. Where the work was done

| Commit | Date | What |
| --- | --- | --- |
| `16b5a05` | 2026-08-27 | Rewrote the libstdc++/glibc-derived passages in the facet headers |
| `9841634` | 2026-08-27 | Cleared the clang-tidy warnings the rewrite introduced |
| `919d9a8` | 2026-08-27 | Rewrote era-year parsing (`parse_glibc_era_entries`) |
| `7e4bd82` | 2026-08-27 | Reconstructed backward era years |
| `5a2c2e0` … `ab5ddb4` | 2026-08-28 – 2026-08-31 | Converted all 57 test suites to GoogleTest, re-deriving the fixtures rather than porting them |
| `b40a29d` | 2026-09-01 | Re-derived the constructs that still resembled libstdc++: the `code_cvt.h` bias constants, the `numeric.h` integer parser, the remaining test fixtures |

## 6. Deliberately unchanged

Rewriting the following would make the code worse without making it more
original, because there is only one way to say them:

* **ISO C++ requirements** — the names, signatures and required order of
  operations for the locale facets. The Stage 1 / Stage 2 / Stage 3 structure
  of `num_put` is prescribed by `[facet.num.put.virtuals]`, not copied.
* **POSIX requirements** for `strptime` / `strftime` — the `switch` over the
  conversion letter, the `%j` bounds `(1, 366, 3)`, the `%m` bounds
  `(1, 12, 2)`, the `%y` 69/99 century split, the four accepted `%z` forms, the
  cumulative month-length table, and the leap rule.
* **glibc's era data layout**, read through `nl_langinfo`. This is a
  description of an interface, not an expression of one; there is no way to
  "rewrite" an ABI you must match. The reference comment naming glibc's
  `era.c` is retained on purpose, as interface documentation.

A second LLM review of `9841634` raised five items as blockers. Four were the
categories above — the `insert_float` / `insert_int` / `pad_impl` skeletons and
the era binary decode — and were not defects. The two genuine leftovers it
found in `group_float` / `convert_to_v`, a redundant signature and one variable
name, were cleared.

## 7. The one behaviour change

`day_of_the_week` previously used glibc's truncating division, which is wrong
for `year <= 0`: `0000-01-01` came out as Sunday when it is a Saturday. The
rewrite shifts through the 400-year cycle before dividing. For `year >= 1`
every value is unchanged, verified by exhaustive comparison over the whole
32-bit year range. Only the `%U` / `%W` week-number derivation is affected.

No other rewrite changes observable behaviour.

## 8. How equivalence was verified

* **Small pure functions**: exhaustive differential testing of the old and new
  implementations side by side. `day_of_the_week` was run over all 2^32 years.
* **Large facets**: a golden corpus replayed through both implementations and
  reduced to a streaming FNV-1a digest — 14.68 million cases for `monetary`.
* **Performance**: callgrind instruction counts, never wall-clock.
* **Regression**: all 57 suites pass under CTest.

## 9. Known residual

One fingerprint is still present, and is recorded here rather than hidden.

`facet/numeric.h` carries the digit-atom tables:

```cpp
constexpr std::string_view in_atoms  = "-+xX0123456789abcdefABCDEF";
constexpr std::string_view out_atoms = "-+xX0123456789abcdef0123456789ABCDEF";
```

Both strings are byte-identical to libstdc++'s `_S_atoms_in` and
`_S_atoms_out`, and the members and index constants correspond one-to-one after
the normalisation of §3 — `m_in_atoms` / `m_out_atoms` against `_M_atoms_in` /
`_M_atoms_out`, and `s_ominus`, `s_oplus`, `s_ox`, `s_oX`, `s_odigits`,
`s_oudigits` against `_S_ominus`, `_S_oplus`, `_S_ox`, `_S_oX`, `_S_odigits`,
`_S_oudigits`.

The character *set* is dictated by the language: these are exactly the
characters an integer field can contain across bases 8, 10 and 16, with sign
and base prefix. The *order*, and the duplication of `0123456789` in the output
table so that two contiguous sixteen-character alphabets can be indexed by a
single case flag, is a design choice that was adopted rather than invented.

It has not been renamed, because renaming is concealment rather than
independence — see §3. Removing it properly means changing the design, not the
identifiers.

## 10. Limitations of this record

**This is not a clean-room reconstruction.** The author has read libstdc++'s
source. What this document records is an independent rewrite with equivalence
verification and a provenance audit — a weaker claim than clean-room, and the
accurate one.

**Git cannot corroborate any of it.** `git log -S` reaches only the root commit
`4c336f0`; there is no pre-history in the repository, so version control can
neither confirm nor refute where the original material came from. The internal
evidence described in §3 is the whole of the case.

**Detection has a floor.** The calibration in §3 shows unrelated files reaching
26.6 % containment at the extreme. A sufficiently thorough paraphrase would
score inside the noise. The claim made here is that everything the described
method could find has been found and dealt with, not that nothing could exist
below it.

---

## 中文摘要

本文件记录 IOv2 的第三方材料审计：发现了什么、替换了什么、有意保留了什么、
已知还剩什么。目的是让 `LICENSE` 中声明的 MIT 许可可被核查，而非仅凭信任。
本文件是工作记录，不是法律意见。

* **§1 发现**：本仓库早期历史含有取自 libstdc++ 与 glibc 的代码、注释与命名，
  自根提交 `4c336f0`（2026-04-03，244 文件批量导入）起即存在，作者于
  2026-08-24 披露。涉及 8 处，见上表。
* **§2 对照基准**：libstdc++ 15.3.1 的头文件（`.tcc` 是关键）＋ `gcc-15` 分支
  31 个源文件 ＋ glibc `release/2.42/master` 13 个源文件。已排除 Rust
  chrono/time 与 Robbins 的 `strftime.c` 两个来源。
* **§3 方法**：三层机器复扫——归一化 token 8-gram containment（标识符去下划线
  转小写，否则改名即可骗过检测）、注释整行精确匹配、≥8 字符串字面量匹配。
  必须配阳性对照，否则分不清"干净"与"探测器坏了"。定案靠的是**表达层的无意义
  残留**（指向不存在变量的注释、复现的笔误、宏直译的冗余括号、风格突兀的类型
  与魔数），不是算法相似——后者通常属 merger。
* **§4 结果**：注释整行精确匹配 27 → **0**；`monetary.h` 8-gram 命中 222 → ≤3，
  `numeric.h` 206 → ≤3；glibc 侧零命中；测试树最高 containment 2.6 %。
* **§5 落地提交**：`16b5a05`、`9841634`、`919d9a8`、`7e4bd82`、gtest 转换系列、
  `b40a29d`。
* **§6 有意不改**：ISO C++ 规定的 facet 接口与阶段结构、POSIX 规定的
  strptime/strftime 语义与边界、glibc 纪元数据的 ABI（属接口描述，无从"重写"）。
* **§7 唯一行为变更**：`day_of_the_week` 在 `year <= 0` 时的取整错误已修，
  `year >= 1` 逐值不变（全 2^32 穷举验证）。
* **§8 等价性验证**：小纯函数穷举差分；大 facet 用 golden corpus ＋ 流式 FNV-1a
  摘要（monetary 1468 万条）；性能用 callgrind 指令数；57/57 套件通过。
* **§9 已知残留**：`numeric.h` 的 `in_atoms` / `out_atoms` 两张表与 libstdc++ 的
  `_S_atoms_in` / `_S_atoms_out` 逐字节相同，成员与索引常量在归一化后一一对应。
  字符集本身由语言决定，但顺序与输出表重复 `0123456789` 的编码手法是沿用而非
  独立设计。未改名，因为改名是隐藏而不是独立；要真正去除须改设计。
* **§10 局限**：这**不是** clean-room——作者读过 libstdc++ 源码；准确的说法是
  "独立重写 ＋ 等价性验证 ＋ 来源审计"。Git 只能追到根提交，无法佐证。检测有
  下限：无关文件极端值可达 26.6 % containment，足够彻底的改写会落在噪声内。
