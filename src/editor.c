#include "editor.h"
#include <stdlib.h>

// ------------------------------------------------------------
// Construction / Destruction
// ------------------------------------------------------------

Editor *editor_new_empty(void) {
    Editor *e = malloc(sizeof(Editor));
    if (!e) return NULL;

    e->buf = newBuf();
    e->history = new_editor_history();

    if (!e->buf || !e->history) {
        editor_free(e);
        return NULL;
    }
    return e;
}

Editor *editor_new_from_file(FILE *f) {
    Editor *e = malloc(sizeof(Editor));
    if (!e) return NULL;

    e->buf = fileToBuf(f);
    e->history = new_editor_history();

    if (!e->buf || !e->history) {
        editor_free(e);
        return NULL;
    }
    return e;
}

void editor_free(Editor *e) {
    if (!e) return;
    free_editor_history(e->history);
    freeBuf(e->buf);
    free(e);
}

// ------------------------------------------------------------
// High-level editing API
// ------------------------------------------------------------

void editorInsertChar(Editor *e, int row, int col, char c) {
    Action a = {
        .type = INSERT_CHAR,
        .position = { row, col },
        .character = c,
        .text = NULL
    };
    history_record(e->history, a);
    insertChar(&e->buf->rows[row], col, c);
}

void editorDeleteChar(Editor *e, int row, int col) {
    char deleted = e->buf->rows[row].line[col];

    Action a = {
        .type = DELETE_CHAR,
        .position = { row, col },
        .character = deleted,
        .text = NULL
    };
    history_record(e->history, a);
    deleteChar(e->buf, row, col);
}

void editorInsertCR(Editor *e, int row, int col) {
    Action a = {
        .type = INSERT_CR,
        .position = { row, col },
        .character = 0,
        .text = NULL
    };
    history_record(e->history, a);
    insertCR(e->buf, row, col);
}

// void editorDeleteCR(Editor *e, int row) {
//     int col = e->buf->rows[row - 1].length;

//     Action a = {
//         .type = DELETE_CR,
//         .position = { row, col },
//         .character = 0,
//         .text = NULL
//     };
//     history_record(e->history, a);
//     deleteCR(e->buf, row);
// }

void editorDeleteCR(Editor *e, int row) {
    int split_row = row - 1;
    int split_col = e->buf->rows[split_row].length;

    Action a = {
        .type = DELETE_CR,
        .position = { split_row, split_col },  // FIXED
        .character = 0,
        .text = NULL
    };

    history_record(e->history, a);

    deleteCR(e->buf, row);
}

// ------------------------------------------------------------
// Undo / Redo
// ------------------------------------------------------------

bool editorUndo(Editor *e) {
    return history_undo(e->history, e->buf);
}

bool editorRedo(Editor *e) {
    return history_redo(e->history, e->buf);
}

// ------------------------------------------------------------
// Convenience
// ------------------------------------------------------------

void editorPrint(Editor *e) {
    printBuf(e->buf);
}
