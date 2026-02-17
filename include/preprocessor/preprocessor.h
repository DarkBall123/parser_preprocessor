#ifndef PREPROCESSOR_PREPROCESSOR_H
#define PREPROCESSOR_PREPROCESSOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum pp_status_code {
    PP_STATUS_OK = 0,
    PP_STATUS_INVALID_ARGUMENT = 1,
    PP_STATUS_INPUT_TOO_LARGE = 2,
    PP_STATUS_OUTPUT_TOO_LARGE = 3,
    PP_STATUS_ALLOC_FAILED = 4,
    PP_STATUS_INCLUDE_DISABLED = 5,
    PP_STATUS_INCLUDE_NOT_FOUND = 6,
    PP_STATUS_INTERNAL_ERROR = 7,
    PP_STATUS_SYNTAX_ERROR = 8,
    PP_STATUS_UNKNOWN_DIRECTIVE = 9,
    PP_STATUS_CONDITIONAL_MISMATCH = 10,
    PP_STATUS_CONDITIONAL_UNCLOSED = 11,
    PP_STATUS_DEPTH_LIMIT_EXCEEDED = 12,
    PP_STATUS_MACRO_LIMIT_EXCEEDED = 13
} pp_status_code;

typedef enum pp_include_status {
    PP_INCLUDE_STATUS_OK = 0,
    PP_INCLUDE_STATUS_NOT_FOUND = 1,
    PP_INCLUDE_STATUS_ERROR = 2
} pp_include_status;

typedef struct pp_include_result {
    pp_include_status status;
    const char *content;
    size_t content_len;
    void (*release)(const char *content, void *user_ctx);
} pp_include_result;

typedef pp_include_result (*pp_include_resolver_fn)(const char *path, void *user_ctx);

typedef struct pp_config {
    size_t max_input_len;
    size_t max_output_len;
    size_t max_macro_count;
    size_t max_conditional_depth;
    size_t max_include_depth;
    int enable_include;
    int strict_mode;
    int support_block_comments;
} pp_config;

typedef struct pp_diagnostic {
    pp_status_code code;
    size_t line;
    size_t column;
    char *message;
} pp_diagnostic;

typedef struct pp_result {
    pp_status_code status;
    char *out_text;
    size_t out_len;
    pp_diagnostic diagnostic;
} pp_result;

void pp_config_init(pp_config *config);

pp_status_code pp_preprocess(
    const char *source,
    const pp_config *config,
    pp_include_resolver_fn include_resolver,
    void *include_user_ctx,
    pp_result *out_result
);

void pp_result_dispose(pp_result *result);

const char *pp_status_to_string(pp_status_code code);

#ifdef __cplusplus
}
#endif

#endif
