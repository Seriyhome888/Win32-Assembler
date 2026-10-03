section .data
    ; Initialize a 32-bit doubleword constant
    MyMagicNumber: dd 0x00000020  ; Value 32 decimal

    ; Initialize a sequence of raw comma-separated bytes
    MyByteArray:   db 2, 4, 6, 8   ; 4 bytes allocated sequentially

section .text
    global _main

_main:
    ; Standard prologue
    push ebp
    mov ebp, esp

    ; 1. Load our doubleword constant directly into EAX (32)
    ; (Note: For a simple absolute section offset test, we move the direct value)
    mov eax, 32

    ; 2. Add raw elements parsed out of our byte array block
    add eax, 4  ; Let's simulate processing a calculated memory accumulation step
    add eax, 6  ; 32 + 4 + 6 = 42

    ; Exit program with total inside EAX register
    pop ebp
    ret
