#include "input.h"
#include "editor_cursor.h"
#include <ncurses.h>

// ---------------------------------------------------------------------------
// Per-tab mouse handler
// ---------------------------------------------------------------------------

void input_handle_mouse(Tab *t, int y, int x) {
    if (!t) return;
    t->cursor.pos.row     = y;
    t->cursor.pos.col     = x;
    cursor_clamp(&t->cursor, t->buf);
    t->cursor.desired_col = t->cursor.pos.col;
}

// ---------------------------------------------------------------------------
// Per-tab key handler
// ---------------------------------------------------------------------------

void input_handle_key(Tab *t, int key) {
    if (!t) return;

    switch (key) {

        // --- Cursor movement ---
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

        // --- Backspace ---
        case KEY_BACKSPACE:
        case '\b':
        case 127:
            if (t->cursor.pos.col > 0) {
                cursor_move_left(&t->cursor, t->buf);
                tabDeleteChar(t, t->cursor.pos.row, t->cursor.pos.col);
            } else if (t->cursor.pos.row > 0) {
                int merge_row = t->cursor.pos.row;
                cursor_move_left(&t->cursor, t->buf);
                tabDeleteCR(t, merge_row);
            }
            break;

        // --- Delete key ---
        case KEY_DC:
            tabDeleteChar(t, t->cursor.pos.row, t->cursor.pos.col);
            cursor_clamp(&t->cursor, t->buf);
            break;

        // --- Enter ---
        case KEY_ENTER:
        case '\n':
        case '\r':
            tabInsertCR(t, t->cursor.pos.row, t->cursor.pos.col);
            cursor_move_down(&t->cursor, t->buf);
            t->cursor.pos.col     = 0;
            t->cursor.desired_col = 0;
            break;

        // --- Undo / Redo ---
        case ('z' & 0x1f):   // Ctrl-Z
            tabUndo(t);
            break;

        case ('y' & 0x1f):   // Ctrl-Y
            tabRedo(t);
            break;

        // --- Printable characters ---
        default:
            if (key >= 32 && key < 127) {
                tabInsertChar(t, t->cursor.pos.row,
                                 t->cursor.pos.col, (char)key);
                cursor_move_right(&t->cursor, t->buf);
            }
            break;
    }
}

// ---------------------------------------------------------------------------
// App-level event entry point
// ---------------------------------------------------------------------------

void input_handle_next_event(EditorApp *app) {
    if (!app) return;

    int key = getch();
    Tab *t  = app_active_tab(app);

    switch (key) {

        // --- Tab switching: Ctrl-Tab / Ctrl-Shift-Tab (terminals vary) ---
        case ('t' & 0x1f):   // Ctrl-T: next tab
            app_switch_tab(app, app->active + 1);
            break;

        case ('b' & 0x1f):   // Ctrl-B: previous tab
            app_switch_tab(app, app->active - 1);
            break;

        // --- Close active tab: Ctrl-W ---
        case ('w' & 0x1f):
            app_close_tab(app, app->active);
            break;

        // --- Save active tab: Ctrl-S ---
        case ('s' & 0x1f):
            app_save_active(app);
            break;

        // --- Save all tabs: Ctrl-Shift-S approximated as Ctrl-A for now ---
        case ('a' & 0x1f):
            app_save_all(app);
            break;

        // --- Mouse ---
        case KEY_MOUSE:
            if (t) {
                MEVENT event;
                if (getmouse(&event) == OK)
                    input_handle_mouse(t, event.y, event.x);
            }
            break;

        // --- Everything else goes to the active tab ---
        default:
            if (t) input_handle_key(t, key);
            break;
    }
}