# JOCKY DSL — Language Frontend

> The JOCKY DSL frontend is a custom compiler that translates `.jky` source files into LLVM IR. It is the first stage of the TSAR build pipeline and defines the JOCKY language itself.

---

## Overview

JOCKY is a statically typed, systems-level domain-specific language designed specifically for writing forensic analysis and adversary detection scripts. Its design philosophy is shaped by one constraint above all others: the output must resist static analysis.

Standard languages produce predictable compiler output. GCC and MSVC emit recognisable patterns — standard prologues, predictable register usage, well-known runtime initialisation sequences. JOCKY's design removes the starting points that signature engines rely on. Because the language has no standard library, there are no known import sequences. Because it emits raw LLVM IR, the downstream Polaris compiler can apply aggressive structural transformations that would be unsafe on standard C/C++ IR. Because the syntax explicitly supports the polymorphic engine's transformation rules, variable names and dead code patterns are randomised before the IR is even generated.

The result is a language where every compile of the same source file produces structurally distinct machine code.

---

## Language Design

### Type System

JOCKY uses a simple static type system with the following primitives:

| Type | Description |
|---|---|
| `int` | 64-bit signed integer |
| `bool` | Boolean (`true` / `false`) |
| `void` | No return value |

Types are explicitly declared. There is no implicit conversion and no type inference — every variable declaration requires a type annotation. This is intentional: the explicit type annotations give the LLVM IR codegen precise information without any ambiguity, producing cleaner IR that the obfuscation passes can work with more predictably.

### Variable Declarations

```jocky
let x: int = 42;
let flag: bool = true;
```

`let` bindings are scoped to the enclosing function body. There are no globals in the current version.

### Functions

```jocky
fn add(a: int, b: int) -> int {
    let result: int = a + b;
    return result;
}
```

### Control Flow

```jocky
if condition {
    // ...
} else {
    // ...
}

while condition {
    // ...
}
```

### No Standard Library

JOCKY has no built-in standard library. All system interaction is performed through the JOCKY runtime layer, which is linked separately by the Polaris compiler pipeline. The runtime resolves Windows API functions at runtime via PEB walking and djb2 hashing — no import names appear in the binary's IAT. This is a language-level guarantee: it is not possible to accidentally link against a named Windows API from JOCKY source code.

---

## Compiler Pipeline

```
.jky source file
        │
        ▼  ── Lexer ─────────────────────────────────────────────────
        │     Token.cpp / Lexer.cpp
        │     Tokenises source into a typed token stream.
        │     Handles keywords, identifiers, literals, operators.
        │
        ▼  ── Parser ──────────────────────────────────────────────────
        │     Parser.cpp
        │     Recursive-descent parser. Builds a typed Abstract Syntax
        │     Tree (AST) from the flat token stream. Each AST node
        │     carries its source location for error reporting.
        │
        ▼  ── Semantic Analyser ────────────────────────────────────────
        │     SemanticAnalyzer.cpp / SymbolTable.cpp / Types.cpp
        │     Walks the AST and performs:
        │       - Type checking (operand compatibility, return types)
        │       - Scope resolution (variable and function lookup)
        │       - Symbol table construction (one scope per block)
        │     Rejects type errors and undefined references before
        │     any code is generated.
        │
        ▼  ── Code Generator ───────────────────────────────────────────
        │     CodeGenerator.cpp
        │     Walks the type-annotated AST and emits LLVM IR using the
        │     LLVM Core C++ API (LLVMContext, IRBuilder, Module).
        │     Produces human-readable .ll text IR.
        │
        ▼
  output.ll  ──►  Polaris Compiler  (compiler/)
```

The `.ll` output is handed off to the Polaris compiler in `compiler/` for obfuscation, linking against the modular JOCKY runtime, and PE packaging. The two stages are integrated transparently by the pipeline driver — invoking `jocky.exe` on a `.jky` file runs both stages end-to-end.

---

## Polymorphic Engine Integration

Before the compiler processes source, `poly_engine.py` (in `compiler/jocky/polymorphic/`) transforms the `.jky` source to ensure no two compilations produce the same IR. Specifically:

- **Variable renaming** — every `let` variable is renamed to a random 8-character hex identifier (`_jk_a3f9b2c1`)
- **Dead declaration chains** — sequences of dummy `let` declarations with arithmetic cross-references are injected at every function body opening; they are semantically inert but syntactically valid and produce real IR that the optimiser cannot trivially elide
- **Always-false conditional blocks** — `if (x & 0) == x { <dead code> }` blocks are injected to add unreachable branches to the CFG

The random seed changes every run, so the same source file produces different variable names, different dead code, and different IR structure on every invocation.

---

## Building

### Prerequisites

- Windows x64
- CMake 3.20+
- Visual Studio 2022 Build Tools (MSVC v143)
- LLVM 16 development headers and libraries

### Build

```powershell
cd jocky_v1
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Output: `build/Release/jocky.exe`

The compiler driver in `compiler/` auto-detects this binary at `../../jocky_v1/build/Release/jocky.exe` relative to its own location.

### Verify

```powershell
.\build\Release\jocky.exe --help
```

---

## Standalone Usage

```powershell
jocky.exe <input.jky> -o <output.ll>
```

To run the full pipeline (DSL → binary) from the compiler directory:

```powershell
cd compiler
.\jocky\driver\jocky.exe input.jky -o output.exe
```

---

## Error Handling

The compiler produces structured error messages with source locations:

```
error: type mismatch — expected int, got bool [line 12, col 18]
error: undefined symbol 'foo' [line 7, col 5]
error: function 'bar' must return int — missing return statement
```

The compiler exits with a non-zero status on any error. No partial output is written.

---

## Directory Layout

```
jocky_v1/
├── CMakeLists.txt
├── include/
│   └── jocky/
│       └── semantic/
│           └── SymbolTable.h       Symbol table interface
├── src/
│   ├── main.cpp                    Entry point — arg parsing, pipeline orchestration
│   ├── lexer/
│   │   ├── Token.cpp               Token type and keyword table
│   │   └── Lexer.cpp               Character stream → token stream
│   ├── ast/
│   │   └── AST.cpp                 AST node definitions (Expr, Stmt, Decl)
│   ├── parser/
│   │   └── Parser.cpp              Token stream → typed AST
│   ├── semantic/
│   │   ├── Types.cpp               Type representation and compatibility rules
│   │   ├── SymbolTable.cpp         Scoped symbol table with nested lookup
│   │   └── SemanticAnalyzer.cpp    AST walker — type checking and resolution
│   └── codegen/
│       └── CodeGenerator.cpp       AST → LLVM IR (LLVMContext, IRBuilder, Module)
├── docs/
│   ├── grammar.md                  Formal BNF grammar
│   ├── language_spec.md            Full language specification
│   └── runtime_contract.md        ABI contract between .jky code and the runtime
└── examples/
    └── *.ll / *.exe                Compiled example outputs for reference
```

---

## Relationship to Other Components

| Component | Relationship |
|---|---|
| `compiler/` | Receives the `.ll` output; applies obfuscation passes, links the runtime, spoof the PE |
| `compiler/jocky/polymorphic/poly_engine.py` | Transforms `.jky` source before this compiler sees it |
| `compiler/jocky/runtime/` | The runtime objects linked into every `.jky`-compiled binary |
| `jocky-framework/` | The operator CLI invokes the full pipeline, which calls into this frontend |
