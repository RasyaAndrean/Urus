#ifndef URUS_RUNTIME_H
#define URUS_RUNTIME_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <ctype.h>

// ============================================================
// String (ref-counted)
// ============================================================

typedef struct {
    int rc;
    size_t len;
    char data[];
} urus_str;

static urus_str *urus_str_new(const char *s, size_t len) {
    urus_str *str = (urus_str *)malloc(sizeof(urus_str) + len + 1);
    str->rc = 1;
    str->len = len;
    memcpy(str->data, s, len);
    str->data[len] = '\0';
    return str;
}

static urus_str *urus_str_from(const char *s) {
    return urus_str_new(s, strlen(s));
}

static void urus_str_retain(urus_str *s) { if (s) s->rc++; }
static void urus_str_release(urus_str *s) { if (s && --s->rc <= 0) free(s); }

static urus_str *urus_str_concat(urus_str *a, urus_str *b) {
    size_t len = a->len + b->len;
    urus_str *str = (urus_str *)malloc(sizeof(urus_str) + len + 1);
    str->rc = 1;
    str->len = len;
    memcpy(str->data, a->data, a->len);
    memcpy(str->data + a->len, b->data, b->len);
    str->data[len] = '\0';
    return str;
}

// ---- String stdlib ----

static int64_t urus_str_len(urus_str *s) {
    return (int64_t)s->len;
}

static urus_str *urus_str_slice(urus_str *s, int64_t start, int64_t end) {
    if (start < 0) start = 0;
    if (end > (int64_t)s->len) end = (int64_t)s->len;
    if (start >= end) return urus_str_from("");
    return urus_str_new(s->data + start, (size_t)(end - start));
}

static int64_t urus_str_find(urus_str *s, urus_str *sub) {
    char *p = strstr(s->data, sub->data);
    if (!p) return -1;
    return (int64_t)(p - s->data);
}

static bool urus_str_contains(urus_str *s, urus_str *sub) {
    return strstr(s->data, sub->data) != NULL;
}

static urus_str *urus_str_upper(urus_str *s) {
    urus_str *r = urus_str_new(s->data, s->len);
    for (size_t i = 0; i < r->len; i++) r->data[i] = (char)toupper((unsigned char)r->data[i]);
    return r;
}

static urus_str *urus_str_lower(urus_str *s) {
    urus_str *r = urus_str_new(s->data, s->len);
    for (size_t i = 0; i < r->len; i++) r->data[i] = (char)tolower((unsigned char)r->data[i]);
    return r;
}

static urus_str *urus_str_trim(urus_str *s) {
    size_t start = 0, end = s->len;
    while (start < end && isspace((unsigned char)s->data[start])) start++;
    while (end > start && isspace((unsigned char)s->data[end - 1])) end--;
    return urus_str_new(s->data + start, end - start);
}

static urus_str *urus_str_replace(urus_str *s, urus_str *old, urus_str *new_s) {
    if (old->len == 0) return urus_str_new(s->data, s->len);
    int count = 0;
    const char *p = s->data;
    while ((p = strstr(p, old->data)) != NULL) { count++; p += old->len; }
    if (count == 0) return urus_str_new(s->data, s->len);

    // Use signed arithmetic to avoid unsigned underflow
    ptrdiff_t diff = (ptrdiff_t)new_s->len - (ptrdiff_t)old->len;
    size_t new_len = (size_t)((ptrdiff_t)s->len + count * diff);
    urus_str *r = (urus_str *)malloc(sizeof(urus_str) + new_len + 1);
    r->rc = 1; r->len = new_len;

    char *dst = r->data;
    p = s->data;
    const char *q;
    while ((q = strstr(p, old->data)) != NULL) {
        size_t chunk = (size_t)(q - p);
        memcpy(dst, p, chunk); dst += chunk;
        memcpy(dst, new_s->data, new_s->len); dst += new_s->len;
        p = q + old->len;
    }
    strcpy(dst, p);
    return r;
}

static bool urus_str_starts_with(urus_str *s, urus_str *prefix) {
    if (prefix->len > s->len) return false;
    return memcmp(s->data, prefix->data, prefix->len) == 0;
}

static bool urus_str_ends_with(urus_str *s, urus_str *suffix) {
    if (suffix->len > s->len) return false;
    return memcmp(s->data + s->len - suffix->len, suffix->data, suffix->len) == 0;
}

static urus_str *urus_char_at(urus_str *s, int64_t i) {
    if (i < 0 || i >= (int64_t)s->len) return urus_str_from("");
    return urus_str_new(s->data + i, 1);
}

// ============================================================
// Array (ref-counted, generic)
// ============================================================

typedef struct {
    int rc;
    size_t len;
    size_t cap;
    size_t elem_size;
    void *data;
} urus_array;

static urus_array *urus_array_new(size_t elem_size, size_t initial_cap);
static void urus_array_push(urus_array *arr, const void *elem);

// Forward declare for str_split
static urus_array *urus_str_split(urus_str *s, urus_str *delim) {
    urus_array *arr = urus_array_new(sizeof(urus_str *), 4);
    if (delim->len == 0) {
        for (size_t i = 0; i < s->len; i++) {
            urus_str *c = urus_str_new(s->data + i, 1);
            urus_array_push(arr, &c);
        }
        return arr;
    }
    const char *p = s->data;
    const char *q;
    while ((q = strstr(p, delim->data)) != NULL) {
        urus_str *part = urus_str_new(p, (size_t)(q - p));
        urus_array_push(arr, &part);
        p = q + delim->len;
    }
    urus_str *last = urus_str_from(p);
    urus_array_push(arr, &last);
    return arr;
}

static urus_array *urus_array_new(size_t elem_size, size_t initial_cap) {
    urus_array *arr = (urus_array *)malloc(sizeof(urus_array));
    arr->rc = 1;
    arr->len = 0;
    arr->cap = initial_cap > 0 ? initial_cap : 4;
    arr->elem_size = elem_size;
    arr->data = malloc(arr->elem_size * arr->cap);
    return arr;
}

static void urus_array_retain(urus_array *a) { if (a) a->rc++; }
static void urus_array_release(urus_array *a) {
    if (a && --a->rc <= 0) { free(a->data); free(a); }
}

static void urus_array_push(urus_array *arr, const void *elem) {
    if (arr->len >= arr->cap) {
        arr->cap *= 2;
        arr->data = realloc(arr->data, arr->elem_size * arr->cap);
    }
    memcpy((char *)arr->data + arr->len * arr->elem_size, elem, arr->elem_size);
    arr->len++;
}

static void *urus_array_get_ptr(urus_array *arr, size_t index) {
    if (index >= arr->len) {
        fprintf(stderr, "Error: array index %zu out of bounds (len=%zu)\n", index, arr->len);
        exit(1);
    }
    return (char *)arr->data + index * arr->elem_size;
}

static int64_t urus_array_get_int(urus_array *arr, size_t index) {
    return *(int64_t *)urus_array_get_ptr(arr, index);
}

static double urus_array_get_float(urus_array *arr, size_t index) {
    return *(double *)urus_array_get_ptr(arr, index);
}

static bool urus_array_get_bool(urus_array *arr, size_t index) {
    return *(bool *)urus_array_get_ptr(arr, index);
}

static urus_str *urus_array_get_str(urus_array *arr, size_t index) {
    return *(urus_str **)urus_array_get_ptr(arr, index);
}

static void urus_array_set(urus_array *arr, size_t index, const void *elem) {
    if (index >= arr->len) {
        fprintf(stderr, "Error: array index %zu out of bounds (len=%zu)\n", index, arr->len);
        exit(1);
    }
    memcpy((char *)arr->data + index * arr->elem_size, elem, arr->elem_size);
}

static int64_t urus_len(urus_array *arr) {
    return (int64_t)arr->len;
}

static void urus_pop(urus_array *arr) {
    if (arr->len > 0) arr->len--;
}

// ============================================================
// to_str / to_int / to_float conversions
// ============================================================

static urus_str *urus_int_to_str(int64_t v) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%" PRId64, v);
    return urus_str_new(buf, (size_t)n);
}

static urus_str *urus_float_to_str(double v) {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%g", v);
    return urus_str_new(buf, (size_t)n);
}

static urus_str *urus_bool_to_str(bool v) {
    return v ? urus_str_from("true") : urus_str_from("false");
}

static urus_str *urus_str_to_str(urus_str *s) {
    urus_str_retain(s);
    return s;
}

static int64_t urus_str_to_int(urus_str *s) { return strtoll(s->data, NULL, 10); }
static int64_t urus_float_to_int(double v) { return (int64_t)v; }
static int64_t urus_int_to_int(int64_t v) { return v; }

static double urus_str_to_float(urus_str *s) { return strtod(s->data, NULL); }
static double urus_int_to_float(int64_t v) { return (double)v; }
static double urus_float_to_float(double v) { return v; }

// ============================================================
// print
// ============================================================

static void urus_print_str(urus_str *s) { printf("%s\n", s->data); }
static void urus_print_int(int64_t v) { printf("%" PRId64 "\n", v); }
static void urus_print_float(double v) { printf("%g\n", v); }
static void urus_print_bool(bool v) { printf("%s\n", v ? "true" : "false"); }

#define urus_print(x) _Generic((x), \
    urus_str*: urus_print_str, \
    int64_t:   urus_print_int, \
    int:       urus_print_int, \
    double:    urus_print_float, \
    bool:      urus_print_bool \
)(x)

#define to_str(x) _Generic((x), \
    int64_t:   urus_int_to_str, \
    int:       urus_int_to_str, \
    double:    urus_float_to_str, \
    bool:      urus_bool_to_str, \
    urus_str*: urus_str_to_str \
)(x)

#define to_int(x) _Generic((x), \
    urus_str*: urus_str_to_int, \
    double:    urus_float_to_int, \
    int64_t:   urus_int_to_int, \
    int:       urus_int_to_int \
)(x)

#define to_float(x) _Generic((x), \
    urus_str*: urus_str_to_float, \
    int64_t:   urus_int_to_float, \
    int:       urus_int_to_float, \
    double:    urus_float_to_float \
)(x)

// ============================================================
// Math
// ============================================================

static int64_t urus_abs(int64_t x) { return x < 0 ? -x : x; }
static double urus_fabs(double x) { return fabs(x); }
static double urus_sqrt(double x) { return sqrt(x); }
static double urus_pow(double x, double y) { return pow(x, y); }
static int64_t urus_min(int64_t a, int64_t b) { return a < b ? a : b; }
static int64_t urus_max(int64_t a, int64_t b) { return a > b ? a : b; }
static double urus_fmin(double a, double b) { return fmin(a, b); }
static double urus_fmax(double a, double b) { return fmax(a, b); }

// ============================================================
// I/O
// ============================================================

static urus_str *urus_input(void) {
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) return urus_str_from("");
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') len--;
    return urus_str_new(buf, len);
}

static urus_str *urus_read_file(urus_str *path) {
    FILE *f = fopen(path->data, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open file '%s'\n", path->data);
        return urus_str_from("");
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);
    urus_str *s = urus_str_new(buf, (size_t)sz);
    free(buf);
    return s;
}

static void urus_write_file(urus_str *path, urus_str *content) {
    FILE *f = fopen(path->data, "wb");
    if (!f) { fprintf(stderr, "Error: cannot write file '%s'\n", path->data); return; }
    fwrite(content->data, 1, content->len, f);
    fclose(f);
}

static void urus_append_file(urus_str *path, urus_str *content) {
    FILE *f = fopen(path->data, "ab");
    if (!f) { fprintf(stderr, "Error: cannot append file '%s'\n", path->data); return; }
    fwrite(content->data, 1, content->len, f);
    fclose(f);
}

// ============================================================
// Result type (tagged union: Ok or Err)
// ============================================================

typedef union {
    int64_t as_int;
    double as_float;
    bool as_bool;
    void *as_ptr;
} urus_box;

typedef struct {
    int rc;
    int tag;  // 0 = Ok, 1 = Err
    union {
        urus_box ok;
        urus_str *err;
    } data;
} urus_result;

static urus_result *urus_result_ok(urus_box *val) {
    urus_result *r = (urus_result *)malloc(sizeof(urus_result));
    r->rc = 1;
    r->tag = 0;
    r->data.ok = *val;
    return r;
}

static urus_result *urus_result_err(urus_str *msg) {
    urus_result *r = (urus_result *)malloc(sizeof(urus_result));
    r->rc = 1;
    r->tag = 1;
    r->data.err = msg;
    urus_str_retain(msg);
    return r;
}

static bool urus_result_is_ok(urus_result *r) { return r->tag == 0; }
static bool urus_result_is_err(urus_result *r) { return r->tag == 1; }

static int64_t urus_result_unwrap(urus_result *r) {
    if (r->tag != 0) {
        fprintf(stderr, "Error: unwrap called on Err: %s\n", r->data.err->data);
        exit(1);
    }
    return r->data.ok.as_int;
}

static double urus_result_unwrap_float(urus_result *r) {
    if (r->tag != 0) {
        fprintf(stderr, "Error: unwrap called on Err: %s\n", r->data.err->data);
        exit(1);
    }
    return r->data.ok.as_float;
}

static bool urus_result_unwrap_bool(urus_result *r) {
    if (r->tag != 0) {
        fprintf(stderr, "Error: unwrap called on Err: %s\n", r->data.err->data);
        exit(1);
    }
    return r->data.ok.as_bool;
}

static urus_str *urus_result_unwrap_str(urus_result *r) {
    if (r->tag != 0) {
        fprintf(stderr, "Error: unwrap called on Err: %s\n", r->data.err->data);
        exit(1);
    }
    return (urus_str *)r->data.ok.as_ptr;
}

static void *urus_result_unwrap_ptr(urus_result *r) {
    if (r->tag != 0) {
        fprintf(stderr, "Error: unwrap called on Err: %s\n", r->data.err->data);
        exit(1);
    }
    return r->data.ok.as_ptr;
}

static urus_str *urus_result_unwrap_err(urus_result *r) {
    if (r->tag != 1) {
        fprintf(stderr, "Error: unwrap_err called on Ok\n");
        exit(1);
    }
    return r->data.err;
}

// ============================================================
// Misc
// ============================================================

static void urus_exit(int64_t code) {
    exit((int)code);
}

static void urus_assert(bool cond, urus_str *msg) {
    if (!cond) {
        fprintf(stderr, "Assertion failed: %s\n", msg->data);
        exit(1);
    }
}

#endif
