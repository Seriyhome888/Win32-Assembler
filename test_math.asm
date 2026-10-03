section .text
    global _main

_main:
    ; --- 1. Multiplication Setup ---
    mov eax, 15         ; Load 15 into accumulator (eax = 15)
    mov ecx, 4          ; Load 4 into multiplier register (ecx = 4)
    mul ecx             ; edx:eax = eax * ecx (15 * 4 = 60. eax = 60, edx = 0)

    ; --- 2. Division Setup ---
    mov ecx, 3          ; Load 3 into divisor register (ecx = 3)
    div ecx             ; eax = edx:eax / ecx (60 / 3 = 20)
                        ; The quotient (20) goes to eax. Remainder (0) goes to edx.

    ; --- 3. Exit Program ---
    ; Since our final result (20) is inside eax, returning from _main 
    ; treats the value in eax as the process exit status code!
    ret
