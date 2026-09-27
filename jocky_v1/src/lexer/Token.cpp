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
        case TokenType::FN:        return "FN";
        case TokenType::LET:       return "LET";
        case TokenType::VOLATILE:  return "VOLATILE";
        case TokenType::IF:        return "IF";
        case TokenType::ELSE:      return "ELSE";
        case TokenType::WHILE:     return "WHILE";
        case TokenType::FOR:       return "FOR";
        case TokenType::BREAK:     return "BREAK";
        case TokenType::CONTINUE:  return "CONTINUE";
        case TokenType::RETURN:    return "RETURN";
        case TokenType::MAIN:      return "MAIN";
        case TokenType::STRUCT:    return "STRUCT";

        case TokenType::INT:       return "INT";
        case TokenType::LONG:      return "LONG";
        case TokenType::BYTE:      return "BYTE";
        case TokenType::BOOL_KW:   return "BOOL_KW";
        case TokenType::PTR:       return "PTR";
        case TokenType::HANDLE:    return "HANDLE";
        case TokenType::FNPTR:     return "FNPTR";
        case TokenType::VOID:      return "VOID";
        case TokenType::STRING:    return "STRING";

        case TokenType::TRUE_LIT:  return "TRUE_LIT";
        case TokenType::FALSE_LIT: return "FALSE_LIT";
        case TokenType::NULL_KW:   return "NULL_KW";

        case TokenType::INTEGER:        return "INTEGER";
        case TokenType::HEX_INTEGER:    return "HEX_INTEGER";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::IDENTIFIER:     return "IDENTIFIER";

        case TokenType::PLUS:   return "PLUS";
        case TokenType::MINUS:  return "MINUS";
        case TokenType::STAR:   return "STAR";
        case TokenType::SLASH:  return "SLASH";

        case TokenType::AMPERSAND: return "AMPERSAND";
        case TokenType::PIPE:      return "PIPE";
        case TokenType::CARET:     return "CARET";
        case TokenType::TILDE:     return "TILDE";
        case TokenType::LSHIFT:    return "LSHIFT";
        case TokenType::RSHIFT:    return "RSHIFT";

        case TokenType::ASSIGN: return "ASSIGN";

        case TokenType::EQ: return "EQ";
        case TokenType::NE: return "NE";
        case TokenType::LT: return "LT";
        case TokenType::GT: return "GT";
        case TokenType::LE: return "LE";
        case TokenType::GE: return "GE";

        case TokenType::AND_AND: return "AND_AND";
        case TokenType::OR_OR:   return "OR_OR";
        case TokenType::BANG:    return "BANG";

        case TokenType::LPAREN:    return "LPAREN";
        case TokenType::RPAREN:    return "RPAREN";
        case TokenType::LBRACE:    return "LBRACE";
        case TokenType::RBRACE:    return "RBRACE";
        case TokenType::LBRACKET:  return "LBRACKET";
        case TokenType::RBRACKET:  return "RBRACKET";
        case TokenType::COLON:     return "COLON";
        case TokenType::COMMA:     return "COMMA";
        case TokenType::ARROW:     return "ARROW";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::QUESTION:  return "QUESTION";

        case TokenType::AT_SIGN:   return "AT_SIGN";
        case TokenType::EOF_TOKEN: return "EOF";
    }

    return "UNKNOWN";
}

} // namespace jocky
