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
#include "token.h"

#include <stdlib.h>
#include <string.h>

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

static void addLocal(Compiler *compiler, Token token);
static void compile_statement(Compiler *compiler, Statement *stmt);

Chunk *compile(Compiler *compiler);
static void compile_expression(Compiler *compiler, Expression *expr);

static int emitJump(Compiler *compiler, uint8_t instruction, SourceSpan span)
{
    writeChunk(compiler->currentChunk, instruction, span);
    writeChunk(compiler->currentChunk, 0xFF, span);
    writeChunk(compiler->currentChunk, 0xFF, span);

    return compiler->currentChunk->count - 2;
}

static void patchJump(Compiler *compiler, int offset, SourceSpan span)
{
    int jump = compiler->currentChunk->count - offset - 2;

    if (jump > UINT16_MAX)
        error_report(404, "CompileError: Line %d column %d\nToo many instructions for conditional jump.", span.startline, span.startcol);

    compiler->currentChunk->code[offset] = jump & 0xFF;
    compiler->currentChunk->code[offset + 1] = (jump >> 8) & 0xFF;
}

static void emitLoop(Compiler *compiler, int start, SourceSpan span) {
    writeChunk(compiler->currentChunk, OP_LOOP, span);

    int jump = compiler->currentChunk->count - start + 2;

    if (jump > UINT16_MAX) 
        error_report(405, "CompileError: Line %d column %d\nLoop body too large.", span.startline, span.startcol);

    writeU16(compiler->currentChunk, jump, span);
}

static void declareLocal(Compiler *compiler, Token tok)
{
    for (int i = compiler->localCount - 1; i >= 0; i--)
    {
        Local *local = &compiler->locals[i];
        if (local->depth < compiler->scopeDepth)
        {
            break;
        }

        if (tok.len == local->name.len && (memcmp(tok.lexeme, local->name.lexeme, tok.len) == 0))
        {
            error_report(402, "SyntaxError: Line %d column %d\nAlready a variable with this name in this scope.", tok.line, tok.column);
        }
    }

    addLocal(compiler, tok);
}

static void addLocal(Compiler *compiler, Token token)
{
    if (compiler->localCount > UINT16_COUNT)
    {
        error_report(403, "MemoryError: Line %d column %d\nToo many local variables in one function.", token.line, token.column);
    }
    Local *local = &compiler->locals[compiler->localCount++];
    local->name = token;
    local->depth = compiler->scopeDepth;
}

static int resolveLocal(Compiler *compiler, Token name)
{
    for (int i = compiler->localCount - 1; i >= 0; i--)
    {
        Local *local = &compiler->locals[i];
        if (local->name.len == name.len && memcmp(local->name.lexeme, name.lexeme, name.len) == 0)
        {
            return i;
        }
    }

    return -1;
}

static void namedVariable(Compiler *compiler, Token name, int canAssign, SourceSpan span)
{
    uint8_t setOp, getOp;
    int arg = resolveLocal(compiler, name);

    if (arg != -1)
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else
    {
        Value val;
        val.type = VAL_OBJ;
        val.obj = (Obj *)allocateString(compiler->vm, name.lexeme, name.len);

        arg = addConstant(compiler->currentChunk, val);
        setOp = OP_SET_GLOBAL;
        getOp = OP_GET_GLOBAL;
    }

    if (canAssign == 1)
    {
        writeChunk(compiler->currentChunk, setOp, span);
        writeU16(compiler->currentChunk, arg, span);
    }
    else if (canAssign == 2)
    {
        writeChunk(compiler->currentChunk, OP_DEFINE_GLOBAL, span);
        writeU16(compiler->currentChunk, arg, span);
    }
    else
    {
        writeChunk(compiler->currentChunk, getOp, span);
        writeU16(compiler->currentChunk, arg, span);
    }
}

static void compile_and(Compiler *compiler, Expression *expr) {
    compile_expression(compiler, expr->Binary.Left);
    int endJump = emitJump(compiler, OP_JUMP_IF_FALSE, expr->span);
    writeChunk(compiler->currentChunk, OP_POP, expr->span);

    compile_expression(compiler, expr->Binary.Right);
    patchJump(compiler, endJump, expr->span);
}

static void compile_or(Compiler *compiler, Expression *expr) {
    compile_expression(compiler, expr->Binary.Left);

    int endJump = emitJump(compiler, OP_JUMP_IF_TRUE, expr->span);
    writeChunk(compiler->currentChunk, OP_POP, expr->span);

    compile_expression(compiler, expr->Binary.Right);
    patchJump(compiler, endJump, expr->span);
}

static void compile_expression(Compiler *compiler, Expression *expr)
{
    switch (expr->type)
    {
    case BINARY:
    {
        if (expr->Binary.Operator.type == AND) {
            compile_and(compiler, expr);
            return;
        } else if (expr->Binary.Operator.type == OR) {
            compile_or(compiler, expr);
            return;
        }

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
            value.obj = (Obj *)allocateString(compiler->vm, expr->Literal.Value.lexeme, expr->Literal.string_len);
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
    case VARIABLE:
    {
        namedVariable(compiler, expr->Variable.identifier, 0, expr->span);
        break;
    }
    case ASSIGNMENT:
    {
        if (expr->Assignment.operator.type != EQUAL)
        {
            namedVariable(compiler, expr->Assignment.identifier, 0, expr->span);
        }

        compile_expression(compiler, expr->Assignment.value);

        switch (expr->Assignment.operator.type)
        {
        case EQUAL:
            break;
        case PLUS_EQUAL:
            writeChunk(compiler->currentChunk, OP_ADD, expr->span);
            break;
        case MINUS_EQUAL:
            writeChunk(compiler->currentChunk, OP_SUBTRACT, expr->span);
            break;
        case STAR_EQUAL:
            writeChunk(compiler->currentChunk, OP_MULTIPLY, expr->span);
            break;
        case SLASH_EQUAL:
            writeChunk(compiler->currentChunk, OP_DIVIDE, expr->span);
            break;
        case MOD_EQUAL:
            writeChunk(compiler->currentChunk, OP_MOD, expr->span);
            break;
        }

        namedVariable(compiler, expr->Assignment.identifier, 1, expr->span);
        break;
    }
    }
}

static void compile_vardecl(Compiler *compiler, Statement *stmt)
{
    VarDecl *decl = stmt->varDecl;

    Token name = {
        .lexeme = decl->name,
        .len = decl->len,
        .type = decl->type};

    if (decl->initializer == NULL)
        writeChunk(compiler->currentChunk, OP_NIL, stmt->span);
    else
        compile_expression(compiler, decl->initializer);

    if (compiler->scopeDepth > 0)
    {
        addLocal(compiler, name);
        namedVariable(compiler, name, 1, stmt->span);
        return;
    }
        
    namedVariable(compiler, name, 2, stmt->span);
}

static void compile_block(Compiler *compiler, Statement *stmt)
{
    Block *block = stmt->block;
    compiler->scopeDepth++;

    for (int i = 0; i < block->count; i++)
    {
        compile_statement(compiler, block->statements[i]);
    }

    compiler->scopeDepth--;

    uint16_t count = 0;
    while (compiler->localCount > 0 && compiler->locals[compiler->localCount - 1].depth > compiler->scopeDepth)
    {
        count++;
        compiler->localCount--;
    }

    if (count > 0)
    {
        writeChunk(compiler->currentChunk, OP_POPN, stmt->span);
        writeU16(compiler->currentChunk, count, stmt->span);
    }
}

static void compile_if(Compiler *compiler, Statement *statement)
{
    IfStmt *stmt = statement->ifStmt;
    compile_expression(compiler, stmt->condition);

    int jumpOverThen = emitJump(compiler, OP_JUMP_IF_FALSE, stmt->condition->span);
    writeChunk(compiler->currentChunk, OP_POP, statement->span);
    compile_statement(compiler, stmt->thenBranch);

    int jumpOverElse = emitJump(compiler, OP_JUMP, stmt->condition->span);
    patchJump(compiler, jumpOverThen, stmt->condition->span);

    writeChunk(compiler->currentChunk, OP_POP, statement->span);

    if (stmt->elseBranch != NULL) {
        compile_statement(compiler, stmt->elseBranch);
    }

    patchJump(compiler, jumpOverElse, stmt->condition->span);
}

static void compile_while(Compiler *compiler, Statement *statement) {
    WhileStmt *stmt = statement->whileStmt;
    int loopStart = compiler->currentChunk->count;

    compile_expression(compiler, stmt->condition);

    int exitLoop = emitJump(compiler, OP_JUMP_IF_FALSE, stmt->condition->span);
    writeChunk(compiler->currentChunk, OP_POP, stmt->condition->span);
    compile_block(compiler, stmt->body);
    emitLoop(compiler, loopStart, stmt->body->span);

    patchJump(compiler, exitLoop, stmt->condition->span);
    writeChunk(compiler->currentChunk, OP_POP, stmt->condition->span);
}

static void compile_statement(Compiler *compiler, Statement *stmt)
{
    switch (stmt->type)
    {
    case TYPE_EXPR:
        compile_expression(compiler, stmt->exprStmt->expr);
        writeChunk(compiler->currentChunk, OP_POP, stmt->exprStmt->expr->span);
        break;
    case TYPE_VARDECL:
        compile_vardecl(compiler, stmt);
        break;
    case TYPE_BLOCK:
        compile_block(compiler, stmt);
        break;
    case TYPE_IF:
        compile_if(compiler, stmt);
        break;
    case TYPE_WHILE:
        compile_while(compiler, stmt);
        break;
    }
}

Chunk *compile(Compiler *compiler)
{
    Program *source = compiler->source;

    for (int i = 0; i < source->count; i++)
    {
        compile_statement(compiler, source->statements[i]);
    }

    SourceSpan span;
    span = source->statements[source->count - 1]->span;

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