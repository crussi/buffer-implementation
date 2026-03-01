#ifndef ACTION_STACK_H
#define ACTION_STACK_H

#include <stdbool.h>
#include <stddef.h>

// --- Action types for undo/redo ---
typedef enum {
    INSERT_CHAR,
    DELETE_CHAR,
    INSERT_CR,
    DELETE_CR
} ActionType;

// --- Cursor / text position ---
typedef struct {
    int row;
    int col;
} Position;

// --- Undo/redo action ---
typedef struct {
    ActionType  type;
    Position    position;
    char        character;  // single-char payload (INSERT_CHAR / DELETE_CHAR)
    char       *text;       // multi-char payload (nullable, heap-allocated)
} Action;

// --- Dynamic stack of Actions ---
typedef struct {
    Action  *actions;   // pointer to dynamic array of Actions
    size_t   size;      // number of Actions currently in the stack
    size_t   capacity;  // total allocated capacity
    size_t   max_capacity; // 0 = unlimited; >0 = hard ceiling
} ActionStack;

ActionStack *new_action_stack(size_t initial_capacity, size_t max_capacity);

bool push_action(ActionStack *stack, Action action);

bool pop_action(ActionStack *stack, Action *out_action);

bool peek_action(const ActionStack *stack, Action *out_action);

bool action_stack_is_empty(const ActionStack *stack);

void free_action_stack(ActionStack *stack);

void reset_action_stack(ActionStack *stack);

void free_action(Action *action);

#endif