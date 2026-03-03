#ifndef PP_TEXT_H
#define PP_TEXT_H

#include <stddef.h>
#include <string.h>

static inline int pp_is_space_tab(char ch) {
    return ch == ' ' || ch == '\t';
}

static inline int pp_is_ident_start(char ch) {
    return (ch >= 'A' && ch <= 'Z') ||
        (ch >= 'a' && ch <= 'z') ||
        ch == '_';
}

static inline int pp_is_ident_char(char ch) {
    return pp_is_ident_start(ch) || (ch >= '0' && ch <= '9');
}

static inline size_t pp_skip_space_tab(const char *source, size_t start, size_t end) {
    size_t pos = start;

    while (pos < end && pp_is_space_tab(source[pos])) {
        pos += 1u;
    }

    return pos;
}

static inline int pp_has_non_space_tab(const char *source, size_t start, size_t end) {
    size_t pos;

    for (pos = start; pos < end; ++pos) {
        if (!pp_is_space_tab(source[pos])) {
            return 1;
        }
    }

    return 0;
}

static inline int pp_keyword_eq_range(
    const char *source,
    size_t start,
    size_t len,
    const char *keyword
) {
    size_t keyword_len = strlen(keyword);

    if (len != keyword_len) {
        return 0;
    }

    return memcmp(source + start, keyword, keyword_len) == 0;
}

#endif
