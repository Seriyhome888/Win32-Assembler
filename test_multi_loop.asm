section .text
global _main

_main:
    ; --- STANDARD WIN32 PROLOGUE ---
    push ebp            ; Save operating system parent frame pointer
    mov ebp, esp        ; Establish our local base pointer frame anchor
    and esp, 0xF0       ; Safely align ESP downward to a standard 16-byte boundary

    ; --- SETUP REGISTER VALUES ---
    mov eax, 110        ; Initialize EAX to 110
    mov ecx, 0          ; Initialize ECX to 0

    ; --- FIRST DYNAMIC LOOP ---
_first_loop:
    sub eax, 1          ; Decrement EAX by 1
    cmp eax, 100        ; Check if EAX has reached 100
    jne _first_loop     ; If not 100, jump backward to _first_loop

    ; --- INTERMEDIATE MATH ---
    mov ecx, 4          ; Set ECX to 4
    add eax, ecx        ; EAX = 100 + 4 = 104

    ; --- SECOND DYNAMIC LOOP ---
_second_loop:
    sub eax, 1          ; Decrement EAX by 1
    sub ecx, 1          ; Decrement loop controller ECX by 1
    cmp ecx, 0          ; Check if ECX loop controller hit 0
    jne _second_loop    ; If not 0, jump backward to _second_loop

    ; --- Final Math Result Check: 
    ; EAX started the second loop at 104. 
    ; It decremented exactly 4 times (since ECX was 4).
    ; EAX final value = 100.
    
    sub eax, 1          ; EAX = 100 - 1 = 99 (Our intended global success token)

    ; --- STACK-SAFE BARE EXIT ---
    mov esp, ebp        ; Restore the original stack pointer
    pop ebp             ; Restore the parent caller's frame base pointer
    ret                 ; Return control safely to OS. EAX (99) is the exit code.
