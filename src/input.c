// input.c
//
// Vim-modal key dispatch.
//
// Mode map
// --------
//   Normal mode       – navigation + single/two-key commands + operators
//   Insert mode       – characters are inserted; Esc returns to Normal
//   Replace mode      – characters overwrite; Esc returns to Normal
//   Visual mode       – character-wise selection
//   Visual-line mode  – whole-line selection  (V)
//   Command mode      – ex commands after ':'
//
// Normal-mode operators implemented
// -----------------------------------
//   d{motion}  – delete + yank
//   y{motion}  – yank
//   c{motion}  – change (delete + enter Insert) – single undo unit
//   dd / yy / cc – line-wise shortcuts
//   >< indentation stubs (no-op, placeholder)
//
// Motions
// --------
//   h j k l  /  arrow keys
//   w b e    – word motions
//   0  $     – start / end of line
//   gg / G   – start / end of file
//
// Other Normal-mode commands
// ---------------------------
//   i I a A o O  – enter Insert mode (various positions)
//   R            – enter Replace mode
//   r<char>      – replace single char
//   x X          – delete char under/before cursor
//   ~            – toggle case
//   p P          – put (paste) after/before
//   u            – undo
//   Ctrl-R       – redo
//   v            – enter Visual mode
//   V            – enter Visual-line mode
//   :            – enter Command mode
//   Ctrl-T       – next tab
//   Ctrl-B       – previous tab
//   Ctrl-W       – close tab
//   Ctrl-S       – save
//   Ctrl-E       – save as
//   Ctrl-A       – save all
//   Ctrl-Q       – quit
//
// Command mode
// ------------
//   :w           – write (save)
//   :w <path>    – write to path
//   :q           – quit (refuses if dirty)
//   :q!          – force quit
//   :wq / :x    – write and quit
//   :e <path>    – open file in current tab
//   :tabnew      – open new tab
//   :tabn        – next tab
//   :tabp        – previous tab

#include "input.h"
#include "render.h"
#include "editor_cursor.h"
#include "tab.h"
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static bool execute_command(EditorApp *app, const char *cmd);

// ---------------------------------------------------------------------------
// Prompt helpers (used in Normal mode, not Command mode)
// ---------------------------------------------------------------------------

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
        if (ch == 27) { noecho(); curs_set(0); return false; }
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) break;
        if ((ch == KEY_BACKSPACE || ch == 127 || ch == '\b') && pos > 0) {
            pos--;
            out_buf[pos] = '\0';
            mvaddch(screen_rows - 1, prompt_len + pos, ' ');
            move(screen_rows - 1, prompt_len + pos);
            refresh();
            continue;
        }
        if (ch >= 32 && ch < 127 && pos < max_input) {
            out_buf[pos++] = (char)ch;
            out_buf[pos]   = '\0';
        }
    }

    noecho(); curs_set(0);
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
    // Vim does NOT force a mode change on mouse click; it repositions the
    // cursor within the current mode.  We do leave Visual/Command modes since
    // a click implicitly cancels a selection, but we do NOT close Insert mode
    // (which would prematurely end the undo group).
    if (t->mode == MODE_VISUAL || t->mode == MODE_VISUAL_LINE ||
        t->mode == MODE_COMMAND) {
        tab_enter_normal_mode(t);
    }
    // Adjust for scroll and tab bar.
    t->cursor.pos.row     = t->scroll_top  + y;
    t->cursor.pos.col     = t->scroll_left + x;
    cursor_clamp(&t->cursor, t->buf);
    t->cursor.desired_col = t->cursor.pos.col;
}

// ---------------------------------------------------------------------------
// Helpers: apply operator to a motion range
// ---------------------------------------------------------------------------

// Determine the end position after applying a motion key once.
// Returns false if the key is not a recognised motion.
static bool motion_end(Tab *t, int key, Position *end_out) {
    // We run the motion on a temporary cursor.
    EditorCursor tmp = t->cursor;
    bool found = true;

    switch (key) {
        case 'h': case KEY_LEFT:  cursor_move_left_normal (&tmp, t->buf); break;
        case 'l': case KEY_RIGHT: cursor_move_right_normal(&tmp, t->buf); break;
        case 'k': case KEY_UP:    cursor_move_up  (&tmp, t->buf); break;
        case 'j': case KEY_DOWN:  cursor_move_down(&tmp, t->buf); break;
        case 'w':  cursor_move_word_forward (&tmp, t->buf); break;
        case 'b':  cursor_move_word_backward(&tmp, t->buf); break;
        case 'e':  cursor_move_word_end     (&tmp, t->buf); break;
        case '0':  tmp.pos.col = 0; break;
        case '$':
            if (t->buf && tmp.pos.row < t->buf->numrows) {
                int len = t->buf->rows[tmp.pos.row].length;
                tmp.pos.col = len > 0 ? len - 1 : 0;
            }
            break;
        default:
            found = false;
            break;
    }

    if (found) *end_out = tmp.pos;
    return found;
}

// Apply pending_op ('d', 'y', 'c') between start and end positions.
//
// For 'c': the delete and the subsequent Insert-mode session must form a
// SINGLE undo group so that pressing `u` reverts the entire change (delete +
// typed text) in one step, exactly as Vim does.  We therefore open a group
// BEFORE the delete and leave it open when entering Insert mode; it will be
// closed by tab_leave_insert_mode / tab_enter_normal_mode.
static void apply_operator(Tab *t, char op, Position start, Position end) {
    // Normalise start <= end
    if (start.row > end.row || (start.row == end.row && start.col > end.col)) {
        Position tmp = start; start = end; end = tmp;
    }

    if (op == 'y') {
        tab_yank_range(t, start, end, false);
        t->cursor.pos = start;
    } else if (op == 'd') {
        tab_yank_range(t, start, end, false);
        tabDeleteRange(t, start, end);
        t->cursor.pos = start;
        cursor_clamp_normal(&t->cursor, t->buf);
    } else if (op == 'c') {
        tab_yank_range(t, start, end, false);

        // Open the group NOW so the delete actions AND all subsequent
        // Insert-mode typing share the same undo node.
        history_begin_group(t->history, t->cursor.pos);

        tabDeleteRange(t, start, end);
        t->cursor.pos = start;
        cursor_clamp(&t->cursor, t->buf);

        // Enter Insert mode without opening a second group.
        // tab_enter_insert_mode guards against MODE_INSERT, but the mode is
        // still MODE_NORMAL here, so we set it directly to avoid that guard
        // opening a redundant group.
        t->mode         = MODE_INSERT;
        t->pending_op   = '\0';
        t->repeat_count = 0;
    }
}

// ---------------------------------------------------------------------------
// Normal mode
// ---------------------------------------------------------------------------

static void handle_normal_key(Tab *t, int key) {
    if (!t) return;

    // --- Digit accumulation for repeat count ---
    if (key >= '1' && key <= '9' && t->pending_op == '\0') {
        t->repeat_count = t->repeat_count * 10 + (key - '0');
        return;
    }
    if (key == '0' && t->repeat_count > 0) {
        // '0' while accumulating a count extends it; otherwise it is BOL.
        t->repeat_count *= 10;
        return;
    }

    int count = (t->repeat_count > 0) ? t->repeat_count : 1;

    // --- Pending operator + motion ---
    if (t->pending_op != '\0') {
        char op = t->pending_op;

        // Same key as operator = line-wise shortcut (dd, yy, cc)
        if ((op == 'd' && key == 'd') ||
            (op == 'y' && key == 'y') ||
            (op == 'c' && key == 'c')) {
            for (int i = 0; i < count; i++) {
                if (op == 'y') {
                    tab_yank_line(t, t->cursor.pos.row);
                } else if (op == 'd') {
                    tab_yank_line(t, t->cursor.pos.row);
                    tabDeleteLine(t, t->cursor.pos.row);
                    cursor_clamp_normal(&t->cursor, t->buf);
                } else { // 'c'
                    tab_yank_line(t, t->cursor.pos.row);
                    // Open the group before deletions so the clear + subsequent
                    // insert-mode typing form a single undoable unit.
                    history_begin_group(t->history, t->cursor.pos);
                    int ccrow = t->cursor.pos.row;
                    for (int col = t->buf->rows[ccrow].length - 1; col >= 0; col--)
                        tabDeleteChar(t, ccrow, col);
                    t->cursor.pos.col = 0;
                    t->cursor.desired_col = 0;
                    // Set mode directly – group is already open.
                    t->mode = MODE_INSERT;
                }
            }
            t->pending_op   = '\0';
            t->repeat_count = 0;
            return;
        }

        Position end;
        if (motion_end(t, key, &end)) {
            for (int i = 0; i < count; i++) {
                apply_operator(t, op, t->cursor.pos, end);
                if (i + 1 < count) {
                    // Recalculate end from new cursor position.
                    if (!motion_end(t, key, &end)) break;
                }
            }
        }
        t->pending_op   = '\0';
        t->repeat_count = 0;
        return;
    }

    switch (key) {

        // ---- Enter Insert mode ----
        case 'i':
            t->repeat_count = 0;
            tab_enter_insert_mode(t);
            break;

        case 'I':
            t->cursor.pos.col = 0; t->cursor.desired_col = 0;
            t->repeat_count = 0;
            tab_enter_insert_mode(t);
            break;

        case 'a':
            // Move one position right, but allow landing on col == len
            // (i.e. after the last character) so Insert mode can append.
            if (t->buf && t->cursor.pos.row < t->buf->numrows) {
                int len = t->buf->rows[t->cursor.pos.row].length;
                if (t->cursor.pos.col < len) t->cursor.pos.col++;
            }
            t->cursor.desired_col = t->cursor.pos.col;
            t->repeat_count = 0;
            tab_enter_insert_mode(t);
            break;

        case 'A':
            if (t->buf && t->cursor.pos.row < t->buf->numrows)
                t->cursor.pos.col = t->buf->rows[t->cursor.pos.row].length;
            t->cursor.desired_col = t->cursor.pos.col;
            t->repeat_count = 0;
            tab_enter_insert_mode(t);
            break;

        case 'o': {
            // Open a line below current: insert CR at end of current line,
            // move cursor to the new empty line, enter Insert mode.
            // The group is opened FIRST so the CR and all subsequent typing
            // are a single undo unit (Vim `o` undoes back to pre-open state).
            int row     = t->cursor.pos.row;
            int end_col = t->buf->rows[row].length;
            history_begin_group(t->history, t->cursor.pos);
            tabInsertCR(t, row, end_col);
            t->cursor.pos.row++;
            t->cursor.pos.col     = 0;
            t->cursor.desired_col = 0;
            t->repeat_count = 0;
            // Set mode directly – group is already open.
            t->mode = MODE_INSERT;
            break;
        }

        case 'O': {
            // Open a line above current.
            int row = t->cursor.pos.row;
            history_begin_group(t->history, t->cursor.pos);
            tabInsertCR(t, row, 0);
            // Cursor stays on `row` — the new empty line.
            t->cursor.pos.col     = 0;
            t->cursor.desired_col = 0;
            t->repeat_count = 0;
            t->mode = MODE_INSERT;
            break;
        }

        case 'R':
            t->repeat_count = 0;
            tab_enter_replace_mode(t);
            break;

        // ---- Operators: set pending and wait for motion ----
        case 'd':
        case 'y':
        case 'c':
            t->pending_op = (char)key;
            // repeat_count carries forward into the operator application.
            break;

        // ---- Single-char replace ----
        // `r<char>` with a count replaces `count` characters at and after the
        // cursor.  All replacements are grouped into one undo step.
        case 'r': {
            int ch = getch();
            if (ch >= 32 && ch < 127 &&
                t->cursor.pos.row < t->buf->numrows &&
                t->cursor.pos.col < t->buf->rows[t->cursor.pos.row].length) {
                history_begin_group(t->history, t->cursor.pos);
                for (int i = 0; i < count; i++) {
                    if (t->cursor.pos.col <
                        t->buf->rows[t->cursor.pos.row].length) {
                        tabDeleteChar(t, t->cursor.pos.row, t->cursor.pos.col);
                        tabInsertChar(t, t->cursor.pos.row, t->cursor.pos.col,
                                      (char)ch);
                        // Advance cursor for multi-char replace, except on
                        // the last iteration (cursor stays on last replaced).
                        if (i + 1 < count)
                            cursor_move_right_normal(&t->cursor, t->buf);
                    }
                }
                history_end_group(t->history, t->cursor.pos);
            }
            t->repeat_count = 0;
            break;
        }

        // ---- Delete char ----
        // `x` with a count deletes `count` characters; the whole operation is
        // one undo step (like Vim).
        case 'x': {
            int row = t->cursor.pos.row;
            if (row < t->buf->numrows &&
                t->buf->rows[row].length > 0) {
                history_begin_group(t->history, t->cursor.pos);
                for (int i = 0; i < count; i++) {
                    if (t->cursor.pos.col <
                        t->buf->rows[t->cursor.pos.row].length) {
                        tab_yank_range(t, t->cursor.pos, t->cursor.pos, false);
                        tabDeleteChar(t, t->cursor.pos.row, t->cursor.pos.col);
                        cursor_clamp_normal(&t->cursor, t->buf);
                    }
                }
                history_end_group(t->history, t->cursor.pos);
            }
            t->repeat_count = 0;
            break;
        }

        case 'X': {
            if (t->cursor.pos.col > 0) {
                history_begin_group(t->history, t->cursor.pos);
                for (int i = 0; i < count; i++) {
                    if (t->cursor.pos.col > 0) {
                        t->cursor.pos.col--;
                        tab_yank_range(t, t->cursor.pos, t->cursor.pos, false);
                        tabDeleteChar(t, t->cursor.pos.row, t->cursor.pos.col);
                        cursor_clamp_normal(&t->cursor, t->buf);
                    }
                }
                history_end_group(t->history, t->cursor.pos);
            }
            t->repeat_count = 0;
            break;
        }

        // ---- Toggle case ----
        // `count ~` toggles `count` characters as one undo step.
        case '~': {
            int row = t->cursor.pos.row;
            if (row < t->buf->numrows &&
                t->cursor.pos.col < t->buf->rows[row].length) {
                history_begin_group(t->history, t->cursor.pos);
                for (int i = 0; i < count; i++) {
                    int r = t->cursor.pos.row;
                    int c = t->cursor.pos.col;
                    if (r < t->buf->numrows &&
                        c < t->buf->rows[r].length) {
                        char ch      = t->buf->rows[r].line[c];
                        char toggled = isupper((unsigned char)ch)
                                       ? (char)tolower((unsigned char)ch)
                                       : (char)toupper((unsigned char)ch);
                        tabDeleteChar(t, r, c);
                        tabInsertChar(t, r, c, toggled);
                        cursor_move_right_normal(&t->cursor, t->buf);
                    }
                }
                history_end_group(t->history, t->cursor.pos);
            }
            t->repeat_count = 0;
            break;
        }

        // ---- Undo / Redo ----
        case 'u':
            for (int i = 0; i < count; i++) tabUndo(t);
            t->repeat_count = 0;
            break;

        case ('r' & 0x1f):   // Ctrl-R
            for (int i = 0; i < count; i++) tabRedo(t);
            t->repeat_count = 0;
            break;

        // ---- Navigation ----
        case 'h': case KEY_LEFT:
            for (int i = 0; i < count; i++) cursor_move_left_normal(&t->cursor, t->buf);
            t->repeat_count = 0;
            break;

        case 'l': case KEY_RIGHT:
            for (int i = 0; i < count; i++) cursor_move_right_normal(&t->cursor, t->buf);
            t->repeat_count = 0;
            break;

        case 'k': case KEY_UP:
            for (int i = 0; i < count; i++) cursor_move_up(&t->cursor, t->buf);
            cursor_clamp_normal(&t->cursor, t->buf);
            t->repeat_count = 0;
            break;

        case 'j': case KEY_DOWN:
            for (int i = 0; i < count; i++) cursor_move_down(&t->cursor, t->buf);
            cursor_clamp_normal(&t->cursor, t->buf);
            t->repeat_count = 0;
            break;

        // ---- Word motions ----
        case 'w':
            for (int i = 0; i < count; i++) cursor_move_word_forward(&t->cursor, t->buf);
            cursor_clamp_normal(&t->cursor, t->buf);
            t->repeat_count = 0;
            break;

        case 'b':
            for (int i = 0; i < count; i++) cursor_move_word_backward(&t->cursor, t->buf);
            cursor_clamp_normal(&t->cursor, t->buf);
            t->repeat_count = 0;
            break;

        case 'e':
            for (int i = 0; i < count; i++) cursor_move_word_end(&t->cursor, t->buf);
            cursor_clamp_normal(&t->cursor, t->buf);
            t->repeat_count = 0;
            break;

        // ---- Line start / end ----
        case '0':
            t->cursor.pos.col     = 0;
            t->cursor.desired_col = 0;
            t->repeat_count = 0;
            break;

        case '$':
            if (t->buf && t->cursor.pos.row < t->buf->numrows) {
                int len = t->buf->rows[t->cursor.pos.row].length;
                t->cursor.pos.col     = len > 0 ? len - 1 : 0;
                t->cursor.desired_col = t->cursor.pos.col;
            }
            t->repeat_count = 0;
            break;

        // ---- File start / end ----
        case 'g':
            {
                int next = getch();
                if (next == 'g') {
                    t->cursor.pos.row     = 0;
                    t->cursor.pos.col     = 0;
                    t->cursor.desired_col = 0;
                }
            }
            t->repeat_count = 0;
            break;

        case 'G':
            if (t->buf) {
                int dest = (count > 1) ? count - 1 : t->buf->numrows - 1;
                if (dest >= t->buf->numrows) dest = t->buf->numrows - 1;
                t->cursor.pos.row     = dest;
                t->cursor.pos.col     = 0;
                t->cursor.desired_col = 0;
                cursor_clamp_normal(&t->cursor, t->buf);
            }
            t->repeat_count = 0;
            break;

        // ---- Scroll helpers (Ctrl-D / Ctrl-U half-page) ----
        case ('d' & 0x1f): {   // Ctrl-D
            int half = (getmaxy(stdscr) - 2) / 2;
            for (int i = 0; i < half; i++) cursor_move_down(&t->cursor, t->buf);
            cursor_clamp_normal(&t->cursor, t->buf);
            t->repeat_count = 0;
            break;
        }
        case ('u' & 0x1f): {   // Ctrl-U
            int half = (getmaxy(stdscr) - 2) / 2;
            for (int i = 0; i < half; i++) cursor_move_up(&t->cursor, t->buf);
            cursor_clamp_normal(&t->cursor, t->buf);
            t->repeat_count = 0;
            break;
        }

        // ---- Put ----
        case 'p':
            for (int i = 0; i < count; i++) tab_put_after(t);
            t->repeat_count = 0;
            break;

        case 'P':
            for (int i = 0; i < count; i++) tab_put_before(t);
            t->repeat_count = 0;
            break;

        // ---- Visual modes ----
        case 'v':
            t->repeat_count = 0;
            tab_enter_visual_mode(t);
            break;

        case 'V':
            t->repeat_count = 0;
            tab_enter_visual_line_mode(t);
            break;

        // ---- Command mode ----
        case ':':
            t->repeat_count = 0;
            tab_enter_command_mode(t);
            break;

        default:
            t->repeat_count = 0;
            break;
    }
}

// ---------------------------------------------------------------------------
// Insert / Replace mode
// ---------------------------------------------------------------------------

static void handle_insert_key(Tab *t, int key) {
    if (!t) return;

    switch (key) {
        case 27:   // Escape
            tab_enter_normal_mode(t);
            break;

        case KEY_LEFT:  cursor_move_left (&t->cursor, t->buf); break;
        case KEY_RIGHT: cursor_move_right(&t->cursor, t->buf); break;
        case KEY_UP:    cursor_move_up   (&t->cursor, t->buf); break;
        case KEY_DOWN:  cursor_move_down (&t->cursor, t->buf); break;

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

        case KEY_DC:
            tabDeleteChar(t, t->cursor.pos.row, t->cursor.pos.col);
            cursor_clamp(&t->cursor, t->buf);
            break;

        case KEY_ENTER: case '\n': case '\r':
            tabInsertCR(t, t->cursor.pos.row, t->cursor.pos.col);
            t->cursor.pos.row++;
            t->cursor.pos.col     = 0;
            t->cursor.desired_col = 0;
            break;

        default:
            if (key >= 32 && key < 127) {
                if (t->mode == MODE_REPLACE) {
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
// Visual mode
// ---------------------------------------------------------------------------

static void handle_visual_key(Tab *t, int key) {
    if (!t) return;

    switch (key) {
        // Cancel selection
        case 27:
        case 'v':
            tab_enter_normal_mode(t);
            break;

        case 'V':
            if (t->mode == MODE_VISUAL)
                tab_enter_visual_line_mode(t);
            else
                tab_enter_normal_mode(t);
            break;

        // Navigation
        case 'h': case KEY_LEFT:  cursor_move_left_normal (&t->cursor, t->buf); break;
        case 'l': case KEY_RIGHT: cursor_move_right_normal(&t->cursor, t->buf); break;
        case 'k': case KEY_UP:    cursor_move_up   (&t->cursor, t->buf); break;
        case 'j': case KEY_DOWN:  cursor_move_down (&t->cursor, t->buf); break;
        case 'w': cursor_move_word_forward (&t->cursor, t->buf); break;
        case 'b': cursor_move_word_backward(&t->cursor, t->buf); break;
        case 'e': cursor_move_word_end     (&t->cursor, t->buf); break;
        case '0': t->cursor.pos.col = 0; t->cursor.desired_col = 0; break;
        case '$':
            if (t->buf && t->cursor.pos.row < t->buf->numrows) {
                int len = t->buf->rows[t->cursor.pos.row].length;
                t->cursor.pos.col = len > 0 ? len - 1 : 0;
                t->cursor.desired_col = t->cursor.pos.col;
            }
            break;

        // Yank selection
        case 'y': {
            bool line_wise = (t->mode == MODE_VISUAL_LINE);
            if (line_wise) {
                int r1 = t->cursor.anchor.row, r2 = t->cursor.pos.row;
                if (r1 > r2) { int tmp = r1; r1 = r2; r2 = tmp; }
                size_t total = 0;
                for (int r = r1; r <= r2 && r < t->buf->numrows; r++)
                    total += (size_t)t->buf->rows[r].length + 1;
                char *buf = malloc(total + 1);
                if (buf) {
                    size_t pos = 0;
                    for (int r = r1; r <= r2 && r < t->buf->numrows; r++) {
                        memcpy(buf + pos, t->buf->rows[r].line,
                               (size_t)t->buf->rows[r].length);
                        pos += (size_t)t->buf->rows[r].length;
                        buf[pos++] = '\n';
                    }
                    buf[pos] = '\0';
                    free(t->yank_buf);
                    t->yank_buf = buf;
                    t->yank_is_line = true;
                }
            } else {
                tab_yank_range(t, t->cursor.anchor, t->cursor.pos, false);
            }
            tab_enter_normal_mode(t);
            break;
        }

        // Delete selection
        case 'd':
        case 'x': {
            bool line_wise = (t->mode == MODE_VISUAL_LINE);
            if (line_wise) {
                int r1 = t->cursor.anchor.row, r2 = t->cursor.pos.row;
                if (r1 > r2) { int tmp = r1; r1 = r2; r2 = tmp; }
                // Wrap all line deletions in one group.
                history_begin_group(t->history, t->cursor.pos);
                for (int r = r2; r >= r1; r--)
                    tabDeleteLine(t, r);
                history_end_group(t->history, t->cursor.pos);
            } else {
                tab_yank_range(t, t->cursor.anchor, t->cursor.pos, false);
                tabDeleteRange(t, t->cursor.anchor, t->cursor.pos);
            }
            tab_enter_normal_mode(t);
            cursor_clamp_normal(&t->cursor, t->buf);
            break;
        }

        // Change selection (delete + Insert) – must be one undo unit.
        case 'c': {
            bool line_wise = (t->mode == MODE_VISUAL_LINE);
            if (line_wise) {
                int r1 = t->cursor.anchor.row, r2 = t->cursor.pos.row;
                if (r1 > r2) { int tmp = r1; r1 = r2; r2 = tmp; }
                // Open group before any deletions so the whole change+insert
                // is one undo step.
                history_begin_group(t->history, t->cursor.pos);
                // Leave visual mode (clears selection) without calling
                // tab_enter_normal_mode so we don't reset pending_op, etc.
                cursor_clear_selection(&t->cursor);
                t->mode = MODE_NORMAL;
                for (int r = r2; r > r1; r--)
                    tabDeleteLine(t, r);
                while (t->buf->rows[r1].length > 0)
                    tabDeleteChar(t, r1, 0);
                t->cursor.pos.row = r1;
                t->cursor.pos.col = 0;
            } else {
                // Open group before the delete.
                history_begin_group(t->history, t->cursor.pos);
                tab_yank_range(t, t->cursor.anchor, t->cursor.pos, false);
                tabDeleteRange(t, t->cursor.anchor, t->cursor.pos);
                // Leave visual mode.
                cursor_clear_selection(&t->cursor);
                t->mode = MODE_NORMAL;
                t->cursor.pos = t->cursor.anchor;
                cursor_clamp(&t->cursor, t->buf);
            }
            // Enter Insert mode directly – the group is already open.
            t->mode         = MODE_INSERT;
            t->pending_op   = '\0';
            t->repeat_count = 0;
            break;
        }

        default: break;
    }
}

// ---------------------------------------------------------------------------
// Command mode (:ex commands)
// ---------------------------------------------------------------------------

static void handle_command_key(Tab *t, EditorApp *app, int key) {
    if (!t) return;

    switch (key) {
        case 27:   // Escape – cancel
            tab_enter_normal_mode(t);
            break;

        case '\n': case '\r': case KEY_ENTER: {
            // Execute the command.
            char cmd[CMD_BUF_MAX];
            strncpy(cmd, t->cmd_buf, CMD_BUF_MAX - 1);
            cmd[CMD_BUF_MAX - 1] = '\0';
            tab_enter_normal_mode(t);
            execute_command(app, cmd);
            break;
        }

        case KEY_BACKSPACE: case '\b': case 127:
            if (t->cmd_len > 0) {
                t->cmd_buf[--t->cmd_len] = '\0';
            } else {
                tab_enter_normal_mode(t);
            }
            break;

        default:
            if (key >= 32 && key < 127 && t->cmd_len < CMD_BUF_MAX - 1) {
                t->cmd_buf[t->cmd_len++] = (char)key;
                t->cmd_buf[t->cmd_len]   = '\0';
            }
            break;
    }
}

// ---------------------------------------------------------------------------
// Execute a colon command (:w, :q, :e, etc.)
// Returns false if the app should quit.
// ---------------------------------------------------------------------------

static bool s_quit_requested = false;

static bool execute_command(EditorApp *app, const char *cmd) {
    if (!app || !cmd) return true;

    // Strip leading whitespace.
    while (*cmd == ' ') cmd++;

    // :w [path]
    if (strncmp(cmd, "w", 1) == 0 && (cmd[1] == '\0' || cmd[1] == ' ')) {
        const char *path = cmd[1] == ' ' ? cmd + 2 : NULL;
        while (path && *path == ' ') path++;
        if (path && *path != '\0')
            app_save_active_as(app, path);
        else
            save_active_or_prompt(app);
        return true;
    }

    // :wq / :x
    if (strcmp(cmd, "wq") == 0 || strcmp(cmd, "x") == 0) {
        save_active_or_prompt(app);
        s_quit_requested = true;
        return false;
    }

    // :q!
    if (strcmp(cmd, "q!") == 0) {
        s_quit_requested = true;
        return false;
    }

    // :q
    if (strcmp(cmd, "q") == 0) {
        if (app_any_dirty(app)) {
            int rows = getmaxy(stdscr);
            move(rows - 1, 0); clrtoeol();
            mvprintw(rows - 1, 0, "E: unsaved changes (use :q! to force)");
            refresh();
            getch();
        } else {
            s_quit_requested = true;
            return false;
        }
        return true;
    }

    // :e <path>
    if (strncmp(cmd, "e ", 2) == 0) {
        const char *path = cmd + 2;
        while (*path == ' ') path++;
        if (*path) {
            Tab *t = app_active_tab(app);
            if (t) tab_open(t, path);
        }
        return true;
    }

    // :tabnew
    if (strcmp(cmd, "tabnew") == 0) {
        app_new_tab(app);
        return true;
    }

    // :tabn[ext]
    if (strcmp(cmd, "tabn") == 0 || strcmp(cmd, "tabnext") == 0) {
        app_switch_tab(app, app->active + 1);
        return true;
    }

    // :tabp[revious]
    if (strcmp(cmd, "tabp") == 0 || strcmp(cmd, "tabprev") == 0 ||
        strcmp(cmd, "tabprevious") == 0) {
        app_switch_tab(app, app->active - 1);
        return true;
    }

    // Unknown command – show error briefly.
    {
        int rows = getmaxy(stdscr);
        move(rows - 1, 0); clrtoeol();
        mvprintw(rows - 1, 0, "E: unknown command: %s", cmd);
        refresh();
        getch();
    }
    return true;
}

// ---------------------------------------------------------------------------
// Per-tab key dispatcher
// ---------------------------------------------------------------------------

void input_handle_key(Tab *t, EditorApp *app, int key) {
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
        case MODE_COMMAND:
            handle_command_key(t, app, key);
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// App-level event entry point
// ---------------------------------------------------------------------------

bool input_handle_next_event(EditorApp *app) {
    if (!app) return false;

    // Render before waiting for input.
    render_screen(app);

    if (s_quit_requested) return false;

    int key = getch();
    Tab *t  = app_active_tab(app);

    // App-level keys (work in any mode).
    switch (key) {

        // Tab switching
        case ('t' & 0x1f):   // Ctrl-T
            if (t && t->mode != MODE_COMMAND)
                app_switch_tab(app, app->active + 1);
            break;

        case ('b' & 0x1f):   // Ctrl-B
            if (t && t->mode != MODE_COMMAND)
                app_switch_tab(app, app->active - 1);
            break;

        // Close active tab: Ctrl-W (only in Normal mode to avoid accidents)
        case ('w' & 0x1f): {
            if (!t || t->mode != MODE_NORMAL) {
                if (t) input_handle_key(t, app, key);
                break;
            }
            if (t->dirty) {
                if (!confirm_prompt("Tab has unsaved changes. Close anyway?"))
                    break;
            }
            app_close_tab(app, app->active);
            if (app_tab_count(app) == 0) return false;
            break;
        }

        // Save: Ctrl-S
        case ('s' & 0x1f):
            if (t && t->mode != MODE_COMMAND)
                save_active_or_prompt(app);
            break;

        // Save As: Ctrl-E
        case ('e' & 0x1f): {
            if (t && t->mode != MODE_COMMAND) {
                char path[512];
                if (prompt_filename("Save as: ", path, sizeof(path)))
                    app_save_active_as(app, path);
            }
            break;
        }

        // Save all: Ctrl-A
        case ('a' & 0x1f):
            if (t && t->mode != MODE_COMMAND)
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

        // Everything else dispatches to the active tab's modal handler.
        default:
            if (t) input_handle_key(t, app, key);
            break;
    }

    if (s_quit_requested) return false;

    return true;
}