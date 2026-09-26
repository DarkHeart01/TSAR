#include "jocky/codegen/CodeGenerator.h"

#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

namespace jocky {

// ============================================================
// Constructor
// ============================================================

CodeGenerator::CodeGenerator()
    : context(),
      builder(context),
      module(std::make_unique<llvm::Module>("jocky_module", context)) {

    module->setDataLayout(
        "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
    );

    module->setTargetTriple(
        "x86_64-pc-windows-msvc19.44.35225"
    );
}


// ============================================================
// Type Mapping
// ============================================================

llvm::Type*
CodeGenerator::getLLVMType(
    const std::string& type
) {

    if (type == "int") {
        return llvm::Type::getInt32Ty(context);
    }

    if (type == "void") {
        return llvm::Type::getVoidTy(context);
    }

    if (
        type == "ptr" ||
        type == "string"
    ) {
        return llvm::PointerType::getUnqual(
            context
        );
    }

    if (type == "bool") {
        return llvm::Type::getInt1Ty(context);
    }

    throw std::runtime_error(
        "Unknown LLVM type: " + type
    );
}


// ============================================================
// Runtime Functions
// ============================================================

void CodeGenerator::declareRuntimeFunctions() {

    auto* voidType =
        llvm::Type::getVoidTy(context);

    auto* intType =
        llvm::Type::getInt32Ty(context);

    auto* ptrType =
        llvm::PointerType::getUnqual(
            context
        );


    // jocky_write(ptr, ptr, int) -> void

    auto* writeType =
        llvm::FunctionType::get(
            voidType,
            {ptrType, ptrType, intType},
            false
        );

    auto* writeFunction =
        llvm::Function::Create(
            writeType,
            llvm::Function::ExternalLinkage,
            "jocky_write",
            module.get()
        );

    writeFunction->addParamAttr(0, llvm::Attribute::NoUndef);
    writeFunction->addParamAttr(1, llvm::Attribute::NoUndef);
    writeFunction->addParamAttr(2, llvm::Attribute::NoUndef);


    // jocky_exit(int) -> void

    auto* exitType =
        llvm::FunctionType::get(
            voidType,
            {intType},
            false
        );

    auto* exitFunction =
        llvm::Function::Create(
            exitType,
            llvm::Function::ExternalLinkage,
            "jocky_exit",
            module.get()
        );

    exitFunction->addParamAttr(0, llvm::Attribute::NoUndef);


    // jocky_alloc(int) -> ptr

    auto* allocType =
        llvm::FunctionType::get(
            ptrType,
            {intType},
            false
        );

    auto* allocFunction =
        llvm::Function::Create(
            allocType,
            llvm::Function::ExternalLinkage,
            "jocky_alloc",
            module.get()
        );

    allocFunction->addParamAttr(0, llvm::Attribute::NoUndef);


    // jocky_get_stdout() -> ptr

    auto* stdoutType =
        llvm::FunctionType::get(
            ptrType,
            {},
            false
        );

    llvm::Function::Create(
        stdoutType,
        llvm::Function::ExternalLinkage,
        "jocky_get_stdout",
        module.get()
    );


    // jocky_create_thread(ptr, ptr) -> ptr

    auto* createThreadType =
        llvm::FunctionType::get(
            ptrType,
            {ptrType, ptrType},
            false
        );

    auto* createThreadFunction =
        llvm::Function::Create(
            createThreadType,
            llvm::Function::ExternalLinkage,
            "jocky_create_thread",
            module.get()
        );

    createThreadFunction->addParamAttr(0, llvm::Attribute::NoUndef);
    createThreadFunction->addParamAttr(1, llvm::Attribute::NoUndef);


    // jocky_wait(ptr, int) -> int

    auto* waitType =
        llvm::FunctionType::get(
            intType,
            {ptrType, intType},
            false
        );

    auto* waitFunction =
        llvm::Function::Create(
            waitType,
            llvm::Function::ExternalLinkage,
            "jocky_wait",
            module.get()
        );

    waitFunction->addParamAttr(0, llvm::Attribute::NoUndef);
    waitFunction->addParamAttr(1, llvm::Attribute::NoUndef);
}


// ============================================================
// Declare User Function Prototypes
// ============================================================

void CodeGenerator::declareUserFunctions(
    Program& program
) {

    for (auto& function :
         program.functions) {

        std::vector<llvm::Type*>
            parameterTypes;

        for (const auto& parameter :
             function->parameters) {

            parameterTypes.push_back(
                getLLVMType(
                    parameter.type
                )
            );
        }

        auto* returnType =
            getLLVMType(
                function->returnType
            );

        auto* functionType =
            llvm::FunctionType::get(
                returnType,
                parameterTypes,
                false
            );

        auto* llvmFunction =
            llvm::Function::Create(
                functionType,
                llvm::Function::ExternalLinkage,
                function->name,
                module.get()
            );
        for (auto& argument : llvmFunction->args()) {
            argument.addAttr(
                llvm::Attribute::NoUndef
            );
        }

        llvmFunction->addFnAttr(llvm::Attribute::NoInline);
        llvmFunction->addFnAttr(llvm::Attribute::NoUnwind);
        llvmFunction->addFnAttr(llvm::Attribute::OptimizeNone);
        llvmFunction->setUWTableKind(llvm::UWTableKind::Default);
    }
}


// ============================================================
// Scope Management
// ============================================================

void CodeGenerator::enterScope() {
    variableScopes.emplace_back();
}


void CodeGenerator::exitScope() {

    if (!variableScopes.empty()) {
        variableScopes.pop_back();
    }
}


void CodeGenerator::declareVariable(
    const std::string& name,
    llvm::AllocaInst* value
) {

    if (variableScopes.empty()) {
        enterScope();
    }

    variableScopes.back()[name] =
        value;
}


llvm::AllocaInst*
CodeGenerator::lookupVariable(
    const std::string& name
) {

    for (
        auto scope =
            variableScopes.rbegin();

        scope != variableScopes.rend();

        ++scope
    ) {

        auto found =
            scope->find(name);

        if (found != scope->end()) {
            return found->second;
        }
    }

    throw std::runtime_error(
        "Codegen could not find variable '" +
        name +
        "'"
    );
}


// ============================================================
// Alloca Helper
// ============================================================

llvm::AllocaInst*
CodeGenerator::createEntryBlockAlloca(
    llvm::Function* function,
    const std::string& name,
    llvm::Type* type
) {

    llvm::IRBuilder<> temporaryBuilder(
        &function->getEntryBlock(),
        function->getEntryBlock().begin()
    );

    return temporaryBuilder.CreateAlloca(
        type,
        nullptr,
        name
    );
}


// ============================================================
// Generate Whole Program
// ============================================================

void CodeGenerator::generate(
    Program& program
) {

    declareRuntimeFunctions();

    declareUserFunctions(program);

    for (auto& function :
         program.functions) {

        generateFunction(
            *function
        );
    }

    if (program.mainBlock) {

        generateMain(
            *program.mainBlock
        );
    }
}


// ============================================================
// Generate Function
// ============================================================

void CodeGenerator::generateFunction(
    FunctionDeclaration& functionNode
) {

    llvm::Function* function =
        module->getFunction(
            functionNode.name
        );


    if (!function) {

        throw std::runtime_error(
            "LLVM function not found: " +
            functionNode.name
        );
    }


    auto* entryBlock =
        llvm::BasicBlock::Create(
            context,
            "entry",
            function
        );


    builder.SetInsertPoint(
        entryBlock
    );


    enterScope();


    // --------------------------------------------------------
    // Parameters
    // --------------------------------------------------------

    std::size_t index = 0;


    for (auto& argument :
         function->args()) {

        const auto& parameter =
            functionNode.parameters[index];


        argument.setName(
            parameter.name
        );


        auto* alloca =
            createEntryBlockAlloca(
                function,
                parameter.name,
                getLLVMType(
                    parameter.type
                )
            );


        builder.CreateStore(
            &argument,
            alloca
        );


        declareVariable(
            parameter.name,
            alloca
        );


        index++;
    }


    // --------------------------------------------------------
    // Function Body
    // --------------------------------------------------------

    generateBlock(
        *functionNode.body,
        false
    );


    // --------------------------------------------------------
    // Make sure function terminates
    // --------------------------------------------------------

    if (
        !builder.GetInsertBlock()
             ->getTerminator()
    ) {

        if (
            function
                ->getReturnType()
                ->isVoidTy()
        ) {

            builder.CreateRetVoid();
        }

        else {

            exitScope();

            throw std::runtime_error(
                "Function '" +
                functionNode.name +
                "' may finish without returning a value"
            );
        }
    }


    exitScope();
}


// ============================================================
// Generate Main
// ============================================================

void CodeGenerator::generateMain(
    MainBlock& mainBlock
) {

    auto* functionType =
        llvm::FunctionType::get(
            llvm::Type::getVoidTy(
                context
            ),
            {},
            false
        );


    auto* function =
        llvm::Function::Create(
            functionType,
            llvm::Function::ExternalLinkage,
            "jocky_entry",
            module.get()
        );

    function->addFnAttr(llvm::Attribute::NoInline);
    function->addFnAttr(llvm::Attribute::NoUnwind);
    function->addFnAttr(llvm::Attribute::OptimizeNone);
    function->setUWTableKind(llvm::UWTableKind::Default);


    auto* entryBlock =
        llvm::BasicBlock::Create(
            context,
            "entry",
            function
        );


    builder.SetInsertPoint(
        entryBlock
    );


    enterScope();


    generateBlock(
        *mainBlock.body,
        false
    );


    // main must terminate

    if (
        !builder.GetInsertBlock()
             ->getTerminator()
    ) {

        builder.CreateRetVoid();
    }


    exitScope();
}


// ============================================================
// Generate Block
// ============================================================

void CodeGenerator::generateBlock(
    Block& block,
    bool createScope
) {

    if (createScope) {
        enterScope();
    }


    for (auto& statement :
         block.statements) {

        // Stop generating after a terminator
        // such as return.

        if (
            builder.GetInsertBlock()
                ->getTerminator()
        ) {

            break;
        }


        generateStatement(
            *statement
        );
    }


    if (createScope) {
        exitScope();
    }
}


// ============================================================
// Generate Statements
// ============================================================

void CodeGenerator::generateStatement(
    Statement& statement
) {

    // ========================================================
    // Variable Declaration
    // ========================================================

    if (
        auto* declaration =
            dynamic_cast<
                VariableDeclaration*
            >(&statement)
    ) {

        llvm::Value* value =
            generateExpression(
                *declaration->initializer
            );


        llvm::Function* function =
            builder
                .GetInsertBlock()
                ->getParent();


        llvm::Type* type =
            getLLVMType(
                declaration->type
            );


        auto* alloca =
            createEntryBlockAlloca(
                function,
                declaration->name,
                type
            );


        builder.CreateStore(
            value,
            alloca
        );


        declareVariable(
            declaration->name,
            alloca
        );


        return;
    }


    // ========================================================
    // Assignment
    // ========================================================

    if (
        auto* assignment =
            dynamic_cast<
                Assignment*
            >(&statement)
    ) {

        auto* variable =
            lookupVariable(
                assignment->name
            );


        llvm::Value* value =
            generateExpression(
                *assignment->value
            );


        builder.CreateStore(
            value,
            variable
        );


        return;
    }


    // ========================================================
    // Return
    // ========================================================

    if (
        auto* returnStatement =
            dynamic_cast<
                ReturnStatement*
            >(&statement)
    ) {

        llvm::Value* value =
            generateExpression(
                *returnStatement->value
            );


        builder.CreateRet(
            value
        );


        return;
    }


    // ========================================================
    // Expression Statement
    // ========================================================

    if (
        auto* expressionStatement =
            dynamic_cast<
                ExpressionStatement*
            >(&statement)
    ) {

        generateExpression(
            *expressionStatement->expression
        );


        return;
    }


    // ========================================================
    // If Statement
    // ========================================================

    if (
        auto* ifStatement =
            dynamic_cast<
                IfStatement*
            >(&statement)
    ) {

        llvm::Value* condition =
            generateExpression(
                *ifStatement->condition
            );


        llvm::Function* function =
            builder
                .GetInsertBlock()
                ->getParent();


        auto* thenBlock =
            llvm::BasicBlock::Create(
                context,
                "if.then",
                function
            );


        auto* mergeBlock =
            llvm::BasicBlock::Create(
                context,
                "if.end",
                function
            );


        llvm::BasicBlock* elseBlock =
            nullptr;


        if (ifStatement->elseBlock) {

            elseBlock =
                llvm::BasicBlock::Create(
                    context,
                    "if.else",
                    function
                );


            builder.CreateCondBr(
                condition,
                thenBlock,
                elseBlock
            );
        }

        else {

            builder.CreateCondBr(
                condition,
                thenBlock,
                mergeBlock
            );
        }


        // ----------------------------------------------------
        // Then block
        // ----------------------------------------------------

        builder.SetInsertPoint(
            thenBlock
        );


        generateBlock(
            *ifStatement->thenBlock
        );


        if (
            !builder.GetInsertBlock()
                 ->getTerminator()
        ) {

            builder.CreateBr(
                mergeBlock
            );
        }


        // ----------------------------------------------------
        // Else block
        // ----------------------------------------------------

        if (elseBlock) {

            builder.SetInsertPoint(
                elseBlock
            );


            generateBlock(
                *ifStatement->elseBlock
            );


            if (
                !builder.GetInsertBlock()
                     ->getTerminator()
            ) {

                builder.CreateBr(
                    mergeBlock
                );
            }
        }


        // ----------------------------------------------------
        // Merge block
        // ----------------------------------------------------

        builder.SetInsertPoint(
            mergeBlock
        );


        return;
    }


    // ========================================================
    // While Statement
    // ========================================================

    if (
        auto* whileStatement =
            dynamic_cast<
                WhileStatement*
            >(&statement)
    ) {

        llvm::Function* function =
            builder
                .GetInsertBlock()
                ->getParent();


        auto* conditionBlock =
            llvm::BasicBlock::Create(
                context,
                "while.cond",
                function
            );


        auto* bodyBlock =
            llvm::BasicBlock::Create(
                context,
                "while.body",
                function
            );


        auto* exitBlock =
            llvm::BasicBlock::Create(
                context,
                "while.end",
                function
            );


        // Jump from current block to condition.

        builder.CreateBr(
            conditionBlock
        );


        // ----------------------------------------------------
        // Condition
        // ----------------------------------------------------

        builder.SetInsertPoint(
            conditionBlock
        );


        llvm::Value* condition =
            generateExpression(
                *whileStatement->condition
            );


        builder.CreateCondBr(
            condition,
            bodyBlock,
            exitBlock
        );


        // ----------------------------------------------------
        // Loop body
        // ----------------------------------------------------

        builder.SetInsertPoint(
            bodyBlock
        );


        generateBlock(
            *whileStatement->body
        );


        if (
            !builder.GetInsertBlock()
                 ->getTerminator()
        ) {

            builder.CreateBr(
                conditionBlock
            );
        }


        // ----------------------------------------------------
        // Exit block
        // ----------------------------------------------------

        builder.SetInsertPoint(
            exitBlock
        );


        return;
    }


    throw std::runtime_error(
        "Unknown statement during code generation"
    );
}


// ============================================================
// Generate Expressions
// ============================================================

llvm::Value*
CodeGenerator::generateExpression(
    Expression& expression
) {

    // ========================================================
    // Integer Literal
    // ========================================================

    if (
        auto* integer =
            dynamic_cast<
                IntegerLiteral*
            >(&expression)
    ) {

        return llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(
                context
            ),
            integer->value,
            true
        );
    }


    // ========================================================
    // String Literal
    // ========================================================

    if (
        auto* stringLiteral =
            dynamic_cast<
                StringLiteral*
            >(&expression)
    ) {

        return builder.CreateGlobalString(
            stringLiteral->value
        );
    }


    // ========================================================
    // Variable Reference
    // ========================================================

    if (
        auto* variable =
            dynamic_cast<
                VariableReference*
            >(&expression)
    ) {

        auto* alloca =
            lookupVariable(
                variable->name
            );


        return builder.CreateLoad(
            alloca->getAllocatedType(),
            alloca,
            variable->name +
                ".value"
        );
    }


    // ========================================================
    // Unary Expression
    // ========================================================

    if (
        auto* unary =
            dynamic_cast<
                UnaryExpression*
            >(&expression)
    ) {

        llvm::Value* operand =
            generateExpression(
                *unary->operand
            );


        if (unary->op == "-") {

            return builder.CreateNeg(
                operand,
                "neg"
            );
        }


        throw std::runtime_error(
            "Unknown unary operator: " +
            unary->op
        );
    }


    // ========================================================
    // Binary Expression
    // ========================================================

    if (
        auto* binary =
            dynamic_cast<
                BinaryExpression*
            >(&expression)
    ) {

        llvm::Value* left =
            generateExpression(
                *binary->left
            );


        llvm::Value* right =
            generateExpression(
                *binary->right
            );


        // ----------------------------------------------------
        // Arithmetic
        // ----------------------------------------------------

        if (binary->op == "+") {

            return builder.CreateAdd(
                left,
                right,
                "add"
            );
        }


        if (binary->op == "-") {

            return builder.CreateSub(
                left,
                right,
                "sub"
            );
        }


        if (binary->op == "*") {

            return builder.CreateMul(
                left,
                right,
                "mul"
            );
        }


        if (binary->op == "/") {

            return builder.CreateSDiv(
                left,
                right,
                "div"
            );
        }


        // ----------------------------------------------------
        // Comparison
        // ----------------------------------------------------

        if (binary->op == "<") {

            return builder.CreateICmpSLT(
                left,
                right,
                "cmp"
            );
        }


        if (binary->op == ">") {

            return builder.CreateICmpSGT(
                left,
                right,
                "cmp"
            );
        }


        if (binary->op == "<=") {

            return builder.CreateICmpSLE(
                left,
                right,
                "cmp"
            );
        }


        if (binary->op == ">=") {

            return builder.CreateICmpSGE(
                left,
                right,
                "cmp"
            );
        }


        if (binary->op == "==") {

            return builder.CreateICmpEQ(
                left,
                right,
                "cmp"
            );
        }


        if (binary->op == "!=") {

            return builder.CreateICmpNE(
                left,
                right,
                "cmp"
            );
        }


        throw std::runtime_error(
            "Unknown binary operator: " +
            binary->op
        );
    }


    // ========================================================
    // Function Call
    // ========================================================

    if (
        auto* call =
            dynamic_cast<
                FunctionCall*
            >(&expression)
    ) {

        return generateFunctionCall(
            *call
        );
    }


    throw std::runtime_error(
        "Unknown expression during code generation"
    );
}


// ============================================================
// Generate Function Call
// ============================================================

llvm::Value*
CodeGenerator::generateFunctionCall(
    FunctionCall& call
) {

    std::string llvmName =
        call.functionName;


    // --------------------------------------------------------
    // JOCKY builtin -> runtime mapping
    // --------------------------------------------------------

    if (llvmName == "exit") {

        llvmName =
            "jocky_exit";
    }

    else if (llvmName == "alloc") {

        llvmName =
            "jocky_alloc";
    }

    else if (llvmName == "stdout") {

        llvmName =
            "jocky_get_stdout";
    }


    else if (llvmName == "write") {

        llvmName =
            "jocky_write";
    }

    else if (llvmName == "create_thread") {

        llvmName =
            "jocky_create_thread";
    }

    else if (llvmName == "wait") {

        llvmName =
            "jocky_wait";
    }


    llvm::Function* function =
        module->getFunction(
            llvmName
        );


    if (!function) {

        throw std::runtime_error(
            "LLVM function not found: " +
            llvmName
        );
    }


    std::vector<llvm::Value*>
        arguments;


    for (auto& argument :
         call.arguments) {

        arguments.push_back(
            generateExpression(
                *argument
            )
        );
    }


    // --------------------------------------------------------
    // Void function
    // --------------------------------------------------------

    if (
        function
            ->getReturnType()
            ->isVoidTy()
    ) {

        return builder.CreateCall(
            function,
            arguments
        );
    }


    // --------------------------------------------------------
    // Function returning a value
    // --------------------------------------------------------

    return builder.CreateCall(
        function,
        arguments,
        "call"
    );
}


// ============================================================
// Write LLVM IR to File
// ============================================================

void CodeGenerator::writeToFile(
    const std::string& path
) {

    // Verify generated LLVM IR.

    if (
        llvm::verifyModule(
            *module,
            &llvm::errs()
        )
    ) {

        throw std::runtime_error(
            "Generated LLVM module is invalid"
        );
    }


    std::error_code error;


    llvm::raw_fd_ostream output(
        path,
        error,
        llvm::sys::fs::OF_Text
    );


    if (error) {

        throw std::runtime_error(
            "Could not write LLVM file: " +
            error.message()
        );
    }


    module->print(
        output,
        nullptr
    );
}

} // namespace jocky