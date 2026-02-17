#include "preprocessor/preprocessor.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    const char *input =
        "#define N 3\n"
        "#ifdef N\n"
        "hint format [\"N=%1\", N];\n"
        "#else\n"
        "hint \"no\";\n"
        "#endif\n";
    const char *expected = "hint format [\"N=%1\", 3];\n";
    pp_config config;
    pp_result result;
    pp_status_code status;

    pp_config_init(&config);
    config.max_input_len = 4096u;
    config.max_output_len = 4096u;

    status = pp_preprocess(input, &config, NULL, NULL, &result);
    if (status != PP_STATUS_OK) {
        fprintf(stderr, "smoke failed: status=%d message=%s\n", status,
            result.diagnostic.message != NULL ? result.diagnostic.message : "(null)");
        pp_result_dispose(&result);
        return 1;
    }

    if (result.out_text == NULL || strcmp(result.out_text, expected) != 0) {
        fprintf(stderr, "smoke failed: output mismatch\n");
        pp_result_dispose(&result);
        return 1;
    }

    pp_result_dispose(&result);
    fprintf(stdout, "smoke ok\n");
    return 0;
}
