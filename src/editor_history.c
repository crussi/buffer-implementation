#include "editor_history.h"
#include "editor_cursor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

EditorHistory *new_editor_history(void) {
    EditorHistory *h = malloc(sizeof(EditorHistory));
    if (!h) return NULL;
    h->tree = undo_tree_new();
    if (!h->tree) { free(h); return NULL; }
    return h;
}

void free_editor_history(EditorHistory *h) {
    if (!h) return;
    undo_tree_free(h->tree);
    free(h);
}

// ---------------------------------------------------------------------------
// Mode transitions
// ---------------------------------------------------------------------------

void history_begin_group(EditorHistory *h, Position cursor_before) {
    if (!h) return;
    undo_tree_open_group(h->tree, cursor_before);
}

void history_end_group(EditorHistory *h, Position cursor_after) {
    if (!h) return;
    undo_tree_close_group(h->tree, cursor_after);
}

// ---------------------------------------------------------------------------
// Recording individual actions
// ---------------------------------------------------------------------------

void history_record(EditorHistory *h, Action a, Position cursor_after) {
    if (!h) return;

    bool auto_group = (h->tree->open_group == NULL);
    if (auto_group) {
        // Safety net: Normal-mode single-command edits (e.g. `x`, `r`, `~`)
        // that don't wrap themselves in a group land here.  We open a group
        // using the action's own pre-edit cursor position as cursor_before.
        undo_tree_open_group(h->tree, a.cursor_before);
    }

    a.cursor_after = cursor_after;
    undo_tree_push_action(h->tree, a);

    if (auto_group) {
        undo_tree_close_group(h->tree, cursor_after);
    }
}

// ---------------------------------------------------------------------------
// Apply / reverse a single Action on the buffer
// ---------------------------------------------------------------------------

static void apply_action(const Action *a, buffer *buf) {
    if (!a || !buf) return;
    switch (a->type) {
        case INSERT_CHAR:
            if (a->position.row < buf->numrows)
                insertChar(&buf->rows[a->position.row],
                           a->position.col, a->character);
            break;
        case DELETE_CHAR:
            deleteChar(buf, a->position.row, a->position.col);
            break;
        case INSERT_CR:
            insertCR(buf, a->position.row, a->position.col);
            break;
        case DELETE_CR:
            deleteCR(buf, a->position.row + 1);
            break;
        case INSERT_TEXT:
            insertText(buf, a->position.row, a->position.col,
                       a->text ? a->text : "");
            break;
    }
}

static void reverse_action(const Action *a, buffer *buf) {
    if (!a || !buf) return;
    switch (a->type) {
        case INSERT_CHAR:
            deleteChar(buf, a->position.row, a->position.col);
            break;
        case DELETE_CHAR:
            if (a->position.row < buf->numrows)
                insertChar(&buf->rows[a->position.row],
                           a->position.col, a->character);
            break;
        case INSERT_CR:
            deleteCR(buf, a->position.row + 1);
            break;
        case DELETE_CR:
            insertCR(buf, a->position.row, a->position.col);
            break;
        case INSERT_TEXT: {
            int len = a->text ? (int)strlen(a->text) : 0;
            deleteTextRange(buf, a->position, len);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Undo
// ---------------------------------------------------------------------------

bool history_undo(EditorHistory *h, buffer *buf, EditorCursor *c) {
    if (!h || !buf) return false;

    // If there's an open group (user is mid-Insert-mode), close it first.
    // This matches Vim: pressing `u` in Insert mode first ends the insert,
    // then undoes it as a single group.
    //
    // We perform the cursor col-1 adjustment that tab_leave_insert_mode
    // normally handles, so the cursor is correct in the closed group.
    if (h->tree->open_group) {
        Position cur = c ? c->pos : (Position){0, 0};
        // Mirror the col-- that tab_leave_insert_mode applies.
        if (c && c->pos.col > 0) {
            cur.col--;
            c->pos.col--;
            c->desired_col = c->pos.col;
        }
        undo_tree_close_group(h->tree, cur);
    }

    Position cursor_out;
    if (!undo_tree_undo(h->tree, &cursor_out)) return false;

    // The node that was undone is recorded in current->last_undone_child.
    const UndoNode *undone = undo_tree_last_undone(h->tree);
    if (!undone) return false;

    // Reverse actions in REVERSE order.
    for (int i = (int)undone->count - 1; i >= 0; i--)
        reverse_action(&undone->actions[i], buf);

    if (c) {
        c->pos         = cursor_out;
        c->desired_col = cursor_out.col;
        cursor_clamp(c, buf);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Redo
// ---------------------------------------------------------------------------

bool history_redo(EditorHistory *h, buffer *buf, EditorCursor *c) {
    if (!h || !buf) return false;

    // Cannot redo while a group is open.
    if (h->tree->open_group) return false;

    Position cursor_out;
    if (!undo_tree_redo(h->tree, &cursor_out)) return false;

    // The node that was redone is now tree->current.
    const UndoNode *redone = undo_tree_last_redone(h->tree);
    if (!redone) return false;

    // Re-apply actions in FORWARD order.
    for (size_t i = 0; i < redone->count; i++)
        apply_action(&redone->actions[i], buf);

    if (c) {
        c->pos         = cursor_out;
        c->desired_col = cursor_out.col;
        cursor_clamp(c, buf);
    }
    return true;
}