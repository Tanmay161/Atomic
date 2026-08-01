#include "memory.h"
#include "shared.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, newSize);

    if (!result) 
        error_report(300, "MemoryError: Failed to allocate memory.");
    return result;
}