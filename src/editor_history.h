#ifndef EDITOR_HISTORY_H
#define EDITOR_HISTORY_H

#include <stdbool.h>
#include "action_stack.h"
#include "buffer.h"
#include "editor_cursor.h"

// ---------------------------------------------------------------------------
// EditorHistory – Vim-compatible undo/redo
//
// Vim's undo model has two important properties that differ from a naive
// linear stack:
//
//  1. CHANGE GROUPS
//     All edits made during a single Insert-mode session are grouped into one
//     undoable unit.  Pressing `u` undoes the entire group at once; pressing
//     `u` again undoes the group before that.  A new group begins each time
//     the user enters Insert mode and ends each time they return to Normal
//     mode (or execute a Normal-mode command that modifies the buffer).
//
//  2. UNDO TREE
//     Rather than erasing redo history when a new edit is made, Vim records
//     every state as a node in a tree.  The default Ctrl-R always follows the
//     most-recently-created branch, reproducing familiar linear behaviour.
//     Older branches remain accessible via g-/g+ (future work).
//
// This struct wraps UndoTree and exposes the mode-transition API that the
// rest of the editor calls.
// ---------------------------------------------------------------------------

typedef struct {
    UndoTree *tree;
} EditorHistory;

// --- Lifecycle ---

EditorHistory *new_editor_history(void);
void           free_editor_history(EditorHistory *h);

// --- Mode transitions (must be called by input layer) ---

// Call when the editor enters Insert mode (or any state that will produce
// buffered edits).  Opens a new change group.
// cursor_before: cursor position at the moment of mode entry.
void history_begin_group(EditorHistory *h, Position cursor_before);

// Call when the editor leaves Insert mode (returns to Normal mode).
// Commits the open group, even if empty (empty groups are discarded).
// cursor_after: cursor position at the moment of mode exit.
void history_end_group(EditorHistory *h, Position cursor_after);

// --- Record a single action (called by tab.c helpers) ---
//
// The action is appended to the currently open change group.
// If no group is open (Normal-mode single-command edits such as `x`, `r`,
// `~`) a single-action group is opened and immediately closed, making each
// command its own undoable step.
// The EditorHistory takes ownership of action.text.
void history_record(EditorHistory *h, Action a, Position cursor_after);

// --- Undo / Redo ---
//
// Both functions apply buffer mutations directly and update the cursor.
// They return true if something changed, false if nothing to undo/redo.

bool history_undo(EditorHistory *h, buffer *buf, EditorCursor *c);
bool history_redo(EditorHistory *h, buffer *buf, EditorCursor *c);

#endif // EDITOR_HISTORY_H