#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "jocky/ast/AST.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

namespace jocky {

class CodeGenerator {
public:
    CodeGenerator();

    void generate(Program& program);

    void writeToFile(
        const std::string& path
    );

private:
    llvm::LLVMContext context;
    llvm::IRBuilder<> builder;

    std::unique_ptr<llvm::Module> module;

    std::vector<
        std::unordered_map<
            std::string,
            llvm::AllocaInst*
        >
    > variableScopes;

    // -------------------------
    // Module setup
    // -------------------------

    void declareRuntimeFunctions();

    void declareUserFunctions(
        Program& program
    );

    llvm::Type* getLLVMType(
        const std::string& type
    );

    // -------------------------
    // Scope management
    // -------------------------

    void enterScope();
    void exitScope();

    void declareVariable(
        const std::string& name,
        llvm::AllocaInst* value
    );

    llvm::AllocaInst* lookupVariable(
        const std::string& name
    );

    // -------------------------
    // Program generation
    // -------------------------

    void generateFunction(
        FunctionDeclaration& function
    );

    void generateMain(
        MainBlock& mainBlock
    );

    void generateBlock(
        Block& block,
        bool createScope = true
    );

    // -------------------------
    // Statements
    // -------------------------

    void generateStatement(
        Statement& statement
    );

    // -------------------------
    // Expressions
    // -------------------------

    llvm::Value* generateExpression(
        Expression& expression
    );

    llvm::Value* generateFunctionCall(
        FunctionCall& call
    );

    // -------------------------
    // Helpers
    // -------------------------

    llvm::AllocaInst*
    createEntryBlockAlloca(
        llvm::Function* function,
        const std::string& name,
        llvm::Type* type
    );
};

} // namespace jocky