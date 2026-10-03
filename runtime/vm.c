#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "error.h"
#include "vm.h"
#include "debug.h"
#include "memory.h"
#include "value.h"
#include "ast.h"
#include "stringPool.h"
#include "object.h"
#include "stringPool.h"
#include "hashmap.h"

#define STRING_CONCAT_MAX_STACK_LIMIT 4096

// #define DEBUG_TRACE_EXECUTION
#define GET_COUNT(VM) ((size_t)((VM)->stackTop - (VM)->stack->values))

#define IS_OBJ(value) (value.type == VAL_OBJ)
#define IS_NUMERIC(value) (value.type == VAL_FLOAT || value.type == VAL_INT)
#define IS_BOOL(value) (value.type == VAL_BOOL)
#define IS_NIL(value) (value.type == VAL_NIL)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)

static inline int isObjType(Value value, ObjType type)
{
    return IS_OBJ(value) && value.obj->type == type;
}

#define IS_STRING(value) (isObjType(value, OBJ_STRING))
#define AS_STRING(value) ((ObjString *)value.obj)
#define AS_FUNCTION(value) ((ObjFunction *)value.obj)
#define AS_NATIVE(value) \
    (((ObjNative *) value.obj)->function)

#define BOOL_VAL(value) ((Value){.type = VAL_BOOL, .bool_val = value})
#define OBJ_VAL(object) ((Value){.type = VAL_OBJ, .obj = (Obj *)object})

#define NIL_VAL ((Value){.type = VAL_NIL})

#define IS_FALSEY(value) (IS_NIL(value) || (IS_BOOL(value) && !(value).bool_val))
#define IS_TRUTHY(value) (!IS_FALSEY(value))

static void printObject(Value value)
{
    Obj *obj = value.obj;
    switch (obj->type)
    {
    case OBJ_STRING:
    {
        ObjString *string = AS_STRING(value);
        printf("String: '%.*s'", string->len, string->lexeme);
        break;
    }
    case OBJ_FUNCTION:
    {
        ObjFunction *function = AS_FUNCTION(value);

        if (function->name == NULL)
        {
            printf("<script>");
            return;
        }

        printf("<Function: %.*s>", function->name->len, function->name->lexeme);
        break;
    }
    case OBJ_NATIVE:
        printf("<Native function>");
        break;
    }
}

static void printObjectWithoutType(Value value)
{
    Obj *obj = value.obj;
    switch (obj->type)
    {
    case OBJ_STRING:
    {
        ObjString *string = AS_STRING(value);
        printf("%.*s", string->len, string->lexeme);
        break;
    }
    case OBJ_FUNCTION:
    {
        ObjFunction *function = AS_FUNCTION(value);

        if (function->name == NULL)
        {
            printf("<script>");
            return;
        }

        printf("<Function: %.*s>", function->name->len, function->name->lexeme);
        break;
    }
    case OBJ_NATIVE:
        printf("<Native function>");
        break;
    }
}

static void printValue(Value value)
{
    switch (value.type)
    {
    case VAL_INT:
        printf("Integer: '%lld'", value.int_val);
        break;
    case VAL_FLOAT:
        printf("Float: '%g'", value.float_val);
        break;
    case VAL_OBJ:
        printObject(value);
        break;
    case VAL_BOOL:
        printf("Boolean: ");
        printf((value.bool_val == 1) ? "true" : "false");
        break;
    case VAL_NIL:
        printf("nil");
        break;
    }
}

static void printValueWithoutType(Value value)
{
    switch (value.type)
    {
    case VAL_INT:
        printf("%lld", value.int_val);
        break;
    case VAL_FLOAT:
        printf("%g", value.float_val);
        break;
    case VAL_OBJ:
        printObjectWithoutType(value);
        break;
    case VAL_BOOL:
        printf((value.bool_val == 1) ? "true" : "false");
        break;
    case VAL_NIL:
        printf("nil");
        break;
    }
}

static void objToString(Value value, char *buffer, size_t size)
{
    Obj *obj = value.obj;

    switch (obj->type)
    {
    case OBJ_STRING:
    {
        ObjString *string = AS_STRING(value);
        snprintf(buffer, size, "String: '%.*s'", string->len, string->lexeme);
        break;
    }
    case OBJ_FUNCTION:
    {
        ObjFunction *function = AS_FUNCTION(value);
        ObjString *string = function->name;

        snprintf(buffer, size, "<Function: %.*s>", string->len, string->lexeme);
        break;
    }
    case OBJ_NATIVE: {
        snprintf(buffer, size, "<Native function>");
    }
    }
}

static void valueAsString(Value value, char *buffer, size_t size)
{
    switch (value.type)
    {
    case VAL_INT:
        snprintf(buffer, size, "Integer: %lld", value.int_val);
        break;
    case VAL_FLOAT:
        snprintf(buffer, size, "Float: %g", value.float_val);
        break;
    case VAL_OBJ:
        return objToString(value, buffer, size);
    case VAL_BOOL:
        snprintf(buffer, size, "Boolean: %s", (value.bool_val == 1) ? "true" : "false");
        break;
    case VAL_NIL:
        snprintf(buffer, size, "nil");
        break;
    }
}

static void runtimeError(VM *vm, int code, const char *message, ...)
{
    va_list args;
    va_start(args, message);

    fprintf(stderr, "\n");
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n\n");

    CallFrame *outer = &vm->frames[vm->frameCount - 1];
    ObjFunction *offending = outer->function;

    SourceSpan outerSpan = offending->chunk.spans[outer->ip - offending->chunk.code - 1];

    fprintf(stderr, "The error occurred here:\n");
    fprintf(stderr, "   ");

    if (offending->name == NULL)
    {
        fprintf(stderr, "script");
    }
    else
    {
        fprintf(stderr, "%.*s()", offending->name->len, offending->name->lexeme);
    }
    fprintf(stderr, " - line %d, column %d\n\n", outerSpan.startline, outerSpan.startcol);

    fprintf(stderr, "Call stack:\n");
    for (int i = vm->frameCount - 2; i >= 0; i--)
    {
        CallFrame *frame = &vm->frames[i];
        ObjFunction *function = frame->function;
        size_t instruction = frame->ip - function->chunk.code - 1;

        SourceSpan span = function->chunk.spans[instruction];

        fprintf(stderr, "   ");
        if (function->name == NULL)
        {
            fprintf(stderr, "%-8s", "script");
        }
        else
        {
            char name[function->name->len + 3];
            snprintf(name, sizeof(name), "%.*s()", function->name->len, function->name->lexeme);

            fprintf(stderr, "%-8s", name);
        }
        fprintf(stderr, " - called from line %d, column %d\n", span.startline, span.startcol);
    }

    fprintf(stderr, "\n");
    exit(code);
}

static int objsEqual(Value a, Value b)
{
    if (a.obj->type != b.obj->type)
        return 0;

    switch (a.obj->type)
    {
    case OBJ_STRING:
        return AS_STRING(a)->lexeme == AS_STRING(b)->lexeme;
    }

    return 0;
}

static int valuesEqual(Value a, Value b)
{
    if (IS_NUMERIC(a) && IS_NUMERIC(b))
    {
        double a_val = (a.type == VAL_FLOAT) ? a.float_val : (double)a.int_val;
        double b_val = (b.type == VAL_FLOAT) ? b.float_val : (double)b.int_val;

        return a_val == b_val;
    }

    if (a.type != b.type)
        return 0;
    switch (a.type)
    {
    case VAL_BOOL:
        return a.bool_val == b.bool_val;
    case VAL_NIL:
        return 1;
    case VAL_OBJ:
        return objsEqual(a, b);
    default:
        return 0;
    }
}

static void concatenate(VM *vm)
{
    ObjString *b = AS_STRING(pop(vm));
    ObjString *a = AS_STRING(pop(vm));

    int final_len = a->len + b->len;

    ObjString *final;
    if (final_len <= STRING_CONCAT_MAX_STACK_LIMIT)
    {
        char concat[final_len];

        memcpy(concat, a->lexeme, a->len);
        memcpy(concat + a->len, b->lexeme, b->len);
        final = allocateString(vm, concat, final_len);
    }
    else
    {
        char *concat = malloc(final_len);
        if (!concat)
            runtimeError(vm, 501, "MemoryError: Unable to allocate memory for string");

        memcpy(concat, a->lexeme, a->len);
        memcpy(concat + a->len, b->lexeme, b->len);

        final = allocateString(vm, concat, final_len);
        free(concat);
    }

    push(vm, OBJ_VAL(final));
}

static Value clockNative(int argCount, Value *args) {
    return (Value) {.type = VAL_FLOAT, .float_val = (double)clock() / CLOCKS_PER_SEC};
}

static Value printNative(int argCount, Value *args) {
    printValueWithoutType(args[0]);
    printf("\n");
    
    return (Value) {.type = VAL_NIL};
}

static void defineNative(VM *vm, char *name, int len, NativeFn function) {
    push(vm, OBJ_VAL(allocateString(vm, name, len)));
    push(vm, OBJ_VAL(newNative(vm, function)));

    map_set(&vm->globals, AS_STRING(vm->stack->values[0])->lexeme, AS_STRING(vm->stack->values[0])->len, vm->stack->values[1]);
    pop(vm);
    pop(vm);
}

static int call(VM *vm, ObjFunction *function, int argCount, SourceSpan span)
{
    if (argCount != function->arity)
    {
        runtimeError(vm, 505, "Runtime Error: Expected %d arguments, got %d", function->arity, argCount);
    }
    if (vm->frameCount == FRAMES_MAX)
        runtimeError(vm, 506, "Runtime Error: Stack Overflow");

    CallFrame *frame = &vm->frames[vm->frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;
    frame->slots = (size_t)(vm->stackTop - vm->stack->values - argCount - 1);

    return 1;
}

static int callValue(VM *vm, Value callee, int argCount, SourceSpan span)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
        case OBJ_FUNCTION:
            return call(vm, AS_FUNCTION(callee), argCount, span);
        case OBJ_NATIVE: {
            NativeFn native = AS_NATIVE(callee);
            Value result = native(argCount, vm->stackTop - argCount);
            vm->stackTop -= argCount + 1;
            push(vm, result);
            return 1;
        }
        default:
            break;
        }
    }

    char buffer[1024];
    valueAsString(callee, buffer, 1024);
    runtimeError(vm, 505, "Runtime Error: Can only call functions, instead got %s", buffer);
    return 0;
}

VM *initVM()
{
    printf("initiate vm...\n");
    VM *vm = malloc(sizeof(VM));
    if (!vm)
        runtimeError(vm, 500, "MemoryError: Unable to allocate memory for virtual machine");

    ValueStack *stack = malloc(sizeof(ValueStack));
    if (!stack)
        runtimeError(vm, 500, "MemoryError: Unable to allocate memory for value stack");

    Value *values = malloc(sizeof(Value) * 64);
    if (!values)
        runtimeError(vm, 500, "MemoryError: Unable to allocate memory for value stack");

    vm->stack = stack;
    vm->stack->values = values;
    vm->stackTop = vm->stack->values;
    vm->stack->capacity = 64;
    vm->objs = NULL;

    vm->strings = initPool();
    vm->globals = initMap();

    vm->frameCount = 0;

    defineNative(vm, "clock", 5, clockNative);
    defineNative(vm, "print", 5, printNative);
    return vm;
}

static InterpretResult run(VM *vm)
{
    printf("VM initiate run...\n\n");
    CallFrame *frame = &vm->frames[vm->frameCount - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_U16()            \
    ((uint16_t)READ_BYTE()) | \
        ((uint16_t)READ_BYTE() << 8)
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_U16()])
#define GET_INDEX() (frame->ip - frame->function->chunk.code - 1)

#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(op)                                                                                                                          \
    do                                                                                                                                         \
    {                                                                                                                                          \
        Value b = pop(vm);                                                                                                                     \
        Value a = pop(vm);                                                                                                                     \
        SourceSpan span = frame->function->chunk.spans[GET_INDEX()];                                                                           \
                                                                                                                                               \
        if (!IS_NUMERIC(a) || !IS_NUMERIC(b))                                                                                                  \
        {                                                                                                                                      \
            char buffer_a[1024];                                                                                                               \
            char buffer_b[1024];                                                                                                               \
            valueAsString(a, buffer_a, 1024);                                                                                                  \
            valueAsString(b, buffer_b, 1024);                                                                                                  \
            runtimeError(vm, 501, "Runtime Error: Operands of operator '%s' must be numeric, instead got %s and %s", #op, buffer_a, buffer_b); \
        }                                                                                                                                      \
        else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT)                                                                                   \
        {                                                                                                                                      \
            double val_a = (a.type == VAL_FLOAT) ? a.float_val : (double)a.int_val;                                                            \
            double val_b = (b.type == VAL_FLOAT) ? b.float_val : (double)b.int_val;                                                            \
                                                                                                                                               \
            if (#op[0] == '>' || #op[0] == '<')                                                                                                \
            {                                                                                                                                  \
                push(vm, BOOL_VAL(val_a op val_b));                                                                                            \
                break;                                                                                                                         \
            }                                                                                                                                  \
            else                                                                                                                               \
                push(vm, (Value){.type = VAL_FLOAT, .float_val = val_a op val_b});                                                             \
            break;                                                                                                                             \
        }                                                                                                                                      \
        else                                                                                                                                   \
        {                                                                                                                                      \
            if (#op[0] == '>' || #op[0] == '<' || (#op[0] == '>' && #op[1] == '=') || (#op[0] == '<' && #op[1] == '='))                        \
            {                                                                                                                                  \
                push(vm, BOOL_VAL(a.int_val op b.int_val));                                                                                    \
                break;                                                                                                                         \
            }                                                                                                                                  \
            else if (#op[0] == '/')                                                                                                            \
            {                                                                                                                                  \
                if (b.int_val == 0)                                                                                                            \
                    runtimeError(vm, 504, "Runtime Error: Division by 0");                                                                     \
                push(vm, (Value){.type = VAL_FLOAT, .float_val = (double)a.int_val op(double) b.int_val});                                     \
            }                                                                                                                                  \
            else                                                                                                                               \
                push(vm, (Value){.type = VAL_INT, .int_val = a.int_val op b.int_val});                                                         \
        }                                                                                                                                      \
    } while (0)

    for (;;)
    { // can be replaced with while (1)
#ifdef DEBUG_TRACE_EXECUTION
        printf("       ");

        if (GET_COUNT(vm) == 0)
            printf("[]");

        for (Value *slot = vm->stack->values; slot < vm->stackTop; slot++)
        {
            printf("[");
            printValue(*slot);
            printf("]");
        }

        printf("\n");
        FILE *output = fopen("./compiler/result.abc", "a");
        // disassembleInstruction(vm->chunk, output, (int) (vm->ip - vm->chunk->code));
#endif
        uint8_t instruction = READ_BYTE();
        SourceSpan span = frame->function->chunk.spans[GET_INDEX()];

        // printf("IP: %ld\n", vm->ip - vm->chunk->code);
        // printf("OPCODE: %d\n", instruction);

        switch (instruction)
        {
        case OP_DEFINE_GLOBAL:
        {
            ObjString *name = READ_STRING();
            map_set(&vm->globals, name->lexeme, name->len, vm->stackTop[-1]);
            pop(vm);
            break;
        }
        case OP_GET_GLOBAL:
        {
            ObjString *name = READ_STRING();
            Value *val = map_get(&vm->globals, name->lexeme, name->len);

            if (!val)
            {
                runtimeError(vm, 502, "Runtime Error: Undefined variable '%.*s'", name->len, name->lexeme);
            }

            push(vm, *val);
            break;
        }
        case OP_SET_GLOBAL:
        {
            ObjString *name = READ_STRING();

            if (map_set(&vm->globals, name->lexeme, name->len, vm->stackTop[-1]))
            {
                map_remove(&vm->globals, name->lexeme, name->len);
                runtimeError(vm, 502, "Runtime Error: Undefined variable '%.*s'", name->len, name->lexeme);
            }
            break;
        }
        case OP_GET_LOCAL:
        {
            uint16_t slot = READ_U16();
            push(vm, vm->stack->values[frame->slots + slot + 1]);
            break;
        }
        case OP_SET_LOCAL:
        {
            uint16_t slot = READ_U16();
            vm->stack->values[frame->slots + slot + 1] = vm->stackTop[-1];
            break;
        }
        case OP_JUMP_IF_FALSE:
        {
            uint16_t offset = READ_U16();
            if (IS_FALSEY(vm->stackTop[-1]))
                frame->ip += offset;
            break;
        }
        case OP_JUMP_IF_TRUE:
        {
            uint16_t offset = READ_U16();
            if (IS_TRUTHY(vm->stackTop[-1]))
                frame->ip += offset;
            break;
        }
        case OP_JUMP:
        {
            uint16_t offset = READ_U16();
            frame->ip += offset;
            break;
        }
        case OP_LOOP:
        {
            uint16_t offset = READ_U16();
            frame->ip -= offset;
            break;
        }
        case OP_NEGATE:
        {
            Value top = vm->stackTop[-1];

            if (!IS_NUMERIC(top))
            {
                char buffer[1024];
                valueAsString(top, buffer, 1024);
                runtimeError(vm, 501, "Runtime Error: Operand of operator '-' must be numeric, instead got %s", buffer);
            }

            if (top.type == VAL_INT)
                top.int_val = -top.int_val;
            else
                top.float_val = -top.float_val;

            vm->stackTop[-1] = top;
            break;
        }

        case OP_NOT:
            vm->stackTop[-1] = BOOL_VAL(IS_FALSEY(vm->stackTop[-1]));
            break;

        case OP_ADD:
        {
            Value b = vm->stackTop[-1];
            Value a = vm->stackTop[-2];

            if (IS_STRING(a) && IS_STRING(b))
                concatenate(vm);
            else
                BINARY_OP(+);
            break;
        }
        case OP_SUBTRACT:
            BINARY_OP(-);
            break;
        case OP_MULTIPLY:
            BINARY_OP(*);
            break;
        case OP_DIVIDE:
            BINARY_OP(/);
            break;
        case OP_GREATER:
            BINARY_OP(>);
            break;
        case OP_GREATER_EQUAL:
            BINARY_OP(>=);
            break;
        case OP_LESS:
            BINARY_OP(<);
            break;
        case OP_LESS_EQUAL:
            BINARY_OP(<=);
            break;
        case OP_MOD:
        {
            Value b = pop(vm);
            Value a = pop(vm);

            if (a.type != VAL_INT && a.type != VAL_FLOAT && b.type != VAL_INT && b.type != VAL_FLOAT)
                runtimeError(vm, 501, "Runtime Error: Operands of '%%' operator must be numeric");

            double val_a = (a.type == VAL_FLOAT) ? a.float_val : (double)a.int_val;
            double val_b = (b.type == VAL_FLOAT) ? b.float_val : (double)b.int_val;

            push(vm, (Value){.type = VAL_FLOAT, .float_val = fmod(val_a, val_b)});
            break;
        }

        case OP_CONSTANT:
        {
            Value constant = READ_CONSTANT();
            push(vm, constant);

            break;
        }

        case OP_TRUE:
            push(vm, BOOL_VAL(1));
            break;

        case OP_FALSE:
            push(vm, BOOL_VAL(0));
            break;

        case OP_NIL:
            push(vm, NIL_VAL);
            break;

        case OP_EQUAL:
        {
            Value b = pop(vm);
            Value a = pop(vm);
            push(vm, BOOL_VAL(valuesEqual(a, b)));
            break;
        }

        case OP_NOT_EQUAL:
        {
            Value b = pop(vm);
            Value a = pop(vm);
            push(vm, BOOL_VAL(!valuesEqual(a, b)));
            break;
        }

        case OP_POP:
            pop(vm);
            break;

        case OP_POPN:
        {
            uint16_t count = READ_U16();
            vm->stackTop = &vm->stackTop[-count];
            break;
        }
        case OP_CALL:
        {
            int argCount = READ_U16();
            if (!callValue(vm, vm->stackTop[-argCount - 1], argCount, span))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm->frames[vm->frameCount - 1];
            break;
        }
        case OP_RETURN:
        {
            Value result = pop(vm);

            if (--vm->frameCount == 0)
            {
                pop(vm);
                printf("       []\n");
                return INTERPRET_OK;
            }

            vm->stackTop = &vm->stack->values[frame->slots];
            push(vm, result);
            frame = &vm->frames[vm->frameCount - 1];
            break;
        }
        }
    }

#undef READ_BYTE
#undef READ_U16
#undef READ_CONSTANT
#undef GET_INDEX
#undef BINARY_OP
#undef READ_STRING
}

InterpretResult interpret(VM *vm, ObjFunction *function)
{
    push(vm, OBJ_VAL(function));
    SourceSpan sourceSpan;
    sourceSpan.startline = function->chunk.spans[0].startline;
    sourceSpan.startcol = function->chunk.spans[0].startcol;
    sourceSpan.endline = function->chunk.spans[function->chunk.count - 1].endline;
    sourceSpan.endcol = function->chunk.spans[function->chunk.count - 1].endcol;

    call(vm, function, 0, sourceSpan);

    return run(vm);
}

void push(VM *vm, Value value)
{
    size_t count = GET_COUNT(vm);

    if (count + 1 > vm->stack->capacity)
    {
        int oldcap = vm->stack->capacity;
        vm->stack->capacity = GROW_CAPACITY(oldcap);

        Value *temp = realloc(vm->stack->values, sizeof(Value) * vm->stack->capacity);
        if (!temp)
            runtimeError(vm, 500, "MemoryError: Unable to allocate memory for value stack");

        vm->stack->values = temp;
        vm->stackTop = &vm->stack->values[count];
    }

    *vm->stackTop++ = value;
}

Value pop(VM *vm)
{
    return *--vm->stackTop;
}

static void freeObj(Obj *obj)
{
    switch (obj->type)
    {
    case OBJ_STRING:
    {
        ObjString *string = (ObjString *)obj;
        free(string);
        break;
    }
    case OBJ_FUNCTION:
    {
        ObjFunction *function = (ObjFunction *)obj;
        free(function);
        break;
    }
    case OBJ_NATIVE:
    {
        ObjNative *native = (ObjNative *)obj;
        free(native);
        break;
    }
    }
}

static void freeObjs(VM *vm)
{
    Obj *object = vm->objs;
    if (object == NULL)
        return;

    while (object->next != NULL)
    {
        Obj *next = object->next;

        freeObj(object);
        object = next;
    }
}

void freeVM(VM *vm)
{
    freeObjs(vm);
    free(vm->stack->values);
    free(vm->stack);
    free_pool(&vm->strings);
    free_map(&vm->globals);

    free(vm);
}