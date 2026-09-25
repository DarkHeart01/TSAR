#include <Windows.h>
#include <stdio.h>
#include "hellsgate.h"

const char* k = "[+]";
const char* e = "[-]";
const char* i = "[*]";

extern "C" NTSTATUS DirectSyscall(DWORD ssn, ...);

DWORD ResolveSSN(LPCSTR functionName) {
    DWORD ssn = GetSSN(functionName);           // Hell's Gate
    if (ssn != -1) return ssn;

    ssn = GetSSN_HalosGate(functionName);       // Halo's Gate
    if (ssn != -1) return ssn;

    return GetSSN_FreshCopy(functionName);      // Fresh copy from disk
}

int main() {
    DWORD ssn = ResolveSSN("NtAllocateVirtualMemory");
    if (ssn == -1) {
        printf("%s Failed to resolve SSN\n", e);
        return 1;
    }
    printf("%s NtAllocateVirtualMemory SSN: 0x%X\n", k, ssn);

    HANDLE hProcess = GetCurrentProcess();
    PVOID baseAddress = NULL;
    SIZE_T regionSize = 0x1000; // or 4KB

    NTSTATUS status = DirectSyscall(
        ssn,
        hProcess,
        &baseAddress,
        0,
        &regionSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (status == 0) {
        printf("%s Allocated memory at: %p\n", k, baseAddress);
    } else {
        printf("[-] Failed: 0x%X\n", status);
    }

    return 0;
}