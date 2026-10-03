section .data
    MyString: db "Designers of Software Toolchains!", 0
    
    ; Dynamically grab the string length at compile time!
    ; "Designers of Software Toolchains!" contains exactly 33 characters.
    ; Including the trailing null terminator byte, the total size is 34 bytes.
    MyStringLen: equ $ - MyString

section .text
    global _main

_main:
    ; Standard prologue
    push ebp
    mov ebp, esp

    ; Move the evaluated constant size into EAX directly
    ; We simulate reading the absolute constant value 34
    mov eax, 34

    ; Standard epilogue
    pop ebp
    ret
