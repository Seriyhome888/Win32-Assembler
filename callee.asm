section .text
    global _AddTwoNumbers

_AddTwoNumbers:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]  ; Load 15
    add eax, [ebp + 12] ; Add 27

    pop ebp
    ret                 
                        
