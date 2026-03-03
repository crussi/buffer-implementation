#include "tab.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ------------------------------------------------------------
// Internal helper
// ------------------------------------------------------------

// Duplicate a string onto the heap. Returns NULL on alloc failure.
static char *dup_path(const char *path) {
    if (!path) return NULL;
    size_t len = strlen(path);
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, path, len + 1);
    return copy;
}

// ------------------------------------------------------------
// Construction / Destruction
// ------------------------------------------------------------

Tab *tab_new_empty(void) {
    Tab *t = malloc(sizeof(Tab));
    if (!t) return NULL;

    t->buf      = newBuf();
    t->history  = new_editor_history();
    t->filepath = NULL;
    t->dirty    = false;

    if (!t->buf || !t->history) {
        tab_free(t);
        return NULL;
    }

    cursor_init(&t->cursor);
    return t;
}

Tab *tab_new_from_file(FILE *f) {
    if (!f) return NULL;

    Tab *t = malloc(sizeof(Tab));
    if (!t) return NULL;

    t->buf      = fileToBuf(f);
    t->history  = new_editor_history();
    t->filepath = NULL;
    t->dirty    = false;

    if (!t->buf || !t->history) {
        tab_free(t);
        return NULL;
    }

    cursor_init(&t->cursor);
    return t;
}

void tab_free(Tab *t) {
    if (!t) return;
    free_editor_history(t->history);
    freeBuf(t->buf);
    free(t->filepath);
    free(t);
}

// ------------------------------------------------------------
// High-level editing API
// All edit functions set dirty = true after modifying the buffer.
// ------------------------------------------------------------

void tabInsertChar(Tab *t, int row, int col, char c) {
    if (!t || row < 0 || row >= t->buf->numrows) return;
    Action a = {
        .type      = INSERT_CHAR,
        .position  = { row, col },
        .character = c,
        .text      = NULL
    };
    history_record(t->history, a);
    insertChar(&t->buf->rows[row], col, c);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;
}

void tabDeleteChar(Tab *t, int row, int col) {
    if (!t || row < 0 || row >= t->buf->numrows) return;
    if (col < 0 || col >= t->buf->rows[row].length) return;
    char deleted = t->buf->rows[row].line[col];

    Action a = {
        .type      = DELETE_CHAR,
        .position  = { row, col },
        .character = deleted,
        .text      = NULL
    };
    history_record(t->history, a);
    deleteChar(t->buf, row, col);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;
}

void tabInsertCR(Tab *t, int row, int col) {
    if (!t || row < 0 || row >= t->buf->numrows) return;
    Action a = {
        .type      = INSERT_CR,
        .position  = { row, col },
        .character = 0,
        .text      = NULL
    };
    history_record(t->history, a);
    insertCR(t->buf, row, col);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;
}

void tabDeleteCR(Tab *t, int row) {
    if (!t || row <= 0 || row >= t->buf->numrows) return;
    int split_row = row - 1;
    int split_col = t->buf->rows[split_row].length;

    Action a = {
        .type      = DELETE_CR,
        .position  = { split_row, split_col },
        .character = 0,
        .text      = NULL
    };
    history_record(t->history, a);
    deleteCR(t->buf, row);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;
}

// ------------------------------------------------------------
// Undo / Redo
// Undo/redo modify the buffer so they also set dirty = true.
// The only time dirty becomes false is after a successful save.
// ------------------------------------------------------------

bool tabUndo(Tab *t) {
    if (!t) return false;
    bool result = history_undo(t->history, t->buf, &t->cursor);
    if (result) t->dirty = true;
    return result;
}

bool tabRedo(Tab *t) {
    if (!t) return false;
    bool result = history_redo(t->history, t->buf, &t->cursor);
    if (result) t->dirty = true;
    return result;
}

// ------------------------------------------------------------
// Save / load
// ------------------------------------------------------------

// Open a file by path: replace this tab's buffer with the file's
// contents, update filepath, and clear dirty + history.
bool tab_open(Tab *t, const char *path) {
    if (!t || !path) return false;

    FILE *f = fopen(path, "r");
    if (!f) return false;

    buffer *new_buf = fileToBuf(f);
    fclose(f);
    if (!new_buf) return false;

    // Replace buffer and reset all derived state
    freeBuf(t->buf);
    t->buf = new_buf;

    free(t->filepath);
    t->filepath = dup_path(path);
    if (!t->filepath) return false;   // path copy failed but buf is valid

    // Reset history and cursor so they don't reference stale positions
    free_editor_history(t->history);
    t->history = new_editor_history();
    cursor_init(&t->cursor);
    t->dirty = false;

    return true;
}

// Save to the tab's own filepath. Returns false if filepath is NULL
// (tab has never been saved -- caller should use tab_save_as instead).
bool tab_save(Tab *t) {
    if (!t || !t->filepath) return false;
    return tab_save_as(t, t->filepath);
}

// Save to an explicit path. Updates filepath on success.
bool tab_save_as(Tab *t, const char *path) {
    if (!t || !path) return false;

    FILE *f = fopen(path, "w");
    if (!f) return false;

    for (int i = 0; i < t->buf->numrows; i++) {
        fwrite(t->buf->rows[i].line, 1, t->buf->rows[i].length, f);
        fputc('\n', f);
    }

    int flush_ok = fflush(f);
    fclose(f);
    if (flush_ok != 0) return false;

    // Update filepath if saving to a new location
    if (!t->filepath || strcmp(t->filepath, path) != 0) {
        free(t->filepath);
        t->filepath = dup_path(path);
        if (!t->filepath) return false;
    }

    t->dirty = false;
    return true;
}

// ------------------------------------------------------------
// Convenience
// ------------------------------------------------------------

void tabPrint(Tab *t) {
    if (!t) return;
    printBuf(t->buf);
}