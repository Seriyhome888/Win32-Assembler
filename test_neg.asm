section .text
global _main

_main:
    ; --- STANDARD WIN32 PROLOGUE ---
    push ebp
    mov ebp, esp
    and esp, 0xF0

    ; --- NEG TESTING ---
    mov eax, 5
    neg eax              ; EAX = -5 (Arithmetic Two's Complement)
    add eax, 105         ; EAX = -5 + 105 = 100

    ; --- NOT TESTING ---
    mov ecx, -1          ; ECX = 0xFFFFFFFF
    not ecx              ; ECX = 0x00000000 (Bitwise One's Complement)
    
    dec eax              ; EAX = 100 - 1 = 99
    add eax, ecx         ; EAX = 99 + 0 = 99 (Target Success Status Flag!)

    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp
    pop ebp
    ret
