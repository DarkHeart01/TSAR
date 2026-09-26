#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "jocky/semantic/Types.h"

namespace jocky {

struct VariableSymbol {
    std::string name;
    JockyType type;
};

struct FunctionSymbol {
    std::string name;
    std::vector<JockyType> parameterTypes;
    JockyType returnType;
    bool builtin;
};

class SymbolTable {
public:
    SymbolTable();

    void enterScope();
    void exitScope();

    bool declareVariable(
        const std::string& name,
        JockyType type
    );

    const VariableSymbol* lookupVariable(
        const std::string& name
    ) const;

    bool declareFunction(
        const std::string& name,
        const std::vector<JockyType>& parameterTypes,
        JockyType returnType,
        bool builtin = false
    );

    const FunctionSymbol* lookupFunction(
        const std::string& name
    ) const;

private:
    std::vector<
        std::unordered_map<std::string, VariableSymbol>
    > variableScopes;

    std::unordered_map<
        std::string,
        FunctionSymbol
    > functions;
};

} // namespace jocky