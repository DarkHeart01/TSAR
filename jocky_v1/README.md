# JOCKY DSL — Language Frontend

The JOCKY DSL frontend is a custom compiler that translates `.jky` source files into LLVM IR. It is the first stage of the TSAR build pipeline and the defining component of the JOCKY language.

The language is designed specifically for generating analysis scripts that produce structurally variable binaries — the syntax maps to LLVM IR constructs that the downstream Polaris compiler can aggressively transform without semantic change.

---

## Compiler Stages

```
.jky source
    │
    ▼  Lexer          Token.cpp / Lexer.cpp
    │  Tokenises source into a flat token stream
    │
    ▼  Parser          Parser.cpp
    │  Builds a typed Abstract Syntax Tree from the token stream
    │
    ▼  Semantic        SemanticAnalyzer.cpp / SymbolTable.cpp / Types.cpp
    │  Type checking, scope resolution, symbol table construction
    │
    ▼  Code Generator  CodeGenerator.cpp
    │  Walks the AST and emits LLVM IR via the LLVM Core API
    │
    ▼  output.ll       LLVM bitcode / human-readable IR
```

The emitted `.ll` file is fed directly into the Polaris compiler pipeline in `compiler/` for obfuscation, linking, and PE packaging.

---

## Language Features

- **Static typing** with primitive types: `int`, `bool`, `void`
- **`let` variable declarations** with type annotation: `let x: int = 42;`
- **Functions** with typed parameters and return types
- **Control flow**: `if`/`else`, `while` loops
- **Arithmetic and boolean expressions**
- **Direct syscall intrinsics** — the runtime contract (`docs/runtime_contract.md`) defines the ABI boundary between `.jky` code and the modular runtime objects linked by the compiler

The language intentionally excludes standard library bindings. All system interaction is performed through the JOCKY runtime layer linked at compile time, which uses direct NT syscalls via Hell's Gate SSN resolution rather than the Windows API.

---

## Building

### Prerequisites

- Windows x64
- CMake 3.20+
- Visual Studio 2022 Build Tools (MSVC v143)
- LLVM 16 development headers and libraries

### Build steps

```powershell
cd jocky_v1
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Output: `build/Release/jocky.exe`

The compiler driver in `compiler/jocky/driver/jocky.cpp` auto-detects `jocky.exe` at `../../jocky_v1/build/Release/jocky.exe` relative to itself.

---

## Usage

```powershell
jocky.exe <input.jky> -o <output.ll>
```

The output `.ll` file can be passed directly to the Polaris pipeline:

```powershell
# From compiler/
.\jocky\driver\jocky.exe input.jky -o output.exe
# The driver invokes jocky_v1 internally for .jky inputs
```

---

## Directory Layout

```
jocky_v1/
├── CMakeLists.txt
├── include/
│   └── jocky/
│       └── semantic/
│           └── SymbolTable.h
├── src/
│   ├── main.cpp               Entry point — parse args, invoke pipeline
│   ├── lexer/
│   │   ├── Token.cpp          Token type definitions
│   │   └── Lexer.cpp          Source → token stream
│   ├── ast/
│   │   └── AST.cpp            AST node definitions
│   ├── parser/
│   │   └── Parser.cpp         Token stream → AST
│   ├── semantic/
│   │   ├── Types.cpp          Type system
│   │   ├── SymbolTable.cpp    Scope-aware symbol table
│   │   └── SemanticAnalyzer.cpp  Type checking and resolution
│   └── codegen/
│       └── CodeGenerator.cpp  AST → LLVM IR
├── docs/
│   ├── grammar.md             Formal grammar (BNF)
│   ├── language_spec.md       Language specification
│   └── runtime_contract.md    ABI contract with the modular runtime
└── examples/
    └── *.ll / *.exe           Compiled example outputs
```
