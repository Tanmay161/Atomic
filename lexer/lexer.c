#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "stringPool.h"

// Macros
#define KEYWORD_COUNT 14

/* ===== Error Codes =====
    101: Failed to open input file
    102: Cannot memory allocation for input buffer
    103: Failed to read input file
    104: Invalid escape sequence
    105: Failed to allocate memory for string literal
    106: Unclosed string
    107: Invalid float
    108: New line not allowed in single line string
    109: Unclosed multiline comment
    110: Memory allocation failed for string intern pool
    111: Unexpected character
*/

typedef enum
{
    //  Single character tokens
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACE,
    RIGHT_BRACE,
    COMMA,
    DOT,
    MINUS,
    PLUS,
    SEMICOLON,
    SLASH,
    STAR,
    LEFT_BRACKET,
    RIGHT_BRACKET,
    PLUS_PLUS,
    MINUS_MINUS,

    // Multi-character tokens
    NOT_EQUAL,
    EQUAL,
    EQUAL_EQUAL,
    GREATER,
    MOD,
    MOD_EQUAL,
    TILDE,
    TILDE_SLASH,
    TILDE_SLASH_EQUAL,
    GREATER_EQUAL,
    LESS,
    LESS_EQUAL,
    PLUS_EQUAL,
    MINUS_EQUAL,
    STAR_EQUAL,
    SLASH_EQUAL,

    // Literals
    IDENTIFIER,
    STRING,
    INTEGER,
    FLOAT,

    // Datatypes
    DATATYPE_INT,
    DATATYPE_FLOAT,
    DATATYPE_STRING,

    // Keywords
    AND,
    OR,
    IF,
    ELSE,
    FOR,
    NIL,
    RETURN,
    WHILE,
    TRUE,
    FALSE,
    NOT,

    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

// ===== Token Struct =====
typedef struct
{
    TokenType type;
    char *lexeme;
    size_t len;
    int line;

    // For TOKEN_ERROR
    int code;
} Token;

// ===== Files and Buffers =====
FILE *input;
FILE *output;

// ===== Scanner Struct =====
typedef struct
{
    char *stream;
    char *start;
    char *pos;
    int line;
    int column;
} Scanner;

// ===== Helpers =====
char *loadInput(char *fileName);
Token reportError(int exitCode, int line, const char *message, ...);
int peek(Scanner *s);
int next_char(Scanner *s);

// ===== Main Functions =====
Token scan_number(Scanner *s);
Token scan_identifier(Scanner *s);
Token next_token(Scanner *s);

typedef struct
{
    char *lexeme;
    TokenType type;
} Keyword;

Keyword keyword_list[KEYWORD_COUNT];

void init_keywords()
{
    keyword_list[0] = (Keyword){.lexeme = insert_return_ptr_to_string("if", 2), IF};
    keyword_list[1] = (Keyword){.lexeme = insert_return_ptr_to_string("while", 5), WHILE};
    keyword_list[2] = (Keyword){.lexeme = insert_return_ptr_to_string("and", 3), AND};
    keyword_list[3] = (Keyword){.lexeme = insert_return_ptr_to_string("or", 2), OR};
    keyword_list[4] = (Keyword){.lexeme = insert_return_ptr_to_string("else", 4), ELSE};
    keyword_list[5] = (Keyword){.lexeme = insert_return_ptr_to_string("false", 5), FALSE};
    keyword_list[6] = (Keyword){.lexeme = insert_return_ptr_to_string("for", 3), FOR};
    keyword_list[7] = (Keyword){.lexeme = insert_return_ptr_to_string("nil", 3), NIL};
    keyword_list[8] = (Keyword){.lexeme = insert_return_ptr_to_string("return", 6), RETURN};
    keyword_list[9] = (Keyword){.lexeme = insert_return_ptr_to_string("true", 4), TRUE};
    keyword_list[10] = (Keyword){.lexeme = insert_return_ptr_to_string("not", 3), NOT};
    keyword_list[11] = (Keyword){.lexeme = insert_return_ptr_to_string("int", 3), DATATYPE_INT};
    keyword_list[12] = (Keyword){.lexeme = insert_return_ptr_to_string("float", 5), DATATYPE_FLOAT};
    keyword_list[13] = (Keyword){.lexeme = insert_return_ptr_to_string("str", 3), DATATYPE_STRING};
}

int main()
{
    // Initialize string intern pool
    init_string_pool();
    init_keywords();
    // Create scanner
    Scanner scanner;
    scanner.stream = loadInput("./scanner/input.txt");
    scanner.pos = scanner.stream;
    scanner.line = 1;
    scanner.column = 1;

    Token cur = next_token(&scanner);

    // Debugging purposes
    /* if (cur.type == IDENTIFIER) {
        for (size_t i = 0; i < cur.object.identifierinfo.len; i++) {
            printf("%c", cur.object.identifierinfo.start[i]);
        }
    }

    else if (cur.type == INTEGER) {
        for (size_t i = 0; i < cur.object.numberinfo.len; i++) {
            printf("%c", cur.object.numberinfo.start[i]);
        }
    }

    else if (cur.type == FLOAT) {
        for (size_t i = 0; i < cur.object.numberinfo.len; i++) {
            printf("%c", cur.object.numberinfo.start[i]);
        }
    }

    else if (cur.type == STRING) {
        printf("%s", cur.object.stringinfo.string);
    }

    printf("\n"); */
    return 0;
}

// ===== Error Report Generation =====
Token reportError(int exitCode, int line, const char *message, ...)
{
    va_list args1, args2;
    va_start(args1, message);
    va_copy(args2, args1);

    char buffer[512];
    int length = vsnprintf(buffer, sizeof(buffer), message, args1);
    va_end(args1);

    // Buffer too small
    char *interned;
    if (length > 512)
    {
        char *heapBuffer = malloc(length + 1);
        int length2 = vsnprintf(heapBuffer, length + 1, message, args2);
        interned = insert_return_ptr_to_string(heapBuffer, length2);

        free(heapBuffer);
    }
    else
    {
        interned = insert_return_ptr_to_string(buffer, length);
    }

    va_end(args2);

    return (Token){
        .type = TOKEN_ERROR,
        .code = exitCode,
        .len = length,
        .lexeme = interned,
        .line = line};
}

// ===== Input Loading to Buffer =====
char *loadInput(char *fileName)
{
    // Open file for reading bytes
    input = fopen(fileName, "rb");

    // Failed read operation
    if (!input)
    {
        fprintf(stderr, "MemoryError: Failed to open input file: %s\n", fileName);
        exit(101);
    }

    // Get size of file
    fseek(input, 0, SEEK_END);
    long size = ftell(input);
    rewind(input);

    // Malloc appropriate bytes
    char *buffer = malloc(size + 1);

    // Failed malloc
    if (!buffer)
    {
        fprintf(stderr, "MemoryError: Failed memory allocation for input buffer\n");
        exit(102);
    }

    // Read from the input
    size_t read = fread(buffer, 1, size, input);
    // Failed read
    if (read != size)
    {
        fprintf(stderr, "MemoryError: Failed to read input file\n");
        exit(103);
    }

    // String termination
    buffer[size] = '\0';
    fclose(input);

    return buffer;
}

// ===== Consumes a Character =====
int next_char(Scanner *s)
{
    char c = *(s->pos++);

    if (c == '\n')
    {
        s->line++;
        s->column = 1;
    }
    else
    {
        s->column++;
    }

    return c;
}

// ===== Lookahead =====
int peek(Scanner *s)
{
    return *(s->pos);
}

// ===== Number Scanning =====
Token scan_number(Scanner *s)
{
    // Starting position
    char *start = s->start;

    int isDecimal = 0;
    int scientific = 0;
    TokenType type = INTEGER;

    int next = peek(s);

    if (*start == '.')
    {
        type = FLOAT;
        isDecimal = 1;
    }

    while (isdigit(next) || next == '.')
    {
        // Check for float
        if (next == '.')
        {
            // Invalid float
            if (isDecimal == 1)
            {

                Token returnToken = reportError(
                    107,
                    s->line,
                    "SyntaxError: Line %d column %d\nInvalid float: '%s' (Unexpected dot)",
                    s->line,
                    s->column,
                    insert_return_ptr_to_string(start, s->pos - start));

                while (*s->pos != '\n')
                {
                    if (*s->pos == '\0')
                    {
                        break;
                    }
                    next_char(s);
                }

                return returnToken;
            }

            else if (!isdigit(s->pos[1]))
            {
                Token returnToken = reportError(
                    107,
                    s->line,
                    "SyntaxError: Line %d column %d\nInvalid float '%s' (Expected integer after dot)",
                    s->line,
                    s->column,
                    insert_return_ptr_to_string(start, s->pos - start));

                while (*s->pos != '\n')
                {
                    if (*s->pos == '\0')
                        break;
                    next_char(s);
                }

                return returnToken;
            }

            // Change float flag so no more periods
            isDecimal = 1;
            type = FLOAT;
        }
        else if (next == 'e')
        {
            if (!scientific)
            {
                scientific = 1;
            }
            else
            {
                return (Token){
                    .len = s->pos - start,
                    .lexeme = insert_return_ptr_to_string(start, s->pos - start),
                    .line = s->line,
                    .type = type};
            }
        }
        else if (next == '-')
        {
            if (!scientific)
            {
                return (Token){
                    .len = s->pos - start,
                    .lexeme = insert_return_ptr_to_string(start, s->pos - start),
                    .line = s->line,
                    .type = type};
            }
        }

        // Advance input and increment len
        next_char(s);
        next = peek(s);
    }

    return (Token){
        .type = type,
        .lexeme = insert_return_ptr_to_string(start, s->pos - start),
        .line = s->line,
        .len = s->pos - start};
}

// ===== Identifier Scanning =====
Token scan_identifier(Scanner *s)
{
    // Get start pointer
    char *start = s->start;
    int next = peek(s);

    // Run while token remains an identifier
    while (isalnum(next) || next == '_')
    {
        next_char(s);
        next = peek(s);
    }

    // Keyword check
    char *interned = insert_return_ptr_to_string(start, s->pos - start);
    for (size_t i = 0; i < KEYWORD_COUNT; i++)
    {
        if (interned == keyword_list[i].lexeme)
        {
            return (Token){.type = keyword_list[i].type, .lexeme = interned, .len = s->pos - start, .line = s->line};
        }
    }

    return (Token){
        .type = IDENTIFIER,
        .lexeme = interned,
        .line = s->line,
        .len = s->pos - start};
}

// ===== String Handling =====
Token scan_string(Scanner *s)
{
    // We don't go back to the previously consumed character as that is a quote.
    int multiline = 0;

    size_t size = 32;
    size_t len = 0;
    char *string = malloc(size);

    if (!string)
    {
        fprintf(stderr, "MemoryError: Memory allocation failed for string literal");
        exit(105);
    }

    // Check for multiline string
    if (peek(s) == '\"' && s->pos[1] == '\"')
    {
        multiline = 1;
        next_char(s);
        next_char(s);
    }

    while (1)
    {
        char c = next_char(s);
        // printf("%c\n", next);

        if (c == '\"')
        {
            // Quote detection
            if (!multiline)
                break;
            else
            {
                // If the file ends before string closes, because if file ends then s->pos + 1 will be invalid.
                if (peek(s) == '\0' || s->pos[1] == '\0')
                {
                    Token returnError = reportError(
                        106,
                        s->line,
                        "SyntaxError: Line %d column %d\nUnclosed string:\n%s (Expected \"\"\")",
                        s->line,
                        s->column,
                        insert_return_ptr_to_string(string, len));

                    free(string);
                    return returnError;
                }

                // String closed as intended
                if (peek(s) == '\"' && s->pos[1] == '\"')
                {
                    next_char(s);
                    next_char(s);
                    break;
                }
                // String unclosed
                else
                {
                    // Reallocate more memory if required
                    if (len + 1 >= size)
                    {
                        size *= 2;
                        char *temp = realloc(string, size);

                        // Failed allocation
                        if (!temp)
                        {
                            fprintf(stderr, "MemoryError: Memory allocation failed for string literal");
                            exit(105);
                        }
                        string = temp;
                    }
                    string[len++] = '"';
                    continue;
                }
            }
        }
        // End of string without closing
        else if (c == '\0')
        {
            string[len] = '\0';
            if (multiline == 1)
            {
                Token returnError = reportError(
                    106,
                    s->line,
                    "SyntaxError: Line %d column %d\nUnclosed string:\n%s (Expected \"\"\")",
                    s->line,
                    s->column,
                    insert_return_ptr_to_string(string, len));

                free(string);
                return returnError;
            }
            else
            {
                Token returnError = reportError(
                    106,
                    s->line,
                    "SyntaxError: Line %d column %d\nUnclosed string:\n%s (Expected \")",
                    s->line,
                    s->column,
                    insert_return_ptr_to_string(string, len));

                free(string);
                return returnError;
            }
        }
        // Reallocation
        if (len + 1 >= size)
        {
            size *= 2;

            char *temp = realloc(string, size);
            // Failed reallocation
            if (!temp)
            {
                fprintf(stderr, "MemoryError: Memory allocation failed for string literal");
                exit(105);
            }

            string = temp;
        }

        // Escape sequences
        if (c == '\\')
        {
            char next = next_char(s);
            switch (next)
            {
            case 'n':
                string[len++] = '\n';
                break;
            case 't':
                string[len++] = '\t';
                break;
            case '\\':
                string[len++] = '\\';
                break;
            case '"':
                string[len++] = '\"';
                break;
            case '\'':
                string[len++] = '\'';
                break;
            case '\0':
            {
                if (!multiline)
                {
                    Token returnError = reportError(
                        106,
                        s->line,
                        "SyntaxError: Line %d column %d\nUnclosed string:\n%s (Expected \")",
                        s->line,
                        s->column,
                        insert_return_ptr_to_string(string, len));

                    free(string);
                    return returnError;
                }
                else
                {
                    Token returnError = reportError(
                        106,
                        s->line,
                        "SyntaxError: Line %d column %d\nUnclosed string:\n%s (Expected \"\"\")",
                        s->line,
                        s->column,
                        insert_return_ptr_to_string(string, len));

                    free(string);
                    return returnError;
                }
                break;
            }
            default:
            {
                free(string);
                return reportError(
                    104,
                    s->line,
                    "SyntaxError: Line %d column %d\nInvalid escape sequence: '\\%c'",
                    s->line,
                    s->column,
                    next);
            }
            }

            continue;
        }
        // Multiline string using single line string syntax
        else if (c == '\n')
        {
            if (!multiline)
            {
                Token returnToken = reportError(
                    108,
                    s->line,
                    "SyntaxError: Line %d column %d\nUse of line break in single line string is not permitted. To create a multiline string, wrap the string in triple quotes (\"\"\")",
                    s->line,
                    s->column);

                while (*s->pos != '\"')
                {
                    if (*s->pos == '\0')
                        break;
                    next_char(s);
                }

                return returnToken;
            }
        }
        // Increment line at new line.

        string[len++] = c;
    }

    string[len] = '\0';
    char *interned = insert_return_ptr_to_string(string, len);
    free(string);

    return (Token){
        .type = STRING,
        .lexeme = interned,
        .line = s->line,
        .len = len};
}

// ===== Return Next Token =====
Token next_token(Scanner *s)
{
    char c;

    while (isspace(peek(s)))
    {
        next_char(s);
    }

    s->start = s->pos;
    c = next_char(s);

    if (c == '\0')
        return (Token){.type = TOKEN_EOF};

    // Identifier handling
    else if (isalpha(c) || c == '_')
        return scan_identifier(s);
    // Number handling
    else if (isdigit(c))
        return scan_number(s);
    // String handling
    else if (c == '"')
        return scan_string(s);
    // Comment / operator handling
    else
    {
        switch (c)
        {
        case '(':
            return (Token){.type = LEFT_PAREN, .lexeme = insert_return_ptr_to_string("(", 1), .len = 1, .line = s->line};
            break;
        case ')':
            return (Token){.type = RIGHT_PAREN, .lexeme = insert_return_ptr_to_string(")", 1), .len = 1, .line = s->line};
            break;
        case '{':
            return (Token){.type = LEFT_BRACE, .lexeme = insert_return_ptr_to_string("{", 1), .len = 1, .line = s->line};
            break;
        case '}':
            return (Token){.type = RIGHT_BRACE, .lexeme = insert_return_ptr_to_string("}", 1), .len = 1, .line = s->line};
            break;
        case '[':
            return (Token){.type = LEFT_BRACKET, .lexeme = insert_return_ptr_to_string("[", 1), .len = 1, .line = s->line};
            break;
        case ']':
            return (Token){.type = RIGHT_BRACKET, .lexeme = insert_return_ptr_to_string("]", 1), .len = 1, .line = s->line};
            break;
        case ',':
            return (Token){.type = COMMA, .lexeme = insert_return_ptr_to_string(",", 1), .len = 1, .line = s->line};
            break;
        // We need to check for floats like .5
        case '.':
        {
            if (isdigit(peek(s)))
            {
                return scan_number(s);
            }
            else
                return (Token){.type = DOT, .lexeme = insert_return_ptr_to_string(".", 1), .len = 1, .line = s->line};
            break;
        }

        case '/':
        {
            if (peek(s) == '/')
            {
                next_char(s);
                while (peek(s) != '\n' && peek(s) != '\0')
                    next_char(s);

                return next_token(s);
            }
            else if (peek(s) == '*')
            {
                int start_line = s->line;
                next_char(s);
                char c;

                while (1)
                {
                    c = next_char(s);
                    if (c == '*' && peek(s) == '/')
                    {
                        next_char(s);
                        break;
                    }
                    else if (c == '\0')
                        return reportError(
                            109,
                            start_line,
                            "SyntaxError: Line %d column %d\nUnclosed multiline comment",
                            start_line,
                            s->column);
                }

                return next_token(s);
            }
            else if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = SLASH_EQUAL, .lexeme = insert_return_ptr_to_string("/=", 2), .len = 2, .line = s->line};
            }
            else
                return (Token){.type = SLASH, .lexeme = insert_return_ptr_to_string("/", 1), .len = 1, .line = s->line};
            break;
        }
        case '+':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = PLUS_EQUAL, .lexeme = insert_return_ptr_to_string("+=", 2), .len = 2, .line = s->line};
            }
            else if (peek(s) == '+')
            {
                next_char(s);
                return (Token){.type = PLUS_PLUS, .lexeme = insert_return_ptr_to_string("++", 2), .len = 2, .line = s->line};
            }
            else
                return (Token){.type = PLUS, .lexeme = insert_return_ptr_to_string("+", 1), .len = 1, .line = s->line};
            break;
        }
        case '-':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = MINUS_EQUAL, .lexeme = insert_return_ptr_to_string("-=", 2), .len = 2, .line = s->line};
            }
            else if (peek(s) == '-')
            {
                next_char(s);
                return (Token){.type = MINUS_MINUS, .lexeme = insert_return_ptr_to_string("--", 2), .len = 2, .line = s->line};
            }
            else
                return (Token){.type = MINUS, .lexeme = insert_return_ptr_to_string("-", 1), .len = 1, .line = s->line};
            break;
        }
        case '*':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = STAR_EQUAL, .lexeme = insert_return_ptr_to_string("*=", 2), .len = 2, .line = s->line};
            }
            else
                return (Token){.type = STAR, .lexeme = insert_return_ptr_to_string("*", 1), .len = 1, .line = s->line};
            break;
        }
        case ';':
            return (Token){.type = SEMICOLON, .lexeme = insert_return_ptr_to_string(";", 1), .len = 1, .line = s->line};
            break;
        case '!':
        {
            if (peek(s) != '=')
                return reportError(
                    111,
                    s->line,
                    "SyntaxError: Line %d column %d\nUnexpected character '!'",
                    s->line,
                    s->column);
            else
                return (Token){.type = NOT_EQUAL, .lexeme = insert_return_ptr_to_string("!=", 2), .len = 2, .line = s->line};
        }
        case '=':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = EQUAL_EQUAL, .lexeme = insert_return_ptr_to_string("==", 2), .len = 2, .line = s->line};
            }
            else
                return (Token){.type = EQUAL, .lexeme = insert_return_ptr_to_string("=", 1), .len = 1, .line = s->line};
        }
        case '%':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = MOD_EQUAL, .lexeme = insert_return_ptr_to_string("%=", 2), .len = 2, .line = s->line};
            }
            else
                return (Token){.type = MOD, .lexeme = insert_return_ptr_to_string("%", 1), .len = 1, .line = s->line};
        }
        case '~':
        {
            if (peek(s) == '/')
            {
                next_char(s);
                if (peek(s) == '=')
                {
                    next_char(s);
                    return (Token){.type = TILDE_SLASH_EQUAL, .lexeme = insert_return_ptr_to_string("~/=", 3), .len = 3, .line = s->line};
                }
                else
                    return (Token){.type = TILDE_SLASH, .lexeme = insert_return_ptr_to_string("~/", 2), .len = 2, .line = s->line};
            }
            else
                return reportError(
                    111,
                    s->line,
                    "SyntaxError: Line %d column %d\nUnexpected character '~'",
                    s->line,
                    s->column);
        }
        case '>':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = GREATER_EQUAL, .lexeme = insert_return_ptr_to_string(">=", 2), .len = 2, .line = s->line};
            }
            return (Token){.type = GREATER, .lexeme = insert_return_ptr_to_string(">", 1), .len = 1, .line = s->line};
        }
        case '<':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = LESS_EQUAL, .lexeme = insert_return_ptr_to_string("<=", 2), .len = 2, .line = s->line};
            }
            return (Token){.type = LESS, .lexeme = insert_return_ptr_to_string("<", 1), .len = 1, .line = s->line};
        }
        }
    }

    return reportError(
        111,
        s->line,
        "SyntaxError: Line %d column %d\nUnexpected character '%c'",
        s->line,
        s->column,
        c);
}