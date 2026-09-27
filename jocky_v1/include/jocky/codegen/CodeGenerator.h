#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
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

    void writeToFile(const std::string& path);

private:
    llvm::LLVMContext context;
    llvm::IRBuilder<> builder;
    std::unique_ptr<llvm::Module> module;

    std::vector<
        std::unordered_map<std::string, llvm::Value*>
    > variableScopes;

    // Global variables (emitted as @name = global)
    std::unordered_map<std::string, llvm::GlobalVariable*> globalVars_;

    std::vector<std::pair<llvm::Function*, std::string>>
        pendingAnnotations_;

    // For break/continue targets
    struct LoopContext {
        llvm::BasicBlock* continueTarget;
        llvm::BasicBlock* breakTarget;
    };
    std::vector<LoopContext> loopStack_;

    // Struct name → total byte size (matches SemanticAnalyzer)
    std::unordered_map<std::string, int> structSizes_;

    // Running counter for string literal globals
    int strIdx_ = 0;

    // -------------------------
    // Module setup
    // -------------------------

    void declareRuntimeFunctions();

    void declareUserFunctions(Program& program);

    void collectStructSizes(Program& program);

    void generateGlobals(Program& program);

    llvm::Type* getLLVMType(const std::string& type);

    // -------------------------
    // Scope management
    // -------------------------

    void enterScope();
    void exitScope();

    void declareVariable(
        const std::string& name,
        llvm::Value* value
    );

    llvm::Value* lookupVariable(const std::string& name);

    // Returns storage ptr if found, nullptr otherwise (no throw)
    llvm::Value* tryLookupVariable(const std::string& name);

    // Get the pointee type for either an AllocaInst or GlobalVariable
    llvm::Type* getStorageType(llvm::Value* storage);

    // -------------------------
    // Program generation
    // -------------------------

    void generateFunction(FunctionDeclaration& function);

    void generateMain(MainBlock& mainBlock);

    void generateBlock(Block& block, bool createScope = true);

    // -------------------------
    // Statements
    // -------------------------

    void generateStatement(Statement& statement);

    // -------------------------
    // Expressions
    // -------------------------

    llvm::Value* generateExpression(Expression& expression);

    llvm::Value* generateFunctionCall(FunctionCall& call);

    // -------------------------
    // Helpers
    // -------------------------

    llvm::AllocaInst* createEntryBlockAlloca(
        llvm::Function* function,
        const std::string& name,
        llvm::Type* type
    );

    // Coerce value to the LLVM type of the target alloca
    llvm::Value* coerceToType(llvm::Value* val, llvm::Type* targetTy);

    // Create a string global with align 1 and return the ptr
    llvm::Value* createStringConstant(const std::string& str);

    void flushAnnotations();
};

} // namespace jocky
