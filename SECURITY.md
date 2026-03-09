# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 1.0.1   | Yes (current) |
| 1.0.0   | Yes       |
| < 1.0   | No        |

## Reporting a Vulnerability

If you discover a security vulnerability in the URUS compiler or runtime, please report it responsibly.

### How to Report

1. **Do not open a public issue.** Security vulnerabilities should not be disclosed publicly until a fix is available.
2. **Email:** Send a detailed report to the maintainer via GitHub private message or the email listed on the [maintainer's profile](https://github.com/RasyaAndrean).
3. **Include:**
   - Description of the vulnerability
   - Steps to reproduce
   - Affected version(s)
   - Potential impact
   - Suggested fix (if any)

### What to Expect

- **Acknowledgment** within 72 hours of your report
- **Assessment** of severity and impact within 1 week
- **Fix or mitigation** as soon as possible, depending on severity
- **Credit** in the changelog and release notes (unless you prefer anonymity)

## Scope

The following are in scope for security reports:

| Area | Examples |
|------|----------|
| **Compiler** | Buffer overflow, crash on crafted input, arbitrary code execution |
| **Runtime** | Memory corruption, bounds check bypass, ref-count manipulation |
| **Generated code** | Codegen producing unsafe C, missing bounds checks |
| **Import system** | Path traversal, unintended file access |

The following are **out of scope**:

- Vulnerabilities in GCC itself
- Issues in user-written URUS programs (e.g., logic bugs)
- Denial of service via extremely large input files (known limitation)

## Security Design

For details on URUS's security model, memory safety, and known limitations, see the [Security Documentation](./documentation/security/).

### Key Points

- **Memory safety:** Automatic reference counting with runtime bounds checking
- **Type safety:** All types verified at compile time, no implicit coercion
- **No unsafe operations:** No pointer arithmetic, no manual memory management in user code
- **Immutable by default:** Variables require explicit `mut` for mutation
- **No network access:** The compiler and runtime have no networking capabilities
