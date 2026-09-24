#pragma once

#ifndef _KERNEL_MODE
#include <windows.h>
#include <winioctl.h>
#else
#include <ntddk.h>
#endif

// Define device type and custom IOCTL code (Function codes 0x800-0xFFF are reserved for custom drivers)
#define JOCKY_DEVICE_TYPE 0x8000
#define IOCTL_JOCKY_PING CTL_CODE(JOCKY_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

// System device path and user-mode symbolic link
#define DEVICE_NAME_SYS      L"\\Device\\JockyTestDevice"
#define SYMBOLIC_LINK_NAME   L"\\DosDevices\\JockyTestDevice"
#define USER_MODE_PATH       L"\\\\.\\JockyTestDevice"