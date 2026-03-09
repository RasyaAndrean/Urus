# URUS Language Specification v1.0.1(F)

## Overview

URUS is a statically-typed, compiled programming language that transpiles to C.
Memory is managed automatically via reference counting.

## Syntax Style

C-like: curly braces for blocks, semicolons as statement terminators.

---

## 1. Primitive Types

| Type    | Description              | C equivalent     |
|---------|--------------------------|-------------------|
| `int`   | 64-bit signed integer    | `int64_t`         |
| `float` | 64-bit floating point    | `double`          |
| `bool`  | Boolean                  | `bool` (stdbool)  |
| `str`   | UTF-8 string (ref-counted) | `urus_str*`     |
| `void`  | No value (return type)   | `void`            |

## 2. Variables

```
let x: int = 10;
let name: str = "hello";
let flag: bool = true;
```

Type annotation is required (no type inference).

Variables are immutable by default. Use `mut` for mutable:

```
let mut count: int = 0;
count = count + 1;
```

## 3. Functions

```
fn add(a: int, b: int): int {
    return a + b;
}

fn greet(name: str): void {
    print(name);
}
```

- Return type after `:` following parameter list.
- `void` can be omitted: `fn greet(name: str) { ... }` is valid.
- No function overloading.

### Entry Point

```
fn main(): void {
    // program starts here
}
```

## 4. Control Flow

### If / Else

```
if x > 10 {
    print("big");
} else if x > 5 {
    print("medium");
} else {
    print("small");
}
```

No parentheses required around condition (but allowed).

### While Loop

```
while x < 100 {
    x = x + 1;
}
```

### For Loop

Range-based:

```
for i in 0..10 {
    print(i);
}
```

`0..10` is exclusive (0 to 9). `0..=10` is inclusive (0 to 10).

### For-each Loop

Iterate over array elements:

```
let names: [str] = ["Alice", "Bob"];
for name in names {
    print(name);
}
```

### Break / Continue

```
while true {
    if done {
        break;
    }
    continue;
}
```

`break` and `continue` are validated to only appear inside loops.

## 5. Operators

### Arithmetic
`+`, `-`, `*`, `/`, `%`

### Comparison
`==`, `!=`, `<`, `>`, `<=`, `>=`

### Logical
`&&`, `||`, `!`

### Assignment
`=`, `+=`, `-=`, `*=`, `/=`

### String Concatenation
`+` for strings: `"hello" + " world"`

## 6. Structs

```
struct Point {
    x: float;
    y: float;
}

fn main(): void {
    let p: Point = Point { x: 1.0, y: 2.0 };
    print(p.x);
}
```

No methods on structs. Use free functions:

```
fn distance(a: Point, b: Point): float {
    let dx: float = a.x - b.x;
    let dy: float = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}
```

## 7. Arrays

```
let nums: [int] = [1, 2, 3, 4, 5];
let first: int = nums[0];
let size: int = len(nums);
```

Arrays are dynamic (ref-counted, growable):

```
let mut items: [int] = [];
push(items, 42);
```

Supported element types: `int`, `float`, `bool`, `str`.

Array index assignment:

```
let mut nums: [int] = [1, 2, 3];
nums[0] = 10;
```

## 8. Enums / Tagged Unions

```
enum Color {
    Red;
    Green;
    Blue;
    Custom(r: int, g: int, b: int);
}

enum Shape {
    Circle(r: float);
    Rect(w: float, h: float);
    Point;
}
```

- Variants can be unit (no data) or carry fields.
- Construct with `EnumName.Variant` or `EnumName.Variant(args)`.

## 9. Pattern Matching

```
fn describe(s: Shape): str {
    match s {
        Shape.Circle(r) => {
            return f"Circle with radius {r}";
        }
        Shape.Rect(w, h) => {
            return f"Rectangle {w} x {h}";
        }
        Shape.Point => {
            return "A point";
        }
    }
    return "Unknown";
}
```

- Match target must be an enum type.
- Arms bind variant fields as local variables.

## 10. String Interpolation

```
let name: str = "World";
let count: int = 42;
print(f"Hello {name}! Count is {count}.");
```

- `f"..."` strings can contain `{expr}` blocks.
- Expressions are converted via `to_str()` and concatenated.

## 11. Modules / Imports

```
import "math_utils.urus";
```

- Imports are resolved relative to the importing file's directory.
- Circular imports are detected and rejected.
- All top-level declarations from imported files are merged into the program.

## 12. Error Handling (Result Type)

```
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

- `Result<T, E>` is a tagged union with `Ok(T)` and `Err(E)` variants.
- Built-in functions: `is_ok()`, `is_err()`, `unwrap()`, `unwrap_err()`.
- `unwrap()` on an Err or `unwrap_err()` on an Ok aborts with an error message.

## 13. Comments

```
// single line comment

/*
   multi-line
   comment
*/
```

## 14. Built-in Functions

### I/O

| Function              | Description                          |
|-----------------------|--------------------------------------|
| `print(value)`        | Print to stdout with newline         |
| `input()`             | Read one line from stdin             |
| `read_file(path)`     | Read file contents as string         |
| `write_file(path, s)` | Write string to file                 |
| `append_file(path, s)`| Append string to file                |

### Array

| Function              | Description                          |
|-----------------------|--------------------------------------|
| `len(array)`          | Array length                         |
| `push(array, v)`      | Append to array                      |
| `pop(array)`          | Remove last element                  |

### String

| Function              | Description                          |
|-----------------------|--------------------------------------|
| `str_len(s)`          | String length                        |
| `str_upper(s)`        | Uppercase string                     |
| `str_lower(s)`        | Lowercase string                     |
| `str_trim(s)`         | Trim whitespace                      |
| `str_contains(s, sub)`| Check if string contains substring   |
| `str_find(s, sub)`    | Find index of substring (-1 if not found) |
| `str_slice(s, a, b)`  | Substring from index a to b          |
| `str_replace(s, a, b)`| Replace occurrences of a with b      |
| `str_starts_with(s, p)`| Check if string starts with prefix  |
| `str_ends_with(s, p)` | Check if string ends with suffix     |
| `str_split(s, delim)` | Split string into array of strings   |
| `char_at(s, i)`       | Get character at index (as string)   |

### Conversion

| Function              | Description                          |
|-----------------------|--------------------------------------|
| `to_str(value)`       | Convert to string                    |
| `to_int(value)`       | Convert to int                       |
| `to_float(value)`     | Convert to float                     |

### Math

| Function              | Description                          |
|-----------------------|--------------------------------------|
| `abs(x)`              | Absolute value (int)                 |
| `fabs(x)`             | Absolute value (float)               |
| `sqrt(x)`             | Square root                          |
| `pow(x, y)`           | Power (x^y)                          |
| `min(a, b)`           | Minimum of two ints                  |
| `max(a, b)`           | Maximum of two ints                  |
| `fmin(a, b)`          | Minimum of two floats                |
| `fmax(a, b)`          | Maximum of two floats                |

### Result

| Function              | Description                          |
|-----------------------|--------------------------------------|
| `is_ok(result)`       | Check if Result is Ok                |
| `is_err(result)`      | Check if Result is Err               |
| `unwrap(result)`      | Extract Ok value (aborts on Err)     |
| `unwrap_err(result)`  | Extract Err value (aborts on Ok)     |

### Misc

| Function              | Description                          |
|-----------------------|--------------------------------------|
| `exit(code)`          | Exit program with code               |
| `assert(cond, msg)`   | Abort with message if condition false|

## 15. Memory Model

All heap-allocated values (strings, arrays, structs) use reference counting.

- Assignment copies the reference, increments refcount.
- When a variable goes out of scope, refcount decrements.
- When refcount reaches 0, memory is freed.
- **No cycle detection.** Programmer must avoid circular references.

## 16. Scope Rules

- Block scoping: variables live within their `{ }` block.
- No variable shadowing in the same scope.
- Inner scopes can shadow outer variables.

```
let x: int = 1;
{
    let x: int = 2;  // OK: shadows outer x
    print(x);        // prints 2
}
print(x);            // prints 1
```

---

## Grammar (EBNF)

```ebnf
program        = { declaration } ;

declaration    = fn_decl | struct_decl | enum_decl | import_decl | statement ;

fn_decl        = "fn" IDENT "(" param_list ")" [ ":" type ] block ;
param_list     = [ param { "," param } ] ;
param          = IDENT ":" type ;

struct_decl    = "struct" IDENT "{" { field } "}" ;
field          = IDENT ":" type ";" ;

enum_decl      = "enum" IDENT "{" { variant } "}" ;
variant        = IDENT [ "(" param_list ")" ] ";" ;

import_decl    = "import" STR_LIT ";" ;

block          = "{" { statement } "}" ;

statement      = let_stmt
               | assign_stmt
               | if_stmt
               | while_stmt
               | for_stmt
               | return_stmt
               | break_stmt
               | continue_stmt
               | match_stmt
               | expr_stmt ;

let_stmt       = "let" [ "mut" ] IDENT ":" type "=" expr ";" ;
assign_stmt    = lvalue assign_op expr ";" ;
assign_op      = "=" | "+=" | "-=" | "*=" | "/=" ;
lvalue         = IDENT { "." IDENT | "[" expr "]" } ;

if_stmt        = "if" expr block { "else" "if" expr block } [ "else" block ] ;
while_stmt     = "while" expr block ;
for_stmt       = "for" IDENT "in" expr ".." ["="] expr block
               | "for" IDENT "in" expr block ;
return_stmt    = "return" [ expr ] ";" ;
break_stmt     = "break" ";" ;
continue_stmt  = "continue" ";" ;

match_stmt     = "match" expr "{" { match_arm } "}" ;
match_arm      = pattern "=>" block ;
pattern        = IDENT "." IDENT [ "(" ident_list ")" ] ;
ident_list     = [ IDENT { "," IDENT } ] ;

expr_stmt      = expr ";" ;

expr           = logic_or ;
logic_or       = logic_and { "||" logic_and } ;
logic_and      = equality { "&&" equality } ;
equality       = comparison { ( "==" | "!=" ) comparison } ;
comparison     = addition { ( "<" | ">" | "<=" | ">=" ) addition } ;
addition       = multiplication { ( "+" | "-" ) multiplication } ;
multiplication = unary { ( "*" | "/" | "%" ) unary } ;
unary          = ( "!" | "-" ) unary | call ;
call           = primary { "(" arg_list ")" | "." IDENT | "[" expr "]" } ;
arg_list       = [ expr { "," expr } ] ;

primary        = INT_LIT | FLOAT_LIT | STR_LIT | FSTR_LIT
               | "true" | "false"
               | "Ok" "(" expr ")"
               | "Err" "(" expr ")"
               | IDENT
               | IDENT "{" field_init_list "}"
               | IDENT "." IDENT [ "(" arg_list ")" ]
               | "[" [ expr { "," expr } ] "]"
               | "(" expr ")" ;

FSTR_LIT       = "f" '"' { any_char | "{" expr "}" } '"' ;

field_init_list = field_init { "," field_init } ;
field_init      = IDENT ":" expr ;

type           = "int" | "float" | "bool" | "str" | "void"
               | "[" type "]"
               | "Result" "<" type "," type ">"
               | IDENT ;

(* Tokens *)
IDENT          = ALPHA { ALPHA | DIGIT | "_" } ;
INT_LIT        = DIGIT { DIGIT } ;
FLOAT_LIT      = DIGIT { DIGIT } "." DIGIT { DIGIT } ;
STR_LIT        = '"' { any_char - '"' | '\\"' } '"' ;
ALPHA          = "a".."z" | "A".."Z" | "_" ;
DIGIT          = "0".."9" ;
```

---

## Example Program

```urus
struct Circle {
    radius: float;
}

fn area(c: Circle): float {
    return 3.14159 * c.radius * c.radius;
}

fn main(): void {
    let c: Circle = Circle { radius: 5.0 };
    print(f"Area: {area(c)}");

    let mut sum: int = 0;
    for i in 1..=100 {
        sum += i;
    }
    print(f"Sum 1-100: {sum}");
}
```

---

## Not Yet Implemented

These are deferred to future versions:

- Generics / templates
- Closures / lambdas
- Interfaces / traits
- Cycle detection in refcount
- Pointer/unsafe operations
- Method syntax on structs
