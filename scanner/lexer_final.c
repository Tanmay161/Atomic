#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "stringPool.h"

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

    TOKEN_EOF
} TokenType;

// ===== Token Struct =====
typedef struct
{
    TokenType type;
    char *lexeme;
    size_t len;
} Token;

// ===== Files and Buffers =====
FILE *input;
FILE *output;

// ===== Scanner Struct =====
typedef struct
{
    char *stream;
    char *pos;
    int line;
} Scanner;

// ===== Helpers =====
char *loadInput(char *fileName);
void reportError(int exitCode, char *message, ...);
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

Keyword keyword_list[11];

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

    Token cur = next_token(&scanner);

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
void reportError(int exitCode, char *message, ...)
{
    va_list args;
    va_start(args, message);

    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");

    va_end(args);
    exit(exitCode);
}

// ===== Input Loading to Buffer =====
char *loadInput(char *fileName)
{
    // Open file for reading bytes
    input = fopen(fileName, "rb");

    // Failed read operation
    if (!input)
    {
        reportError(
            101,
            "MemoryError: Failed to open input file: %s",
            fileName);
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
        reportError(
            102,
            "MemoryErorr: Failed memory allocation for input buffer");
    }

    // Read from the input
    size_t read = fread(buffer, 1, size, input);
    // Failed read
    if (read != size)
    {
        reportError(
            103,
            "MemoryError: Failed to read input file");
    }

    // String termination
    buffer[size] = '\0';
    fclose(input);

    return buffer;
}

// ===== Consumes a Character =====
int next_char(Scanner *s)
{
    return *(s->pos++);
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
    char *start = s->pos - 1;
    size_t len = 1;

    int isDecimal = 0;
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
                char buffer[len + 1];

                int i = 0;
                for (; i < len; i++)
                {
                    buffer[i] = *(start + i);
                }

                buffer[i] = '\0';
                reportError(
                    107,
                    "SyntaxError: Invalid float at line %d: '%s.' (Unexpected dot)",
                    (s->line),
                    buffer);
            }

            // Change float flag so no more periods
            isDecimal = 1;
            type = FLOAT;
        }

        // Advance input and increment len
        next_char(s);
        len++;
        next = peek(s);
    }

    return (Token){
        .type = type,
        .lexeme = insert_return_ptr_to_string(start, len),
        .len = len};
}

// ===== Identifier Scanning =====
Token scan_identifier(Scanner *s)
{
    // Get start pointer
    char *start = s->pos - 1;
    size_t len = 1;
    int next = peek(s);

    // Run while token remains an identifier
    while (isalnum(next) || next == '_')
    {
        next_char(s);
        len++;
        next = peek(s);
    }

    // Keyword check
    char *interned = insert_return_ptr_to_string(start, len);
    for (size_t i = 0; i < 11; i++)
    {
        if (interned == keyword_list[i].lexeme)
        {
            return (Token){.type = keyword_list[i].type, .lexeme = interned, .len = len};
        }
    }

    return (Token){
        .type = IDENTIFIER,
        .lexeme = interned,
        .len = len};
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
        reportError(
            105,
            "MemoryError: Memory allocation failed for string literal");

    // Check for multiline string
    if (s->pos[0] == '\"' && s->pos[1] == '\"')
    {
        multiline = 1;
        s->pos += 2;
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
                if (s->pos[0] == '\0' || s->pos[1] == '\0')
                    reportError(
                        106,
                        "SyntaxError: Line %d\nUnclosed string:\n%s (Expected \"\"\")",
                        s->line,
                        string);
                // String closed as intended
                if (s->pos[0] == '\"' && s->pos[1] == '\"')
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
                            reportError(
                                105,
                                "MemoryError: Memory allocation failed for string literal");
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
                reportError(
                    106,
                    "SyntaxError: Line %d\nUnclosed string:\n%s (Expected \"\"\")",
                    s->line,
                    string);
            else
                reportError(
                    106,
                    "SyntaxError: Line %d\nUnclosed string:\n%s (Expected \")",
                    s->line,
                    string);
        }
        // Reallocation
        if (len + 1 >= size)
        {
            size *= 2;

            char *temp = realloc(string, size);
            // Failed reallocation
            if (!temp)
            {
                reportError(
                    105,
                    "MemoryError: Memory allocation failed for string literal");
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
                    reportError(
                        106,
                        "SyntaxError: Line %d\nUnclosed string:\n%s (Expected \")",
                        s->line,
                        string);
                else
                    reportError(
                        106,
                        "SyntaxError: Line %d\nUnclosed string:\n%s (Expected \"\"\")",
                        s->line,
                        string);
                break;
            }
            default:
                reportError(
                    104,
                    "SyntaxError: Invalid escape sequence at line %d: '\\%c'",
                    (s->line),
                    next);
            }

            continue;
        }
        // Multiline string using single line string syntax
        else if (c == '\n')
        {
            if (!multiline)
                reportError(
                    108,
                    "Use of line break in single line string is not permitted. To create a multiline string, wrap the string in triple quotes (\"\"\")");
            else
                s->line++;
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
        .len = len};
}

// ===== Return Next Token =====
Token next_token(Scanner *s)
{
    char c;

    while (isspace(peek(s)))
    {
        if (peek(s) == '\n')
            s->line++;
        next_char(s);
    }

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
            return (Token){.type = LEFT_PAREN, .lexeme = "(", .len = 1};
            break;
        case ')':
            return (Token){.type = RIGHT_PAREN, .lexeme = ")", .len = 1};
            break;
        case '{':
            return (Token){.type = LEFT_BRACE, .lexeme = "{", .len = 1};
            break;
        case '}':
            return (Token){.type = RIGHT_BRACE, .lexeme = "}", .len = 1};
            break;
        case '[':
            return (Token){.type = LEFT_BRACKET, .lexeme = "[", .len = 1};
            break;
        case ']':
            return (Token){.type = RIGHT_BRACKET, .lexeme = "]", .len = 1};
            break;
        case ',':
            return (Token){.type = COMMA, .lexeme = ",", .len = 1};
            break;
        // We need to check for floats like .5
        case '.':
        {
            if (isdigit(peek(s)))
            {
                return scan_number(s);
            }
            else
                return (Token){.type = DOT, .lexeme = ".", .len = 1};
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
                    else if (c == '\n')
                        s->line++;
                    else if (c == '\0')
                        reportError(
                            109,
                            "SyntaxError: Unclosed multiline comment starting at line %d",
                            start_line);
                }

                return next_token(s);
            }
            else if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = SLASH_EQUAL, .lexeme = "/=", .len = 2};
            }
            else
                return (Token){.type = SLASH, .lexeme = "/", .len = 1};
            break;
        }
        case '+':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = PLUS_EQUAL, .lexeme = "+=", .len = 2};
            }
            else if (peek(s) == '+')
            {
                next_char(s);
                return (Token){.type = PLUS_PLUS, .lexeme = "++", .len = 2};
            }
            else
                return (Token){.type = PLUS, .lexeme = "+", .len = 1};
            break;
        }
        case '-':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = MINUS_EQUAL, .lexeme = "-=", .len = 2};
            }
            else if (peek(s) == '-')
            {
                next_char(s);
                return (Token){.type = MINUS_MINUS, .lexeme = "--", .len = 2};
            }
            else
                return (Token){.type = MINUS, .lexeme = "-", .len = 1};
            break;
        }
        case '*':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = STAR_EQUAL, .lexeme = "*=", .len = 2};
            }
            else
                return (Token){.type = STAR, .lexeme = "*", .len = 1};
            break;
        }
        case ';':
            return (Token){.type = SEMICOLON, .lexeme = ";", .len = 1};
            break;
        case '!':
        {
            if (peek(s) != '=')
                reportError(
                    111,
                    "SyntaxError: Unexpected chacter '!' at line %d",
                    s->line);
            else
                return (Token){.type = NOT_EQUAL, .lexeme = "!=", .len = 2};
        }
        case '=':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = EQUAL_EQUAL, .lexeme = "==", .len = 2};
            }
            else
                return (Token){.type = EQUAL, .lexeme = "=", .len = 1};
        }
        case '%':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = MOD_EQUAL, .lexeme = "%=", .len = 2};
            }
            else
                return (Token){.type = MOD, .lexeme = "%", .len = 1};
        }
        case '~':
        {
            if (peek(s) == '/')
            {
                next_char(s);
                if (peek(s) == '=')
                {
                    next_char(s);
                    return (Token){.type = TILDE_SLASH_EQUAL, .lexeme = "~/=", .len = 3};
                }
                else
                    return (Token){.type = TILDE_SLASH, .lexeme = "~/", .len = 2};
            }
            else
                reportError(
                    111,
                    "SyntaxError: Unexpected character '~' at line %d",
                    s->line);
        }
        case '>':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = GREATER_EQUAL, .lexeme = ">=", .len = 2};
            }
            return (Token){.type = GREATER, .lexeme = ">", .len = 1};
        }
        case '<':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = LESS_EQUAL, .lexeme = "<=", .len = 2};
            }
            return (Token){.type = LESS, .lexeme = "<", .len = 1};
        }
        }
    }

    reportError(
        111,
        "SyntaxError: Unexpected character '%c' at line %d",
        c,
        s->line);
}