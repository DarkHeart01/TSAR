#pragma once

#include <string>
#include <unordered_map>
#include <vector>

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
    int loopDepth_;

    // Struct name → total byte size
    std::unordered_map<std::string, int> structSizes_;

    void registerBuiltins();

    void registerFunctions(Program& program);

    void registerStructs(Program& program);

    void registerGlobals(Program& program);

    void analyzeFunction(FunctionDeclaration& function);

    void analyzeMain(MainBlock& mainBlock);

    void analyzeBlock(Block& block, bool createScope = true);

    void analyzeStatement(Statement& statement);

    JockyType analyzeExpression(Expression& expression);

    void analyzeVariableDeclaration(VariableDeclaration& statement);

    void analyzeAssignment(Assignment& statement);

    void analyzeArrayAssignment(ArrayAssignment& statement);

    void analyzeIfStatement(IfStatement& statement);

    void analyzeWhileStatement(WhileStatement& statement);

    void analyzeForStatement(ForStatement& statement);

    void analyzeReturnStatement(ReturnStatement& statement);

    void analyzeExpressionStatement(ExpressionStatement& statement);

    // Returns true if initType is implicitly convertible to declaredType
    static bool typesCompatible(
        JockyType declared,
        JockyType init,
        const std::string& declaredStr = ""
    );
};

} // namespace jocky
