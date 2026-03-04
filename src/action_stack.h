#ifndef ACTION_STACK_H
#define ACTION_STACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// --- Cursor / text position ---
typedef struct {
    int row;
    int col;
} Position;

// --- Action types for undo/redo ---
// Each type records enough information to both undo and redo itself.
typedef enum {
    INSERT_CHAR,    // inserted a single character
    DELETE_CHAR,    // deleted a single character
    INSERT_CR,      // inserted a newline (split row)
    DELETE_CR,      // deleted a newline (merged rows)
    INSERT_TEXT     // bulk string insertion; payload stored in `text`
} ActionType;

// --- A single, atomic edit action ---
typedef struct {
    ActionType  type;
    Position    position;       // where the edit occurred (before the edit)
    char        character;      // single-char payload (INSERT_CHAR / DELETE_CHAR)
    char       *text;           // heap-allocated multi-char payload (INSERT_TEXT)
                                // Owned by whoever holds the Action.
    Position    cursor_before;  // cursor position before this action
    Position    cursor_after;   // cursor position after this action
} Action;

// ---------------------------------------------------------------------------
// Vim-style undo tree
//
// Vim records history as a TREE, not a stack.  Every undo-able change group
// is a node.  Each node has:
//   - a parent (the state before this change group was applied)
//   - zero or more children (alternative futures produced by redo or new edits
//     made after undoing)
//   - a "next" sibling pointer that links children of the same parent together
//
// The "redo" operation always follows the most-recently-created child
// (matching Vim's default `u`/`Ctrl-R` behaviour).  Older branches remain
// reachable via the sequence number (g-/g+ equivalent) but that traversal is
// not implemented here – the infrastructure is in place for it.
//
// A "change group" corresponds to one Insert-mode session (or one Normal-mode
// command that modifies the buffer).  It is an ordered list of Actions.
// ---------------------------------------------------------------------------

// A node in the undo tree.  One node == one undoable change group.
typedef struct UndoNode {
    // Intrusive tree links
    struct UndoNode *parent;        // NULL for the root (initial state)
    struct UndoNode *children;      // linked list of child nodes (head)
    struct UndoNode *next_sibling;  // next child of our parent

    // Sequence number – monotonically increasing across the whole tree.
    // Lets us implement g-/g+ (travel to state N) in the future.
    uint64_t seq;

    // The actions that make up this change group, in application order.
    Action  *actions;
    size_t   count;
    size_t   capacity;

    // Cursor position saved at the moment this group was COMMITTED (i.e. the
    // cursor state the user sees after the edits, before any undo).
    Position cursor_after;

    // The most-recently-visited child – used so that Ctrl-R always goes back
    // to where the user was before the last `u`.
    struct UndoNode *last_child;
} UndoNode;

// The undo tree container held by each Tab.
typedef struct {
    UndoNode *root;    // sentinel root – the "empty buffer / just-opened" state
    UndoNode *current; // the node whose change group is CURRENTLY applied

    // The change group being accumulated in Insert mode.  NULL when no group
    // is open (i.e. we are in Normal mode and not accumulating edits).
    UndoNode *open_group;

    uint64_t  next_seq; // ever-increasing sequence counter
} UndoTree;

// ---------------------------------------------------------------------------
// UndoTree API
// ---------------------------------------------------------------------------

// Allocate and initialise an empty undo tree.
UndoTree *undo_tree_new(void);

// Free the entire tree and all its nodes/actions.
void      undo_tree_free(UndoTree *tree);

// --- Change-group management ---

// Open a new change group (call on entering Insert mode, or before any
// Normal-mode modifying command).  Does nothing if a group is already open.
void undo_tree_open_group(UndoTree *tree, Position cursor_before);

// Append a single action to the currently open group.
// The tree takes ownership of action.text.
// Asserts (no-op) if no group is open.
void undo_tree_push_action(UndoTree *tree, Action action);

// Close the current change group (call on leaving Insert mode, or after a
// Normal-mode modifying command).  If the group is empty it is discarded.
// cursor_after is the cursor position the user sees after all edits.
void undo_tree_close_group(UndoTree *tree, Position cursor_after);

// --- Undo / Redo ---

// Undo the most recent committed change group.
// Reverses each action in the group in reverse order.
// Returns true and writes the target cursor position to *cursor_out on
// success; returns false if there is nothing to undo.
bool undo_tree_undo(UndoTree *tree, Position *cursor_out);

// Redo the most-recently-undone change group (follows last_child pointer,
// matching Vim's Ctrl-R behaviour).
// Returns true and writes the target cursor position to *cursor_out on
// success; returns false if there is nothing to redo.
bool undo_tree_redo(UndoTree *tree, Position *cursor_out);

// --- Iteration helpers (used by history_undo / history_redo) ---

// Return the actions array of the node being undone/redone.
// Used so that editor_history.c can apply the actual buffer mutations.

// Returns the node that was just undone (parent of new current), or NULL.
const UndoNode *undo_tree_last_undone(const UndoTree *tree);

// Returns the node that was just redone (new current), or NULL.
const UndoNode *undo_tree_last_redone(const UndoTree *tree);

// ---------------------------------------------------------------------------
// Legacy ActionStack shim
//
// The rest of the codebase uses ActionStack / push_action / pop_action.
// We keep a minimal shim so that nothing outside editor_history.c needs to
// change.  New code should use UndoTree directly.
// ---------------------------------------------------------------------------

typedef struct {
    Action  *actions;
    size_t   size;
    size_t   capacity;
    size_t   max_capacity;
} ActionStack;

ActionStack *new_action_stack(size_t initial_capacity, size_t max_capacity);
bool         push_action(ActionStack *stack, Action action);
bool         pop_action (ActionStack *stack, Action *out_action);
bool         peek_action(const ActionStack *stack, Action *out_action);
bool         action_stack_is_empty(const ActionStack *stack);
void         reset_action_stack(ActionStack *stack);
void         free_action_stack(ActionStack *stack);

#endif // ACTION_STACK_H