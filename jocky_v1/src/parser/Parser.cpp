#include "jocky/parser/Parser.h"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace jocky {

// Named Windows constants — compile-time integer values
static const std::unordered_map<std::string, long long> g_namedConstants = {
    {"MEM_COMMIT",          0x1000LL},
    {"MEM_RESERVE",         0x2000LL},
    {"MEM_COMMIT_RESERVE",  0x3000LL},
    {"MEM_RELEASE",         0x8000LL},
    {"PAGE_NOACCESS",       0x01LL},
    {"PAGE_READONLY",       0x02LL},
    {"PAGE_READWRITE",      0x04LL},
    {"PAGE_WRITECOPY",      0x08LL},
    {"PAGE_EXECUTE",        0x10LL},
    {"PAGE_EXECUTE_READ",   0x20LL},
    {"PAGE_EXECUTE_RW",     0x40LL},
    {"CREATE_SUSPENDED",    0x00000004LL},
    {"CREATE_NEW_CONSOLE",  0x00000010LL},
    {"DETACHED_PROCESS",    0x00000008LL},
    {"GENERIC_READ",        (long long)0x80000000LL},
    {"GENERIC_WRITE",       0x40000000LL},
    {"FILE_SHARE_READ",     0x00000001LL},
    {"OPEN_EXISTING",       3LL},
    {"CREATE_ALWAYS",       2LL},
    {"INVALID_HANDLE",      -1LL},
    {"INFINITE",            (long long)0xFFFFFFFFLL},
    {"WAIT_OBJECT_0",       0LL},
    {"WAIT_TIMEOUT",        0x00000102LL},
};

// ============================================================
// Constructor
// ============================================================

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens),
      position(0) {
}


// ============================================================
// Token Navigation
// ============================================================

const Token& Parser::current() const {
    return tokens[position];
}


const Token& Parser::previous() const {
    return tokens[position - 1];
}


bool Parser::isAtEnd() const {
    return current().type == TokenType::EOF_TOKEN;
}


void Parser::advance() {
    if (!isAtEnd()) position++;
}


bool Parser::check(TokenType type) const {
    if (isAtEnd()) return type == TokenType::EOF_TOKEN;
    return current().type == type;
}


bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}


const Token& Parser::consume(
    TokenType type,
    const std::string& message
) {
    if (check(type)) {
        const Token& token = current();
        advance();
        return token;
    }

    throw std::runtime_error(
        message +
        " at line " + std::to_string(current().line) +
        ", column " + std::to_string(current().column)
    );
}


// ============================================================
// Types
// ============================================================

std::string Parser::parseType() {
    // int  OR  int[N]
    if (match(TokenType::INT)) {
        if (check(TokenType::LBRACKET)) {
            advance();
            const Token& sizeTok = consume(
                TokenType::INTEGER, "Expected array size"
            );
            consume(TokenType::RBRACKET, "Expected ']'");
            return "int[" + sizeTok.lexeme + "]";
        }
        return "int";
    }

    if (match(TokenType::LONG))    return "long";
    if (match(TokenType::BOOL_KW)) return "bool";
    if (match(TokenType::PTR))     return "ptr";
    if (match(TokenType::FNPTR))   return "fnptr";
    if (match(TokenType::VOID))    return "void";
    if (match(TokenType::STRING))  return "string";

    // byte  OR  byte[N]
    if (match(TokenType::BYTE)) {
        if (check(TokenType::LBRACKET)) {
            advance();
            const Token& sizeTok = consume(
                TokenType::INTEGER, "Expected array size"
            );
            consume(TokenType::RBRACKET, "Expected ']'");
            return "byte[" + sizeTok.lexeme + "]";
        }
        return "byte";
    }

    // handle  OR  handle[N]
    if (match(TokenType::HANDLE)) {
        if (check(TokenType::LBRACKET)) {
            advance();
            const Token& sizeTok = consume(
                TokenType::INTEGER, "Expected array size"
            );
            consume(TokenType::RBRACKET, "Expected ']'");
            return "handle[" + sizeTok.lexeme + "]";
        }
        return "handle";
    }

    // Struct name
    if (check(TokenType::IDENTIFIER)) {
        std::string name = current().lexeme;
        advance();
        return name;
    }

    throw std::runtime_error(
        "Expected type at line " + std::to_string(current().line) +
        ", column " + std::to_string(current().column)
    );
}


// ============================================================
// Parameters
// ============================================================

Parameter Parser::parseParameter() {
    const Token& nameToken = consume(
        TokenType::IDENTIFIER,
        "Expected parameter name"
    );

    consume(TokenType::COLON, "Expected ':' after parameter name");

    std::string type = parseType();

    return Parameter(nameToken.lexeme, type);
}


// ============================================================
// Attributes
// ============================================================

std::vector<std::string> Parser::parseAttributes() {
    std::vector<std::string> attributes;

    while (match(TokenType::AT_SIGN)) {
        const Token& nameToken = consume(
            TokenType::IDENTIFIER,
            "Expected attribute name after '@'"
        );
        attributes.push_back(nameToken.lexeme);
    }

    return attributes;
}


// ============================================================
// Struct Declaration
// ============================================================

std::unique_ptr<StructDeclaration>
Parser::parseStructDeclaration() {
    consume(TokenType::STRUCT, "Expected 'struct'");

    const Token& nameToken = consume(
        TokenType::IDENTIFIER,
        "Expected struct name"
    );

    consume(TokenType::LBRACE, "Expected '{'");

    std::vector<Parameter> fields;

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        const Token& fieldName = consume(
            TokenType::IDENTIFIER,
            "Expected field name"
        );
        consume(TokenType::COLON, "Expected ':' after field name");
        std::string fieldType = parseType();
        fields.emplace_back(fieldName.lexeme, fieldType);
    }

    consume(TokenType::RBRACE, "Expected '}'");

    return std::make_unique<StructDeclaration>(
        nameToken.lexeme,
        std::move(fields)
    );
}


// ============================================================
// Function Declaration
// ============================================================

std::unique_ptr<FunctionDeclaration>
Parser::parseFunctionDeclaration() {
    auto attributes = parseAttributes();

    consume(TokenType::FN, "Expected 'fn'");

    const Token& nameToken = consume(
        TokenType::IDENTIFIER,
        "Expected function name"
    );

    consume(TokenType::LPAREN, "Expected '(' after function name");

    std::vector<Parameter> parameters;

    if (!check(TokenType::RPAREN)) {
        parameters.push_back(parseParameter());

        while (match(TokenType::COMMA)) {
            parameters.push_back(parseParameter());
        }
    }

    consume(TokenType::RPAREN, "Expected ')' after parameters");
    consume(TokenType::ARROW, "Expected '->' after parameters");

    std::string returnType = parseType();
    auto body = parseBlock();

    return std::make_unique<FunctionDeclaration>(
        nameToken.lexeme,
        std::move(parameters),
        returnType,
        std::move(body),
        std::move(attributes)
    );
}


// ============================================================
// Program
// ============================================================

std::unique_ptr<Program> Parser::parse() {
    auto program = std::make_unique<Program>();

    // Structs, global variables and functions may appear in any order before main
    while (!check(TokenType::MAIN) && !isAtEnd()) {
        if (check(TokenType::STRUCT)) {
            program->structs.push_back(parseStructDeclaration());
        } else if (check(TokenType::AT_SIGN) || check(TokenType::FN)) {
            program->functions.push_back(parseFunctionDeclaration());
        } else if (check(TokenType::VOLATILE) || check(TokenType::LET)) {
            bool isVol = match(TokenType::VOLATILE);
            auto stmt = parseVariableDeclaration(isVol);
            auto* decl = dynamic_cast<VariableDeclaration*>(stmt.get());
            if (decl) {
                stmt.release();
                program->globals.push_back(
                    std::unique_ptr<VariableDeclaration>(decl));
            }
        } else {
            throw std::runtime_error(
                "Expected main block at line " +
                std::to_string(current().line)
            );
        }
    }

    if (!check(TokenType::MAIN)) {
        throw std::runtime_error(
            "Expected main block at line " +
            std::to_string(current().line)
        );
    }

    program->mainBlock = parseMainBlock();

    consume(TokenType::EOF_TOKEN, "Expected end of file");

    return program;
}


// ============================================================
// Main Block
// ============================================================

std::unique_ptr<MainBlock> Parser::parseMainBlock() {
    consume(TokenType::MAIN, "Expected 'main'");
    auto body = parseBlock();
    return std::make_unique<MainBlock>(std::move(body));
}


// ============================================================
// Block
// ============================================================

std::unique_ptr<Block> Parser::parseBlock() {
    consume(TokenType::LBRACE, "Expected '{'");

    auto block = std::make_unique<Block>();

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        block->statements.push_back(parseStatement());
    }

    consume(TokenType::RBRACE, "Expected '}'");

    return block;
}


// ============================================================
// Statement Dispatcher
// ============================================================

std::unique_ptr<Statement> Parser::parseStatement() {

    // volatile let ...
    if (check(TokenType::VOLATILE)) {
        advance();
        return parseVariableDeclaration(true);
    }

    if (check(TokenType::LET)) {
        return parseVariableDeclaration(false);
    }

    if (check(TokenType::IF)) {
        return parseIfStatement();
    }

    if (check(TokenType::WHILE)) {
        return parseWhileStatement();
    }

    if (check(TokenType::FOR)) {
        return parseForStatement();
    }

    if (check(TokenType::BREAK)) {
        return parseBreakStatement();
    }

    if (check(TokenType::CONTINUE)) {
        return parseContinueStatement();
    }

    if (check(TokenType::RETURN)) {
        return parseReturnStatement();
    }

    // Assignment:  identifier = expr
    // or Array:    identifier [ expr ] = expr
    if (check(TokenType::IDENTIFIER)) {
        // Peek ahead
        if (position + 1 < tokens.size()) {
            TokenType next = tokens[position + 1].type;

            if (next == TokenType::ASSIGN) {
                std::string name = current().lexeme;
                advance(); // identifier
                return parseAssignment(name);
            }

            if (next == TokenType::LBRACKET) {
                std::string name = current().lexeme;
                advance(); // identifier
                return parseArrayAssignment(name);
            }
        }
    }

    return parseExpressionStatement();
}


// ============================================================
// Variable Declaration
// ============================================================

std::unique_ptr<Statement>
Parser::parseVariableDeclaration(bool isVolatile) {
    consume(TokenType::LET, "Expected 'let'");

    const Token& nameToken = consume(
        TokenType::IDENTIFIER,
        "Expected variable name"
    );

    consume(TokenType::COLON, "Expected ':' after variable name");

    std::string type = parseType();

    // Array/struct declarations have no initializer
    if (!check(TokenType::ASSIGN)) {
        return std::make_unique<VariableDeclaration>(
            nameToken.lexeme,
            type,
            nullptr,
            isVolatile
        );
    }

    consume(TokenType::ASSIGN, "Expected '='");

    auto initializer = parseExpression();

    return std::make_unique<VariableDeclaration>(
        nameToken.lexeme,
        type,
        std::move(initializer),
        isVolatile
    );
}


// ============================================================
// Assignment
// ============================================================

std::unique_ptr<Statement>
Parser::parseAssignment(const std::string& name) {
    consume(TokenType::ASSIGN, "Expected '='");
    auto value = parseExpression();
    return std::make_unique<Assignment>(name, std::move(value));
}


// ============================================================
// Array Assignment:  name [ expr ] = expr
// ============================================================

std::unique_ptr<Statement>
Parser::parseArrayAssignment(const std::string& name) {
    consume(TokenType::LBRACKET, "Expected '['");
    auto index = parseExpression();
    consume(TokenType::RBRACKET, "Expected ']'");
    consume(TokenType::ASSIGN, "Expected '='");
    auto value = parseExpression();
    return std::make_unique<ArrayAssignment>(
        name,
        std::move(index),
        std::move(value)
    );
}


// ============================================================
// If Statement
// ============================================================

std::unique_ptr<Statement> Parser::parseIfStatement() {
    consume(TokenType::IF, "Expected 'if'");
    auto condition = parseExpression();
    auto thenBlock = parseBlock();

    std::unique_ptr<Block> elseBlock = nullptr;
    if (match(TokenType::ELSE)) {
        elseBlock = parseBlock();
    }

    return std::make_unique<IfStatement>(
        std::move(condition),
        std::move(thenBlock),
        std::move(elseBlock)
    );
}


// ============================================================
// While Statement
// ============================================================

std::unique_ptr<Statement> Parser::parseWhileStatement() {
    consume(TokenType::WHILE, "Expected 'while'");
    auto condition = parseExpression();
    auto body = parseBlock();
    return std::make_unique<WhileStatement>(
        std::move(condition),
        std::move(body)
    );
}


// ============================================================
// For Statement:  for name : type = expr ; expr ; name = expr  { }
// ============================================================

std::unique_ptr<Statement> Parser::parseForStatement() {
    consume(TokenType::FOR, "Expected 'for'");

    const Token& initName = consume(
        TokenType::IDENTIFIER,
        "Expected variable name in for-init"
    );
    consume(TokenType::COLON, "Expected ':' in for-init");
    std::string initType = parseType();
    consume(TokenType::ASSIGN, "Expected '=' in for-init");
    auto initExpr = parseExpression();

    consume(TokenType::SEMICOLON, "Expected ';' after for-init");

    auto condition = parseExpression();

    consume(TokenType::SEMICOLON, "Expected ';' after for-condition");

    const Token& incName = consume(
        TokenType::IDENTIFIER,
        "Expected variable name in for-increment"
    );
    consume(TokenType::ASSIGN, "Expected '=' in for-increment");
    auto incExpr = parseExpression();

    auto body = parseBlock();

    return std::make_unique<ForStatement>(
        initName.lexeme,
        initType,
        std::move(initExpr),
        std::move(condition),
        incName.lexeme,
        std::move(incExpr),
        std::move(body)
    );
}


// ============================================================
// Break / Continue
// ============================================================

std::unique_ptr<Statement> Parser::parseBreakStatement() {
    consume(TokenType::BREAK, "Expected 'break'");
    return std::make_unique<BreakStatement>();
}


std::unique_ptr<Statement> Parser::parseContinueStatement() {
    consume(TokenType::CONTINUE, "Expected 'continue'");
    return std::make_unique<ContinueStatement>();
}


// ============================================================
// Return Statement
// ============================================================

std::unique_ptr<Statement> Parser::parseReturnStatement() {
    consume(TokenType::RETURN, "Expected 'return'");

    // Bare return (void function)
    if (check(TokenType::RBRACE)) {
        return std::make_unique<ReturnStatement>(nullptr);
    }

    auto value = parseExpression();
    return std::make_unique<ReturnStatement>(std::move(value));
}


// ============================================================
// Expression Statement
// ============================================================

std::unique_ptr<Statement> Parser::parseExpressionStatement() {
    auto expression = parseExpression();
    return std::make_unique<ExpressionStatement>(std::move(expression));
}


// ============================================================
// Expression Root → Ternary
// ============================================================

std::unique_ptr<Expression> Parser::parseExpression() {
    return parseTernary();
}


// ============================================================
// Ternary:  expr ? expr : expr
// ============================================================

std::unique_ptr<Expression> Parser::parseTernary() {
    auto condition = parseLogical();

    if (match(TokenType::QUESTION)) {
        auto thenExpr = parseExpression();
        consume(TokenType::COLON, "Expected ':' in ternary");
        auto elseExpr = parseExpression();
        return std::make_unique<TernaryExpression>(
            std::move(condition),
            std::move(thenExpr),
            std::move(elseExpr)
        );
    }

    return condition;
}


// ============================================================
// Logical:  && ||
// ============================================================

std::unique_ptr<Expression> Parser::parseLogical() {
    auto expr = parseBitwise();

    while (check(TokenType::AND_AND) || check(TokenType::OR_OR)) {
        std::string op = current().lexeme;
        advance();
        auto right = parseBitwise();
        expr = std::make_unique<BinaryExpression>(
            std::move(expr), op, std::move(right)
        );
    }

    return expr;
}


// ============================================================
// Bitwise:  & | ^ << >>
// ============================================================

std::unique_ptr<Expression> Parser::parseBitwise() {
    auto expr = parseEquality();

    while (
        check(TokenType::AMPERSAND) ||
        check(TokenType::PIPE) ||
        check(TokenType::CARET) ||
        check(TokenType::LSHIFT) ||
        check(TokenType::RSHIFT)
    ) {
        std::string op = current().lexeme;
        advance();
        auto right = parseEquality();
        expr = std::make_unique<BinaryExpression>(
            std::move(expr), op, std::move(right)
        );
    }

    return expr;
}


// ============================================================
// Equality:  == !=
// ============================================================

std::unique_ptr<Expression> Parser::parseEquality() {
    auto expr = parseComparison();

    while (check(TokenType::EQ) || check(TokenType::NE)) {
        std::string op = current().lexeme;
        advance();
        auto right = parseComparison();
        expr = std::make_unique<BinaryExpression>(
            std::move(expr), op, std::move(right)
        );
    }

    return expr;
}


// ============================================================
// Comparison:  < > <= >=
// ============================================================

std::unique_ptr<Expression> Parser::parseComparison() {
    auto expr = parseTerm();

    while (
        check(TokenType::LT) ||
        check(TokenType::GT) ||
        check(TokenType::LE) ||
        check(TokenType::GE)
    ) {
        std::string op = current().lexeme;
        advance();
        auto right = parseTerm();
        expr = std::make_unique<BinaryExpression>(
            std::move(expr), op, std::move(right)
        );
    }

    return expr;
}


// ============================================================
// Addition / Subtraction
// ============================================================

std::unique_ptr<Expression> Parser::parseTerm() {
    auto expr = parseFactor();

    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        std::string op = current().lexeme;
        advance();
        auto right = parseFactor();
        expr = std::make_unique<BinaryExpression>(
            std::move(expr), op, std::move(right)
        );
    }

    return expr;
}


// ============================================================
// Multiplication / Division
// ============================================================

std::unique_ptr<Expression> Parser::parseFactor() {
    auto expr = parseUnary();

    while (check(TokenType::STAR) || check(TokenType::SLASH)) {
        std::string op = current().lexeme;
        advance();
        auto right = parseUnary();
        expr = std::make_unique<BinaryExpression>(
            std::move(expr), op, std::move(right)
        );
    }

    return expr;
}


// ============================================================
// Unary:  - ! ~
// ============================================================

std::unique_ptr<Expression> Parser::parseUnary() {
    if (match(TokenType::MINUS)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryExpression>("-", std::move(operand));
    }

    if (match(TokenType::BANG)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryExpression>("!", std::move(operand));
    }

    if (match(TokenType::TILDE)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryExpression>("~", std::move(operand));
    }

    return parsePrimary();
}


// ============================================================
// Primary Expressions
// ============================================================

std::unique_ptr<Expression> Parser::parsePrimary() {

    // Decimal integer
    if (match(TokenType::INTEGER)) {
        long long val = std::stoll(previous().lexeme);
        return std::make_unique<IntegerLiteral>(val);
    }

    // Hex integer
    if (match(TokenType::HEX_INTEGER)) {
        long long val = std::stoll(previous().lexeme, nullptr, 16);
        return std::make_unique<IntegerLiteral>(val);
    }

    // String literal
    if (match(TokenType::STRING_LITERAL)) {
        return std::make_unique<StringLiteral>(previous().lexeme);
    }

    // Boolean literals
    if (match(TokenType::TRUE_LIT)) {
        return std::make_unique<BoolLiteral>(true);
    }
    if (match(TokenType::FALSE_LIT)) {
        return std::make_unique<BoolLiteral>(false);
    }

    // null
    if (match(TokenType::NULL_KW)) {
        return std::make_unique<NullLiteral>();
    }

    // cast(expr, type)
    if (check(TokenType::IDENTIFIER) && current().lexeme == "cast") {
        advance();
        consume(TokenType::LPAREN, "Expected '(' after 'cast'");
        auto expr = parseExpression();
        consume(TokenType::COMMA, "Expected ',' in cast");
        std::string targetType = parseType();
        consume(TokenType::RPAREN, "Expected ')' after cast type");
        return std::make_unique<CastExpression>(std::move(expr), targetType);
    }

    // sizeof(type or identifier)
    if (check(TokenType::IDENTIFIER) && current().lexeme == "sizeof") {
        advance();
        consume(TokenType::LPAREN, "Expected '(' after 'sizeof'");

        // Try to parse a type; fall back to identifier
        std::string typeName;
        if (
            check(TokenType::INT)    || check(TokenType::LONG)   ||
            check(TokenType::BYTE)   || check(TokenType::BOOL_KW) ||
            check(TokenType::PTR)    || check(TokenType::HANDLE) ||
            check(TokenType::FNPTR)  || check(TokenType::VOID)   ||
            check(TokenType::STRING)
        ) {
            typeName = parseType();
        } else {
            typeName = consume(
                TokenType::IDENTIFIER,
                "Expected type or struct name in sizeof"
            ).lexeme;
        }

        consume(TokenType::RPAREN, "Expected ')'");
        return std::make_unique<SizeofExpression>(typeName);
    }

    // Identifier: variable reference, array index, function call, or named constant
    if (match(TokenType::IDENTIFIER)) {
        std::string name = previous().lexeme;

        // Named constant?
        auto constIt = g_namedConstants.find(name);
        if (constIt != g_namedConstants.end()) {
            return std::make_unique<IntegerLiteral>(constIt->second);
        }

        // Function call?
        if (check(TokenType::LPAREN)) {
            return parseFunctionCall(name);
        }

        // Array index?
        if (check(TokenType::LBRACKET)) {
            advance(); // consume '['
            auto index = parseExpression();
            consume(TokenType::RBRACKET, "Expected ']'");
            return std::make_unique<ArrayIndexExpression>(
                name, std::move(index)
            );
        }

        return std::make_unique<VariableReference>(name);
    }

    // Parenthesised expression
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpression();
        consume(TokenType::RPAREN, "Expected ')'");
        return expr;
    }

    throw std::runtime_error(
        "Expected expression at line " +
        std::to_string(current().line) +
        ", column " + std::to_string(current().column)
    );
}


// ============================================================
// Function Call
// ============================================================

std::unique_ptr<Expression>
Parser::parseFunctionCall(const std::string& name) {
    consume(TokenType::LPAREN, "Expected '('");

    std::vector<std::unique_ptr<Expression>> arguments;

    if (!check(TokenType::RPAREN)) {
        arguments.push_back(parseExpression());

        while (match(TokenType::COMMA)) {
            arguments.push_back(parseExpression());
        }
    }

    consume(TokenType::RPAREN, "Expected ')' after arguments");

    return std::make_unique<FunctionCall>(name, std::move(arguments));
}

} // namespace jocky
