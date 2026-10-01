dumpbin /disasm /relocations stack_test.obj

rem see mov dword ptr [ebp-4], eax mapping to 89 45 FC in your output log, along with a valid relocation linking to the external function.