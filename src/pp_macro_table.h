#ifndef PP_MACRO_TABLE_H
#define PP_MACRO_TABLE_H

#include <stddef.h>

#include "preprocessor/preprocessor.h"

typedef struct pp_macro_entry {
    char *name;
    size_t name_len;
    char *replacement;
    size_t replacement_len;
} pp_macro_entry;

typedef struct pp_macro_table {
    pp_macro_entry *entries;
    size_t count;
    size_t cap;
    size_t max_count;
} pp_macro_table;

void pp_macro_table_init(pp_macro_table *table, size_t max_count);
void pp_macro_table_free(pp_macro_table *table);

int pp_macro_table_is_defined(const pp_macro_table *table, const char *name, size_t name_len);
const pp_macro_entry *pp_macro_table_find(const pp_macro_table *table, const char *name, size_t name_len);

pp_status_code pp_macro_table_define(
    pp_macro_table *table,
    const char *name,
    size_t name_len,
    const char *replacement,
    size_t replacement_len
);

void pp_macro_table_undef(pp_macro_table *table, const char *name, size_t name_len);

#endif
