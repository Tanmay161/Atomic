#ifndef OBJECT_H
#define OBJECT_H

#include "chunk.h"


#define OBJ_TYPE(value) (value.obj->type)

typedef struct VM VM;
typedef Value (*NativeFn) (int argCount, Value *args);

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
} ObjType;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct Obj {
    ObjType type;
    struct Obj *next;
} Obj;

typedef struct {
    Obj obj;
    int len;
    char *lexeme;
} ObjString;

typedef struct {
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString *name;
} ObjFunction;

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

ObjString *allocateString(VM *vm, char *lexeme, int len);
ObjFunction *newFunction(VM *vm);
ObjNative *newNative(VM *vm, NativeFn function);

#endif