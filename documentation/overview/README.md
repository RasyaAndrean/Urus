# Overview

## Project Goals

URUS is a programming language designed to be **simple, safe, and easy to learn** — while being powerful enough to build real applications. URUS transpiles to C, producing fast native binaries without requiring a heavy runtime.

## Problems Solved

| Problem | URUS Solution |
|---------|---------------|
| C is too low-level and prone to memory bugs | Automatic ref-counting, strict type safety |
| Python/JS are too slow for some use cases | Compiles to native C binary |
| Rust's learning curve is too steep | Simple syntax, focused feature set |
| New languages often lack documentation | Complete documentation from V0.2/1 |
| Cross-platform build is painful | CMake build system, works on Windows/Linux/macOS |
| Distributing compilers is complex | Standalone binary — runtime embedded, zero external files |

## Target Users

- **Students** who want to learn programming language and compiler concepts
- **Developers** who need a simple language with native performance
- **Educators** who teach compiler design
- **Hobbyists** who enjoy exploring new languages
- **Teams** looking for a lightweight compiled language for small tools

## High-Level Explanation

```
Source Code (.urus)
       |
       v
  +-----------+
  |   Lexer   |  Tokenize source code
  +-----+-----+
        |
        v
  +-----------+
  |  Parser   |  Build Abstract Syntax Tree (AST)
  +-----+-----+
        |
        v
  +-----------+
  |   Sema    |  Type checking & semantic analysis
  +-----+-----+
        |
        v
  +-----------+
  |  Codegen  |  Generate C code
  +-----+-----+
        |
        v
  +-----------+
  |    GCC    |  Compile C to binary
  +-----+-----+
        |
        v
  Native Binary (.exe / ELF)
```

URUS takes a **transpiler** approach: source code is first translated to C, then GCC compiles the C into a binary. This provides:
- Native performance without a VM
- Portability to all platforms supported by GCC
- Easier debugging (you can inspect the generated C with `--emit-c`)
- Leverage decades of GCC optimizations

## Tech Stack

| Component | Technology |
|-----------|------------|
| Compiler | C11 (self-contained, no dependencies) |
| Runtime | Header-only C library (`urus_runtime.h`, embedded in binary) |
| Memory Management | Reference counting with automatic retain/release |
| Backend | GCC (C compiler) |
| Build System | CMake 3.10+ |
| Target Output | Standard C11 code |
| Platforms | Windows (MSVC/MinGW), Linux, macOS |

## Key Features

| Feature | Description |
|---------|-------------|
| Static typing | All types checked at compile time |
| Immutable by default | Variables are immutable unless declared with `mut` |
| Pattern matching | `match` expressions with enum destructuring |
| String interpolation | `f"Hello {name}"` syntax |
| Error handling | `Result<T, E>` type with `Ok`/`Err` |
| Reference counting | Automatic memory management for strings, arrays, structs |
| Modules | `import "file.urus"` for multi-file projects |
| Struct spread syntax | `Point { x: 10.0, ..p1 }` — functional update |
| Numeric separators | `1_000_000` — underscores in number literals |
| Standalone compiler | Single binary, no external dependencies at runtime |

## Current Status

**Version:** V0.2/3(A) "Added"

The language is in active pre-stable development. Core features are functional and tested. See the [roadmap](../roadmap/) for planned features.
