#ifndef OBJECT_H
#define OBJECT_H

#define OBJ_TYPE(value) (value.obj->type)

typedef struct VM VM;

typedef enum {
    OBJ_STRING,
} ObjType;

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

#endif