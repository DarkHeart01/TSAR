# JOCKY Compiler — Layer 1 & 2

Custom LLVM/Polaris-based compiler pipeline that defeats AV/EDR static detection.
Built for **SIH 2026 NTRO PS 26148**.

---

## Prerequisites

- Windows x64
- Python 3
- Windows SDK 10.0.26100.0
- Visual Studio 2022 BuildTools (v14.51+)
- System clang for linking — `winget install LLVM.LLVM`

---

## Quick Start

```powershell
# From the compiler/ directory:

# JOCKY DSL source (recommended)
.\jocky\driver\jocky.exe payload.jky -o payload.exe

# C / C++ source
.\jocky\driver\jocky.exe payload.cpp -o payload.exe

# Pre-compiled LLVM IR (skips poly transform)
.\jocky\driver\jocky.exe payload.ll -o payload.exe
```

> **PowerShell note:** always quote the passes flag: `"-passes=fla,sub"`

---

## Supported Input Types

| Extension | Description | Poly Transform |
|-----------|-------------|----------------|
| `.jky` | JOCKY DSL source | Yes — JOCKY-syntax transforms |
| `.cpp` / `.c` | C / C++ source | Yes — C++ transforms |
| `.ll` | LLVM IR | No |

`.jky` files are compiled via the JOCKY DSL frontend (`jocky_v1/`) to LLVM IR first, then fed into Polaris.

---

## Full Command Reference

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

# Skip PE spoofing
.\jocky\driver\jocky.exe payload.jky -o payload.exe -no-spoof

# Verbose output
.\jocky\driver\jocky.exe payload.jky -o payload.exe -v

# C++ with all recommended passes
.\jocky\driver\jocky.exe payload.cpp -o payload.exe "-passes=fla,sub,api-hash"
```

---

## Available Obfuscation Passes

| Pass | What It Does | Status |
|------|-------------|--------|
| `fla` | CFG flattening — destroys control flow structure | ✅ Recommended |
| `sub` | Instruction substitution — replaces arithmetic ops | ✅ Recommended |
| `api-hash` | API name hashing via PEB walk — removes IAT strings | ✅ Recommended |
| `mba` | Mixed boolean arithmetic — hardens expressions | ✅ Optional |
| `indcall` | Indirect call obfuscation | ✅ Optional |
| `indbr` | Indirect branch obfuscation | ✅ Optional |
| `bcf` | Bogus control flow — fake branches | ⚠️ Avoid — triggers detections |
| `gvenc` | Global variable (string) encryption | ⚠️ Avoid — XOR stub fingerprinted, raises .rdata entropy |

**Default:** `fla,sub,api-hash`

---

## Pipeline Stages

```
Input (.jky / .cpp / .ll)
        │
        ▼ Stage 3   poly_engine.py
        │           Variable rename + dead code injection
        │           (JOCKY-syntax for .jky, C++ syntax for .cpp, skipped for .ll)
        │
        ▼ Stage 3B  JOCKY DSL frontend  [.jky only]
        │           .jky → LLVM IR (.ll)
        │
        ▼ Stage 1A  Polaris clang (patched LLVM 16)
        │           Applies obfuscation passes on IR
        │
        ▼ Stage 1B  lld linker
        │           Modular runtime link (core + syscall + optional modules)
        │
        ▼ Stage 6   pe_header_spoofer.py
        │           Rich Header graft, timestamp fake, version identity rotation
        │
        Output .exe
```

---

## Modular Runtime

For `.jky` and `.ll` inputs, the pipeline links a custom nostdlib runtime instead of the CRT. Modules are auto-selected based on symbol usage in the source.

| Object | Always linked | Purpose |
|--------|--------------|---------|
| `core.obj` | ✅ | Entry point, alloc/free, I/O, thread management |
| `syscall.obj` | ✅ | Direct NT syscalls via Hell's Gate (bypasses EDR hooks) |
| `dummy_imports.obj` | ✅ | Benign IAT entries across 5 DLLs (normal-looking imports) |
| `entropy_pad.obj` | ✅ | Entropy normalisation padding |
| `strings.obj` | ✅ if present | Benign string tables compiled into .rdata |
| `crypto.obj` | if used | AES/XOR crypto primitives |
| `memory.obj` | if used | Cross-process read/write |
| `process.obj` | if used | Process creation, thread suspend/resume |

---

## PE Header Spoofing

`pe_header_spoofer.py` runs post-link on every build and applies:

- **Rich Header** grafted from an MSVC-compiled Windows binary template
- **Timestamp** randomised within a plausible historical range
- **Version identity** rotated across a pool of legitimate software identities:
  - 7-Zip File Manager
  - VLC media player 3.0.20.0
  - LibreOffice
  - WinSCP 6.1.2.0
- **VS_VERSIONINFO** resource block + application manifest injected into `.rsrc`
- **PE checksum** recalculated

> Firefox was removed from the identity pool — Microsoft Defender performs identity-fingerprint cross-checking and flags binaries claiming to be Firefox that do not match known Firefox IAT/size profiles.

---

## Polymorphic Engine (`poly_engine.py`)

Transforms source before compilation so every build produces structurally distinct IR.

**C++ / C transforms:**
- Local variable renaming (`int x` → `int __jk_a3f9b2c1`)
- Dead variable chains with void-casts to prevent elision
- Always-false conditional dead blocks

**JOCKY DSL transforms:**
- `let`-variable renaming (`let x: int` → `let _jk_a3f9b2c1: int`)
- Dead int declaration chains with arithmetic cross-references
- Always-false conditional blocks (`if (x & 0) == x { ... }`)

Both modes insert transforms at every function body opening. Random seed per run ensures no two outputs share the same structure.

---

## Directory Structure

```
compiler/
├── build/
│   └── Release/bin/
│       ├── clang.exe          ← Prebuilt Polaris-patched LLVM 16 clang
│       └── clang++.exe
│
├── jocky/
│   ├── driver/
│   │   ├── jocky.cpp          ← Pipeline driver source (no std::string — compiles with Polaris)
│   │   └── jocky.exe          ← Prebuilt pipeline driver
│   ├── polymorphic/
│   │   └── poly_engine.py     ← Dual-language polymorphic transformer (.cpp + .jky)
│   ├── runtime/
│   │   ├── runtime_core.cpp   ← Core entry, alloc, I/O, threads
│   │   ├── runtime_syscall.cpp← Direct NT syscalls (Hell's Gate SSN resolution)
│   │   ├── runtime_crypto.cpp ← AES/XOR primitives
│   │   ├── runtime_memory.cpp ← Cross-process memory ops
│   │   ├── runtime_process.cpp← Process/thread management
│   │   ├── runtime_strings.cpp← Benign string tables (.rdata noise)
│   │   ├── runtime_tls.cpp    ← TLS slot management
│   │   ├── peb_walk.h         ← PEB walking + djb2 API hashing (zero imports)
│   │   ├── string_hide.h      ← Compile-time string obfuscation
│   │   ├── dummy_imports.cpp  ← Benign IAT population
│   │   ├── entropy_pad.cpp    ← .text entropy normalisation
│   │   ├── *.obj              ← Precompiled runtime objects
│   │   └── runtime.cpp        ← Legacy monolithic runtime
│   └── tools/
│       ├── pe_header_spoofer.py   ← PE metadata spoofer
│       └── extract_rich_header.py ← Extract Rich Header from any binary
│
├── config/
│   └── msvc_rich_template.bin ← MSVC Rich Header template
│
├── src/
│   └── Flattening.cpp         ← Modified Polaris CFG flattening source
│
└── README.md
```

The JOCKY DSL frontend lives at `../../jocky_v1/build/Release/jocky.exe` (relative to the driver) and is auto-detected at runtime.

---

## Build jocky.exe (if needed)

The driver has no C++ STL dependency — compiles directly with Polaris clang:

```powershell
.\build\Release\bin\clang.exe jocky\driver\jocky.cpp `
  -o jocky\driver\jocky.exe `
  -target x86_64-pc-windows-msvc `
  -lkernel32 -luser32 `
  -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH
```

---

## Rebuild Polaris Clang (if obfuscation source changed)

```powershell
# Rebuild obfuscation pass library
msbuild "build\lib\Transforms\Obfuscation\LLVMObfuscation.vcxproj" /p:Configuration=Release /maxcpucount:4

# Relink clang.exe
msbuild "build\tools\clang\tools\driver\clang.vcxproj" /p:Configuration=Release /maxcpucount:16
```

---

## VirusTotal Results (benchmark binary, fla+sub+api-hash)

| Binary | Detection | Notes |
|--------|-----------|-------|
| Unobfuscated | 0/71 | Clean baseline |
| fla only | 1/71 | |
| fla + sub | 1/71 | |
| fla + sub + spoof | 4/71 | Hard floor — cloud/DNN engines |

**Hard floor (4/71) — cannot be reduced without code signing:**

| Engine | Detection | Type |
|--------|-----------|------|
| CrowdStrike Falcon | Win/malicious_confidence_70% | Cloud ML |
| DeepInstinct | MALICIOUS | Deep neural network |
| McAfee | Ti! (hash-based) | Ephemeral — changes each build |
| SentinelOne Static ML | Static AI - Suspicious PE | Static heuristic |

**Research findings:**
- `gvenc` disabled: global encryption raises `.rdata` entropy, triggers ML detectors
- `bcf` disabled: opaque predicate pattern is a known malware IOC
- `0xCC` cave (INT3 padding) in `.text` triggers BitDefender "Lazy" family static detection
- FNV-1a hash constants (`0x811c9dc5`, `0x01000193`) are in BitDefender's malware database
- djb2 (seed `0x1505`) and FNV-1a are both named by VT Code Insights — well-known algorithm constants are IOCs
- Firefox version identity triggers Microsoft harder than WinSCP/VLC (identity-fingerprint cross-check)
