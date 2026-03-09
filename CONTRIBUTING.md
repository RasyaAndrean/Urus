# Contributing to URUS

Thank you for your interest in contributing to URUS! This guide will help you get started.

## Getting Started

1. **Fork** the repository on GitHub
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/<your-username>/urus.git
   cd urus
   ```
3. **Build** the compiler:
   ```bash
   cd compiler
   cmake -S . -B build
   cmake --build build
   ```
4. **Run tests** to make sure everything works:
   ```bash
   cd tests
   bash run_tests.sh ../compiler/urusc
   ```

## How to Contribute

### Reporting Bugs

- Search [existing issues](https://github.com/Urus-Foundation/Urus/issues) first to avoid duplicates
- Open a new issue with:
  - URUS version (`urusc --help` shows version)
  - OS and GCC version
  - Minimal `.urus` file that reproduces the bug
  - Expected vs actual behavior
  - Error message (if any)

### Suggesting Features

- Open an issue with the `enhancement` label
- Describe the feature and its use case
- If possible, show example URUS syntax for the proposed feature

### Submitting Code

1. **Create a branch** from `dev`:
   ```bash
   git checkout dev
   git pull origin dev
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes** following the [coding standards](#coding-standards)

3. **Add tests:**
   - `tests/valid/` — programs that should compile
   - `tests/invalid/` — programs that should produce errors
   - `tests/run/` — programs with expected output (`.urus` + `.expected`)

4. **Run the test suite:**
   ```bash
   cd tests
   bash run_tests.sh ../compiler/urusc
   ```

5. **Commit** with a clear message:
   ```bash
   git commit -m "Add: short description of changes"
   ```

6. **Push** and open a Pull Request:
   ```bash
   git push -u origin feature/your-feature-name
   ```

## Coding Standards

### C Code (Compiler)

- **Standard:** C11 (`-std=c11`)
- **Style:** K&R braces, 4-space indentation
- **Naming:** `snake_case` for everything
- **Prefixes:** `lexer_`, `parser_`, `sema_`, `codegen_`, `ast_`, `urus_`
- **Compile clean:** No warnings with `-Wall -Wextra`

### Commit Messages

```
<type>: <short description>
```

| Type | Usage |
|------|-------|
| `Add` | New feature |
| `Fix` | Bug fix |
| `Update` | Change to existing feature |
| `Refactor` | Internal change, no behavior change |
| `Docs` | Documentation only |
| `Test` | New or updated tests |

### Adding a New Language Feature

If you're adding a new language feature, follow this checklist:

1. `compiler/include/token.h` — Add token (if new keyword/operator)
2. `compiler/src/lexer.c` — Recognize token
3. `compiler/include/ast.h` + `compiler/src/ast.c` — Add AST node type
4. `compiler/src/parser.c` — Parse new syntax
5. `compiler/src/sema.c` — Type-check
6. `compiler/src/codegen.c` — Generate C code
7. `compiler/include/urus_runtime.h` — Add runtime support (if needed)
8. `tests/` — Add test cases
9. `SPEC.md` — Update language spec
10. `examples/` — Add example program

See the [Development Guide](./documentation/development-guide/) for more details.

## Pull Request Guidelines

- Target the `dev` branch (not `main`)
- Keep PRs focused — one feature or fix per PR
- Include tests for new functionality
- Update documentation if the change affects user-facing behavior
- Make sure all existing tests pass before submitting
- Describe what your PR does and why in the PR description

## Project Structure

```
compiler/
  src/          # C source files
  include/      # Header files + runtime
tests/
  valid/        # Should compile without errors
  invalid/      # Should produce compile errors
  run/          # Compile, run, check output
examples/       # Example URUS programs
documentation/  # Full project docs
```

## Need Help?

- Read the [Documentation](./documentation/)
- Check the [Language Spec](SPEC.md)
- Look at [Examples](./examples/) for syntax reference
- Open an issue if you're stuck

## License

By contributing, you agree that your contributions will be licensed under the [Apache License 2.0](./LICENSE).
