#include "jocky/semantic/SemanticAnalyzer.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace jocky {

// ============================================================
// Constructor
// ============================================================

SemanticAnalyzer::SemanticAnalyzer()
    : currentReturnType(JockyType::VOID) {

    registerBuiltins();
}


// ============================================================
// Built-in Functions
// ============================================================

void SemanticAnalyzer::registerBuiltins() {

    // stdout() -> ptr
    symbols.declareFunction(
        "stdout",
        {},
        JockyType::PTR,
        true
    );

    // exit(int) -> void
    symbols.declareFunction(
        "exit",
        {JockyType::INT},
        JockyType::VOID,
        true
    );

    // alloc(int) -> ptr
    symbols.declareFunction(
        "alloc",
        {JockyType::INT},
        JockyType::PTR,
        true
    );
}


// ============================================================
// Analyze Whole Program
// ============================================================

void SemanticAnalyzer::analyze(
    Program& program
) {

    // First register every function.
    //
    // This allows:
    //
    // fn a() -> int {
    //     return b()
    // }
    //
    // fn b() -> int {
    //     return 10
    // }

    registerFunctions(program);

    // Now analyze function bodies.

    for (auto& function : program.functions) {
        analyzeFunction(*function);
    }

    // Finally analyze main.

    if (program.mainBlock) {
        analyzeMain(*program.mainBlock);
    }
}


// ============================================================
// Register User Functions
// ============================================================

void SemanticAnalyzer::registerFunctions(
    Program& program
) {

    for (auto& function : program.functions) {

        std::vector<JockyType> parameterTypes;

        for (const auto& parameter :
             function->parameters) {

            JockyType type =
                stringToType(parameter.type);

            if (type == JockyType::UNKNOWN) {
                throw std::runtime_error(
                    "Unknown parameter type '" +
                    parameter.type +
                    "' in function '" +
                    function->name +
                    "'"
                );
            }

            parameterTypes.push_back(type);
        }

        JockyType returnType =
            stringToType(function->returnType);

        if (returnType == JockyType::UNKNOWN) {
            throw std::runtime_error(
                "Unknown return type '" +
                function->returnType +
                "' in function '" +
                function->name +
                "'"
            );
        }

        bool success =
            symbols.declareFunction(
                function->name,
                parameterTypes,
                returnType,
                false
            );

        if (!success) {
            throw std::runtime_error(
                "Function '" +
                function->name +
                "' is already declared"
            );
        }
    }
}


// ============================================================
// Analyze Function
// ============================================================

void SemanticAnalyzer::analyzeFunction(
    FunctionDeclaration& function
) {

    currentReturnType =
        stringToType(function.returnType);

    symbols.enterScope();

    // Function parameters become variables
    // inside the function.

    for (const auto& parameter :
         function.parameters) {

        JockyType parameterType =
            stringToType(parameter.type);

        bool success =
            symbols.declareVariable(
                parameter.name,
                parameterType
            );

        if (!success) {
            symbols.exitScope();

            throw std::runtime_error(
                "Duplicate parameter '" +
                parameter.name +
                "' in function '" +
                function.name +
                "'"
            );
        }
    }

    // False because we already created
    // the function scope above.

    analyzeBlock(
        *function.body,
        false
    );

    symbols.exitScope();

    currentReturnType =
        JockyType::VOID;
}


// ============================================================
// Analyze Main
// ============================================================

void SemanticAnalyzer::analyzeMain(
    MainBlock& mainBlock
) {

    currentReturnType =
        JockyType::VOID;

    symbols.enterScope();

    analyzeBlock(
        *mainBlock.body,
        false
    );

    symbols.exitScope();
}


// ============================================================
// Analyze Block
// ============================================================

void SemanticAnalyzer::analyzeBlock(
    Block& block,
    bool createScope
) {

    if (createScope) {
        symbols.enterScope();
    }

    for (auto& statement :
         block.statements) {

        analyzeStatement(
            *statement
        );
    }

    if (createScope) {
        symbols.exitScope();
    }
}


// ============================================================
// Statement Dispatcher
// ============================================================

void SemanticAnalyzer::analyzeStatement(
    Statement& statement
) {

    if (
        auto* declaration =
            dynamic_cast<
                VariableDeclaration*
            >(&statement)
    ) {

        analyzeVariableDeclaration(
            *declaration
        );

        return;
    }


    if (
        auto* assignment =
            dynamic_cast<
                Assignment*
            >(&statement)
    ) {

        analyzeAssignment(
            *assignment
        );

        return;
    }


    if (
        auto* ifStatement =
            dynamic_cast<
                IfStatement*
            >(&statement)
    ) {

        analyzeIfStatement(
            *ifStatement
        );

        return;
    }


    if (
        auto* whileStatement =
            dynamic_cast<
                WhileStatement*
            >(&statement)
    ) {

        analyzeWhileStatement(
            *whileStatement
        );

        return;
    }


    if (
        auto* returnStatement =
            dynamic_cast<
                ReturnStatement*
            >(&statement)
    ) {

        analyzeReturnStatement(
            *returnStatement
        );

        return;
    }


    if (
        auto* expressionStatement =
            dynamic_cast<
                ExpressionStatement*
            >(&statement)
    ) {

        analyzeExpressionStatement(
            *expressionStatement
        );

        return;
    }


    throw std::runtime_error(
        "Unknown statement type"
    );
}


// ============================================================
// Variable Declaration
// ============================================================

void SemanticAnalyzer::analyzeVariableDeclaration(
    VariableDeclaration& statement
) {

    JockyType declaredType =
        stringToType(statement.type);

    if (declaredType == JockyType::UNKNOWN) {

        throw std::runtime_error(
            "Unknown type '" +
            statement.type +
            "' for variable '" +
            statement.name +
            "'"
        );
    }


    if (declaredType == JockyType::VOID) {

        throw std::runtime_error(
            "Variable '" +
            statement.name +
            "' cannot have type void"
        );
    }


    JockyType initializerType =
        analyzeExpression(
            *statement.initializer
        );


    if (declaredType != initializerType) {

        throw std::runtime_error(
            "Type mismatch for variable '" +
            statement.name +
            "': expected " +
            typeToString(declaredType) +
            ", got " +
            typeToString(initializerType)
        );
    }


    bool success =
        symbols.declareVariable(
            statement.name,
            declaredType
        );


    if (!success) {

        throw std::runtime_error(
            "Variable '" +
            statement.name +
            "' is already declared in this scope"
        );
    }
}


// ============================================================
// Assignment
// ============================================================

void SemanticAnalyzer::analyzeAssignment(
    Assignment& statement
) {

    const VariableSymbol* variable =
        symbols.lookupVariable(
            statement.name
        );


    if (!variable) {

        throw std::runtime_error(
            "Variable '" +
            statement.name +
            "' is not declared"
        );
    }


    JockyType valueType =
        analyzeExpression(
            *statement.value
        );


    if (valueType != variable->type) {

        throw std::runtime_error(
            "Cannot assign " +
            typeToString(valueType) +
            " to variable '" +
            statement.name +
            "' of type " +
            typeToString(variable->type)
        );
    }
}


// ============================================================
// If Statement
// ============================================================

void SemanticAnalyzer::analyzeIfStatement(
    IfStatement& statement
) {

    JockyType conditionType =
        analyzeExpression(
            *statement.condition
        );


    if (conditionType != JockyType::BOOL) {

        throw std::runtime_error(
            "If condition must be bool, got " +
            typeToString(conditionType)
        );
    }


    analyzeBlock(
        *statement.thenBlock
    );


    if (statement.elseBlock) {

        analyzeBlock(
            *statement.elseBlock
        );
    }
}


// ============================================================
// While Statement
// ============================================================

void SemanticAnalyzer::analyzeWhileStatement(
    WhileStatement& statement
) {

    JockyType conditionType =
        analyzeExpression(
            *statement.condition
        );


    if (conditionType != JockyType::BOOL) {

        throw std::runtime_error(
            "While condition must be bool, got " +
            typeToString(conditionType)
        );
    }


    analyzeBlock(
        *statement.body
    );
}


// ============================================================
// Return Statement
// ============================================================

void SemanticAnalyzer::analyzeReturnStatement(
    ReturnStatement& statement
) {

    // Our V0 grammar currently requires:
    //
    // return expression

    JockyType returnType =
        analyzeExpression(
            *statement.value
        );


    if (currentReturnType == JockyType::VOID) {

        throw std::runtime_error(
            "Cannot return a value from a void function or main"
        );
    }


    if (returnType != currentReturnType) {

        throw std::runtime_error(
            "Return type mismatch: expected " +
            typeToString(currentReturnType) +
            ", got " +
            typeToString(returnType)
        );
    }
}


// ============================================================
// Expression Statement
// ============================================================

void SemanticAnalyzer::analyzeExpressionStatement(
    ExpressionStatement& statement
) {

    analyzeExpression(
        *statement.expression
    );
}


// ============================================================
// Expression Analysis
// ============================================================

JockyType SemanticAnalyzer::analyzeExpression(
    Expression& expression
) {

    // --------------------------------------------------------
    // Integer Literal
    // --------------------------------------------------------

    if (
        dynamic_cast<
            IntegerLiteral*
        >(&expression)
    ) {

        return JockyType::INT;
    }


    // --------------------------------------------------------
    // String Literal
    // --------------------------------------------------------

    if (
        dynamic_cast<
            StringLiteral*
        >(&expression)
    ) {

        return JockyType::STRING;
    }


    // --------------------------------------------------------
    // Variable Reference
    // --------------------------------------------------------

    if (
        auto* variable =
            dynamic_cast<
                VariableReference*
            >(&expression)
    ) {

        const VariableSymbol* symbol =
            symbols.lookupVariable(
                variable->name
            );


        if (!symbol) {

            throw std::runtime_error(
                "Variable '" +
                variable->name +
                "' is not declared"
            );
        }


        return symbol->type;
    }


    // --------------------------------------------------------
    // Unary Expression
    // --------------------------------------------------------

    if (
        auto* unary =
            dynamic_cast<
                UnaryExpression*
            >(&expression)
    ) {

        JockyType operandType =
            analyzeExpression(
                *unary->operand
            );


        if (unary->op == "-") {

            if (
                operandType !=
                JockyType::INT
            ) {

                throw std::runtime_error(
                    "Unary '-' requires int operand"
                );
            }

            return JockyType::INT;
        }


        throw std::runtime_error(
            "Unknown unary operator '" +
            unary->op +
            "'"
        );
    }


    // --------------------------------------------------------
    // Binary Expression
    // --------------------------------------------------------

    if (
        auto* binary =
            dynamic_cast<
                BinaryExpression*
            >(&expression)
    ) {

        JockyType leftType =
            analyzeExpression(
                *binary->left
            );

        JockyType rightType =
            analyzeExpression(
                *binary->right
            );


        // Arithmetic operators

        if (
            binary->op == "+" ||
            binary->op == "-" ||
            binary->op == "*" ||
            binary->op == "/"
        ) {

            if (
                leftType != JockyType::INT ||
                rightType != JockyType::INT
            ) {

                throw std::runtime_error(
                    "Arithmetic operator '" +
                    binary->op +
                    "' requires int operands"
                );
            }

            return JockyType::INT;
        }


        // Ordering comparisons

        if (
            binary->op == "<" ||
            binary->op == ">" ||
            binary->op == "<=" ||
            binary->op == ">="
        ) {

            if (
                leftType != JockyType::INT ||
                rightType != JockyType::INT
            ) {

                throw std::runtime_error(
                    "Comparison operator '" +
                    binary->op +
                    "' requires int operands"
                );
            }

            return JockyType::BOOL;
        }


        // Equality comparisons

        if (
            binary->op == "==" ||
            binary->op == "!="
        ) {

            if (
                leftType != rightType
            ) {

                throw std::runtime_error(
                    "Cannot compare " +
                    typeToString(leftType) +
                    " with " +
                    typeToString(rightType)
                );
            }

            return JockyType::BOOL;
        }


        throw std::runtime_error(
            "Unknown binary operator '" +
            binary->op +
            "'"
        );
    }


    // --------------------------------------------------------
    // Function Call
    // --------------------------------------------------------

    if (
        auto* call =
            dynamic_cast<
                FunctionCall*
            >(&expression)
    ) {

        const FunctionSymbol* function =
            symbols.lookupFunction(
                call->functionName
            );


        if (!function) {

            throw std::runtime_error(
                "Function '" +
                call->functionName +
                "' is not declared"
            );
        }


        if (
            call->arguments.size() !=
            function->parameterTypes.size()
        ) {

            throw std::runtime_error(
                "Function '" +
                call->functionName +
                "' expects " +
                std::to_string(
                    function->parameterTypes.size()
                ) +
                " arguments, but got " +
                std::to_string(
                    call->arguments.size()
                )
            );
        }


        for (
            std::size_t i = 0;
            i < call->arguments.size();
            ++i
        ) {

            JockyType argumentType =
                analyzeExpression(
                    *call->arguments[i]
                );


            JockyType expectedType =
                function->parameterTypes[i];


            if (
                argumentType !=
                expectedType
            ) {

                throw std::runtime_error(
                    "Argument " +
                    std::to_string(i + 1) +
                    " of function '" +
                    call->functionName +
                    "' expects " +
                    typeToString(expectedType) +
                    ", got " +
                    typeToString(argumentType)
                );
            }
        }


        return function->returnType;
    }


    throw std::runtime_error(
        "Unknown expression type"
    );
}

} // namespace jocky