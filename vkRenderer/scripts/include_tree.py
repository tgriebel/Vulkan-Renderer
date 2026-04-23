#!/usr/bin/env python3
"""
include_tree.py  —  Print a project include dependency tree.

Usage:
    python include_tree.py [root_file]  [--all]  [--cycles]  [--no-ext]

    root_file   Entry point to trace from (default: src/render_core/renderer.h).
                Relative to the vkRenderer directory.

    --all       Print a tree rooted at every .h file instead of one root.
    --cycles    Only report circular include chains.
    --no-ext    Suppress external / third-party headers (Vulkan, GfxCore, SysCore, STL).

Reads all .h files under the vkRenderer/ tree.  External headers that cannot
be resolved to a file on disk are shown in brackets, e.g. [SysCore/timer.h].
"""

import os
import re
import sys
import argparse
from collections import defaultdict

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

VKRENDERER_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Search paths — order matters for resolution
SEARCH_ROOTS = [
    VKRENDERER_DIR,
    os.path.join(VKRENDERER_DIR, "src"),
    os.path.join(VKRENDERER_DIR, "shaders"),
    os.path.join(VKRENDERER_DIR, "scenes"),
]

EXTERNAL_PREFIXES = (
    "vulkan/", "vk_", "GLFW/", "glfw",
    "GfxCore/", "SysCore/", "syscore/", "gfxcore/",
    "imgui", "implot",
    "external/",
)

STL_HEADERS = {
    "algorithm", "array", "assert.h", "atomic", "cassert", "chrono",
    "cinttypes", "cstdint", "cstdlib", "ctime", "deque", "filesystem",
    "fstream", "functional", "iostream", "iterator", "list", "map",
    "math.h", "memory", "mutex", "optional", "queue", "ratio", "set",
    "sstream", "stdexcept", "stdlib.h", "string", "thread", "tuple",
    "type_traits", "unordered_map", "unordered_set", "utility", "variant",
    "vector", "windows.h", "winsock2.h",
}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*["<]([^">]+)[">]')

# ---------------------------------------------------------------------------
# Index all project headers
# ---------------------------------------------------------------------------

def build_file_index(roots):
    """Return {basename_and_subpath -> absolute_path} for every .h/.cpp found."""
    index = {}
    for root in roots:
        for dirpath, _, filenames in os.walk(root):
            for fname in filenames:
                if not fname.endswith((".h", ".cpp")):
                    continue
                abs_path = os.path.join(dirpath, fname)
                # Store under every suffix that could appear in an #include
                rel = os.path.relpath(abs_path, VKRENDERER_DIR).replace("\\", "/")
                index[rel] = abs_path
                # Also index by shorter suffixes so relative includes resolve
                parts = rel.split("/")
                for i in range(len(parts)):
                    key = "/".join(parts[i:])
                    if key not in index:
                        index[key] = abs_path
    return index


FILE_INDEX = build_file_index(SEARCH_ROOTS)


def resolve(include_path, including_file_abs):
    """Return absolute path for an include, or None if external/unresolvable."""
    include_path = include_path.replace("\\", "/")

    # STL
    if include_path in STL_HEADERS:
        return None

    # Known external prefixes
    if any(include_path.lower().startswith(p.lower()) for p in EXTERNAL_PREFIXES):
        return None

    # Try relative to the including file's directory first
    if including_file_abs:
        candidate = os.path.normpath(
            os.path.join(os.path.dirname(including_file_abs), include_path)
        ).replace("\\", "/")
        if os.path.isfile(candidate):
            return candidate

    # Try index lookup
    if include_path in FILE_INDEX:
        return FILE_INDEX[include_path]

    # Try stripping leading ../
    stripped = include_path.lstrip("./").lstrip("../")
    if stripped in FILE_INDEX:
        return FILE_INDEX[stripped]

    return None  # external or missing


# ---------------------------------------------------------------------------
# Parse includes for a single file
# ---------------------------------------------------------------------------

def parse_includes(abs_path):
    """Return list of (raw_include_str, resolved_abs_path_or_None)."""
    results = []
    try:
        with open(abs_path, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = INCLUDE_RE.match(line)
                if m:
                    raw = m.group(1)
                    resolved = resolve(raw, abs_path)
                    results.append((raw, resolved))
    except OSError:
        pass
    return results


# ---------------------------------------------------------------------------
# Build full dependency graph (for cycle detection)
# ---------------------------------------------------------------------------

def build_graph(file_index):
    graph = defaultdict(set)
    for abs_path in set(file_index.values()):
        for _raw, resolved in parse_includes(abs_path):
            if resolved:
                graph[abs_path].add(resolved)
    return graph


def find_cycles(graph):
    """Return list of cycles as lists of absolute paths."""
    cycles = []
    visited = set()
    path = []
    path_set = set()

    def dfs(node):
        if node in path_set:
            idx = path.index(node)
            cycles.append(path[idx:] + [node])
            return
        if node in visited:
            return
        visited.add(node)
        path.append(node)
        path_set.add(node)
        for neighbour in graph.get(node, []):
            dfs(neighbour)
        path.pop()
        path_set.discard(node)

    for node in list(graph.keys()):
        dfs(node)
    return cycles


# ---------------------------------------------------------------------------
# Tree printer
# ---------------------------------------------------------------------------

def short(abs_path):
    """Return a short display name relative to VKRENDERER_DIR."""
    try:
        rel = os.path.relpath(abs_path, VKRENDERER_DIR).replace("\\", "/")
        # Drop leading src/ for brevity
        if rel.startswith("src/"):
            rel = rel[4:]
        return rel
    except ValueError:
        return abs_path


def _expand(abs_path, show_external, prefix, visited, ancestors):
    """Print the children of abs_path.  The node's own name line is the caller's job."""
    if abs_path in ancestors or abs_path in visited:
        return

    visited.add(abs_path)
    ancestors = ancestors | {abs_path}

    includes = [
        (raw, resolved)
        for raw, resolved in parse_includes(abs_path)
        if resolved is not None or show_external
    ]

    for i, (raw, resolved) in enumerate(includes):
        is_last   = (i == len(includes) - 1)
        connector = "`-- " if is_last else "+-- "
        extender  = "    " if is_last else "|   "

        if resolved is None:
            print(prefix + connector + "[" + raw + "]")
            continue

        if resolved in ancestors:
            print(prefix + connector + short(resolved) + "  *** CYCLE ***")
        elif resolved in visited:
            print(prefix + connector + short(resolved) + "  [seen]")
        else:
            print(prefix + connector + short(resolved))
            _expand(resolved, show_external, prefix + extender, visited, ancestors)


def print_tree(root_abs, show_external, prefix="", visited=None, ancestors=None):
    if visited is None:
        visited = set()
    if ancestors is None:
        ancestors = set()
    print(short(root_abs))
    _expand(root_abs, show_external, prefix, visited, ancestors)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("root_file", nargs="?",
                        default="src/render_core/renderer.h",
                        help="File to root the tree at (relative to vkRenderer/)")
    parser.add_argument("--all", action="store_true",
                        help="Print trees for all .h files")
    parser.add_argument("--cycles", action="store_true",
                        help="Only print circular include chains")
    parser.add_argument("--no-ext", action="store_true",
                        help="Hide external / system headers")
    args = parser.parse_args()

    show_external = not args.no_ext

    if args.cycles:
        graph = build_graph(FILE_INDEX)
        cycles = find_cycles(graph)
        if not cycles:
            print("No circular includes detected.")
        else:
            print(f"{len(cycles)} circular include chain(s):\n")
            for i, cycle in enumerate(cycles, 1):
                print(f"  Cycle {i}:")
                for node in cycle:
                    print(f"    {short(node)}")
                print()
        return

    if args.all:
        headers = sorted(set(
            p for p in FILE_INDEX.values()
            if p.endswith(".h")
        ))
        visited_global = set()
        for h in headers:
            print_tree(h, show_external, prefix="", visited=visited_global)
            print()
        return

    # Single root
    root_abs = os.path.normpath(os.path.join(VKRENDERER_DIR, args.root_file))
    if not os.path.isfile(root_abs):
        # Try index lookup
        key = args.root_file.replace("\\", "/")
        if key in FILE_INDEX:
            root_abs = FILE_INDEX[key]
        else:
            print(f"Error: cannot find '{args.root_file}'", file=sys.stderr)
            sys.exit(1)

    print_tree(root_abs, show_external)


if __name__ == "__main__":
    main()
