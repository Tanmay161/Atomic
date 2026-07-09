#include "environment.h"
#include "hashmap.h"

Environment *new_env() {
    Hashmap *map = init_hashmap(16);
    
    Environment *env = malloc(sizeof(Environment));
    if (!env) {
        fprintf(stderr, "MemoryError: Unable to allocate memory for environment.\n");
        exit(110);
    }

    env->variables = *map;
    free_hashmap(map);

    return env;
}

void define(Environment *env, char* identifier, int len, Expression *initializer) {
    put(&env->variables, identifier, initializer, len);
}