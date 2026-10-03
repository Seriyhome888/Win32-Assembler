del *.obj
del *.exe
del *.dll
del *.lib
del *.exp
del *.o

cl assembler.c
cl linker.c

assembler.exe math_test.asm math_test.obj
assembler.exe loop_test.asm loop_test.obj
assembler.exe test_multi_loop.asm test_multi_loop.obj
assembler.exe test_string.asm test_string.obj
assembler.exe test_je.asm test_je.obj
assembler.exe test_branches.asm test_branches.obj

assembler.exe test_stack_extern.asm stack_test.obj

assembler.exe test_ultimate.asm test_ultimate.obj
assembler.exe test_bss.asm test_bss.obj
assembler.exe test_shifts.asm test_shifts.obj
assembler.exe test_inc_dec.asm test_inc_dec.obj
assembler.exe test_call.asm test_call.obj
assembler.exe test_neg.asm test_neg.obj
assembler.exe test_math.asm test_math.obj
assembler.exe test_signed.asm test_signed.obj
assembler.exe test_cmp.asm test_cmp.obj

linker.exe math_app.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" math_test.obj
linker.exe loop_app.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" loop_test.obj
linker.exe test_multi_loop.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_multi_loop.obj
linker.exe test_string.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_string.obj
linker.exe test_je.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_je.obj
linker.exe test_branches.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_branches.obj
linker.exe test_ultimate.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_ultimate.obj
linker.exe test_bss.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_bss.obj
linker.exe test_shifts.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_shifts.obj
linker.exe test_inc_dec.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_inc_dec.obj
linker.exe test_call.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_call.obj
linker.exe test_neg.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_neg.obj
linker.exe test_math.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_math.obj
linker.exe test_signed.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_signed.obj
linker.exe test_cmp.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_cmp.obj

math_app.exe
echo %errorlevel%

loop_app.exe
echo %errorlevel%

test_multi_loop.exe
echo %errorlevel%

test_string.exe
echo %errorlevel%

test_je.exe
echo %errorlevel%

test_branches.exe
echo %errorlevel%

test_ultimate.exe
echo %errorlevel%

test_bss.exe
echo %errorlevel%

test_shifts.exe
echo %errorlevel%

test_inc_dec.exe
echo %errorlevel%

test_call.exe
echo %errorlevel%

test_neg.exe
echo %errorlevel%

test_math.exe
echo %errorlevel%

test_signed.exe
echo %errorlevel%

test_cmp.exe
echo %errorlevel%