#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "parser.h"
#include "lexer.h"

/* ===== Error Codes =====
    201: Unable to allocate memory for syntax tree
    202: Unable to parse expression
*/

// === Main parsing functions ===
Expression *expression(Parser *p);
Expression *equality(Parser *p);
Expression *comparison(Parser *p);
Expression *term(Parser *p);
Expression *factor(Parser *p);
Expression *unary(Parser *p);
Expression *postfix(Parser *p);
Expression *primary(Parser *p);
Expression *parse(Parser *p);

// === Helpers ===
Expression *construct_binary(Expression *left, Token operator, Expression *right);
Expression *construct_unary(Token operator, Expression *right);
Expression *construct_postfix(Token operator, Expression *left);

// === Experimenting with panic recovery ===
void synchronize(Parser *p);

void error_report(int exitCode, const char *message, ...);

int main()
{
    Scanner *s = init_scanner("./parser/test.txt");
    /* while (peek_token(s).type != TOKEN_EOF) {
        Token cur = next_token(s);

        if (cur.type == LEFT_PAREN) {
            printf("Left parenthesis\n");
        }
        else if (cur.type == RIGHT_PAREN) {
            printf("Right parenthesis\n");
        }
        else if (cur.type == EQUAL_EQUAL) {
            printf("Double equal\n");
        }
        else {
            printf("%.*s\n", (int) cur.len, cur.lexeme);
        }
    } */

   // Parser dry run to fix an error
   /* Parser p = (Parser) {.scanner = s};

    Expression *expr = expression(&p);
    printf("%s\n", mapping[expr->type]);
    //printf("Left term: %lld\n", expr->Grouping.Expr->Binary.Left->Literal.Value.int_value);
    printf("Left term: %lld\n", expr->Grouping.Expr->Binary.Left->Literal.Value.int_value);    
    printf("Operator: ");

    Token operator = expr->Grouping.Expr->Binary.Operator;
    if (operator.type == PLUS_PLUS) {
        printf("++\n");
    }
    else if (operator.type == GREATER_EQUAL) {
        printf(">=\n");
    }
    else if (operator.type == MINUS) {
        printf("-\n");
    }
    else if (operator.type == SLASH) {
        printf("/\n");
    }
    else if (operator.type == PLUS) {
        printf("+\n");
    }

    printf("Right grouping left term: %lld\n", expr->Grouping.Expr->Binary.Right->Grouping.Expr->Binary.Left->Literal.Value.int_value);
    printf("Right grouping operator: ");

    operator = expr->Grouping.Expr->Binary.Right->Grouping.Expr->Binary.Operator;

    if (operator.type == PLUS_PLUS) {
        printf("++\n");
    }
    else if (operator.type == GREATER_EQUAL) {
        printf(">=\n");
    }
    else if (operator.type == MINUS) {
        printf("-\n");
    }
    else if (operator.type == SLASH) {
        printf("/\n");
    }
    else if (operator.type == PLUS) {
        printf("+\n");
    }

    printf("Right grouping right term: %lld\n", expr->Grouping.Expr->Binary.Right->Grouping.Expr->Binary.Right->Literal.Value.int_value);

    //expr = expression(&p); */

    return 0;
}

// Still need to think about language design choice and whether or not the program should exit at the first error
void synchronize(Parser *p) {
    next_token(p->scanner);
    
    Token next = peek_token(p->scanner);
    while (next.type != TOKEN_EOF) {
        if (next.type == SEMICOLON) {
            next_token(p->scanner);
            return;
        }

        switch (next.type) {
            case FOR:
            case IF:
            case WHILE:
            case DATATYPE_FLOAT:
            case DATATYPE_INT:
            case DATATYPE_STRING:
            case RETURN:
                return;
            default: 
                return;
        }

        next_token(p->scanner);
        next = peek_token(p->scanner);
    }
}

void error_report(int exitCode, const char *message, ...) {
    va_list args;
    va_start(args, message);

    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");

    va_end(args);
    exit(exitCode);
}

Expression *construct_binary(Expression *left, Token operator, Expression *right)
{
    Expression *expression = malloc(sizeof(Expression));
    if (!expression)
    {
        fprintf(stderr, "ParsingError: Failed to allocate memory for AST Node.\n");
        exit(201);
    }

    expression->type = BINARY;
    expression->Binary.Left = left;
    expression->Binary.Operator = operator;
    expression->Binary.Right = right;

    return expression;
}

Expression *construct_unary(Token operator, Expression *right)
{
    Expression *expression = malloc(sizeof(Expression));
    if (!expression)
    {
        fprintf(stderr, "ParsingError: Failed to allocate memory for AST Node.\n");
        exit(201);
    }

    expression->type = UNARY;
    expression->Unary.Operator = operator;
    expression->Unary.Expr = right;

    return expression;
}

Expression *construct_postfix(Token operator, Expression *left)
{
    Expression *expression = malloc(sizeof(Expression));
    if (!expression)
    {
        fprintf(stderr, "ParsingError: Failed to allocate memory for AST Node.\n");
        exit(201);
    }

    expression->type = POSTFIX;
    expression->Postfix.Expr = left;
    expression->Postfix.Operator = operator;

    return expression;
}

Expression *parse(Parser *p) {
    return expression(p);
}

Expression *expression(Parser *p)
{
    return equality(p);
}

Expression *equality(Parser *p)
{
    //printf("Performing comparison...\n");
    Expression *expr = comparison(p);
    Token next = peek_token(p->scanner);
    //printf("Is the next token '=='?: %d\n", next.type == EQUAL_EQUAL);

    while (next.type == EQUAL_EQUAL || next.type == NOT_EQUAL)
    {
        //printf("Passed equality check\n");
        Token operator = next_token(p->scanner);
        Expression *right = comparison(p);

        expr = construct_binary(expr, operator, right);
        next = peek_token(p->scanner);
    }

    return expr;
}

Expression *comparison(Parser *p)
{
    //printf("Performing term...\n");
    Expression *expr = term(p);
    Token next = peek_token(p->scanner);

    while (next.type == GREATER || next.type == GREATER_EQUAL || next.type == LESS || next.type == LESS_EQUAL)
    {
        Token operator = next_token(p->scanner);
        printf("Parsing right side of comparison...\n");
        Expression *right = term(p);

        expr = construct_binary(expr, operator, right);
        next = peek_token(p->scanner);
    }

    return expr;
}

Expression *term(Parser *p)
{
    //printf("Performing factor...\n");
    Expression *expr = factor(p);
    Token next = peek_token(p->scanner);

    while (next.type == PLUS || next.type == MINUS)
    {
        Token operator = next_token(p->scanner);
        Expression *right = factor(p);

        expr = construct_binary(expr, operator, right);
        next = peek_token(p->scanner);
    }

    return expr;
}

Expression *factor(Parser *p)
{
    //printf("Performing unary...\n");
    Expression *expr = unary(p);
    Token next = peek_token(p->scanner);

    while (next.type == SLASH || next.type == STAR)
    {
        Token operator = next_token(p->scanner);
        Expression *right = unary(p);

        expr = construct_binary(expr, operator, right);
        next = peek_token(p->scanner);
    }

    return expr;
}

Expression *unary(Parser *p)
{
    Token next = peek_token(p->scanner);

    if (next.type == NOT || next.type == MINUS)
    {
        Token operator = next_token(p->scanner);
        Expression *right = unary(p);

        next = peek_token(p->scanner);
        return construct_unary(operator, right);
    }

    return postfix(p);
}

Expression *postfix(Parser *p)
{
    //printf("Performing primary...\n");
    Expression *left = primary(p);
    Token next = peek_token(p->scanner);

    if (next.type == PLUS_PLUS || next.type == MINUS_MINUS)
    {
        Token operator = next_token(p->scanner);
        return construct_postfix(operator, left);
    }

    return left;
}

Expression *primary(Parser *p)
{
    Token next = next_token(p->scanner);
    //printf("Token info:\nLine: %d\nColumn: %d\nLexeme: %.*s\n", next.line, next.column, (int) next.len, next.lexeme);
    //printf("Is this a number?: %d\n", next.type == INTEGER);
    //printf("Is this '=='?: %d\n", next.type == EQUAL_EQUAL);
    //printf("Is this '('?: %d\n\n", next.type == LEFT_PAREN);

    if (next.type == FALSE)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr) {
            fprintf(stderr, "ParsingError: Failed to allocate memory for AST Node.");
            exit(202);
        }

        expr->type = LITERAL;

        expr->Literal.type = TYPE_FALSE;
        return expr;
    }
    else if (next.type == TRUE)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr) {
            fprintf(stderr, "ParsingError: Failed to allocate memory for AST Node.");
            exit(202);
        }

        expr->type = LITERAL;

        expr->Literal.type = TYPE_TRUE;
        return expr;
    }
    else if (next.type == NIL)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr) {
            fprintf(stderr, "ParsingError: Failed to allocate memory for AST Node.");
            exit(202);
        }

        expr->type = LITERAL;

        expr->Literal.type = TYPE_NIL;
        return expr;
    }
    else if (next.type == INTEGER)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr) {
            fprintf(stderr, "ParsingError: Failed to allocate memory for AST Node.");
            exit(202);
        }

        expr->type = LITERAL;

        expr->Literal.type = TYPE_INTEGER;

        char buf[next.len + 1];
        memcpy(buf, next.lexeme, next.len);
        buf[next.len] = '\0';

        long long value = strtoll(buf, NULL, 10);

        expr->Literal.Value.int_value = value;
        return expr;
    }
    else if (next.type == FLOAT)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr) {
            fprintf(stderr, "ParsingError: Failed to allocate memory for AST Node.");
            exit(202);
        }

        expr->type = LITERAL;

        expr->Literal.type = TYPE_FLOAT;

        char *buf = malloc(next.len + 1);
        memcpy(buf, next.lexeme, next.len);
        buf[next.len] = '\0';

        double value = strtod(buf, NULL);
        free(buf);

        expr->Literal.Value.float_value = value;
        return expr;
    }
    else if (next.type == STRING)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr) {
            fprintf(stderr, "ParsingError: Failed to allocate memory for AST Node.");
            exit(202);
        }

        expr->type = LITERAL;

        expr->Literal.type = TYPE_STRING;
        expr->Literal.Value.lexeme = next.lexeme;
        expr->Literal.Value.len = next.len;

        return expr;
    }
    else if (next.type == LEFT_PAREN)
    {
        //printf("Reached left paren\n");
        Expression *expr = expression(p);
        //printf("Expression Details\nLeft:%c\nRight:%c\n", expr->Binary.Left, expr->Binary.Right);
        //printf("%.*s\n", (int) peek_token(p->scanner).len, peek_token(p->scanner).lexeme);
        if (peek_token(p->scanner).type == RIGHT_PAREN)
        {
            next_token(p->scanner);
            Expression *grouping = malloc(sizeof(Expression));

            if (!grouping)
            {
                fprintf(stderr, "ParsingError: Failed to allocate memory for AST Node.\n");
                exit(201);
            }

            grouping->type = GROUPING;
            grouping->Grouping.Expr = expr;

            return grouping;
        }
        else
        {
            Token got = peek_token(p->scanner);
            ///printf("Token info:\nLine: %d\nColumn: %d\nLexeme: %s\n", got.line, got.column, got.lexeme);
            fprintf(stderr, "SyntaxError: Line %d column %d\nExpected ')' to close expression, got '%.*s'\n", got.line, got.column, (int) got.len, got.lexeme);
            exit(202);
        }
    }
    else if (next.type == TOKEN_EOF) {
        fprintf(stderr, "SyntaxError: Line %d column %d\nExpected expression, got <EOF>\n", p->scanner->line, p->scanner->column);
        exit(202);
    }
    else if (next.type == TOKEN_ERROR) error_report(next.code, "%.*s\n", next.len, next.lexeme);

    fprintf(stderr, "SyntaxError: Line %d column %d\nExpected an expression, got '%.*s'\n", next.line, next.column, (int) next.len, next.lexeme);
    exit(202);
}