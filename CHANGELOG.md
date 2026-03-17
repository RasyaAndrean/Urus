# Changelog

## V0.2/3(A) "Added" (2026-03-17)

### New Features
- **Struct Spread Syntax**: `Point { x: 10.0, ..p1 }` — create a new struct by copying fields from an existing instance and selectively overriding fields (#47)
- **Numeric Separators**: `1_000_000`, `3.14_159` — underscores as visual separators in integer and float literals (#49)

### Bug Fixes
- Fixed parser infinite loop on nested struct/enum with syntax errors — added error recovery breaks (#27)
- Fixed empty struct literal `Abc{}` producing confusing error — parser now handles it correctly (#42)
- Fixed string `+=` codegen — now generates `urus_str_concat()` instead of invalid C pointer arithmetic (#43)
- Fixed trailing decimal float `20.` not accepted by lexer — changed condition to allow floats without fractional digits (#48)
- Fixed garbage column numbers in sema error messages — `lexer_init()` was not initializing `line_start` field (#50)
- Fixed undefined function call causing segfault (PR #45)
- Fixed more accurate semantic error messages (PR #44)
- Fixed default output name based on platform: `a.out` (Linux), `a.exe` (Windows) (PR #39)
- Fixed version mismatch in Dockerfile
- Removed `inline` from `urus_str_equal` — GCC `-O2` already handles inlining

### Improvements
- Added Termux (Android) build instructions in README
- Editor support separated into dedicated repo: `Urus-Foundation/editor-support`
- Assets and diary moved to `Urus-Foundation/initial-resource`
- GitHub issue templates standardized to English (PR #38)

### Contributors
- **fepfitra** — issue reports (#27, #42, #43, #47, #48, #49, #50), default output name fix (PR #39)
- **John-fried** — segfault fix (PR #45), error accuracy (PR #44), assets removal (PR #40), issue templates (PR #38)
- **RasyaAndrean** — bug fixes, feature implementation, documentation, repo management

---

## V0.2/2(F) "Fixed" (2026-03-09)

### Build System
- Migrated from Makefile/build.bat to **CMake** for cross-platform portability
- Added `cmake/embed-string.cmake` to embed runtime header into the compiler binary

### Bug Fixes
- Fixed missing `stddef.h` include in `urus_runtime.h` (Linux compatibility)
- Fixed implicit declaration of POSIX functions in C11 mode
- Fixed unterminated string in `emit()` codegen function
- Compiler is now **standalone** — `urus_runtime.h` is embedded into the binary, no external runtime file needed
- Fixed `error.c` using POSIX `getline()` — replaced with portable `fgets()` for MSVC/Windows compatibility
- Fixed `_urus_tmp.c` double CRLF corruption on Windows — temp file now written in binary mode
- Fixed `--help`/`--version` flags not recognized (was treated as filename)
- Fixed GCC `cc1` not found on Windows — compiler now injects GCC bin directory into PATH
- Removed obsolete `-I include` flag from GCC invocation (runtime is now embedded)
- Added MSVC compatibility defines (`_CRT_SECURE_NO_WARNINGS`, `_CRT_NONSTDC_NO_DEPRECATE`)

### Improvements
- Added `show_help()` CLI usage with `--help` and `-h` flags
- Added `--version` / `-v` flag to display compiler version
- Rich **error diagnostics**: colored output with filename, line number, column caret (^) pointer
- Error reporting integrated into both **parser** and **semantic analysis**
- Added `install` and `uninstall` targets via CMake
- Updated installation documentation for CMake workflow
- Added parser error test case (`tests/invalid/parser/unclosed_brace.urus`)
- Dockerfile updated to use CMake build and org URL updated

### Contributors
- **John-fried** — Linux fixes, CMake migration, standalone compiler, error logging (PR #2-#7)
- **RasyaAndrean** — Project maintenance, PR reviews

---

## V0.2/1 (2026-03-02)

### New Features
- **Enums / Tagged Unions**: `enum Shape { Circle(r: float); Rect(w: float, h: float); Point; }`
- **Pattern Matching**: `match` statement with variant bindings
- **Modules / Imports**: `import "module.urus";` with circular import detection
- **Error Handling**: `Result<T, E>` type with `Ok(val)` / `Err(msg)`, `is_ok()`, `is_err()`, `unwrap()`, `unwrap_err()`
- **String Interpolation**: `f"Hello {name}, age {age}"` desugars to to_str + concat
- **For-each Loops**: `for item in array { ... }` — iterate over array elements
- **Conversion Functions**: `to_int()` and `to_float()` now fully implemented

### Bug Fixes
- Fixed Makefile missing sema.c and codegen.c
- Fixed `ast_type_str` static buffer clobber (round-robin buffers)
- Fixed `urus_str_replace` unsigned underflow with signed diff
- Fixed array codegen removing GCC statement expressions (standard C11)
- Fixed array element types (now supports `[float]`, `[str]`, `[bool]`, `[MyStruct]`)
- Fixed array index assignment generating invalid C lvalue
- Fixed temp file path dead ternary
- Added bounds checking on array access
- Break/continue now validated to be inside loops

### Improvements
- Reference counting: `retain`/`release` functions for str, array
- Larger emit buffer (4096 from 2048)
- `to_str` now retains the string (proper refcounting)
- Empty function params emit `void` in C for correctness
- Version string in compiler output

---

## V0.1 (2025-03-01)

### Initial Release
- Primitive types: int, float, bool, str, void
- Variables with `let` / `let mut`, mandatory type annotation
- Functions with typed parameters and return types
- Control flow: if/else, while, for (range-based), break, continue
- Operators: arithmetic, comparison, logical, assignment
- Structs (declaration, literal creation, field access)
- Arrays (literal, indexing, len, push)
- String concatenation with `+`
- Comments (single-line `//`, multi-line `/* */`)
- 30+ built-in functions (string ops, math, file I/O, assert)
- Transpiles to C via GCC
