#include "editor_history.h"
#include "editor_cursor.h"
#include <stdlib.h>
#include <string.h>

EditorHistory *new_editor_history(void) {
    EditorHistory *h = malloc(sizeof(EditorHistory));
    if (!h) return NULL;
    h->undo_stack = new_action_stack(0, 0);
    h->redo_stack = new_action_stack(0, 0);
    if (!h->undo_stack || !h->redo_stack) { free_editor_history(h); return NULL; }
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
            deleteChar(buf, a.position.row, a.position.col);
            break;
        case DELETE_CHAR:
            insertChar(&buf->rows[a.position.row], a.position.col, a.character);
            break;
        case INSERT_CR:
            deleteCR(buf, a.position.row + 1);
            break;
        case DELETE_CR:
            insertCR(buf, a.position.row, a.position.col);
            break;
        case INSERT_TEXT: {
            int len = a.text ? (int)strlen(a.text) : 0;
            deleteTextRange(buf, a.position, len);
            break;
        }
    }
    if (c) {
        c->pos         = a.position;
        c->desired_col = a.position.col;
        cursor_clamp(c, buf);
    }
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
        case INSERT_TEXT:
            insertText(buf, a.position.row, a.position.col, a.text ? a.text : "");
            break;
    }
    if (c) {
        c->pos         = a.position;
        c->desired_col = a.position.col;
        cursor_clamp(c, buf);
    }
    push_action(h->undo_stack, a);
    return true;
}

void free_editor_history(EditorHistory *h) {
    if (!h) return;
    free_action_stack(h->undo_stack);
    free_action_stack(h->redo_stack);
    free(h);
}