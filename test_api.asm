section .text
    extern _ExitProcess@4    ; Declare the external Windows API dependency
    global _main

_main:
    ; --- ExitProcess Application Terminate Flow ---
    ; ExitProcess takes 1 parameter: the exit status code.
    ; Under __stdcall, the callee cleans the stack, so no 'add esp, 4' is needed!
    push 123                ; Push our chosen target exit status code
    call _ExitProcess@4     ; Call the Windows Kernel routine

    ; Execution will never reach here because the OS terminates the process above.
    ret
