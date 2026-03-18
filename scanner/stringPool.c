#include "stringPool.h"
#include <stdio.h>

Block string_pool = {.capacity = 50, .next = NULL, .used = 0};

int main() {
    StringObject *temp = malloc(sizeof(StringObject) * 50);
    if (!temp) {
        fprintf(stderr, "MemoryError: Unable to allocate memory for string intern pool.\n");
        exit(110);
    }

    string_pool.data = temp;

    return 0;
}

/*v oid clean_up() {
    Block *cur = &string_pool;
    for (int i = 0; i < cur->data->len; i++) {
        free(cur->data[i]);
    }
} */