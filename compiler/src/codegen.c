#include "config.h"
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// runtime embbeded
extern const unsigned char urus_runtime_header_data[];
extern const unsigned int urus_runtime_header_data_len;

// ---- Forward declarations ----
static void gen_expr(CodeBuf *buf, AstNode *node);
static void gen_stmt(CodeBuf *buf, AstNode *node);
static void gen_block(CodeBuf *buf, AstNode *node);

// ---- Buffer helpers ----

void codegen_init(CodeBuf *buf) {
    buf->cap = 8192;
    buf->len = 0;
    buf->data = malloc(buf->cap);
    buf->data[0] = '\0';
    buf->indent = 0;
    buf->tmp_counter = 0;
    buf->current_scope = NULL;
}

void codegen_free(CodeBuf *buf) {
    free(buf->data);
}

static void buf_ensure(CodeBuf *buf, size_t extra) {
    while (buf->len + extra + 1 >= buf->cap) {
        buf->cap *= 2;
        buf->data = realloc(buf->data, buf->cap);
    }
}

static void emit(CodeBuf *buf, const char *fmt, ...) {
    va_list args;
    va_list args_copy; // for finding actual size
    va_start(args, fmt);
    va_copy(args_copy, args);
    int n = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (n > 0) { 
        buf_ensure(buf, n);
        vsnprintf(buf->data + buf->len, (size_t)n + 1, fmt, args);
        buf->len += (size_t)n;
        buf->data[buf->len] = '\0';
    }
    va_end(args);
}

static void emit_indent(CodeBuf *buf) {
    for (int i = 0; i < buf->indent; i++) emit(buf, "    ");
}

static void emit_type_drop_cname(CodeBuf *buf, AstType *t, const char *c_name) {
    if (!t) return;
    switch(t->kind) {
    case TYPE_STR:
        emit(buf, "urus_str_drop(&%s);\n", c_name);
        break;
    case TYPE_ARRAY:
        emit(buf, "urus_array_drop(&%s);\n", c_name);
        break;
    case TYPE_NAMED:
        emit(buf, "%s_drop(&%s);\n", t->name, c_name);
        break;
    case TYPE_RESULT:
        emit(buf, "urus_result_drop(&%s);\n", c_name);
        break;
    default:
        break;
    }
}

// ---- Type emission ----

static void gen_type(CodeBuf *buf, AstType *t) {
    if (!t) { emit(buf, "void"); return; }
    switch (t->kind) {
    case TYPE_INT:    emit(buf, "int64_t"); break;
    case TYPE_FLOAT:  emit(buf, "double"); break;
    case TYPE_BOOL:   emit(buf, "bool"); break;
    case TYPE_STR:    emit(buf, "urus_str*"); break;
    case TYPE_VOID:   emit(buf, "void"); break;
    case TYPE_ARRAY:  emit(buf, "urus_array*"); break;
    case TYPE_NAMED:  emit(buf, "%s*", t->name); break;
    case TYPE_RESULT: emit(buf, "urus_result*"); break;
    case TYPE_FN:    emit(buf, "void*"); break; // function pointers as void*
    }
}

static bool type_needs_rc(AstType *t) {
    if (!t) return false;
    if (t->kind == TYPE_STR || t->kind == TYPE_ARRAY ||
            t->kind == TYPE_NAMED || t->kind == TYPE_RESULT) return true;
    return false;
}

// Return the C sizeof expression for an array element type
static const char *elem_sizeof(AstType *t) {
    if (!t) return "sizeof(int64_t)";
    switch (t->kind) {
    case TYPE_INT:   return "sizeof(int64_t)";
    case TYPE_FLOAT: return "sizeof(double)";
    case TYPE_BOOL:  return "sizeof(bool)";
    case TYPE_STR:   return "sizeof(urus_str*)";
    case TYPE_NAMED: return "sizeof(void*)";
    case TYPE_ARRAY: return "sizeof(urus_array*)";
    default: return "sizeof(int64_t)";
    }
}

// Return the C type cast for compound literal in push
static const char *elem_ctype(AstType *t) {
    if (!t) return "int64_t";
    switch (t->kind) {
    case TYPE_INT:   return "int64_t";
    case TYPE_FLOAT: return "double";
    case TYPE_BOOL:  return "bool";
    case TYPE_STR:   return "urus_str*";
    case TYPE_NAMED: return "void*";
    case TYPE_ARRAY: return "urus_array*";
    default: return "int64_t";
    }
}

// ---- Expression emission ----

static const char *binop_str(TokenType op) {
    switch (op) {
    case TOK_PLUS:    return "+";
    case TOK_MINUS:   return "-";
    case TOK_STAR:    return "*";
    case TOK_SLASH:   return "/";
    case TOK_PERCENT: return "%";
    case TOK_EQ:      return "==";
    case TOK_NEQ:     return "!=";
    case TOK_LT:      return "<";
    case TOK_GT:      return ">";
    case TOK_LTE:     return "<=";
    case TOK_GTE:     return ">=";
    case TOK_AND:     return "&&";
    case TOK_OR:      return "||";
    default: return "?";
    }
}

static bool expr_is_string(AstNode *n) {
    if (!n) return false;
    if (n->resolved_type && n->resolved_type->kind == TYPE_STR) return true;
    if (n->kind == NODE_STR_LIT) return true;
    if (n->kind == NODE_BINARY && n->as.binary.op == TOK_PLUS) {
        return expr_is_string(n->as.binary.left) || expr_is_string(n->as.binary.right);
    }
    return false;
}

static void gen_array_get(CodeBuf *buf, AstNode *node) {
    AstType *elem = node->resolved_type;
    const char *getter = "urus_array_get_int";
    if (elem) {
        switch (elem->kind) {
        case TYPE_FLOAT:  getter = "urus_array_get_float"; break;
        case TYPE_BOOL:   getter = "urus_array_get_bool"; break;
        case TYPE_STR:    getter = "urus_array_get_str"; break;
        case TYPE_NAMED:  getter = "urus_array_get_ptr"; break;
        case TYPE_ARRAY:  getter = "urus_array_get_ptr"; break;
        default: break;
        }
    }
    emit(buf, "%s(", getter);
    gen_expr(buf, node->as.index_expr.object);
    emit(buf, ", ");
    gen_expr(buf, node->as.index_expr.index);
    emit(buf, ")");
}

static void gen_expr(CodeBuf *buf, AstNode *node) {
    if (!node) return;
    switch (node->kind) {
    case NODE_INT_LIT:
        emit(buf, "((int64_t)%lld)", node->as.int_lit.value);
        break;
    case NODE_FLOAT_LIT:
        emit(buf, "%f", node->as.float_lit.value);
        break;
    case NODE_STR_LIT: {
        emit(buf, "urus_str_from(\"");
        const char *s = node->as.str_lit.value;
        for (size_t i = 0; s[i]; i++) {
            if (s[i] == '"') emit(buf, "\\\"");
            else if (s[i] == '\\') emit(buf, "\\\\");
            else if (s[i] == '\n') emit(buf, "\\n");
            else emit(buf, "%c", s[i]);
        }
        emit(buf, "\")");
        break;
    }
    case NODE_BOOL_LIT:
        emit(buf, "%s", node->as.bool_lit.value ? "true" : "false");
        break;
    case NODE_IDENT:
        emit(buf, "%s", node->as.ident.name);
        break;
    case NODE_BINARY:
        if ((node->as.binary.op == TOK_EQ || node->as.binary.op == TOK_NEQ) &&
                node->as.binary.left->resolved_type->kind == TYPE_STR) {
            emit(buf, "(%surus_%s_equal(", node->as.binary.op == TOK_EQ ? "" : "!",
                    ast_type_str(node->as.binary.left->resolved_type));
            gen_expr(buf, node->as.binary.left);
            emit(buf, ", ");
            gen_expr(buf, node->as.binary.right);
            emit(buf, "))");
            break;
        }

        if (node->as.binary.op == TOK_PLUS && expr_is_string(node)) {
            emit(buf, "urus_str_concat(");
            gen_expr(buf, node->as.binary.left);
            emit(buf, ", ");
            gen_expr(buf, node->as.binary.right);
            emit(buf, ")");
        } else {
            emit(buf, "(");
            gen_expr(buf, node->as.binary.left);
            emit(buf, " %s ", binop_str(node->as.binary.op));
            gen_expr(buf, node->as.binary.right);
            emit(buf, ")");
        }
        break;
    case NODE_UNARY:
        emit(buf, "(");
        emit(buf, "%s", node->as.unary.op == TOK_NOT ? "!" : "-");
        gen_expr(buf, node->as.unary.operand);
        emit(buf, ")");
        break;
    case NODE_CALL: {
        const char *fn_name = NULL;
        if (node->as.call.callee->kind == NODE_IDENT) {
            fn_name = node->as.call.callee->as.ident.name;
        }

        typedef struct { const char *urus; const char *c; } BuiltinMap;
        static const BuiltinMap direct_maps[] = {
            {"str_len",        "urus_str_len"},
            {"str_slice",      "urus_str_slice"},
            {"str_find",       "urus_str_find"},
            {"str_contains",   "urus_str_contains"},
            {"str_upper",      "urus_str_upper"},
            {"str_lower",      "urus_str_lower"},
            {"str_trim",       "urus_str_trim"},
            {"str_replace",    "urus_str_replace"},
            {"str_starts_with","urus_str_starts_with"},
            {"str_ends_with",  "urus_str_ends_with"},
            {"str_split",      "urus_str_split"},
            {"char_at",        "urus_char_at"},
            {"abs",            "urus_abs"},
            {"fabs",           "urus_fabs"},
            {"sqrt",           "urus_sqrt"},
            {"pow",            "urus_pow"},
            {"min",            "urus_min"},
            {"max",            "urus_max"},
            {"fmin",           "urus_fmin"},
            {"fmax",           "urus_fmax"},
            {"input",          "urus_input"},
            {"read_file",      "urus_read_file"},
            {"write_file",     "urus_write_file"},
            {"append_file",    "urus_append_file"},
            {"exit",           "urus_exit"},
            {"assert",         "urus_assert"},
            {"len",            "urus_len"},
            {"pop",            "urus_pop"},
            {"is_ok",         "urus_result_is_ok"},
            {"is_err",        "urus_result_is_err"},
            {"unwrap_err",    "urus_result_unwrap_err"},
            {NULL, NULL}
        };

        if (fn_name && strcmp(fn_name, "unwrap") == 0) {
            // Determine unwrap variant based on result's ok type
            const char *unwrap_fn = "urus_result_unwrap";
            if (node->resolved_type) {
                switch (node->resolved_type->kind) {
                case TYPE_FLOAT: unwrap_fn = "urus_result_unwrap_float"; break;
                case TYPE_BOOL:  unwrap_fn = "urus_result_unwrap_bool"; break;
                case TYPE_STR:   unwrap_fn = "urus_result_unwrap_str"; break;
                case TYPE_NAMED:
                case TYPE_ARRAY: unwrap_fn = "urus_result_unwrap_ptr"; break;
                default: break;
                }
            }
            emit(buf, "%s(", unwrap_fn);
            if (node->as.call.arg_count > 0) gen_expr(buf, node->as.call.args[0]);
            emit(buf, ")");
        } else if (fn_name && strcmp(fn_name, "print") == 0) {
            emit(buf, "urus_print(");
            if (node->as.call.arg_count > 0) gen_expr(buf, node->as.call.args[0]);
            emit(buf, ")");
        } else if (fn_name && strcmp(fn_name, "to_str") == 0) {
            emit(buf, "to_str(");
            if (node->as.call.arg_count > 0) gen_expr(buf, node->as.call.args[0]);
            emit(buf, ")");
        } else if (fn_name && strcmp(fn_name, "to_int") == 0) {
            emit(buf, "to_int(");
            if (node->as.call.arg_count > 0) gen_expr(buf, node->as.call.args[0]);
            emit(buf, ")");
        } else if (fn_name && strcmp(fn_name, "to_float") == 0) {
            emit(buf, "to_float(");
            if (node->as.call.arg_count > 0) gen_expr(buf, node->as.call.args[0]);
            emit(buf, ")");
        } else if (fn_name && strcmp(fn_name, "push") == 0) {
            // Determine element type from array arg's resolved_type
            AstType *elem = NULL;
            if (node->as.call.arg_count > 0) {
                AstType *arr_type = node->as.call.args[0]->resolved_type;
                if (arr_type && arr_type->kind == TYPE_ARRAY) {
                    elem = arr_type->element;
                }
            }
            const char *ctype = elem_ctype(elem);
            emit(buf, "urus_array_push(");
            if (node->as.call.arg_count > 0) gen_expr(buf, node->as.call.args[0]);
            emit(buf, ", &(%s){", ctype);
            if (node->as.call.arg_count > 1) gen_expr(buf, node->as.call.args[1]);
            emit(buf, "})");
        } else {
            const char *c_name = NULL;
            if (fn_name) {
                for (const BuiltinMap *m = direct_maps; m->urus; m++) {
                    if (strcmp(fn_name, m->urus) == 0) { c_name = m->c; break; }
                }
            }
            if (c_name) {
                emit(buf, "%s(", c_name);
            } else {
                gen_expr(buf, node->as.call.callee);
                emit(buf, "(");
            }
            for (int i = 0; i < node->as.call.arg_count; i++) {
                if (i > 0) emit(buf, ", ");
                gen_expr(buf, node->as.call.args[i]);
            }
            emit(buf, ")");
        }
        break;
    }
    case NODE_FIELD_ACCESS:
        gen_expr(buf, node->as.field_access.object);
        emit(buf, "->%s", node->as.field_access.field);
        break;
    case NODE_INDEX:
        gen_array_get(buf, node);
        break;
    case NODE_ARRAY_LIT:
        emit(buf, "_urus_arr_%d", node->_codegen_tmp);
        break;
    case NODE_STRUCT_LIT:
        emit(buf, "_urus_st_%d", node->_codegen_tmp);
        break;
    case NODE_ENUM_INIT:
        emit(buf, "_urus_en_%d", node->_codegen_tmp);
        break;
    case NODE_OK_EXPR:
        emit(buf, "_urus_res_%d", node->_codegen_tmp);
        break;
    case NODE_ERR_EXPR:
        emit(buf, "_urus_res_%d", node->_codegen_tmp);
        break;
    default:
        emit(buf, "/* unsupported expr */0");
        break;
    }
}

// Emit pre-statements for complex expressions (array/struct/enum literals)
// Returns the tmp variable name index, or -1 if no pre-statement needed
static int gen_expr_pre(CodeBuf *buf, AstNode *node) {
    if (!node) return -1;

    switch (node->kind) {
    case NODE_ARRAY_LIT: {
        // First emit pre-statements for sub-expressions
        for (int i = 0; i < node->as.array_lit.count; i++) {
            gen_expr_pre(buf, node->as.array_lit.elements[i]);
        }
        int tmp = buf->tmp_counter++;
        node->_codegen_tmp = tmp;
        AstType *elem = NULL;
        if (node->resolved_type && node->resolved_type->kind == TYPE_ARRAY) {
            elem = node->resolved_type->element;
        }
        char dtor_str[128] = "NULL";
        if (elem && type_needs_rc(elem)) {
            if (elem->kind == TYPE_STR) strcpy(dtor_str, "(urus_drop_fn)urus_str_drop");
            else if (elem->kind == TYPE_ARRAY) strcpy(dtor_str, "(urus_drop_fn)urus_array_drop");
            else if (elem->kind == TYPE_RESULT) strcpy(dtor_str, "(urus_drop_fn)urus_result_drop");
            else if (elem->kind == TYPE_NAMED) snprintf(dtor_str, sizeof(dtor_str), "(urus_drop_fn)%s_drop", elem->name);
        }

        const char *sz = elem_sizeof(elem);
        const char *ctype = elem_ctype(elem);
        emit_indent(buf);
        emit(buf, "urus_array* _urus_arr_%d = urus_array_new(%s, %d, %s);\n\n",
             tmp, sz, node->as.array_lit.count > 0 ? node->as.array_lit.count : 4, dtor_str);
        for (int i = 0; i < node->as.array_lit.count; i++) {
            emit_indent(buf);
            emit(buf, "urus_array_push(_urus_arr_%d, &(%s){", tmp, ctype);
            gen_expr(buf, node->as.array_lit.elements[i]);
            emit(buf, "});\n");
        }

        return tmp;
    }
    case NODE_STRUCT_LIT: {
        for (int i = 0; i < node->as.struct_lit.field_count; i++) {
            gen_expr_pre(buf, node->as.struct_lit.fields[i].value);
        }
        if (node->as.struct_lit.spread) {
            gen_expr_pre(buf, node->as.struct_lit.spread);
        }
        int tmp = buf->tmp_counter++;
        node->_codegen_tmp = tmp;
        emit_indent(buf);
        emit(buf, "%s* _urus_st_%d = malloc(sizeof(%s));\n",
             node->as.struct_lit.name, tmp, node->as.struct_lit.name);

        // Spread: copy all fields from source, then override explicit ones
        if (node->as.struct_lit.spread) {
            emit_indent(buf);
            emit(buf, "*_urus_st_%d = *", tmp);
            gen_expr(buf, node->as.struct_lit.spread);
            emit(buf, "; // spread copy\n");
        }

        // Field assign (overrides spread fields if present)
        for (int i = 0; i < node->as.struct_lit.field_count; i++) {
            emit_indent(buf);
            emit(buf, "_urus_st_%d->%s = ", tmp, node->as.struct_lit.fields[i].name);
            gen_expr(buf, node->as.struct_lit.fields[i].value);
            emit(buf, ";\n");

            if (type_needs_rc(node->as.struct_lit.fields[i].value->resolved_type) &&
                    node->as.struct_lit.fields[i].value->kind == NODE_IDENT) {
                emit_indent(buf);
                emit(buf, "%s = NULL; // move to struct field\n", node->as.struct_lit.fields[i].value->as.ident.name);
            }
        }
        return tmp;
    }
    case NODE_ENUM_INIT: {
        for (int i = 0; i < node->as.enum_init.arg_count; i++) {
            gen_expr_pre(buf, node->as.enum_init.args[i]);
        }
        int tmp = buf->tmp_counter++;
        node->_codegen_tmp = tmp;
        const char *ename = node->as.enum_init.enum_name;
        const char *vname = node->as.enum_init.variant_name;
        emit_indent(buf);
        emit(buf, "%s* _urus_en_%d = malloc(sizeof(%s));\n", ename, tmp, ename);

        emit_indent(buf);
        emit(buf, "_urus_en_%d->tag = %s_TAG_%s;\n", tmp, ename, vname);
        for (int i = 0; i < node->as.enum_init.arg_count; i++) {
            emit_indent(buf);
            emit(buf, "_urus_en_%d->data.%s.f%d = ", tmp, vname, i);
            gen_expr(buf, node->as.enum_init.args[i]);
            emit(buf, ";\n");
            if (type_needs_rc(node->as.enum_init.args[i]->resolved_type) &&
                    node->as.enum_init.args[i]->kind == NODE_IDENT) {
                emit_indent(buf);
                emit(buf, "%s = NULL; // move to enum variant\n", node->as.enum_init.args[i]->as.ident.name);
            }
        }
        return tmp;
    }
    case NODE_OK_EXPR: {
        gen_expr_pre(buf, node->as.result_expr.value);
        int tmp = buf->tmp_counter++;
        node->_codegen_tmp = tmp;
        emit_indent(buf);
        emit(buf, "urus_result* _urus_res_%d = urus_result_ok(", tmp);
        // Determine the correct box field based on value type
        AstType *val_type = node->as.result_expr.value ? node->as.result_expr.value->resolved_type : NULL;
        if (val_type && val_type->kind == TYPE_FLOAT) {
            emit(buf, "&(urus_box){.as_float = ");
            gen_expr(buf, node->as.result_expr.value);
            emit(buf, "});\n");
        } else if (val_type && val_type->kind == TYPE_BOOL) {
            emit(buf, "&(urus_box){.as_bool = ");
            gen_expr(buf, node->as.result_expr.value);
            emit(buf, "});\n");
        } else if (val_type && (val_type->kind == TYPE_STR || val_type->kind == TYPE_NAMED ||
                                val_type->kind == TYPE_ARRAY)) {
            emit(buf, "&(urus_box){.as_ptr = (void*)(");
            gen_expr(buf, node->as.result_expr.value);
            emit(buf, ")});\n");
        } else {
            emit(buf, "&(urus_box){.as_int = (int64_t)(");
            gen_expr(buf, node->as.result_expr.value);
            emit(buf, ")});\n");
        }
        return tmp;
    }
    case NODE_ERR_EXPR: {
        gen_expr_pre(buf, node->as.result_expr.value);
        int tmp = buf->tmp_counter++;
        node->_codegen_tmp = tmp;
        emit_indent(buf);
        emit(buf, "urus_result* _urus_res_%d = urus_result_err(", tmp);
        gen_expr(buf, node->as.result_expr.value);
        emit(buf, ");\n");
        return tmp;
    }
    default:
        return -1;
    }
}

// ---- Statement emission ----

static void gen_stmt(CodeBuf *buf, AstNode *node) {
    if (!node) return;

    switch (node->kind) {
    case NODE_LET_STMT:
        // Emit pre-statements for complex initializers
        gen_expr_pre(buf, node->as.let_stmt.init);
        emit_indent(buf);

        // Emit RAII auto destruct __attribute((cleanup()))
        bool needs_rc = type_needs_rc(node->as.let_stmt.type);
        if (needs_rc) {
            const char *dtor = "NULL";
            if (node->as.let_stmt.type->kind == TYPE_STR) dtor = "urus_str_drop";
            else if (node->as.let_stmt.type->kind == TYPE_ARRAY) dtor = "urus_array_drop";
            else if (node->as.let_stmt.type->kind == TYPE_RESULT) dtor = "urus_result_drop";
            else if (node->as.let_stmt.type->kind == TYPE_NAMED) {
                emit(buf, "URUS_RAII(%s_drop) ", node->as.let_stmt.type->name);
                needs_rc = false;
            }
            if (needs_rc) emit(buf, "URUS_RAII(%s) ", dtor);
        }

        gen_type(buf, node->as.let_stmt.type);
        emit(buf, " %s = ", node->as.let_stmt.name);
        gen_expr(buf, node->as.let_stmt.init);
        emit(buf, ";\n");
        break;
    case NODE_ASSIGN_STMT: {
        // Check if target is array index - use setter instead
        // TODO: Add drop RAII handle here
        if (node->as.assign_stmt.target->kind == NODE_INDEX &&
            node->as.assign_stmt.op == TOK_ASSIGN) {
            gen_expr_pre(buf, node->as.assign_stmt.value);
            AstType *elem = node->as.assign_stmt.target->resolved_type;
            const char *ctype = elem_ctype(elem);
            emit_indent(buf);
            emit(buf, "urus_array_set(");
            gen_expr(buf, node->as.assign_stmt.target->as.index_expr.object);
            emit(buf, ", ");
            gen_expr(buf, node->as.assign_stmt.target->as.index_expr.index);
            emit(buf, ", &(%s){", ctype);
            gen_expr(buf, node->as.assign_stmt.value);
            emit(buf, "});\n");
        } else if (node->as.assign_stmt.op == TOK_PLUS_EQ &&
                   node->as.assign_stmt.target->resolved_type &&
                   node->as.assign_stmt.target->resolved_type->kind == TYPE_STR) {
            // String +=: expand to target = urus_str_concat(target, value)
            gen_expr_pre(buf, node->as.assign_stmt.value);
            emit_indent(buf);
            gen_expr(buf, node->as.assign_stmt.target);
            emit(buf, " = urus_str_concat(");
            gen_expr(buf, node->as.assign_stmt.target);
            emit(buf, ", ");
            gen_expr(buf, node->as.assign_stmt.value);
            emit(buf, ");\n");
        } else {
            gen_expr_pre(buf, node->as.assign_stmt.value);
            emit_indent(buf);
            gen_expr(buf, node->as.assign_stmt.target);
            const char *op = "=";
            switch (node->as.assign_stmt.op) {
            case TOK_PLUS_EQ:  op = "+="; break;
            case TOK_MINUS_EQ: op = "-="; break;
            case TOK_STAR_EQ:  op = "*="; break;
            case TOK_SLASH_EQ: op = "/="; break;
            default: break;
            }
            emit(buf, " %s ", op);
            gen_expr(buf, node->as.assign_stmt.value);
            emit(buf, ";\n");
        }
        break;
    }
    case NODE_IF_STMT:
        emit_indent(buf);
        emit(buf, "if (");
        gen_expr(buf, node->as.if_stmt.condition);
        emit(buf, ") ");
        gen_block(buf, node->as.if_stmt.then_block);
        if (node->as.if_stmt.else_branch) {
            emit(buf, " else ");
            if (node->as.if_stmt.else_branch->kind == NODE_IF_STMT) {
                gen_stmt(buf, node->as.if_stmt.else_branch);
            } else {
                gen_block(buf, node->as.if_stmt.else_branch);
                emit(buf, "\n");
            }
        } else {
            emit(buf, "\n");
        }
        break;
    case NODE_WHILE_STMT:
        emit_indent(buf);
        emit(buf, "while (");
        gen_expr(buf, node->as.while_stmt.condition);
        emit(buf, ") ");
        gen_block(buf, node->as.while_stmt.body);
        emit(buf, "\n");
        break;
    case NODE_FOR_STMT:
        if (node->as.for_stmt.is_foreach) {
            // For-each: for item in array { ... }
            gen_expr_pre(buf, node->as.for_stmt.iterable);
            int tmp = buf->tmp_counter++;
            char iterator_name[64];
            snprintf(iterator_name, sizeof(iterator_name), "_urus_iter_%d", tmp);
            emit_indent(buf);
            emit(buf, "urus_array* %s = ", iterator_name);
            gen_expr(buf, node->as.for_stmt.iterable);
            emit(buf, ";\n");

            emit_indent(buf);
            emit(buf, "for (int64_t _urus_idx_%d = 0; _urus_idx_%d < (int64_t)_urus_iter_%d->len; _urus_idx_%d++) ",
                 tmp, tmp, tmp, tmp);
            emit(buf, "{\n");
            buf->indent++;

            // Determine element type and getter
            AstType *elem = NULL;
            if (node->as.for_stmt.iterable->resolved_type &&
                node->as.for_stmt.iterable->resolved_type->kind == TYPE_ARRAY) {
                elem = node->as.for_stmt.iterable->resolved_type->element;
            }
            const char *getter = "urus_array_get_int";
            if (elem) {
                switch (elem->kind) {
                case TYPE_FLOAT: getter = "urus_array_get_float"; break;
                case TYPE_BOOL:  getter = "urus_array_get_bool"; break;
                case TYPE_STR:   getter = "urus_array_get_str"; break;
                case TYPE_NAMED:
                case TYPE_ARRAY: getter = "urus_array_get_ptr"; break;
                default: break;
                }
            }
            emit_indent(buf);
            if (elem) gen_type(buf, elem); else emit(buf, "int64_t");
            emit(buf, " %s = %s(_urus_iter_%d, _urus_idx_%d);\n",
                 node->as.for_stmt.var_name, getter, tmp, tmp);

            // Emit body statements
            for (int i = 0; i < node->as.for_stmt.body->as.block.stmt_count; i++) {
                gen_stmt(buf, node->as.for_stmt.body->as.block.stmts[i]);
            }
            buf->indent--;
            emit_indent(buf);
            emit(buf, "}\n");
        } else {
            emit_indent(buf);
            emit(buf, "for (int64_t %s = ", node->as.for_stmt.var_name);
            gen_expr(buf, node->as.for_stmt.start);
            emit(buf, "; %s %s ", node->as.for_stmt.var_name,
                 node->as.for_stmt.inclusive ? "<=" : "<");
            gen_expr(buf, node->as.for_stmt.end);
            emit(buf, "; %s++) ", node->as.for_stmt.var_name);
            gen_block(buf, node->as.for_stmt.body);
            emit(buf, "\n");
        }
        break;
    case NODE_RETURN_STMT:
        if (node->as.return_stmt.value) {
            gen_expr_pre(buf, node->as.return_stmt.value);

            int tmp = buf->tmp_counter++;
            AstType *t = node->as.return_stmt.value->resolved_type;

            emit_indent(buf);
            gen_type(buf, t);
            emit(buf, " _urus_ret_%d = ", tmp);
            gen_expr(buf, node->as.return_stmt.value);
            emit(buf, ";\n");

            if (type_needs_rc(t) && node->as.return_stmt.value->kind == NODE_IDENT) {
                // the identifier must be set to NULL to prevent destructed from URUS_RAII()
                emit_indent(buf);
                emit(buf, "%s = NULL; // move to _urus_ret_%d\n", node->as.return_stmt.value->as.ident.name, tmp);
            }

            emit_indent(buf);
            emit(buf, "return _urus_ret_%d;\n", tmp);
        } else {
            emit_indent(buf);
            emit(buf, "return;\n");
        }
        break;
    case NODE_BREAK_STMT:
        emit_indent(buf);
        emit(buf, "break;\n");
        break;
    case NODE_CONTINUE_STMT:
        emit_indent(buf);
        emit(buf, "continue;\n");
        break;
    case NODE_EXPR_STMT:
        gen_expr_pre(buf, node->as.expr_stmt.expr);
        emit_indent(buf);
        gen_expr(buf, node->as.expr_stmt.expr);
        emit(buf, ";\n");
        break;
    case NODE_BLOCK:
        gen_block(buf, node);
        emit(buf, "\n");
        break;
    case NODE_MATCH: {
        gen_expr_pre(buf, node->as.match_stmt.target);
        // Store target in temp
        int tmp = buf->tmp_counter++;
        emit_indent(buf);
        // Determine enum type name from first arm
        const char *ename = "";
        if (node->as.match_stmt.arm_count > 0) {
            ename = node->as.match_stmt.arms[0].enum_name;
        }
        emit(buf, "%s* _urus_match_%d = ", ename, tmp);
        gen_expr(buf, node->as.match_stmt.target);
        emit(buf, ";\n");

        for (int i = 0; i < node->as.match_stmt.arm_count; i++) {
            MatchArm *arm = &node->as.match_stmt.arms[i];
            emit_indent(buf);
            if (i == 0) emit(buf, "if ");
            else emit(buf, "else if ");
            emit(buf, "(_urus_match_%d->tag == %s_TAG_%s) ", tmp, arm->enum_name, arm->variant_name);
            emit(buf, "{\n");
            buf->indent++;
            // Bind variant fields
            for (int b = 0; b < arm->binding_count; b++) {
                emit_indent(buf);
                if (arm->binding_types && arm->binding_types[b]) {
                    gen_type(buf, arm->binding_types[b]);
                } else {
                    emit(buf, "int64_t");
                }
                emit(buf, " %s = _urus_match_%d->data.%s.f%d;\n",
                     arm->bindings[b], tmp, arm->variant_name, b);
            }
            // Emit body statements
            for (int s = 0; s < arm->body->as.block.stmt_count; s++) {
                gen_stmt(buf, arm->body->as.block.stmts[s]);
            }
            buf->indent--;
            emit_indent(buf);
            emit(buf, "}\n");
        }
        break;
    }
    default:
        emit_indent(buf);
        emit(buf, "/* unsupported stmt */\n");
        break;
    }
}

static void gen_block(CodeBuf *buf, AstNode *node) {
    emit(buf, "{\n");
    buf->indent++;
    for (int i = 0; i < node->as.block.stmt_count; i++) {
        gen_stmt(buf, node->as.block.stmts[i]);
    }
    buf->indent--;
    emit_indent(buf);
    emit(buf, "}");
}

// ---- Top-level declarations ----

static void gen_enum_decl(CodeBuf *buf, AstNode *node) {
    const char *name = node->as.enum_decl.name;

    // Tag enum
    emit(buf, "enum {\n");
    for (int i = 0; i < node->as.enum_decl.variant_count; i++) {
        emit(buf, "    %s_TAG_%s = %d,\n", name, node->as.enum_decl.variants[i].name, i);
    }
    emit(buf, "};\n\n");

    // Tagged union struct
    emit(buf, "typedef struct %s {\n", name);
    emit(buf, "    int tag;\n");
    emit(buf, "    union {\n");
    for (int i = 0; i < node->as.enum_decl.variant_count; i++) {
        EnumVariant *v = &node->as.enum_decl.variants[i];
        if (v->field_count > 0) {
            emit(buf, "        struct {\n");
            for (int j = 0; j < v->field_count; j++) {
                emit(buf, "            ");
                gen_type(buf, v->fields[j].type);
                emit(buf, " f%d; // %s\n", j, v->fields[j].name);
            }
            emit(buf, "        } %s;\n", v->name);
        }
    }
    emit(buf, "    } data;\n");
    emit(buf, "} %s;\n\n", name);
}

static void gen_fn_forward(CodeBuf *buf, AstNode *node) {
    bool is_main = strcmp(node->as.fn_decl.name, "main") == 0;
    gen_type(buf, node->as.fn_decl.return_type);
    emit(buf, " %s(", is_main ? "urus_main" : node->as.fn_decl.name);
    if (node->as.fn_decl.param_count == 0) {
        emit(buf, "void");
    } else {
        for (int i = 0; i < node->as.fn_decl.param_count; i++) {
            if (i > 0) emit(buf, ", ");
            gen_type(buf, node->as.fn_decl.params[i].type);
            emit(buf, " %s", node->as.fn_decl.params[i].name);
        }
    }
    emit(buf, ");\n");
}

static void gen_fn_decl(CodeBuf *buf, AstNode *node) {
    bool is_main = strcmp(node->as.fn_decl.name, "main") == 0;
    gen_type(buf, node->as.fn_decl.return_type);
    emit(buf, " %s(", is_main ? "urus_main" : node->as.fn_decl.name);
    if (node->as.fn_decl.param_count == 0) {
        emit(buf, "void");
    } else {
        for (int i = 0; i < node->as.fn_decl.param_count; i++) {
            if (i > 0) emit(buf, ", ");
            gen_type(buf, node->as.fn_decl.params[i].type);
            emit(buf, " %s", node->as.fn_decl.params[i].name);
        }
    }
    emit(buf, ") ");
    gen_block(buf, node->as.fn_decl.body);
    emit(buf, "\n\n");
}

// ---- Program ----

void codegen_generate(CodeBuf *buf, AstNode *program) {
    emit(buf, "// Generated by: URUS Compiler, version %s\n", URUS_COMPILER_VERSION);
    emit(buf, "%.*s\n", urus_runtime_header_data_len, urus_runtime_header_data);
    emit(buf, "\n\n/* +---+ Program start +---+ */\n\n");

    // Pass 1: struct and enum forward declarations
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_STRUCT_DECL) {
            emit(buf, "typedef struct %s %s;\n", d->as.struct_decl.name, d->as.struct_decl.name);
        }
        // Enums get forward-declared via typedef
        if (d->kind == NODE_ENUM_DECL) {
            emit(buf, "typedef struct %s %s;\n", d->as.enum_decl.name, d->as.enum_decl.name);
        }
    }
    emit(buf, "\n");

    // Pass 2: struct definitions
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_STRUCT_DECL) {
            emit(buf, "struct %s {\n", d->as.struct_decl.name);
            for (int j = 0; j < d->as.struct_decl.field_count; j++) {
                emit(buf, "    ");
                gen_type(buf, d->as.struct_decl.fields[j].type);
                emit(buf, " %s;\n", d->as.struct_decl.fields[j].name);
            }
            emit(buf, "};\n\n");
        }
    }

    // Pass 2b: enum definitions
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_ENUM_DECL) {
            gen_enum_decl(buf, d);
        }
    }

    // Pass 3: function forward declarations
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_FN_DECL) {
            gen_fn_forward(buf, d);
        }
    }
    emit(buf, "\n");

    // Pass 3B: struct drop forward declarations
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_STRUCT_DECL) {
            emit(buf, "static void %s_drop(%s **obj);\n", d->as.struct_decl.name, d->as.struct_decl.name);
        }
    }

    // Pass 3C: enum drop forward declarations
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_ENUM_DECL) {
            emit(buf, "static void %s_drop(%s **obj);\n", d->as.enum_decl.name, d->as.enum_decl.name);
        }
    }
    emit(buf, "\n");

    // Pass 3D: struct drop function
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_STRUCT_DECL) {
            emit(buf, "static void %s_drop(%s **obj) {\n", d->as.struct_decl.name, d->as.struct_decl.name);
            emit(buf, "    if (obj && *obj) {\n");
            for (int j = 0; j < d->as.struct_decl.field_count; j++) {
                AstType *ft = d->as.struct_decl.fields[j].type;
                if (type_needs_rc(ft)) {
                    char field_acc[256];
                    snprintf(field_acc, sizeof(field_acc), "(*obj)->%s", d->as.struct_decl.fields[j].name);
                    emit(buf, "        ");
                    emit_type_drop_cname(buf, ft, field_acc);
                }
            }
            emit(buf,
                    "        free(*obj);\n"
                    "        *obj = NULL;\n"
                    "    }\n"
                    "}\n\n"
            );
        }
    }

    // Pass 3E: enum drop function
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_ENUM_DECL) {
            emit(buf, "static void %s_drop(%s **obj) {\n", d->as.enum_decl.name, d->as.enum_decl.name);
            emit(buf, "    if (obj && *obj) {\n");
            emit(buf, "        switch ((*obj)->tag) {\n");

            for (int j = 0; j < d->as.enum_decl.variant_count; j++) {
                EnumVariant *v = &d->as.enum_decl.variants[j];
                if (v->field_count > 0) {
                    emit(buf, "            case %s_TAG_%s:\n", d->as.enum_decl.name, v->name);
                    for (int k = 0; k < v->field_count; k++) {
                        if (type_needs_rc(v->fields[k].type)) {
                            char field_acc[256];
                            snprintf(field_acc, sizeof(field_acc), "(*obj)->data.%s.f%d", v->name, k);
                            emit(buf, "                ");
                            emit_type_drop_cname(buf, v->fields[k].type, field_acc);
                        }
                    }
                    emit(buf, "                break;\n");
                }
            }

            emit(buf,
                    "        }\n"
                    "        free(*obj);\n"
                    "        *obj = NULL;\n"
                    "    }\n"
                    "}\n\n"
            );
        }
    }

    // Pass 4: function definitions
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_FN_DECL) {
            gen_fn_decl(buf, d);
        }
    }

    // C main wrapper
    emit(buf,
        "int main() {\n"
        "   urus_main();\n"
        "   return 0;\n"
        "}\n"
    );
}
