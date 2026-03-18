#ifndef STRINGPOOL_H

#define STRINGPOOL_H
#include <stdlib.h>

typedef struct {
    char * string;
    size_t len;
} StringObject;

typedef struct Block {
    StringObject *data;
    struct Block *next;
    size_t used;
    size_t capacity;
} Block;

void clean_up();
int is_in_pool();
extern Block string_pool;

#endif