#ifndef chunk_h
#define chunk_h

#include <stdint.h>
#include "value.h"

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
} Chunk;

void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte);
int addConstant(Chunk *chunk, Value value);
uint16_t readU16(uint8_t low, uint8_t high);
void writeU16(Chunk *chunk, int index);

#endif