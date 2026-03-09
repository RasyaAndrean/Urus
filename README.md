<p align="center">
  <h1 align="center">URUS Programming Language</h1>
  <p align="center">
    <strong>A statically-typed, compiled programming language that transpiles to portable C11.</strong>
  </p>
  <p align="center">Safe. Simple. Fast.</p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/version-1.0.1(F)-blue" alt="Version" />
  <img src="https://img.shields.io/badge/build-passing-brightgreen" alt="Build" />
  <img src="https://img.shields.io/badge/license-Apache%202.0-green" alt="License" />
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey" alt="Platform" />
  <img src="https://img.shields.io/badge/C-C11-orange" alt="C Standard" />
  <img src="https://img.shields.io/badge/status-stable-brightgreen" alt="Status" />
</p>

<p align="center">
  <a href="#quick-start">Quick Start</a> &nbsp;&bull;&nbsp;
  <a href="#features">Features</a> &nbsp;&bull;&nbsp;
  <a href="./documentation/">Documentation</a> &nbsp;&bull;&nbsp;
  <a href="./examples/">Examples</a> &nbsp;&bull;&nbsp;
  <a href="#roadmap">Roadmap</a>
</p>

---

## Language Status

URUS **v1.0.1(F) "Fixed"** is stable. The core language syntax and features are finalized. Breaking changes will only occur in major version bumps following [Semantic Versioning](https://semver.org/).

---

## Why URUS?

| Goal | How |
|------|-----|
| **Safer than C** | Reference counting, bounds checking, immutable by default |
| **Simpler than Rust** | No borrow checker, no lifetimes — just write code |
| **Faster than Python** | Compiles to native binary via C11 |
| **More portable than Go** | Transpiles to standard C11 — runs anywhere GCC runs |
| **Modern syntax** | Enums, pattern matching, string interpolation, Result type |

---

## Quick Start

### Requirements

- **GCC** 8+ (MinGW-w64 / MSYS2 on Windows)
- **CMake** 3.10+

### Build from Source

```bash
git clone https://github.com/Urus-Foundation/Urus.git
cd Urus/compiler

cmake -S . -B build
cmake --build build
```

Install to system:

```bash
# Linux / macOS
sudo cmake --install build

# Windows (Run As Administrator)
cmake --install build
```

### Prebuilt Binary

> Coming soon — check [Releases](https://github.com/Urus-Foundation/Urus/releases) page.

### Hello World

```urus
fn main(): void {
    print("Hello, World!");
}
```

```bash
urusc hello.urus -o hello
./hello
# Hello, World!
```

---

## Features

### Primitive Types

| Type | Description | C Equivalent |
|------|-------------|--------------|
| `int` | 64-bit signed integer | `int64_t` |
| `float` | 64-bit floating point | `double` |
| `bool` | Boolean (`true` / `false`) | `bool` |
| `str` | UTF-8 string (ref-counted) | `urus_str*` |
| `void` | No value | `void` |
| `[T]` | Dynamic array of T | `urus_array*` |
| `Result<T, E>` | Ok or Err value | `urus_result` |

### Variables

```urus
let x: int = 10;           // immutable
let mut count: int = 0;    // mutable
count += 1;
```

### Functions

```urus
fn add(a: int, b: int): int {
    return a + b;
}

fn greet(name: str) {
    print(f"Hello {name}!");
}
```

### Control Flow

```urus
// If / Else
if x > 10 {
    print("big");
} else {
    print("small");
}

// While
while x < 100 {
    x += 1;
}

// For (range)
for i in 0..10 {       // exclusive: 0 to 9
    print(f"{i}");
}

for i in 0..=10 {      // inclusive: 0 to 10
    print(f"{i}");
}

// For-each (array)
let names: [str] = ["Alice", "Bob"];
for name in names {
    print(name);
}
```

### Structs

```urus
struct Point {
    x: float;
    y: float;
}

fn distance(a: Point, b: Point): float {
    let dx: float = a.x - b.x;
    let dy: float = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}
```

### Enums & Pattern Matching

```urus
enum Shape {
    Circle(r: float);
    Rect(w: float, h: float);
    Point;
}

fn area(s: Shape): float {
    match s {
        Shape.Circle(r) => {
            return 3.14159 * r * r;
        }
        Shape.Rect(w, h) => {
            return w * h;
        }
        Shape.Point => {
            return 0.0;
        }
    }
    return 0.0;
}
```

### Arrays

```urus
let nums: [int] = [1, 2, 3, 4, 5];
let first: int = nums[0];

let mut items: [int] = [];
push(items, 42);
print(f"Length: {len(items)}");
```

### String Interpolation

```urus
let name: str = "World";
let count: int = 42;
print(f"Hello {name}! Answer: {count}");
```

### Modules / Imports

```urus
// math_utils.urus
fn square(x: int): int {
    return x * x;
}

// main.urus
import "math_utils.urus";

fn main(): void {
    print(f"5^2 = {square(5)}");
}
```

### Error Handling (Result Type)

```urus
fn divide(a: int, b: int): Result<int, str> {
    if b == 0 {
        return Err("division by zero");
    }
    return Ok(a / b);
}

fn main(): void {
    let r: Result<int, str> = divide(10, 0);
    if is_err(r) {
        print(f"Error: {unwrap_err(r)}");
    } else {
        print(f"Result: {unwrap(r)}");
    }
}
```

---

## Built-in Functions

### I/O

| Function | Description |
|----------|-------------|
| `print(value)` | Print to stdout with newline |
| `input()` | Read one line from stdin |
| `read_file(path)` | Read file contents as string |
| `write_file(path, s)` | Write string to file |
| `append_file(path, s)` | Append string to file |

### Array

| Function | Description |
|----------|-------------|
| `len(array)` | Array length |
| `push(array, v)` | Append to array |
| `pop(array)` | Remove last element |

### String

| Function | Description |
|----------|-------------|
| `str_len(s)` | String length |
| `str_upper(s)` | Uppercase |
| `str_lower(s)` | Lowercase |
| `str_trim(s)` | Trim whitespace |
| `str_contains(s, sub)` | Check if contains substring |
| `str_find(s, sub)` | Find index of substring |
| `str_slice(s, a, b)` | Substring from a to b |
| `str_replace(s, a, b)` | Replace occurrences |
| `str_starts_with(s, p)` | Check prefix |
| `str_ends_with(s, p)` | Check suffix |
| `str_split(s, delim)` | Split into array |
| `char_at(s, i)` | Character at index |

### Conversion

| Function | Description |
|----------|-------------|
| `to_str(value)` | Convert to string |
| `to_int(value)` | Convert to int |
| `to_float(value)` | Convert to float |

### Math

| Function | Description |
|----------|-------------|
| `abs(x)` / `fabs(x)` | Absolute value |
| `sqrt(x)` | Square root |
| `pow(x, y)` | Power |
| `min(a, b)` / `max(a, b)` | Min/max (int) |
| `fmin(a, b)` / `fmax(a, b)` | Min/max (float) |

### Result

| Function | Description |
|----------|-------------|
| `is_ok(result)` | Check if Ok |
| `is_err(result)` | Check if Err |
| `unwrap(result)` | Extract Ok value (aborts on Err) |
| `unwrap_err(result)` | Extract Err value (aborts on Ok) |

### Misc

| Function | Description |
|----------|-------------|
| `exit(code)` | Exit program |
| `assert(cond, msg)` | Abort if false |

---

## CLI Usage

```
URUS Compiler v1.0.1(F)

usage: urusc <file.urus> [options]

Options:
  --help      Show help message
  --version   Show compiler version
  --tokens    Display lexer tokens
  --ast       Display the AST
  --emit-c    Print generated C code to stdout
  -o <file>   Output executable name (default: a.exe)

Example:
  urusc main.urus -o app
```

---

## Architecture

```
Source (.urus)
     |
     v
  [ Lexer ]       Tokenize source code
     |
     v
  [ Parser ]      Build Abstract Syntax Tree
     |
     v
  [ Sema ]        Type checking & semantic analysis
     |
     v
  [ Codegen ]     Generate standard C11 code
     |
     v
  [ GCC ]         Compile to native binary
     |
     v
  Executable
```

---

## Project Stats

| Metric | Value |
|--------|-------|
| Version | 1.0.1(F) "Fixed" |
| Compiler Size | ~464 KB (standalone, runtime embedded) |
| Runtime | ~16 KB header-only (embedded in binary) |
| Compiler LOC | ~4,700+ |
| Runtime LOC | ~467 |
| Output | C11 compliant |
| Platforms | Windows, Linux, macOS |
| Build System | CMake 3.10+ |
| Dependencies | GCC 8+ only |

---

## Running Tests

```bash
cd tests/

# Linux / macOS
bash run_tests.sh ../compiler/build/urusc

# Windows
run_tests.bat ..\compiler\build\Release\urusc.exe
```

---

## Repository Structure

```
Urus/
|
|-- compiler/                   # Compiler source code
|   |-- src/                    # Implementation (.c)
|   |   |-- main.c             # CLI entry point & import resolver
|   |   |-- lexer.c            # Tokenizer
|   |   |-- parser.c           # Recursive descent parser
|   |   |-- sema.c             # Semantic analysis & type checking
|   |   |-- codegen.c          # C11 code generator
|   |   |-- ast.c              # AST node constructors
|   |   |-- error.c            # Error reporting with source context
|   |   '-- util.c             # File reading utilities
|   |
|   |-- include/               # Headers (.h)
|   |   |-- ast.h              # AST node definitions
|   |   |-- token.h            # Token types & definitions
|   |   |-- lexer.h            # Lexer interface
|   |   |-- parser.h           # Parser interface
|   |   |-- sema.h             # Semantic analysis interface
|   |   |-- codegen.h          # Code generator interface
|   |   |-- error.h            # Error reporting interface
|   |   |-- util.h             # Utility functions
|   |   '-- urus_runtime.h     # Runtime library (embedded in binary)
|   |
|   |-- cmake/
|   |   '-- embed-string.cmake # Embeds runtime header into binary
|   |
|   '-- CMakeLists.txt         # Build configuration
|
|-- examples/                   # Sample programs
|   |-- hello.urus             # Hello world
|   |-- fibonacci.urus         # Fibonacci sequence
|   |-- structs.urus           # Struct usage
|   |-- arrays.urus            # Array operations
|   |-- enums.urus             # Enums & pattern matching
|   |-- strings.urus           # String operations
|   |-- result.urus            # Error handling with Result
|   |-- files.urus             # File I/O
|   '-- modules/               # Multi-file import example
|       |-- main.urus
|       '-- math_utils.urus
|
|-- tests/
|   |-- valid/                  # Programs that should compile
|   |   |-- hello.urus
|   |   '-- stdlib_test.urus
|   |
|   |-- invalid/               # Programs that should be rejected
|   |   |-- bad_return.urus
|   |   |-- break_outside_loop.urus
|   |   |-- duplicate_var.urus
|   |   |-- immutable.urus
|   |   |-- type_mismatch.urus
|   |   |-- undefined_var.urus
|   |   |-- wrong_arg_count.urus
|   |   '-- parser/
|   |       '-- unclosed_brace.urus
|   |
|   |-- run/                    # Compile + run + check output
|   |   |-- hello.urus          + hello.expected
|   |   |-- arithmetic.urus     + arithmetic.expected
|   |   |-- arrays.urus         + arrays.expected
|   |   |-- foreach.urus        + foreach.expected
|   |   |-- functions.urus      + functions.expected
|   |   |-- if_else.urus        + if_else.expected
|   |   |-- loops.urus          + loops.expected
|   |   |-- strings.urus        + strings.expected
|   |   |-- structs.urus        + structs.expected
|   |   '-- variables.urus      + variables.expected
|   |
|   |-- run_tests.sh            # Test runner (Linux/macOS)
|   '-- run_tests.bat           # Test runner (Windows)
|
|-- documentation/              # Full project documentation
|   |-- overview/
|   |-- architecture/
|   |-- installation/
|   |-- usage/
|   |-- configuration/
|   |-- api-reference/
|   |-- development-guide/
|   |-- security/
|   |-- roadmap/
|   |-- changelog/
|   |-- diagrams/
|   '-- decisions/              # Architectural Decision Records
|
|-- SPEC.md                     # Language specification & grammar
|-- CHANGELOG.md                # Version history
|-- CATATANRASYA.md             # Development notes (v1.0.1)
|-- CODE_OF_CONDUCT.md          # Community guidelines
|-- CONTRIBUTING.md             # Contribution guide
|-- SECURITY.md                 # Security policy
|-- Dockerfile                  # Container build
'-- LICENSE                     # Apache 2.0
```

---

## Comparison

| Feature | URUS | C | Rust | Go | Python |
|---------|:----:|:-:|:----:|:--:|:------:|
| Static typing | Yes | Yes | Yes | Yes | No |
| Memory safety | RC + bounds | Manual | Ownership | GC | GC |
| Pattern matching | Yes | No | Yes | No | Limited |
| String interpolation | Yes | No | No | No | Yes |
| Result type | Yes | No | Yes | No | No |
| Null safety | Yes | No | Yes | No | No |
| Compiles to native | Yes | Yes | Yes | Yes | No |
| Learning curve | Low | Medium | High | Low | Low |
| Zero dependencies | Yes | Yes | No | No | No |

---

## Roadmap

### v1.1.0 — Quality of Life
- Default parameter values
- ~~Better error messages with source context~~ *(Done in v1.0.1)*
- Warning system (unused variables, etc.)

### v1.2.0 — Type System
- Type aliases (`type ID = int;`)
- Optional type (`Option<T>`)
- Type inference (`let x = 42;`)

### v2.0.0 — Major Features
- Methods (`impl Point { fn distance() }`)
- Traits / Interfaces
- Generics (`fn max<T>(a: T, b: T): T`)
- Closures
- Standard library
- Package manager

### v3.0.0 — Advanced
- Async/await
- Concurrency
- WASM target
- Self-hosting compiler
- LSP server for IDE support

Full roadmap: [documentation/roadmap](./documentation/roadmap/)

---

## Inspiration

URUS draws inspiration from:

- **Rust** — enums, pattern matching, Result type, immutability by default
- **Go** — simplicity, fast compilation, clean syntax
- **Zig** — transpile-to-C philosophy, minimal runtime
- **Python** — f-string interpolation, readability

---

## Contributing

```
1. Fork the repo
2. Create a feature branch
3. Follow coding style (see documentation/development-guide)
4. Add tests
5. Update documentation
6. Submit PR
```

See [CONTRIBUTING.md](./CONTRIBUTING.md) for details.

---

## License

Apache License 2.0 — see [LICENSE](./LICENSE)

---

<p align="center">
  <strong>Built with care. Designed for clarity.</strong>
  <br><br>
  <a href="./documentation/">Documentation</a> &nbsp;&bull;&nbsp;
  <a href="./examples/">Examples</a> &nbsp;&bull;&nbsp;
  <a href="./SPEC.md">Specification</a> &nbsp;&bull;&nbsp;
  <a href="./CHANGELOG.md">Changelog</a>
</p>
