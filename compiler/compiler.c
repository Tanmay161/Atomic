// #include "parser.h"
// #include "ast.h"
// #include "token.h"
#include "chunk.h"
#include "debug.h"
#include "ast.h"
#include "compiler.h"
#include "error.h"
#include "value.h"
#include "object.h"
#include "vm.h"

#include <stdlib.h>

OpCode tok_to_code_binary[] = {
    [PLUS] = OP_ADD,
    [MINUS] = OP_SUBTRACT,
    [STAR] = OP_MULTIPLY,
    [SLASH] = OP_DIVIDE,
    [MOD] = OP_MOD,
    [EQUAL_EQUAL] = OP_EQUAL,
    [NOT_EQUAL] = OP_NOT_EQUAL,
    [GREATER] = OP_GREATER,
    [GREATER_EQUAL] = OP_GREATER_EQUAL,
    [LESS] = OP_LESS,
    [LESS_EQUAL] = OP_LESS_EQUAL,
};

OpCode tok_to_code_unary[] = {
    [MINUS] = OP_NEGATE,
    [NOT] = OP_NOT,
};

ValueType type_to_val_type[] = {
    [TYPE_INTEGER] = VAL_INT,
    [TYPE_FLOAT] = VAL_FLOAT,
    [TYPE_TRUE] = VAL_BOOL,
    [TYPE_FALSE] = VAL_BOOL,
    [TYPE_STRING] = VAL_OBJ,
};

Chunk *compile(Compiler *compiler);
static void compile_expression(Compiler *compiler, Expression *expr);

static void compile_expression(Compiler *compiler, Expression *expr)
{
    switch (expr->type)
    {
    case BINARY:
    {
        compile_expression(compiler, expr->Binary.Left);
        compile_expression(compiler, expr->Binary.Right);

        writeChunk(compiler->currentChunk, tok_to_code_binary[expr->Binary.Operator.type], expr->span);
        break;
    }
    case LITERAL:
    {
        LiteralType type = expr->Literal.type;
        Value value = {.type = type_to_val_type[type]};

        // check if its a constant
        switch (type)
        {
        case TYPE_INTEGER:
            value.int_val = expr->Literal.Value.int_value;
            break;
        case TYPE_FLOAT:
            value.float_val = expr->Literal.Value.float_value;
            break;
        case TYPE_TRUE:
            writeChunk(compiler->currentChunk, OP_TRUE, expr->span);
            return;
        case TYPE_FALSE:
            writeChunk(compiler->currentChunk, OP_FALSE, expr->span);
            return;
        case TYPE_NIL:
            writeChunk(compiler->currentChunk, OP_NIL, expr->span);
            return;
        case TYPE_STRING:
            value.obj = (Obj *) allocateString(compiler->vm, expr->Literal.Value.lexeme, expr->Literal.string_len);
            free(expr->Literal.Value.lexeme);
            break;
        }

        int index = addConstant(compiler->currentChunk, value);
        writeChunk(compiler->currentChunk, OP_CONSTANT, expr->span);
        writeU16(compiler->currentChunk, index, expr->span);

        break;
    }
    case UNARY:
        compile_expression(compiler, expr->Unary.Expr);
        writeChunk(compiler->currentChunk, tok_to_code_unary[expr->Unary.Operator.type], expr->span);
        break;
    case GROUPING:
        compile_expression(compiler, expr->Grouping.Expr);
        break;
    }
}

Chunk *compile(Compiler *compiler)
{
    Program *source = compiler->source;

    for (int i = 0; i < source->count; i++)
    {
        Statement *stmt = source->statements[i];

        switch (stmt->type)
        {
        case TYPE_EXPR:
            compile_expression(compiler, stmt->exprStmt->expr);
        }
    }

    SourceSpan span;
    span = source->statements[source->count-1]->span;

    writeChunk(compiler->currentChunk, OP_RETURN, span);
    return compiler->currentChunk;
}

Compiler *init_compiler(VM *vm, Program *source)
{
    Compiler *compiler = malloc(sizeof(Compiler));

    if (!compiler)
        error_report(400, "MemoryError: Failed to initialize compiler");

    Chunk *chunk = malloc(sizeof(Chunk));
    if (!chunk)
        error_report(401, "MemoryError: Failed to initialize chunk");

    initChunk(chunk);

    compiler->source = source;
    compiler->currentChunk = chunk;
    compiler->vm = vm;

    return compiler;
}

void free_compiler(Compiler *compiler)
{
    free(compiler->currentChunk);
    free(compiler);
}