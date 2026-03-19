# CJSD - C JSON Serialized/Deserializer

> [!WARNING]
> **This entire library has been generated using AI and may not work as expected.** Use at your own risk. It is recommended to thoroughly test and review the code before using it in production.

CJSD is a single-header library for parsing and serializing JSON documents in C99. It follows the stb-style pattern, requiring only a single include and a define to use the implementation.

## Features

- **Parsing**: Convert JSON strings to in-memory data structures
- **Serialization**: Convert data structures back to JSON strings
- **Functional API**: Immutable operations that return new values
- **Memory Management**: Explicit allocation/deallocation functions
- **C99 Compatible**: No external dependencies, pure C99 standard

## How It Works

CJSD represents JSON values using a union-based struct (`cjsd_value_t`) that can hold null, boolean, number, string, array, or object types. Parsing builds a tree of these structures from a JSON string, while serialization traverses the tree to generate a JSON string.

The library uses a functional approach for modifications - instead of mutating existing values, functions return new copies with the requested changes.

## Quick Start

```c
#define CJSD_IMPLEMENTATION
#include "cjsd.h"

int main() {
    // Parse JSON
    cjsd_parse_result_t res = cjsd_parse("{\"key\": \"value\"}");
    if (res.success) {
        // Use the parsed value
        char* json = cjsd_serialize(&res.result.value);
        printf("Serialized: %s\n", json);
        cjsd_free_string(json);
        cjsd_free_value(&res.result.value);
    } else {
        printf("Parse error: %s\n", res.result.error);
        free(res.result.error);
    }
    return 0;
}
```

## Data Types

- `cjsd_type_t`: Enum for JSON value types (NULL, BOOL, NUMBER, STRING, ARRAY, OBJECT)
- `cjsd_value_t`: Union struct representing any JSON value
- `cjsd_parse_result_t`: Result of parsing, containing either a value or error message

## API Reference

### Parsing

```c
cjsd_parse_result_t cjsd_parse(const char* json_str);
```

Parses a JSON string and returns a result. Check `result.success` to see if parsing succeeded.

### Serialization

```c
char* cjsd_serialize(const cjsd_value_t* value);
```

Converts a JSON value to a string. Returns a heap-allocated string that must be freed with `cjsd_free_string()`.

### Memory Management

```c
void cjsd_free_value(cjsd_value_t* value);
void cjsd_free_string(char* str);
```

Free memory allocated by CJSD functions.

### Constructors

```c
cjsd_value_t cjsd_null(void);
cjsd_value_t cjsd_bool(bool b);
cjsd_value_t cjsd_number(double n);
cjsd_value_t cjsd_string(const char* s);
cjsd_value_t cjsd_array(void);
cjsd_value_t cjsd_object(void);
```

Create new JSON values of various types.

### Access Functions

```c
cjsd_value_t cjsd_get_object(const cjsd_value_t* obj, const char* key);
cjsd_value_t cjsd_get_array(const cjsd_value_t* arr, size_t index);
```

Retrieve values from objects and arrays. Returns a copy of the value (functional approach).

### Modify Functions

```c
cjsd_value_t cjsd_set_object(const cjsd_value_t* obj, const char* key, const cjsd_value_t* val);
cjsd_value_t cjsd_set_array(const cjsd_value_t* arr, size_t index, const cjsd_value_t* val);
cjsd_value_t cjsd_push_array(const cjsd_value_t* arr, const cjsd_value_t* val);
```

Create modified copies of objects and arrays (functional approach).

## Examples

### Basic Parsing and Serialization

```c
cjsd_parse_result_t res = cjsd_parse("[1, \"hello\", true, null]");
if (res.success) {
    char* json = cjsd_serialize(&res.result.value);
    printf("Round-trip: %s\n", json); // Outputs: [1,"hello",true,null]
    cjsd_free_string(json);
    cjsd_free_value(&res.result.value);
}
```

### Building JSON Programmatically

```c
cjsd_value_t obj = cjsd_object();
cjsd_value_t name = cjsd_string("Alice");
obj = cjsd_set_object(&obj, "name", &name);
cjsd_free_value(&name);

cjsd_value_t age = cjsd_number(30);
obj = cjsd_set_object(&obj, "age", &age);
cjsd_free_value(&age);

char* json = cjsd_serialize(&obj);
printf("Object: %s\n", json); // Outputs: {"name":"Alice","age":30}
cjsd_free_string(json);
cjsd_free_value(&obj);
```

### Accessing Nested Values

```c
cjsd_parse_result_t res = cjsd_parse("{\"users\": [{\"name\": \"Bob\"}]}");
if (res.success) {
    cjsd_value_t users = cjsd_get_object(&res.result.value, "users");
    cjsd_value_t first_user = cjsd_get_array(&users, 0);
    cjsd_value_t name = cjsd_get_object(&first_user, "name");

    if (name.type == CJSD_STRING) {
        printf("First user name: %s\n", name.data.string);
    }

    cjsd_free_value(&name);
    cjsd_free_value(&first_user);
    cjsd_free_value(&users);
    cjsd_free_value(&res.result.value);
}
```

### Modifying Arrays

```c
cjsd_value_t arr = cjsd_array();
cjsd_value_t item1 = cjsd_number(1);
arr = cjsd_push_array(&arr, &item1);
cjsd_free_value(&item1);

cjsd_value_t item2 = cjsd_number(2);
cjsd_value_t new_arr = cjsd_set_array(&arr, 0, &item2); // Replace first element
cjsd_free_value(&item2);

char* json = cjsd_serialize(&new_arr);
printf("Modified array: %s\n", json); // Outputs: [2]
cjsd_free_string(json);
cjsd_free_value(&new_arr);
cjsd_free_value(&arr);
```

## Error Handling

Parsing can fail due to malformed JSON. Always check the result:

```c
cjsd_parse_result_t res = cjsd_parse("invalid json");
if (!res.success) {
    printf("Error: %s\n", res.result.error);
} else {
    // Use res.result.value
    cjsd_free_value(&res.result.value);
}
```

## Notes

- Serialized strings are heap-allocated and must be freed with `cjsd_free_string()`
- The functional approach means frequent copying - not optimized for performance
- Supports standard JSON types but not comments or trailing commas
- Thread-safe as long as you don't share values between threads