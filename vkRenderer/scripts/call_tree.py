#!/usr/bin/env python3
"""
call_tree.py  —  Print a function call dependency tree.

Usage:
    python call_tree.py [root_function]  [--all]  [--cycles]  [--no-ext]  [--file FILE]

    root_function   Function name to root the tree at (default: PSMain).

    --all       Print a tree rooted at every defined function.
    --cycles    Only report circular call chains.
    --no-ext    Hide calls to functions not defined in the project.
    --file FILE Limit the root search to functions defined in FILE
                (relative to vkRenderer/).  Calls are still resolved globally.

Scans all .h, .cpp, .hlsl files under vkRenderer/src/ and vkRenderer/shaders/.
Functions defined in multiple files (overloads / header guards) keep the first
occurrence found.  Method calls (obj.Foo(), ptr->Bar()) are excluded.
"""

import os
import re
import sys
import argparse

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

VKRENDERER_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SEARCH_ROOTS = [
    os.path.join(VKRENDERER_DIR, "src"),
    os.path.join(VKRENDERER_DIR, "shaders"),
    os.path.join(VKRENDERER_DIR, "scenes"),
]

FILE_EXTENSIONS = (".h", ".cpp", ".hlsl")

# Identifiers that look like calls but aren't user functions
KEYWORDS = {
    # C / C++
    "if", "else", "for", "while", "do", "switch", "case", "default",
    "return", "break", "continue", "goto", "sizeof", "alignof", "decltype",
    "static_assert", "catch", "throw", "new", "delete", "operator",
    "namespace", "typedef", "typename", "template",
    # HLSL built-ins
    "abs", "acos", "all", "any", "asin", "atan", "atan2", "ceil",
    "clamp", "clip", "cos", "cross", "degrees", "determinant", "distance",
    "dot", "exp", "exp2", "floor", "frac", "fmod", "lerp", "length",
    "log", "log2", "mad", "max", "min", "modf", "mul", "normalize",
    "pow", "radians", "reflect", "refract", "round", "rsqrt", "saturate",
    "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt", "step",
    "tan", "tanh", "transpose", "trunc", "fma", "rcp",
    "isnan", "isinf", "isfinite", "asfloat", "asint", "asuint", "f16tof32",
    "ddx", "ddy", "ddx_coarse", "ddy_coarse", "DDX", "DDY",
    "InterlockedAdd", "InterlockedMin", "InterlockedMax", "InterlockedOr",
    "InterlockedAnd", "InterlockedExchange", "InterlockedCompareExchange",
    "GroupMemoryBarrierWithGroupSync", "DeviceMemoryBarrier", "AllMemoryBarrier",
    "WaveActiveSum", "WaveActiveBallot", "WaveGetLaneIndex",
    # GLSL built-ins
    "texture", "textureLod", "texelFetch", "textureSize", "textureCubeLod",
    "mix", "fract", "mod", "dFdx", "dFdy", "discard", "emit",
    # Type constructors — treated as keywords so they don't pollute the graph
    "void", "bool", "int", "uint", "float", "double", "half",
    "bool2", "bool3", "bool4",
    "int2", "int3", "int4",
    "uint2", "uint3", "uint4",
    "float2", "float3", "float4",
    "half2", "half3", "half4",
    "double2", "double3", "double4",
    "float1x1", "float2x2", "float3x3", "float4x4",
    "float2x3", "float3x2", "float2x4", "float4x2", "float3x4", "float4x3",
    "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4",
    "uvec2", "uvec3", "uvec4", "bvec2", "bvec3", "bvec4",
    "mat2", "mat3", "mat4",
    "mat2x2", "mat2x3", "mat2x4", "mat3x2", "mat3x3", "mat3x4",
    "mat4x2", "mat4x3", "mat4x4",
}

# ---------------------------------------------------------------------------
# Source file discovery
# ---------------------------------------------------------------------------

def find_source_files(roots, extensions):
    files = []
    for root in roots:
        if not os.path.isdir(root):
            continue
        for dirpath, _, filenames in os.walk(root):
            for fname in filenames:
                if fname.endswith(extensions):
                    files.append(os.path.join(dirpath, fname))
    return files


def resolve_file_arg(arg, source_files):
    """
    Resolve --file argument to an absolute path.  Tries in order:
      1. Absolute path as given.
      2. Path relative to the current working directory.
      3. Path relative to VKRENDERER_DIR (repo-anchored).
      4. Suffix match against any known source file (e.g. 'renderer.cpp').
    Returns the normalized absolute path, or None if nothing matches.
    """
    arg_norm = arg.replace("\\", "/")

    # 1. Absolute
    if os.path.isabs(arg):
        return os.path.normpath(arg) if os.path.isfile(arg) else None

    # 2. Relative to cwd
    cwd_path = os.path.normpath(os.path.abspath(arg))
    if os.path.isfile(cwd_path):
        return cwd_path

    # 3. Relative to VKRENDERER_DIR
    repo_path = os.path.normpath(os.path.join(VKRENDERER_DIR, arg))
    if os.path.isfile(repo_path):
        return repo_path

    # 4. Suffix match within indexed files (must be preceded by a path separator
    # or match the full relative path, so 'renderer.cpp' doesn't grab 'my_renderer.cpp')
    matches = [
        f for f in source_files
        if f.replace("\\", "/").endswith("/" + arg_norm) or f.replace("\\", "/") == arg_norm
    ]
    if len(matches) == 1:
        return os.path.normpath(matches[0])
    if len(matches) > 1:
        print(f"'{arg}' is ambiguous -- matched:", file=sys.stderr)
        for m in matches:
            print(f"    {short(m)}", file=sys.stderr)
        return None

    return None

# ---------------------------------------------------------------------------
# Comment stripping
# ---------------------------------------------------------------------------

def strip_comments(text):
    """Remove // line comments and /* */ block comments."""
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.DOTALL)
    text = re.sub(r'//[^\n]*', '', text)
    return text

# ---------------------------------------------------------------------------
# Function definition extraction
# ---------------------------------------------------------------------------

# Matches the start of a function definition:
#   optional qualifiers  +  return type(s)  +  name  +  opening paren
# Does NOT match: pure control-flow keywords (in KEYWORDS), preprocessor lines.
_QUALIFIERS  = r'(?:static|inline|virtual|explicit|friend|const|unsigned|signed|__forceinline|__inline|override|final)\s+'
_RETURN_TYPE = r'(?:[\w:~<>\*&]+\s+)+'
# Name may be qualified:  Class::Method  or  Outer::Inner::Method  or just foo
_FUNC_NAME   = r'((?:[\w~]+::)*[\w~]+)'

FUNC_START_RE = re.compile(
    r'^[ \t]*'           # leading whitespace (line-anchored via re.MULTILINE)
    r'(?:' + _QUALIFIERS + r')*'
    r'(?:' + _RETURN_TYPE + r')'
    r'' + _FUNC_NAME + r'\s*\(',
    re.MULTILINE,
)

CALL_RE = re.compile(r'\b((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)\s*\(')


def _skip_parens(text, pos):
    """Advance pos past a balanced paren group starting at pos (which is inside the group)."""
    depth = 1
    n = len(text)
    while pos < n and depth > 0:
        c = text[pos]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
        pos += 1
    return pos


def _find_body_brace(text, pos):
    """
    Scan forward from pos looking for '{' or ';'.
    Returns (index_of_brace, True) for a definition, (index, False) for a declaration.
    """
    n = len(text)
    while pos < n:
        c = text[pos]
        if c == '{':
            return pos, True
        if c == ';':
            return pos, False
        # Nested parens can appear in trailing qualifiers like noexcept(expr)
        if c == '(':
            pos = _skip_parens(text, pos + 1)
            continue
        pos += 1
    return pos, False


def extract_functions(filepath):
    """
    Parse filepath and return a list of (name, body_text, line_number) tuples,
    one per function definition found.
    """
    try:
        with open(filepath, encoding="utf-8", errors="replace") as f:
            raw = f.read()
    except OSError:
        return []

    text = strip_comments(raw)
    n    = len(text)
    results = []
    search_from = 0

    while search_from < n:
        m = FUNC_START_RE.search(text, search_from)
        if not m:
            break

        func_name = m.group(1)

        # Reject preprocessor lines (the regex already requires leading whitespace
        # but a macro line that starts with whitespace could slip through)
        line_start = text.rfind('\n', 0, m.start()) + 1
        if text[line_start:m.start()].lstrip().startswith('#'):
            search_from = m.end()
            continue

        if func_name in KEYWORDS:
            search_from = m.end()
            continue

        # Skip past parameter list
        after_params = _skip_parens(text, m.end())

        # Find the opening brace (or semicolon for a declaration)
        brace_pos, is_definition = _find_body_brace(text, after_params)

        if not is_definition:
            search_from = m.end()
            continue

        # Extract body by brace-matching
        body_start = brace_pos + 1
        depth      = 1
        body_pos   = body_start
        while body_pos < n and depth > 0:
            c = text[body_pos]
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
            body_pos += 1

        body     = text[body_start : body_pos - 1]
        line_no  = raw[: m.start()].count('\n') + 1

        results.append((func_name, body, line_no))
        search_from = body_pos

    return results

# ---------------------------------------------------------------------------
# Call graph construction
# ---------------------------------------------------------------------------

def build_call_graph(source_files):
    """
    Returns:
        func_defs     : { name -> [ (abs_filepath, line_no, body), ... ] }
        call_graph    : { (name, filepath) -> set of callee names that are defined somewhere }
        unknown_calls : { (name, filepath) -> set of callee names NOT defined }
    """
    # Pass 1 — collect all defined function names (keep ALL occurrences)
    func_defs = {}  # name -> list of (filepath, line_no, body)

    for filepath in source_files:
        for name, body, line_no in extract_functions(filepath):
            func_defs.setdefault(name, []).append((filepath, line_no, body))

    # Pass 2 — resolve calls per definition
    call_graph    = {}
    unknown_calls = {}

    for name, defs in func_defs.items():
        # Enclosing class (if this is a C++ method):  "Renderer::Commit" -> "Renderer"
        if "::" in name:
            owner_class = name.rsplit("::", 1)[0]
        else:
            owner_class = None

        for filepath, _line_no, body in defs:
            key     = (name, filepath)
            known   = set()
            unknown = set()

            for cm in CALL_RE.finditer(body):
                callee = cm.group(1)

                if callee in KEYWORDS or callee == name:
                    continue

                # Skip method / member calls:  obj.Foo(  or  ptr->Foo(
                start = cm.start()
                if start > 0 and body[start - 1] in '.>':
                    continue

                # Direct match
                if callee in func_defs:
                    known.add(callee)
                    continue

                # Unqualified call inside a class method — try the owning class first
                if owner_class and "::" not in callee:
                    qualified = owner_class + "::" + callee
                    if qualified in func_defs:
                        known.add(qualified)
                        continue

                # Last resort: if only one class defines a method with this name, use it
                if "::" not in callee:
                    matches = [n for n in func_defs if n.endswith("::" + callee)]
                    if len(matches) == 1:
                        known.add(matches[0])
                        continue

                unknown.add(callee)

            call_graph[key]    = known
            unknown_calls[key] = unknown

    return func_defs, call_graph, unknown_calls

# ---------------------------------------------------------------------------
# Cycle detection
# ---------------------------------------------------------------------------

def pick_primary(func_defs, name):
    """Pick a single (filepath, line_no, body) for a name.
    Prefer .cpp over .h so definition beats declaration-like inline."""
    defs = func_defs[name]
    if len(defs) == 1:
        return defs[0]
    for d in defs:
        if d[0].endswith(".cpp"):
            return d
    return defs[0]


def find_cycles(func_defs, call_graph):
    """Cycle detection on name-only graph, using each name's primary definition."""
    cycles   = []
    visited  = set()
    path     = []
    path_set = set()

    def dfs(name):
        if name in path_set:
            idx = path.index(name)
            cycles.append(path[idx:] + [name])
            return
        if name in visited:
            return
        visited.add(name)
        path.append(name)
        path_set.add(name)
        primary = pick_primary(func_defs, name)
        key     = (name, primary[0])
        for callee in call_graph.get(key, []):
            dfs(callee)
        path.pop()
        path_set.discard(name)

    for name in list(func_defs.keys()):
        dfs(name)
    return cycles

# ---------------------------------------------------------------------------
# Tree printer
# ---------------------------------------------------------------------------

def short(abs_path):
    """Return a compact display path relative to VKRENDERER_DIR."""
    try:
        rel = os.path.relpath(abs_path, VKRENDERER_DIR).replace("\\", "/")
        if rel.startswith("src/"):
            rel = rel[4:]
        return rel
    except ValueError:
        return abs_path


def _expand(name, definition, func_defs, call_graph, unknown_calls,
            show_external, prefix, visited, ancestors):

    if name in ancestors or name in visited:
        return

    visited.add(name)
    ancestors = ancestors | {name}

    key     = (name, definition[0])
    known   = sorted(call_graph.get(key, set()))
    unknown = sorted(unknown_calls.get(key, set())) if show_external else []

    entries = [(c, True) for c in known] + [(c, False) for c in unknown]

    for i, (callee, is_known) in enumerate(entries):
        is_last   = (i == len(entries) - 1)
        connector = "`-- " if is_last else "+-- "
        extender  = "    " if is_last else "|   "

        if not is_known:
            print(prefix + connector + "[" + callee + "]")
            continue

        callee_def = pick_primary(func_defs, callee)
        dup_marker = f"  [+{len(func_defs[callee]) - 1} more]" if len(func_defs[callee]) > 1 else ""
        label      = f"{callee}  ({short(callee_def[0])}:{callee_def[1]}){dup_marker}"

        if callee in ancestors:
            print(prefix + connector + label + "  *** CYCLE ***")
        elif callee in visited:
            print(prefix + connector + label + "  [seen]")
        else:
            print(prefix + connector + label)
            _expand(callee, callee_def, func_defs, call_graph, unknown_calls,
                    show_external, prefix + extender, visited, ancestors)


def print_tree(name, definition, func_defs, call_graph, unknown_calls,
               show_external, prefix="", visited=None, ancestors=None):
    if visited  is None: visited   = set()
    if ancestors is None: ancestors = set()

    print(f"{name}  ({short(definition[0])}:{definition[1]})")
    _expand(name, definition, func_defs, call_graph, unknown_calls,
            show_external, prefix, visited, ancestors)

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("root_function", nargs="?", default="PSMain",
                        help="Function to root the tree at (default: PSMain)")
    parser.add_argument("--all", action="store_true",
                        help="Print trees for all defined functions")
    parser.add_argument("--cycles", action="store_true",
                        help="Only report circular call chains")
    parser.add_argument("--no-ext", action="store_true",
                        help="Hide calls to unresolved / external functions")
    parser.add_argument("--file", metavar="FILE",
                        help="Root candidates limited to functions defined in FILE "
                             "(relative to vkRenderer/).  Calls still resolved globally.")
    args = parser.parse_args()

    show_external = not args.no_ext

    source_files = find_source_files(SEARCH_ROOTS, FILE_EXTENSIONS)

    func_defs, call_graph, unknown_calls = build_call_graph(source_files)

    # --cycles
    if args.cycles:
        cycles = find_cycles(func_defs, call_graph)
        if not cycles:
            print("No circular call chains detected.")
        else:
            print(f"{len(cycles)} circular call chain(s):\n")
            for i, cycle in enumerate(cycles, 1):
                print(f"  Cycle {i}:")
                for name in cycle:
                    primary = pick_primary(func_defs, name)
                    print(f"    {name}  ({short(primary[0])}:{primary[1]})")
                print()
        return

    # Candidate pool (optionally filtered to --file)
    abs_file = None
    if args.file:
        abs_file   = resolve_file_arg(args.file, source_files)
        if abs_file is None:
            print(f"Error: could not resolve '{args.file}' to a source file.", file=sys.stderr)
            sys.exit(1)
        candidates = {n for n, defs in func_defs.items()
                      if any(os.path.normpath(d[0]) == abs_file for d in defs)}
        if not candidates:
            print(f"Error: no functions found in '{args.file}'", file=sys.stderr)
            sys.exit(1)
    else:
        candidates = set(func_defs.keys())

    # Helper to pick definition for a root — honours --file if given
    def select_definition(name):
        defs = func_defs[name]
        if abs_file is not None:
            for d in defs:
                if os.path.normpath(d[0]) == abs_file:
                    return d
        return pick_primary(func_defs, name)

    # --all
    if args.all:
        visited_global = set()
        for name in sorted(candidates):
            defn = select_definition(name)
            print_tree(name, defn, func_defs, call_graph, unknown_calls,
                       show_external, visited=visited_global)
            print()
        return

    # Single root
    root = args.root_function
    if root not in func_defs:
        # Try case-insensitive exact match
        lower_map = {k.lower(): k for k in func_defs}
        matched   = lower_map.get(root.lower())
        if matched:
            root = matched
        else:
            # Try matching any qualified name ending in "::root"
            qualified_matches = [k for k in func_defs if k.endswith("::" + root)]
            if len(qualified_matches) == 1:
                root = qualified_matches[0]
            elif len(qualified_matches) > 1:
                print(f"'{root}' matches multiple qualified names -- pass one explicitly:",
                      file=sys.stderr)
                for q in qualified_matches:
                    print(f"    {q}", file=sys.stderr)
                sys.exit(1)
            else:
                print(f"Error: function '{root}' not found.", file=sys.stderr)
                close = [k for k in sorted(func_defs) if root.lower() in k.lower()][:10]
                if close:
                    print(f"  Partial matches: {', '.join(close)}", file=sys.stderr)
                sys.exit(1)

    # If ambiguous and no --file, list the choices and bail
    if len(func_defs[root]) > 1 and abs_file is None:
        print(f"'{root}' is defined in multiple files -- pass --file to pick one:",
              file=sys.stderr)
        for fp, ln, _ in func_defs[root]:
            print(f"    --file {short(fp)}   (line {ln})", file=sys.stderr)
        sys.exit(1)

    print_tree(root, select_definition(root), func_defs, call_graph, unknown_calls,
               show_external)


if __name__ == "__main__":
    main()
