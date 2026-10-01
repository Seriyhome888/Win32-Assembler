section .text
global _main

_main:
    push ebp            ; Save operating system parent frame pointer
    mov ebp, esp        ; Establish our local base pointer frame anchor
    and esp, 0xF0       ; Safely align ESP downward to a standard valid 16-byte boundary

    mov eax, 10         ; Set EAX to 10
    mov ecx, 5          ; Set ECX to 5
    add eax, ecx        ; EAX = 10 + 5 = 15
    sub eax, 3          ; EAX = 15 - 3 = 12

    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp        ; Restore the original stack pointer (clears the alignment padding)
    pop ebp             ; Restore the parent caller's frame base pointer
    ret                 ; Return control safely to CMD. EAX (12) is read as the exit code.
