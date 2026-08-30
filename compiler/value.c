#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "value.h"
#include "memory.h"

void initValueArray(ValueArray* array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count++] = value;
}

void freeValueArray(ValueArray *array) {
    FREE_ARRAY(uint8_t, array->values, array->capacity);
    initValueArray(array);
}

void fprintObject(FILE *output, Value value) {
    switch (value.obj->type) {
        case OBJ_STRING: {
            ObjString *string = (ObjString *) value.obj;
            fprintf(output, "%.*s", string->len, string->lexeme);
            break;
        }
    }
}

void fprintValue(FILE *output, Value value) {
    switch (value.type) {
        case VAL_INT: 
            fprintf(output, "%lld", value.int_val);
            break;
        case VAL_FLOAT:
            fprintf(output, "%g", value.float_val);
            break;
        case VAL_NIL:
            fprintf(output, "NIL");
            break;
        case VAL_BOOL:
            if (value.bool_val == 1) fprintf(output, "TRUE"); 
            else fprintf(output, "FALSE");
            break;
        case VAL_OBJ:
            fprintObject(output, value);
            break;
    }
}