#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "jocky/lexer/Lexer.h"
#include "jocky/parser/Parser.h"
#include "jocky/semantic/SemanticAnalyzer.h"
#include "jocky/codegen/CodeGenerator.h"

namespace {

void printUsage(const char* programName) {
    std::cout
        << "JOCKY Compiler\n"
        << "\n"
        << "Usage:\n"
        << "  " << programName << " <input.jky> -o <output.ll>\n"
        << "\n"
        << "Options:\n"
        << "  -o, --output <file>   Specify output LLVM IR file\n"
        << "  -h, --help            Show this help message\n"
        << "  --version             Show compiler version\n";
}

void printVersion() {
    std::cout << "JOCKY compiler version 0.1.0\n";
}

} // namespace

int main(int argc, char* argv[]) {

    // --------------------------------------------------
    // Command-line arguments
    // --------------------------------------------------

    if (argc == 1) {
        printUsage(argv[0]);
        return 1;
    }

    std::string inputFile;
    std::string outputFile;

    for (int i = 1; i < argc; ++i) {

        std::string arg = argv[i];

        // Help
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }

        // Version
        if (arg == "--version") {
            printVersion();
            return 0;
        }

        // Output file
        if (arg == "-o" || arg == "--output") {

            if (i + 1 >= argc) {
                std::cerr
                    << "Error: missing output file after "
                    << arg << "\n";

                return 1;
            }

            outputFile = argv[++i];
            continue;
        }

        // Unknown option
        if (!arg.empty() && arg[0] == '-') {
            std::cerr
                << "Error: unknown option: "
                << arg << "\n";

            return 1;
        }

        // Input file
        if (inputFile.empty()) {
            inputFile = arg;
        }
        else {
            std::cerr
                << "Error: multiple input files are not supported.\n";

            return 1;
        }
    }

    // --------------------------------------------------
    // Validate input
    // --------------------------------------------------

    if (inputFile.empty()) {
        std::cerr
            << "Error: no input file specified.\n\n";

        printUsage(argv[0]);
        return 1;
    }

    // --------------------------------------------------
    // Default output
    // --------------------------------------------------

    if (outputFile.empty()) {

        std::filesystem::path inputPath(inputFile);

        outputFile =
            inputPath.stem().string() + ".ll";
    }

    try {

        // --------------------------------------------------
        // Read source file
        // --------------------------------------------------

        std::ifstream file(inputFile);

        if (!file.is_open()) {
            throw std::runtime_error(
                "Could not open input file: " + inputFile
            );
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        std::string source = buffer.str();

        // --------------------------------------------------
        // Lexer
        // --------------------------------------------------

        jocky::Lexer lexer(source);

        auto tokens = lexer.tokenize();

        std::cout
            << "Lexing successful\n";

        // --------------------------------------------------
        // Parser
        // --------------------------------------------------

        jocky::Parser parser(tokens);

        auto program = parser.parse();

        std::cout
            << "Parsing successful\n";

        // --------------------------------------------------
        // Semantic Analysis
        // --------------------------------------------------

        jocky::SemanticAnalyzer analyzer;

        analyzer.analyze(*program);

        std::cout
            << "Semantic analysis successful\n";

        // --------------------------------------------------
        // LLVM Code Generation
        // --------------------------------------------------

        jocky::CodeGenerator generator;

        generator.generate(*program);

        // --------------------------------------------------
        // Create output directory if necessary
        // --------------------------------------------------

        std::filesystem::path outputPath(outputFile);

        if (outputPath.has_parent_path()) {
            std::filesystem::create_directories(
                outputPath.parent_path()
            );
        }

        // --------------------------------------------------
        // Write LLVM IR
        // --------------------------------------------------

        generator.writeToFile(outputFile);

        std::cout
            << "LLVM IR generation successful\n";

        std::cout
            << "Generated: "
            << outputFile
            << "\n";
    }
    catch (const std::exception& e) {

        std::cerr
            << "Compiler error: "
            << e.what()
            << "\n";

        return 1;
    }

    return 0;
}