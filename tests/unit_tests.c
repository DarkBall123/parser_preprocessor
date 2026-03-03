#include "preprocessor/preprocessor.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
            return 1; \
        } \
    } while (0)

#define ASSERT_EQ_INT(actual, expected) ASSERT_TRUE((actual) == (expected))
#define ASSERT_EQ_SIZE(actual, expected) ASSERT_TRUE((actual) == (expected))
#define ASSERT_EQ_STR(actual, expected) ASSERT_TRUE(strcmp((actual), (expected)) == 0)

static int run_preprocess(const char *input, const pp_config *config, pp_result *result) {
    return pp_preprocess(input, config, NULL, NULL, result);
}

static int test_empty_input_returns_empty_output(void) {
    pp_result result;
    pp_status_code status;

    status = run_preprocess("", NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_INT(result.status, PP_STATUS_OK);
    ASSERT_TRUE(result.out_text != NULL);
    ASSERT_EQ_SIZE(result.out_len, 0u);
    ASSERT_EQ_STR(result.out_text, "");

    pp_result_dispose(&result);
    return 0;
}

static int test_plain_text_is_passthrough(void) {
    const char *input = "line1\nline2\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_TRUE(result.out_text != NULL);
    ASSERT_EQ_SIZE(result.out_len, strlen(input));
    ASSERT_EQ_STR(result.out_text, input);

    pp_result_dispose(&result);
    return 0;
}

static int test_define_and_ifdef_branch(void) {
    const char *input =
        "#define FLAG\n"
        "#ifdef FLAG\n"
        "ok\n"
        "#endif\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "ok\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_ifndef_and_else_branch(void) {
    const char *input =
        "#ifndef FLAG\n"
        "left\n"
        "#else\n"
        "right\n"
        "#endif\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "left\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_else_branch_when_macro_not_defined(void) {
    const char *input =
        "#ifdef FLAG\n"
        "bad\n"
        "#else\n"
        "good\n"
        "#endif\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "good\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_undef_removes_macro(void) {
    const char *input =
        "#define FLAG 1\n"
        "#undef FLAG\n"
        "#ifdef FLAG\n"
        "bad\n"
        "#endif\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "");

    pp_result_dispose(&result);
    return 0;
}

static int test_nested_conditionals(void) {
    const char *input =
        "#define OUTER\n"
        "#ifdef OUTER\n"
        "#ifndef INNER\n"
        "x\n"
        "#endif\n"
        "#endif\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "x\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_ifdef_with_trailing_line_comment(void) {
    const char *input =
        "#define FLAG\n"
        "#ifdef FLAG // note\n"
        "ok\n"
        "#endif\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "ok\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_ifndef_with_trailing_block_comment(void) {
    const char *input =
        "#ifndef FLAG /* note */\n"
        "ok\n"
        "#endif\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "ok\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_undef_with_trailing_comment(void) {
    const char *input =
        "#define FLAG 1\n"
        "#undef FLAG // note\n"
        "#ifdef FLAG\n"
        "bad\n"
        "#endif\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "");

    pp_result_dispose(&result);
    return 0;
}

static int test_else_and_endif_with_trailing_comments(void) {
    const char *input =
        "#ifdef FLAG\n"
        "bad\n"
        "#else // note\n"
        "ok\n"
        "#endif /* tail */\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "ok\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_macro_substitution_basic(void) {
    const char *input =
        "#define N 3\n"
        "N\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "3\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_define_ignores_trailing_line_comment(void) {
    const char *input =
        "#define X 1 // note\n"
        "X+1\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "1 +1\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_define_ignores_trailing_block_comment(void) {
    const char *input =
        "#define X 1 /* note */\n"
        "X+1\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "1 +1\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_define_keeps_comment_markers_inside_string(void) {
    const char *input =
        "#define S \"a // b\"\n"
        "S\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "\"a // b\"\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_macro_substitution_identifier_boundaries(void) {
    const char *input =
        "#define N 3\n"
        "N NN _N N1 N\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "3 NN _N N1 3\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_macro_not_replaced_in_string_literal(void) {
    const char *input =
        "#define N 3\n"
        "hint \"N=%1\"; N;\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "hint \"N=%1\"; 3;\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_macro_not_replaced_in_line_comment(void) {
    const char *input =
        "#define N 3\n"
        "N // N\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "3 // N\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_macro_not_replaced_in_block_comment(void) {
    const char *input =
        "#define N 3\n"
        "/* N */ N\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "/* N */ 3\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_macro_not_replaced_in_multiline_block_comment(void) {
    const char *input =
        "#define N 3\n"
        "/* N\n"
        "N */\n"
        "N\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "/* N\nN */\n3\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_directive_inside_block_comment_not_processed(void) {
    const char *input =
        "#define F 1\n"
        "/*\n"
        "#ifdef F\n"
        "INNER\n"
        "#endif\n"
        "*/\n"
        "F\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "/*\n#ifdef F\nINNER\n#endif\n*/\n1\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_unclosed_block_comment_strict_mode_reports_error(void) {
    const char *input =
        "#define A 1\n"
        "/* A\n"
        "A\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_SYNTAX_ERROR);
    ASSERT_TRUE(result.diagnostic.line >= 3u);

    pp_result_dispose(&result);
    return 0;
}

static int test_unclosed_block_comment_non_strict_is_allowed(void) {
    const char *input =
        "#define A 1\n"
        "/* A\n"
        "A\n";
    pp_config config;
    pp_result result;
    pp_status_code status;

    pp_config_init(&config);
    config.strict_mode = 0;

    status = run_preprocess(input, &config, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "/* A\nA\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_block_comments_disabled_strict_mode_reports_error(void) {
    const char *input =
        "#define N 3\n"
        "/* N */ N\n";
    pp_config config;
    pp_result result;
    pp_status_code status;

    pp_config_init(&config);
    config.support_block_comments = 0;

    status = run_preprocess(input, &config, &result);
    ASSERT_EQ_INT(status, PP_STATUS_SYNTAX_ERROR);
    ASSERT_EQ_INT(result.diagnostic.line, 2u);

    pp_result_dispose(&result);
    return 0;
}

static int test_block_comments_disabled_non_strict_keeps_plain_text_behavior(void) {
    const char *input =
        "#define N 3\n"
        "/* N */ N\n";
    pp_config config;
    pp_result result;
    pp_status_code status;

    pp_config_init(&config);
    config.support_block_comments = 0;
    config.strict_mode = 0;

    status = run_preprocess(input, &config, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "/* 3 */ 3\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_crlf_preserved_in_output(void) {
    const char *input =
        "#define N 3\r\n"
        "N\r\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "3\r\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_macro_redefinition_works(void) {
    const char *input =
        "#define X 1\n"
        "#define X 2\n"
        "X\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "2\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_empty_macro_replacement(void) {
    const char *input =
        "#define EMPTY\n"
        "AEMPTYB EMPTY\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "AEMPTYB \n");

    pp_result_dispose(&result);
    return 0;
}

static int test_spec_example_behavior(void) {
    const char *input =
        "#define N 3\n"
        "#ifdef N\n"
        "hint format [\"N=%1\", N];\n"
        "#else\n"
        "hint \"no\";\n"
        "#endif\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "hint format [\"N=%1\", 3];\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_input_limit_is_enforced(void) {
    pp_config config;
    pp_result result;
    pp_status_code status;

    pp_config_init(&config);
    config.max_input_len = 3u;

    status = run_preprocess("abcd", &config, &result);
    ASSERT_EQ_INT(status, PP_STATUS_INPUT_TOO_LARGE);
    ASSERT_EQ_INT(result.status, PP_STATUS_INPUT_TOO_LARGE);
    ASSERT_TRUE(result.out_text == NULL);
    ASSERT_TRUE(result.diagnostic.message != NULL);

    pp_result_dispose(&result);
    return 0;
}

static int test_output_limit_is_enforced(void) {
    pp_config config;
    pp_result result;
    pp_status_code status;

    pp_config_init(&config);
    config.max_output_len = 2u;

    status = run_preprocess("abcd", &config, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OUTPUT_TOO_LARGE);
    ASSERT_EQ_INT(result.status, PP_STATUS_OUTPUT_TOO_LARGE);
    ASSERT_TRUE(result.out_text == NULL);
    ASSERT_TRUE(result.diagnostic.message != NULL);

    pp_result_dispose(&result);
    return 0;
}

static int test_null_source_returns_invalid_argument(void) {
    pp_result result;
    pp_status_code status;

    status = pp_preprocess(NULL, NULL, NULL, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_INVALID_ARGUMENT);
    ASSERT_EQ_INT(result.status, PP_STATUS_INVALID_ARGUMENT);
    ASSERT_TRUE(result.diagnostic.message != NULL);

    pp_result_dispose(&result);
    return 0;
}

static int test_else_without_if_reports_error(void) {
    pp_result result;
    pp_status_code status;

    status = run_preprocess("#else\n", NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_CONDITIONAL_MISMATCH);
    ASSERT_EQ_INT(result.diagnostic.line, 1u);

    pp_result_dispose(&result);
    return 0;
}

static int test_endif_without_if_reports_error(void) {
    pp_result result;
    pp_status_code status;

    status = run_preprocess("#endif\n", NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_CONDITIONAL_MISMATCH);
    ASSERT_EQ_INT(result.diagnostic.line, 1u);

    pp_result_dispose(&result);
    return 0;
}

static int test_duplicate_else_reports_syntax_error(void) {
    const char *input =
        "#ifdef X\n"
        "a\n"
        "#else\n"
        "b\n"
        "#else\n"
        "c\n"
        "#endif\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_SYNTAX_ERROR);
    ASSERT_EQ_INT(result.diagnostic.line, 5u);

    pp_result_dispose(&result);
    return 0;
}

static int test_unclosed_conditional_reports_error(void) {
    const char *input =
        "#ifdef X\n"
        "line\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_CONDITIONAL_UNCLOSED);
    ASSERT_TRUE(result.diagnostic.line >= 2u);

    pp_result_dispose(&result);
    return 0;
}

static int test_include_disabled_reports_error(void) {
    pp_result result;
    pp_status_code status;

    status = run_preprocess("#include \"x.hpp\"\n", NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_INCLUDE_DISABLED);
    ASSERT_EQ_INT(result.diagnostic.line, 1u);

    pp_result_dispose(&result);
    return 0;
}

static int test_include_in_inactive_branch_is_ignored(void) {
    const char *input =
        "#ifdef NOT_DEFINED\n"
        "#include \"x.hpp\"\n"
        "#endif\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "");

    pp_result_dispose(&result);
    return 0;
}

static int test_define_without_name_reports_syntax_error(void) {
    pp_result result;
    pp_status_code status;

    status = run_preprocess("#define\n", NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_SYNTAX_ERROR);
    ASSERT_EQ_INT(result.diagnostic.line, 1u);

    pp_result_dispose(&result);
    return 0;
}

static int test_unknown_directive_strict_mode_reports_error(void) {
    pp_result result;
    pp_status_code status;

    status = run_preprocess("#unknown\n", NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_UNKNOWN_DIRECTIVE);
    ASSERT_EQ_INT(result.diagnostic.line, 1u);

    pp_result_dispose(&result);
    return 0;
}

static int test_unknown_directive_non_strict_is_kept(void) {
    pp_config config;
    pp_result result;
    pp_status_code status;

    pp_config_init(&config);
    config.strict_mode = 0;

    status = run_preprocess("#unknown value\ntext\n", &config, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "#unknown value\ntext\n");

    pp_result_dispose(&result);
    return 0;
}

static int test_unknown_directive_in_inactive_branch_is_ignored(void) {
    const char *input =
        "#ifdef OFF\n"
        "#unknown inside\n"
        "#endif\n";
    pp_result result;
    pp_status_code status;

    status = run_preprocess(input, NULL, &result);
    ASSERT_EQ_INT(status, PP_STATUS_OK);
    ASSERT_EQ_STR(result.out_text, "");

    pp_result_dispose(&result);
    return 0;
}

static int test_depth_limit_is_enforced(void) {
    const char *input =
        "#ifdef A\n"
        "#ifdef B\n"
        "#endif\n"
        "#endif\n";
    pp_config config;
    pp_result result;
    pp_status_code status;

    pp_config_init(&config);
    config.max_conditional_depth = 1u;

    status = run_preprocess(input, &config, &result);
    ASSERT_EQ_INT(status, PP_STATUS_DEPTH_LIMIT_EXCEEDED);
    ASSERT_EQ_INT(result.diagnostic.line, 2u);

    pp_result_dispose(&result);
    return 0;
}

static int test_macro_limit_is_enforced(void) {
    const char *input =
        "#define A 1\n"
        "#define B 2\n";
    pp_config config;
    pp_result result;
    pp_status_code status;

    pp_config_init(&config);
    config.max_macro_count = 1u;

    status = run_preprocess(input, &config, &result);
    ASSERT_EQ_INT(status, PP_STATUS_MACRO_LIMIT_EXCEEDED);
    ASSERT_EQ_INT(result.diagnostic.line, 2u);

    pp_result_dispose(&result);
    return 0;
}

int main(void) {
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"empty_input_returns_empty_output", test_empty_input_returns_empty_output},
        {"plain_text_is_passthrough", test_plain_text_is_passthrough},
        {"define_and_ifdef_branch", test_define_and_ifdef_branch},
        {"ifndef_and_else_branch", test_ifndef_and_else_branch},
        {"else_branch_when_macro_not_defined", test_else_branch_when_macro_not_defined},
        {"undef_removes_macro", test_undef_removes_macro},
        {"nested_conditionals", test_nested_conditionals},
        {"ifdef_with_trailing_line_comment", test_ifdef_with_trailing_line_comment},
        {"ifndef_with_trailing_block_comment", test_ifndef_with_trailing_block_comment},
        {"undef_with_trailing_comment", test_undef_with_trailing_comment},
        {"else_and_endif_with_trailing_comments", test_else_and_endif_with_trailing_comments},
        {"macro_substitution_basic", test_macro_substitution_basic},
        {"define_ignores_trailing_line_comment", test_define_ignores_trailing_line_comment},
        {"define_ignores_trailing_block_comment", test_define_ignores_trailing_block_comment},
        {"define_keeps_comment_markers_inside_string", test_define_keeps_comment_markers_inside_string},
        {"macro_substitution_identifier_boundaries", test_macro_substitution_identifier_boundaries},
        {"macro_not_replaced_in_string_literal", test_macro_not_replaced_in_string_literal},
        {"macro_not_replaced_in_line_comment", test_macro_not_replaced_in_line_comment},
        {"macro_not_replaced_in_block_comment", test_macro_not_replaced_in_block_comment},
        {"macro_not_replaced_in_multiline_block_comment", test_macro_not_replaced_in_multiline_block_comment},
        {"directive_inside_block_comment_not_processed", test_directive_inside_block_comment_not_processed},
        {"unclosed_block_comment_strict_mode_reports_error", test_unclosed_block_comment_strict_mode_reports_error},
        {"unclosed_block_comment_non_strict_is_allowed", test_unclosed_block_comment_non_strict_is_allowed},
        {"block_comments_disabled_strict_mode_reports_error", test_block_comments_disabled_strict_mode_reports_error},
        {"block_comments_disabled_non_strict_keeps_plain_text_behavior", test_block_comments_disabled_non_strict_keeps_plain_text_behavior},
        {"crlf_preserved_in_output", test_crlf_preserved_in_output},
        {"macro_redefinition_works", test_macro_redefinition_works},
        {"empty_macro_replacement", test_empty_macro_replacement},
        {"spec_example_behavior", test_spec_example_behavior},
        {"input_limit_is_enforced", test_input_limit_is_enforced},
        {"output_limit_is_enforced", test_output_limit_is_enforced},
        {"null_source_returns_invalid_argument", test_null_source_returns_invalid_argument},
        {"else_without_if_reports_error", test_else_without_if_reports_error},
        {"endif_without_if_reports_error", test_endif_without_if_reports_error},
        {"duplicate_else_reports_syntax_error", test_duplicate_else_reports_syntax_error},
        {"unclosed_conditional_reports_error", test_unclosed_conditional_reports_error},
        {"include_disabled_reports_error", test_include_disabled_reports_error},
        {"include_in_inactive_branch_is_ignored", test_include_in_inactive_branch_is_ignored},
        {"define_without_name_reports_syntax_error", test_define_without_name_reports_syntax_error},
        {"unknown_directive_strict_mode_reports_error", test_unknown_directive_strict_mode_reports_error},
        {"unknown_directive_non_strict_is_kept", test_unknown_directive_non_strict_is_kept},
        {"unknown_directive_in_inactive_branch_is_ignored", test_unknown_directive_in_inactive_branch_is_ignored},
        {"depth_limit_is_enforced", test_depth_limit_is_enforced},
        {"macro_limit_is_enforced", test_macro_limit_is_enforced},
    };

    size_t i;
    int failed = 0;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        int rc = tests[i].fn();
        if (rc != 0) {
            fprintf(stderr, "[FAIL] %s\n", tests[i].name);
            failed = 1;
        } else {
            fprintf(stdout, "[OK] %s\n", tests[i].name);
        }
    }

    return failed;
}
