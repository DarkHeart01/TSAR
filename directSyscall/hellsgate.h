#pragma once
#include <Windows.h>

DWORD GetSSN(LPCSTR functionName) {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return -1;

    BYTE* pFunc = (BYTE*)GetProcAddress(hNtdll, functionName);
    if (!pFunc) return -1;

    if (pFunc[0] == 0xE9) {
        return -1;
    }
    if (pFunc[3] == 0xB8) {
        return *(DWORD*)(pFunc + 4);
    }

    return -1;
}

DWORD GetSSN_HalosGate(LPCSTR functionName) {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    BYTE* pFunc = (BYTE*)GetProcAddress(hNtdll, functionName);
    if (!pFunc) return -1;

    if(pFunc[3] == 0xB8) {
        return *(DWORD*)(pFunc + 4);
    }
    //Basically stub is hooked and we gotta check the neighbours (each address 4 byte diff bruv)


    for (int i = 1; i < 10; i++) {
        BYTE* neighbour = pFunc + (i * 32); // each stub is 32 bytes apart    
        if (neighbour[3] == 0xB8) {
            DWORD neighborSSN = *(DWORD*)(neighbour + 4);
            return neighborSSN - i;
        }
        neighbour = pFunc - (i * 32);
        if (neighbour[3] == 0xB8) {
            DWORD neighbourSSN = *(DWORD*)(neighbour + 4);
            return neighbourSSN + i; // our SSN is higher by i
        }
    }

    return -1;
}

DWORD GetSSN_FreshCopy(LPCSTR functionName) {

    HANDLE hFile = CreateFileA(
        "C:\\Windows\\System32\\ntdll.dll",
        GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, 0, NULL
    );

    HANDLE hMapping = CreateFileMappingA(
        hFile, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL
    );
    LPVOID pMapping = MapViewOfFile(
        hMapping, FILE_MAP_READ, 0, 0, 0
    );

    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pMapping;
    PIMAGE_NT_HEADERS pNt  = (PIMAGE_NT_HEADERS)(
        (BYTE*)pMapping + pDos->e_lfanew
    );
    PIMAGE_EXPORT_DIRECTORY pExport = (PIMAGE_EXPORT_DIRECTORY)(
        (BYTE*)pMapping +
        pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
            .VirtualAddress
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