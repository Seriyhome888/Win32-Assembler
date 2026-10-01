section .text
global _main

_main:
    ; --- STANDARD WIN32 PROLOGUE ---
    push ebp
    mov ebp, esp
    and esp, 0xF0

    ; --- SHIFT TESTING PIPELINE ---
    mov eax, 24          ; Start with 24
    shl eax, 2           ; EAX = 24 * 4 = 96 (Shift Left 2 bits)
    
    mov ecx, 12          ; Load 12 into ECX
    shr ecx, 2           ; ECX = 12 / 4 = 3  (Shift Right 2 bits)

    add eax, ecx         ; EAX = 96 + 3 = 99 (Target Success Status Code!)

    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp
    pop ebp
    ret
