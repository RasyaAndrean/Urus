# Installation

## Requirements

| Requirement | Version | Notes |
|-------------|---------|-------|
| GCC | 8.0+ | C compiler (to build urusc and compile output) |
| CMake | 3.10+ | Build system (cross-platform) |
| OS | Windows 10+, Linux, macOS | Any platform with GCC |

### Install GCC

**Windows:**
```bash
# Option 1: MSYS2 (Recommended)
# Download from https://www.msys2.org/
# Then run:
pacman -S mingw-w64-x86_64-gcc make

# Option 2: WinLibs
# Download from https://winlibs.com/
# Extract and add to PATH

# Option 3: Chocolatey
choco install mingw
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt update
sudo apt install gcc cmake
```

**Linux (Fedora/RHEL):**
```bash
sudo dnf install gcc cmake
```

**macOS:**
```bash
# Xcode Command Line Tools (includes gcc/clang)
xcode-select --install

# Or via Homebrew
brew install gcc cmake
```

### Verification

```bash
gcc --version
# gcc (GCC) 12.x.x or newer

cmake --version
# cmake version 3.10.x or newer
```

## Build Compiler

### Cmake build (All Platforms)

```bash
cd compiler/
cmake -S . -B build
cmake --build build
```

Output: `compiler/build/urusc` (or `compiler/build/urusc.exe` in windows)

### Manual Build (All Platforms)

```bash
cd compiler/
gcc -Wall -Wextra -std=c11 -g -Iinclude -o urusc src/main.c src/lexer.c src/ast.c src/parser.c src/util.c src/sema.c src/codegen.c -lm
```

## Setup Environment

### Add to PATH (Optional)

**Linux/macOS** — add to `~/.bashrc` or `~/.zshrc`:
```bash
export PATH="$PATH:/path/to/Urus/compiler"
```

**Windows** — add via System Properties:
1. Open Settings → System → About → Advanced system settings
2. Environment Variables → Path → Edit
3. Add the path to the `compiler\` folder

## Install to your system

### Linux / MacOS
```bash
sudo cmake --install build
```

### Windows (Run As Administrator)
```
cmake --install build
```

## Verify Installation

```bash
urusc --help
```

Output:
```
URUS Compiler v1.0.1(F)
...
```

## Getting Started

### 1. Create a `hello.urus` file

```
fn main(): void {
    print("Hello, World!");
}
```

### 2. Compile and run

```bash
# Compile
urusc hello.urus -o hello

# Run
./hello
# Output: Hello, World!
```

### 3. View generated C (optional)

```bash
urusc --emit-c hello.urus
```

## Troubleshooting

| Error | Solution |
|-------|----------|
| `gcc: command not found` | Install GCC (see above) |
| `urusc: command not found` | Add to PATH or use the full path |
| `undefined reference to 'sqrt'` | Make sure `-lm` is in the compile flags |
| `Permission denied` | `chmod +x urusc` (Linux/macOS) |
