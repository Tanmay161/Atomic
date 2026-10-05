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
#include "memory.h"

#include <stdlib.h>
#include <string.h>

#define DEBUG_FLAG

OpCode tok_to_code_operators[] = {
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
    [PLUS_PLUS] = OP_INCREMENT,
    [MINUS_MINUS] = OP_DECREMENT,
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

static void compile_block(Compiler *compiler, Statement *stmt);

ObjFunction *compile(Compiler *compiler);
static void compile_expression(Compiler *compiler, Expression *expr);

static Chunk *currentChunk(Compiler *compiler) {
    return &compiler->function->chunk;
}

static int emitJump(Compiler *compiler, uint8_t instruction, SourceSpan span)
{
    writeChunk(currentChunk(compiler), instruction, span);
    writeChunk(currentChunk(compiler), 0xFF, span);
    writeChunk(currentChunk(compiler), 0xFF, span);

    return currentChunk(compiler)->count - 2;
}

static void patchJump(Compiler *compiler, int offset, SourceSpan span)
{
    int jump = currentChunk(compiler)->count - offset - 2;

    if (jump > UINT16_MAX)
        error_report(404, "CompileError: Line %d column %d\nToo many instructions for conditional jump.", span.startline, span.startcol);

    currentChunk(compiler)->code[offset] = jump & 0xFF;
    currentChunk(compiler)->code[offset + 1] = (jump >> 8) & 0xFF;
}

static void customPatchJump(Compiler *compiler, int offset, int jump, SourceSpan span)
{
    if (jump > UINT16_MAX)
        error_report(404, "CompileError: Line %d column %d\nToo many instructions for conditional jump.", span.startline, span.startcol);

    currentChunk(compiler)->code[offset] = jump & 0xFF;
    currentChunk(compiler)->code[offset + 1] = (jump >> 8) & 0xFF;
}

static void emitLoop(Compiler *compiler, int start, SourceSpan span)
{
    writeChunk(currentChunk(compiler), OP_LOOP, span);

    int jump = currentChunk(compiler)->count - start + 2;

    if (jump > UINT16_MAX)
        error_report(405, "CompileError: Line %d column %d\nLoop body too large.", span.startline, span.startcol);

    writeU16(currentChunk(compiler), jump, span);
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

        arg = addConstant(currentChunk(compiler), val);
        setOp = OP_SET_GLOBAL;
        getOp = OP_GET_GLOBAL;
    }

    if (canAssign == 1)
    {
        writeChunk(currentChunk(compiler), setOp, span);
        writeU16(currentChunk(compiler), arg, span);
    }
    else if (canAssign == 2)
    {
        writeChunk(currentChunk(compiler), OP_DEFINE_GLOBAL, span);
        writeU16(currentChunk(compiler), arg, span);
    }
    else
    {
        writeChunk(currentChunk(compiler), getOp, span);
        writeU16(currentChunk(compiler), arg, span);
    }
}

static void compile_and(Compiler *compiler, Expression *expr)
{
    compile_expression(compiler, expr->Binary.Left);
    int endJump = emitJump(compiler, OP_JUMP_IF_FALSE, expr->span);
    writeChunk(currentChunk(compiler), OP_POP, expr->span);

    compile_expression(compiler, expr->Binary.Right);
    patchJump(compiler, endJump, expr->span);
}

static void compile_or(Compiler *compiler, Expression *expr)
{
    compile_expression(compiler, expr->Binary.Left);

    int endJump = emitJump(compiler, OP_JUMP_IF_TRUE, expr->span);
    writeChunk(currentChunk(compiler), OP_POP, expr->span);

    compile_expression(compiler, expr->Binary.Right);
    patchJump(compiler, endJump, expr->span);
}

static void compile_expression(Compiler *compiler, Expression *expr)
{
    switch (expr->type)
    {
    case BINARY:
    {
        if (expr->Binary.Operator.type == AND)
        {
            compile_and(compiler, expr);
            return;
        }
        else if (expr->Binary.Operator.type == OR)
        {
            compile_or(compiler, expr);
            return;
        }

        compile_expression(compiler, expr->Binary.Left);
        compile_expression(compiler, expr->Binary.Right);

        writeChunk(currentChunk(compiler), tok_to_code_operators[expr->Binary.Operator.type], expr->span);
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
            writeChunk(currentChunk(compiler), OP_TRUE, expr->span);
            return;
        case TYPE_FALSE:
            writeChunk(currentChunk(compiler), OP_FALSE, expr->span);
            return;
        case TYPE_NIL:
            writeChunk(currentChunk(compiler), OP_NIL, expr->span);
            return;
        case TYPE_STRING:
            value.obj = (Obj *)allocateString(compiler->vm, expr->Literal.Value.lexeme, expr->Literal.string_len);
            free(expr->Literal.Value.lexeme);
            break;
        }

        int index = addConstant(currentChunk(compiler), value);
        writeChunk(currentChunk(compiler), OP_CONSTANT, expr->span);
        writeU16(currentChunk(compiler), index, expr->span);

        break;
    }
    case UNARY:
        compile_expression(compiler, expr->Unary.Expr);
        writeChunk(currentChunk(compiler), tok_to_code_unary[expr->Unary.Operator.type], expr->span);
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
            writeChunk(currentChunk(compiler), OP_ADD, expr->span);
            break;
        case MINUS_EQUAL:
            writeChunk(currentChunk(compiler), OP_SUBTRACT, expr->span);
            break;
        case STAR_EQUAL:
            writeChunk(currentChunk(compiler), OP_MULTIPLY, expr->span);
            break;
        case SLASH_EQUAL:
            writeChunk(currentChunk(compiler), OP_DIVIDE, expr->span);
            break;
        case MOD_EQUAL:
            writeChunk(currentChunk(compiler), OP_MOD, expr->span);
            break;
        }

        namedVariable(compiler, expr->Assignment.identifier, 1, expr->span);
        break;
    }
    case CALL: {
        compile_expression(compiler, expr->Call.callee);
        for (int i = 0; i < expr->Call.argCount; i++) {
            compile_expression(compiler, expr->Call.arguments[i]);
        }

        writeChunk(currentChunk(compiler), OP_CALL, expr->span);
        writeU16(currentChunk(compiler), expr->Call.argCount, expr->span);
    }
    }
}

static void compile_vardecl(Compiler *compiler, Statement *stmt)
{
    printf("Current scope depth: %d\n", compiler->scopeDepth);
    VarDecl *decl = stmt->varDecl;

    Token name = {
        .lexeme = decl->name,
        .len = decl->len,
        .type = decl->type};

    if (decl->initializer == NULL)
        writeChunk(currentChunk(compiler), OP_NIL, stmt->span);
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

static void compile_funcdecl(Compiler *compiler, Statement *stmt) {
    FuncDecl *funcdecl = stmt->funcDecl;

    Compiler c = {0};
    c.vm = compiler->vm;
    c.localCount = 0;
    c.scopeDepth = 1;
    c.currentLoop = NULL;
    
    Local *local = &c.locals[c.localCount++];
    local->depth = 0;
    local->name.len = 0;
    local->name.lexeme = "";

    c.function = newFunction(compiler->vm);

    c.function->name = allocateString(compiler->vm, funcdecl->identifier.lexeme, funcdecl->identifier.len);
    c.type = TYPE_FUNCTION;

    c.function->arity = funcdecl->arity;
    initChunk(&c.function->chunk);

    for (int i = 0; i < c.function->arity; i++) {
        declareLocal(&c, funcdecl->parameters[i].identifier);
    }
    
    Statement blockStmt;
    blockStmt.type = TYPE_BLOCK;
    blockStmt.block = funcdecl->body;

    compile_block(&c, &blockStmt);
    writeChunk(currentChunk(&c), OP_NIL, stmt->span);
    writeChunk(currentChunk(&c), OP_RETURN, stmt->span);

    char path_buffer[c.function->name->len + 16];
    snprintf(path_buffer, sizeof(path_buffer), "./compiler/%.*s.abc", c.function->name->len, c.function->name->lexeme);

    #ifdef DEBUG_FLAG
        disassembleChunk(currentChunk(&c), path_buffer);
    #endif

    ObjFunction *function = c.function;
    writeChunk(currentChunk(compiler), OP_CONSTANT, stmt->span);

    Value val;
    val.type = VAL_OBJ;
    val.obj = (Obj *) function;

    int index = addConstant(currentChunk(compiler), val);
    writeU16(currentChunk(compiler), index, stmt->span);

    if (compiler->scopeDepth > 0)
    {
        addLocal(compiler, funcdecl->identifier);
        namedVariable(compiler, funcdecl->identifier, 1, stmt->span);
        return;
    }

    namedVariable(compiler, funcdecl->identifier, 2, stmt->span);
}

static void compile_block_custom(Compiler *compiler, Statement *stmt, int len)
{
    Block *block = stmt->block;
    compiler->scopeDepth++;

    for (int i = 0; i < len; i++)
    {
        compile_statement(compiler, block->statements[i]);
    }
}

static void compile_block(Compiler *compiler, Statement *stmt)
{
    printf("Compiling block...\n");
    Block *block = stmt->block;
    compiler->scopeDepth++;
    printf("Modified scopeDepth: %d\n", compiler->scopeDepth);

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
        writeChunk(currentChunk(compiler), OP_POPN, stmt->span);
        writeU16(currentChunk(compiler), count, stmt->span);
    }
}

static void compile_if(Compiler *compiler, Statement *statement)
{
    IfStmt *stmt = statement->ifStmt;
    compile_expression(compiler, stmt->condition);

    int jumpOverThen = emitJump(compiler, OP_JUMP_IF_FALSE, stmt->condition->span);
    writeChunk(currentChunk(compiler), OP_POP, statement->span);
    compile_statement(compiler, stmt->thenBranch);

    int jumpOverElse = emitJump(compiler, OP_JUMP, stmt->condition->span);
    patchJump(compiler, jumpOverThen, stmt->condition->span);

    writeChunk(currentChunk(compiler), OP_POP, statement->span);

    if (stmt->elseBranch != NULL)
    {
        compile_statement(compiler, stmt->elseBranch);
    }

    patchJump(compiler, jumpOverElse, stmt->condition->span);
}

static void compile_while(Compiler *compiler, Statement *statement)
{
    WhileStmt *stmt = statement->whileStmt;
    int loopStart = currentChunk(compiler)->count;

    compile_expression(compiler, stmt->condition);

    int exitLoop = emitJump(compiler, OP_JUMP_IF_FALSE, stmt->condition->span);
    writeChunk(currentChunk(compiler), OP_POP, stmt->condition->span);

    LoopContext context;

    context.continueTarget = (stmt->forIncrement != NULL) ? -1 : loopStart;
    context.parent = compiler->currentLoop;
    context.breakCount = 0;
    context.breakCapacity = 8;

    context.continueCount = 0;
    context.continueCapacity = 8;

    context.breakJumps = calloc(context.breakCapacity, sizeof(int));
    if (context.continueTarget == -1)
        context.continueJumps = calloc(context.continueCapacity, sizeof(int));
    else context.continueJumps = NULL;

    compiler->currentLoop = &context;

    compile_block_custom(compiler, stmt->body, stmt->body->block->count - 1);
    context.continueTarget = currentChunk(compiler)->count;

    compile_statement(compiler, stmt->body->block->statements[stmt->body->block->count - 1]);
    compiler->localCount = (compiler->localCount > 0) ? compiler->localCount - 1 : 0;
    compiler->scopeDepth--;

    uint16_t count = 0;
    while (compiler->localCount > 0 && compiler->locals[compiler->localCount - 1].depth > compiler->scopeDepth)
    {
        count++;
        compiler->localCount--;
    }

    if (count > 0)
    {
        printf("Emitting POP_%d\n", count);
        writeChunk(currentChunk(compiler), OP_POPN, stmt->body->span);
        writeU16(currentChunk(compiler), count, stmt->body->span);
    }

    emitLoop(compiler, loopStart, stmt->body->span);

    context.breakTarget = currentChunk(compiler)->count;

    for (int i = 0; i < compiler->currentLoop->breakCount; i++)
    {
        customPatchJump(compiler, compiler->currentLoop->breakJumps[i], context.breakTarget - compiler->currentLoop->breakJumps[i] - 2, statement->span);
    }

    for (int j = 0; j < compiler->currentLoop->continueCount; j++)
    {
        customPatchJump(compiler, compiler->currentLoop->continueJumps[j], context.continueTarget - compiler->currentLoop->continueJumps[j] - 2, statement->span);
    }

    patchJump(compiler, exitLoop, stmt->condition->span);
    writeChunk(currentChunk(compiler), OP_POP, stmt->condition->span);

    free(compiler->currentLoop->breakJumps);
    free(compiler->currentLoop->continueJumps);
    compiler->currentLoop = context.parent;
}

static void compile_continue(Compiler *compiler, Statement *statement)
{
    if (compiler->currentLoop == NULL)
    {
        error_report(406, "CompileError: Line %d column %d\n'continue' cannot be used outside a loop", statement->span.startline, statement->span.startcol);
    }

    if (compiler->currentLoop->continueTarget == -1)
    {
        if (compiler->currentLoop->continueCount > compiler->currentLoop->continueCapacity - 1)
        {
            int capacity = GROW_CAPACITY(compiler->currentLoop->continueCapacity);
            GROW_ARRAY(int, compiler->currentLoop->continueJumps, compiler->currentLoop->continueCapacity, capacity);

            compiler->currentLoop->continueCapacity = capacity;
        }

        int continueJump = emitJump(compiler, OP_JUMP, statement->span);
        compiler->currentLoop->continueJumps[compiler->currentLoop->continueCount++] = continueJump;
    }
    else
    {
        int distance = currentChunk(compiler)->count - compiler->currentLoop->continueTarget;
        if (distance > UINT16_MAX)
            error_report(404, "CompileError: Line %d column %d\nToo many instructions for conditional jump.", statement->span.startline, statement->span.startcol);

        emitLoop(compiler, distance, statement->span);
    }
}

static void compile_break(Compiler *compiler, Statement *statement)
{
    if (compiler->currentLoop == NULL)
    {
        error_report(406, "CompileError: Line %d column %d\n'break' cannot be used outside a loop", statement->span.startline, statement->span.startcol);
    }

    int distance = compiler->currentLoop->breakTarget - currentChunk(compiler)->count;
    if (distance > UINT16_MAX)
        error_report(404, "CompileError: Line %d column %d\nToo many instructions for conditional jump.", statement->span.startline, statement->span.startcol);

    int breakJump = emitJump(compiler, OP_JUMP, statement->span);

    if (compiler->currentLoop->breakCount > compiler->currentLoop->breakCapacity - 1)
    {
        int capacity = GROW_CAPACITY(compiler->currentLoop->breakCapacity);
        GROW_ARRAY(int, compiler->currentLoop->breakJumps, compiler->currentLoop->breakCapacity, capacity);

        compiler->currentLoop->breakCapacity = capacity;
    }

    compiler->currentLoop->breakJumps[compiler->currentLoop->breakCount++] = breakJump;
}

static void compile_return(Compiler *compiler, Statement *stmt) {
    if (compiler->type == TYPE_SCRIPT) {
        error_report(407, "CompileError: Line %d column %d\nCan't return from top-level code.", stmt->span.startline, stmt->span.startcol);
    }

    ReturnStmt *returnStmt = stmt->ReturnStmt;
    compile_expression(compiler, returnStmt->value);

    writeChunk(currentChunk(compiler), OP_RETURN, stmt->span);
}

static void compile_statement(Compiler *compiler, Statement *stmt)
{
    switch (stmt->type)
    {
    case TYPE_EXPR:
        compile_expression(compiler, stmt->exprStmt->expr);
        writeChunk(currentChunk(compiler), OP_POP, stmt->exprStmt->expr->span);
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
    case TYPE_CONTINUE:
        compile_continue(compiler, stmt);
        break;
    case TYPE_BREAK:
        compile_break(compiler, stmt);
        break;
    case TYPE_FUNCDECL:
        compile_funcdecl(compiler, stmt);
        break;
    case TYPE_RETURN:
        compile_return(compiler, stmt);
        break;
    }
}

ObjFunction *compile(Compiler *compiler)
{
    Program *source = compiler->source;

    for (int i = 0; i < source->count; i++)
    {
        compile_statement(compiler, source->statements[i]);
    }

    SourceSpan span;

    if (source->count > 0) {
        span = source->statements[source->count - 1]->span;
    } else {
        span.startline = 0;
        span.startcol = 0;
        span.endline = 0;
        span.endcol = 0;
    }

    writeChunk(currentChunk(compiler), OP_RETURN, span);

    #ifdef DEBUG_FLAG
        disassembleChunk(currentChunk(compiler), "./compiler/script.abc");
    #endif
    return compiler->function;
}

Compiler *init_compiler(VM *vm, Program *source)
{
    Compiler *compiler = malloc(sizeof(Compiler));

    if (!compiler)
        error_report(400, "MemoryError: Failed to initialize compiler");

    compiler->source = source;
    compiler->vm = vm;

    compiler->function = newFunction(vm);
    compiler->type = TYPE_SCRIPT;
    initChunk(&compiler->function->chunk);

    compiler->localCount = 0;
    compiler->currentLoop = NULL;

    Local *local = &compiler->locals[compiler->localCount++];
    local->depth = 0;
    local->name.lexeme = "";
    local->name.len = 0;

    return compiler;
}

void free_compiler(Compiler *compiler)
{
    free(currentChunk(compiler));
    free(compiler);
}