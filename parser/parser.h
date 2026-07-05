#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Scanner *scanner;
} Parser;

Parser *init_parser(char *file_name);
Program *parse(Parser *p);

#endif