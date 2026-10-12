# Notes

[中文](#中文) | [English](#english)

---

## 中文

本目录存放 IOv2 开发过程中的记录，**不属于库本身**：

- 构建、测试、文档生成都不读取这里的任何文件；
- `make install` / `make install-shared` 不安装它；
- 仓库根目录的 `.gitattributes` 将其标为 `export-ignore`，因此 `git archive`
  与 GitHub 生成的源码包中也不包含它。

打包 IOv2 的发行版时，不应包含本目录。

### `checked.md`

审查的**工作记录**：列出已经审查过、结论为"当前代码在这一点上是正确的"的检查项，
供后续审查跳过，避免重复劳动。

它是写给审查者的底稿，而不是面向使用者的文档：结论对应写作时的代码，条目中的行号
会随代码改动漂移，引用的探针源码也已不再保留。文件开头说明了它的范围与读法。

### `upstream-bugs/`

**跟踪相关系统的状态**：IOv2 在所依赖或与之交互的项目（GCC、LLVM 等）中发现并上报
的缺陷，以及它们在上游的处理进展和 IOv2 这边的应对。详见该目录下的 `README.md`。

---

## English

This directory holds records kept while developing IOv2. **It is not part of the
library**:

- nothing here is read by the build, the tests or the documentation;
- `make install` / `make install-shared` do not install it;
- the repository's `.gitattributes` marks it `export-ignore`, so `git archive`
  and the source archives GitHub generates leave it out as well.

A distribution package of IOv2 should not include this directory.

### `checked.md`

A **working record** of review: the checks already carried out whose conclusion
was "the current code is correct on this point", so later reviews can skip them
instead of repeating the work. It is written in Chinese.

It is a reviewer's worksheet, not user documentation: each conclusion refers to
the code as it was when written, line numbers drift as the code changes, and the
probe sources it mentions are no longer kept. Its opening section explains its
scope and how to read it.

### `upstream-bugs/`

**Tracks the state of related systems**: defects IOv2 found and reported in
projects it depends on or interacts with (GCC, LLVM and others), how upstream is
handling them, and what IOv2 does in the meantime. See the `README.md` in that
directory.
