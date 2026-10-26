#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "chunk.h"
#include "vm.h"
#include "debug.h"

int main() {
    Scanner *s = init_scanner("./parser/test.txt");
    Parser *p = init_parser(s);

    Program *program = parse(p);

    VM *vm = initVM();
    printf("Initiating compiler...\n");
    Compiler *c = init_compiler(vm, program);
    printf("Initiated!\n");

    ObjFunction *result = compile(c);

    interpret(vm, result);
    freeVM(vm);

    return 0;
}