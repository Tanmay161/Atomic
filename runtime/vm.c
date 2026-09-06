#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

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

#define DEBUG_TRACE_EXECUTION
#define GET_COUNT(VM) ((size_t)((VM)->stackTop - (VM)->stack->values))

#define IS_OBJ(value) (value.type == VAL_OBJ)
#define IS_NUMERIC(value) (value.type == VAL_FLOAT || value.type == VAL_INT)
#define IS_BOOL(value) (value.type == VAL_BOOL)
#define IS_NIL(value) (value.type == VAL_NIL)

static inline int isObjType(Value value, ObjType type)
{
    return IS_OBJ(value) && value.obj->type == type;
}

#define IS_STRING(value) (isObjType(value, OBJ_STRING))
#define AS_STRING(value) ((ObjString *)value.obj)

#define BOOL_VAL(value) ((Value){.type = VAL_BOOL, .bool_val = value})
#define OBJ_VAL(objstring) ((Value){.type = VAL_OBJ, .obj = (Obj *)objstring})

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
    }
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

static int objsEqual(Value a, Value b)
{
    if (a.obj->type != b.obj->type)
        return 0;
    switch (a.obj->type)
    {
    case OBJ_STRING:
        return AS_STRING(a)->lexeme == AS_STRING(b)->lexeme;
    }
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
            error_report(501, "MemoryError: Unable to allocate memory for string");

        memcpy(concat, a->lexeme, a->len);
        memcpy(concat + a->len, b->lexeme, b->len);

        final = allocateString(vm, concat, final_len);
        free(concat);
    }

    push(vm, OBJ_VAL(final));
}

VM *initVM()
{
    printf("initiate vm...\n");
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
    vm->objs = NULL;

    vm->strings = initPool();
    vm->globals = initMap();

    return vm;
}

static InterpretResult run(VM *vm)
{
    printf("VM initiate run...\n\n");
#define READ_BYTE(vm) (*vm->ip++)
#define READ_U16(vm)            \
    ((uint16_t)READ_BYTE(vm)) | \
        ((uint16_t)READ_BYTE(vm) << 8)
#define READ_CONSTANT(vm) (vm->chunk->constants.values[READ_U16(vm)])
#define GET_INDEX(vm) (vm->ip - vm->chunk->code - 1)

#define READ_STRING(vm) AS_STRING(READ_CONSTANT(vm))

#define BINARY_OP(vm, op)                                                                                                                           \
    do                                                                                                                                              \
    {                                                                                                                                               \
        Value b = pop(vm);                                                                                                                          \
        Value a = pop(vm);                                                                                                                          \
        SourceSpan span = vm->chunk->spans[GET_INDEX(vm)];                                                                                          \
                                                                                                                                                    \
        if (!IS_NUMERIC(a) || !IS_NUMERIC(b))                                                                                                       \
        {                                                                                                                                           \
            printf("RuntimeError: Line %d column %d\nOperands of operator '%s' must be numeric, instead got ", span.startline, span.startcol, #op); \
            printValue(a);                                                                                                                          \
            printf(" and ");                                                                                                                        \
            printValue(b);                                                                                                                          \
            printf("\n");                                                                                                                           \
            exit(501);                                                                                                                              \
        }                                                                                                                                           \
        else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT)                                                                                        \
        {                                                                                                                                           \
            double val_a = (a.type == VAL_FLOAT) ? a.float_val : (double)a.int_val;                                                                 \
            double val_b = (b.type == VAL_FLOAT) ? b.float_val : (double)b.int_val;                                                                 \
                                                                                                                                                    \
            if (#op[0] == '>' || #op[0] == '<')                                                                                                     \
            {                                                                                                                                       \
                push(vm, BOOL_VAL(val_a op val_b));                                                                                                 \
                break;                                                                                                                              \
            }                                                                                                                                       \
            else                                                                                                                                    \
                push(vm, (Value){.type = VAL_FLOAT, .float_val = val_a op val_b});                                                                  \
            break;                                                                                                                                  \
        }                                                                                                                                           \
        else                                                                                                                                        \
        {                                                                                                                                           \
            if (#op[0] == '>' || #op[0] == '<' || (#op[0] == '>' && #op[1] == '=') || (#op[0] == '<' && #op[1] == '='))                             \
            {                                                                                                                                       \
                push(vm, BOOL_VAL(a.int_val op b.int_val));                                                                                         \
                break;                                                                                                                              \
            }                                                                                                                                       \
            else if (#op[0] == '/')                                                                                                                 \
            {                                                                                                                                       \
                if (b.int_val == 0)                                                                                                                 \
                    error_report(504, "RuntimeError: Line %d column %d\nDivision by 0", span.startline, span.startcol);                             \
                push(vm, (Value){.type = VAL_FLOAT, .float_val = (double)a.int_val op(double) b.int_val});                                          \
            }                                                                                                                                       \
            else                                                                                                                                    \
                push(vm, (Value){.type = VAL_INT, .int_val = a.int_val op b.int_val});                                                              \
        }                                                                                                                                           \
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
        uint8_t instruction = READ_BYTE(vm);
        SourceSpan span = vm->chunk->spans[GET_INDEX(vm)];

        switch (instruction)
        {
        case OP_DEFINE_GLOBAL:
        {
            ObjString *name = READ_STRING(vm);
            map_set(&vm->globals, name->lexeme, name->len, vm->stackTop[-1]);
            pop(vm);
            break;
        }
        case OP_GET_GLOBAL:
        {
            ObjString *name = READ_STRING(vm);
            Value *val = map_get(&vm->globals, name->lexeme, name->len);

            if (!val)
            {
                error_report(502, "RuntimeError: Line %d column %d\nUndefined variable '%.*s'", span.startline, span.startcol, name->len, name->lexeme);
            }

            push(vm, *val);
            break;
        }
        case OP_SET_GLOBAL:
        {
            ObjString *name = READ_STRING(vm);

            if (map_set(&vm->globals, name->lexeme, name->len, vm->stackTop[-1]))
            {
                map_remove(&vm->globals, name->lexeme, name->len);
                error_report(502, "RuntimeError: Line %d column %d\nUndefined variable '%.*s'", span.startline, span.startcol, name->len, name->lexeme);
            }
            break;
        }
        case OP_NEGATE:
        {
            Value top = vm->stackTop[-1];

            if (!IS_NUMERIC(top))
            {
                printf("RuntimeError: Line %d column %d\nOperand of operator '-' must be numeric, instead got ", span.startline, span.startcol);
                printValue(top);
                printf("\n");
                exit(501);
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
                BINARY_OP(vm, +);
            break;
        }
        case OP_SUBTRACT:
            BINARY_OP(vm, -);
            break;
        case OP_MULTIPLY:
            BINARY_OP(vm, *);
            break;
        case OP_DIVIDE:
            BINARY_OP(vm, /);
            break;
        case OP_GREATER:
            BINARY_OP(vm, >);
            break;
        case OP_GREATER_EQUAL:
            BINARY_OP(vm, >=);
            break;
        case OP_LESS:
            BINARY_OP(vm, <);
            break;
        case OP_LESS_EQUAL:
            BINARY_OP(vm, <=);
            break;
        case OP_MOD:
        {
            Value b = pop(vm);
            Value a = pop(vm);

            if (a.type != VAL_INT && a.type != VAL_FLOAT && b.type != VAL_INT && b.type != VAL_FLOAT)
                error_report(501, "RuntimeError: Line %d column %d\nOperands of '%%' operator must be numeric", span.startline, span.startcol);

            double val_a = (a.type == VAL_FLOAT) ? a.float_val : (double)a.int_val;
            double val_b = (b.type == VAL_FLOAT) ? b.float_val : (double)b.int_val;

            push(vm, (Value){.type = VAL_FLOAT, .float_val = fmod(val_a, val_b)});
            break;
        }

        case OP_CONSTANT:
        {
            Value constant = READ_CONSTANT(vm);
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

        case OP_RETURN:
        {
            printValue(pop(vm));
            printf("\n");
            return INTERPRET_OK;
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

InterpretResult interpret(VM *vm, Chunk *chunk)
{
    vm->chunk = chunk;
    vm->ip = vm->chunk->code;

    InterpretResult result = run(vm);

    freeChunk(chunk);
    return result;
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
            error_report(500, "MemoryError: Unable to allocate memory for value stack");

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
    }
}

static void freeObjs(VM *vm)
{
    Obj *object = vm->objs;
    if (object == NULL) return;

    while (object->next != NULL)
    {
        Obj *next = object->next;

        FREE(Obj, object);
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