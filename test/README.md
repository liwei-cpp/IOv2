# IOv2Test: Build Instructions

[中文](#中文) | [English](#english)

---

## 中文

### 构建系统

本目录的测试由顶层的 CMake/CTest 构建，配置以 preset 形式给出（见仓库根目录的
`CMakePresets.json`）。189 个源文件被分成 **57 个套件**，每个套件对应一个目录，
CTest 中的名字就是该目录的点分形式：`concur`、`facet.collate`、`io.istream.read`。

构建产物一律在 `build/<preset>/` 下，不写进源码树。所有命令都在**仓库根目录**执行，
不是本目录。

### 快速开始

```bash
cmake --preset gcc-release                     # 配置
cmake --build --preset gcc-release --parallel  # 编译全部 57 个套件
ctest --preset gcc-release --parallel          # 运行全部
```

`cmake --list-presets` 列出全部可用配置。

### 预设一览

| preset | 编译选项 | 用途 |
|---|---|---|
| `gcc-release` / `clang-release` | `-O3 -g -DNDEBUG` | 默认，CI 的主力 |
| `gcc-debug` / `clang-debug` | `-g -O0` | 本地调试 |
| `gcc-sanitizer` / `clang-sanitizer` | `-O1 -g` + ASan/UBSan（**无** `NDEBUG`） | 越界、未定义行为 |
| `gcc-tsan` | `-O1 -g` + TSan，只编 `concur` | 数据竞争 |
| `gcc-coverage` | `-O0 -g --coverage` | 覆盖率 |
| `gcc-installed-shared` | release，链接已安装的 `libiov2.so` | 验证安装产物 |
| `gcc-installed-header-only` | release，只用已安装的头文件 | 验证安装产物 |

模式由 `IOV2_TEST_MODE` 决定，不是 `CMAKE_BUILD_TYPE`（后者被强制清空：sanitizer
模式是 `-O1 -g` 且不带 `NDEBUG`，没有任何标准 build type 对应）。

### 只跑一部分

```bash
ctest --preset gcc-release -L io               # 按模块标签：io 下的全部套件
ctest --preset gcc-release -R '^io\.ostream'   # 按名字正则
ctest --preset gcc-release -R '^facet\.collate$' -V   # 单个套件，输出全部
ctest --preset gcc-release --rerun-failed      # 只重跑上次失败的
```

可用标签：`all`（每个套件都有）、七个模块名 `common` `concur` `cvt` `device`
`facet` `io` `locale`，以及 `thread`（只有 `concur`）。

只编不跑某个套件，用目标名（点换成下划线并加 `test_` 前缀）：

```bash
cmake --build --preset gcc-release --target test_facet_collate
```

### Valgrind

```bash
ctest --test-dir build/gcc-release -T memcheck --output-on-failure -L all --parallel
```

注意是 `--test-dir` 而不是 `--preset`：`-T memcheck` 让 ctest 进入 dashboard 模式，
该模式从当前工作目录解析工程目录、忽略 test preset 的 `binaryDir`，给了 `--preset`
反而会找不到 `DartConfiguration.tcl` 而报 “Memory checker not set”。

判据是 **ctest 的退出码**，不是日志：`Testing/Temporary` 下没有任何文件记录缺陷计数。

### 覆盖率

```bash
cmake --preset gcc-coverage
cmake --build --preset gcc-coverage --parallel
ctest --preset gcc-coverage --parallel
lcov --capture --directory build/gcc-coverage --output-file coverage.info \
     --ignore-errors mismatch --ignore-errors inconsistent --ignore-errors negative
lcov --extract coverage.info "${PWD}/include/*" --output-file coverage.info \
     --ignore-errors unused
lcov --list coverage.info
```

`--extract` 的模式必须锚定在仓库路径上。写成 `*/include/*` 会把
`/usr/local/include/c++/...` 一并匹配进来，多出上百个 libstdc++ 头文件。

### ThreadSanitizer

```bash
cmake --preset gcc-tsan
cmake --build --preset gcc-tsan --parallel
ctest --preset gcc-tsan
```

只编 `concur`：其它套件不起线程，TSan 无可观察。test preset 自带 `until-fail` 重复
10 次——竞争只有在调度真的撞上时才会被报出来，跑一次干净说明不了什么。反方向是可靠的：
TSan 是 happens-before 检测器，不会凭空造出竞争，所以报红就是真缺陷。

### 新增一个测试文件

1. 把 `.cpp` 放进它所属的目录。同一目录下的所有 `.cpp` 自动进入该目录对应的套件，
   glob 带 `CONFIGURE_DEPENDS`，不需要登记文件名。
2. 新建目录 = 新建套件，无需额外配置。
3. 新的测试函数要真的被调用，得让它出现在套件入口表里。入口表在 `test/suites.cmake`，
   由脚本生成，**不要手改**：

   ```bash
   python3 test/tools/derive_suites.py          # 重新生成
   python3 test/tools/derive_suites.py --check  # 校验：与源码不符则失败
   ```

4. 重新配置一次（`cmake --preset gcc-release`），CMake 会核对源文件总数并检查每个
   `.cpp` 都归属于某个套件。数量对不上或有文件无人认领，配置阶段直接报错——这两种
   情况都会让测试悄悄变少而 CI 仍然全绿。

入口的顺序来自当年七个聚合 `main()`。那七个文件和几个纯派发文件已经没人编译了，但
脚本还要读它们，所以仍留在树里；详见 `derive_suites.py` 的模块文档字符串。

### 对着已安装的库跑

```bash
make install-shared PREFIX=/opt/iov2                 # 顶层 Makefile 负责打包安装
export PKG_CONFIG_PATH=/opt/iov2/lib/pkgconfig
export LD_LIBRARY_PATH=/opt/iov2/lib
cmake --preset gcc-installed-shared
cmake --build --preset gcc-installed-shared --parallel
ctest --preset gcc-installed-shared --parallel
```

头文件模式同理，用 `make install` 与 `gcc-installed-header-only`。这两种模式下源码树的
`include/` 不参与编译，CMake 会拒绝落在源码树内的 pkg-config 前缀。

### 清理

```bash
rm -rf build/gcc-release   # 单个配置
rm -rf build              # 全部
```

---

## English

### Build System

The tests here are built by the top-level CMake/CTest; configurations are given as
presets (see `CMakePresets.json` in the repository root). The 189 sources are split
into **57 suites**, one per directory, and a suite's CTest name is that directory in
dotted form: `concur`, `facet.collate`, `io.istream.read`.

Build output always goes to `build/<preset>/`, never into the source tree. Run every
command below from the **repository root**, not from this directory.

### Quick Start

```bash
cmake --preset gcc-release                     # configure
cmake --build --preset gcc-release --parallel  # build all 57 suites
ctest --preset gcc-release --parallel          # run them
```

`cmake --list-presets` lists every available configuration.

### Presets

| Preset | Flags | Purpose |
|---|---|---|
| `gcc-release` / `clang-release` | `-O3 -g -DNDEBUG` | default; what CI runs |
| `gcc-debug` / `clang-debug` | `-g -O0` | local debugging |
| `gcc-sanitizer` / `clang-sanitizer` | `-O1 -g` + ASan/UBSan (**no** `NDEBUG`) | out-of-bounds, UB |
| `gcc-tsan` | `-O1 -g` + TSan, builds `concur` only | data races |
| `gcc-coverage` | `-O0 -g --coverage` | coverage |
| `gcc-installed-shared` | release, against an installed `libiov2.so` | validates the install |
| `gcc-installed-header-only` | release, against installed headers only | validates the install |

The mode comes from `IOV2_TEST_MODE`, not `CMAKE_BUILD_TYPE` (which is forced empty:
the sanitizer mode is `-O1 -g` with `NDEBUG` absent, which no standard build type
produces).

### Running a Subset

```bash
ctest --preset gcc-release -L io               # by module label: every suite under io
ctest --preset gcc-release -R '^io\.ostream'   # by name, as a regex
ctest --preset gcc-release -R '^facet\.collate$' -V   # one suite, full output
ctest --preset gcc-release --rerun-failed      # only what failed last time
```

Labels: `all` (on every suite), the seven module names `common` `concur` `cvt`
`device` `facet` `io` `locale`, and `thread` (on `concur` alone).

To build one suite without running it, use its target name -- dots become
underscores, with a `test_` prefix:

```bash
cmake --build --preset gcc-release --target test_facet_collate
```

### Valgrind

```bash
ctest --test-dir build/gcc-release -T memcheck --output-on-failure -L all --parallel
```

`--test-dir`, not `--preset`: `-T memcheck` puts ctest in dashboard mode, which
resolves the project directory from the working directory and ignores a test
preset's `binaryDir`. Given `--preset` it looks for `DartConfiguration.tcl` beside
the sources, does not find it, and fails with "Memory checker not set".

The verdict is **ctest's exit code**, not the log: no file under
`Testing/Temporary` carries the defect count.

### Coverage

```bash
cmake --preset gcc-coverage
cmake --build --preset gcc-coverage --parallel
ctest --preset gcc-coverage --parallel
lcov --capture --directory build/gcc-coverage --output-file coverage.info \
     --ignore-errors mismatch --ignore-errors inconsistent --ignore-errors negative
lcov --extract coverage.info "${PWD}/include/*" --output-file coverage.info \
     --ignore-errors unused
lcov --list coverage.info
```

The `--extract` pattern has to be anchored at the repository. Written as
`*/include/*` it also matches `/usr/local/include/c++/...` and drags in a hundred-odd
libstdc++ headers.

### ThreadSanitizer

```bash
cmake --preset gcc-tsan
cmake --build --preset gcc-tsan --parallel
ctest --preset gcc-tsan
```

Only `concur` is built: no other suite starts a thread, so TSan has nothing to
observe. The test preset repeats it ten times with `until-fail` -- a race is only
reported when the schedule actually exercises it, so one clean run is weak evidence.
The reverse direction is reliable: TSan is a happens-before detector and does not
invent races, so a red result is a real defect.

### Adding a Test File

1. Put the `.cpp` in the directory it belongs to. Every `.cpp` in a directory joins
   that directory's suite automatically; the glob uses `CONFIGURE_DEPENDS`, so no
   file has to be registered by name.
2. A new directory is a new suite, with no further configuration.
3. For a new test function to actually run, it has to appear in the suite's entry
   list. That list lives in `test/suites.cmake`, is generated, and **must not be
   edited by hand**:

   ```bash
   python3 test/tools/derive_suites.py          # regenerate
   python3 test/tools/derive_suites.py --check  # verify against the sources
   ```

4. Configure once more (`cmake --preset gcc-release`). CMake checks the total source
   count against the manifest and that every `.cpp` is claimed by some suite; either
   a wrong count or an unclaimed file is a hard error at configure time. Both are
   ways to silently run fewer tests while CI stays green.

The order of the entries comes from the seven aggregate `main()`s of the old build.
Those seven files and a few dispatch-only ones are no longer compiled by anything,
but the script still reads them, which is why they remain in the tree; see the
module docstring in `derive_suites.py`.

### Running Against an Installed Library

```bash
make install-shared PREFIX=/opt/iov2                 # the top-level Makefile packages and installs
export PKG_CONFIG_PATH=/opt/iov2/lib/pkgconfig
export LD_LIBRARY_PATH=/opt/iov2/lib
cmake --preset gcc-installed-shared
cmake --build --preset gcc-installed-shared --parallel
ctest --preset gcc-installed-shared --parallel
```

Header-only mode is the same with `make install` and `gcc-installed-header-only`. In
both modes the source tree's `include/` takes no part in the build, and CMake refuses
a pkg-config prefix that lies inside the source tree.

### Cleaning

```bash
rm -rf build/gcc-release   # one configuration
rm -rf build               # all of them
```
