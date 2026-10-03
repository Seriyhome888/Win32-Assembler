section .text
    extern _AddTwoNumbers
    extern _ExitProcess@4    
    global _main

_main:
    ; Push arguments (CDECL convention)
    push 27             
    push 15             
    call _AddTwoNumbers 
    add esp, 8          ; <-- Clean up the 8 bytes we pushed to the stack!

    ret
