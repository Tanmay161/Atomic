#include "lexer.h"

// Defining Expression for self-reference
typedef struct Expression Expression;

// Eval type
typedef enum {
    BINARY,
    UNARY,
    POSTFIX,
    GROUPING,
    LITERAL,
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

char* mapping[] = {"BINARY", "UNARY", "POSTFIX", "GROUPING","LITERAL",};

// Expression struct
typedef struct Expression {
    EvalType type;

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
    } ;
} Expression;

typedef struct {
    Scanner *scanner;
} Parser;