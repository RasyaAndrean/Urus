# URUS Documentation

Complete documentation for URUS Programming Language V0.2/3(A).

## Table of Contents

| Section | Description |
|---------|-------------|
| [Overview](./overview/) | Project goals, target users, high-level architecture |
| [Architecture](./architecture/) | Compiler pipeline, module dependencies, data flow |
| [Installation](./installation/) | How to install GCC, CMake, and build the compiler |
| [Usage](./usage/) | CLI options, language examples, built-in functions |
| [Configuration](./configuration/) | Compiler flags, build options, import resolution |
| [API Reference](./api-reference/) | Compiler C API and runtime API |
| [Development Guide](./development-guide/) | Coding standards, branching, testing, adding features |
| [Security](./security/) | Memory safety, compiler security, best practices |
| [Roadmap](./roadmap/) | Version history, planned features, milestones |
| [Changelog](./changelog/) | Detailed version-by-version changes |
| [Diagrams](./diagrams/) | Technical diagrams and visual references |
| [Decisions](./decisions/) | Architectural Decision Records (ADRs) |

## Quick Links

- [Language Specification](../SPEC.md)
- [Contributing Guide](../CONTRIBUTING.md)
- [Security Policy](../SECURITY.md)
- [License](../LICENSE)
- [Development Notes (Diary)](../Diary/)

## Versioning

URUS uses a custom versioning format: `V{major}.{minor}/{patch}(tag)`

| Tag | Meaning |
|-----|---------|
| `(F)` | Fixed — bug fixes |
| `(A)` | Added — new features |
| `(U)` | Updated — improvements |
| `(P)` | Patched — small patches |
| `(B)` | Beta — testing version |
| `(R)` | Release — stable |
| `(E)` | Experimental — experimental features |
| `(D)` | Dev — development version |

Current version: **V0.2/3(A) "Added"**

## Getting Started

```bash
# 1. Clone
git clone https://github.com/Urus-Foundation/Urus.git
cd Urus/compiler

# 2. Build
cmake -S . -B build
cmake --build build --config Release

# 3. Run
./build/Release/urusc hello.urus -o hello    # Windows
./build/urusc hello.urus -o hello            # Linux/macOS

# 4. Test
cd ../tests
bash run_tests.sh ../compiler/build/urusc    # Linux/macOS
run_tests.bat ..\compiler\build\Release\urusc.exe  # Windows
```

## Community

- **GitHub:** https://github.com/Urus-Foundation/Urus
- **Issues:** https://github.com/Urus-Foundation/Urus/issues
- **Sponsor:** https://www.patreon.com/15684319
- **Complaints & Feedback:** See [Diary/COMPLAINTS.md](../Diary/COMPLAINTS.md)
