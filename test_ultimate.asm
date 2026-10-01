section .data
    greeting: db "Hello, Multi-Section World!", 0

section .text
global _main

_main:
    ; --- STANDARD WIN32 PROLOGUE ---
    push ebp
    mov ebp, esp
    and esp, 0xF0

    ; --- RELOCATION COFF RESOLUTION TRACKING ---
    mov eax, greeting    ; EAX is loaded with the absolute pointer to our initialized string

    ; --- MULTI-LOOP AND MATH CHECKS ---
    mov ecx, 120
_loop_one:
    sub ecx, 2
    cmp ecx, 100
    jne _loop_one        ; Backward branch tracks down loop until ECX is 100

    ; --- BITWISE LOGICAL FILTER CHECKS ---
    xor edx, edx         ; EDX = 0 (Register-to-register variant verification)
    or edx, 5            ; EDX = 0 | 5 = 5 (Immediate filtering validation)
    
    sub ecx, edx         ; ECX = 100 - 5 = 95
    add ecx, 4           ; ECX = 95 + 4 = 99

    mov eax, ecx         ; EAX = 99 (Prepares success exit status token)

    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp
    pop ebp
    ret