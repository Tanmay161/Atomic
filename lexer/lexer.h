#ifndef LEXER_H
#define LEXER_H

#include "token.h"

Token next_token(Scanner *s);
Scanner *init_scanner(char *inputFile);
Token peek_token(Scanner *s);

#endif