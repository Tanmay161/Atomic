// #include "parser.h"
// #include "ast.h"
// #include "token.h"
#include "chunk.h"
#include "debug.h"

int main() {
    Chunk chunk;
    initChunk(&chunk);

    int constant = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT);  
    writeU16(&chunk, constant);
    writeChunk(&chunk, OP_RETURN);

    disassembleChunk(&chunk);
    freeChunk(&chunk);

    return 0;
}