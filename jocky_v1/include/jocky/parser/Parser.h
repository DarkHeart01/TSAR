#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "jocky/ast/AST.h"
#include "jocky/lexer/Token.h"

namespace jocky {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    std::unique_ptr<Program> parse();

private:
    const std::vector<Token>& tokens;
    std::size_t position;

    // -----------------------------
    // Token navigation
    // -----------------------------

    const Token& current() const;
    const Token& previous() const;

    bool isAtEnd() const;

    void advance();

    bool check(TokenType type) const;
    bool match(TokenType type);

    const Token& consume(
        TokenType type,
        const std::string& message
    );

    // -----------------------------
    // Program structure
    // -----------------------------

    std::unique_ptr<FunctionDeclaration>
    parseFunctionDeclaration();

    std::unique_ptr<MainBlock>
    parseMainBlock();

    std::unique_ptr<Block>
    parseBlock();

    Parameter parseParameter();

    std::string parseType();

    // -----------------------------
    // Statements
    // -----------------------------

    std::unique_ptr<Statement>
    parseStatement();

    std::unique_ptr<Statement>
    parseVariableDeclaration();

    std::unique_ptr<Statement>
    parseAssignment();

    std::unique_ptr<Statement>
    parseIfStatement();

    std::unique_ptr<Statement>
    parseWhileStatement();

    std::unique_ptr<Statement>
    parseReturnStatement();

    std::unique_ptr<Statement>
    parseExpressionStatement();

    // -----------------------------
    // Expressions
    // -----------------------------

    std::unique_ptr<Expression>
    parseExpression();

    std::unique_ptr<Expression>
    parseEquality();

    std::unique_ptr<Expression>
    parseComparison();

    std::unique_ptr<Expression>
    parseTerm();

    std::unique_ptr<Expression>
    parseFactor();

    std::unique_ptr<Expression>
    parseUnary();

    std::unique_ptr<Expression>
    parsePrimary();

    std::unique_ptr<Expression>
    parseFunctionCall(const std::string& name);
};

}