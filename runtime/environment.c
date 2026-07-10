#include "environment.h"
#include "hashmap.h"
#include <stdlib.h>

Environment *construct_global_env() {
    Hashmap *map = init_hashmap(16);
    
    Environment *env = malloc(sizeof(Environment));
    if (!env) {
        fprintf(stderr, "MemoryError: Unable to allocate memory for environment.\n");
        exit(300);
    }

    env->variables = *map;
    env->enclosing = NULL;
    free(map);

    return env;
}

Environment *construct_enclosed_env(Environment *enclosing) {
   Hashmap *map = init_hashmap(16);
    
    Environment *env = malloc(sizeof(Environment));
    if (!env) {
        fprintf(stderr, "MemoryError: Unable to allocate memory for environment.\n");
        exit(300);
    }

    env->variables = *map;
    env->enclosing = enclosing;
    free(map);

    return env;
}

void define(Environment *env, char *identifier, int len, Expression *initializer) {
    put(&env->variables, identifier, initializer, len);
}

Expression *find(Environment *env, char *identifier, size_t len) {
    Environment *cur = env;

    while (cur != NULL) {
        Expression *res = get(&cur->variables, identifier, len);
        if (res != NULL) return res;

        cur = cur->enclosing;
    }

    fprintf(stderr, "RuntimeError: Unknown identifier '%.*s'\n", len, identifier);
    exit(301);
}

void assign(Environment *env, char *identifier, size_t len, Expression *value) {
    Environment *cur = env;

    while (cur != NULL) {
        Expression *res = get(&cur->variables, identifier, len);
        if (res != NULL) {
            put(&cur->variables, identifier, value, len);
            return;
        }

        cur = cur->enclosing;
    }

    fprintf(stderr, "RuntimeError: Variable '%.*s' was never declared.\n", len, identifier);
    exit(302);
}