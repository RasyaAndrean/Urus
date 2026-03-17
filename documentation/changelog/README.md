# Changelog

See also: [CHANGELOG.md](../../CHANGELOG.md) in the project root.

## Format

URUS uses a custom versioning format: `V{major}.{minor}/{patch}(tag)`

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

---

## V0.2/3(A) — 2026-03-17

### Added
- **Struct Functional Update (Spread) Syntax** — `Point { x: 10.0, ..p1 }` creates a new struct by copying fields from an existing instance and overriding specific fields (#47)
- **Numeric Separator** — underscores in integer and float literals for readability: `1_000_000`, `3.14_159` (#49)
- Termux (Android) build instructions in README

### Fixed
- **Parser infinite loop** on nested struct/enum errors — added `if (p->had_error) break;` recovery in struct and enum declaration parsing loops (#27)
- **Empty struct literal** — `Abc{}` no longer produces confusing error, parser now handles empty struct literals correctly (#42)
- **String `+=` codegen** — `s += "text"` now correctly generates `urus_str_concat()` instead of invalid C pointer arithmetic (#43)
- **Trailing decimal float** — lexer now accepts `20.` as a valid float literal while preserving the range operator `0..10` (#48)
- **Garbage column numbers in sema errors** — `lexer_init()` now initializes `line_start` to 0, fixing garbage values in f-string sub-lexer token positions (#50)
- Undefined function call causing segfault (PR #45)
- More accurate semantic error messages (PR #44)
- Default output name based on platform: `a.out` (Linux), `a.exe` (Windows) (PR #39)
- Version mismatch in Dockerfile
- Removed `inline` from `urus_str_equal` — GCC `-O2` already handles inlining

### Changed
- Removed `assets/` folder from main repo (moved to `Urus-Foundation/initial-resource`)
- Editor support separated into `Urus-Foundation/editor-support` repository
- GitHub issue templates standardized to English (PR #38)

### Contributors
- **fepfitra** — issue reports (#27, #42, #43, #47, #48, #49, #50), default output name fix (PR #39)
- **John-fried** — segfault fix (PR #45), error accuracy (PR #44), assets removal (PR #40), issue templates (PR #38)
- **RasyaAndrean** — bug fixes, feature implementation, documentation

---

## V0.2/2(F) — 2026-03-09

### Fixed
- Replace POSIX `getline()` with portable `fgets()` for MSVC compatibility
- Fix double CRLF corruption on Windows — use binary mode (`"wb"`) for generated C files
- Fix `--help` and `--version` flags being treated as filenames
- Fix GCC `cc1` not found on Windows — inject GCC bin dir into PATH
- Remove obsolete `-I include` flag (runtime now embedded)
- Suppress MSVC `strdup` deprecation warnings

### Added
- `--version` / `-v` CLI flag
- Colored error reporting with source context and caret pointer (PR #7)
- Compiler is now standalone — runtime embedded in binary (PR #6)
- CMake build system replacing Makefile + build.bat (PR #5)
- `install` / `uninstall` targets (PR #3)
- `show_help()` function in CLI (PR #4)
- Linux compatibility fix for `urus_runtime.h` (PR #2)

### Changed
- Build system migrated from Makefile to CMake
- Version scheme changed from semver to `V{major}.{minor}/{patch}(tag)`
- Error reporting now shows filename, line, source code, and caret pointer

---

## V0.2/1 — 2026-03-02

### Added
- **Enums / Tagged Unions** — `enum Shape { Circle(r: float); Point; }`
- **Pattern Matching** — `match expr { Variant(x) => { ... } }`
- **String Interpolation** — `f"Hello {name}, count={count}"`
- **Modules / Imports** — `import "utils.urus";`
- **Error Handling** — `Result<T, E>`, `Ok(val)`, `Err(msg)`, `is_ok`, `is_err`, `unwrap`, `unwrap_err`
- **For-each Loops** — `for item in array { ... }`
- **Reference Counting** — `retain`/`release` for str, array
- **Array type support** — `[float]`, `[bool]`, `[str]` (previously only `[int]`)
- **Array index assignment** — `nums[i] = value;`
- **Built-in functions** — `to_int()`, `to_float()`, bounds checking
- **Break/continue validation** — error if outside a loop
- **File restructuring** — `compiler/src/` and `compiler/include/`
- **Test suite** — valid, invalid, and run tests with test runner
- **Documentation** — README, SPEC, CHANGELOG, and documentation/ folder
- **Examples** — 9 example programs (hello, fibonacci, structs, arrays, enums, strings, result, files, modules)
- **License** — Apache 2.0

### Fixed
- `ast_type_str` static buffer clobber (round-robin 4 buffers)
- `urus_str_replace` unsigned underflow (use `ptrdiff_t`)
- GCC statement expressions `({...})` replaced with temp variable pattern (standard C11)
- Makefile missing `sema.c codegen.c` and `-lm`
- Dead ternary in temp file path
- Array codegen only supported `int` (now all types)
- Array index assignment generated invalid C lvalue

### Changed
- Compiler output is now standard C11 (no GCC extensions required)
- Enum/struct/array literals use temp variable pattern

---

## V0.1 — 2025

### Added
- Initial compiler: lexer, parser, sema, codegen
- Primitive types: `int`, `float`, `bool`, `str`, `void`
- Variables (immutable by default, `mut`)
- Functions
- Structs
- Arrays (`[int]` only)
- Control flow: `if/else`, `while`, `for..in` range
- Operators: arithmetic, comparison, logical, assignment
- Built-in: `print`, `len`, `push`, `to_str`
- String operations: `str_len`, `str_upper`, `str_lower`, `str_trim`, `str_contains`, `str_slice`, `str_replace`
- File I/O: `read_file`, `write_file`, `append_file`
- Comments: `//` and `/* */`
- Header-only runtime (`urus_runtime.h`)
