#include "jocky/codegen/CodeGenerator.h"
#include "jocky/semantic/Types.h"

#include <stdexcept>
#include <system_error>
#include <unordered_map>
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

// Annotation name → Polaris pass string
static const std::unordered_map<std::string, std::string> g_attrPassMap = {
    {"obfuscate", "flatten,substitution"},
    {"heavy",     "flatten,substitution,boguscfg"},
    {"encrypt",   "gvenc"},
    {"full",      "flatten,linearmba,substitution,boguscfg,gvenc"},
    {"crypto",    "flatten,sub,mba,indcall"},
    // Legacy alias kept for old test files
    {"flatten",   "flatten"},
};

// JOCKY user-facing name → LLVM function name
static const std::unordered_map<std::string, std::string> g_builtinMap = {
    {"exit",          "jocky_exit"},
    {"alloc",         "jocky_alloc"},
    {"stdout",        "jocky_get_stdout"},
    {"write",         "jocky_write"},
    {"create_thread", "jocky_create_thread"},
    {"wait",          "jocky_wait"},

    {"alloc_ex",      "jocky_alloc_ex"},
    {"protect",       "jocky_protect"},
    {"protect_ex",    "jocky_protect_ex"},
    {"free_mem",      "jocky_free"},
    {"memcopy",       "jocky_memcopy"},
    {"memset_zero",   "jocky_memzero"},
    {"read_proc_mem", "jocky_read_proc"},
    {"write_proc_mem","jocky_write_proc"},

    {"create_proc",   "jocky_create_proc"},
    {"open_proc",     "jocky_open_proc"},
    {"terminate_proc","jocky_terminate_proc"},
    {"get_proc_id",   "jocky_get_pid"},
    {"get_thread_id", "jocky_get_tid"},
    {"get_tick_count","jocky_get_tick"},
    {"close_handle",  "jocky_close"},

    {"wait_all",      "jocky_wait_all"},
    {"suspend_thread","jocky_suspend"},
    {"resume_thread", "jocky_resume"},
    {"get_thread_ctx","jocky_get_ctx"},
    {"set_thread_ctx","jocky_set_ctx"},
    {"atomic_inc",    "jocky_atomic_inc"},
    {"atomic_dec",    "jocky_atomic_dec"},

    {"nt_unmap",      "jocky_nt_unmap"},
    {"nt_query_proc", "jocky_nt_query_proc"},

    {"file_open",     "jocky_file_open"},
    {"file_read",     "jocky_file_read"},
    {"file_size",     "jocky_file_size"},
    {"file_close",    "jocky_file_close"},
    {"file_load",     "jocky_file_load"},

    {"xor_buf",       "jocky_xor_buf"},
    {"aes_decrypt",   "jocky_aes_decrypt"},
    {"sha256",        "jocky_sha256"},

    {"get_proc_addr", "jocky_get_proc_addr"},
    {"get_module",    "jocky_get_module"},
    {"load_lib",      "jocky_load_lib"},
    {"get_last_err",  "jocky_get_last_err"},

    {"read_long",     "jocky_read_long"},

    // ptr+offset helpers
    {"read_int",      "jocky_read_int"},
    {"write_int",     "jocky_write_int"},
    {"read_byte",     "jocky_read_byte"},
    {"write_byte",    "jocky_write_byte"},

    // Short-name aliases
    {"get_tick",      "jocky_get_tick"},
    {"get_tid",       "jocky_get_tid"},
    {"memzero",       "jocky_memzero"},
};

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

    module->setTargetTriple("x86_64-pc-windows-msvc19.44.35225");
    module->setSourceFileName("jocky.jky");

    // llvm.ident metadata
    auto* identMD = module->getOrInsertNamedMetadata("llvm.ident");
    auto* identStr = llvm::MDString::get(context, "JOCKY Language Frontend v2.0");
    identMD->addOperand(llvm::MDNode::get(context, identStr));

    // Module flags
    module->addModuleFlag(llvm::Module::Error, "wchar_size", 2);
    module->addModuleFlag(llvm::Module::Max,   "uwtable",    2);
}


// ============================================================
// Type Mapping
// ============================================================

llvm::Type* CodeGenerator::getLLVMType(const std::string& type) {

    if (type == "int")    return llvm::Type::getInt32Ty(context);
    if (type == "long")   return llvm::Type::getInt64Ty(context);
    if (type == "byte")   return llvm::Type::getInt8Ty(context);
    if (type == "bool")   return llvm::Type::getInt1Ty(context);
    if (type == "void")   return llvm::Type::getVoidTy(context);

    if (type == "ptr"    ||
        type == "handle" ||
        type == "fnptr"  ||
        type == "string") {
        return llvm::PointerType::getUnqual(context);
    }

    // byte[N]
    if (type.rfind("byte[", 0) == 0) {
        int n = getArraySize(type);
        if (n <= 0) n = 1;
        return llvm::ArrayType::get(
            llvm::Type::getInt8Ty(context), (unsigned)n
        );
    }

    // int[N]  /  handle[N]
    if (type.rfind("int[", 0) == 0 || type.rfind("handle[", 0) == 0) {
        int n = getArraySize(type);
        if (n <= 0) n = 1;
        // handle[N] uses ptr-sized elements (i64 on x64)
        llvm::Type* elemTy = (type.rfind("handle[", 0) == 0)
            ? llvm::Type::getInt64Ty(context)
            : llvm::Type::getInt32Ty(context);
        return llvm::ArrayType::get(elemTy, (unsigned)n);
    }

    // Struct type — we don't create LLVM struct types; accessed via i8 ptr.
    // Return i8 as a dummy; the alloca will hold the struct bytes.
    auto it = structSizes_.find(type);
    if (it != structSizes_.end()) {
        return llvm::ArrayType::get(
            llvm::Type::getInt8Ty(context), (unsigned)it->second
        );
    }

    throw std::runtime_error("Unknown LLVM type: " + type);
}


// ============================================================
// Struct Sizes
// ============================================================

void CodeGenerator::collectStructSizes(Program& program) {
    static const std::unordered_map<std::string, int> typeBytes = {
        {"int", 4}, {"long", 8}, {"byte", 1}, {"bool", 1},
        {"ptr", 8}, {"handle", 8}, {"fnptr", 8}, {"string", 8},
    };

    for (auto& s : program.structs) {
        int total = 0;
        for (const auto& field : s->fields) {
            auto it = typeBytes.find(field.type);
            total += (it != typeBytes.end()) ? it->second : 8;
        }
        structSizes_[s->name] = total;
    }
}


// ============================================================
// Helper: create string constant with align 1
// ============================================================

llvm::Value* CodeGenerator::createStringConstant(const std::string& str) {
    auto* data = llvm::ConstantDataArray::getString(context, str, true);

    auto* gv = new llvm::GlobalVariable(
        *module,
        data->getType(),
        true,
        llvm::GlobalValue::PrivateLinkage,
        data,
        ".str." + std::to_string(strIdx_++)
    );

    gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    gv->setAlignment(llvm::Align(1));

    return gv;
}


// ============================================================
// Helper: coerce value to target type
// ============================================================

llvm::Value* CodeGenerator::coerceToType(
    llvm::Value* val,
    llvm::Type* targetTy
) {
    if (val->getType() == targetTy) return val;

    llvm::Type* srcTy = val->getType();

    // i32 → i64
    if (srcTy->isIntegerTy(32) && targetTy->isIntegerTy(64)) {
        return builder.CreateSExt(val, targetTy, "sext");
    }
    // i64 → i32
    if (srcTy->isIntegerTy(64) && targetTy->isIntegerTy(32)) {
        return builder.CreateTrunc(val, targetTy, "trunc");
    }
    // i32 → i8
    if (srcTy->isIntegerTy(32) && targetTy->isIntegerTy(8)) {
        return builder.CreateTrunc(val, targetTy, "trunc8");
    }
    // i8 → i32
    if (srcTy->isIntegerTy(8) && targetTy->isIntegerTy(32)) {
        return builder.CreateSExt(val, targetTy, "sext32");
    }
    // i8 → i64
    if (srcTy->isIntegerTy(8) && targetTy->isIntegerTy(64)) {
        return builder.CreateSExt(val, targetTy, "sext64");
    }
    // i1 → i32
    if (srcTy->isIntegerTy(1) && targetTy->isIntegerTy(32)) {
        return builder.CreateZExt(val, targetTy, "zext");
    }
    // integer → ptr
    if (srcTy->isIntegerTy() && targetTy->isPointerTy()) {
        return builder.CreateIntToPtr(val, targetTy, "i2p");
    }
    // ptr → integer
    if (srcTy->isPointerTy() && targetTy->isIntegerTy()) {
        return builder.CreatePtrToInt(val, targetTy, "p2i");
    }

    return val; // best effort
}


// ============================================================
// Runtime Function Declarations
// ============================================================

void CodeGenerator::declareRuntimeFunctions() {
    auto* voidTy = llvm::Type::getVoidTy(context);
    auto* i1Ty   = llvm::Type::getInt1Ty(context);
    auto* i8Ty   = llvm::Type::getInt8Ty(context);
    auto* i32Ty  = llvm::Type::getInt32Ty(context);
    auto* i64Ty  = llvm::Type::getInt64Ty(context);
    auto* ptrTy  = llvm::PointerType::getUnqual(context);

    auto declare = [&](const char* name,
                       llvm::Type* ret,
                       std::vector<llvm::Type*> params,
                       bool noUndef = true) {
        auto* ft = llvm::FunctionType::get(ret, params, false);
        auto* fn = llvm::Function::Create(
            ft, llvm::Function::ExternalLinkage, name, module.get()
        );
        if (noUndef) {
            for (std::size_t i = 0; i < params.size(); ++i) {
                fn->addParamAttr((unsigned)i, llvm::Attribute::NoUndef);
            }
        }
    };

    // v1
    declare("jocky_exit",          voidTy, {i32Ty});
    declare("jocky_alloc",         ptrTy,  {i32Ty});
    declare("jocky_get_stdout",    ptrTy,  {});
    declare("jocky_write",         voidTy, {ptrTy, ptrTy, i32Ty});
    declare("jocky_create_thread", ptrTy,  {ptrTy, ptrTy});
    declare("jocky_wait",          i32Ty,  {ptrTy, i32Ty});

    // v2 — memory
    declare("jocky_alloc_ex",   ptrTy,  {ptrTy, i32Ty, i32Ty, i32Ty});
    declare("jocky_protect",    i32Ty,  {ptrTy, i32Ty, i32Ty, ptrTy});
    declare("jocky_protect_ex", i32Ty,  {ptrTy, ptrTy, i32Ty, i32Ty, ptrTy});
    declare("jocky_free",       i32Ty,  {ptrTy, i32Ty});
    declare("jocky_memcopy",    voidTy, {ptrTy, ptrTy, i32Ty});
    declare("jocky_memzero",    voidTy, {ptrTy, i32Ty});
    declare("jocky_read_proc",  i32Ty,  {ptrTy, ptrTy, ptrTy, i32Ty});
    declare("jocky_write_proc", i32Ty,  {ptrTy, ptrTy, ptrTy, i32Ty});

    // v2 — process
    declare("jocky_create_proc",   ptrTy,  {ptrTy, i32Ty});
    declare("jocky_open_proc",     ptrTy,  {i32Ty, i32Ty});
    declare("jocky_terminate_proc",i32Ty,  {ptrTy, i32Ty});
    declare("jocky_get_pid",       i32Ty,  {});
    declare("jocky_get_tid",       i32Ty,  {});
    declare("jocky_get_tick",      i32Ty,  {});
    declare("jocky_close",         i32Ty,  {ptrTy});

    // v2 — thread
    declare("jocky_wait_all",   i32Ty,  {ptrTy, i32Ty, i32Ty});
    declare("jocky_suspend",    i32Ty,  {ptrTy});
    declare("jocky_resume",     i32Ty,  {ptrTy});
    declare("jocky_get_ctx",    i32Ty,  {ptrTy, ptrTy});
    declare("jocky_set_ctx",    i32Ty,  {ptrTy, ptrTy});
    declare("jocky_atomic_inc", i32Ty,  {ptrTy});
    declare("jocky_atomic_dec", i32Ty,  {ptrTy});

    // v2 — NT
    declare("jocky_nt_unmap",      i32Ty, {ptrTy, ptrTy});
    declare("jocky_nt_query_proc", i32Ty, {ptrTy, i32Ty, ptrTy, i32Ty});

    // v2 — file
    declare("jocky_file_open",  ptrTy,  {ptrTy, i32Ty, i32Ty, i32Ty});
    declare("jocky_file_read",  i32Ty,  {ptrTy, ptrTy, i32Ty});
    declare("jocky_file_size",  i32Ty,  {ptrTy});
    declare("jocky_file_close", voidTy, {ptrTy});
    declare("jocky_file_load",  ptrTy,  {ptrTy});

    // v2 — crypto
    declare("jocky_xor_buf",    voidTy, {ptrTy, i32Ty, ptrTy, i32Ty});
    declare("jocky_aes_decrypt",i32Ty,  {ptrTy, i32Ty, ptrTy, i32Ty});
    declare("jocky_sha256",     voidTy, {ptrTy, i32Ty, ptrTy});

    // v2 — utility
    declare("jocky_get_proc_addr", ptrTy,  {ptrTy, ptrTy});
    declare("jocky_get_module",    ptrTy,  {ptrTy});
    declare("jocky_load_lib",      ptrTy,  {ptrTy});
    declare("jocky_get_last_err",  i32Ty,  {});

    // read_long helper: read 8 bytes from ptr+offset
    declare("jocky_read_long", i64Ty, {ptrTy, i32Ty});

    // ptr+offset memory access
    declare("jocky_read_int",   i32Ty,  {ptrTy, i32Ty});
    declare("jocky_write_int",  voidTy, {ptrTy, i32Ty, i32Ty});
    declare("jocky_read_byte",  i8Ty,   {ptrTy, i32Ty});
    declare("jocky_write_byte", voidTy, {ptrTy, i32Ty, i8Ty});

    (void)i1Ty; // used in coerce only
}


// ============================================================
// Declare User Function Prototypes
// ============================================================

static void addFnAttrs(llvm::Function* fn) {
    fn->addFnAttr(llvm::Attribute::NoInline);
    fn->addFnAttr(llvm::Attribute::NoUnwind);
    fn->addFnAttr(llvm::Attribute::OptimizeNone);
    fn->setUWTableKind(llvm::UWTableKind::Default);
    fn->setDSOLocal(true);
}

void CodeGenerator::declareUserFunctions(Program& program) {
    for (auto& function : program.functions) {
        std::vector<llvm::Type*> paramTypes;

        for (const auto& p : function->parameters) {
            paramTypes.push_back(getLLVMType(p.type));
        }

        auto* retType     = getLLVMType(function->returnType);
        auto* functionType = llvm::FunctionType::get(retType, paramTypes, false);

        auto* fn = llvm::Function::Create(
            functionType,
            llvm::Function::ExternalLinkage,
            function->name,
            module.get()
        );

        for (auto& arg : fn->args()) {
            arg.addAttr(llvm::Attribute::NoUndef);
        }

        addFnAttrs(fn);

        // Collect pending annotations from attributes
        for (const auto& attr : function->attributes) {
            auto it = g_attrPassMap.find(attr);
            if (it != g_attrPassMap.end()) {
                pendingAnnotations_.emplace_back(fn, it->second);
            }
        }
    }
}


// ============================================================
// Scope Management
// ============================================================

void CodeGenerator::enterScope() {
    variableScopes.emplace_back();
}


void CodeGenerator::exitScope() {
    if (!variableScopes.empty()) variableScopes.pop_back();
}


void CodeGenerator::declareVariable(
    const std::string& name,
    llvm::Value* value
) {
    if (variableScopes.empty()) enterScope();
    variableScopes.back()[name] = value;
}


llvm::Value* CodeGenerator::lookupVariable(
    const std::string& name
) {
    for (auto scope = variableScopes.rbegin();
         scope != variableScopes.rend();
         ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) return found->second;
    }
    auto gIt = globalVars_.find(name);
    if (gIt != globalVars_.end()) return gIt->second;
    throw std::runtime_error(
        "Codegen: variable '" + name + "' not found"
    );
}


llvm::Value* CodeGenerator::tryLookupVariable(
    const std::string& name
) {
    for (auto scope = variableScopes.rbegin();
         scope != variableScopes.rend();
         ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) return found->second;
    }
    auto gIt = globalVars_.find(name);
    if (gIt != globalVars_.end()) return gIt->second;
    return nullptr;
}


llvm::Type* CodeGenerator::getStorageType(llvm::Value* storage) {
    if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(storage))
        return ai->getAllocatedType();
    if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(storage))
        return gv->getValueType();
    return llvm::Type::getInt8Ty(context);
}


// ============================================================
// Alloca Helper
// ============================================================

llvm::AllocaInst* CodeGenerator::createEntryBlockAlloca(
    llvm::Function* function,
    const std::string& name,
    llvm::Type* type
) {
    llvm::IRBuilder<> tmp(
        &function->getEntryBlock(),
        function->getEntryBlock().begin()
    );
    return tmp.CreateAlloca(type, nullptr, name);
}


// ============================================================
// Generate Whole Program
// ============================================================

void CodeGenerator::generateGlobals(Program& program) {
    for (auto& g : program.globals) {
        llvm::Type* ty = getLLVMType(g->type);
        llvm::Constant* init = llvm::Constant::getNullValue(ty);

        if (g->initializer) {
            auto* asInt  = dynamic_cast<IntegerLiteral*>(g->initializer.get());
            auto* asBool = dynamic_cast<BoolLiteral*>(g->initializer.get());
            if (asInt) {
                if (ty->isIntegerTy(32))
                    init = llvm::ConstantInt::get(ty, (uint64_t)(int32_t)asInt->value, true);
                else if (ty->isIntegerTy(64))
                    init = llvm::ConstantInt::get(ty, (uint64_t)asInt->value, true);
                else
                    init = llvm::ConstantInt::get(ty, (uint64_t)asInt->value, false);
            } else if (asBool) {
                init = llvm::ConstantInt::get(ty, asBool->value ? 1ULL : 0ULL, false);
            }
        }

        auto* gv = new llvm::GlobalVariable(
            *module, ty, false,
            llvm::GlobalValue::ExternalLinkage,
            init, g->name
        );
        gv->setDSOLocal(true);

        globalVars_[g->name] = gv;
    }
}


void CodeGenerator::generate(Program& program) {
    collectStructSizes(program);
    declareRuntimeFunctions();
    generateGlobals(program);
    declareUserFunctions(program);

    for (auto& fn : program.functions) {
        generateFunction(*fn);
    }

    if (program.mainBlock) {
        generateMain(*program.mainBlock);
    }

    flushAnnotations();
}


// ============================================================
// Generate Function
// ============================================================

void CodeGenerator::generateFunction(
    FunctionDeclaration& functionNode
) {
    llvm::Function* function = module->getFunction(functionNode.name);

    if (!function) {
        throw std::runtime_error(
            "LLVM function not found: " + functionNode.name
        );
    }

    auto* entryBlock = llvm::BasicBlock::Create(
        context, "entry", function
    );
    builder.SetInsertPoint(entryBlock);
    enterScope();

    std::size_t idx = 0;
    for (auto& arg : function->args()) {
        const auto& param = functionNode.parameters[idx];
        arg.setName(param.name);

        auto* alloca = createEntryBlockAlloca(
            function, param.name, getLLVMType(param.type)
        );
        builder.CreateStore(&arg, alloca);
        declareVariable(param.name, alloca);
        idx++;
    }

    generateBlock(*functionNode.body, false);

    if (!builder.GetInsertBlock()->getTerminator()) {
        if (function->getReturnType()->isVoidTy()) {
            builder.CreateRetVoid();
        } else {
            exitScope();
            throw std::runtime_error(
                "Function '" + functionNode.name +
                "' may finish without returning a value"
            );
        }
    }

    exitScope();
}


// ============================================================
// Generate Main
// ============================================================

void CodeGenerator::generateMain(MainBlock& mainBlock) {
    auto* ft = llvm::FunctionType::get(
        llvm::Type::getVoidTy(context), {}, false
    );

    auto* function = llvm::Function::Create(
        ft,
        llvm::Function::ExternalLinkage,
        "jocky_entry",
        module.get()
    );

    addFnAttrs(function);

    auto* entryBlock = llvm::BasicBlock::Create(context, "entry", function);
    builder.SetInsertPoint(entryBlock);
    enterScope();

    generateBlock(*mainBlock.body, false);

    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateRetVoid();
    }

    exitScope();
}


// ============================================================
// Generate Block
// ============================================================

void CodeGenerator::generateBlock(Block& block, bool createScope) {
    if (createScope) enterScope();

    for (auto& stmt : block.statements) {
        if (builder.GetInsertBlock()->getTerminator()) break;
        generateStatement(*stmt);
    }

    if (createScope) exitScope();
}


// ============================================================
// Generate Statements
// ============================================================

void CodeGenerator::generateStatement(Statement& statement) {

    // ----------------------------------------------------
    // Variable Declaration
    // ----------------------------------------------------
    if (auto* decl = dynamic_cast<VariableDeclaration*>(&statement)) {
        llvm::Function* fn = builder.GetInsertBlock()->getParent();
        llvm::Type* ty = getLLVMType(decl->type);

        auto* alloca = createEntryBlockAlloca(fn, decl->name, ty);

        if (decl->initializer) {
            llvm::Value* val = generateExpression(*decl->initializer);
            val = coerceToType(val, ty);
            if (decl->isVolatile) {
                auto* si = builder.CreateStore(val, alloca);
                si->setVolatile(true);
            } else {
                builder.CreateStore(val, alloca);
            }
        } else {
            // Zero-initialise array/struct allocas
            builder.CreateStore(llvm::Constant::getNullValue(ty), alloca);
        }

        declareVariable(decl->name, alloca);
        return;
    }

    // ----------------------------------------------------
    // Assignment
    // ----------------------------------------------------
    if (auto* assign = dynamic_cast<Assignment*>(&statement)) {
        auto* storage = lookupVariable(assign->name);
        llvm::Value* val = generateExpression(*assign->value);
        val = coerceToType(val, getStorageType(storage));
        builder.CreateStore(val, storage);
        return;
    }

    // ----------------------------------------------------
    // Array Assignment:  arr[i] = val
    // ----------------------------------------------------
    if (auto* aa = dynamic_cast<ArrayAssignment*>(&statement)) {
        auto* storage = lookupVariable(aa->name);
        llvm::Value* idx = generateExpression(*aa->index);
        llvm::Value* val = generateExpression(*aa->value);

        llvm::Type* arrTy = getStorageType(storage);
        llvm::Value* elemPtr;

        if (arrTy->isArrayTy()) {
            auto* zero = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(context), 0
            );
            llvm::Value* gep_idxs[] = {zero, idx};
            elemPtr = builder.CreateGEP(arrTy, storage, gep_idxs, "elem");
        } else {
            // ptr arithmetic
            elemPtr = builder.CreateGEP(
                llvm::Type::getInt8Ty(context),
                storage, idx, "elem"
            );
        }

        // coerce val to element type
        llvm::Type* elemTy = arrTy->isArrayTy()
            ? arrTy->getArrayElementType()
            : llvm::Type::getInt8Ty(context);

        val = coerceToType(val, elemTy);
        builder.CreateStore(val, elemPtr);
        return;
    }

    // ----------------------------------------------------
    // Return
    // ----------------------------------------------------
    if (auto* ret = dynamic_cast<ReturnStatement*>(&statement)) {
        if (!ret->value) {
            builder.CreateRetVoid();
        } else {
            llvm::Value* val = generateExpression(*ret->value);
            llvm::Type* retTy =
                builder.GetInsertBlock()->getParent()->getReturnType();
            val = coerceToType(val, retTy);
            builder.CreateRet(val);
        }
        return;
    }

    // ----------------------------------------------------
    // Expression Statement
    // ----------------------------------------------------
    if (auto* es = dynamic_cast<ExpressionStatement*>(&statement)) {
        generateExpression(*es->expression);
        return;
    }

    // ----------------------------------------------------
    // If Statement
    // ----------------------------------------------------
    if (auto* ifStmt = dynamic_cast<IfStatement*>(&statement)) {
        llvm::Value* cond = generateExpression(*ifStmt->condition);
        llvm::Function* fn = builder.GetInsertBlock()->getParent();

        auto* thenBlock  = llvm::BasicBlock::Create(context, "if.then",  fn);
        auto* mergeBlock = llvm::BasicBlock::Create(context, "if.end",   fn);
        llvm::BasicBlock* elseBlock = nullptr;

        if (ifStmt->elseBlock) {
            elseBlock = llvm::BasicBlock::Create(context, "if.else", fn);
            builder.CreateCondBr(cond, thenBlock, elseBlock);
        } else {
            builder.CreateCondBr(cond, thenBlock, mergeBlock);
        }

        builder.SetInsertPoint(thenBlock);
        generateBlock(*ifStmt->thenBlock);
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(mergeBlock);

        if (elseBlock) {
            builder.SetInsertPoint(elseBlock);
            generateBlock(*ifStmt->elseBlock);
            if (!builder.GetInsertBlock()->getTerminator())
                builder.CreateBr(mergeBlock);
        }

        builder.SetInsertPoint(mergeBlock);
        return;
    }

    // ----------------------------------------------------
    // While Statement
    // ----------------------------------------------------
    if (auto* ws = dynamic_cast<WhileStatement*>(&statement)) {
        llvm::Function* fn = builder.GetInsertBlock()->getParent();

        auto* condBlock = llvm::BasicBlock::Create(context, "while.cond", fn);
        auto* bodyBlock = llvm::BasicBlock::Create(context, "while.body", fn);
        auto* exitBlock = llvm::BasicBlock::Create(context, "while.end",  fn);

        builder.CreateBr(condBlock);
        builder.SetInsertPoint(condBlock);

        llvm::Value* cond = generateExpression(*ws->condition);
        builder.CreateCondBr(cond, bodyBlock, exitBlock);

        builder.SetInsertPoint(bodyBlock);
        loopStack_.push_back({condBlock, exitBlock});
        generateBlock(*ws->body);
        loopStack_.pop_back();

        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(condBlock);

        builder.SetInsertPoint(exitBlock);
        return;
    }

    // ----------------------------------------------------
    // For Statement
    // ----------------------------------------------------
    if (auto* fs = dynamic_cast<ForStatement*>(&statement)) {
        llvm::Function* fn = builder.GetInsertBlock()->getParent();

        enterScope();

        // Init
        llvm::Type* initTy = getLLVMType(fs->initType);
        auto* initAlloca   = createEntryBlockAlloca(fn, fs->initName, initTy);
        llvm::Value* initVal = generateExpression(*fs->initExpr);
        initVal = coerceToType(initVal, initTy);
        builder.CreateStore(initVal, initAlloca);
        declareVariable(fs->initName, initAlloca);

        auto* condBlock = llvm::BasicBlock::Create(context, "for.cond", fn);
        auto* bodyBlock = llvm::BasicBlock::Create(context, "for.body", fn);
        auto* incBlock  = llvm::BasicBlock::Create(context, "for.inc",  fn);
        auto* exitBlock = llvm::BasicBlock::Create(context, "for.end",  fn);

        builder.CreateBr(condBlock);
        builder.SetInsertPoint(condBlock);

        llvm::Value* cond = generateExpression(*fs->condition);
        builder.CreateCondBr(cond, bodyBlock, exitBlock);

        builder.SetInsertPoint(bodyBlock);
        loopStack_.push_back({incBlock, exitBlock});
        generateBlock(*fs->body, false); // scope already opened
        loopStack_.pop_back();

        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(incBlock);

        // Increment
        builder.SetInsertPoint(incBlock);
        auto* incAlloca = lookupVariable(fs->incName);
        llvm::Value* incVal = generateExpression(*fs->incExpr);
        incVal = coerceToType(incVal, getStorageType(incAlloca));
        builder.CreateStore(incVal, incAlloca);
        builder.CreateBr(condBlock);

        builder.SetInsertPoint(exitBlock);
        exitScope();
        return;
    }

    // ----------------------------------------------------
    // Break
    // ----------------------------------------------------
    if (dynamic_cast<BreakStatement*>(&statement)) {
        if (loopStack_.empty()) {
            throw std::runtime_error("'break' outside of loop");
        }
        builder.CreateBr(loopStack_.back().breakTarget);
        // Insert unreachable block so IRBuilder stays valid
        llvm::Function* fn = builder.GetInsertBlock()->getParent();
        auto* dead = llvm::BasicBlock::Create(context, "break.dead", fn);
        builder.SetInsertPoint(dead);
        return;
    }

    // ----------------------------------------------------
    // Continue
    // ----------------------------------------------------
    if (dynamic_cast<ContinueStatement*>(&statement)) {
        if (loopStack_.empty()) {
            throw std::runtime_error("'continue' outside of loop");
        }
        builder.CreateBr(loopStack_.back().continueTarget);
        llvm::Function* fn = builder.GetInsertBlock()->getParent();
        auto* dead = llvm::BasicBlock::Create(context, "cont.dead", fn);
        builder.SetInsertPoint(dead);
        return;
    }

    throw std::runtime_error("Unknown statement during code generation");
}


// ============================================================
// Generate Expressions
// ============================================================

llvm::Value* CodeGenerator::generateExpression(Expression& expression) {

    // --------------------------------------------------
    // Integer Literal
    // --------------------------------------------------
    if (auto* lit = dynamic_cast<IntegerLiteral*>(&expression)) {
        // Emit i32 by default; coerceToType handles widening
        return llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context),
            (uint64_t)(int32_t)lit->value,
            true
        );
    }

    // --------------------------------------------------
    // Bool Literal
    // --------------------------------------------------
    if (auto* lit = dynamic_cast<BoolLiteral*>(&expression)) {
        return llvm::ConstantInt::get(
            llvm::Type::getInt1Ty(context),
            lit->value ? 1 : 0
        );
    }

    // --------------------------------------------------
    // Null Literal
    // --------------------------------------------------
    if (dynamic_cast<NullLiteral*>(&expression)) {
        return llvm::Constant::getNullValue(
            llvm::PointerType::getUnqual(context)
        );
    }

    // --------------------------------------------------
    // String Literal
    // --------------------------------------------------
    if (auto* lit = dynamic_cast<StringLiteral*>(&expression)) {
        return createStringConstant(lit->value);
    }

    // --------------------------------------------------
    // sizeof()
    // --------------------------------------------------
    if (auto* se = dynamic_cast<SizeofExpression*>(&expression)) {
        int size = 0;
        const std::string& tn = se->typeName;

        if (tn == "int")    size = 4;
        else if (tn == "long")   size = 8;
        else if (tn == "byte")   size = 1;
        else if (tn == "bool")   size = 1;
        else if (tn == "ptr" || tn == "handle" || tn == "fnptr") size = 8;
        else if (tn == "string") size = 8;
        else {
            // struct?
            auto it = structSizes_.find(tn);
            if (it != structSizes_.end()) size = it->second;
            else size = 8; // default
        }

        return llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context), (uint64_t)size
        );
    }

    // --------------------------------------------------
    // cast(expr, type)
    // --------------------------------------------------
    if (auto* ce = dynamic_cast<CastExpression*>(&expression)) {
        llvm::Type* target = getLLVMType(ce->targetType);

        // cast(arrayOrStructVar, ptr) → return address of the aggregate directly
        if (target->isPointerTy()) {
            if (auto* vr = dynamic_cast<VariableReference*>(ce->expr.get())) {
                auto* storage = tryLookupVariable(vr->name);
                if (storage) {
                    llvm::Type* st = getStorageType(storage);
                    if (st->isArrayTy() || st->isStructTy()) {
                        return storage;
                    }
                }
            }
        }

        llvm::Value* val = generateExpression(*ce->expr);
        llvm::Type* srcTy = val->getType();

        if (srcTy == target) return val;

        if (srcTy->isIntegerTy() && target->isPointerTy()) {
            return builder.CreateIntToPtr(val, target, "cast.i2p");
        }
        if (srcTy->isPointerTy() && target->isIntegerTy()) {
            return builder.CreatePtrToInt(val, target, "cast.p2i");
        }
        if (srcTy->isPointerTy() && target->isPointerTy()) {
            return val; // opaque ptr — no-op
        }
        return coerceToType(val, target);
    }

    // --------------------------------------------------
    // Ternary
    // --------------------------------------------------
    if (auto* te = dynamic_cast<TernaryExpression*>(&expression)) {
        llvm::Value* cond  = generateExpression(*te->condition);
        llvm::Value* then_ = generateExpression(*te->thenExpr);
        llvm::Value* else_ = generateExpression(*te->elseExpr);
        else_ = coerceToType(else_, then_->getType());
        return builder.CreateSelect(cond, then_, else_, "ternary");
    }

    // --------------------------------------------------
    // Array Index Expression
    // --------------------------------------------------
    if (auto* ai = dynamic_cast<ArrayIndexExpression*>(&expression)) {
        auto* storage = lookupVariable(ai->name);
        llvm::Value* idx = generateExpression(*ai->index);
        llvm::Type* arrTy = getStorageType(storage);

        llvm::Value* elemPtr;
        llvm::Type* elemTy;

        if (arrTy->isArrayTy()) {
            elemTy = arrTy->getArrayElementType();
            auto* zero = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(context), 0
            );
            llvm::Value* idxs[] = {zero, idx};
            elemPtr = builder.CreateGEP(arrTy, storage, idxs, "elem.ptr");
        } else {
            // ptr arithmetic on non-array storage (ptr type var)
            elemTy = llvm::Type::getInt8Ty(context);
            llvm::Value* base = builder.CreateLoad(arrTy, storage, "base");
            elemPtr = builder.CreateGEP(
                llvm::Type::getInt8Ty(context), base, idx, "elem.ptr"
            );
        }

        return builder.CreateLoad(elemTy, elemPtr, ai->name + ".elem");
    }

    // --------------------------------------------------
    // Variable Reference
    // --------------------------------------------------
    if (auto* vr = dynamic_cast<VariableReference*>(&expression)) {
        // Check local variables and globals
        auto* storage = tryLookupVariable(vr->name);
        if (storage) {
            return builder.CreateLoad(
                getStorageType(storage), storage, vr->name + ".val"
            );
        }

        // Check if it's a function reference (used as ptr)
        auto* fn = module->getFunction(vr->name);
        if (fn) return fn;

        throw std::runtime_error(
            "Codegen: variable '" + vr->name + "' not found"
        );
    }

    // --------------------------------------------------
    // Unary Expression
    // --------------------------------------------------
    if (auto* ue = dynamic_cast<UnaryExpression*>(&expression)) {
        llvm::Value* operand = generateExpression(*ue->operand);

        if (ue->op == "-") return builder.CreateNeg(operand, "neg");
        if (ue->op == "!") return builder.CreateNot(operand, "not");
        if (ue->op == "~") return builder.CreateNot(operand, "bnot");

        throw std::runtime_error("Unknown unary op: " + ue->op);
    }

    // --------------------------------------------------
    // Binary Expression
    // --------------------------------------------------
    if (auto* be = dynamic_cast<BinaryExpression*>(&expression)) {
        llvm::Value* left  = generateExpression(*be->left);
        llvm::Value* right = generateExpression(*be->right);

        // ptr + int → GEP
        if (be->op == "+" &&
            left->getType()->isPointerTy() &&
            right->getType()->isIntegerTy()) {
            return builder.CreateGEP(
                llvm::Type::getInt8Ty(context),
                left, right, "ptradd"
            );
        }

        // Promote both sides to same integer width
        if (left->getType()->isIntegerTy() &&
            right->getType()->isIntegerTy()) {
            auto* lty = left->getType();
            auto* rty = right->getType();
            if (lty != rty) {
                // Widen the narrower one
                unsigned lw = lty->getIntegerBitWidth();
                unsigned rw = rty->getIntegerBitWidth();
                if (lw < rw) left  = builder.CreateSExt(left,  rty, "widen");
                else         right = builder.CreateSExt(right, lty, "widen");
            }
        }

        if (be->op == "+")  return builder.CreateAdd(left, right, "add");
        if (be->op == "-")  return builder.CreateSub(left, right, "sub");
        if (be->op == "*")  return builder.CreateMul(left, right, "mul");
        if (be->op == "/")  return builder.CreateSDiv(left, right, "div");

        if (be->op == "&")  return builder.CreateAnd(left, right, "band");
        if (be->op == "|")  return builder.CreateOr(left,  right, "bor");
        if (be->op == "^")  return builder.CreateXor(left, right, "bxor");
        if (be->op == "<<") return builder.CreateShl(left, right, "shl");
        if (be->op == ">>") return builder.CreateLShr(left, right, "shr");

        if (be->op == "&&") return builder.CreateAnd(left, right, "land");
        if (be->op == "||") return builder.CreateOr(left,  right, "lor");

        if (be->op == "<")  return builder.CreateICmpSLT(left, right, "cmp");
        if (be->op == ">")  return builder.CreateICmpSGT(left, right, "cmp");
        if (be->op == "<=") return builder.CreateICmpSLE(left, right, "cmp");
        if (be->op == ">=") return builder.CreateICmpSGE(left, right, "cmp");
        if (be->op == "==") return builder.CreateICmpEQ(left,  right, "cmp");
        if (be->op == "!=") return builder.CreateICmpNE(left,  right, "cmp");

        throw std::runtime_error("Unknown binary op: " + be->op);
    }

    // --------------------------------------------------
    // Function Call
    // --------------------------------------------------
    if (auto* call = dynamic_cast<FunctionCall*>(&expression)) {
        return generateFunctionCall(*call);
    }

    throw std::runtime_error("Unknown expression during code generation");
}


// ============================================================
// Generate Function Call
// ============================================================

llvm::Value* CodeGenerator::generateFunctionCall(FunctionCall& call) {

    // Special: call(fn_ptr)  — indirect void() call
    if (call.functionName == "call") {
        llvm::Value* fn = generateExpression(*call.arguments[0]);
        auto* ft = llvm::FunctionType::get(
            llvm::Type::getVoidTy(context), {}, false
        );
        return builder.CreateCall(ft, fn);
    }

    // Special: call_with(fn, arg1, ...)  — indirect call with args
    if (call.functionName == "call_with") {
        llvm::Value* fn = generateExpression(*call.arguments[0]);
        std::vector<llvm::Value*> args;
        std::vector<llvm::Type*>  argTys;
        for (std::size_t i = 1; i < call.arguments.size(); ++i) {
            auto* v = generateExpression(*call.arguments[i]);
            args.push_back(v);
            argTys.push_back(v->getType());
        }
        auto* ft = llvm::FunctionType::get(
            llvm::Type::getVoidTy(context), argTys, false
        );
        return builder.CreateCall(ft, fn, args);
    }

    // Special: atomic_inc / atomic_dec — pass alloca address, not loaded value
    if (call.functionName == "atomic_inc" ||
        call.functionName == "atomic_dec") {
        std::string llvmName = (call.functionName == "atomic_inc")
            ? "jocky_atomic_inc" : "jocky_atomic_dec";

        llvm::Function* fn = module->getFunction(llvmName);
        if (!fn) throw std::runtime_error("Runtime not declared: " + llvmName);

        // Get address of the variable (not its loaded value).
        // Handles both bare VariableReference and cast(varName, ptr).
        llvm::Value* addr;
        auto* arg0 = call.arguments[0].get();
        if (auto* vr = dynamic_cast<VariableReference*>(arg0)) {
            addr = lookupVariable(vr->name);
        } else if (auto* ce = dynamic_cast<CastExpression*>(arg0)) {
            if (auto* vr2 = dynamic_cast<VariableReference*>(ce->expr.get())) {
                addr = lookupVariable(vr2->name);
            } else {
                addr = generateExpression(*arg0);
            }
        } else {
            addr = generateExpression(*arg0);
        }

        return builder.CreateCall(fn, {addr}, "atomic");
    }

    // Map JOCKY name → LLVM function name
    std::string llvmName = call.functionName;

    auto it = g_builtinMap.find(call.functionName);
    if (it != g_builtinMap.end()) {
        llvmName = it->second;
    }

    llvm::Function* fn = module->getFunction(llvmName);
    if (!fn) {
        throw std::runtime_error(
            "LLVM function not found: " + llvmName
        );
    }

    std::vector<llvm::Value*> args;
    unsigned paramIdx = 0;

    for (auto& argExpr : call.arguments) {
        llvm::Value* val = generateExpression(*argExpr);

        // Coerce to the declared parameter type if known
        if (paramIdx < fn->arg_size()) {
            llvm::Type* paramTy =
                fn->getArg(paramIdx)->getType();
            val = coerceToType(val, paramTy);
        }

        args.push_back(val);
        paramIdx++;
    }

    if (fn->getReturnType()->isVoidTy()) {
        return builder.CreateCall(fn, args);
    }

    return builder.CreateCall(fn, args, "call");
}


// ============================================================
// Flush Annotations (llvm.global.annotations)
// ============================================================

void CodeGenerator::flushAnnotations() {
    if (pendingAnnotations_.empty()) return;

    llvm::LLVMContext& ctx = module->getContext();
    auto* ptrTy = llvm::PointerType::getUnqual(ctx);
    auto* i32Ty = llvm::Type::getInt32Ty(ctx);

    // Shared source file name global
    auto* fileStr = llvm::ConstantDataArray::getString(
        ctx, "jocky.jky", true
    );
    auto* fileGV = new llvm::GlobalVariable(
        *module,
        fileStr->getType(),
        true,
        llvm::GlobalValue::PrivateLinkage,
        fileStr,
        ".jocky.src"
    );
    fileGV->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    fileGV->setAlignment(llvm::Align(1));
    fileGV->setSection("llvm.metadata");

    auto* entryTy = llvm::StructType::get(
        ptrTy, ptrTy, ptrTy, i32Ty, ptrTy
    );

    std::vector<llvm::Constant*> entries;
    int idx = 0;

    for (auto& [fn, passStr] : pendingAnnotations_) {
        auto* annotData = llvm::ConstantDataArray::getString(
            ctx, passStr, true
        );
        auto* annotGV = new llvm::GlobalVariable(
            *module,
            annotData->getType(),
            true,
            llvm::GlobalValue::PrivateLinkage,
            annotData,
            ".jocky.attr." + std::to_string(idx++)
        );
        annotGV->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
        annotGV->setAlignment(llvm::Align(1));
        annotGV->setSection("llvm.metadata");

        llvm::Constant* fields[] = {
            fn,
            annotGV,
            fileGV,
            llvm::ConstantInt::get(i32Ty, 0),
            llvm::Constant::getNullValue(ptrTy)
        };

        entries.push_back(
            llvm::ConstantStruct::get(entryTy, fields)
        );
    }

    auto* arrTy = llvm::ArrayType::get(entryTy, entries.size());
    auto* arr   = llvm::ConstantArray::get(arrTy, entries);

    auto* globAnnot = new llvm::GlobalVariable(
        *module,
        arrTy,
        false,
        llvm::GlobalValue::AppendingLinkage,
        arr,
        "llvm.global.annotations"
    );
    globAnnot->setSection("llvm.metadata");
}


// ============================================================
// Write LLVM IR to File
// ============================================================

void CodeGenerator::writeToFile(const std::string& path) {
    if (llvm::verifyModule(*module, &llvm::errs())) {
        throw std::runtime_error("Generated LLVM module is invalid");
    }

    std::error_code error;
    llvm::raw_fd_ostream output(path, error, llvm::sys::fs::OF_Text);

    if (error) {
        throw std::runtime_error(
            "Could not write LLVM file: " + error.message()
        );
    }

    module->print(output, nullptr);
}

} // namespace jocky
