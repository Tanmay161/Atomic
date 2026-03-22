#include "stringPool.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Macros
#define FNV_PRIME_32 16777619U
#define FNV_OFFSET_BASIS_32 2166136261U
#define LOAD_FACTOR 0.75

// Base pool
Pool pool = (Pool){.items = 0, .capacity = 64};
Pool *string_pool = &pool;

// FNV-1a algorithm
uint32_t get_hash(const unsigned char* data, size_t len) {
    uint32_t hash = FNV_OFFSET_BASIS_32;

    for (size_t i = 0; i < len; i++) {
        hash ^= (uint32_t)data[i];
        hash *= FNV_PRIME_32;
    }

    return hash;
}

// Helper
Node ** constructBuckets(size_t capacity) {
    // Assign buckets
    Node **buckets = calloc(capacity, sizeof(Node*));
    if (!buckets) {
        fprintf(stderr, "MemoryError: Failed to allocate memory for string intern pool\n");
        exit(110);
    }

    return buckets;
}

// Initiation
void init_string_pool() {
    Node **buckets = constructBuckets(string_pool->capacity);
    string_pool->buckets = buckets;
}

// Insert or return the pointer to the string
char *insert_return_ptr_to_string(char *string, size_t len) {
    // The table is 'full', so we reconstruct it and double the size
    // By full, I actually mean the performance will take a hit as the linked lists start getting larger
    if (string_pool->items >= LOAD_FACTOR * string_pool->capacity) {
        // Double the capacity, store the old capacity
        size_t old_capacity = string_pool->capacity;
        string_pool -> capacity *= 2;
        Node **buckets = constructBuckets(string_pool->capacity);

        for (size_t i = 0; i < old_capacity; i++) {
            Node *bucket = string_pool->buckets[i];
            Node *cur = bucket;

            while (cur != NULL) {
                Node *next = cur->next;

                size_t index = cur->hash & (string_pool->capacity - 1);

                cur->next = buckets[index];
                buckets[index] = cur;

                cur = next;
            }
        }

        free(string_pool->buckets);
        string_pool->buckets = buckets;
    }
    // Get index of the bucket to jump to
    uint32_t hash = get_hash((const unsigned char*) string, len);
    // We use bitwise AND as it is faster than modulo (%)
    size_t index = hash & (string_pool->capacity - 1);

    Node *bucket = string_pool->buckets[index];
    Node *cur = bucket;

    // Traverse linked list
    while (cur != NULL) {
        if (len == cur->len && (memcmp(cur->string, string, len) == 0)) {
            return cur->string;
        }

        cur = cur->next;
    }

    // Create the new node once we realize that the string is unique
    Node *newNode = malloc(sizeof(Node));
    if (!newNode) {
        fprintf(stderr, "MemoryError: Failed to allocate memory for string intern pool\n");
        exit(110);
    }

    newNode->next = NULL;
    newNode->string = malloc(len + 1);
    memcpy(newNode->string, string, len);
    newNode->string[len] = '\0';
    newNode->len = len;
    newNode->hash = hash;

    // Make the new node the head of the linkedlist
    // Making it the tail would be O(n), whereas making it the head is O(1)
    newNode->next = string_pool->buckets[index];
    string_pool->buckets[index] = newNode;
    string_pool->items++;

    return newNode->string;
}