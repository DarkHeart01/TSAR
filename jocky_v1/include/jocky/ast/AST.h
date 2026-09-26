#pragma once

#include <memory>
#include <string>
#include <vector>

namespace jocky {

// ============================================================
// Base AST Nodes
// ============================================================

class ASTNode {
public:
    virtual ~ASTNode() = default;
};

class Statement : public ASTNode {
public:
    ~Statement() override = default;
};

class Expression : public ASTNode {
public:
    ~Expression() override = default;
};

// ============================================================
// Program-Level Nodes
// ============================================================

class Parameter {
public:
    std::string name;
    std::string type;

    Parameter(
        std::string name,
        std::string type
    );
};


class Block : public ASTNode {
public:
    std::vector<std::unique_ptr<Statement>> statements;

    Block() = default;
};


class FunctionDeclaration : public ASTNode {
public:
    std::string name;

    std::vector<Parameter> parameters;

    std::string returnType;

    std::unique_ptr<Block> body;

    FunctionDeclaration(
        std::string name,
        std::vector<Parameter> parameters,
        std::string returnType,
        std::unique_ptr<Block> body
    );
};


class MainBlock : public ASTNode {
public:
    std::unique_ptr<Block> body;

    explicit MainBlock(
        std::unique_ptr<Block> body
    );
};


class Program : public ASTNode {
public:
    std::vector<
        std::unique_ptr<FunctionDeclaration>
    > functions;

    std::unique_ptr<MainBlock> mainBlock;

    Program() = default;
};

// ============================================================
// Statement Nodes
// ============================================================

class VariableDeclaration : public Statement {
public:
    std::string name;

    std::string type;

    std::unique_ptr<Expression> initializer;

    VariableDeclaration(
        std::string name,
        std::string type,
        std::unique_ptr<Expression> initializer
    );
};


class Assignment : public Statement {
public:
    std::string name;

    std::unique_ptr<Expression> value;

    Assignment(
        std::string name,
        std::unique_ptr<Expression> value
    );
};


class IfStatement : public Statement {
public:
    std::unique_ptr<Expression> condition;

    std::unique_ptr<Block> thenBlock;

    std::unique_ptr<Block> elseBlock;

    IfStatement(
        std::unique_ptr<Expression> condition,
        std::unique_ptr<Block> thenBlock,
        std::unique_ptr<Block> elseBlock
    );
};


class WhileStatement : public Statement {
public:
    std::unique_ptr<Expression> condition;

    std::unique_ptr<Block> body;

    WhileStatement(
        std::unique_ptr<Expression> condition,
        std::unique_ptr<Block> body
    );
};


class ReturnStatement : public Statement {
public:
    std::unique_ptr<Expression> value;

    explicit ReturnStatement(
        std::unique_ptr<Expression> value
    );
};


class ExpressionStatement : public Statement {
public:
    std::unique_ptr<Expression> expression;

    explicit ExpressionStatement(
        std::unique_ptr<Expression> expression
    );
};

// ============================================================
// Expression Nodes
// ============================================================

class IntegerLiteral : public Expression {
public:
    int value;

    explicit IntegerLiteral(int value);
};


class StringLiteral : public Expression {
public:
    std::string value;

    explicit StringLiteral(std::string value);
};


class VariableReference : public Expression {
public:
    std::string name;

    explicit VariableReference(
        std::string name
    );
};


class UnaryExpression : public Expression {
public:
    std::string op;

    std::unique_ptr<Expression> operand;

    UnaryExpression(
        std::string op,
        std::unique_ptr<Expression> operand
    );
};


class BinaryExpression : public Expression {
public:
    std::unique_ptr<Expression> left;

    std::string op;

    std::unique_ptr<Expression> right;

    BinaryExpression(
        std::unique_ptr<Expression> left,
        std::string op,
        std::unique_ptr<Expression> right
    );
};


class FunctionCall : public Expression {
public:
    std::string functionName;

    std::vector<
        std::unique_ptr<Expression>
    > arguments;

    FunctionCall(
        std::string functionName,
        std::vector<
            std::unique_ptr<Expression>
        > arguments
    );
};

}