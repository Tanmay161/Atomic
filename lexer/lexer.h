#ifndef LEXER_H
#define LEXER_H

#include "token.h"

#include <stdio.h>

typedef struct
{
    char *stream;
    char *start;
    char *pos;
    int line;
    int column;
    FILE *input;
} Scanner;

Token next_token(Scanner *s);
Scanner *init_scanner(char *inputFile);
Token peek_token(Scanner *s);

#endif