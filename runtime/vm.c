#include <stdio.h>
#include <math.h>

#include "shared.h"
#include "vm.h"
#include "debug.h"
#include "memory.h"
#include "value.h"
#include "ast.h"

#define DEBUG_TRACE_EXECUTION
#define GET_COUNT(VM) ((size_t) ((VM)->stackTop - (VM)->stack->values))

static void printValue(Value value) {
    printf("%f", value);
}

VM *initVM() {
    VM *vm = malloc(sizeof(VM));
    if (!vm) 
        error_report(500, "MemoryError: Unable to allocate memory for virtual machine");
    
    vm->chunk = NULL;

    ValueStack *stack = malloc(sizeof(ValueStack));
    if (!stack) 
        error_report(500, "MemoryError: Unable to allocate memory for value stack");
    
    Value *values = malloc(sizeof(Value) * 64);
    if (!values) 
        error_report(500, "MemoryError: Unable to allocate memory for value stack");

    vm->stack = stack;
    vm->stack->values = values;
    vm->stackTop = vm->stack->values;
    vm->stack->capacity = 64;

    return vm;
}

void freeVM(VM *vm) {
    free(vm->stack->values);
    free(vm->stack);

    free(vm);
}

static InterpretResult run(VM *vm) {
    
#define READ_BYTE(vm) (*vm->ip++)
#define READ_U16(vm) \
    ((uint16_t) READ_BYTE(vm)) | \
    ((uint16_t) READ_BYTE(vm) << 8)
#define READ_CONSTANT(vm) (vm->chunk->constants.values[READ_U16(vm)]) 

#define BINARY_OP(vm, op) \
    do { \
        Value b = pop(vm); \
        Value a = pop(vm); \
        push(vm, a op b); \
    } while (0)
    
    for (;;) { // can be replaced with while (1)
    #ifdef DEBUG_TRACE_EXECUTION
        printf("       ");

        for (Value* slot = vm->stack->values; slot < vm->stackTop; slot++) {
            printf("[");
            printValue(*slot);
            printf("]");
        }

        printf("\n");
        FILE *output = fopen("./compiler/result.abc", "a");
        //disassembleInstruction(vm->chunk, output, (int) (vm->ip - vm->chunk->code));
    #endif
        uint8_t instruction = READ_BYTE(vm);
        switch (instruction) {
            case OP_NEGATE: vm->stackTop[-1] = -vm->stackTop[-1]; break;

            case OP_ADD: BINARY_OP(vm, +); break;
            case OP_SUBTRACT: BINARY_OP(vm, -); break;
            case OP_MULTIPLY: BINARY_OP(vm, *); break;
            case OP_DIVIDE: BINARY_OP(vm, /); break;
            case OP_MOD: {
                Value b = pop(vm);
                Value a = pop(vm);

                push(vm, fmod(a, b));
                break;
            }

            case OP_CONSTANT: {
                Value constant = READ_CONSTANT(vm);
                push(vm, constant);

                break;
            }

            case OP_RETURN: {
                printValue(pop(vm));
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_U16
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(VM *vm, Program *program) {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(program, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm->chunk = &chunk;
    vm->ip = vm->chunk->code;

    InterpretResult result = run(vm);

    freeChunk(&chunk);
    return result;
}

void push(VM *vm, Value value) {
    size_t count = GET_COUNT(vm);

    if (count + 1 > vm->stack->capacity) {
        int oldcap = vm->stack->capacity;
        vm->stack->capacity = GROW_CAPACITY(oldcap);

        Value* temp = realloc(vm->stack->values, sizeof(Value) * vm->stack->capacity);
        if (!temp)
            error_report(500, "MemoryError: Unable to allocate memory for value stack");
        
        vm->stack->values = temp;
        vm->stackTop = &vm->stack->values[count];
    }

    *vm->stackTop++ = value;
}

Value pop(VM *vm) {
    return *--vm->stackTop;
}