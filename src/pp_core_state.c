#include "pp_core_state.h"

#include "pp_text.h"

typedef struct pp_line_span {
    size_t start;
    size_t content_end;
    size_t out_end;
    size_t number;
} pp_line_span;

typedef struct pp_directive_span {
    size_t hash_pos;
    size_t keyword_start;
    size_t keyword_len;
    size_t args_start;
    size_t args_end;
} pp_directive_span;

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

static int pp_parse_identifier_token(
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

static int pp_next_line(
    const char *source,
    size_t source_len,
    size_t *cursor,
    size_t line_number,
    pp_line_span *line
) {
    size_t line_end;

    if (*cursor >= source_len) {
        return 0;
    }

    line->start = *cursor;

    while (*cursor < source_len && source[*cursor] != '\n') {
        *cursor += 1u;
    }

    line_end = *cursor;
    line->content_end = line_end;
    if (line->content_end > line->start && source[line->content_end - 1u] == '\r') {
        line->content_end -= 1u;
    }

    if (*cursor < source_len && source[*cursor] == '\n') {
        *cursor += 1u;
    }

    line->out_end = *cursor;
    line->number = line_number;
    return 1;
}

static size_t pp_find_directive_args_end(
    const char *source,
    size_t args_start,
    size_t line_end
) {
    size_t pos = args_start;
    int in_string = 0;

    while (pos < line_end) {
        char ch = source[pos];

        if (in_string) {
            if (ch == '\\' && pos + 1u < line_end) {
                pos += 2u;
                continue;
            }

            if (ch == '"') {
                in_string = 0;
            }

            pos += 1u;
            continue;
        }

        if (ch == '"') {
            in_string = 1;
            pos += 1u;
            continue;
        }

        if (ch == '/' && pos + 1u < line_end) {
            if (source[pos + 1u] == '/') {
                return pos;
            }

            if (source[pos + 1u] == '*') {
                size_t close = pos + 2u;
                int found_close = 0;

                while (close + 1u < line_end) {
                    if (source[close] == '*' && source[close + 1u] == '/') {
                        close += 2u;
                        found_close = 1;
                        break;
                    }
                    close += 1u;
                }

                if (!found_close) {
                    return pos;
                }

                if (!pp_has_non_space_tab(source, close, line_end)) {
                    return pos;
                }

                pos = close;
                continue;
            }
        }

        pos += 1u;
    }

    return line_end;
}

static void pp_parse_directive(
    const char *source,
    const pp_line_span *line,
    size_t hash_pos,
    pp_directive_span *out
) {
    size_t cursor = pp_skip_space_tab(source, hash_pos + 1u, line->content_end);

    out->hash_pos = hash_pos;
    out->keyword_start = cursor;

    while (cursor < line->content_end && pp_is_ident_char(source[cursor])) {
        cursor += 1u;
    }

    out->keyword_len = cursor - out->keyword_start;
    out->args_start = pp_skip_space_tab(source, cursor, line->content_end);
    out->args_end = pp_find_directive_args_end(source, out->args_start, line->content_end);
}

static int pp_directive_is(
    const char *source,
    const pp_directive_span *directive,
    const char *keyword
) {
    return pp_keyword_eq_range(source, directive->keyword_start, directive->keyword_len, keyword);
}

static pp_status_code pp_parse_macro_name(
    const char *source,
    const pp_line_span *line,
    const pp_directive_span *directive,
    pp_diagnostic *diag,
    size_t *out_name_start,
    size_t *out_name_len,
    size_t *out_after_name
) {
    size_t cursor = pp_skip_space_tab(source, directive->args_start, directive->args_end);

    if (!pp_parse_identifier_token(
            source,
            cursor,
            directive->args_end,
            out_name_start,
            out_name_len,
            out_after_name
        )) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line->number,
            (cursor - line->start) + 1u,
            "expected macro name"
        );
    }

    return PP_STATUS_OK;
}

static pp_status_code pp_require_no_extra_tokens(
    const char *source,
    size_t start,
    size_t end,
    const pp_line_span *line,
    pp_diagnostic *diag,
    const char *message
) {
    size_t token_start = pp_skip_space_tab(source, start, end);

    if (token_start < end) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line->number,
            (token_start - line->start) + 1u,
            message
        );
    }

    return PP_STATUS_OK;
}

static pp_status_code pp_handle_ifdef_like(
    pp_core_state *state,
    const char *source,
    const pp_line_span *line,
    const pp_directive_span *directive,
    int negate,
    pp_diagnostic *diag
) {
    size_t name_start;
    size_t name_len;
    size_t after_name;
    int condition;
    pp_status_code status;

    status = pp_parse_macro_name(
        source,
        line,
        directive,
        diag,
        &name_start,
        &name_len,
        &after_name
    );
    if (status != PP_STATUS_OK) {
        return status;
    }

    status = pp_require_no_extra_tokens(
        source,
        after_name,
        directive->args_end,
        line,
        diag,
        "unexpected tokens after macro name"
    );
    if (status != PP_STATUS_OK) {
        return status;
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
            line->number,
            1u,
            "conditional nesting depth exceeded"
        );
    }

    if (status != PP_STATUS_OK) {
        return pp_set_error(diag, status, line->number, 1u, "failed to push conditional frame");
    }

    state->is_active = state->is_active && condition;
    return PP_STATUS_OK;
}

static pp_status_code pp_handle_define(
    pp_core_state *state,
    const char *source,
    const pp_line_span *line,
    const pp_directive_span *directive,
    pp_diagnostic *diag
) {
    size_t name_start;
    size_t name_len;
    size_t after_name;
    size_t replacement_start;
    pp_status_code status;

    if (!state->is_active) {
        return PP_STATUS_OK;
    }

    status = pp_parse_macro_name(
        source,
        line,
        directive,
        diag,
        &name_start,
        &name_len,
        &after_name
    );
    if (status != PP_STATUS_OK) {
        return status;
    }

    if (after_name < directive->args_end && !pp_is_space_tab(source[after_name])) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line->number,
            (after_name - line->start) + 1u,
            "invalid macro name"
        );
    }

    replacement_start = pp_skip_space_tab(source, after_name, directive->args_end);

    status = pp_macro_table_define(
        &state->macros,
        source + name_start,
        name_len,
        source + replacement_start,
        directive->args_end - replacement_start
    );

    if (status == PP_STATUS_MACRO_LIMIT_EXCEEDED) {
        return pp_set_error(
            diag,
            PP_STATUS_MACRO_LIMIT_EXCEEDED,
            line->number,
            1u,
            "macro table limit exceeded"
        );
    }

    if (status != PP_STATUS_OK) {
        return pp_set_error(diag, status, line->number, 1u, "failed to define macro");
    }

    return PP_STATUS_OK;
}

static pp_status_code pp_handle_undef(
    pp_core_state *state,
    const char *source,
    const pp_line_span *line,
    const pp_directive_span *directive,
    pp_diagnostic *diag
) {
    size_t name_start;
    size_t name_len;
    size_t after_name;
    pp_status_code status;

    if (!state->is_active) {
        return PP_STATUS_OK;
    }

    status = pp_parse_macro_name(
        source,
        line,
        directive,
        diag,
        &name_start,
        &name_len,
        &after_name
    );
    if (status != PP_STATUS_OK) {
        return status;
    }

    status = pp_require_no_extra_tokens(
        source,
        after_name,
        directive->args_end,
        line,
        diag,
        "unexpected tokens after macro name"
    );
    if (status != PP_STATUS_OK) {
        return status;
    }

    pp_macro_table_undef(&state->macros, source + name_start, name_len);
    return PP_STATUS_OK;
}

static pp_status_code pp_handle_else(
    pp_core_state *state,
    const char *source,
    const pp_line_span *line,
    const pp_directive_span *directive,
    pp_diagnostic *diag
) {
    pp_condition_frame *frame;
    pp_status_code status;

    status = pp_require_no_extra_tokens(
        source,
        directive->args_start,
        directive->args_end,
        line,
        diag,
        "#else does not accept arguments"
    );
    if (status != PP_STATUS_OK) {
        return status;
    }

    frame = pp_condition_stack_peek(&state->condition_stack);
    if (frame == NULL) {
        return pp_set_error(
            diag,
            PP_STATUS_CONDITIONAL_MISMATCH,
            line->number,
            1u,
            "#else without matching #ifdef/#ifndef"
        );
    }

    if (frame->else_seen) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line->number,
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
    const pp_line_span *line,
    const pp_directive_span *directive,
    pp_diagnostic *diag
) {
    pp_condition_frame frame;
    pp_status_code status;

    status = pp_require_no_extra_tokens(
        source,
        directive->args_start,
        directive->args_end,
        line,
        diag,
        "#endif does not accept arguments"
    );
    if (status != PP_STATUS_OK) {
        return status;
    }

    status = pp_condition_stack_pop(&state->condition_stack, &frame);
    if (status != PP_STATUS_OK) {
        return pp_set_error(
            diag,
            PP_STATUS_CONDITIONAL_MISMATCH,
            line->number,
            1u,
            "#endif without matching #ifdef/#ifndef"
        );
    }

    state->is_active = frame.parent_active;
    return PP_STATUS_OK;
}

static pp_status_code pp_handle_include(
    pp_core_state *state,
    const pp_line_span *line,
    const pp_directive_span *directive,
    pp_diagnostic *diag
) {
    size_t column = (directive->hash_pos - line->start) + 1u;

    if (!state->is_active) {
        return PP_STATUS_OK;
    }

    if (!state->config->enable_include) {
        return pp_set_error(
            diag,
            PP_STATUS_INCLUDE_DISABLED,
            line->number,
            column,
            "#include is disabled by config"
        );
    }

    return pp_set_error(
        diag,
        PP_STATUS_INTERNAL_ERROR,
        line->number,
        column,
        "#include support is not implemented yet"
    );
}

static pp_status_code pp_handle_unknown_directive(
    pp_core_state *state,
    const pp_line_span *line,
    const pp_directive_span *directive,
    int *emit_original_line,
    pp_diagnostic *diag
) {
    if (!state->is_active) {
        return PP_STATUS_OK;
    }

    if (state->config->strict_mode) {
        return pp_set_error(
            diag,
            PP_STATUS_UNKNOWN_DIRECTIVE,
            line->number,
            (directive->hash_pos - line->start) + 1u,
            "unknown preprocessor directive"
        );
    }

    *emit_original_line = 1;
    return PP_STATUS_OK;
}

static pp_status_code pp_process_directive_line(
    pp_core_state *state,
    const char *source,
    const pp_line_span *line,
    size_t hash_pos,
    int *emit_original_line,
    pp_diagnostic *diag
) {
    pp_directive_span directive;

    *emit_original_line = 0;
    pp_parse_directive(source, line, hash_pos, &directive);

    if (directive.keyword_len == 0u) {
        return pp_handle_unknown_directive(state, line, &directive, emit_original_line, diag);
    }

    if (pp_directive_is(source, &directive, "define")) {
        return pp_handle_define(state, source, line, &directive, diag);
    }

    if (pp_directive_is(source, &directive, "undef")) {
        return pp_handle_undef(state, source, line, &directive, diag);
    }

    if (pp_directive_is(source, &directive, "ifdef")) {
        return pp_handle_ifdef_like(state, source, line, &directive, 0, diag);
    }

    if (pp_directive_is(source, &directive, "ifndef")) {
        return pp_handle_ifdef_like(state, source, line, &directive, 1, diag);
    }

    if (pp_directive_is(source, &directive, "else")) {
        return pp_handle_else(state, source, line, &directive, diag);
    }

    if (pp_directive_is(source, &directive, "endif")) {
        return pp_handle_endif(state, source, line, &directive, diag);
    }

    if (pp_directive_is(source, &directive, "include")) {
        return pp_handle_include(state, line, &directive, diag);
    }

    return pp_handle_unknown_directive(state, line, &directive, emit_original_line, diag);
}

static pp_status_code pp_expand_line(
    pp_core_state *state,
    const char *source,
    const pp_line_span *line,
    pp_buffer *out,
    pp_diagnostic *diag
) {
    return pp_expand_active_line(
        &state->macros,
        state->config,
        &state->expand_state,
        source,
        line->start,
        line->content_end,
        line->out_end,
        line->number,
        out,
        diag
    );
}

static pp_status_code pp_process_line(
    pp_core_state *state,
    const char *source,
    const pp_line_span *line,
    pp_buffer *out,
    pp_diagnostic *diag
) {
    size_t cursor;
    pp_status_code status;

    if (state->is_active && state->expand_state.in_block_comment) {
        return pp_expand_line(state, source, line, out, diag);
    }

    cursor = pp_skip_space_tab(source, line->start, line->content_end);
    if (cursor < line->content_end && source[cursor] == '#') {
        int emit_original_line;

        status = pp_process_directive_line(state, source, line, cursor, &emit_original_line, diag);
        if (status != PP_STATUS_OK) {
            return status;
        }

        if (emit_original_line && state->is_active) {
            return pp_buffer_append_n(out, source + line->start, line->out_end - line->start);
        }

        return PP_STATUS_OK;
    }

    if (!state->is_active) {
        return PP_STATUS_OK;
    }

    return pp_expand_line(state, source, line, out, diag);
}

static pp_status_code pp_validate_final_state(
    const pp_core_state *state,
    size_t line_after_end,
    pp_diagnostic *diag
) {
    if (state->expand_state.in_block_comment && state->config->strict_mode) {
        return pp_set_error(
            diag,
            PP_STATUS_SYNTAX_ERROR,
            line_after_end,
            1u,
            "unclosed block comment"
        );
    }

    if (state->condition_stack.depth != 0u) {
        return pp_set_error(
            diag,
            PP_STATUS_CONDITIONAL_UNCLOSED,
            line_after_end,
            1u,
            "unclosed conditional block"
        );
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
    size_t cursor = 0u;
    size_t line_number = 1u;

    if (state == NULL || source == NULL || out == NULL || diag == NULL) {
        return PP_STATUS_INVALID_ARGUMENT;
    }

    while (1) {
        pp_line_span line;
        pp_status_code status;

        if (!pp_next_line(source, source_len, &cursor, line_number, &line)) {
            break;
        }

        status = pp_process_line(state, source, &line, out, diag);
        if (status != PP_STATUS_OK) {
            return status;
        }

        line_number += 1u;
    }

    return pp_validate_final_state(state, line_number, diag);
}
