section .text
global _main

_main:
    ; --- STANDARD WIN32 PROLOGUE ---
    push ebp
    mov ebp, esp
    and esp, 0xF0

    mov eax, 40          ; Start with base value 40
    
    call _perform_math   ; Jump to subroutine, pushing next instruction onto stack

    ; --- POST-CALL VERIFICATION ---
    add eax, 19          ; EAX = 80 + 19 = 99 (Our final targeted success status flag!)

    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp
    pop ebp
    ret                  ; Return cleanly back to CMD shell execution context

; --- INTERNAL SUBROUTINE BLOCK ---
_perform_math:
    shl eax, 1           ; EAX = 40 * 2 = 80
    ret                  ; Pop return address off stack and branch back to main path
