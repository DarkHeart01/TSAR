#!/usr/bin/env python3
"""
poly_engine.py
JOCKY Polymorphic Engine

Transforms C++ source before compilation.
Every run produces structurally unique source code.
Defeats hash-based and string-based AV detection.

Usage: python poly_engine.py input.cpp output.cpp
"""

import re
import random
import string
import sys

def random_id(length=8):
    """Generate a random valid C++ identifier"""
    first = random.choice(string.ascii_lowercase + '_')
    rest  = ''.join(random.choices(
        string.ascii_lowercase + string.digits + '_',
        k=length-1))
    return first + rest

def rename_local_variables(source):
    """
    Rename local variable declarations with random names.
    Preserves: function names, types, keywords, macros.
    """
    # Match: type varname = ... or type varname;
    pattern = re.compile(
        r'\b(int|char|void|DWORD|BOOL|HANDLE|PVOID|QWORD|'
        r'unsigned\s+int|unsigned\s+long|unsigned\s+char)\s+'
        r'([a-zA-Z_][a-zA-Z0-9_]*)\s*([=;(,)])'
    )

    # Names to never rename
    protected = {
        'main', 'jocky_entry', 'jocky_write', 'jocky_exit',
        'jocky_alloc', 'jocky_protect', 'jocky_get_stdout',
        'jocky_create_thread', 'jocky_wait',
        'NULL', 'TRUE', 'FALSE', 'INFINITE',
        'rv', 'th', 'handle', 'written', 'ret'
    }

    name_map = {}

    def replace_decl(match):
        type_name = match.group(1)
        var_name  = match.group(2)
        suffix    = match.group(3)

        if var_name in protected:
            return match.group(0)
        if var_name.startswith('__'):
            return match.group(0)

        if var_name not in name_map:
            name_map[var_name] = '_' + random_id(6)

        return f"{type_name} {name_map[var_name]}{suffix}"

    result = pattern.sub(replace_decl, source)

    # Apply renames to all usages of renamed variables
    for old_name, new_name in name_map.items():
        result = re.sub(r'\b' + re.escape(old_name) + r'\b',
                        new_name, result)

    return result, len(name_map)

def insert_junk_variables(source):
    """
    Insert meaningless volatile variable declarations.
    Changes binary layout and hash every build.
    volatile prevents optimizer from removing them.
    """
    junk_templates = [
        'volatile int {n} = {v};',
        'volatile unsigned int {n} = {v}U;',
        'volatile int {n} = ({v} ^ {v2});',
        'volatile long {n} = {v}L;',
    ]

    lines = source.split('\n')
    result = []
    junk_count = 0

    for line in lines:
        result.append(line)
        stripped = line.strip()

        # Insert junk after opening brace of function body
        if stripped == '{' and random.random() < 0.35:
            template = random.choice(junk_templates)
            junk_line = '    ' + template.format(
                n  = '_j' + random_id(5),
                v  = random.randint(100, 99999),
                v2 = random.randint(100, 99999)
            )
            result.append(junk_line)
            junk_count += 1

    return '\n'.join(result), junk_count

def reorder_declarations(source):
    """
    Shuffle consecutive independent variable declarations.
    Changes IR structure between builds.
    Only shuffles declarations — preserves logic.
    """
    # Pattern for simple declarations on their own line
    decl_re = re.compile(
        r'^(\s*)(int|char|DWORD|BOOL|HANDLE|PVOID)\s+'
        r'\w+\s*=\s*[^;{]+;'
    )

    lines = source.split('\n')
    result = []
    buffer = []

    for line in lines:
        if decl_re.match(line):
            buffer.append(line)
        else:
            if len(buffer) > 1:
                random.shuffle(buffer)
            result.extend(buffer)
            buffer = []
            result.append(line)

    result.extend(buffer)
    return '\n'.join(result)

def add_build_comment(source):
    """Add unique build ID comment — changes hash slightly"""
    build_id = ''.join(random.choices(string.hexdigits.lower(), k=16))
    comment = f"// JOCKY build: {build_id}\n"
    return comment + source

def transform(input_path, output_path):
    print(f"[*] Polymorphic transform: {input_path}")

    with open(input_path, 'r', encoding='utf-8', errors='ignore') as f:
        source = f.read()

    original_len = len(source)

    # Apply transforms in order
    source = add_build_comment(source)
    source, var_count  = rename_local_variables(source)
    source, junk_count = insert_junk_variables(source)
    source = reorder_declarations(source)

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(source)

    print(f"[+] Variables renamed:  {var_count}")
    print(f"[+] Junk inserted:      {junk_count}")
    print(f"[+] Output:             {output_path} "
          f"({len(source) - original_len:+d} bytes)")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: poly_engine.py input.cpp output.cpp")
        sys.exit(1)
    transform(sys.argv[1], sys.argv[2])