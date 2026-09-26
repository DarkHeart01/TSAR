#pragma once

#include <string>

namespace jocky {

enum class TokenType {
    // Keywords
    FN,
    LET,
    IF,
    ELSE,
    WHILE,
    RETURN,
    MAIN,

    // Types
    INT,
    PTR,
    VOID,
    STRING,

    // Literals / identifiers
    INTEGER,
    STRING_LITERAL,
    IDENTIFIER,

    // Arithmetic
    PLUS,
    MINUS,
    STAR,
    SLASH,

    // Assignment
    ASSIGN,

    // Comparison
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,

    // Structural
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    COLON,
    COMMA,
    ARROW,

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

}