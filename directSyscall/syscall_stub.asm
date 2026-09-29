.code

; NTSTATUS DirectSyscall(DWORD ssn, <syscall args...>)
;
; x64 calling convention on entry:
;   rcx = ssn
;   rdx = A1, r8 = A2, r9 = A3
;   [rsp+28h] = A4, [rsp+30h] = A5, [rsp+38h] = A6
;
; x64 syscall ABI the kernel expects:
;   eax = ssn
;   r10 = A1, rdx = A2, r8 = A3, r9 = A4
;   [rsp+28h] = A5, [rsp+30h] = A6
;
DirectSyscall PROC
    mov r10, rdx            ; A1 -> r10
    mov eax, ecx            ; SSN -> eax
    mov rdx, r8             ; A2 -> rdx
    mov r8, r9              ; A3 -> r8
    mov r9, [rsp+28h]       ; A4 -> r9  (5th DirectSyscall param)
    ; Shift stack args so kernel reads A5/A6 from the right slots
    mov r11, [rsp+30h]      ; A5
    mov [rsp+28h], r11
    mov r11, [rsp+38h]      ; A6
    mov [rsp+30h], r11
    syscall
    ret
DirectSyscall ENDP

end
