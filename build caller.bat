assembler callee.asm callee.obj
assembler caller.asm caller.obj

linker.exe caller.exe 2 "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\kernel32.lib" "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x86\user32.lib" caller.obj callee.obj

caller.exe
echo %errorlevel%
