#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "stringPool.h"
#include "lexer.h"

/* ===== Error Codes =====
    100: Failed allocation for scanner
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

// ===== Helpers =====
char *loadInput(Scanner *s, char *fileName);
Token reportError(int exitCode, int line, int column, const char *message, ...);
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

    if (!scanner)
    {
        fprintf(stderr, "MemoryError: Failed to allocate scanner\n");
        exit(100);
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
Token reportError(int exitCode, int line, int column, const char *message, ...)
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
        .line = line,
        .column = column};
}

// ===== Input Loading to Buffer =====
char *loadInput(Scanner *s, char *fileName)
{
    // Open file for reading bytes
    s->input = fopen(fileName, "rb");

    // Failed read operation
    if (!s->input)
    {
        fprintf(stderr, "FileError: Failed to open input file: %s\n", fileName);
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
    int start_col = s->column - 1;

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
                    start_col,
                    "SyntaxError: Line %d column %d\nInvalid float: '%s' (Unexpected dot)",
                    s->line,
                    start_col,
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
                    start_col,
                    "SyntaxError: Line %d column %d\nInvalid scientific notation (Expected digit)",
                    s->line,
                    start_col);

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
        .column = start_col,
        .len = s->pos - start};
}

// ===== Identifier Scanning =====
Token scan_identifier(Scanner *s)
{
    // Get start pointer
    char *start = s->start;
    int start_col = s->column - 1;
    int next = peek(s);

    // Run while token remains an identifier
    while (isalnum(next) || next == '_')
    {
        next_char(s);
        next = peek(s);
    }

    // Keyword check
    size_t len = (size_t)(s->pos - start);
    switch (start[0])
    {
    case 'i':
    {
        if (len == 2 && (memcmp(start, "if", 2) == 0))
            return (Token){.type = IF, .len = 2, .column = start_col, .lexeme = "if", .line = s->line};
        else if (len == 3 && (memcmp(start, "int", 3) == 0))
            return (Token){.type = DATATYPE_INT, .len = 3, .column = start_col, .lexeme = "int", .line = s->line};
        break;
    }
    case 'w':
    {
        if (len == 5 && (memcmp(start, "while", 5) == 0))
            return (Token){.type = WHILE, .len = 5, .column = start_col, .lexeme = "while", .line = s->line};
        break;
    }
    case 'a':
    {
        if (len == 3 && (memcmp(start, "and", 3) == 0))
            return (Token){.type = AND, .len = 3, .column = start_col, .lexeme = "and", .line = s->line};
        break;
    }
    case 'o':
    {
        if (len == 2 && (memcmp(start, "or", 2) == 0))
            return (Token){.type = OR, .len = 2, .column = start_col, .lexeme = "or", .line = s->line};
        break;
    }
    case 'e':
    {
        if (len == 4 && (memcmp(start, "else", 4) == 0))
            return (Token){.type = ELSE, .len = 4, .column = start_col, .lexeme = "else", .line = s->line};
        break;
    }
    case 'f':
    {
        if (len == 5 && (memcmp(start, "false", 5) == 0))
            return (Token){.type = FALSE, .len = 5, .column = start_col, .lexeme = "false", .line = s->line};
        else if (len == 3 && (memcmp(start, "for", 3) == 0))
            return (Token){.type = FOR, .len = 3, .column = start_col, .lexeme = "for", .line = s->line};
        else if (len == 5 && (memcmp(start, "float", 5) == 0))
            return (Token){.type = DATATYPE_FLOAT, .len = 5, .column = start_col, .lexeme = "float", .line = s->line};
        else if (len == 4 && (memcmp(start, "func", 4) == 0))
            return (Token){.type = FUNC, .len = 4, .column = start_col, .lexeme = "func", .line = s->line};
        break;
    }
    case 'n':
    {
        if (len == 3 && (memcmp(start, "nil", 3) == 0))
            return (Token){.type = NIL, .len = 3, .column = start_col, .lexeme = "nil", .line = s->line};
        else if (len == 3 && (memcmp(start, "not", 3) == 0))
            return (Token){.type = NOT, .len = 3, .column = start_col, .lexeme = "not", .line = s->line};
        break;
    }
    case 'r':
    {
        if (len == 6 && (memcmp(start, "return", 6) == 0))
            return (Token){.type = RETURN, .len = 6, .column = start_col, .lexeme = "return", .line = s->line};
        break;
    }
    case 't':
    {
        if (len == 4 && (memcmp(start, "true", 4) == 0))
            return (Token){.type = TRUE, .len = 4, .column = start_col, .lexeme = "true", .line = s->line};
        break;
    }
    case 's':
    {
        if (len == 3 && (memcmp(start, "str", 3) == 0))
            return (Token){.type = DATATYPE_STRING, .len = 3, .column = start_col, .lexeme = "str", .line = s->line};
        break;
    }
    case 'b':
    {
        if (len == 4 && (memcmp(start, "bool", 4) == 0))
            return (Token){.type = DATATYPE_BOOL, .len = 4, .column = start_col, .lexeme = "bool", .line = s->line};
        break;
    }
    case 'v':
    {
        if (len == 4 && (memcmp(start, "void", 4) == 0))
            return (Token){.type = DATATYPE_VOID, .len = 4, .column = start_col, .lexeme = "void", .line = s->line};
        break;
    }
    }

    return (Token){
        .type = IDENTIFIER,
        .lexeme = insert_return_ptr_to_string(start, len),
        .line = s->line,
        .column = start_col,
        .len = len};
}

// ===== String Handling =====
Token scan_string(Scanner *s)
{
    // We don't go back to the previously consumed character as that is a quote.
    int multiline = 0;
    int start_line = s->line;
    int start_col = s->column - 1;

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
                        start_line,
                        start_col,
                        "SyntaxError: Line %d column %d\nUnclosed string:\n%s (Expected \"\"\")",
                        start_line,
                        start_col,
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
                    start_line,
                    s->column,
                    "SyntaxError: Line %d column %d\nUnclosed string:\n%s (Expected \"\"\")",
                    start_line,
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
                    start_col,
                    "SyntaxError: Line %d column %d\nUnclosed string:\n%s (Expected \")",
                    s->line,
                    start_col,
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
                        start_col,
                        "SyntaxError: Line %d column %d\nUnclosed string:\n%s (Expected \")",
                        s->line,
                        start_col,
                        insert_return_ptr_to_string(string, len));

                    free(string);
                    return returnError;
                }
                else
                {
                    Token returnError = reportError(
                        106,
                        s->line,
                        start_col,
                        "SyntaxError: Line %d column %d\nUnclosed string:\n%s (Expected \"\"\")",
                        s->line,
                        start_col,
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
                    start_col,
                    "SyntaxError: Line %d column %d\nInvalid escape sequence: '\\%c'",
                    s->line,
                    start_col,
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
                    start_col,
                    "SyntaxError: Line %d column %d\nUse of line break in single line string is not permitted. To create a multiline string, wrap the string in triple quotes (\"\"\")",
                    s->line,
                    start_col);

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
        .column = start_col,
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
        return (Token){.type = TOKEN_EOF, .column = s->column - 1, .lexeme = "<EOF>", .len = 5, .line = s->line};

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
        int start_column = s->column - 1;
        switch (c)
        {
        case '(':
            return (Token){.type = LEFT_PAREN, .line = s->line, .column = start_column, .lexeme = "(", .len = 1};
            break;
        case ')':
            return (Token){.type = RIGHT_PAREN, .line = s->line, .column = start_column, .lexeme = ")", .len = 1};
            break;
        case '{':
            return (Token){.type = LEFT_BRACE, .line = s->line, .column = start_column, .lexeme = "{", .len = 1};
            break;
        case '}':
            return (Token){.type = RIGHT_BRACE, .line = s->line, .column = start_column, .lexeme = "}", .len = 1};
            break;
        case '[':
            return (Token){.type = LEFT_BRACKET, .line = s->line, .column = start_column, .lexeme = "[", .len = 1};
            break;
        case ']':
            return (Token){.type = RIGHT_BRACKET, .line = s->line, .column = start_column, .lexeme = "]", .len = 1};
            break;
        case ',':
            return (Token){.type = COMMA, .line = s->line, .column = start_column, .lexeme = ",", .len = 1};
            break;
        // We need to check for floats like .5
        case '.':
        {
            if (isdigit(peek(s)))
            {
                return scan_number(s);
            }
            else
                return (Token){.type = DOT, .line = s->line, .column = start_column, .lexeme = ".", .len = 1};
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
                            start_column,
                            "SyntaxError: Line %d column %d\nUnclosed multiline comment",
                            start_line,
                            start_column);
                }

                goto start;
            }
            else if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = SLASH_EQUAL, .line = s->line, .column = start_column, .lexeme = "/=", .len = 2};
            }
            else
                return (Token){.type = SLASH, .line = s->line, .column = start_column, .lexeme = "/", .len = 1};
            break;
        }
        case '+':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = PLUS_EQUAL, .line = s->line, .column = start_column, .lexeme = "+=", .len = 2};
            }
            else if (peek(s) == '+')
            {
                next_char(s);
                return (Token){.type = PLUS_PLUS, .line = s->line, .column = start_column, .lexeme = "++", .len = 2};
            }
            else
                return (Token){.type = PLUS, .line = s->line, .column = start_column, .lexeme = "+", .len = 1};
            break;
        }
        case '-':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = MINUS_EQUAL, .line = s->line, .column = start_column, .lexeme = "-=", .len = 2};
            }
            else if (peek(s) == '-')
            {
                next_char(s);
                return (Token){.type = MINUS_MINUS, .line = s->line, .column = start_column, .lexeme = "--", .len = 2};
            }
            else if (peek(s) == '>')
            {
                next_char(s);
                return (Token){.type = ARROW, .line = s->line, .column = start_column, .lexeme = "->", .len = 2};
            }
            else
                return (Token){.type = MINUS, .line = s->line, .column = start_column, .lexeme = "-", .len = 1};
            break;
        }
        case '*':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = STAR_EQUAL, .line = s->line, .column = start_column, .lexeme = "*=", .len = 2};
            }
            else
                return (Token){.type = STAR, .line = s->line, .column = start_column, .lexeme = "*", .len = 1};
            break;
        }
        case ';':
            return (Token){.type = SEMICOLON, .line = s->line, .column = start_column, .lexeme = ";", .len = 1};
            break;
        case '!':
        {
            if (peek(s) != '=')
                return reportError(
                    111,
                    s->line,
                    start_column,
                    "SyntaxError: Line %d column %d\nUnexpected character '!'",
                    s->line,
                    start_column);
            else
                next_char(s);
            return (Token){.type = NOT_EQUAL, .line = s->line, .column = start_column, .lexeme = "!=", .len = 2};
        }
        case '=':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = EQUAL_EQUAL, .line = s->line, .column = start_column, .lexeme = "==", .len = 2};
            }
            else
                return (Token){.type = EQUAL, .line = s->line, .column = start_column, .lexeme = "=", .len = 1};
        }
        case '%':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = MOD_EQUAL, .line = s->line, .column = start_column, .lexeme = "%=", .len = 2};
            }
            else
                return (Token){.type = MOD, .line = s->line, .column = start_column, .lexeme = "%", .len = 1};
        }
        case '>':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = GREATER_EQUAL, .line = s->line, .column = start_column, .lexeme = ">=", .len = 2};
            }
            return (Token){.type = GREATER, .line = s->line, .column = start_column, .lexeme = ">", .len = 1};
        }
        case '<':
        {
            if (peek(s) == '=')
            {
                next_char(s);
                return (Token){.type = LESS_EQUAL, .line = s->line, .column = start_column, .lexeme = "<=", .len = 2};
            }
            return (Token){.type = LESS, .line = s->line, .column = start_column, .lexeme = "<", .len = 1};
        }
        }
    }

    return reportError(
        111,
        s->line,
        s->column - 1,
        "SyntaxError: Line %d column %d\nUnexpected character '%c'",
        s->line,
        s->column - 1,
        c);
}

// == Peek token (for parsing) ==
Token peek_token(Scanner *s)
{
    Scanner saved = *s;
    Token next = next_token(s);

    *s = saved;
    return next;
}