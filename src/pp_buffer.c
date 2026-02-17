#include "pp_buffer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static pp_status_code pp_buffer_reserve(pp_buffer *buffer, size_t required_cap) {
    size_t new_cap;
    char *new_data;

    if (required_cap <= buffer->cap) {
        return PP_STATUS_OK;
    }

    new_cap = buffer->cap == 0 ? 64u : buffer->cap;
    while (new_cap < required_cap) {
        if (new_cap > (SIZE_MAX / 2u)) {
            new_cap = required_cap;
            break;
        }
        new_cap *= 2u;
    }

    new_data = (char *)realloc(buffer->data, new_cap);
    if (new_data == NULL) {
        return PP_STATUS_ALLOC_FAILED;
    }

    buffer->data = new_data;
    buffer->cap = new_cap;
    return PP_STATUS_OK;
}

void pp_buffer_init(pp_buffer *buffer, size_t max_len) {
    if (buffer == NULL) {
        return;
    }

    buffer->data = NULL;
    buffer->len = 0;
    buffer->cap = 0;
    buffer->max_len = max_len;
}

void pp_buffer_free(pp_buffer *buffer) {
    if (buffer == NULL) {
        return;
    }

    free(buffer->data);
    buffer->data = NULL;
    buffer->len = 0;
    buffer->cap = 0;
}

pp_status_code pp_buffer_append(pp_buffer *buffer, const char *text) {
    if (text == NULL) {
        return PP_STATUS_INVALID_ARGUMENT;
    }

    return pp_buffer_append_n(buffer, text, strlen(text));
}

pp_status_code pp_buffer_append_n(pp_buffer *buffer, const char *text, size_t len) {
    size_t next_len;
    pp_status_code status;

    if (buffer == NULL || text == NULL) {
        return PP_STATUS_INVALID_ARGUMENT;
    }

    if (len > (SIZE_MAX - buffer->len)) {
        return PP_STATUS_OUTPUT_TOO_LARGE;
    }

    next_len = buffer->len + len;
    if (buffer->max_len > 0 && next_len > buffer->max_len) {
        return PP_STATUS_OUTPUT_TOO_LARGE;
    }

    status = pp_buffer_reserve(buffer, next_len + 1u);
    if (status != PP_STATUS_OK) {
        return status;
    }

    if (len > 0) {
        memcpy(buffer->data + buffer->len, text, len);
    }
    buffer->len = next_len;
    buffer->data[buffer->len] = '\0';

    return PP_STATUS_OK;
}

char *pp_buffer_take(pp_buffer *buffer, size_t *out_len) {
    char *owned;

    if (buffer == NULL) {
        return NULL;
    }

    if (buffer->data == NULL) {
        owned = (char *)malloc(1u);
        if (owned == NULL) {
            return NULL;
        }
        owned[0] = '\0';
        if (out_len != NULL) {
            *out_len = 0;
        }
        return owned;
    }

    owned = buffer->data;
    buffer->data = NULL;
    if (out_len != NULL) {
        *out_len = buffer->len;
    }
    buffer->len = 0;
    buffer->cap = 0;
    return owned;
}
