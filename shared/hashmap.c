#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "hashmap.h"
#include "error.h"
#include "memory.h"
#include "object.h"
#include "debug.h"

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR 0.75

static Entry *constructEntries(size_t capacity);

Map initMap()
{
    Map map = {.items = 0, .capacity = INITIAL_CAPACITY};
    map.entries = constructEntries(INITIAL_CAPACITY);

    return map;
}

static uint32_t compute_hash(char *string, int len)
{
    uint32_t hash = 2166136261u;

    for (int i = 0; i < len; i++)
    {
        hash ^= string[i];
        hash *= 16777619;
    }

    return hash;
}

static Entry *constructEntries(size_t capacity)
{
    Entry *entries = calloc(capacity, sizeof(Entry));

    if (!entries)
        error_report(1, "MemoryError: Failed to initialize buckets for intern table");
    
    for (int i = 0; i < capacity; i++) {
        entries[i].value = -1;
    }

    return entries;
}

static Entry *findEntry(Entry *entries, int capacity, const char *key, int len, uint32_t hash)
{
    uint32_t index = hash & (capacity - 1);

    for (;;)
    {
        Entry *entry = &entries[index];

        if (entry->key == NULL && entry->value == -1)
        {
            return entry;
        }

        if (entry->hash == hash && entry->len == len && memcmp(entry->key, key, len) == 0)
            return entry;

        index = (index + 1) & (capacity - 1);
    }
}

static Entry *findEntryForSet(Entry *entries, int capacity, const char *key, int len, uint32_t hash)
{
    uint32_t index = hash & (capacity - 1);

    Entry *tombstone = NULL;
    for (;;)
    {
        Entry *entry = &entries[index];

        if (entry->key == NULL)
        {
            if (entry->value != -1)
                tombstone = (tombstone == NULL) ? entry : tombstone;
            else
                return tombstone ? tombstone : entry;
        }

        else if (entry->hash == hash && len == entry->len && memcmp(key, entry->key, len) == 0)
            return entry;

        index = (index + 1) & (capacity - 1);
    }
}

static void adjustBuckets(Map *map, int capacity)
{
    Entry *entries = constructEntries(capacity);

    for (int i = 0; i < map->capacity; i++)
    {
        Entry *entry = &map->entries[i];
        if (entry->key == NULL)
            continue;

        Entry *new_entry = findEntryForSet(entries, capacity, entry->key, entry->len, entry->hash);
        new_entry->hash = entry->hash;
        new_entry->key = entry->key;
        new_entry->value = entry->value;
    }

    FREE_ARRAY(Entry, map->entries, map->capacity);
    map->entries = entries;
    map->capacity = capacity;
}

int map_set(Map *map, char *key, int len, int value)
{
    if (map->items + 1 > map->capacity * LOAD_FACTOR)
    {
        int capacity = GROW_CAPACITY(map->capacity);
        adjustBuckets(map, capacity);
    }

    uint32_t hash = compute_hash(key, len);
    Entry *entry = findEntryForSet(map->entries, map->capacity, key, len, hash);

    int isNewKey = (entry->key == NULL);
    if (isNewKey)
        map->items++;

    entry->hash = hash;
    entry->key = key;
    entry->len = len;
    entry->value = value;

    return isNewKey;
}

int map_get(Map *map, char *key, int len)
{
    uint32_t hash = compute_hash(key, len);
    Entry *entry = findEntry(map->entries, map->capacity, key, len, hash);

    if (entry->key == NULL)
        return -1;

    return entry->value;
}

void map_remove(Map *map, char *key, int len)
{
    uint32_t hash = compute_hash(key, len);
    Entry *entry = findEntry(map->entries, map->capacity, key, len, hash);

    if (entry->key != NULL)
    {
        entry->key = NULL;
        map->items--;
    }
}

void free_map(Map *map)
{
    free(map->entries);
}