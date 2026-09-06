#ifndef STRINGPOOL_H
#define STRINGPOOL_H

#include <stdlib.h>
#include <stdint.h>

typedef struct  {
    uint32_t hash;
    char *lexeme;
    int len;
} Node;

typedef struct {
    Node *buckets;
    size_t items;
    size_t capacity;
} Pool;

Pool initPool();
char *insert_return_ptr_to_string(Pool *pool, const char *key, int len);
void free_pool(Pool *pool);

#endif