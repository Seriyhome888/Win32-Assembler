section .text
global _main

_main:
    ; --- STANDARD WIN32 PROLOGUE ---
    push ebp
    mov ebp, esp
    and esp, 0xF0

    mov eax, 50
    cmp eax, 20
    jg _is_greater     ; 50 > 20, should branch forward!

    mov eax, 1         ; Failure Trap 1
    jmp _exit

_is_greater:
    mov ecx, 10
    cmp ecx, 15
    jl _is_less        ; 10 < 15, should branch forward!

    mov eax, 2         ; Failure Trap 2
    jmp _exit

_is_less:
    mov eax, 99        ; Success Destination Token
    jmp _exit          ; Jump over structural failure traps

    mov eax, 3         ; Failure Trap 3

_exit:
    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp
    pop ebp
    ret
