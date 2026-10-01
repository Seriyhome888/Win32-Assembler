section .data
    initial_seed: db "d", 0   ; ASCII character value for 'd' is 104

section .bss
    my_global_buffer: resb 4  ; Reserves 4 bytes dynamically in RAM uninitialized data space

section .text
global _main

_main:
    ; --- STANDARD WIN32 PROLOGUE ---
    push ebp
    mov ebp, esp
    and esp, 0xF0

    ; --- PULL FROM DATA -> WRITE TO BSS ---
    mov ebx, initial_seed
    mov al, [ebx]             ; Load the byte value 104 into AL register segment

    mov ebx, my_global_buffer ; Resolve the BSS absolute memory storage map location address pointer
    mov [ebx], eax            ; Write the payload value 104 safely into RAM space

    ; --- CLEAR VALS & READ BACK FROM BSS ---
    mov eax, 0
    mov eax, [ebx]            ; Retrieve our uninitialized segment runtime state variable (EAX = 104)

    ; --- VERIFICATION LOOP TICK ---
_loop:
    sub eax, 1
    cmp eax, 99
    jne _loop                 ; Loop continues until status code conditions balance cleanly at 99

    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp
    pop ebp
    ret
