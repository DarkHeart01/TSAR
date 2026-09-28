# JOCKY Compiler — Polaris Pipeline

The compiler directory contains the full build pipeline that takes JOCKY DSL source (`.jky`), C/C++ source, or pre-compiled LLVM IR (`.ll`) and produces a hardened Windows PE executable. It is built on a patched LLVM 16 toolchain — **Polaris** — with custom obfuscation passes, a polymorphic source transformer, and a PE metadata spoofer.

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
# From compiler/

# JOCKY DSL source (recommended)
.\jocky\driver\jocky.exe payload.jky -o payload.exe

# C / C++ source
.\jocky\driver\jocky.exe payload.cpp -o payload.exe

# Pre-compiled LLVM IR (skips polymorphic transform)
.\jocky\driver\jocky.exe payload.ll -o payload.exe
```

---

## Supported Inputs

| Extension | Description | Polymorphic Transform |
|---|---|---|
| `.jky` | JOCKY DSL source | Yes — JOCKY-syntax transforms |
| `.cpp` / `.c` | C / C++ source | Yes — C++ transforms |
| `.ll` | LLVM IR | No |

`.jky` files are compiled via the JOCKY DSL frontend (`jocky_v1/`) to LLVM IR first, then fed into Polaris.

---

## Full CLI Reference

```powershell
.\jocky\driver\jocky.exe <input> -o <output.exe> [options]

Options:
  -passes=<list>          Obfuscation passes, comma-separated (default: fla,sub,api-hash)
  -no-poly                Skip polymorphic source transform
  -no-spoof               Skip PE header spoofing
  -nostdlib               Use modular JOCKY runtime (auto-set for .jky and .ll inputs)
  -entry=<name>           Entry point symbol (default: main; auto-set to jocky_entry for .jky/.ll)
  -spoof-template=<path>  Custom Rich Header template binary
  -v                      Verbose — print every sub-command
```

### Examples

```powershell
# Default pipeline — fla + sub + api-hash, full spoof
.\jocky\driver\jocky.exe payload.jky -o payload.exe

# Custom passes
.\jocky\driver\jocky.exe payload.jky -o payload.exe "-passes=fla,sub,mba,indcall"

# Skip polymorphic transform (faster iteration)
.\jocky\driver\jocky.exe payload.jky -o payload.exe -no-poly

# Verbose output
.\jocky\driver\jocky.exe payload.jky -o payload.exe -v
```

---

## Obfuscation Passes

| Pass | Effect | Status |
|---|---|---|
| `fla` | CFG flattening — destroys control flow structure | Recommended |
| `sub` | Instruction substitution — replaces arithmetic ops with equivalent forms | Recommended |
| `api-hash` | API name hashing via PEB walk — removes all IAT strings | Recommended |
| `mba` | Mixed boolean arithmetic — hardens expressions against symbolic analysis | Optional |
| `indcall` | Indirect call obfuscation | Optional |
| `indbr` | Indirect branch obfuscation | Optional |
| `bcf` | Bogus control flow — fake branches | Avoid — opaque predicate pattern is a known malware IOC |
| `gvenc` | Global variable (string) encryption | Avoid — XOR stub raises `.rdata` entropy, triggers ML detectors |

**Default:** `fla,sub,api-hash`

---

## Pipeline Stages

```
Input (.jky / .cpp / .ll)
        │
        ▼  Stage 3    poly_engine.py
        │             Variable rename + dead code injection
        │             (.jky syntax for .jky inputs, C++ syntax for .cpp, skipped for .ll)
        │
        ▼  Stage 3B   JOCKY DSL frontend  [.jky only]
        │             .jky → LLVM IR (.ll)
        │
        ▼  Stage 1A   Polaris clang (patched LLVM 16)
        │             Applies obfuscation passes on IR
        │
        ▼  Stage 1B   lld linker
        │             Modular runtime link (core + syscall + optional modules)
        │
        ▼  Stage 6    pe_header_spoofer.py
        │             Rich Header graft, timestamp rotation, version identity swap
        │
        Output .exe
```

---

## Modular Runtime

For `.jky` and `.ll` inputs, the pipeline links a custom nostdlib runtime in place of the CRT. Modules are auto-selected based on symbol usage.

| Object | Always linked | Purpose |
|---|---|---|
| `core.obj` | Yes | Entry point, alloc/free, I/O, thread management |
| `syscall.obj` | Yes | Direct NT syscalls via Hell's Gate (bypasses EDR hooks) |
| `dummy_imports.obj` | Yes | Benign IAT entries across 5 DLLs |
| `entropy_pad.obj` | Yes | `.text` entropy normalisation |
| `strings.obj` | If present | Benign string tables compiled into `.rdata` |
| `crypto.obj` | If used | AES/XOR primitives |
| `memory.obj` | If used | Cross-process read/write |
| `process.obj` | If used | Process creation, thread suspend/resume |

Hell's Gate resolves NT syscall numbers at runtime by walking the PEB, ensuring no `ntdll.dll` imports appear in the IAT and no static syscall stubs are present for EDR hooks to intercept.

---

## Polymorphic Engine

`poly_engine.py` transforms source before compilation so every build produces structurally distinct IR.

**C++ / C transforms:**
- Local variable renaming (`int x` → `int __jk_a3f9b2c1`)
- Dead variable chains with void-casts to prevent elision
- Always-false conditional dead blocks

**JOCKY DSL transforms:**
- `let`-variable renaming (`let x: int` → `let _jk_a3f9b2c1: int`)
- Dead int declaration chains with arithmetic cross-references
- Always-false conditional blocks (`if (x & 0) == x { ... }`)

Random seed per run ensures no two outputs share the same structure.

---

## PE Header Spoofing

`pe_header_spoofer.py` runs post-link on every build:

- **Rich Header** grafted from an MSVC-compiled Windows binary template
- **Timestamp** randomised within a plausible historical range
- **Version identity** rotated across: 7-Zip File Manager, VLC 3.0.20.0, LibreOffice, WinSCP 6.1.2.0
- **VS_VERSIONINFO** resource block + application manifest injected into `.rsrc`
- **PE checksum** recalculated

Firefox is excluded from the identity pool — Microsoft Defender performs identity-fingerprint cross-checking and flags binaries claiming to be Firefox that do not match known Firefox IAT and size profiles.

---

## Detection Benchmark

| Binary | Detections (VirusTotal) | Notes |
|---|---|---|
| Unobfuscated | 0/71 | Clean baseline |
| `fla` only | 1/71 | |
| `fla` + `sub` | 1/71 | |
| `fla` + `sub` + `spoof` | 4/71 | Hard floor — cloud/ML engines |

Remaining detections (4/71) are CrowdStrike Falcon (cloud ML), DeepInstinct (DNN), McAfee Ti! (hash-based, ephemeral), SentinelOne Static ML. All 67 signature-based and static-heuristic engines report clean.

**Known detection triggers to avoid:**
- `gvenc` — raises `.rdata` entropy
- `bcf` — opaque predicate pattern is indexed by BitDefender
- `0xCC` cave padding in `.text` — triggers BitDefender "Lazy" family detection
- FNV-1a constants (`0x811c9dc5`, `0x01000193`) are in BitDefender's malware database
- djb2 seed `0x1505` and FNV-1a are both named by VT Code Insights

---

## Directory Layout

```
compiler/
├── build/
│   └── Release/bin/
│       ├── clang.exe           Prebuilt Polaris-patched LLVM 16 clang
│       └── clang++.exe
├── jocky/
│   ├── driver/
│   │   ├── jocky.cpp           Pipeline driver (no STL dependency)
│   │   └── jocky.exe           Prebuilt pipeline driver
│   ├── polymorphic/
│   │   └── poly_engine.py      Dual-language polymorphic transformer
│   ├── runtime/
│   │   ├── runtime_core.cpp    Core entry, alloc, I/O, threads
│   │   ├── runtime_syscall.cpp Direct NT syscalls (Hell's Gate)
│   │   ├── runtime_crypto.cpp  AES/XOR primitives
│   │   ├── runtime_memory.cpp  Cross-process memory ops
│   │   ├── runtime_process.cpp Process/thread management
│   │   ├── runtime_strings.cpp Benign string tables
│   │   ├── peb_walk.h          PEB walking + djb2 API hashing
│   │   ├── string_hide.h       Compile-time string obfuscation
│   │   ├── dummy_imports.cpp   Benign IAT population
│   │   ├── entropy_pad.cpp     .text entropy normalisation
│   │   └── *.obj               Precompiled runtime objects
│   └── tools/
│       ├── pe_header_spoofer.py    PE metadata spoofer
│       └── extract_rich_header.py  Extract Rich Header from any binary
├── config/
│   └── msvc_rich_template.bin  MSVC Rich Header template
├── src/
│   └── Flattening.cpp          Modified Polaris CFG flattening source
└── README.md
```

---

## Building from Source

### Rebuild the pipeline driver

The driver has no C++ STL dependency and compiles directly with Polaris clang:

```powershell
.\build\Release\bin\clang.exe jocky\driver\jocky.cpp `
  -o jocky\driver\jocky.exe `
  -target x86_64-pc-windows-msvc `
  -lkernel32 -luser32 `
  -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH
```

### Rebuild Polaris clang (if obfuscation source changed)

```powershell
# Rebuild obfuscation pass library
msbuild "build\lib\Transforms\Obfuscation\LLVMObfuscation.vcxproj" /p:Configuration=Release /maxcpucount:4

# Relink clang.exe
msbuild "build\tools\clang\tools\driver\clang.vcxproj" /p:Configuration=Release /maxcpucount:16
```
