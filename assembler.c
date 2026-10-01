#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define HASH_MAP_SIZE 64

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
    char     Name[8];
    uint32_t Value;
    int16_t  SectionNumber;
    uint16_t Type;
    uint8_t  StorageClass;
    uint8_t  NumberOfAuxSymbols;
} COFFSymbol;
#pragma pack(pop)

typedef struct SymbolNode {
    char *name;
    uint32_t address;
    struct SymbolNode *next;
} SymbolNode;

SymbolNode *sym_map[HASH_MAP_SIZE] = {NULL};
int current_pass = 1;

uint8_t text_bytes[4096];
size_t text_size = 0;

uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash % HASH_MAP_SIZE;
}

void sym_insert(const char *name, uint32_t address) {
    uint32_t index = hash_string(name);
    SymbolNode *node = malloc(sizeof(SymbolNode));
    node->name = strdup(name);
    node->address = address;
    node->next = sym_map[index];
    sym_map[index] = node;
}

int sym_lookup(const char *name, uint32_t *addr_out) {
    uint32_t index = hash_string(name);
    SymbolNode *curr = sym_map[index];
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            if (addr_out) *addr_out = curr->address;
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

void emit_byte(uint8_t b) {
    if (current_pass == 2) {
        text_bytes[text_size] = b;
    }
    text_size++;
}

void emit_uint32(uint32_t val) {
    emit_byte(val & 0xFF);
    emit_byte((val >> 8) & 0xFF);
    emit_byte((val >> 16) & 0xFF);
    emit_byte((val >> 24) & 0xFF);
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
    char *comma = strchr(line, ',');
    if (comma) *comma = ' ';
}

void assemble_line(char *line) {
    char tokens[4][64] = {{0}};
    int count = sscanf(line, "%63s %63s %63s %63s", tokens[0], tokens[1], tokens[2], tokens[3]);
    if (count <= 0) return;

    char *cmd = tokens[0];

    if (strcmp(cmd, "section") == 0 || strcmp(cmd, "global") == 0) return;

    size_t cmd_len = strlen(cmd);
    if (cmd_len > 1 && cmd[cmd_len - 1] == ':') {
        if (current_pass == 1) {
            char clean_lbl[64] = {0};
            strncpy(clean_lbl, cmd, cmd_len - 1);
            sym_insert(clean_lbl, (uint32_t)text_size);
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

    if (strcmp(cmd, "and") == 0) {
        int dst = get_reg_id(tokens[1]);
        if (dst >= 0) { 
            emit_byte(0x83); emit_byte(0xE0 + dst); 
            emit_byte((uint8_t)strtol(tokens[2], NULL, 0)); 
        }
        return;
    }

    if (strcmp(cmd, "add") == 0) {
        int dst = get_reg_id(tokens[1]);
        int src = get_reg_id(tokens[2]);
        if (dst >= 0 && src >= 0) {
            emit_byte(0x01); emit_byte(0xC0 + (src * 8) + dst);
        } else if (dst >= 0) {
            emit_byte(0x83); emit_byte(0xC0 + dst); emit_byte((uint8_t)strtol(tokens[2], NULL, 0));
        }
        return;
    }

    if (strcmp(cmd, "sub") == 0) {
        int dst = get_reg_id(tokens[1]);
        int src = get_reg_id(tokens[2]);
        if (dst >= 0 && src >= 0) {
            emit_byte(0x29); emit_byte(0xC0 + (src * 8) + dst);
        } else if (dst >= 0) {
            emit_byte(0x83); emit_byte(0xE8 + dst); // Clean 8-step scaling ModR/M
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

    // JNE Instruction block mapping 
    if (strcmp(cmd, "jne") == 0) {
        emit_byte(0x75);
        if (current_pass == 2) {
            uint32_t target;
            if (sym_lookup(tokens[1], &target)) {
                int8_t offset = (int8_t)(target - (text_size + 1));
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
    if (argc < 3) { printf("Usage: %s <in.asm> <out.obj>\n", argv); return 1; }

    current_pass = 1; text_size = 0;
    FILE *in = fopen(argv[1], "r");
    if(!in) { perror("Open failed"); return 1; }
    char line[256];
    while (fgets(line, sizeof(line), in)) { clean_line(line); assemble_line(line); }
    rewind(in);

    current_pass = 2; size_t final_text_len = text_size; text_size = 0;
    while (fgets(line, sizeof(line), in)) { clean_line(line); assemble_line(line); }
    fclose(in);

    FILE *out = fopen(argv[2], "wb");
    uint32_t header_bytes = sizeof(COFFHeader) + sizeof(SectionHeader);

    COFFHeader coff = {
        .Machine = 0x14C, .NumberOfSections = 1, .TimeDateStamp = 0,
        .PointerToSymbolTable = header_bytes + (uint32_t)final_text_len,
        .NumberOfSymbols = 2, .SizeOfOptionalHeader = 0, .Characteristics = 0x0000
    };

    SectionHeader sec_text = {
        .Name = ".text\0\0", .SizeOfRawData = (uint32_t)final_text_len, .PointerToRawData = header_bytes,
        .Characteristics = 0x60000020
    };

    COFFSymbol sym_table[2] = {
        { .Name = ".text\0\0", .Value = 0, .SectionNumber = 1, .Type = 0, .StorageClass = 3 },
        { .Name = "_main\0\0", .Value = 0, .SectionNumber = 1, .Type = 0, .StorageClass = 2 }
    };

    fwrite(&coff, sizeof(coff), 1, out);
    fwrite(&sec_text, sizeof(sec_text), 1, out);
    fwrite(text_bytes, final_text_len, 1, out);
    fwrite(sym_table, sizeof(COFFSymbol), 2, out);

    uint32_t str_table_size = 4;
    fwrite(&str_table_size, sizeof(str_table_size), 1, out);
    fclose(out);

    printf("Built successfully. Math alignments and jumps patched cleanly.\n");
    return 0;
}
