#include <iostream>
#include <string>

#include "jocky/lexer/Lexer.h"
#include "jocky/parser/Parser.h"
#include "jocky/semantic/SemanticAnalyzer.h"
#include "jocky/codegen/CodeGenerator.h"

int main() {

    std::string source = R"(

        fn add(a: int, b: int) -> int {
            let result: int = a + b
            return result
        }

        main {
            let value: int = add(10, 20)
            exit(value)
        }

    )";

    try {

        // Lexer

        jocky::Lexer lexer(source);

        auto tokens =
            lexer.tokenize();

        std::cout
            << "Lexing successful\n";


        // Parser

        jocky::Parser parser(tokens);

        auto program =
            parser.parse();

        std::cout
            << "Parsing successful\n";


        // Semantic Analysis

        jocky::SemanticAnalyzer analyzer;

        analyzer.analyze(
            *program
        );

        std::cout
            << "Semantic analysis successful\n";


        // LLVM Code Generation

        jocky::CodeGenerator generator;

        generator.generate(
            *program
        );

        generator.writeToFile(
            "output.ll"
        );

        std::cout
            << "LLVM IR generation successful\n";

        std::cout
            << "Generated: output.ll\n";

    }
    catch (
        const std::exception& e
    ) {

        std::cerr
            << "Compiler error: "
            << e.what()
            << "\n";

        return 1;
    }

    return 0;
}