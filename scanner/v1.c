#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define MAX_IDENTIFIER_SIZE 1023
#define TABLE_SIZE 1024

typedef enum {
    // Arithmetic Operators
    ADD,
    SUB,
    MUL,
    DIV,
    POW,
    MOD,
    ADDEQ,
    SUBEQ,
    MULEQ,
    DIVEQ,
    MODEQ,
    POWEQ,
    // Logical Operators
    EQ,
    MT,
    MTE,
    LT,
    LTE,
    NOT,
    NOTEQ,
    // Boolean operators
    AND,
    OR,
    // Data types
    INTEGER,
    STRING,
    // Braces
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    // Loops
    WHILE,
    FOR,
    // Conditionals
    IF,
    // Misc tokens
    TOKEN_EOF,
    TOKEN_ERROR,
    ASSIGN,
    INCREMENT,
    DECREMENT,
    SEMICOLON,
    IDENTIFIER
} TokenType;

const char *map[] = {"ADD", "SUB", "MUL", "DIV", "POW", "MOD", "ADDEQ", "SUBEQ","MULEQ","DIVEQ","MODEQ","POWEQ", "EQ", "MT", "MTE", "LT", "LTE", "NOT", "NOTEQ", "AND", "OR", "INTEGER", "STRING", "LPAREN", "RPAREN", "LBRACE", "RBRACE", "WHILE", "FOR", "IF", "EOF", "ERROR", "ASSIGN", "INCREMENT", "DECREMENT", "SEMICOLON", "IDENTIFIER"};
char *keywords[] = {"if", "while", "for", "and", "or"};
TokenType kwtypes[] = {IF, WHILE, FOR, AND, OR};

typedef struct Entry {
    char *start;
    int len;
    TokenType type;
    struct Entry *next;
} Entry;

typedef union {
    int intval;
    Entry *node;
    char *strval;
} TokenValue;

typedef struct {
    TokenType type;
    char *name;
    TokenValue value;
} Token;

Entry *idHashMap[TABLE_SIZE];

Token next_token();

// Helpers
int next_char();
int read_number();
int peek();
unsigned long getHash(const unsigned char *str);
TokenType checkReturnCompoundOperator(char start);
void init_keywords(char *words[], TokenType types[], int count);

FILE *input;
FILE *output;

int lookahead = EOF;

int main() {
    init_keywords(keywords, kwtypes, 5);
    input = fopen("./scanner/input.txt", "r");
    output = fopen("./scanner/output.scanned", "w");
    if (!input) {
        fprintf(stderr, "Error: Could not open input.txt\n");
        return 1;
    }

    lookahead = fgetc(input);
    Token res;
    
    do {
        res = next_token();
        if (res.type == INTEGER) {
            fprintf(output, "INTEGER(%d)\n", res.value.intval);
        } else if (res.type == STRING) {
            fprintf(output, "STRING(%s)\n", res.value.strval);
        } else if (res.type == IDENTIFIER) {
            fprintf(output, "IDENTIFIER(%s)\n", res.value.node->start);
        } else {
            fprintf(output, "%s\n", map[res.type]);
        }
    } while (res.type != TOKEN_EOF);

    return 0;
}

int next_char() {
    int c = lookahead;
    lookahead = fgetc(input);

    return c;
}

int peek() {
    return lookahead;
}

unsigned long getHash(const unsigned char *str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++)) hash = ((hash << 5) + hash) + c;

    return hash;
}

TokenType checkReturnCompoundOperator(char start) {
    char next = peek();
    
    // Compound operator case
    if (next == '=') {
        switch (start) {
            case '+': next_char(); return ADDEQ;
            case '-': next_char(); return SUBEQ;
            case '*': next_char(); return MULEQ;
            case '/': next_char(); return DIVEQ;
            case '%': next_char(); return MODEQ;
            case '^': next_char(); return POWEQ;
            case '=': next_char(); return EQ;
            case '>': next_char(); return MTE;
            case '<': next_char(); return LTE;
            case '!': next_char(); return NOTEQ;
        }
    } 
    // ++ case (increment)
    else if (next == '+' && start == '+') {next_char(); return INCREMENT;} 
    // -- case (decrement)
    else if (next == '-' && start == '-') {next_char(); return DECREMENT;}
    // Not a compound operator
    else {
        switch (start) {
            case '+': return ADD;
            case '-': return SUB;
            case '*': return MUL;
            case '/': return DIV;
            case '%': return MOD;
            case '^': return POW;
            case '=': return ASSIGN;
            case '>': return MT;
            case '<': return LT;
            case '!': return NOT;
        }
    }
    
    // Not an operator
    return TOKEN_ERROR;
}

void init_keywords(char *words[], TokenType types[], int count) {
    for (int i = 0; i < count; i++) {
        char *word = words[i];

        unsigned long hash = getHash((const unsigned char *)word);
        int pos = hash % TABLE_SIZE;
        
        if (idHashMap[pos] == NULL) {
            Entry *newEntry = malloc(sizeof(Entry));

            if (!newEntry) {
                fprintf(stderr, "Error: Unable to allocate memory for LinkedListNode");
                return;
            }

            newEntry->start = strdup(word);
            newEntry->type = types[i];
            newEntry->next = NULL;
            newEntry->len = strlen(word);

            idHashMap[pos] = newEntry;
        } else {
            Entry *curToken = idHashMap[pos];

            while (curToken->next != NULL) curToken = curToken->next;
            Entry *newEntry = malloc(sizeof(Entry));

            if (!newEntry) {
                fprintf(stderr, "Error: Unable to allocate memory for LinkedListNode");
                return;
            }

            newEntry->type = types[i];
            newEntry->next = NULL;
            newEntry->start = strdup(word);
            newEntry->len = strlen(word);

            curToken->next = newEntry;
        }
    }
}

Token next_token() {
    int c;

    do {
        c = next_char(); // Consume next character
        if (c == EOF) return (Token){.type=TOKEN_EOF}; // If EOF, return
    } while (isspace(c));

    if (isdigit(c)) {
        // Loop while isdigit(c) and convert to number
        int val = c-'0';

        // Loop until the end of the number
        while (isdigit(peek())) {
            // Consume because we know the next digit is a number
            c = next_char();
            
            // Char -> int conversion
            int num = c - '0';
            val = val * 10 + num;
        }

        // Return number token
        return (Token){.type=INTEGER, .value.intval=val};
    } else if (c == '"' || c == '\'') {
        char start = c;
        // Initial size
        int size = 16;
        // Allocate memory
        char *word = malloc(sizeof(char) * size);
        int len = 0;

        while (1) {
            // Reallocate if size exceeds limit
            if (len + 1 >= size) {
                size *= 2;
                char *temp = realloc(word, sizeof(char) * size);

                if (!temp){
                    // Prevent memory leak of word
                    free(word);
                    fprintf(stderr, "Erorr: Failed to allocate memory for string\n");
                    return (Token){.type=TOKEN_ERROR};
                }
                
                word = temp;
            }

            // Cosume next character
            c = next_char();

            // Validity checks
            if (c == EOF) {
                free(word);
                fprintf(stderr, "Error: Opened quotation mark was never closed\n");
                return (Token){.type=TOKEN_ERROR};
            } else if (c == '\\') {   // Escape sequences
                // Consume next character
                c = next_char();

                switch (c) {
                    case 'n': word[len++] = '\n'; break;
                    case 't': word[len++] = '\t'; break;
                    case '\\': word[len++] = '\\'; break;
                    case '"': word[len++] = '"'; break;
                    default: word[len++] = c; break; // Ignore backslash
                }

                continue; // Skip the rest of the loop
            } else if (c == '\n') {
                fprintf(stderr, "%sError: Unclosed string: %c%s%s\n", "\x1b[31m", '"', word, "\x1b[0m");
                free(word);
                return (Token){.type=TOKEN_ERROR};
            }
            else if (c == start) break;
            
            // Consume and append next character, as we know it is safe
            word[len++] = c;
        }

        // Add null terminator
        word[len] = '\0';
        // Make sure to free memory later to avoid leaks
        return (Token){.type=STRING, .value.strval=word};
    } 
    else if (c == ';') return (Token){.type=SEMICOLON};
    else if (c == '(') return (Token){.type=LPAREN};
    else if (c == ')') return (Token){.type=RPAREN};
    else if (c == '{') return (Token){.type=LBRACE};
    else if (c == '}') return (Token){.type=RBRACE};
    else {
        TokenType resOp = checkReturnCompoundOperator(c);
        if (resOp != TOKEN_ERROR) return (Token){.type=resOp};

        // Identifier checks (no starting with integer)
        if (isalpha(c) || c == '_') {
            // Allocate a heap on the stack
            char buffer[MAX_IDENTIFIER_SIZE+1];
            int len = 0;

            buffer[len++] = c;

            while (1) {
                if (len >= MAX_IDENTIFIER_SIZE) {
                    fprintf(stderr, "Error: Maximum identifier length reached (1024 characters)\n");
                    return (Token){.type=TOKEN_ERROR};
                }
                // Consume next character
                int next = peek();

                // End of file checks -- would throw error because no semicolon to end statement
                if (next != '_' && !isalnum(next)) {
                    // Null terminator
                    buffer[len] = '\0';

                    // Keyword checks
                    /* if (strcmp(buffer, "and") == 0) return (Token){.type=AND};
                    else if (strcmp(buffer, "or") == 0) return (Token){.type=OR};
                    else if (strcmp(buffer, "for") == 0) return (Token){.type=FOR};
                    else if (strcmp(buffer, "while") == 0) return (Token){.type=WHILE};
                    else if (strcmp(buffer, "if") == 0) return (Token){.type=IF}; */

                    // Get the hash of the string;
                    unsigned long hash = getHash((unsigned char*)buffer);
                    int pos = hash % TABLE_SIZE;

                    // If the index is not occupied
                    if (idHashMap[pos] == NULL) {
                        // Heap memory allocation, because stack memory buffer will be invalid outside of the function
                        char *identifier = strdup(buffer);

                        // Memory allocation fail
                        if (!identifier) {
                            fprintf(stderr, "Error: Unable to allocate memory towards storage of identifier %s\n", buffer);
                            return (Token){.type=TOKEN_ERROR};
                        }
                        strcpy(identifier, buffer);

                        // Create the linked list at the index
                        Entry *newNode = malloc(sizeof(Entry));

                        if (!newNode) {
                            fprintf(stderr, "Error: Failed to allocate memory for storage of LinkedList Node");
                            return (Token){.type=TOKEN_ERROR};
                        }
                    
                        newNode->start = identifier;
                        newNode->len = len;
                        newNode->next = NULL;
                        newNode->type = IDENTIFIER;

                        idHashMap[pos] = newNode;
                        return (Token){.type=IDENTIFIER, .value.node=newNode};
                    } else {
                        // We need to traverse the linked list and check against our value
                        Entry *curNode = idHashMap[pos];    
                        if (strcmp(curNode->start, buffer) == 0) return (Token){.type=curNode->type, .value.node=curNode};                    
                        // Traversing
                        while (curNode->next != NULL) {
                            // Move on to next node and compare   
                            curNode = curNode->next;
                      
                            if (strcmp(curNode->start, buffer) == 0) {
                                return (Token){.type=curNode->type, .value.node=curNode};
                            }
                        }
                        // Dynamic memory allocation
                        char *identifier = strdup(buffer);
                        if (!identifier) {
                            fprintf(stderr, "Error: Unable to allocate memory towards storage of identifier %s\n", buffer);
                            return (Token){.type=TOKEN_ERROR};
                        }
                        strcpy(identifier, buffer);

                        // Allocating memory for a new node
                        Entry *newNode = malloc(sizeof(Entry));
                        
                        // In case of failed memory allocation
                        if (!newNode) {
                            fprintf(stderr, "Error: Failed to allocate memory for entry in identifier lookup table\n");
                            return (Token){.type=TOKEN_ERROR};
                        }

                        newNode->len = len;
                        newNode->next = NULL;
                        newNode->start = identifier;
                        newNode->type = IDENTIFIER;

                        // Join new node to linked list
                        curNode->next = newNode;

                        return (Token){.type=IDENTIFIER, .value.node = newNode};
                    }
                }
                
                // Consume next character and append
                c = next_char();
                buffer[len++] = c;
            }
        }
    }

    return (Token){.type=TOKEN_ERROR};
}