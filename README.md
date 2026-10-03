# Win32 x86 COFF Assembler (Custom Toolchain Compiler)

A custom, low-level, specification-compliant **32-bit Intel x86 Assembler** written in C from scratch. This compiler parses standard Intel-syntax assembly files (.asm) and formats raw machine bytecode into valid **Microsoft PE/COFF (Common Object File Format) Object binaries (.obj)**. The resulting files interface seamlessly with standard Windows development tools, including the Microsoft Linker (link.exe) and Disassembler (dumpbin.exe).

# Internal Engineering Mechanics

The assembler leverages a structural **Two-Pass Engine** backed by localized state vectors to accurately decouple label offsets from absolute code emission loops.

**1. Dual-Pass Compiler Engine**
  * **Pass 1 (Sizing, Scope Tracking, & Symbol Collection):** \
Scans the source file line-by-line. It strips comments and processes structural directives (section, global, extern). It inserts discovered identifiers into a specialized **Hash-Map Symbol Table**. At this phase, instructions are parsed strictly to measure their compiled byte lengths, which updates the internal location tracking counters (text_size and data_size). This ensures every local label is mapped to its precise byte offset before compilation begins.

  * **Pass 2 (Bytecode Serialization & Fix-up Logging):** \
Resets section layout counters to zero and performs a comprehensive secondary scan. It encodes assembly text strings into little-endian machine bytecode, populating text_bytes and data_bytes buffers. Simultaneously, it generates structural relocation metadata when it encounters symbols that depend on runtime link calculations.

**2. Multi-File Linker Interoperability & Relocation**
Cross-file function invocations (e.g., call _AddTwoNumbers) cannot be evaluated at assemble time because the absolute destination address is unknown.

* To resolve this, Pass 2 emits a 4-byte little-endian placeholder payload (00 00 00 00) inside the bytecode stream.

* It then creates and appends an active entry to the **COFFRelocation** table with the type **IMAGE_REL_I386_REL32 (0x0014)**. This tells the Windows Linker (link.exe) to compute the final jump offset at build time using the formula:
  ```
  Target Address - Relocation Position - 4
  ```

**3. COFF String Table Management**
The core COFF symbol layout imposes a strict maximum boundary of 8 characters for inline identifier names (ShortName).
* If an identifier name is 8 characters or fewer, it is written inline.
* For decorated identifiers longer than 8 characters (e.g., _AddTwoNumbers, _ExitProcess@4), the assembler forces compliance by assigning a special configuration: LongName.Zeroes = 0 and LongName.Offset = StringTableOffset. The full string label is safely offloaded to a tail-end **COFF String Table**, preventing name truncation and linking failures.

**4. Absolute Constant Separation**
To support dynamic sizing calculations using the location counter (e.g., MyStringLen: equ $ - MyString), the assembler implements the absolute section layout flag **IMAGE_SYM_ABSOLUTE (-1)**. When an absolute symbol is evaluated, it is cached in the symbol map with a section marker of -1. This instructs the instruction encoders to emit its raw immediate integer directly into fields (like mov eax, MyStringLen) without generating an unnecessary relocation record.

# COFF File Layout Architecture
The assembler writes data to disk sequentially using standard aligned binary configurations:
```
┌────────────────────────────────────────────────────────┐
│ 1. COFF Header (IMAGE_FILE_HEADER - 20 Bytes)          │
├────────────────────────────────────────────────────────┤
│ 2. Section Headers (IMAGE_SECTION_HEADER Array)        │
│    - .text  (Characteristics: 0x60000020 - RX Code)    │
│    - .data  (Characteristics: 0xC0000040 - RW Data)    │
├────────────────────────────────────────────────────────┤
│ 3. Raw Section Data Binary Blobs                       │
│    - .text raw compiled bytecode bytes                 │
│    - .data raw initialized data bytes                  │
├────────────────────────────────────────────────────────┤
│ 4. COFF Relocation Records (IMAGE_RELOCATION Array)    │
├────────────────────────────────────────────────────────┤
│ 5. COFF Symbol Table (IMAGE_SYMBOL Array)              │
├────────────────────────────────────────────────────────┤
│ 6. COFF String Table (Size prefix + null-term strings) │
└────────────────────────────────────────────────────────┘
```

# Supported Instruction Set Architecture (ISA)
The text parser handles comma, space, tab, and plus-separated tokens, supporting a wide range of x86 instructions:

# Data Movement & Memory Referencing
* **mov reg, reg** → Flat register reassignment (e.g., mov ebp, esp → 89 E5).
* **mov reg, imm32** → Direct value or constant loading (e.g., mov eax, 42).
* **mov reg, [ebp + disp8]** → Stack-frame parameter displacement dereferencing (e.g., mov eax, [ebp + 8] → 8B 45 08).
* **mov reg, symbol** → Loads a relocatable data section address pointer (emits IMAGE_REL_I386_DIR32).

# Mathematics Engine (Signed & Unsigned)
* **add / sub** → Dual-operand math operations for registers, immediates, and stack offsets (e.g., add esp, 8).
* **mul reg / div reg** → Unsigned single-operand implicit math execution targets utilizing the edx:eax register pair.
* **imul reg, reg** → Two-operand signed multiplication matrix (0x0F 0xAF).
* **imul reg / idiv reg** → Single-operand signed math extensions.
* **cdq** → Sign-extends eax into edx prior to signed division operations (0x99).

# Logic, Shifts, and Bitwise Unary
* **and / or / xor** → Bitwise manipulation operators (e.g., xor edx, edx to clear a register).
* **shl / shr** → Immediate-driven bitwise shifts.
* **inc / dec** → Single register value step modification.
* **not / neg** → Two's complement inversion and sign negation.

# Control Flow, Status Flags, & Branches  
* **cmp reg, reg / cmp reg, imm8** → Sets status flags based on subtraction without altering operands.
* **jmp** → Short absolute 8-bit jump instructions (0xEB).
* **je / jne / jnz / jl / jg** → Status flag short conditional jumps for loops and branches.
* **call** → Relative 32-bit execution jumps. Automatically matches internal labels or external OS APIs.
* **push / pop / ret** → Stack maintenance operations and subroutine returns.

# Memory & Storage Directives
* **db (Define Byte):** Supports quoted strings (db "Hello", 0) and comma-separated lists (db 1, 2, 3).
* **dw (Define Word):** Serializes 16-bit integer blocks.
* **dd (Define Doubleword):** Serializes 32-bit integer blocks or global pointer tracking addresses.
* **equ $ - label:** Compile-time counter subtraction expression that calculates sizes dynamically.

# Execution & Verification Guide
## Compilation & Build Loop
To compile the assembler using the native MSVC compiler tools:
```
cl.exe /W3 assembler.c
```
## Linking Standalone Target Modules
To assemble your source files and build an executable against native Windows OS import libraries (like kernel32.lib or user32.lib), execute these commands inside a **Developer Command Prompt for Visual Studio:**

```
:: 1. Run your assembler to generate standard COFF object components
assembler.exe test_program.asm test_program.obj

:: 2. Invoke link.exe to cross-bridge dependencies into a PE executable
link.exe /subsystem:console /entry:main test_program.obj kernel32.lib /nodefaultlib /largeaddressaware:no

:: 3. Run the binary application
test_program.exe

:: 4. Verify outputs via the process error level status return variable
echo %errorlevel%
```

## Inspecting Internal Byte Compliance
You can verify that your assembler's generated bytes match the official x86 specifications by running the Visual Studio disassembler utility:
```
dumpbin /disasm test_program.obj
```


