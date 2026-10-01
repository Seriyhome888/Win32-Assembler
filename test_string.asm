section .data
    hello_msg: db "Hello World", 0

section .text
global _main

_main:
    ; --- STANDARD WIN32 PROLOGUE ---
    push ebp
    mov ebp, esp
    and esp, 0xF0

    ; --- FETCH STRING ADDRESS ---
    mov eax, hello_msg  ; EAX now holds the memory pointer (0x402000)

    ; --- LOOP VERIFICATION CHECKS ---
    mov ecx, 104        ; We use ECX as our counter loop token
_loop:
    sub ecx, 1
    cmp ecx, 99
    jne _loop           ; Loop ticks down until ECX equals 99

    ; --- FIX: PRESERVE MATH RESULT FOR EXIT STATUS ---
    mov eax, ecx        ; Move the 99 from ECX into EAX so it becomes the exit code!

    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp
    pop ebp
    ret                 ; Returns control. EAX (99) is now read properly.
