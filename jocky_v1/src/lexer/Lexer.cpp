#include "jocky/lexer/Lexer.h"

#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace jocky {

Lexer::Lexer(const std::string& source)
    : source(source),
      position(0),
      line(1),
      column(1) {
}


char Lexer::currentChar() const {
    if (isAtEnd()) {
        return '\0';
    }

    return source[position];
}


char Lexer::peekChar() const {
    if (position + 1 >= source.size()) {
        return '\0';
    }

    return source[position + 1];
}


bool Lexer::isAtEnd() const {
    return position >= source.size();
}


void Lexer::advance() {
    if (isAtEnd()) {
        return;
    }

    if (source[position] == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }

    position++;
}


void Lexer::skipWhitespace() {
    while (!isAtEnd()) {

        char c = currentChar();

        if (
            c == ' ' ||
            c == '\t' ||
            c == '\r' ||
            c == '\n'
        ) {
            advance();
        } else {
            break;
        }
    }
}


void Lexer::skipComment() {

    while (
        !isAtEnd() &&
        currentChar() != '\n'
    ) {
        advance();
    }
}


Token Lexer::makeToken(
    TokenType type,
    const std::string& lexeme,
    int tokenLine,
    int tokenColumn
) const {

    return Token(
        type,
        lexeme,
        tokenLine,
        tokenColumn
    );
}

Token Lexer::readNumber() {

    int startLine = line;
    int startColumn = column;

    std::size_t start = position;

    while (
        !isAtEnd() &&
        std::isdigit(
            static_cast<unsigned char>(currentChar())
        )
    ) {
        advance();
    }

    std::string value =
        source.substr(start, position - start);

    return makeToken(
        TokenType::INTEGER,
        value,
        startLine,
        startColumn
    );
}

Token Lexer::readIdentifierOrKeyword() {

    static const std::unordered_map<
        std::string,
        TokenType
    > keywords = {

        {"fn", TokenType::FN},
        {"let", TokenType::LET},
        {"if", TokenType::IF},
        {"else", TokenType::ELSE},
        {"while", TokenType::WHILE},
        {"return", TokenType::RETURN},
        {"main", TokenType::MAIN},

        {"int", TokenType::INT},
        {"ptr", TokenType::PTR},
        {"void", TokenType::VOID},
        {"string", TokenType::STRING}
    };

    int startLine = line;
    int startColumn = column;

    std::size_t start = position;

    while (
        !isAtEnd() &&
        (
            std::isalnum(
                static_cast<unsigned char>(currentChar())
            )
            ||
            currentChar() == '_'
        )
    ) {
        advance();
    }

    std::string value =
        source.substr(start, position - start);

    auto found = keywords.find(value);

    if (found != keywords.end()) {

        return makeToken(
            found->second,
            value,
            startLine,
            startColumn
        );
    }

    return makeToken(
        TokenType::IDENTIFIER,
        value,
        startLine,
        startColumn
    );
}

Token Lexer::readString() {

    int startLine = line;
    int startColumn = column;

    advance();

    std::size_t start = position;

    while (
        !isAtEnd() &&
        currentChar() != '"'
    ) {

        if (currentChar() == '\n') {
            throw std::runtime_error(
                "Unterminated string literal"
            );
        }

        advance();
    }

    if (isAtEnd()) {
        throw std::runtime_error(
            "Unterminated string literal"
        );
    }

    std::string value =
        source.substr(start, position - start);

    advance();

    return makeToken(
        TokenType::STRING_LITERAL,
        value,
        startLine,
        startColumn
    );
}


std::vector<Token> Lexer::tokenize() {

    std::vector<Token> tokens;

    while (!isAtEnd()) {

        skipWhitespace();

        if (isAtEnd()) {
            break;
        }

        char c = currentChar();

        int tokenLine = line;
        int tokenColumn = column;

        // Comments
        if (
            c == '/' &&
            peekChar() == '/'
        ) {
            skipComment();
            continue;
        }

        // Numbers
        if (
            std::isdigit(
                static_cast<unsigned char>(c)
            )
        ) {
            tokens.push_back(
                readNumber()
            );

            continue;
        }

        // Identifiers / keywords
        if (
            std::isalpha(
                static_cast<unsigned char>(c)
            )
            ||
            c == '_'
        ) {
            tokens.push_back(
                readIdentifierOrKeyword()
            );

            continue;
        }

        // String literal
        if (c == '"') {

            tokens.push_back(
                readString()
            );

            continue;
        }

        switch (c) {

            case '+':
                tokens.push_back(
                    makeToken(
                        TokenType::PLUS,
                        "+",
                        tokenLine,
                        tokenColumn
                    )
                );

                advance();
                break;


            case '-':

                if (peekChar() == '>') {

                    tokens.push_back(
                        makeToken(
                            TokenType::ARROW,
                            "->",
                            tokenLine,
                            tokenColumn
                        )
                    );

                    advance();
                    advance();

                } else {

                    tokens.push_back(
                        makeToken(
                            TokenType::MINUS,
                            "-",
                            tokenLine,
                            tokenColumn
                        )
                    );

                    advance();
                }

                break;


            case '*':

                tokens.push_back(
                    makeToken(
                        TokenType::STAR,
                        "*",
                        tokenLine,
                        tokenColumn
                    )
                );

                advance();
                break;


            case '/':

                tokens.push_back(
                    makeToken(
                        TokenType::SLASH,
                        "/",
                        tokenLine,
                        tokenColumn
                    )
                );

                advance();
                break;


            case '=':

                if (peekChar() == '=') {

                    tokens.push_back(
                        makeToken(
                            TokenType::EQ,
                            "==",
                            tokenLine,
                            tokenColumn
                        )
                    );

                    advance();
                    advance();

                } else {

                    tokens.push_back(
                        makeToken(
                            TokenType::ASSIGN,
                            "=",
                            tokenLine,
                            tokenColumn
                        )
                    );

                    advance();
                }

                break;


            case '!':

                if (peekChar() == '=') {

                    tokens.push_back(
                        makeToken(
                            TokenType::NE,
                            "!=",
                            tokenLine,
                            tokenColumn
                        )
                    );

                    advance();
                    advance();

                } else {

                    throw std::runtime_error(
                        "Unexpected character '!'."
                    );
                }

                break;


            case '<':

                if (peekChar() == '=') {

                    tokens.push_back(
                        makeToken(
                            TokenType::LE,
                            "<=",
                            tokenLine,
                            tokenColumn
                        )
                    );

                    advance();
                    advance();

                } else {

                    tokens.push_back(
                        makeToken(
                            TokenType::LT,
                            "<",
                            tokenLine,
                            tokenColumn
                        )
                    );

                    advance();
                }

                break;


            case '>':

                if (peekChar() == '=') {

                    tokens.push_back(
                        makeToken(
                            TokenType::GE,
                            ">=",
                            tokenLine,
                            tokenColumn
                        )
                    );

                    advance();
                    advance();

                } else {

                    tokens.push_back(
                        makeToken(
                            TokenType::GT,
                            ">",
                            tokenLine,
                            tokenColumn
                        )
                    );

                    advance();
                }

                break;


            case '(':

                tokens.push_back(
                    makeToken(
                        TokenType::LPAREN,
                        "(",
                        tokenLine,
                        tokenColumn
                    )
                );

                advance();
                break;


            case ')':

                tokens.push_back(
                    makeToken(
                        TokenType::RPAREN,
                        ")",
                        tokenLine,
                        tokenColumn
                    )
                );

                advance();
                break;


            case '{':

                tokens.push_back(
                    makeToken(
                        TokenType::LBRACE,
                        "{",
                        tokenLine,
                        tokenColumn
                    )
                );

                advance();
                break;


            case '}':

                tokens.push_back(
                    makeToken(
                        TokenType::RBRACE,
                        "}",
                        tokenLine,
                        tokenColumn
                    )
                );

                advance();
                break;


            case ':':

                tokens.push_back(
                    makeToken(
                        TokenType::COLON,
                        ":",
                        tokenLine,
                        tokenColumn
                    )
                );

                advance();
                break;


            case ',':

                tokens.push_back(
                    makeToken(
                        TokenType::COMMA,
                        ",",
                        tokenLine,
                        tokenColumn
                    )
                );

                advance();
                break;


            default:

                throw std::runtime_error(
                    "Unexpected character '" +
                    std::string(1, c) +
                    "' at line " +
                    std::to_string(line) +
                    ", column " +
                    std::to_string(column)
                );
        }
    }

    tokens.emplace_back(
        TokenType::EOF_TOKEN,
        "",
        line,
        column
    );

    return tokens;
}

}

