# JOCKY Compiler — Polaris Pipeline

> A patched LLVM 16 toolchain with custom obfuscation passes, a polymorphic source transformer, and a PE metadata spoofer. Takes JOCKY DSL source, C/C++, or pre-compiled LLVM IR and produces a hardened Windows PE executable that is structurally unique on every build.

---

## Overview

The compiler is the core transformation engine of TSAR. It takes source and produces binaries that defeat static analysis through four independent mechanisms applied in sequence:

1. **Polymorphic source transformation** — source is mutated before compilation so that the IR entering LLVM is structurally unique per run
2. **LLVM obfuscation passes** — CFG flattening, instruction substitution, API name hashing, and MBA applied at the IR level by Polaris (a patched LLVM 16 `clang`)
3. **Modular nostdlib runtime** — the binary is linked against a custom runtime that uses direct NT syscalls instead of the Windows API, eliminating all meaningful IAT entries
4. **PE metadata spoofing** — the output binary's Rich Header, timestamp, version information, and checksum are rewritten post-link to match legitimate software identities

Each mechanism attacks a different detection surface. Together they produce binaries that pass as benign software to static scanners while retaining full functionality.

---

## Prerequisites

- Windows x64
- Python 3
- Windows SDK 10.0.26100.0
- Visual Studio 2022 Build Tools (v14.51+)
- System clang for linking: `winget install LLVM.LLVM`

---

## Quick Start

```powershell
# From the compiler/ directory

# JOCKY DSL source — recommended
.\jocky\driver\jocky.exe payload.jky -o payload.exe

# C / C++ source
.\jocky\driver\jocky.exe payload.cpp -o payload.exe

# Pre-compiled LLVM IR — skips polymorphic transform
.\jocky\driver\jocky.exe payload.ll -o payload.exe
```

---

## Supported Inputs

| Extension | Description | Polymorphic Transform |
|---|---|---|
| `.jky` | JOCKY DSL source | Yes — JOCKY-syntax transforms via `poly_engine.py` |
| `.cpp` / `.c` | C / C++ source | Yes — C++ transforms via `poly_engine.py` |
| `.ll` | Pre-compiled LLVM IR | No — fed directly to Polaris |

`.jky` files are compiled by the JOCKY DSL frontend (`jocky_v1/`) to LLVM IR first, then fed into Polaris. The driver handles this automatically.

---

## Full CLI Reference

```powershell
.\jocky\driver\jocky.exe <input> -o <output.exe> [options]

Options:
  -passes=<list>          Obfuscation passes, comma-separated (default: fla,sub,api-hash)
  -no-poly                Skip polymorphic source transform
  -no-spoof               Skip PE header spoofing
  -nostdlib               Link modular JOCKY runtime instead of CRT (auto-set for .jky/.ll)
  -entry=<name>           Entry point symbol (default: main; auto-set to jocky_entry for .jky/.ll)
  -spoof-template=<path>  Custom Rich Header template binary
  -v                      Verbose — print every sub-command as it runs
```

### Examples

```powershell
# Default pipeline — fla + sub + api-hash, full PE spoof
.\jocky\driver\jocky.exe payload.jky -o payload.exe

# Extended obfuscation
.\jocky\driver\jocky.exe payload.jky -o payload.exe "-passes=fla,sub,api-hash,mba,indcall"

# Skip polymorphic transform for faster iteration during development
.\jocky\driver\jocky.exe payload.jky -o payload.exe -no-poly

# C++ source, all recommended passes
.\jocky\driver\jocky.exe payload.cpp -o payload.exe "-passes=fla,sub,api-hash"

# Verbose output — shows every stage and sub-command
.\jocky\driver\jocky.exe payload.jky -o payload.exe -v
```

---

## Pipeline Stages

```
Input (.jky / .cpp / .ll)
        │
        ▼  Stage 3 ── Polymorphic Source Transform
        │             poly_engine.py
        │             Mutates source before compilation:
        │               - Variable renaming (random hex identifiers)
        │               - Dead variable chains with void-casts
        │               - Always-false conditional dead blocks
        │             Random seed per run — no two outputs share structure.
        │             Skipped for .ll inputs (IR cannot be source-transformed).
        │
        ▼  Stage 3B ─ JOCKY DSL Frontend  [.jky only]
        │             Invokes jocky_v1/build/Release/jocky.exe
        │             Compiles .jky source → LLVM IR (.ll)
        │
        ▼  Stage 1A ─ Polaris clang (patched LLVM 16)
        │             Compiles .ll → object file.
        │             Applies obfuscation passes on the IR before codegen.
        │             Passes run in order: fla → sub → api-hash → (mba) → (indcall)
        │
        ▼  Stage 1B ─ lld linker
        │             Links the object against the modular JOCKY runtime.
        │             Modules selected based on symbol usage in the source.
        │             No CRT — entry point is jocky_entry or main.
        │
        ▼  Stage 6 ── PE Header Spoofer
        │             pe_header_spoofer.py
        │             Post-link rewrite of all PE metadata:
        │               - Rich Header graft from MSVC template
        │               - Timestamp randomisation
        │               - Version identity rotation
        │               - VS_VERSIONINFO + manifest injection
        │               - PE checksum recalculation
        │
        Output .exe
```

---

## Obfuscation Passes

Polaris is a patched build of LLVM 16's `clang` with custom obfuscation passes added to the optimisation pipeline. Passes run at the IR level before native code generation.

| Pass | What it does |
|---|---|
| `fla` | **CFG Flattening** — transforms all control flow into a single `switch` dispatch loop. The original structure (branches, loops, conditionals) is completely destroyed at the IR level. Pattern-based CFG analysis finds nothing recognisable. |
| `sub` | **Instruction Substitution** — replaces arithmetic and boolean operations with functionally equivalent but structurally different sequences (e.g. `a + b` → `a - (-b)`, boolean ops → bit manipulation chains). Hardens against decompiler pattern matching. |
| `api-hash` | **API Name Hashing** — removes all Windows API imports from the IAT. API calls are resolved at runtime via a PEB walk over loaded modules, computing a djb2 hash of each export name and comparing against pre-computed constants in the code. The IAT contains only benign dummy entries. |
| `mba` | **Mixed Boolean Arithmetic** — replaces expressions with semantically equivalent MBA expansions. Hardens against symbolic execution and SMT-based analysis. Optional — adds significant complexity to the binary. |
| `indcall` | **Indirect Call Obfuscation** — replaces direct call instructions with indirect calls through computed pointers. Defeats simple call-graph reconstruction. |
| `indbr` | **Indirect Branch Obfuscation** — similar to `indcall` but for branch targets. |

**Default: `fla,sub,api-hash`**

Two passes are intentionally excluded from the defaults:

| Pass | Reason excluded |
|---|---|
| `bcf` | Bogus control flow inserts opaque predicates. The specific predicate patterns used are indexed as malware IOCs by several engines — using it raises the detection rate rather than lowering it. |
| `gvenc` | Global variable encryption raises `.rdata` section entropy significantly. High entropy in `.rdata` is a strong ML signal for packed/encrypted binaries. |

---

## Modular Runtime

For `.jky` and `.ll` inputs, the pipeline links a custom nostdlib runtime instead of the Windows CRT. This eliminates all CRT-associated imports and init sequences that would identify the binary as a compiler-generated artifact.

The runtime is split into independent modules. The linker selects only the modules whose symbols are actually referenced, keeping the binary lean.

| Module | Always linked | Purpose |
|---|---|---|
| `core.obj` | Yes | Entry point (`jocky_entry`), memory allocation, basic I/O, thread management |
| `syscall.obj` | Yes | Direct NT syscalls via Hell's Gate SSN resolution — bypasses all EDR hooks in `ntdll.dll` |
| `dummy_imports.obj` | Yes | Populates the IAT with benign-looking imports across 5 common DLLs to make the import table appear normal |
| `entropy_pad.obj` | Yes | Padding in `.text` that normalises section entropy to a range typical of legitimate software |
| `strings.obj` | If present | Benign string tables compiled into `.rdata` to further normalise the section's content |
| `crypto.obj` | If used | AES-256 and XOR encryption primitives |
| `memory.obj` | If used | Cross-process read/write operations |
| `process.obj` | If used | Process creation, thread suspend/resume |
| `tls.obj` | If used | TLS slot allocation and management |

### Hell's Gate Syscall Resolution

`syscall.obj` implements Hell's Gate, a technique for resolving NT syscall service numbers (SSNs) at runtime without importing anything from `ntdll.dll`:

1. Walk the Process Environment Block (PEB) to locate the base address of `ntdll.dll` in memory
2. Parse the module's export directory to find the target function
3. Read the `mov eax, <ssn>` instruction from the function's stub to extract the service number
4. Issue the syscall directly using an inline assembly stub

This completely bypasses any hooks that an EDR has installed by patching `ntdll.dll` stubs — the hook is never reached because the call never goes through `ntdll.dll`.

---

## PE Header Spoofing

`pe_header_spoofer.py` runs post-link on every build. It rewrites PE metadata to make the binary resemble a known, legitimate application:

### Rich Header

The Rich Header is a pre-PE metadata block inserted by the Microsoft linker containing version fingerprints of the build tools used. Polaris-built binaries would produce a distinctive Rich Header that identifies them as LLVM output. The spoofer grafts a Rich Header extracted from a genuine MSVC-compiled Windows binary over the top.

### Timestamp

The `TimeDateStamp` field in the PE COFF header is randomised within a plausible historical range, removing any correlation between build time and deployment time.

### Version Identity

A `VS_VERSIONINFO` resource block and application manifest are injected into the `.rsrc` section. The identity is selected at random from a pool of legitimate software:

| Identity |
|---|
| 7-Zip File Manager |
| VLC media player 3.0.20.0 |
| LibreOffice |
| WinSCP 6.1.2.0 |

Firefox is excluded from the pool. Microsoft Defender performs identity-fingerprint cross-checking and flags PE files that claim to be Firefox but do not match known Firefox IAT and size profiles.

### Checksum

The `CheckSum` field in the optional header is recalculated after all modifications are applied, ensuring the binary passes integrity checks.

---

## Polymorphic Engine

`poly_engine.py` is a dual-mode source transformer that supports both C/C++ and JOCKY DSL syntax.

### C++ / C mode

Applied to `.cpp` and `.c` inputs:

- **Variable renaming** — all local variable declarations are renamed: `int counter` → `int __jk_a3f9b2c1`
- **Dead variable chains** — sequences of unused variable declarations with void-cast suppressions to prevent the compiler from eliding them entirely
- **Always-false dead blocks** — `if (0) { <code> }` style dead branches inserted at function entry

### JOCKY DSL mode

Applied to `.jky` inputs:

- **`let` variable renaming** — `let x: int` → `let _jk_a3f9b2c1: int`
- **Dead int chains** — sequences of dummy `let` declarations with arithmetic cross-references
- **Always-false conditional blocks** — `if (x & 0) == x { <dead code> }`

Both modes use a fresh random seed per invocation. No two consecutive runs of the same source produce the same transformed output.

---

## Building from Source

### Rebuild the pipeline driver

The driver (`jocky.cpp`) has no C++ STL dependency and compiles directly with Polaris clang:

```powershell
.\build\Release\bin\clang.exe jocky\driver\jocky.cpp `
  -o jocky\driver\jocky.exe `
  -target x86_64-pc-windows-msvc `
  -lkernel32 -luser32 `
  -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH
```

### Rebuild Polaris clang

Required only if the obfuscation pass source (`src/Flattening.cpp` or other pass sources) has changed:

```powershell
# Rebuild the obfuscation pass library
msbuild "build\lib\Transforms\Obfuscation\LLVMObfuscation.vcxproj" /p:Configuration=Release /maxcpucount:4

# Relink clang.exe against the updated library
msbuild "build\tools\clang\tools\driver\clang.vcxproj" /p:Configuration=Release /maxcpucount:16
```

---

## Directory Layout

```
compiler/
├── build/
│   └── Release/bin/
│       ├── clang.exe                   Prebuilt Polaris-patched LLVM 16 clang
│       └── clang++.exe
│
├── jocky/
│   ├── driver/
│   │   ├── jocky.cpp                   Pipeline driver source (no STL dependency)
│   │   └── jocky.exe                   Prebuilt pipeline driver
│   │
│   ├── polymorphic/
│   │   └── poly_engine.py              Dual-language source transformer
│   │
│   ├── runtime/
│   │   ├── runtime_core.cpp            Entry point, alloc, I/O, threads
│   │   ├── runtime_syscall.cpp         Direct NT syscalls (Hell's Gate SSN resolution)
│   │   ├── runtime_crypto.cpp          AES-256 / XOR primitives
│   │   ├── runtime_memory.cpp          Cross-process memory read/write
│   │   ├── runtime_process.cpp         Process creation, thread management
│   │   ├── runtime_strings.cpp         Benign string tables for .rdata normalisation
│   │   ├── runtime_tls.cpp             TLS slot management
│   │   ├── peb_walk.h                  PEB walking + djb2 export name hashing
│   │   ├── string_hide.h               Compile-time string literal obfuscation macros
│   │   ├── dummy_imports.cpp           Benign IAT population (5 DLLs)
│   │   ├── entropy_pad.cpp             .text entropy normalisation padding
│   │   └── *.obj                       Precompiled runtime objects
│   │
│   └── tools/
│       ├── pe_header_spoofer.py        Post-link PE metadata rewriter
│       └── extract_rich_header.py      Utility: extract Rich Header from any binary
│
├── config/
│   └── msvc_rich_template.bin          MSVC Rich Header template for grafting
│
├── src/
│   └── Flattening.cpp                  Modified Polaris CFG flattening pass source
│
└── README.md
```

---

## Relationship to Other Components

| Component | Relationship |
|---|---|
| `jocky_v1/` | Provides the `.ll` IR that this pipeline obfuscates and links |
| `jocky-framework/` | The build server invokes `jocky.exe` directly to fulfil operator build requests |
| `cicd/` | The CI/CD worker calls `jocky.exe` to execute automated polymorphic rebuilds |
| `processhollowing/` | The output binary is the payload loaded by the hollowing module |
