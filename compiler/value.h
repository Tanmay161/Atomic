#ifndef value_h
#define value_h

#include <stdio.h>
#include "stringPool.h"
#include "object.h"

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_OBJ,
    VAL_BOOL,
    VAL_NIL,
} ValueType;

typedef struct {
    ValueType type;

    union {
        long long int_val;
        double float_val;
        Obj *obj;
        int bool_val;
    };
} Value;

typedef struct {
    int capacity;
    int count;
    Value *values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void fprintValue(FILE *output, Value value);

#endif