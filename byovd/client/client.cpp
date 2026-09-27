#include <windows.h>
#include <stdio.h>
#include "IoctlCommon.h"

const char* k = "[+]";
const char* e = "[-]";
const char* i = "[*]";

int main() {
    printf("%s Attempting to open handle to driver at %ws...\n", i, USER_MODE_PATH);

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
        printf("%s Failed to open handle to driver. Error: %lu\n", e, GetLastError());
        printf("    (Ensure the driver service is running and launched as Administrator)\n");
        return 1;
    }

    printf("%s Handle successfully opened!\n", k);

    // --- IOCTL 1: Ping ---
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_JOCKY_PING,
        NULL, 0,
        NULL, 0,
        &bytesReturned,
        NULL
    );

    if (result) {
        printf("%s IOCTL_JOCKY_PING: Success\n", k);
    } else {
        printf("%s IOCTL_JOCKY_PING failed. Error: %lu\n", e, GetLastError());
    }

    // --- IOCTL 2: Enumerate Processes ---
    PROCESS_LIST procList = {0};
    bytesReturned = 0;

    result = DeviceIoControl(
        hDevice,
        IOCTL_JOCKY_ENUM_PROCS,
        NULL, 0,
        &procList, sizeof(procList),
        &bytesReturned,
        NULL
    );

    if (result) {
        printf("%s IOCTL_JOCKY_ENUM_PROCS: Got %lu processes from kernel:\n", k, procList.Count);
        for (ULONG i = 0; i < procList.Count; i++) {
            printf("    PID: %-6lu  Name: %s\n", procList.Entries[i].Pid, procList.Entries[i].ImageName);
        }
    } else {
        printf("%s IOCTL_JOCKY_ENUM_PROCS failed. Error: %lu\n", e, GetLastError());
    }

    CloseHandle(hDevice);
    return 0;
}