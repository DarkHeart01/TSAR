#include <windows.h>
#include <stdio.h>
#include "IoctlCommon.h"

int main() {
    printf("[*] Attempting to open handle to driver at %ws...\n", USER_MODE_PATH);

    // Open a handle to the driver using its symbolic link
    HANDLE hDevice = CreateFileW(
        USER_MODE_PATH,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open handle to driver. Error: %lu\n", GetLastError());
        printf("    (Ensure the driver service is running and launched as Administrator)\n");
        return 1;
    }

    printf("[+] Handle successfully opened! Sending IOCTL ping...\n");

    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_JOCKY_PING,
        NULL, 0,    // No input buffer needed for ping
        NULL, 0,    // No output buffer needed for ping
        &bytesReturned,
        NULL
    );

    if (result) {
        printf("[+] Success: IOCTL communication verified by kernel driver!\n");
    } else {
        printf("[-] DeviceIoControl failed. Error: %lu\n", GetLastError());
    }

    CloseHandle(hDevice);
    return 0;
}