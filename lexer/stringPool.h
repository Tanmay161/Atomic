#ifndef STRINGPOOL_H

#define STRINGPOOL_H
#include <stdlib.h>

typedef struct Node {
    char *string;
    struct Node *next;
    uint32_t hash;
    size_t len;
} Node;

typedef struct {
    Node **buckets;
    int items;
    size_t capacity;
} Pool;

extern Pool *string_pool;
void init_string_pool();
char *insert_return_ptr_to_string(char *string, size_t len);

#endif