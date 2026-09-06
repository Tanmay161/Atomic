#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "debug.h"
#include "error.h"
#include "chunk.h"

static int simpleInstruction(const char *name, FILE *output, int offset);
static int constantInstruction(Chunk *chunk, const char *name, FILE *output, int offset);
static int globalDef(Chunk *chunk, FILE *output, int offset);

void disassembleChunk(Chunk *chunk)
{
    FILE *output = fopen("./compiler/result.abc", "w");

    if (!output)
    {
        fprintf(stderr, "Failed to create debug file\n");
        exit(1);
    }

    for (int offset = 0; offset < chunk->count;)
    {
        offset = disassembleInstruction(chunk, output, offset);
    }
}

int disassembleInstruction(Chunk *chunk, FILE *output, int offset)
{
    fprintf(output, "%04d ", offset);

    uint8_t instruction = chunk->code[offset];
    switch (instruction)
    {
    case OP_RETURN:
        return simpleInstruction("OP_RETURN", output, offset);
    case OP_CONSTANT:
        return constantInstruction(chunk, "OP_CONSTANT", output, offset);
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
    case OP_TRUE:
        return simpleInstruction("OP_TRUE", output, offset);
    case OP_FALSE:
        return simpleInstruction("OP_FALSE", output, offset);
    case OP_NIL:
        return simpleInstruction("OP_NIL", output, offset);
    case OP_NOT:
        return simpleInstruction("OP_NOT", output, offset);
    case OP_EQUAL:
        return simpleInstruction("OP_EQUAL", output, offset);
    case OP_NOT_EQUAL:
        return simpleInstruction("OP_NOT_EQUAL", output, offset);
    case OP_LESS:
        return simpleInstruction("OP_LESS", output, offset);
    case OP_LESS_EQUAL:
        return simpleInstruction("OP_LESS_EQUAL", output, offset);
    case OP_GREATER:
        return simpleInstruction("OP_GREATER", output, offset);
    case OP_GREATER_EQUAL:
        return simpleInstruction("OP_GREATER_EQUAL", output, offset);
    case OP_POP:
        return simpleInstruction("OP_POP", output, offset);
    case OP_DEFINE_GLOBAL:
        return constantInstruction(chunk, "OP_DEFINE_GLOBAL", output, offset);
    case OP_GET_GLOBAL:
        return constantInstruction(chunk, "OP_GET_GLOBAL", output, offset);
    case OP_SET_GLOBAL:
        return constantInstruction(chunk, "OP_SET_GLOBAL", output, offset);
    case OP_AND:
        return simpleInstruction("OP_AND", output, offset);
    case OP_OR:
        return simpleInstruction("OP_OR", output, offset);
    default:
    {
        printf("Unknown opcode: %d\n", instruction);
        return offset + 1;
    }
    }
}

static int simpleInstruction(const char *name, FILE *output, int offset)
{
    fprintf(output, "%s\n", name);
    return offset + 1;
}

static int constantInstruction(Chunk *chunk, const char *name, FILE *output, int offset)
{
    uint8_t low = chunk->code[offset + 1];
    uint8_t high = chunk->code[offset + 2];

    uint16_t index = readU16(low, high);

    // fprintf(output, "%-16s %4d '", name, index);
    fprintf(output, "%s %d \n", name, index);
    //fprintValue(output, chunk->constants.values[index]);
    //fprintf(output, "'\n");

    return offset + 3;
}