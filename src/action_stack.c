#include "action_stack.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

#define UNDO_NODE_INIT_CAPACITY 8
#define ACTION_STACK_DEFAULT_CAPACITY 16

static void free_action_payload(Action *a) {
    if (!a) return;
    free(a->text);
    a->text = NULL;
}

// Allocate a fresh UndoNode with no parent and no actions yet.
static UndoNode *node_new(uint64_t seq) {
    UndoNode *n = calloc(1, sizeof(UndoNode));
    if (!n) return NULL;
    n->seq      = seq;
    n->capacity = UNDO_NODE_INIT_CAPACITY;
    n->actions  = malloc(n->capacity * sizeof(Action));
    if (!n->actions) { free(n); return NULL; }
    return n;
}

// Free a single node and all its action payloads.
// Does NOT free children – the caller must walk the tree.
static void node_free(UndoNode *n) {
    if (!n) return;
    for (size_t i = 0; i < n->count; i++)
        free_action_payload(&n->actions[i]);
    free(n->actions);
    free(n);
}

// Recursively free an entire subtree rooted at n.
static void subtree_free(UndoNode *n) {
    if (!n) return;
    // Free sibling chain first (avoids deep recursion on wide trees)
    UndoNode *child = n->children;
    while (child) {
        UndoNode *next = child->next_sibling;
        subtree_free(child);
        child = next;
    }
    node_free(n);
}

// Append child to parent's child list and update last_child.
static void node_add_child(UndoNode *parent, UndoNode *child) {
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children    = child;
    parent->last_child  = child;   // most recently added == preferred redo target
}

// ---------------------------------------------------------------------------
// UndoTree lifecycle
// ---------------------------------------------------------------------------

UndoTree *undo_tree_new(void) {
    UndoTree *tree = calloc(1, sizeof(UndoTree));
    if (!tree) return NULL;

    // seq 0 is the sentinel root – the "nothing has happened yet" state.
    tree->root = node_new(0);
    if (!tree->root) { free(tree); return NULL; }

    tree->current   = tree->root;
    tree->open_group = NULL;
    tree->next_seq  = 1;
    return tree;
}

void undo_tree_free(UndoTree *tree) {
    if (!tree) return;
    // The open_group (if any) is already in the tree as a child of current,
    // so subtree_free will reach it.  But if it was never linked (shouldn't
    // happen with correct usage), free it separately.
    subtree_free(tree->root);
    free(tree);
}

// ---------------------------------------------------------------------------
// Change-group management
// ---------------------------------------------------------------------------

void undo_tree_open_group(UndoTree *tree, Position cursor_before) {
    if (!tree || tree->open_group) return;   // already open

    UndoNode *n = node_new(tree->next_seq++);
    if (!n) return;

    // Record where the cursor was BEFORE this group.
    // We store it as cursor_after of the *parent* node so that undo can
    // restore it.  Actually we stash it in a field on the new node for
    // easy access.
    n->cursor_after = cursor_before;   // will be overwritten on close

    // Link into tree immediately so actions can reference it.
    node_add_child(tree->current, n);

    tree->open_group = n;
}

void undo_tree_push_action(UndoTree *tree, Action action) {
    if (!tree || !tree->open_group) return;

    UndoNode *n = tree->open_group;

    // Grow action array if needed.
    if (n->count == n->capacity) {
        size_t new_cap = n->capacity * 2;
        Action *tmp = realloc(n->actions, new_cap * sizeof(Action));
        if (!tmp) {
            fprintf(stderr, "undo_tree: failed to grow action array\n");
            free_action_payload(&action);
            return;
        }
        n->actions  = tmp;
        n->capacity = new_cap;
    }

    n->actions[n->count++] = action;
    // Tree owns action.text from here on; caller must not free it.
}

void undo_tree_close_group(UndoTree *tree, Position cursor_after) {
    if (!tree || !tree->open_group) return;

    UndoNode *n = tree->open_group;
    tree->open_group = NULL;

    if (n->count == 0) {
        // Empty group – discard it to keep the tree clean.
        // Remove from parent's child list.
        UndoNode *parent = n->parent;
        if (parent) {
            if (parent->children == n) {
                parent->children = n->next_sibling;
            } else {
                UndoNode *prev = parent->children;
                while (prev && prev->next_sibling != n) prev = prev->next_sibling;
                if (prev) prev->next_sibling = n->next_sibling;
            }
            // Restore last_child if we just removed it.
            if (parent->last_child == n)
                parent->last_child = parent->children;
        }
        node_free(n);
        return;
    }

    n->cursor_after = cursor_after;
    // Make it the current node – the edits are now "applied".
    tree->current = n;
}

// ---------------------------------------------------------------------------
// Undo / Redo
// ---------------------------------------------------------------------------

bool undo_tree_undo(UndoTree *tree, Position *cursor_out) {
    if (!tree || !cursor_out) return false;

    // Cannot undo past the root.
    if (tree->current == tree->root) return false;

    // The node to undo is the current node.
    UndoNode *node = tree->current;

    // Move current to parent BEFORE reporting cursor – the cursor returned
    // should be the position the user had before this group was applied,
    // which is stored in the parent's cursor_after.
    tree->current = node->parent;

    // Cursor goes back to wherever it was before this group.
    // We stored that in the parent's cursor_after (or, equivalently, the
    // pre-edit cursor of this group).  We use the parent's cursor_after.
    *cursor_out = tree->current->cursor_after;

    return true;
}

bool undo_tree_redo(UndoTree *tree, Position *cursor_out) {
    if (!tree || !cursor_out) return false;

    // Follow last_child to reproduce the most recent redo branch (Vim Ctrl-R).
    UndoNode *target = tree->current->last_child;
    if (!target) return false;

    tree->current = target;
    *cursor_out   = target->cursor_after;
    return true;
}

// ---------------------------------------------------------------------------
// Iteration helpers
// ---------------------------------------------------------------------------

// These are simple accessors; the caller drives the actual buffer mutations.

const UndoNode *undo_tree_last_undone(const UndoTree *tree) {
    // After undo_tree_undo() returns true, current == parent.
    // The node that was undone is current->last_child (we just moved up).
    if (!tree) return NULL;
    return tree->current->last_child;
}

const UndoNode *undo_tree_last_redone(const UndoTree *tree) {
    if (!tree) return NULL;
    return tree->current;
}

// ---------------------------------------------------------------------------
// Legacy ActionStack shim (unchanged public API)
// ---------------------------------------------------------------------------

ActionStack *new_action_stack(size_t initial_capacity, size_t max_capacity) {
    ActionStack *stack = malloc(sizeof(ActionStack));
    if (!stack) return NULL;

    size_t cap = (initial_capacity > 0) ? initial_capacity
                                        : ACTION_STACK_DEFAULT_CAPACITY;
    if (max_capacity > 0 && cap > max_capacity) cap = max_capacity;

    stack->actions = malloc(cap * sizeof(Action));
    if (!stack->actions) { free(stack); return NULL; }

    stack->size         = 0;
    stack->capacity     = cap;
    stack->max_capacity = max_capacity;
    return stack;
}

bool push_action(ActionStack *stack, Action action) {
    if (!stack || !stack->actions) return false;

    if (stack->max_capacity > 0 && stack->size >= stack->max_capacity) {
        free_action_payload(&stack->actions[0]);
        memmove(stack->actions, stack->actions + 1,
                (stack->size - 1) * sizeof(Action));
        stack->size--;
    }

    if (stack->size == stack->capacity) {
        if (stack->capacity > SIZE_MAX / 2) return false;
        size_t new_cap = stack->capacity * 2;
        if (stack->max_capacity > 0 && new_cap > stack->max_capacity)
            new_cap = stack->max_capacity;
        Action *tmp = realloc(stack->actions, new_cap * sizeof(Action));
        if (!tmp) return false;
        stack->actions  = tmp;
        stack->capacity = new_cap;
    }

    stack->actions[stack->size++] = action;
    return true;
}

bool pop_action(ActionStack *stack, Action *out_action) {
    if (!stack || !stack->actions || stack->size == 0) return false;
    *out_action = stack->actions[--stack->size];
    return true;
}

bool peek_action(const ActionStack *stack, Action *out_action) {
    if (!stack || !stack->actions || stack->size == 0) return false;
    *out_action       = stack->actions[stack->size - 1];
    out_action->text  = NULL;
    return true;
}

bool action_stack_is_empty(const ActionStack *stack) {
    return !stack || stack->size == 0;
}

void reset_action_stack(ActionStack *stack) {
    if (!stack || !stack->actions) return;
    for (size_t i = 0; i < stack->size; i++)
        free_action_payload(&stack->actions[i]);
    stack->size = 0;
}

void free_action_stack(ActionStack *stack) {
    if (!stack) return;
    for (size_t i = 0; i < stack->size; i++)
        free_action_payload(&stack->actions[i]);
    free(stack->actions);
    free(stack);
}