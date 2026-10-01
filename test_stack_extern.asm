section .text
extern _DummyExternCall
global _main

_main:
    ; --- STANDARD WIN32 PROLOGUE ---
    push ebp
    mov ebp, esp
    sub esp, 4          ; Allocate 4 bytes of local stack storage space

    ; --- STORE AND LOAD FROM STACK DISPLACEMENT OFFSETS ---
    mov eax, 99         ; Load 99 into intermediate execution register
    mov [ebp-4], eax    ; Store it inside our newly provisioned local stack variable

    mov eax, 0          ; Clear EAX completely
    mov eax, [ebp-4]    ; Load value back out from stack offset memory space (EAX becomes 99)

    ; --- SIMULATE LINKER DEPENDENCY RESOLUTION ---
    call _DummyExternCall

    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp        ; Automatically tears down our 4-byte stack allocation payload padding safely
    pop ebp
    ret