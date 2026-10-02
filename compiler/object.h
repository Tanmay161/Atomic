#ifndef OBJECT_H
#define OBJECT_H

#define OBJ_TYPE(value) (value.obj->type)

typedef struct VM VM;
#include "chunk.h"

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
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

ObjString *allocateString(VM *vm, char *lexeme, int len);
ObjFunction *newFunction(VM *vm);

#endif