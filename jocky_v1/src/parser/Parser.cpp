#include "jocky/parser/Parser.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace jocky {

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
    if (!isAtEnd()) {
        position++;
    }
}


bool Parser::check(TokenType type) const {
    if (isAtEnd()) {
        return type == TokenType::EOF_TOKEN;
    }

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
        " at line " +
        std::to_string(current().line) +
        ", column " +
        std::to_string(current().column)
    );
}


// ============================================================
// Program
// ============================================================

std::unique_ptr<Program> Parser::parse() {
    auto program =
        std::make_unique<Program>();

    while (check(TokenType::FN)) {
        program->functions.push_back(
            parseFunctionDeclaration()
        );
    }

    if (!check(TokenType::MAIN)) {
        throw std::runtime_error(
            "Expected main block at line " +
            std::to_string(current().line) +
            ", column " +
            std::to_string(current().column)
        );
    }

    program->mainBlock =
        parseMainBlock();

    consume(
        TokenType::EOF_TOKEN,
        "Expected end of file"
    );

    return program;
}


// ============================================================
// Types
// ============================================================

std::string Parser::parseType() {
    if (match(TokenType::INT)) {
        return "int";
    }

    if (match(TokenType::PTR)) {
        return "ptr";
    }

    if (match(TokenType::VOID)) {
        return "void";
    }

    if (match(TokenType::STRING)) {
        return "string";
    }

    throw std::runtime_error(
        "Expected type at line " +
        std::to_string(current().line) +
        ", column " +
        std::to_string(current().column)
    );
}


// ============================================================
// Parameters
// ============================================================

Parameter Parser::parseParameter() {
    const Token& nameToken =
        consume(
            TokenType::IDENTIFIER,
            "Expected parameter name"
        );

    consume(
        TokenType::COLON,
        "Expected ':' after parameter name"
    );

    std::string type =
        parseType();

    return Parameter(
        nameToken.lexeme,
        type
    );
}


// ============================================================
// Function Declaration
// ============================================================

std::unique_ptr<FunctionDeclaration>
Parser::parseFunctionDeclaration() {
    consume(
        TokenType::FN,
        "Expected 'fn'"
    );

    const Token& nameToken =
        consume(
            TokenType::IDENTIFIER,
            "Expected function name"
        );

    consume(
        TokenType::LPAREN,
        "Expected '(' after function name"
    );

    std::vector<Parameter> parameters;

    if (!check(TokenType::RPAREN)) {
        parameters.push_back(
            parseParameter()
        );

        while (match(TokenType::COMMA)) {
            parameters.push_back(
                parseParameter()
            );
        }
    }

    consume(
        TokenType::RPAREN,
        "Expected ')' after parameters"
    );

    consume(
        TokenType::ARROW,
        "Expected '->' after parameters"
    );

    std::string returnType =
        parseType();

    auto body =
        parseBlock();

    return std::make_unique<
        FunctionDeclaration
    >(
        nameToken.lexeme,
        std::move(parameters),
        returnType,
        std::move(body)
    );
}


// ============================================================
// Main Block
// ============================================================

std::unique_ptr<MainBlock>
Parser::parseMainBlock() {
    consume(
        TokenType::MAIN,
        "Expected 'main'"
    );

    auto body =
        parseBlock();

    return std::make_unique<MainBlock>(
        std::move(body)
    );
}


// ============================================================
// Block
// ============================================================

std::unique_ptr<Block>
Parser::parseBlock() {
    consume(
        TokenType::LBRACE,
        "Expected '{'"
    );

    auto block =
        std::make_unique<Block>();

    while (
        !check(TokenType::RBRACE) &&
        !isAtEnd()
    ) {
        block->statements.push_back(
            parseStatement()
        );
    }

    consume(
        TokenType::RBRACE,
        "Expected '}'"
    );

    return block;
}


// ============================================================
// Statement Dispatcher
// ============================================================

std::unique_ptr<Statement>
Parser::parseStatement() {
    if (check(TokenType::LET)) {
        return parseVariableDeclaration();
    }

    if (check(TokenType::IF)) {
        return parseIfStatement();
    }

    if (check(TokenType::WHILE)) {
        return parseWhileStatement();
    }

    if (check(TokenType::RETURN)) {
        return parseReturnStatement();
    }

    // Assignment:
    // identifier = expression
    if (
        check(TokenType::IDENTIFIER) &&
        position + 1 < tokens.size() &&
        tokens[position + 1].type ==
            TokenType::ASSIGN
    ) {
        return parseAssignment();
    }

    return parseExpressionStatement();
}


// ============================================================
// Variable Declaration
// ============================================================

std::unique_ptr<Statement>
Parser::parseVariableDeclaration() {
    consume(
        TokenType::LET,
        "Expected 'let'"
    );

    const Token& nameToken =
        consume(
            TokenType::IDENTIFIER,
            "Expected variable name"
        );

    consume(
        TokenType::COLON,
        "Expected ':' after variable name"
    );

    std::string type =
        parseType();

    consume(
        TokenType::ASSIGN,
        "Expected '=' after variable type"
    );

    auto initializer =
        parseExpression();

    return std::make_unique<
        VariableDeclaration
    >(
        nameToken.lexeme,
        type,
        std::move(initializer)
    );
}


// ============================================================
// Assignment
// ============================================================

std::unique_ptr<Statement>
Parser::parseAssignment() {
    const Token& nameToken =
        consume(
            TokenType::IDENTIFIER,
            "Expected variable name"
        );

    consume(
        TokenType::ASSIGN,
        "Expected '='"
    );

    auto value =
        parseExpression();

    return std::make_unique<Assignment>(
        nameToken.lexeme,
        std::move(value)
    );
}


// ============================================================
// If Statement
// ============================================================

std::unique_ptr<Statement>
Parser::parseIfStatement() {
    consume(
        TokenType::IF,
        "Expected 'if'"
    );

    auto condition =
        parseExpression();

    auto thenBlock =
        parseBlock();

    std::unique_ptr<Block> elseBlock =
        nullptr;

    if (match(TokenType::ELSE)) {
        elseBlock =
            parseBlock();
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

std::unique_ptr<Statement>
Parser::parseWhileStatement() {
    consume(
        TokenType::WHILE,
        "Expected 'while'"
    );

    auto condition =
        parseExpression();

    auto body =
        parseBlock();

    return std::make_unique<
        WhileStatement
    >(
        std::move(condition),
        std::move(body)
    );
}


// ============================================================
// Return Statement
// ============================================================

std::unique_ptr<Statement>
Parser::parseReturnStatement() {
    consume(
        TokenType::RETURN,
        "Expected 'return'"
    );

    auto value =
        parseExpression();

    return std::make_unique<
        ReturnStatement
    >(
        std::move(value)
    );
}


// ============================================================
// Expression Statement
// ============================================================

std::unique_ptr<Statement>
Parser::parseExpressionStatement() {
    auto expression =
        parseExpression();

    return std::make_unique<
        ExpressionStatement
    >(
        std::move(expression)
    );
}


// ============================================================
// Expression Root
// ============================================================

std::unique_ptr<Expression>
Parser::parseExpression() {
    return parseEquality();
}


// ============================================================
// Equality
//
// ==
// !=
// ============================================================

std::unique_ptr<Expression>
Parser::parseEquality() {
    auto expression =
        parseComparison();

    while (
        check(TokenType::EQ) ||
        check(TokenType::NE)
    ) {
        std::string op =
            current().lexeme;

        advance();

        auto right =
            parseComparison();

        expression =
            std::make_unique<
                BinaryExpression
            >(
                std::move(expression),
                op,
                std::move(right)
            );
    }

    return expression;
}


// ============================================================
// Comparison
//
// <
// >
// <=
// >=
// ============================================================

std::unique_ptr<Expression>
Parser::parseComparison() {
    auto expression =
        parseTerm();

    while (
        check(TokenType::LT) ||
        check(TokenType::GT) ||
        check(TokenType::LE) ||
        check(TokenType::GE)
    ) {
        std::string op =
            current().lexeme;

        advance();

        auto right =
            parseTerm();

        expression =
            std::make_unique<
                BinaryExpression
            >(
                std::move(expression),
                op,
                std::move(right)
            );
    }

    return expression;
}


// ============================================================
// Addition / Subtraction
//
// +
// -
// ============================================================

std::unique_ptr<Expression>
Parser::parseTerm() {
    auto expression =
        parseFactor();

    while (
        check(TokenType::PLUS) ||
        check(TokenType::MINUS)
    ) {
        std::string op =
            current().lexeme;

        advance();

        auto right =
            parseFactor();

        expression =
            std::make_unique<
                BinaryExpression
            >(
                std::move(expression),
                op,
                std::move(right)
            );
    }

    return expression;
}


// ============================================================
// Multiplication / Division
//
// *
// /
// ============================================================

std::unique_ptr<Expression>
Parser::parseFactor() {
    auto expression =
        parseUnary();

    while (
        check(TokenType::STAR) ||
        check(TokenType::SLASH)
    ) {
        std::string op =
            current().lexeme;

        advance();

        auto right =
            parseUnary();

        expression =
            std::make_unique<
                BinaryExpression
            >(
                std::move(expression),
                op,
                std::move(right)
            );
    }

    return expression;
}


// ============================================================
// Unary
//
// -expression
// ============================================================

std::unique_ptr<Expression>
Parser::parseUnary() {
    if (match(TokenType::MINUS)) {
        auto operand =
            parseUnary();

        return std::make_unique<
            UnaryExpression
        >(
            "-",
            std::move(operand)
        );
    }

    return parsePrimary();
}


// ============================================================
// Primary Expressions
//
// INTEGER
// STRING_LITERAL
// IDENTIFIER
// function_call
// (expression)
// ============================================================

std::unique_ptr<Expression>
Parser::parsePrimary() {
    if (match(TokenType::INTEGER)) {
        return std::make_unique<
            IntegerLiteral
        >(
            std::stoi(
                previous().lexeme
            )
        );
    }

    if (match(TokenType::STRING_LITERAL)) {
        return std::make_unique<
            StringLiteral
        >(
            previous().lexeme
        );
    }

    if (match(TokenType::IDENTIFIER)) {
        std::string name =
            previous().lexeme;

        if (check(TokenType::LPAREN)) {
            return parseFunctionCall(name);
        }

        return std::make_unique<
            VariableReference
        >(name);
    }

    if (match(TokenType::LPAREN)) {
        auto expression =
            parseExpression();

        consume(
            TokenType::RPAREN,
            "Expected ')'"
        );

        return expression;
    }

    throw std::runtime_error(
        "Expected expression at line " +
        std::to_string(current().line) +
        ", column " +
        std::to_string(current().column)
    );
}


// ============================================================
// Function Call
// ============================================================

std::unique_ptr<Expression>
Parser::parseFunctionCall(
    const std::string& name
) {
    consume(
        TokenType::LPAREN,
        "Expected '('"
    );

    std::vector<
        std::unique_ptr<Expression>
    > arguments;

    if (!check(TokenType::RPAREN)) {
        arguments.push_back(
            parseExpression()
        );

        while (match(TokenType::COMMA)) {
            arguments.push_back(
                parseExpression()
            );
        }
    }

    consume(
        TokenType::RPAREN,
        "Expected ')' after arguments"
    );

    return std::make_unique<
        FunctionCall
    >(
        name,
        std::move(arguments)
    );
}

} // namespace jocky