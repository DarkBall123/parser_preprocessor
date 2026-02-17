#include "pp_diagnostics.h"

#include <stdlib.h>
#include <string.h>

static char *pp_strdup_local(const char *text) {
    size_t len;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    len = strlen(text);
    copy = (char *)malloc(len + 1u);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, len + 1u);
    return copy;
}

void pp_diag_init(pp_diagnostic *diag) {
    if (diag == NULL) {
        return;
    }

    diag->code = PP_STATUS_OK;
    diag->line = 0;
    diag->column = 0;
    diag->message = NULL;
}

void pp_diag_clear(pp_diagnostic *diag) {
    if (diag == NULL) {
        return;
    }

    free(diag->message);
    diag->message = NULL;
    diag->code = PP_STATUS_OK;
    diag->line = 0;
    diag->column = 0;
}

pp_status_code pp_diag_set(
    pp_diagnostic *diag,
    pp_status_code code,
    size_t line,
    size_t column,
    const char *message
) {
    char *copy;

    if (diag == NULL) {
        return PP_STATUS_INVALID_ARGUMENT;
    }

    pp_diag_clear(diag);

    diag->code = code;
    diag->line = line;
    diag->column = column;

    if (message == NULL || message[0] == '\0') {
        return PP_STATUS_OK;
    }

    copy = pp_strdup_local(message);
    if (copy == NULL) {
        return PP_STATUS_ALLOC_FAILED;
    }

    diag->message = copy;
    return PP_STATUS_OK;
}
