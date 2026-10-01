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

linker.exe math_app.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" math_test.obj
linker.exe loop_app.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" loop_test.obj
linker.exe test_multi_loop.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_multi_loop.obj
linker.exe test_string.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_string.obj
linker.exe test_je.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_je.obj
linker.exe test_branches.exe 1 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" test_branches.obj

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