#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdlib.h>
#include <stdint.h>

#include "value.h"

typedef struct {
    char *key;
    Value value;
    uint32_t hash;
} Entry;

typedef struct {
    Entry *entries;
    size_t items;
    size_t capacity;
} Map;

Map initMap();

int map_set(Map *map, char *key, int len, Value value);
Value *map_get(Map *map, char *key, int len);
void map_remove(Map *map, char *key, int len);

void free_map(Map *pool);

#endif