#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "stringPool.h"
#include "error.h"
#include "memory.h"
#include "object.h"

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR 0.75

Pool pool = (Pool){
    .items = 0,
    .capacity = INITIAL_CAPACITY,
    .buckets = NULL};

Pool *string_pool = &pool;

static uint32_t compute_hash(const void *string, int len)
{
    uint32_t hash = 2166136261u;
    const uint8_t *bytes = string;

    for (int i = 0; i < len; i++)
    {
        hash ^= bytes[i];
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

void init_string_pool()
{
    Node *buckets = constructBuckets(string_pool->capacity);
    string_pool->buckets = buckets;
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

static void adjustBuckets(int capacity)
{
    Node *buckets = calloc(capacity, sizeof(Node));
    if (!buckets)
        error_report(1, "MemoryError: Failed to allocate memory for intern table");

    for (int i = 0; i < string_pool->capacity; i++)
    {
        Node *bucket = &string_pool->buckets[i];
        if (bucket->lexeme == NULL)
            continue;

        Node *new_bucket = findBucket(buckets, capacity, bucket->lexeme, bucket->len, bucket->hash);
        new_bucket->hash = bucket->hash;
        new_bucket->lexeme = bucket->lexeme;
    }

    FREE_ARRAY(Node, string_pool->buckets, string_pool->capacity);
    string_pool->buckets = buckets;
    string_pool->capacity = capacity;
}

char *insert_return_ptr_to_string(const char *key, int len)
{
    if (string_pool->items + 1 > string_pool->capacity * LOAD_FACTOR)
    {
        int capacity = GROW_CAPACITY(string_pool->capacity);
        adjustBuckets(capacity);
    }

    uint32_t hash = compute_hash(key, len);
    Node *bucket = findBucket(string_pool->buckets, string_pool->capacity, key, len, hash);

    if (bucket->lexeme == NULL)
    {
        bucket->lexeme = copyString(key, len);
        bucket->len = len;
        bucket->hash = hash;

        string_pool->items++;
        return bucket->lexeme;
    }
    else
    {
        return bucket->lexeme;
    }
}