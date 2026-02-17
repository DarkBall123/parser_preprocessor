#include "pp_core_state.h"

#include <string.h>

static int pp_is_space_tab(char ch) {
    return ch == ' ' || ch == '\t';
}

static int pp_is_ident_start(char ch) {
    return (ch >= 'A' && ch <= 'Z') ||
        (ch >= 'a' && ch <= 'z') ||
        ch == '_';
}

static int pp_is_ident_char(char ch) {
    return pp_is_ident_start(ch) || (ch >= '0' && ch <= '9');
}

static size_t pp_skip_space_tab(const char *source, size_t pos, size_t end) {
    while (pos < end && pp_is_space_tab(source[pos])) {
        pos += 1u;
    }
    return pos;
}

static int pp_has_non_space_tab(const char *source, size_t start, size_t end) {
    size_t i;

    for (i = start; i < end; ++i) {
        if (!pp_is_space_tab(source[i])) {
            return 1;
        }
    }

    return 0;
}

static int pp_keyword_eq(const char *source, size_t start, size_t len, const char *kw) {
    size_t kw_len = strlen(kw);

    if (len != kw_len) {
        return 0;
    }

    return memcmp(source + start, kw, kw_len) == 0;
}

static int pp_parse_identifier(
    const char *source,
    size_t start,
    size_t end,
    size_t *out_start,
    size_t *out_len,
    size_t *out_next
) {
    size_t pos;

    if (start >= end || !pp_is_ident_start(source[start])) {
        return 0;
    }

    pos = start + 1u;
    while (pos < end && pp_is_ident_char(source[pos])) {
        pos += 1u;
    }

    *out_start = start;
    *out_len = pos - start;
    *out_next = pos;
    return 1;
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

static pp_status_code pp_parse_single_macro_name(
    const char *source,
    size_t args_start,
    size_t args_end,
    size_t line,
    size_t line_start,
    pp_diagnostic *diag,
    size_t *out_name_start,
    size_t *out_name_len,
    size_t *out_after_name
) {
    size_t name_start;
    size_t name_len;
    size_t next;
    size_t cursor;

    cursor = pp_skip_space_tab(source, args_start, args_end);
    if (!pp_parse_identifier(source, cursor, args_end, &name_start, &name_len, &next)) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line,
            (cursor - line_start) + 1u,
            "expected macro name"
        );
    }

    *out_name_start = name_start;
    *out_name_len = name_len;
    *out_after_name = next;
    return PP_STATUS_OK;
}

static pp_status_code pp_handle_if_directive(
    pp_core_state *state,
    const char *source,
    size_t args_start,
    size_t args_end,
    int negate,
    size_t line,
    size_t line_start,
    pp_diagnostic *diag
) {
    size_t name_start;
    size_t name_len;
    size_t next;
    int condition;
    pp_status_code status;

    status = pp_parse_single_macro_name(
        source,
        args_start,
        args_end,
        line,
        line_start,
        diag,
        &name_start,
        &name_len,
        &next
    );
    if (status != PP_STATUS_OK) {
        return status;
    }

    if (pp_has_non_space_tab(source, next, args_end)) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line,
            (next - line_start) + 1u,
            "unexpected tokens after macro name"
        );
    }

    condition = pp_macro_table_is_defined(&state->macros, source + name_start, name_len);
    if (negate) {
        condition = !condition;
    }

    status = pp_condition_stack_push(&state->condition_stack, state->is_active, condition);
    if (status == PP_STATUS_DEPTH_LIMIT_EXCEEDED) {
        return pp_set_error(
            diag,
            PP_STATUS_DEPTH_LIMIT_EXCEEDED,
            line,
            1u,
            "conditional nesting depth exceeded"
        );
    }
    if (status != PP_STATUS_OK) {
        return pp_set_error(diag, status, line, 1u, "failed to push conditional frame");
    }

    state->is_active = state->is_active && condition;
    return PP_STATUS_OK;
}

static pp_status_code pp_handle_define(
    pp_core_state *state,
    const char *source,
    size_t args_start,
    size_t args_end,
    size_t line,
    size_t line_start,
    pp_diagnostic *diag
) {
    size_t name_start;
    size_t name_len;
    size_t next;
    size_t replacement_start;
    pp_status_code status;

    if (!state->is_active) {
        return PP_STATUS_OK;
    }

    status = pp_parse_single_macro_name(
        source,
        args_start,
        args_end,
        line,
        line_start,
        diag,
        &name_start,
        &name_len,
        &next
    );
    if (status != PP_STATUS_OK) {
        return status;
    }

    if (next < args_end && !pp_is_space_tab(source[next])) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line,
            (next - line_start) + 1u,
            "invalid macro name"
        );
    }

    replacement_start = pp_skip_space_tab(source, next, args_end);

    status = pp_macro_table_define(
        &state->macros,
        source + name_start,
        name_len,
        source + replacement_start,
        args_end - replacement_start
    );

    if (status == PP_STATUS_MACRO_LIMIT_EXCEEDED) {
        return pp_set_error(
            diag,
            PP_STATUS_MACRO_LIMIT_EXCEEDED,
            line,
            1u,
            "macro table limit exceeded"
        );
    }

    if (status != PP_STATUS_OK) {
        return pp_set_error(diag, status, line, 1u, "failed to define macro");
    }

    return PP_STATUS_OK;
}

static pp_status_code pp_handle_undef(
    pp_core_state *state,
    const char *source,
    size_t args_start,
    size_t args_end,
    size_t line,
    size_t line_start,
    pp_diagnostic *diag
) {
    size_t name_start;
    size_t name_len;
    size_t next;
    pp_status_code status;

    if (!state->is_active) {
        return PP_STATUS_OK;
    }

    status = pp_parse_single_macro_name(
        source,
        args_start,
        args_end,
        line,
        line_start,
        diag,
        &name_start,
        &name_len,
        &next
    );
    if (status != PP_STATUS_OK) {
        return status;
    }

    if (pp_has_non_space_tab(source, next, args_end)) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line,
            (next - line_start) + 1u,
            "unexpected tokens after macro name"
        );
    }

    pp_macro_table_undef(&state->macros, source + name_start, name_len);
    return PP_STATUS_OK;
}

static pp_status_code pp_handle_else(
    pp_core_state *state,
    const char *source,
    size_t args_start,
    size_t args_end,
    size_t line,
    size_t line_start,
    pp_diagnostic *diag
) {
    pp_condition_frame *frame;

    if (pp_has_non_space_tab(source, args_start, args_end)) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line,
            (args_start - line_start) + 1u,
            "#else does not accept arguments"
        );
    }

    frame = pp_condition_stack_peek(&state->condition_stack);
    if (frame == NULL) {
        return pp_set_error(
            diag,
            PP_STATUS_CONDITIONAL_MISMATCH,
            line,
            1u,
            "#else without matching #ifdef/#ifndef"
        );
    }

    if (frame->else_seen) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line,
            1u,
            "duplicate #else in the same conditional block"
        );
    }

    frame->else_seen = 1;
    state->is_active = frame->parent_active && !frame->condition_true;
    return PP_STATUS_OK;
}

static pp_status_code pp_handle_endif(
    pp_core_state *state,
    const char *source,
    size_t args_start,
    size_t args_end,
    size_t line,
    size_t line_start,
    pp_diagnostic *diag
) {
    pp_condition_frame frame;
    pp_status_code status;

    if (pp_has_non_space_tab(source, args_start, args_end)) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line,
            (args_start - line_start) + 1u,
            "#endif does not accept arguments"
        );
    }

    status = pp_condition_stack_pop(&state->condition_stack, &frame);
    if (status != PP_STATUS_OK) {
        return pp_set_error(
            diag,
            PP_STATUS_CONDITIONAL_MISMATCH,
            line,
            1u,
            "#endif without matching #ifdef/#ifndef"
        );
    }

    state->is_active = frame.parent_active;
    return PP_STATUS_OK;
}

static pp_status_code pp_process_directive(
    pp_core_state *state,
    const char *source,
    size_t line_start,
    size_t directive_start,
    size_t line_content_end,
    size_t line,
    int *emit_original_line,
    pp_diagnostic *diag
) {
    size_t cursor;
    size_t keyword_start;
    size_t keyword_len;
    size_t args_start;

    *emit_original_line = 0;

    cursor = directive_start + 1u;
    cursor = pp_skip_space_tab(source, cursor, line_content_end);

    keyword_start = cursor;
    while (cursor < line_content_end && pp_is_ident_char(source[cursor])) {
        cursor += 1u;
    }
    keyword_len = cursor - keyword_start;

    args_start = pp_skip_space_tab(source, cursor, line_content_end);

    if (keyword_len == 0u) {
        if (state->is_active && state->config->strict_mode) {
            return pp_set_error(
                diag,
                PP_STATUS_UNKNOWN_DIRECTIVE,
                line,
                (directive_start - line_start) + 1u,
                "unknown preprocessor directive"
            );
        }

        if (state->is_active && !state->config->strict_mode) {
            *emit_original_line = 1;
        }
        return PP_STATUS_OK;
    }

    if (pp_keyword_eq(source, keyword_start, keyword_len, "define")) {
        return pp_handle_define(state, source, args_start, line_content_end, line, line_start, diag);
    }

    if (pp_keyword_eq(source, keyword_start, keyword_len, "undef")) {
        return pp_handle_undef(state, source, args_start, line_content_end, line, line_start, diag);
    }

    if (pp_keyword_eq(source, keyword_start, keyword_len, "ifdef")) {
        return pp_handle_if_directive(state, source, args_start, line_content_end, 0, line, line_start, diag);
    }

    if (pp_keyword_eq(source, keyword_start, keyword_len, "ifndef")) {
        return pp_handle_if_directive(state, source, args_start, line_content_end, 1, line, line_start, diag);
    }

    if (pp_keyword_eq(source, keyword_start, keyword_len, "else")) {
        return pp_handle_else(state, source, args_start, line_content_end, line, line_start, diag);
    }

    if (pp_keyword_eq(source, keyword_start, keyword_len, "endif")) {
        return pp_handle_endif(state, source, args_start, line_content_end, line, line_start, diag);
    }

    if (pp_keyword_eq(source, keyword_start, keyword_len, "include")) {
        if (!state->is_active) {
            return PP_STATUS_OK;
        }

        if (!state->config->enable_include) {
            return pp_set_error(
                diag,
                PP_STATUS_INCLUDE_DISABLED,
                line,
                (directive_start - line_start) + 1u,
                "#include is disabled by config"
            );
        }

        return pp_set_error(
            diag,
            PP_STATUS_INTERNAL_ERROR,
            line,
            (directive_start - line_start) + 1u,
            "#include support is not implemented yet"
        );
    }

    if (state->is_active && state->config->strict_mode) {
        return pp_set_error(
            diag,
            PP_STATUS_UNKNOWN_DIRECTIVE,
            line,
            (directive_start - line_start) + 1u,
            "unknown preprocessor directive"
        );
    }

    if (state->is_active && !state->config->strict_mode) {
        *emit_original_line = 1;
    }

    return PP_STATUS_OK;
}

void pp_core_state_init(
    pp_core_state *state,
    const pp_config *config,
    pp_include_resolver_fn include_resolver,
    void *include_user_ctx
) {
    if (state == NULL) {
        return;
    }

    state->config = config;
    state->include_resolver = include_resolver;
    state->include_user_ctx = include_user_ctx;
    pp_macro_table_init(&state->macros, config->max_macro_count);
    pp_condition_stack_init(&state->condition_stack, config->max_conditional_depth);
    pp_expand_state_init(&state->expand_state);
    state->is_active = 1;
}

void pp_core_state_dispose(pp_core_state *state) {
    if (state == NULL) {
        return;
    }

    pp_macro_table_free(&state->macros);
    pp_condition_stack_free(&state->condition_stack);
    state->is_active = 1;
}

pp_status_code pp_core_process(
    pp_core_state *state,
    const char *source,
    size_t source_len,
    pp_buffer *out,
    pp_diagnostic *diag
) {
    size_t pos;
    size_t line;

    if (state == NULL || source == NULL || out == NULL || diag == NULL) {
        return PP_STATUS_INVALID_ARGUMENT;
    }

    pos = 0u;
    line = 1u;

    while (pos < source_len) {
        size_t line_start = pos;
        size_t line_end;
        size_t content_end;
        size_t line_out_end;
        size_t cursor;
        pp_status_code status;

        while (pos < source_len && source[pos] != '\n') {
            pos += 1u;
        }

        line_end = pos;
        content_end = line_end;
        if (content_end > line_start && source[content_end - 1u] == '\r') {
            content_end -= 1u;
        }

        if (pos < source_len && source[pos] == '\n') {
            pos += 1u;
        }
        line_out_end = pos;

        if (state->is_active && state->expand_state.in_block_comment) {
            status = pp_expand_active_line(
                &state->macros,
                state->config,
                &state->expand_state,
                source,
                line_start,
                content_end,
                line_out_end,
                line,
                out,
                diag
            );
            if (status != PP_STATUS_OK) {
                return status;
            }
        } else {
            cursor = pp_skip_space_tab(source, line_start, content_end);
            if (cursor < content_end && source[cursor] == '#') {
                int emit_original_line = 0;
                status = pp_process_directive(
                    state,
                    source,
                    line_start,
                    cursor,
                    content_end,
                    line,
                    &emit_original_line,
                    diag
                );
                if (status != PP_STATUS_OK) {
                    return status;
                }

                if (emit_original_line && state->is_active) {
                    status = pp_buffer_append_n(out, source + line_start, line_out_end - line_start);
                    if (status != PP_STATUS_OK) {
                        return status;
                    }
                }
            } else if (state->is_active) {
                status = pp_expand_active_line(
                    &state->macros,
                    state->config,
                    &state->expand_state,
                    source,
                    line_start,
                    content_end,
                    line_out_end,
                    line,
                    out,
                    diag
                );
                if (status != PP_STATUS_OK) {
                    return status;
                }
            }
        }

        line += 1u;
    }

    if (state->expand_state.in_block_comment && state->config->strict_mode) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line,
            1u,
            "unclosed block comment"
        );
    }

    if (state->condition_stack.depth != 0u) {
        return pp_set_error(
            diag,
            PP_STATUS_CONDITIONAL_UNCLOSED,
            line,
            1u,
            "unclosed conditional block"
        );
    }

    return PP_STATUS_OK;
}
