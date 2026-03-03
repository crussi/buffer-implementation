//input.c

#include "input.h"
#include "editor_cursor.h"
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// Internal UI helpers
// ---------------------------------------------------------------------------

// Render a one-line tab bar at the top of the screen.
// Active tab is shown in reverse video; dirty tabs get a '*' prefix.
// Format: [*filename] [ filename] ...
static void render_tab_bar(EditorApp *app) {
    if (!app) return;

    int screen_cols = getmaxx(stdscr);
    move(0, 0);
    clrtoeol();

    int x = 0;
    for (int i = 0; i < app->count && x < screen_cols; i++) {
        Tab *t = app->tabs[i];

        // Build label: "*name" or "name", capped at 20 chars
        const char *base = t->filepath ? t->filepath : "[new]";
        // Use only the final path component for brevity
        const char *slash = strrchr(base, '/');
        const char *name  = slash ? slash + 1 : base;

        char label[24];
        snprintf(label, sizeof(label), "%s%s",
                 t->dirty ? "*" : " ", name);

        int label_len = (int)strlen(label) + 2; // +2 for brackets

        if (i == app->active) attron(A_REVERSE);
        mvprintw(0, x, "[%s]", label);
        if (i == app->active) attroff(A_REVERSE);

        x += label_len;
        if (x < screen_cols) {
            mvaddch(0, x, ' ');
            x++;
        }
    }
    refresh();
}

// Prompt the user to type a file path at the bottom of the screen.
// Writes into `out_buf` (max `out_len` bytes including NUL).
// Returns true if the user entered a non-empty name, false if they cancelled
// with Escape.
static bool prompt_filename(const char *prompt, char *out_buf, int out_len) {
    int screen_rows = getmaxy(stdscr);
    int screen_cols = getmaxx(stdscr);

    // Draw prompt on the last line
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

    // Read characters manually so we can detect Escape
    int pos = 0;
    out_buf[0] = '\0';
    while (1) {
        int ch = getch();
        if (ch == 27) {           // Escape — cancel
            noecho();
            curs_set(0);
            // Clear the prompt line
            move(screen_rows - 1, 0);
            clrtoeol();
            return false;
        }
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) break;
        if ((ch == KEY_BACKSPACE || ch == 127 || ch == '\b') && pos > 0) {
            pos--;
            out_buf[pos] = '\0';
            // Erase the last character on screen
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

    noecho();
    curs_set(0);

    // Clear the prompt line
    move(screen_rows - 1, 0);
    clrtoeol();

    return pos > 0;
}

// Display a yes/no confirmation on the last line.
// Returns true if the user presses 'y' or 'Y'.
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

// Try to save the active tab. If the tab has no path yet, prompt for one.
// Returns false if the save ultimately failed or was cancelled.
static bool save_active_or_prompt(EditorApp *app) {
    Tab *t = app_active_tab(app);
    if (!t) return false;

    if (t->filepath) {
        // Normal save
        return app_save_active(app);
    }

    // No filepath — need Save As
    char path[512];
    if (!prompt_filename("Save as: ", path, sizeof(path)))
        return false;   // user cancelled

    return app_save_active_as(app, path);
}

// ---------------------------------------------------------------------------
// Per-tab mouse handler
// ---------------------------------------------------------------------------

// input_handle_mouse accepts plain buffer coordinates (row, col).
// Callers that receive raw screen coordinates (e.g. from ncurses MEVENT)
// are responsible for subtracting any UI chrome offset (tab bar, etc.)
// before calling this function.
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

// Returns false when the user has confirmed they want to quit.
bool input_handle_next_event(EditorApp *app) {
    if (!app) return false;

    // Re-render the tab bar on every event so it stays current
    render_tab_bar(app);

    int key = getch();
    Tab *t  = app_active_tab(app);

    switch (key) {

        // --- Tab switching: Ctrl-T = next, Ctrl-B = previous ---
        case ('t' & 0x1f):
            app_switch_tab(app, app->active + 1);
            break;

        case ('b' & 0x1f):
            app_switch_tab(app, app->active - 1);
            break;

        // --- Close active tab: Ctrl-W ---
        // Warn if the tab has unsaved changes before closing.
        case ('w' & 0x1f): {
            Tab *ct = app_active_tab(app);
            if (ct && ct->dirty) {
                if (!confirm_prompt("Tab has unsaved changes. Close anyway?"))
                    break;
            }
            app_close_tab(app, app->active);
            break;
        }

        // --- Save active tab: Ctrl-S ---
        // Prompts for a filename if the tab has never been saved.
        case ('s' & 0x1f):
            save_active_or_prompt(app);
            break;

        // --- Save As active tab: Ctrl-E  (explicit rename/new path) ---
        // Always prompts for a filename, allowing the user to rename.
        case ('e' & 0x1f): {
            char path[512];
            if (prompt_filename("Save as: ", path, sizeof(path)))
                app_save_active_as(app, path);
            break;
        }

        // --- Save all tabs: Ctrl-A ---
        // Tabs without a filepath are silently skipped; user must
        // Ctrl-S each new tab individually to assign a path first.
        case ('a' & 0x1f):
            app_save_all(app);
            break;

        // --- Quit: Ctrl-Q ---
        // If any tab has unsaved changes, confirm before quitting.
        case ('q' & 0x1f):
            if (app_any_dirty(app)) {
                if (!confirm_prompt(
                        "You have unsaved changes. Quit anyway?"))
                    break;
            }
            return false;   // signal the event loop to exit

        // --- Mouse ---
        // event.y is a raw screen row. Row 0 is the tab bar — ignore clicks
        // there. For all other rows subtract 1 before passing to
        // input_handle_mouse, which works in buffer coordinates.
        case KEY_MOUSE:
            if (t) {
                MEVENT event;
                if (getmouse(&event) == OK && event.y > 0)
                    input_handle_mouse(t, event.y - 1, event.x);
            }
            break;

        // --- Everything else goes to the active tab ---
        default:
            if (t) input_handle_key(t, key);
            break;
    }

    return true;   // keep running
}