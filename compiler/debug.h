#ifndef debug_h
#define debug_h

#include <stdio.h>

#include "chunk.h"

void disassembleChunk(Chunk *chunk);
int disassembleInstruction(Chunk *chunk, FILE *output, int offset);

#endif