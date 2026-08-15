// #include "parser.h"
// #include "ast.h"
// #include "token.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"
#include "ast.h"
#include "compiler.h"
#include "shared.h"

int compile(Compiler *compiler, Program *source);
static void emitByte(Compiler *compiler, uint8_t byte);
static void emitBytes(Compiler *compiler, uint8_t byte1, uint8_t byte2);

static void endCompiler(Compiler *compiler);
static void emitReturn(Compiler *compiler);

static void expression(Compiler *compiler);

static void emitByte(Compiler *compiler, uint8_t byte) {
    writeChunk(compiler->currentChunk, byte, compiler->currentSpan);
}

static void emitBytes(Compiler *compiler, uint8_t byte1, uint8_t byte2) {
    emitByte(compiler, byte1);
    emitByte(compiler, byte2);
}

static void emitReturn(Compiler *compiler) {
    emitByte(compiler, OP_RETURN);
}

static void endCompiler(Compiler *compiler) {
    emitReturn(compiler);
}

int compile(Compiler *compiler, Program *source) {
    for (int i = 0; i < source->count; i++) {
        Statement *stmt = source->statements[i];

    }
}

Compiler *init_compiler(Program *source) {
    Compiler *compiler = malloc(sizeof(Compiler));

    if (!compiler) 
        error_report(400, "MemoryError: Failed to initialize compiler");
    
    Chunk chunk;
    initChunk(&chunk);

    compiler->source = source;
    compiler->currentChunk = &chunk;
    
    if (source->count == 0) {
        SourceSpan EmptySource;
        EmptySource.startline = 0;
        EmptySource.endline = 0;
        EmptySource.startcol = 0;
        EmptySource.endcol = 0;

        compiler->currentSpan = EmptySource;
    }
    else {
        compiler->currentSpan = source->statements[source->count - 1]->span;
    }

    return compiler;
}

void free_compiler(Compiler *compiler) {
    free(compiler);
}