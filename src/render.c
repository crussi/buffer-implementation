// render.c
//
// Screen rendering for the Vim-modal editor.
// Draws: tab bar (row 0), buffer content (rows 1..LINES-2),
//        status / mode line (row LINES-1).

#include "render.h"
#include "editor_cursor.h"
#include <ncurses.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Tab bar
// ---------------------------------------------------------------------------

void render_tab_bar(EditorApp *app) {
    if (!app) return;

    int screen_cols = getmaxx(stdscr);
    move(0, 0);
    clrtoeol();

    int x = 0;
    for (int i = 0; i < app->count && x < screen_cols; i++) {
        Tab *t = app->tabs[i];

        const char *base  = t->filepath ? t->filepath : "[new]";
        const char *slash = strrchr(base, '/');
        const char *name  = slash ? slash + 1 : base;

        char label[26];
        snprintf(label, sizeof(label), "%s%s", t->dirty ? "*" : " ", name);

        int label_len = (int)strlen(label) + 2; // for '[' and ']'

        if (i == app->active) attron(A_REVERSE);
        mvprintw(0, x, "[%s]", label);
        if (i == app->active) attroff(A_REVERSE);

        x += label_len;
        if (x < screen_cols) { mvaddch(0, x, ' '); x++; }
    }
}

// ---------------------------------------------------------------------------
// Buffer content
// ---------------------------------------------------------------------------

// Determine if a position is within the visual selection.
static bool pos_in_selection(const Tab *t, int row, int col) {
    if (!cursor_has_selection(&t->cursor)) return false;

    Position a = t->cursor.anchor;
    Position b = t->cursor.pos;

    // Normalise so a <= b
    if (a.row > b.row || (a.row == b.row && a.col > b.col)) {
        Position tmp = a; a = b; b = tmp;
    }

    if (t->mode == MODE_VISUAL_LINE) {
        return row >= a.row && row <= b.row;
    }

    // Character-wise
    if (row < a.row || row > b.row) return false;
    if (row == a.row && col < a.col) return false;
    if (row == b.row && col > b.col) return false;
    return true;
}

void render_buffer(Tab *t, int top_row, int left_col,
                   int view_rows, int view_cols) {
    if (!t || !t->buf) return;

    for (int screen_row = 0; screen_row < view_rows; screen_row++) {
        int buf_row = top_row + screen_row;
        move(1 + screen_row, 0);  // row 0 is tab bar
        clrtoeol();

        if (buf_row >= t->buf->numrows) {
            // Vim-style tilde for lines past end of buffer
            attron(A_DIM);
            addch('~');
            attroff(A_DIM);
            continue;
        }

        const char *line     = t->buf->rows[buf_row].line;
        int         line_len = t->buf->rows[buf_row].length;

        for (int screen_col = 0; screen_col < view_cols; screen_col++) {
            int buf_col = left_col + screen_col;
            char ch = (buf_col < line_len) ? line[buf_col] : ' ';

            bool highlighted = pos_in_selection(t, buf_row, buf_col);
            if (highlighted) attron(A_REVERSE);
            addch((unsigned char)ch);
            if (highlighted) attroff(A_REVERSE);
        }
    }
}

// ---------------------------------------------------------------------------
// Status / mode line
// ---------------------------------------------------------------------------

void render_status_line(Tab *t, const char *cmd_buf) {
    if (!t) return;
    int screen_rows = getmaxy(stdscr);
    int screen_cols = getmaxx(stdscr);
    move(screen_rows - 1, 0);
    clrtoeol();

    if (t->mode == MODE_COMMAND) {
        // Command-line mode: show ':' followed by whatever the user has typed.
        mvprintw(screen_rows - 1, 0, ":%s", cmd_buf ? cmd_buf : "");
        // Position cursor at end of command string
        int cpos = 1 + (cmd_buf ? (int)strlen(cmd_buf) : 0);
        if (cpos < screen_cols)
            move(screen_rows - 1, cpos);
    } else {
        const char *label = "";
        switch (t->mode) {
            case MODE_INSERT:      label = "-- INSERT --";      break;
            case MODE_REPLACE:     label = "-- REPLACE --";     break;
            case MODE_VISUAL:      label = "-- VISUAL --";      break;
            case MODE_VISUAL_LINE: label = "-- VISUAL LINE --"; break;
            default:               label = "";                  break;
        }
        mvprintw(screen_rows - 1, 0, "%s", label);

        // Right-aligned: row,col indicator
        char pos_str[32];
        snprintf(pos_str, sizeof(pos_str), "%d,%d",
                 t->cursor.pos.row + 1, t->cursor.pos.col + 1);
        int plen = (int)strlen(pos_str);
        mvprintw(screen_rows - 1, screen_cols - plen - 1, "%s", pos_str);
    }
}

// ---------------------------------------------------------------------------
// Full-screen refresh
// ---------------------------------------------------------------------------

void render_screen(EditorApp *app) {
    if (!app) return;

    Tab *t = app_active_tab(app);

    int screen_rows = getmaxy(stdscr);
    int screen_cols = getmaxx(stdscr);

    // Row 0 = tab bar, rows 1..screen_rows-2 = buffer, row screen_rows-1 = status
    int view_rows = screen_rows - 2;
    int view_cols = screen_cols;

    render_tab_bar(app);

    if (t) {
        // Simple scroll: keep cursor visible.
        // We store scroll offsets directly on the Tab for simplicity.
        // Vertical scroll
        if (t->cursor.pos.row < t->scroll_top)
            t->scroll_top = t->cursor.pos.row;
        if (t->cursor.pos.row >= t->scroll_top + view_rows)
            t->scroll_top = t->cursor.pos.row - view_rows + 1;

        // Horizontal scroll
        if (t->cursor.pos.col < t->scroll_left)
            t->scroll_left = t->cursor.pos.col;
        if (t->cursor.pos.col >= t->scroll_left + view_cols)
            t->scroll_left = t->cursor.pos.col - view_cols + 1;

        render_buffer(t, t->scroll_top, t->scroll_left, view_rows, view_cols);
        render_status_line(t, t->cmd_buf);

        // Place the hardware cursor.
        if (t->mode != MODE_COMMAND) {
            int cy = 1 + (t->cursor.pos.row - t->scroll_top);
            int cx = t->cursor.pos.col - t->scroll_left;
            if (cy >= 1 && cy < screen_rows - 1 &&
                cx >= 0 && cx < screen_cols)
                move(cy, cx);
        }
    }

    refresh();
}