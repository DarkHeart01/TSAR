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


StructDeclaration::StructDeclaration(
    std::string name,
    std::vector<Parameter> fields
)
    : name(std::move(name)),
      fields(std::move(fields)) {
}


FunctionDeclaration::FunctionDeclaration(
    std::string name,
    std::vector<Parameter> parameters,
    std::string returnType,
    std::unique_ptr<Block> body,
    std::vector<std::string> attributes
)
    : name(std::move(name)),
      parameters(std::move(parameters)),
      returnType(std::move(returnType)),
      body(std::move(body)),
      attributes(std::move(attributes)) {
}


MainBlock::MainBlock(
    std::unique_ptr<Block> body
)
    : body(std::move(body)) {
}


VariableDeclaration::VariableDeclaration(
    std::string name,
    std::string type,
    std::unique_ptr<Expression> initializer,
    bool isVolatile
)
    : name(std::move(name)),
      type(std::move(type)),
      initializer(std::move(initializer)),
      isVolatile(isVolatile) {
}


Assignment::Assignment(
    std::string name,
    std::unique_ptr<Expression> value
)
    : name(std::move(name)),
      value(std::move(value)) {
}


ArrayAssignment::ArrayAssignment(
    std::string name,
    std::unique_ptr<Expression> index,
    std::unique_ptr<Expression> value
)
    : name(std::move(name)),
      index(std::move(index)),
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


ForStatement::ForStatement(
    std::string initName,
    std::string initType,
    std::unique_ptr<Expression> initExpr,
    std::unique_ptr<Expression> condition,
    std::string incName,
    std::unique_ptr<Expression> incExpr,
    std::unique_ptr<Block> body
)
    : initName(std::move(initName)),
      initType(std::move(initType)),
      initExpr(std::move(initExpr)),
      condition(std::move(condition)),
      incName(std::move(incName)),
      incExpr(std::move(incExpr)),
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


IntegerLiteral::IntegerLiteral(long long value)
    : value(value) {
}


BoolLiteral::BoolLiteral(bool value)
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


ArrayIndexExpression::ArrayIndexExpression(
    std::string name,
    std::unique_ptr<Expression> index
)
    : name(std::move(name)),
      index(std::move(index)) {
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


CastExpression::CastExpression(
    std::unique_ptr<Expression> expr,
    std::string targetType
)
    : expr(std::move(expr)),
      targetType(std::move(targetType)) {
}


SizeofExpression::SizeofExpression(std::string typeName)
    : typeName(std::move(typeName)) {
}


TernaryExpression::TernaryExpression(
    std::unique_ptr<Expression> condition,
    std::unique_ptr<Expression> thenExpr,
    std::unique_ptr<Expression> elseExpr
)
    : condition(std::move(condition)),
      thenExpr(std::move(thenExpr)),
      elseExpr(std::move(elseExpr)) {
}


FunctionCall::FunctionCall(
    std::string functionName,
    std::vector<std::unique_ptr<Expression>> arguments
)
    : functionName(std::move(functionName)),
      arguments(std::move(arguments)) {
}

} // namespace jocky
