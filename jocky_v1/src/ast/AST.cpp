#include "jocky/ast/AST.h"

#include <utility>

namespace jocky {

Parameter::Parameter(
    std::string name,
    std::string type
)
    : name(std::move(name)),
      type(std::move(type)) {
}


FunctionDeclaration::FunctionDeclaration(
    std::string name,
    std::vector<Parameter> parameters,
    std::string returnType,
    std::unique_ptr<Block> body
)
    : name(std::move(name)),
      parameters(std::move(parameters)),
      returnType(std::move(returnType)),
      body(std::move(body)) {
}


MainBlock::MainBlock(
    std::unique_ptr<Block> body
)
    : body(std::move(body)) {
}


VariableDeclaration::VariableDeclaration(
    std::string name,
    std::string type,
    std::unique_ptr<Expression> initializer
)
    : name(std::move(name)),
      type(std::move(type)),
      initializer(std::move(initializer)) {
}


Assignment::Assignment(
    std::string name,
    std::unique_ptr<Expression> value
)
    : name(std::move(name)),
      value(std::move(value)) {
}


IfStatement::IfStatement(
    std::unique_ptr<Expression> condition,
    std::unique_ptr<Block> thenBlock,
    std::unique_ptr<Block> elseBlock
)
    : condition(std::move(condition)),
      thenBlock(std::move(thenBlock)),
      elseBlock(std::move(elseBlock)) {
}


WhileStatement::WhileStatement(
    std::unique_ptr<Expression> condition,
    std::unique_ptr<Block> body
)
    : condition(std::move(condition)),
      body(std::move(body)) {
}


ReturnStatement::ReturnStatement(
    std::unique_ptr<Expression> value
)
    : value(std::move(value)) {
}


ExpressionStatement::ExpressionStatement(
    std::unique_ptr<Expression> expression
)
    : expression(std::move(expression)) {
}


IntegerLiteral::IntegerLiteral(int value)
    : value(value) {
}


StringLiteral::StringLiteral(
    std::string value
)
    : value(std::move(value)) {
}


VariableReference::VariableReference(
    std::string name
)
    : name(std::move(name)) {
}


UnaryExpression::UnaryExpression(
    std::string op,
    std::unique_ptr<Expression> operand
)
    : op(std::move(op)),
      operand(std::move(operand)) {
}


BinaryExpression::BinaryExpression(
    std::unique_ptr<Expression> left,
    std::string op,
    std::unique_ptr<Expression> right
)
    : left(std::move(left)),
      op(std::move(op)),
      right(std::move(right)) {
}


FunctionCall::FunctionCall(
    std::string functionName,
    std::vector<
        std::unique_ptr<Expression>
    > arguments
)
    : functionName(std::move(functionName)),
      arguments(std::move(arguments)) {
}

}