#include "hashmap.h"

#define LOCALS_MAX (UINT16_MAX + 1)

typedef struct {
    Token name;
    int depth;
    int slot;
} Local;

typedef struct {
    int scope;

    Local localStorage[LOCALS_MAX];
    Local *locals;
    Local *nextLocal;

    Map globals_map;
    int globalCount;
} Resolver;

Resolver initResolver();
int resolve(Resolver *resolver, Program *program);