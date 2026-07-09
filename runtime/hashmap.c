#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "hashmap.h"

// Macros
#define FNV_PRIME_32 16777619U
#define FNV_OFFSET_BASIS_32 2166136261U
#define LOAD_FACTOR 0.75

// FNV-1a algorithm
uint32_t get_hash(const unsigned char *data, size_t len)
{
    uint32_t hash = FNV_OFFSET_BASIS_32;

    for (size_t i = 0; i < len; i++)
    {
        hash ^= (uint32_t)data[i];
        hash *= FNV_PRIME_32;
    }

    return hash;
}

// Helper
Node **constructBuckets(size_t capacity)
{
    // Assign buckets
    Node **buckets = calloc(capacity, sizeof(Node *));
    if (!buckets)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for string intern pool\n");
        exit(110);
    }

    return buckets;
}

// Initiation
Hashmap *init_hashmap(size_t capacity)
{
    Hashmap *pool = malloc(sizeof(Hashmap));

    if (!pool)
    {
        fprintf(stderr, "MemoryError: Unable to allocate memory for pool.\n");
        exit(110);
    }

    pool->capacity = capacity;
    pool->items = 0;
    Node **buckets = constructBuckets(pool->capacity);
    pool->buckets = buckets;

    return pool;
}

void free_hashmap(Hashmap *p)
{
    for (size_t i = 0; i < p->capacity; i++)
    {
        Node *cur = p->buckets[i];

        while (cur != NULL)
        {
            Node *next = cur->next;
            free(cur->identifier);
            free(cur);

            cur = next;
        }
    }

    free(p->buckets);
    free(p);
}

// Insert or return the pointer to the string
void put(Hashmap *p, char *identifier, Expression *initializer, size_t len)
{
    // The table is 'full', so we reconstruct it and double the size
    // By full, I actually mean the performance will take a hit as the linked lists start getting larger
    if (p->items >= LOAD_FACTOR * p->capacity)
    {
        // Double the capacity, store the old capacity
        size_t old_capacity = p->capacity;
        p->capacity *= 2;
        Node **buckets = constructBuckets(p->capacity);

        for (size_t i = 0; i < old_capacity; i++)
        {
            Node *bucket = p->buckets[i];
            Node *cur = bucket;

            while (cur != NULL)
            {
                Node *next = cur->next;

                size_t index = cur->hash & (p->capacity - 1);

                cur->next = buckets[index];
                buckets[index] = cur;

                cur = next;
            }
        }

        free(p->buckets);
        p->buckets = buckets;
    }
    // Get index of the bucket to jump to
    uint32_t hash = get_hash((const unsigned char *)identifier, len);
    // We use bitwise AND as it is faster than modulo (%)
    size_t index = hash & (p->capacity - 1);

    Node *bucket = p->buckets[index];
    Node *cur = bucket;

    // Traverse linked list
    while (cur != NULL)
    {
        if (len == cur->len && (memcmp(cur->identifier, identifier, len) == 0))
        {
            cur->initializer = initializer;
            return;
        }
        else
            cur = cur->next;
    }

    // Create the new node once we realize that the string is unique
    Node *newNode = malloc(sizeof(Node));
    if (!newNode)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for string intern pool\n");
        exit(110);
    }

    newNode->next = NULL;
    newNode->identifier = malloc(len + 1);

    if (!newNode->identifier)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for identifier\n");
        exit(110);
    }

    memcpy(newNode->identifier, identifier, len);
    newNode->identifier[len] = '\0';
    newNode->len = len;
    newNode->hash = hash;

    newNode->initializer = initializer;

    // Make the new node the head of the linkedlist
    // Making it the tail would be O(n), whereas making it the head is O(1)
    newNode->next = p->buckets[index];
    p->buckets[index] = newNode;
    p->items++;
}

Expression *get(Hashmap *p, char *identifier, size_t len)
{
    uint32_t hash = get_hash((const unsigned char *)identifier, len);
    // We use bitwise AND as it is faster than modulo (%)
    size_t index = hash & (p->capacity - 1);

    Node *bucket = p->buckets[index];
    Node *cur = bucket;

    // Traverse linked list
    while (cur != NULL)
    {
        if (len == cur->len && (memcmp(cur->identifier, identifier, len) == 0))
        {
            return cur->initializer;
        }

        cur = cur->next;
    }

    return NULL;
}