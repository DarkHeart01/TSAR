# BYOVD — Kernel-Mode Driver Component

The BYOVD module is a Windows kernel driver and accompanying user-mode client that provides kernel-level telemetry collection and system manipulation capabilities for the JOCKY framework. It operates by loading a signed (or vulnerable) driver into the kernel and communicating with it via IOCTL from user space.

---

## Components

| File | Description |
|---|---|
| `driver/driver.c` | Kernel-mode driver (`ntddk.h`-based) — device creation, IOCTL dispatch, kernel operations |
| `client/client.cpp` | User-mode client — opens a handle to the driver device and invokes IOCTLs |
| `shared/ioctlcommon.h` | Shared IOCTL codes and data structures used by both driver and client |

---

## Driver Operations

The driver exposes a device at `\\Device\\JockyDriver` (symlink: `\\DosDevices\\JockyDriver`) and handles the following IOCTL codes:

### `IOCTL_JOCKY_PING`

Connectivity check. Returns `STATUS_SUCCESS` if the driver is loaded and the handle is valid.

---

### `IOCTL_JOCKY_ENUM_PROCS`

Enumerates all running processes by calling `ZwQuerySystemInformation(SystemProcessInformation)` from kernel space. Returns a `PROCESS_LIST` structure containing up to `MAX_PROCESSES` entries, each with a PID and image name.

This bypasses user-mode enumeration APIs that are subject to hooking or DKOM manipulation by other drivers.

---

### `IOCTL_JOCKY_STEAL_TOKEN`

Performs a token theft privilege escalation. Copies the `TOKEN` pointer from the SYSTEM process (`PID 4`) `EPROCESS` structure into the target process's `EPROCESS.Token` field. The reference count bits (bottom 4) are masked off before the write.

Input: `JOCKY_PID_INPUT` containing the target PID.

After a successful call the target process holds a SYSTEM-level access token.

---

### `IOCTL_JOCKY_HIDE_PROCESS`

Removes a process from the kernel's `PsActiveProcessLinks` doubly-linked list by unlinking its `EPROCESS.ActiveProcessLinks` entry and pointing it back to itself. The process continues running but is invisible to any user-mode or kernel-mode enumeration that walks the list.

Input: `JOCKY_PID_INPUT` containing the target PID.

---

### `IOCTL_JOCKY_REMOVE_CALLBACKS` *(currently disabled)*

Locates `PspCreateProcessNotifyRoutine` in `ntoskrnl.exe` via a byte-pattern scan and zeroes all registered callback slots, blinding EDR/AV drivers that rely on process-creation notifications.

This IOCTL is **disabled in the current client** — the 3-byte scan pattern (`0x4C 0x8D 0x2D`) produces false positives across the kernel `.text` section, and zeroing the wrong slot causes an immediate bugcheck. It will be re-enabled once the callback location method is replaced with a safer RVA-based approach.

---

## IOCTL Interface

Defined in `shared/ioctlcommon.h`:

```c
#define JOCKY_DEVICE_TYPE  0x8000

#define IOCTL_JOCKY_PING              CTL_CODE(JOCKY_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JOCKY_REMOVE_CALLBACKS  CTL_CODE(JOCKY_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JOCKY_HIDE_PROCESS      CTL_CODE(JOCKY_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JOCKY_STEAL_TOKEN       CTL_CODE(JOCKY_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JOCKY_ENUM_PROCS        CTL_CODE(JOCKY_DEVICE_TYPE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _JOCKY_PID_INPUT {
    ULONG TargetPid;
} JOCKY_PID_INPUT;
```

---

## Kernel Offsets

The driver uses hardcoded `EPROCESS` offsets for Windows 10 21H2 (build 19044):

| Field | Offset |
|---|---|
| `EPROCESS.Token` | `0x4B8` |
| `EPROCESS.UniqueProcessId` | `0x440` |
| `EPROCESS.ActiveProcessLinks` | `0x448` |

These must be updated for other Windows versions.

---

## User-Mode Client

The client (`client/client.cpp`) opens a handle to `\\\\.\JockyDriver` via `CreateFileW` and sequentially invokes:

1. `IOCTL_JOCKY_PING` — verify connectivity
2. `IOCTL_JOCKY_ENUM_PROCS` — list all running processes
3. `IOCTL_JOCKY_STEAL_TOKEN` — escalate the current process to SYSTEM
4. A process-hollowing pipeline (`RunHollowPipeline()` from `processhollowing/hollow.h`) — spawns a hollowed `dllhost.exe`
5. `IOCTL_JOCKY_HIDE_PROCESS` — hide the hollowed process from enumeration

---

## Building

### Driver

Open `driver/driver.vcxproj` in Visual Studio 2022 with the WDK installed. Build for `x64 Release`. The driver must be signed (or test-signed with `bcdedit /set testsigning on`) to load.

### Client

Open `client/client.vcxproj` in Visual Studio 2022. Build for `x64 Release`. Run as Administrator with the driver service already loaded:

```cmd
sc create JockyDriver type= kernel binPath= C:\path\to\driver.sys
sc start JockyDriver
client.exe
```

---

## Directory Layout

```
byovd/
├── shared/
│   └── ioctlcommon.h     IOCTL codes and shared data structures
├── driver/
│   ├── driver.c          Kernel-mode driver source
│   └── driver.vcxproj    MSVC WDK project
└── client/
    ├── client.cpp         User-mode IOCTL client
    └── client.vcxproj     MSVC project
```
