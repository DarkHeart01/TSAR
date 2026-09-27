#pragma once

#include <string>

namespace jocky {

enum class TokenType {
    // Keywords
    FN,
    LET,
    VOLATILE,
    IF,
    ELSE,
    WHILE,
    FOR,
    BREAK,
    CONTINUE,
    RETURN,
    MAIN,
    STRUCT,

    // Types
    INT,
    LONG,
    BYTE,
    BOOL_KW,
    PTR,
    HANDLE,
    FNPTR,
    VOID,
    STRING,

    // Literal keywords
    TRUE_LIT,
    FALSE_LIT,
    NULL_KW,

    // Literals / identifiers
    INTEGER,
    HEX_INTEGER,
    STRING_LITERAL,
    IDENTIFIER,

    // Arithmetic
    PLUS,
    MINUS,
    STAR,
    SLASH,

    // Bitwise
    AMPERSAND,
    PIPE,
    CARET,
    TILDE,
    LSHIFT,
    RSHIFT,

    // Assignment
    ASSIGN,

    // Comparison
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,

    // Logical
    AND_AND,
    OR_OR,
    BANG,

    // Structural
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COLON,
    COMMA,
    ARROW,
    SEMICOLON,
    QUESTION,

    // Attributes
    AT_SIGN,

    // Special
    EOF_TOKEN
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;

    Token(
        TokenType type,
        std::string lexeme,
        int line,
        int column
    );
};

std::string tokenTypeToString(TokenType type);

} // namespace jocky
