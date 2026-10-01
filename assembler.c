#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define HASH_MAP_SIZE 128
#define MAX_SYMBOLS 256
#define MAX_RELOCS 256
#define BUFFER_SIZE 8192

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
    int16_t section_num;
    struct SymbolNode *next;
} SymbolNode;

SymbolNode *sym_map[HASH_MAP_SIZE] = {NULL};
COFFSymbol coff_syms[MAX_SYMBOLS];
uint32_t coff_sym_count = 0;

COFFRelocation text_relocs[MAX_RELOCS];
uint32_t text_reloc_count = 0;

uint8_t text_bytes[BUFFER_SIZE];
size_t text_size = 0;
uint8_t data_bytes[BUFFER_SIZE];
size_t data_size = 0;

int current_section = 1;
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
        if (current_pass == 2 && text_size < BUFFER_SIZE) text_bytes[text_size] = b;
        text_size++;
    } else {
        if (current_pass == 2 && data_size < BUFFER_SIZE) data_bytes[data_size] = b;
        data_size++;
    }
}

void emit_uint32(uint32_t val) {
    emit_byte(val & 0xFF); emit_byte((val >> 8) & 0xFF);
    emit_byte((val >> 16) & 0xFF); emit_byte((val >> 24) & 0xFF);
}

int get_reg_id(const char *reg_name) {
    if (!reg_name) return -1;
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
    if (strstr(line, "db") && strchr(line, '"')) return;
    for (int i = 0; line[i]; i++) {
        if (line[i] == ',' || line[i] == '[' || line[i] == ']') line[i] = ' ';
    }
}

void assemble_line(char *line) {
    char mutable_line[256];
    strncpy(mutable_line, line, sizeof(mutable_line) - 1);
    mutable_line[sizeof(mutable_line) - 1] = '\0';

    // Tokenize cleanly using robust sequential strtok pointers
    char *tokens[4] = {NULL, NULL, NULL, NULL};
    char *token = strtok(mutable_line, " \t\r\n");
    int tok_idx = 0;
    while (token && tok_idx < 4) {
        tokens[tok_idx++] = token;
        token = strtok(NULL, " \t\r\n");
    }

    if (tok_idx == 0) return;
    char *cmd = tokens[0];

    // Handle section shifts
    if (strcmp(cmd, "section") == 0) {
        if (tok_idx > 1) {
            if (strcmp(tokens[1], ".text") == 0) current_section = 1;
            if (strcmp(tokens[1], ".data") == 0) current_section = 2;
        }
        return;
    }
    if (strcmp(cmd, "global") == 0 || strcmp(cmd, "extern") == 0) return;

    // Handle data segment labels
    if (current_section == 2) {
        char *lbl = strchr(line, ':');
        if (lbl) {
            *lbl = '\0';
            char lbl_name[64] = {0};
            if (sscanf(line, "%63s", lbl_name) > 0) {
                if (current_pass == 1) sym_insert(lbl_name, (uint32_t)data_size, 2, 3);
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
        }
        return;
    }

    // Handle text segment labels
    size_t cmd_len = strlen(cmd);
    if (cmd_len > 1 && cmd[cmd_len - 1] == ':') {
        if (current_pass == 1) {
            char clean_lbl[64] = {0};
            strncpy(clean_lbl, cmd, cmd_len - 1 < 63 ? cmd_len - 1 : 63);
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
            emit_byte(0x8B); emit_byte(0xC0 + (dst * 8) + src);
        } else if (dst >= 0 && tokens[2]) {
            SymbolNode *node;
            if (sym_lookup(tokens[2], &node) && node->section_num == 2) {
                emit_byte(0xB8 + dst);
                if (current_pass == 2) {
                    text_relocs[text_reloc_count].VirtualAddress = (uint32_t)text_size;
                    text_relocs[text_reloc_count].SymbolTableIndex = node->id;
                    text_relocs[text_reloc_count].Type = 0x0006;
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

    if (strcmp(cmd, "and") == 0) {
        int dst = get_reg_id(tokens[1]);
        if (dst >= 0 && tokens[2]) {
            emit_byte(0x83); emit_byte(0xE0 + dst);
            emit_byte((uint8_t)strtol(tokens[2], NULL, 0));
        }
        return;
    }

    if (strcmp(cmd, "sub") == 0 || strcmp(cmd, "add") == 0) {
        int dst = get_reg_id(tokens[1]);
        int src = get_reg_id(tokens[2]);
        if (dst >= 0 && src >= 0) {
            uint8_t op = (strcmp(cmd, "sub") == 0) ? 0x29 : 0x01;
            emit_byte(op); emit_byte(0xC0 + (src * 8) + dst);
        } else if (dst >= 0 && tokens[2]) {
            uint8_t op_extension = (strcmp(cmd, "sub") == 0) ? 0xE8 : 0xC0;
            emit_byte(0x83); emit_byte(op_extension + dst);
            emit_byte((uint8_t)strtol(tokens[2], NULL, 0));
        }
        return;
    }

    if (strcmp(cmd, "inc") == 0 || strcmp(cmd, "dec") == 0) {
        int r = get_reg_id(tokens[1]);
        if (r >= 0) {
            uint8_t base_opcode = (strcmp(cmd, "inc") == 0) ? 0x40 : 0x48;
            emit_byte(base_opcode + r);
        }
        return;
    }

    if (strcmp(cmd, "xor") == 0 || strcmp(cmd, "or") == 0) {
        int dst = get_reg_id(tokens[1]);
        int src = get_reg_id(tokens[2]);
        int is_xor = (strcmp(cmd, "xor") == 0);
        if (dst >= 0 && src >= 0) {
            emit_byte(is_xor ? 0x31 : 0x09); emit_byte(0xC0 + (src * 8) + dst);
        } else if (dst >= 0 && tokens[2]) {
            emit_byte(0x83);
            emit_byte((is_xor ? 0xF0 : 0xC8) + dst);
            emit_byte((uint8_t)strtol(tokens[2], NULL, 0));
        }
        return;
    }

    if (strcmp(cmd, "shl") == 0 || strcmp(cmd, "shr") == 0) {
        int dst = get_reg_id(tokens[1]);
        int is_shr = (strcmp(cmd, "shr") == 0);
        if (dst >= 0 && tokens[2]) {
            emit_byte(0xC1);
            emit_byte((is_shr ? 0xE8 : 0xE0) + dst);
            emit_byte((uint8_t)strtol(tokens[2], NULL, 0));
        }
        return;
    }

    if (strcmp(cmd, "cmp") == 0) {
        int dst = get_reg_id(tokens[1]);
        if (dst >= 0 && tokens[2]) {
            emit_byte(0x83); emit_byte(0xF8 + dst);
            emit_byte((uint8_t)strtol(tokens[2], NULL, 0));
        }
        return;
    }

    if (strcmp(cmd, "call") == 0) {
uint32_t instr_start_addr = (uint32_t)text_size;
emit_byte(0xE8);
if (current_pass == 2 && tokens[1]) {
SymbolNode *node;
if (sym_lookup(tokens[1], &node)) {
uint32_t rel32_offset = node->address - (instr_start_addr + 5);
emit_uint32(rel32_offset);
} else { emit_uint32(0); }
} else { emit_uint32(0); }
return;
}
uint8_t opcode = 0;
if (strcmp(cmd, "jmp") == 0) opcode = 0xEB;
else if (strcmp(cmd, "je") == 0)  opcode = 0x74;
else if (strcmp(cmd, "jne") == 0 || strcmp(cmd, "jnz") == 0) opcode = 0x75;
else if (strcmp(cmd, "jl") == 0)  opcode = 0x7C;
else if (strcmp(cmd, "jg") == 0)  opcode = 0x7F;
if (opcode != 0 && tokens[1]) {
uint32_t instr_start_addr = (uint32_t)text_size;
emit_byte(opcode);
if (current_pass == 2) {
SymbolNode *node;
if (sym_lookup(tokens[1], &node)) {
int8_t offset = (int8_t)((int32_t)node->address - ((int32_t)instr_start_addr + 2));
emit_byte((uint8_t)offset);
} else { emit_byte(0x00); }
} else { emit_byte(0x00); }
return;
}
}
int main(int argc, char **argv) {
if (argc < 3) {
printf("Usage: %s <in.asm> <out.obj>\n", argv[0]);
return 1;
}
char *in_filename = argv[1];
char *out_filename = argv[2];
sym_insert(".text", 0, 1, 3);
sym_insert(".data", 0, 2, 3);
char line[256];
// PASS 1
current_pass = 1; text_size = 0; data_size = 0; current_section = 1;
FILE *in = fopen(in_filename, "r");
if (!in) { perror("Input assembly file failed to open"); return 1; }
while (fgets(line, sizeof(line), in)) { clean_line(line); assemble_line(line); }
rewind(in);
// PASS 2
current_pass = 2; size_t final_text_len = text_size; size_t final_data_len = data_size;
text_size = 0; data_size = 0; current_section = 1;
while (fgets(line, sizeof(line), in)) { clean_line(line); assemble_line(line); }
fclose(in);
FILE *out = fopen(out_filename, "wb");
if (!out) { perror("Output object file path opening failed"); return 1; }
uint32_t header_bytes = sizeof(COFFHeader) + (sizeof(SectionHeader) * 2);
uint32_t text_raw_ptr = header_bytes;
uint32_t data_raw_ptr = text_raw_ptr + (uint32_t)final_text_len;
uint32_t text_reloc_ptr = data_raw_ptr + (uint32_t)final_data_len;
COFFHeader coff = {
.Machine = 0x14C, .NumberOfSections = 2, .TimeDateStamp = 0,
.PointerToSymbolTable = text_reloc_ptr + (sizeof(COFFRelocation) * text_reloc_count),
.NumberOfSymbols = coff_sym_count, .SizeOfOptionalHeader = 0, .Characteristics = 0x0000
};
SectionHeader sec_text;
memset(&sec_text, 0, sizeof(sec_text));
memcpy(sec_text.Name, ".text", 5);
sec_text.SizeOfRawData = (uint32_t)final_text_len;
sec_text.PointerToRawData = text_raw_ptr;
sec_text.PointerToRelocations = text_reloc_count > 0 ? text_reloc_ptr : 0;
sec_text.NumberOfRelocations = (uint16_t)text_reloc_count;
sec_text.Characteristics = 0x60000020;
SectionHeader sec_data;
memset(&sec_data, 0, sizeof(sec_data));
memcpy(sec_data.Name, ".data", 5);
sec_data.SizeOfRawData = (uint32_t)final_data_len;
sec_data.PointerToRawData = data_raw_ptr;
sec_data.Characteristics = 0xC0000040;
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
printf("Success! Formatted COFF file generated cleanly at: %s\n", out_filename);
return 0;
}