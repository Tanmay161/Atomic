#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// === Helpers ===
Expression *construct_binary(Expression *left, Token operator, Expression *right);
Expression *construct_unary(Token operator, Expression *right);
Expression *construct_postfix(Token operator, Expression *left);

int main()
{
    return 0;
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

Expression *expression(Parser *p)
{
    return equality(p);
}

Expression *equality(Parser *p)
{
    Expression *expr = comparison(p);
    Token next = peek_token(p->scanner);

    while (next.type == EQUAL || next.type == NOT_EQUAL)
    {
        Token operator = next_token(p->scanner);
        Expression *right = comparison(p);

        expr = construct_binary(expr, operator, right);
        next = peek_token(p->scanner);
    }

    return expr;
}

Expression *comparison(Parser *p)
{
    Expression *expr = term(p);
    Token next = peek_token(p->scanner);

    while (next.type == GREATER || next.type == GREATER_EQUAL || next.type == LESS || next.type == LESS_EQUAL)
    {
        Token operator = next_token(p->scanner);
        Expression *right = term(p);

        expr = construct_binary(expr, operator, right);
        next = peek_token(p->scanner);
    }

    return expr;
}

Expression *term(Parser *p)
{
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

    while (next.type == NOT || next.type == MINUS)
    {
        Token operator = next_token(p->scanner);
        Expression *right = postfix(p);

        next = peek_token(p->scanner);
        return construct_unary(operator, right);
    }

    return postfix(p);
}

Expression *postfix(Parser *p)
{
    Expression *left = primary(p);
    Token next = peek_token(p->scanner);

    if (next.type == PLUS_PLUS || next.type == MINUS_MINUS)
    {
        Token operator = next_token(p->scanner);
        return construct_postfix(operator, left);
    }

    return primary(p);
}

Expression *primary(Parser *p)
{
    Token next = next_token(p->scanner);
    Expression *expr = malloc(sizeof(Expression));

    if (!expr)
    {
        fprintf(stderr, "ParsingError: Failed to allocate memory for AST Node.\n");
        exit(201);
    }

    expr->type = LITERAL;

    if (next.type == FALSE)
    {
        expr->Literal.type = TYPE_FALSE;
    }
    else if (next.type == TRUE)
    {
        expr->Literal.type = TYPE_TRUE;
    }
    else if (next.type == NIL)
    {
        expr->Literal.type = TYPE_NIL;
    }
    else if (next.type == INTEGER)
    {
        expr->Literal.type = TYPE_INTEGER;

        char *buf = malloc(next.len + 1);
        memcpy(buf, next.lexeme, next.len);
        buf[next.len] = '\0';

        long long value = strtoll(buf, NULL, 10);
        free(buf);

        expr->Literal.Value.int_value = value;
    }
    else if (next.type == FLOAT)
    {
        expr->Literal.type = TYPE_FLOAT;

        char *buf = malloc(next.len + 1);
        memcpy(buf, next.lexeme, next.len);
        buf[next.len] = '\0';

        double value = strtod(buf, NULL);
        free(buf);

        expr->Literal.Value.float_value = value;
    }
    else if (next.type == STRING)
    {
        expr->Literal.type == TYPE_STRING;
        expr->Literal.Value.lexeme = next.lexeme;
        expr->Literal.Value.len = next.len;
    }
    else if (next.type == LEFT_PAREN)
    {
        Expression *expr = expression(p);
        if (peek_token(p->scanner).type == LEFT_PAREN)
        {
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
            fprintf(stderr, "Unable to resolve expression.\n");
            exit(202);
        }
    }
}
