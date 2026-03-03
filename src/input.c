#include "input.h"
#include "editor_cursor.h"
#include <ncurses.h>

// ---------------------------------------------------------------------------
// Mouse handler -- accepts screen coordinates directly so it can be called
// from unit tests without a real MEVENT.
// ---------------------------------------------------------------------------

void input_handle_mouse(Editor *e, int y, int x) {
    if (!e) return;

    e->cursor.pos.row     = y;
    e->cursor.pos.col     = x;
    cursor_clamp(&e->cursor, e->buf);
    // Sync desired_col so vertical movement behaves correctly from new position
    e->cursor.desired_col = e->cursor.pos.col;
}

// ---------------------------------------------------------------------------
// Key handler -- exported so unit tests can inject synthetic key codes
// without a terminal.
// ---------------------------------------------------------------------------

void input_handle_key(Editor *e, int key) {
    if (!e) return;

    switch (key) {

        // --- Cursor movement ---
        case KEY_LEFT:
            cursor_move_left(&e->cursor, e->buf);
            break;

        case KEY_RIGHT:
            cursor_move_right(&e->cursor, e->buf);
            break;

        case KEY_UP:
            cursor_move_up(&e->cursor, e->buf);
            break;

        case KEY_DOWN:
            cursor_move_down(&e->cursor, e->buf);
            break;

        // --- Backspace: delete the character to the LEFT of the cursor ---
        case KEY_BACKSPACE:
        case '\b':
        case 127:
            if (e->cursor.pos.col > 0) {
                // Move left first, then delete the character now at cursor.
                // This avoids a double-move caused by cursor_clamp inside
                // editorDeleteChar firing before cursor_move_left.
                cursor_move_left(&e->cursor, e->buf);
                editorDeleteChar(e, e->cursor.pos.row, e->cursor.pos.col);
            } else if (e->cursor.pos.row > 0) {
                // At col 0: merge this row into the previous one.
                // Save the row number first since cursor_move_left will
                // change e->cursor.pos.row before we call editorDeleteCR.
                int merge_row = e->cursor.pos.row;
                cursor_move_left(&e->cursor, e->buf);
                editorDeleteCR(e, merge_row);
            }
            break;

        // --- Delete key: delete the character AT the cursor ---
        case KEY_DC:
            // editorDeleteChar guards against col >= length, so this is
            // safe even at the end of a line.
            editorDeleteChar(e, e->cursor.pos.row, e->cursor.pos.col);
            cursor_clamp(&e->cursor, e->buf);
            break;

        // --- Enter: split the current line at the cursor ---
        case KEY_ENTER:
        case '\n':
        case '\r':
            editorInsertCR(e, e->cursor.pos.row, e->cursor.pos.col);
            cursor_move_down(&e->cursor, e->buf);
            e->cursor.pos.col     = 0;
            e->cursor.desired_col = 0;
            break;

        // --- Undo / Redo (Ctrl-Z / Ctrl-Y) ---
        case ('z' & 0x1f):   // Ctrl-Z
            editorUndo(e);
            break;

        case ('y' & 0x1f):   // Ctrl-Y
            editorRedo(e);
            break;

        // --- Printable characters ---
        default:
            if (key >= 32 && key < 127) {
                editorInsertChar(e, e->cursor.pos.row,
                                    e->cursor.pos.col, (char)key);
                cursor_move_right(&e->cursor, e->buf);
            }
            // All other keys (function keys, etc.) are silently ignored.
            break;
    }
}

// ---------------------------------------------------------------------------
// Main event entry point -- called once per iteration of the UI loop.
// Blocks on getch() until the user produces input, then dispatches to
// input_handle_key or input_handle_mouse.  This is the only function in
// the input layer that touches the terminal directly.
// ---------------------------------------------------------------------------

void input_handle_next_event(Editor *e) {
    if (!e) return;

    int key = getch();

    if (key == KEY_MOUSE) {
        MEVENT event;
        if (getmouse(&event) == OK)
            input_handle_mouse(e, event.y, event.x);
    } else {
        input_handle_key(e, key);
    }
}