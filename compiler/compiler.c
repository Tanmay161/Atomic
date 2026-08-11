// #include "parser.h"
// #include "ast.h"
// #include "token.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main() {
    VM *vm = initVM();

    Chunk chunk;
    initChunk(&chunk);

    int constant = addConstant(&chunk, 1.2);

    writeChunk(&chunk, OP_CONSTANT); 
    writeU16(&chunk, constant); 

    constant = addConstant(&chunk, 3.4);
    writeChunk(&chunk, OP_CONSTANT);
    writeU16(&chunk, constant);

    writeChunk(&chunk, OP_ADD);

    constant = addConstant(&chunk, 5.6);
    writeChunk(&chunk, OP_CONSTANT);
    writeU16(&chunk, constant);

    writeChunk(&chunk, OP_DIVIDE);
    writeChunk(&chunk, OP_NEGATE);

    writeChunk(&chunk, OP_RETURN);

    disassembleChunk(&chunk);
    InterpretResult result = interpret(vm, &chunk);
    freeVM(vm);
    freeChunk(&chunk);

    return 0;
}