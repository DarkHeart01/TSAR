#!/usr/bin/env python3
"""
poly_engine.py
JOCKY Polymorphic Source Transformer

Supports two modes, selected by input file extension:
  .cpp / .c  — C/C++ transform (original, unchanged)
  .jky       — JOCKY DSL transform (new)

C/C++ transforms:
  - Variable renaming
  - Dead variable insertion
  - Dead arithmetic insertion
  - Dead conditional blocks
  - Independent statement reordering

JOCKY DSL transforms:
  - let-variable renaming inside function bodies
  - Dead int chains inserted after each fn/main opening brace
  - Dead always-false conditionals using the dead chain
"""

import sys
import re
import random
import string

# ── Shared name generator ─────────────────────────────────────

def rand_name(length=8):
    """Random C/C++ identifier."""
    return "__jk_" + ''.join(random.choices(string.ascii_lowercase + string.digits, k=length))

def jky_rand_name(length=8):
    """Random JOCKY identifier (single underscore prefix to stay valid)."""
    return "_jk_" + ''.join(random.choices(string.ascii_lowercase + string.digits, k=length))


# ════════════════════════════════════════════════════════════════
#  C / C++ TRANSFORM  (original — untouched)
# ════════════════════════════════════════════════════════════════

def dead_int_declaration():
    name = rand_name()
    val  = random.randint(0, 0xFFFF)
    return f"    int {name} = {val};"

def dead_arithmetic(existing_names):
    if len(existing_names) < 2:
        return None
    a, b = random.sample(existing_names, 2)
    op   = random.choice(['+', '-', '^', '|', '&'])
    name = rand_name()
    existing_names.append(name)
    return f"    int {name} = {a} {op} {b};"

def dead_conditional():
    name = rand_name()
    val  = random.randint(1, 0xFFFF)
    return (
        f"    if (({val} & 0) == {val}) {{\n"
        f"        int {name} = {val};\n"
        f"        (void){name};\n"
        f"    }}"
    )

def dead_void_cast(existing_names):
    return [f"    (void){n};" for n in existing_names]

def generate_junk_block(count=5):
    lines      = []
    dead_names = []
    actual     = 0

    for _ in range(count):
        name = rand_name()
        val  = random.randint(0, 0xFFFF)
        lines.append(f"    int {name} = {val};")
        dead_names.append(name)
        actual += 1

    for _ in range(max(1, count // 2)):
        arith = dead_arithmetic(dead_names)
        if arith:
            lines.append(arith)
            actual += 1

    lines.append(dead_conditional())
    actual += 1
    lines.extend(dead_void_cast(dead_names))

    return "\n".join(lines), actual

def rename_variables(content):
    pattern = re.compile(
        r'^(\s+)(int|unsigned int|size_t|DWORD|BOOL|char)\s+'
        r'([a-zA-Z_][a-zA-Z0-9_]*)\s*([=;])',
        re.MULTILINE
    )
    rename_map = {}
    renamed    = 0

    def replacer(m):
        nonlocal renamed
        indent, type_, varname, suffix = m.groups()
        skip = {'i', 'j', 'k', 'n', 'x', 'y', 'z', 'result', 'ret',
                'main', 'argc', 'argv', 'NULL', 'TRUE', 'FALSE'}
        if varname in skip or varname.startswith('__jk_'):
            return m.group(0)
        if varname not in rename_map:
            rename_map[varname] = rand_name()
            renamed += 1
        return f"{indent}{type_} {rename_map[varname]}{suffix}"

    new_content = pattern.sub(replacer, content)
    for orig, new in rename_map.items():
        new_content = re.sub(r'\b' + re.escape(orig) + r'\b', new, new_content)
    return new_content, renamed

def insert_junk(content):
    func_body_pattern = re.compile(
        r'(\b(?:int|void|char|BOOL|DWORD|auto)\b[^;{]*\([^)]*\)\s*\{)',
        re.MULTILINE | re.DOTALL
    )
    total  = 0
    offset = 0
    result = content

    for m in func_body_pattern.finditer(content):
        insert_pos = m.end() + offset
        junk, count = generate_junk_block(random.randint(3, 6))
        insertion  = "\n" + junk + "\n"
        result     = result[:insert_pos] + insertion + result[insert_pos:]
        offset    += len(insertion)
        total     += count

    return result, total

def transform(input_path, output_path):
    """C/C++ polymorphic transform."""
    print(f"[*] Polymorphic transform (C/C++): {input_path}")

    with open(input_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    original_size  = len(content)
    content, rc    = rename_variables(content)
    content, jc    = insert_junk(content)

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"[+] Variables renamed:  {rc}")
    print(f"[+] Junk inserted:      {jc}")
    print(f"[+] Output:             {output_path} (+{len(content) - original_size} bytes)")


# ════════════════════════════════════════════════════════════════
#  JOCKY DSL TRANSFORM
# ════════════════════════════════════════════════════════════════

# Keywords and built-ins that must never be renamed
_JKY_RESERVED = {
    'int', 'ptr', 'void', 'byte', 'bool', 'handle', 'fnptr', 'let', 'volatile',
    'fn', 'main', 'return', 'if', 'else', 'while', 'for', 'cast', 'null',
    'true', 'false', 'INFINITE', 'INVALID_HANDLE',
    'read_int', 'write_int', 'read_byte', 'write_byte',
    'alloc', 'free_mem', 'memcopy', 'memzero', 'memset',
    'atomic_inc', 'get_tick', 'get_tid',
    'create_thread', 'wait_all', 'close_handle', 'exit',
    'jocky_entry', 'jocky_alloc', 'jocky_free', 'jocky_write',
    'jocky_create_thread', 'jocky_wait', 'jocky_wait_all',
    'jocky_close', 'jocky_exit', 'jocky_atomic_inc',
    'jocky_get_stdout', 'jocky_get_tick', 'jocky_get_tid',
    'jky_get_string',
}

def _jky_dead_decl():
    name = jky_rand_name()
    val  = random.randint(1, 0x7FFF)
    return f"    let {name}: int = {val}", name

def _jky_dead_arith(names):
    if len(names) < 2:
        return None, None
    a, b = random.sample(names, 2)
    op   = random.choice(['+', '-', '^', '|', '&'])
    name = jky_rand_name()
    return f"    let {name}: int = {a} {op} {b}", name

def _jky_dead_conditional(ref_name):
    """Always-false if block referencing a dead variable so it isn't unused."""
    inner = jky_rand_name()
    return (
        f"    if ({ref_name} & 0) == {ref_name} {{\n"
        f"        let {inner}: int = {ref_name}\n"
        f"    }}"
    )

def _jky_generate_junk(count=3):
    """
    Build a self-contained dead code block:
      - N seed int declarations
      - Arithmetic chains between them
      - Always-false conditional referencing the chain
    All variables are referenced at least once so no unused-var warnings.
    """
    lines      = []
    dead_names = []

    # Seed declarations
    for _ in range(max(2, count)):
        decl, name = _jky_dead_decl()
        lines.append(decl)
        dead_names.append(name)

    # Arithmetic chain
    chain = list(dead_names)
    for _ in range(max(1, count // 2)):
        arith, name = _jky_dead_arith(chain)
        if arith:
            lines.append(arith)
            chain.append(name)

    # Dead conditional uses the last chain var so everything is referenced
    ref = chain[-1]
    lines.append(_jky_dead_conditional(ref))

    return "\n".join(lines), len(lines)


def _jky_rename_variables(content):
    """
    Rename let-declared local variables inside fn/main bodies.
    Skips: single-letter names, ALL-CAPS constants, reserved words,
           function parameters (declared as `param: type` not `let param:`),
           globals (declared at top level with no leading indent).
    """
    # Only match let declarations that are indented (inside a block)
    pattern = re.compile(
        r'^(\s+)let\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*:',
        re.MULTILINE
    )

    rename_map = {}
    renamed    = 0

    def replacer(m):
        nonlocal renamed
        indent  = m.group(1)
        varname = m.group(2)

        if (varname in _JKY_RESERVED
                or len(varname) == 1           # single-letter loop vars
                or varname.isupper()           # ALL_CAPS constants
                or varname.startswith('_jk_')  # already junk
                or varname.startswith('jocky_')
                or varname.startswith('jky_')):
            return m.group(0)

        if varname not in rename_map:
            rename_map[varname] = jky_rand_name()
            renamed += 1

        return f"{indent}let {rename_map[varname]}:"

    new_content = pattern.sub(replacer, content)

    # Apply rename to all usages of renamed identifiers
    for orig, new in rename_map.items():
        new_content = re.sub(r'\b' + re.escape(orig) + r'\b', new, new_content)

    return new_content, renamed


def _jky_insert_junk(content):
    """
    Insert dead code block immediately after the opening brace of every
    fn declaration and the main block.
    Handles optional decorator lines (@crypto, @obfuscate, etc.) before fn.
    """
    # Matches:  [@decorator\n] fn name(...) -> type {
    # Or:       main {
    pattern = re.compile(
        r'((?:@\w+\s*\n\s*)?'           # optional decorator
        r'(?:fn\s+\w+\s*\([^)]*\)\s*->\s*\w+|main)'  # fn sig or main
        r'\s*\{)',                        # opening brace
        re.MULTILINE
    )

    total  = 0
    offset = 0
    result = content

    for m in pattern.finditer(content):
        insert_pos = m.end() + offset
        junk, count = _jky_generate_junk(random.randint(2, 4))
        insertion   = "\n" + junk + "\n"
        result      = result[:insert_pos] + insertion + result[insert_pos:]
        offset     += len(insertion)
        total      += count

    return result, total


def transform_jky(input_path, output_path):
    """JOCKY DSL polymorphic transform."""
    print(f"[*] Polymorphic transform (JOCKY): {input_path}")

    with open(input_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    original_size = len(content)

    content, renamed_count = _jky_rename_variables(content)
    content, junk_count    = _jky_insert_junk(content)

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"[+] Variables renamed:  {renamed_count}")
    print(f"[+] Junk inserted:      {junk_count}")
    print(f"[+] Output:             {output_path} (+{len(content) - original_size} bytes)")


# ── Entry point ───────────────────────────────────────────────

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: poly_engine.py input.cpp  output.cpp")
        print("       poly_engine.py input.jky  output.jky")
        sys.exit(1)
    random.seed()
    inp = sys.argv[1]
    if inp.lower().endswith('.jky'):
        transform_jky(inp, sys.argv[2])
    else:
        transform(inp, sys.argv[2])
