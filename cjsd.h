#ifndef CJSD_H
#define CJSD_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

// CJSD - C JSON Serialized/Deserializer
// Single-header library for parsing and serializing JSON in C99

typedef enum {
    CJSD_NULL,
    CJSD_BOOL,
    CJSD_NUMBER,
    CJSD_STRING,
    CJSD_ARRAY,
    CJSD_OBJECT
} cjsd_type_t;

typedef struct cjsd_value {
    cjsd_type_t type;
    union {
        bool boolean;
        double number;
        char* string;
        struct {
            struct cjsd_value* elements;
            size_t count;
            size_t capacity;
        } array;
        struct {
            char** keys;
            struct cjsd_value* values;
            size_t count;
            size_t capacity;
        } object;
    } data;
} cjsd_value_t;

typedef struct {
    bool success;
    union {
        cjsd_value_t value;
        char error[256];
    } result;
} cjsd_parse_result_t;

// API Functions
cjsd_parse_result_t cjsd_parse(const char* json_str);
char* cjsd_serialize(const cjsd_value_t* value);
void cjsd_free_value(cjsd_value_t* value);
void cjsd_free_string(char* str);

// Constructor functions
cjsd_value_t cjsd_null(void);
cjsd_value_t cjsd_bool(bool b);
cjsd_value_t cjsd_number(double n);
cjsd_value_t cjsd_string(const char* s);
cjsd_value_t cjsd_array(void);
cjsd_value_t cjsd_object(void);

// Access functions (functional approach - return copies)
cjsd_value_t cjsd_get_object(const cjsd_value_t* obj, const char* key);
cjsd_value_t cjsd_get_array(const cjsd_value_t* arr, size_t index);

// Modify functions (functional approach - return new values)
cjsd_value_t cjsd_set_object(const cjsd_value_t* obj, const char* key, const cjsd_value_t* val);
cjsd_value_t cjsd_set_array(const cjsd_value_t* arr, size_t index, const cjsd_value_t* val);
cjsd_value_t cjsd_push_array(const cjsd_value_t* arr, const cjsd_value_t* val);

#ifdef CJSD_IMPLEMENTATION

// Internal helper functions
static void skip_whitespace(const char** p, int* line, int* col) {
    while (**p && isspace(**p)) {
        if (**p == '\n') {
            (*line)++;
            *col = 1;
        } else {
            (*col)++;
        }
        (*p)++;
    }
}

static bool parse_string(const char** p, char** out, int* line, int* col) {
    if (**p != '"') return false;
    (*p)++;
    (*col)++;
    const char* start = *p;
    while (**p && **p != '"') {
        if (**p == '\\') {
            (*p) += 2;
            (*col) += 2;
        } else {
            if (**p == '\n') {
                (*line)++;
                *col = 1;
            } else {
                (*col)++;
            }
            (*p)++;
        }
    }
    if (**p != '"') return false;
    size_t len = *p - start;
    *out = malloc(len + 1);
    if (!*out) return false;
    memcpy(*out, start, len);
    (*out)[len] = '\0';
    // Handle escapes (simplified)
    char* q = *out;
    for (char* r = *out; *r; r++, q++) {
        if (*r == '\\') {
            r++;
            switch (*r) {
                case 'n': *q = '\n'; break;
                case 't': *q = '\t'; break;
                case 'r': *q = '\r'; break;
                case '"': *q = '"'; break;
                case '\\': *q = '\\'; break;
                default: *q = *r; break;
            }
        } else {
            *q = *r;
        }
    }
    *q = '\0';
    (*p)++;
    (*col)++;
    return true;
}

static cjsd_parse_result_t parse_value(const char** p, int* line, int* col);

static cjsd_parse_result_t parse_array(const char** p, int* line, int* col) {
    cjsd_parse_result_t res = {false};
    if (**p != '[') {
        snprintf(res.result.error, sizeof(res.result.error), "Expected '[' at line %d, column %d", *line, *col);
        return res;
    }
    (*p)++;
    (*col)++;
    skip_whitespace(p, line, col);
    cjsd_value_t arr = cjsd_array();
    while (**p && **p != ']') {
        cjsd_parse_result_t elem = parse_value(p, line, col);
        if (!elem.success) {
            cjsd_free_value(&arr);
            return elem;
        }
        arr = cjsd_push_array(&arr, &elem.result.value);
        cjsd_free_value(&elem.result.value);
        skip_whitespace(p, line, col);
        if (**p == ',') {
            (*p)++;
            (*col)++;
            skip_whitespace(p, line, col);
        } else if (**p != ']') {
            cjsd_free_value(&arr);
            snprintf(res.result.error, sizeof(res.result.error), "Expected ',' or ']' at line %d, column %d", *line, *col);
            return res;
        }
    }
    if (**p != ']') {
        cjsd_free_value(&arr);
        snprintf(res.result.error, sizeof(res.result.error), "Expected ']' at line %d, column %d", *line, *col);
        return res;
    }
    (*p)++;
    (*col)++;
    res.success = true;
    res.result.value = arr;
    return res;
}

static cjsd_parse_result_t parse_object(const char** p, int* line, int* col) {
    cjsd_parse_result_t res = {false};
    if (**p != '{') {
        snprintf(res.result.error, sizeof(res.result.error), "Expected '{' at line %d, column %d", *line, *col);
        return res;
    }
    (*p)++;
    (*col)++;
    skip_whitespace(p, line, col);
    cjsd_value_t obj = cjsd_object();
    while (**p && **p != '}') {
        char* key;
        if (!parse_string(p, &key, line, col)) {
            cjsd_free_value(&obj);
            snprintf(res.result.error, sizeof(res.result.error), "Expected string key at line %d, column %d", *line, *col);
            return res;
        }
        skip_whitespace(p, line, col);
        if (**p != ':') {
            free(key);
            cjsd_free_value(&obj);
            snprintf(res.result.error, sizeof(res.result.error), "Expected ':' after key at line %d, column %d", *line, *col);
            return res;
        }
        (*p)++;
        (*col)++;
        skip_whitespace(p, line, col);
        cjsd_parse_result_t val_res = parse_value(p, line, col);
        if (!val_res.success) {
            free(key);
            cjsd_free_value(&obj);
            return val_res;
        }
        obj = cjsd_set_object(&obj, key, &val_res.result.value);
        free(key);
        cjsd_free_value(&val_res.result.value);
        skip_whitespace(p, line, col);
        if (**p == ',') {
            (*p)++;
            (*col)++;
            skip_whitespace(p, line, col);
        } else if (**p != '}') {
            cjsd_free_value(&obj);
            snprintf(res.result.error, sizeof(res.result.error), "Expected ',' or '}' at line %d, column %d", *line, *col);
            return res;
        }
    }
    if (**p != '}') {
        cjsd_free_value(&obj);
        snprintf(res.result.error, sizeof(res.result.error), "Expected '}' at line %d, column %d", *line, *col);
        return res;
    }
    (*p)++;
    (*col)++;
    res.success = true;
    res.result.value = obj;
    return res;
}

static cjsd_parse_result_t parse_value(const char** p, int* line, int* col) {
    skip_whitespace(p, line, col);
    cjsd_parse_result_t res = {false};
    if (!**p) {
        snprintf(res.result.error, sizeof(res.result.error), "Unexpected end of input at line %d, column %d", *line, *col);
        return res;
    }
    if (**p == 'n' && strncmp(*p, "null", 4) == 0) {
        *p += 4;
        *col += 4;
        res.success = true;
        res.result.value = cjsd_null();
        return res;
    }
    if (**p == 't' && strncmp(*p, "true", 4) == 0) {
        *p += 4;
        *col += 4;
        res.success = true;
        res.result.value = cjsd_bool(true);
        return res;
    }
    if (**p == 'f' && strncmp(*p, "false", 5) == 0) {
        *p += 5;
        *col += 5;
        res.success = true;
        res.result.value = cjsd_bool(false);
        return res;
    }
    if (**p == '"') {
        char* str;
        if (!parse_string(p, &str, line, col)) {
            snprintf(res.result.error, sizeof(res.result.error), "Invalid string at line %d, column %d", *line, *col);
            return res;
        }
        res.success = true;
        res.result.value = cjsd_string(str);
        free(str);
        return res;
    }
    if (**p == '[') {
        return parse_array(p, line, col);
    }
    if (**p == '{') {
        return parse_object(p, line, col);
    }
    if (isdigit(**p) || **p == '-') {
        const char* start = *p;
        char* end;
        double num = strtod(*p, &end);
        if (end == *p) {
            snprintf(res.result.error, sizeof(res.result.error), "Invalid number at line %d, column %d", *line, *col);
            return res;
        }
        *p = end;
        // Update line and col
        for (const char* c = start; c < end; c++) {
            if (*c == '\n') {
                (*line)++;
                *col = 1;
            } else {
                (*col)++;
            }
        }
        res.success = true;
        res.result.value = cjsd_number(num);
        return res;
    }
    snprintf(res.result.error, sizeof(res.result.error), "Unexpected character '%c' at line %d, column %d", **p, *line, *col);
    return res;
}

cjsd_parse_result_t cjsd_parse(const char* json_str) {
    const char* p = json_str;
    int line = 1, col = 1;
    cjsd_parse_result_t res = parse_value(&p, &line, &col);
    if (res.success) {
        skip_whitespace(&p, &line, &col);
        if (*p) {
            cjsd_free_value(&res.result.value);
            res.success = false;
            snprintf(res.result.error, sizeof(res.result.error), "Extra characters after JSON at line %d, column %d", line, col);
        }
    }
    return res;
}

static void serialize_value(const cjsd_value_t* value, char** buf, size_t* len, size_t* cap) {
    char temp[256];
    size_t tlen;
    switch (value->type) {
        case CJSD_NULL:
            strncpy(temp, "null", sizeof(temp)-1);
            temp[sizeof(temp)-1] = '\0';
            tlen = 4;
            break;
        case CJSD_BOOL:
            if (value->data.boolean) {
                strncpy(temp, "true", sizeof(temp)-1);
                tlen = 4;
            } else {
                strncpy(temp, "false", sizeof(temp)-1);
                tlen = 5;
            }
            temp[sizeof(temp)-1] = '\0';
            break;
        case CJSD_NUMBER:
            snprintf(temp, sizeof(temp), "%.17g", value->data.number);
            tlen = strlen(temp);
            break;
        case CJSD_STRING: {
            size_t slen = strlen(value->data.string);
            size_t needed = slen * 2 + 3; // worst case escaping
            if (*len + needed >= *cap) {
                *cap = (*len + needed) * 2;
                *buf = realloc(*buf, *cap);
            }
            (*buf)[(*len)++] = '"';
            for (size_t i = 0; i < slen; i++) {
                char c = value->data.string[i];
                if (c == '"' || c == '\\') {
                    (*buf)[(*len)++] = '\\';
                } else if (c == '\n') {
                    (*buf)[(*len)++] = '\\';
                    c = 'n';
                } else if (c == '\t') {
                    (*buf)[(*len)++] = '\\';
                    c = 't';
                } else if (c == '\r') {
                    (*buf)[(*len)++] = '\\';
                    c = 'r';
                }
                (*buf)[(*len)++] = c;
            }
            (*buf)[(*len)++] = '"';
            return;
        }
        case CJSD_ARRAY:
            if (*len + 1 >= *cap) {
                *cap *= 2;
                *buf = realloc(*buf, *cap);
            }
            (*buf)[(*len)++] = '[';
            for (size_t i = 0; i < value->data.array.count; i++) {
                if (i > 0) {
                    if (*len + 1 >= *cap) {
                        *cap *= 2;
                        *buf = realloc(*buf, *cap);
                    }
                    (*buf)[(*len)++] = ',';
                }
                serialize_value(&value->data.array.elements[i], buf, len, cap);
            }
            if (*len + 1 >= *cap) {
                *cap *= 2;
                *buf = realloc(*buf, *cap);
            }
            (*buf)[(*len)++] = ']';
            return;
        case CJSD_OBJECT:
            if (*len + 1 >= *cap) {
                *cap *= 2;
                *buf = realloc(*buf, *cap);
            }
            (*buf)[(*len)++] = '{';
            for (size_t i = 0; i < value->data.object.count; i++) {
                if (i > 0) {
                    if (*len + 1 >= *cap) {
                        *cap *= 2;
                        *buf = realloc(*buf, *cap);
                    }
                    (*buf)[(*len)++] = ',';
                }
                // key
                const char* key = value->data.object.keys[i];
                size_t klen = strlen(key);
                size_t needed = klen * 2 + 3;
                if (*len + needed >= *cap) {
                    *cap = (*len + needed) * 2;
                    *buf = realloc(*buf, *cap);
                }
                (*buf)[(*len)++] = '"';
                for (size_t j = 0; j < klen; j++) {
                    char c = key[j];
                    if (c == '"' || c == '\\') {
                        (*buf)[(*len)++] = '\\';
                    }
                    (*buf)[(*len)++] = c;
                }
                (*buf)[(*len)++] = '"';
                (*buf)[(*len)++] = ':';
                serialize_value(&value->data.object.values[i], buf, len, cap);
            }
            if (*len + 1 >= *cap) {
                *cap *= 2;
                *buf = realloc(*buf, *cap);
            }
            (*buf)[(*len)++] = '}';
            return;
    }
    tlen = strlen(temp);
    if (*len + tlen >= *cap) {
        *cap = (*len + tlen) * 2;
        *buf = realloc(*buf, *cap);
    }
    strncpy(*buf + *len, temp, tlen);
    *len += tlen;
}

char* cjsd_serialize(const cjsd_value_t* value) {
    size_t cap = 256;
    char* buf = malloc(cap);
    if (!buf) return NULL;
    size_t len = 0;
    serialize_value(value, &buf, &len, &cap);
    buf[len] = '\0';
    return buf;
}

void cjsd_free_value(cjsd_value_t* value) {
    switch (value->type) {
        case CJSD_STRING:
            free(value->data.string);
            break;
        case CJSD_ARRAY:
            for (size_t i = 0; i < value->data.array.count; i++) {
                cjsd_free_value(&value->data.array.elements[i]);
            }
            free(value->data.array.elements);
            break;
        case CJSD_OBJECT:
            for (size_t i = 0; i < value->data.object.count; i++) {
                free(value->data.object.keys[i]);
                cjsd_free_value(&value->data.object.values[i]);
            }
            free(value->data.object.keys);
            free(value->data.object.values);
            break;
        default:
            break;
    }
    value->type = CJSD_NULL;
}

void cjsd_free_string(char* str) {
    free(str);
}

// Constructors
cjsd_value_t cjsd_null(void) {
    cjsd_value_t v = {CJSD_NULL};
    return v;
}

cjsd_value_t cjsd_bool(bool b) {
    cjsd_value_t v = {CJSD_BOOL, {.boolean = b}};
    return v;
}

cjsd_value_t cjsd_number(double n) {
    cjsd_value_t v = {CJSD_NUMBER, {.number = n}};
    return v;
}

cjsd_value_t cjsd_string(const char* s) {
    cjsd_value_t v = {CJSD_STRING};
    v.data.string = strdup(s);
    return v;
}

cjsd_value_t cjsd_array(void) {
    cjsd_value_t v = {CJSD_ARRAY, {.array = {NULL, 0, 0}}};
    return v;
}

cjsd_value_t cjsd_object(void) {
    cjsd_value_t v = {CJSD_OBJECT, {.object = {NULL, NULL, 0, 0}}};
    return v;
}

// Access functions
cjsd_value_t cjsd_get_object(const cjsd_value_t* obj, const char* key) {
    if (obj->type != CJSD_OBJECT) return cjsd_null();
    for (size_t i = 0; i < obj->data.object.count; i++) {
        if (strcmp(obj->data.object.keys[i], key) == 0) {
            // Return a copy
            cjsd_value_t copy = obj->data.object.values[i];
            if (copy.type == CJSD_STRING) {
                copy.data.string = strdup(copy.data.string);
            } else if (copy.type == CJSD_ARRAY) {
                copy.data.array.elements = malloc(sizeof(cjsd_value_t) * copy.data.array.count);
                for (size_t j = 0; j < copy.data.array.count; j++) {
                    copy.data.array.elements[j] = cjsd_get_array(&copy, j); // recursive copy
                }
            } else if (copy.type == CJSD_OBJECT) {
                // Deep copy needed, but simplified
                copy.data.object.keys = malloc(sizeof(char*) * copy.data.object.count);
                copy.data.object.values = malloc(sizeof(cjsd_value_t) * copy.data.object.count);
                for (size_t j = 0; j < copy.data.object.count; j++) {
                    copy.data.object.keys[j] = strdup(copy.data.object.keys[j]);
                    copy.data.object.values[j] = cjsd_get_object(&copy, copy.data.object.keys[j]); // recursive
                }
            }
            return copy;
        }
    }
    return cjsd_null();
}

cjsd_value_t cjsd_get_array(const cjsd_value_t* arr, size_t index) {
    if (arr->type != CJSD_ARRAY || index >= arr->data.array.count) return cjsd_null();
    cjsd_value_t copy = arr->data.array.elements[index];
    if (copy.type == CJSD_STRING) {
        copy.data.string = strdup(copy.data.string);
    } else if (copy.type == CJSD_ARRAY) {
        // Deep copy
        copy.data.array.elements = malloc(sizeof(cjsd_value_t) * copy.data.array.count);
        copy.data.array.capacity = copy.data.array.count;
        for (size_t i = 0; i < copy.data.array.count; i++) {
            copy.data.array.elements[i] = cjsd_get_array(&copy, i);
        }
    } else if (copy.type == CJSD_OBJECT) {
        // Deep copy
        copy.data.object.keys = malloc(sizeof(char*) * copy.data.object.count);
        copy.data.object.values = malloc(sizeof(cjsd_value_t) * copy.data.object.count);
        copy.data.object.capacity = copy.data.object.count;
        for (size_t i = 0; i < copy.data.object.count; i++) {
            copy.data.object.keys[i] = strdup(copy.data.object.keys[i]);
            copy.data.object.values[i] = cjsd_get_object(&copy, copy.data.object.keys[i]);
        }
    }
    return copy;
}

// Modify functions
cjsd_value_t cjsd_set_object(const cjsd_value_t* obj, const char* key, const cjsd_value_t* val) {
    if (obj->type != CJSD_OBJECT) return cjsd_null();
    cjsd_value_t new_obj = cjsd_object();
    new_obj.data.object.capacity = obj->data.object.count + 1;
    new_obj.data.object.keys = malloc(sizeof(char*) * new_obj.data.object.capacity);
    new_obj.data.object.values = malloc(sizeof(cjsd_value_t) * new_obj.data.object.capacity);
    bool found = false;
    for (size_t i = 0; i < obj->data.object.count; i++) {
        if (strcmp(obj->data.object.keys[i], key) == 0) {
            new_obj.data.object.keys[new_obj.data.object.count] = strdup(key);
            new_obj.data.object.values[new_obj.data.object.count] = *val; // copy
            if (val->type == CJSD_STRING) {
                new_obj.data.object.values[new_obj.data.object.count].data.string = strdup(val->data.string);
            } // deep copy for complex types needed
            found = true;
        } else {
            new_obj.data.object.keys[new_obj.data.object.count] = strdup(obj->data.object.keys[i]);
            new_obj.data.object.values[new_obj.data.object.count] = obj->data.object.values[i]; // shallow copy for now
        }
        new_obj.data.object.count++;
    }
    if (!found) {
        new_obj.data.object.keys[new_obj.data.object.count] = strdup(key);
        new_obj.data.object.values[new_obj.data.object.count] = *val;
        if (val->type == CJSD_STRING) {
            new_obj.data.object.values[new_obj.data.object.count].data.string = strdup(val->data.string);
        }
        new_obj.data.object.count++;
    }
    return new_obj;
}

cjsd_value_t cjsd_set_array(const cjsd_value_t* arr, size_t index, const cjsd_value_t* val) {
    if (arr->type != CJSD_ARRAY || index >= arr->data.array.count) return cjsd_null();
    cjsd_value_t new_arr = cjsd_array();
    new_arr.data.array.capacity = arr->data.array.count;
    new_arr.data.array.elements = malloc(sizeof(cjsd_value_t) * new_arr.data.array.capacity);
    new_arr.data.array.count = arr->data.array.count;
    for (size_t i = 0; i < arr->data.array.count; i++) {
        if (i == index) {
            new_arr.data.array.elements[i] = *val;
            if (val->type == CJSD_STRING) {
                new_arr.data.array.elements[i].data.string = strdup(val->data.string);
            }
        } else {
            new_arr.data.array.elements[i] = arr->data.array.elements[i];
        }
    }
    return new_arr;
}

cjsd_value_t cjsd_push_array(const cjsd_value_t* arr, const cjsd_value_t* val) {
    if (arr->type != CJSD_ARRAY) return cjsd_null();
    cjsd_value_t new_arr = cjsd_array();
    new_arr.data.array.capacity = arr->data.array.count + 1;
    new_arr.data.array.elements = malloc(sizeof(cjsd_value_t) * new_arr.data.array.capacity);
    new_arr.data.array.count = arr->data.array.count + 1;
    for (size_t i = 0; i < arr->data.array.count; i++) {
        new_arr.data.array.elements[i] = arr->data.array.elements[i];
    }
    new_arr.data.array.elements[arr->data.array.count] = *val;
    if (val->type == CJSD_STRING) {
        new_arr.data.array.elements[arr->data.array.count].data.string = strdup(val->data.string);
    }
    return new_arr;
}

#endif // CJSD_IMPLEMENTATION

#endif // CJSD_H
