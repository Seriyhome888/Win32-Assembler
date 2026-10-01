section .text
global _main

_main:
    ; --- STANDARD WIN32 PROLOGUE ---
    push ebp
    mov ebp, esp
    and esp, 0xF0

    mov eax, 10
    sub eax, 10         ; EAX = 10 - 10 = 0
    
    cmp eax, 0          ; Test if zero flag matches condition
    je _matched_zero    ; If zero, branch forward immediately!

    mov eax, 7          ; Failure path fallback (returns 7)
    jmp _exit_block

_matched_zero:
    mov eax, 99         ; Success path (overwrites EAX with 99)

_exit_block:
    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp
    pop ebp
    ret
