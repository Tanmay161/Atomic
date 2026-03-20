#include "stringPool.h"
#include <stdio.h>
#include <string.h>

Block string_pool = {.capacity = 50, .next = NULL, .used = 0};

Block *createNewBlock() {
    Block *newBlock = malloc(sizeof(Block));
    if (!newBlock) {
        fprintf(stderr, "MemoryError: Failed to allocate memory for string intern pool.");
        exit(110);
    }   

    StringObject **temp = malloc(sizeof(StringObject*) * 50);

    if (!temp) {
        fprintf(stderr, "MemoryError: Failed to allocate memory for string intern pool.");
        exit(110);
    }

    newBlock->capacity = 50;
    newBlock->next = NULL;
    newBlock->used = 0;
    newBlock->data = temp;

    return newBlock;
}

// Init first block
int main() {
    StringObject **temp = malloc(sizeof(StringObject*) * 50);
    if (!temp) {
        fprintf(stderr, "MemoryError: Failed to allocate memory for string intern pool.");
        exit(110);
    }

    string_pool.data = temp;
    return 0;
}

// If string found in pool, return pointer. If not, add to pool and return new address.
char *add_return_address_in_pool(char *string, size_t len) {
    Block *cur = &string_pool;
    
    while (cur != NULL) {
        for (size_t i = 0; i < cur->used; i++) {
            if (len == cur->data[i]->len && (memcmp(cur->data[i]->string, string, len) == 0)) {
                return cur->data[i]->string;
            }
        }

        cur = cur->next;
    }

    // Adding to the pool
    cur = &string_pool;
    while (1) {
        // Move on if block is full
        if (cur->used == cur->capacity - 1) {
            if (cur->next == NULL) {
                Block *newBlock = createNewBlock();
                
                StringObject *object = malloc(sizeof(StringObject));

                if (!object) {
                    fprintf(stderr, "MemoryError: Failed to allocate memory for string intern pool.");
                    exit(110);
                }

                object->string = malloc(len + 1);
                memcpy(object->string, string, len);
                object->string[len] = '\0';

                object->len = len;

                newBlock->data[newBlock->used++] = object;
                cur->next = newBlock;

                return string;
            }
            cur = cur->next;
            continue;
        }
        else {
            StringObject *object = malloc(sizeof(StringObject));

            if (!object) {
                fprintf(stderr, "MemoryError: Failed to allocate memory for string intern pool.");
                exit(110);
            }

            object->string = string;
            object->len = len;

            cur->data[cur->used++] = object;

            // If block becomes full with this allocation, create new block
            if (cur->used == cur->capacity - 1) {
                Block *newBlock = createNewBlock();
                cur->next = newBlock;
            }
            return string;
        }
    }
}