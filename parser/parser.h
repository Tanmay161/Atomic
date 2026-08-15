#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Scanner *scanner;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_POSTFIX,
    PREC_PRIMARY
} Precedence;

typedef Expression *(*ParseFn)(Parser *p, Expression *left);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    ParseFn postfix;
    Precedence precedence;
} ParseRule;

Parser *init_parser(char *file_name);
Program *parse(Parser *p);

#endif