#ifndef PP_EXPANDER_H
#define PP_EXPANDER_H

#include <stddef.h>

#include "pp_buffer.h"
#include "pp_diagnostics.h"
#include "pp_macro_table.h"

typedef struct pp_expand_state {
    int in_block_comment;
} pp_expand_state;

void pp_expand_state_init(pp_expand_state *state);

pp_status_code pp_expand_active_line(
    const pp_macro_table *macros,
    const pp_config *config,
    pp_expand_state *state,
    const char *source,
    size_t line_start,
    size_t content_end,
    size_t line_out_end,
    size_t line,
    pp_buffer *out,
    pp_diagnostic *diag
);

#endif
