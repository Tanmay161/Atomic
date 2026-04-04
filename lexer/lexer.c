#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "stringPool.h"
#include "lexer.h"

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
    112: Memory allocation failed for scanner
*/

// ===== Helpers =====
char *loadInput(Scanner *s, char *fileName);
Token reportError(int exitCode, int line, const char *message, ...);
int peek(Scanner *s);
int next_char(Scanner *s);
Scanner *init_scanner(char *inputFile);

// ===== Main Functions =====
Token scan_number(Scanner *s);
Token scan_identifier(Scanner *s);
Token next_token(Scanner *s);

Scanner *init_scanner(char *inputFile)
{
    init_string_pool();
    Scanner *scanner = malloc(sizeof(Scanner));

    if (!scanner) {
        fprintf(stderr, "MemoryError: Failed to allocate scanner\n");
        exit(112);
    }

    scanner->column = 1;
    scanner->line = 1;
    scanner->input = NULL;
    scanner->stream = loadInput(scanner, inputFile);
    scanner->pos = scanner->stream;
    scanner->start = scanner->pos;

    return scanner;
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
    if (length >= 512)
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
char *loadInput(Scanner *s, char *fileName)
{
    // Open file for reading bytes
    s->input = fopen(fileName, "rb");

    // Failed read operation
    if (!s->input)
    {
        fprintf(stderr, "MemoryError: Failed to open input file: %s\n", fileName);
        exit(101);
    }

    // Get size of file
    fseek(s->input, 0, SEEK_END);
    long size = ftell(s->input);
    rewind(s->input);

    // Malloc appropriate bytes
    char *buffer = malloc(size + 1);

    // Failed malloc
    if (!buffer)
    {
        fprintf(stderr, "MemoryError: Failed memory allocation for input buffer\n");
        exit(102);
    }

    // Read from the input
    size_t read = fread(buffer, 1, size, s->input);
    // Failed read
    if (read != size)
    {
        fprintf(stderr, "MemoryError: Failed to read input file\n");
        exit(103);
    }

    // String termination
    buffer[size] = '\0';
    fclose(s->input);

    return buffer;
}

// ===== Consumes a Character =====
int next_char(Scanner *s)
{
    char c = *(s->pos);
    s->pos++;

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

    while (isdigit(next) || (next == '.' && !scientific) || ((next == 'e' || next == 'E') && !scientific))
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

            // Change float flag so no more periods
            isDecimal = 1;
            type = FLOAT;
        }
        else if (next == 'e' || next == 'E')
        {
            scientific = 1;
            type = FLOAT;

            next_char(s);

            if (peek(s) == '-' || peek(s) == '+')
            {
                next_char(s);
            }

            if (!isdigit(peek(s)))
            {
                Token returnToken = reportError(
                    107,
                    s->line,
                    "SyntaxError: Line %d column %d\nInvalid scientific notation (Expected digit)",
                    s->line,
                    s->column);

                while (!isspace(*s->pos))
                {
                    if (*s->pos == '\0')
                        break;
                    next_char(s);
                }

                return returnToken;
            }
        }

        // Advance input and increment len
        next_char(s);
        next = peek(s);
    }

    return (Token){
        .type = type,
        .lexeme = start,
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
    size_t len = (size_t) (s->pos - start);
    switch (start[0]) {
        case 'i': {
            if (len == 2 && (memcmp(start, "if", 2) == 0)) return (Token){.type = IF};
            else if (len == 3 && (memcmp(start, "int", 3) == 0)) return (Token){.type = DATATYPE_INT};
            break;
        }
        case 'w': {
            if (len == 5 && (memcmp(start, "while", 5) == 0)) return (Token){.type = WHILE};
            break;
        }
        case 'a': {
            if (len == 3 && (memcmp(start, "and", 3) == 0)) return (Token){.type = AND};
            break;
        }
        case 'o': {
            if (len == 2 && (memcmp(start, "or", 2) == 0)) return (Token){.type = OR};
            break;
        }
        case 'e': {
            if (len == 4 && (memcmp(start, "else", 4) == 0)) return (Token){.type = ELSE};
            break;
        }
        case 'f': {
            if (len == 5 && (memcmp(start, "false", 5) == 0)) return (Token){.type = FALSE};
            else if (len == 3 && (memcmp(start, "for", 3) == 0)) return (Token){.type = FOR};
            else if (len == 5 && (memcmp(start, "float", 5) == 0)) return (Token){.type = DATATYPE_FLOAT};
            break;
        }
        case 'n': {
            if (len == 3 && (memcmp(start, "nil", 3) == 0)) return (Token){.type = NIL};
            else if (len == 3 && (memcmp(start, "not", 3) == 0)) return (Token){.type = NOT};
            break;
        }
        case 'r': {
            if (len == 6 && (memcmp(start, "return", 6) == 0)) return (Token){.type = RETURN};
            break;
        }
        case 't': {
            if (len == 4 && (memcmp(start, "true", 4) == 0)) return (Token){.type = TRUE};
            break;
        }
        case 's': {
            if (len == 3 && (memcmp(start, "str", 3) == 0)) return (Token){.type = DATATYPE_STRING};
            break;
        }
    }

    return (Token){
        .type = IDENTIFIER,
        .lexeme = insert_return_ptr_to_string(start, len),
        .line = s->line,
        .len = len
    };
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
start:

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
            return (Token){.type = LEFT_PAREN, .lexeme = NULL, .line = s->line};
            break;
        case ')':
            return (Token){.type = RIGHT_PAREN, .lexeme = NULL, .line = s->line};
            break;
        case '{':
            return (Token){.type = LEFT_BRACE, .lexeme = NULL, .line = s->line};
            break;
        case '}':
            return (Token){.type = RIGHT_BRACE, .lexeme = NULL, .line = s->line};
            break;
        case '[':
            return (Token){.type = LEFT_BRACKET, .lexeme = NULL, .line = s->line};
            break;
        case ']':
            return (Token){.type = RIGHT_BRACKET, .lexeme = NULL, .line = s->line};
            break;
        case ',':
            return (Token){.type = COMMA, .lexeme = NULL, .line = s->line};
            break;
        // We need to check for floats like .5
        case '.':
        {
            if (isdigit(peek(s)))
            {
                return scan_number(s);
            }
            else
                return (Token){.type = DOT, .lexeme = NULL, .line = s->line};
            break;
        }

        case '/':
        {
            if (peek(s) == '/')
            {
                next_char(s);
                while (peek(s) != '\n' && peek(s) != '\0')
                    next_char(s);

                goto start;
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

                goto start;
            }
            else if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = SLASH_EQUAL, .lexeme = NULL, .line = s->line};
            }
            else
                return (Token){.type = SLASH, .lexeme = NULL, .line = s->line};
            break;
        }
        case '+':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = PLUS_EQUAL, .lexeme = NULL, .line = s->line};
            }
            else if (peek(s) == '+')
            {
                next_char(s);
                return (Token){.type = PLUS_PLUS, .lexeme = NULL, .line = s->line};
            }
            else
                return (Token){.type = PLUS, .lexeme = NULL, .line = s->line};
            break;
        }
        case '-':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = MINUS_EQUAL, .lexeme = NULL, .line = s->line};
            }
            else if (peek(s) == '-')
            {
                next_char(s);
                return (Token){.type = MINUS_MINUS, .lexeme = NULL, .line = s->line};
            }
            else
                return (Token){.type = MINUS, .lexeme = NULL, .line = s->line};
            break;
        }
        case '*':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = STAR_EQUAL, .lexeme = NULL, .line = s->line};
            }
            else
                return (Token){.type = STAR, .lexeme = NULL, .line = s->line};
            break;
        }
        case ';':
            return (Token){.type = SEMICOLON, .lexeme = NULL, .line = s->line};
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
                return (Token){.type = NOT_EQUAL, .lexeme = NULL, .line = s->line};
        }
        case '=':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = EQUAL_EQUAL, .lexeme = NULL, .line = s->line};
            }
            else
                return (Token){.type = EQUAL, .lexeme = NULL, .line = s->line};
        }
        case '%':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = MOD_EQUAL, .lexeme = NULL, .line = s->line};
            }
            else
                return (Token){.type = MOD, .lexeme = NULL, .line = s->line};
        }
        case '~':
        {
            if (peek(s) == '/')
            {
                next_char(s);
                if (peek(s) == '=')
                {
                    next_char(s);
                    return (Token){.type = TILDE_SLASH_EQUAL, .lexeme = NULL, .line = s->line};
                }
                else
                    return (Token){.type = TILDE_SLASH, .lexeme = NULL, .line = s->line};
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
                return (Token){.type = GREATER_EQUAL, .lexeme = NULL, .line = s->line};
            }
            return (Token){.type = GREATER, .lexeme = NULL, .line = s->line};
        }
        case '<':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = LESS_EQUAL, .lexeme = NULL, .line = s->line};
            }
            return (Token){.type = LESS, .lexeme = NULL, .line = s->line};
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