#include <ntddk.h>
#include "IoctlCommon.h"

void DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING symLink;
    RtlInitUnicodeString(&symLink, SYMBOLIC_LINK_NAME);
    
    // Clean up symbolic link and device object on unload
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
    } else {
        DbgPrint("[JOCKY Driver] Unknown IOCTL code received: 0x%X\n", ioctlCode);
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    UNICODE_STRING devName, symLink;

    RtlInitUnicodeString(&devName, DEVICE_NAME_SYS);
    RtlInitUnicodeString(&symLink, SYMBOLIC_LINK_NAME);

    // 1. Create the Device Object
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

    // 2. Create Symbolic Link for User-Mode Access
    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[JOCKY Driver] Failed to create symbolic link (0x%X)\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    // 3. Register Dispatch Routines
    DriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCloseRoutine;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateCloseRoutine;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoControlRoutine;
    DriverObject->DriverUnload = DriverUnload;

    DbgPrint("[JOCKY Driver] Loaded successfully and device registered.\n");
    return STATUS_SUCCESS;
}