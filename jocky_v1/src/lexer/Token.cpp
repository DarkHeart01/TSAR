#include "jocky/lexer/Token.h"

#include <utility>

namespace jocky {

Token::Token(
    TokenType type,
    std::string lexeme,
    int line,
    int column
)
    : type(type),
      lexeme(std::move(lexeme)),
      line(line),
      column(column) {
}

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::FN: return "FN";
        case TokenType::LET: return "LET";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::WHILE: return "WHILE";
        case TokenType::RETURN: return "RETURN";
        case TokenType::MAIN: return "MAIN";

        case TokenType::INT: return "INT";
        case TokenType::PTR: return "PTR";
        case TokenType::VOID: return "VOID";
        case TokenType::STRING: return "STRING";

        case TokenType::INTEGER: return "INTEGER";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::IDENTIFIER: return "IDENTIFIER";

        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";

        case TokenType::ASSIGN: return "ASSIGN";

        case TokenType::EQ: return "EQ";
        case TokenType::NE: return "NE";
        case TokenType::LT: return "LT";
        case TokenType::GT: return "GT";
        case TokenType::LE: return "LE";
        case TokenType::GE: return "GE";

        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::COLON: return "COLON";
        case TokenType::COMMA: return "COMMA";
        case TokenType::ARROW: return "ARROW";

        case TokenType::EOF_TOKEN: return "EOF";
    }

    return "UNKNOWN";
}

}