#ifndef _WIN32
#  define _POSIX_C_SOURCE 200809L
#endif

#include "parser.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// ---- Forward declarations ----
static AstNode *parse_expr(Parser *p);
static AstNode *parse_statement(Parser *p);
static AstNode *parse_block(Parser *p);
static AstType *parse_type(Parser *p);

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
        char *s = tok_str(t);
        n->as.int_lit.value = strtoll(s, NULL, 10);
        free(s);
        return n;
    }
    if (match(p, TOK_FLOAT_LIT)) {
        AstNode *n = ast_new(NODE_FLOAT_LIT, t);
        char *s = tok_str(t);
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
        // Check for struct literal: Ident { field: val, ... }
        if (check(p, TOK_LBRACE)) {
            int saved = p->pos;
            advance_tok(p); // skip {
            if (check(p, TOK_IDENT)) {
                int saved2 = p->pos;
                advance_tok(p);
                if (check(p, TOK_COLON)) {
                    p->pos = saved + 1;
                    AstNode *n = ast_new(NODE_STRUCT_LIT, t);
                    n->as.struct_lit.name = name;
                    int cap = 4, count = 0;
                    FieldInit *fields = malloc(sizeof(FieldInit) * (size_t)cap);
                    do {
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
        AstNode *expr = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')'");
        return expr;
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
            Token field = expect(p, TOK_IDENT, "expected field name after '.'");
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
    expect(p, TOK_LET, "expected 'let'");
    bool is_mut = match(p, TOK_MUT);
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

    AstNode *expr = parse_expr(p);
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
        expect(p, TOK_COLON, "expected ':' after field name");
        AstType *ftype = parse_type(p);
        expect(p, TOK_SEMICOLON, "expected ';' after field");
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
                    expect(p, TOK_COLON, "expected ':' after field name");
                    AstType *ftype = parse_type(p);
                    fields[fcount].name = tok_str(fname);
                    fields[fcount].type = ftype;
                    fcount++;
                } while (match(p, TOK_COMMA));
            }
            expect(p, TOK_RPAREN, "expected ')' after variant fields");
            variants[count].fields = fields;
            variants[count].field_count = fcount;
        }

        expect(p, TOK_SEMICOLON, "expected ';' after variant");
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

static AstNode *parse_declaration(Parser *p) {
    if (check(p, TOK_FN)) return parse_fn_decl(p);
    if (check(p, TOK_STRUCT)) return parse_struct_decl(p);
    if (check(p, TOK_ENUM)) return parse_enum_decl(p);
    if (check(p, TOK_IMPORT)) return parse_import(p);
    return parse_statement(p);
}

AstNode *parser_parse(Parser *p) {
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
