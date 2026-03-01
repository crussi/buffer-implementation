#include "action_stack.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Default initial capacity when the caller passes 0.
#define ACTION_STACK_DEFAULT_CAPACITY 16

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Free the text payload of a single Action (safe on NULL text).
static void free_action(Action *action) {
    if (!action) return;
    free(action->text);
    action->text = NULL;
}

// Free every text payload between indices [from, to).
static void free_text_payloads(ActionStack *stack, size_t from, size_t to) {
    for (size_t i = from; i < to; i++) {
        free_action(&stack->actions[i]);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

ActionStack *new_action_stack(size_t initial_capacity, size_t max_capacity) {
    ActionStack *stack = malloc(sizeof(ActionStack));
    if (!stack) {
        fprintf(stderr, "action_stack: failed to allocate ActionStack\n");
        return NULL;
    }

    size_t cap = (initial_capacity > 0) ? initial_capacity
                                        : ACTION_STACK_DEFAULT_CAPACITY;

    // If a max_capacity ceiling is set, don't allocate more than needed.
    if (max_capacity > 0 && cap > max_capacity) {
        cap = max_capacity;
    }

    stack->actions = malloc(cap * sizeof(Action));
    if (!stack->actions) {
        fprintf(stderr, "action_stack: failed to allocate actions array\n");
        free(stack);
        return NULL;
    }

    stack->size         = 0;
    stack->capacity     = cap;
    stack->max_capacity = max_capacity;
    return stack;
}

bool push_action(ActionStack *stack, Action action) {
    if (!stack || !stack->actions) return false;

    // Enforce optional hard ceiling.
    if (stack->max_capacity > 0 && stack->size >= stack->max_capacity) {
        // Drop the oldest entry (index 0) to make room, preserving a
        // sliding-window of the most recent actions.
        free_action(&stack->actions[0]);
        memmove(stack->actions, stack->actions + 1,
                (stack->size - 1) * sizeof(Action));
        stack->size--;
    }

    // Grow the buffer if needed.
    if (stack->size == stack->capacity) {
        // Guard against size_t overflow when doubling.
        if (stack->capacity > SIZE_MAX / 2) {
            fprintf(stderr, "action_stack: capacity overflow\n");
            return false;
        }
        size_t new_capacity = stack->capacity * 2;

        // Don't exceed max_capacity when growing.
        if (stack->max_capacity > 0 && new_capacity > stack->max_capacity) {
            new_capacity = stack->max_capacity;
        }

        Action *temp = realloc(stack->actions, new_capacity * sizeof(Action));
        if (!temp) {
            // Original pointer is still valid; stack remains usable at
            // current capacity.
            fprintf(stderr, "action_stack: realloc failed, push dropped\n");
            return false;
        }
        stack->actions  = temp;
        stack->capacity = new_capacity;
    }

    stack->actions[stack->size++] = action;
    return true;
}

bool pop_action(ActionStack *stack, Action *out_action) {
    if (!stack || !stack->actions || stack->size == 0) return false;
    // Transfer ownership of text payload to the caller.
    *out_action = stack->actions[--stack->size];
    return true;
}

bool peek_action(const ActionStack *stack, Action *out_action) {
    if (!stack || !stack->actions || stack->size == 0) return false;
    // Caller must NOT free out_action->text; the stack still owns it.
    *out_action = stack->actions[stack->size - 1];
    return true;
}

bool action_stack_is_empty(const ActionStack *stack) {
    return !stack || stack->size == 0;
}

void reset_action_stack(ActionStack *stack) {
    if (!stack || !stack->actions) return;
    free_text_payloads(stack, 0, stack->size);
    stack->size = 0;
}

 void free_action_stack(ActionStack *stack) {
    if (!stack) return;
    free_text_payloads(stack, 0, stack->size);
    free(stack->actions);
    free(stack);
}