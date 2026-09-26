#pragma once

#include "jocky/ast/AST.h"
#include "jocky/semantic/SymbolTable.h"
#include "jocky/semantic/Types.h"

namespace jocky {

class SemanticAnalyzer {
public:
    SemanticAnalyzer();

    void analyze(Program& program);

private:
    SymbolTable symbols;

    JockyType currentReturnType;

    void registerBuiltins();

    void registerFunctions(
        Program& program
    );

    void analyzeFunction(
        FunctionDeclaration& function
    );

    void analyzeMain(
        MainBlock& mainBlock
    );

    void analyzeBlock(
        Block& block,
        bool createScope = true
    );

    void analyzeStatement(
        Statement& statement
    );

    JockyType analyzeExpression(
        Expression& expression
    );

    void analyzeVariableDeclaration(
        VariableDeclaration& statement
    );

    void analyzeAssignment(
        Assignment& statement
    );

    void analyzeIfStatement(
        IfStatement& statement
    );

    void analyzeWhileStatement(
        WhileStatement& statement
    );

    void analyzeReturnStatement(
        ReturnStatement& statement
    );

    void analyzeExpressionStatement(
        ExpressionStatement& statement
    );
};

} // namespace jocky