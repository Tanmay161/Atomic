#ifndef ENVIRONMENT_H

#define ENVIRONMENT_H
#include "hashmap.h"

typedef struct {
    Environment *enclosing;
    Hashmap variables;
} Environment;

Environment *new_env();
void define(Environment *env, char* identifier, int len, Expression *initializer);

#endif