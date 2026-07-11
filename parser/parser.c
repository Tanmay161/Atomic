#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

#include "parser.h"
#include "lexer.h"
#include "ast.h"

/* ===== Error Codes =====
    200: Unable to allocate memory for parser
    201: Unable to allocate memory for syntax tree
    202: Unable to parse expression
*/

// Parser constructor
Parser *init_parser(char *file_name);

// === Main parsing functions ===
Expression *expression(Parser *p);
Expression *assignment(Parser *p);
Expression *or(Parser *p);
Expression *and(Parser *p);
Expression *equality(Parser *p);
Expression *comparison(Parser *p);
Expression *term(Parser *p);
Expression *factor(Parser *p);
Expression *unary(Parser *p);
Expression *postfix(Parser *p);
Expression *primary(Parser *p);
Statement *statement(Parser *p);
Statement *declaration(Parser *p);
Program *parse(Parser *p);

// === Helpers ===
Expression *construct_binary(Expression *left, Token operator, Expression *right);
Expression *construct_logical(Expression *left, Token operator, Expression *right);
Expression *construct_unary(Token operator, Expression *right, int startline, int endline, int startcol, int endcol);
Expression *construct_postfix(Token operator, Expression *left, int startline, int endline, int startcol, int endcol);
Statement *construct_statement(Expression *expr, Token semicolon, StatementType type, ...);
Statement *variable_declaration(Parser *p);
Statement *construct_block(Parser *p);
Statement *if_statement(Parser *p);
Statement *while_statement(Parser *p);

// === Experimenting with panic recovery ===
void synchronize(Parser *p);

// === Testing purposes only ===
void output_expression(Expression *expr);
void output_statement(Statement *statement);
void output_block(Block *block);

// Error reporting
void error_report(int exitCode, const char *message, ...);

int main()
{
    Parser *p = init_parser("./parser/test.txt");
    Program *program = parse(p);

    for (int i = 0; i < program->count; i++)
    {
        Statement *statement = program->statements[i];
        output_statement(statement);
    }

    return 0;
}

void output_statement(Statement *statement)
{
    switch (statement->type)
    {
    case TYPE_EXPR:
    {
        printf("Expression statement\n");
        output_expression(statement->exprStmt->expr);
        printf("\n");
        break;
    }
    case TYPE_VARDECL:
    {
        printf("Variable declaration\n");
        printf("Identifier: '%.*s'\n", statement->varDecl->len, statement->varDecl->name);

        if (statement->varDecl->initializer != NULL) {
            printf("Initializer: ");
            output_expression(statement->varDecl->initializer);
            printf("\n");
        }
        break;
    }
    case TYPE_BLOCK:
    {
        output_block(statement->block);
        break;
    }
    case TYPE_IF:
    {
        printf("If statement\n");
        printf("Condition: ");
        output_expression(statement->ifStmt->condition);
        printf("\n");
        printf("Then branch: ");
        output_block(statement->ifStmt->thenBranch->block);
        printf("\n");

        if (statement->ifStmt->elseBranch != NULL) {
            printf("Else branch: ");
            output_statement(statement->ifStmt->elseBranch);
        }
        break;
    }
    }
    printf("\n");
}

void output_block(Block *block)
{
    printf("Block\n");
    printf("{\n");
    for (int i = 0; i < block->count; i++)
    {
        output_statement(block->statements[i]);
    }
    printf("}\n");
}

void output_expression(Expression *expr)
{
    EvalType type = expr->type;

    switch (type)
    {
    case BINARY:
    {
        printf("(");
        output_expression(expr->Binary.Left);
        printf(")");
        Token operator = expr->Binary.Operator;
        printf(" %.*s ", operator.len, operator.lexeme);
        printf("(");
        output_expression(expr->Binary.Right);
        printf(")");

        break;
    }
    case UNARY:
    {
        printf("%.*s ", expr->Unary.Operator.len, expr->Unary.Operator.lexeme);
        output_expression(expr->Unary.Expr);
        break;
    }
    case POSTFIX:
    {
        output_expression(expr->Postfix.Expr);
        printf("%.*s", expr->Postfix.Operator.len, expr->Postfix.Operator.lexeme);
        break;
    }
    case GROUPING:
    {
        printf("(");
        output_expression(expr->Grouping.Expr);
        printf(")");
        break;
    }
    case LITERAL:
    {
        switch (expr->Literal.type)
        {
        case TYPE_INTEGER:
        {
            printf("%d", expr->Literal.Value.int_value);
            break;
        }
        case TYPE_STRING:
        {
            printf("%.*s", expr->Literal.Value.len, expr->Literal.Value.lexeme);
            break;
        }
        case TYPE_FALSE:
        {
            printf("false");
            break;
        }
        case TYPE_TRUE:
        {
            printf("true");
            break;
        }
        case TYPE_FLOAT:
        {
            printf("%f", expr->Literal.Value.float_value);
            break;
        }
        case TYPE_NIL:
        {
            printf("nil");
            break;
        }
        }
    }
    case VARIABLE:
    {
        printf("%.*s", expr->Variable.identifier.len, expr->Variable.identifier.lexeme);
        break;
    }
    case ASSIGNMENT:
    {
        printf("%.*s = ", expr->Assignment.identifier.len, expr->Assignment.identifier.lexeme);
        output_expression(expr->Assignment.value);
        break;
    }
    case LOGICAL:
    {
        printf("(");
        output_expression(expr->Logical.Left);
        printf(")");
        printf(" %.*s ", expr->Logical.Operator.len, expr->Logical.Operator.lexeme);
        printf("(");
        output_expression(expr->Logical.Right);
        printf(")");
        break;
    }
    }
}

Parser *init_parser(char *file_name)
{
    Scanner *s = init_scanner(file_name);

    Parser *parser = malloc(sizeof(Parser));

    if (!parser)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for Parser.\n");
        exit(200);
    }

    parser->scanner = s;

    return parser;
}

// Still need to think about language design choice and whether or not the program should exit at the first error
void synchronize(Parser *p)
{
    next_token(p->scanner);

    Token next = peek_token(p->scanner);
    while (next.type != TOKEN_EOF)
    {
        if (next.type == SEMICOLON)
        {
            next_token(p->scanner);
            return;
        }

        switch (next.type)
        {
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

void error_report(int exitCode, const char *message, ...)
{
    va_list args;
    va_start(args, message);

    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");

    va_end(args);
    exit(exitCode);
}

Expression *construct_binary(Expression *left, Token operator, Expression *right) {
    Expression *expression = malloc(sizeof(Expression));
    if (!expression)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
        exit(201);
    }

    expression->type = BINARY;
    expression->Binary.Left = left;
    expression->Binary.Operator = operator;
    expression->Binary.Right = right;

    expression->span.startline = left->span.startline;
    expression->span.startcol = left->span.startcol;
    expression->span.endline = right->span.endline;
    expression->span.endcol = right->span.endcol;

    return expression;
}

Expression *construct_logical(Expression *left, Token operator, Expression *right)
{
    Expression *expression = malloc(sizeof(Expression));
    if (!expression)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
        exit(201);
    }

    expression->type = LOGICAL;
    expression->Logical.Left = left;
    expression->Logical.Operator = operator;
    expression->Logical.Right = right;

    expression->span.startline = left->span.startline;
    expression->span.startcol = left->span.startcol;
    expression->span.endline = right->span.endline;
    expression->span.endcol = right->span.endcol;

    return expression;
}

Expression *construct_unary(Token operator, Expression *right, int startline, int endline, int startcol, int endcol)
{
    Expression *expression = malloc(sizeof(Expression));
    if (!expression)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
        exit(201);
    }

    expression->type = UNARY;
    expression->Unary.Operator = operator;
    expression->Unary.Expr = right;

    expression->span.startline = startline;
    expression->span.startcol = startcol;
    expression->span.endline = endline;
    expression->span.endcol = endcol;

    return expression;
}

Expression *construct_postfix(Token operator, Expression *left, int startline, int endline, int startcol, int endcol)
{
    Expression *expression = malloc(sizeof(Expression));
    if (!expression)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
        exit(201);
    }

    expression->type = POSTFIX;
    expression->Postfix.Expr = left;
    expression->Postfix.Operator = operator;

    expression->span.startline = startline;
    expression->span.startcol = startcol;
    expression->span.endline = endline;
    expression->span.endcol = endcol;

    return expression;
}

Expression *construct_assignment(Token identifier, Expression *value)
{
    Expression *expression = malloc(sizeof(Expression));
    if (!expression)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
        exit(201);
    }

    expression->type = ASSIGNMENT;
    expression->Assignment.identifier = identifier;
    expression->Assignment.value = value;

    expression->span.startline = identifier.line;
    expression->span.startcol = identifier.column;
    expression->span.endline = value->span.endline;
    expression->span.endcol = value->span.endcol;

    return expression;
}

Statement *construct_statement(Expression *expr, Token semicolon, StatementType type, ...)
{
    va_list args;
    va_start(args, type);

    Statement *stmt = malloc(sizeof(Statement));

    if (!stmt)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
        exit(201);
    }

    stmt->type = type;

    switch (type)
    {
    case TYPE_EXPR:
    {
        ExprStmt *expr_stmt = malloc(sizeof(ExprStmt));
        if (!expr_stmt)
            error_report(202, "MemoryError: Unable to allocate memory for AST node.");

        stmt->exprStmt = expr_stmt;
        stmt->exprStmt->expr = expr;

        stmt->span.startline = expr->span.startline;
        stmt->span.startcol = expr->span.startcol;

        stmt->span.endline = semicolon.line;
        stmt->span.endcol = semicolon.column;

        break;
    }
    case TYPE_VARDECL:
    {
        VarDecl *var_decl = malloc(sizeof(VarDecl));
        if (!var_decl)
            error_report(202, "MemoryError: Unable to allocate memory for AST node.");

        Token identifier = va_arg(args, Token);
        TokenType type = va_arg(args, TokenType);

        stmt->varDecl = var_decl;
        stmt->varDecl->name = identifier.lexeme;
        stmt->varDecl->len = identifier.len;
        stmt->varDecl->type = type;
        stmt->varDecl->initializer = expr;

        stmt->span.startline = expr->span.startline;
        stmt->span.startcol = expr->span.startcol;

        stmt->span.endline = semicolon.line;
        stmt->span.endcol = semicolon.column;

        break;
    }
    case TYPE_BLOCK:
    {
        Block *block = malloc(sizeof(Block));
        if (!block)
            error_report(202, "MemoryError: Unable to allocate memory for AST node.");

        Token left_brace = va_arg(args, Token);
        Statement **statements = va_arg(args, Statement **);
        size_t count = va_arg(args, size_t);

        stmt->block = block;
        stmt->block->statements = statements;
        stmt->block->count = count;

        stmt->span.startline = left_brace.line;
        stmt->span.startcol = left_brace.column;

        stmt->span.endline = semicolon.line;
        stmt->span.endcol = semicolon.column;

        break;
    }
    case TYPE_IF:
    {
        IfStmt *if_stmt = malloc(sizeof(IfStmt));
        if (!if_stmt)
            error_report(202, "MemoryError: Unable to allocate memory for AST node.");

        Statement *thenBranch = va_arg(args, Statement *);
        Statement *elseBranch = va_arg(args, Statement *);

        Statement *final = thenBranch;
        if (elseBranch != NULL)
            final = elseBranch;

        stmt->ifStmt = if_stmt;
        stmt->ifStmt->condition = expr;
        stmt->ifStmt->thenBranch = thenBranch;
        stmt->ifStmt->elseBranch = elseBranch;

        // Weird workaround here... remember that semicolon refers to the IF token
        stmt->span.startline = semicolon.line;
        stmt->span.startcol = semicolon.column;

        stmt->span.endline = final->span.endline;
        stmt->span.endcol = final->span.endcol;

        break;
    }
    case TYPE_WHILE:
    {
        WhileStmt *while_stmt = malloc(sizeof(WhileStmt));
        if (!while_stmt)
            error_report(202, "MemoryError: Unable to allocate memory for AST node.");
        
        Statement *body = va_arg(args, Statement*);
        stmt->whileStmt = while_stmt;
        stmt->whileStmt->body = body;
        stmt->whileStmt->condition = expr;

        stmt->span.startline = semicolon.line;
        stmt->span.startcol = semicolon.column;

        stmt->span.endline = body->span.endline;
        stmt->span.endcol = body->span.endcol;

        break;
    }
    }

    return stmt;
}

Statement *variable_declaration(Parser *p)
{
    TokenType type = next_token(p->scanner).type;
    Token identifier = next_token(p->scanner);

    if (identifier.type != IDENTIFIER)
        error_report(202, "ParsingError: Line %d column %d\nExpected identifier, got '%.*s'", identifier.line, identifier.column, identifier.len, identifier.lexeme);

    Expression *initializer = NULL;

    if (peek_token(p->scanner).type == EQUAL)
    {
        next_token(p->scanner);
        initializer = expression(p);
    }

    Token next = next_token(p->scanner);
    if (next.type != SEMICOLON)
        error_report(202, "ParsingError: Line %d column %d\nExpected ';', got '%.*s'", next.line, next.column, next.len, next.lexeme);

    return construct_statement(initializer, next, TYPE_VARDECL, identifier, type);
}

Statement *construct_block(Parser *p)
{
    Token left_brace = next_token(p->scanner);

    size_t capacity = 4;
    Statement **statements = calloc(capacity, sizeof(Statement *));

    if (!statements)
        error_report(201, "MemoryError: Failed to allocate memory for AST node.");

    size_t count = 0;
    Token next = peek_token(p->scanner);

    while (next.type != RIGHT_BRACE && next.type != TOKEN_EOF)
    {
        if (count == capacity)
        {
            capacity *= 2;
            Statement **temp = realloc(statements, capacity * sizeof(Statement *));

            if (!temp)
                error_report(201, "MemoryError: Failed to allocate memory for AST node.");

            statements = temp;
        }

        statements[count++] = declaration(p);
        next = peek_token(p->scanner);
    }

    if (next.type != RIGHT_BRACE)
        error_report(202, "ParsingError: Line %d column %d\nExpected '}', got '%.*s'", next.line, next.column, next.len, next.lexeme);

    Statement *block = construct_statement(NULL, next, TYPE_BLOCK, left_brace, statements, count);
    next_token(p->scanner);

    return block;
}

// I chose to enforce braces to avoid the dangling else problem
Statement *if_statement(Parser *p)
{
    Token if_tok = next_token(p->scanner);
    Token next = peek_token(p->scanner);

    if (next.type != LEFT_PAREN)
        error_report(202, "SyntaxError: Line %d column %d\nExpected '(' after 'if', got '%.*s'", next.line, next.column, next.len, next.lexeme);

    next_token(p->scanner);
    Expression *condition = expression(p);

    next = peek_token(p->scanner);
    if (next.type != RIGHT_PAREN)
        error_report(202, "SyntaxError: Line %d column %d\nExpected ')' after if condition, got '%.*s'", next.line, next.column, next.len, next.lexeme);

    next_token(p->scanner);
    next = peek_token(p->scanner);
    if (next.type != LEFT_BRACE)
        error_report(202, "SyntaxError: Line %d column %d\nExpected '{' after if condition, got '%.*s'", next.line, next.column, next.len, next.lexeme);

    Statement *then_branch = construct_block(p);
    Statement *else_branch = NULL;

    next = peek_token(p->scanner);
    if (next.type == ELSE)
    {
        next_token(p->scanner);
        next = peek_token(p->scanner);

        if (next.type != LEFT_BRACE && next.type != IF)
            error_report(202, "SyntaxError: Line %d column %d\nExpected '{' or 'if' after 'else', got '%.*s'", next.line, next.column, next.len, next.lexeme);
        
        if (next.type == LEFT_BRACE) else_branch = construct_block(p);
        else else_branch = if_statement(p);
    }

    return construct_statement(condition, if_tok, TYPE_IF, then_branch, else_branch);
}

// While statement
Statement *while_statement(Parser *p) {
    Token while_token = next_token(p->scanner);
    Token next = peek_token(p->scanner);

    if (next.type != LEFT_PAREN) 
        error_report(202, "SyntaxError: Line %d column %d\nExpected '(' after 'while', got '%.*s'", next.line, next.column, next.len, next.lexeme);
    
    next_token(p->scanner);
    Expression *condition = expression(p);

    next = peek_token(p->scanner);
    if (next.type != RIGHT_PAREN) 
        error_report(202, "SyntaxError: Line %d column %d\nExpected ')' after while condition, got '%.*s'", next.line, next.column, next.len, next.lexeme);

    next_token(p->scanner);
    next = peek_token(p->scanner);
    if (next.type != LEFT_BRACE) 
        error_report(202, "SyntaxError: Line %d column %d\nExpected '{' after if condition, got '%.*s'", next.line, next.column, next.len, next.lexeme);

    Statement *body = construct_block(p);
    return construct_statement(condition, while_token, TYPE_WHILE, body);
}

Program *parse(Parser *p)
{
    size_t capacity = 8;
    size_t stmtCount = 0;

    Program *program = malloc(sizeof(Program));

    if (!program)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for Program.\n");
        exit(201);
    }

    Statement **statements = malloc(capacity * sizeof(Statement *));

    if (!statements)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for Statement Array.\n");
        exit(201);
    }

    while (peek_token(p->scanner).type != TOKEN_EOF)
    {
        if (stmtCount == capacity)
        {
            capacity *= 2;
            Statement **temp = realloc(statements, capacity * sizeof(Statement *));

            if (!temp)
            {
                fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
                exit(201);
            }

            statements = temp;
        }

        Statement *fetched = declaration(p);
        statements[stmtCount++] = fetched;
    }

    program->statements = statements;
    program->count = stmtCount;

    return program;
}

Statement *declaration(Parser *p)
{
    Token next = peek_token(p->scanner);
    if (next.type == DATATYPE_FLOAT || next.type == DATATYPE_INT || next.type == DATATYPE_STRING || next.type == DATATYPE_BOOL)
        return variable_declaration(p);

    return statement(p);
}

Statement *statement(Parser *p)
{
    Token next = peek_token(p->scanner);

    switch (next.type) {
        case LEFT_BRACE: return construct_block(p);
        case IF: return if_statement(p);
        case WHILE: return while_statement(p);
        default: {
            Expression *expr = expression(p);
            next = peek_token(p->scanner);

            if (next.type == SEMICOLON)
            {
                next_token(p->scanner);
                return construct_statement(expr, next, TYPE_EXPR);
            }
            else
                error_report(202, "ParsingError: Line %d column %d\nExpected ';', got '%.*s'", next.line, next.column, next.len, next.lexeme);
        }
    }
    

    return NULL;
}

Expression *expression(Parser *p)
{
    return assignment(p);
}

Expression *assignment(Parser *p)
{
    Expression *expr = or(p);

    if (peek_token(p->scanner).type == EQUAL)
    {
        next_token(p->scanner);
        Expression *value = assignment(p);

        if (expr->type == VARIABLE)
        {
            Token identifier = expr->Variable.identifier;
            return construct_assignment(identifier, value);
        }

        error_report(202, "SyntaxError: Line %d column %d\nInvalid assignment target", expr->span.startline, expr->span.startcol);
    }

    return expr;
}

Expression *or(Parser *p)
{
    Expression *expr = and(p);

    while (peek_token(p->scanner).type == OR)
    {
        Token operator = next_token(p->scanner);
        Expression *right = and(p);

        expr = construct_logical(expr, operator, right);
    }

    return expr;
}

Expression *and(Parser *p)
{
    Expression *expr = equality(p);

    while (peek_token(p->scanner).type == AND)
    {
        Token operator = next_token(p->scanner);
        Expression *right = equality(p);

        expr = construct_logical(expr, operator, right);
    }

    return expr;
}

Expression *equality(Parser *p)
{
    // printf("Performing comparison...\n");
    Expression *expr = comparison(p);
    Token next = peek_token(p->scanner);
    // printf("Is the next token '=='?: %d\n", next.type == EQUAL_EQUAL);

    while (next.type == EQUAL_EQUAL || next.type == NOT_EQUAL)
    {
        // printf("Passed equality check\n");
        Token operator = next_token(p->scanner);
        Expression *right = comparison(p);

        expr = construct_binary(expr, operator, right);
        next = peek_token(p->scanner);
    }

    return expr;
}

Expression *comparison(Parser *p)
{
    // printf("Performing term...\n");
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
    // printf("Performing factor...\n");
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
    // printf("Performing unary...\n");
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

    int startline = next.line;
    int startcol = next.column;

    if (next.type == NOT || next.type == MINUS)
    {
        Token operator = next_token(p->scanner);
        Expression *right = unary(p);

        int endline = right->span.endline;
        int endcol = right->span.endcol;

        next = peek_token(p->scanner);
        return construct_unary(operator, right, startline, endline, startcol, endcol);
    }

    return postfix(p);
}

Expression *postfix(Parser *p)
{
    // printf("Performing primary...\n");
    Expression *left = primary(p);

    int startline = left->span.startline;
    int startcol = left->span.startcol;

    Token next = peek_token(p->scanner);

    if (next.type == PLUS_PLUS || next.type == MINUS_MINUS)
    {
        Token operator = next_token(p->scanner);

        int endline = operator.line;
        int endcol = operator.column + operator.len;

        return construct_postfix(operator, left, startline, endline, startcol, endcol);
    }

    return left;
}

Expression *primary(Parser *p)
{
    Token next = next_token(p->scanner);
    // printf("Token info:\nLine: %d\nColumn: %d\nLexeme: %.*s\n", next.line, next.column, (int) next.len, next.lexeme);
    // printf("Is this a number?: %d\n", next.type == INTEGER);
    // printf("Is this '=='?: %d\n", next.type == EQUAL_EQUAL);
    // printf("Is this '('?: %d\n\n", next.type == LEFT_PAREN);

    if (next.type == FALSE)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr)
        {
            fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
            exit(201);
        }

        expr->type = LITERAL;
        expr->Literal.type = TYPE_FALSE;

        expr->span.startline = next.line;
        expr->span.startcol = next.column;

        expr->span.endline = next.line;
        expr->span.endcol = next.column + next.len;

        return expr;
    }
    else if (next.type == TRUE)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr)
        {
            fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
            exit(201);
        }

        expr->type = LITERAL;
        expr->Literal.type = TYPE_TRUE;

        expr->span.startline = next.line;
        expr->span.startcol = next.column;

        expr->span.endline = next.line;
        expr->span.endcol = next.column + next.len;

        return expr;
    }
    else if (next.type == NIL)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr)
        {
            fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
            exit(201);
        }

        expr->type = LITERAL;
        expr->Literal.type = TYPE_NIL;

        expr->span.startline = next.line;
        expr->span.startcol = next.column;

        expr->span.endline = next.line;
        expr->span.endcol = next.column + next.len;

        return expr;
    }
    else if (next.type == INTEGER)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr)
        {
            fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
            exit(201);
        }

        expr->type = LITERAL;

        expr->Literal.type = TYPE_INTEGER;

        char buf[next.len + 1];
        memcpy(buf, next.lexeme, next.len);
        buf[next.len] = '\0';

        long long value = strtoll(buf, NULL, 10);

        expr->Literal.Value.int_value = value;

        expr->span.startline = next.line;
        expr->span.startcol = next.column;

        expr->span.endline = next.line;
        expr->span.endcol = next.column + next.len;

        return expr;
    }
    else if (next.type == FLOAT)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr)
        {
            fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
            exit(201);
        }

        expr->type = LITERAL;

        expr->Literal.type = TYPE_FLOAT;

        char *buf = malloc(next.len + 1);
        memcpy(buf, next.lexeme, next.len);
        buf[next.len] = '\0';

        double value = strtod(buf, NULL);
        free(buf);

        expr->Literal.Value.float_value = value;

        expr->span.startline = next.line;
        expr->span.startcol = next.column;

        expr->span.endline = next.line;
        expr->span.endcol = next.column + next.len;

        return expr;
    }
    else if (next.type == STRING)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr)
        {
            fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
            exit(201);
        }

        expr->type = LITERAL;

        expr->Literal.type = TYPE_STRING;
        expr->Literal.Value.len = next.len;
        expr->Literal.Value.lexeme = next.lexeme;

        expr->span.startline = next.line;
        expr->span.startcol = next.column - 1;

        expr->span.endline = next.line;
        expr->span.endcol = next.column + next.len + 1;

        return expr;
    }
    else if (next.type == IDENTIFIER)
    {
        Expression *expr = malloc(sizeof(Expression));

        if (!expr)
        {
            fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
            exit(201);
        }

        expr->type = VARIABLE;
        expr->Variable.identifier = next;

        return expr;
    }
    else if (next.type == LEFT_PAREN)
    {
        // printf("Reached left paren\n");
        Expression *expr = expression(p);
        int startline = next.line;
        int startcol = next.column;
        // printf("Expression Details\nLeft:%c\nRight:%c\n", expr->Binary.Left, expr->Binary.Right);
        // printf("%.*s\n", (int) peek_token(p->scanner).len, peek_token(p->scanner).lexeme);
        if (peek_token(p->scanner).type == RIGHT_PAREN)
        {
            Token paren = next_token(p->scanner);
            Expression *grouping = malloc(sizeof(Expression));

            if (!grouping)
            {
                fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
                exit(201);
            }

            grouping->type = GROUPING;
            grouping->Grouping.Expr = expr;

            grouping->span.startline = startline;
            grouping->span.startcol = startcol;

            grouping->span.endline = paren.line;
            grouping->span.endcol = paren.column + 1;

            return grouping;
        }
        else
        {
            Token got = peek_token(p->scanner);
            /// printf("Token info:\nLine: %d\nColumn: %d\nLexeme: %s\n", got.line, got.column, got.lexeme);
            fprintf(stderr, "SyntaxError: Line %d column %d\nExpected ')' to close expression, got '%.*s'\n", got.line, got.column, (int)got.len, got.lexeme);
            exit(202);
        }
    }
    else if (next.type == TOKEN_EOF)
    {
        fprintf(stderr, "SyntaxError: Line %d column %d\nExpected expression, got <EOF>\n", p->scanner->line, p->scanner->column);
        exit(202);
    }
    else if (next.type == TOKEN_ERROR)
        error_report(next.code, "%.*s\n", next.len, next.lexeme);

    fprintf(stderr, "SyntaxError: Line %d column %d\nExpected an expression, got '%.*s'\n", next.line, next.column, (int)next.len, next.lexeme);
    exit(202);
}