#ifndef PP_CORE_STATE_H
#define PP_CORE_STATE_H

#include <stddef.h>

#include "pp_buffer.h"
#include "pp_condition_stack.h"
#include "pp_diagnostics.h"
#include "pp_expander.h"
#include "pp_macro_table.h"

typedef struct pp_core_state {
    const pp_config *config;
    pp_include_resolver_fn include_resolver;
    void *include_user_ctx;
    pp_macro_table macros;
    pp_condition_stack condition_stack;
    pp_expand_state expand_state;
    int is_active;
} pp_core_state;

void pp_core_state_init(
    pp_core_state *state,
    const pp_config *config,
    pp_include_resolver_fn include_resolver,
    void *include_user_ctx
);
void pp_core_state_dispose(pp_core_state *state);

pp_status_code pp_core_process(
    pp_core_state *state,
    const char *source,
    size_t source_len,
    pp_buffer *out,
    pp_diagnostic *diag
);

#endif
