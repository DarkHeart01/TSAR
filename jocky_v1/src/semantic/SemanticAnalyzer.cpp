#include "jocky/semantic/SemanticAnalyzer.h"

#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace jocky {

// ============================================================
// Constructor
// ============================================================

SemanticAnalyzer::SemanticAnalyzer()
    : currentReturnType(JockyType::VOID),
      loopDepth_(0) {
    registerBuiltins();
}


// ============================================================
// Type Compatibility
// ============================================================

bool SemanticAnalyzer::typesCompatible(
    JockyType declared,
    JockyType init,
    const std::string& /*declaredStr*/
) {
    if (declared == init) return true;

    // int literal → long or byte (implicit narrowing/widening)
    if (init == JockyType::INT &&
        (declared == JockyType::LONG || declared == JockyType::BYTE)) {
        return true;
    }

    // null (PTR) → any pointer-like declared type
    if (init == JockyType::PTR &&
        (declared == JockyType::PTR || declared == JockyType::STRING)) {
        return true;
    }

    // string literal → ptr
    if (init == JockyType::STRING && declared == JockyType::PTR) {
        return true;
    }

    // int ↔ bool (comparisons produce bool, used in conditions)
    // No implicit conversion — comparisons must be explicit.

    return false;
}


// ============================================================
// Built-in Functions
// ============================================================

void SemanticAnalyzer::registerBuiltins() {
    using T = JockyType;

    // v1 retained
    symbols.declareFunction("stdout",        {},                              T::PTR,  true);
    symbols.declareFunction("exit",          {T::INT},                       T::VOID, true);
    symbols.declareFunction("alloc",         {T::INT},                       T::PTR,  true);
    symbols.declareFunction("write",         {T::PTR, T::PTR, T::INT},       T::VOID, true);
    symbols.declareFunction("create_thread", {T::PTR, T::PTR},               T::PTR,  true);
    symbols.declareFunction("wait",          {T::PTR, T::INT},               T::INT,  true);

    // v2 — memory
    symbols.declareFunction("alloc_ex",      {T::PTR, T::INT, T::INT, T::INT}, T::PTR,  true);
    symbols.declareFunction("protect",       {T::PTR, T::INT, T::INT, T::PTR}, T::BOOL, true);
    symbols.declareFunction("protect_ex",    {T::PTR, T::PTR, T::INT, T::INT, T::PTR}, T::BOOL, true);
    symbols.declareFunction("free_mem",      {T::PTR, T::INT},               T::BOOL, true);
    symbols.declareFunction("memcopy",       {T::PTR, T::PTR, T::INT},       T::VOID, true);
    symbols.declareFunction("memset_zero",   {T::PTR, T::INT},               T::VOID, true);
    symbols.declareFunction("read_proc_mem", {T::PTR, T::PTR, T::PTR, T::INT}, T::BOOL, true);
    symbols.declareFunction("write_proc_mem",{T::PTR, T::PTR, T::PTR, T::INT}, T::BOOL, true);

    // v2 — process
    symbols.declareFunction("create_proc",   {T::PTR, T::INT},               T::PTR,  true);
    symbols.declareFunction("open_proc",     {T::INT, T::INT},               T::PTR,  true);
    symbols.declareFunction("terminate_proc",{T::PTR, T::INT},               T::BOOL, true);
    symbols.declareFunction("get_proc_id",   {},                             T::INT,  true);
    symbols.declareFunction("get_thread_id", {},                             T::INT,  true);
    symbols.declareFunction("get_tick_count",{},                             T::INT,  true);
    symbols.declareFunction("close_handle",  {T::PTR},                      T::BOOL, true);

    // v2 — thread
    symbols.declareFunction("wait_all",      {T::PTR, T::INT, T::INT},       T::INT,  true);
    symbols.declareFunction("suspend_thread",{T::PTR},                       T::INT,  true);
    symbols.declareFunction("resume_thread", {T::PTR},                       T::INT,  true);
    symbols.declareFunction("get_thread_ctx",{T::PTR, T::PTR},               T::BOOL, true);
    symbols.declareFunction("set_thread_ctx",{T::PTR, T::PTR},               T::BOOL, true);
    symbols.declareFunction("atomic_inc",    {T::PTR},                       T::INT,  true);
    symbols.declareFunction("atomic_dec",    {T::PTR},                       T::INT,  true);

    // v2 — NT
    symbols.declareFunction("nt_unmap",      {T::PTR, T::PTR},               T::INT,  true);
    symbols.declareFunction("nt_query_proc", {T::PTR, T::INT, T::PTR, T::INT}, T::INT, true);

    // v2 — file
    symbols.declareFunction("file_open",     {T::PTR, T::INT, T::INT, T::INT}, T::PTR, true);
    symbols.declareFunction("file_read",     {T::PTR, T::PTR, T::INT},       T::INT,  true);
    symbols.declareFunction("file_size",     {T::PTR},                       T::INT,  true);
    symbols.declareFunction("file_close",    {T::PTR},                       T::VOID, true);
    symbols.declareFunction("file_load",     {T::PTR},                       T::PTR,  true);

    // v2 — crypto
    symbols.declareFunction("xor_buf",       {T::PTR, T::INT, T::PTR, T::INT}, T::VOID, true);
    symbols.declareFunction("aes_decrypt",   {T::PTR, T::INT, T::PTR, T::INT}, T::INT,  true);
    symbols.declareFunction("sha256",        {T::PTR, T::INT, T::PTR},        T::VOID, true);

    // v2 — utility
    symbols.declareFunction("get_proc_addr", {T::PTR, T::PTR},               T::PTR,  true);
    symbols.declareFunction("get_module",    {T::PTR},                       T::PTR,  true);
    symbols.declareFunction("load_lib",      {T::PTR},                       T::PTR,  true);
    symbols.declareFunction("get_last_err",  {},                             T::INT,  true);

    // indirect call helpers
    symbols.declareFunction("call",          {T::PTR},                       T::VOID, true);
    symbols.declareFunction("call_with",     {T::PTR},                       T::VOID, true); // variadic-ish

    // read_long helper — read 8 bytes (long) from ptr+offset
    symbols.declareFunction("read_long",     {T::PTR, T::INT},               T::LONG, true);

    // ptr+offset memory access helpers
    symbols.declareFunction("read_int",      {T::PTR, T::INT},               T::INT,  true);
    symbols.declareFunction("write_int",     {T::PTR, T::INT, T::INT},       T::VOID, true);
    symbols.declareFunction("read_byte",     {T::PTR, T::INT},               T::BYTE, true);
    symbols.declareFunction("write_byte",    {T::PTR, T::INT, T::BYTE},      T::VOID, true);

    // Short-name aliases used in user code
    symbols.declareFunction("get_tick",      {},                             T::INT,  true);
    symbols.declareFunction("get_tid",       {},                             T::INT,  true);
    symbols.declareFunction("memzero",       {T::PTR, T::INT},               T::VOID, true);
}


// ============================================================
// Analyze Whole Program
// ============================================================

void SemanticAnalyzer::registerGlobals(Program& program) {
    for (auto& g : program.globals) {
        JockyType t = stringToType(g->type);
        if (t == JockyType::UNKNOWN) t = JockyType::PTR;
        symbols.declareVariable(g->name, t);
    }
}


void SemanticAnalyzer::analyze(Program& program) {
    registerStructs(program);
    registerGlobals(program);
    registerFunctions(program);

    for (auto& function : program.functions) {
        analyzeFunction(*function);
    }

    if (program.mainBlock) {
        analyzeMain(*program.mainBlock);
    }
}


// ============================================================
// Register Structs
// ============================================================

void SemanticAnalyzer::registerStructs(Program& program) {
    static const std::unordered_map<std::string, int> typeBytes = {
        {"int", 4}, {"long", 8}, {"byte", 1}, {"bool", 1},
        {"ptr", 8}, {"handle", 8}, {"fnptr", 8}, {"string", 8},
    };

    for (auto& s : program.structs) {
        int total = 0;
        for (const auto& field : s->fields) {
            auto it = typeBytes.find(field.type);
            if (it != typeBytes.end()) {
                total += it->second;
            } else {
                total += 8; // unknown → assume pointer-sized
            }
        }
        structSizes_[s->name] = total;
    }
}


// ============================================================
// Register User Functions
// ============================================================

void SemanticAnalyzer::registerFunctions(Program& program) {
    for (auto& function : program.functions) {
        std::vector<JockyType> paramTypes;

        for (const auto& p : function->parameters) {
            JockyType t = stringToType(p.type);
            if (t == JockyType::UNKNOWN) {
                // struct param → treat as PTR
                t = JockyType::PTR;
            }
            paramTypes.push_back(t);
        }

        JockyType retType = stringToType(function->returnType);
        if (retType == JockyType::UNKNOWN) {
            throw std::runtime_error(
                "Unknown return type '" + function->returnType +
                "' in function '" + function->name + "'"
            );
        }

        if (!symbols.declareFunction(
                function->name, paramTypes, retType, false)) {
            throw std::runtime_error(
                "Function '" + function->name + "' is already declared"
            );
        }
    }
}


// ============================================================
// Analyze Function
// ============================================================

void SemanticAnalyzer::analyzeFunction(FunctionDeclaration& function) {
    static const std::unordered_set<std::string> knownAttributes = {
        "obfuscate", "heavy", "encrypt", "full", "crypto"
    };

    for (const auto& attr : function.attributes) {
        if (knownAttributes.find(attr) == knownAttributes.end()) {
            throw std::runtime_error(
                "Unknown attribute '@" + attr +
                "' on function '" + function.name + "'"
            );
        }
    }

    currentReturnType = stringToType(function.returnType);

    symbols.enterScope();

    for (const auto& p : function.parameters) {
        JockyType t = stringToType(p.type);
        if (t == JockyType::UNKNOWN) t = JockyType::PTR;

        if (!symbols.declareVariable(p.name, t)) {
            symbols.exitScope();
            throw std::runtime_error(
                "Duplicate parameter '" + p.name +
                "' in function '" + function.name + "'"
            );
        }
    }

    analyzeBlock(*function.body, false);

    symbols.exitScope();
    currentReturnType = JockyType::VOID;
}


// ============================================================
// Analyze Main
// ============================================================

void SemanticAnalyzer::analyzeMain(MainBlock& mainBlock) {
    currentReturnType = JockyType::VOID;
    symbols.enterScope();
    analyzeBlock(*mainBlock.body, false);
    symbols.exitScope();
}


// ============================================================
// Analyze Block
// ============================================================

void SemanticAnalyzer::analyzeBlock(Block& block, bool createScope) {
    if (createScope) symbols.enterScope();

    for (auto& stmt : block.statements) {
        analyzeStatement(*stmt);
    }

    if (createScope) symbols.exitScope();
}


// ============================================================
// Statement Dispatcher
// ============================================================

void SemanticAnalyzer::analyzeStatement(Statement& statement) {

    if (auto* s = dynamic_cast<VariableDeclaration*>(&statement)) {
        analyzeVariableDeclaration(*s); return;
    }
    if (auto* s = dynamic_cast<Assignment*>(&statement)) {
        analyzeAssignment(*s); return;
    }
    if (auto* s = dynamic_cast<ArrayAssignment*>(&statement)) {
        analyzeArrayAssignment(*s); return;
    }
    if (auto* s = dynamic_cast<IfStatement*>(&statement)) {
        analyzeIfStatement(*s); return;
    }
    if (auto* s = dynamic_cast<WhileStatement*>(&statement)) {
        analyzeWhileStatement(*s); return;
    }
    if (auto* s = dynamic_cast<ForStatement*>(&statement)) {
        analyzeForStatement(*s); return;
    }
    if (dynamic_cast<BreakStatement*>(&statement)) {
        if (loopDepth_ == 0) {
            throw std::runtime_error("'break' outside of loop");
        }
        return;
    }
    if (dynamic_cast<ContinueStatement*>(&statement)) {
        if (loopDepth_ == 0) {
            throw std::runtime_error("'continue' outside of loop");
        }
        return;
    }
    if (auto* s = dynamic_cast<ReturnStatement*>(&statement)) {
        analyzeReturnStatement(*s); return;
    }
    if (auto* s = dynamic_cast<ExpressionStatement*>(&statement)) {
        analyzeExpressionStatement(*s); return;
    }

    throw std::runtime_error("Unknown statement type");
}


// ============================================================
// Variable Declaration
// ============================================================

void SemanticAnalyzer::analyzeVariableDeclaration(
    VariableDeclaration& stmt
) {
    JockyType declared = stringToType(stmt.type);

    if (declared == JockyType::UNKNOWN) {
        // Could be a struct type — treat as PTR
        declared = JockyType::PTR;
    }

    if (declared == JockyType::VOID) {
        throw std::runtime_error(
            "Variable '" + stmt.name + "' cannot have type void"
        );
    }

    // Arrays and structs may have no initializer
    if (!stmt.initializer) {
        if (!symbols.declareVariable(stmt.name, declared)) {
            throw std::runtime_error(
                "Variable '" + stmt.name + "' is already declared"
            );
        }
        return;
    }

    JockyType initType = analyzeExpression(*stmt.initializer);

    if (!typesCompatible(declared, initType, stmt.type)) {
        throw std::runtime_error(
            "Type mismatch for variable '" + stmt.name +
            "': expected " + typeToString(declared) +
            ", got " + typeToString(initType)
        );
    }

    if (!symbols.declareVariable(stmt.name, declared)) {
        throw std::runtime_error(
            "Variable '" + stmt.name + "' is already declared in this scope"
        );
    }
}


// ============================================================
// Assignment
// ============================================================

void SemanticAnalyzer::analyzeAssignment(Assignment& stmt) {
    const VariableSymbol* var = symbols.lookupVariable(stmt.name);

    if (!var) {
        throw std::runtime_error(
            "Variable '" + stmt.name + "' is not declared"
        );
    }

    JockyType valType = analyzeExpression(*stmt.value);

    if (!typesCompatible(var->type, valType)) {
        throw std::runtime_error(
            "Cannot assign " + typeToString(valType) +
            " to variable '" + stmt.name +
            "' of type " + typeToString(var->type)
        );
    }
}


// ============================================================
// Array Assignment
// ============================================================

void SemanticAnalyzer::analyzeArrayAssignment(ArrayAssignment& stmt) {
    const VariableSymbol* var = symbols.lookupVariable(stmt.name);

    if (!var) {
        throw std::runtime_error(
            "Variable '" + stmt.name + "' is not declared"
        );
    }

    // Just check index is numeric and value is compatible
    JockyType idxType = analyzeExpression(*stmt.index);
    (void)idxType; // index type not strictly checked

    analyzeExpression(*stmt.value);
}


// ============================================================
// If Statement
// ============================================================

void SemanticAnalyzer::analyzeIfStatement(IfStatement& stmt) {
    JockyType condType = analyzeExpression(*stmt.condition);

    if (condType != JockyType::BOOL) {
        throw std::runtime_error(
            "If condition must be bool, got " + typeToString(condType)
        );
    }

    analyzeBlock(*stmt.thenBlock);

    if (stmt.elseBlock) {
        analyzeBlock(*stmt.elseBlock);
    }
}


// ============================================================
// While Statement
// ============================================================

void SemanticAnalyzer::analyzeWhileStatement(WhileStatement& stmt) {
    JockyType condType = analyzeExpression(*stmt.condition);

    if (condType != JockyType::BOOL) {
        throw std::runtime_error(
            "While condition must be bool, got " + typeToString(condType)
        );
    }

    loopDepth_++;
    analyzeBlock(*stmt.body);
    loopDepth_--;
}


// ============================================================
// For Statement
// ============================================================

void SemanticAnalyzer::analyzeForStatement(ForStatement& stmt) {
    symbols.enterScope();

    JockyType initType = stringToType(stmt.initType);
    if (initType == JockyType::UNKNOWN) initType = JockyType::INT;

    JockyType exprType = analyzeExpression(*stmt.initExpr);
    if (!typesCompatible(initType, exprType)) {
        throw std::runtime_error(
            "For-init type mismatch"
        );
    }

    symbols.declareVariable(stmt.initName, initType);

    JockyType condType = analyzeExpression(*stmt.condition);
    if (condType != JockyType::BOOL) {
        throw std::runtime_error(
            "For condition must be bool"
        );
    }

    // Check increment variable exists
    const VariableSymbol* incVar = symbols.lookupVariable(stmt.incName);
    if (!incVar) {
        throw std::runtime_error(
            "For-increment variable '" + stmt.incName + "' not declared"
        );
    }

    analyzeExpression(*stmt.incExpr);

    loopDepth_++;
    analyzeBlock(*stmt.body, false); // scope already open
    loopDepth_--;

    symbols.exitScope();
}


// ============================================================
// Return Statement
// ============================================================

void SemanticAnalyzer::analyzeReturnStatement(ReturnStatement& stmt) {
    // Void return
    if (!stmt.value) {
        if (currentReturnType != JockyType::VOID) {
            throw std::runtime_error(
                "Void return in non-void function"
            );
        }
        return;
    }

    if (currentReturnType == JockyType::VOID) {
        throw std::runtime_error(
            "Cannot return a value from a void function or main"
        );
    }

    JockyType retType = analyzeExpression(*stmt.value);

    if (!typesCompatible(currentReturnType, retType)) {
        throw std::runtime_error(
            "Return type mismatch: expected " +
            typeToString(currentReturnType) +
            ", got " + typeToString(retType)
        );
    }
}


// ============================================================
// Expression Statement
// ============================================================

void SemanticAnalyzer::analyzeExpressionStatement(
    ExpressionStatement& stmt
) {
    analyzeExpression(*stmt.expression);
}


// ============================================================
// Expression Analysis
// ============================================================

JockyType SemanticAnalyzer::analyzeExpression(Expression& expression) {

    if (dynamic_cast<IntegerLiteral*>(&expression)) {
        return JockyType::INT;
    }

    if (dynamic_cast<BoolLiteral*>(&expression)) {
        return JockyType::BOOL;
    }

    if (dynamic_cast<NullLiteral*>(&expression)) {
        return JockyType::PTR;
    }

    if (dynamic_cast<StringLiteral*>(&expression)) {
        return JockyType::STRING;
    }

    if (auto* e = dynamic_cast<SizeofExpression*>(&expression)) {
        (void)e;
        return JockyType::INT;
    }

    if (auto* e = dynamic_cast<CastExpression*>(&expression)) {
        analyzeExpression(*e->expr);
        JockyType target = stringToType(e->targetType);
        if (target == JockyType::UNKNOWN) target = JockyType::PTR;
        return target;
    }

    if (auto* e = dynamic_cast<TernaryExpression*>(&expression)) {
        JockyType condType = analyzeExpression(*e->condition);
        if (condType != JockyType::BOOL) {
            throw std::runtime_error("Ternary condition must be bool");
        }
        JockyType thenType = analyzeExpression(*e->thenExpr);
        analyzeExpression(*e->elseExpr);
        return thenType;
    }

    if (auto* e = dynamic_cast<ArrayIndexExpression*>(&expression)) {
        const VariableSymbol* var = symbols.lookupVariable(e->name);
        if (!var) {
            throw std::runtime_error(
                "Variable '" + e->name + "' is not declared"
            );
        }
        analyzeExpression(*e->index);
        if (var->type == JockyType::BYTE_ARRAY) return JockyType::BYTE;
        if (var->type == JockyType::INT_ARRAY)  return JockyType::INT;
        return JockyType::PTR; // handle arrays / ptr arrays
    }

    if (auto* e = dynamic_cast<VariableReference*>(&expression)) {
        const VariableSymbol* var = symbols.lookupVariable(e->name);
        if (var) return var->type;

        // Could be a function reference used as a pointer
        const FunctionSymbol* fn = symbols.lookupFunction(e->name);
        if (fn) return JockyType::PTR;

        throw std::runtime_error(
            "Variable '" + e->name + "' is not declared"
        );
    }

    if (auto* e = dynamic_cast<UnaryExpression*>(&expression)) {
        JockyType opType = analyzeExpression(*e->operand);

        if (e->op == "-") {
            if (opType != JockyType::INT && opType != JockyType::LONG) {
                throw std::runtime_error(
                    "Unary '-' requires int or long operand"
                );
            }
            return opType;
        }

        if (e->op == "!") {
            if (opType != JockyType::BOOL) {
                throw std::runtime_error(
                    "Unary '!' requires bool operand"
                );
            }
            return JockyType::BOOL;
        }

        if (e->op == "~") {
            return opType; // bitwise NOT preserves type
        }

        throw std::runtime_error("Unknown unary operator '" + e->op + "'");
    }

    if (auto* e = dynamic_cast<BinaryExpression*>(&expression)) {
        JockyType left  = analyzeExpression(*e->left);
        JockyType right = analyzeExpression(*e->right);

        // Arithmetic
        if (e->op == "+" || e->op == "-" || e->op == "*" || e->op == "/") {
            // Allow int arithmetic and long arithmetic; also ptr + int
            if (left == JockyType::PTR && right == JockyType::INT) {
                return JockyType::PTR;
            }
            if ((left == JockyType::INT || left == JockyType::LONG ||
                 left == JockyType::BYTE) &&
                (right == JockyType::INT || right == JockyType::LONG ||
                 right == JockyType::BYTE)) {
                // Promote to the wider type
                if (left == JockyType::LONG || right == JockyType::LONG) {
                    return JockyType::LONG;
                }
                return JockyType::INT;
            }
            throw std::runtime_error(
                "Arithmetic operator '" + e->op + "' requires numeric operands"
            );
        }

        // Bitwise
        if (e->op == "&" || e->op == "|" || e->op == "^" ||
            e->op == "<<" || e->op == ">>") {
            return (left == JockyType::LONG || right == JockyType::LONG)
                       ? JockyType::LONG
                       : JockyType::INT;
        }

        // Comparison
        if (e->op == "<"  || e->op == ">"  ||
            e->op == "<=" || e->op == ">=") {
            return JockyType::BOOL;
        }

        // Equality
        if (e->op == "==" || e->op == "!=") {
            return JockyType::BOOL;
        }

        // Logical
        if (e->op == "&&" || e->op == "||") {
            if (left != JockyType::BOOL || right != JockyType::BOOL) {
                throw std::runtime_error(
                    "Logical operator '" + e->op + "' requires bool operands"
                );
            }
            return JockyType::BOOL;
        }

        throw std::runtime_error(
            "Unknown binary operator '" + e->op + "'"
        );
    }

    if (auto* e = dynamic_cast<FunctionCall*>(&expression)) {
        const FunctionSymbol* fn = symbols.lookupFunction(e->functionName);

        if (!fn) {
            throw std::runtime_error(
                "Function '" + e->functionName + "' is not declared"
            );
        }

        // For variadic-ish builtins (call_with), skip arg count check
        bool isVariadic =
            (e->functionName == "call_with" || e->functionName == "call");

        if (!isVariadic &&
            e->arguments.size() != fn->parameterTypes.size()) {
            throw std::runtime_error(
                "Function '" + e->functionName + "' expects " +
                std::to_string(fn->parameterTypes.size()) +
                " arguments, got " +
                std::to_string(e->arguments.size())
            );
        }

        // Analyze arguments — relax type checking for builtins
        for (std::size_t i = 0; i < e->arguments.size(); ++i) {
            JockyType argType = analyzeExpression(*e->arguments[i]);

            if (!fn->builtin && i < fn->parameterTypes.size()) {
                JockyType expected = fn->parameterTypes[i];
                if (!typesCompatible(expected, argType)) {
                    throw std::runtime_error(
                        "Argument " + std::to_string(i + 1) +
                        " of '" + e->functionName + "' expects " +
                        typeToString(expected) +
                        ", got " + typeToString(argType)
                    );
                }
            }
            // Built-ins: accept any type
        }

        return fn->returnType;
    }

    throw std::runtime_error("Unknown expression type");
}

} // namespace jocky
