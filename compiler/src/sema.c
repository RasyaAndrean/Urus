#ifndef _WIN32
#  define _POSIX_C_SOURCE 200809L
#endif

#include "sema.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============================================================
// Symbol & Scope
// ============================================================

typedef struct {
    char *name;
    AstType *type;
    Token tok; // For error tracking
    bool is_mut;
    bool is_fn;
    bool is_referenced;
    bool is_builtin; // To avoid making warning on builtin function/variable
    Param *params;
    int param_count;
    AstType *return_type;
    bool is_struct;
    Param *fields;
    int field_count;
    bool is_enum;
    EnumVariant *variants;
    int variant_count;
} Symbol;

typedef struct Scope {
    Symbol *syms;
    int count, cap;
    struct Scope *parent;
} Scope;

typedef struct {
    Scope *current;
    AstType *current_fn_return;
    const char *filename;
    int errors;
    int loop_depth;
} Sema;

// ---- Scope management ----

static Scope *scope_new(Scope *parent) {
    Scope *s = calloc(1, sizeof(Scope));
    s->parent = parent;
    s->cap = 8;
    s->syms = malloc(sizeof(Symbol) * (size_t)s->cap);
    return s;
}

static void scope_free(Scope *s) {
    free(s->syms);
    free(s);
}

static Symbol *scope_lookup_local(Scope *s, const char *name) {
    for (int i = 0; i < s->count; i++) {
        if (strcmp(s->syms[i].name, name) == 0) return &s->syms[i];
    }
    return NULL;
}

static Symbol *scope_lookup(Scope *s, const char *name) {
    for (Scope *cur = s; cur; cur = cur->parent) {
        Symbol *sym = scope_lookup_local(cur, name);
        if (sym) return sym;
    }
    return NULL;
}

static Symbol *scope_add(Scope *s, const char *name, Token tok) {
    if (s->count >= s->cap) {
        s->cap *= 2;
        s->syms = realloc(s->syms, sizeof(Symbol) * (size_t)s->cap);
    }
    Symbol *sym = &s->syms[s->count++];
    memset(sym, 0, sizeof(Symbol));
    sym->name = (char *)name;
    sym->tok = tok;
    return sym;
}

// ---- Reporting system ----

static void sema_error(Sema *ctx, Token *t, const char *fmt, ...) {
    char msg[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    report_error(ctx->filename, t, msg);
    ctx->errors++;
}

static void sema_warn(Sema *ctx, Token *t, const char *fmt, ...) {
    char msg[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    report_warn(ctx->filename, t, msg);
}

// ---- Forward declarations ----
static AstType *check_expr(Sema *ctx, AstNode *node);
static void check_stmt(Sema *ctx, AstNode *node);
static void check_block(Sema *ctx, AstNode *node);

// ---- Expression type checking ----

static AstType *set_type(AstNode *node, AstType *t) {
    node->resolved_type = t;
    return t;
}

static AstType *check_expr(Sema *ctx, AstNode *node) {
    if (!node) return NULL;

    switch (node->kind) {
    case NODE_INT_LIT:
        return set_type(node, ast_type_simple(TYPE_INT));

    case NODE_FLOAT_LIT:
        return set_type(node, ast_type_simple(TYPE_FLOAT));

    case NODE_STR_LIT:
        return set_type(node, ast_type_simple(TYPE_STR));

    case NODE_BOOL_LIT:
        return set_type(node, ast_type_simple(TYPE_BOOL));

    case NODE_IDENT: {
        Symbol *sym = scope_lookup(ctx->current, node->as.ident.name);
        if (!sym) {
            sema_error(ctx, &node->tok, "undefined variable '%s'", node->as.ident.name);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        sym->is_referenced = true;
        return set_type(node, ast_type_clone(sym->type));
    }

    case NODE_BINARY: {
        AstType *lt = check_expr(ctx, node->as.binary.left);
        AstType *rt = check_expr(ctx, node->as.binary.right);
        if (!lt || !rt) return set_type(node, ast_type_simple(TYPE_VOID));

        TokenType op = node->as.binary.op;

        if (op == TOK_EQ || op == TOK_NEQ || op == TOK_LT || op == TOK_GT ||
            op == TOK_LTE || op == TOK_GTE) {
            if (!ast_types_equal(lt, rt)) {
                sema_error(ctx, &node->tok, "cannot compare '%s' with '%s'",
                           ast_type_str(lt), ast_type_str(rt));
            }
            return set_type(node, ast_type_simple(TYPE_BOOL));
        }

        if (op == TOK_AND || op == TOK_OR) {
            if (lt->kind != TYPE_BOOL) {
                sema_error(ctx, &node->tok, "left operand of '%s' must be bool, got '%s'",
                           token_type_name(op), ast_type_str(lt));
            }
            if (rt->kind != TYPE_BOOL) {
                sema_error(ctx, &node->tok, "right operand of '%s' must be bool, got '%s'",
                           token_type_name(op), ast_type_str(rt));
            }
            return set_type(node, ast_type_simple(TYPE_BOOL));
        }

        if (op == TOK_PLUS && lt->kind == TYPE_STR && rt->kind == TYPE_STR) {
            return set_type(node, ast_type_simple(TYPE_STR));
        }

        if (op == TOK_PLUS || op == TOK_MINUS || op == TOK_STAR ||
            op == TOK_SLASH || op == TOK_PERCENT) {
            if (!ast_types_equal(lt, rt)) {
                sema_error(ctx, &node->tok, "mismatched types in '%s': '%s' and '%s'",
                           token_type_name(op), ast_type_str(lt), ast_type_str(rt));
                return set_type(node, ast_type_clone(lt));
            }
            if (lt->kind != TYPE_INT && lt->kind != TYPE_FLOAT) {
                sema_error(ctx, &node->tok, "operator '%s' requires numeric types, got '%s'",
                           token_type_name(op), ast_type_str(lt));
            }
            return set_type(node, ast_type_clone(lt));
        }

        return set_type(node, ast_type_simple(TYPE_VOID));
    }

    case NODE_UNARY: {
        AstType *t = check_expr(ctx, node->as.unary.operand);
        if (!t) return set_type(node, ast_type_simple(TYPE_VOID));
        if (node->as.unary.op == TOK_NOT) {
            if (t->kind != TYPE_BOOL) {
                sema_error(ctx, &node->tok, "'!' requires bool, got '%s'", ast_type_str(t));
            }
            return set_type(node, ast_type_simple(TYPE_BOOL));
        }
        if (node->as.unary.op == TOK_MINUS) {
            if (t->kind != TYPE_INT && t->kind != TYPE_FLOAT) {
                sema_error(ctx, &node->tok, "unary '-' requires numeric type, got '%s'", ast_type_str(t));
            }
            return set_type(node, ast_type_clone(t));
        }
        return set_type(node, ast_type_clone(t));
    }

    case NODE_CALL: {
        // Method call: obj.method(args) -> StructName_method(obj, args)
        if (node->as.call.callee->kind == NODE_FIELD_ACCESS) {
            AstNode *obj = node->as.call.callee->as.field_access.object;
            const char *method = node->as.call.callee->as.field_access.field;
            AstType *obj_type = check_expr(ctx, obj);

            if (obj_type && obj_type->kind == TYPE_NAMED) {
                // Look for StructName_method
                char fn_name_buf[256];
                snprintf(fn_name_buf, sizeof(fn_name_buf), "%s_%s", obj_type->name, method);
                Symbol *method_sym = scope_lookup(ctx->current, fn_name_buf);

                if (method_sym && method_sym->is_fn) {
                    // Rewrite: change callee to ident, prepend obj as first arg
                    node->as.call.callee->kind = NODE_IDENT;
                    node->as.call.callee->as.ident.name = strdup(fn_name_buf);

                    int new_count = node->as.call.arg_count + 1;
                    AstNode **new_args = malloc(sizeof(AstNode *) * (size_t)new_count);
                    new_args[0] = obj;
                    for (int i = 0; i < node->as.call.arg_count; i++)
                        new_args[i + 1] = node->as.call.args[i];
                    free(node->as.call.args);
                    node->as.call.args = new_args;
                    node->as.call.arg_count = new_count;
                    // Fall through to normal call checking below
                } else {
                    sema_error(ctx, &node->tok, "no method '%s' on type '%s'",
                               method, obj_type->name);
                    for (int i = 0; i < node->as.call.arg_count; i++)
                        check_expr(ctx, node->as.call.args[i]);
                    return set_type(node, ast_type_simple(TYPE_VOID));
                }
            } else {
                sema_error(ctx, &node->tok, "method call on non-struct type '%s'",
                           ast_type_str(obj_type));
                for (int i = 0; i < node->as.call.arg_count; i++)
                    check_expr(ctx, node->as.call.args[i]);
                return set_type(node, ast_type_simple(TYPE_VOID));
            }
        }

        if (node->as.call.callee->kind != NODE_IDENT) {
            sema_error(ctx, &node->tok, "callee must be a function name");
            for (int i = 0; i < node->as.call.arg_count; i++)
                check_expr(ctx, node->as.call.args[i]);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }

        const char *fn_name = node->as.call.callee->as.ident.name;
        Symbol *sym = scope_lookup(ctx->current, fn_name);
        sym->is_referenced = true;
        if (!sym) {
            sema_error(ctx, &node->tok, "undefined function '%s'", fn_name);
            for (int i = 0; i < node->as.call.arg_count; i++)
                check_expr(ctx, node->as.call.args[i]);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        if (!sym->is_fn) {
            sema_error(ctx, &node->tok, "'%s' is not a function", fn_name);
            for (int i = 0; i < node->as.call.arg_count; i++)
                check_expr(ctx, node->as.call.args[i]);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }

        if (sym->param_count >= 0 && node->as.call.arg_count != sym->param_count) {
            sema_error(ctx, &node->tok, "'%s' expects %d arguments, got %d",
                       fn_name, sym->param_count, node->as.call.arg_count);
        }

        for (int i = 0; i < node->as.call.arg_count; i++) {
            AstType *at = check_expr(ctx, node->as.call.args[i]);
            if (sym->param_count > 0 && i < sym->param_count && sym->params) {
                if (!ast_types_equal(at, sym->params[i].type)) {
                    if (sym->params[i].type && sym->params[i].type->kind != TYPE_VOID) {
                        sema_error(ctx, &node->tok,
                                   "argument %d of '%s': expected '%s', got '%s'",
                                   i + 1, fn_name,
                                   ast_type_str(sym->params[i].type),
                                   ast_type_str(at));
                    }
                }
            }
        }

        // Special: unwrap returns the ok_type of the Result argument
        AstType *final_return = sym->return_type;
        if (fn_name && strcmp(fn_name, "unwrap") == 0 && node->as.call.arg_count > 0) {
            AstType *arg_type = node->as.call.args[0]->resolved_type;
            if (arg_type && arg_type->kind == TYPE_RESULT && arg_type->ok_type) {
                final_return = arg_type->ok_type;
            }
        }
        if (fn_name && strcmp(fn_name, "unwrap_err") == 0 && node->as.call.arg_count > 0) {
            AstType *arg_type = node->as.call.args[0]->resolved_type;
            if (arg_type && arg_type->kind == TYPE_RESULT && arg_type->err_type) {
                final_return = arg_type->err_type;
            }
        }

        node->as.call.callee->resolved_type = ast_type_clone(final_return);
        return set_type(node, ast_type_clone(final_return));
    }

    case NODE_FIELD_ACCESS: {
        AstType *obj_type = check_expr(ctx, node->as.field_access.object);
        if (!obj_type || obj_type->kind != TYPE_NAMED) {
            sema_error(ctx, &node->tok, "field access on non-struct type '%s'",
                       ast_type_str(obj_type));
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        Symbol *st = scope_lookup(ctx->current, obj_type->name);
        if (!st || !st->is_struct) {
            sema_error(ctx, &node->tok, "unknown struct '%s'", obj_type->name);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        for (int i = 0; i < st->field_count; i++) {
            if (strcmp(st->fields[i].name, node->as.field_access.field) == 0) {
                return set_type(node, ast_type_clone(st->fields[i].type));
            }
        }
        sema_error(ctx, &node->tok, "struct '%s' has no field '%s'",
                   obj_type->name, node->as.field_access.field);
        return set_type(node, ast_type_simple(TYPE_VOID));
    }

    case NODE_INDEX: {
        AstType *obj_type = check_expr(ctx, node->as.index_expr.object);
        AstType *idx_type = check_expr(ctx, node->as.index_expr.index);
        if (idx_type && idx_type->kind != TYPE_INT) {
            sema_error(ctx, &node->tok, "array index must be int, got '%s'", ast_type_str(idx_type));
        }
        if (!obj_type || obj_type->kind != TYPE_ARRAY) {
            sema_error(ctx, &node->tok, "index operator on non-array type '%s'", ast_type_str(obj_type));
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        return set_type(node, ast_type_clone(obj_type->element));
    }

    case NODE_ARRAY_LIT: {
        AstType *elem_type = NULL;
        for (int i = 0; i < node->as.array_lit.count; i++) {
            AstType *t = check_expr(ctx, node->as.array_lit.elements[i]);
            if (i == 0) {
                elem_type = t;
            } else if (!ast_types_equal(elem_type, t)) {
                sema_error(ctx, &node->tok, "array element type mismatch: expected '%s', got '%s'",
                           ast_type_str(elem_type), ast_type_str(t));
            }
        }
        if (!elem_type) elem_type = ast_type_simple(TYPE_INT);
        return set_type(node, ast_type_array(ast_type_clone(elem_type)));
    }

    case NODE_STRUCT_LIT: {
        const char *name = node->as.struct_lit.name;
        Symbol *st = scope_lookup(ctx->current, name);
        if (!st || !st->is_struct) {
            sema_error(ctx, &node->tok, "unknown struct '%s'", name);
            for (int i = 0; i < node->as.struct_lit.field_count; i++)
                check_expr(ctx, node->as.struct_lit.fields[i].value);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        if (node->as.struct_lit.field_count != st->field_count) {
            sema_error(ctx, &node->tok, "struct '%s' has %d fields, got %d",
                       name, st->field_count, node->as.struct_lit.field_count);
        }
        for (int i = 0; i < node->as.struct_lit.field_count; i++) {
            AstType *vt = check_expr(ctx, node->as.struct_lit.fields[i].value);
            bool found = false;
            for (int j = 0; j < st->field_count; j++) {
                if (strcmp(node->as.struct_lit.fields[i].name, st->fields[j].name) == 0) {
                    found = true;
                    if (!ast_types_equal(vt, st->fields[j].type)) {
                        sema_error(ctx, &node->tok, "field '%s': expected '%s', got '%s'",
                                   st->fields[j].name,
                                   ast_type_str(st->fields[j].type),
                                   ast_type_str(vt));
                    }
                    break;
                }
            }
            if (!found) {
                sema_error(ctx, &node->tok, "struct '%s' has no field '%s'",
                           name, node->as.struct_lit.fields[i].name);
            }
        }
        return set_type(node, ast_type_named(name));
    }

    case NODE_ENUM_INIT: {
        const char *ename = node->as.enum_init.enum_name;
        Symbol *sym = scope_lookup(ctx->current, ename);
        if (!sym || !sym->is_enum) {
            sema_error(ctx, &node->tok, "unknown enum '%s'", ename);
            for (int i = 0; i < node->as.enum_init.arg_count; i++)
                check_expr(ctx, node->as.enum_init.args[i]);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        // Find variant
        const char *vname = node->as.enum_init.variant_name;
        EnumVariant *variant = NULL;
        for (int i = 0; i < sym->variant_count; i++) {
            if (strcmp(sym->variants[i].name, vname) == 0) {
                variant = &sym->variants[i];
                break;
            }
        }
        if (!variant) {
            sema_error(ctx, &node->tok, "enum '%s' has no variant '%s'", ename, vname);
            for (int i = 0; i < node->as.enum_init.arg_count; i++)
                check_expr(ctx, node->as.enum_init.args[i]);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        if (node->as.enum_init.arg_count != variant->field_count) {
            sema_error(ctx, &node->tok, "variant '%s.%s' expects %d args, got %d",
                       ename, vname, variant->field_count, node->as.enum_init.arg_count);
        }
        for (int i = 0; i < node->as.enum_init.arg_count && i < variant->field_count; i++) {
            AstType *at = check_expr(ctx, node->as.enum_init.args[i]);
            if (!ast_types_equal(at, variant->fields[i].type)) {
                sema_error(ctx, &node->tok, "variant '%s.%s' arg %d: expected '%s', got '%s'",
                           ename, vname, i + 1,
                           ast_type_str(variant->fields[i].type), ast_type_str(at));
            }
        }
        return set_type(node, ast_type_named(ename));
    }

    case NODE_OK_EXPR: {
        AstType *val_type = check_expr(ctx, node->as.result_expr.value);
        // We don't know the error type here, use void as placeholder
        return set_type(node, ast_type_result(val_type ? ast_type_clone(val_type) : ast_type_simple(TYPE_VOID),
                                               ast_type_simple(TYPE_STR)));
    }

    case NODE_ERR_EXPR: {
        AstType *val_type = check_expr(ctx, node->as.result_expr.value);
        return set_type(node, ast_type_result(ast_type_simple(TYPE_VOID),
                                               val_type ? ast_type_clone(val_type) : ast_type_simple(TYPE_STR)));
    }

    default:
        return set_type(node, ast_type_simple(TYPE_VOID));
    }
}

// ---- Statement checking ----

static void check_stmt(Sema *ctx, AstNode *node) {
    if (!node) return;

    switch (node->kind) {
    case NODE_LET_STMT: {
        AstType *init_type = check_expr(ctx, node->as.let_stmt.init);
        AstType *decl_type = node->as.let_stmt.type;

        if (init_type && decl_type && !ast_types_equal(init_type, decl_type)) {
            // Allow Result type coercion (Ok/Err assign to Result<T,E>)
            if (!(decl_type->kind == TYPE_RESULT &&
                  (node->as.let_stmt.init->kind == NODE_OK_EXPR ||
                   node->as.let_stmt.init->kind == NODE_ERR_EXPR))) {
                sema_error(ctx, &node->tok, "cannot assign '%s' to variable of type '%s'",
                           ast_type_str(init_type), ast_type_str(decl_type));
            }
        }

        if (scope_lookup_local(ctx->current, node->as.let_stmt.name)) {
            sema_error(ctx, &node->tok, "variable '%s' already declared in this scope",
                       node->as.let_stmt.name);
        }

        Symbol *sym = scope_add(ctx->current, node->as.let_stmt.name, node->tok);
        sym->type = decl_type;
        sym->is_mut = node->as.let_stmt.is_mut;
        break;
    }

    case NODE_ASSIGN_STMT: {
        AstType *target_type = check_expr(ctx, node->as.assign_stmt.target);
        AstType *val_type = check_expr(ctx, node->as.assign_stmt.value);

        if (node->as.assign_stmt.target->kind == NODE_IDENT) {
            Symbol *sym = scope_lookup(ctx->current, node->as.assign_stmt.target->as.ident.name);
            if (sym && !sym->is_mut && !sym->is_fn) {
                sema_error(ctx, &node->tok, "cannot assign to immutable variable '%s'",
                           node->as.assign_stmt.target->as.ident.name);
            }
        }

        if (target_type && val_type && !ast_types_equal(target_type, val_type)) {
            sema_error(ctx, &node->tok, "cannot assign '%s' to '%s'",
                       ast_type_str(val_type), ast_type_str(target_type));
        }
        break;
    }

    case NODE_IF_STMT: {
        AstType *cond = check_expr(ctx, node->as.if_stmt.condition);
        if (cond && cond->kind != TYPE_BOOL) {
            sema_error(ctx, &node->tok, "if condition must be bool, got '%s'", ast_type_str(cond));
        }
        check_block(ctx, node->as.if_stmt.then_block);
        if (node->as.if_stmt.else_branch) {
            if (node->as.if_stmt.else_branch->kind == NODE_IF_STMT) {
                check_stmt(ctx, node->as.if_stmt.else_branch);
            } else {
                check_block(ctx, node->as.if_stmt.else_branch);
            }
        }
        break;
    }

    case NODE_WHILE_STMT: {
        AstType *cond = check_expr(ctx, node->as.while_stmt.condition);
        if (cond && cond->kind != TYPE_BOOL) {
            sema_error(ctx, &node->tok, "while condition must be bool, got '%s'", ast_type_str(cond));
        }
        ctx->loop_depth++;
        check_block(ctx, node->as.while_stmt.body);
        ctx->loop_depth--;
        break;
    }

    case NODE_FOR_STMT: {
        if (node->as.for_stmt.is_foreach) {
            // For-each over array
            AstType *iter_type = check_expr(ctx, node->as.for_stmt.iterable);
            if (!iter_type || iter_type->kind != TYPE_ARRAY) {
                sema_error(ctx, &node->tok, "for-each requires array type, got '%s'",
                           ast_type_str(iter_type));
            }
            AstType *elem_type = (iter_type && iter_type->kind == TYPE_ARRAY && iter_type->element)
                                 ? ast_type_clone(iter_type->element)
                                 : ast_type_simple(TYPE_INT);

            Scope *body_scope = scope_new(ctx->current);
            ctx->current = body_scope;
            Symbol *loop_var = scope_add(body_scope, node->as.for_stmt.var_name, node->tok);
            loop_var->type = elem_type;
            loop_var->is_mut = false;

            ctx->loop_depth++;
            AstNode *body = node->as.for_stmt.body;
            for (int i = 0; i < body->as.block.stmt_count; i++) {
                check_stmt(ctx, body->as.block.stmts[i]);
            }
            ctx->loop_depth--;

            ctx->current = body_scope->parent;
            scope_free(body_scope);
        } else {
            // Range for
            AstType *start = check_expr(ctx, node->as.for_stmt.start);
            AstType *end = check_expr(ctx, node->as.for_stmt.end);
            if (start && start->kind != TYPE_INT) {
                sema_error(ctx, &node->tok, "for range start must be int, got '%s'", ast_type_str(start));
            }
            if (end && end->kind != TYPE_INT) {
                sema_error(ctx, &node->tok, "for range end must be int, got '%s'", ast_type_str(end));
            }

            Scope *body_scope = scope_new(ctx->current);
            ctx->current = body_scope;
            Symbol *loop_var = scope_add(body_scope, node->as.for_stmt.var_name, node->tok);
            loop_var->type = ast_type_simple(TYPE_INT);
            loop_var->is_mut = false;

            ctx->loop_depth++;
            AstNode *body = node->as.for_stmt.body;
            for (int i = 0; i < body->as.block.stmt_count; i++) {
                check_stmt(ctx, body->as.block.stmts[i]);
            }
            ctx->loop_depth--;

            ctx->current = body_scope->parent;
            scope_free(body_scope);
        }
        break;
    }

    case NODE_RETURN_STMT: {
        if (node->as.return_stmt.value) {
            AstType *t = check_expr(ctx, node->as.return_stmt.value);
            if (ctx->current_fn_return && t &&
                !ast_types_equal(t, ctx->current_fn_return)) {
                // Allow Result coercion
                if (!(ctx->current_fn_return->kind == TYPE_RESULT &&
                      (node->as.return_stmt.value->kind == NODE_OK_EXPR ||
                       node->as.return_stmt.value->kind == NODE_ERR_EXPR))) {
                    sema_error(ctx, &node->tok, "return type mismatch: expected '%s', got '%s'",
                               ast_type_str(ctx->current_fn_return), ast_type_str(t));
                }
            }
        } else {
            if (ctx->current_fn_return && ctx->current_fn_return->kind != TYPE_VOID) {
                sema_error(ctx, &node->tok, "function expects return value of type '%s'",
                           ast_type_str(ctx->current_fn_return));
            }
        }
        break;
    }

    case NODE_BREAK_STMT:
        if (ctx->loop_depth == 0) {
            sema_error(ctx, &node->tok, "break outside of loop");
        }
        break;

    case NODE_CONTINUE_STMT:
        if (ctx->loop_depth == 0) {
            sema_error(ctx, &node->tok, "continue outside of loop");
        }
        break;

    case NODE_EXPR_STMT:
        check_expr(ctx, node->as.expr_stmt.expr);
        break;

    case NODE_BLOCK:
        check_block(ctx, node);
        break;

    case NODE_MATCH: {
        AstType *target_type = check_expr(ctx, node->as.match_stmt.target);
        if (!target_type || target_type->kind != TYPE_NAMED) {
            sema_error(ctx, &node->tok, "match target must be an enum type, got '%s'",
                       ast_type_str(target_type));
            break;
        }
        Symbol *enum_sym = scope_lookup(ctx->current, target_type->name);
        if (!enum_sym || !enum_sym->is_enum) {
            sema_error(ctx, &node->tok, "match target type '%s' is not an enum", target_type->name);
            break;
        }

        for (int i = 0; i < node->as.match_stmt.arm_count; i++) {
            MatchArm *arm = &node->as.match_stmt.arms[i];
            // Find variant
            EnumVariant *variant = NULL;
            for (int v = 0; v < enum_sym->variant_count; v++) {
                if (strcmp(enum_sym->variants[v].name, arm->variant_name) == 0) {
                    variant = &enum_sym->variants[v];
                    break;
                }
            }
            if (!variant) {
                sema_error(ctx, &node->tok, "enum '%s' has no variant '%s'",
                           target_type->name, arm->variant_name);
                continue;
            }
            if (arm->binding_count != variant->field_count) {
                sema_error(ctx, &node->tok, "variant '%s' has %d fields, got %d bindings",
                           arm->variant_name, variant->field_count, arm->binding_count);
            }

            // Store binding types for codegen
            if (arm->binding_count > 0) {
                arm->binding_types = malloc(sizeof(AstType *) * (size_t)arm->binding_count);
                for (int b = 0; b < arm->binding_count && b < variant->field_count; b++) {
                    arm->binding_types[b] = ast_type_clone(variant->fields[b].type);
                }
            }

            // Check arm body with bindings in scope
            Scope *arm_scope = scope_new(ctx->current);
            ctx->current = arm_scope;
            for (int b = 0; b < arm->binding_count && b < variant->field_count; b++) {
                Symbol *binding = scope_add(arm_scope, arm->bindings[b], node->tok);
                binding->type = ast_type_clone(variant->fields[b].type);
                binding->is_mut = false;
            }
            AstNode *arm_body = arm->body;
            for (int s = 0; s < arm_body->as.block.stmt_count; s++) {
                check_stmt(ctx, arm_body->as.block.stmts[s]);
            }
            ctx->current = arm_scope->parent;
            scope_free(arm_scope);
        }
        break;
    }

    default:
        break;
    }
}

static void check_unused_symbols(Sema *ctx, Scope *s) {
    for (int i = 0; i < s->count; i++) {
        Symbol *sym = &s->syms[i];
        if (sym->name[0] != '_' && strcmp(sym->name, "main") != 0 && !sym->is_builtin &&
                !sym->is_referenced) {
            char *type = "variable";
            if (sym->is_fn) type = "function";
            else if (sym->is_struct) type = "struct";
            else if (sym->is_enum) type = "enum";

            if (type[0] != 'v') report(ctx->filename, "In %s '%s':", type, sym->name);
            sema_warn(ctx, &sym->tok, "unused %s '%s'", type, sym->name);
        }
    }
}

static void check_block(Sema *ctx, AstNode *node) {
    Scope *block_scope = scope_new(ctx->current);
    ctx->current = block_scope;
    for (int i = 0; i < node->as.block.stmt_count; i++) {
        check_stmt(ctx, node->as.block.stmts[i]);
    }
    check_unused_symbols(ctx, block_scope);
    ctx->current = block_scope->parent;
    scope_free(block_scope);
}

// ---- Builtins ----

static void add_builtin(Scope *global, const char *name, AstType *ret,
                        int nparams, ...) {
    Symbol *s = scope_add(global, name, (Token){0});
    s->is_fn = true;
    s->is_builtin = true;
    s->param_count = nparams;
    s->return_type = ret;
    if (nparams > 0) {
        s->params = calloc((size_t)nparams, sizeof(Param));
        va_list args;
        va_start(args, nparams);
        for (int i = 0; i < nparams; i++) {
            s->params[i].name = va_arg(args, char *);
            s->params[i].type = va_arg(args, AstType *);
        }
        va_end(args);
    }
}

#define T_INT   ast_type_simple(TYPE_INT)
#define T_FLOAT ast_type_simple(TYPE_FLOAT)
#define T_BOOL  ast_type_simple(TYPE_BOOL)
#define T_STR   ast_type_simple(TYPE_STR)
#define T_VOID  ast_type_simple(TYPE_VOID)
#define T_ANY   NULL

static void register_builtins(Scope *global) {
    add_builtin(global, "print",    T_VOID,  1, "value", T_ANY);
    add_builtin(global, "to_str",   T_STR,   1, "value", T_ANY);
    add_builtin(global, "to_int",   T_INT,   1, "value", T_ANY);
    add_builtin(global, "to_float", T_FLOAT, 1, "value", T_ANY);
    add_builtin(global, "len",      T_INT,   1, "arr",   T_ANY);
    add_builtin(global, "push",     T_VOID,  2, "arr", T_ANY, "value", T_ANY);
    add_builtin(global, "pop",      T_VOID,  1, "arr", T_ANY);

    add_builtin(global, "str_len",        T_INT,  1, "s", T_STR);
    add_builtin(global, "str_slice",      T_STR,  3, "s", T_STR, "start", T_INT, "end", T_INT);
    add_builtin(global, "str_find",       T_INT,  2, "s", T_STR, "sub", T_STR);
    add_builtin(global, "str_contains",   T_BOOL, 2, "s", T_STR, "sub", T_STR);
    add_builtin(global, "str_upper",      T_STR,  1, "s", T_STR);
    add_builtin(global, "str_lower",      T_STR,  1, "s", T_STR);
    add_builtin(global, "str_trim",       T_STR,  1, "s", T_STR);
    add_builtin(global, "str_replace",    T_STR,  3, "s", T_STR, "old", T_STR, "new", T_STR);
    add_builtin(global, "str_starts_with",T_BOOL, 2, "s", T_STR, "prefix", T_STR);
    add_builtin(global, "str_ends_with",  T_BOOL, 2, "s", T_STR, "suffix", T_STR);
    add_builtin(global, "str_split",      ast_type_array(T_STR), 2, "s", T_STR, "delim", T_STR);
    add_builtin(global, "char_at",        T_STR,  2, "s", T_STR, "i", T_INT);

    add_builtin(global, "abs",  T_INT,   1, "x", T_INT);
    add_builtin(global, "fabs", T_FLOAT, 1, "x", T_FLOAT);
    add_builtin(global, "sqrt", T_FLOAT, 1, "x", T_FLOAT);
    add_builtin(global, "pow",  T_FLOAT, 2, "x", T_FLOAT, "y", T_FLOAT);
    add_builtin(global, "min",  T_INT,   2, "a", T_INT, "b", T_INT);
    add_builtin(global, "max",  T_INT,   2, "a", T_INT, "b", T_INT);
    add_builtin(global, "fmin", T_FLOAT, 2, "a", T_FLOAT, "b", T_FLOAT);
    add_builtin(global, "fmax", T_FLOAT, 2, "a", T_FLOAT, "b", T_FLOAT);

    add_builtin(global, "input",       T_STR,  0);
    add_builtin(global, "read_file",   T_STR,  1, "path", T_STR);
    add_builtin(global, "write_file",  T_VOID, 2, "path", T_STR, "content", T_STR);
    add_builtin(global, "append_file", T_VOID, 2, "path", T_STR, "content", T_STR);

    add_builtin(global, "exit",   T_VOID, 1, "code", T_INT);
    add_builtin(global, "assert", T_VOID, 2, "cond", T_BOOL, "msg", T_STR);

    add_builtin(global, "is_ok",  T_BOOL, 1, "r", T_ANY);
    add_builtin(global, "is_err", T_BOOL, 1, "r", T_ANY);
    add_builtin(global, "unwrap", T_ANY,  1, "r", T_ANY);
    add_builtin(global, "unwrap_err", T_ANY, 1, "r", T_ANY);
}

// ---- Top-level ----

bool sema_analyze(AstNode *program, const char *filename) {
    Sema ctx = {0};
    Scope *global = scope_new(NULL);
    ctx.filename = filename;
    ctx.current = global;

    register_builtins(global);

    // Pass 1: register all structs, enums, and function signatures
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_STRUCT_DECL) {
            if (scope_lookup_local(global, d->as.struct_decl.name)) {
                sema_error(&ctx, &d->tok, "duplicate struct '%s'", d->as.struct_decl.name);
                continue;
            }
            Symbol *s = scope_add(global, d->as.struct_decl.name, d->tok);
            s->is_struct = true;
            s->fields = d->as.struct_decl.fields;
            s->field_count = d->as.struct_decl.field_count;
            s->type = ast_type_named(d->as.struct_decl.name);
        } else if (d->kind == NODE_ENUM_DECL) {
            if (scope_lookup_local(global, d->as.enum_decl.name)) {
                sema_error(&ctx, &d->tok, "duplicate enum '%s'", d->as.enum_decl.name);
                continue;
            }
            Symbol *s = scope_add(global, d->as.enum_decl.name, d->tok);
            s->is_enum = true;
            s->variants = d->as.enum_decl.variants;
            s->variant_count = d->as.enum_decl.variant_count;
            s->type = ast_type_named(d->as.enum_decl.name);
        } else if (d->kind == NODE_FN_DECL) {
            if (scope_lookup_local(global, d->as.fn_decl.name)) {
                sema_error(&ctx, &d->tok, "duplicate function '%s'", d->as.fn_decl.name);
                continue;
            }
            else if (d->as.fn_decl.return_type->kind == TYPE_NAMED &&
                    !scope_lookup(ctx.current, d->as.fn_decl.return_type->name)) {
                sema_error(&ctx, &d->tok, "function '%s' has unknown return type '%s'", d->as.fn_decl.name,
                           d->as.fn_decl.return_type->name);
                continue;
            }
            Symbol *s = scope_add(global, d->as.fn_decl.name, d->tok);
            s->is_fn = true;
            s->params = d->as.fn_decl.params;
            s->param_count = d->as.fn_decl.param_count;
            s->return_type = d->as.fn_decl.return_type;
        }
    }

    // Pass 2: check function bodies
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_FN_DECL) {
            Scope *fn_scope = scope_new(global);
            ctx.current = fn_scope;
            ctx.current_fn_return = d->as.fn_decl.return_type;

            for (int j = 0; j < d->as.fn_decl.param_count; j++) {
                Symbol *p = scope_add(fn_scope, d->as.fn_decl.params[j].name, d->tok);
                p->type = d->as.fn_decl.params[j].type;
                p->is_mut = false;
            }

            AstNode *body = d->as.fn_decl.body;
            for (int j = 0; j < body->as.block.stmt_count; j++) {
                check_stmt(&ctx, body->as.block.stmts[j]);
            }

            check_unused_symbols(&ctx, fn_scope);
            ctx.current = global;
            scope_free(fn_scope);
        }
    }

    check_unused_symbols(&ctx, global);
    scope_free(global);
    return ctx.errors == 0;
}
