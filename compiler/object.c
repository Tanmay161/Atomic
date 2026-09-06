#include "object.h"
#include "stringPool.h"
#include "memory.h"
#include "vm.h"

#define ALLOCATE_OBJ(vm, type, objType) \
    (type *)allocateObject(vm, sizeof(type), objType)

static Obj* allocateObject(VM *vm, size_t size, ObjType type) {
    Obj *object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm->objs;
    vm->objs = object;

    return object;
}

ObjString *allocateString(VM *vm, char *lexeme, int len) {    
    ObjString *string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    string->len = len;
    string->lexeme = insert_return_ptr_to_string(&vm->strings, lexeme, len);

    return string;
}