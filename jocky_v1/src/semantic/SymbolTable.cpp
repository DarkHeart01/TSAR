#include "jocky/semantic/SymbolTable.h"

namespace jocky {

SymbolTable::SymbolTable() {
    enterScope();
}

void SymbolTable::enterScope() {
    variableScopes.emplace_back();
}

void SymbolTable::exitScope() {
    if (!variableScopes.empty()) {
        variableScopes.pop_back();
    }
}

bool SymbolTable::declareVariable(
    const std::string& name,
    JockyType type
) {
    if (variableScopes.empty()) {
        enterScope();
    }

    auto& currentScope = variableScopes.back();

    if (currentScope.find(name) != currentScope.end()) {
        return false;
    }

    currentScope.emplace(
        name,
        VariableSymbol{name, type}
    );

    return true;
}

const VariableSymbol*
SymbolTable::lookupVariable(
    const std::string& name
) const {
    for (
        auto scope = variableScopes.rbegin();
        scope != variableScopes.rend();
        ++scope
    ) {
        auto found = scope->find(name);

        if (found != scope->end()) {
            return &found->second;
        }
    }

    return nullptr;
}

bool SymbolTable::declareFunction(
    const std::string& name,
    const std::vector<JockyType>& parameterTypes,
    JockyType returnType,
    bool builtin
) {
    if (functions.find(name) != functions.end()) {
        return false;
    }

    functions.emplace(
        name,
        FunctionSymbol{
            name,
            parameterTypes,
            returnType,
            builtin
        }
    );

    return true;
}

const FunctionSymbol*
SymbolTable::lookupFunction(
    const std::string& name
) const {
    auto found = functions.find(name);

    if (found == functions.end()) {
        return nullptr;
    }

    return &found->second;
}

} // namespace jocky