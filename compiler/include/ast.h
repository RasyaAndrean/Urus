#ifndef URUS_AST_H
#define URUS_AST_H

#include "token.h"
#include <stdbool.h>

// Forward declarations
typedef struct AstNode AstNode;
typedef struct AstType AstType;

// ---- Type representation ----
typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_STR,
    TYPE_VOID,
    TYPE_ARRAY,   // element type in child
    TYPE_NAMED,   // struct or enum name
    TYPE_RESULT,  // Result<ok_type, err_type>
    TYPE_FN,      // fn(T1, T2) -> R
} TypeKind;

struct AstType {
    TypeKind kind;
    char *name;         // for TYPE_NAMED
    AstType *element;   // for TYPE_ARRAY
    AstType *ok_type;   // for TYPE_RESULT
    AstType *err_type;  // for TYPE_RESULT
    // for TYPE_FN
    AstType **param_types;
    int param_count;
    AstType *return_type;
};

// ---- AST Node kinds ----
typedef enum {
    // Top-level
    NODE_PROGRAM,
    NODE_FN_DECL,
    NODE_STRUCT_DECL,
    NODE_ENUM_DECL,
    NODE_IMPORT,

    // Statements
    NODE_BLOCK,
    NODE_LET_STMT,
    NODE_ASSIGN_STMT,
    NODE_IF_STMT,
    NODE_WHILE_STMT,
    NODE_FOR_STMT,
    NODE_RETURN_STMT,
    NODE_BREAK_STMT,
    NODE_CONTINUE_STMT,
    NODE_EXPR_STMT,
    NODE_MATCH,

    // Expressions
    NODE_BINARY,
    NODE_UNARY,
    NODE_CALL,
    NODE_FIELD_ACCESS,
    NODE_INDEX,
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STR_LIT,
    NODE_BOOL_LIT,
    NODE_IDENT,
    NODE_ARRAY_LIT,
    NODE_STRUCT_LIT,
    NODE_ENUM_INIT,   // EnumName.Variant or EnumName.Variant(args)
    NODE_OK_EXPR,     // Ok(value)
    NODE_ERR_EXPR,    // Err(value)
    NODE_LAMBDA,      // |params| -> type { body }
} NodeKind;

// ---- Param ----
typedef struct {
    char *name;
    AstType *type;
} Param;

// ---- Struct field init ----
typedef struct {
    char *name;
    AstNode *value;
} FieldInit;

// ---- Enum variant ----
typedef struct {
    char *name;
    Param *fields;  // NULL if no data
    int field_count;
} EnumVariant;

// ---- Match arm ----
typedef struct {
    char *enum_name;    // e.g. "Shape"
    char *variant_name; // e.g. "Circle"
    char **bindings;    // bound variable names, e.g. ["r"]
    AstType **binding_types; // resolved types for each binding (filled by sema)
    int binding_count;
    bool is_wildcard;   // true for _ arm
    AstNode *pattern_expr; // for primitive match (int/str/bool literal), NULL for enum
    AstNode *body;      // block
} MatchArm;

// ---- AST Node ----
struct AstNode {
    NodeKind kind;
    Token tok;

    union {
        // NODE_PROGRAM
        struct { AstNode **decls; int decl_count; } program;

        // NODE_FN_DECL
        struct {
            char *name;
            Param *params; int param_count;
            AstType *return_type;
            AstNode *body; // block
        } fn_decl;

        // NODE_STRUCT_DECL
        struct {
            char *name;
            Param *fields; int field_count;
        } struct_decl;

        // NODE_BLOCK
        struct { AstNode **stmts; int stmt_count; } block;

        // NODE_LET_STMT
        struct {
            char *name;
            bool is_mut;
            AstType *type;
            AstNode *init;
        } let_stmt;

        // NODE_ASSIGN_STMT
        struct {
            AstNode *target; // lvalue
            TokenType op;    // ASSIGN, PLUS_EQ, etc.
            AstNode *value;
        } assign_stmt;

        // NODE_IF_STMT
        struct {
            AstNode *condition;
            AstNode *then_block;
            AstNode *else_branch; // block or another if_stmt, or NULL
        } if_stmt;

        // NODE_WHILE_STMT
        struct { AstNode *condition; AstNode *body; } while_stmt;

        // NODE_FOR_STMT
        struct {
            char *var_name;
            AstNode *start;      // NULL for foreach
            AstNode *end;        // NULL for foreach
            AstNode *iterable;   // non-NULL for foreach
            bool inclusive;
            bool is_foreach;
            AstNode *body;
        } for_stmt;

        // NODE_RETURN_STMT
        struct { AstNode *value; /* NULL if void */ } return_stmt;

        // NODE_EXPR_STMT
        struct { AstNode *expr; } expr_stmt;

        // NODE_BINARY
        struct { AstNode *left; TokenType op; AstNode *right; } binary;

        // NODE_UNARY
        struct { TokenType op; AstNode *operand; } unary;

        // NODE_CALL
        struct { AstNode *callee; AstNode **args; int arg_count; } call;

        // NODE_FIELD_ACCESS
        struct { AstNode *object; char *field; } field_access;

        // NODE_INDEX
        struct { AstNode *object; AstNode *index; } index_expr;

        // NODE_INT_LIT
        struct { long long value; } int_lit;

        // NODE_FLOAT_LIT
        struct { double value; } float_lit;

        // NODE_STR_LIT
        struct { char *value; } str_lit;

        // NODE_BOOL_LIT
        struct { bool value; } bool_lit;

        // NODE_IDENT
        struct { char *name; } ident;

        // NODE_ARRAY_LIT
        struct { AstNode **elements; int count; } array_lit;

        // NODE_STRUCT_LIT
        struct { char *name; FieldInit *fields; int field_count; } struct_lit;

        // NODE_ENUM_DECL
        struct {
            char *name;
            EnumVariant *variants;
            int variant_count;
        } enum_decl;

        // NODE_MATCH
        struct {
            AstNode *target;
            MatchArm *arms;
            int arm_count;
        } match_stmt;

        // NODE_ENUM_INIT  (EnumName.Variant(args...))
        struct {
            char *enum_name;
            char *variant_name;
            AstNode **args;
            int arg_count;
        } enum_init;

        // NODE_IMPORT
        struct { char *path; } import_decl;

        // NODE_OK_EXPR / NODE_ERR_EXPR
        struct { AstNode *value; } result_expr;

        // NODE_LAMBDA
        struct {
            Param *params;
            int param_count;
            AstType *return_type;
            AstNode *body;
            char **captures;     // captured variable names (filled by sema)
            AstType **capture_types; // captured variable types
            int capture_count;
            int lambda_id;       // unique ID for codegen
        } lambda;
    } as;

    // Filled by semantic analysis
    AstType *resolved_type;

    // Filled by codegen (temp variable ID)
    int _codegen_tmp;
};

// Constructors
AstNode *ast_new(NodeKind kind, Token tok);
AstType *ast_type_simple(TypeKind kind);
AstType *ast_type_array(AstType *element);
AstType *ast_type_named(const char *name);
AstType *ast_type_result(AstType *ok_type, AstType *err_type);
AstType *ast_type_fn(AstType **param_types, int param_count, AstType *return_type);
char *ast_strdup(const char *s, size_t len);

// Type utilities
AstType *ast_type_clone(AstType *t);
bool ast_types_equal(AstType *a, AstType *b);
const char *ast_type_str(AstType *t); // returns static buffer (round-robin)

// Debug printing
void ast_print(AstNode *node, int indent);

// Cleanup
void ast_free(AstNode *node);
void ast_type_free(AstType *type);

#endif
