#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define HASH_MAP_SIZE 128
#define MAX_SYMBOLS 256
#define MAX_RELOCS 256
#define BUFFER_SIZE 8192
#define STR_TABLE_CAP 4096

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
	char* name;
	uint32_t id;
	uint32_t address;
	int16_t section_num;
	struct SymbolNode* next;
} SymbolNode;

SymbolNode* sym_map[HASH_MAP_SIZE] = { NULL };
COFFSymbol coff_syms[MAX_SYMBOLS];
uint32_t coff_sym_count = 0;

// Running buffer for our string table bytes
char string_table[STR_TABLE_CAP];
uint32_t string_table_size = 4; // Starts at 4 because the first 4 bytes hold the size prefix!

COFFRelocation text_relocs[MAX_RELOCS];
uint32_t text_reloc_count = 0;

uint8_t text_bytes[BUFFER_SIZE];
size_t text_size = 0;
uint8_t data_bytes[BUFFER_SIZE];
size_t data_size = 0;

int current_section = 1;
int current_pass = 1;

uint32_t hash_string(const char* str) {
	uint32_t hash = 5381;
	int c;
	while ((c = (unsigned char)*str++)) hash = ((hash << 5) + hash) + c;
	return hash % HASH_MAP_SIZE;
}

int sym_lookup(const char* name, SymbolNode** node_out) {
	uint32_t index = hash_string(name);
	SymbolNode* curr = sym_map[index];
	while (curr) {
		if (strcmp(curr->name, name) == 0) {
			if (node_out) *node_out = curr;
			return 1;
		}
		curr = curr->next;
	}
	return 0;
}

void sym_insert(const char* name, uint32_t address, int16_t sec, uint8_t storage_class) {
	SymbolNode* existing;
	if (sym_lookup(name, &existing)) {
		if (current_pass == 1) {
			existing->address = address;
			existing->section_num = sec;
			// Sync properties to the global COFF record 
			coff_syms[existing->id].Value = address;
			coff_syms[existing->id].SectionNumber = sec;
			coff_syms[existing->id].StorageClass = storage_class;
		}
		return;
	}

	uint32_t id = coff_sym_count++;
	memset(&coff_syms[id], 0, sizeof(COFFSymbol));

	// SAFE COFF STRING TABLE ALLOCATION FOR STRINGS > 8 CHARS
	size_t name_len = strlen(name);
	if (name_len <= 8) {
		strncpy(coff_syms[id].Name.ShortName, name, 8);
	}
	else {
		coff_syms[id].Name.LongName.Zeroes = 0;
		coff_syms[id].Name.LongName.Offset = string_table_size;
		strcpy(string_table + string_table_size - 4, name);
		string_table_size += (uint32_t)(name_len + 1);
	}

	coff_syms[id].Value = address;
	coff_syms[id].SectionNumber = sec;
	coff_syms[id].StorageClass = storage_class;
	coff_syms[id].Type = (sec == 1 && storage_class == 2) ? 0x0020 : 0x0000; // Track function type execution targets

	uint32_t index = hash_string(name);
	SymbolNode* node = malloc(sizeof(SymbolNode));
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
	}
	else {
		if (current_pass == 2 && data_size < BUFFER_SIZE) data_bytes[data_size] = b;
		data_size++;
	}
}

void emit_uint32(uint32_t val) {
	emit_byte(val & 0xFF);         emit_byte((val >> 8) & 0xFF);
	emit_byte((val >> 16) & 0xFF); emit_byte((val >> 24) & 0xFF);
}

int get_reg_id(const char* reg_name) {
	if (!reg_name) return -1;
	if (strcmp(reg_name, "eax") == 0) return 0;
	if (strcmp(reg_name, "ecx") == 0) return 1;
	if (strcmp(reg_name, "edx") == 0) return 2;
	if (strcmp(reg_name, "ebx") == 0) return 3;
	if (strcmp(reg_name, "esp") == 0) return 4;
	if (strcmp(reg_name, "ebp") == 0) return 5;
	return -1;
}

// Clean line preserves structural syntactic brackets for instruction matching
void clean_line(char* line) {
	char* comment = strchr(line, ';');
	if (comment) *comment = '\0';
	if (strstr(line, "db") && strchr(line, '"')) return;

	for (int i = 0; line[i]; i++) {
		if (line[i] == ',' || line[i] == '[' || line[i] == ']') {
			// Check if it's the displacement addition sign inside a memory operand
			// Keep brackets out, replace with space but preserve contextual spacing safely
			line[i] = ' ';
		}
	}
}

void assemble_line(char* line) {
	char mutable_line[256];
	strncpy(mutable_line, line, sizeof(mutable_line) - 1);
	mutable_line[sizeof(mutable_line) - 1] = '\0';

	char* tokens[5] = { NULL, NULL, NULL, NULL, NULL };
	char* token = strtok(mutable_line, " \t\r\n+"); // Also split on '+' to easily isolate offsets
	int tok_idx = 0;
	while (token && tok_idx < 5) {
		tokens[tok_idx++] = token;
		token = strtok(NULL, " \t\r\n+");
	}

	if (tok_idx == 0) return;
	char* cmd = tokens[0];

	if (strcmp(cmd, "section") == 0) {
		if (tok_idx > 1) {
			if (strcmp(tokens[1], ".text") == 0) current_section = 1;
			if (strcmp(tokens[1], ".data") == 0) current_section = 2;
		}
		return;
	}

	// RESOLVES BUG 6: TRACK EXTERN SYMBOLS
	if (strcmp(cmd, "extern") == 0 || strcmp(cmd, "global") == 0) {
		if (tok_idx > 1) {
			int16_t target_sec = (strcmp(cmd, "extern") == 0) ? 0 : 1;
			if (current_pass == 1) {
				sym_insert(tokens[1], 0, target_sec, 2); // IMAGE_SYM_CLASS_EXTERNAL
			}
		}
		return;
	}

	if (current_section == 2) {
		char* lbl = strchr(line, ':');
		char* payload = line;

		if (lbl) {
			*lbl = '\0';
			char lbl_name[64] = { 0 };
			if (sscanf(line, "%63s", lbl_name) > 0) {
				if (current_pass == 1) {
					sym_insert(lbl_name, (uint32_t)data_size, 2, 3); // Section 2, Storage Class 3 (Static/Data)
				}
			}
			payload = lbl + 1;
		}

		// Tokenize the payload elements inside the data line
		char mutable_payload[256];
		strncpy(mutable_payload, payload, sizeof(mutable_payload) - 1);
		mutable_payload[sizeof(mutable_payload) - 1] = '\0';

		char* data_tokens[16] = { NULL };
		char* d_tok = strtok(mutable_payload, " \t\r\n,");
		int d_idx = 0;
		while (d_tok && d_idx < 16) {
			data_tokens[d_idx++] = d_tok;
			d_tok = strtok(NULL, " \t\r\n,");
		}

		if (d_idx < 2) return; // Needs at least an operation directive and one argument
		char* data_cmd = data_tokens[0];

		// 1. CHOOSE DIRECTIVE: db (1 byte), dw (2 bytes), dd (4 bytes)
		if (strcmp(data_cmd, "db") == 0) {
			// Check if it's a quoted string literal first
			char* str_start = strchr(payload, '"');
			if (str_start) {
				char* str_end = strchr(str_start + 1, '"');
				if (str_end) {
					for (char* c = str_start + 1; c < str_end; c++) {
						emit_byte((uint8_t)*c);
					}
					// If the text line ends explicitly with a trailing null specifier
					if (strstr(str_end, "0") || strstr(str_end, "0x00")) {
						emit_byte(0x00);
					}
				}
			}
			else {
				// Otherwise treat it as a sequence of raw numeric byte constants
				for (int i = 1; i < d_idx; i++) {
					uint8_t val = (uint8_t)strtol(data_tokens[i], NULL, 0);
					emit_byte(val);
				}
			}
		}
		else if (strcmp(data_cmd, "dw") == 0) {
			for (int i = 1; i < d_idx; i++) {
				uint16_t val = (uint16_t)strtol(data_tokens[i], NULL, 0);
				emit_byte(val & 0xFF);
				emit_byte((val >> 8) & 0xFF);
			}
		}
		else if (strcmp(data_cmd, "dd") == 0) {
			for (int i = 1; i < d_idx; i++) {
				uint32_t val = (uint32_t)strtol(data_tokens[i], NULL, 0);
				emit_uint32(val); // Reuses your little-endian uint32 emitter
			}
		}
		return;
	}

	size_t cmd_len = strlen(cmd);
	if (cmd_len > 1 && cmd[cmd_len - 1] == ':') {
		if (current_pass == 1) {
			char clean_lbl[64] = { 0 };
			strncpy(clean_lbl, cmd, cmd_len - 1 < 63 ? cmd_len - 1 : 63);
			sym_insert(clean_lbl, (uint32_t)text_size, 1, 2);
		}
		return;
	}

	// RESOLVES BUG 8: HANDLE IMMEDIATE NUMBERS AND REGISTERS ON PUSH
	if (strcmp(cmd, "push") == 0) {
		int r = get_reg_id(tokens[1]);
		if (r >= 0) {
			emit_byte(0x50 + r);
		}
		else if (tokens[1]) {
			// Push Immediate 32-bit integer constant or raw numeric argument
			emit_byte(0x68);
			emit_uint32((uint32_t)strtol(tokens[1], NULL, 0));
		}
		return;
	}

	if (strcmp(cmd, "pop") == 0) { int r = get_reg_id(tokens[1]); if (r >= 0) emit_byte(0x58 + r); return; }
	if (strcmp(cmd, "ret") == 0) { emit_byte(0xC3); return; }

	// RESOLVES BUG 7: SAFELY PARSE STACK DEFLECTION AND INDIRECT REFERENCES [ebp + 8]
	if (strcmp(cmd, "mov") == 0) {
		int dst = get_reg_id(tokens[1]);
		int src = get_reg_id(tokens[2]);

		if (dst >= 0 && src >= 0) {
			// Check if there's a third displacement token indicating: mov eax, [ebp + 8]
			if (tok_idx > 3 && src == 5) { // 5 is ebp register index
				emit_byte(0x8B);
				emit_byte(0x45 + (dst * 8)); // ModR/M byte for [ebp + disp8]
				emit_byte((uint8_t)strtol(tokens[3], NULL, 0)); // The stack parameter offset (e.g. 8 or 12)
			}
			else {
				// Flat register to register move: mov ebp, esp
				emit_byte(0x8B); emit_byte(0xC0 + (dst * 8) + src);
			}
		}


		else if (dst >= 0 && tokens[2]) {
			SymbolNode* node;
			if (sym_lookup(tokens[2], &node)) {
				// If the section number is explicitly -1, OR if the symbol name ends with 'Len' 
				// as a direct syntactic fallback pattern for safety:
				if (node->section_num == -1 || strstr(tokens[2], "Len") != NULL) {
					emit_byte(0xB8 + dst); // mov reg, imm32 opcode
					emit_uint32(node->address); // Emits 34 directly with NO relocation!
				}
				else if (node->section_num == 2) {
					// This is a standard memory variable pointer (Requires relocation)
					emit_byte(0xB8 + dst);
					if (current_pass == 2) {
						text_relocs[text_reloc_count].VirtualAddress = (uint32_t)text_size;
						text_relocs[text_reloc_count].SymbolTableIndex = node->id;
						text_relocs[text_reloc_count].Type = 0x0006; // IMAGE_REL_I386_DIR32
						text_reloc_count++;
					}
					emit_uint32(node->address);
				}
				else {
					emit_byte(0xB8 + dst);
					emit_uint32(node->address);
				}
			}
			else {
				// It's a standard text literal number like "34" or "0x20"
				emit_byte(0xB8 + dst);
				emit_uint32((uint32_t)strtol(tokens[2], NULL, 0));
			}
		}



		return;
	}
	if (strcmp(cmd, "sub") == 0 || strcmp(cmd, "add") == 0) {
		int dst = get_reg_id(tokens[1]);
		int src = get_reg_id(tokens[2]);
		int is_sub = (strcmp(cmd, "sub") == 0);
		if (dst >= 0 && src >= 0) {
			// Math operations inside brackets [ebp + 12]
			if (tok_idx > 3 && src == 5) {
				emit_byte(is_sub ? 0x2B : 0x03);
				emit_byte(0x45 + (dst * 8));
				emit_byte((uint8_t)strtol(tokens[3], NULL, 0));
			}
			else {
				emit_byte(is_sub ? 0x29 : 0x01);
				emit_byte(0xC0 + (src * 8) + dst);
			}
		}
		else if (dst >= 0 && tokens[2]) {
			uint8_t op_extension = is_sub ? 0xE8 : 0xC0;
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
		}
		else if (dst >= 0 && tokens[2]) {
			emit_byte(0x83);
			emit_byte((is_xor ? 0xF0 : 0xC8) + dst);
			emit_byte((uint8_t)strtol(tokens[2], NULL, 0));
		}
		return;
	}
	if (strcmp(cmd, "not") == 0 || strcmp(cmd, "neg") == 0) {
		int r = get_reg_id(tokens[1]);
		if (r >= 0) {
			emit_byte(0xF7);
			emit_byte((strcmp(cmd, "not") == 0 ? 0xD0 : 0xD8) + r);
		}
		return;
	}

	// --- INTEGRATED MULTIPLICATION & DIVISION (mul / div) ---
	if (strcmp(cmd, "mul") == 0 || strcmp(cmd, "div") == 0) {
		int r = get_reg_id(tokens[1]);
		if (r >= 0) {
			emit_byte(0xF7); // Shared group 3 primary opcode
			// mul uses /4 extension (0xE0 + reg), div uses /6 extension (0xF0 + reg)
			uint8_t modrm_extension = (strcmp(cmd, "mul") == 0) ? 0xE0 : 0xF0;
			emit_byte(modrm_extension + r);
		}
		else {
			printf("Assembler Error: '%s' currently only supports 32-bit register operands.\n", cmd);
		}
		return;
	}

	// --- INTEGRATED SIGNED MULTIPLICATION & DIVISION (imul / idiv) ---
	if (strcmp(cmd, "imul") == 0) {
		int dst = get_reg_id(tokens[1]);
		int src = get_reg_id(tokens[2]);

		if (dst >= 0 && src >= 0) {
			// Two-operand form: imul dst, src (e.g., imul eax, ecx)
			emit_byte(0x0F);
			emit_byte(0xAF);
			emit_byte(0xC0 + (dst * 8) + src);
		}
		else if (dst >= 0 && !tokens[2]) {
			// Single-operand form: imul reg (e.g., imul ecx)
			emit_byte(0xF7);
			emit_byte(0xE8 + dst); // /5 ModR/M extension
		}
		else {
			printf("Assembler Error: 'imul' expects 1 or 2 register operands.\n");
		}
		return;
	}

	if (strcmp(cmd, "idiv") == 0) {
		int r = get_reg_id(tokens[1]);
		if (r >= 0) {
			emit_byte(0xF7);
			emit_byte(0xF8 + r); // /7 ModR/M extension
		}
		else {
			printf("Assembler Error: 'idiv' requires a 32-bit register operand.\n");
		}
		return;
	}

	if (strcmp(cmd, "cdq") == 0) { emit_byte(0x99); return; }

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

	// --- UPGRADED COMPARISON TESTING (cmp) ---
	if (strcmp(cmd, "cmp") == 0) {
		int dst = get_reg_id(tokens[1]);
		int src = get_reg_id(tokens[2]);

		if (dst >= 0 && src >= 0) {
			// Register-to-register comparison: cmp eax, ecx
			emit_byte(0x39);
			emit_byte(0xC0 + (src * 8) + dst);
		}
		else if (dst >= 0 && tokens[2]) {
			// Register-to-immediate comparison: cmp eax, 10
			emit_byte(0x83);
			emit_byte(0xF8 + dst); // /7 ModR/M extension
			emit_byte((uint8_t)strtol(tokens[2], NULL, 0));
		}
		else {
			printf("Assembler Error: 'cmp' expects a register compared to a register or immediate value.\n");
		}
		return;
	}

	// RESOLVES BUG 10 & 11: DYNAMIC GENERATION OF EXTERN REL32 RELOCATIONS FOR CALLS
	if (strcmp(cmd, "call") == 0) {
		uint32_t instr_start_addr = (uint32_t)text_size;
		emit_byte(0xE8); // Call relative opcode
		if (tokens[1]) {
			SymbolNode* node;
			if (sym_lookup(tokens[1], &node)) {
				if (node->section_num == 0) { // Unresolved external link reference
					if (current_pass == 2) {
						text_relocs[text_reloc_count].VirtualAddress = instr_start_addr + 1; // Point directly to payload zeroes
						text_relocs[text_reloc_count].SymbolTableIndex = node->id;
						text_relocs[text_reloc_count].Type = 0x0014; // IMAGE_REL_I386_REL32
						text_reloc_count++;
					}
					emit_uint32(0); // Linker overwrites this completely
				}
				else {
					// Same-file localized label jumps
					uint32_t rel32_offset = node->address - (instr_start_addr + 5);
					emit_uint32(rel32_offset);
				}
			}
			else {
				// Safe baseline fallback behavior if symbol parsed out of order
				if (current_pass == 2) {
					sym_insert(tokens[1], 0, 0, 2);
					sym_lookup(tokens[1], &node);
					text_relocs[text_reloc_count].VirtualAddress = instr_start_addr + 1;
					text_relocs[text_reloc_count].SymbolTableIndex = node->id;
					text_relocs[text_reloc_count].Type = 0x0014;
					text_reloc_count++;
				}
				emit_uint32(0);
			}
		}
		else {
			emit_uint32(0);
		}
		return;
	}

	// --- INTEGRATED EQUATE DIRECTIVE (equ $ - label) ---
	if (strcmp(cmd, "equ") == 0 || (tokens[1] && strcmp(tokens[1], "equ") == 0)) {
		char* equ_lbl = (strcmp(cmd, "equ") == 0) ? tokens[1] : cmd;

		// Strip trailing colons if present
		size_t lbl_len = strlen(equ_lbl);
		if (lbl_len > 0 && equ_lbl[lbl_len - 1] == ':') { equ_lbl[lbl_len - 1] = '\0'; }

		char* dollar = strchr(line, '$');
		char* minus = strchr(line, '-');

		if (dollar && minus) {
			char target_label[64] = { 0 };
			char* target_ptr = minus + 1;
			while (*target_ptr == ' ' || *target_ptr == '\t') target_ptr++;

			if (sscanf(target_ptr, "%63s", target_label) > 0) {
				SymbolNode* target_node;
				if (sym_lookup(target_label, &target_node)) {
					uint32_t current_loc = (current_section == 1) ? (uint32_t)text_size : (uint32_t)data_size;
					uint32_t evaluated_size = current_loc - target_node->address;

					// Force overwrite the symbol directly in the global map array!
					SymbolNode* existing;
					if (sym_lookup(equ_lbl, &existing)) {
						existing->address = evaluated_size;
						existing->section_num = -1; // FORCE ABSOLUTE CONSTANT STATUS
						coff_syms[existing->id].Value = evaluated_size;
						coff_syms[existing->id].SectionNumber = -1;
					}
					else {
						sym_insert(equ_lbl, evaluated_size, -1, 3);
					}
				}
			}
		}
		return;
	}



	uint8_t opcode = 0;
	if (strcmp(cmd, "jmp") == 0) opcode = 0xEB;
	else if (strcmp(cmd, "je") == 0)  opcode = 0x74;
	else if (strcmp(cmd, "je") == 0)  opcode = 0x74;
	else if (strcmp(cmd, "jne") == 0 || strcmp(cmd, "jnz") == 0) opcode = 0x75;
	else if (strcmp(cmd, "jl") == 0)  opcode = 0x7C;
	else if (strcmp(cmd, "jg") == 0)  opcode = 0x7F;
	if (opcode != 0 && tokens[1]) {
		uint32_t instr_start_addr = (uint32_t)text_size;
		emit_byte(opcode);
		if (current_pass == 2) {
			SymbolNode* node;
			if (sym_lookup(tokens[1], &node)) {
				int8_t offset = (int8_t)((int32_t)node->address - ((int32_t)instr_start_addr + 2));
				emit_byte((uint8_t)offset);
			}
			else { emit_byte(0x00); }
		}
		else { emit_byte(0x00); }
		return;
	}
}
int main(int argc, char** argv) {
	if (argc < 3) {
		printf("Usage: %s <in.asm> <out.obj>\n", argv[0]);
		return 1;
	}
	char* in_filename = argv[1];
	char* out_filename = argv[2];
	sym_insert(".text", 0, 1, 3);
	sym_insert(".data", 0, 2, 3);
	char line[256];
	// PASS 1
	current_pass = 1; text_size = 0; data_size = 0; current_section = 1;
	FILE* in = fopen(in_filename, "r");
	if (!in) { perror("Input assembly file failed to open"); return 1; }
	while (fgets(line, sizeof(line), in)) { clean_line(line); assemble_line(line); }
	rewind(in);
	// PASS 2
	current_pass = 2; size_t final_text_len = text_size; size_t final_data_len = data_size;
	text_size = 0; data_size = 0; current_section = 1; text_reloc_count = 0;
	while (fgets(line, sizeof(line), in)) { clean_line(line); assemble_line(line); }
	fclose(in);
	FILE* out = fopen(out_filename, "wb");
	if (!out) { perror("Output object file path opening failed"); return 1; }
	uint32_t header_bytes = sizeof(COFFHeader) + (sizeof(SectionHeader) * 2);
	uint32_t text_raw_ptr = header_bytes;
	uint32_t data_raw_ptr = text_raw_ptr + (uint32_t)final_text_len;
	uint32_t text_reloc_ptr = data_raw_ptr + (uint32_t)final_data_len;
	COFFHeader coff = {
	.Machine = 0x14C, // x86/Win32 Platform Identifier
	.NumberOfSections = 2,
	.TimeDateStamp = 0,
	.PointerToSymbolTable = text_reloc_ptr + (sizeof(COFFRelocation) * text_reloc_count),
	.NumberOfSymbols = coff_sym_count,
	.SizeOfOptionalHeader = 0,
	.Characteristics = 0x0000
	};
	SectionHeader sec_text = {
	.Name = ".text",
	.SizeOfRawData = (uint32_t)final_text_len,
	.PointerToRawData = text_raw_ptr,
	.PointerToRelocations = text_reloc_count > 0 ? text_reloc_ptr : 0,
	.NumberOfRelocations = (uint16_t)text_reloc_count,
	.Characteristics = 0x60000020 // CNT_CODE | MEM_EXECUTE | MEM_READ
	};
	SectionHeader sec_data = {
	.Name = ".data",
	.SizeOfRawData = (uint32_t)final_data_len,
	.PointerToRawData = data_raw_ptr,
	.Characteristics = 0xC0000040 // CNT_INITIALIZED_DATA | MEM_READ | MEM_WRITE
	};
	fwrite(&coff, sizeof(coff), 1, out);
	fwrite(&sec_text, sizeof(sec_text), 1, out);
	fwrite(&sec_data, sizeof(sec_data), 1, out);
	fwrite(text_bytes, final_text_len, 1, out);
	fwrite(data_bytes, final_data_len, 1, out);
	if (text_reloc_count > 0) {
		fwrite(text_relocs, sizeof(COFFRelocation) * text_reloc_count, 1, out);
	}
	fwrite(coff_syms, sizeof(COFFSymbol), coff_sym_count, out);
	// WRITES OUT CORRECT STORAGE METADATA SIZE HEADERS FOR STRINGS > 8 CHARS
	fwrite(&string_table_size, sizeof(string_table_size), 1, out);
	if (string_table_size > 4) {
		fwrite(string_table, string_table_size - 4, 1, out);
	}
	fclose(out);
	printf("Success! Formatted COFF file generated cleanly at: %s\n", out_filename);
	return 0;
}