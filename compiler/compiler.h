#ifndef COMPILER_H

#include "ast.h"
#include "chunk.h"

typedef struct {
    Program *source;
    SourceSpan currentSpan;
    Chunk *currentChunk;
} Compiler;

Compiler *init_compiler(Program *source);
void free_compiler(Compiler *compiler);

int compile(Compiler *compiler, Program *source);

#endif