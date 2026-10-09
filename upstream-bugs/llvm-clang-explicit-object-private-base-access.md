# LLVM Clang — explicit object member function called through an inaccessible (private) base with a qualified name is accepted

- **Upstream tracker**: <https://github.com/llvm/llvm-project/issues/230610>
- **Upstream project / component**: LLVM / Clang (Sema, access control)
- **Bug ID**: Issue #230610
- **Reported**: 2026-10-09 18:07 UTC by liwei-cpp
- **Last modified upstream**: 2026-10-09 18:07 UTC
- **Current status**: `Open` — submitted, awaiting triage; no labels, no assignee, no comments yet.

[中文](#中文) | [English](#english)

---

## 中文

### 创建时间

2026-10-09 18:07 UTC。前一天在调整 IOv2 标准流的继承方式时发现；当天在 Compiler Explorer 上确认新版本仍未修复、在 LLVM 的 issue 里没有找到现成报告后提交。

### 当前状态

`Open`（已提交，尚未分拣）。目前没有标签、指派人和评论。后续状态以上游链接为准：<https://github.com/llvm/llvm-project/issues/230610>。

### Bug 描述

经私有基类、用限定名调用显式对象成员函数（deducing this，`f(this auto&)`）时，Clang 不报错；同样的调用换成普通成员函数，Clang 会正确报错。

```cpp
namespace N {
struct B {
    int f(this auto&) { return 1; }  // 显式对象成员函数
    int g() { return 2; }            // 隐式对象成员函数
};
}
struct D : private N::B {};

int main() {
    D d;
    return d.N::B::f();  // Clang 放行；应与 d.N::B::g() 一样 ill-formed
}
```

| | Clang | GCC |
|---|---|---|
| `d.N::B::f()`（显式对象） | **放行** | 报错：`'N::B' is an inaccessible base of 'D'` |
| `d.N::B::g()`（隐式对象） | 报错：`cannot cast 'D' to its private base class 'N::B'` | 报错，同上 |

复现范围：Compiler Explorer 上的 Clang 21.1.0、22.1.0、23.1.0 与 trunk，本地 Clang 21.1.8。示例：<https://godbolt.org/z/qexETK6M5>。

依据（当前草案）：

- [dcl.fct]/8：显式对象成员函数是带显式对象参数的**非静态成员函数**。
- [class.access.base]/6：用成员访问运算符访问非静态数据成员或非静态成员函数时，若左操作数（`.` 的情形按指针看）不能隐式转换成指向右操作数「指明类」的指针，该引用 ill-formed；Note 4 说明这条要求叠加在「成员按指明方式可访问」之上。
- 本例的指明类是 `N::B`（名字在它的作用域中找到）；`N::B` 是 `D` 的私有基类，`main` 既非 `D` 的成员也非友元，`D*` 到 `N::B*` 的转换在此不可访问（[class.access.base]/4、[conv.ptr]/3）。

所以 `d.N::B::f()` 与 `d.N::B::g()` 一样 ill-formed。推测 Clang 对显式对象成员函数跳过了这条检查：对象实参绑定到推导为 `D&` 的 `this auto&`，并不发生派生类到基类的转换。但 /6 约束的是成员访问表达式本身，与被调函数的参数类型无关。

### IOv2 的处理

**IOv2 已不依赖这条检查，用户不受影响。**

- 发现经过：曾尝试让 `stdin_api` / `stdout_api` **私有继承** `stream_common_operators`、用 `using` 只暴露需要的接口，以便在编译期挡住 `cin.stream_common_operators::detach()` 这类限定名调用（这些成员全是 deducing this）。gcc 正确拒绝 `cout.IOv2::stream_common_operators::detach()`，clang 却放行，由此发现本 bug。
- 最终方案（`main` 上的 `0e2f4a6d`）：改为 `basic_stream_common_operators<is_std>` 模板，标准流公有继承 `<true>`，其中 `detach` / `attach` / `adjust` 是 **protected** 成员（以 `requires` 区分的一对重载）。成员本身受保护时，「成员按指明方式可访问」这一层就过不去，Clang 会正常检查，gcc 与 clang 都在编译期拒绝限定名调用。
- `test/io/objects/test_io_objects_char.cpp` 的 `static_assert` 在两个编译器上锁住这一点，无需按编译器区分。

---

## English

### Created at

2026-10-09 18:07 UTC. Found the day before while reworking how IOv2's standard streams inherit their common operations; filed after confirming on Compiler Explorer that newer releases still have it and finding no existing LLVM report.

### Current status

`Open` — submitted, awaiting triage; no labels, no assignee and no comments yet. For up-to-date status, see <https://github.com/llvm/llvm-project/issues/230610>.

### Bug description

Clang accepts a qualified call to an explicit object member function (deducing this, `f(this auto&)`) through a private base class, while it correctly rejects the same call to an implicit object member function.

```cpp
namespace N {
struct B {
    int f(this auto&) { return 1; }  // explicit object member function
    int g() { return 2; }            // implicit object member function
};
}
struct D : private N::B {};

int main() {
    D d;
    return d.N::B::f();  // accepted by Clang; should be ill-formed like d.N::B::g()
}
```

| | Clang | GCC |
|---|---|---|
| `d.N::B::f()` (explicit object) | **accepted** | error: `'N::B' is an inaccessible base of 'D'` |
| `d.N::B::g()` (implicit object) | error: `cannot cast 'D' to its private base class 'N::B'` | error: same as above |

Reproduced with Clang 21.1.0, 22.1.0, 23.1.0 and trunk on Compiler Explorer, and locally with Clang 21.1.8. Example: <https://godbolt.org/z/qexETK6M5>.

Grounds (current draft):

- [dcl.fct]/8: an explicit object member function is a **non-static member function** with an explicit object parameter.
- [class.access.base]/6: when a class member access operator accesses a non-static data member or non-static member function, the reference is ill-formed if the left operand (considered as a pointer in the `.` case) cannot be implicitly converted to a pointer to the designating class of the right operand; Note 4 says this is in addition to the member being accessible as designated.
- Here the designating class is `N::B` (the name is found in its scope); `N::B` is a private base of `D`, and `main` is neither a member nor a friend of `D`, so the `D*` to `N::B*` conversion is inaccessible there ([class.access.base]/4, [conv.ptr]/3).

So `d.N::B::f()` is ill-formed for the same reason as `d.N::B::g()`. Clang appears to skip the check for explicit object member functions, perhaps because the object argument binds to `this auto&` deduced as `D&` and no derived-to-base conversion takes place; but /6 constrains the member access expression itself, independent of the parameter type.

### What IOv2 does

**IOv2 no longer depends on this check; users are not affected.**

- How it was found: we tried making `stdin_api` / `stdout_api` inherit `stream_common_operators` **privately**, exposing only what they need through `using`, so that qualified calls such as `cin.stream_common_operators::detach()` would fail to compile (all those members use deducing this). GCC correctly rejected `cout.IOv2::stream_common_operators::detach()`; Clang accepted it, which is how this bug surfaced.
- The final design (`0e2f4a6d` on `main`): a `basic_stream_common_operators<is_std>` template; the standard streams derive publicly from `<true>`, where `detach` / `attach` / `adjust` are **protected** members (a pair of overloads told apart by `requires`). With the member itself protected, the "accessible as designated" check already fails, which Clang does perform, so GCC and Clang both reject the qualified calls at compile time.
- `static_assert`s in `test/io/objects/test_io_objects_char.cpp` pin this down on both compilers, with no per-compiler guard.
