#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define HASH_MAP_SIZE 128
#define MAX_SYMBOLS 256
#define MAX_RELOCS 256

// --- COFF STRUCTS (Win32 Specification) ---
#pragma pack(push, 1)
typedef struct {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} COFFHeader;

typedef struct {
    char     Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} SectionHeader;

typedef struct {
    uint32_t VirtualAddress;
    uint32_t SymbolTableIndex;
    uint16_t Type; // 0x0006 = IMAGE_REL_I386_DIR32 (32-bit absolute address)
} COFFRelocation;

typedef struct {
    union {
        char     ShortName[8];
        struct {
            uint32_t Zeroes;
            uint32_t Offset;
        } LongName;
    } Name;
    uint32_t Value;
    int16_t  SectionNumber; // 1 = .text, 2 = .data
    uint16_t Type;
    uint8_t  StorageClass;  // 2 = External, 3 = Static
    uint8_t  NumberOfAuxSymbols;
} COFFSymbol;
#pragma pack(pop)

// --- MEMORY LAYOUT STATE ---
typedef struct SymbolNode {
    char *name;
    uint32_t id;
    uint32_t address;
    int16_t section_num;
    struct SymbolNode *next;
} SymbolNode;

SymbolNode *sym_map[HASH_MAP_SIZE] = {NULL};
COFFSymbol coff_syms[MAX_SYMBOLS];
uint32_t coff_sym_count = 0;

COFFRelocation text_relocs[MAX_RELOCS];
uint32_t text_reloc_count = 0;

uint8_t text_bytes[4096];
size_t text_size = 0;

uint8_t data_bytes[4096];
size_t data_size = 0;

int current_section = 1; // 1 = .text, 2 = .data
int current_pass = 1;

uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) hash = ((hash << 5) + hash) + c;
    return hash % HASH_MAP_SIZE;
}

int sym_lookup(const char *name, SymbolNode **node_out) {
    uint32_t index = hash_string(name);
    SymbolNode *curr = sym_map[index];
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            if (node_out) *node_out = curr;
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

void sym_insert(const char *name, uint32_t address, int16_t sec, uint8_t storage_class) {
    SymbolNode *existing;
    if (sym_lookup(name, &existing)) {
        if (current_pass == 1) existing->address = address;
        return;
    }

    uint32_t id = coff_sym_count++;
    memset(&coff_syms[id], 0, sizeof(COFFSymbol));
    strncpy(coff_syms[id].Name.ShortName, name, 8);
    coff_syms[id].Value = address;
    coff_syms[id].SectionNumber = sec;
    coff_syms[id].StorageClass = storage_class;

    uint32_t index = hash_string(name);
    SymbolNode *node = malloc(sizeof(SymbolNode));
    node->name = strdup(name);
    node->id = id;
    node->address = address;
    node->section_num = sec;
    node->next = sym_map[index];
    sym_map[index] = node;
}

void emit_byte(uint8_t b) {
    if (current_section == 1) {
        if (current_pass == 2) text_bytes[text_size] = b;
        text_size++;
    } else {
        if (current_pass == 2) data_bytes[data_size] = b;
        data_size++;
    }
}

void emit_uint32(uint32_t val) {
    emit_byte(val & 0xFF); emit_byte((val >> 8) & 0xFF);
    emit_byte((val >> 16) & 0xFF); emit_byte((val >> 24) & 0xFF);
}

int get_reg_id(const char *reg_name) {
    if (strcmp(reg_name, "eax") == 0) return 0;
    if (strcmp(reg_name, "ecx") == 0) return 1;
    if (strcmp(reg_name, "edx") == 0) return 2;
    if (strcmp(reg_name, "ebx") == 0) return 3;
    if (strcmp(reg_name, "esp") == 0) return 4;
    if (strcmp(reg_name, "ebp") == 0) return 5;
    return -1;
}

void clean_line(char *line) {
    char *comment = strchr(line, ';');
    if (comment) *comment = '\0';
    // If it's a raw string directive, don't strip spaces/commas inside quotes
    if (strstr(line, "db") && strchr(line, '"')) return;
    
    for (int i = 0; line[i]; i++) {
        if (line[i] == ',' || line[i] == '[' || line[i] == ']') line[i] = ' ';
    }
}

void assemble_line(char *line) {
    char tokens[4][64] = {{{0}}};
    char peek[64] = {0};
    if (sscanf(line, "%63s", peek) <= 0) return;

    // Handle structural section switching
    if (strcmp(peek, "section") == 0) {
        char sec_name[64] = {0};
        sscanf(line, "section %63s", sec_name);
        if (strcmp(sec_name, ".text") == 0) current_section = 1;
        if (strcmp(sec_name, ".data") == 0) current_section = 2;
        return;
    }
    if (strcmp(peek, "global") == 0 || strcmp(peek, "extern") == 0) return;

    // --- DATA SECTION PARSING (db Directive) ---
    if (current_section == 2) {
        char *lbl = strchr(line, ':');
        if (lbl) {
            *lbl = '\0';
            char lbl_name[64];
            sscanf(line, "%63s", lbl_name);
            if (current_pass == 1) {
                sym_insert(lbl_name, (uint32_t)data_size, 2, 3); // Section 2, Static Class
            }
            char *payload = lbl + 1;
            char op[32] = {0};
            sscanf(payload, "%31s", op);
            if (strcmp(op, "db") == 0) {
                char *str_start = strchr(payload, '"');
                if (str_start) {
                    char *str_end = strchr(str_start + 1, '"');
                    if (str_end) {
                        for (char *c = str_start + 1; c < str_end; c++) emit_byte((uint8_t)*c);
                    }
                }
                if (strstr(payload, ", 0") || strstr(payload, ",0")) emit_byte(0x00);
            }
        }
        return;
    }

    // --- TEXT SECTION PARSING ---
    size_t peek_len = strlen(peek);
    if (peek_len > 1 && peek[peek_len - 1] == ':') {
        if (current_pass == 1) {
            char clean_lbl[64] = {0};
            strncpy(clean_lbl, peek, peek_len - 1);
            sym_insert(clean_lbl, (uint32_t)text_size, 1, 2);
        }
        return;
    }

    int count = sscanf(line, "%63s %63s %63s %63s", tokens[0], tokens[1], tokens[2], tokens[3]);
    if (count <= 0) return;

    char *cmd = tokens[0];

    if (strcmp(cmd, "push") == 0) { int r = get_reg_id(tokens[1]); if (r>=0) emit_byte(0x50+r); return; }
    if (strcmp(cmd, "pop") == 0)  { int r = get_reg_id(tokens[1]); if (r>=0) emit_byte(0x58+r); return; }
    if (strcmp(cmd, "ret") == 0)  { emit_byte(0xC3); return; }

    if (strcmp(cmd, "mov") == 0) {
        int dst = get_reg_id(tokens[1]);
        int src = get_reg_id(tokens[2]);
        if (dst >= 0 && src >= 0) {
            emit_byte(0x89); emit_byte(0xC0 + (src * 8) + dst);
        } else if (dst >= 0) {
            SymbolNode *node;
            // Detect if loading a pointer to a global variable defined in the .data section
            if (sym_lookup(tokens[2], &node) && node->section_num == 2) {
                emit_byte(0xB8 + dst); // mov reg, imm32
                if (current_pass == 2) {
                    text_relocs[text_reloc_count].VirtualAddress = (uint32_t)text_size;
                    text_relocs[text_reloc_count].SymbolTableIndex = node->id;
                    text_relocs[text_reloc_count].Type = 0x0006; // IMAGE_REL_I386_DIR32
                    text_reloc_count++;
                }
                emit_uint32(node->address);
            } else {
                emit_byte(0xB8 + dst);
                emit_uint32((uint32_t)strtol(tokens[2], NULL, 0));
            }
        }
        return;
    }

    // Mathematical Instructions System (add/sub)
    if (strcmp(cmd, "sub") == 0 || strcmp(cmd, "add") == 0) {
        int dst = get_reg_id(tokens[1]);
        int src = get_reg_id(tokens[2]);
        if (dst >= 0 && src >= 0) {
            uint8_t op = (strcmp(cmd, "sub") == 0) ? 0x29 : 0x01;
            emit_byte(op); emit_byte(0xC0 + (src * 8) + dst);
        } else if (dst >= 0) {
            uint8_t op_extension = (strcmp(cmd, "sub") == 0) ? 0xE8 : 0xC0;
            emit_byte(0x83); emit_byte(op_extension + dst);
            emit_byte((uint8_t)strtol(tokens[2], NULL, 0));
        }
        return;
    }

    // --- LOGICAL FILTERS EXTRACTION IMPLEMENTATION (xor / or) ---
    if (strcmp(cmd, "xor") == 0 || strcmp(cmd, "or") == 0) {
        int dst = get_reg_id(tokens[1]);
        int src = get_reg_id(tokens[2]);
        int is_xor = (strcmp(cmd, "xor") == 0);
        
        if (dst >= 0 && src >= 0) {
            // xor reg1, reg2 (Opcode 0x31) | or reg1, reg2 (Opcode 0x09)
            emit_byte(is_xor ? 0x31 : 0x09);
            emit_byte(0xC0 + (src * 8) + dst);
        } else if (dst >= 0) {
            // xor reg, imm8 (Opcode 0x83 /6 -> 0xF0) | or reg, imm8 (Opcode 0x83 /1 -> 0xC8)
            emit_byte(0x83);
            emit_byte((is_xor ? 0xF0 : 0xC8) + dst);
            emit_byte((uint8_t)strtol(tokens[2], NULL, 0));
        }
        return;
    }

    if (strcmp(cmd, "cmp") == 0) {
        int dst = get_reg_id(tokens[1]);
        if (dst >= 0) {
            emit_byte(0x83); emit_byte(0xF8 + dst);
            emit_byte((uint8_t)strtol(tokens[2], NULL, 0));
        }
        return;
    }

    // --- TRACKING RESOLUTION JUMPS ---
    uint8_t opcode = 0;
    if (strcmp(cmd, "jmp") == 0) opcode = 0xEB;
    else if (strcmp(cmd, "je") == 0)  opcode = 0x74;
    else if (strcmp(cmd, "jne") == 0) opcode = 0x75;
    else if (strcmp(cmd, "jl") == 0)  opcode = 0x7C;
    else if (strcmp(cmd, "jg") == 0)  opcode = 0x7F;

    if (opcode != 0) {
        uint32_t instr_start_addr = (uint32_t)text_size;
        emit_byte(opcode);
        if (current_pass == 2) {
            SymbolNode *node;
            if (sym_lookup(tokens[1], &node)) {
                int8_t offset = (int8_t)((int32_t)node->address - ((int32_t)instr_start_addr + 2));
                emit_byte((uint8_t)offset);
} else {
emit_byte(0x00);
}
} else {
emit_byte(0x00);
}
return;
}
}
int main(int argc, char **argv) {
if (argc < 3) { printf("Usage: %s <in.asm> <out.obj>\n", argv[0]); return 1; }
sym_insert(".text", 0, 1, 3);
sym_insert(".data", 0, 2, 3);
// PASS 1
current_pass = 1; text_size = 0; data_size = 0; current_section = 1;
FILE *in = fopen(argv[1], "r");
if (!in) { perror("Input load error"); return 1; }
char line[256];
while (fgets(line, sizeof(line), in)) { clean_line(line); assemble_line(line); }
rewind(in);
// PASS 2
current_pass = 2; size_t final_text_len = text_size; size_t final_data_len = data_size;
text_size = 0; data_size = 0; current_section = 1;
while (fgets(line, sizeof(line), in)) { clean_line(line); assemble_line(line); }
fclose(in);
// WRITE COFF
FILE *out = fopen(argv[2], "wb");
if (!out) { perror("Output initialization error"); return 1; }
uint32_t header_bytes = sizeof(COFFHeader) + (sizeof(SectionHeader) * 2);
uint32_t text_raw_ptr = header_bytes;
uint32_t data_raw_ptr = text_raw_ptr + (uint32_t)final_text_len;
uint32_t text_reloc_ptr = data_raw_ptr + (uint32_t)final_data_len;
COFFHeader coff = {
.Machine = 0x14C, .NumberOfSections = 2, .TimeDateStamp = 0,
.PointerToSymbolTable = text_reloc_ptr + (sizeof(COFFRelocation) * text_reloc_count),
.NumberOfSymbols = coff_sym_count, .SizeOfOptionalHeader = 0, .Characteristics = 0x0000
};
SectionHeader sec_text = {
.Name = ".text\0\0\0", .SizeOfRawData = (uint32_t)final_text_len, .PointerToRawData = text_raw_ptr,
.PointerToRelocations = text_reloc_count > 0 ? text_reloc_ptr : 0, .NumberOfRelocations = (uint16_t)text_reloc_count,
.Characteristics = 0x60000020 // CODE | EXECUTE | READ
};
SectionHeader sec_data = {
.Name = ".data\0\0\0", .SizeOfRawData = (uint32_t)final_data_len, .PointerToRawData = data_raw_ptr,
.Characteristics = 0xC0000040 // INITIALIZED_DATA | READ | WRITE
};
fwrite(&coff, sizeof(coff), 1, out);
fwrite(&sec_text, sizeof(sec_text), 1, out);
fwrite(&sec_data, sizeof(sec_data), 1, out);
fwrite(text_bytes, final_text_len, 1, out);
fwrite(data_bytes, final_data_len, 1, out);
if (text_reloc_count > 0) fwrite(text_relocs, sizeof(COFFRelocation) * text_reloc_count, 1, out);
fwrite(coff_syms, sizeof(COFFSymbol), coff_sym_count, out);
uint32_t str_table_size = 4;
fwrite(&str_table_size, sizeof(str_table_size), 1, out);
fclose(out);
printf("Ultimate Assembler Success: Built sections and logical filters completely!\n");
return 0;
}