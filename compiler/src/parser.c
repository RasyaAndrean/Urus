#ifndef _WIN32
#  define _POSIX_C_SOURCE 200809L
#endif

#include "parser.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// ---- Helpers ----

void parser_init(Parser *p, Token *tokens, int count) {
    p->tokens = tokens;
    p->count = count;
    p->pos = 0;
    p->had_error = false;
}

static Token current(Parser *p) {
    return p->tokens[p->pos];
}

static Token previous(Parser *p) {
    return p->tokens[p->pos - 1];
}

static bool check(Parser *p, TokenType type) {
    return current(p).type == type;
}

static bool at_end(Parser *p) {
    return current(p).type == TOK_EOF;
}

static Token advance_tok(Parser *p) {
    Token t = current(p);
    if (!at_end(p)) p->pos++;
    return t;
}

static void error_at(Parser *p, Token t, const char *msg) {
    if (p->had_error) return;
    p->had_error = true;
    report_error(p->filename, &t, msg);
}

static void warn_at(Parser *p, Token t, const char *msg) {
    report_warn(p->filename, &t, msg);
}

static Token expect(Parser *p, TokenType type, const char *msg) {
    if (check(p, type)) return advance_tok(p);
    error_at(p, current(p), msg);
    return current(p);
}

static bool match(Parser *p, TokenType type) {
    if (check(p, type)) { advance_tok(p); return true; }
    return false;
}

static char *tok_str(Token t) {
    return ast_strdup(t.start, t.length);
}

static char *tok_str_value(Token t) {
    return ast_strdup(t.start + 1, t.length - 2);
}

// Strip underscores from numeric literal for strtoll/strtod
static char *tok_num_str(Token t) {
    char *buf = malloc(t.length + 1);
    size_t j = 0;
    for (size_t i = 0; i < t.length; i++) {
        if (t.start[i] != '_') buf[j++] = t.start[i];
    }
    buf[j] = '\0';
    return buf;
}

// ---- Forward declarations ----
static AstNode *parse_expr(Parser *p);
static AstNode *parse_statement(Parser *p);
static AstNode *parse_block(Parser *p);
static AstType *parse_type(Parser *p);

// ---- Rune (macro) table ----
typedef struct {
    char *name;
    char **param_names;
    int param_count;
    Token *body_tokens;
    int body_token_count;
} RuneDef;

#define MAX_RUNES 64
static RuneDef rune_defs[MAX_RUNES];
static int rune_count = 0;

static RuneDef *find_rune(const char *name) {
    for (int i = 0; i < rune_count; i++) {
        if (strcmp(rune_defs[i].name, name) == 0) return &rune_defs[i];
    }
    return NULL;
}

// ---- Type parsing ----

static AstType *parse_type(Parser *p) {
    if (match(p, TOK_INT))   return ast_type_simple(TYPE_INT);
    if (match(p, TOK_FLOAT)) return ast_type_simple(TYPE_FLOAT);
    if (match(p, TOK_BOOL))  return ast_type_simple(TYPE_BOOL);
    if (match(p, TOK_STR))   return ast_type_simple(TYPE_STR);
    if (match(p, TOK_VOID))  return ast_type_simple(TYPE_VOID);
    if (match(p, TOK_LBRACKET)) {
        AstType *elem = parse_type(p);
        expect(p, TOK_RBRACKET, "expected ']' after array type");
        return ast_type_array(elem);
    }
    // Tuple type: (T1, T2, ...)
    if (match(p, TOK_LPAREN)) {
        int cap = 4, count = 0;
        AstType **elems = malloc(sizeof(AstType *) * (size_t)cap);
        if (!check(p, TOK_RPAREN)) {
            do {
                if (count >= cap) {
                    cap *= 2;
                    elems = realloc(elems, sizeof(AstType *) * (size_t)cap);
                }
                elems[count++] = parse_type(p);
            } while (match(p, TOK_COMMA));
        }
        expect(p, TOK_RPAREN, "expected ')' after tuple type");
        return ast_type_tuple(elems, count);
    }
    // Result<T, E>
    if (check(p, TOK_IDENT)) {
        Token t = current(p);
        if (t.length == 6 && memcmp(t.start, "Result", 6) == 0) {
            advance_tok(p);
            expect(p, TOK_LT, "expected '<' after Result");
            AstType *ok = parse_type(p);
            expect(p, TOK_COMMA, "expected ',' in Result<T, E>");
            AstType *err = parse_type(p);
            expect(p, TOK_GT, "expected '>' after Result type");
            return ast_type_result(ok, err);
        }
        advance_tok(p);
        return ast_type_named(ast_strdup(t.start, t.length));
    }
    error_at(p, current(p), "expected type");
    return ast_type_simple(TYPE_VOID);
}

// ---- F-string parsing ----
// Desugar f"text {expr} text" into chain of to_str() + concat
static AstNode *parse_fstring(Parser *p, Token t) {
    (void)p;
    // Token content is f"...", strip f" and "
    const char *raw = t.start + 2; // skip f"
    size_t raw_len = t.length - 3; // minus f" and "

    AstNode *result = NULL;

    size_t i = 0;
    while (i < raw_len) {
        // Escaped braces: {{ → literal {, }} → literal }
        if (raw[i] == '{' && i + 1 < raw_len && raw[i + 1] == '{') {
            // Collect literal text including escaped braces
            size_t start = i;
            while (i < raw_len) {
                if (raw[i] == '{' && i + 1 < raw_len && raw[i + 1] == '{') { i += 2; continue; }
                if (raw[i] == '}' && i + 1 < raw_len && raw[i + 1] == '}') { i += 2; continue; }
                if (raw[i] == '{' || raw[i] == '}') break;
                if (raw[i] == '\\' && i + 1 < raw_len) i++;
                i++;
            }
            // Build the literal with escaped braces resolved
            size_t seg_len = i - start;
            char *buf = malloc(seg_len + 1);
            size_t j = 0;
            for (size_t k = start; k < i; k++) {
                if (raw[k] == '{' && k + 1 < i && raw[k + 1] == '{') { buf[j++] = '{'; k++; }
                else if (raw[k] == '}' && k + 1 < i && raw[k + 1] == '}') { buf[j++] = '}'; k++; }
                else buf[j++] = raw[k];
            }
            buf[j] = '\0';
            AstNode *lit = ast_new(NODE_STR_LIT, t);
            lit->as.str_lit.value = buf;
            if (!result) { result = lit; }
            else {
                AstNode *bin = ast_new(NODE_BINARY, t);
                bin->as.binary.left = result;
                bin->as.binary.op = TOK_PLUS;
                bin->as.binary.right = lit;
                result = bin;
            }
            continue;
        }
        if (raw[i] == '{') {
            i++; // skip {
            // Find matching }
            size_t start = i;
            int depth = 1;
            while (i < raw_len && depth > 0) {
                if (raw[i] == '{') depth++;
                else if (raw[i] == '}') depth--;
                if (depth > 0) i++;
            }
            size_t expr_len = i - start;
            if (i < raw_len) i++; // skip }

            // Parse the expression substring
            // Create a mini-lexer for the expression
            Lexer sub_lexer;
            lexer_init(&sub_lexer, raw + start, expr_len);
            sub_lexer.line = t.line;
            int sub_count;
            Token *sub_tokens = lexer_tokenize(&sub_lexer, &sub_count);
            if (sub_tokens && sub_count > 0) {
                Parser sub_parser;
                parser_init(&sub_parser, sub_tokens, sub_count);
                AstNode *expr = parse_expr(&sub_parser);

                // Wrap in to_str() call
                AstNode *to_str_ident = ast_new(NODE_IDENT, t);
                to_str_ident->as.ident.name = strdup("to_str");
                AstNode *call = ast_new(NODE_CALL, t);
                call->as.call.callee = to_str_ident;
                call->as.call.args = malloc(sizeof(AstNode *));
                call->as.call.args[0] = expr;
                call->as.call.arg_count = 1;

                if (!result) {
                    result = call;
                } else {
                    AstNode *bin = ast_new(NODE_BINARY, t);
                    bin->as.binary.left = result;
                    bin->as.binary.op = TOK_PLUS;
                    bin->as.binary.right = call;
                    result = bin;
                }
                free(sub_tokens);
            }
        } else {
            // Collect literal text until { or end
            size_t start = i;
            while (i < raw_len && raw[i] != '{') {
                if (raw[i] == '\\' && i + 1 < raw_len) i++; // skip escape
                i++;
            }
            if (i > start) {
                AstNode *lit = ast_new(NODE_STR_LIT, t);
                lit->as.str_lit.value = ast_strdup(raw + start, i - start);

                if (!result) {
                    result = lit;
                } else {
                    AstNode *bin = ast_new(NODE_BINARY, t);
                    bin->as.binary.left = result;
                    bin->as.binary.op = TOK_PLUS;
                    bin->as.binary.right = lit;
                    result = bin;
                }
            }
        }
    }

    if (!result) {
        result = ast_new(NODE_STR_LIT, t);
        result->as.str_lit.value = strdup("");
    }

    return result;
}

// ---- Expression parsing (precedence climbing) ----

static AstNode *parse_primary(Parser *p) {
    Token t = current(p);

    if (match(p, TOK_INT_LIT)) {
        AstNode *n = ast_new(NODE_INT_LIT, t);
        errno = 0;
        char *s = tok_num_str(t);
        // Detect base from prefix
        int base = 10;
        char *num = s;
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; num = s + 2; }
        else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) { base = 2; num = s + 2; }
        else if (s[0] == '0' && (s[1] == 'o' || s[1] == 'O')) { base = 8; num = s + 2; }
        n->as.int_lit.value = strtoll(num, NULL, base);
        if (errno == ERANGE) {
            warn_at(p, t, "integer literal out of range");
        }
        free(s);
        return n;
    }
    if (match(p, TOK_FLOAT_LIT)) {
        AstNode *n = ast_new(NODE_FLOAT_LIT, t);
        char *s = tok_num_str(t);
        n->as.float_lit.value = strtod(s, NULL);
        free(s);
        return n;
    }
    if (match(p, TOK_STR_LIT)) {
        AstNode *n = ast_new(NODE_STR_LIT, t);
        n->as.str_lit.value = tok_str_value(t);
        return n;
    }
    if (match(p, TOK_FSTR_LIT)) {
        return parse_fstring(p, t);
    }
    if (match(p, TOK_TRUE)) {
        AstNode *n = ast_new(NODE_BOOL_LIT, t);
        n->as.bool_lit.value = true;
        return n;
    }
    if (match(p, TOK_FALSE)) {
        AstNode *n = ast_new(NODE_BOOL_LIT, t);
        n->as.bool_lit.value = false;
        return n;
    }
    // Ok(value)
    if (match(p, TOK_OK)) {
        expect(p, TOK_LPAREN, "expected '(' after Ok");
        AstNode *val = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')' after Ok value");
        AstNode *n = ast_new(NODE_OK_EXPR, t);
        n->as.result_expr.value = val;
        return n;
    }
    // Err(value)
    if (match(p, TOK_ERR)) {
        expect(p, TOK_LPAREN, "expected '(' after Err");
        AstNode *val = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')' after Err value");
        AstNode *n = ast_new(NODE_ERR_EXPR, t);
        n->as.result_expr.value = val;
        return n;
    }
    if (match(p, TOK_IDENT)) {
        char *name = tok_str(t);

        // Check for rune invocation: name!(args)
        if (check(p, TOK_NOT)) {
            int saved_bang = p->pos;
            advance_tok(p); // skip !
            if (check(p, TOK_LPAREN)) {
                RuneDef *rune = find_rune(name);
                if (rune) {
                    advance_tok(p); // skip (

                    // Collect argument token spans
                    // Each argument is a span of tokens delimited by , or )
                    int arg_cap = 8;
                    Token **arg_tokens = malloc(sizeof(Token *) * (size_t)arg_cap);
                    int *arg_lens = malloc(sizeof(int) * (size_t)arg_cap);
                    int arg_count = 0;

                    if (!check(p, TOK_RPAREN)) {
                        while (1) {
                            if (arg_count >= arg_cap) {
                                arg_cap *= 2;
                                arg_tokens = realloc(arg_tokens, sizeof(Token *) * (size_t)arg_cap);
                                arg_lens = realloc(arg_lens, sizeof(int) * (size_t)arg_cap);
                            }
                            int start = p->pos;
                            int depth2 = 0;
                            // Scan until , or ) at depth 0
                            while (!at_end(p)) {
                                TokenType tt = current(p).type;
                                if (tt == TOK_LPAREN || tt == TOK_LBRACKET || tt == TOK_LBRACE) depth2++;
                                else if (tt == TOK_RPAREN || tt == TOK_RBRACKET || tt == TOK_RBRACE) {
                                    if (depth2 == 0) break;
                                    depth2--;
                                }
                                else if (tt == TOK_COMMA && depth2 == 0) break;
                                advance_tok(p);
                            }
                            int len = p->pos - start;
                            arg_tokens[arg_count] = &p->tokens[start];
                            arg_lens[arg_count] = len;
                            arg_count++;
                            if (!match(p, TOK_COMMA)) break;
                        }
                    }
                    expect(p, TOK_RPAREN, "expected ')' after rune arguments");

                    if (arg_count != rune->param_count) {
                    {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "rune '%s' expects %d arguments, got %d", rune->name, rune->param_count, arg_count);
                        error_at(p, t, msg);
                    }
                        free(arg_tokens); free(arg_lens); free(name);
                        return ast_new(NODE_INT_LIT, t);
                    }

                    // Build expanded token stream: substitute params with arg tokens
                    int exp_cap = rune->body_token_count * 2 + 16;
                    Token *expanded = malloc(sizeof(Token) * (size_t)exp_cap);
                    int exp_count = 0;

                    for (int i = 0; i < rune->body_token_count; i++) {
                        Token bt = rune->body_tokens[i];
                        bool substituted = false;
                        if (bt.type == TOK_IDENT) {
                            for (int j = 0; j < rune->param_count; j++) {
                                if (bt.length == strlen(rune->param_names[j]) &&
                                    memcmp(bt.start, rune->param_names[j], bt.length) == 0) {
                                    // Replace this token with the argument's tokens
                                    int needed = exp_count + arg_lens[j] + 1;
                                    if (needed >= exp_cap) {
                                        exp_cap = needed * 2;
                                        expanded = realloc(expanded, sizeof(Token) * (size_t)exp_cap);
                                    }
                                    for (int k = 0; k < arg_lens[j]; k++) {
                                        expanded[exp_count++] = arg_tokens[j][k];
                                    }
                                    substituted = true;
                                    break;
                                }
                            }
                        }
                        if (!substituted) {
                            if (exp_count >= exp_cap) {
                                exp_cap *= 2;
                                expanded = realloc(expanded, sizeof(Token) * (size_t)exp_cap);
                            }
                            expanded[exp_count++] = bt;
                        }
                    }

                    // Add EOF token
                    if (exp_count >= exp_cap) {
                        exp_cap++;
                        expanded = realloc(expanded, sizeof(Token) * (size_t)exp_cap);
                    }
                    Token eof_tok = {TOK_EOF, "", 0, t.line, t.col};
                    expanded[exp_count++] = eof_tok;

                    // Save parser state and parse expanded tokens
                    Token *saved_tokens = p->tokens;
                    int saved_count = p->count;
                    int saved_pos = p->pos;

                    p->tokens = expanded;
                    p->count = exp_count;
                    p->pos = 0;

                    AstNode *result = parse_expr(p);

                    // Restore parser state
                    p->tokens = saved_tokens;
                    p->count = saved_count;
                    p->pos = saved_pos;

                    free(expanded);
                    free(arg_tokens);
                    free(arg_lens);
                    free(name);
                    return result;
                }
                // Not a rune — backtrack
                p->pos = saved_bang;
            } else {
                p->pos = saved_bang;
            }
        }

        // Check for enum init: EnumName.Variant or EnumName.Variant(args)
        // Only treat as enum init if the name starts with an uppercase letter
        // (enum names are PascalCase; lowercase names are variables/field access)
        if (check(p, TOK_DOT) && name[0] >= 'A' && name[0] <= 'Z') {
            int saved = p->pos;
            advance_tok(p); // skip .
            if (check(p, TOK_IDENT)) {
                Token variant_tok = advance_tok(p);
                char *variant = tok_str(variant_tok);
                int arg_cap = 4, arg_count = 0;
                AstNode **args = NULL;
                if (match(p, TOK_LPAREN)) {
                    args = malloc(sizeof(AstNode *) * (size_t)arg_cap);
                    if (!check(p, TOK_RPAREN)) {
                        do {
                            if (arg_count >= arg_cap) {
                                arg_cap *= 2;
                                args = realloc(args, sizeof(AstNode *) * (size_t)arg_cap);
                            }
                            args[arg_count++] = parse_expr(p);
                        } while (match(p, TOK_COMMA));
                    }
                    expect(p, TOK_RPAREN, "expected ')' after enum args");
                }
                AstNode *n = ast_new(NODE_ENUM_INIT, t);
                n->as.enum_init.enum_name = name;
                n->as.enum_init.variant_name = variant;
                n->as.enum_init.args = args;
                n->as.enum_init.arg_count = arg_count;
                return n;
            }
            p->pos = saved;
        }
        // Check for struct literal: Ident { field: val, ... } or Ident {} or Ident { ..expr }
        if (check(p, TOK_LBRACE)) {
            int saved = p->pos;
            advance_tok(p); // skip {
            // Empty struct literal: Ident {}
            if (check(p, TOK_RBRACE)) {
                advance_tok(p); // skip }
                AstNode *n = ast_new(NODE_STRUCT_LIT, t);
                n->as.struct_lit.name = name;
                n->as.struct_lit.fields = NULL;
                n->as.struct_lit.field_count = 0;
                n->as.struct_lit.spread = NULL;
                return n;
            }
            // Spread-only: Ident { ..expr }
            if (check(p, TOK_DOTDOT)) {
                advance_tok(p); // skip ..
                AstNode *spread = parse_expr(p);
                expect(p, TOK_RBRACE, "expected '}' after spread expression");
                AstNode *n = ast_new(NODE_STRUCT_LIT, t);
                n->as.struct_lit.name = name;
                n->as.struct_lit.fields = NULL;
                n->as.struct_lit.field_count = 0;
                n->as.struct_lit.spread = spread;
                return n;
            }
            if (check(p, TOK_IDENT)) {
                int saved2 = p->pos;
                advance_tok(p);
                if (check(p, TOK_COLON)) {
                    p->pos = saved + 1;
                    AstNode *n = ast_new(NODE_STRUCT_LIT, t);
                    n->as.struct_lit.name = name;
                    int cap = 4, count = 0;
                    FieldInit *fields = malloc(sizeof(FieldInit) * (size_t)cap);
                    AstNode *spread = NULL;
                    do {
                        // Trailing comma: allow } after comma
                        if (check(p, TOK_RBRACE)) break;
                        // Check for spread: ..expr (must be last)
                        if (check(p, TOK_DOTDOT)) {
                            advance_tok(p); // skip ..
                            spread = parse_expr(p);
                            break;
                        }
                        if (count >= cap) {
                            cap *= 2;
                            fields = realloc(fields, sizeof(FieldInit) * (size_t)cap);
                        }
                        Token fname = expect(p, TOK_IDENT, "expected field name");
                        expect(p, TOK_COLON, "expected ':' in struct literal");
                        AstNode *val = parse_expr(p);
                        fields[count].name = tok_str(fname);
                        fields[count].value = val;
                        count++;
                    } while (match(p, TOK_COMMA));
                    expect(p, TOK_RBRACE, "expected '}' after struct literal");
                    n->as.struct_lit.fields = fields;
                    n->as.struct_lit.field_count = count;
                    n->as.struct_lit.spread = spread;
                    return n;
                }
                p->pos = saved2;
            }
            p->pos = saved;
        }
        AstNode *n = ast_new(NODE_IDENT, t);
        n->as.ident.name = name;
        return n;
    }
    if (match(p, TOK_LBRACKET)) {
        AstNode *n = ast_new(NODE_ARRAY_LIT, t);
        int cap = 4, count = 0;
        AstNode **elems = malloc(sizeof(AstNode *) * (size_t)cap);
        if (!check(p, TOK_RBRACKET)) {
            do {
                if (count >= cap) {
                    cap *= 2;
                    elems = realloc(elems, sizeof(AstNode *) * (size_t)cap);
                }
                elems[count++] = parse_expr(p);
            } while (match(p, TOK_COMMA));
        }
        expect(p, TOK_RBRACKET, "expected ']' after array literal");
        n->as.array_lit.elements = elems;
        n->as.array_lit.count = count;
        return n;
    }
    if (match(p, TOK_LPAREN)) {
        Token paren_tok = previous(p);
        AstNode *first = parse_expr(p);
        if (match(p, TOK_COMMA)) {
            // Tuple literal: (e1, e2, ...)
            int cap = 4, count = 1;
            AstNode **elems = malloc(sizeof(AstNode *) * (size_t)cap);
            elems[0] = first;
            if (!check(p, TOK_RPAREN)) {
                do {
                    if (count >= cap) {
                        cap *= 2;
                        elems = realloc(elems, sizeof(AstNode *) * (size_t)cap);
                    }
                    elems[count++] = parse_expr(p);
                } while (match(p, TOK_COMMA));
            }
            expect(p, TOK_RPAREN, "expected ')' after tuple");
            AstNode *n = ast_new(NODE_TUPLE_LIT, paren_tok);
            n->as.tuple_lit.elements = elems;
            n->as.tuple_lit.count = count;
            return n;
        }
        expect(p, TOK_RPAREN, "expected ')'");
        first->parenthesized = true;
        return first;
    }

    // if-expression: if cond { expr } else { expr }
    if (match(p, TOK_IF)) {
        AstNode *cond = parse_expr(p);
        expect(p, TOK_LBRACE, "expected '{' after if condition");
        AstNode *then_expr = parse_expr(p);
        expect(p, TOK_RBRACE, "expected '}' after then expression");
        expect(p, TOK_ELSE, "expected 'else' in if-expression");
        expect(p, TOK_LBRACE, "expected '{' after else");
        AstNode *else_expr = parse_expr(p);
        expect(p, TOK_RBRACE, "expected '}' after else expression");
        AstNode *n = ast_new(NODE_IF_EXPR, t);
        n->as.if_expr.condition = cond;
        n->as.if_expr.then_expr = then_expr;
        n->as.if_expr.else_expr = else_expr;
        return n;
    }

    error_at(p, t, "expected expression");
    return ast_new(NODE_INT_LIT, t);
}

static AstNode *parse_call(Parser *p) {
    AstNode *expr = parse_primary(p);
    while (true) {
        if (match(p, TOK_LPAREN)) {
            int cap = 4, count = 0;
            AstNode **args = malloc(sizeof(AstNode *) * (size_t)cap);
            if (!check(p, TOK_RPAREN)) {
                do {
                    if (count >= cap) {
                        cap *= 2;
                        args = realloc(args, sizeof(AstNode *) * (size_t)cap);
                    }
                    args[count++] = parse_expr(p);
                } while (match(p, TOK_COMMA));
            }
            expect(p, TOK_RPAREN, "expected ')' after arguments");
            AstNode *call = ast_new(NODE_CALL, previous(p));
            call->as.call.callee = expr;
            call->as.call.args = args;
            call->as.call.arg_count = count;
            expr = call;
        } else if (match(p, TOK_DOT)) {
            Token field;
            if (check(p, TOK_INT_LIT)) {
                field = advance_tok(p);
            } else {
                field = expect(p, TOK_IDENT, "expected field name after '.'");
            }
            AstNode *access = ast_new(NODE_FIELD_ACCESS, field);
            access->as.field_access.object = expr;
            access->as.field_access.field = tok_str(field);
            expr = access;
        } else if (match(p, TOK_LBRACKET)) {
            AstNode *index = parse_expr(p);
            expect(p, TOK_RBRACKET, "expected ']' after index");
            AstNode *idx = ast_new(NODE_INDEX, previous(p));
            idx->as.index_expr.object = expr;
            idx->as.index_expr.index = index;
            expr = idx;
        } else {
            break;
        }
    }
    return expr;
}

static AstNode *parse_unary(Parser *p) {
    if (match(p, TOK_NOT) || match(p, TOK_MINUS)) {
        Token op = previous(p);
        AstNode *operand = parse_unary(p);
        AstNode *n = ast_new(NODE_UNARY, op);
        n->as.unary.op = op.type;
        n->as.unary.operand = operand;
        return n;
    }
    return parse_call(p);
}

static AstNode *parse_binary(Parser *p, AstNode *(*sub)(Parser *), int n_ops, const TokenType *ops) {
    AstNode *left = sub(p);
    while (true) {
        bool found = false;
        for (int i = 0; i < n_ops; i++) {
            if (match(p, ops[i])) {
                Token op = previous(p);
                AstNode *right = sub(p);
                AstNode *bin = ast_new(NODE_BINARY, op);
                bin->as.binary.left = left;
                bin->as.binary.op = op.type;
                bin->as.binary.right = right;
                left = bin;
                found = true;
                break;
            }
        }
        if (!found) break;
    }
    return left;
}

static AstNode *parse_multiplication(Parser *p) {
    static const TokenType ops[] = { TOK_STAR, TOK_SLASH, TOK_PERCENT };
    return parse_binary(p, parse_unary, 3, ops);
}

static AstNode *parse_addition(Parser *p) {
    static const TokenType ops[] = { TOK_PLUS, TOK_MINUS };
    return parse_binary(p, parse_multiplication, 2, ops);
}

static AstNode *parse_comparison(Parser *p) {
    static const TokenType ops[] = { TOK_LT, TOK_GT, TOK_LTE, TOK_GTE };
    return parse_binary(p, parse_addition, 4, ops);
}

static AstNode *parse_equality(Parser *p) {
    static const TokenType ops[] = { TOK_EQ, TOK_NEQ };
    return parse_binary(p, parse_comparison, 2, ops);
}

static AstNode *parse_logic_and(Parser *p) {
    static const TokenType ops[] = { TOK_AND };
    return parse_binary(p, parse_equality, 1, ops);
}

static AstNode *parse_logic_or(Parser *p) {
    static const TokenType ops[] = { TOK_OR };
    return parse_binary(p, parse_logic_and, 1, ops);
}

static AstNode *parse_expr(Parser *p) {
    return parse_logic_or(p);
}

// ---- Statement parsing ----

static AstNode *parse_block(Parser *p) {
    expect(p, TOK_LBRACE, "expected '{'");
    AstNode *block = ast_new(NODE_BLOCK, previous(p));
    int cap = 8, count = 0;
    AstNode **stmts = malloc(sizeof(AstNode *) * (size_t)cap);
    while (!check(p, TOK_RBRACE) && !at_end(p)) {
        if (count >= cap) {
            cap *= 2;
            stmts = realloc(stmts, sizeof(AstNode *) * (size_t)cap);
        }
        stmts[count++] = parse_statement(p);
        if (p->had_error) break;
    }
    expect(p, TOK_RBRACE, "expected '}'");
    block->as.block.stmts = stmts;
    block->as.block.stmt_count = count;
    return block;
}

static AstNode *parse_let(Parser *p) {
    Token let_tok = expect(p, TOK_LET, "expected 'let'");
    bool is_mut = match(p, TOK_MUT);

    // Tuple destructuring: let (x, y): (int, str) = expr;
    if (check(p, TOK_LPAREN)) {
        advance_tok(p);
        char *names[16];
        int name_count = 0;
        do {
            Token nt = expect(p, TOK_IDENT, "expected variable name in destructuring");
            names[name_count++] = tok_str(nt);
        } while (match(p, TOK_COMMA));
        expect(p, TOK_RPAREN, "expected ')' after destructuring names");
        expect(p, TOK_COLON, "expected ':' after destructuring pattern");
        AstType *type = parse_type(p);
        expect(p, TOK_ASSIGN, "expected '=' in let statement");
        AstNode *init = parse_expr(p);
        expect(p, TOK_SEMICOLON, "expected ';' after let statement");
        AstNode *n = ast_new(NODE_LET_STMT, let_tok);
        n->as.let_stmt.name = NULL;
        n->as.let_stmt.is_mut = is_mut;
        n->as.let_stmt.type = type;
        n->as.let_stmt.init = init;
        n->as.let_stmt.is_destructure = true;
        n->as.let_stmt.names = malloc(sizeof(char *) * (size_t)name_count);
        memcpy(n->as.let_stmt.names, names, sizeof(char *) * (size_t)name_count);
        n->as.let_stmt.name_count = name_count;
        return n;
    }

    Token name = expect(p, TOK_IDENT, "expected variable name");
    expect(p, TOK_COLON, "expected ':' after variable name");
    AstType *type = parse_type(p);
    expect(p, TOK_ASSIGN, "expected '=' in let statement");
    AstNode *init = parse_expr(p);
    expect(p, TOK_SEMICOLON, "expected ';' after let statement");
    AstNode *n = ast_new(NODE_LET_STMT, name);
    n->as.let_stmt.name = tok_str(name);
    n->as.let_stmt.is_mut = is_mut;
    n->as.let_stmt.type = type;
    n->as.let_stmt.init = init;
    n->as.let_stmt.is_destructure = false;
    return n;
}

static AstNode *parse_if(Parser *p) {
    Token if_tok = expect(p, TOK_IF, "expected 'if'");
    AstNode *cond = parse_expr(p);
    AstNode *then_block = parse_block(p);
    AstNode *else_branch = NULL;
    if (match(p, TOK_ELSE)) {
        if (check(p, TOK_IF)) {
            else_branch = parse_if(p);
        } else {
            else_branch = parse_block(p);
        }
    }
    AstNode *n = ast_new(NODE_IF_STMT, if_tok);
    n->as.if_stmt.condition = cond;
    n->as.if_stmt.then_block = then_block;
    n->as.if_stmt.else_branch = else_branch;
    return n;
}

static AstNode *parse_while(Parser *p) {
    Token while_tok = expect(p, TOK_WHILE, "expected 'while'");
    AstNode *cond = parse_expr(p);
    AstNode *body = parse_block(p);
    AstNode *n = ast_new(NODE_WHILE_STMT, while_tok);
    n->as.while_stmt.condition = cond;
    n->as.while_stmt.body = body;
    return n;
}

static AstNode *parse_for(Parser *p) {
    Token for_tok = expect(p, TOK_FOR, "expected 'for'");

    // Tuple destructuring: for (k, v) in arr { }
    if (check(p, TOK_LPAREN)) {
        advance_tok(p);
        char *names[16];
        int name_count = 0;
        do {
            Token nt = expect(p, TOK_IDENT, "expected variable name in destructuring");
            names[name_count++] = tok_str(nt);
        } while (match(p, TOK_COMMA));
        expect(p, TOK_RPAREN, "expected ')' after destructuring names");
        expect(p, TOK_IN, "expected 'in'");
        AstNode *iterable = parse_expr(p);
        AstNode *body = parse_block(p);
        AstNode *n = ast_new(NODE_FOR_STMT, for_tok);
        n->as.for_stmt.var_name = NULL;
        n->as.for_stmt.start = NULL;
        n->as.for_stmt.end = NULL;
        n->as.for_stmt.inclusive = false;
        n->as.for_stmt.is_foreach = true;
        n->as.for_stmt.iterable = iterable;
        n->as.for_stmt.body = body;
        n->as.for_stmt.is_destructure = true;
        n->as.for_stmt.var_names = malloc(sizeof(char *) * (size_t)name_count);
        memcpy(n->as.for_stmt.var_names, names, sizeof(char *) * (size_t)name_count);
        n->as.for_stmt.var_count = name_count;
        return n;
    }

    Token var = expect(p, TOK_IDENT, "expected loop variable");
    expect(p, TOK_IN, "expected 'in'");
    AstNode *expr = parse_expr(p);

    // Check if this is a range (expr .. expr) or foreach (expr is iterable)
    if (check(p, TOK_DOTDOT) || check(p, TOK_DOTDOTEQ)) {
        bool inclusive = match(p, TOK_DOTDOTEQ);
        if (!inclusive) expect(p, TOK_DOTDOT, "expected '..'");
        AstNode *end = parse_expr(p);
        AstNode *body = parse_block(p);
        AstNode *n = ast_new(NODE_FOR_STMT, for_tok);
        n->as.for_stmt.var_name = tok_str(var);
        n->as.for_stmt.start = expr;
        n->as.for_stmt.end = end;
        n->as.for_stmt.inclusive = inclusive;
        n->as.for_stmt.is_foreach = false;
        n->as.for_stmt.iterable = NULL;
        n->as.for_stmt.body = body;
        n->as.for_stmt.is_destructure = false;
        return n;
    }

    // For-each: for item in iterable { ... }
    AstNode *body = parse_block(p);
    AstNode *n = ast_new(NODE_FOR_STMT, for_tok);
    n->as.for_stmt.var_name = tok_str(var);
    n->as.for_stmt.start = NULL;
    n->as.for_stmt.end = NULL;
    n->as.for_stmt.inclusive = false;
    n->as.for_stmt.is_foreach = true;
    n->as.for_stmt.iterable = expr;
    n->as.for_stmt.body = body;
    n->as.for_stmt.is_destructure = false;
    return n;
}

static AstNode *parse_return(Parser *p) {
    Token return_tok = expect(p, TOK_RETURN, "expected 'return'");
    AstNode *val = NULL;
    if (!check(p, TOK_SEMICOLON)) {
        val = parse_expr(p);
    }
    expect(p, TOK_SEMICOLON, "expected ';' after return");
    AstNode *n = ast_new(NODE_RETURN_STMT, return_tok);
    n->as.return_stmt.value = val;
    return n;
}

static bool is_lvalue(AstNode *n) {
    return n->kind == NODE_IDENT || n->kind == NODE_FIELD_ACCESS || n->kind == NODE_INDEX;
}

static bool is_assign_op(TokenType t) {
    return t == TOK_ASSIGN || t == TOK_PLUS_EQ || t == TOK_MINUS_EQ ||
           t == TOK_STAR_EQ || t == TOK_SLASH_EQ;
}

static AstNode *parse_match(Parser *p) {
    Token match_tok = expect(p, TOK_MATCH, "expected 'match'");
    AstNode *target = parse_expr(p);
    expect(p, TOK_LBRACE, "expected '{' after match target");

    int cap = 4, count = 0;
    MatchArm *arms = malloc(sizeof(MatchArm) * (size_t)cap);

    while (!check(p, TOK_RBRACE) && !at_end(p)) {
        if (count >= cap) {
            cap *= 2;
            arms = realloc(arms, sizeof(MatchArm) * (size_t)cap);
        }
        Token enum_tok = expect(p, TOK_IDENT, "expected enum name in match arm");
        expect(p, TOK_DOT, "expected '.' after enum name");
        Token var_tok = expect(p, TOK_IDENT, "expected variant name");

        arms[count].enum_name = tok_str(enum_tok);
        arms[count].variant_name = tok_str(var_tok);
        arms[count].bindings = NULL;
        arms[count].binding_count = 0;

        if (match(p, TOK_LPAREN)) {
            int bcap = 4, bcount = 0;
            char **bindings = malloc(sizeof(char *) * (size_t)bcap);
            if (!check(p, TOK_RPAREN)) {
                do {
                    if (bcount >= bcap) {
                        bcap *= 2;
                        bindings = realloc(bindings, sizeof(char *) * (size_t)bcap);
                    }
                    Token b = expect(p, TOK_IDENT, "expected binding name");
                    bindings[bcount++] = tok_str(b);
                } while (match(p, TOK_COMMA));
            }
            expect(p, TOK_RPAREN, "expected ')' after bindings");
            arms[count].bindings = bindings;
            arms[count].binding_count = bcount;
        }

        expect(p, TOK_ARROW, "expected '=>' after match pattern");
        arms[count].body = parse_block(p);
        count++;
    }
    expect(p, TOK_RBRACE, "expected '}' after match arms");

    AstNode *n = ast_new(NODE_MATCH, match_tok);
    n->as.match_stmt.target = target;
    n->as.match_stmt.arms = arms;
    n->as.match_stmt.arm_count = count;
    return n;
}

static AstNode *parse_statement(Parser *p) {
    if (check(p, TOK_LET)) return parse_let(p);
    if (check(p, TOK_IF)) return parse_if(p);
    if (check(p, TOK_WHILE)) return parse_while(p);
    if (check(p, TOK_FOR)) return parse_for(p);
    if (check(p, TOK_MATCH)) return parse_match(p);
    if (check(p, TOK_RETURN)) return parse_return(p);
    if (match(p, TOK_BREAK)) {
        expect(p, TOK_SEMICOLON, "expected ';' after break");
        return ast_new(NODE_BREAK_STMT, previous(p));
    }
    if (match(p, TOK_CONTINUE)) {
        expect(p, TOK_SEMICOLON, "expected ';' after continue");
        return ast_new(NODE_CONTINUE_STMT, previous(p));
    }

    // Prefix increment/decrement: ++x; or --x;
    if (check(p, TOK_PLUSPLUS) || check(p, TOK_MINUSMINUS)) {
        Token op = advance_tok(p);
        AstNode *target = parse_expr(p);
        expect(p, TOK_SEMICOLON, "expected ';' after increment/decrement");
        AstNode *one = ast_new(NODE_INT_LIT, op);
        one->as.int_lit.value = 1;
        AstNode *n = ast_new(NODE_ASSIGN_STMT, op);
        n->as.assign_stmt.target = target;
        n->as.assign_stmt.op = (op.type == TOK_PLUSPLUS) ? TOK_PLUS_EQ : TOK_MINUS_EQ;
        n->as.assign_stmt.value = one;
        return n;
    }

    AstNode *expr = parse_expr(p);

    // Postfix increment/decrement: x++; or x--;
    if (is_lvalue(expr) && (check(p, TOK_PLUSPLUS) || check(p, TOK_MINUSMINUS))) {
        Token op = advance_tok(p);
        expect(p, TOK_SEMICOLON, "expected ';' after increment/decrement");
        AstNode *one = ast_new(NODE_INT_LIT, op);
        one->as.int_lit.value = 1;
        AstNode *n = ast_new(NODE_ASSIGN_STMT, op);
        n->as.assign_stmt.target = expr;
        n->as.assign_stmt.op = (op.type == TOK_PLUSPLUS) ? TOK_PLUS_EQ : TOK_MINUS_EQ;
        n->as.assign_stmt.value = one;
        return n;
    }

    if (is_lvalue(expr) && is_assign_op(current(p).type)) {
        Token op = advance_tok(p);
        AstNode *value = parse_expr(p);
        expect(p, TOK_SEMICOLON, "expected ';' after assignment");
        AstNode *n = ast_new(NODE_ASSIGN_STMT, op);
        n->as.assign_stmt.target = expr;
        n->as.assign_stmt.op = op.type;
        n->as.assign_stmt.value = value;
        return n;
    }

    expect(p, TOK_SEMICOLON, "expected ';' after expression");
    AstNode *n = ast_new(NODE_EXPR_STMT, expr->tok);
    n->as.expr_stmt.expr = expr;
    return n;
}

// ---- Top-level parsing ----

static AstNode *parse_fn_decl(Parser *p) {
    expect(p, TOK_FN, "expected 'fn'");
    Token name = expect(p, TOK_IDENT, "expected function name");
    expect(p, TOK_LPAREN, "expected '('");

    int cap = 4, count = 0;
    Param *params = malloc(sizeof(Param) * (size_t)cap);
    if (!check(p, TOK_RPAREN)) {
        do {
            if (count >= cap) {
                cap *= 2;
                params = realloc(params, sizeof(Param) * (size_t)cap);
            }
            Token pname = expect(p, TOK_IDENT, "expected parameter name");
            expect(p, TOK_COLON, "expected ':' after parameter name");
            AstType *ptype = parse_type(p);
            params[count].name = tok_str(pname);
            params[count].type = ptype;

            // parse default parameter value
            if (match(p, TOK_ASSIGN)) {
                params[count].default_value = parse_expr(p);
            } else {
                if (count > 0 && params[count - 1].default_value != NULL) {
                    error_at(p, pname, "non-default argument follow default argument");
                }
                params[count].default_value = NULL;
            }

            count++;
        } while (match(p, TOK_COMMA));
    }
    expect(p, TOK_RPAREN, "expected ')'");

    AstType *ret = NULL;
    if (match(p, TOK_COLON)) {
        ret = parse_type(p);
    } else {
        ret = ast_type_simple(TYPE_VOID);
    }

    AstNode *body = parse_block(p);

    AstNode *n = ast_new(NODE_FN_DECL, name);
    n->as.fn_decl.name = tok_str(name);
    n->as.fn_decl.params = params;
    n->as.fn_decl.param_count = count;
    n->as.fn_decl.return_type = ret;
    n->as.fn_decl.body = body;
    return n;
}

static AstNode *parse_struct_decl(Parser *p) {
    Token struct_tok = expect(p, TOK_STRUCT, "expected 'struct'");
    Token name = expect(p, TOK_IDENT, "expected struct name");
    expect(p, TOK_LBRACE, "expected '{'");

    int cap = 4, count = 0;
    Param *fields = malloc(sizeof(Param) * (size_t)cap);
    while (!check(p, TOK_RBRACE) && !at_end(p)) {
        if (count >= cap) {
            cap *= 2;
            fields = realloc(fields, sizeof(Param) * (size_t)cap);
        }
        Token fname = expect(p, TOK_IDENT, "expected field name");
        if (p->had_error) break;
        expect(p, TOK_COLON, "expected ':' after field name");
        if (p->had_error) break;
        AstType *ftype = parse_type(p);
        expect(p, TOK_SEMICOLON, "expected ';' after field");
        if (p->had_error) break;
        fields[count].name = tok_str(fname);
        fields[count].type = ftype;
        count++;
    }
    expect(p, TOK_RBRACE, "expected '}'");

    AstNode *n = ast_new(NODE_STRUCT_DECL, struct_tok);
    n->as.struct_decl.name = tok_str(name);
    n->as.struct_decl.fields = fields;
    n->as.struct_decl.field_count = count;
    return n;
}

static AstNode *parse_enum_decl(Parser *p) {
    Token enum_tok = expect(p, TOK_ENUM, "expected 'enum'");
    Token name = expect(p, TOK_IDENT, "expected enum name");
    expect(p, TOK_LBRACE, "expected '{'");

    int cap = 4, count = 0;
    EnumVariant *variants = malloc(sizeof(EnumVariant) * (size_t)cap);

    while (!check(p, TOK_RBRACE) && !at_end(p)) {
        if (count >= cap) {
            cap *= 2;
            variants = realloc(variants, sizeof(EnumVariant) * (size_t)cap);
        }
        Token vname = expect(p, TOK_IDENT, "expected variant name");
        if (p->had_error) break;
        variants[count].name = tok_str(vname);
        variants[count].fields = NULL;
        variants[count].field_count = 0;

        if (match(p, TOK_LPAREN)) {
            int fcap = 4, fcount = 0;
            Param *fields = malloc(sizeof(Param) * (size_t)fcap);
            if (!check(p, TOK_RPAREN)) {
                do {
                    if (fcount >= fcap) {
                        fcap *= 2;
                        fields = realloc(fields, sizeof(Param) * (size_t)fcap);
                    }
                    Token fname = expect(p, TOK_IDENT, "expected field name");
                    if (p->had_error) break;
                    expect(p, TOK_COLON, "expected ':' after field name");
                    if (p->had_error) break;
                    AstType *ftype = parse_type(p);
                    fields[fcount].name = tok_str(fname);
                    fields[fcount].type = ftype;
                    fcount++;
                } while (match(p, TOK_COMMA));
            }
            if (p->had_error) break;
            expect(p, TOK_RPAREN, "expected ')' after variant fields");
            if (p->had_error) break;
            variants[count].fields = fields;
            variants[count].field_count = fcount;
        }

        expect(p, TOK_SEMICOLON, "expected ';' after variant");
        if (p->had_error) break;
        count++;
    }
    expect(p, TOK_RBRACE, "expected '}'");

    AstNode *n = ast_new(NODE_ENUM_DECL, enum_tok);
    n->as.enum_decl.name = tok_str(name);
    n->as.enum_decl.variants = variants;
    n->as.enum_decl.variant_count = count;
    return n;
}

static AstNode *parse_import(Parser *p) {
    Token import_tok = expect(p, TOK_IMPORT, "expected 'import'");
    Token path = expect(p, TOK_STR_LIT, "expected module path string");
    expect(p, TOK_SEMICOLON, "expected ';' after import");
    AstNode *n = ast_new(NODE_IMPORT, import_tok);
    n->as.import_decl.path = tok_str_value(path);
    return n;
}

// ---- Rune declaration: rune name(p1, p2) { body tokens } ----
static AstNode *parse_rune_decl(Parser *p) {
    Token rune_tok = advance_tok(p); // consume 'rune'
    Token name_tok = expect(p, TOK_IDENT, "expected rune name");
    char *name = tok_str(name_tok);

    // Parse parameters
    expect(p, TOK_LPAREN, "expected '(' after rune name");
    int pcap = 4, pcount = 0;
    char **params = malloc(sizeof(char *) * (size_t)pcap);
    if (!check(p, TOK_RPAREN)) {
        do {
            if (pcount >= pcap) {
                pcap *= 2;
                params = realloc(params, sizeof(char *) * (size_t)pcap);
            }
            Token pt = expect(p, TOK_IDENT, "expected parameter name");
            params[pcount++] = tok_str(pt);
        } while (match(p, TOK_COMMA));
    }
    expect(p, TOK_RPAREN, "expected ')' after rune parameters");

    // Collect body tokens between { and } (track brace nesting)
    expect(p, TOK_LBRACE, "expected '{' for rune body");
    int bcap = 32, bcount = 0;
    Token *body = malloc(sizeof(Token) * (size_t)bcap);
    int depth = 1;
    while (!at_end(p) && depth > 0) {
        Token tok = current(p);
        if (tok.type == TOK_LBRACE) depth++;
        if (tok.type == TOK_RBRACE) {
            depth--;
            if (depth == 0) break;
        }
        if (bcount >= bcap) {
            bcap *= 2;
            body = realloc(body, sizeof(Token) * (size_t)bcap);
        }
        body[bcount++] = tok;
        advance_tok(p);
    }
    expect(p, TOK_RBRACE, "expected '}' after rune body");

    // Register in rune table
    if (rune_count < MAX_RUNES) {
        rune_defs[rune_count].name = strdup(name);
        rune_defs[rune_count].param_names = malloc(sizeof(char *) * (size_t)pcount);
        for (int i = 0; i < pcount; i++)
            rune_defs[rune_count].param_names[i] = strdup(params[i]);
        rune_defs[rune_count].param_count = pcount;
        rune_defs[rune_count].body_tokens = malloc(sizeof(Token) * (size_t)bcount);
        memcpy(rune_defs[rune_count].body_tokens, body, sizeof(Token) * (size_t)bcount);
        rune_defs[rune_count].body_token_count = bcount;
        rune_count++;
    } else {
        error_at(p, rune_tok, "maximum number of runes exceeded (64)");
    }

    AstNode *n = ast_new(NODE_RUNE_DECL, rune_tok);
    n->as.rune_decl.name = name;
    n->as.rune_decl.param_names = params;
    n->as.rune_decl.param_count = pcount;
    n->as.rune_decl.body_tokens = body;
    n->as.rune_decl.body_token_count = bcount;
    return n;
}

static AstNode *parse_declaration(Parser *p) {
    if (check(p, TOK_FN)) return parse_fn_decl(p);
    if (check(p, TOK_STRUCT)) return parse_struct_decl(p);
    if (check(p, TOK_ENUM)) return parse_enum_decl(p);
    if (check(p, TOK_IMPORT)) return parse_import(p);
    if (check(p, TOK_RUNE)) return parse_rune_decl(p);
    return parse_statement(p);
}

AstNode *parser_parse(Parser *p) {
    rune_count = 0; // reset rune table
    AstNode *program = ast_new(NODE_PROGRAM, current(p));
    int cap = 16, count = 0;
    AstNode **decls = malloc(sizeof(AstNode *) * (size_t)cap);

    while (!at_end(p) && !p->had_error) {
        if (count >= cap) {
            cap *= 2;
            decls = realloc(decls, sizeof(AstNode *) * (size_t)cap);
        }
        decls[count++] = parse_declaration(p);
    }

    program->as.program.decls = decls;
    program->as.program.decl_count = count;
    return program;
}
