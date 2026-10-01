section .text
global _main

_main:
    ; --- STANDARD WIN32 PROLOGUE ---
    push ebp            ; Save operating system parent frame pointer
    mov ebp, esp        ; Establish our local base pointer frame anchor
    and esp, 0xF0       ; Safely align ESP downward to a standard valid 16-byte boundary

    ; --- FLAWLESS CODE LOOP ---
    mov eax, 104        ; Initialize our decrementing value to 104

_loop_start:
    sub eax, 1          ; Decrement EAX register loop count
    cmp eax, 99         ; Compare EAX counter value against our success token 99
    jne _loop_start     ; If not 99, jump back and loop again

    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp        ; Restore the original stack pointer (clears the alignment padding)
    pop ebp             ; Restore the parent caller's frame base pointer
    ret                 ; Return control safely to CMD. EAX (99) is now your exit code!
