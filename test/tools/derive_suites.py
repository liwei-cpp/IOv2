#!/usr/bin/env python3
"""Derive test/suites.cmake from the hand-written aggregate main()s.

test/Makefile built seven executables, each with a main() that called a fixed
sequence of test functions.  Splitting those into per-feature suites had to
preserve that sequence exactly, so the split is computed from the mains rather
than written out by hand: for every call, walk down until the callee is defined
in a directory that is itself a suite, and record it there in the order it is
reached.  What comes out is a partition -- every function that ran in an
aggregate binary runs in exactly one suite, and in the same relative order.

The Makefile is gone and nothing compiles those mains any more, but they are
still the input here, which is why they are still in the tree.  The obvious
replacement -- take each directory's call-graph roots as its entries -- is not
equivalent: it yields 238 entries against these 235, because three test
functions were written and never called from any main().  Adopting it would
enable them, which changes the coverage line set and so has to be its own change
with its own baseline, not a side effect of deleting a build system.

Run with --check to verify the checked-in manifest still matches the sources.
"""

import argparse
import collections
import os
import re
import sys

TEST_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(TEST_DIR, "suites.cmake")

# The files test/Makefile turned into executables.  Each holds one main().
#
# A directory drops off this list once its last source becomes GoogleTest: the
# cases register themselves, so there is no order left for the main() to fix and
# nothing here left to read.  test/common went first, then test/device, then
# test/concur and test/locale, then test/cvt, then test/facet.
MAIN_SOURCES = [
    "io/test_io.cpp",
]

# Directories that only dispatch: their sole source declares the aggregators of
# the directories below and calls them in order.  Once each of those becomes a
# suite there is nothing left for the dispatcher to do, so it is not a suite and
# its source is not compiled.  It is read, though -- see the module docstring for
# why these files are excluded rather than deleted.
DISPATCH_DIRS = ["cvt", "device", "facet", "io",
                 "io/iostream", "io/istream", "io/ostream"]

DEF_RE = re.compile(r'^(?:void|int)\s+(test_\w+)\s*\(\s*\)\s*(?:\n\s*)?\{', re.M)
CALL_RE = re.compile(r'\b(test_\w+)\s*\(\s*\)\s*;')
# `void test_x();` matches CALL_RE too, and a forward declaration references no
# symbol.  Drop declarations before scanning a whole file for calls.
DECL_RE = re.compile(r'^[ \t]*(?:void|int)\s+test_\w+\s*\(\s*\)\s*;[ \t]*$\n?', re.M)
MAIN_RE = re.compile(r'\bint\s+main\s*\([^)]*\)\s*\{', re.S)
GTEST_RE = re.compile(r'^\s*#\s*include\s*<gtest/gtest\.h>', re.M)


def read(rel):
    with open(os.path.join(TEST_DIR, rel), encoding="utf-8", errors="replace") as f:
        return f.read()


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def all_sources():
    out = []
    for dirpath, dirnames, filenames in os.walk(TEST_DIR):
        dirnames[:] = [d for d in dirnames
                       if not d.startswith(("obj_", "bin_", ".", "tools"))]
        for name in sorted(filenames):
            if name.endswith(".cpp"):
                out.append(os.path.relpath(os.path.join(dirpath, name), TEST_DIR))
    return sorted(out)


def brace_body(text, start):
    """Text between the braces of the block that opens at or after `start`."""
    open_at = text.index("{", start)
    depth = 0
    for i in range(open_at, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[open_at + 1:i]
    raise ValueError("unbalanced braces")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="fail if suites.cmake differs from what the sources imply")
    args = ap.parse_args()

    sources = all_sources()

    # Every nullary test function, and the source that defines it.
    definition = {}
    for src in sources:
        for m in DEF_RE.finditer(strip_comments(read(src))):
            name = m.group(1)
            if name in definition:
                sys.exit(f"{name} defined in both {definition[name]} and {src}")
            definition[name] = src

    suite_dirs = sorted({os.path.dirname(s) for s in sources} - set(DISPATCH_DIRS))

    bodies = {}

    def body_of(name):
        if name not in bodies:
            text = strip_comments(read(definition[name]))
            m = re.search(r'^(?:void|int)\s+%s\s*\(\s*\)' % re.escape(name),
                          text, re.M)
            bodies[name] = brace_body(text, m.end())
        return bodies[name]

    def dispatches_elsewhere(name):
        """True if `name` is a pure dispatcher reaching outside its own suite.

        Such a function has to be expanded: leaving it whole would drag another
        suite's tests into this one and run them twice.
        """
        body = body_of(name)
        calls = CALL_RE.findall(body)
        if not calls or CALL_RE.sub("", body).strip():
            return False
        home = os.path.dirname(definition[name])
        return any(os.path.dirname(definition[c]) != home for c in calls)

    entries = collections.OrderedDict((d, []) for d in suite_dirs)
    placed = {}
    expanded = []

    def walk(name, path):
        if name not in definition:
            sys.exit(f"{name} called from {path[-1]} but never defined")
        home = os.path.dirname(definition[name])
        if home in DISPATCH_DIRS or dispatches_elsewhere(name):
            expanded.append(name)
            for callee in CALL_RE.findall(body_of(name)):
                walk(callee, path + [name])
            return
        if name in placed:
            sys.exit(f"{name} is reached twice: via {placed[name]} and via {path}")
        placed[name] = list(path)
        entries[home].append(name)

    top_level = []
    for main_source in MAIN_SOURCES:
        text = strip_comments(read(main_source))
        for call in CALL_RE.findall(brace_body(text, MAIN_RE.search(text).end() - 1)):
            top_level.append(call)
            walk(call, [main_source])

    # The point of the split is that nothing changes about what runs.  Expanding a
    # dispatcher is only sound if its body was calls and nothing else, so check
    # that directly, then check that the two sets of reachable functions agree.
    def closure(roots):
        seen, stack = set(), list(roots)
        while stack:
            name = stack.pop()
            if name in seen:
                continue
            seen.add(name)
            stack += [c for c in CALL_RE.findall(body_of(name)) if c in definition]
        return seen

    for dispatcher in expanded:
        if CALL_RE.sub("", body_of(dispatcher)).strip():
            sys.exit(f"{dispatcher} was expanded but does more than dispatch")
    before = closure(top_level) - set(expanded)
    after = closure(sum(entries.values(), []))
    if before != after:
        sys.exit("suites do not run the same functions as the aggregate main()s: "
                 f"dropped {sorted(before - after)}, added {sorted(after - before)}")

    # Sources CMake must not compile: the aggregate mains, and the files left holding
    # nothing but dispatchers the walk expanded away.  Nothing calls such a
    # dispatcher any more, and its body still calls functions that now link into
    # other suites, so compiling it would only break the link.
    defined_in = collections.defaultdict(set)
    for name, src in definition.items():
        defined_in[src].add(name)
    dead = set()
    for src, names in defined_in.items():
        live = names - set(expanded)
        if not live:
            dead.add(src)
        elif live != names:
            sys.exit(f"{src} defines both the expanded dispatcher(s) "
                     f"{sorted(names - live)} and tests that still run "
                     f"{sorted(live)}; it can be neither compiled nor excluded")
    excluded = sorted(set(MAIN_SOURCES) | dead)

    # Each suite links only its own directory, so a compiled source may only call
    # test functions that land in the same suite.  The linker enforces this, but
    # only after a full build; checking it here names the offending call instead.
    for src in sources:
        if src in excluded:
            continue
        home = os.path.dirname(src)
        for callee in CALL_RE.findall(DECL_RE.sub("", strip_comments(read(src)))):
            target = definition.get(callee)
            if target is None or target == src:
                continue
            if target in excluded:
                sys.exit(f"{src} calls {callee}(), defined in {target}, "
                         "which no suite compiles")
            if os.path.dirname(target) != home:
                sys.exit(f"{src} calls {callee}(), which compiles into "
                         f"{suite_name(os.path.dirname(target))} rather than "
                         f"{suite_name(home)}; that suite would not link")

    # Which suites are GoogleTest.  CMake needs the list to link gtest and to
    # generate the right main(), and deriving it here keeps it from drifting out
    # of step with the sources the way a hand-kept list would.
    gtest_dirs = sorted({os.path.dirname(src) for src in sources
                         if src not in excluded and GTEST_RE.search(read(src))})

    text = render(suite_dirs, gtest_dirs, entries, excluded, sources)

    if args.check:
        current = open(MANIFEST, encoding="utf-8").read() if os.path.exists(MANIFEST) else ""
        if current != text:
            sys.exit("suites.cmake is stale; re-run test/tools/derive_suites.py")
        print(f"suites.cmake matches: {len(suite_dirs)} suites, "
              f"{len(placed)} entry points, {len(sources) - len(excluded)} sources")
        return

    with open(MANIFEST, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"wrote {MANIFEST}: {len(suite_dirs)} suites, {len(placed)} entry points, "
          f"{len(sources) - len(excluded)} of {len(sources)} sources compiled")


def suite_name(directory):
    return "test_" + directory.replace("/", "_")


def render(suite_dirs, gtest_dirs, entries, excluded, sources):
    lines = [
        "# Generated by test/tools/derive_suites.py -- do not edit by hand.",
        "#",
        "# One entry per suite: the directory it is built from, and the functions its",
        "# generated main() calls, in the order the aggregate main() it came from",
        "# reached them.  Re-run the script after adding, removing or renaming a test",
        "# function; `--check` verifies this file still matches the sources.",
        "",
        "set(IOV2_TEST_SUITE_DIRS",
    ]
    lines += [f"    {d}" for d in suite_dirs]
    lines += [")", ""]
    lines += [
        "# Suites whose sources are GoogleTest.  A directory appears here as soon as",
        "# its first source is converted, not when the last one is: the generated",
        "# main() can hold TEST()s and the remaining hand-written entry points at the",
        "# same time, which is what lets a directory be converted one file at a time.",
        "set(IOV2_TEST_GTEST_SUITE_DIRS",
    ]
    lines += [f"    {d}" for d in gtest_dirs]
    if not gtest_dirs:
        lines.append("    # none yet")
    lines += [")", ""]
    lines += [
        "# Sources nothing compiles: the aggregate main()s that test/Makefile",
        "# built, and the dispatch-only files whose callers they were.  They remain in",
        "# the tree because derive_suites.py reads them -- they are what fixes the",
        "# entries below and their order.  See its module docstring.",
        "set(IOV2_TEST_EXCLUDED_SOURCES",
    ]
    lines += [f"    {s}" for s in excluded]
    lines += [")", ""]
    lines.append(f"set(IOV2_TEST_SOURCE_TOTAL_EXPECTED {len(sources)})")
    lines.append("")
    for d in suite_dirs:
        lines.append(f"set({suite_name(d)}_ENTRIES")
        for name in entries[d]:
            lines.append(f"    {name}")
        if not entries[d]:
            lines.append("    # none: every case registers itself" if d in gtest_dirs
                         else "    # none: this suite is compile-time only")
        lines.append(")")
        lines.append("")
    return "\n".join(lines)


if __name__ == "__main__":
    main()
