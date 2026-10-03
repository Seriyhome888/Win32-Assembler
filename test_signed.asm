section .text
    global _main

_main:
    mov eax, -50        
    mov ecx, 2          
    imul eax, ecx       ; eax should be -100

    cdq                 ; Automatically fills EDX perfectly based on EAX's sign bit!
    
    mov ecx, 5          
    idiv ecx            ; eax = edx:eax / 5

    ret
