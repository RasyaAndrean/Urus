# Roadmap

## Version History

| Version | Status | Date | Highlight |
|---------|--------|------|-----------|
| V0.1 | Released | 2025 | Initial prototype — lexer, parser, basic codegen |
| V0.2/1 | Released | 2026-03-02 | Enums, match, imports, Result, f-strings, refcounting |
| V0.2/2(F) | Released | 2026-03-09 | CMake, standalone compiler, error diagnostics, cross-platform fixes |
| V0.2/3(A) | Released | 2026-03-17 | Struct spread syntax, numeric separators, bug fixes (#27,#42,#43,#47,#48,#49,#50) |

## Versioning Scheme

URUS uses a custom versioning format: `V{major}.{minor}/{patch}(tag)`

| Part | Meaning |
|------|---------|
| `V0` | Pre-stable (major) |
| `.2` | Feature set (minor) |
| `/2` | Patch within that feature set |
| `(X)` | Tag — describes the nature of the release |

### Tags

| Tag | Name | Description |
|-----|------|-------------|
| `(F)` | Fixed | Bug fixes |
| `(A)` | Added | New features added |
| `(U)` | Updated | Changes / improvements |
| `(P)` | Patched | Small patches |
| `(B)` | Beta | Testing version |
| `(R)` | Release | Stable release |
| `(E)` | Experimental | Experimental features |
| `(D)` | Dev | Development version |

### Release Stages

| Stage | Description |
|-------|-------------|
| Alpha | Early development, unstable |
| Beta | Feature-complete, under testing |
| RC | Release Candidate — final testing before stable |
| Stable | Production-ready |

### Examples

| Version | Meaning |
|---------|---------|
| `V0.2/1` | Feature set 2, first patch (no tag) |
| `V0.2/2(F)` | Feature set 2, second patch — bug fixes |
| `V0.2/3(A)` | Feature set 2, third patch — new features |
| `V0.3/1(B)` | Feature set 3, first patch — beta |
| `V1.0/1(R)` | Stable release |

---

## Current: V0.2/3(A) "Added"

Features included:
- Primitive types: `int`, `float`, `bool`, `str`, `void`
- Variables (immutable by default, `mut` for mutable)
- Functions
- Structs with functional update (spread) syntax: `Type { field: val, ..source }`
- Arrays (dynamic, growable)
- Enums / tagged unions
- Pattern matching (`match`)
- String interpolation (`f"..."`)
- Modules / imports
- For-each loops (`for item in array { ... }`)
- Error handling (`Result<T, E>`, `Ok`, `Err`)
- Numeric separators: `1_000_000`, `3.14_159`
- Reference counting memory management
- File I/O
- Full test suite (14 run, 11 invalid, 2 valid)
- CMake build system
- Standalone compiler (runtime embedded)
- Rich error diagnostics with accurate line/column numbers

---

## Planned: V0.3/1 — Quality of Life

**Target:** Stability and developer experience

| Feature | Description | Priority |
|---------|-------------|----------|
| Default parameter values | `fn foo(x: int = 10)` | Medium |
| Multi-line string literals | Triple-quote `"""..."""` | Medium |
| Warning system | Non-fatal warnings (unused vars, etc.) | Medium |
| Const expressions | `const PI: float = 3.14159;` | Low |

---

## Planned: V0.4/1 — Type System

**Target:** More expressive type system

| Feature | Description | Priority |
|---------|-------------|----------|
| Type aliases | `type ID = int;` | Medium |
| Optional type | `Option<T>` (sugar for `Result<T, void>`) | High |
| Tuple types | `(int, str)` — anonymous product types | Medium |
| Type inference | `let x = 42;` (infer `int`) | High |

---

## Planned: V0.5/1 — Methods & Traits

**Target:** Object-oriented capabilities

| Feature | Description | Priority |
|---------|-------------|----------|
| Methods | `impl Point { fn distance(...) }` | High |
| Traits/Interfaces | `trait Printable { fn to_string(): str; }` | High |
| Generics | `fn max<T>(a: T, b: T): T` | High |
| Closures | `let f = \|x\| x * 2;` | Medium |

---

## Planned: V1.0/1 — Stable Release

**Target:** Production-ready language

| Feature | Description | Priority |
|---------|-------------|----------|
| Standard library | `std.collections`, `std.io`, `std.math` | High |
| Package manager | Download & manage dependencies | Medium |
| Full documentation | Complete language guide | High |
| Stability guarantee | No breaking changes in V1.x | High |

---

## Future: V2.0/1 — Advanced

| Feature | Description |
|---------|-------------|
| Async/await | Asynchronous I/O |
| Concurrency | Lightweight threads / goroutine-style |
| Cycle detection | Detect ref-count cycles at runtime |
| Compile-time evaluation | `comptime` blocks |
| WASM target | Compile to WebAssembly |
| Self-hosting | Compiler written in URUS |
| LSP server | Language Server Protocol for IDE support |
| Debugger integration | Source-level debugging |

---

## Milestones

```
2025 Q4  ── V0.1 prototype (done)
2026 Q1  ── V0.2/1 core features (done)
2026 Q1  ── V0.2/2(F) bug fixes (done)
2026 Q1  ── V0.2/3(A) spread syntax, numeric separators, bug fixes (done)
2026 Q2  ── V0.3/1 quality of life
2026 Q3  ── V0.4/1 type system
2026 Q4  ── V0.5/1 methods & traits
2027 H1  ── V1.0/1 stable release
2027 H2  ── V1.x standard library & tooling
2028     ── V2.0/1 async, concurrency, WASM
```

## Contributing

Want to contribute? See the [Development Guide](../development-guide/README.md) for:
- Coding standards
- Branch strategy
- Testing guidelines
- How to add new features
