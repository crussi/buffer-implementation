#include "tab.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static char *dup_path(const char *path) {
    if (!path) return NULL;
    size_t len = strlen(path);
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, path, len + 1);
    return copy;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

Tab *tab_new_empty(void) {
    Tab *t = malloc(sizeof(Tab));
    if (!t) return NULL;
    t->buf      = newBuf();
    t->history  = new_editor_history();
    t->filepath = NULL;
    t->dirty    = false;
    t->mode     = MODE_NORMAL;
    if (!t->buf || !t->history) { tab_free(t); return NULL; }
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
    t->mode     = MODE_NORMAL;
    if (!t->buf || !t->history) { tab_free(t); return NULL; }
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

// ---------------------------------------------------------------------------
// Mode transitions
// ---------------------------------------------------------------------------
//
// Vim rule:
//   - Enter Insert mode  → open a new change group (history_begin_group)
//   - Leave Insert mode  → close the change group   (history_end_group)
//   - Normal-mode single commands (x, r, etc.) are each their own group;
//     tab.c handles this automatically via the auto_group path in
//     history_record() when no group is open.

void tab_enter_insert_mode(Tab *t) {
    if (!t || t->mode == MODE_INSERT) return;
    t->mode = MODE_INSERT;
    history_begin_group(t->history, t->cursor.pos);
}

void tab_leave_insert_mode(Tab *t) {
    if (!t || t->mode != MODE_INSERT) return;
    t->mode = MODE_NORMAL;
    history_end_group(t->history, t->cursor.pos);
    // Vim moves the cursor one position left when leaving Insert mode
    // (unless already at column 0).
    if (t->cursor.pos.col > 0)
        t->cursor.pos.col--;
    t->cursor.desired_col = t->cursor.pos.col;
}

void tab_enter_visual_mode(Tab *t) {
    if (!t) return;
    // Leave Insert first if needed.
    if (t->mode == MODE_INSERT) tab_leave_insert_mode(t);
    t->mode = MODE_VISUAL;
    cursor_start_selection(&t->cursor);
}

void tab_enter_replace_mode(Tab *t) {
    if (!t) return;
    if (t->mode == MODE_INSERT) tab_leave_insert_mode(t);
    t->mode = MODE_REPLACE;
    // Replace mode uses the same change-group mechanism as Insert.
    history_begin_group(t->history, t->cursor.pos);
}

void tab_enter_normal_mode(Tab *t) {
    if (!t) return;
    switch (t->mode) {
        case MODE_INSERT:  tab_leave_insert_mode(t);  break;
        case MODE_REPLACE:
            t->mode = MODE_NORMAL;
            history_end_group(t->history, t->cursor.pos);
            break;
        case MODE_VISUAL:
        case MODE_VISUAL_LINE:
            cursor_clear_selection(&t->cursor);
            t->mode = MODE_NORMAL;
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Editing operations
//
// These record an Action and then perform the buffer mutation.
// When called from Insert mode the action goes into the open group.
// When called from Normal mode history_record()'s auto_group path wraps
// each action in its own group automatically.
// ---------------------------------------------------------------------------

void tabInsertChar(Tab *t, int row, int col, char c) {
    if (!t || row < 0 || row >= t->buf->numrows) return;

    Action a;
    a.type           = INSERT_CHAR;
    a.position.row   = row;
    a.position.col   = col;
    a.character      = c;
    a.text           = NULL;
    a.cursor_before  = t->cursor.pos;

    // Perform the mutation first so cursor_after is accurate.
    insertChar(&t->buf->rows[row], col, c);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;

    a.cursor_after = t->cursor.pos;
    history_record(t->history, a, a.cursor_after);
}

void tabDeleteChar(Tab *t, int row, int col) {
    if (!t || row < 0 || row >= t->buf->numrows) return;
    if (col < 0 || col >= t->buf->rows[row].length) return;

    char deleted = t->buf->rows[row].line[col];

    Action a;
    a.type           = DELETE_CHAR;
    a.position.row   = row;
    a.position.col   = col;
    a.character      = deleted;
    a.text           = NULL;
    a.cursor_before  = t->cursor.pos;

    deleteChar(t->buf, row, col);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;

    a.cursor_after = t->cursor.pos;
    history_record(t->history, a, a.cursor_after);
}

void tabInsertCR(Tab *t, int row, int col) {
    if (!t || row < 0 || row >= t->buf->numrows) return;

    Action a;
    a.type           = INSERT_CR;
    a.position.row   = row;
    a.position.col   = col;
    a.character      = 0;
    a.text           = NULL;
    a.cursor_before  = t->cursor.pos;

    insertCR(t->buf, row, col);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;

    a.cursor_after = t->cursor.pos;
    history_record(t->history, a, a.cursor_after);
}

void tabDeleteCR(Tab *t, int row) {
    if (!t || row <= 0 || row >= t->buf->numrows) return;

    int split_row = row - 1;
    int split_col = t->buf->rows[split_row].length;

    Action a;
    a.type           = DELETE_CR;
    a.position.row   = split_row;
    a.position.col   = split_col;
    a.character      = 0;
    a.text           = NULL;
    a.cursor_before  = t->cursor.pos;

    deleteCR(t->buf, row);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;

    a.cursor_after = t->cursor.pos;
    history_record(t->history, a, a.cursor_after);
}

Position tabInsertText(Tab *t, int row, int col, const char *text) {
    Position start = { row, col };
    if (!t || !text || row < 0 || row >= t->buf->numrows) return start;
    if (text[0] == '\0') return start;

    size_t len = strlen(text);
    char *text_copy = malloc(len + 1);
    if (!text_copy) return start;
    memcpy(text_copy, text, len + 1);

    Action a;
    a.type           = INSERT_TEXT;
    a.position.row   = row;
    a.position.col   = col;
    a.character      = 0;
    a.text           = text_copy;   // ownership transfers to history
    a.cursor_before  = t->cursor.pos;

    Position end = insertText(t->buf, row, col, text);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;

    a.cursor_after = end;
    history_record(t->history, a, end);

    return end;
}

// ---------------------------------------------------------------------------
// Undo / Redo
// ---------------------------------------------------------------------------

bool tabUndo(Tab *t) {
    if (!t) return false;
    // If we are in Insert mode, leaving Insert mode IS the first undo step
    // (matching Vim: pressing `u` from Insert mode first ends the insert).
    if (t->mode == MODE_INSERT) {
        tab_leave_insert_mode(t);
        // In Vim, this alone counts as one undo step – the insertion is now
        // a committed group that history_undo will reverse.
    }
    bool result = history_undo(t->history, t->buf, &t->cursor);
    if (result) t->dirty = true;
    return result;
}

bool tabRedo(Tab *t) {
    if (!t) return false;
    // Cannot redo while inserting.
    if (t->mode == MODE_INSERT) return false;
    bool result = history_redo(t->history, t->buf, &t->cursor);
    if (result) t->dirty = true;
    return result;
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------

bool tab_open(Tab *t, const char *path) {
    if (!t || !path) return false;
    FILE *f = fopen(path, "r");
    if (!f) return false;
    buffer *new_buf = fileToBuf(f);
    fclose(f);
    if (!new_buf) return false;
    freeBuf(t->buf);
    t->buf = new_buf;
    free(t->filepath);
    t->filepath = dup_path(path);
    if (!t->filepath) return false;
    free_editor_history(t->history);
    t->history = new_editor_history();
    cursor_init(&t->cursor);
    t->mode  = MODE_NORMAL;
    t->dirty = false;
    return true;
}

bool tab_save(Tab *t) {
    if (!t || !t->filepath) return false;
    return tab_save_as(t, t->filepath);
}

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
    if (!t->filepath || strcmp(t->filepath, path) != 0) {
        free(t->filepath);
        t->filepath = dup_path(path);
        if (!t->filepath) return false;
    }
    t->dirty = false;
    return true;
}

void tabPrint(Tab *t) {
    if (!t) return;
    printBuf(t->buf);
}