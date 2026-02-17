#include "pp_macro_table.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int pp_macro_entry_name_eq(const pp_macro_entry *entry, const char *name, size_t name_len) {
    if (entry->name_len != name_len) {
        return 0;
    }
    return memcmp(entry->name, name, name_len) == 0;
}

static ptrdiff_t pp_macro_find_index(const pp_macro_table *table, const char *name, size_t name_len) {
    size_t i;

    for (i = 0; i < table->count; ++i) {
        if (pp_macro_entry_name_eq(&table->entries[i], name, name_len)) {
            return (ptrdiff_t)i;
        }
    }

    return -1;
}

static char *pp_dup_range(const char *data, size_t len) {
    char *copy;

    copy = (char *)malloc(len + 1u);
    if (copy == NULL) {
        return NULL;
    }

    if (len > 0u) {
        memcpy(copy, data, len);
    }
    copy[len] = '\0';
    return copy;
}

static pp_status_code pp_macro_table_reserve(pp_macro_table *table, size_t required) {
    size_t new_cap;
    pp_macro_entry *new_entries;

    if (required <= table->cap) {
        return PP_STATUS_OK;
    }

    new_cap = table->cap == 0u ? 16u : table->cap;
    while (new_cap < required) {
        if (new_cap > (SIZE_MAX / 2u)) {
            new_cap = required;
            break;
        }
        new_cap *= 2u;
    }

    new_entries = (pp_macro_entry *)realloc(table->entries, new_cap * sizeof(pp_macro_entry));
    if (new_entries == NULL) {
        return PP_STATUS_ALLOC_FAILED;
    }

    table->entries = new_entries;
    table->cap = new_cap;
    return PP_STATUS_OK;
}

void pp_macro_table_init(pp_macro_table *table, size_t max_count) {
    if (table == NULL) {
        return;
    }

    table->entries = NULL;
    table->count = 0u;
    table->cap = 0u;
    table->max_count = max_count;
}

void pp_macro_table_free(pp_macro_table *table) {
    size_t i;

    if (table == NULL) {
        return;
    }

    for (i = 0u; i < table->count; ++i) {
        free(table->entries[i].name);
        free(table->entries[i].replacement);
    }

    free(table->entries);
    table->entries = NULL;
    table->count = 0u;
    table->cap = 0u;
}

int pp_macro_table_is_defined(const pp_macro_table *table, const char *name, size_t name_len) {
    if (table == NULL || name == NULL) {
        return 0;
    }

    return pp_macro_find_index(table, name, name_len) >= 0;
}

const pp_macro_entry *pp_macro_table_find(const pp_macro_table *table, const char *name, size_t name_len) {
    ptrdiff_t idx;

    if (table == NULL || name == NULL) {
        return NULL;
    }

    idx = pp_macro_find_index(table, name, name_len);
    if (idx < 0) {
        return NULL;
    }

    return &table->entries[idx];
}

pp_status_code pp_macro_table_define(
    pp_macro_table *table,
    const char *name,
    size_t name_len,
    const char *replacement,
    size_t replacement_len
) {
    ptrdiff_t idx;
    char *name_copy;
    char *replacement_copy;

    if (table == NULL || name == NULL || replacement == NULL || name_len == 0u) {
        return PP_STATUS_INVALID_ARGUMENT;
    }

    idx = pp_macro_find_index(table, name, name_len);
    if (idx >= 0) {
        replacement_copy = pp_dup_range(replacement, replacement_len);
        if (replacement_copy == NULL) {
            return PP_STATUS_ALLOC_FAILED;
        }

        free(table->entries[idx].replacement);
        table->entries[idx].replacement = replacement_copy;
        table->entries[idx].replacement_len = replacement_len;
        return PP_STATUS_OK;
    }

    if (table->max_count > 0u && table->count >= table->max_count) {
        return PP_STATUS_MACRO_LIMIT_EXCEEDED;
    }

    if (pp_macro_table_reserve(table, table->count + 1u) != PP_STATUS_OK) {
        return PP_STATUS_ALLOC_FAILED;
    }

    name_copy = pp_dup_range(name, name_len);
    if (name_copy == NULL) {
        return PP_STATUS_ALLOC_FAILED;
    }

    replacement_copy = pp_dup_range(replacement, replacement_len);
    if (replacement_copy == NULL) {
        free(name_copy);
        return PP_STATUS_ALLOC_FAILED;
    }

    table->entries[table->count].name = name_copy;
    table->entries[table->count].name_len = name_len;
    table->entries[table->count].replacement = replacement_copy;
    table->entries[table->count].replacement_len = replacement_len;
    table->count += 1u;

    return PP_STATUS_OK;
}

void pp_macro_table_undef(pp_macro_table *table, const char *name, size_t name_len) {
    ptrdiff_t idx;
    size_t i;

    if (table == NULL || name == NULL || name_len == 0u) {
        return;
    }

    idx = pp_macro_find_index(table, name, name_len);
    if (idx < 0) {
        return;
    }

    free(table->entries[idx].name);
    free(table->entries[idx].replacement);

    for (i = (size_t)idx; i + 1u < table->count; ++i) {
        table->entries[i] = table->entries[i + 1u];
    }

    table->count -= 1u;
}
