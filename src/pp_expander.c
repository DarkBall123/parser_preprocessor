#include "pp_expander.h"

#include "pp_text.h"

static pp_status_code pp_set_error(
    pp_diagnostic *diag,
    pp_status_code code,
    size_t line,
    size_t column,
    const char *message
) {
    if (pp_diag_set(diag, code, line, column, message) == PP_STATUS_ALLOC_FAILED) {
        return PP_STATUS_ALLOC_FAILED;
    }

    return code;
}

static pp_status_code pp_append_range(
    pp_buffer *out,
    const char *source,
    size_t start,
    size_t end
) {
    return pp_buffer_append_n(out, source + start, end - start);
}

static size_t pp_find_string_end(const char *source, size_t start, size_t end) {
    size_t pos = start + 1u;

    while (pos < end) {
        if (source[pos] == '\\' && pos + 1u < end) {
            pos += 2u;
            continue;
        }

        if (source[pos] == '"') {
            pos += 1u;
            break;
        }

        pos += 1u;
    }

    return pos;
}

static size_t pp_find_identifier_end(const char *source, size_t start, size_t end) {
    size_t pos = start + 1u;

    while (pos < end && pp_is_ident_char(source[pos])) {
        pos += 1u;
    }

    return pos;
}

static pp_status_code pp_copy_in_block_comment(
    pp_expand_state *state,
    const char *source,
    size_t content_end,
    pp_buffer *out,
    size_t *pos
) {
    size_t close = *pos;
    int found_close = 0;

    while (close + 1u < content_end) {
        if (source[close] == '*' && source[close + 1u] == '/') {
            close += 2u;
            found_close = 1;
            break;
        }
        close += 1u;
    }

    if (!found_close) {
        pp_status_code status = pp_append_range(out, source, *pos, content_end);
        if (status != PP_STATUS_OK) {
            return status;
        }

        *pos = content_end;
        return PP_STATUS_OK;
    }

    {
        pp_status_code status = pp_append_range(out, source, *pos, close);
        if (status != PP_STATUS_OK) {
            return status;
        }
    }

    *pos = close;
    state->in_block_comment = 0;
    return PP_STATUS_OK;
}

void pp_expand_state_init(pp_expand_state *state) {
    if (state == NULL) {
        return;
    }

    state->in_block_comment = 0;
}

pp_status_code pp_expand_active_line(
    const pp_macro_table *macros,
    const pp_config *config,
    pp_expand_state *state,
    const char *source,
    size_t line_start,
    size_t content_end,
    size_t line_out_end,
    size_t line,
    pp_buffer *out,
    pp_diagnostic *diag
) {
    size_t pos;

    if (macros == NULL || config == NULL || state == NULL || source == NULL || out == NULL || diag == NULL) {
        return PP_STATUS_INVALID_ARGUMENT;
    }

    pos = line_start;

    while (pos < content_end) {
        pp_status_code status;

        if (state->in_block_comment) {
            status = pp_copy_in_block_comment(state, source, content_end, out, &pos);
            if (status != PP_STATUS_OK) {
                return status;
            }
            continue;
        }

        if (source[pos] == '"') {
            size_t string_end = pp_find_string_end(source, pos, content_end);
            status = pp_append_range(out, source, pos, string_end);
            if (status != PP_STATUS_OK) {
                return status;
            }

            pos = string_end;
            continue;
        }

        if (source[pos] == '/' && pos + 1u < content_end && source[pos + 1u] == '/') {
            status = pp_append_range(out, source, pos, content_end);
            if (status != PP_STATUS_OK) {
                return status;
            }

            pos = content_end;
            continue;
        }

        if (source[pos] == '/' && pos + 1u < content_end && source[pos + 1u] == '*') {
            if (config->support_block_comments) {
                status = pp_append_range(out, source, pos, pos + 2u);
                if (status != PP_STATUS_OK) {
                    return status;
                }

                pos += 2u;
                state->in_block_comment = 1;
                continue;
            }

            if (config->strict_mode) {
                return pp_set_error(
                    diag,
                    PP_STATUS_SYNTAX_ERROR,
                    line,
                    (pos - line_start) + 1u,
                    "block comments are disabled by config"
                );
            }
        }

        if (pp_is_ident_start(source[pos])) {
            size_t token_end = pp_find_identifier_end(source, pos, content_end);
            const pp_macro_entry *entry = pp_macro_table_find(macros, source + pos, token_end - pos);

            if (entry != NULL) {
                status = pp_buffer_append_n(out, entry->replacement, entry->replacement_len);
            } else {
                status = pp_append_range(out, source, pos, token_end);
            }

            if (status != PP_STATUS_OK) {
                return status;
            }

            pos = token_end;
            continue;
        }

        status = pp_buffer_append_n(out, source + pos, 1u);
        if (status != PP_STATUS_OK) {
            return status;
        }

        pos += 1u;
    }

    if (line_out_end > content_end) {
        return pp_append_range(out, source, content_end, line_out_end);
    }

    return PP_STATUS_OK;
}
