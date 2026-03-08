![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Version](https://img.shields.io/badge/version-1.0.0-blue)
![License](https://img.shields.io/badge/license-Apache%202.0-green)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)
![C Standard](https://img.shields.io/badge/C-C11-orange)
![Status](https://img.shields.io/badge/status-stable-brightgreen)

<div align="center">

# URUS Programming Language

**A statically-typed, compiled programming language that transpiles to portable C11.**

Safe. Simple. Fast.

[Quick Start](#quick-start) | [Documentation](./documentation/) | [Examples](./examples/) | [Roadmap](#roadmap)

</div>

---

## Language Status

URUS v1.0.0 is **stable**. The core language syntax and features are finalized. Breaking changes will only occur in major version bumps following [Semantic Versioning](https://semver.org/).

## Why URUS?

| Goal | How |
|------|-----|
| **Safer than C** | Reference counting, bounds checking, immutable by default |
| **Simpler than Rust** | No borrow checker, no lifetimes — just write code |
| **Faster than Python** | Compiles to native binary via C11 |
| **More portable than Go** | Transpiles to standard C11 — runs anywhere GCC runs |
| **Modern syntax** | Enums, pattern matching, string interpolation, Result type |

```urus
fn main(): void {
    let name: str = "World";
    print(f"Hello, {name}!");

    let nums: [int] = [1, 2, 3, 4, 5];
    let mut sum: int = 0;
    for i in 0..len(nums) {
        sum += nums[i];
    }
    print(f"Sum: {sum}");
}
```

## Project Stats

| Metric | Value |
|--------|-------|
| Current Version | 1.0.0 |
| Compiler Size | ~464 KB |
| Runtime Size | ~16 KB (header-only) |
| Compiler LOC | ~4,200 |
| Runtime LOC | ~430 |
| Generated Output | C11 compliant |
| Supported Platforms | Windows, Linux, macOS |
| Dependencies | GCC only |

## Architecture

```
Source (.urus)
     |
     v
  [ Lexer ]       tokenize source code
     |
     v
  [ Parser ]      build Abstract Syntax Tree
     |
     v
  [ Sema ]        type checking & semantic analysis
     |
     v
  [ Codegen ]     generate standard C11 code
     |
     v
  [ GCC ]         compile to native binary
     |
     v
  Executable
```

Full architecture docs: [documentation/architecture](./documentation/architecture/)

## Quick Start

### Install

**Requirements:** GCC 8+ (MinGW-w64 / MSYS2 on Windows)

```bash
# Clone
git clone https://github.com/RasyaAndrean/urus.git
cd urus/compiler

# Build (Linux / macOS)
make

# Build (Windows)
build.bat
```

### Prebuilt Binary

> Coming soon — check [Releases](https://github.com/RasyaAndrean/urus/releases) page.

### Hello World

```urus
fn main(): void {
    print("Hello, World!");
}
```

```bash
urusc hello.urus -o hello
./hello
# Output: Hello, World!
```

## Features

### Types

| Type | Description |
|------|-------------|
| `int` | 64-bit signed integer |
| `float` | 64-bit floating point |
| `bool` | Boolean (`true` / `false`) |
| `str` | UTF-8 string (ref-counted) |
| `void` | No value |
| `[T]` | Dynamic array of T |
| `Result<T, E>` | Ok or Err value |

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
if x > 10 {
    print("big");
} else {
    print("small");
}

while x < 100 {
    x += 1;
}

for i in 0..10 {       // exclusive: 0-9
    print(f"{i}");
}

for i in 0..=10 {      // inclusive: 0-10
    print(f"{i}");
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

### Modules

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

### Error Handling

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

### Built-in Functions

| Category | Functions |
|----------|-----------|
| **I/O** | `print`, `read_file`, `write_file`, `append_file` |
| **Array** | `len`, `push` |
| **String** | `str_len`, `str_upper`, `str_lower`, `str_trim`, `str_contains`, `str_slice`, `str_replace` |
| **Conversion** | `to_str`, `to_int`, `to_float` |
| **Math** | `sqrt`, `abs` |
| **Result** | `is_ok`, `is_err`, `unwrap`, `unwrap_err` |

## CLI Usage

```
usage: urusc <file.urus> [options]

Rust-like safety with Python-like simplicity, transpiling to C11

Options:
  --tokens    Display Lexer tokens
  --ast       Display the Abstract Syntax Tree (AST)
  --emit-c    Print generated C code to stdout
  -o <file>   Specify output executable name (default to: a.exe)

Example:
  `urusc main.urus -o app`
```

## Running Tests

```bash
cd tests/

# Linux / macOS
bash run_tests.sh ../compiler/urusc

# Windows
run_tests.bat ..\compiler\urusc.exe
```

## Repository Structure

```
.
├── compiler/
│   ├── src/             # Compiler source (.c)
│   ├── include/         # Headers (.h) + runtime
│   ├── Makefile         # Linux/macOS build
│   └── build.bat        # Windows build
├── examples/            # Sample URUS programs
│   ├── hello.urus
│   ├── fibonacci.urus
│   ├── structs.urus
│   ├── arrays.urus
│   ├── enums.urus
│   ├── strings.urus
│   ├── result.urus
│   ├── files.urus
│   └── modules/         # Multi-file import example
├── tests/
│   ├── valid/           # Should compile
│   ├── invalid/         # Should be rejected
│   ├── run/             # Compile + run + check output
│   ├── run_tests.sh     # Test runner (Linux/macOS)
│   └── run_tests.bat    # Test runner (Windows)
├── documentation/       # Full project documentation
│   ├── overview/
│   ├── architecture/
│   ├── installation/
│   ├── usage/
│   ├── configuration/
│   ├── api-reference/
│   ├── development-guide/
│   ├── security/
│   ├── roadmap/
│   ├── changelog/
│   ├── diagrams/
│   └── decisions/       # Architectural Decision Records
├── SPEC.md              # Language specification
├── CHANGELOG.md         # Version history
├── CODE_OF_CONDUCT.md   # Community guidelines
├── CONTRIBUTING.md      # Contribution guide
├── SECURITY.md          # Security policy
└── LICENSE              # Apache 2.0
```

## Comparison

| Feature | URUS | C | Rust | Go | Python |
|---------|------|---|------|----|--------|
| Static typing | Yes | Yes | Yes | Yes | No |
| Memory safety | RC + bounds check | Manual | Ownership | GC | GC |
| Pattern matching | Yes | No | Yes | No | Limited |
| String interpolation | Yes | No | No | No | Yes |
| Result type | Yes | No | Yes | No | No |
| Null safety | Yes (no null) | No | Yes | No | No |
| Compiles to native | Yes (via C) | Yes | Yes | Yes | No |
| Learning curve | Low | Medium | High | Low | Low |
| Zero dependencies | Yes | Yes | No (LLVM) | No | No |

## Roadmap

### v1.1.0 — Quality of Life
- Default parameter values
- Better error messages with source context
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

## Contributing

Pull requests are welcome.

```
1. Fork the repo
2. Create a feature branch
3. Follow coding style (see documentation/development-guide)
4. Add tests
5. Update documentation
6. Submit PR
```

See [Development Guide](./documentation/development-guide/) for coding standards, branch strategy, and testing guidelines.

## Inspiration

URUS draws inspiration from:

- **Rust** — enums, pattern matching, Result type, immutability by default
- **Go** — simplicity, fast compilation, clean syntax
- **Zig** — transpile-to-C philosophy, minimal runtime
- **Python** — f-string interpolation, readability

## License

Apache License 2.0 — see [LICENSE](LICENSE)

---

<div align="center">

**Built with care. Designed for clarity.**

[Documentation](./documentation/) | [Examples](./examples/) | [Specification](./SPEC.md) | [Changelog](./CHANGELOG.md)

</div>
