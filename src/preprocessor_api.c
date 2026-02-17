#include "preprocessor/preprocessor.h"

#include <stdlib.h>
#include <string.h>

#include "pp_buffer.h"
#include "pp_core_state.h"
#include "pp_diagnostics.h"

static void pp_set_defaults(pp_config *config) {
    config->max_input_len = 0;
    config->max_output_len = 0;
    config->max_macro_count = 4096u;
    config->max_conditional_depth = 256u;
    config->max_include_depth = 64u;
    config->enable_include = 0;
    config->strict_mode = 1;
    config->support_block_comments = 1;
}

void pp_config_init(pp_config *config) {
    if (config == NULL) {
        return;
    }

    pp_set_defaults(config);
}

static void pp_result_reset(pp_result *result) {
    result->status = PP_STATUS_OK;
    result->out_text = NULL;
    result->out_len = 0;
    pp_diag_init(&result->diagnostic);
}

static pp_status_code pp_fail(
    pp_result *result,
    pp_status_code code,
    size_t line,
    size_t column,
    const char *message
) {
    result->status = code;
    if (pp_diag_set(&result->diagnostic, code, line, column, message) == PP_STATUS_ALLOC_FAILED) {
        result->status = PP_STATUS_ALLOC_FAILED;
        return PP_STATUS_ALLOC_FAILED;
    }
    return code;
}

pp_status_code pp_preprocess(
    const char *source,
    const pp_config *config,
    pp_include_resolver_fn include_resolver,
    void *include_user_ctx,
    pp_result *out_result
) {
    pp_config local_cfg;
    const pp_config *effective_cfg;
    size_t source_len;
    pp_buffer out;
    pp_core_state state;
    pp_status_code status;

    if (out_result == NULL) {
        return PP_STATUS_INVALID_ARGUMENT;
    }

    pp_result_reset(out_result);

    if (source == NULL) {
        return pp_fail(out_result, PP_STATUS_INVALID_ARGUMENT, 1, 1, "source is NULL");
    }

    if (config == NULL) {
        pp_set_defaults(&local_cfg);
        effective_cfg = &local_cfg;
    } else {
        effective_cfg = config;
    }

    source_len = strlen(source);
    if (effective_cfg->max_input_len > 0 && source_len > effective_cfg->max_input_len) {
        return pp_fail(
            out_result,
            PP_STATUS_INPUT_TOO_LARGE,
            1,
            1,
            "input exceeds max_input_len"
        );
    }

    pp_buffer_init(&out, effective_cfg->max_output_len);

    pp_core_state_init(&state, effective_cfg, include_resolver, include_user_ctx);
    status = pp_core_process(&state, source, source_len, &out, &out_result->diagnostic);
    pp_core_state_dispose(&state);
    if (status != PP_STATUS_OK) {
        pp_buffer_free(&out);
        out_result->status = status;

        if (out_result->diagnostic.code == PP_STATUS_OK) {
            if (status == PP_STATUS_OUTPUT_TOO_LARGE) {
                return pp_fail(
                    out_result,
                    status,
                    1,
                    1,
                    "output exceeds max_output_len"
                );
            }

            if (pp_diag_set(&out_result->diagnostic, status, 1, 1, pp_status_to_string(status)) == PP_STATUS_ALLOC_FAILED) {
                out_result->status = PP_STATUS_ALLOC_FAILED;
                return PP_STATUS_ALLOC_FAILED;
            }
        }
        return status;
    }

    out_result->out_text = pp_buffer_take(&out, &out_result->out_len);
    if (out_result->out_text == NULL) {
        pp_buffer_free(&out);
        return pp_fail(out_result, PP_STATUS_ALLOC_FAILED, 1, 1, "failed to allocate output buffer");
    }

    out_result->status = PP_STATUS_OK;
    return PP_STATUS_OK;
}

void pp_result_dispose(pp_result *result) {
    if (result == NULL) {
        return;
    }

    free(result->out_text);
    result->out_text = NULL;
    result->out_len = 0;
    result->status = PP_STATUS_OK;

    pp_diag_clear(&result->diagnostic);
}

const char *pp_status_to_string(pp_status_code code) {
    switch (code) {
        case PP_STATUS_OK:
            return "ok";
        case PP_STATUS_INVALID_ARGUMENT:
            return "invalid argument";
        case PP_STATUS_INPUT_TOO_LARGE:
            return "input too large";
        case PP_STATUS_OUTPUT_TOO_LARGE:
            return "output too large";
        case PP_STATUS_ALLOC_FAILED:
            return "allocation failed";
        case PP_STATUS_INCLUDE_DISABLED:
            return "include is disabled";
        case PP_STATUS_INCLUDE_NOT_FOUND:
            return "include not found";
        case PP_STATUS_INTERNAL_ERROR:
            return "internal error";
        case PP_STATUS_SYNTAX_ERROR:
            return "syntax error";
        case PP_STATUS_UNKNOWN_DIRECTIVE:
            return "unknown directive";
        case PP_STATUS_CONDITIONAL_MISMATCH:
            return "conditional mismatch";
        case PP_STATUS_CONDITIONAL_UNCLOSED:
            return "conditional block is not closed";
        case PP_STATUS_DEPTH_LIMIT_EXCEEDED:
            return "nesting depth limit exceeded";
        case PP_STATUS_MACRO_LIMIT_EXCEEDED:
            return "macro limit exceeded";
        default:
            return "unknown status";
    }
}
