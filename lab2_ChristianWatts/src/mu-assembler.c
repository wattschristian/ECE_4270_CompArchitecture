#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "mu-riscv.h"

#define MAX_LINE    256
#define MAX_LBL     64
#define MAX_INST    1024
#define MAX_SYM     256
#define MAX_DWORDS  1024

typedef enum { SEG_NONE, SEG_TEXT, SEG_DATA } SegType;

typedef struct {
    char lbl[MAX_LBL];
    int addr;
} SymEntry;

typedef struct {
    char instLine[MAX_LINE];
    int addr;
} InstRec;

typedef struct {
    int addr;
    uint32_t val;
} DataRec;

SymEntry symTab[MAX_SYM];
int symCount = 0;

InstRec instArr[MAX_INST];
int instCount = 0;

DataRec dataArr[MAX_DWORDS];
int dataCount = 0;

// Remove leading and trailing spaces 
void trimStr(char *s) {
    int i = 0, j = strlen(s) - 1;

    // Remove leading spaces
    while (s[i] && isspace((unsigned char)s[i])) i++;
    // Remove trailing spaces
    while (j > i && isspace((unsigned char)s[j])) j--;

    // Copy the trimmed string manually
    char temp[MAX_LINE];
    int k = 0;
    for (int idx = i; idx <= j; idx++) {
        temp[k++] = s[idx];
    }
    temp[k] = '\0'; // Null terminate
    strcpy(s, temp);
}


// Process a line from the text segment 
void processTextLine(char *line, int *curAddr) {
    char *colon = strchr(line, ':');
    if(colon) {
        char tmpLbl[MAX_LBL];
        int len = colon - line;
        strncpy(tmpLbl, line, len);
        tmpLbl[len] = '\0';
        trimStr(tmpLbl);
        strcpy(symTab[symCount].lbl, tmpLbl);
        symTab[symCount].addr = *curAddr;
        symCount++;
        char *rest = colon+1;
        trimStr(rest);
        if(strlen(rest) > 0) {
            strcpy(instArr[instCount].instLine, rest);
            instArr[instCount].addr = *curAddr;
            instCount++;
            *curAddr += 4;
        }
    } else {
        strcpy(instArr[instCount].instLine, line);
        instArr[instCount].addr = *curAddr;
        instCount++;
        *curAddr += 4;
    }
}

// Process a line from the data segment
void processDataLine(char *line, int *curAddr) {
    char *colon = strchr(line, ':');
    if(colon) {
        char tmpLbl[MAX_LBL];
        int len = colon - line;
        strncpy(tmpLbl, line, len);
        tmpLbl[len] = '\0';
        trimStr(tmpLbl);
        strcpy(symTab[symCount].lbl, tmpLbl);
        symTab[symCount].addr = *curAddr;
        symCount++;
        char *rest = colon+1;
        trimStr(rest);
        if(strncmp(rest, ".word", 5) == 0) {
            rest += 5;
            char *tok = strtok(rest, ",");
            while(tok) {
                trimStr(tok);
                dataArr[dataCount].addr = *curAddr;
                dataArr[dataCount].val = strtol(tok, NULL, 0);
                dataCount++;
                *curAddr += 4;
                tok = strtok(NULL, ",");
            }
        }
    } else if(strncmp(line, ".word", 5) == 0) {
        char *rest = line+5;
        char *tok = strtok(rest, ",");
        while(tok) {
            trimStr(tok);
            dataArr[dataCount].addr = *curAddr;
            dataArr[dataCount].val = strtol(tok, NULL, 0);
            dataCount++;
            *curAddr += 4;
            tok = strtok(NULL, ",");
        }
    } else {
        printf("Data error: expected .word\n");
        exit(1);
    }
}

// First pass: read file and build symbol table and text/data segment arrays 
void processInput(FILE *fp) {
    char line[MAX_LINE];
    SegType seg = SEG_NONE;
    int curAddr = 0;
    while(fgets(line, MAX_LINE, fp)) {
        trimStr(line);
        if(strlen(line)==0) continue;
        if(strcmp(line, ".text") == 0) {
            seg = SEG_TEXT;
            curAddr = MEM_TEXT_BEGIN;
            continue;
        } else if(strcmp(line, ".data") == 0) {
            seg = SEG_DATA;
            curAddr = MEM_DATA_BEGIN;
            continue;
        }
        if(seg == SEG_TEXT) {
            processTextLine(line, &curAddr);
        } else if(seg == SEG_DATA) {
            processDataLine(line, &curAddr);
        }
    }
}

int getReg(char *t) {
    trimStr(t);
    if(t[0]=='x') return atoi(t+1);
    if(strcmp(t,"zero")==0) return 0;
    return atoi(t);
}

int lookupSym(const char *lbl) {
    if (!lbl || strlen(lbl) == 0) {
        printf("Error: lookupSym called with NULL or empty.\n");
        return -1;
    }

    for (int i = 0; i < symCount; i++) {
        if (strcmp(symTab[i].lbl, lbl) == 0) {
            //printf("Debug: Found symbol '%s' at address %d\n", lbl, symTab[i].addr);
            return symTab[i].addr;
        }
    }

    printf("Label '%s' not found.\n", lbl);
    return -1;
}


// Encoding functions
uint32_t encode_R(uint8_t opcode, uint8_t rd, uint8_t funct3, uint8_t rs1, uint8_t rs2, uint8_t funct7) {
    uint32_t instruction = 0;

    // Shift and place each field into its position
    uint32_t funct7_part = ((uint32_t)funct7) << 25;
    uint32_t rs2_part = ((uint32_t)rs2) << 20;
    uint32_t rs1_part = ((uint32_t)rs1) << 15;
    uint32_t funct3_part = ((uint32_t)funct3) << 12;
    uint32_t rd_part = ((uint32_t)rd) << 7;

    // Construct the final instruction
    instruction |= funct7_part;
    instruction |= rs2_part;
    instruction |= rs1_part;
    instruction |= funct3_part;
    instruction |= rd_part;
    instruction |= opcode;

    //printf("Encoding R-Type: funct7=%02X, rs2=%02X, rs1=%02X, funct3=%02X, rd=%02X, opcode=%02X -> %08X\n", funct7, rs2, rs1, funct3, rd, opcode, instruction);
    return instruction;
}


uint32_t encode_I(uint8_t opcode, uint8_t rd, uint8_t funct3, uint8_t rs1, int32_t imm) {
    uint32_t instruction = 0;
    uint32_t imm_part = (imm & 0xFFF) << 20;
    uint32_t rs1_part = ((uint32_t)rs1) << 15;
    uint32_t funct3_part = ((uint32_t)funct3) << 12;
    uint32_t rd_part = ((uint32_t)rd) << 7;

    instruction |= imm_part;
    instruction |= rs1_part;
    instruction |= funct3_part;
    instruction |= rd_part;
    instruction |= opcode;

    //printf("Encoding I-Type: imm=%X, rs1=%02X, funct3=%02X, rd=%02X, opcode=%02X -> %08X\n", imm, rs1, funct3, rd, opcode, instruction);
    return instruction;
}


uint32_t encode_S(uint8_t opcode, uint8_t funct3, uint8_t rs1, uint8_t rs2, int32_t imm) {
    uint32_t lowBits  = imm & 0x1F;
    uint32_t highBits = (imm >> 5) & 0x7F;
    return (highBits << 25) | (((uint32_t)rs2) << 20) | (((uint32_t)rs1) << 15) | (((uint32_t)funct3) << 12) | (lowBits << 7) | opcode;
}

uint32_t encode_B(uint8_t opcode, uint8_t funct3, uint8_t rs1, uint8_t rs2, int32_t offset) {
    if (offset % 2 != 0) {
        fprintf(stderr, "Error: Branch offset must be even. Given: %d\n", offset);
        return 0;
    }

    uint32_t instruction = 0;
    uint32_t bit12 = (offset >> 12) & 0x1;
    uint32_t bit11 = (offset >> 11) & 0x1;
    uint32_t bits10_5 = (offset >> 5) & 0x3F;
    uint32_t bits4_1 = (offset >> 1) & 0xF;

    instruction |= (bit12 << 31);
    instruction |= (bits10_5 << 25);
    instruction |= ((uint32_t)rs2 << 20);
    instruction |= ((uint32_t)rs1 << 15);
    instruction |= ((uint32_t)funct3 << 12);
    instruction |= (bits4_1 << 8);
    instruction |= (bit11 << 7);
    instruction |= opcode;

    //printf("Encoding B-Type: offset=%X, rs1=%02X, rs2=%02X, funct3=%02X, opcode=%02X -> %08X\n", offset, rs1, rs2, funct3, opcode, instruction);

    return instruction;
}


uint32_t encode_U(uint8_t opcode, uint8_t rd, int32_t imm) {
    return (((imm & 0xFFFFF)) << 12) | (((uint32_t)rd) << 7) | opcode;
}

uint32_t encode_J(uint8_t opcode, uint8_t rd, int32_t offset) {
    if (offset % 2 != 0) {
        printf("Jump offset error\n");
        exit(1);
    }
    uint32_t bit20    = (offset >> 20) & 0x1;
    uint32_t bits10_1 = (offset >> 1) & 0x3FF;
    uint32_t bit11    = (offset >> 11) & 0x1;
    uint32_t bits19_12 = (offset >> 12) & 0xFF;
    return (bit20 << 31) | (bits19_12 << 12) | (bit11 << 20) | (bits10_1 << 21) | (((uint32_t)rd) << 7) | opcode;
}


// Assemble a single instruction line into machine code
uint32_t assembleInst(char *line, int curAddr) {
    char orig[MAX_LINE];
    strcpy(orig, line);
    char *token = strtok(line, " ,\t");
    if(token == NULL) return 0;
    char operation[16];
    strcpy(operation, token);
    int rd, rs1, rs2, imm, target;
    
    if(strcmp(operation, "add")==0 || strcmp(operation, "sub")==0 || strcmp(operation, "sll")==0 || strcmp(operation, "slt")==0 || strcmp(operation, "sltu")==0 || strcmp(operation, "xor")==0 ||
       strcmp(operation, "srl")==0 || strcmp(operation, "sra")==0 || strcmp(operation, "or")==0 || strcmp(operation, "and")==0) {

        token = strtok(NULL, " ,\t"); 
        rd = getReg(token);
        token = strtok(NULL, " ,\t"); 
        rs1 = getReg(token);
        token = strtok(NULL, " ,\t"); 
        rs2 = getReg(token);

        uint8_t op = OPCODE_R_TYPE;
        uint8_t f3 = 0, f7 = 0;
        if(strcmp(operation, "add")==0){ f3 = ADD_SUB; f7 = ADD; }
        else if(strcmp(operation, "sub")==0){ 
            f3 = ADD_SUB; 
            f7 = SUB; 
        }
        else if(strcmp(operation, "sll")==0){ 
            f3 = SLL; 
            f7 = ADD; 
        }
        else if(strcmp(operation, "slt")==0){ 
            f3 = SLT; 
            f7 = ADD; 
        }
        else if(strcmp(operation, "sltu")==0){ 
            f3 = SLTU; 
            f7 = ADD; 
        }
        else if(strcmp(operation, "xor")==0){ 
            f3 = XOR; 
            f7 = ADD; 
        }
        else if(strcmp(operation, "srl")==0){ 
            f3 = SRL_SRA; 
            f7 = SRL;
        }
        else if(strcmp(operation, "sra")==0){ 
            f3 = SRL_SRA; 
            f7 = SRA; 
        }
        else if(strcmp(operation, "or")==0){ 
            f3 = OR; 
            f7 = ADD; 
        }
        else if(strcmp(operation, "and")==0){ 
            f3 = AND; 
            f7 = ADD; 
        }
        return encode_R(op, rd, f3, rs1, rs2, f7);
    }
    else if(strcmp(operation, "addi")==0 || strcmp(operation, "slti")==0 || strcmp(operation, "sltiu")==0 || strcmp(operation, "xori")==0 || strcmp(operation, "ori")==0 || strcmp(operation, "andi")==0) {

        token = strtok(NULL, " ,\t"); 
        rd = getReg(token);
        token = strtok(NULL, " ,\t"); 
        rs1 = getReg(token);
        token = strtok(NULL, " ,\t"); 
        imm = strtol(token, NULL, 0);

        uint8_t op = OPCODE_I_TYPE, f3 = 0;
        if(strcmp(operation, "addi")==0) f3 = 0x0;
        else if(strcmp(operation, "slti")==0) f3 = 0x2;
        else if(strcmp(operation, "sltiu")==0) f3 = 0x3;
        else if(strcmp(operation, "xori")==0) f3 = 0x4;
        else if(strcmp(operation, "ori")==0) f3 = 0x6;
        else if(strcmp(operation, "andi")==0) f3 = 0x7;
        return encode_I(op, rd, f3, rs1, imm);
    }
    else if(strcmp(operation, "slli")==0 || strcmp(operation, "srli")==0 || strcmp(operation, "srai")==0) {
        token = strtok(NULL, " ,\t"); 
        rd = getReg(token);
        token = strtok(NULL, " ,\t"); 
        rs1 = getReg(token);
        token = strtok(NULL, " ,\t"); 
        imm = strtol(token, NULL, 0);

        uint8_t op = OPCODE_I_TYPE, f3 = 0x1, f7 = 0;
        if(strcmp(operation, "slli")==0) f7 = 0x00;
        else if(strcmp(operation, "srli")==0) f7 = 0x00;
        else if(strcmp(operation, "srai")==0) f7 = 0x20;
        uint32_t shamt = imm & 0x1F;
        return (((uint32_t)f7 << 25) | ((uint32_t)shamt << 20) | ((uint32_t)rs1 << 15) | ((uint32_t)f3 << 12) | ((uint32_t)rd << 7) | op);
    }
    else if(strcmp(operation, "lw")==0 || strcmp(operation, "lh")==0 || strcmp(operation, "lb")==0 || strcmp(operation, "lhu")==0 || strcmp(operation, "lbu")==0) {
        token = strtok(NULL, " ,\t"); 
        rd = getReg(token);
        token = strtok(NULL, " ,\t");
        char offStr[32]={0}, regStr[32]={0};
        char *paren = strchr(token, '(');
        if(!paren){ printf("Load format error\n"); exit(1); }
        int len = paren - token;
        strncpy(offStr, token, len); 
        offStr[len] = '\0'; 
        trimStr(offStr);
        imm = strtol(offStr, NULL, 0);
        paren++;
        char *endParen = strchr(paren, ')');
        if(!endParen){ printf("Missing ) in load\n"); exit(1); }
        int rlen = endParen - paren;
        strncpy(regStr, paren, rlen); 
        regStr[rlen] = '\0';
        rs1 = getReg(regStr);
        uint8_t op = OPCODE_LOAD, f3 = 0;
        if(strcmp(operation, "lb")==0) f3 = 0x0;
        else if(strcmp(operation, "lh")==0) f3 = 0x1;
        else if(strcmp(operation, "lw")==0) f3 = 0x2;
        else if(strcmp(operation, "lbu")==0) f3 = 0x4;
        else if(strcmp(operation, "lhu")==0) f3 = 0x5;
        return encode_I(op, rd, f3, rs1, imm);
    }
    else if(strcmp(operation, "sw")==0 || strcmp(operation, "sh")==0 || strcmp(operation, "sb")==0) {
        token = strtok(NULL, " ,\t"); 
        rs2 = getReg(token);
        token = strtok(NULL, " ,\t");
        char offStr[32]={0}, regStr[32]={0};
        char *paren = strchr(token, '(');
        if(!paren){ printf("Store format error\n"); exit(1); }
        int len = paren - token;
        strncpy(offStr, token, len); 
        offStr[len] = '\0'; 
        trimStr(offStr);
        imm = strtol(offStr, NULL, 0);
        paren++;
        char *endParen = strchr(paren, ')');
        if(!endParen){ printf("Missing ) in store\n"); exit(1); }
        int rlen = endParen - paren;
        strncpy(regStr, paren, rlen); 
        regStr[rlen] = '\0';
        rs1 = getReg(regStr);
        uint8_t op = OPCODE_STORE, f3 = 0;
        if(strcmp(operation, "sb")==0) f3 = SB;
        else if(strcmp(operation, "sh")==0) f3 = SH;
        else if(strcmp(operation, "sw")==0) f3 = SW;
        return encode_S(op, f3, rs1, rs2, imm);
    }
    else if(strcmp(operation, "beq")==0 || strcmp(operation, "bne")==0 || strcmp(operation, "blt")==0 || strcmp(operation, "bge")==0 || strcmp(operation, "bltu")==0 || strcmp(operation, "bgeu")==0) {
        token = strtok(NULL, " ,\t"); 
        rs1 = getReg(token);
        token = strtok(NULL, " ,\t"); 
        rs2 = getReg(token);
        token = strtok(NULL, " ,\t");

        char lbl[MAX_LBL];
        strcpy(lbl, token); 
        trimStr(lbl);
        target = lookupSym(lbl);
        if(target == -1){ printf("Undefined label: %s\n", lbl); exit(1); }
        int offset = target - curAddr;
        uint8_t op = OPCODE_BRANCH, f3 = 0;
        if(strcmp(operation, "beq")==0) f3 = 0x0;
        else if(strcmp(operation, "bne")==0) f3 = 0x1;
        else if(strcmp(operation, "blt")==0) f3 = 0x4;
        else if(strcmp(operation, "bge")==0) f3 = 0x5;
        else if(strcmp(operation, "bltu")==0) f3 = 0x6;
        else if(strcmp(operation, "bgeu")==0) f3 = 0x7;
        return encode_B(op, f3, rs1, rs2, offset);
    }
    else if(strcmp(operation, "jal")==0) {
        token = strtok(NULL, " ,\t"); 
        rd = getReg(token);
        token = strtok(NULL, " ,\t");
        char lbl[MAX_LBL];
        strcpy(lbl, token); 
        trimStr(lbl);
        target = lookupSym(lbl);
        if(target == -1){ printf("Undefined label: %s\n", lbl); exit(1); }
        int offset = target - curAddr;
        uint8_t op = OPCODE_JAL;
        return encode_J(op, rd, offset);
    }
    else if(strcmp(operation, "j")==0) {
        rd = 0;
        token = strtok(NULL, " ,\t");
        char lbl[MAX_LBL];
        strcpy(lbl, token); trimStr(lbl);
        target = lookupSym(lbl);
        if(target == -1){ printf("Undefined label: %s\n", lbl); exit(1); }
        int offset = target - curAddr;
        uint8_t op = OPCODE_JAL;
        return encode_J(op, rd, offset);
    }
    else if(strcmp(operation, "lui")==0 || strcmp(operation, "auipc")==0) {
        token = strtok(NULL, " ,\t"); 
        rd = getReg(token);
        token = strtok(NULL, " ,\t"); 
        imm = strtol(token, NULL, 0);
        uint8_t op = (strcmp(operation, "lui")==0) ? OPCODE_LUI : 0x17;
        return encode_U(op, rd, imm);
    }
    else {
        printf("Unknown operationonic %s in line: %s\n", operation, orig);
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if(argc < 3) {
        printf("Usage: ./assemble <input file> <output file>\n");
        exit(1);
    }
    FILE *input = fopen(argv[1], "r");
    if(!input) { printf("Error opening input file"); exit(1); }

    processInput(input);
    fclose(input);

    FILE *output = fopen(argv[2], "w");
    if(!output) { printf("Error opening ouptut file"); exit(1); }

    for(int i = 0; i < instCount; i++) {
        char lineCopy[MAX_LINE];
        strcpy(lineCopy, instArr[i].instLine);
        uint32_t mc = assembleInst(lineCopy, instArr[i].addr);
        fprintf(output, "%08x\n", mc);
    }
    for(int i = 0; i < dataCount; i++) {
        fprintf(output, "%08x\n", dataArr[i].val);
    }
    fclose(output);
    printf("Assembly done. %d instructions, %d data words\n", instCount, dataCount);
    return 0;
}
