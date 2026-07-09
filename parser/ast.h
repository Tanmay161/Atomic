#ifndef AST_H
#define AST_H

#include <stdio.h>
#include "lexer.h"

// Defining Statement and Expression for self-reference
typedef struct Expression Expression;
typedef struct Statement Statement;

// Statement types
typedef enum {
    TYPE_EXPR,
    TYPE_VARDECL,
} StatementType;

// Eval type
typedef enum {
    BINARY,
    UNARY,
    POSTFIX,
    GROUPING,
    LITERAL,
    VARIABLE,
    ASSIGNMENT,
} EvalType;

// Literal types
typedef enum {
    TYPE_INTEGER,
    TYPE_STRING,
    TYPE_FALSE,
    TYPE_TRUE,
    TYPE_FLOAT,
    TYPE_NIL,
} LiteralType;

//char* mapping[] = {"BINARY", "UNARY", "POSTFIX", "GROUPING","LITERAL",};

// Span for error diagnostics (tracking start and end)
typedef struct {
    int startline;
    int endline;
    
    int startcol;
    int endcol;
} SourceSpan;

// Program struct
typedef struct {
    Statement **statements;

    // Number of statements
    size_t count;
} Program;

// Individual statement structs
typedef struct {
    Expression *expr;
} ExprStmt;

// Variable declarations
typedef struct {
    char *name;
    int len;

    TokenType type;
    Expression *initializer;
} VarDecl;

// Statement struct
typedef struct Statement {
    SourceSpan span;
    StatementType type;

    union {
        ExprStmt *exprStmt;
        VarDecl *varDecl;
    };
} Statement;

// Expression struct
typedef struct Expression {
    EvalType type;
    SourceSpan span;

    union {
        // Binary: Any operation between two operations 
        // Ex: (3 * 5) + 4
        struct {
            Expression *Left;
            Token Operator;
            Expression *Right;
        } Binary;

        // Unary: Negating 
        // Ex: -(37 + 5), not True
        struct {
            Token Operator;
            Expression *Expr;
        } Unary;

        // Postfix: Increment / Decrement
        // Ex: (3+5)++, 1--
        struct {
            Expression *Expr;
            Token Operator;
        } Postfix;

        // Grouping: Expression
        // Ex: (3+5)
        struct {
            Expression *Expr;
        } Grouping;

        // Literal
        // Ex: 7, 13, "Hello, world!"
        struct {
            LiteralType type;
            union {
                long long int_value;
                double float_value;
                char *lexeme;
                size_t len;
            } Value;
        } Literal;

        struct {
            char *lexeme;
            size_t len;
            int line;
            int column;
        } Error;

        struct {
            Token identifier;
        } Variable;

        struct {
            Token identifier;
            Expression *value;
        } Assignment;
    } ;
} Expression;

#endif