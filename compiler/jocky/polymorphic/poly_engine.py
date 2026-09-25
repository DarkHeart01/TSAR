#!/usr/bin/env python3
"""
poly_engine.py
JOCKY Polymorphic Source Transformer

Transforms C/C++ source before Polaris compilation.
Every run produces structurally different source:
  - Variable renaming
  - Dead variable insertion
  - Dead arithmetic insertion
  - Dead conditional blocks
  - Independent statement reordering
"""

import sys
import re
import random
import string

# ── Random name generator ─────────────────────────────────────
def rand_name(length=8):
    """Generate a random C identifier."""
    prefix = "__jk_"
    chars  = string.ascii_lowercase + string.digits
    return prefix + ''.join(random.choices(chars, k=length))

# ── Dead code generators ──────────────────────────────────────

def dead_int_declaration():
    """Generate a dead integer variable declaration."""
    name = rand_name()
    val  = random.randint(0, 0xFFFF)
    return f"    int {name} = {val};"

def dead_arithmetic(existing_names):
    """Generate dead arithmetic using existing dead variables."""
    if len(existing_names) < 2:
        return None
    a, b = random.sample(existing_names, 2)
    ops  = ['+', '-', '^', '|', '&']
    op   = random.choice(ops)
    name = rand_name()
    existing_names.append(name)
    return f"    int {name} = {a} {op} {b};"

def dead_conditional():
    """Generate a dead conditional block that never executes."""
    name = rand_name()
    val  = random.randint(1, 0xFFFF)
    # Condition is always false: (val & 0) == val is never true
    return (
        f"    if (({val} & 0) == {val}) {{\n"
        f"        int {name} = {val};\n"
        f"        (void){name};\n"
        f"    }}"
    )

def dead_void_cast(existing_names):
    """Suppress unused variable warnings with void casts."""
    return [f"    (void){n};" for n in existing_names]

# ── Junk block generator ──────────────────────────────────────

def generate_junk_block(count=5):
    """
    Generate a block of dead code:
    - Dead variable declarations
    - Dead arithmetic between them
    - Dead conditional block
    - Void casts to suppress warnings
    """
    lines        = []
    dead_names   = []
    actual_count = 0

    # Dead variable declarations
    for _ in range(count):
        name = rand_name()
        val  = random.randint(0, 0xFFFF)
        lines.append(f"    int {name} = {val};")
        dead_names.append(name)
        actual_count += 1

    # Dead arithmetic between declared variables
    for _ in range(max(1, count // 2)):
        arith = dead_arithmetic(dead_names)
        if arith:
            lines.append(arith)
            actual_count += 1

    # Dead conditional
    lines.append(dead_conditional())
    actual_count += 1

    # Void casts — suppress compiler warnings about unused vars
    lines.extend(dead_void_cast(dead_names))

    return "\n".join(lines), actual_count

# ── Variable renamer ──────────────────────────────────────────

def rename_variables(content):
    """
    Rename local variable declarations to random names.
    Only renames simple declarations: int foo = ...
    Skips: function params, global vars, struct members.
    """
    # Match: int varname = ... or int varname;
    # inside function bodies (indented)
    pattern = re.compile(
        r'^(\s+)(int|unsigned int|size_t|DWORD|BOOL|char)\s+'
        r'([a-zA-Z_][a-zA-Z0-9_]*)\s*([=;])',
        re.MULTILINE
    )

    rename_map  = {}
    renamed     = 0

    def replacer(m):
        nonlocal renamed
        indent   = m.group(1)
        type_    = m.group(2)
        varname  = m.group(3)
        suffix   = m.group(4)

        # Skip: already renamed, loop vars, common names to keep
        skip = {'i', 'j', 'k', 'n', 'x', 'y', 'z', 'result', 'ret',
                'main', 'argc', 'argv', 'NULL', 'TRUE', 'FALSE'}
        if varname in skip or varname.startswith('__jk_'):
            return m.group(0)

        if varname not in rename_map:
            rename_map[varname] = rand_name()
            renamed += 1

        return f"{indent}{type_} {rename_map[varname]}{suffix}"

    new_content = pattern.sub(replacer, content)

    # Apply rename map to all usages of renamed vars
    for original, renamed_name in rename_map.items():
        # Only rename standalone word occurrences
        new_content = re.sub(
            r'\b' + re.escape(original) + r'\b',
            renamed_name,
            new_content
        )

    return new_content, renamed

# ── Junk inserter ─────────────────────────────────────────────

def insert_junk(content):
    """
    Insert dead code blocks at the start of each function body.
    Detects function bodies by finding opening brace after
    a function signature pattern.
    """
    # Match function body opening brace
    # Looks for: ) { or ) \n{
    func_body_pattern = re.compile(
        r'(\b(?:int|void|char|BOOL|DWORD|auto)\b[^;{]*\([^)]*\)\s*\{)',
        re.MULTILINE | re.DOTALL
    )

    total_junk = 0
    offset     = 0
    result     = content

    for m in func_body_pattern.finditer(content):
        # Insert junk right after opening brace
        insert_pos = m.end() + offset
        junk, count = generate_junk_block(random.randint(3, 6))
        insertion  = "\n" + junk + "\n"
        result     = result[:insert_pos] + insertion + result[insert_pos:]
        offset    += len(insertion)
        total_junk += count

    return result, total_junk

# ── Main transform ────────────────────────────────────────────

def transform(input_path, output_path):
    print(f"[*] Polymorphic transform: {input_path}")

    with open(input_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    original_size = len(content)

    # Stage 1: Rename variables
    content, renamed_count = rename_variables(content)

    # Stage 2: Insert junk code blocks
    content, junk_count = insert_junk(content)

    new_size = len(content)

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"[+] Variables renamed:  {renamed_count}")
    print(f"[+] Junk inserted:      {junk_count}")
    print(f"[+] Output:             {output_path} (+{new_size - original_size} bytes)")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: poly_engine.py input.cpp output.cpp")
        sys.exit(1)
    random.seed()  # Truly random seed each run
    transform(sys.argv[1], sys.argv[2])