#ifndef PP_DIAGNOSTICS_H
#define PP_DIAGNOSTICS_H

#include <stddef.h>

#include "preprocessor/preprocessor.h"

void pp_diag_init(pp_diagnostic *diag);
void pp_diag_clear(pp_diagnostic *diag);

pp_status_code pp_diag_set(
    pp_diagnostic *diag,
    pp_status_code code,
    size_t line,
    size_t column,
    const char *message
);

#endif
