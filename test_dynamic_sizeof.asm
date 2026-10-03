section .data
    MyString: db "Designers of Software Toolchains!", 0
    MyStringLen: equ $ - MyString

section .text
    global _main

_main:
    push ebp
    mov ebp, esp

    ; This now looks up the symbol 'MyStringLen' and emits its value dynamically!
    mov eax, MyStringLen 

    pop ebp
    ret
