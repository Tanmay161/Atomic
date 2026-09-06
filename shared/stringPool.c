#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "stringPool.h"
#include "error.h"
#include "memory.h"
#include "object.h"

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR 0.75

static Node *constructBuckets(size_t capacity);

Pool initPool() {
    Pool pool = {.items = 0, .capacity = INITIAL_CAPACITY};
    pool.buckets = constructBuckets(INITIAL_CAPACITY);

    return pool;
}

static uint32_t compute_hash(const char *string, int len)
{
    uint32_t hash = 2166136261u;

    for (int i = 0; i < len; i++)
    {
        hash ^= string[i];
        hash *= 16777619;
    }

    return hash;
}

static Node *constructBuckets(size_t capacity)
{
    Node *buckets = calloc(capacity, sizeof(Node));

    if (!buckets)
        error_report(1, "MemoryError: Failed to initialize buckets for intern table");

    return buckets;
}

static char *copyString(const char *key, int len)
{
    char *chars = malloc(len);
    if (!chars)
        error_report(1, "MemoryError: Failed to allocate memory for string");

    memcpy(chars, key, (size_t)len);
    return chars;
}

static Node *findBucket(Node *buckets, int capacity, const char *key, int len, uint32_t hash)
{
    uint32_t index = hash & (capacity - 1);

    for (;;)
    {
        Node *bucket = &buckets[index];

        if (bucket->lexeme == NULL)
        {
            return bucket;
        }

        if (bucket->hash == hash && bucket->len == len && (memcmp(bucket->lexeme, key, len) == 0))
            return bucket;

        index = (index + 1) & (capacity - 1);
    }
}

static void adjustBuckets(Pool *pool, int capacity)
{
    Node *buckets = constructBuckets(capacity);

    for (int i = 0; i < pool->capacity; i++)
    {
        Node *bucket = &pool->buckets[i];
        if (bucket->lexeme == NULL)
            continue;

        Node *new_bucket = findBucket(buckets, capacity, bucket->lexeme, bucket->len, bucket->hash);
        new_bucket->hash = bucket->hash;
        new_bucket->lexeme = bucket->lexeme;
    }

    FREE_ARRAY(Node, pool->buckets, pool->capacity);
    pool->buckets = buckets;
    pool->capacity = capacity;
}

char *insert_return_ptr_to_string(Pool *pool, const char *key, int len)
{
    if (pool->items + 1 > pool->capacity * LOAD_FACTOR)
    {
        int capacity = GROW_CAPACITY(pool->capacity);
        adjustBuckets(pool, capacity);
    }

    uint32_t hash = compute_hash(key, len);
    Node *bucket = findBucket(pool->buckets, pool->capacity, key, len, hash);

    if (bucket->lexeme == NULL)
    {
        bucket->lexeme = copyString(key, len);
        bucket->len = len;
        bucket->hash = hash;

        pool->items++;
        return bucket->lexeme;
    }
    else
    {
        return bucket->lexeme;
    }
}

void free_pool(Pool *pool) {
    for (int i = 0; i < pool->capacity; i++) {  
        if (pool->buckets[i].lexeme == NULL) continue;
        free(pool->buckets[i].lexeme);
    }

    free(pool->buckets);
}