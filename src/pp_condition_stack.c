#include "pp_condition_stack.h"

#include <stdint.h>
#include <stdlib.h>

static pp_status_code pp_condition_stack_reserve(pp_condition_stack *stack, size_t required) {
    size_t new_cap;
    pp_condition_frame *new_items;

    if (required <= stack->cap) {
        return PP_STATUS_OK;
    }

    new_cap = stack->cap == 0u ? 16u : stack->cap;
    while (new_cap < required) {
        if (new_cap > (SIZE_MAX / 2u)) {
            new_cap = required;
            break;
        }
        new_cap *= 2u;
    }

    new_items = (pp_condition_frame *)realloc(stack->items, new_cap * sizeof(pp_condition_frame));
    if (new_items == NULL) {
        return PP_STATUS_ALLOC_FAILED;
    }

    stack->items = new_items;
    stack->cap = new_cap;
    return PP_STATUS_OK;
}

void pp_condition_stack_init(pp_condition_stack *stack, size_t max_depth) {
    if (stack == NULL) {
        return;
    }

    stack->items = NULL;
    stack->depth = 0u;
    stack->cap = 0u;
    stack->max_depth = max_depth;
}

void pp_condition_stack_free(pp_condition_stack *stack) {
    if (stack == NULL) {
        return;
    }

    free(stack->items);
    stack->items = NULL;
    stack->depth = 0u;
    stack->cap = 0u;
}

pp_status_code pp_condition_stack_push(
    pp_condition_stack *stack,
    int parent_active,
    int condition_true
) {
    pp_status_code status;

    if (stack == NULL) {
        return PP_STATUS_INVALID_ARGUMENT;
    }

    if (stack->max_depth > 0u && stack->depth >= stack->max_depth) {
        return PP_STATUS_DEPTH_LIMIT_EXCEEDED;
    }

    status = pp_condition_stack_reserve(stack, stack->depth + 1u);
    if (status != PP_STATUS_OK) {
        return status;
    }

    stack->items[stack->depth].parent_active = parent_active;
    stack->items[stack->depth].condition_true = condition_true;
    stack->items[stack->depth].else_seen = 0;
    stack->depth += 1u;

    return PP_STATUS_OK;
}

pp_status_code pp_condition_stack_pop(pp_condition_stack *stack, pp_condition_frame *out_frame) {
    if (stack == NULL) {
        return PP_STATUS_INVALID_ARGUMENT;
    }

    if (stack->depth == 0u) {
        return PP_STATUS_CONDITIONAL_MISMATCH;
    }

    stack->depth -= 1u;
    if (out_frame != NULL) {
        *out_frame = stack->items[stack->depth];
    }

    return PP_STATUS_OK;
}

pp_condition_frame *pp_condition_stack_peek(pp_condition_stack *stack) {
    if (stack == NULL || stack->depth == 0u) {
        return NULL;
    }

    return &stack->items[stack->depth - 1u];
}
