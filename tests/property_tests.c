#include "preprocessor/preprocessor.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t next_rand(uint32_t *state) {
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static char random_char(uint32_t *state) {
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "_#/\\\"* []();,+-"
        "\t\n\r";
    uint32_t v = next_rand(state);
    return charset[v % (sizeof(charset) - 1u)];
}

static char *generate_input(uint32_t *state, size_t *out_len) {
    size_t len = (size_t)(next_rand(state) % 512u);
    char *buf = (char *)malloc(len + 1u);
    size_t i;

    if (buf == NULL) {
        return NULL;
    }

    for (i = 0u; i < len; ++i) {
        buf[i] = random_char(state);
    }
    buf[len] = '\0';

    if (out_len != NULL) {
        *out_len = len;
    }

    return buf;
}

static int str_eq_nullable(const char *a, const char *b) {
    if (a == NULL && b == NULL) {
        return 1;
    }
    if (a == NULL || b == NULL) {
        return 0;
    }
    return strcmp(a, b) == 0;
}

int main(void) {
    uint32_t seed = 0xC0FFEEu;
    size_t i;

    for (i = 0u; i < 500u; ++i) {
        char *input;
        size_t input_len;
        pp_config config;
        pp_result a;
        pp_result b;
        pp_status_code sa;
        pp_status_code sb;

        input = generate_input(&seed, &input_len);
        if (input == NULL) {
            fprintf(stderr, "property test alloc failed\n");
            return 1;
        }

        pp_config_init(&config);
        config.strict_mode = (int)(next_rand(&seed) & 1u);
        config.support_block_comments = (int)(next_rand(&seed) & 1u);
        config.max_input_len = input_len + 8u;
        config.max_output_len = 0u;

        sa = pp_preprocess(input, &config, NULL, NULL, &a);
        sb = pp_preprocess(input, &config, NULL, NULL, &b);

        if (sa != sb) {
            fprintf(stderr, "determinism failed: status mismatch on case %zu\n", i);
            free(input);
            pp_result_dispose(&a);
            pp_result_dispose(&b);
            return 1;
        }

        if (a.status != b.status || a.diagnostic.code != b.diagnostic.code ||
            a.diagnostic.line != b.diagnostic.line || a.diagnostic.column != b.diagnostic.column) {
            fprintf(stderr, "determinism failed: metadata mismatch on case %zu\n", i);
            free(input);
            pp_result_dispose(&a);
            pp_result_dispose(&b);
            return 1;
        }

        if (sa == PP_STATUS_OK) {
            if (a.out_len != b.out_len || !str_eq_nullable(a.out_text, b.out_text)) {
                fprintf(stderr, "determinism failed: output mismatch on case %zu\n", i);
                free(input);
                pp_result_dispose(&a);
                pp_result_dispose(&b);
                return 1;
            }
        }

        if (!str_eq_nullable(a.diagnostic.message, b.diagnostic.message)) {
            fprintf(stderr, "determinism failed: diagnostic mismatch on case %zu\n", i);
            free(input);
            pp_result_dispose(&a);
            pp_result_dispose(&b);
            return 1;
        }

        free(input);
        pp_result_dispose(&a);
        pp_result_dispose(&b);
    }

    fprintf(stdout, "property ok\n");
    return 0;
}
