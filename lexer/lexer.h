#ifndef LEXER_H
#define LEXER_H

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

typedef enum
{
    //  Single character tokens
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACE,
    RIGHT_BRACE,
    COMMA,
    DOT,
    MINUS,
    PLUS,
    SEMICOLON,
    SLASH,
    STAR,
    LEFT_BRACKET,
    RIGHT_BRACKET,
    PLUS_PLUS,
    MINUS_MINUS,

    // Multi-character tokens
    NOT_EQUAL,
    EQUAL,
    EQUAL_EQUAL,
    GREATER,
    MOD,
    MOD_EQUAL,
    TILDE,
    TILDE_SLASH,
    TILDE_SLASH_EQUAL,
    GREATER_EQUAL,
    LESS,
    LESS_EQUAL,
    PLUS_EQUAL,
    MINUS_EQUAL,
    STAR_EQUAL,
    SLASH_EQUAL,

    // Literals
    IDENTIFIER,
    STRING,
    INTEGER,
    FLOAT,

    // Datatypes
    DATATYPE_INT,
    DATATYPE_FLOAT,
    DATATYPE_STRING,

    // Keywords
    AND,
    OR,
    IF,
    ELSE,
    FOR,
    NIL,
    RETURN,
    WHILE,
    TRUE,
    FALSE,
    NOT,

    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

// ===== Token Struct =====
typedef struct
{
    TokenType type;
    char *lexeme;
    size_t len;
    int line;

    // For TOKEN_ERROR
    int code;
} Token;

typedef struct
{
    char *lexeme;
    TokenType type;
} Keyword;

Token next_token(Scanner *s);
Scanner *init_scanner(char *inputFile);

#endif