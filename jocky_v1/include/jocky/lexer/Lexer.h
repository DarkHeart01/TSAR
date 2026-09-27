#pragma once

#include <string>
#include <vector>

#include "jocky/lexer/Token.h"

namespace jocky {

class Lexer {
public:
    explicit Lexer(const std::string& source);

    std::vector<Token> tokenize();

private:
    std::string source;

    std::size_t position;
    int line;
    int column;

    char currentChar() const;
    char peekChar() const;
    bool isAtEnd() const;

    void skipLineComment();
    void skipBlockComment();
    void skipWhitespace();
    void skipComment();
    void advance();
    Token readNumber();
    Token readIdentifierOrKeyword();
    Token readString();

    Token makeToken(
        TokenType type,
        const std::string& lexeme,
        int tokenLine,
        int tokenColumn
    ) const;
};

}