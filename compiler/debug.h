#ifndef debug_h
#define debug_h

#include <stdio.h>

#include "chunk.h"

void disassembleChunk(Chunk *chunk);
int disassembleInstruction(Chunk *chunk, FILE *output, int offset);
static int simpleInstruction(const char *name, FILE *output, int offset);
static int constantInstruction(const char *name, Chunk *chunk, FILE *output, int offset);

#endif