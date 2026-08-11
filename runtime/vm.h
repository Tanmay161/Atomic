#ifndef vm_h
#define vm_h

#include "chunk.h"
#include "value.h"

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

typedef struct {
    Value *values;
    int capacity;
} ValueStack;

typedef struct {
    Chunk *chunk;
    uint8_t* ip;
    ValueStack *stack;
    Value *stackTop;
} VM;

VM *initVM();
void freeVM(VM *vm);
InterpretResult interpret(VM *vm, Chunk *chunk);

void push(VM *vm, Value value);
Value pop(VM *vm);

#endif