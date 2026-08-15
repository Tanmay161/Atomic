#ifndef chunk_h
#define chunk_h

#include <stdint.h>
#include "value.h"
#include "ast.h"

typedef enum {
    OP_RETURN,
    OP_CONSTANT,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MOD,
} OpCode;

typedef struct {
    uint8_t *code;
    int count;
    int capacity;
    ValueArray constants;

    SourceSpan *spans;
} Chunk;

void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);

void writeChunk(Chunk *chunk, uint8_t byte, SourceSpan span);
void writeU16(Chunk *chunk, int index, SourceSpan span);

uint16_t readU16(uint8_t low, uint8_t high);

int addConstant(Chunk *chunk, Value value);

#endif