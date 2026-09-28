# Process Hollowing — In-Memory Execution Module

This module implements process hollowing with direct syscall execution, providing fileless in-memory payload delivery within a trusted host process. It is consumed by the BYOVD client (`byovd/client/client.cpp`) and can be linked into any component that needs stealthy in-memory execution.

All critical NT operations use direct syscalls rather than the Windows API, bypassing EDR hooks installed in `ntdll.dll`.

---

## How It Works

`RunHollowPipeline()` performs a complete process hollowing operation in eleven steps:

```
1.  Read payload PE from disk
2.  Spawn dllhost.exe in a suspended state
3.  Read the thread context via NtGetContextThread          [direct syscall]
4.  Locate the PEB and dllhost's image base via NtQueryInformationProcess
5.  Unmap dllhost's image via NtUnmapViewOfSection          [direct syscall]
6.  Allocate memory at the payload's preferred base via NtAllocateVirtualMemory [direct syscall]
7.  Write PE headers into the allocated region via NtWriteVirtualMemory [direct syscall]
8.  Write each PE section via NtWriteVirtualMemory          [direct syscall]
9.  Apply base relocations (IMAGE_REL_BASED_DIR64)
10. Update the PEB ImageBase pointer
11. Redirect RCX to the payload entry point via NtSetContextThread [direct syscall]
    Resume execution via NtResumeThread                     [direct syscall]
```

The result is the payload running inside `dllhost.exe` with the process's identity intact. The PID is returned to the caller — the BYOVD client then uses it to hide the process from the kernel process list via `IOCTL_JOCKY_HIDE_PROCESS`.

---

## SSN Resolution

Before issuing any direct syscall, the module resolves the syscall service number (SSN) at runtime using a three-stage fallback chain:

| Stage | Method | When used |
|---|---|---|
| **Hell's Gate** | Reads the SSN directly from the `mov eax, <ssn>` stub in the loaded `ntdll.dll` | ntdll is clean (no hook at the stub) |
| **Halo's Gate** | Scans neighbouring syscall stubs ±10 entries to reconstruct the SSN from an adjacent unhooked stub | ntdll stub is overwritten with a JMP |
| **Fresh Copy** | Maps a clean copy of `ntdll.dll` from disk and reads the SSN from it | Both in-memory methods fail |

This ensures SSN resolution succeeds regardless of whether an EDR has patched `ntdll.dll` in-process.

---

## Direct Syscall Stub

The low-level `DirectSyscall` function is implemented in MASM:

```asm
DirectSyscall PROC
    mov r10, rdx
    mov eax, ecx      ; SSN passed as first argument
    mov rcx, r8
    mov rdx, r9
    syscall
    ret
DirectSyscall ENDP
```

Enable MASM in Visual Studio: **Build Customizations → masm**

---

## API

```cpp
// hollow.h
DWORD RunHollowPipeline();
```

Returns the PID of the hollowed `dllhost.exe` process on success, `0` on failure.

The payload is read from `C:\Users\Public\payload.exe`. This path is the agreed handoff point between the compiler pipeline (which deposits the built artifact) and the hollowing module.

---

## Syscall Wrappers

The following NT functions are wrapped with direct syscall dispatch:

| Wrapper | NT function | Purpose |
|---|---|---|
| `Syscall_NtGetContextThread` | `NtGetContextThread` | Read suspended thread CPU context |
| `Syscall_NtUnmapViewOfSection` | `NtUnmapViewOfSection` | Unmap host process image |
| `Syscall_NtAllocateVirtualMemory` | `NtAllocateVirtualMemory` | Allocate RWX region in target process |
| `Syscall_NtWriteVirtualMemory` | `NtWriteVirtualMemory` | Write payload headers and sections |
| `Syscall_NtSetContextThread` | `NtSetContextThread` | Redirect entry point |
| `Syscall_NtResumeThread` | `NtResumeThread` | Resume execution |

---

## Integration

Include `hollow.h` and compile `hollow.cpp` alongside the caller. The BYOVD client uses it as follows:

```cpp
#include "../../processhollowing/hollow.h"

DWORD hollowedPid = RunHollowPipeline();
if (hollowedPid > 4) {
    // Use IOCTL_JOCKY_HIDE_PROCESS to remove from kernel process list
}
```

---

## Directory Layout

```
processhollowing/
├── hollow.h        Public API — RunHollowPipeline() declaration
├── hollow.cpp      Full implementation: SSN resolution, syscall wrappers, hollowing pipeline
└── payload.cpp     Standalone test payload (used for hollowing validation)
```
