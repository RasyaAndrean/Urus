# Development Guide

## Coding Standard

### C Language (Compiler)

- **Standard:** C11 (`-std=c11`)
- **Naming:** `snake_case` for all functions and variables
- **Prefix:** All public APIs use a module prefix: `lexer_`, `parser_`, `sema_`, `codegen_`, `ast_`, `error_`
- **Runtime prefix:** `urus_` for all runtime functions
- **Indentation:** 4 spaces (not tabs)
- **Max line length:** ~100 characters (soft limit)
- **Braces:** K&R style (opening brace on the same line)

```c
// Correct style example
void lexer_next(Lexer *l) {
    if (l->pos >= l->len) {
        return;
    }
    // ...
}
```

### Memory Rules

- All heap allocations must have a matching free
- String literals in AST: duplicate with `strdup()`, free in `ast_free()`
- Runtime strings: ref-counted via `urus_str_retain/release`
- Static buffers: use round-robin to avoid clobber (see `ast_type_str`)
- On Windows: use `fopen("wb")` for generated C files to prevent CRLF issues

### Error Handling in the Compiler

- Fatal errors: print to stderr with source context, then `exit(1)`
- Error format: `filename:line: Error: message` (with ANSI color codes)
- Error display includes: source line, caret pointer to exact column
- No error recovery — compiler stops at the first error

## Branch Strategy

```
main          <-- stable, released versions
  |
  +-- dev     <-- development branch (public contributions)
  |   |
  |   +-- feature/enums
  |   +-- feature/imports
  |   +-- fix/array-bounds
  |
  +-- release/V0.3-1
```

| Branch | Purpose |
|--------|---------|
| `main` | Stable release, always compilable |
| `dev` | Integration branch for new features and public contributions |
| `feature/*` | One feature per branch |
| `fix/*` | Bug fixes |
| `release/*` | Release preparation |

## How to Create a Pull Request

1. **Fork the repo** on GitHub

2. **Create a branch** from `dev`:
   ```bash
   git checkout dev
   git pull
   git checkout -b feature/feature-name
   ```

3. **Develop & commit:**
   ```bash
   git add compiler/src/file.c compiler/include/file.h
   git commit -m "feat: short description of changes"
   ```

4. **Build & test:**
   ```bash
   cd compiler
   cmake -S . -B build
   cmake --build build --config Release
   cd ../tests
   # Linux/macOS
   bash run_tests.sh ../compiler/build/urusc
   # Windows
   run_tests.bat ..\compiler\build\Release\urusc.exe
   ```

5. **Push & create PR:**
   ```bash
   git push -u origin feature/feature-name
   # Create PR via GitHub UI targeting `dev` branch
   ```

### Commit Message Convention

```
<type>: <short description>

<optional body>
```

| Type | Description |
|------|-------------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation |
| `refactor` | Internal changes without behavior change |
| `test` | New tests or test updates |
| `chore` | Build, CI, or tooling changes |

## Testing Guideline

### Test Structure

```
tests/
+-- valid/       # Programs that must compile without errors
+-- invalid/     # Programs that must produce compile errors
+-- run/         # Programs that are compiled, run, and output checked
|   +-- test.urus
|   +-- test.expected
+-- run_tests.sh   # Test runner (Linux/macOS)
+-- run_tests.bat  # Test runner (Windows)
```

### Test Types

| Type | Folder | Pass Condition |
|------|--------|----------------|
| Valid | `valid/` | `urusc --emit-c file.urus` exits 0 |
| Invalid | `invalid/` | `urusc --emit-c file.urus` exits non-0 |
| Run | `run/` | Program output matches `.expected` exactly |

### Adding New Tests

**Valid test:**
```bash
echo 'fn main(): void { print("ok"); }' > tests/valid/new_test.urus
```

**Invalid test:**
```bash
echo 'fn main(): void { let x: int = "string"; }' > tests/invalid/type_error.urus
```

**Run test:**
```bash
# 1. Create the program
cat > tests/run/new_test.urus << 'EOF'
fn main(): void {
    print("expected output");
}
EOF

# 2. Create the expected output
echo "expected output" > tests/run/new_test.expected
```

### Running Tests

```bash
cd tests/

# Linux/macOS
bash run_tests.sh ../compiler/build/urusc

# Windows
run_tests.bat ..\compiler\build\Release\urusc.exe
```

### Current Test Results (V0.2/3(A))

| Category | Count | Status |
|----------|-------|--------|
| Run tests | 14 | All PASS |
| Valid programs | 2 | All PASS |
| Invalid file tests | 11 | All PASS |
| CLI flags | 6 | All functional |

## How to Add a New Feature

### Checklist

1. **Token** — Add new token in `token.h` if a new keyword/operator is needed
2. **Lexer** — Recognize the new token in `lexer.c`
3. **AST** — Add node type in `ast.h`, constructor in `ast.c`
4. **Parser** — Parse new syntax in `parser.c`
5. **Sema** — Type-check in `sema.c`
6. **Codegen** — Generate C code in `codegen.c`
7. **Runtime** — Add runtime support in `urus_runtime.h` if needed
8. **Test** — Add tests in `tests/` (valid, invalid, and run)
9. **Docs** — Update SPEC.md and documentation/
10. **Example** — Add example in `examples/`

### Example: Adding the `**` (Power) Operator

```
1. token.h    -> TOK_POWER
2. lexer.c    -> recognize "**"
3. ast.h      -> (reuse NODE_BINARY, op = "**")
4. parser.c   -> add to precedence table
5. sema.c     -> type-check: numeric operands
6. codegen.c  -> emit pow(a, b)
7. runtime    -> (pow from math.h, already available)
8. tests/     -> valid/power.urus, run/power.urus + .expected
9. SPEC.md    -> add ** to operators
```

## Build from Source

### Prerequisites

- GCC 8.0+ or compatible C compiler
- CMake 3.10+

### Steps

```bash
git clone https://github.com/Urus-Foundation/Urus.git
cd Urus/compiler
cmake -S . -B build
cmake --build build --config Release

# Verify
./build/Release/urusc --version   # Windows
./build/urusc --version           # Linux/macOS
```

## Debugging Tips

- Use `--emit-c` to inspect generated C code
- Use `--tokens` to debug lexer issues
- Use `--ast` to debug parser issues
- Check `_urus_tmp.c` if GCC compilation fails
- On Windows, ensure GCC is installed (MSYS2 recommended)
