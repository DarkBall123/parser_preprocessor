#include "pp_expander.h"

#include <string.h>

static int pp_is_ident_start(char ch) {
    return (ch >= 'A' && ch <= 'Z') ||
        (ch >= 'a' && ch <= 'z') ||
        ch == '_';
}

static int pp_is_ident_char(char ch) {
    return pp_is_ident_start(ch) || (ch >= '0' && ch <= '9');
}

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
            size_t close_pos = pos;
            int found = 0;

            while (close_pos + 1u < content_end) {
                if (source[close_pos] == '*' && source[close_pos + 1u] == '/') {
                    found = 1;
                    close_pos += 2u;
                    break;
                }
                close_pos += 1u;
            }

            if (!found) {
                status = pp_buffer_append_n(out, source + pos, content_end - pos);
                if (status != PP_STATUS_OK) {
                    return status;
                }
                pos = content_end;
            } else {
                status = pp_buffer_append_n(out, source + pos, close_pos - pos);
                if (status != PP_STATUS_OK) {
                    return status;
                }
                pos = close_pos;
                state->in_block_comment = 0;
            }
            continue;
        }

        if (source[pos] == '"') {
            size_t end = pos + 1u;

            while (end < content_end) {
                if (source[end] == '\\' && end + 1u < content_end) {
                    end += 2u;
                    continue;
                }

                if (source[end] == '"') {
                    end += 1u;
                    break;
                }

                end += 1u;
            }

            status = pp_buffer_append_n(out, source + pos, end - pos);
            if (status != PP_STATUS_OK) {
                return status;
            }
            pos = end;
            continue;
        }

        if (source[pos] == '/' && pos + 1u < content_end && source[pos + 1u] == '/') {
            status = pp_buffer_append_n(out, source + pos, content_end - pos);
            if (status != PP_STATUS_OK) {
                return status;
            }
            pos = content_end;
            continue;
        }

        if (source[pos] == '/' && pos + 1u < content_end && source[pos + 1u] == '*' && config->support_block_comments) {
            status = pp_buffer_append_n(out, source + pos, 2u);
            if (status != PP_STATUS_OK) {
                return status;
            }
            pos += 2u;
            state->in_block_comment = 1;
            continue;
        }

        if (source[pos] == '/' && pos + 1u < content_end && source[pos + 1u] == '*' && !config->support_block_comments) {
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
            size_t token_end = pos + 1u;
            const pp_macro_entry *entry;

            while (token_end < content_end && pp_is_ident_char(source[token_end])) {
                token_end += 1u;
            }

            entry = pp_macro_table_find(macros, source + pos, token_end - pos);
            if (entry != NULL) {
                status = pp_buffer_append_n(out, entry->replacement, entry->replacement_len);
                if (status != PP_STATUS_OK) {
                    return status;
                }
            } else {
                status = pp_buffer_append_n(out, source + pos, token_end - pos);
                if (status != PP_STATUS_OK) {
                    return status;
                }
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
        return pp_buffer_append_n(out, source + content_end, line_out_end - content_end);
    }

    return PP_STATUS_OK;
}
