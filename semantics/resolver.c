#include "ast.h"
#include "hashmap.h"
#include "resolver.h"
#include "token.h"
#include "error.h"
#include "string.h"

#define GET_COUNT(resolver) ((int) (resolver->nextLocal - resolver->locals))

int resolve_statement(Resolver *resolver, Statement *stmt);

int declareVariable(Resolver *resolver, char *lexeme, int len, int *isLocal) {
    if (resolver->scope == 0) {
        printf("Before map_get\n");
        if (map_get(&resolver->globals_map, lexeme, len) != -1) 
            error_report(302, "Attempt to declare global variable '%.*s' after it was already declared.", len, lexeme);
        
        printf("After map_get\n");
        map_set(&resolver->globals_map, lexeme, len, resolver->globalCount);
        printf("After map_set\n");
        *isLocal = 0;
        return resolver->globalCount++;
    }
    else {
        for (Local *local = resolver->nextLocal; local > resolver->locals;) {
            --local;
            if (local->depth < resolver->scope)
                break;
            
            if (local->depth == resolver->scope && local->name.len == len && memcmp(local->name.lexeme, lexeme, len) == 0)
                error_report(302, "Attempt to declare local variable '%.*s' after it was already declared.", len, lexeme);
        }

        Local local;
        local.depth = resolver->scope;
        local.slot = GET_COUNT(resolver);
        local.name = (Token) {.lexeme = lexeme, .len = len};

        *resolver->nextLocal++ = local;
        *isLocal = 1;
        return local.slot;
    }
}

int resolveVariable(Resolver *resolver, char *lexeme, int len, int *isLocal) {
    for (Local *local = resolver->nextLocal; local > resolver->locals;) {
        --local;
        if (local->name.len == len && memcmp(local->name.lexeme, lexeme, len) == 0) {
            *isLocal = 1;
            return local->slot;
        }
    }

    int res = map_get(&resolver->globals_map, lexeme, len);
    if (res == -1)
        error_report(301, "Unable to resolve variable '%.*s'", len, lexeme);

    *isLocal = 0;
    return res;
}

int resolve_block(Resolver *resolver, Block *block) {
    Local *localsBefore = resolver->nextLocal;
    resolver->scope++;

    for (int i = 0; i < block->count; i++) {
        if (resolve_statement(resolver, block->statements[i]) == 0) {
            resolver->nextLocal = localsBefore;
            resolver->scope--;
            return 0;
        }
    }

    resolver->nextLocal = localsBefore;
    resolver->scope--;
    return 1;
}

int resolve_expr(Resolver *resolver, Expression *expr) {
    switch (expr->type) {
        case VARIABLE: {
            Token identifier = expr->Variable.identifier;
            expr->Variable.resolvedSlot = resolveVariable(resolver, identifier.lexeme, identifier.len, &expr->Variable.isLocal);
            return 1;
        }
        case BINARY: {
            resolve_expr(resolver, expr->Binary.Left);
            resolve_expr(resolver, expr->Binary.Right);
            return 1;
        }
        case UNARY: {
            resolve_expr(resolver, expr->Unary.Expr);
            return 1;
        }
        case POSTFIX: {
            resolve_expr(resolver, expr->Postfix.Expr);
            return 1;
        }
        case GROUPING: {
            resolve_expr(resolver, expr->Grouping.Expr);
            return 1;
        }
        case ASSIGNMENT: {
            resolve_expr(resolver, expr->Assignment.value);
            int slot = resolveVariable(resolver, expr->Assignment.identifier.lexeme, expr->Assignment.identifier.len, &expr->Assignment.isLocal);
            expr->Assignment.resolvedSlot = slot;
            return 1;
        }
        case CALL: {
            resolve_expr(resolver, expr->Call.callee);
            for (int i = 0; i < expr->Call.argCount; i++) {
                resolve_expr(resolver, expr->Call.arguments[i]);
            }
            return 1;
        }
        case LITERAL: return 1;
        default: printf("Expression type not found\n");
    }

    return 1;
}

int resolve_vardecl(Resolver *resolver, Statement *stmt) {
    VarDecl *decl = stmt->varDecl;

    if (decl->initializer != NULL)
        if (!resolve_expr(resolver, decl->initializer)) return 0;

    int slot = declareVariable(resolver, decl->name, decl->len, &stmt->varDecl->isLocal);
    stmt->varDecl->resolvedSlot = slot;

    return 1;
}

int resolve_funcdecl(Resolver *resolver, Statement *stmt) {
    printf("Before declaring function\n");
    int slot = declareVariable(resolver, stmt->funcDecl->identifier.lexeme, stmt->funcDecl->identifier.len, &stmt->funcDecl->isLocal);
    printf("After declaring function\n");

    Local *startBefore = resolver->locals;
    Local *nextBefore = resolver->nextLocal;
    int scopeBefore = resolver->scope;

    printf("locals offset: %td\n",
       resolver->locals - resolver->localStorage);
    printf("nextLocal offset: %td\n",
        resolver->nextLocal - resolver->localStorage);
    printf("relative count: %d\n", GET_COUNT(resolver));

    resolver->scope++;
    resolver->locals = &resolver->localStorage[GET_COUNT(resolver)];
    resolver->nextLocal = resolver->locals;

    Local reserved = {
        .depth = resolver->scope,
        .name = (Token){ .lexeme = "", .len = 0 },
        .slot = GET_COUNT(resolver)
    };

    *resolver->nextLocal++ = reserved;

    printf("Entering function resolver\n");
    for (int i = 0; i < stmt->funcDecl->arity; i++) {
        int isLocal;
        int slotParam = declareVariable(resolver, stmt->funcDecl->parameters[i].identifier.lexeme, stmt->funcDecl->parameters[i].identifier.len, &isLocal);
        stmt->funcDecl->parameters[i].resolvedSlot = slotParam;
    }

    for (int i = 0; i < stmt->funcDecl->body->count; i++) {
        resolve_statement(resolver, stmt->funcDecl->body->statements[i]);
    }

    printf("Function body resolution complete\n");
    
    resolver->scope--;
    resolver->locals = startBefore;
    resolver->nextLocal = nextBefore;
    resolver->scope = scopeBefore;

    stmt->funcDecl->resolvedSlot = slot;

    return 1;
}

int resolve_if(Resolver *resolver, Statement *stmt) {
    resolve_expr(resolver, stmt->ifStmt->condition);
    resolve_statement(resolver, stmt->ifStmt->thenBranch);

    if (stmt->ifStmt->elseBranch != NULL)
        resolve_statement(resolver, stmt->ifStmt->elseBranch);

    return 1;
}

int resolve_while(Resolver *resolver, Statement *stmt) {
    if (stmt->whileStmt->condition != NULL)
        resolve_expr(resolver, stmt->whileStmt->condition);

    resolve_statement(resolver, stmt->whileStmt->body);
    
    if (stmt->whileStmt->forIncrement != NULL)
        resolve_statement(resolver, stmt->whileStmt->forIncrement);

    return 1;
}

int resolve_statement(Resolver *resolver, Statement *stmt) {
    printf("Resolving statement of type %d at %p\n", stmt->type, (void *)stmt);
    switch(stmt->type) {
        case TYPE_BLOCK: return resolve_block(resolver, stmt->block);
        case TYPE_EXPR: return resolve_expr(resolver, stmt->exprStmt->expr);
        case TYPE_VARDECL: return resolve_vardecl(resolver, stmt);
        case TYPE_FUNCDECL: return resolve_funcdecl(resolver, stmt);
        case TYPE_RETURN: return resolve_expr(resolver, stmt->ReturnStmt->value);
        case TYPE_IF: return resolve_if(resolver, stmt);
        case TYPE_WHILE: return resolve_while(resolver, stmt);
        case TYPE_CONTINUE: return 1;
        case TYPE_BREAK: return 1;
        default: printf("Statement type not found\n");
    }

    return 1;
}

int resolve(Resolver *resolver, Program *program) {
    for (int i = 0; i < program->count; i++) {
        if (resolve_statement(resolver, program->statements[i]) == 0) {
            return 0;
        }
    }

    return 1;
}

Resolver initResolver() {
    Resolver resolver = {0};
    resolver.globals_map = initMap();
    resolver.scope = 0;
    resolver.globalCount = 0;

    map_set(&resolver.globals_map, "clock", 5, resolver.globalCount++);
    map_set(&resolver.globals_map, "print", 5, resolver.globalCount++);
    map_set(&resolver.globals_map, "debug", 5, resolver.globalCount++);

    resolver.locals = &resolver.localStorage[0];
    resolver.nextLocal = &resolver.localStorage[0];

    *resolver.nextLocal++ = (Local) {.depth = resolver.scope, .name = (Token) {.lexeme = "", .len = 0}, .slot = 0};

    return resolver;
}