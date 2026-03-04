// input.c
//
// Key dispatch for a Vim-modal editor.
//
// Mode map
// --------
//   Normal mode  – navigation + single-key commands
//   Insert mode  – characters are inserted; Esc returns to Normal
//   Replace mode – characters overwrite; Esc returns to Normal
//   Visual mode  – character-wise selection; Esc / v returns to Normal
//   Visual-line  – whole-line selection; Esc / V returns to Normal
//
// Undo / redo
// -----------
//   u      – undo one change group (Vim `u`)
//   Ctrl-R – redo one change group (Vim `Ctrl-R`)
//   Each Insert/Replace session is one change group; single Normal-mode
//   commands (x, r, ~, etc.) are each their own group.

#include "input.h"
#include "editor_cursor.h"
#include "tab.h"
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// ---------------------------------------------------------------------------
// Internal UI helpers
// ---------------------------------------------------------------------------

static void render_tab_bar(EditorApp *app) {
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

        char label[24];
        snprintf(label, sizeof(label), "%s%s", t->dirty ? "*" : " ", name);

        int label_len = (int)strlen(label) + 2;

        if (i == app->active) attron(A_REVERSE);
        mvprintw(0, x, "[%s]", label);
        if (i == app->active) attroff(A_REVERSE);

        x += label_len;
        if (x < screen_cols) { mvaddch(0, x, ' '); x++; }
    }
    refresh();
}

// Display the current mode in the bottom-left corner.
static void render_mode_line(Tab *t) {
    if (!t) return;
    int rows = getmaxy(stdscr);
    move(rows - 1, 0);
    clrtoeol();

    const char *label;
    switch (t->mode) {
        case MODE_INSERT:      label = "-- INSERT --";      break;
        case MODE_REPLACE:     label = "-- REPLACE --";     break;
        case MODE_VISUAL:      label = "-- VISUAL --";      break;
        case MODE_VISUAL_LINE: label = "-- VISUAL LINE --"; break;
        default:               label = "";                  break;
    }
    mvprintw(rows - 1, 0, "%s", label);
    refresh();
}

static bool prompt_filename(const char *prompt, char *out_buf, int out_len) {
    int screen_rows = getmaxy(stdscr);
    int screen_cols = getmaxx(stdscr);

    move(screen_rows - 1, 0);
    clrtoeol();
    mvprintw(screen_rows - 1, 0, "%s", prompt);
    refresh();

    echo();
    curs_set(1);

    int prompt_len = (int)strlen(prompt);
    int max_input  = out_len - 1;
    if (max_input > screen_cols - prompt_len - 1)
        max_input = screen_cols - prompt_len - 1;

    int pos = 0;
    out_buf[0] = '\0';
    while (1) {
        int ch = getch();
        if (ch == 27) {
            noecho(); curs_set(0);
            move(screen_rows - 1, 0); clrtoeol();
            return false;
        }
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) break;
        if ((ch == KEY_BACKSPACE || ch == 127 || ch == '\b') && pos > 0) {
            pos--;
            out_buf[pos] = '\0';
            int cur_x = prompt_len + pos;
            mvaddch(screen_rows - 1, cur_x, ' ');
            move(screen_rows - 1, cur_x);
            refresh();
            continue;
        }
        if (ch >= 32 && ch < 127 && pos < max_input) {
            out_buf[pos++] = (char)ch;
            out_buf[pos]   = '\0';
        }
    }

    noecho(); curs_set(0);
    move(screen_rows - 1, 0); clrtoeol();
    return pos > 0;
}

static bool confirm_prompt(const char *message) {
    int screen_rows = getmaxy(stdscr);
    move(screen_rows - 1, 0);
    clrtoeol();
    mvprintw(screen_rows - 1, 0, "%s (y/n): ", message);
    refresh();

    int ch = getch();

    move(screen_rows - 1, 0);
    clrtoeol();
    refresh();

    return (ch == 'y' || ch == 'Y');
}

static bool save_active_or_prompt(EditorApp *app) {
    Tab *t = app_active_tab(app);
    if (!t) return false;

    if (t->filepath) return app_save_active(app);

    char path[512];
    if (!prompt_filename("Save as: ", path, sizeof(path))) return false;
    return app_save_active_as(app, path);
}

// ---------------------------------------------------------------------------
// Mouse handler
// ---------------------------------------------------------------------------

void input_handle_mouse(Tab *t, int y, int x) {
    if (!t) return;
    // Clicking always returns to Normal mode.
    if (t->mode != MODE_NORMAL) tab_enter_normal_mode(t);
    t->cursor.pos.row     = y;
    t->cursor.pos.col     = x;
    cursor_clamp(&t->cursor, t->buf);
    t->cursor.desired_col = t->cursor.pos.col;
}

// ---------------------------------------------------------------------------
// Normal mode key handler
// ---------------------------------------------------------------------------
//
// Returns false if the event should be forwarded to the app-level handler
// (shouldn't happen for keys we know about, but keeps the logic tidy).

static void handle_normal_key(Tab *t, int key) {
    if (!t) return;

    switch (key) {

        // --- Enter Insert mode ---
        case 'i':
            tab_enter_insert_mode(t);
            break;

        // Insert before first non-blank (Vim `I`)
        case 'I': {
            // Move to column 0 of current line then enter Insert.
            t->cursor.pos.col = 0;
            t->cursor.desired_col = 0;
            tab_enter_insert_mode(t);
            break;
        }

        // Append after cursor (Vim `a`)
        case 'a':
            cursor_move_right(&t->cursor, t->buf);
            tab_enter_insert_mode(t);
            break;

        // Append at end of line (Vim `A`)
        case 'A':
            if (t->buf && t->cursor.pos.row < t->buf->numrows)
                t->cursor.pos.col = t->buf->rows[t->cursor.pos.row].length;
            t->cursor.desired_col = t->cursor.pos.col;
            tab_enter_insert_mode(t);
            break;

        // Open new line below (Vim `o`)
        case 'o': {
            int row = t->cursor.pos.row;
            int end = t->buf->rows[row].length;
            tabInsertCR(t, row, end);
            t->cursor.pos.row++;
            t->cursor.pos.col     = 0;
            t->cursor.desired_col = 0;
            tab_enter_insert_mode(t);
            break;
        }

        // Open new line above (Vim `O`)
        case 'O': {
            int row = t->cursor.pos.row;
            tabInsertCR(t, row, 0);
            t->cursor.pos.col     = 0;
            t->cursor.desired_col = 0;
            tab_enter_insert_mode(t);
            break;
        }

        // Enter Replace mode (Vim `R`)
        case 'R':
            tab_enter_replace_mode(t);
            break;

        // Replace single char under cursor (Vim `r`)
        case 'r': {
            // Read the replacement character.
            int ch = getch();
            if (ch >= 32 && ch < 127 &&
                t->cursor.pos.row < t->buf->numrows &&
                t->cursor.pos.col < t->buf->rows[t->cursor.pos.row].length) {
                tabDeleteChar(t, t->cursor.pos.row, t->cursor.pos.col);
                tabInsertChar(t, t->cursor.pos.row, t->cursor.pos.col, (char)ch);
            }
            break;
        }

        // Delete char under cursor (Vim `x`)
        case 'x':
            tabDeleteChar(t, t->cursor.pos.row, t->cursor.pos.col);
            break;

        // Delete char before cursor (Vim `X`)
        case 'X':
            if (t->cursor.pos.col > 0) {
                t->cursor.pos.col--;
                tabDeleteChar(t, t->cursor.pos.row, t->cursor.pos.col);
            }
            break;

        // Toggle case (Vim `~`)
        case '~': {
            int row = t->cursor.pos.row;
            int col = t->cursor.pos.col;
            if (row < t->buf->numrows && col < t->buf->rows[row].length) {
                char c = t->buf->rows[row].line[col];
                char toggled = isupper((unsigned char)c)
                               ? (char)tolower((unsigned char)c)
                               : (char)toupper((unsigned char)c);
                tabDeleteChar(t, row, col);
                tabInsertChar(t, row, col, toggled);
                cursor_move_right(&t->cursor, t->buf);
            }
            break;
        }

        // --- Undo / Redo ---
        case 'u':
            tabUndo(t);
            break;

        case ('r' & 0x1f):   // Ctrl-R
            tabRedo(t);
            break;

        // --- Navigation (hjkl and arrow keys) ---
        case 'h': case KEY_LEFT:
            cursor_move_left(&t->cursor, t->buf);
            break;

        case 'l': case KEY_RIGHT:
            cursor_move_right(&t->cursor, t->buf);
            break;

        case 'k': case KEY_UP:
            cursor_move_up(&t->cursor, t->buf);
            break;

        case 'j': case KEY_DOWN:
            cursor_move_down(&t->cursor, t->buf);
            break;

        // Go to beginning / end of line (Vim `0` / `$`)
        case '0':
            t->cursor.pos.col     = 0;
            t->cursor.desired_col = 0;
            break;

        case '$':
            if (t->buf && t->cursor.pos.row < t->buf->numrows) {
                int len = t->buf->rows[t->cursor.pos.row].length;
                t->cursor.pos.col     = len > 0 ? len - 1 : 0;
                t->cursor.desired_col = t->cursor.pos.col;
            }
            break;

        // Go to first / last line (Vim `gg` is simplified to `g` here; `G`)
        case 'G':
            if (t->buf) {
                t->cursor.pos.row = t->buf->numrows - 1;
                t->cursor.pos.col = 0;
                t->cursor.desired_col = 0;
                cursor_clamp(&t->cursor, t->buf);
            }
            break;

        // Enter Visual mode (Vim `v`)
        case 'v':
            tab_enter_visual_mode(t);
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Insert / Replace mode key handler
// ---------------------------------------------------------------------------

static void handle_insert_key(Tab *t, int key) {
    if (!t) return;

    switch (key) {

        // Esc – leave Insert/Replace, return to Normal
        case 27:   // Escape
            tab_enter_normal_mode(t);
            break;

        // Arrow keys still move the cursor in Insert mode.
        case KEY_LEFT:
            cursor_move_left(&t->cursor, t->buf);
            break;
        case KEY_RIGHT:
            cursor_move_right(&t->cursor, t->buf);
            break;
        case KEY_UP:
            cursor_move_up(&t->cursor, t->buf);
            break;
        case KEY_DOWN:
            cursor_move_down(&t->cursor, t->buf);
            break;

        // Backspace
        case KEY_BACKSPACE: case '\b': case 127:
            if (t->cursor.pos.col > 0) {
                cursor_move_left(&t->cursor, t->buf);
                tabDeleteChar(t, t->cursor.pos.row, t->cursor.pos.col);
            } else if (t->cursor.pos.row > 0) {
                int merge_row = t->cursor.pos.row;
                cursor_move_left(&t->cursor, t->buf);
                tabDeleteCR(t, merge_row);
            }
            break;

        // Delete
        case KEY_DC:
            tabDeleteChar(t, t->cursor.pos.row, t->cursor.pos.col);
            cursor_clamp(&t->cursor, t->buf);
            break;

        // Enter
        case KEY_ENTER: case '\n': case '\r':
            tabInsertCR(t, t->cursor.pos.row, t->cursor.pos.col);
            t->cursor.pos.row++;
            t->cursor.pos.col     = 0;
            t->cursor.desired_col = 0;
            break;

        // Printable characters
        default:
            if (key >= 32 && key < 127) {
                if (t->mode == MODE_REPLACE) {
                    // Overwrite: delete the char under cursor first (if any).
                    int row = t->cursor.pos.row;
                    int col = t->cursor.pos.col;
                    if (row < t->buf->numrows &&
                        col < t->buf->rows[row].length) {
                        tabDeleteChar(t, row, col);
                    }
                }
                tabInsertChar(t, t->cursor.pos.row,
                                 t->cursor.pos.col, (char)key);
                cursor_move_right(&t->cursor, t->buf);
            }
            break;
    }
}

// ---------------------------------------------------------------------------
// Visual mode key handler (minimal; expand as needed)
// ---------------------------------------------------------------------------

static void handle_visual_key(Tab *t, int key) {
    if (!t) return;

    switch (key) {
        case 27:          // Esc – cancel selection
        case 'v':         // toggle off
            tab_enter_normal_mode(t);
            break;

        case 'h': case KEY_LEFT:  cursor_move_left (&t->cursor, t->buf); break;
        case 'l': case KEY_RIGHT: cursor_move_right(&t->cursor, t->buf); break;
        case 'k': case KEY_UP:    cursor_move_up   (&t->cursor, t->buf); break;
        case 'j': case KEY_DOWN:  cursor_move_down (&t->cursor, t->buf); break;

        default: break;
    }
}

// ---------------------------------------------------------------------------
// Per-tab key dispatcher
// ---------------------------------------------------------------------------

void input_handle_key(Tab *t, int key) {
    if (!t) return;

    switch (t->mode) {
        case MODE_NORMAL:
            handle_normal_key(t, key);
            break;
        case MODE_INSERT:
        case MODE_REPLACE:
            handle_insert_key(t, key);
            break;
        case MODE_VISUAL:
        case MODE_VISUAL_LINE:
            handle_visual_key(t, key);
            break;
        default:
            break;
    }

    render_mode_line(t);
}

// ---------------------------------------------------------------------------
// App-level event entry point
// ---------------------------------------------------------------------------

bool input_handle_next_event(EditorApp *app) {
    if (!app) return false;

    render_tab_bar(app);

    int key = getch();
    Tab *t  = app_active_tab(app);

    switch (key) {

        // Tab switching: Ctrl-T = next, Ctrl-B = previous
        case ('t' & 0x1f):
            app_switch_tab(app, app->active + 1);
            break;

        case ('b' & 0x1f):
            app_switch_tab(app, app->active - 1);
            break;

        // Close active tab: Ctrl-W
        case ('w' & 0x1f): {
            Tab *ct = app_active_tab(app);
            if (ct && ct->dirty) {
                if (!confirm_prompt("Tab has unsaved changes. Close anyway?"))
                    break;
            }
            app_close_tab(app, app->active);
            break;
        }

        // Save: Ctrl-S
        case ('s' & 0x1f):
            save_active_or_prompt(app);
            break;

        // Save As: Ctrl-E
        case ('e' & 0x1f): {
            char path[512];
            if (prompt_filename("Save as: ", path, sizeof(path)))
                app_save_active_as(app, path);
            break;
        }

        // Save all: Ctrl-A
        case ('a' & 0x1f):
            app_save_all(app);
            break;

        // Quit: Ctrl-Q
        case ('q' & 0x1f):
            if (app_any_dirty(app)) {
                if (!confirm_prompt("You have unsaved changes. Quit anyway?"))
                    break;
            }
            return false;

        // Mouse
        case KEY_MOUSE:
            if (t) {
                MEVENT event;
                if (getmouse(&event) == OK && event.y > 0)
                    input_handle_mouse(t, event.y - 1, event.x);
            }
            break;

        // Everything else: dispatch to active tab's modal handler
        default:
            if (t) input_handle_key(t, key);
            break;
    }

    return true;
}