#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

#include "parser.h"
#include "token.h"
#include "ast.h"
#include "error.h"

/* ===== Error Codes =====
    200: Unable to allocate memory for parser
    201: Unable to allocate memory for syntax tree
    202: Unable to parse expression
*/

// Parser constructor
Parser *init_parser(Scanner *s);

// Main parsing function
Program *parse(Parser *p);

// === Helpers ===
Expression *construct_binary(Expression *left, Token operator, Expression *right);
Expression *construct_logical(Expression *left, Token operator, Expression *right);
Expression *construct_unary(Token operator, Expression *right);
Expression *construct_postfix(Token operator, Expression *left);
Expression *construct_assignment(Token identifier, Expression *value, Token operator);
Expression *construct_call(Expression *callee, Expression **args, int argcount, Token right_paren);
Expression *finishCall(Parser *p, Expression *callee);
Statement *construct_statement(Expression *expr, Token semicolon, StatementType type, ...);
Statement *variable_declaration(Parser *p);
Statement *function_declaration(Parser *p);
Statement *construct_block(Parser *p);
Statement *expression_statement(Parser *p);
Statement *if_statement(Parser *p);
Statement *while_statement(Parser *p);
Statement *for_statement(Parser *p);
Statement *return_statement(Parser *p);
Statement *statement(Parser *p);
Statement *declaration(Parser *p);

Token consume(Parser *p, TokenType type, const char *message, int errorCode);
int check(Parser *p, TokenType type);
int match(Parser *p, TokenType type);

// === Experimenting with panic recovery ===
void synchronize(Parser *p);

// === Testing purposes only ===
void output_expression(Expression *expr);
void output_statement(Statement *statement);
void output_block(Block *block);

static inline ParseRule *getRule(TokenType type);
static Expression *expression(Parser *p);
static Expression *parsePrecedence(Parser *p, Precedence precedence);

static Expression *parse_number(Parser *p, Expression *left) {
    Token prev = next_token(p->scanner);
    Expression *expr = malloc(sizeof(Expression));

    if (!expr)
        error_report(201, "Failed to allocate memory for AST node.");

    expr->type = LITERAL;

    char *buf = malloc(prev.len + 1);

    if (!buf)
        error_report(201, "Failed to allocate memory for AST node.");

    memcpy(buf, prev.lexeme, prev.len);
    buf[prev.len] = '\0';

    expr->span.startline = prev.line;
    expr->span.startcol = prev.column;

    expr->span.endline = prev.line;
    expr->span.endcol = prev.column + prev.len;

    if (prev.type == INTEGER) {
        expr->Literal.type = TYPE_INTEGER;

        long long value = strtoll(buf, NULL, 10);

        expr->Literal.Value.int_value = value;
    }

    else if (prev.type == FLOAT) {
        expr->Literal.type = TYPE_FLOAT;

        double value = strtod(buf, NULL);

        expr->Literal.Value.float_value = value;
    }

    free(buf);
    return expr;
}

static Expression *grouping(Parser *p, Expression *left) {
    Token l_paren = next_token(p->scanner);
    Expression *expr = parsePrecedence(p, PREC_ASSIGNMENT);
    Token r_paren = consume(p, RIGHT_PAREN, "SyntaxError: Line %d column %d\nExpected ')' to close expression, got '%.*s'\n\nMaybe you forgot a closing ')'?", 202);

    Expression *final = malloc(sizeof(Expression));
    if (!final) 
        error_report(201, "MemoryError: Failed to allocate memory for AST node.\n");

    final->type = GROUPING;
    final->Grouping.Expr = expr;

    final->span.startline = l_paren.line;
    final->span.startcol = l_paren.column;

    final->span.endline = r_paren.line;
    final->span.endcol = r_paren.column + 1;

    return final;
}

static Expression *unary(Parser *p, Expression *left) {
    Token operator = next_token(p->scanner);
    Expression *expr = parsePrecedence(p, PREC_UNARY);

    return construct_unary(operator, expr);
}

static Expression *binary(Parser *p, Expression *left) {
    Token operator = next_token(p->scanner);
    ParseRule *rule = getRule(operator.type);
    Expression *right = parsePrecedence(p, (Precedence)(rule->precedence + 1));

    return construct_binary(left, operator, right);
}

static Expression *postfix(Parser *p, Expression *left) {    
    while (match(p, LEFT_PAREN))
    {
        left = finishCall(p, left);
    }

    if (check(p, PLUS_PLUS) || check(p, MINUS_MINUS))
    {
        Token operator = next_token(p->scanner);
        return construct_postfix(operator, left);
    }

    return left;
}

static Expression *assignment(Parser *p, Expression *left) {
    Token operator = next_token(p->scanner);
    Expression *value = parsePrecedence(p, PREC_ASSIGNMENT);

    if (left->type == VARIABLE)
    {
        Token identifier = left->Variable.identifier;
        return construct_assignment(identifier, value, operator);
    }
    else     
        error_report(202, "SyntaxError: Line %d column %d\nInvalid assignment target", left->span.startline, left->span.startcol);
    
    return left;
}

static Expression *identifier(Parser *p, Expression *left) {
    Expression *expr = malloc(sizeof(Expression));
    Token next = next_token(p->scanner);

    if (!expr)
        error_report(202, "MemoryError: Failed to allocate memory for AST node.\n");

    expr->type = VARIABLE;
    expr->Variable.identifier = next;

    expr->span.startline = next.line;
    expr->span.startcol = next.column;

    expr->span.endline = next.line;
    expr->span.endcol = next.column + next.len;

    return expr;
}

static Expression *boolean(Parser *p, Expression *left) {
    Token next = next_token(p->scanner);

    Expression *expr = malloc(sizeof(Expression));
    if (!expr)
        error_report(201, "MemoryError: Failed to allocate memory for AST node.");
    
    expr->type = LITERAL;

    if (next.type == TRUE) expr->Literal.type = TYPE_TRUE;
    else expr->Literal.type = TYPE_FALSE;

    expr->span.startline = next.line;
    expr->span.startcol = next.column;
    
    expr->span.endline = next.line;
    expr->span.endcol = next.column + next.len;

    return expr;
}

static Expression *nil(Parser *p, Expression *left) {
    Token next = next_token(p->scanner);
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

static Expression *parse_string(Parser *p, Expression *left) {
    Token next = next_token(p->scanner);
    Expression *expr = malloc(sizeof(Expression));

    if (!expr)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
        exit(201);
    }

    expr->type = LITERAL;

    expr->Literal.type = TYPE_STRING;
    expr->Literal.Value.lexeme = next.lexeme;
    expr->Literal.string_len = next.len;

    expr->span.startline = next.line;
    expr->span.startcol = next.column - 1;

    expr->span.endline = next.line;
    expr->span.endcol = next.column + next.len + 1;

    return expr;
}

static Expression *unexpected_end(Parser *p, Expression *expression) {
    error_report(202, "SyntaxError: Line %d column %d\nExpected expression, got <EOF>\n", p->scanner->line, p->scanner->column);
    return NULL;
}

static Expression *error(Parser *p, Expression *expression) {
    Token next = next_token(p->scanner);
    error_report(next.code, next.lexeme);
    return NULL;
} 

// Parse table
ParseRule rules[] = {
    [LEFT_PAREN] = {grouping, NULL, postfix, PREC_POSTFIX},
    [RIGHT_PAREN] = {NULL, NULL, NULL, PREC_NONE},
    [LEFT_BRACE] = {NULL, NULL, NULL, PREC_NONE},
    [RIGHT_BRACE] = {NULL, NULL, NULL, PREC_NONE},
    [COMMA] = {NULL, NULL, NULL, PREC_NONE},
    [DOT] = {NULL, NULL, NULL, PREC_NONE},
    [MINUS] = {unary, binary, NULL, PREC_TERM},
    [PLUS] = {NULL, binary, NULL, PREC_TERM},
    [SEMICOLON] = {NULL, NULL, NULL, PREC_NONE},
    [SLASH] = {NULL, binary, NULL, PREC_FACTOR},
    [STAR] = {NULL, binary, NULL, PREC_FACTOR},
    [LEFT_BRACKET] = {NULL, NULL, NULL, PREC_POSTFIX},
    [RIGHT_BRACKET] = {NULL, NULL, NULL, PREC_NONE},
    [PLUS_PLUS] = {NULL, NULL, postfix, PREC_POSTFIX},
    [MINUS_MINUS] = {NULL, NULL, postfix, PREC_POSTFIX},
    [NOT_EQUAL] = {NULL, binary, NULL, PREC_EQUALITY},
    [EQUAL] = {NULL, assignment, NULL, PREC_ASSIGNMENT},
    [EQUAL_EQUAL] = {NULL, binary, NULL, PREC_EQUALITY},
    [GREATER] = {NULL, binary, NULL, PREC_COMPARISON},
    [MOD] = {NULL, binary, NULL, PREC_FACTOR},
    [MOD_EQUAL] = {NULL, assignment, NULL, PREC_ASSIGNMENT},
    [GREATER_EQUAL] = {NULL, binary, NULL, PREC_COMPARISON},
    [LESS] = {NULL, binary, NULL, PREC_COMPARISON},
    [LESS_EQUAL] = {NULL, binary, NULL, PREC_COMPARISON},
    [PLUS_EQUAL] = {NULL, assignment, NULL, PREC_ASSIGNMENT},
    [MINUS_EQUAL] = {NULL, assignment, NULL, PREC_ASSIGNMENT},
    [STAR_EQUAL] = {NULL, assignment, NULL, PREC_ASSIGNMENT},
    [SLASH_EQUAL] = {NULL, assignment, NULL, PREC_ASSIGNMENT},
    [ARROW] = {NULL, NULL, NULL, PREC_NONE},
    [IDENTIFIER] = {identifier, NULL, NULL, PREC_PRIMARY},
    [STRING] = {parse_string, NULL, NULL, PREC_PRIMARY},
    [INTEGER] = {parse_number, NULL, NULL, PREC_PRIMARY},
    [FLOAT] = {parse_number, NULL, NULL, PREC_PRIMARY},
    [FUNC] = {NULL, NULL, NULL, PREC_NONE},
    [DATATYPE_INT] = {NULL, NULL, NULL, PREC_NONE},
    [DATATYPE_FLOAT] = {NULL, NULL, NULL, PREC_NONE},
    [DATATYPE_STRING] = {NULL, NULL, NULL, PREC_NONE},
    [DATATYPE_BOOL] = {NULL, NULL, NULL, PREC_NONE},
    [DATATYPE_VOID] = {NULL, NULL, NULL, PREC_NONE},
    [LET] = {NULL, NULL, NULL, PREC_NONE},
    [AND] = {NULL, binary, NULL, PREC_AND},
    [OR] = {NULL, binary, NULL, PREC_OR},
    [IF] = {NULL, NULL, NULL, PREC_NONE},
    [ELSE] = {NULL, NULL, NULL, PREC_NONE},
    [FOR] = {NULL, NULL, NULL, PREC_NONE},
    [NIL] = {nil, NULL, NULL, PREC_PRIMARY},
    [RETURN] = {NULL, NULL, NULL, PREC_NONE},
    [WHILE] = {NULL, NULL, NULL, PREC_NONE},
    [TRUE] = {boolean, NULL, NULL, PREC_PRIMARY},
    [FALSE] = {boolean, NULL, NULL, PREC_PRIMARY},
    [NOT] = {unary, NULL, NULL, PREC_UNARY},
    [TOKEN_EOF] = {unexpected_end, NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {error, NULL, NULL, PREC_NONE},
};

static Expression *parsePrecedence(Parser *p, Precedence precedence) {
    Token next = peek_token(p->scanner);
    ParseFn prefixRule = getRule(next.type)->prefix;

    if (!prefixRule)
        error_report(202, "SyntaxError: Line %d column %d\nExpected expression, got '%.*s'", next.line, next.column, next.len, next.lexeme);
    
    Expression *expr = prefixRule(p, NULL);

    next = peek_token(p->scanner);
    ParseRule *rule = getRule(next.type);

    while (precedence <= rule->precedence) {
        ParseFn nextRule = rule->infix;
        if (!nextRule) nextRule = rule->postfix;
        if (!nextRule) break;
        
        expr = nextRule(p, expr);
        next = peek_token(p->scanner);
        rule = getRule(next.type);
    }

    return expr;
}

static inline ParseRule *getRule(TokenType type) {
    return &rules[type];
}

// Debugging
/*int main()
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

        if (statement->varDecl->initializer != NULL)
        {
            printf("Initializer: ");
            output_expression(statement->varDecl->initializer);
            printf("\n");
        }
        break;
    }
    case TYPE_FUNCDECL:
    {
        printf("Function declaration\n");
        printf("Identifier: %.*s\n", statement->funcDecl->identifier.len, statement->funcDecl->identifier.lexeme);
        printf("Parameter count: %d\n", statement->funcDecl->arity);

        if (statement->funcDecl->parameters != NULL)
        {
            printf("Parameters: \n");
            for (int i = 0; i < statement->funcDecl->arity; i++)
            {
                printf("%.*s\n", statement->funcDecl->parameters[i].identifier.len, statement->funcDecl->parameters[i].identifier.lexeme);
            }
        }

        printf("Body: ");
        output_block(statement->funcDecl->body);

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

        if (statement->ifStmt->elseBranch != NULL)
        {
            printf("Else branch: ");
            output_statement(statement->ifStmt->elseBranch);
        }
        break;
    }
    case TYPE_WHILE:
    {
        printf("While statement\n");
        printf("Condition: ");
        output_expression(statement->whileStmt->condition);
        printf("\n");
        printf("Body: ");
        output_block(statement->whileStmt->body->block);
        printf("\n");
        break;
    }
    case TYPE_RETURN:
    {
        printf("return ");
        if (statement->ReturnStmt->value != NULL) 
            output_expression(statement->ReturnStmt->value);
        else
            printf("nil");
        
        printf("\n");
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
        break;
    }
    case VARIABLE:
    {
        printf("%.*s", expr->Variable.identifier.len, expr->Variable.identifier.lexeme);
        break;
    }
    case ASSIGNMENT:
    {
        printf("%.*s %.*s ", expr->Assignment.identifier.len, expr->Assignment.identifier.lexeme, expr->Assignment.operator.len, expr->Assignment.operator.lexeme);
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
    case CALL:
    {
        printf("Call\n");
        printf("Callee: ");
        output_expression(expr->Call.callee);
        if (expr->Call.argCount != 0)
        {
            printf("\nArguments: ");

            for (int i = 0; i < expr->Call.argCount; i++)
            {
                output_expression(expr->Call.arguments[i]);
                printf(",");
            }
        }
        printf("\nArgument Count: ");
        printf("%d\n", expr->Call.argCount);

        break;
    }
    }
} */

int check(Parser *p, TokenType type)
{
    return peek_token(p->scanner).type == type;
}

int match(Parser *p, TokenType type)
{
    int result = check(p, type);

    if (result)
        next_token(p->scanner);

    return result;
}

Token consume(Parser *p, TokenType type, const char *message, int errorCode)
{
    if (!check(p, type))
    {
        Token offending = peek_token(p->scanner);
        error_report(errorCode, message, offending.line, offending.column, offending.len, offending.lexeme);
    }

    return next_token(p->scanner);
}

Parser *init_parser(Scanner *s)
{
    Parser *parser = malloc(sizeof(Parser));

    if (!parser)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for Parser.\n");
        exit(200);
    }

    parser->scanner = s;

    return parser;
}

Expression *construct_binary(Expression *left, Token operator, Expression *right)
{
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

    expression->inferred = TYPE_UNKNOWN;

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
        
    expression->inferred = TYPE_UNKNOWN;

    expression->span.startline = left->span.startline;
    expression->span.startcol = left->span.startcol;
    expression->span.endline = right->span.endline;
    expression->span.endcol = right->span.endcol;

    return expression;
}

Expression *construct_unary(Token operator, Expression *right)
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

    expression->inferred = TYPE_UNKNOWN;

    expression->span.startline = operator.line;
    expression->span.startcol = operator.column + operator.len;
    expression->span.endline = right->span.endline;
    expression->span.endcol = right->span.endcol;

    return expression;
}

Expression *construct_postfix(Token operator, Expression *left)
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

    expression->inferred = TYPE_UNKNOWN;

    expression->span.startline = left->span.startline;
    expression->span.startcol = left->span.startcol;
    expression->span.endline = operator.line;
    expression->span.endcol = operator.column;

    return expression;
}

Expression *construct_assignment(Token identifier, Expression *value, Token operator)
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
    expression->Assignment.operator = operator;

    expression->inferred = TYPE_UNKNOWN;

    expression->span.startline = identifier.line;
    expression->span.startcol = identifier.column;
    expression->span.endline = value->span.endline;
    expression->span.endcol = value->span.endcol;

    return expression;
}

Expression *construct_call(Expression *callee, Expression **args, int argCount, Token right_paren)
{
    Expression *expression = malloc(sizeof(Expression));
    if (!expression)
    {
        fprintf(stderr, "MemoryError: Failed to allocate memory for AST node.\n");
        exit(201);
    }

    expression->type = CALL;
    expression->Call.callee = callee;
    expression->Call.arguments = args;
    expression->Call.argCount = argCount;

    expression->inferred = TYPE_UNKNOWN;

    expression->span.startline = callee->span.startline;
    expression->span.startcol = callee->span.startcol;

    expression->span.endline = right_paren.line;
    expression->span.endcol = right_paren.column;

    return expression;
}

Expression *finishCall(Parser *p, Expression *callee)
{
    Expression **args = NULL;
    int count = 0;

    if (!check(p, RIGHT_PAREN))
    {
        size_t capacity = 4;

        args = calloc(capacity, sizeof(Expression *));

        do
        {
            if (count > 255)
                error_report(202, "SyntaxError: line %d column %d\nCalls may not pass more than 255 arguments.", args[255]->span.startline, args[255]->span.startcol);
            if (count == capacity)
            {
                capacity *= 2;
                Expression **temp = realloc(args, capacity * sizeof(Expression *));

                if (!temp)
                    error_report(201, "MemoryError: Failed to allocate memory for AST node.");

                args = temp;
            }
            args[count++] = parsePrecedence(p, PREC_ASSIGNMENT);
        } while (match(p, COMMA));
    }

    Token right_paren = consume(p, RIGHT_PAREN, "SyntaxError: Line %d column %d\nExpected ')' after function arguments, got '%.*s'\n\nMaybe you forgot a comma?", 202);
    return construct_call(callee, args, count, right_paren);
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

        stmt->span.startline = identifier.line;
        stmt->span.startcol = identifier.column;

        stmt->span.endline = semicolon.line;
        stmt->span.endcol = semicolon.column;

        break;
    }
    case TYPE_FUNCDECL:
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

        Statement *body = va_arg(args, Statement *);
        stmt->whileStmt = while_stmt;
        stmt->whileStmt->body = body;
        stmt->whileStmt->condition = expr;

        stmt->span.startline = semicolon.line;
        stmt->span.startcol = semicolon.column;

        stmt->span.endline = body->span.endline;
        stmt->span.endcol = body->span.endcol;

        break;
    }
    case TYPE_RETURN:
    {
        ReturnStmt *return_stmt = malloc(sizeof(ReturnStmt));
        if (!return_stmt)
            error_report(202, "MemoryError: Unable to allocate memory for AST node.");
        
        Token return_token = va_arg(args, Token);

        stmt->ReturnStmt = return_stmt;
        stmt->ReturnStmt->value = expr;
        
        stmt->span.startline = return_token.line;
        stmt->span.startcol = return_token.column;

        stmt->span.endline = semicolon.line;
        stmt->span.endcol = semicolon.column;

        break;
    }
    }

    return stmt;
}

Statement *variable_declaration(Parser *p)
{
    TokenType type = next_token(p->scanner).type;

    Token identifier = consume(p, IDENTIFIER, "SyntaxError: Line %d column %d\nExpected identifier, got '%.*s'", 202);
    Expression *initializer = NULL;

    if (check(p, EQUAL))
    {
        next_token(p->scanner);
        initializer = parsePrecedence(p, PREC_ASSIGNMENT);
    }

    Token next = consume(p, SEMICOLON, "SyntaxError: Line %d column %d\nExpected ';' after declaration, got '%.*s'\n\nMaybe you forgot a semicolon?", 202);
    return construct_statement(initializer, next, TYPE_VARDECL, identifier, type);
}

Statement *function_declaration(Parser *p)
{
    Token func_token = next_token(p->scanner);

    Token identifier = consume(p, IDENTIFIER, "SyntaxError: Line %d column %d\nExpected identifier after 'func', got '%.*s'", 202);
    consume(p, LEFT_PAREN, "SyntaxError: Line %d column %d\nExpected '(' after function name, got '%.*s'\nMaybe you forgot an opening '('?", 202);

    Parameter *parameters = NULL;
    int count = 0;

    if (!check(p, RIGHT_PAREN))
    {
        size_t capacity = 4;
        parameters = calloc(capacity, sizeof(Parameter));

        if (!parameters)
            error_report(201, "MemoryError: Failed to allocate memory for AST node.");

        do
        {
            if (count > 255)
                error_report(202, "SyntaxError: Line %d column %d\nFunctions may not have more than 255 arguments.", parameters[255].identifier.line, parameters[255].identifier.column);

            if (count == capacity)
            {
                capacity *= 2;
                Parameter *temp = realloc(parameters, capacity * sizeof(Parameter));

                if (!temp)
                    error_report(201, "MemoryError: Failed to allocate memory for AST node.");

                parameters = temp;
            }

            Token next = peek_token(p->scanner);
            if (next.type != DATATYPE_BOOL && next.type != DATATYPE_FLOAT && next.type != DATATYPE_INT && next.type != DATATYPE_STRING && next.type != DATATYPE_VOID)
                error_report(202, "SyntaxError: Line %d column %d\nExpected datatype of parameter, got '%.*s'", next.line, next.column, next.len, next.lexeme);

            next_token(p->scanner);

            Token identifier = consume(p, IDENTIFIER, "SyntaxError: Line %d column %d\nExpected parameter name after datatype, got '%.*s'", 202);

            Parameter newParameter;

            newParameter.datatype = next.type;
            newParameter.identifier = identifier;
            parameters[count++] = newParameter;
        } while (match(p, COMMA));
    }

    Token next = peek_token(p->scanner);
    if (next.type != RIGHT_PAREN)
    {
        if (next.type == DATATYPE_BOOL || next.type == DATATYPE_FLOAT || next.type == DATATYPE_INT || next.type == DATATYPE_STRING || next.type == DATATYPE_VOID)
            error_report(202, "SyntaxError: Line %d column %d\nExpected ')' after function parameters, got '%.*s'\n\nMaybe you forgot a comma between parameters?", next.line, next.column, next.len, next.lexeme);
        else
            error_report(202, "SyntaxError: Line %d column %d\nExpected ')' after function parameters, got '%.*s'\n\nMaybe you forgot a closing ')'?", next.line, next.column, next.len, next.lexeme);
    }

    next_token(p->scanner);
    consume(p, ARROW, "SyntaxError: Line %d column %d\nExpected '->' after function parameters, got '%.*s'\n\nMaybe you forgot a '->' after ')'?", 202);

    Token returnType = peek_token(p->scanner);
    if (returnType.type != DATATYPE_BOOL && returnType.type != DATATYPE_FLOAT && returnType.type != DATATYPE_INT && returnType.type != DATATYPE_STRING && returnType.type != DATATYPE_VOID)
        error_report(202, "SyntaxError: Line %d column %d\nExpected return datatype after '->', got '%.*s'", returnType.line, returnType.column, returnType.len, returnType.lexeme);
    next_token(p->scanner);

    if (!check(p, LEFT_BRACE))
        error_report(202, "SyntaxError: Line %d column %d\nExpected '{' after function return type, got '%.*s'\n\nMaybe you forgot an opening '{'?");

    Statement *body = construct_block(p);

    Statement *final = malloc(sizeof(Statement));
    if (!final)
        error_report(201, "MemoryError: Failed to allocate memory for AST node.");

    final->type = TYPE_FUNCDECL;

    FuncDecl *declaration = malloc(sizeof(FuncDecl));
    if (!declaration)
        error_report(201, "MemoryError: Failed to allocate memory for AST node.");

    declaration->parameters = parameters;
    declaration->arity = count;
    declaration->returnType = returnType.type;
    declaration->identifier = identifier;
    declaration->body = body->block;

    final->funcDecl = declaration;

    final->span.startline = func_token.line;
    final->span.startcol = func_token.column;

    final->span.endline = body->span.endline;
    final->span.endcol = body->span.endcol;

    return final;
}

Statement *construct_block(Parser *p)
{
    Token left_brace = next_token(p->scanner);

    size_t capacity = 4;
    Statement **statements = calloc(capacity, sizeof(Statement *));

    if (!statements)
        error_report(201, "MemoryError: Failed to allocate memory for AST node.");

    size_t count = 0;
    while (!check(p, RIGHT_BRACE) && !check(p, TOKEN_EOF))
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
    }

    Token next = consume(p, RIGHT_BRACE, "SyntaxError: Line %d column %d\nExpected '}', got '%.*s'\n\nMaybe you forgot a closing '}'?", 202);
    Statement *block = construct_statement(NULL, next, TYPE_BLOCK, left_brace, statements, count);

    return block;
}

Statement *expression_statement(Parser *p)
{
    Expression *expr = parsePrecedence(p, PREC_ASSIGNMENT);
    Token next = peek_token(p->scanner);

    if (match(p, SEMICOLON))
        return construct_statement(expr, next, TYPE_EXPR);
    else
        error_report(202, "SyntaxError: Line %d column %d\nExpected ';', got '%.*s'", next.line, next.column, next.len, next.lexeme);

    return NULL;
}

// I chose to enforce braces to avoid the dangling else problem
Statement *if_statement(Parser *p)
{
    Token if_tok = next_token(p->scanner);

    consume(p, LEFT_PAREN, "SyntaxError: Line %d column %d\nExpected '(' after 'if', got '%.*s'\n\nMaybe you forgot an opening '(' after 'if'?", 202);
    Expression *condition = parsePrecedence(p, PREC_ASSIGNMENT);

    consume(p, RIGHT_PAREN, "SyntaxError: Line %d column %d\nExpected ')' after if condition, got '%.*s'\n\nMaybe you forgot a closing ')'?", 202);

    Token next = peek_token(p->scanner);
    if (!check(p, LEFT_BRACE))
        error_report(202, "SyntaxError: Line %d column %d\nExpected '{' after if condition, got '%.*s'\n\nBlocks in Atomic must be enclosed in braces.", next.line, next.column, next.len, next.lexeme);

    Statement *then_branch = construct_block(p);
    Statement *else_branch = NULL;

    if (match(p, ELSE))
    {
        next = peek_token(p->scanner);

        if (!check(p, LEFT_BRACE) && !check(p, IF))
            error_report(202, "SyntaxError: Line %d column %d\nExpected '{' or 'if' after 'else', got '%.*s'\n\nBlocks in Atomic must be enclosed in braces.", next.line, next.column, next.len, next.lexeme);

        if (check(p, LEFT_BRACE))
            else_branch = construct_block(p);
        else
            else_branch = if_statement(p);
    }

    return construct_statement(condition, if_tok, TYPE_IF, then_branch, else_branch);
}

// While statement
Statement *while_statement(Parser *p)
{
    Token while_token = next_token(p->scanner);

    consume(p, LEFT_PAREN, "SyntaxError: Line %d column %d\nExpected '(' after 'while', got '%.*s'", 202);
    Expression *condition = parsePrecedence(p, PREC_ASSIGNMENT);

    consume(p, RIGHT_PAREN, "SyntaxError: Line %d column %d\nExpected ')' after while condition, got '%.*s'", 202);

    Token next = peek_token(p->scanner);
    if (!check(p, LEFT_BRACE))
        error_report(202, "SyntaxError: Line %d column %d\nExpected '{' after if condition, got '%.*s'\n\nBlocks in Atomic must be enclosed in braces.", next.line, next.column, next.len, next.lexeme);

    Statement *body = construct_block(p);
    return construct_statement(condition, while_token, TYPE_WHILE, body);
}

// for loops
Statement *for_statement(Parser *p)
{
    Token for_token = next_token(p->scanner);
    consume(p, LEFT_PAREN, "SyntaxError: Line %d column %d\nExpected '(' after 'for', got '%.*s'", 202);

    Statement *initializer = NULL;
    Expression *condition = NULL;
    Expression *increment = NULL;

    Token next = peek_token(p->scanner);
    if (next.type == SEMICOLON)
    {
        next_token(p->scanner);
        initializer = NULL;
    }
    else if (next.type == DATATYPE_BOOL || next.type == DATATYPE_FLOAT || next.type == DATATYPE_INT || next.type == DATATYPE_STRING || next.type == DATATYPE_VOID)
    {
        initializer = variable_declaration(p);
    }
    else
    {
        initializer = expression_statement(p);
    }

    if (!check(p, SEMICOLON))
        condition = parsePrecedence(p, PREC_ASSIGNMENT);

    consume(p, SEMICOLON, "SyntaxError: Line %d column %d\nExpected ';' after loop condition, got '%.*s'", 202);

    if (!check(p, RIGHT_PAREN))
    {
        increment = parsePrecedence(p, PREC_ASSIGNMENT);
    }

    Token right_paren = consume(p, RIGHT_PAREN, "SyntaxError: Line %d column %d\nExpected ')' after for conditions, got '%.*s'", 202);

    if (!check(p, LEFT_BRACE))
    {
        Token offending = peek_token(p->scanner);
        error_report(202, "SyntaxError: Line %d column %d\nExpected '{' after for loop conditions, got '%.*s'\n\nBlocks in Atomic must be enclosed in braces.", offending.line, offending.column, offending.len, offending.lexeme);
    }

    Statement *body = construct_block(p);

    if (increment != NULL)
    {
        Statement **newStmts = realloc(body->block->statements, (body->block->count + 1) * sizeof(Statement *));

        if (!newStmts)
            error_report(201, "MemoryError: Failed to allocate memory for AST node.");
        
        Statement *incrementStmt = construct_statement(increment, right_paren, TYPE_EXPR);
        newStmts[body->block->count++] = incrementStmt;

        body->block->statements = newStmts;
    }
    if (!condition)
    {
        Expression *trueNode = malloc(sizeof(Expression));

        if (!trueNode)
            error_report(201, "MemoryError: Failed to allocate memory for AST node.");

        trueNode->Literal.type = TYPE_TRUE;
        trueNode->type = LITERAL;

        condition = trueNode;
    }

    Statement *body_while = malloc(sizeof(Statement));
    if (!body_while)
        error_report(201, "MemoryError: Failed to allocate memory for AST node.");

    WhileStmt *whileStmt = malloc(sizeof(WhileStmt));
    if (!whileStmt)
        error_report(201, "MemoryError: Failed to allocate memory for AST node.");

    body_while->type = TYPE_WHILE;
    body_while->whileStmt = whileStmt;
    body_while->whileStmt->body = body;
    body_while->whileStmt->condition = condition;

    Statement *final;
    if (initializer != NULL)
    {
        final = malloc(sizeof(Statement));
        if (!final)
            error_report(201, "MemoryError: Failed to allocate memory for AST node.");

        Block *finalBlock = malloc(sizeof(Block));
        if (!finalBlock)
            error_report(201, "MemoryError: Failed to allocate memory for AST node.");

        final->type = TYPE_BLOCK;
        final->block = finalBlock;
        final->block->count = 2;

        Statement **finalStmts = calloc(2, sizeof(Statement *));
        if (!finalStmts)
            error_report(201, "MemoryError: Failed to allocate memory for AST node.");

        finalStmts[0] = initializer;
        finalStmts[1] = body_while;

        final->block->statements = finalStmts;
    }
    else
        final = body_while;

    final->span.startline = for_token.line;
    final->span.startcol = for_token.column;
    final->span.endline = body->span.endline;
    final->span.endcol = body->span.endcol;

    return final;
}

Statement *return_statement(Parser *p) {
    Token return_token = next_token(p->scanner);
    Expression *value = NULL;

    if (!check(p, SEMICOLON))
        value = parsePrecedence(p, PREC_ASSIGNMENT);
    
    Token semicolon = consume(p, SEMICOLON, "SyntaxError: Line %d column %d\nExpected ';', got '%.*s'\nMaybe you forgot a ';' to end the statement?", 202);
    return construct_statement(value, semicolon, TYPE_RETURN, return_token);
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
        error_report(201, "MemoryError: Failed to allocate memory for Statement Array.");

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
    if (next.type == LET || next.type == DATATYPE_FLOAT || next.type == DATATYPE_INT || next.type == DATATYPE_STRING || next.type == DATATYPE_BOOL || next.type == DATATYPE_VOID)
        return variable_declaration(p);
    else if (next.type == FUNC)
        return function_declaration(p);

    return statement(p);
}

Statement *statement(Parser *p)
{
    Token next = peek_token(p->scanner);

    switch (next.type)
    {
    case LEFT_BRACE:
        return construct_block(p);
    case IF:
        return if_statement(p);
    case WHILE:
        return while_statement(p);
    case FOR:
        return for_statement(p);
    case RETURN:
        return return_statement(p);
    default:
        return expression_statement(p);
    }

    return NULL;
}