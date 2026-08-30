#ifndef COMPILER_H

#include "ast.h"
#include "chunk.h"
#include "vm.h"

typedef struct {
    Program *source;
    Chunk *currentChunk;
    VM *vm;
} Compiler;

Compiler *init_compiler(VM *vm, Program *source);
void free_compiler(Compiler *compiler);

Chunk *compile(Compiler *compiler);

#endif