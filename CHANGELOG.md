# Changelog

## v1.0.1(F) "Fixed" (2026-03-09)

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

## v1.0.0 (2026-03-02)

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

## v0.1 (2025-03-01)

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
