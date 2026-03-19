#ifndef _WIN32
#  define _POSIX_C_SOURCE 200809L
#endif

#include "sema.h"
#include "./scope.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ---- Reporting system ----

static void sema_error(SemaCtx *ctx, Token *t, const char *fmt, ...) {
    char msg[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    report_error(ctx->filename, t, msg);
    ctx->errors++;
}

static void sema_warn(SemaCtx *ctx, Token *t, const char *fmt, ...) {
    char msg[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    report_warn(ctx->filename, t, msg);
}

// ---- Forward declarations ----
static AstType *check_expr(SemaCtx *ctx, AstNode *node);
static void check_stmt(SemaCtx *ctx, AstNode *node);
static void check_block(SemaCtx *ctx, AstNode *node);

// ---- Expression type checking ----

static AstType *set_type(AstNode *node, AstType *t) {
    node->resolved_type = t;
    return t;
}

static AstType *check_expr(SemaCtx *ctx, AstNode *node) {
    if (!node) return NULL;

    // Warn about unnecessary parentheses: ((expr)), (literal), (ident)
    if (node->parenthesized) {
        if (node->kind == NODE_INT_LIT || node->kind == NODE_FLOAT_LIT ||
            node->kind == NODE_BOOL_LIT || node->kind == NODE_STR_LIT ||
            node->kind == NODE_IDENT) {
            sema_warn(ctx, &node->tok, "unnecessary parentheses");
        }
    }

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
        SemaSymbol *sym = scope_lookup(ctx->current, node->as.ident.name);
        if (!sym || (sym->is_fn || sym->is_enum || sym->is_struct)) {
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
                sema_error(ctx, &node->as.binary.left->tok, "cannot compare '%s' with '%s'",
                           ast_type_str(lt), ast_type_str(rt));
            }
            return set_type(node, ast_type_simple(TYPE_BOOL));
        }

        if (op == TOK_AND || op == TOK_OR) {
            if (lt->kind != TYPE_BOOL) {
                sema_error(ctx, &node->as.binary.left->tok, "left operand of '%s' must be bool, got '%s'",
                           token_type_name(op), ast_type_str(lt));
            }
            if (rt->kind != TYPE_BOOL) {
                sema_error(ctx, &node->as.binary.right->tok, "right operand of '%s' must be bool, got '%s'",
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
            if (op != TOK_PLUS && lt->kind != TYPE_INT && lt->kind != TYPE_FLOAT) {
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
                sema_error(ctx, &node->as.unary.operand->tok, "'!' requires bool, got '%s'", ast_type_str(t));
            }
            return set_type(node, ast_type_simple(TYPE_BOOL));
        }
        if (node->as.unary.op == TOK_MINUS) {
            if (t->kind != TYPE_INT && t->kind != TYPE_FLOAT) {
                sema_error(ctx, &node->as.unary.operand->tok, "unary '-' requires numeric type, got '%s'", ast_type_str(t));
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
                SemaSymbol *method_sym = scope_lookup(ctx->current, fn_name_buf);

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
            sema_error(ctx, &node->as.call.callee->tok, "callee must be a function name");
            for (int i = 0; i < node->as.call.arg_count; i++)
                check_expr(ctx, node->as.call.args[i]);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }

        const char *fn_name = node->as.call.callee->as.ident.name;
        SemaSymbol *sym = scope_lookup(ctx->current, fn_name);
        if (!sym) {
            sema_error(ctx, &node->as.call.callee->tok, "undefined function '%s'", fn_name);
            for (int i = 0; i < node->as.call.arg_count; i++)
                check_expr(ctx, node->as.call.args[i]);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        if (!sym->is_fn) {
            sema_error(ctx, &node->as.call.callee->tok, "'%s' is not a function", fn_name);
            for (int i = 0; i < node->as.call.arg_count; i++)
                check_expr(ctx, node->as.call.args[i]);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }

        sym->is_referenced = true;
        int min_args = 0; //minimal arguments (argument with default value will not counted)
        for (int i = 0; i < sym->param_count; i++) {
            if (sym->params[i].default_value == NULL) {
                min_args++;
            }
        }

        if (node->as.call.arg_count < min_args || node->as.call.arg_count > sym->param_count) {
            sema_error(ctx, &node->as.call.callee->tok, "'%s' expects %d arguments, got %d",
                       fn_name, sym->param_count, node->as.call.arg_count);
        }

        for (int i = 0; i < node->as.call.arg_count; i++) {
            AstType *at = check_expr(ctx, node->as.call.args[i]);
            if (sym->param_count > 0 && i < sym->param_count && sym->params) {
                if (!ast_types_equal(at, sym->params[i].type)) {
                    if (sym->params[i].type && sym->params[i].type->kind != TYPE_VOID) {
                        sema_error(ctx, &node->as.call.args[i]->tok,
                                   "argument %d of '%s': expected '%s', got '%s'",
                                   i + 1, fn_name,
                                   ast_type_str(sym->params[i].type),
                                   ast_type_str(at));
                    }
                }
            }
        }

		// Inject default parameter types to args
		if (node->as.call.arg_count < sym->param_count) {
            AstNode **new_args = malloc(sizeof(AstNode *) * (size_t)sym->param_count);

            for (int i = 0; i < node->as.call.arg_count; i++) {
                new_args[i] = node->as.call.args[i];
            }

            for (int i = node->as.call.arg_count; i < sym->param_count; i++) {
                if (sym->params[i].default_value != NULL) {
                    new_args[i] = sym->params[i].default_value;
                    new_args[i]->ref_count++;
                }
            }

            free(node->as.call.args);
            node->as.call.args = new_args;
            node->as.call.arg_count = sym->param_count;
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
        if (obj_type && obj_type->kind == TYPE_TUPLE) {
            const char *field = node->as.field_access.field;
            char *end;
            long idx = strtol(field, &end, 10);
            if (*end != '\0' || idx < 0) {
                sema_error(ctx, &node->tok, "invalid tuple index '%s'", field);
                return set_type(node, ast_type_simple(TYPE_VOID));
            }
            if (idx >= obj_type->element_count) {
                sema_error(ctx, &node->tok, "tuple index %ld out of range (tuple has %d elements)",
                           idx, obj_type->element_count);
                return set_type(node, ast_type_simple(TYPE_VOID));
            }
            return set_type(node, ast_type_clone(obj_type->element_types[idx]));
        }
        if (!obj_type || obj_type->kind != TYPE_NAMED) {
            sema_error(ctx, &node->as.field_access.object->tok, "field access on non-struct type '%s'",
                       ast_type_str(obj_type));
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        SemaSymbol *st = scope_lookup(ctx->current, obj_type->name);
        if (!st || !st->is_struct) {
            if (!st->is_struct) {
                sema_error(ctx, &node->tok, "'%s' is not struct", obj_type->name);
            } else {
                sema_error(ctx, &node->tok, "unknown struct '%s'", obj_type->name);
            }
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        for (int i = 0; i < st->field_count; i++) {
            if (strcmp(st->fields[i].name, node->as.field_access.field) == 0) {
                st->is_referenced = true;
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
            sema_error(ctx, &node->as.index_expr.index->tok, "array index must be int, got '%s'", ast_type_str(idx_type));
        }
        if (!obj_type || obj_type->kind != TYPE_ARRAY) {
            sema_error(ctx, &node->as.index_expr.object->tok, "index operator on non-array type '%s'", ast_type_str(obj_type));
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
                sema_error(ctx, &node->as.array_lit.elements[i]->tok, "array element type mismatch: expected '%s', got '%s'",
                           ast_type_str(elem_type), ast_type_str(t));
            }
        }
        // element type is default considered as INT type
        if (!elem_type) elem_type = ast_type_simple(TYPE_INT);
        return set_type(node, ast_type_array(ast_type_clone(elem_type)));
    }

    case NODE_STRUCT_LIT: {
        const char *name = node->as.struct_lit.name;
        SemaSymbol *st = scope_lookup(ctx->current, name);
        if (!st || !st->is_struct) {
            sema_error(ctx, &node->tok, "unknown struct '%s'", name);
            for (int i = 0; i < node->as.struct_lit.field_count; i++)
                check_expr(ctx, node->as.struct_lit.fields[i].value);
            if (node->as.struct_lit.spread)
                check_expr(ctx, node->as.struct_lit.spread);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        // Validate spread expression type
        if (node->as.struct_lit.spread) {
            AstType *spread_type = check_expr(ctx, node->as.struct_lit.spread);
            if (spread_type->kind != TYPE_NAMED || strcmp(spread_type->name, name) != 0) {
                sema_error(ctx, &node->tok, "spread expression must be of type '%s', got '%s'",
                           name, ast_type_str(spread_type));
            }
        }
        // Without spread, field count must match exactly
        if (!node->as.struct_lit.spread && node->as.struct_lit.field_count != st->field_count) {
            sema_error(ctx, &node->tok, "struct '%s' has %d fields, got %d",
                       name, st->field_count, node->as.struct_lit.field_count);
        }
        // With spread, explicit fields must not exceed struct field count
        if (node->as.struct_lit.spread && node->as.struct_lit.field_count > st->field_count) {
            sema_error(ctx, &node->tok, "struct '%s' has %d fields, got %d explicit fields with spread",
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
        st->is_referenced = true;
        return set_type(node, ast_type_named(name));
    }

    case NODE_ENUM_INIT: {
        const char *ename = node->as.enum_init.enum_name;
        SemaSymbol *sym = scope_lookup(ctx->current, ename);
        if (!sym || !sym->is_enum) {
            if (!sym->is_enum) {
                sema_error(ctx, &node->tok, "'%s' is not enum", ename);
            } else {
                sema_error(ctx, &node->tok, "unknown enum '%s'", ename);
            }
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
                sym->is_referenced = true;
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

    case NODE_TUPLE_LIT: {
        int count = node->as.tuple_lit.count;
        AstType **elem_types = malloc(sizeof(AstType *) * (size_t)count);
        for (int i = 0; i < count; i++) {
            AstType *t = check_expr(ctx, node->as.tuple_lit.elements[i]);
            elem_types[i] = t ? ast_type_clone(t) : ast_type_simple(TYPE_VOID);
        }
        return set_type(node, ast_type_tuple(elem_types, count));
    }

    default:
        return set_type(node, ast_type_simple(TYPE_VOID));
    }
}

// ---- Statement checking ----

static void check_stmt(SemaCtx *ctx, AstNode *node) {
    if (!node) return;

    switch (node->kind) {
    case NODE_LET_STMT: {
        AstType *init_type = check_expr(ctx, node->as.let_stmt.init);
        AstType *decl_type = node->as.let_stmt.type;

        if (init_type && decl_type && (!ast_types_equal(init_type, decl_type) && !ast_types_compatible(init_type, decl_type))) {
            // Allow Result type coercion (Ok/Err assign to Result<T,E>)
            if (!(decl_type->kind == TYPE_RESULT &&
                  (node->as.let_stmt.init->kind == NODE_OK_EXPR ||
                   node->as.let_stmt.init->kind == NODE_ERR_EXPR))) {
                sema_error(ctx, &node->as.let_stmt.init->tok, "cannot assign '%s' to variable of type '%s'",
                           ast_type_str(init_type), ast_type_str(decl_type));
            }
        }

        if (scope_lookup_local(ctx->current, node->as.let_stmt.name)) {
            sema_error(ctx, &node->tok, "variable '%s' already declared in this scope",
                       node->as.let_stmt.name);
        }

        SemaSymbol *sym = scope_add(ctx->current, node->as.let_stmt.name, node->tok);
        sym->type = decl_type;
        sym->is_mut = node->as.let_stmt.is_mut;
        break;
    }

    case NODE_ASSIGN_STMT: {
        AstType *target_type = check_expr(ctx, node->as.assign_stmt.target);
        AstType *val_type = check_expr(ctx, node->as.assign_stmt.value);

        if (node->as.assign_stmt.target->kind == NODE_IDENT) {
            SemaSymbol *sym = scope_lookup(ctx->current, node->as.assign_stmt.target->as.ident.name);
            if (sym && !sym->is_mut && !sym->is_fn) {
                sema_error(ctx, &node->as.assign_stmt.value->tok, "cannot assign to immutable variable '%s'",
                           node->as.assign_stmt.target->as.ident.name);
            }
        }

        if (target_type && val_type && (!ast_types_equal(target_type, val_type)) && !ast_types_compatible(val_type, target_type)) {
            sema_error(ctx, &node->as.assign_stmt.value->tok, "cannot assign '%s' to '%s'",
                       ast_type_str(val_type), ast_type_str(target_type));
        }
        break;
    }

    case NODE_IF_STMT: {
        AstType *cond = check_expr(ctx, node->as.if_stmt.condition);
        if (cond && cond->kind != TYPE_BOOL) {
            sema_error(ctx, &node->as.if_stmt.condition->tok, "if condition must be bool, got '%s'", ast_type_str(cond));
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
            sema_error(ctx, &node->as.while_stmt.condition->tok, "while condition must be bool, got '%s'", ast_type_str(cond));
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
                sema_error(ctx, &node->as.for_stmt.iterable->tok, "for-each requires array type, got '%s'",
                           ast_type_str(iter_type));
            }
            AstType *elem_type = (iter_type && iter_type->kind == TYPE_ARRAY && iter_type->element)
                                 ? ast_type_clone(iter_type->element)
                                 : ast_type_simple(TYPE_INT);

            SemaScope *body_scope = scope_new(ctx->current);
            ctx->current = body_scope;
            SemaSymbol *loop_var = scope_add(body_scope, node->as.for_stmt.var_name, node->tok);
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
                sema_error(ctx, &node->as.for_stmt.start->tok, "for range start must be int, got '%s'", ast_type_str(start));
            }
            if (end && end->kind != TYPE_INT) {
                sema_error(ctx, &node->as.for_stmt.end->tok, "for range end must be int, got '%s'", ast_type_str(end));
            }

            SemaScope *body_scope = scope_new(ctx->current);
            ctx->current = body_scope;
            SemaSymbol *loop_var = scope_add(body_scope, node->as.for_stmt.var_name, node->tok);
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
                    sema_error(ctx, &node->as.return_stmt.value->tok, "return type mismatch: expected '%s', got '%s'",
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
            sema_error(ctx, &node->as.match_stmt.target->tok, "match target must be an enum type, got '%s'",
                       ast_type_str(target_type));
            break;
        }
        SemaSymbol *enum_sym = scope_lookup(ctx->current, target_type->name);
        if (!enum_sym || !enum_sym->is_enum) {
            sema_error(ctx, &node->as.match_stmt.target->tok, "match target type '%s' is not an enum", target_type->name);
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
                sema_error(ctx, &node->as.match_stmt.target->tok, "enum '%s' has no variant '%s'",
                           target_type->name, arm->variant_name);
                continue;
            }
            if (arm->binding_count != variant->field_count) {
                sema_error(ctx, &node->as.match_stmt.target->tok, "variant '%s' has %d fields, got %d bindings",
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
            SemaScope *arm_scope = scope_new(ctx->current);
            ctx->current = arm_scope;
            for (int b = 0; b < arm->binding_count && b < variant->field_count; b++) {
                SemaSymbol *binding = scope_add(arm_scope, arm->bindings[b], node->tok);
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

static void check_unused_symbols(SemaCtx *ctx, SemaScope *s) {
    for (int i = 0; i < s->count; i++) {
        SemaSymbol *sym = &s->syms[i];
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

static void check_block(SemaCtx *ctx, AstNode *node) {
    SemaScope *block_scope = scope_new(ctx->current);
    ctx->current = block_scope;
    for (int i = 0; i < node->as.block.stmt_count; i++) {
        check_stmt(ctx, node->as.block.stmts[i]);
    }
    check_unused_symbols(ctx, block_scope);
    ctx->current = block_scope->parent;
    scope_free(block_scope);
}

// ---- Top-level ----

bool sema_analyze(AstNode *program, const char *filename) {
    SemaCtx ctx = {0};
    SemaScope *global = scope_new(NULL);
    ctx.filename = filename;
    ctx.current = global;

    sema_register_builtins(global);

    // Pass 1: register all structs, enums, and function signatures
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_STRUCT_DECL) {
            if (scope_lookup_local(global, d->as.struct_decl.name)) {
                sema_error(&ctx, &d->tok, "duplicate struct '%s'", d->as.struct_decl.name);
                continue;
            }
            SemaSymbol *s = scope_add(global, d->as.struct_decl.name, d->tok);
            s->is_struct = true;
            s->fields = d->as.struct_decl.fields;
            s->field_count = d->as.struct_decl.field_count;
            s->type = ast_type_named(d->as.struct_decl.name);
        } else if (d->kind == NODE_ENUM_DECL) {
            if (scope_lookup_local(global, d->as.enum_decl.name)) {
                sema_error(&ctx, &d->tok, "duplicate enum '%s'", d->as.enum_decl.name);
                continue;
            }
            SemaSymbol *s = scope_add(global, d->as.enum_decl.name, d->tok);
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
            SemaSymbol *s = scope_add(global, d->as.fn_decl.name, d->tok);
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
            SemaScope *fn_scope = scope_new(global);
            ctx.current = fn_scope;
            ctx.current_fn_return = d->as.fn_decl.return_type;

            for (int j = 0; j < d->as.fn_decl.param_count; j++) {
                Param *p_decl = &d->as.fn_decl.params[j];
                SemaSymbol *p = scope_add(fn_scope, d->as.fn_decl.params[j].name, d->tok);
                p->type = d->as.fn_decl.params[j].type;
                p->is_mut = false;

                if (p_decl->default_value != NULL) {
                    AstType *def_type = check_expr(&ctx, p_decl->default_value);
                    if (!ast_types_equal(def_type, p_decl->type)) {
                        sema_error(&ctx, &p_decl->default_value->tok, "default value type mismatch for parameter '%s': expected '%s' but got '%s'",
                                p_decl->name, ast_type_str(p_decl->type), ast_type_str(def_type));
                    }
                }
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
