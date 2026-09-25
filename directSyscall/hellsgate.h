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