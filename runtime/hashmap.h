#ifndef HASHMAP_H

#define HASHMAP_H
#include <stdlib.h>
#include <stdint.h>
#include "ast.h"

typedef struct Node {
    char *identifier;
    Expression *initializer;

    struct Node *next;
    uint32_t hash;
    size_t len;
} Node;

typedef struct {
    Node **buckets;
    int items;
    size_t capacity;
} Hashmap;

Hashmap *init_hashmap(size_t capacity);
void free_hashmap(Hashmap *p);

void put(Hashmap *p, char *identifier, Expression *initializer, size_t len);
Expression *get(Hashmap *p, char *identifier, size_t len);

#endif