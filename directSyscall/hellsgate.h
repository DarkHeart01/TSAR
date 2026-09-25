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