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


class StructDeclaration : public ASTNode {
public:
    std::string name;
    std::vector<Parameter> fields;

    StructDeclaration(
        std::string name,
        std::vector<Parameter> fields
    );
};


class FunctionDeclaration : public ASTNode {
public:
    std::string name;
    std::vector<Parameter> parameters;
    std::string returnType;
    std::unique_ptr<Block> body;
    std::vector<std::string> attributes;

    FunctionDeclaration(
        std::string name,
        std::vector<Parameter> parameters,
        std::string returnType,
        std::unique_ptr<Block> body,
        std::vector<std::string> attributes = {}
    );
};


class MainBlock : public ASTNode {
public:
    std::unique_ptr<Block> body;

    explicit MainBlock(
        std::unique_ptr<Block> body
    );
};


class VariableDeclaration; // forward declaration for Program::globals

class Program : public ASTNode {
public:
    std::vector<std::unique_ptr<StructDeclaration>> structs;
    std::vector<std::unique_ptr<VariableDeclaration>> globals;
    std::vector<std::unique_ptr<FunctionDeclaration>> functions;
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
    std::unique_ptr<Expression> initializer; // nullable for array/struct decls
    bool isVolatile;

    VariableDeclaration(
        std::string name,
        std::string type,
        std::unique_ptr<Expression> initializer,
        bool isVolatile = false
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


class ArrayAssignment : public Statement {
public:
    std::string name;
    std::unique_ptr<Expression> index;
    std::unique_ptr<Expression> value;

    ArrayAssignment(
        std::string name,
        std::unique_ptr<Expression> index,
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


class ForStatement : public Statement {
public:
    std::string initName;
    std::string initType;
    std::unique_ptr<Expression> initExpr;
    std::unique_ptr<Expression> condition;
    std::string incName;
    std::unique_ptr<Expression> incExpr;
    std::unique_ptr<Block> body;

    ForStatement(
        std::string initName,
        std::string initType,
        std::unique_ptr<Expression> initExpr,
        std::unique_ptr<Expression> condition,
        std::string incName,
        std::unique_ptr<Expression> incExpr,
        std::unique_ptr<Block> body
    );
};


class BreakStatement : public Statement {
public:
    BreakStatement() = default;
};


class ContinueStatement : public Statement {
public:
    ContinueStatement() = default;
};


class ReturnStatement : public Statement {
public:
    std::unique_ptr<Expression> value; // nullable for void return

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
    long long value;

    explicit IntegerLiteral(long long value);
};


class BoolLiteral : public Expression {
public:
    bool value;

    explicit BoolLiteral(bool value);
};


class NullLiteral : public Expression {
public:
    NullLiteral() = default;
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


class ArrayIndexExpression : public Expression {
public:
    std::string name;
    std::unique_ptr<Expression> index;

    ArrayIndexExpression(
        std::string name,
        std::unique_ptr<Expression> index
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


class CastExpression : public Expression {
public:
    std::unique_ptr<Expression> expr;
    std::string targetType;

    CastExpression(
        std::unique_ptr<Expression> expr,
        std::string targetType
    );
};


class SizeofExpression : public Expression {
public:
    std::string typeName; // type or struct name

    explicit SizeofExpression(std::string typeName);
};


class TernaryExpression : public Expression {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> thenExpr;
    std::unique_ptr<Expression> elseExpr;

    TernaryExpression(
        std::unique_ptr<Expression> condition,
        std::unique_ptr<Expression> thenExpr,
        std::unique_ptr<Expression> elseExpr
    );
};


class FunctionCall : public Expression {
public:
    std::string functionName;
    std::vector<std::unique_ptr<Expression>> arguments;

    FunctionCall(
        std::string functionName,
        std::vector<std::unique_ptr<Expression>> arguments
    );
};

} // namespace jocky
