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
    TYPE_FUNCDECL,
    TYPE_BLOCK,
    TYPE_IF,
    TYPE_WHILE,
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
    LOGICAL,
    CALL,
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

// Parameter (for functions)
typedef struct {
    TokenType datatype;
    Token identifier;
} Parameter;

// Blocks
typedef struct {
    Statement **statements;
    size_t count;
} Block;

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

// Function declarations
typedef struct {
    Token identifier;
    TokenType returnType;
    Parameter **parameters;
    int paramCount;

    Block *body;
} FuncDecl;

// If statements
typedef struct {
    Expression *condition;
    Statement *thenBranch;
    Statement *elseBranch;
} IfStmt;

// While statements
typedef struct {
    Expression *condition;
    Statement *body;
} WhileStmt;

// Statement struct
typedef struct Statement {
    SourceSpan span;
    StatementType type;

    union {
        ExprStmt *exprStmt;
        VarDecl *varDecl;
        FuncDecl *funcDecl;
        Block *block;
        IfStmt *ifStmt;
        WhileStmt *whileStmt;
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

        // Errors
        struct {
            char *lexeme;
            size_t len;
            int line;
            int column;
        } Error;

        // Variable struct containing the identifier
        struct {
            Token identifier;
        } Variable;

        // Assignment struct
        // Ex: x = 5
        struct {
            Token identifier;
            Expression *value;
            Token operator;
        } Assignment;

        // Logical struct
        // Ex: x and y, x or (y and z)
        struct {
            Expression *Left;
            Token Operator;
            Expression *Right;
        } Logical;

        struct {
            Expression *callee;
            Expression **arguments;
            int argCount;
        } Call;
    } ;
} Expression;

#endif