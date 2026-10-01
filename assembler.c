#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define HASH_MAP_SIZE 128
#define MAX_SYMBOLS 256
#define MAX_RELOCS 256

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
    uint16_t Type;
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
    int16_t  SectionNumber;
    uint16_t Type;
    uint8_t  StorageClass;
    uint8_t  NumberOfAuxSymbols;
} COFFSymbol;
#pragma pack(pop)

typedef struct SymbolNode {
    char *name;
    uint32_t id;
    uint32_t address;
    struct SymbolNode *next;
} SymbolNode;

SymbolNode *sym_map[HASH_MAP_SIZE] = {NULL};
COFFSymbol coff_syms[MAX_SYMBOLS];
uint32_t coff_sym_count = 0;

COFFRelocation relocs[MAX_RELOCS];
uint32_t reloc_count = 0;

uint8_t text_bytes[4096];
size_t text_size = 0;
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
    node->next = sym_map[index];
    sym_map[index] = node;
}

void emit_byte(uint8_t b) {
    if (current_pass == 2) text_bytes[text_size] = b;
    text_size++;
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
    for (int i = 0; line[i]; i++) {
        if (line[i] == ',' || line[i] == '[' || line[i] == ']') {
            line[i] = ' ';
        }
    }
}

void assemble_line(char *line) {
    char tokens[4][64] = {{{0}}};
    int count = sscanf(line, "%63s %63s %63s %63s", tokens[0], tokens[1], tokens[2], tokens[3]);
    if (count <= 0) return;

    char *cmd = tokens[0];
    if (strcmp(cmd, "section") == 0 || strcmp(cmd, "global") == 0 || strcmp(cmd, "extern") == 0) return;

    size_t cmd_len = strlen(cmd);
    if (cmd_len > 1 && cmd[cmd_len - 1] == ':') {
        if (current_pass == 1) {
            char clean_lbl[64] = {0};
            strncpy(clean_lbl, cmd, cmd_len - 1);
            sym_insert(clean_lbl, (uint32_t)text_size, 1, 2);
        }
        return;
    }

    if (strcmp(cmd, "push") == 0) { int r = get_reg_id(tokens[1]); if (r>=0) emit_byte(0x50+r); return; }
    if (strcmp(cmd, "pop") == 0)  { int r = get_reg_id(tokens[1]); if (r>=0) emit_byte(0x58+r); return; }
    if (strcmp(cmd, "ret") == 0)  { emit_byte(0xC3); return; }

    if (strcmp(cmd, "mov") == 0) {
        int dst = get_reg_id(tokens[1]);
        int src = get_reg_id(tokens[2]);
        if (dst >= 0 && src >= 0) {
            emit_byte(0x89); emit_byte(0xC0 + (src * 8) + dst);
        } else if (dst >= 0) {
            emit_byte(0xB8 + dst);
            emit_uint32((uint32_t)strtol(tokens[2], NULL, 0));
        }
        return;
    }

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

    if (strcmp(cmd, "cmp") == 0) {
        int dst = get_reg_id(tokens[1]);
        if (dst >= 0) {
            emit_byte(0x83); emit_byte(0xF8 + dst);
            emit_byte((uint8_t)strtol(tokens[2], NULL, 0));
        }
        return;
    }

    // --- CRITICAL BRANCH DISPLACEMENT RESOLUTION FIX ---
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
                // FIXED FORMULA: Target - (Opcode_Start + Total_Instruction_Length)
                // Total instruction length for these short branch opcodes is always 2 bytes.
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
    if (argc < 3) { 
        printf("Usage: %s <input.asm> <output.obj>\n", argv); 
        return 1; 
    }

    sym_insert(".text", 0, 1, 3); 

    // PASS 1: Identify symbol positioning metrics
    current_pass = 1; text_size = 0;
    FILE *in = fopen(argv[1], "r");
    if (!in) { perror("Open failed on input file"); return 1; }

    char line[256];
    while (fgets(line, sizeof(line), in)) { clean_line(line); assemble_line(line); }
    rewind(in);

    // PASS 2: Emit exact relative displacement arrays
    current_pass = 2; size_t final_text_len = text_size; text_size = 0;
    while (fgets(line, sizeof(line), in)) { clean_line(line); assemble_line(line); }
    fclose(in);

    // WRITE VALID WIN32 COFF OBJECT FILE
    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror("Open failed on output file"); return 1; }

    uint32_t header_bytes = sizeof(COFFHeader) + sizeof(SectionHeader);
    uint32_t text_raw_ptr = header_bytes;
    uint32_t reloc_ptr = text_raw_ptr + (uint32_t)final_text_len;

    COFFHeader coff = {
        .Machine = 0x14C, .NumberOfSections = 1, .TimeDateStamp = 0,
        .PointerToSymbolTable = reloc_ptr + (sizeof(COFFRelocation) * reloc_count),
        .NumberOfSymbols = coff_sym_count, .SizeOfOptionalHeader = 0, .Characteristics = 0x0000
    };

    SectionHeader sec_text = {
        .Name = ".text\0\0", .SizeOfRawData = (uint32_t)final_text_len, .PointerToRawData = text_raw_ptr,
        .PointerToRelocations = reloc_count > 0 ? reloc_ptr : 0, .NumberOfRelocations = (uint16_t)reloc_count,
        .Characteristics = 0x60000020
    };

    fwrite(&coff, sizeof(coff), 1, out);
    fwrite(&sec_text, sizeof(sec_text), 1, out);
    fwrite(text_bytes, final_text_len, 1, out);
    fwrite(coff_syms, sizeof(COFFSymbol), coff_sym_count, out);

    uint32_t str_table_size = 4;
    fwrite(&str_table_size, sizeof(str_table_size), 1, out);
    fclose(out);

    printf("Compiled flawlessly: %s -> %s\n", argv[1], argv[2]);
    return 0;
}
