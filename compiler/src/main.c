#ifndef _WIN32
#  define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOGDI
// Avoid winnt.h TokenType conflict with our TokenType
#  define TokenType _win_TokenType
#  include <windows.h>
#  undef TokenType
#  include <process.h>
#else
#  include <unistd.h>
#  include <limits.h>
#endif

#include "config.h"
#include "util.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "codegen.h"
#include "sema.h"

// Find gcc executable, trying common paths on Windows
static const char *find_gcc(void) {
#ifdef _WIN32
    // Try common MSYS2/MinGW locations with forward slashes (GCC handles these)
    static const char *paths[] = {
        "C:/msys64/mingw64/bin/gcc.exe",
        "C:/msys64/mingw32/bin/gcc.exe",
        "C:/mingw64/bin/gcc.exe",
        "C:/MinGW/bin/gcc.exe",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "rb");
        if (f) { fclose(f); return paths[i]; }
    }
    return "gcc"; // fallback to PATH
#else
    return "gcc";
#endif
}

// ---- Import resolution ----

// Track imported files to detect circular imports
#define MAX_IMPORTS 64
static const char *imported_files[MAX_IMPORTS];
static int import_count = 0;

static bool already_imported(const char *path) {
    for (int i = 0; i < import_count; i++) {
        if (strcmp(imported_files[i], path) == 0) return true;
    }
    return false;
}

// Resolve a relative import path based on the base file directory
static char *resolve_import_path(const char *base_file, const char *import_path) {
    // Find last / or backslash in base_file
    const char *last_sep = NULL;
    for (const char *p = base_file; *p; p++) {
        if (*p == '/' || *p == '\\') last_sep = p;
    }

    if (!last_sep) {
        return strdup(import_path);
    }

    size_t dir_len = (size_t)(last_sep - base_file + 1);
    size_t imp_len = strlen(import_path);
    char *full = malloc(dir_len + imp_len + 1);
    memcpy(full, base_file, dir_len);
    memcpy(full + dir_len, import_path, imp_len);
    full[dir_len + imp_len] = '\0';
    return full;
}

// Process imports: lex+parse imported files and merge declarations into program
static bool process_imports(AstNode *program, const char *base_file) {
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind != NODE_IMPORT) continue;

        char *path = resolve_import_path(base_file, d->as.import_decl.path);

        if (already_imported(path)) {
            free(path);
            continue;
        }

        if (import_count >= MAX_IMPORTS) {
            fprintf(stderr, "Error: too many imports (max %d)\n", MAX_IMPORTS);
            free(path);
            return false;
        }
        imported_files[import_count++] = path;

        size_t len;
        char *source = read_file(path, &len);
        if (!source) {
            fprintf(stderr, "Error: cannot import '%s'\n", d->as.import_decl.path);
            return false;
        }

        Lexer lexer;
        lexer_init(&lexer, source, len);
        int token_count;
        Token *tokens = lexer_tokenize(&lexer, &token_count);
        if (!tokens) {
            free(source);
            return false;
        }

        Parser parser;
        parser.filename = path;
        parser_init(&parser, tokens, token_count);
        AstNode *imported = parser_parse(&parser);

        if (parser.had_error) {
            fprintf(stderr, "Error parsing imported file '%s'\n", path);
            ast_free(imported);
            free(tokens);
            free(source);
            return false;
        }

        // Recursively process imports in the imported file
        if (!process_imports(imported, path)) {
            ast_free(imported);
            free(tokens);
            free(source);
            return false;
        }

        // Merge imported declarations into program (insert before current position)
        int new_count = program->as.program.decl_count + imported->as.program.decl_count - 1;
        AstNode **new_decls = malloc(sizeof(AstNode *) * (size_t)(new_count + 1));

        int pos = 0;
        // Copy declarations before the import statement
        for (int j = 0; j < i; j++) {
            new_decls[pos++] = program->as.program.decls[j];
        }
        // Insert imported declarations (skip imports from imported file)
        for (int j = 0; j < imported->as.program.decl_count; j++) {
            if (imported->as.program.decls[j]->kind != NODE_IMPORT) {
                new_decls[pos++] = imported->as.program.decls[j];
                imported->as.program.decls[j] = NULL; // transfer ownership
            }
        }
        // Skip the import statement itself
        // Copy declarations after the import
        for (int j = i + 1; j < program->as.program.decl_count; j++) {
            new_decls[pos++] = program->as.program.decls[j];
        }

        free(program->as.program.decls);
        program->as.program.decls = new_decls;
        program->as.program.decl_count = pos;

        // Don't free imported->decls since we transferred ownership
        free(imported->as.program.decls);
        imported->as.program.decls = NULL;
        imported->as.program.decl_count = 0;
        ast_free(imported);
        free(tokens);
        // Note: source memory is borrowed by tokens, don't free yet

        // Re-scan from beginning since we modified the array
        i = -1;
    }
    return true;
}

// ---- Main ----

static void print_tokens(Token *tokens, int count) {
    printf("=== Tokens ===\n");
    for (int i = 0; i < count; i++) {
        printf("  %-12s '%.*s'\n",
               token_type_name(tokens[i].type),
               (int)tokens[i].length, tokens[i].start);
    }
    printf("\n");
}

static void show_help(char *progname) {
    printf(
        "URUS Compiler, version "URUS_COMPILER_VERSION"\n"
        "usage: %s [options] file\n\n"
        "Rust-like safety with Python-like simplicity, transpiling to C11\n\n"
        "Options:\n"
        "  --help      Show help message\n"
        "  --version   Show compiler message\n"
        "  --tokens    Display Lexer tokens\n"
        "  --ast       Display the Abstract Syntax Tree (AST)\n"
        "  --emit-c    Print generated C code to stdout\n"
        "  -o <file>   Specify output executable name (default: a.out on Linux, a.exe on Windows)\n\n"
        "Example:\n"
        "  %s main.urus -o app \n", progname, progname
    );
}

static void show_version(void) {
    printf(
        "URUS Compiler, version "URUS_COMPILER_VERSION"\n"
        "Copyright (C) 2026 Urus Foundation.\n"
        "License: Apache License 2.0 <http://www.apache.org>\n"
        "Homepage: https://github.com/Urus-Foundation/Urus\n\n"
        "This is free software: you are free to change and redistribute it.\n"
        "There is NO WARRANTY, to the extent permitted by law.\n"
    );
}

int main(int argc, char **argv) {
    if (argc < 2) {
        show_help(argv[0]);
        return 1;
    }

    const char *path = NULL;
    bool show_tokens = false;
    bool show_ast = false;
    bool emit_c = false;
    const char *output = NULL;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if ((strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0)) {
                show_help(argv[0]);
                return 0;
            }
            else if ((strcmp(argv[i], "--version") == 0) || (strcmp(argv[i], "-v") == 0)) {
                show_version();
                return 0;
            }
            else if (strcmp(argv[i], "--tokens") == 0) show_tokens = true;
            else if (strcmp(argv[i], "--ast") == 0) show_ast = true;
            else if (strcmp(argv[i], "--emit-c") == 0) emit_c = true;
            else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output = argv[++i];
            else {
                fprintf(stderr, "%s: error: invalid option %s\n", argv[0], argv[i]);
                return 1;
            }
        }
        else {
            // TODO: Create support for files... input array,  by making objects before compilation
            path = argv[i];
        }
    }

    if (!path) {
        fprintf(stderr, "%s: error: input file required\n", argv[0]);
        show_help(argv[0]);
        return 1;
    }

    size_t len;
    char *source = read_file(path, &len);
    if (!source) return 1;

    // Mark base file as imported (to prevent circular self-import)
    imported_files[import_count++] = path;

    // Lexing
    Lexer lexer;
    lexer_init(&lexer, source, len);
    int token_count;
    Token *tokens = lexer_tokenize(&lexer, &token_count);
    if (!tokens) {
        free(source);
        return 1;
    }

    if (show_tokens) {
        print_tokens(tokens, token_count);
    }

    // Parsing
    Parser parser;
    parser.filename = path;
    parser_init(&parser, tokens, token_count);
    AstNode *program = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "Parsing failed.\n");
        ast_free(program);
        free(tokens);
        free(source);
        return 1;
    }

    // Process imports
    if (!process_imports(program, path)) {
        fprintf(stderr, "Import resolution failed.\n");
        ast_free(program);
        free(tokens);
        free(source);
        return 1;
    }

    if (show_ast) {
        printf("=== AST ===\n");
        ast_print(program, 0);
    }

    // Semantic analysis
    if (!sema_analyze(program, path)) {
        fprintf(stderr, "Semantic analysis failed.\n");
        ast_free(program);
        free(tokens);
        free(source);
        return 1;
    }

    // Code generation
    CodeBuf cbuf;
    codegen_init(&cbuf);
    codegen_generate(&cbuf, program);

    if (emit_c) {
        printf("%s", cbuf.data);
    } else {
        const char *c_path = "_urus_tmp.c";
#ifdef _WIN32
        const char *out_path = output ? output : "a.exe";
#else
        const char *out_path = output ? output : "a.out";
#endif

        FILE *f = fopen(c_path, "wb");
        if (!f) {
            fprintf(stderr, "Error: cannot create temp file '%s'\n", c_path);
            codegen_free(&cbuf);
            ast_free(program);
            free(tokens);
            free(source);
            return 1;
        }
        fwrite(cbuf.data, 1, cbuf.len, f);
        fclose(f);

        const char *gcc_path = find_gcc();

#ifdef _WIN32
        // Ensure TMP/TEMP point to a valid Windows temp directory
        // (MSYS2 bash may set these to /tmp which native apps can't resolve)
        char win_tmp[MAX_PATH];
        DWORD tmp_len = GetTempPathA(sizeof(win_tmp), win_tmp);
        if (tmp_len > 0 && tmp_len < sizeof(win_tmp)) {
            _putenv_s("TMP", win_tmp);
            _putenv_s("TEMP", win_tmp);
        }

        // Ensure GCC's bin directory is in PATH so cc1 can be found
        {
            const char *last_slash = strrchr(gcc_path, '/');
            if (!last_slash) last_slash = strrchr(gcc_path, '\\');
            if (last_slash) {
                size_t dir_len = (size_t)(last_slash - gcc_path);
                char gcc_dir[4096];
                snprintf(gcc_dir, sizeof(gcc_dir), "%.*s", (int)dir_len, gcc_path);
                const char *old_path = getenv("PATH");
                char new_path[16384];
                snprintf(new_path, sizeof(new_path), "%s;%s", gcc_dir, old_path ? old_path : "");
                _putenv_s("PATH", new_path);
            }
        }

        // Runtime header is embedded in generated C, no -I needed
        char cmd[8192];
        snprintf(cmd, sizeof(cmd),
                 "\"%s\" -std=c11 -O2 -o \"%s\" \"%s\" -lm",
                 gcc_path, out_path, c_path);
        printf("Compiling: %s\n", cmd);

        // Use _spawnl for reliable execution on Windows
        int ret = (int)_spawnl(_P_WAIT, gcc_path, "gcc",
                               "-std=c11", "-O2",
                               "-o", out_path,
                               c_path, "-lm", NULL);
#else
        // Runtime header is embedded in generated C, no -I needed
        char cmd[8192];
        snprintf(cmd, sizeof(cmd),
                 "%s -std=c11 -O2 -o \"%s\" \"%s\" -lm",
                 gcc_path, out_path, c_path);
        printf("Compiling: %s\n", cmd);
        int ret = system(cmd);
#endif

        remove(c_path);

        if (ret != 0) {
            fprintf(stderr, "Compilation failed.\n");
            codegen_free(&cbuf);
            ast_free(program);
            free(tokens);
            free(source);
            return 1;
        }
        printf("Output: %s\n", out_path);
    }

    codegen_free(&cbuf);
    ast_free(program);
    free(tokens);
    free(source);
    return 0;
}
