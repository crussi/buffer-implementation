#include "editor_history.h"
#include "editor_cursor.h" 
#include <stdlib.h>

EditorHistory *new_editor_history(void) {
    EditorHistory *h = malloc(sizeof(EditorHistory));
    if (!h) return NULL;

    h->undo_stack = new_action_stack(0, 0);
    h->redo_stack = new_action_stack(0, 0);

    if (!h->undo_stack || !h->redo_stack) {
        free_editor_history(h);
        return NULL;
    }

    return h;
}

void history_record(EditorHistory *h, Action a) {
    if (!h) return;
    reset_action_stack(h->redo_stack);
    push_action(h->undo_stack, a);
}

bool history_undo(EditorHistory *h, buffer *buf, EditorCursor *c) {
    if (!h || !buf) return false;

    Action a;
    if (!pop_action(h->undo_stack, &a)) return false;

    switch (a.type) {
        case INSERT_CHAR:
            // INSERT_CHAR was recorded before inserting, so we delete it.
            deleteChar(buf, a.position.row, a.position.col);
            break;

        case DELETE_CHAR:
            // DELETE_CHAR was recorded with the deleted character saved, so
            // we restore it by inserting it back at the same position.
            insertChar(&buf->rows[a.position.row], a.position.col, a.character);
            break;

        case INSERT_CR:
            // INSERT_CR split a row at (row, col), creating row+1.
            // Undo by merging row+1 back into row.
            deleteCR(buf, a.position.row + 1);
            break;

        case DELETE_CR:
            // DELETE_CR merged row into row-1 at position col (the old
            // length of row-1). Undo by splitting at that same point.
            insertCR(buf, a.position.row, a.position.col);
            break;
    }
    if (c) {
        c->pos         = a.position;
        c->desired_col = a.position.col;
        cursor_clamp(c, buf);
    }
    // Push the inverse onto the redo stack so it can be re-applied.
    push_action(h->redo_stack, a);
    return true;
}

bool history_redo(EditorHistory *h, buffer *buf, EditorCursor *c) {
    if (!h || !buf) return false;

    Action a;
    if (!pop_action(h->redo_stack, &a)) return false;

    switch (a.type) {
        case INSERT_CHAR:
            insertChar(&buf->rows[a.position.row], a.position.col, a.character);
            break;

        case DELETE_CHAR:
            deleteChar(buf, a.position.row, a.position.col);
            break;

        case INSERT_CR:
            insertCR(buf, a.position.row, a.position.col);
            break;

        case DELETE_CR:
            deleteCR(buf, a.position.row + 1);
            break;
    }

    if (c) {
        c->pos         = a.position;
        c->desired_col = a.position.col;
        cursor_clamp(c, buf);
    }
    // Push back onto the undo stack so it can be undone again.
    push_action(h->undo_stack, a);
    return true;
}

void free_editor_history(EditorHistory *h) {
    if (!h) return;
    free_action_stack(h->undo_stack);
    free_action_stack(h->redo_stack);
    free(h);
}