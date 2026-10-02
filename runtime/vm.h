#ifndef vm_h
#define vm_h

#include "chunk.h"
#include "value.h"
#include "stringPool.h"
#include "hashmap.h"
#include "object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * 256)

typedef struct Obj Obj;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

typedef struct {
    ObjFunction *function;
    uint8_t *ip;
    Value *slots;
} CallFrame;

typedef struct {
    Value *values;
    int capacity;
} ValueStack;

typedef struct VM {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    ValueStack *stack;
    Value *stackTop;
    Obj *objs;
    Pool strings;
    Map globals;
} VM;

VM *initVM();
void freeVM(VM *vm);
InterpretResult interpret(VM *vm, ObjFunction *function);

void push(VM *vm, Value value);
Value pop(VM *vm);

#endif