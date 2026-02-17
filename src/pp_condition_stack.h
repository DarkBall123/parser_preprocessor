#ifndef PP_CONDITION_STACK_H
#define PP_CONDITION_STACK_H

#include <stddef.h>

#include "preprocessor/preprocessor.h"

typedef struct pp_condition_frame {
    int parent_active;
    int condition_true;
    int else_seen;
} pp_condition_frame;

typedef struct pp_condition_stack {
    pp_condition_frame *items;
    size_t depth;
    size_t cap;
    size_t max_depth;
} pp_condition_stack;

void pp_condition_stack_init(pp_condition_stack *stack, size_t max_depth);
void pp_condition_stack_free(pp_condition_stack *stack);

pp_status_code pp_condition_stack_push(
    pp_condition_stack *stack,
    int parent_active,
    int condition_true
);

pp_status_code pp_condition_stack_pop(pp_condition_stack *stack, pp_condition_frame *out_frame);
pp_condition_frame *pp_condition_stack_peek(pp_condition_stack *stack);

#endif
