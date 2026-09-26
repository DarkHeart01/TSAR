## JOCKY compiler

pre-req:
1. Python 3
2. Clang (system, for linking)
3. Windows SDK at 10.0.26100.0
4. VS BuildTools 2022

## Command to run the JOCKY pipeline to get obfuscated .exe
1. Change to jocky directory
    `cd compiler\jocky\driver`
2. Command
    `.\jocky.exe \path\to\your\file.cpp -o \path\to\your\result.exe "-passes=fla,sub,bcf,gvenc,mba,indcall,indbr" -v`

Example-
`.\compiler\jocky\driver\jocky.exe C:\Users\Ameya\Documents\GitHub\JOCKY-TSAR\build\evil.cpp -o test_output.exe "-passes=fla,sub,bcf,gvenc,mba,indcall,indbr" -v`


# JOCKY Compiler — Layer 1 & 2

Custom LLVM/Polaris-based compiler pipeline that defeats AV/EDR static detection.
Built for **SIH 2026 NTRO PS 26148**.

---

## What It Does

- **Layer 1 — Hash/Signature Defeat**: Every build produces a unique binary. Same source → different SHA-256 every time. 70/71 AV engines defeated.
- **Layer 2 — Compiler Fingerprint Defeat**: Output binary appears as legitimate MSVC-compiled software. Rich Header grafted from Windows system binary, timestamp faked, debug directory zeroed.
- **Polymorphism**: Source-level variable renaming + dead code insertion before compilation. Random flattening seed ensures structural uniqueness every build.

---

## Directory Structure
compiler/
├── build/
│ └── Release/
│ └── bin/
│ ├── clang.exe ← Prebuilt Polaris-patched LLVM 16 clang
│ └── clang++.exe
│
├── jocky/
│ ├── driver/
│ │ ├── jocky.cpp ← Pipeline driver source
│ │ └── jocky.exe ← Prebuilt pipeline driver
│ ├── polymorphic/
│ │ └── poly_engine.py ← Source-level polymorphic transformer
│ ├── runtime/
│ │ ├── runtime.cpp ← Custom runtime (dynamic IAT resolution)
│ │ └── runtime.obj ← Precompiled runtime object
│ └── tools/
│ ├── pe_header_spoofer.py ← PE metadata spoofer
│ └── extract_rich_header.py ← Extract Rich Header from legitimate binary
│
├── config/
│ └── msvc_rich_template.bin ← MSVC Rich Header template (from calc.exe)
│
├── src/
│ └── Flattening.cpp ← Modified Polaris CFG flattening source
│
└── README.md


---

## Requirements

- Windows x64
- Python 3
- System clang (for linking) — `winget install LLVM.LLVM`
- Windows SDK 10.0.26100.0
- Visual Studio 2022 BuildTools

---

## Usage

```powershell
# Basic — fla + sub passes, full pipeline
.\jocky\driver\jocky.exe payload.c -o payload.exe "-passes=fla,sub"

# All passes
.\jocky\driver\jocky.exe payload.c -o payload.exe "-passes=fla,sub,mba,indcall"

# No CRT — custom entry point
.\jocky\driver\jocky.exe payload.c -o payload.exe "-passes=fla,sub" -nostdlib -entry=jocky_entry

# Skip spoofing (faster testing)
.\jocky\driver\jocky.exe payload.c -o payload.exe "-passes=fla,sub" -no-spoof

# Verbose — see all commands
.\jocky\driver\jocky.exe payload.c -o payload.exe "-passes=fla,sub" -v
```

> **Note:** On PowerShell always quote the passes argument: `"-passes=fla,sub"`

---

## Available Passes

| Pass | What It Does |
|------|-------------|
| `fla` | CFG flattening — destroys control flow structure |
| `sub` | Instruction substitution — replaces arithmetic |
| `bcf` | Bogus control flow — fake branches (avoid — triggers detections) |
| `gvenc` | String encryption (avoid — XOR stub fingerprinted) |
| `mba` | Mixed boolean arithmetic |
| `indcall` | Indirect call obfuscation |
| `indbr` | Indirect branch obfuscation |

**Recommended:** `"-passes=fla,sub,mba,indcall"`

---

## Pipeline Stages

Input source file
↓
Stage 3 poly_engine.py Variable rename + dead code insertion
↓
Stage 2 Polaris clang fla + sub + other passes on IR
↓
Stage 1B lld linker Link with CRT or runtime.obj
↓
Stage 6 pe_header_spoofer.py Rich Header graft + timestamp fake
↓
Output .exe


---

## Build jocky.exe (if needed)

```powershell
clang++ jocky\driver\jocky.cpp -o jocky\driver\jocky.exe `
  -target x86_64-pc-windows-msvc `
  -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH `
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64\ucrt.lib" `
  "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64\kernel32.lib" `
  "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.51.36231\lib\x64\msvcrt.lib" `
  "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.51.36231\lib\x64\vcruntime.lib"
```

## Rebuild Flattening (if Flattening.cpp changed)

```powershell
# Step 1 — Rebuild obfuscation library
msbuild "build\lib\Transforms\Obfuscation\LLVMObfuscation.vcxproj" /p:Configuration=Release /maxcpucount:4

# Step 2 — Relink clang.exe
msbuild "build\tools\clang\tools\driver\clang.vcxproj" /p:Configuration=Release /maxcpucount:16
```

---

## VirusTotal Results

| Binary | Passes | Detection |
|--------|--------|-----------|
| Unobfuscated | none | 0/71 |
| fla only | fla | 1/71 |
| Full pipeline | fla,sub | 1/71 |
| Full pipeline + spoof | fla,sub + PE spoof | 1/71 |
| Complex source | fla,sub + all stages | 1/71 (Microsoft only) |

Microsoft detection (`Trojan:Win64/LummaC.AA!MTB`) is a graph-based ML false positive on the Polaris CFG topology. Defeated on live machines by Layer 4 (BYOVD).