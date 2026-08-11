#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "debug.h"
#include "shared.h"
#include "chunk.h"

void disassembleChunk(Chunk *chunk) {
    FILE *output = fopen("./compiler/result.abc", "w");

    if (!output) {
        fprintf(stderr, "Failed to create debug file\n");
        exit(1);
    }

    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, output, offset);
    }
}

int disassembleInstruction(Chunk *chunk, FILE *output, int offset) {
    fprintf(output, "%04d ", offset);

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_RETURN: 
            return simpleInstruction("OP_RETURN", output, offset);
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, output, offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", output, offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", output, offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", output, offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", output, offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", output, offset); 
        default: {
            printf("Unknown opcode: %d\n", instruction);
            return offset + 1;
        }
    }
}

static int simpleInstruction(const char *name, FILE *output, int offset) {
    fprintf(output, "%s\n", name);
    return offset + 1;
}

static int constantInstruction(const char *name, Chunk *chunk, FILE *output, int offset) {
    uint8_t low = chunk->code[offset + 1];
    uint8_t high = chunk->code[offset + 2];

    uint16_t index = readU16(low, high);

    //fprintf(output, "%-16s %4d '", name, index);
    fprintf(output, "OP_CONSTANT %d '", index);
    fprintValue(output, chunk->constants.values[index]);
    fprintf(output, "'\n");

    return offset + 3;
}