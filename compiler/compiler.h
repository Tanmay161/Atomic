#ifndef COMPILER_H

#include "ast.h"
#include "chunk.h"
#include "vm.h"

#define UINT16_COUNT (UINT16_MAX + 1)

typedef struct {
    Token name;
    int depth;
} Local;

typedef struct {
    Local locals[UINT16_COUNT];
    int localCount;
    int scopeDepth;
    Program *source;
    Chunk *currentChunk;
    VM *vm;
} Compiler;

Compiler *init_compiler(VM *vm, Program *source);
void free_compiler(Compiler *compiler);

Chunk *compile(Compiler *compiler);

#endif