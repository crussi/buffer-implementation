#include "editor_cursor.h"

void cursor_init(EditorCursor *c) {
    if (!c) return;
    c->pos.row     = 0;
    c->pos.col     = 0;
    c->anchor.row  = 0;
    c->anchor.col  = 0;
    c->selecting   = false;
    c->desired_col = 0;
}

void cursor_clamp(EditorCursor *c, buffer *buf) {
    if (!c || !buf) return;
    if (c->pos.row < 0)             c->pos.row = 0;
    if (c->pos.row >= buf->numrows) c->pos.row = buf->numrows - 1;
    int line_len = buf->rows[c->pos.row].length;
    if (c->pos.col < 0)        c->pos.col = 0;
    if (c->pos.col > line_len) c->pos.col = line_len;
}

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