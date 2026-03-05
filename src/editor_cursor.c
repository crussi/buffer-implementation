// editor_cursor.c

#include "editor_cursor.h"
#include <ctype.h>

void cursor_init(EditorCursor *c) {
    if (!c) return;
    c->pos.row     = 0;
    c->pos.col     = 0;
    c->anchor.row  = 0;
    c->anchor.col  = 0;
    c->selecting   = false;
    c->desired_col = 0;
}

// General clamp: col may equal line_len (Insert-mode end position).
void cursor_clamp(EditorCursor *c, buffer *buf) {
    if (!c || !buf) return;
    if (c->pos.row < 0)             c->pos.row = 0;
    if (c->pos.row >= buf->numrows) c->pos.row = buf->numrows - 1;
    int line_len = buf->rows[c->pos.row].length;
    if (c->pos.col < 0)        c->pos.col = 0;
    if (c->pos.col > line_len) c->pos.col = line_len;
}

// Normal-mode clamp: col is at most len-1 (cursor cannot rest on newline).
void cursor_clamp_normal(EditorCursor *c, buffer *buf) {
    if (!c || !buf) return;
    cursor_clamp(c, buf);
    int line_len = buf->rows[c->pos.row].length;
    int max_col  = line_len > 0 ? line_len - 1 : 0;
    if (c->pos.col > max_col) c->pos.col = max_col;
}

// ---------------------------------------------------------------------------
// Basic movement (Insert / Visual – col may equal line_len)
// ---------------------------------------------------------------------------

void cursor_move_left(EditorCursor *c, buffer *buf) {
    if (!c || !buf) return;
    if (c->pos.col > 0) {
        c->pos.col--;
    } else if (c->pos.row > 0) {
        c->pos.row--;
        c->pos.col = buf->rows[c->pos.row].length;
    }
    c->desired_col = c->pos.col;
}

void cursor_move_right(EditorCursor *c, buffer *buf) {
    if (!c || !buf) return;
    int line_len = buf->rows[c->pos.row].length;
    if (c->pos.col < line_len) {
        c->pos.col++;
    } else if (c->pos.row < buf->numrows - 1) {
        c->pos.row++;
        c->pos.col = 0;
    }
    c->desired_col = c->pos.col;
}

void cursor_move_up(EditorCursor *c, buffer *buf) {
    if (!c || !buf || c->pos.row == 0) return;
    c->pos.row--;
    int line_len = buf->rows[c->pos.row].length;
    c->pos.col = (c->desired_col <= line_len) ? c->desired_col : line_len;
}

void cursor_move_down(EditorCursor *c, buffer *buf) {
    if (!c || !buf || c->pos.row >= buf->numrows - 1) return;
    c->pos.row++;
    int line_len = buf->rows[c->pos.row].length;
    c->pos.col = (c->desired_col <= line_len) ? c->desired_col : line_len;
}

// ---------------------------------------------------------------------------
// Normal-mode movement (col capped at len-1)
// ---------------------------------------------------------------------------

void cursor_move_left_normal(EditorCursor *c, buffer *buf) {
    if (!c || !buf) return;
    if (c->pos.col > 0) c->pos.col--;
    // Does NOT wrap to previous line (Vim `h` stays on same line).
    c->desired_col = c->pos.col;
}

void cursor_move_right_normal(EditorCursor *c, buffer *buf) {
    if (!c || !buf) return;
    int line_len = buf->rows[c->pos.row].length;
    int max_col  = line_len > 0 ? line_len - 1 : 0;
    if (c->pos.col < max_col) c->pos.col++;
    // Does NOT wrap to next line (Vim `l` stays on same line).
    c->desired_col = c->pos.col;
}

// ---------------------------------------------------------------------------
// Word motions
// ---------------------------------------------------------------------------

// Helper: is char a "word character" (alphanumeric or underscore)?
static bool is_word_char(char ch) {
    return isalnum((unsigned char)ch) || ch == '_';
}

// w – move to start of next word.
void cursor_move_word_forward(EditorCursor *c, buffer *buf) {
    if (!c || !buf) return;

    int row = c->pos.row;
    int col = c->pos.col;
    int len = buf->rows[row].length;

    // Advance past current word/punctuation group.
    if (col < len) {
        if (is_word_char(buf->rows[row].line[col])) {
            while (col < len && is_word_char(buf->rows[row].line[col])) col++;
        } else if (!isspace((unsigned char)buf->rows[row].line[col])) {
            while (col < len && !is_word_char(buf->rows[row].line[col])
                              && !isspace((unsigned char)buf->rows[row].line[col])) col++;
        }
    }

    // Skip whitespace / newlines between words.
    while (1) {
        while (col < len && isspace((unsigned char)buf->rows[row].line[col])) col++;
        if (col < len) break;
        // Move to next row
        if (row + 1 >= buf->numrows) { col = len > 0 ? len - 1 : 0; break; }
        row++;
        col = 0;
        len = buf->rows[row].length;
    }

    c->pos.row     = row;
    c->pos.col     = col < len ? col : (len > 0 ? len - 1 : 0);
    c->desired_col = c->pos.col;
}

// b – move to start of previous word.
void cursor_move_word_backward(EditorCursor *c, buffer *buf) {
    if (!c || !buf) return;

    int row = c->pos.row;
    int col = c->pos.col;

    // Step back one position first.
    if (col > 0) {
        col--;
    } else {
        if (row == 0) return;
        row--;
        col = buf->rows[row].length > 0 ? buf->rows[row].length - 1 : 0;
    }

    int len = buf->rows[row].length;

    // Skip whitespace going backwards.
    while (row >= 0) {
        while (col >= 0 && isspace((unsigned char)(col < len ? buf->rows[row].line[col] : ' '))) col--;
        if (col >= 0) break;
        if (row == 0) { col = 0; break; }
        row--;
        len = buf->rows[row].length;
        col = len > 0 ? len - 1 : 0;
    }

    if (row < 0) { c->pos.row = 0; c->pos.col = 0; c->desired_col = 0; return; }

    len = buf->rows[row].length;
    if (col >= len) col = len > 0 ? len - 1 : 0;

    // Find start of this word.
    if (col < len && is_word_char(buf->rows[row].line[col])) {
        while (col > 0 && is_word_char(buf->rows[row].line[col - 1])) col--;
    } else {
        while (col > 0 && !is_word_char(buf->rows[row].line[col - 1])
                       && !isspace((unsigned char)buf->rows[row].line[col - 1])) col--;
    }

    c->pos.row     = row;
    c->pos.col     = col;
    c->desired_col = col;
}

// e – move to end of current/next word.
void cursor_move_word_end(EditorCursor *c, buffer *buf) {
    if (!c || !buf) return;

    int row = c->pos.row;
    int col = c->pos.col;
    int len = buf->rows[row].length;

    // Step forward one.
    col++;
    while (1) {
        if (col >= len) {
            if (row + 1 >= buf->numrows) {
                col = len > 0 ? len - 1 : 0;
                break;
            }
            row++;
            col = 0;
            len = buf->rows[row].length;
            continue;
        }
        // Skip whitespace.
        if (isspace((unsigned char)buf->rows[row].line[col])) { col++; continue; }
        // Now at start of a word: advance to its end.
        if (is_word_char(buf->rows[row].line[col])) {
            while (col + 1 < len && is_word_char(buf->rows[row].line[col + 1])) col++;
        } else {
            while (col + 1 < len && !is_word_char(buf->rows[row].line[col + 1])
                                 && !isspace((unsigned char)buf->rows[row].line[col + 1])) col++;
        }
        break;
    }

    c->pos.row     = row;
    c->pos.col     = col < len ? col : (len > 0 ? len - 1 : 0);
    c->desired_col = c->pos.col;
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

void cursor_start_selection(EditorCursor *c) {
    if (!c) return;
    c->anchor    = c->pos;
    c->selecting = true;
}

void cursor_clear_selection(EditorCursor *c) {
    if (!c) return;
    c->selecting = false;
}

bool cursor_has_selection(const EditorCursor *c) {
    if (!c || !c->selecting) return false;
    return !(c->anchor.row == c->pos.row && c->anchor.col == c->pos.col);
}