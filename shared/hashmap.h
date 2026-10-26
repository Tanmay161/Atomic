#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdlib.h>
#include <stdint.h>

#include "value.h"

typedef struct {
    char *key;
    int len;

    int value;
    uint32_t hash;
} Entry;

typedef struct {
    Entry *entries;
    size_t items;
    size_t capacity;
} Map;

Map initMap();

int map_set(Map *map, char *key, int len, int value);
int map_get(Map *map, char *key, int len);
void map_remove(Map *map, char *key, int len);

void free_map(Map *pool);

#endif