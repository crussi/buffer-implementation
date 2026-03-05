// tab.c

#include "tab.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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
    Tab *t = calloc(1, sizeof(Tab));
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
    Tab *t = calloc(1, sizeof(Tab));
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
    free(t->yank_buf);
    free(t);
}

// ---------------------------------------------------------------------------
// Mode transitions
// ---------------------------------------------------------------------------

void tab_enter_insert_mode(Tab *t) {
    if (!t || t->mode == MODE_INSERT) return;
    t->mode = MODE_INSERT;
    t->pending_op   = '\0';
    t->repeat_count = 0;
    history_begin_group(t->history, t->cursor.pos);
}

void tab_leave_insert_mode(Tab *t) {
    if (!t || t->mode != MODE_INSERT) return;
    // Vim moves cursor one position left on leaving Insert mode.
    if (t->cursor.pos.col > 0)
        t->cursor.pos.col--;
    t->cursor.desired_col = t->cursor.pos.col;
    t->mode = MODE_NORMAL;
    history_end_group(t->history, t->cursor.pos);
    // Refresh dirty flag from tree.
    t->dirty = undo_tree_is_dirty(t->history->tree);
}

void tab_enter_visual_mode(Tab *t) {
    if (!t) return;
    if (t->mode == MODE_INSERT) tab_leave_insert_mode(t);
    t->mode = MODE_VISUAL;
    t->pending_op   = '\0';
    t->repeat_count = 0;
    cursor_start_selection(&t->cursor);
}

void tab_enter_visual_line_mode(Tab *t) {
    if (!t) return;
    if (t->mode == MODE_INSERT) tab_leave_insert_mode(t);
    t->mode = MODE_VISUAL_LINE;
    t->pending_op   = '\0';
    t->repeat_count = 0;
    cursor_start_selection(&t->cursor);
}

void tab_enter_replace_mode(Tab *t) {
    if (!t) return;
    if (t->mode == MODE_INSERT) tab_leave_insert_mode(t);
    t->mode = MODE_REPLACE;
    t->pending_op   = '\0';
    t->repeat_count = 0;
    history_begin_group(t->history, t->cursor.pos);
}

void tab_enter_command_mode(Tab *t) {
    if (!t) return;
    if (t->mode == MODE_INSERT) tab_leave_insert_mode(t);
    t->mode    = MODE_COMMAND;
    t->cmd_buf[0] = '\0';
    t->cmd_len    = 0;
    t->pending_op   = '\0';
    t->repeat_count = 0;
}

void tab_enter_normal_mode(Tab *t) {
    if (!t) return;
    switch (t->mode) {
        case MODE_INSERT:
            tab_leave_insert_mode(t);
            break;
        case MODE_REPLACE:
            // Mirror tab_leave_insert_mode col-- for Replace mode.
            if (t->cursor.pos.col > 0)
                t->cursor.pos.col--;
            t->cursor.desired_col = t->cursor.pos.col;
            t->mode = MODE_NORMAL;
            history_end_group(t->history, t->cursor.pos);
            t->dirty = undo_tree_is_dirty(t->history->tree);
            break;
        case MODE_VISUAL:
        case MODE_VISUAL_LINE:
            cursor_clear_selection(&t->cursor);
            t->mode = MODE_NORMAL;
            break;
        case MODE_COMMAND:
            t->mode = MODE_NORMAL;
            t->cmd_buf[0] = '\0';
            t->cmd_len    = 0;
            break;
        default:
            break;
    }
    t->pending_op   = '\0';
    t->repeat_count = 0;
}

// ---------------------------------------------------------------------------
// Editing operations
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

    insertChar(&t->buf->rows[row], col, c);
    cursor_clamp(&t->cursor, t->buf);

    a.cursor_after = t->cursor.pos;
    history_record(t->history, a, a.cursor_after);
    t->dirty = undo_tree_is_dirty(t->history->tree);
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

    a.cursor_after = t->cursor.pos;
    history_record(t->history, a, a.cursor_after);
    t->dirty = undo_tree_is_dirty(t->history->tree);
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

    a.cursor_after = t->cursor.pos;
    history_record(t->history, a, a.cursor_after);
    t->dirty = undo_tree_is_dirty(t->history->tree);
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

    a.cursor_after = t->cursor.pos;
    history_record(t->history, a, a.cursor_after);
    t->dirty = undo_tree_is_dirty(t->history->tree);
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
    a.text           = text_copy;
    a.cursor_before  = t->cursor.pos;

    Position end = insertText(t->buf, row, col, text);
    cursor_clamp(&t->cursor, t->buf);

    a.cursor_after = end;
    history_record(t->history, a, end);
    t->dirty = undo_tree_is_dirty(t->history->tree);

    return end;
}

// ---------------------------------------------------------------------------
// Delete range (used by Visual-mode 'd' and operator+motion)
// ---------------------------------------------------------------------------

// Build a string from start..end (inclusive, character-wise) in the buffer.
// Caller must free() the result.
static char *extract_range(buffer *buf, Position start, Position end) {
    if (!buf) return NULL;

    // Compute total length
    size_t total = 0;
    for (int r = start.row; r <= end.row && r < buf->numrows; r++) {
        int cs = (r == start.row) ? start.col : 0;
        int ce = (r == end.row)   ? end.col   : buf->rows[r].length - 1;
        if (ce >= buf->rows[r].length) ce = buf->rows[r].length - 1;
        if (ce >= cs) total += (size_t)(ce - cs + 1);
        if (r < end.row) total++; // newline
    }

    char *s = malloc(total + 1);
    if (!s) return NULL;
    size_t pos = 0;
    for (int r = start.row; r <= end.row && r < buf->numrows; r++) {
        int cs = (r == start.row) ? start.col : 0;
        int ce = (r == end.row)   ? end.col   : buf->rows[r].length - 1;
        if (ce >= buf->rows[r].length) ce = buf->rows[r].length - 1;
        for (int c = cs; c <= ce; c++)
            s[pos++] = buf->rows[r].line[c];
        if (r < end.row) s[pos++] = '\n';
    }
    s[pos] = '\0';
    return s;
}

void tabDeleteRange(Tab *t, Position start, Position end) {
    if (!t) return;
    // Normalise
    if (start.row > end.row || (start.row == end.row && start.col > end.col)) {
        Position tmp = start; start = end; end = tmp;
    }

    // Open a group so the whole delete is one undo step.
    bool auto_group = (t->history->tree->open_group == NULL);
    if (auto_group) history_begin_group(t->history, t->cursor.pos);

    // Delete character by character: always delete at 'start' position since
    // the buffer shifts after each deletion.
    int rows_to_process = end.row - start.row + 1;
    for (int ri = 0; ri < rows_to_process; ri++) {
        int row = start.row;
        if (row >= t->buf->numrows) break;

        int col_start = (ri == 0)                    ? start.col : 0;
        int col_end   = (ri == rows_to_process - 1)  ? end.col
                                                     : t->buf->rows[row].length - 1;

        // Delete chars in this row from right to left to keep indices stable.
        for (int col = col_end; col >= col_start; col--) {
            if (col < t->buf->rows[row].length)
                tabDeleteChar(t, row, col);
        }

        // If not the last row, merge this row with the next (delete the newline).
        if (ri < rows_to_process - 1 && start.row + 1 < t->buf->numrows)
            tabDeleteCR(t, start.row + 1);
    }

    t->cursor.pos = start;
    cursor_clamp(&t->cursor, t->buf);

    if (auto_group) history_end_group(t->history, t->cursor.pos);
    t->dirty = undo_tree_is_dirty(t->history->tree);
}

void tabDeleteLine(Tab *t, int row) {
    if (!t || !t->buf || row < 0 || row >= t->buf->numrows) return;

    bool auto_group = (t->history->tree->open_group == NULL);
    if (auto_group) history_begin_group(t->history, t->cursor.pos);

    // Delete all characters on the line.
    while (t->buf->rows[row].length > 0)
        tabDeleteChar(t, row, 0);

    // If there is more than one row, delete the newline (merge with next or prev).
    if (t->buf->numrows > 1) {
        if (row + 1 < t->buf->numrows)
            tabDeleteCR(t, row + 1);   // merge next into this (now empty) row
        else
            tabDeleteCR(t, row);       // last row: merge into previous
    }

    // Clamp cursor
    if (t->cursor.pos.row >= t->buf->numrows)
        t->cursor.pos.row = t->buf->numrows - 1;
    t->cursor.pos.col     = 0;
    t->cursor.desired_col = 0;
    cursor_clamp(&t->cursor, t->buf);

    if (auto_group) history_end_group(t->history, t->cursor.pos);
    t->dirty = undo_tree_is_dirty(t->history->tree);
}

// ---------------------------------------------------------------------------
// Yank / Put
// ---------------------------------------------------------------------------

void tab_yank_range(Tab *t, Position start, Position end, bool line_wise) {
    if (!t) return;
    if (start.row > end.row || (start.row == end.row && start.col > end.col)) {
        Position tmp = start; start = end; end = tmp;
    }
    free(t->yank_buf);
    t->yank_buf     = extract_range(t->buf, start, end);
    t->yank_is_line = line_wise;
}

void tab_yank_line(Tab *t, int row) {
    if (!t || !t->buf || row < 0 || row >= t->buf->numrows) return;
    free(t->yank_buf);
    const char *line = t->buf->rows[row].line;
    int   len  = t->buf->rows[row].length;
    // Yank including the implicit newline so put works correctly.
    char *buf = malloc((size_t)len + 2);
    if (!buf) return;
    memcpy(buf, line, (size_t)len);
    buf[len]   = '\n';
    buf[len+1] = '\0';
    t->yank_buf     = buf;
    t->yank_is_line = true;
}

void tab_put_after(Tab *t) {
    if (!t || !t->yank_buf) return;
    if (t->yank_is_line) {
        // Line-wise put: insert a new line BELOW current.
        int row = t->cursor.pos.row;
        int end = t->buf->rows[row].length;
        tabInsertCR(t, row, end);
        t->cursor.pos.row++;
        t->cursor.pos.col = 0;
        // Insert the yanked text (strip trailing newline).
        size_t len = strlen(t->yank_buf);
        char *text = malloc(len + 1);
        if (!text) return;
        memcpy(text, t->yank_buf, len + 1);
        if (len > 0 && text[len-1] == '\n') text[len-1] = '\0';
        tabInsertText(t, t->cursor.pos.row, 0, text);
        free(text);
    } else {
        // Character-wise put: insert after cursor.
        int row = t->cursor.pos.row;
        int col = t->cursor.pos.col + 1;
        if (col > t->buf->rows[row].length) col = t->buf->rows[row].length;
        Position end = tabInsertText(t, row, col, t->yank_buf);
        t->cursor.pos = end;
        if (t->cursor.pos.col > 0) t->cursor.pos.col--;
        cursor_clamp(&t->cursor, t->buf);
    }
}

void tab_put_before(Tab *t) {
    if (!t || !t->yank_buf) return;
    if (t->yank_is_line) {
        // Line-wise put: insert a new line ABOVE current.
        int row = t->cursor.pos.row;
        tabInsertCR(t, row, 0);
        t->cursor.pos.col = 0;
        size_t len = strlen(t->yank_buf);
        char *text = malloc(len + 1);
        if (!text) return;
        memcpy(text, t->yank_buf, len + 1);
        if (len > 0 && text[len-1] == '\n') text[len-1] = '\0';
        tabInsertText(t, row, 0, text);
        free(text);
    } else {
        // Character-wise put: insert before cursor.
        int row = t->cursor.pos.row;
        int col = t->cursor.pos.col;
        Position end = tabInsertText(t, row, col, t->yank_buf);
        t->cursor.pos = end;
        if (t->cursor.pos.col > 0) t->cursor.pos.col--;
        cursor_clamp(&t->cursor, t->buf);
    }
}

// ---------------------------------------------------------------------------
// Undo / Redo
// ---------------------------------------------------------------------------

bool tabUndo(Tab *t) {
    if (!t) return false;
    // history_undo handles closing any open group (mid-Insert-mode undo).
    // tab_leave_insert_mode must NOT be called here: history_undo already
    // performs the col-- adjustment internally when closing an open group.
    if (t->mode == MODE_INSERT) {
        t->mode = MODE_NORMAL;   // switch mode without closing the group;
                                 // history_undo closes it with the col-- fix.
    }
    bool result = history_undo(t->history, t->buf, &t->cursor);
    // Reflect the true dirty state from the undo tree.
    t->dirty = undo_tree_is_dirty(t->history->tree);
    return result;
}

bool tabRedo(Tab *t) {
    if (!t) return false;
    if (t->mode == MODE_INSERT) return false;
    bool result = history_redo(t->history, t->buf, &t->cursor);
    t->dirty = undo_tree_is_dirty(t->history->tree);
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
    t->mode       = MODE_NORMAL;
    t->dirty      = false;
    t->scroll_top = 0; t->scroll_left = 0;
    t->cmd_buf[0] = '\0'; t->cmd_len = 0;
    t->pending_op = '\0'; t->repeat_count = 0;
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
    // Mark the undo tree's current position as the saved state so that
    // undo_tree_is_dirty() correctly returns false after saving, and true
    // again if the user makes further edits (or undoes back past this point).
    undo_tree_mark_saved(t->history->tree);
    t->dirty = false;
    return true;
}

void tabPrint(Tab *t) {
    if (!t) return;
    printBuf(t->buf);
}