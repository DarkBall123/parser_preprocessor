#ifndef PP_BUFFER_H
#define PP_BUFFER_H

#include <stddef.h>

#include "preprocessor/preprocessor.h"

typedef struct pp_buffer {
    char *data;
    size_t len;
    size_t cap;
    size_t max_len;
} pp_buffer;

void pp_buffer_init(pp_buffer *buffer, size_t max_len);
void pp_buffer_free(pp_buffer *buffer);

pp_status_code pp_buffer_append(pp_buffer *buffer, const char *text);
pp_status_code pp_buffer_append_n(pp_buffer *buffer, const char *text, size_t len);

char *pp_buffer_take(pp_buffer *buffer, size_t *out_len);

#endif
