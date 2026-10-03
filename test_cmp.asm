section .text
    global _main

_main:
    mov eax, 5
    mov ecx, 5

    cmp eax, ecx        ; Compare registers! Sets the Zero Flag (ZF=1) if equal.
    je .matched         ; If equal, perform short jump to .matched label block

    ; --- Else Path (No Match) ---
    mov eax, 99         ; Exit code fallback value
    jmp .exit

.matched:
    ; --- If Path (Matched) ---
    mov eax, 42         ; Our target success exit code value

.exit:
    ret
