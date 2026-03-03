#include "tab.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char *dup_path(const char *path) {
    if (!path) return NULL;
    size_t len = strlen(path);
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, path, len + 1);
    return copy;
}

Tab *tab_new_empty(void) {
    Tab *t = malloc(sizeof(Tab));
    if (!t) return NULL;
    t->buf      = newBuf();
    t->history  = new_editor_history();
    t->filepath = NULL;
    t->dirty    = false;
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

void tabInsertChar(Tab *t, int row, int col, char c) {
    if (!t || row < 0 || row >= t->buf->numrows) return;
    Action a;
    a.type           = INSERT_CHAR;
    a.position.row   = row;
    a.position.col   = col;
    a.character      = c;
    a.text           = NULL;
    history_record(t->history, a);
    insertChar(&t->buf->rows[row], col, c);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;
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
    history_record(t->history, a);
    deleteChar(t->buf, row, col);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;
}

void tabInsertCR(Tab *t, int row, int col) {
    if (!t || row < 0 || row >= t->buf->numrows) return;
    Action a;
    a.type           = INSERT_CR;
    a.position.row   = row;
    a.position.col   = col;
    a.character      = 0;
    a.text           = NULL;
    history_record(t->history, a);
    insertCR(t->buf, row, col);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;
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
    history_record(t->history, a);
    deleteCR(t->buf, row);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;
}

Position tabInsertText(Tab *t, int row, int col, const char *text) {
    Position start = { row, col };
    if (!t || !text || row < 0 || row >= t->buf->numrows) return start;

    // Early return if string is empty
    if (text[0] == '\0') return start;

    size_t len = strlen(text);
    char *text_copy = malloc(len + 1);
    if (!text_copy) return start;
    memcpy(text_copy, text, len + 1);

    Action a;
    a.type = INSERT_TEXT;
    a.position.row = row;
    a.position.col = col;
    a.character = 0;
    a.text = text_copy;
    history_record(t->history, a);

    Position end = insertText(t->buf, row, col, text);
    cursor_clamp(&t->cursor, t->buf);
    t->dirty = true;

    return end;
}

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