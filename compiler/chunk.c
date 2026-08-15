#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "value.h"

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    initValueArray(&chunk->constants);
}

void writeChunk(Chunk* chunk, uint8_t byte, SourceSpan span) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);

        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->spans = GROW_ARRAY(SourceSpan, chunk->spans, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->spans[chunk->count++] = span;

    printf("%d\n", chunk->count);
}

int addConstant(Chunk *chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

void writeU16(Chunk *chunk, int index, SourceSpan span) {
    uint16_t pos = (uint16_t) index;

    uint8_t low = (pos & 0xFF);
    uint8_t high = ((pos >> 8) & 0xFF);

    writeChunk(chunk, low, span);
    writeChunk(chunk, high, span);
}

uint16_t readU16(uint8_t low, uint8_t high) {
    return ((uint16_t) high << 8) | low;
}

void freeChunk(Chunk *chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}