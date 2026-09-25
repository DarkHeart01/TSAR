.code

; NTSTATUS DirectSyscall
; First arg (RCX) = SSN
; Remaining args are the actual syscall arguments shifted right
DirectSyscall PROC
    mov r10, rdx        ; arg1 of actual syscall (was RDX, becomes R10)
    mov eax, ecx        ; SSN was in RCX (first param), load into EAX
    mov rcx, r8         ; arg2 -> RCX
    mov rdx, r9         ; arg3 -> RDX
    ; arg4 onward are already on stack, kernel reads them from there
    syscall
    ret
DirectSyscall ENDP

end

;lets hope this works again 5th time or fuck AI