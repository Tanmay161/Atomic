#ifndef ENVIRONMENT_H

#define ENVIRONMENT_H
#include "hashmap.h"

typedef struct {
    Hashmap variables;
} Environment;

Environment *new_env();
void define(Environment *env, char* identifier, int len, Expression *initializer);

#endif