#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

void lexer_init(Lexer *l, const char *source, size_t length) {
    l->source = source;
    l->length = length;
    l->pos = 0;
    l->line = 1;
}

static char peek(Lexer *l) {
    if (l->pos >= l->length) return '\0';
    return l->source[l->pos];
}

static char peek_next(Lexer *l) {
    if (l->pos + 1 >= l->length) return '\0';
    return l->source[l->pos + 1];
}

static char advance(Lexer *l) {
    char c = l->source[l->pos++];
    if (c == '\n') {
        l->line++;
        l->line_start = l->pos;
    }
    return c;
}

static void skip_whitespace(Lexer *l) {
    while (l->pos < l->length) {
        char c = peek(l);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(l);
        } else if (c == '/' && peek_next(l) == '/') {
            while (l->pos < l->length && peek(l) != '\n') advance(l);
        } else if (c == '/' && peek_next(l) == '*') {
            advance(l); advance(l);
            while (l->pos < l->length) {
                if (peek(l) == '*' && peek_next(l) == '/') {
                    advance(l); advance(l);
                    break;
                }
                advance(l);
            }
        } else {
            break;
        }
    }
}

static Token make_token(Lexer *l, TokenType type, const char *start, size_t len) {
    return (Token){ .type = type, .start = start, .length = len, .line = l->line, .col = (int)(start - (l->source + l->line_start)) + 1 };
}

static Token error_token(Lexer *l, const char *msg) {
    return (Token){ .type = TOK_ERROR, .start = msg, .length = strlen(msg), .line = l->line };
}

static TokenType check_keyword(const char *start, size_t len) {
    typedef struct { const char *kw; size_t kw_len; TokenType type; } KW;
    static const KW keywords[] = {
        {"fn",       2, TOK_FN},
        {"let",      3, TOK_LET},
        {"mut",      3, TOK_MUT},
        {"struct",   6, TOK_STRUCT},
        {"if",       2, TOK_IF},
        {"else",     4, TOK_ELSE},
        {"while",    5, TOK_WHILE},
        {"for",      3, TOK_FOR},
        {"in",       2, TOK_IN},
        {"return",   6, TOK_RETURN},
        {"break",    5, TOK_BREAK},
        {"continue", 8, TOK_CONTINUE},
        {"true",     4, TOK_TRUE},
        {"false",    5, TOK_FALSE},
        {"enum",     4, TOK_ENUM},
        {"match",    5, TOK_MATCH},
        {"import",   6, TOK_IMPORT},
        {"int",      3, TOK_INT},
        {"float",    5, TOK_FLOAT},
        {"bool",     4, TOK_BOOL},
        {"str",      3, TOK_STR},
        {"void",     4, TOK_VOID},
        {"Ok",       2, TOK_OK},
        {"Err",      3, TOK_ERR},
    };
    for (size_t i = 0; i < sizeof(keywords)/sizeof(keywords[0]); i++) {
        if (len == keywords[i].kw_len && memcmp(start, keywords[i].kw, len) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_IDENT;
}

static Token lex_string(Lexer *l) {
    const char *start = l->source + l->pos;
    advance(l); // skip opening "
    while (l->pos < l->length && peek(l) != '"') {
        if (peek(l) == '\\') advance(l);
        advance(l);
    }
    if (l->pos >= l->length) return error_token(l, "unterminated string");
    advance(l); // skip closing "
    return make_token(l, TOK_STR_LIT, start, (size_t)(l->source + l->pos - start));
}

static Token lex_fstring(Lexer *l) {
    // f" already matched: 'f' consumed, now at '"'
    const char *start = l->source + l->pos - 1; // include the 'f'
    advance(l); // skip opening "
    int brace_depth = 0;
    while (l->pos < l->length) {
        char c = peek(l);
        if (c == '{') {
            brace_depth++;
            advance(l);
        } else if (c == '}') {
            brace_depth--;
            advance(l);
        } else if (c == '"' && brace_depth == 0) {
            break; // end of f-string
        } else if (c == '"' && brace_depth > 0) {
            // String literal inside expression — skip it
            advance(l); // skip opening "
            while (l->pos < l->length && peek(l) != '"') {
                if (peek(l) == '\\') advance(l);
                advance(l);
            }
            if (l->pos < l->length) advance(l); // skip closing "
        } else {
            if (c == '\\') advance(l);
            advance(l);
        }
    }
    if (l->pos >= l->length) return error_token(l, "unterminated f-string");
    advance(l); // skip closing "
    return make_token(l, TOK_FSTR_LIT, start, (size_t)(l->source + l->pos - start));
}

static Token lex_number(Lexer *l) {
    const char *start = l->source + l->pos;
    while (isdigit(peek(l))) advance(l);
    if (peek(l) == '.' && isdigit(peek_next(l))) {
        advance(l);
        while (isdigit(peek(l))) advance(l);
        return make_token(l, TOK_FLOAT_LIT, start, (size_t)(l->source + l->pos - start));
    }
    return make_token(l, TOK_INT_LIT, start, (size_t)(l->source + l->pos - start));
}

static Token lex_ident(Lexer *l) {
    const char *start = l->source + l->pos;
    while (isalnum(peek(l)) || peek(l) == '_') advance(l);
    size_t len = (size_t)(l->source + l->pos - start);
    TokenType type = check_keyword(start, len);

    // Check for f-string: identifier 'f' followed by '"'
    if (type == TOK_IDENT && len == 1 && start[0] == 'f' && peek(l) == '"') {
        return lex_fstring(l);
    }

    return make_token(l, type, start, len);
}

Token lexer_next(Lexer *l) {
    skip_whitespace(l);
    if (l->pos >= l->length) {
        return make_token(l, TOK_EOF, l->source + l->pos, 0);
    }

    char c = peek(l);

    if (c == '"') return lex_string(l);
    if (isdigit(c)) return lex_number(l);
    if (isalpha(c) || c == '_') return lex_ident(l);

    const char *start = l->source + l->pos;
    advance(l);

    switch (c) {
    case '(': return make_token(l, TOK_LPAREN, start, 1);
    case ')': return make_token(l, TOK_RPAREN, start, 1);
    case '{': return make_token(l, TOK_LBRACE, start, 1);
    case '}': return make_token(l, TOK_RBRACE, start, 1);
    case '[': return make_token(l, TOK_LBRACKET, start, 1);
    case ']': return make_token(l, TOK_RBRACKET, start, 1);
    case ',': return make_token(l, TOK_COMMA, start, 1);
    case ':': return make_token(l, TOK_COLON, start, 1);
    case ';': return make_token(l, TOK_SEMICOLON, start, 1);
    case '%': return make_token(l, TOK_PERCENT, start, 1);
    case '+':
        if (peek(l) == '=') { advance(l); return make_token(l, TOK_PLUS_EQ, start, 2); }
        return make_token(l, TOK_PLUS, start, 1);
    case '-':
        if (peek(l) == '=') { advance(l); return make_token(l, TOK_MINUS_EQ, start, 2); }
        return make_token(l, TOK_MINUS, start, 1);
    case '*':
        if (peek(l) == '=') { advance(l); return make_token(l, TOK_STAR_EQ, start, 2); }
        return make_token(l, TOK_STAR, start, 1);
    case '/':
        if (peek(l) == '=') { advance(l); return make_token(l, TOK_SLASH_EQ, start, 2); }
        return make_token(l, TOK_SLASH, start, 1);
    case '=':
        if (peek(l) == '=') { advance(l); return make_token(l, TOK_EQ, start, 2); }
        if (peek(l) == '>') { advance(l); return make_token(l, TOK_ARROW, start, 2); }
        return make_token(l, TOK_ASSIGN, start, 1);
    case '!':
        if (peek(l) == '=') { advance(l); return make_token(l, TOK_NEQ, start, 2); }
        return make_token(l, TOK_NOT, start, 1);
    case '<':
        if (peek(l) == '=') { advance(l); return make_token(l, TOK_LTE, start, 2); }
        return make_token(l, TOK_LT, start, 1);
    case '>':
        if (peek(l) == '=') { advance(l); return make_token(l, TOK_GTE, start, 2); }
        return make_token(l, TOK_GT, start, 1);
    case '&':
        if (peek(l) == '&') { advance(l); return make_token(l, TOK_AND, start, 2); }
        return error_token(l, "unexpected '&'");
    case '|':
        if (peek(l) == '|') { advance(l); return make_token(l, TOK_OR, start, 2); }
        return make_token(l, TOK_PIPE, start, 1);
    case '.':
        if (peek(l) == '.') {
            advance(l);
            if (peek(l) == '=') { advance(l); return make_token(l, TOK_DOTDOTEQ, start, 3); }
            return make_token(l, TOK_DOTDOT, start, 2);
        }
        return make_token(l, TOK_DOT, start, 1);
    }

    return error_token(l, "unexpected character");
}

Token *lexer_tokenize(Lexer *l, int *count) {
    int cap = 256;
    int n = 0;
    Token *tokens = malloc(sizeof(Token) * (size_t)cap);

    while (1) {
        if (n >= cap) {
            cap *= 2;
            tokens = realloc(tokens, sizeof(Token) * (size_t)cap);
        }
        tokens[n] = lexer_next(l);
        if (tokens[n].type == TOK_EOF) { n++; break; }
        if (tokens[n].type == TOK_ERROR) {
            fprintf(stderr, "Lexer error at line %d: %.*s\n",
                    tokens[n].line, (int)tokens[n].length, tokens[n].start);
            free(tokens);
            *count = 0;
            return NULL;
        }
        n++;
    }
    *count = n;
    return tokens;
}

const char *token_type_name(TokenType type) {
    switch (type) {
    case TOK_INT_LIT: return "INT_LIT";
    case TOK_FLOAT_LIT: return "FLOAT_LIT";
    case TOK_STR_LIT: return "STR_LIT";
    case TOK_FSTR_LIT: return "FSTR_LIT";
    case TOK_IDENT: return "IDENT";
    case TOK_FN: return "FN";
    case TOK_LET: return "LET";
    case TOK_MUT: return "MUT";
    case TOK_STRUCT: return "STRUCT";
    case TOK_IF: return "IF";
    case TOK_ELSE: return "ELSE";
    case TOK_WHILE: return "WHILE";
    case TOK_FOR: return "FOR";
    case TOK_IN: return "IN";
    case TOK_RETURN: return "RETURN";
    case TOK_BREAK: return "BREAK";
    case TOK_CONTINUE: return "CONTINUE";
    case TOK_TRUE: return "TRUE";
    case TOK_FALSE: return "FALSE";
    case TOK_ENUM: return "ENUM";
    case TOK_MATCH: return "MATCH";
    case TOK_IMPORT: return "IMPORT";
    case TOK_ARROW: return "ARROW";
    case TOK_INT: return "INT";
    case TOK_FLOAT: return "FLOAT";
    case TOK_BOOL: return "BOOL";
    case TOK_STR: return "STR";
    case TOK_VOID: return "VOID";
    case TOK_OK: return "OK";
    case TOK_ERR: return "ERR";
    case TOK_PLUS: return "PLUS";
    case TOK_MINUS: return "MINUS";
    case TOK_STAR: return "STAR";
    case TOK_SLASH: return "SLASH";
    case TOK_PERCENT: return "PERCENT";
    case TOK_EQ: return "EQ";
    case TOK_NEQ: return "NEQ";
    case TOK_LT: return "LT";
    case TOK_GT: return "GT";
    case TOK_LTE: return "LTE";
    case TOK_GTE: return "GTE";
    case TOK_AND: return "AND";
    case TOK_OR: return "OR";
    case TOK_NOT: return "NOT";
    case TOK_ASSIGN: return "ASSIGN";
    case TOK_PLUS_EQ: return "PLUS_EQ";
    case TOK_MINUS_EQ: return "MINUS_EQ";
    case TOK_STAR_EQ: return "STAR_EQ";
    case TOK_SLASH_EQ: return "SLASH_EQ";
    case TOK_DOTDOT: return "DOTDOT";
    case TOK_DOTDOTEQ: return "DOTDOTEQ";
    case TOK_LPAREN: return "LPAREN";
    case TOK_RPAREN: return "RPAREN";
    case TOK_LBRACE: return "LBRACE";
    case TOK_RBRACE: return "RBRACE";
    case TOK_LBRACKET: return "LBRACKET";
    case TOK_RBRACKET: return "RBRACKET";
    case TOK_COMMA: return "COMMA";
    case TOK_COLON: return "COLON";
    case TOK_SEMICOLON: return "SEMICOLON";
    case TOK_DOT: return "DOT";
    case TOK_PIPE: return "PIPE";
    case TOK_EOF: return "EOF";
    case TOK_ERROR: return "ERROR";
    }
    return "UNKNOWN";
}
