#ifndef value_h
#define value_h

#include <stdio.h>

typedef double Value;

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