# Usage

## CLI Commands

```
urusc [options] <file.urus>
```

### Options

| Flag | Description |
|------|-------------|
| `--emit-c` | Show generated C on stdout (without compiling) |
| `-o <name>` | Set output binary name |
| `--help` | Show help |

### Examples

```bash
# Compile to binary (default: file name without .urus)
urusc program.urus

# Compile with custom output name
urusc program.urus -o myapp

# View generated C code only
urusc --emit-c program.urus

# View C code and save to file
urusc --emit-c program.urus > output.c
```

## Usage Examples

### Hello World

```urus
fn main(): void {
    print("Hello, World!");
}
```

```bash
$ urusc hello.urus -o hello && ./hello
Hello, World!
```

### Variables & Arithmetic

```urus
fn main(): void {
    let x: int = 10;
    let mut y: int = 20;
    y += x;
    print(f"x = {x}, y = {y}");
}
```

```
x = 10, y = 30
```

### Functions

```urus
fn fibonacci(n: int): int {
    if n <= 1 {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

fn main(): void {
    for i in 0..10 {
        print(f"fib({i}) = {fibonacci(i)}");
    }
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

fn main(): void {
    let a: Point = Point { x: 0.0, y: 0.0 };
    let b: Point = Point { x: 3.0, y: 4.0 };
    print(f"Distance: {distance(a, b)}");
}
```

### Struct Spread (Functional Update)

```urus
struct Config {
    width: int;
    height: int;
    fullscreen: bool;
}

fn main(): void {
    let default_cfg: Config = Config { width: 800, height: 600, fullscreen: false };

    // Create a new Config, copying fields from default_cfg but overriding width
    let wide_cfg: Config = Config { width: 1920, ..default_cfg };
    print(f"Width: {wide_cfg.width}, Height: {wide_cfg.height}");

    // Full copy
    let copy: Config = Config { ..default_cfg };
}
```

### Numeric Separators

```urus
fn main(): void {
    let million: int = 1_000_000;
    let pi: float = 3.14_159;
    let binary_ish: int = 0;  // underscores for visual grouping
    print(f"Million: {million}, Pi: {pi}");
}
```

### Arrays

```urus
fn main(): void {
    let nums: [int] = [10, 20, 30, 40, 50];
    print(f"Length: {len(nums)}");
    print(f"First: {nums[0]}");

    let mut items: [int] = [];
    for i in 1..=5 {
        push(items, i * i);
    }
    print(f"Squares: {len(items)} items");
}
```

### Enums & Pattern Matching

```urus
enum Shape {
    Circle(r: float);
    Rect(w: float, h: float);
    Point;
}

fn describe(s: Shape): str {
    match s {
        Shape.Circle(r) => {
            return f"Circle r={r}";
        }
        Shape.Rect(w, h) => {
            return f"Rect {w}x{h}";
        }
        Shape.Point => {
            return "Point";
        }
    }
    return "Unknown";
}

fn main(): void {
    let s: Shape = Shape.Circle(5.0);
    print(describe(s));
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
    let r: Result<int, str> = divide(10, 3);
    if is_ok(r) {
        print(f"Result: {unwrap(r)}");
    }

    let r2: Result<int, str> = divide(10, 0);
    if is_err(r2) {
        print(f"Error: {unwrap_err(r2)}");
    }
}
```

### String Interpolation

```urus
fn main(): void {
    let name: str = "URUS";
    let version: int = 1;
    print(f"Welcome to {name} v{version}!");
    print(f"2 + 3 = {2 + 3}");
}
```

### Modules / Import

```urus
// math_utils.urus
fn square(x: int): int {
    return x * x;
}
```

```urus
// main.urus
import "math_utils.urus";

fn main(): void {
    print(f"5^2 = {square(5)}");
}
```

### For-each Loop

```urus
fn main(): void {
    let names: [str] = ["Alice", "Bob", "Charlie"];
    for name in names {
        print(f"Hello, {name}!");
    }

    let nums: [int] = [1, 2, 3, 4, 5];
    let mut total: int = 0;
    for n in nums {
        total += n;
    }
    print(f"Total: {total}");
}
```

### File I/O

```urus
fn main(): void {
    write_file("output.txt", "Hello from URUS!\n");
    let content: str = read_file("output.txt");
    print(f"Read: {str_trim(content)}");

    append_file("output.txt", "Second line.\n");
}
```

## Built-in Functions Reference

### I/O
| Function | Signature | Description |
|----------|-----------|-------------|
| `print` | `(any) → void` | Print with newline |
| `input` | `() → str` | Read one line from stdin |
| `read_file` | `(str) → str` | Read file as string |
| `write_file` | `(str, str) → void` | Write string to file |
| `append_file` | `(str, str) → void` | Append string to file |

### Array
| Function | Signature | Description |
|----------|-----------|-------------|
| `len` | `([T]) → int` | Array length |
| `push` | `([T], T) → void` | Append element to end |
| `pop` | `([T]) → void` | Remove last element |

### String
| Function | Signature | Description |
|----------|-----------|-------------|
| `str_len` | `(str) → int` | String length |
| `str_upper` | `(str) → str` | Uppercase |
| `str_lower` | `(str) → str` | Lowercase |
| `str_trim` | `(str) → str` | Trim whitespace |
| `str_contains` | `(str, str) → bool` | Check for substring |
| `str_find` | `(str, str) → int` | Find substring position (-1 if not found) |
| `str_slice` | `(str, int, int) → str` | Substring from start to end index |
| `str_replace` | `(str, str, str) → str` | Replace all occurrences |
| `str_starts_with` | `(str, str) → bool` | Check if starts with prefix |
| `str_ends_with` | `(str, str) → bool` | Check if ends with suffix |
| `str_split` | `(str, str) → [str]` | Split string by delimiter |
| `char_at` | `(str, int) → str` | Get character at index (1-char string) |

### Conversion
| Function | Signature | Description |
|----------|-----------|-------------|
| `to_str` | `(any) → str` | Convert to string |
| `to_int` | `(any) → int` | Convert to integer |
| `to_float` | `(any) → float` | Convert to float |

### Math
| Function | Signature | Description |
|----------|-----------|-------------|
| `abs` | `(int) → int` | Absolute value (integer) |
| `fabs` | `(float) → float` | Absolute value (float) |
| `sqrt` | `(float) → float` | Square root |
| `pow` | `(float, float) → float` | Power (x^y) |
| `min` | `(int, int) → int` | Minimum of two integers |
| `max` | `(int, int) → int` | Maximum of two integers |
| `fmin` | `(float, float) → float` | Minimum of two floats |
| `fmax` | `(float, float) → float` | Maximum of two floats |

### Result
| Function | Signature | Description |
|----------|-----------|-------------|
| `is_ok` | `(Result) → bool` | Check if Ok |
| `is_err` | `(Result) → bool` | Check if Err |
| `unwrap` | `(Result) → T` | Extract Ok value (aborts on Err) |
| `unwrap_err` | `(Result) → E` | Extract Err value (aborts on Ok) |

### Misc
| Function | Signature | Description |
|----------|-----------|-------------|
| `exit` | `(int) → void` | Exit program with exit code |
| `assert` | `(bool, str) → void` | Abort with message if condition is false |
