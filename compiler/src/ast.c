#ifndef _WIN32
#  define _POSIX_C_SOURCE 200809L
#endif

#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AstNode *ast_new(NodeKind kind, Token tok) {
    AstNode *n = calloc(1, sizeof(AstNode));
    n->kind = kind;
    n->tok = tok;
    return n;
}

AstType *ast_type_simple(TypeKind kind) {
    AstType *t = calloc(1, sizeof(AstType));
    t->kind = kind;
    return t;
}

AstType *ast_type_array(AstType *element) {
    AstType *t = calloc(1, sizeof(AstType));
    t->kind = TYPE_ARRAY;
    t->element = element;
    return t;
}

AstType *ast_type_named(const char *name) {
    AstType *t = calloc(1, sizeof(AstType));
    t->kind = TYPE_NAMED;
    t->name = strdup(name);
    return t;
}

AstType *ast_type_result(AstType *ok_type, AstType *err_type) {
    AstType *t = calloc(1, sizeof(AstType));
    t->kind = TYPE_RESULT;
    t->ok_type = ok_type;
    t->err_type = err_type;
    return t;
}

AstType *ast_type_fn(AstType **param_types, int param_count, AstType *return_type) {
    AstType *t = calloc(1, sizeof(AstType));
    t->kind = TYPE_FN;
    t->param_types = param_types;
    t->param_count = param_count;
    t->return_type = return_type;
    return t;
}

char *ast_strdup(const char *s, size_t len) {
    char *d = malloc(len + 1);
    memcpy(d, s, len);
    d[len] = '\0';
    return d;
}

AstType *ast_type_clone(AstType *t) {
    if (!t) return NULL;
    AstType *c = calloc(1, sizeof(AstType));
    c->kind = t->kind;
    if (t->name) c->name = strdup(t->name);
    if (t->element) c->element = ast_type_clone(t->element);
    if (t->ok_type) c->ok_type = ast_type_clone(t->ok_type);
    if (t->err_type) c->err_type = ast_type_clone(t->err_type);
    if (t->kind == TYPE_FN) {
        c->param_count = t->param_count;
        c->return_type = ast_type_clone(t->return_type);
        if (t->param_count > 0) {
            c->param_types = malloc(sizeof(AstType *) * (size_t)t->param_count);
            for (int i = 0; i < t->param_count; i++)
                c->param_types[i] = ast_type_clone(t->param_types[i]);
        }
    }
    return c;
}

bool ast_types_equal(AstType *a, AstType *b) {
    if (!a || !b) return a == b;
    if (a->kind != b->kind) return false;
    if (a->kind == TYPE_NAMED) return strcmp(a->name, b->name) == 0;
    if (a->kind == TYPE_ARRAY) return ast_types_equal(a->element, b->element);
    if (a->kind == TYPE_RESULT) return ast_types_equal(a->ok_type, b->ok_type) && ast_types_equal(a->err_type, b->err_type);
    if (a->kind == TYPE_FN) {
        if (a->param_count != b->param_count) return false;
        if (!ast_types_equal(a->return_type, b->return_type)) return false;
        for (int i = 0; i < a->param_count; i++)
            if (!ast_types_equal(a->param_types[i], b->param_types[i])) return false;
        return true;
    }
    return true;
}

// Round-robin static buffers to avoid clobber in printf with multiple calls
const char *ast_type_str(AstType *t) {
    static char bufs[4][256];
    static int idx = 0;
    char *buf = bufs[idx++ % 4];

    if (!t) { return "void"; }
    switch (t->kind) {
    case TYPE_INT:   return "int";
    case TYPE_FLOAT: return "float";
    case TYPE_BOOL:  return "bool";
    case TYPE_STR:   return "str";
    case TYPE_VOID:  return "void";
    case TYPE_NAMED: return t->name;
    case TYPE_ARRAY:
        snprintf(buf, 256, "[%s]", ast_type_str(t->element));
        return buf;
    case TYPE_RESULT:
        snprintf(buf, 256, "Result<%s, %s>", ast_type_str(t->ok_type), ast_type_str(t->err_type));
        return buf;
    case TYPE_FN: {
        int pos = snprintf(buf, 256, "fn(");
        for (int i = 0; i < t->param_count; i++) {
            if (i > 0) pos += snprintf(buf + pos, 256 - (size_t)pos, ", ");
            pos += snprintf(buf + pos, 256 - (size_t)pos, "%s", ast_type_str(t->param_types[i]));
        }
        snprintf(buf + pos, 256 - (size_t)pos, ") -> %s", ast_type_str(t->return_type));
        return buf;
    }
    }
    return "?";
}

static void indent(int level) {
    for (int i = 0; i < level; i++) printf("  ");
}

static void print_type(AstType *t) {
    if (!t) { printf("(none)"); return; }
    switch (t->kind) {
    case TYPE_INT:    printf("int"); break;
    case TYPE_FLOAT:  printf("float"); break;
    case TYPE_BOOL:   printf("bool"); break;
    case TYPE_STR:    printf("str"); break;
    case TYPE_VOID:   printf("void"); break;
    case TYPE_ARRAY:  printf("["); print_type(t->element); printf("]"); break;
    case TYPE_NAMED:  printf("%s", t->name); break;
    case TYPE_RESULT: printf("Result<"); print_type(t->ok_type); printf(", "); print_type(t->err_type); printf(">"); break;
    case TYPE_FN:
        printf("fn(");
        for (int i = 0; i < t->param_count; i++) {
            if (i > 0) printf(", ");
            print_type(t->param_types[i]);
        }
        printf(") -> ");
        print_type(t->return_type);
        break;
    }
}

void ast_print(AstNode *node, int ind) {
    if (!node) return;
    indent(ind);

    switch (node->kind) {
    case NODE_PROGRAM:
        printf("Program (%d declarations)\n", node->as.program.decl_count);
        for (int i = 0; i < node->as.program.decl_count; i++)
            ast_print(node->as.program.decls[i], ind + 1);
        break;

    case NODE_FN_DECL:
        printf("FnDecl '%s' -> ", node->as.fn_decl.name);
        print_type(node->as.fn_decl.return_type);
        printf(" (%d params)\n", node->as.fn_decl.param_count);
        for (int i = 0; i < node->as.fn_decl.param_count; i++) {
            indent(ind + 1); printf("param '%s': ", node->as.fn_decl.params[i].name);
            print_type(node->as.fn_decl.params[i].type); printf("\n");
        }
        ast_print(node->as.fn_decl.body, ind + 1);
        break;

    case NODE_STRUCT_DECL:
        printf("StructDecl '%s' (%d fields)\n", node->as.struct_decl.name, node->as.struct_decl.field_count);
        for (int i = 0; i < node->as.struct_decl.field_count; i++) {
            indent(ind + 1); printf("field '%s': ", node->as.struct_decl.fields[i].name);
            print_type(node->as.struct_decl.fields[i].type); printf("\n");
        }
        break;

    case NODE_BLOCK:
        printf("Block (%d stmts)\n", node->as.block.stmt_count);
        for (int i = 0; i < node->as.block.stmt_count; i++)
            ast_print(node->as.block.stmts[i], ind + 1);
        break;

    case NODE_LET_STMT:
        printf("Let%s '%s': ", node->as.let_stmt.is_mut ? " mut" : "", node->as.let_stmt.name);
        print_type(node->as.let_stmt.type); printf("\n");
        ast_print(node->as.let_stmt.init, ind + 1);
        break;

    case NODE_ASSIGN_STMT:
        printf("Assign (op=%s)\n", token_type_name(node->as.assign_stmt.op));
        ast_print(node->as.assign_stmt.target, ind + 1);
        ast_print(node->as.assign_stmt.value, ind + 1);
        break;

    case NODE_IF_STMT:
        printf("If\n");
        indent(ind + 1); printf("condition:\n");
        ast_print(node->as.if_stmt.condition, ind + 2);
        indent(ind + 1); printf("then:\n");
        ast_print(node->as.if_stmt.then_block, ind + 2);
        if (node->as.if_stmt.else_branch) {
            indent(ind + 1); printf("else:\n");
            ast_print(node->as.if_stmt.else_branch, ind + 2);
        }
        break;

    case NODE_WHILE_STMT:
        printf("While\n");
        ast_print(node->as.while_stmt.condition, ind + 1);
        ast_print(node->as.while_stmt.body, ind + 1);
        break;

    case NODE_FOR_STMT:
        if (node->as.for_stmt.is_foreach) {
            printf("ForEach '%s'\n", node->as.for_stmt.var_name);
            indent(ind + 1); printf("iterable:\n");
            ast_print(node->as.for_stmt.iterable, ind + 2);
        } else {
            printf("For '%s' (%s)\n", node->as.for_stmt.var_name,
                   node->as.for_stmt.inclusive ? "inclusive" : "exclusive");
            indent(ind + 1); printf("start:\n");
            ast_print(node->as.for_stmt.start, ind + 2);
            indent(ind + 1); printf("end:\n");
            ast_print(node->as.for_stmt.end, ind + 2);
        }
        ast_print(node->as.for_stmt.body, ind + 1);
        break;

    case NODE_RETURN_STMT:
        printf("Return\n");
        if (node->as.return_stmt.value)
            ast_print(node->as.return_stmt.value, ind + 1);
        break;

    case NODE_BREAK_STMT:    printf("Break\n"); break;
    case NODE_CONTINUE_STMT: printf("Continue\n"); break;

    case NODE_EXPR_STMT:
        printf("ExprStmt\n");
        ast_print(node->as.expr_stmt.expr, ind + 1);
        break;

    case NODE_BINARY:
        printf("Binary '%s'\n", token_type_name(node->as.binary.op));
        ast_print(node->as.binary.left, ind + 1);
        ast_print(node->as.binary.right, ind + 1);
        break;

    case NODE_UNARY:
        printf("Unary '%s'\n", token_type_name(node->as.unary.op));
        ast_print(node->as.unary.operand, ind + 1);
        break;

    case NODE_CALL:
        printf("Call (%d args)\n", node->as.call.arg_count);
        ast_print(node->as.call.callee, ind + 1);
        for (int i = 0; i < node->as.call.arg_count; i++)
            ast_print(node->as.call.args[i], ind + 1);
        break;

    case NODE_FIELD_ACCESS:
        printf("FieldAccess '.%s'\n", node->as.field_access.field);
        ast_print(node->as.field_access.object, ind + 1);
        break;

    case NODE_INDEX:
        printf("Index\n");
        ast_print(node->as.index_expr.object, ind + 1);
        ast_print(node->as.index_expr.index, ind + 1);
        break;

    case NODE_INT_LIT:   printf("IntLit %lld\n", node->as.int_lit.value); break;
    case NODE_FLOAT_LIT: printf("FloatLit %f\n", node->as.float_lit.value); break;
    case NODE_STR_LIT:   printf("StrLit \"%s\"\n", node->as.str_lit.value); break;
    case NODE_BOOL_LIT:  printf("BoolLit %s\n", node->as.bool_lit.value ? "true" : "false"); break;
    case NODE_IDENT:     printf("Ident '%s'\n", node->as.ident.name); break;

    case NODE_ARRAY_LIT:
        printf("ArrayLit (%d elements)\n", node->as.array_lit.count);
        for (int i = 0; i < node->as.array_lit.count; i++)
            ast_print(node->as.array_lit.elements[i], ind + 1);
        break;

    case NODE_STRUCT_LIT:
        printf("StructLit '%s' (%d fields)\n", node->as.struct_lit.name, node->as.struct_lit.field_count);
        for (int i = 0; i < node->as.struct_lit.field_count; i++) {
            indent(ind + 1); printf(".%s =\n", node->as.struct_lit.fields[i].name);
            ast_print(node->as.struct_lit.fields[i].value, ind + 2);
        }
        break;

    case NODE_ENUM_DECL:
        printf("EnumDecl '%s' (%d variants)\n", node->as.enum_decl.name, node->as.enum_decl.variant_count);
        for (int i = 0; i < node->as.enum_decl.variant_count; i++) {
            EnumVariant *v = &node->as.enum_decl.variants[i];
            indent(ind + 1); printf("variant '%s'", v->name);
            if (v->field_count > 0) {
                printf("(");
                for (int j = 0; j < v->field_count; j++) {
                    if (j > 0) printf(", ");
                    printf("%s: ", v->fields[j].name);
                    print_type(v->fields[j].type);
                }
                printf(")");
            }
            printf("\n");
        }
        break;

    case NODE_MATCH:
        printf("Match\n");
        indent(ind + 1); printf("target:\n");
        ast_print(node->as.match_stmt.target, ind + 2);
        for (int i = 0; i < node->as.match_stmt.arm_count; i++) {
            MatchArm *a = &node->as.match_stmt.arms[i];
            indent(ind + 1); printf("arm %s.%s", a->enum_name, a->variant_name);
            if (a->binding_count > 0) {
                printf("(");
                for (int j = 0; j < a->binding_count; j++) {
                    if (j > 0) printf(", ");
                    printf("%s", a->bindings[j]);
                }
                printf(")");
            }
            printf(" =>\n");
            ast_print(a->body, ind + 2);
        }
        break;

    case NODE_ENUM_INIT:
        printf("EnumInit %s.%s (%d args)\n", node->as.enum_init.enum_name,
               node->as.enum_init.variant_name, node->as.enum_init.arg_count);
        for (int i = 0; i < node->as.enum_init.arg_count; i++)
            ast_print(node->as.enum_init.args[i], ind + 1);
        break;

    case NODE_IMPORT:
        printf("Import '%s'\n", node->as.import_decl.path);
        break;

    case NODE_OK_EXPR:
        printf("Ok\n");
        ast_print(node->as.result_expr.value, ind + 1);
        break;

    case NODE_ERR_EXPR:
        printf("Err\n");
        ast_print(node->as.result_expr.value, ind + 1);
        break;

    case NODE_LAMBDA:
        printf("Lambda (%d params) -> ", node->as.lambda.param_count);
        print_type(node->as.lambda.return_type);
        printf("\n");
        ast_print(node->as.lambda.body, ind + 1);
        break;
    }
}

void ast_type_free(AstType *type) {
    if (!type) return;
    free(type->name);
    ast_type_free(type->element);
    ast_type_free(type->ok_type);
    ast_type_free(type->err_type);
    if (type->kind == TYPE_FN) {
        for (int i = 0; i < type->param_count; i++)
            ast_type_free(type->param_types[i]);
        free(type->param_types);
        ast_type_free(type->return_type);
    }
    free(type);
}

void ast_free(AstNode *node) {
    if (!node) return;
    ast_type_free(node->resolved_type);
    switch (node->kind) {
    case NODE_PROGRAM:
        for (int i = 0; i < node->as.program.decl_count; i++)
            ast_free(node->as.program.decls[i]);
        free(node->as.program.decls);
        break;
    case NODE_FN_DECL:
        free(node->as.fn_decl.name);
        for (int i = 0; i < node->as.fn_decl.param_count; i++) {
            free(node->as.fn_decl.params[i].name);
            ast_type_free(node->as.fn_decl.params[i].type);
        }
        free(node->as.fn_decl.params);
        ast_type_free(node->as.fn_decl.return_type);
        ast_free(node->as.fn_decl.body);
        break;
    case NODE_STRUCT_DECL:
        free(node->as.struct_decl.name);
        for (int i = 0; i < node->as.struct_decl.field_count; i++) {
            free(node->as.struct_decl.fields[i].name);
            ast_type_free(node->as.struct_decl.fields[i].type);
        }
        free(node->as.struct_decl.fields);
        break;
    case NODE_BLOCK:
        for (int i = 0; i < node->as.block.stmt_count; i++)
            ast_free(node->as.block.stmts[i]);
        free(node->as.block.stmts);
        break;
    case NODE_LET_STMT:
        free(node->as.let_stmt.name);
        ast_type_free(node->as.let_stmt.type);
        ast_free(node->as.let_stmt.init);
        break;
    case NODE_ASSIGN_STMT:
        ast_free(node->as.assign_stmt.target);
        ast_free(node->as.assign_stmt.value);
        break;
    case NODE_IF_STMT:
        ast_free(node->as.if_stmt.condition);
        ast_free(node->as.if_stmt.then_block);
        ast_free(node->as.if_stmt.else_branch);
        break;
    case NODE_WHILE_STMT:
        ast_free(node->as.while_stmt.condition);
        ast_free(node->as.while_stmt.body);
        break;
    case NODE_FOR_STMT:
        free(node->as.for_stmt.var_name);
        ast_free(node->as.for_stmt.start);
        ast_free(node->as.for_stmt.end);
        ast_free(node->as.for_stmt.iterable);
        ast_free(node->as.for_stmt.body);
        break;
    case NODE_RETURN_STMT:
        ast_free(node->as.return_stmt.value);
        break;
    case NODE_EXPR_STMT:
        ast_free(node->as.expr_stmt.expr);
        break;
    case NODE_BINARY:
        ast_free(node->as.binary.left);
        ast_free(node->as.binary.right);
        break;
    case NODE_UNARY:
        ast_free(node->as.unary.operand);
        break;
    case NODE_CALL:
        ast_free(node->as.call.callee);
        for (int i = 0; i < node->as.call.arg_count; i++)
            ast_free(node->as.call.args[i]);
        free(node->as.call.args);
        break;
    case NODE_FIELD_ACCESS:
        ast_free(node->as.field_access.object);
        free(node->as.field_access.field);
        break;
    case NODE_INDEX:
        ast_free(node->as.index_expr.object);
        ast_free(node->as.index_expr.index);
        break;
    case NODE_STR_LIT: free(node->as.str_lit.value); break;
    case NODE_IDENT: free(node->as.ident.name); break;
    case NODE_ARRAY_LIT:
        for (int i = 0; i < node->as.array_lit.count; i++)
            ast_free(node->as.array_lit.elements[i]);
        free(node->as.array_lit.elements);
        break;
    case NODE_STRUCT_LIT:
        free(node->as.struct_lit.name);
        for (int i = 0; i < node->as.struct_lit.field_count; i++) {
            free(node->as.struct_lit.fields[i].name);
            ast_free(node->as.struct_lit.fields[i].value);
        }
        free(node->as.struct_lit.fields);
        break;
    case NODE_ENUM_DECL:
        free(node->as.enum_decl.name);
        for (int i = 0; i < node->as.enum_decl.variant_count; i++) {
            free(node->as.enum_decl.variants[i].name);
            for (int j = 0; j < node->as.enum_decl.variants[i].field_count; j++) {
                free(node->as.enum_decl.variants[i].fields[j].name);
                ast_type_free(node->as.enum_decl.variants[i].fields[j].type);
            }
            free(node->as.enum_decl.variants[i].fields);
        }
        free(node->as.enum_decl.variants);
        break;
    case NODE_MATCH:
        ast_free(node->as.match_stmt.target);
        for (int i = 0; i < node->as.match_stmt.arm_count; i++) {
            free(node->as.match_stmt.arms[i].enum_name);
            free(node->as.match_stmt.arms[i].variant_name);
            for (int j = 0; j < node->as.match_stmt.arms[i].binding_count; j++)
                free(node->as.match_stmt.arms[i].bindings[j]);
            free(node->as.match_stmt.arms[i].bindings);
            if (node->as.match_stmt.arms[i].binding_types) {
                for (int j = 0; j < node->as.match_stmt.arms[i].binding_count; j++)
                    ast_type_free(node->as.match_stmt.arms[i].binding_types[j]);
                free(node->as.match_stmt.arms[i].binding_types);
            }
            ast_free(node->as.match_stmt.arms[i].pattern_expr);
            ast_free(node->as.match_stmt.arms[i].body);
        }
        free(node->as.match_stmt.arms);
        break;
    case NODE_ENUM_INIT:
        free(node->as.enum_init.enum_name);
        free(node->as.enum_init.variant_name);
        for (int i = 0; i < node->as.enum_init.arg_count; i++)
            ast_free(node->as.enum_init.args[i]);
        free(node->as.enum_init.args);
        break;
    case NODE_IMPORT:
        free(node->as.import_decl.path);
        break;
    case NODE_OK_EXPR:
    case NODE_ERR_EXPR:
        ast_free(node->as.result_expr.value);
        break;
    case NODE_LAMBDA:
        for (int i = 0; i < node->as.lambda.param_count; i++) {
            free(node->as.lambda.params[i].name);
            ast_type_free(node->as.lambda.params[i].type);
        }
        free(node->as.lambda.params);
        ast_type_free(node->as.lambda.return_type);
        ast_free(node->as.lambda.body);
        for (int i = 0; i < node->as.lambda.capture_count; i++) {
            free(node->as.lambda.captures[i]);
            ast_type_free(node->as.lambda.capture_types[i]);
        }
        free(node->as.lambda.captures);
        free(node->as.lambda.capture_types);
        break;
    case NODE_INT_LIT:
    case NODE_FLOAT_LIT:
    case NODE_BOOL_LIT:
    case NODE_BREAK_STMT:
    case NODE_CONTINUE_STMT:
        break;
    }
    free(node);
}
