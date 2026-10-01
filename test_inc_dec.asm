section .text
global _main

_main:
    ; --- STANDARD WIN32 PROLOGUE ---
    push ebp
    mov ebp, esp
    and esp, 0xF0

    mov eax, 102        ; Start validation tracking at 102
    dec eax             ; EAX = 101 (Using optimized 1-byte decrement primitive)
    dec eax             ; EAX = 100 

    mov ecx, 5          ; Initialize ecx counter to 5
_loop_start:
    dec eax             ; Decrement tracking number using shorthand primitive
    dec ecx             ; Decrement loop controller
    jnz _loop_start     ; Loop runs until ECX hits 0 (EAX becomes 100 - 5 = 95)

    inc eax             ; EAX = 96  (Using optimized 1-byte increment primitives)
    inc eax             ; EAX = 97
    inc eax             ; EAX = 98
    inc eax             ; EAX = 99  (Target absolute success exit status token!)

    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp
    pop ebp
    ret
